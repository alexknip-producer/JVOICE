/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI drawing: the header, the four knob cards, the footer (steps + engine / preset / page)
 * and the frame; the panel between the cards and the footer is ui_graph.c. The layout:
 * flat SURF cards and panels on BG,
 * rounded corners, no rules (MENU > STYLE LINE: no SURF, 1 px rules between the areas instead: draw_rules), colours from the theme tokens only (gfx.c T_*). Type: S (12 px) labels,
 * M (15 px) values and the header, L (28 px) big numerals. (JIANT 0.6.56: S Chakra Petch, M IBM Plex Mono, L Michroma 18 px;
 * the JIANT palette's boxes square with HUD corners, gfx.c cv_rrect). A card value too wide for M is set in S;
 * free text (names, messages) is ellipsised at its size. */
static void draw_menu(void);
static int name_on(void);                              /* NAME (ui_name.c) */
static uint64_t lock_held(void);                       /* parameter locks: the steps held on STEP (ui_input.c) */
static void name_draw(void);
static void fill_around(const int16_t (*r)[4], uint32_t n);   /* (0.9.0) BG round a screen's strips */

/* --------------------------------------------------------- drawing --- */
#define COL_W CARD_W                                  /* a card: 57 x 44 at x 3 + 59 c, y 28 */
#define COL_H CARD_H

static int32_t batt_level(void)                         /* thresholds 531 / 561 / 591 on ADC ch3 */
{
    return song.batt_raw >= 591 ? 3 : song.batt_raw >= 561 ? 2 : song.batt_raw >= 531 ? 1 : 0;
}
/* A USB host powers us; there is no separate charger status line.
 * 4 means external power (a steady bolt inside the battery), otherwise its level. */
static int32_t batt_shown(void)
{
    if (usb.config && !usb.suspended)
        return 4;
    return batt_level();
}

/* (0.9.0) a caption drawn over HUD's picture (the strip's words outlined: cv_halo): MID / DIM read poorly there, TEXT */
static uint16_t hud_cap(uint16_t c) { return cv_halo && (c == T_MID || c == T_DIM) ? T_TEXT : c; }
#define HEAD_MID 11                                    /* (0.8.5) the header's middle row: its ink centred on it (the
                                                        * rule at 22) */
/* REC: filled, the selected track armed; outlined, another track armed. (0.8.5, the mock-up's skin) a dot, 9 px, its
 * ink centred on the header's row 11 in a 16 px cell at x (x + 3 .. x + 11) */
static void draw_rec_mark(int32_t x, uint16_t bg)
{
    if (!song.rec)
        return;
    uint16_t keep[25];
    int ring = !((song.rec >> song.sel) & 1u), pic = cv_halo && ring;   /* (0.9.0) HUD's HOME: the ring's hole the picture */
    if (pic)
        cv_keep(x + 5, HEAD_MID - 2, 5, 5, keep, 0);
    cv_rrect(x + 3, HEAD_MID - 4, 9, 9, 4, T_REC, bg);
    if (pic) {
        keep[0] = keep[4] = keep[20] = keep[24] = swap16(T_REC);   /* (the hole's corners: the ring's) */
        cv_keep(x + 5, HEAD_MID - 2, 5, 5, keep, 1);
    } else if (ring)
        cv_rrect(x + 5, HEAD_MID - 2, 5, 5, 2, bg, T_REC);
}

/* the battery: (0.8.5, as the mock-up) an outline 13 x 9 (rows 7..15) and its charge inside, 0..3 thirds of 9 px
 * (the stock thresholds); USB power (4): full, THEME. One third left: the accent; else MID */
static uint32_t batt_icon(int32_t lvl)
{
    static const uint8_t I[5] = {ICON_X_BAT0, ICON_X_BAT1, ICON_X_BAT3, ICON_X_BAT4, ICON_X_BAT_CHG};
    return I[clamp(lvl, 0, 4)];
}
/* (JIANT 0.5) the load: how much of the audio's time the sound takes now (audio.c song.cpu_q8, averaged); 0..5.
 * (0.8.5, the mock-up) five bars 3 x 7 side by side left of the battery, lit THEME from the left, the rest RAISE */
static uint32_t head_load(void)
{
    uint32_t l = (song.cpu_q8 * 5u + 128u) >> 8;
    return song.cpu_q8 && !l ? 1u : l > 5u ? 5u : l;
}
#define HEAD_LOAD_X 200                                /* the load's bars: 200 .. 218 */
static void draw_load(int32_t x)
{
    uint32_t k, n = head_load();
    for (k = 0; k < 5u; k++)
        if (k >= n && cv_halo)                          /* (0.9.0) HUD's HOME: an unlit bar outlined, the beings through it */
            cv_frame(x + (int32_t)k * 4, HEAD_MID - 3, 3, 7, T_DIM);
        else
            cv_rect(x + (int32_t)k * 4, HEAD_MID - 3, 3, 7, k < n ? T_THEME : T_RAISE);
}
static void draw_battery(int32_t bx)
{
    int32_t lvl = batt_shown();
    uint16_t c = lvl == 4 ? T_THEME : lvl == 1 ? T_ACCENT : T_MID;
    cv_frame(bx, HEAD_MID - 4, 13, 9, c);
    if (lvl)
        cv_rect(bx + 2, HEAD_MID - 2, lvl >= 3 ? 9 : lvl * 3, 5, c);
}

/* ---------------------------------------------------- rolling digits --- */
/* A number that changes rolls its changed digits like a slot machine; only the drawing: the value, the gauge,
 * the hot colour and the sound change at once. Up: the old digit leaves upward and the new one comes from below;
 * down: the reverse. ROLL_FRAMES UI frames (ui.frame, ~135 ms) on an integer ease-out (ROLL_EASE: cubic, over
 * half-way in a quarter of the time, no overshoot); the ones digit first, each place one frame later, all ending
 * together on a frame equal to the static render. Clipped to the value strip, which alone is redrawn while it
 * rolls (a card: rows ROLL_Y.., the header: the BPM's columns). Characters other than digits never roll.
 * A change during a roll retargets it: it restarts from the value it was going to. Shown at once (no roll):
 * a change within ROLL_SNAP frames of the last one (a fast turn reads better as plain numbers), another
 * shape (length, sign, a non-digit, a name or an enum, a value set in S), another label or unit, a track /
 * engine / palette change (ui.roll[].sig), ui.force (a page change), a card's first draw; MENU > ANIM OFF (#46). */
#define ROLL_FRAMES 9u
#define ROLL_SNAP 3u                                    /* frames (~45 ms) */
#define ROLL_BPM 4u                                     /* ui.roll[]: the four cards, then the header BPM */
#define ROLL_Y (lz_low() ? 11 : lz_on() ? 14 : 18)        /* a card's value strip: rows 18..34, its figures (21..31) in the middle, */
#define ROLL_H 17                                       /* the label (MONO's chip: to row 17) above it, the gauge (39) below;
                                                         * (0.8.5) a lens page's card (38 tall): rows 14..30, the figures
                                                         * 17..27, the gauge at 33; 0.8.9 SKIN BOTTOM / HUD's
                                                         * knob row (33 tall): rows 11..27, the figures 14..24, the
                                                         * label 4..10, the gauge 28..29) */
#define CARD_VY (ROLL_Y - 1)                            /* the value's line (M) */
#define CARD_XY 4                                       /* (0.8.5) a lens card's label (AF_X): its line, capitals rows 5..11 */
#define CARD_BG (ux.style || lz_low() ? T_SURF : T_RAISE)   /* (0.8.5, the mock-up) a card: RAISE (LINE: no fill; 0.8.9 BOTTOM,
                                                         * HUD: none, the knob row on the background) */
static const uint8_t ROLL_EASE[17] = {0, 45, 84, 118, 147, 172, 193, 210, 223, 234, 242, 247, 251, 253, 254, 255, 255};

static int roll_digit(char c) { return c >= '0' && c <= '9'; }
/* +1 / -1: b rolls in from a (same length, digits where a has digits, the rest equal and one of + - .),
 * the sign of b - a; 0: b is shown at once */
static int roll_dir(const char *a, const char *b)
{
    uint32_t i;
    int d = 0, neg = 0;
    for (i = 0; a[i] || b[i]; i++) {
        if (i + 1u >= sizeof ui.roll[0].from || roll_digit(a[i]) != roll_digit(b[i]))
            return 0;
        if (!roll_digit(a[i]) && (a[i] != b[i] || (a[i] != '-' && a[i] != '+' && a[i] != '.')))
            return 0;
        neg |= a[i] == '-';
        if (!d && a[i] != b[i])
            d = b[i] > a[i] ? 1 : -1;
    }
    return neg ? -d : d;
}
/* field k now shows b instead of a: roll (a retarget mid-roll), or snap */
static void roll_note(uint32_t k, const char *a, const char *b, int snap)
{
    uint32_t el = (uint8_t)(ui.frame - ui.roll[k].t0);
    int d = roll_dir(a, b);
    ui.roll[k].t0 = (uint8_t)ui.frame;
    ui.roll[k].from[0] = 0;
    if (snap || !d || el < ROLL_SNAP || ANIM_SNAP())
        return;
    str_cpy(ui.roll[k].from, a, sizeof ui.roll[k].from);
    ui.roll[k].dir = (int8_t)d;
}
/* s (M) at x, y with field k's roll: each character at its pen in s (the digits are tabular and never kerned,
 * so the old value has the same pens); a rolling digit has moved o of the strip's h rows (ROLL_EASE, one frame
 * later per place from the right), clipped to the strip. The roll's last frame clears it: the static text. */
static int32_t roll_text(uint32_t k, int32_t x, int32_t y, const char *s, uint16_t fg)
{
    const aafont_t *f = &AF_M;                         /* (0.8.5: the header's BPM in S; 0.8.9, the skins' mock-up: in M) */
    uint32_t e = (uint8_t)(ui.frame - ui.roll[k].t0) + 1u, i, p = 0;
    const char *a = ui.roll[k].from;
    int32_t cy = k < ROLL_BPM ? ROLL_Y : 0, h = k < ROLL_BPM ? ROLL_H : HEAD_RULE_Y;   /* (above the header's rule) */
    uint16_t bg = k < ROLL_BPM ? CARD_BG : T_BG;
    char t[8], ch[2] = {0, 0};
    if (e >= ROLL_FRAMES)
        ui.roll[k].from[0] = 0;
    if (!a[0])
        return cv_text_on(x, y, f, s, fg, bg);
    str_cpy(t, s, sizeof t);
    for (i = 0; s[i]; i++)
        p += (uint32_t)roll_digit(s[i]);
    cv_cy0 = (int16_t)(cy + cv_oy);                     /* the strip (the static characters lie inside it) */
    cv_cy1 = (int16_t)(cy + cv_oy + h);
    cv_scroll = 1;                                      /* (the lint: rolling digits are cut on purpose) */
    for (i = 0; s[i]; i++) {
        int32_t px, ny = y;
        t[i] = 0;
        px = x + text_w(f, t);                          /* (its pen: no digit kerns) */
        t[i] = s[i];
        p -= (uint32_t)roll_digit(s[i]);
        if (s[i] != a[i]) {                             /* p: the places right of it */
            int32_t o = 0, dir = ui.roll[k].dir;
            if (e > p)                                  /* (e < ROLL_FRAMES: the LUT index stays in 1..16) */
                o = (int32_t)((ROLL_EASE[((e - p) * 32u / (ROLL_FRAMES - p) + 1u) >> 1] * (uint32_t)h + 128u) >> 8);
            ch[0] = a[i];
            cv_text_on(px, y - dir * o, f, ch, fg, bg);
            ny = y + dir * (h - o);
        }
        ch[0] = s[i];
        cv_text_on(px, ny, f, ch, fg, bg);
    }
    cv_cy0 = 0;
    cv_cy1 = (int16_t)cv_h;
    cv_scroll = 0;
    return x + text_w(f, s);
}

/* the header (y 0..24). (0.8.5) The approved mock-up's skin: one quiet line over a LINE rule at row 22,
 *   left    (0.8.9, the skins' mock-up: the 0.6 colours) the selected track's number in an ACCENT box (4..16), the play
 *           state (x 22: stopped a MID square, playing a THEME triangle, counting in the accent) and the BPM large, in M
 *           (THEME; the knob turning it: the accent) from x 35, then BPM LOCK's lock and the REC mark (a dot: filled the
 *           selected track armed, a ring another)
 *   centre  what the page is of, centred on x 120 (MID): "T1 . ANALOG ACID" (the track, its engine and sound), on the
 *           song's pages (SAVE, ARRANGE, TOOLS, GLOBAL) "P1 NAME" (the project), on FX "T1" (the accent)
 *   right   the octave ("OCT +2"), or the song row while a song plays (the disc and its number), ending at 196; the load
 *           (five bars 200..218) and the battery (222..234)
 * A message or a layer's label replaces the centre and the octave, centred on x 120, up to 158 px; while the BPM is
 * being changed (a SELECT turn: bpm_t) or in the GLO layer the BPM stays and the message goes right of it (over the
 * right slot). Every element's ink centred on row 11 (HEAD_MID; ui_test.c test_head_centres): S's capitals rows 7..15 */
#define HEAD_SY (HEAD_MID - AF_S_CAP_H / 2 - AF_S_CAP_Y)
#define HEAD_XY (HEAD_MID - AF_X_CAP_H / 2 - AF_X_CAP_Y)   /* (0.8.5) the centre's words in AF_X: capitals rows 8..14 */
#define HEAD_MY CAP_IN(M, H_HEAD)                      /* (the menu's and the calibration's own headers: M) */
#define HEAD_BMY (HEAD_MID - AF_M_CAP_H / 2 - AF_M_CAP_Y)   /* (0.8.9) the BPM in M: capitals rows 6..16 */
#define HEAD_TK_X 4                                    /* (0.8.9) the track's box: 4 .. 16, rows 5 .. 17 */
#define HEAD_TR_X 22                                   /* the play state's ink: 22 .. 29 */
#define HEAD_BPM_X 35                                  /* the BPM's pen */
#define HEAD_BAT_X 222                                 /* the battery's outline: 222 .. 234 */
#define HEAD_GRP_R 196                                 /* the octave / song row ends here */
#define HEAD_MSG_W 158                                 /* a message centred on x 120: 41..199 */
#define HEAD_GAP 5                                     /* ink to ink: the BPM -> the lock -> the REC mark */
/* the BPM's pen and ink (b), the play state's ink (mx0 .. mx1), the BPM's rolling strip (sx0 .. sx1): placed by as many
 * 0s as the BPM has digits (tabular figures), so nothing moves while the tempo changes */
typedef struct { int32_t bx, b[4], mx0, mx1, sx0, sx1; } head_geo_t;
static void head_geo(head_geo_t *g, const char *bpm)
{
    char z[8] = "0000000";
    z[str_len(bpm) < 7u ? str_len(bpm) : 7u] = 0;
    g->bx = HEAD_BPM_X;
    text_ink(&AF_M, z, g->b);
    g->mx0 = HEAD_TR_X;
    g->mx1 = HEAD_TR_X + 8;
    g->sx0 = g->mx1 + 1;
    g->sx1 = g->bx + g->b[2] + 2;
}
static void head_transport(void)
{
    int32_t r, k;
    uint16_t c = seq_counting() ? T_ACCENT : song.playing ? T_THEME : T_MID;
    if (!song.playing && !seq_counting()) {
        cv_rect(HEAD_TR_X, HEAD_MID - 4, 8, 9, c);
        return;
    }
    for (k = cv_halo; k >= 0; k--)                      /* (0.8.9) a triangle, 8 x 11, pointing right (HUD: its outline) */
        for (r = 0; r < 11; r++)
            cv_rect(HEAD_TR_X + k, HEAD_MID - 5 + r + k, 1 + (5 - (r < 5 ? 5 - r : r - 5)) * 7 / 5, 1, k ? T_BG : c);
}
static void sound_name(const track_t *t, char *b);
/* the centre's words (b holds 40) and their colour */
static uint16_t head_title(char *b)
{
    const page_t *pg = cur_page();
    int glob = !ui.home && !ui.layer && (pg->graph == GR_SLOTS || pg->graph == GR_SONG || pg->graph == GR_TOOLS ||
                                         pg->fam == FAM_GLO || pg->fam == FAM_SAVE);
    if (ui.menu) {                                      /* (0.8.5) the menu: "MENU . DISPLAY", "ABOUT / CREDITS" */
        str_cpy(b, ui.menu == 2 ? "ABOUT / CREDITS" : "MENU \xB7 ", 40);
        if ((ui.menu | 2u) == 3u)
            str_cpy(b + str_len(b), MTAB_NAME[MI_TAB[ui.menu_sel % MI_COUNT]], 16);
        return T_MID;
    }
    b[0] = 'T';
    b[1] = (char)('1' + song.sel % NTRK);
    b[2] = 0;
    if (!ui.home && !ui.layer && pg->fam == FAM_FX)
        return T_ACCENT;
    if (glob) {
        char nm[16];
        b[0] = 'P';
        fmt_int(b + 1, (int32_t)song_cur + 1);
        if (graph_project_used(0) && graph_project_name(0)[0])   /* (0.8.5) the project's name: its section A's (as the */
            str_cpy(nm, graph_project_name(0), sizeof nm);        /* library's), else the slot's last loaded / saved */
        else
            project_cur_name(nm);
        str_cpy(b + str_len(b), " ", 2);
        str_cpy(b + str_len(b), nm[0] ? nm : "NO NAME", 16);
        return T_MID;
    }
    str_cpy(b + 2, " \xB7 ", 4);
    str_cpy(b + str_len(b), trk_engp(TSEL)->name, 12);
    sound_name(TSEL, b + str_len(b) + 1u);
    if (b[str_len(b) + 1u])
        b[str_len(b)] = ' ';
    return T_MID;
}
/* a message / a layer's label at x, or centred on x 120 (x < 0), at most maxw px with its
 * icon (ui.c MSG_NOFILE: the accent's) */
static void head_msg(int32_t x, int32_t maxw)
{
    const char *m = ui.msg_t ? ui.msg : layer_head();
    int icon = m[0] == MSG_NOFILE[0];
    int32_t iw = icon ? text_w(&AF_X, "!") + KH_GAP : 0;
    /* (0.8.9) a message or a layer's name ("[GLO] SET", "[FX] LATCH") in plain words, AF_X as the header's centre
     * (was S with a keycap, Felucca's): a message TEXT, a layer THEME */
    if (x < 0)
        x = 120 - HALF_UP(iw + (text_w(&AF_X, m + icon) < maxw - iw ? text_w(&AF_X, m + icon) : maxw - iw));
    if (icon) {                                       /* (0.8.5: no icons; "!" in the accent) */
        GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_XY, &AF_X, "!", T_ACCENT) + KH_GAP;
    }
    GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
    cv_free_text(x, HEAD_XY, &AF_X, m + icon, ui.msg_t ? T_TEXT : T_THEME, T_BG, maxw - iw);
}
/* the octave, or the song row while a song plays, its right end at HEAD_GRP_R; its left end */
static int32_t head_group(void)
{
    char b[8];
    int32_t x, x0;
    if (chain.running) {
        fmt_int(b, (int32_t)chain.row + 1);
        uint16_t keep[4];
        x0 = x = HEAD_GRP_R - (12 + 3 + text_w(&AF_S, b));
        if (cv_halo)                                    /* (0.9.0) HUD's HOME: its hole the picture */
            cv_keep(x + 5, HEAD_MID - 1, 2, 2, keep, 0);
        cv_rrect(x + 1, HEAD_MID - 5, 10, 10, -5, hud_cap(T_MID), T_BG);   /* SONG: the disc (drawn, 0.8.5) */
        if (cv_halo)
            cv_keep(x + 5, HEAD_MID - 1, 2, 2, keep, 1);
        else
            cv_rect(x + 5, HEAD_MID - 1, 2, 2, T_BG);
        x += 12 + 3;
    } else {
        str_cpy(b, song.octave > 0 ? "+" : "", sizeof b);
        fmt_int(b + str_len(b), song.octave);
        x0 = x = HEAD_GRP_R - (text_w(&AF_S, "OCT") + 4 + text_w(&AF_S, b));
        GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
        x = cv_text(x, HEAD_SY, &AF_S, "OCT", hud_cap(T_MID)) + 4;
    }
    GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
    cv_text(x, HEAD_SY, &AF_S, b, T_THEME);
    return x0;
}
static void draw_head(void)
{
    char b[16], ttl[40];
    head_geo_t g;
    uint16_t tc = head_title(ttl);
    uint32_t rec = (song.rec >> song.sel) & 1u ? 2u : song.rec != 0u;   /* 2 the selected track armed, 1 another */
    uint32_t sig = (uint32_t)song.playing * 3u + rec * 5u + (uint32_t)(song.octave + 8) * 11u + song.sel * 13131u +
                   (ui.msg_t ? str_hash(7u, ui.msg) : ui.layer * 7919u) + (uint32_t)song.g[G_BPM] * 101u + (ui.bpm_t != 0) * 31u +
                   (uint32_t)seq_counting() * 7u + (ui_prefs & PREF_BPM_LOCK) * 4099u +
                   (uint32_t)batt_shown() * 7777u + (chain.running ? (chain.row + 1u) * 104729u : 0u) +
                   head_load() * 0x9E3779B1u + str_hash(tc, ttl) * 3u;
    int msg = ui.msg_t || ui.layer;
    int centre = !msg || ui.bpm_t || ui.layer == LAYER_GLO;   /* the BPM being changed: it stays, the message right */
    int32_t lx, r = HEAD_LOAD_X - 6;
    if (song.g[G_BPM] != ui.roll_bpm) {
        char a[8];
        fmt_int(a, ui.roll_bpm);
        fmt_int(b, song.g[G_BPM]);
        roll_note(ROLL_BPM, a, b, ui.force);
        ui.roll_bpm = song.g[G_BPM];
    } else if (ui.force) {
        ui.roll[ROLL_BPM].from[0] = 0;
    }
    fmt_int(b, song.g[G_BPM]);
    head_geo(&g, b);
    if (!ui.force && sig == ui.head_sig) {
        if (ui.roll[ROLL_BPM].from[0]) {               /* rolling: the BPM's strip only (the rule under it again) */
            cv_begin((uint32_t)(g.sx1 - g.sx0), H_HEAD, T_BG);
            roll_text(ROLL_BPM, g.bx - g.sx0, HEAD_BMY, b, ui.bpm_t ? T_ACCENT : T_THEME);
            if (!skin_hud())
                cv_rect(0, HEAD_RULE_Y, g.sx1 - g.sx0, 1, T_LINE);
            cv_blit((uint32_t)g.sx0, Y_HEAD);
        }
        return;
    }
    ui.head_sig = sig;
    lz_begin(240, H_HEAD, 0, Y_HEAD);
    tc = hud_cap(tc);
    if (!ui.menu) {                                     /* (0.8.9) the track's box, as the 0.6 had it */
        char n[2] = {(char)('1' + song.sel % NTRK), 0};
        uint8_t h = cv_halo;
        cv_rrect(HEAD_TK_X, HEAD_MID - 6, 13, 13, 3, T_ACCENT, T_BG);
        cv_halo = 0;
        GFX_HOOK_ALIGN(HEAD_TK_X, HEAD_MID - 6, HEAD_TK_X + 13, HEAD_MID + 7, AL_HV, "header track number in its box");
        cv_text_in(HEAD_TK_X, HEAD_XY, 13, &AF_X, n, T_INK, T_ACCENT);
        cv_halo = h;
    }
    head_transport();
    if (!ui.roll[ROLL_BPM].from[0])
        GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
    roll_text(ROLL_BPM, g.bx, HEAD_BMY, b, ui.bpm_t ? T_ACCENT : T_THEME);
    lx = g.bx + g.b[2];                                /* the right end of the BPM's ink (and what follows it) */
    if (ui_prefs & PREF_BPM_LOCK) {                    /* BPM LOCK ON (#58): the lock right of the BPM */
        int32_t x = lx + HEAD_GAP;                     /* (0.8.5: drawn) a shackle over a body, 8 x 10 */
        cv_rect(x + 2, HEAD_MID - 5, 4, 1, T_MID);
        cv_rect(x + 1, HEAD_MID - 4, 1, 3, T_MID);
        cv_rect(x + 6, HEAD_MID - 4, 1, 3, T_MID);
        cv_rect(x, HEAD_MID - 1, 8, 6, T_MID);
        lx += HEAD_GAP + 8;
    }
    if (song.rec) {                                    /* REC: its dot (x + 3 .. x + 11 of its cell) */
        draw_rec_mark(lx + HEAD_GAP - 3, T_BG);
        lx += HEAD_GAP + 9;
    }
    if (!msg && (chain.running || song.octave))
        r = head_group() - 6;
    if (!msg) {                                        /* the centre's words on x 120, between the BPM and the right */
        int32_t w = text_w(&AF_X, ttl), x = 120 - HALF_UP(w), l = lx + 8;
        if (x + w > r) x = r - w;
        if (x < l) x = l;
        GFX_HOOK_ALIGN(0, 0, 0, HEAD_RULE_Y, AL_V, "header text on its middle");
        cv_free_text(x, HEAD_XY, &AF_X, ttl, tc, T_BG, r - x);   /* (0.8.5: AF_X, the mock-up's small mono) */
    } else if (centre) {                               /* right of the BPM, over the right slot */
        int32_t x = lx + 9 > 142 ? lx + 9 : 142;
        head_msg(x, 236 - x);
    } else {                                           /* centred on x 120, clear of the BPM and the REC mark */
        head_msg(-1, lx + 8 > 120 - HEAD_MSG_W / 2 ? 2 * (120 - (lx + 8)) : HEAD_MSG_W);
    }
    if (!(msg && centre)) {
        draw_battery(HEAD_BAT_X);
        draw_load(HEAD_LOAD_X);
    }
    if (!skin_hud()) {                                  /* the rule (0.8.9 HUD, 0.9.7 AIR: none, the words float) */
        cv_rect(0, HEAD_RULE_Y, 240, 1, T_LINE);
        GFX_HOOK_RULE(0, HEAD_RULE_Y, 240, 1);
    }
    lz_blit(0, Y_HEAD);
}
/* LINE: 1 px dividers in the BG gaps between the strips, not around them: under the header, above and
 * below the panel, and between the four columns over rows y .. y + h (the cards; the mixer's strips too) */
#define RULE_X0 CARD_X(0)
#define RULE_W (CARD_X(3) + CARD_W - CARD_X(0))
static void lcd_rule(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    lcd_fill(x, y, w, h, T_RULE);
    GFX_HOOK_RULE(x, y, w, h);
}
static void draw_rules(uint32_t y, uint32_t h)
{
    uint32_t i;
    if (y == card_y() && lz_low() && ux.skin == SKIN_AIR) {   /* (0.9.7) AIR: over the knobs' row and under it */
        lcd_rule(RULE_X0, (LT_Y + LB_TAB_H + card_y()) / 2, RULE_W, 1);
        lcd_rule(RULE_X0, (card_y() + card_h() + graph_y()) / 2, RULE_W, 1);
    } else if (y == card_y() && lz_low()) {             /* (0.8.9) BOTTOM, HUD: over the panel and under it */
        lcd_rule(RULE_X0, (LT_Y + LB_TAB_H + graph_y()) / 2, RULE_W, 1);
        lcd_rule(RULE_X0, (graph_y() + graph_h() + card_y()) / 2, RULE_W, 1);
    } else if (y == card_y()) {                         /* (LARGE: the geometry of the page, ui.c card_h; 0.8.5: a lens
                                                         * page's; the header draws its own rule) */
        lcd_rule(RULE_X0, (card_y() + card_h() + graph_y()) / 2, RULE_W, 1);
        lcd_rule(RULE_X0, (graph_y() + graph_h() + foot_y()) / 2, RULE_W, 1);
    }
    for (i = 0; i < 3u; i++)
        lcd_rule((uint32_t)(CARD_X(i) + CARD_W), y, 1, h);
}
/* full redraw: the strips (header, cards, panel, footer) cover the rest; only the BG between them is filled.
 * (0.8.5, the mock-up's skin) the cards RAISE boxes: no dividers between them (LINE: its rules) */
static void draw_frame(void)
{
    uint32_t i, ch = card_h(), gy = graph_y(), gh = graph_h(), cy = card_y(), fy = foot_y();   /* (MENU > LARGE: tall
                                                         * cards, a strip; 0.8.5 a lens page: the mock-up's) */
    if (hud_home())                                     /* (0.8.9) HUD's HOME: its strips cover the screen */
        return;
    if (cy > gy) {                                      /* (0.8.9) BOTTOM, HUD: the panel, then the knobs' row */
        lcd_fill(0, H_HEAD, 240, gy - H_HEAD, T_BG);
        lcd_fill(0, gy + gh, 240, cy - gy - gh, T_BG);
        lcd_fill(0, cy + ch, 240, fy - cy - ch, T_BG);
    } else {
        lcd_fill(0, H_HEAD, 240, cy - H_HEAD, T_BG);
        lcd_fill(0, cy + ch, 240, gy - cy - ch, T_BG);
        lcd_fill(0, gy + gh, 240, fy - gy - gh, T_BG);
    }
    lcd_fill(0, cy, (uint32_t)CARD_X(0), ch, T_BG);
    for (i = 0; i < 4u; i++)                            /* right of each card */
        lcd_fill((uint32_t)(CARD_X(i) + CARD_W), cy, i < 3u ? (uint32_t)(CARD_X(i + 1u) - CARD_X(i) - CARD_W) :
                 240u - (uint32_t)(CARD_X(i) + CARD_W), ch, T_BG);
    if (ux.style)                                       /* LINE: the dividers in the gaps */
        draw_rules(cy, ch);
}

/* MOTION on the cards (#63: values that move on their own): the selected track's motion-driven parameters
 * (motion.c motion_mask: PLAY ON and an event for the id), once per draw_columns; card_mot_next is set just
 * before a draw_column of a track parameter (card_mot_of) and taken by it; card_mot: the cards drawn with it. */
static uint32_t card_mot_mask[(P_COUNT + 31u) / 32u];
static uint8_t card_mot_next, card_mot;
static void card_mot_of(const int16_t *vp)
{
    uintptr_t id = ((uintptr_t)vp - (uintptr_t)TSEL->p) / sizeof *vp;   /* (not the track's: huge) */
    card_mot_next = (uint8_t)(id < P_COUNT && ((card_mot_mask[id / 32u] >> (id % 32u)) & 1u));
}

/* MENU > LARGE, a tall card (ui.c LK_TALL; 57 x LG_CARD_H): which knob it is (a "K1" pill in M, the keycap's colours,
 * the accent while it turns, DIM when the knob does nothing here) and the icon at the right of it; the label in M
 * centred (S when M is too wide); the value centred in L, its figures twice the height of M (M, then S, when L is too
 * wide or the sparse L face lacks a glyph: gen_aa_font.py L_CHARS); the unit (S) centred under it; the gauge. The
 * value snaps (no rolling digits). MONO, the knob just turned: the label inverted as on the small cards. */
#ifndef LARGE_HOOK
#define LARGE_HOOK(s, why) ((void)0)                    /* host: a tall card's value and its face (0 L, 1 M: a glyph not
                                                         * in L, 2 M: L too wide, 3 S) */
#endif
#define LG_MY 4                                         /* the K pill: rows 4..21 */
#define LG_MH 18
#define LG_LY 24                                        /* the label's line: M capitals rows 28..38 */
#define LG_VB 49                                        /* the value's band: L capitals rows 49..69 */
#define LG_UY 73                                        /* the unit's line (S capitals rows 76..84) */
#define LG_GY 92                                        /* the gauge, 3 px */
/* a card's gauge and value: (0.8.5, the mock-up) the gauge THEME, the value TEXT; only the knob just turned (T_ACCENT)
 * stands out. (0.9.5, as the 0.6.36 had it) JIANT (thermal): each card's colour its value as heat, violet low .. cream
 * high, the gauge in it and the value lifted a fifth toward the text (so the cold end reads); a bipolar parameter (its
 * range across 0) by its distance from the centre, cold in the middle */
static uint8_t card_bip;                                /* the card being drawn: a bipolar parameter (ratio_of) */
static uint16_t gauge_col(int32_t ratio)
{
    return heat_on(ratio) ? card_heat(ratio, card_bip) : T_THEME;
}
static uint16_t value_col(uint16_t vc, int32_t ratio)
{
    if (vc != T_THEME)
        return vc;
    return heat_on(ratio) ? ux_mix(gauge_col(ratio), T_TEXT, 20) : T_TEXT;
}
static uint8_t card_free_next;                          /* (0.8.5) the next draw_column's value is a name: free text */
static uint8_t card_arm_next;                           /* (0.8.5) the next draw_column is an action armed (OCT+ does it):
                                                         * the card TINT, its label THEME, as the mock-up's */
static void draw_column_tall(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc, int32_t ratio,
                             uint32_t icon, int hot, int mot, int32_t kid)
{
    uint16_t lc = hot ? T_ACCENT : mot ? T_THEME : T_MID;
    const aafont_t *lf = &AF_M, *vf = &AF_L;
    int32_t ly = LG_LY, vy, lw = 0, mw;
    char k[3] = {'K', (char)('1' + c), 0};
    uint32_t ic = mot ? ICON_X_MOTION : icon, isz = 16u;
    cv_begin(COL_W, LG_CARD_H, T_BG);
    cv_rrect(0, 0, COL_W, LG_CARD_H, 4, T_SURF, T_BG);
    {   /* the knob: "K1" .. "K4" on a pill */
        uint16_t fill = hot ? T_ACCENT : label[0] || val[0] ? T_KEY : T_DIM;
        mw = ink_w(&AF_M, k) + 10;
        cv_rrect(5, LG_MY, mw, LG_MH, 5, fill, T_SURF);
        GFX_HOOK_ALIGN(5, LG_MY, 5 + mw, LG_MY + LG_MH, AL_HV, "large card knob pill");
        cv_text_in(5, LG_MY + CAP_IN(M, LG_MH), mw, &AF_M, k, T_INK, fill);
    }
    if (!label[0] && !val[0]) {                         /* a knob that does nothing on this page */
        cv_blit((uint32_t)CARD_X(c), card_y());
        return;
    }
    (void)ic; (void)isz;
    if (mot && label[0]) {                              /* (0.8.5: no icons) MOTION: a dot on the pill's line */
        GFX_HOOK_ALIGN(0, LG_MY, 0, LG_MY + LG_MH, AL_V, "large card icon on its pill's line");
        cv_rrect(COL_W - 5 - 12, LG_MY + HALF_UP(LG_MH - 8), 8, 8, -4, hot ? T_ACCENT : T_THEME, T_SURF);
    }
    if (label[0]) {
        if ((lw = ink_w(lf, label)) > COL_W - 6) {      /* M too wide: S, on the same capitals' middle */
            lf = &AF_S;
            ly = LG_LY + AF_M_CAP_Y + HALF_UP(AF_M_CAP_H - AF_S_CAP_H) - AF_S_CAP_Y;
            lw = ink_w(lf, label);
        }
        if (hot && T_ACCENT == T_MID) {                 /* MONO: the label inverted, the chip 2 px round its ink */
            int32_t cy = ly + (lf == &AF_M ? AF_M_CAP_Y : AF_S_CAP_Y), chh = lf == &AF_M ? AF_M_CAP_H : AF_S_CAP_H;
            int32_t x0 = HALF_UP(COL_W - lw) - 2;
            cv_rrect(x0, cy - 3, lw + 4, chh + 6, 3, T_ACCENT, T_SURF);
            GFX_HOOK_ALIGN(x0, cy - 3, x0 + lw + 4, cy + chh + 3, AL_HV, "large card label chip (MONO hot)");
            cv_text_in(0, ly, COL_W, lf, label, T_INK, T_ACCENT);
        } else {
            GFX_HOOK_ALIGN(0, 0, COL_W, 0, AL_H, "large card label centred");
            cv_text_in(0, ly, COL_W, lf, label, lc, T_SURF);
        }
    }
    if (kid >= 0) {                                     /* "[OCT+]": the keycap, centred */
        GFX_HOOK_ALIGN(0, LG_VB, COL_W, LG_VB + AF_L_CAP_H, AL_HV, "large card keycap centred");
        cv_keycap(HALF_UP(COL_W - kc_w((uint32_t)kid)), LG_VB + HALF_UP(AF_L_CAP_H - KC_H), (uint32_t)kid,
                  vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_INK, T_SURF);
    } else if (val[0]) {
        int why = !large_face_has(val) ? 1 : ink_w(vf, val) > COL_W - 6 ? 2 : 0;
        if (why)
            vf = &AF_M;
        if (vf == &AF_M && ink_w(vf, val) > COL_W - 6)
            vf = &AF_S, why = 3;
        LARGE_HOOK(val, why);
        vy = vf == &AF_L ? LG_VB - AF_L_CAP_Y : vf == &AF_M ? LG_VB + HALF_UP(AF_L_CAP_H - AF_M_CAP_H) - AF_M_CAP_Y
                                                          : LG_VB + HALF_UP(AF_L_CAP_H - AF_S_CAP_H) - AF_S_CAP_Y;
        if (vf == &AF_S && ink_w(vf, val) > COL_W - 4) {   /* (a value always fits: the lint reports one cut) */
            cv_text_fit(2, vy, vf, val, vc, T_SURF, COL_W - 4);
        } else {
            GFX_HOOK_ALIGN(0, LG_VB, COL_W, LG_VB + AF_L_CAP_H, AL_HV, "large card value centred");
            cv_text_in(0, vy, COL_W, vf, val, vc, T_SURF);
        }
    }
    if (unit[0]) {
        GFX_HOOK_ALIGN(0, 0, COL_W, 0, AL_H, "large card unit centred");
        cv_text_in(0, LG_UY, COL_W, &AF_S, unit, T_MID, T_SURF);
    }
    if (ratio >= 0) {
        int32_t gw = COL_W - 10, fx = ratio * gw / 1000;
        cv_rrect(5, LG_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
        cv_rrect(5, LG_GY, fx < 3 ? 3 : fx, 3, 1, hot ? T_ACCENT : vc == T_DIM ? T_DIM : gauge_col(ratio), T_BG);
    }
    cv_blit((uint32_t)CARD_X(c), card_y());
}

/* one card = one knob: [icon] LABEL / value unit / gauge on a SURF card, redrawn only when it changed.
 * The value is M, or S when M is too wide (engine names, long ENUMs); the unit S after it. The gauge is
 * a 3 px rounded bar: BG track, THEME fill. The knob just turned (hot): icon, label, value and gauge in
 * the accent. ratio: 0..1000 for the gauge, -1 = no gauge. icon: ICON_* (icons.c), ICON_AUTO = by label;
 * a label too long to share the card with its icon goes without it. A parameter MOTION drives (card_mot_next):
 * the motion icon in place of its own, in the theme colour; a long label moves it left, up to the card's edge;
 * a label with no room for it at all (or FELUCCA_ICONS=0): the label in the theme colour.
 * vc: the value's colour (T_THEME, T_DIM inactive, T_ACCENT the knob just turned) */
/* (0.8.5) a held layer's cards as drawn (ui_layer.c: its KNOBS level draws them as gauges) */
/* (0.8.9, the skins' mock-up: the 0.6 cards' marks) a 7 x 7 mark before a card's label, by the label (a row a byte, bit 6
 * the left column); none for the others (or a label too long to sit after one) */
static const uint8_t CARD_ICO[14][7] = {
    {0x70, 0x08, 0x04, 0x04, 0x02, 0x02, 0x01}, {0x08, 0x14, 0x14, 0x62, 0x02, 0x01, 0x01},   /* cut res */
    {0x08, 0x14, 0x23, 0x20, 0x40, 0x40, 0x40}, {0x70, 0x08, 0x04, 0x04, 0x02, 0x01, 0x01},   /* atk rel */
    {0x04, 0x0C, 0x14, 0x24, 0x44, 0x04, 0x04}, {0x41, 0x22, 0x14, 0x08, 0x14, 0x22, 0x41},   /* wave detune */
    {0x22, 0x22, 0x72, 0x27, 0x22, 0x22, 0x22}, {0x20, 0x2A, 0x6B, 0x7F, 0x5D, 0x09, 0x08},   /* mix noise */
    {0x7F, 0x41, 0x5D, 0x5D, 0x41, 0x7F, 0x00}, {0x0C, 0x0A, 0x09, 0x08, 0x38, 0x78, 0x30},   /* step note */
    {0x7F, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08}, {0x08, 0x1C, 0x3E, 0x08, 0x08, 0x08, 0x08},   /* type accent */
    {0x77, 0x41, 0x41, 0x00, 0x41, 0x41, 0x77}, {0x08, 0x08, 0x08, 0x49, 0x2A, 0x1C, 0x08}};  /* size depth */
static const struct { char l[7]; uint8_t i; } CARD_ICO_OF[] = {
    {"CUT", 0}, {"FREQ", 0}, {"RES", 1}, {"PEAK", 1}, {"Q", 1}, {"ATK", 2}, {"REL", 3}, {"DEC", 3}, {"DECAY", 3},
    {"WAVE", 4}, {"WAVE2", 4}, {"SHAPE", 4}, {"DETUNE", 5}, {"MIX", 6}, {"LEVEL", 6}, {"NOISE", 7}, {"CRUSH", 7},
    {"STEP", 8}, {"LEN", 8}, {"NOTE", 9}, {"PITCH", 9}, {"TUNE", 9}, {"ROOT", 9}, {"TYPE", 10}, {"MODE", 10},
    {"TIME", 10}, {"ACCENT", 11}, {"FLAG", 11}, {"SIZE", 12}, {"DEPTH", 13}, {"AMT", 13}};
static int32_t card_ico(const char *label)            /* its mark, -1 none */
{
    uint32_t k;
    for (k = 0; k < NELEM(CARD_ICO_OF); k++)
        if (str_eq(CARD_ICO_OF[k].l, label))
            return CARD_ICO_OF[k].i;
    return -1;
}
static void card_ico_draw(int32_t x, int32_t y, uint32_t i, uint16_t c)
{
    uint32_t r, b;
    for (r = 0; r < 7u; r++)
        for (b = 0; b < 7u; b++)
            if ((CARD_ICO[i][r] >> (6u - b)) & 1u)
                cv_pset(x + (int32_t)b, y + (int32_t)r, c);
}
static struct { int16_t r; uint8_t b; char l[12], v[16]; } ly_card[4];   /* (b: bipolar) */
/* (0.9.1) GLO > MIXER KNOB 4, the DJ FILTER (fx.c djf_block): its value as "OFF", or the cutoff ("800", "1.2k", "12k")
 * with its side for unit ("LP" / "HP"); its card draws the response (djf_curve) where the gauge would be */
#define DJF_NOCURVE 0x7FFF
static int16_t card_curve_next __attribute__((section(".bss.djf")));   /* the next draw_column's card: the curve of G_DJF
                                                         * value card_curve_next - 256 (0: none; its own section: fx.c djf) */
static void djf_format(int32_t v, char *val, const char **unit)
{
    int32_t c = djf_cut(v), i = c >> 8, h;
    if (!v) {
        str_cpy(val, "OFF", 8);
        *unit = "";
        return;
    }
    h = CUTOFF_HZ[i] + (i < 127 ? ((CUTOFF_HZ[i + 1] - CUTOFF_HZ[i]) * (c & 255)) >> 8 : 0);
    if (h < 1000)
        fmt_int(val, h);
    else if (h < 10000)
        fmt_fix(val, h / 100, 1);
    else
        fmt_int(val, h / 1000);
    if (h >= 1000)
        str_cpy(val + str_len(val), "k", 2);
    *unit = v < 0 ? "LP" : "HP";
}
/* the response of G_DJF value v drawn w px wide from (x, y): rows rows (5 or 6), 0 dB on the second (the bump above it),
 * 12 dB / oct (2 px an octave of CUTOFF_HZ's scale) down to the last row; flat at 0 dB when off. The unity line under it
 * in tc; outlined (BOTTOM, HUD: cv_halo) on BG round the band */
static void djf_curve(int32_t x, int32_t y, int32_t w, int32_t rows, int32_t v, uint16_t vc, uint16_t tc)
{
    int32_t c = v ? djf_cut(v) : 0, py = y + 1, i;
    int32_t bump = v && djf_k(c, v > 0) < (DJF_KMAX + DJF_KMIN) / 2;   /* (Q past ~0.9: the peak a row up) */
    if (cv_halo)
        cv_rect(x - 1, y - 1, w + 2, rows + 2, T_BG);
    cv_rect(x, y + 1, w, 1, tc);
    for (i = 0; i < w; i++) {
        int32_t f = i * (127 << 8) / (w - 1), d = (v < 0 ? f - c : c - f) / 256, cy;   /* (octaves below / above: 14 idx) */
        cy = !v || d < -3 ? y + 1 : d > 0 ? y + 1 + clamp(d / 7, 0, rows - 2) : y + 1 - (bump && d >= -3);
        if (i)
            cv_rect(x + i, cy < py ? cy : py, 1, (cy < py ? py - cy : cy - py) + 1, vc);
        else
            cv_rect(x, cy, 1, 1, vc);
        py = cy;
    }
}
static void draw_column(uint32_t c, const char *label, const char *val, const char *unit, uint16_t vc,
                        int32_t ratio, uint32_t icon)
{
    card_bip = (uint8_t)(ratio >= 0 && ratio == ratio_last.r && ratio_last.bip);   /* (its ratio from RATIO: bipolar?) */
    ratio_last.bip = 0;
    if (ui.layer && c < 4u) {
        ly_card[c].r = (int16_t)ratio;
        ly_card[c].b = card_bip;
        str_cpy(ly_card[c].l, label, sizeof ly_card[c].l);
        str_cpy(ly_card[c].v, val, sizeof ly_card[c].v);
        str_cpy(ly_card[c].v + str_len(ly_card[c].v), unit, sizeof ly_card[c].v - str_len(ly_card[c].v));
    }
    char key[56];
    int hot = c == ui.hot_col && ui.hot_t, named = fmt_named, mot = card_mot_next && label[0], strip, snap;
    int arm = card_arm_next, free = card_free_next;
    int32_t curve = card_curve_next ? card_curve_next - 256 : DJF_NOCURVE;
    card_curve_next = 0;
    uint32_t kind = large_kind();                       /* MENU > LARGE: tall (draw_column_tall), or the label in M */
    uint8_t sig;
    uint16_t lc = hot ? T_ACCENT : arm || mot ? T_THEME : T_MID, cb = arm ? T_TINT : CARD_BG, gk = 0;
    int low = lz_low(), hud = hud_home();
    uint32_t fd = hud_fade();                           /* (0.8.9) HUD: the row faded while the knobs rest */
    int32_t ox = hud ? CARD_X(c) - (int32_t)c * 60 : 0, cw = hud ? 60 : COL_W;   /* (HUD's HOME: the row whole, 4 x 60) */
    int32_t x, lx = 5 + ox, uw, room = COL_W - 8, ch = (int32_t)card_h(), ry = ROLL_Y;
    const aafont_t *vf = &AF_M;
    uint32_t n, kn, hk;
    int32_t kid = kc_tag(val, &kn);
    /* (0.8.9, the skins: the 0.6 colours) a lens card's value and gauge in its knob's colour (the knob turned: its card
     * framed in it, COLORS; its label TEXT, BOTTOM / HUD); the label TEXT on COLORS' cards, MID in the knob row. (0.9.5)
     * JIANT: the knob's colour its value's heat (knob_col), the value lifted a fifth toward the text, as the 0.6's */
    if (lz_on() && kind != LK_TALL) {
        if (kid < 0 && (vc == T_THEME || vc == T_ACCENT)) {
            gk = knob_col(c, ratio, card_bip);          /* (the gauge's, the frame's) */
            vc = heat_on(ratio) ? ux_mix(gk, T_TEXT, 20) : gk;
        }
        lc = arm || mot ? T_THEME : low && !hot && !hud ? T_MID : T_TEXT;   /* (0.9.0 HUD's HOME: TEXT, over the beings) */
    }
    if (!gk)
        vc = value_col(vc, ratio);
    if (fd < 100u) {
        vc = ux_mix(T_BG, vc, (int32_t)fd);
        lc = ux_mix(T_BG, lc, (int32_t)fd);
        if (gk)
            gk = ux_mix(T_BG, gk, (int32_t)fd);
    }
    fmt_named = 0;                                      /* (params.c: a name, for this card only) */
    card_mot_next = 0;                                  /* (the same: MOTION, this card only) */
    card_arm_next = card_free_next = 0;
    card_mot = (uint8_t)((card_mot & ~(1u << c)) | (uint32_t)mot << c);
    if (str_eq(unit, label))
        unit = "";                                      /* "BPM 124 BPM", "USB OFF USB": the label says it */
    (void)icon;                                         /* (0.8.5, the mock-up: no icons on the cards) */
    str_cpy(key, label, 12);                            /* cache key: texts + colour + gauge */
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), val, 14);
    str_cpy(key + str_len(key), "|", 2);
    str_cpy(key + str_len(key), unit, 8);
    n = str_len(key);
    /* every RGB565 bit: status colours can change with identical text */
    key[n] = (char)('A' + (vc & 15u));
    key[n + 1] = (char)('A' + ((vc >> 4) & 15u));
    key[n + 2] = (char)('A' + ((vc >> 8) & 15u));
    key[n + 3] = (char)('A' + (vc >> 12));
    key[n + 4] = (char)(' ' + (ratio < 0 ? 0 : 1 + clamp(ratio, 0, 1000) * (COL_W - 10) / 1000));   /* (the gauge's px) */
    key[n + 5] = (char)('!' + arm + 2 * (int)lz_on());
    key[n + 6] = (char)('0' + hot + 2 * mot + 4 * (int)kind);
    {   /* (0.6.10) the gauge's colour too */
        uint16_t gc = ratio < 0 ? 0u : gk ? gk : gauge_col(ratio);
        key[n + 7] = (char)('A' + (gc & 15u));
        key[n + 8] = (char)('A' + ((gc >> 4) & 15u));
        key[n + 9] = (char)('A' + ((gc >> 8) & 15u));
        key[n + 10] = (char)('A' + (gc >> 12));
    }
    key[n + 11] = (char)('A' + fd / 13u + 8u * (uint32_t)low);
    key[n + 12] = (char)(curve == DJF_NOCURVE ? '#' : 0x80 | ((curve + 64) & 127));   /* (0.9.1) the DJ FILTER's curve */
    key[n + 13] = 0;
    uw = unit[0] ? text_w(&AF_S, unit) + 3 : 0;
    if (text_w(vf, val) + uw > room)
        vf = &AF_S;
    sig = (uint8_t)str_hash(str_hash(song.sel + TSEL->eng_req * 4u + ux.gen * 64u + lz_on() * 16u, label), unit);
    snap = ui.force || sig != ui.roll[c].sig;           /* what the value is of: label, unit, track, engine, palette */
    if (kind == LK_TALL)
        ui.roll[c].from[0] = 0;                         /* (a tall card: no rolling digits) */
    strip = !snap && !hud && str_eq(key, ui.col[c]);
    if (strip && !ui.roll[c].from[0])
        return;
    if (strip) {                                        /* rolling: the value strip only */
        cv_begin(COL_W, ROLL_H, cb);
        cv_oy = -ry;
        if (lz_on() && !low && hot && vc != T_DIM) {    /* (0.8.9 COLORS: the hot card's frame, its sides) */
            uint16_t fc = ux_mix(cb, gk ? gk : T_K(c), 70);
            cv_rect(0, ry, 1, ROLL_H, fc);
            cv_rect(COL_W - 1, ry, 1, ROLL_H, fc);
        }
    } else {
        char ov[16];                                    /* the value drawn before (in the cache key) */
        const char *k = ui.col[c];
        uint32_t i = 0;
        while (*k && *k != '|')
            k++;
        while (*k && k[1] && k[1] != '|' && i + 1u < sizeof ov)
            ov[i++] = *++k;
        ov[i] = 0;
        ui.roll[c].sig = sig;
        if (!str_eq(ov, val))
            roll_note(c, ov, val, snap || named || vf != &AF_M || kid >= 0 || kind == LK_TALL);
        else if (snap)
            ui.roll[c].from[0] = 0;
        str_cpy(ui.col[c], key, sizeof ui.col[c]);
        if (kind == LK_TALL) {
            draw_column_tall(c, label, val, unit, vc, ratio, icon, hot, mot, kid);
            return;
        }
        if (low) {                                      /* (0.8.9) BOTTOM, HUD: no box (HUD's HOME: on the beings) */
            lz_begin((uint32_t)cw, (uint32_t)ch, hud ? c * 60u : (uint32_t)CARD_X(c), card_y());
            if (fd < 100u)                              /* (0.9.0) faded: no outline (it cut holes in the beings) */
                cv_halo = 0;
        } else {
            cv_begin(COL_W, (uint32_t)ch, T_BG);
            if (!lz_on())
                cv_rect(0, 0, COL_W, ch, cb);           /* (0.8.5, the mock-up: a square box) */
            else if (hot && vc != T_DIM) {              /* (0.8.9) COLORS: a rounded card, the knob turned framed in its colour */
                uint16_t fc = ux_mix(cb, gk ? gk : T_K(c), 70);
                cv_rrect(0, 0, COL_W, ch, 5, fc, T_BG);
                cv_rrect(1, 1, COL_W - 2, ch - 2, 4, cb, fc);
            } else
                cv_rrect(0, 0, COL_W, ch, 5, cb, T_BG);
        }
    }
    if (label[0] || val[0]) {
        cv_scroll = (uint8_t)strip;                     /* (0.9.0) rolling: the label again where it reaches into the value's
                                                         * strip (MONO's chip, LARGE's M labels), clipped to it */
        if (label[0]) {
            /* the label (S, MID) at the top left, its capitals rows 4..12; MENU > LARGE on a list / graph page, a lens
             * page or a layer (LK_LABEL): in M where it fits the card (capitals rows 4..14), else in S. A parameter
             * MOTION drives, an armed action: THEME */
            const aafont_t *lf = kind == LK_LABEL && 5 + text_w(&AF_M, label) <= COL_W - 2 ? &AF_M : lz_on() ? &AF_X : &AF_S;
            int32_t ly = lf == &AF_M ? 0 : lf == &AF_X ? CARD_XY - low : 1;   /* (0.8.5 a lens card: AF_X, capitals rows 5..11;
                                                         * 0.8.9 the knob row: 4..10) */
            int32_t cy = ly + (lf == &AF_M ? AF_M_CAP_Y : lf == &AF_X ? AF_X_CAP_Y : AF_S_CAP_Y);
            int32_t chh = lf == &AF_M ? AF_M_CAP_H - 1 : lf == &AF_X ? AF_X_CAP_H : AF_S_CAP_H;   /* (0.8.9 M: Inter Tight's
                                                         * figures reach a row higher: the chip a row shorter) */
            if (hot && T_ACCENT == T_MID) {             /* MONO: no colour tells the hot knob: its label inverted, */
                int32_t b[4], w;                        /* the chip 2 px round its ink */
                text_ink(lf, label, b);
                w = b[2] - b[0] + 4;
                if (w > COL_W + 1 - lx - b[0]) w = COL_W + 1 - lx - b[0];
                cv_rrect(lx + b[0] - 2, cy - 2, w, chh + 4, 2, T_ACCENT, cb);
                GFX_HOOK_ALIGN(lx + b[0] - 2, cy - 2, lx + b[0] - 2 + w, cy + chh + 2, AL_HV, "card label chip (MONO hot)");
                cv_text_fit(lx, ly, lf, label, T_INK, T_ACCENT, COL_W - 2 - 5);
            } else {
                int32_t ic = lf == &AF_X && lz_on() && kind != LK_TALL ? card_ico(label) : -1;
                if (ic >= 0 && 10 + text_w(lf, label) <= COL_W - 2 - 5) {   /* (0.8.9) its mark, then the label */
                    for (hk = 0; cv_halo && hk < HALO_N; hk++)   /* (0.9.0: outlined as the words, 2 px) */
                        card_ico_draw(lx + HALO_X[hk], cy + HALO_Y[hk], (uint32_t)ic, T_BG);
                    card_ico_draw(lx, cy, (uint32_t)ic, low ? (hot ? vc : lc) : lc);
                    cv_text_fit(lx + 10, ly, lf, label, lc, cb, COL_W - 2 - 15);
                } else
                    cv_text_fit(lx, ly, lf, label, lc, cb, COL_W - 2 - 5);
            }
        }
        cv_scroll = 0;
        if (kid >= 0)                                   /* "[OCT+]": the keycap (accent: it would act; DIM: it would not) */
            x = cv_keycap(5 + ox, ry + 2, (uint32_t)kid, vc == T_ACCENT || vc == T_DIM ? vc : T_KEY, T_INK, cb);
        else if (vf == &AF_M) {
            if (!ui.roll[c].from[0] && val[0])
                GFX_HOOK_ALIGN(0, ry, cw, ry + ROLL_H, AL_V, "card value in its roll window");
            x = roll_text(c, 5 + ox, ry - 1, val, vc);
        } else if (free)                                /* (a name: ellipsised where it must be) */
            x = cv_free_text(5 + ox, ry + 2, vf, val, vc, cb, room - uw);
        else
            x = cv_text_fit(5 + ox, ry + 2, vf, val, vc, cb, room - uw);
        if (unit[0]) {
            GFX_HOOK_ALIGN(0, 0, 0, vf == &AF_M ? ry - 1 + AF_M.asc : ry + 2 + AF_S.asc, AL_B, "card unit on its value's baseline");
            uint16_t uc = hud ? T_TEXT : T_MID;         /* (0.9.0 HUD's HOME: TEXT) */
            cv_text_on(x + 3, ry + 2, &AF_S, unit, fd < 100u ? ux_mix(T_BG, uc, (int32_t)fd) : uc, cb);   /* on the value's
                                                         * baseline (S) */
        }
        if (!strip && curve != DJF_NOCURVE && kind != LK_TALL) {   /* (0.9.1) the DJ FILTER: its response (BOTTOM, HUD:
                                                         * 5 rows, under the value's strip) */
            uint16_t gc = hot && !lz_on() ? T_ACCENT : vc == T_DIM ? T_DIM : lz_on() ? (gk ? gk : vc) : gauge_col(ratio);
            uint16_t tc = fd < 100u ? ux_mix(T_BG, T_LINE, (int32_t)fd) : T_LINE;
            djf_curve(5 + ox, low ? ch - 5 : ch - 8, COL_W - 10, low ? 5 : 6, curve,
                      curve ? gc : fd < 100u ? ux_mix(T_BG, T_MID, (int32_t)fd) : T_MID, tc);
        } else if (!strip && ratio >= 0) {              /* the gauge: a 2 px LINE track, the value THEME on it */
            int32_t gw = COL_W - 10, fx = ratio * gw / 1000;
            uint16_t gc = hot && !lz_on() ? T_ACCENT : vc == T_DIM ? T_DIM : lz_on() ? (gk ? gk : vc) : gauge_col(ratio);
            uint16_t tc = fd < 100u ? ux_mix(T_BG, T_LINE, (int32_t)fd) : T_LINE;
            if (low) {                                  /* (0.8.9) BOTTOM, HUD: a hairline, the value 2 px on it */
                if (cv_halo) {                          /* (0.9.0) its outline 1 px round the line and the bar, not a band */
                    cv_rect(4 + ox, ch - 5, gw + 2, 3, T_BG);
                    cv_rect(4 + ox, ch - 6, (fx < 2 ? 2 : fx) + 2, 4, T_BG);
                }
                cv_rect(5 + ox, ch - 4, gw, 1, tc);
                cv_rect(5 + ox, ch - 5, fx < 2 ? 2 : fx, 2, gc);
            } else if (lz_on() && kind != LK_TALL) {   /* COLORS: 3 px, rounded, as the 0.6's */
                cv_rrect(5, ch - 6, gw, 3, 1, T_LINE, cb);
                cv_rrect(5, ch - 6, fx < 3 ? 3 : fx, 3, 1, gc, T_LINE);
            } else {
                cv_rect(5, ch - 5, gw, 2, T_LINE);
                cv_rect(5, ch - 5, fx < 2 ? 2 : fx, 2, gc);
            }
        }
    }
    cv_oy = 0;
    if (low && !strip)
        lz_blit(hud ? c * 60u : (uint32_t)CARD_X(c), card_y());
    else
        cv_blit((uint32_t)CARD_X(c), card_y() + (strip ? (uint32_t)ry : 0u));
}

/* Gauge cards without units share their constant renderer arguments. */
static __attribute__((noinline)) void gauge_column(uint32_t c, const char *label, const char *value, uint16_t color, int32_t ratio)
{
    draw_column(c, label, value, "", color, ratio, ICON_NONE);
}

static __attribute__((noinline)) void hot_gauge_column(uint32_t c, const char *label, const char *value, int32_t ratio)
{
    gauge_column(c, label, value, VAL(c), ratio);
}

static __attribute__((noinline)) void unit_column(uint32_t c, const char *label, const char *value, const char *unit, uint16_t color)
{
    draw_column(c, label, value, unit, color, -1, ICON_NONE);
}

static __attribute__((noinline)) void hot_unit_column(uint32_t c, const char *label, const char *value, const char *unit)
{
    unit_column(c, label, value, unit, VAL(c));
}

/* Cards without units or a gauge share argument setup. Icons are unused by draw_column. */
static __attribute__((noinline)) void text_column(uint32_t c, const char *label, const char *value, uint16_t color)
{
    gauge_column(c, label, value, color, -1);
}

static __attribute__((noinline)) void hot_text_column(uint32_t c, const char *label, const char *value)
{
    text_column(c, label, value, VAL(c));
}

/* Shared empty card: keep the renderer and all of its state-reset behavior. */
static __attribute__((noinline)) void empty_column(uint32_t c)
{
    text_column(c, "", "", T_THEME);
}

/* an action's column (act_cols): picked, the OCT+ keycap (accent while it would do something); else "--" */
static void draw_act_column(uint32_t c, const char *label, uint16_t vc, uint32_t icon)   /* icon: ICON_AUTO = by label */
{
    int picked = act_col() == c + 1u;
    card_arm_next = (uint8_t)picked;                    /* (0.8.5, the mock-up: the armed card TINT, OCT+ its value) */
    text_column(c, label, picked ? "OCT+" : "", picked ? (act_ready() ? T_TEXT : T_DIM) : vc);
}

/* action pages: the footer's first row says what OCT+ and OCT- do ("OCT+ LOAD   OCT- BACK") */
static void foot_hint(char *a, char *b)
{
    uint32_t c = act_col();
    str_cpy(a, "OCT+ ", 8);
    str_cpy(a + 5, c ? act_name(c - 1u) : "--", 8);
    str_cpy(b, ui.act && cur_page()->graph != GR_PATS ? "OCT- CANCEL" : "OCT- BACK", 16);
}

/* the sound's name on the track: a user preset or the engine's preset (b holds 16) */
static void sound_name(const track_t *t, char *b)
{
    const engine_t *e = trk_engp(t);
    b[0] = 0;
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 16);
}

/* (0.9.2, rule 6) the tabs on top, as the approved mock-up (navegacion.html): the section's groups (the menu's tabs, a
 * layer's MAP KNOBS STATE, a dialog's three), the one shown lit, under each name a dot a page (the page shown: its dot
 * marked). COLORS: n rounded tabs (r 4) in a row, x 4 .. 236, 2 apart (three: 76 wide at x 4 + 78 l, as 0.8.5's), y 26,
 * 13 tall; the group shown THEME with INK text, the others RAISE with MID; the name (its short form where it does not
 * fit) centred, capitals rows 29..35; the dots on row 37, 2 x 1, 4 apart (INK on the lit tab, the others a mix; DIM on the
 * rest). BOTTOM and HUD (the mock-up opciones_skin.html): the names centred in n equal slots (capitals rows 28..34), the
 * one shown THEME, the others MID; the dots on row 38 (DIM, the group shown MID), the page's BOTTOM a THEME pill (5 x 3,
 * rows 37..39), HUD a THEME dash (4 x 1); HUD's HOME outlined, over the beings. Drawn when what it shows changes */
#define LT_W 76
#define LT_X(l) (4 + (int32_t)(l) * 78)
#define TAB_X(i, n) (4 + (int32_t)(i) * 234 / (int32_t)(n))   /* tab i of n (COLORS): from here, TAB_X(i + 1) - 2 wide */
static void tabs_paint(uint32_t n, uint32_t lv, const char *const *nm, const char *const *tab, const uint8_t *cnt, uint32_t at)
{
    uint32_t l, d;
    int low = lz_low() || (ux.skin && (dlg_on() || name_on()) && !(ui_prefs & PREF_LARGE)), hud = hud_home();   /* (0.9.0:
                                                         * NAME too, as the dialogs) */
    int32_t y = hud ? H_HEAD : LT_Y, h = hud ? (int32_t)(card_y() < graph_y() ? card_y() : graph_y()) - H_HEAD : low ? LB_TAB_H : LT_H;
    int32_t o = LT_Y - y;                               /* (0.9.7 AIR's HOME: down to its knobs' row) */
    lz_begin(240, (uint32_t)h, 0, (uint32_t)y);
    for (l = 0; l < n; l++) {
        int32_t x = low ? (int32_t)(l * 240u / n) + 2 : TAB_X(l, n), w = low ? (int32_t)((l + 1u) * 240u / n) - 2 - x :
                    TAB_X(l + 1u, n) - 2 - x, nd = cnt ? cnt[l] : 0, dx = x + HALF_UP(w - (nd * 4 - 2)), dy;
        const char *s = nm[l];
        if (tab && tab[l] && text_w(&AF_X, s) > w - 4)
            s = tab[l];
        if (low) {
            GFX_HOOK_ALIGN(x, 0, x + w, 0, AL_H, "group tab word centred on its slot");
            cv_text_in(x, o + 2 - AF_X_CAP_Y, w, &AF_X, s, l == lv ? T_THEME : hud_cap(T_MID), T_BG);
            dy = o + 12;
            if (cv_halo && nd)
                cv_rect(dx - 1, dy - 1, nd * 4, 3, T_BG);
            for (d = 0; d < (uint32_t)nd; d++) {
                int32_t px = dx + (int32_t)d * 4;
                if (l == lv && d == at) {
                    if (ux.skin == SKIN_BOTTOM)
                        cv_rrect(px - 1, dy - 1, 5, 3, -1, T_THEME, T_BG);
                    else
                        cv_rect(px - 1, dy, 4, 1, T_THEME);
                } else
                    cv_rect(px, dy, 2, 1, l == lv ? hud_cap(T_MID) : T_DIM);
            }
            continue;
        }
        {
            uint16_t bg = l == lv ? T_THEME : T_RAISE;
            cv_rrect(x, 0, w, LT_H, 4, bg, T_BG);
            GFX_HOOK_ALIGN(x, 0, x + w, LT_H, AL_HV, "group tab name centred in its tab");
            cv_text_in(x, CAP_IN(X, LT_H), w, &AF_X, s, l == lv ? T_INK : T_MID, bg);
            for (d = 0; d < (uint32_t)nd; d++)
                cv_rect(dx + (int32_t)d * 4, LT_H - 2, 2, 1, l != lv ? T_DIM : d == at ? T_INK : ux_mix(T_THEME, T_INK, 40));
        }
    }
    lz_blit(0, (uint32_t)y);
}
/* what the tabs show (lz_on): the groups' names (short forms), each one's pages, the one lit, its page; how many */
static uint32_t tabs_now(const char **nm, const char **tab, uint8_t *cnt, uint32_t *lv, uint32_t *at)
{
    uint32_t f = lz_fam(), n, k, i, m;
    uint8_t gi[8];
    if (f == LZ_MENU) {                                 /* the menu: its four tabs, a dot a row */
        for (k = 0; k < MTAB_COUNT; k++) {
            nm[k] = MTAB_NAME[k];
            tab[k] = 0;
            cnt[k] = (uint8_t)mtab_rows(k);
        }
        *lv = menu_tab();
        *at = ui.menu_sel - mtab_first(*lv);
        return MTAB_COUNT;
    }
    if (f == LZ_LAYER) {                                /* a layer: its views (ALGORITHM), no dots */
        static const char *const LLV[3] = {"MAP", "KNOBS", "STATE"};
        for (k = 0; k < 3u; k++) {
            nm[k] = LLV[k];
            tab[k] = 0;
            cnt[k] = 0;
        }
        *lv = ui.lylv % 3u;
        *at = 0;
        return 3;
    }
    n = nav_groups(cnt, gi, lv, at, &i, &m);
    for (k = 0; k < n; k++) {
        nm[k] = nav_group_name(gi[k]);
        tab[k] = nav_group_tab(gi[k]);
    }
    return n;
}
static void draw_tabs(void)
{
    static uint32_t drawn;
    const char *nm[8], *tab[8];
    uint8_t cnt[8];
    uint32_t lv, at, n = tabs_now(nm, tab, cnt, &lv, &at), k,
             sig = (lv + 1u) * 2654435761u + at * 40503u + ux.gen * 977u + ux.style * 7u + lz_fam() * 131u + 1u +
                   (uint32_t)hud_home() * 31u + n * 7919u;
    for (k = 0; k < n; k++)
        sig = (sig ^ str_hash(cnt[k], nm[k])) * 16777619u;
    if (!ui.force && sig == drawn)
        return;
    drawn = sig;
    tabs_paint(n, lv, nm, tab, cnt, at);
}
/* (0.9.2, rule 6) the footer's breadcrumb, as the mock-up's: small, DIM, from x 6, its parts apart by " . ": SECTION .
 * GROUP . page ("FX . DELAY . DLY 2"; the menu "MENU . DISPLAY . COLOR", a layer "FX . MAP"), the group THEME, the page
 * TEXT (`last`: a flash TEXT; the DRUM grid with ACC held: ACCENT, "ACCENT"); its place n/N in the chain at the right */
static const char *const FAM_CRUMB[FAM_COUNT] = {"HOME", "ENV", "LFO", "FX", "SCL", "EDIT", "GLO", "SAVE", "ARP", "SEQ",
                                                 "GLO"};
static __attribute__((noinline)) void foot_crumbs(char *b, uint32_t n)   /* b holds n (48) */
{
    char ti[24];
    uint32_t f;
    if (!lz_on() && ui.home) {
        str_cpy(b, "HOME", n);
        return;
    }
    if (!lz_on()) {
        page_title(ti);
        str_cpy(b, FAM_CRUMB[cur_page()->fam % FAM_COUNT], n);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        str_cpy(b + str_len(b), ti, 20);
        return;
    }
    f = lz_fam();
    if (f == LZ_MENU) {
        str_cpy(b, "MENU \xB7 ", n);
        str_cpy(b + str_len(b), MTAB_NAME[menu_tab()], 12);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        str_cpy(b + str_len(b), MI_NAME[ui.menu_sel % MI_COUNT], 16);
        return;
    }
    if (f == LZ_LAYER) {
        static const char *const LN[LAYER_N] = {"", "FX", "GLO", "SCL", "EDIT", "SEQ", "REC", "SAVE", "LFO"};
        static const char *const LLV[3] = {"MAP", "KNOBS", "STATE"};
        str_cpy(b, LN[ui.layer % LAYER_N], n);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        str_cpy(b + str_len(b), LLV[ui.lylv % 3u], 8);
        return;
    }
    {
        uint8_t cnt[8], gi[8];
        uint32_t g, p, i, m;
        (void)nav_groups(cnt, gi, &g, &p, &i, &m);
        str_cpy(b, SEC_NAME[nav_sec()], n);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        str_cpy(b + str_len(b), nav_group_name(gi[g]), 12);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        if (grid_on() && black_held(GK_ACC))
            str_cpy(ti, "ACCENT", sizeof ti);
        else
            str_cpy(ti, nav_page_name(), sizeof ti);
        str_cpy(b + str_len(b), ti, n - str_len(b));
    }
}
#define CRUMB_Y 4                                       /* (0.8.5) a lens footer's breadcrumb (AF_X): capitals rows 5..11 */
/* the page's place in its chain at the footer's right, "6/16" (the menu: the row among all; a layer: its view) */
static int32_t crumb_count(char *b)
{
    uint32_t k, n;
    b[0] = 0;
    if (!lz_on())
        return 0;
    if (lz_fam() == LZ_MENU) {
        k = ui.menu_sel % MI_COUNT;
        n = MI_COUNT;
    } else if (lz_fam() == LZ_LAYER) {
        k = ui.lylv % 3u;
        n = 3;
    } else {
        uint8_t cnt[8], gi[8];
        uint32_t g, p;
        (void)nav_groups(cnt, gi, &g, &p, &k, &n);
    }
    if (n < 2u)
        return 0;
    fmt_frac(b, (int32_t)k + 1, (int32_t)n);
    return text_w(&AF_X, b);
}
/* (0.9.9-seq2g: the 0.6's footer back, as the owner asked) the 16 steps of the bar the cursor is in, on every page: a step
 * with a note (DRUM: any lane hitting there) a lit bar (THEME; accented ACCENT), an empty one a stub (a tie MID), under
 * them the step sounding (TEXT; it moves a step at a time: a function of the step, never of the frame) and in SEQ the
 * step edited (ACCENT, over it); the bars of the pattern as tiny 3 x 3 dots at the strip's right end (the one shown TEXT,
 * the one playing THEME when it is another). The normal footer: row 1, x 12 .. 228 as 0.6's (the dots in a column at
 * x 233); a lens footer (one row of 15 px): a compact strip before the breadcrumb, which shortens to the page's name */
static uint32_t foot_ph(const track_t *t, uint32_t *len)
{
    *len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u;
    return song.playing ? seq_src(t, t->seq_idx) : 0xFFu;
}
static uint32_t foot_strip_sig(void)
{
    const track_t *t = TSEL;
    uint32_t len, ph = foot_ph(t, &len), bk = ui.bank;
    return steps_hash(t) + (len * 131u + bk) * 2654435761u + (ph / 16u == bk ? ph % 16u + 1u : 0u) * 40503u +
           (ph < len ? ph / 16u + 1u : 0u) * 977u + (song.seq_mode ? ui.cursor + 1u : 0u) * 3001u;
}
static int32_t foot_strip(int32_t x0, int32_t y, int lens)
{
    const track_t *t = TSEL;
    uint32_t len, ph = foot_ph(t, &len), nb = (len + 15u) / 16u, i;
    int32_t w = lens ? 4 : 9, hb = lens ? 7 : 11, my = lens ? y + 9 : y + 13, x = x0;
    for (i = 0; i < 16u; i++) {
        uint32_t si = ui.bank * 16u + i;
        const step_t *st = &seq_steps(t)[si];
        x = lens ? x0 + (int32_t)i * 5 + (int32_t)(i / 4u) * 2 : 12 + (int32_t)i * 13 + (int32_t)(i / 4u) * 4;
        if (si >= len)
            continue;
        if (step_on(st))
            cv_rect(x, y, w, hb, (st->flags & SF_ACCENT) || st->acc ? T_ACCENT : T_THEME);
        else
            cv_rect(x, y + hb - 3, w, 3, st->time == ST_TIE ? T_MID : T_RAISE);
        if (song.seq_mode && si == ui.cursor)
            cv_rect(x, my, w, 2, T_ACCENT);
        else if (si == ph)
            cv_rect(x, my, w, 2, T_TEXT);
    }
    x += w + 4;
    for (i = 0; nb > 1u && i < nb; i++) {
        uint16_t c = i == ui.bank ? T_TEXT : ph < len && i == ph / 16u ? T_THEME : T_RAISE;
        if (lens)
            cv_rect(x + (int32_t)i * 4, y + 2, 3, 3, c);
        else
            cv_rect(233, y + (int32_t)i * 4, 3, 3, c);
    }
    return lens ? x + (nb > 1u ? (int32_t)nb * 4 + 3 : 0) : x;
}
/* (0.9.9-seq2h) SEQ's clipboard holding n steps: "[n]" at the footer's right, before the count (ui_input.c cp_copy) */
static int clip_tag(char *b)
{
    if (!clip.n || ui.home || ui.menu || !song.seq_mode)
        return 0;
    b[0] = '[';
    fmt_int(b + 1, clip.n);
    str_cpy(b + str_len(b), "]", 2);
    return 1;
}
static __attribute__((noinline)) void draw_crumbs(int32_t y, uint16_t fg, uint16_t last)
{
    char cnt[8], ct[8];
    int32_t cw = crumb_count(cnt), mx = cw ? 234 - cw - 8 : 234;
    char b[48];
    uint32_t i, k = 0;
    int32_t x, x0;
    const aafont_t *f = lz_on() ? &AF_X : &AF_S;        /* (0.8.5: a lens page, the mock-up's small mono) */
    x0 = f == &AF_X ? foot_strip(6, y + AF_X_CAP_Y, 1) : 8;   /* (seq2g, a lens footer: the steps first, on the capitals) */
    foot_crumbs(b, sizeof b);
    for (i = 0; b[i]; i++)                              /* (the last part: after the last dot) */
        if ((uint8_t)b[i] == 0xB7u)
            k = i + 2u;
    if (k > str_len(b))
        k = 0;
    if (f == &AF_X)
        GFX_HOOK_ALIGN(0, y + AF_X_CAP_Y, 0, y + AF_X_CAP_Y + AF_X_CAP_H, AL_V, "footer breadcrumb on its line");
    else
        GFX_HOOK_ALIGN(0, y + AF_S_CAP_Y, 0, y + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "footer breadcrumb on its line");
    if (cw)
        cv_text_r(234, y, f, cnt, f == &AF_X ? T_TEXT : fg, T_BG);   /* (0.8.9, the skins' mock-up: the count TEXT) */
    if (clip_tag(ct))
        mx = cv_text_r(mx, y, f, ct, T_THEME, T_BG) - 6;   /* (seq2h) the clipboard's [n] */
    if (f == &AF_X && k) {                              /* (seq2g) a lens footer: the page's name alone after the steps
                                                         * (TEXT, as `last` if that is not DIM) */
        cv_free_text(x0, y, f, b + k, last == fg ? T_TEXT : last, T_BG, mx - x0);
        return;
    }
    if (last == fg || !k) {
        cv_free_text(x0, y, f, b, fg, T_BG, mx - x0);
        return;
    }
    {
        char c = b[k];
        b[k] = 0;
        x = cv_text_on(x0, y, f, b, fg, T_BG);
        b[k] = c;
        cv_free_text(x, y, f, b + k, last, T_BG, mx - x);
    }
}
static void draw_foot(void)
{
    char s[48], ti[20];
    const track_t *t = TSEL;
    uint32_t sig;
    int lz = (int)lz_on(), accw = grid_on() && black_held(GK_ACC);
    int32_t ry = lz ? CRUMB_Y + LZ_FOOT_Y - (int32_t)foot_y() : 19;   /* the breadcrumb's line (a lens page: the footer is it) */
    foot_crumbs(s, sizeof s);
    if (ui.home)
        str_cpy(ti, "HOME", sizeof ti);
    else
        page_title(ti);
    sig = str_hash(0x9E3779B9u, s) + str_hash(7u, ti) + (uint32_t)lz * 104729u + foot_strip_sig();   /* (seq2g) the steps */
    if (act_cols() && !lz) {                           /* the hint, and whether OCT+ would act */
        char ha[16], hb[16];
        foot_hint(ha, hb);
        sig += str_hash(str_hash(act_ready() ? 7u : 3u, ha), hb);
    }
    sig += (uint32_t)accw * 977u + (uint32_t)clip.n * 7919u;   /* (seq2h) the clipboard's [n] */
    if (!ui.force && sig == ui.foot_sig)
        return;
    ui.foot_sig = sig;
    lz_begin(240, foot_h(), 0, foot_y());
    if (lz) {
        ;                                             /* (0.8.5) a lens page: the breadcrumb alone, as the mock-up */
    } else if (act_cols()) {                          /* row 1: the OCT+ / OCT- hint in place of the steps */
        char ha[16], hb[16];
        khint_t kh[2];
        foot_hint(ha, hb);                            /* "OCT+ LOAD", "OCT- BACK": keycaps and their words */
        kh[0] = (khint_t){KC_OCTUP, ha + 5};
        kh[1] = (khint_t){KC_OCTDN, hb + 5};
        cv_key_row(8, 232, 2, kh, 2, act_ready() ? 3u : 2u, T_BG);
    } else {                                          /* row 1: the cursor's bar, 16 steps in 4 groups (seq2g, 0.6's) */
        if ((ui.bank + 1u) * 16u <= (uint32_t)t->p[P_SLEN])
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS | AL_N(16), "footer step bars centred");
        (void)foot_strip(12, 1, 0);
    }
    draw_crumbs(ry, hud_cap(T_DIM), accw ? T_ACCENT : hud_cap(T_DIM));   /* (0.9.0 HUD's HOME: TEXT, over the beings) */   /* (0.8.5) row 2: the breadcrumb (the track's engine and sound:
                                                         * the header's centre) */
    lz_blit(0, foot_y());
}
/* the EDIT layer's cards (ui_layer.c; 0.9.9-seq2f): ENG, No. (the engine's sounds: PRESETS' list, ui.c eng_list_pos),
 * FAV (PRESETS' KNOB 3 marks it), an empty card. KNOB 1..4 are MOMENTARY there (ui_layer.c mom_knob); SELECT the dice */
static void engine_columns(void)
{
    uint32_t total, cur = eng_list_pos(&total), e = trk_eng(TSEL);
    char val[8], u[8];
    fmt_int(val, (int32_t)cur + 1);
    str_cpy(u, "/", 8);
    fmt_int(u + 1, (int32_t)total);
    if (cur + 1u >= 100u && total >= 100u) u[0] = 0;
    draw_column(0, "ENG", ENGINES[e]->name, "", VAL(0u), (int32_t)eng_rank(e) * 1000 / (NENG_SHOWN > 1 ? NENG_SHOWN - 1 : 1),
                engine_icon(ENGINES[e]->name));
    draw_column(1, "No.", val, u, VAL(1u), total > 1u ? (int32_t)(cur * 1000u / (total - 1u)) : 0, ICON_NONE);
    hot_text_column(2, "FAV", preset_favorite() ? "ON" : "OFF");
    empty_column(3);
}
/* (0.9.9-seq2d / seq2f) the step components' cards: GLO > COMPS / COMPS 2, SEQ > COMP / COMP 2. (gen3) Its own function,
 * out of draw_columns: new code lands past the pinned sound (tools/build.py), draw_columns' run before it stays as long */
/* (gen3) KNOB 4's card: ARP's ORDER DOWN UP-DN as DN UD, the RATE beside them (here: comp_vname's code pinned in its run,
 * its ORD ui_input.c ENT_ORD's twin, one table) */
static void comp_value(const step_t *s, uint32_t a, uint32_t v, char *lb, char *val, char *un)
{
    comp_vname(TSEL, s, a, v, lb, val, un);
    if (a == CA_ARP && (val[0] == 'D' || val[2] == '-'))
        val[1] = val[0] == 'D' ? 'N' : 'D', val[2] = 0;
}
static __attribute__((noinline)) void comp_columns(void)
{
    char val[12];
    if (cur_page()->graph == GR_GCOMP) {               /* (0.9.9-seq2f) GLO > COMPS: the lane's step; COMPS 2 its tracks */
        const gcomp_t *gc = &glane[gl_cur % GL_N];
        uint32_t a = CA_VIEW(CO_ACT(gc->comp), gc->val);
        char lb[10], un[8];
        if (gco_slot()) {
            fmt_int(val, (int32_t)gl_cur + 1);
            hot_text_column(0, "STEP", val);
            val[0] = 'T'; fmt_int(val + 1, (int32_t)gl_trk + 1);
            hot_text_column(1, "TRACK", val);
            text_column(2, "APPLIES", (gc->mask >> gl_trk) & 1u ? "ON" : "OFF", a ? VAL(2u) : T_DIM);
            hot_text_column(3, "MODE", gl_mono ? "MONO" : "POLY");
            return;
        }
        text_column(0, "ACTION", CA_NAMES[a], a ? VAL(0u) : T_DIM);
        if (a) {
            fmt_int(val, (int32_t)gc->len + 1);
            hot_unit_column(1, "LENGTH", val, "ST");   /* (gen3: "32 ST" in M) */
        } else
            text_column(1, "LENGTH", "-", T_DIM);
        if (a) {
            cond_name(CO_COND(gc->comp), gc->nm, val);
            text_column(2, "COND", val, CO_COND(gc->comp) ? VAL(2u) : T_DIM);   /* (gen3: MONO / POLY on
                                                         * COMPS 2, the value whole) */
            comp_value(TSEL->step, a, gc->val, lb, val, un);
            unit_column(3, lb, val, un, lb[0] ? VAL(3u) : T_DIM);
        } else {
            text_column(2, "COND", "", T_DIM);
            empty_column(3);
        }
        return;
    }
    {                                                   /* (seq2d) the cursor step's C1 / C2: ACTION LENGTH COND VALUE */
        const step_t *cs = &TSEL->step[ui.cursor];
        uint32_t a = comp_view(), k = comp_slot(), c = CO_COND(k ? cs->comp2 : cs->comp), l = cs->clen[k & 1u];
        char lb[10], un[8];
        text_column(0, k ? "ACTION 2" : "ACTION", CA_NAMES[a], a ? VAL(0u) : T_DIM);
        if (a) {
            fmt_int(val, (int32_t)(l < CL_MAX ? l + 1u : 1u));
            hot_unit_column(1, "LENGTH", val, a == CA_RAMP && (comp_arg(cs) & CV_RAMP_BARS) ? "BAR" : "ST");
        } else
            text_column(1, "LENGTH", "-", T_DIM);
        if (a) {
            cond_name(c, cs->cnm[k & 1u], val);
            /* (gen3) MONO / POLY the label (it took the value's room as its unit: "AL..", "!1.."), the condition whole */
            text_column(2, CO_COND(cs->comp2) == CC_ELSE && CO_ACT(cs->comp2) ? "MONO" : CO_ACT(cs->comp) && CO_ACT(cs->comp2) ?
                        "POLY" : "COND", val, c ? VAL(2u) : T_DIM);
            comp_value(cs, a, comp_arg(cs), lb, val, un);
            unit_column(3, lb, val, un, lb[0] ? VAL(3u) : T_DIM);
        } else {
            text_column(2, "COND", "", T_DIM);
            empty_column(3);
        }
        return;
    }
}
/* (V10.30, flash) a parameter's card: its value formatted, its gauge (d's ratio of v) */
static void pf_column(uint32_t c, const char *label, const param_desc_t *d, int32_t v, uint16_t col, uint32_t icon)
{
    char val[12];
    const char *unit;
    param_format(d, v, val, &unit);
    draw_column(c, label, val, unit, col, RATIO(d, v), icon);
}
static void draw_columns(void)
{
    uint32_t c;
    char val[12];
    const char *unit;
    (void)motion_mask(song.sel, card_mot_mask);       /* (PLAY OFF: no scan) */
    if (home_macros()) {                                /* (0.8.5) HOME's MACROS / ROUTES: M1..M4, 0..100 % */
        static const char *const M[4] = {"M1", "M2", "M3", "M4"};
        for (c = 0; c < 4u; c++) {
            fmt_int(val, macro_v[c] * 100 / 127);
            draw_column(c, M[c], val, "%", VAL(c), macro_v[c] * 1000 / 127, ICON_MIX);
        }
        return;
    }
    if (ui.home) {
        for (c = 0; c < 4u; c++) {
            int16_t *vp;
            const param_desc_t *d = home_param(c, &vp);
            param_format(d, *vp, val, &unit);
            card_mot_of(vp);
            draw_column(c, d->label, val, unit, VAL(c), RATIO(d, enum_rank(d, *vp)), param_icon(d, *vp));
        }
        return;
    }
    if (cur_page()->graph == GR_SWARM) {               /* (V10.6z6) EDIT > SWARM, SWARM 2 (ui_dx.c) */
        if (bt_hw_seg())
            sw_ui_cards_bt(cur_page());
        return;
    }
    if (cur_page()->graph == GR_LIVE) {                /* (p2) GLO > LIVE (ui_live.c) */
        live_cards();
        return;
    }
    if (cur_page()->graph == GR_FOCUS) {               /* (0.8.5) EDIT > FOCUS: PARAM, its value, SRC, AMT */
        uint32_t k = focus_k();
        int32_t j = focus_slot(k);
        int16_t *vp = &TSEL->p[P_E0 + k];
        const param_desc_t *d = track_desc(TSEL, P_E0 + k);
        param_format(d, *vp, val, &unit);
        hot_gauge_column(0, "PARAM", d->label, (int32_t)k * 1000 / 7);
        card_mot_of(vp);
        draw_column(1, d->label, val, unit, VAL(1u), RATIO(d, enum_rank(d, *vp)), ICON_NONE);
        text_column(2, "SRC", j < 0 ? "OFF" : N_MSRC[clamp(TSEL->p[P_M1SRC + 3u * (uint32_t)j], 0, MS_N - 1)], j < 0 ? T_DIM : VAL(2u));
        if (j < 0)
            text_column(3, "AMT", "--", T_DIM);
        else {
            const param_desc_t *da = track_desc(TSEL, P_M1AMT + 3u * (uint32_t)j);
            int16_t a = TSEL->p[P_M1AMT + 3u * (uint32_t)j];
            pf_column(3, "AMT", da, a, a ? VAL(3u) : T_DIM, ICON_NONE);
        }
        return;
    }
    if (cur_page()->graph == GR_STAGE) {               /* (0.8.5) ENV > STAGE: the stage, its value, LOOP */
        uint32_t id = P_ATK + (ui.stage & 3u);
        int16_t *vp = &TSEL->p[id];
        hot_gauge_column(0, "STAGE", TP[id].label, (int32_t)(ui.stage & 3u) * 1000 / 3);
        card_mot_of(vp);
        pf_column(1, TP[id].label, &TP[id], *vp, VAL(1u), ICON_NONE);
        pf_column(2, TP[P_ELOOP].label, &TP[P_ELOOP], TSEL->p[P_ELOOP], VAL(2u), ICON_NONE);
        text_column(3, "", "", T_DIM);
        return;
    }
    if (cur_page()->graph == GR_DXSND) {               /* EDIT > SOUND (ui_dx.c) */
        dx_cards(cur_page());
        return;
    }
    if (DX_KIT3(cur_page())) {                         /* (0.6.54) EDIT > KIT 3 */
        dx_kit3_cards();
        return;
    }
    if (SONG_PARTS(cur_page())) {                      /* (V10.6o) PARTS: PART, its ROW in the song, its loop's BARS, PLAY */
        uint32_t s = ui.part & 3u, r = ui.song_row;
        val[0] = (char)('A' + s); val[1] = 0;
        hot_text_column(0, "PART", val);
        if (r < chain_config.count && chain_config.row[r].slot == s) fmt_frac(val, (int32_t)r + 1, (int32_t)chain_config.count);
        else str_cpy(val, "--", sizeof val);
        text_column(1, "ROW", val, val[0] != '-' ? VAL(1u) : T_DIM);
        if (song_bars(s)) fmt_int(val, (int32_t)song_bars(s));
        else str_cpy(val, "--", sizeof val);
        text_column(2, "BARS", val, T_DIM);
        text_column(3, "PLAY", live.mode ? "SONG" : "LOOP", VAL(3u));
        return;
    }
    if (SONG_CLIPS(cur_page())) {                      /* (V10.6s) CLIPS: TRACK, PART (the cell), SONG REC */
        val[0] = 'T'; val[1] = (char)('1' + song.sel % NTRK); val[2] = 0;
        hot_text_column(0, "TRACK", val);
        val[0] = (char)('A' + (ui.part & 3u)); val[1] = 0;
        text_column(1, "PART", val, VAL(1u));
        empty_column(2);
        text_column(3, "REC", live.srec ? "ON" : "OFF", live.srec ? T_ACCENT : T_DIM);
        return;
    }
    if (SONG_TRK(cur_page())) {                        /* (V10.6o) TIMELINE: ROW, SECT, BARS, the ALGORITHM track's VAR */
        uint32_t row = ui.song_row < chain_config.count ? ui.song_row : chain_config.count - 1u, v;
        if (!chain_config.count) {
            hot_text_column(0, "ROW", "--");
            empty_column(1); empty_column(2); empty_column(3);
            return;
        }
        fmt_frac(val, (int32_t)row + 1, (int32_t)chain_config.count);
        hot_text_column(0, "ROW", val);
        val[0] = (char)('A' + (chain_config.row[row].slot & 3u)); val[1] = 0;
        text_column(1, "SECT", val, VAL(1u));
        fmt_int(val, chain_config.row[row].bars);
        text_column(2, "BARS", val, VAL(2u));
        v = row_var(row, song.sel % NTRK);
        val[0] = 'T'; val[1] = (char)('1' + song.sel % NTRK); str_cpy(val + 2, " VAR", 5);
        text_column(3, val, RV_NAME[v < RV_N ? v : 0], v ? VAL(3u) : T_DIM);
        return;
    }
    if (cur_page()->graph == GR_SONG) {
        uint32_t row = ui.song_row < CHAIN_ROWS ? ui.song_row : CHAIN_ROWS - 1u;
        int used = row < chain_config.count;
        if (ui.song_row >= song_end_row()) {            /* (0.6.22) the song's END: STOP / x2 .. x8 / LOOP */
            uint32_t e = SONG_END(&chain_config);
            hot_text_column(0, "ROW", "END");
            if (e == SONG_FOREVER) str_cpy(val, "LOOP", sizeof val);
            else if (e >= 2u) { val[0] = 'x'; fmt_int(val + 1, (int32_t)e); }
            else str_cpy(val, "STOP", sizeof val);
            hot_text_column(1, "END", val);
            empty_column(2);
            empty_column(3);
            return;
        }
        if (row == chain_config.count) str_cpy(val, "ADD", sizeof val);
        else {                                          /* (0.8.5, the mock-up: "2/6") */
            fmt_frac(val, (int32_t)row + 1, (int32_t)chain_config.count);
        }
        hot_text_column(0, "ROW", val);
        if (used) { val[0] = (char)('A' + chain_config.row[row].slot); val[1] = 0; }
        else str_cpy(val, "--", sizeof val);
        text_column(1, "SECT", val, used ? VAL(1u) : T_DIM);
        if (used) fmt_int(val, chain_config.row[row].bars);
        else str_cpy(val, "--", sizeof val);
        text_column(2, "BARS", val, used ? VAL(2u) : T_DIM);
        text_column(3, "SCENE", used && song_idx.scene[song_cur][row].on ? "ON" : "--", used && song_idx.scene[song_cur][row].on ? VAL(3u) : T_DIM);   /* (JIANT) */
        return;
    }
    if (cur_page()->graph == GR_SHAPE) {               /* (0.6.85) STRETCH REPEAT NUDGE; (0.6.92) ROTATE, done on the turn */
        int32_t k = shp_k(), r = (int32_t)shp_rep(), ofs = TSEL->p[P_SOFS];
        val[0] = k > 0 ? 'x' : '/';
        fmt_int(val + 1, k > 0 ? k : -k);
        gauge_column(0, "STRETCH", val, k != 1 ? VAL(0u) : T_DIM, (int32_t)shp_i * 1000 / 14);
        if (r == 1) str_cpy(val, "OFF", sizeof val);
        else if (r) { val[0] = 'x'; fmt_int(val + 1, r); }
        else str_cpy(val, "TO 64", sizeof val);
        gauge_column(1, "REPEAT", val, r != 1 ? VAL(1u) : T_DIM, (int32_t)shp_r * 1000 / 8);
        if (ndg_step == 2u) {                            /* (V10.7.4) the cursor step moved by steps */
            val[0] = 'S'; fmt_int(val + 1, (int32_t)(ui.cursor % NSTEP) + 1);
            text_column(2, "NUDGE", val, T_REC);
        } else if (ndg_step) {                           /* (V10.25) red: the cursor step's MICRO (64ths) */
            int32_t m = ST_MICRO(&TSEL->step[ui.cursor % NSTEP]);
            val[0] = m > 0 ? '+' : 0;
            fmt_int(val + (m > 0), m);
            text_column(2, "NUDGE", val, T_REC);
        } else
            text_column(2, "NUDGE", "< >", T_THEME);   /* blue: the whole sequence by steps */
        val[0] = ofs > 0 ? '+' : 0;
        fmt_int(val + (ofs > 0), ofs);
        card_mot_of(&TSEL->p[P_SOFS]);
        draw_column(3, "ROTATE", val, "ST", ofs ? VAL(3u) : T_DIM, (ofs + 32) * 1000 / 64, ICON_ORDER);
        return;
    }
    if (cur_page()->graph == GR_FUGUE) {               /* (0.6.52) HEAD RATE DIR INTERVAL */
        const fg_set_t *f = &fg_set[song.sel % NTRK][fg_ui % FG_HEADS];
        fmt_int(val, (int32_t)(fg_ui % FG_HEADS) + 2);
        hot_gauge_column(0, "HEAD", val, (int32_t)(fg_ui % FG_HEADS) * 500);
        gauge_column(1, "RATE", N_FGRATE[f->rate % FG_NRATE], f->rate ? VAL(1u) : T_DIM, (int32_t)f->rate * 1000 / (FG_NRATE - 1));
        gauge_column(2, "DIR", N_FGDIR[f->dir & 3u], f->rate ? VAL(2u) : T_DIM, (int32_t)(f->dir & 3u) * 333);
        val[0] = f->ivl > 0 ? '+' : 0;
        fmt_int(val + (f->ivl > 0), f->ivl);
        draw_column(3, "INTVL", val, "DEG", f->rate ? VAL(3u) : T_DIM, (int32_t)(f->ivl + 14) * 1000 / 28, ICON_PITCH);
        return;
    }
    if (cur_page()->graph == GR_GCOMP || cur_page()->graph == GR_COMP) {
        comp_columns();
        return;
    }
    if (cur_page()->graph == GR_CHANCE) {              /* STEP CHANCE RATCH: the cursor step's, of all of it */
        const step_t *cs = &TSEL->step[ui.cursor];
        int rplays = cs->time == ST_NOTE && (cs->n || cs->hit);   /* RATCH does nothing on a REST, a TIE, empty: DIM */
        fmt_int(val, (int32_t)ui.cursor + 1);
        hot_text_column(0, "STEP", val);
        fmt_int(val, (int32_t)step_chance(&TSEL->step[ui.cursor]));
        hot_unit_column(1, "CHANCE", val, "%");   /* the die */
        val[0] = 'x';                                  /* x1 .. x4 */
        val[1] = (char)('0' + step_ratchet(&TSEL->step[ui.cursor]));
        val[2] = 0;
        text_column(2, "RATCH", val, rplays ? VAL(2u) : T_DIM);
        empty_column(3);
        return;
    }
    if (cur_page()->graph == GR_EVENTS) {              /* AUTO LIST (ui_events.c): ROW STEP PARAM VALUE */
        const track_t *t = TSEL;
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx), r = ev_row(n), step, id;
        int16_t v;
        char nm[12];
        const param_desc_t *d;
        if (r < n) {
            const motion_event_t *e = &motion.event[idx[r]];
            fmt_frac(val, (int32_t)r + 1, (int32_t)n);
            step = MOTION_STEP(e);
            id = MOTION_ID(e);
            v = MOTION_VAL(e);
        } else {
            ev_fix();
            str_cpy(val, "ADD", sizeof val);
            step = ui.ev_step;
            id = ui.ev_id;
            v = id < P_COUNT ? motion_base_value(t, id) : 0;
        }
        hot_text_column(0, "ROW", val);
        fmt_int(val, (int32_t)step + 1);
        text_column(1, "STEP", val, r < n || step < (uint32_t)t->p[P_SLEN] ? VAL(1u) : T_DIM);
        if ((id >= P_COUNT && id != MOT_DJF) || !ev_id_ok(t, id)) {
            text_column(2, "PARAM", "--", T_DIM);
            text_column(3, "VALUE", "--", T_DIM);
            return;
        }
        ev_name(t, id, nm);
        hot_text_column(2, "PARAM", nm);
        d = track_desc(t, id);
        param_format(d, v, val, &unit);
        draw_column(3, r == n ? "VALUE" : (motion.event[idx[r]].param & MOTION_LOCK) ? "LOCK" : MOTION_RAMP(&motion.event[idx[r]]) ? "RAMP" : "AUTO", val, unit,
                    r < n ? VAL(3u) : T_DIM, RATIO(d, enum_rank(d, v)), param_icon(d, v));
        return;
    }
    if (cur_page()->graph == GR_PUNCH) {               /* (0.6.99) the punch-in lane: LEN, RES, its state, CLEAR */
        uint32_t n = 0, i;
        for (i = 0; i < 64u; i++) n += pfx_lane_at(i) != 0u;
        fmt_int(val, song.g[G_PFXLEN]);
        draw_column(0, "LEN", val, "ST", VAL(0u), RATIO(&GP[G_PFXLEN], song.g[G_PFXLEN]), ICON_AUTO);
        hot_text_column(1, "RES", N_PFXRES[(uint32_t)clamp(song.g[G_PFXRES], 0, 2)]);
        fmt_int(val, (int32_t)n);
        text_column(2, pfx_rec_ok() ? "REC" : song.playing ? "PLAY" : "STEPS", pfx_rec_ok() ? "ON" : song.playing ? "ON" : val, pfx_rec_ok() ? T_ACCENT : T_MID);
        draw_act_column(3, "CLR AUTO", n ? T_MID : T_DIM, ICON_X_MOTION_DEL);   /* (snd2: the lane only) */
        return;
    }
    if (cur_page()->graph == GR_MOTION) {
        hot_text_column(0, "PLAY", motion_enabled(TSEL) ? "ON" : "OFF");
        fmt_int(val, (int32_t)(motion_count(TSEL) - motion_lock_count(TSEL)));
        text_column(1, "EVENT", val, T_MID);
        fmt_int(val, (int32_t)motion_lock_count(TSEL));   /* parameter locks (SEQ > STEP: a step held, a knob) */
        text_column(2, "LOCK", val, T_MID);
        draw_act_column(3, "CLEAR", T_MID, ICON_X_MOTION_DEL);
        return;
    }
    if (cur_page()->graph == GR_TOOLS) {                 /* (V10.24) KNOB 1 the scope, KNOB 2 the action; OCT+ held */
        static const char *const SC[4] = {"TRACK", "SOUND", "SONG", "SYSTEM"};
        static const char *const AC[6] = {"CLEAR", "INIT", "DEL ROW", "CLEAR", "NEW", "CPU"};
        uint32_t a = ui.act ? ui.act - 1u : 0u;
        text_column(0, "SCOPE", ui.act ? SC[TL_SC[a]] : "--", ui.act ? VAL(0u) : T_DIM);
        text_column(1, "ACTION", ui.act ? AC[a] : "--", ui.act ? VAL(1u) : T_DIM);
        empty_column(2);
        text_column(3, "DO", "OCT+", ui.act && act_ready() ? T_ACCENT : T_DIM);
        return;
    }
    if (cur_page()->scope == SC_TRK) {                 /* LEVEL PAN REV of the selected track, the DJ FILTER */
        const track_t *t = TSEL;
        uint32_t lvl = trk_level(song.sel);
        param_format(&TP[P_LEVEL], (int32_t)lvl, val, &unit);   /* (0: OFF) */
        card_mot_of(&TSEL->p[P_LEVEL]);
        draw_column(0, "LEVEL", val, unit, lvl && !t->p[P_MUTE] ? VAL(0u) : T_DIM, (int32_t)lvl * 1000 / 127, ICON_AUTO);
        card_mot_of(&TSEL->p[P_PAN]);
        pf_column(1, "PAN", &TP[P_PAN], t->p[P_PAN], VAL(1u), param_icon(&TP[P_PAN], t->p[P_PAN]));
        card_mot_of(&TSEL->p[P_REV]);
        pf_column(2, "REV", &TP[P_REV], t->p[P_REV], VAL(2u), ICON_AUTO);
        pf_column(3, "BPM", &GP[G_BPM], song.g[G_BPM], VAL(3u), ICON_AUTO);   /* (V10.37) KNOB 4 the tempo, at hand (the DJ
                                                         * FILTER stays on LIVE > MANO's KNOB 4) */
        return;
    }
    if (cur_page()->graph == GR_BROWSE) {
        uint32_t total, cur = preset_pos(&total);
        char u[8];
        if (cur < total) fmt_int(val, (int32_t)cur + 1);
        else str_cpy(val, total ? "--" : "NONE", 8);   /* (0.6.25: an empty list said "--/0") */
        str_cpy(u, "/", 8);
        fmt_int(u + 1, (int32_t)total);
        if (!total || (cur + 1u >= 100u && total >= 100u)) u[0] = 0;   /* (0.7.0) "147/155" is too wide for the card: the
                                                         * number alone past 99 (every engine's presets counted) */
        hot_unit_column(0, "No.", val, u);
        hot_text_column(1, "ENG", ENGINES[TSEL->eng_req]->name);
        hot_text_column(2, "FAV", preset_favorite() ? "ON" : "OFF");
        hot_text_column(3, "LIST", N_LIST[list_get()]);
        return;
    }
    if (cur_page()->graph == GR_PATS) {                  /* PAT, then LOAD (a GO button) */
        uint32_t n = pat_count(), k = pat_pick();
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        hot_gauge_column(0, "PAT", tag, (int32_t)k * 1000 / (int32_t)(n > 1u ? n - 1u : 1u));
        draw_act_column(1, "LOAD", T_THEME, ICON_AUTO);
        empty_column(2);
        empty_column(3);
        return;
    }
    if(cur_page()->graph==GR_LYRSTEP) {
        char label[20];uint32_t pos=lyric_pick(label);
        fmt_int(val,(int32_t)ui.cursor+1);hot_text_column(0,"STEP",val);
        hot_text_column(1,"WORD",label[0]?label:"NO TEXT");
        fmt_int(val,lyric_syl);hot_text_column(2,"SYLLABLE",pos?val:"AUTO");
        draw_act_column(3,"PLACE",T_THEME,ICON_AUTO);
        return;
    }
    if(cur_page()->graph==GR_TRACKGEN) {trackgen_cards();return;}
    if (SPLIT_PAGE(cur_page())) {
        static const char *const labels[8] = {"SPLIT", "LOW TRK", "HIGH TRK", "KEY CUT", "LOW OCT", "HIGH OCT", "MIDI CH", "MIDI CUT"};
        for (uint32_t c = 0; c < 4; c++) {
            uint32_t x = (cur_page()->graph == GR_SPLIT2 ? 4u : 0u) + c, f = split_field(c);
            int32_t v = split_value(f) + (f == 1u || f == 2u || f == 3u || f == 6u);
            fmt_int(val, v);
            hot_text_column(c, labels[x], f ? val : split_cfg[0] ? "ON" : "OFF");
        }
        return;
    }
    if (FIT_PAGE(cur_page())) {
        pf_column(0, "LEN", &TP[P_SLEN], TSEL->p[P_SLEN], T_TEXT, ICON_AUTO);
        fmt_int(val, (int32_t)fit_span(TSEL));
        hot_gauge_column(1, "SPAN", val, (int32_t)(fit_span(TSEL) - 1u) * 1000 / 63);
        hot_text_column(2, "FIT", TSEL->seq_fit & 128u ? "ON" : "OFF");
        pf_column(3, "DIV", &TP[P_SDIV], TSEL->p[P_SDIV], T_TEXT, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SONGSAVE) {
        fmt_int(val, (int32_t)song_save_target + 1);
        hot_gauge_column(0, "DEST", val, song_save_target * 1000 / (NSONG - 1u));
        text_column(1, "SCOPE", song_save_loop ? "LOOP" : "SONG", T_TEXT);
        empty_column(2);
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SLOTS) {                 /* (0.6.12) PROJECT, SECT A-D, then LOAD and SAVE */
        uint32_t k = (uint32_t)song.g[G_SLOT] - 1u;
        int used = graph_project_used(k);
        if (save_level() == SV_PROJ) {                   /* (0.8.5) SAVE > PROJECT's SECTIONS: SECT, its NAME */
            val[0] = (char)('A' + (k & 3u)); val[1] = 0;
            hot_gauge_column(0, "SECT", val, (int32_t)(k & 3u) * 1000 / 3);
            draw_act_column(1, "ERASE", used ? T_THEME : T_DIM, ICON_AUTO);   /* (V10.17; the name: the card) */
        } else {
            fmt_int(val, (int32_t)(song_draft ? song_save_target : song_cur) + 1);
            hot_gauge_column(0, "PROJECT", val, (int32_t)(song_draft ? song_save_target : song_cur) * 1000 / (int32_t)(NSONG - 1u));
            draw_act_column(1, "ERASE", T_THEME, ICON_AUTO);
        }
        draw_act_column(2, "LOAD", used || (save_level() == SV_LIB && song_draft) ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_USER) {                  /* SLOT, then three GO buttons */
        int used = up_used(ui.uslot);
        up_slot_label(val, ui.uslot);
        hot_gauge_column(0, "SLOT", val, (int32_t)ui.uslot * 1000 / (int32_t)(UP_SLOTS - 1u));
        draw_act_column(1, "LOAD", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(2, "ERASE", used ? T_THEME : T_DIM, ICON_AUTO);
        draw_act_column(3, "SAVE", T_THEME, ICON_AUTO);
        return;
    }
    if (cur_page()->graph == GR_SLPIT) {                 /* (JIANT 0.6.3) STEP, its PIT, LEN, QNT */
        uint32_t id = G_SLP0 + (sl_ui_step & 15u);
        fmt_int(val, (int32_t)sl_ui_step + 1);
        draw_column(0, "STEP", val, "/16", sl_ui_step < (uint32_t)song.g[G_SLPLEN] ? VAL(0u) : T_DIM,
                    (int32_t)sl_ui_step * 1000 / 15, ICON_AUTO);
        pf_column(1, "PITCH", &GP[id], song.g[id], song.g[id] ? VAL(1u) : T_DIM, ICON_AUTO);
        pf_column(2, "LEN", &GP[G_SLPLEN], song.g[G_SLPLEN], VAL(2u), ICON_AUTO);
        param_format(&GP[G_SLPQ], song.g[G_SLPQ], val, &unit);
        hot_unit_column(3, "QUANT", val, unit);
        return;
    }
    if (cur_page()->graph == GR_MSEQ) {                  /* (JIANT) STEP, its LVL, LEN, SLEW */
        const track_t *t = TSEL;
        uint32_t id = P_MS0 + (ms_ui_step & 15u);
        fmt_int(val, (int32_t)ms_ui_step + 1);
        draw_column(0, "STEP", val, "/16", ms_ui_step < (uint32_t)t->p[P_MSLEN] ? VAL(0u) : T_DIM,
                    (int32_t)ms_ui_step * 1000 / 15, ICON_AUTO);
        pf_column(1, "LEVEL", &TP[id], t->p[id], VAL(1u), ICON_LEVEL);
        pf_column(2, "LEN", &TP[P_MSLEN], t->p[P_MSLEN], VAL(2u), ICON_AUTO);
        pf_column(3, "SLEW", &TP[P_MSSLW], t->p[P_MSSLW], t->p[P_MSSLW] ? VAL(3u) : T_DIM, ICON_GLIDE);
        return;
    }
    if (fop_page(cur_page()->graph)) {                   /* (0.6.10) FM6: OP, then three of its bytes (ui_fm6op.c) */
        for (c = 0; c < 4u; c++) {
            const char *lb, *u;
            int32_t r;
            uint32_t ic;
            int on;
            if (!fop_ok()) {                             /* (the engine changed before ui_draw left the page) */
                empty_column(c);
                continue;
            }
            on = fop_card(c, &lb, val, &u, &r, &ic);
            draw_column(c, lb, val, u, on ? VAL(c) : T_DIM, r, ic);
        }
        return;
    }
    if (cur_page()->graph == GR_MOD) {                   /* SLOT, then that slot's SRC DST AMT */
        const track_t *t = TSEL;
        uint32_t id = P_M1SRC + 3u * mod_ui_slot;
        int32_t s = t->p[id], d = t->p[id + 1u], a = t->p[id + 2u];
        fmt_int(val, (int32_t)mod_ui_slot + 1);
        draw_column(0, "SLOT", val, "/4", VAL(0u), (int32_t)mod_ui_slot * 1000 / 3, mod_src_icon(MS_OFF));   /* (the mod icon) */
        text_column(1, "SRC", N_MSRC[clamp(s, 0, MS_N - 1)], s ? VAL(1u) : T_DIM);
        text_column(2, mod_dst_scope(t, d) == 'M' ? "MASTER" : mod_dst_scope(t, d) == 'V' ? "VOICE" : "TRACK", mod_dst_name(t, d), d ? VAL(2u) : T_DIM);
        pf_column(3, "AMT", &TP[id + 2u], a, a ? VAL(3u) : T_DIM, mod_src_icon(MS_OFF));
        return;
    }
    if (cur_page()->graph == GR_ROLL && lock_held()) {   /* a step held: what KNOB 1..4 lock (ui_input.c lock_turn) */
        uint64_t held = lock_held();
        uint32_t st = 0;
        while (!((held >> st) & 1u))
            st++;                                       /* (the first step held: its values) */
        for (c = 0; c < 4u; c++) {
            uint32_t id = lock_id(c);
            const param_desc_t *d;
            int16_t v;
            int on;
            if (id == 0xFFu) {
                empty_column(c);
                continue;
            }
            d = track_desc(TSEL, id);
            on = motion_lock_get(TSEL, st, id, &v);
            if (!on)
                v = motion_base_value(TSEL, id);      /* not locked: the sound's own, DIM */
            param_format(d, v, val, &unit);
            draw_column(c, d->label, val, unit, on ? VAL(c) : T_DIM, RATIO(d, enum_rank(d, v)), param_icon(d, v));
        }
        return;
    }
    if (cur_page()->scope == SC_STEP && drum_track(TSEL)) {   /* the grid: STEP LANE HIT ACC */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        uint32_t b = 1u << ui.lane, on = (step_lanes_of(TSEL,st) & b) != 0u, ac = (step_accents_of(TSEL,st) & b) != 0u;
        char sn[8], sl[8];
        fmt_int(sn, (int32_t)ui.cursor + 1);
        str_cpy(sl, "/", 8);
        fmt_int(sl + 1, TSEL->p[P_SLEN]);
        hot_unit_column(0, "STEP", sn, sl);
        hot_text_column(1, "LANE", drum_lane_name(TSEL, ui.lane));
        text_column(2, "HIT", GRID_HIT[grid_hit_of(st, ui.lane)], on ? VAL(2u) : T_DIM);   /* (0.8.5: its mode) */
        text_column(3, "ACC", ac ? "ON" : "--", ac ? VAL(3u) : T_DIM);
        return;
    }
    if (cur_page()->scope == SC_STEP && cur_page()->id[0] == 4u) {   /* (0.6.81) SEQ > NOTE: FOCUS PITCH MOVE; (0.6.92) MICRO
                                                         * (the step off the grid, 0.6.88's TIMING; on a kit: STEP and MICRO) */
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        uint32_t kit = drum_track(TSEL), on = st->time == ST_NOTE && (kit ? st->hit != 0u : st->n != 0u);
        uint32_t f = on && !kit && ui.pr_focus <= st->n ? ui.pr_focus : 0u;
        int32_t m = ST_MICRO(st);
        char fo[8], pn[8], sn[8], mo[10];
        if (!f) str_cpy(fo, on && !kit && st->n > 1u ? "CHORD" : "ALL", 8);
        else { str_cpy(fo, "NOTE ", 8); fmt_int(fo + 5, (int32_t)f); }
        if (on && !kit) note_name(pn, st->note[f ? f - 1u : 0u]); else str_cpy(pn, "--", 8);
        fmt_int(sn, (int32_t)ui.cursor + 1);
        if (!m) str_cpy(mo, on ? "GRID" : "--", sizeof mo);
        else { mo[0] = m < 0 ? '-' : '+'; fmt_int(mo + 1, m < 0 ? -m : m); str_cpy(mo + str_len(mo), "/64", 4); }   /* (0.9.1) */
        if (kit) hot_text_column(0, "STEP", sn);
        else text_column(0, "FOCUS", fo, on ? VAL(0u) : T_DIM);
        text_column(1, "PITCH", pn, on && !kit ? VAL(1u) : T_DIM);
        unit_column(2, "MOVE", kit ? "--" : sn, kit ? "" : "<>", on && !kit ? VAL(2u) : T_DIM);
        gauge_column(3, "MICRO", mo, on && m ? VAL(3u) : T_DIM, on ? (m - MICRO_MIN) * 1000 / (MICRO_MAX - MICRO_MIN) : -1);
        return;
    }
    if (cur_page()->scope == SC_STEP) {
        static const char *const TIME_N[3] = {"NOTE", "TIE", "REST"};
        const step_t *st = &seq_steps(TSEL)[ui.cursor];
        char u[8];
        uint32_t cnt = st->n, first = st->note[0], l;
        for (l = NLANE; l-- > 0;)                         /* (lane hits count as their notes) */
            if ((st->hit >> l) & 1u) {
                cnt++;
                if (!st->n)
                    first = DRUM_LANE_NOTE[l];
            }
        if (cnt) {
            note_name(val, first);
            u[0] = 0;
            if (cnt > 1) {
                str_cpy(u, "+", 8);
                fmt_int(u + 1, (int32_t)cnt - 1);
            }
        } else {
            str_cpy(val, "--", 12);
            u[0] = 0;
        }
        {
            static const char *const FLAG_N[4] = {"-", "ACC", "SLD", "A+S"};
            char sn[8], sl[8], tv[8];
            uint32_t nl = st->time == ST_NOTE && st->n ? note_len(TSEL, ui.cursor, (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP)) : 1u;
            fmt_int(sn, (int32_t)ui.cursor + 1);
            str_cpy(sl, "/", 8);
            fmt_int(sl + 1, TSEL->p[P_SLEN]);
            if (nl > 1u) fmt_int(tv, (int32_t)nl);            /* (0.8.4) a NOTE's length: 3 ST (NOTE: one step) */
            else str_cpy(tv, TIME_N[st->time % 3u], sizeof tv);
            hot_unit_column(0, "STEP", sn, sl);
            unit_column(1, "NOTE", val, u, step_on(st) ? VAL(1u) : T_DIM);
            hot_unit_column(2, "TIME", tv, nl > 1u ? "ST" : "");
            {   /* (V10.8) VEL, past 127 the flags */
                uint32_t f = (st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u);
                char vv[8];
                if (f) str_cpy(vv, FLAG_N[f], sizeof vv); else fmt_int(vv, st->vel ? st->vel : 96);
                hot_text_column(3, "VEL", vv);
            }
        }
        return;
    }
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        if (c == 0u && BAR_PAGE(cur_page())) {          /* (0.9.2) SEQ > BAR: KNOB 1 the bar ("2/4") */
            uint32_t nb = ((uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP) + 15u) / 16u;
            char u[4] = "/1";
            fmt_int(val, (int32_t)ui.bank + 1);
            u[1] = (char)('0' + nb);
            draw_column(0, "BAR", val, u, VAL(0u), nb > 1u ? (int32_t)ui.bank * 1000 / (int32_t)(nb - 1u) : 0, ICON_X_PATTERN);
            continue;
        }
        if (!d || !d->label || d->label[0] == '-') {
            empty_column(c);
            continue;
        }
        if (cur_page()->id[c] == G_MIDI && cur_page()->scope == SC_GLOBAL) {
            if (c == 2u) {                               /* (V10.37) KNOB 3: Bluetooth (MENU > SYSTEM > BT) */
                str_cpy(val, bt_vname(bt_get()), 12);
                unit_column(c, "BT", val, "", T_THEME);
                continue;
            }
            if (c) {                                     /* (0.9.9-snd8) SYSTEM 2's KNOB 2: the TRS messages come in */
                fmt_int(val, (int32_t)(um.msgs % 10000u));
                unit_column(c, "TRS IN", val, "MSG", T_THEME);
                continue;
            }
            str_cpy(val, !usb.up ? "OFF" : usb.config ? "MIDI" : usb.setups ? "ENUM" : usb.sof_seen ? "BUS" : "WAIT", 12);
            unit = "USB";
            unit_column(c, "USB", val, unit, T_THEME);
            continue;
        }
        if ((act_cols() >> c) & 1u) {
            draw_act_column(c, d->label, T_THEME, ICON_AUTO);
            continue;
        }
        if (cur_page()->id[c] == G_INFO && cur_page()->scope == SC_GLOBAL) {
            fmt_int(val, (int32_t)(song.cpu_q8 * 100u / 256u));
            unit = "%";
        } else {
            param_format(d, *vp, val, &unit);
            if (sys_load && cur_page()->scope == SC_GLOBAL &&   /* (0.6.29) LOAD LITE / ECO swaps it while it sounds */
                ((cur_page()->id[c] == G_RTYPE && REV_LITE[sys_load % 3u][*vp <= 4 ? *vp : 0] != *vp) ||
                 ((cur_page()->id[c] == G_DPIT || cur_page()->id[c] == G_DSPRY) && *vp)))
                unit = N_LOAD[LOAD_ECO - sys_load % 3u];
            if (sys_load == LOAD_ECO && cur_page()->scope == SC_TRACK && cur_page()->id[c] == P_CHOR && *vp)
                unit = "ECO";                           /* (0.6.30) no chorus: the send goes nowhere */
        }
        card_mot_of(vp);                                /* (a global: never) */
        draw_column(c, d->label, val, unit, vp == &lphase_ui && TSEL->p[P_LPHASE] < 0 ? T_DIM : VAL(c), d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, enum_rank(d, *vp)),
                    param_icon(d, *vp));
    }
}


/* The power-on splash (main.c fm1_main, web/emu felucca_web.c; JIANT 0.5): the maker's signature ("Jiant FM1",
 * tools/gen_signature.py: 24 strokes, 214 points) written in the thermal colours, stroke by stroke as the hand went:
 * the line cold (cyan) where the pen started, hotter along its way (violet, red, orange) to yellow white where it
 * ended, the pen's tip a white point while it writes; under it, small, the version and "by Juan Jiant". Written over
 * SPL_WRITE ms (splash_write, main.c's splash loop; MENU > ANIM OFF: whole at once). Lines only, the canvas
 * redrawn a step (~700 short lines). In the saved palette (settings_init before it) */
#include "ui_signature.h"
#define SPL_WRITE 800u                              /* ms: the signature written (was 900; 0.6.51: 600 -> 800) */
#define SPL_HOLD 900u                               /* .. then shown whole, before the UI (was 1100; 0.6.51: 500 -> 900) */
#define SPL_SY 40                                   /* the signature's canvas on the screen (240 x SIG_H + 12) */
#define SPL_TY 186                                  /* the two lines under it */
static const char *const SPLASH_LINES[] = {FELUCCA_VERSION, "by Juan Jiant"};
static uint8_t spl_anim;                            /* main.c: 1 = the signature to be written (splash_write) */
/* (V10.6k) the logo (ui_signature.h, tools/gen_logo.py: "jiant fm" in the owner's pen, a point every ~3 px) up to point
 * n (>= SIG_N: whole), a round pen 5 px wide, solid, in the thermal scale as it is written (violet to red, orange,
 * yellow), the pen's tip white while it writes (V10.6n: was an outline filled at the end). Nothing computed but
 * the colour: no curves, no creatures (the 0.5.2 signature and its beings were ~1.9 KB) */
static uint16_t sig_col(uint32_t i)                 /* the thermal scale along the writing: violet, red, orange, yellow */
{
    return ux.mono || settings.palette == UI_BW_INDEX ? T_TEXT : heat_col(24 + (int32_t)(i * 220u / SIG_N));
}
static void splash_sig(uint32_t n)
{
    uint32_t i;
    cv_begin(240, SIG_H + 12, T_BG);
    for (i = 0; i < SIG_N && i < n; i++) {             /* the pen (a rounded square, 5 px), solid as it writes */
        int32_t x = (240 - SIG_W) / 2 + SIG_PTS[2u * i], y = 6 + (SIG_PTS[2u * i + 1u] & 127);
        uint16_t c = i + 1u == n && n < SIG_N ? T_TEXT : sig_col(i);
        cv_rect(x - 2, y - 1, 5, 3, c);
        cv_rect(x - 1, y - 2, 3, 5, c);
    }
    cv_blit(0, SPL_SY);
}
static void draw_splash(void)
{
    uint32_t k;
    lcd_fill(0, 0, 240, 240, T_BG);
    cv_begin(240, 36, T_BG);
    for (k = 0; k < 2u; k++)
        cv_text_in(0, (int32_t)k * 16, 240, &AF_S, SPLASH_LINES[k], k ? T_DIM : T_MID, T_BG);
    cv_blit(0, SPL_TY);
    if (spl_anim && !(ui_prefs & PREF_ANIM_OFF))
        splash_sig(0);
    else
        splash_sig(SIG_N);
    lcd_sync();
}
/* main.c's splash loop: the signature as written ms after the start */
static void splash_write(uint32_t ms)
{
    if (!spl_anim || (ui_prefs & PREF_ANIM_OFF))
        return;
    splash_sig(ms >= SPL_WRITE ? SIG_N : SIG_N * ms / SPL_WRITE);   /* (V10.6n: solid as it writes, the owner) */
    lcd_sync();
}

/* UPDATE MODE countdown (main.c: OCT- + OCT+ held): over everything, the menu and the dialogs too */
static void draw_uboot(void)
{
    static uint8_t shown;
    char d[4] = {(char)('0' + ui.uboot % 10u), 0, 0, 0};
    if (!ui.force && shown == ui.uboot)
        return;
    shown = ui.uboot;
    lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    ui.head_sig = ~0u;
    draw_text_box(0, 76, 240, &AF_M, "UPDATE MODE IN", T_TEXT, 1);
    draw_text_box(0, 102, 240, &AF_L, d, T_THEME, 1);
    {   /* the two keycaps held, and what letting go does */
        int32_t w = kc_w(KC_OCTDN) + 3 + kh_ink(KC_OCTUP, "LET GO TO CANCEL"), x = HALF_UP(240 - w);
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(3), "update-mode keys centred");
        x = cv_keycap(x, 1, KC_OCTDN, T_KEY, T_INK, T_BG) + 3;
        cv_key_hint(x, 1, KC_OCTUP, "LET GO TO CANCEL", 1, T_BG);
        cv_blit(0, 149);
        lcd_sync();
    }
    ui.force = 0;
}

/* the count-in (seq.c, MENU > COUNT-IN; 1.1, Discussion #131): over the page until the sequencer starts. Its bar's four
 * beats counted down, 4 3 2 1, in four cells (the beat now filled THEME, those gone dim), with 2 BARS which bar it is;
 * PLAY stops it. Drawn again on each beat (seq.c's state; the header stays live) */
#define CI_Y 88                                       /* the cells: 4 x 48 px, 8 px apart, 72 tall */
#define CI_H 72
static __attribute__((noinline)) void draw_countin(void)
{
    static uint32_t shown;
    uint32_t total = cin_total, e = total - cin_left, j, sig = total * 64u + e + 1u;
    char sub[16];
    int full = ui.force || shown / 64u != total;       /* (0.9.0) a beat: its cells and the bar only (was the whole screen) */
    if (!ui.force && shown == sig)
        return;
    shown = sig;
    if (full) {                                       /* (round what it draws, not under it too) */
        const int16_t R[5][4] = {{0, 44, 240, AF_M.h}, {0, 66, 240, AF_S.h}, {12, CI_Y, 216, CI_H},
                                 {0, CI_Y + CI_H + 14, 240, AF_S.h}, {0, 206, 240, KC_H + 2}};
        fill_around(R, 5);
        ui.head_sig = ~0u;
        draw_text_box(0, 44, 240, &AF_M, "COUNT-IN", T_TEXT, 1);
    }
    if (total > 4u) {
        str_cpy(sub, "BAR 1 OF 2", sizeof sub);
        sub[4] = (char)('1' + e / 4u % 9u);
        sub[9] = (char)('0' + total / 4u % 10u);
    } else
        str_cpy(sub, "1 BAR", sizeof sub);
    draw_text_box(0, 66, 240, &AF_S, sub, T_MID, 1);
    cv_begin(216, CI_H, T_BG);
    for (j = 0; j < 4u; j++) {
        int32_t x = (int32_t)j * 56;
        char d[2] = {(char)('4' - j), 0};
        int now = j == e % 4u, gone = j < e % 4u;
        uint16_t fill = now ? T_THEME : T_SURF;
        if (ux.style && !now) {                       /* LINE: no SURF card, a 1 px rule round it */
            cv_rrect(x, 0, 48, CI_H, 8, T_RULE, T_BG);
            cv_rrect(x + 1, 1, 46, CI_H - 2, 7, T_BG, T_RULE);
        } else
            cv_rrect(x, 0, 48, CI_H, 8, fill, T_BG);
        GFX_HOOK_ALIGN(x, 0, x + 48, CI_H, AL_HV, "count-in beat centred in its cell");
        cv_text_in(x, CAP_IN(L, CI_H), 48, &AF_L, d, now ? T_INK : gone ? T_DIM : T_TEXT, fill);
    }
    cv_blit(12, CI_Y);
    if (!full) {
        ui.force = 0;
        return;
    }
    draw_text_box(0, CI_Y + CI_H + 14, 240, &AF_S, "RECORDING STARTS AFTER 1", T_MID, 1);
    {   /* PLAY stops it */
        int32_t x = HALF_UP(240 - kh_ink(KC_PLAY, "STOP"));
        cv_begin(240, KC_H + 2, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_N(2), "count-in key centred");
        cv_key_hint(x, 1, KC_PLAY, "STOP", 1, T_BG);
        cv_blit(0, 206);
        lcd_sync();
    }
    ui.force = 0;
}

/* the OCT- / OCT+ dialog: what it does (ui.confirm, ui.confirm_trk) on two lines, on a surface */
#define DLG_X 16
#define DLG_Y 62
#define DLG_W 208
#define DLG_H 116
static void confirm_text(char *a, char *b)
{
    uint32_t k = ui.confirm_trk;
    b[0] = 0;
    switch (ui.confirm) {
    case CF_CLEAR_TRK:
        str_cpy(a, "CLEAR TRACK 1?", 24);
        a[12] = (char)('1' + k % NTRK);
        break;
    case CF_OVR_SONG:
        str_cpy(a, "OVERWRITE PROJECT ", 24);
        fmt_int(a + str_len(a), (int32_t)k + 1);
        str_cpy(a + str_len(a), "?", 2);
        str_cpy(b, song_save_loop ? "LOOP A; CLEAR B-D" : "ALL A-D + ARRANGEMENT", 24);
        break;
    case CF_OVR_PROJ:
        str_cpy(a, "OVERWRITE PROJECT A?", 24);
        a[18] = (char)('A' + (k & 3u));
        if (!project_name(k & 3u, b) || !b[0])          /* the project's name, else what SONG plays from it */
            str_cpy(b, "SONG PATTERN CHANGES", 24);
        break;
    case CF_DEL_ROW:
        str_cpy(a, "DELETE SONG ROW?", 24);
        break;
    case CF_CLEAR_SONG:
        str_cpy(a, "CLEAR SONG ORDER?", 24);
        break;
    case CF_INIT_SOUND:
        str_cpy(a, drum_track(TSEL) ? "INIT SOUND + KIT?" : "INITIALIZE SOUND?", 24);
        break;
    case CF_CLEAR_ALL:
        str_cpy(a, "CLEAR ALL SEQUENCES?", 24);
        break;
    case CF_CPU_TEST:                                /* (0.6.83) SAVE > TOOLS 6 */
        str_cpy(a, "BUILD CPU TEST SONG?", 24);
        str_cpy(b, "UNDO AFTER: GLO + G4", 24);
        break;
    case CF_LIVE_COMMIT:                             /* (p2) GLO > LIVE (k: the tracks) */
        str_cpy(a, "WRITE LIVE CHANGES?", 24);
        str_cpy(b, "UNDO AFTER: GLO + G4", 24);
        break;
    case CF_INIT_SYS:                                /* (0.6.40) SAVE > TOOLS 5 */
        str_cpy(a, "NEW PROJECT?", 24);
        str_cpy(b, "UNDO AFTER: GLO + G4", 24);
        break;
    case CF_DICE_SOUND:                              /* (0.6.65) EDIT + SELECT: heard already */
    case CF_DICE_SEQ:                                /* (0.6.67) SEQ / LFO / FX + SELECT */
    case CF_DICE_LFO:
    case CF_DICE_FX:
        str_cpy(a, ui.confirm == CF_DICE_SEQ ? "KEEP DICED SEQ?" : ui.confirm == CF_DICE_LFO ? "KEEP DICED LFO?" :
                   ui.confirm == CF_DICE_FX ? "KEEP DICED FX?" : drum_track(TSEL) ? "KEEP DICED KIT?" : "KEEP DICED SOUND?", 24);
        str_cpy(b, "SELECT: DICE AGAIN", 24);
        break;
    case CF_DICE_ALL:                                /* (0.6.21) HOME + SELECT */
        str_cpy(a, "DICE EVERYTHING?", 24);
        str_cpy(b, "UNDO AFTER: GLO + G4", 24);
        break;
    case CF_RECALL_SOUND:
        str_cpy(a, drum_track(TSEL) ? "RECALL SOUND + KIT?" : "RECALL SAVED SOUND?", 24);
        break;
    case CF_CLEAR_MOTION:
        str_cpy(a, "CLEAR T1 AUTOMATION?", 24); a[7] = (char)('1' + k % NTRK);
        break;
    case CF_CLEAR_PFX:
        str_cpy(a, "CLEAR PUNCH AUTOMATION?", 24);   /* (snd2: the lane only) */
        break;
    case CF_OVR_USER:
        str_cpy(a, "OVERWRITE ", 24);
        up_slot_label(a + str_len(a), k);
        str_cpy(a + str_len(a), "?", 2);
        up_name(k, b);                               /* the sound stored there */
        break;
    case CF_ERASE_SONG:
        str_cpy(a, "ERASE PROJECT ", 24); fmt_int(a + str_len(a), (int32_t)k + 1); str_cpy(a + str_len(a), "?", 2);
        str_cpy(b, "ALL A-D + ARRANGEMENT", 24);
        break;
    case CF_ERASE_SECT:                              /* (V10.17) SAVE > SECTIONS > ERASE */
        str_cpy(a, "ERASE SECT A?", 24);
        a[11] = (char)('A' + (k & 3u));
        str_cpy(b, graph_project_used(k & 3u) && graph_project_name(k & 3u)[0] ? graph_project_name(k & 3u) :
                   "THE SLOT GOES EMPTY", 24);
        break;
    case CF_ERASE_USER:
        str_cpy(a, "ERASE ", 24);
        up_slot_label(a + str_len(a), k);
        str_cpy(a + str_len(a), "?", 2);
        up_name(k, b);
        break;
    case CF_LOAD_PAT: {
        char tag[4];
        str_cpy(a, "REPLACE T1 SEQUENCE?", 24);
        a[9] = (char)('1' + k % NTRK);
        pat_label(pat_pick(), tag, b);               /* with the pattern picked */
        break;
    }
    default:
        str_cpy(a, "CLEAR T1 SEQUENCE?", 24);     /* the header (T1..T4) is hidden */
        a[7] = (char)('1' + k % NTRK);
        break;
    }
}
static void gen_cards(void);                            /* (0.8.5) gen.c: GEN in HOME's lens, its cards */
/* (0.8.5) the dialogs as the approved mock-up (NOTICE . DICE . NAME): their tabs under the header, the breadcrumb at the
 * foot. NOTICE (the OCT- / OCT+ questions): a RAISE card under a THEME rule, "?" in the accent, the question, the detail,
 * two buttons (OCT- NO, OCT+ YES in the accent, underlined). DICE (KEEP DICED ...?): the part's cards, BEFORE and NOW
 * side by side (dice_panel, ui_input.c), the question in a strip with its two buttons. No icons */
static void dice_panel(void);                            /* (ui_input.c: what the dice had, what it has) */
/* (0.9.0) BG under the header everywhere but in the n rectangles {x, y, w, h} the screen is about to draw (a dialog's, NAME's
 * strips): a band at a time between their edges, the gaps across it (was all of it filled, then drawn over: ~1.7 screens) */
static void fill_around(const int16_t (*r)[4], uint32_t n)
{
    int32_t ys[2 + 2 * 8], ny = 0, i, j, k;
    ys[ny++] = H_HEAD;
    ys[ny++] = 240;
    for (i = 0; i < (int32_t)n && i < 8; i++) {
        ys[ny++] = r[i][1];
        ys[ny++] = r[i][1] + r[i][3];
    }
    for (i = 1; i < ny; i++)                            /* (sorted: a few) */
        for (j = i; j > 0 && ys[j - 1] > ys[j]; j--)
            k = ys[j], ys[j] = ys[j - 1], ys[j - 1] = k;
    for (i = 0; i + 1 < ny; i++) {
        int32_t y0 = ys[i] < H_HEAD ? H_HEAD : ys[i], y1 = ys[i + 1] > 240 ? 240 : ys[i + 1], x = 0;
        if (y1 <= y0)
            continue;
        for (;;) {                                      /* from x: the next rectangle covering the band, the gap before it */
            int32_t nx = 240, ne = 240;
            for (j = 0; j < (int32_t)n; j++)
                if (r[j][1] <= y0 && r[j][1] + r[j][3] >= y1 && r[j][0] + r[j][2] > x && r[j][0] < nx)
                    nx = r[j][0] < x ? x : r[j][0], ne = r[j][0] + r[j][2];
            if (nx > x)
                lcd_fill((uint32_t)x, (uint32_t)y0, (uint32_t)(nx - x), (uint32_t)(y1 - y0), T_BG);
            if (nx >= 240)
                break;
            x = ne;
        }
    }
}
#define DLG_TAB_H (ux.skin && !(ui_prefs & PREF_LARGE) ? LB_TAB_H : LT_H)   /* the dialogs' tabs (tabs_paint) */
static void dlg_tabs(uint32_t lv)
{
    static const char *const DN[3] = {"NOTICE", "DICE", "NAME"};
    tabs_paint(3, lv, DN, 0, 0, 0);                     /* (0.8.9: in the skin's tabs) */
}
static void dlg_crumb(const char *a, const char *b)     /* "SAVE . NOTICE . CONFIRM": the section, the dialog's kind THEME,
                                                         * what it asks TEXT (0.9.2: was "LENTE") */
{
    int32_t x;
    cv_begin(240, LZ_FOOT_H, T_BG);
    x = cv_text(6, CRUMB_Y, &AF_X, SEC_NAME[nav_sec()], T_DIM);
    x = cv_text(x, CRUMB_Y, &AF_X, " \xB7 ", T_DIM);
    x = cv_text(x, CRUMB_Y, &AF_X, a, T_THEME);
    x = cv_text(x, CRUMB_Y, &AF_X, " \xB7 ", T_DIM);
    cv_text(x, CRUMB_Y, &AF_X, b, T_TEXT);
    cv_blit(0, LZ_FOOT_Y);
}
/* (0.8.9) a dialog's card: COLORS rounded, its top THEME; BOTTOM / HUD none (a THEME line over it) */
static uint16_t dlg_card(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t under)
{
    if (ux.skin) {
        cv_rect(x, y, w, 1, T_THEME);
        return under;
    }
    cv_rrect(x, y, w, h, 5, T_RAISE, under);
    cv_rect(x + 4, y, w - 8, 2, T_THEME);
    return T_RAISE;
}
static void dlg_button(int32_t x, int32_t y, int32_t w, const char *k, const char *a, int yes, uint16_t under)
{
    uint16_t bg = ux.skin ? under : T_SURF;
    if (!ux.skin)
        cv_rrect(x, y, w, 34, 4, T_SURF, under);
    cv_text_on(x + 6, y + 5 - AF_X_CAP_Y, &AF_X, k, T_DIM, bg);
    cv_text_on(x + 6, y + 16 - AF_M_CAP_Y, &AF_M, a, yes ? T_ACCENT : T_TEXT, bg);
    cv_rect(x + 6, y + 30, w - 12, yes ? 2 : 1, yes ? T_ACCENT : T_LINE);
}
static void draw_confirm(void)
{
    char a[24], b[24];
    const aafont_t *tf = &AF_M;
    int dk = ui.confirm >= CF_DICE_SOUND && ui.confirm <= CF_DICE_FX;
    confirm_text(a, b);
    if (dk)
        lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);   /* (DICE: BEFORE / NOW fill the screen, its box) */
    /* (0.9.5) NOTICE: its tabs, its card and the breadcrumb alone, the page live round them (ui_draw_page, dlg_holes; 0.9.0
     * .. 0.9.4: BG round them, the page gone) */
    ui.head_sig = ~0u;
    dlg_tabs(dk ? 1u : 0u);
    if (dk) {                                         /* DICE: the part, BEFORE / NOW, the question in a strip */
        static const char *const PART[4] = {"SOUND", "SEQ", "LFO", "FX"};
        uint32_t pk = (uint32_t)(ui.confirm - CF_DICE_SOUND) & 3u;
        dice_panel();
        uint16_t cb, kb;
        cv_begin(240, 30, T_BG);
        cb = dlg_card(4, 0, 232, 30, T_BG);
        kb = ux.skin ? cb : T_SURF;
        cv_text_on(10, 7 - AF_S_CAP_Y, &AF_S, a, T_TEXT, cb);
        cv_text_on(10, 20 - AF_X_CAP_Y, &AF_X, b, T_DIM, cb);
        if (!ux.skin) {
            cv_rrect(148, 4, 40, 23, 3, T_SURF, cb);
            cv_rrect(192, 4, 40, 23, 3, T_SURF, cb);
        }
        cv_text_in(148, 7 - AF_X_CAP_Y, 40, &AF_X, "OCT-", T_DIM, kb);
        cv_text_in(148, 17 - AF_X_CAP_Y, 40, &AF_X, "CANCEL", T_MID, kb);
        cv_text_in(192, 7 - AF_X_CAP_Y, 40, &AF_X, "OCT+", T_DIM, kb);
        cv_text_in(192, 17 - AF_X_CAP_Y, 40, &AF_X, "OK", T_ACCENT, kb);
        cv_blit(0, 194);
        dlg_crumb("DICE", drum_track(TSEL) && pk == 0u ? "KIT" : PART[pk]);
        return;
    }
    cv_begin(212, 140, T_BG);                         /* NOTICE: the card */
    {
    uint16_t cb = dlg_card(0, 0, 212, 140, T_BG);
    cv_text_on(12, 12 - AF_M_CAP_Y, &AF_M, "?", T_ACCENT, cb);
    if (text_w(tf, a) > 196)
        tf = &AF_S;
    GFX_HOOK_ALIGN(0, 0, 212, 0, AL_H, "dialog question centred");
    cv_text_in(0, 36, 212, tf, a, T_TEXT, cb);
    if (b[0]) {
        char f[32];
        text_fit(f, sizeof f, b, &AF_S, 196);          /* a name: free text */
        GFX_HOOK_ALIGN(0, 0, 212, 0, AL_H, "dialog detail centred");
        cv_text_flags(ink_in(&AF_S, f, 212), 60, &AF_S, f, T_MID, cb, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    }
    dlg_button(12, 92, 90, "OCT -", ui.confirm == CF_DICE_ALL ? "CANCEL" : "NO", 0, cb);
    dlg_button(110, 92, 90, "OCT +", "YES", 1, cb);
    }
    cv_blit(14, 66);
    dlg_crumb("NOTICE", "CONFIRM");
}

/* the stored STYLE (ui_style: the menu, a settings import, a restore) and SKIN (0.8.9, ui_skin) into gfx.c's copies: the
 * tokens again, all redrawn */
static void style_apply(void)
{
    if (ui_style > ST_LINE)                             /* 2, the retired PIXEL: LINE; unknown: FLAT */
        ui_style = ui_style == 2u ? ST_LINE : ST_FLAT;
    if (ui_skin >= SKIN_N)                              /* (0.8.9) SKIN: unknown, COLORS */
        ui_skin = SKIN_COLORS;
    if (ux.style != ui_style || ux.skin != ui_skin) {
        ux.style = ui_style;
        ux.skin = ui_skin;
        palette_set(settings.palette);
        ui.force = 1;
    }
}

/* (0.8.5) the end of a lens screen drawn outside the page path (GEN): the second message, the timers, the footer and the
 * tabs */
static void lz_frame_end(void)
{
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.hot_t)
        ui.hot_t--;
    if (pf_on)
        page_flash_draw();
    if (!pf_on)
        draw_foot();
    draw_tabs();
    ui.force = 0;
}
static void ui_draw_page(uint32_t counting);
static void ui_draw(void)
{
    static uint8_t counting;
    style_apply();
    ui.frame++;
    if (!ANIM_OFF())                                    /* (0.9.9-beta4) the animation's frame (ui.c anim_fr): this one, */
        anim_at = ui.frame;                             /* or the last it moved on while held */
    if (counting != (uint8_t)seq_counting()) {          /* a count-in began or ended: the page drawn again after it */
        counting = (uint8_t)seq_counting();
        ui.force = 1;
    }
    if (!scr_frame())                                   /* MENU > SCREEN OFF: dark (or waking), nothing drawn */
        return;
    ui_draw_page(counting);
    scr_shown();
}
static void hint_tick(void);                            /* (ui_layer.c) */
static uint8_t dlg_behind;                              /* (0.9.5) the page being drawn round a NOTICE */
/* (0.9.5) a NOTICE (the OCT- / OCT+ questions) over the page: its tabs, its card, its breadcrumb the screen's holes (gfx.c
 * scr_holes), the page drawn round them as ever (DICE's BEFORE / NOW are the whole screen; over the menu, NAME, UPDATE MODE:
 * as before) */
static int dlg_notice(void)
{
    return dlg_on() && !(ui.confirm >= CF_DICE_SOUND && ui.confirm <= CF_DICE_FX) && !ui.uboot && !ui.menu && !name_on();
}
static void ui_draw_body(uint32_t counting);
static void ui_draw_page(uint32_t counting)
{
    if (dlg_notice()) {
        const int16_t R[3][4] = {{0, LT_Y, 240, (int16_t)DLG_TAB_H}, {14, 66, 212, 140}, {0, LZ_FOOT_Y, 240, LZ_FOOT_H}};
        uint8_t f = ui.force;
        hint_tick();
        scr_holes(R, 3);
        dlg_behind = 1;
        ui_draw_body(counting);
        dlg_behind = 0;
        scr_holes(R, 0);
        if (f) {                                        /* (the page first: the box drawn over it, the lint's order too) */
            draw_confirm();
            ui.head_sig = ~0u;
        }
        return;
    }
    hint_tick();
    ui_draw_body(counting);
}
static void ui_draw_body(uint32_t counting)
{
    int skip = 0;
    if (ui.hint_pg && ui.hint_pg != (ui.home ? 255u : ui.page + 1u)) {   /* (0.9.0) a layer's hint: its own page's only */
        ui.hint_pg = 0;
        ui.msg_t = 0;
        ui.msg2[0] = 0;
    }
    if (ui.uboot) {
        draw_uboot();
        draw_head();
        return;
    }
    if (ui.menu) {
        draw_menu();
        ui.force = 0;
        return;
    }
    if (counting && !dlg_on() && !name_on()) {          /* the count-in: over the page (and a layer) */
        draw_countin();
        draw_head();
        return;
    }
    if (dlg_on() && !dlg_behind) {                      /* the OCT- / OCT+ dialog (NOTICE: over the page, ui_draw_page) */
        if (ui.force) {
            draw_confirm();
            ui.force = 0;
        }
        draw_head();                                    /* recording remains visible over confirmations */
        return;
    }
    if (name_on()) {                                    /* NAME: a user preset's or a project's (ui_name.c) */
        name_draw();
        return;
    }
    if (ui.layer || mac_latch) {                        /* a layer's map over the page (ui_layer.c); (JIANT 0.4) the MACRO
                                                         * layer latched: its cards (no layer open) */
        uint8_t hm = !ui.layer;
        static uint8_t hm_was;
        if (hm != hm_was)
            ui.force = 1, hm_was = hm;
        if (hm)
            ui.layer = LAYER_MACRO;
        if (ui.force)
            draw_frame();
        draw_head();
        draw_layer();
        if (hm)
            ui.layer = 0;
        if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {
            str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
            ui.msg2[0] = 0;
            ui.msg_t = 60;
        }
        if (ui.hot_t)
            ui.hot_t--;
        if (ui.bpm_t)
            ui.bpm_t--;
        ui.force = 0;
        return;
    }
    if (ui.confirm == CF_DICE_ALL) {                    /* (0.8.5) GEN: HOME's lens, its last level (0.9.2: under
                                                         * a layer's map, a page as the others) */
        if (ui.force)
            draw_frame();
        draw_head();
        gen_cards();
        draw_graph();
        lz_frame_end();
        return;
    }
    if (!ui.home && !page_visible(ui.page)) {          /* an OP page of a track that is not DIGITAL (without
                                                         * FELUCCA_FM4: any track): EDIT 1 */
        ui.page = (uint8_t)page_first(FAM_EDIT);
        page_entered();
    }
    cursor_fix();
    {   /* (0.8.9) SKIN HUD's HOME: every strip on the beings, so all of them each frame (LOAD LITE / ECO: every 50 / 70 ms,
         * as the panel's own pace) */
        static uint8_t was __attribute__((section(".pool")));
        static uint32_t hud_ms __attribute__((section(".pool"))), view __attribute__((section(".pool")));
        uint8_t h = (uint8_t)hud_home();
        uint32_t v = (uint32_t)h + home_set() * 2u + (uint32_t)lz_level() * 4u;
        if (h != was)
            ui.force = 1, was = h;
        if (v != view)                                  /* (0.9.0) HUD's HOME entered, its level or set changed: the knob
                                                         * row whole again */
            ui.knob_ms = fm1_ms, view = v;
        if (h && sys_load && !ui.force && fm1_ms - hud_ms < (sys_load == LOAD_ECO ? 70u : 50u))
            skip = 1;
        else if (h) {                                   /* (0.9.0: drawn whole, sent where it changed: gfx.c cv_blit_cells) */
            hud_ms = fm1_ms;
            dc_frame(ui.force);
            ui.force = 1;
        }
    }
    if (ui.force && !skip)
        draw_frame();
    felucca_dbg.stage = 3;
    if (!skip) {
        draw_head();
        felucca_dbg.stage = 4;
        draw_columns();
        felucca_dbg.stage = 5;
        draw_graph();
    }
    if (ui.msg_t && !--ui.msg_t && ui.msg2[0]) {     /* the second message (ui_notices) */
        str_cpy(ui.msg, ui.msg2, sizeof ui.msg);
        ui.msg2[0] = 0;
        ui.msg_t = 60;
    }
    if (ui.bpm_t)
        ui.bpm_t--;
    if (ui.hot_t)
        ui.hot_t--;
    felucca_dbg.stage = 6;
    if (skip) {
        ui.force = 0;
        return;
    }
    if (pf_on)                                          /* (0.6.61) the page's name for a moment; (0.6.64) over the
                                                         * footer, which says the same: the header, the cards and
                                                         * the graph stay in sight */
        page_flash_draw();
    if (!pf_on)                                         /* (its end: the footer back at once) */
        draw_foot();
    if (lz_on())                                        /* (0.8.5) a lens page: the level's tabs on top */
        draw_tabs();
    ui.force = 0;
}


