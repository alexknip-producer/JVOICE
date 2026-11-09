/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) the lens panels of ENV, LFO . MOD, SCL . ARP and GLO (ui_lens.c), as the approved mock-up's; included by
 * ui_graph.c after ui_lzedit.c (ed_cap, ed_chip). The canvas is the lens's panel (240 x 140, x 4..236).
 *   ENV  TRACKS: the six tracks' envelopes as tiles (the selected one tinted, its live level a dot; DEST: the three
 *        amounts under each); ENV keeps graph_adsr / the DEST columns; STAGE: the envelope with the stage picked
 *        shaded, the stage's own curve large under it, its time */
/* track t's envelope in the box (x, y, w, h): attack straight, decay and release as three bends, sustain flat */
static void env_mini(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, int32_t th)
{
    int32_t a = 2 + t->p[P_ATK] * (w / 4) / 127, d = 3 + t->p[P_DEC] * (w / 4) / 127, r = 3 + t->p[P_REL] * (w / 4) / 127;
    int32_t s = t->p[P_SUS] * h / 127, b = y + h, x1 = x + a, x3 = x + w - r, k, px = x1, py = y;
    cv_line_t(x, b, x1, y, c, th);
    for (k = 1; k <= 4; k++) {                          /* (1/2, 3/4, 7/8, 15/16 of the way) */
        int32_t xx = x1 + d * k / 4, yy = b - s - (h - s) / (1 << k);
        cv_line_t(px, py, xx, yy, c, th);
        px = xx;
        py = yy;
    }
    cv_line_t(px, py, x3, b - s, c, th);
    px = x3;
    py = b - s;
    for (k = 1; k <= 4; k++) {
        int32_t xx = x3 + r * k / 4, yy = b - (k == 4 ? 0 : s / (1 << k));
        cv_line_t(px, py, xx, yy, c, th);
        px = xx;
        py = yy;
    }
}
static int32_t env_now(const track_t *t)                /* the loudest voice's level now, 0..1000 (-1: none) */
{
    int32_t lv = -1;
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].stage && t->v[i].stage < 4u)
            lv = clamp((int32_t)(((int64_t)t->v[i].env * 1000) >> 24), lv, 1000);
    return lv;
}
static void graph_env_tracks(int dest)
{
    uint32_t c, k;
    for (c = 0; c < NTRK; c++) {
        const track_t *t = &trk[c];
        int32_t x = 4 + (int32_t)(c % 3u) * 78, y = 2 + (int32_t)(c / 3u) * 68, sel = c == song.sel, lv;
        uint16_t bg = sel ? T_TINT : T_RAISE, ink = c >= trk_live ? T_DIM : sel ? T_THEME : T_MID;
        char n[3] = {'T', (char)('1' + c), 0};
        cv_rect(x, y, 76, 66, bg);
        if (sel) {
            cv_rect(x, y, 76, 1, T_THEME); cv_rect(x, y + 65, 76, 1, T_THEME);
            cv_rect(x, y, 1, 66, T_THEME); cv_rect(x + 75, y, 1, 66, T_THEME);
        }
        cv_text(x + 4, y + 4 - AF_X_CAP_Y, &AF_X, n, ink);
        cv_clip_text(x + 22, y + 4 - AF_X_CAP_Y, &AF_X, trk_engp(t)->name, T_DIM, bg, 50);
        env_mini(t, x + 5, y + 16, 66, dest ? 34 : 44, c >= trk_live ? T_DIM : settings.palette == UI_JIANT_INDEX ?
                 heat_col(150) : T_THEME, sel ? 2 : 1);
        if (!dest && (lv = env_now(t)) >= 0)
            og_node((x + 70) * 16, (y + 60 - lv * 44 / 1000) * 16, 2, T_TEXT);
        for (k = 0; dest && k < 3u; k++) {              /* FILTER PITCH SHAPE: a bar each from the middle */
            int32_t v = t->p[P_ED_FLT + k], w = v * 34 / 128;
            cv_rect(x + 5, y + 54 + (int32_t)k * 4, 66, 1, T_LINE);
            cv_rect(w < 0 ? x + 38 + w : x + 38, y + 53 + (int32_t)k * 4, w < 0 ? -w : w ? w : 1, 3,
                    k == 0u ? T_THEME : k == 1u ? T_ACCENT : T_MID);
        }
    }
}
static void graph_stage(const track_t *t)
{
    static const char *const SN[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
    uint32_t sg = ui.stage & 3u, id = P_ATK + sg;
    int32_t a = 2 + t->p[P_ATK] * 56 / 127, d = 3 + t->p[P_DEC] * 56 / 127, r = 3 + t->p[P_REL] * 56 / 127;
    int32_t x0 = 10, x1 = x0 + a, x2 = x1 + d, x3 = 226 - r, sx[5] = {x0, x1, x2, x3, 226}, x, py = 0;
    int32_t s = t->p[P_SUS] * 1000 / 127, v = t->p[id] * 1000 / 127;
    const char *u = "";
    char b[24];
    cv_rect(sx[sg], 4, sx[sg + 1u] - sx[sg], 40, T_TINT);   /* the stage picked, on the whole envelope */
    env_mini(t, 10, 6, 216, 36, T_MID, 1);
    for (x = 0; x <= 216; x++) {                        /* its own shape, large: up, down to SUS, held, down to 0 */
        int32_t q = x * 1000 / 216, e = 1000 - (int32_t)((int64_t)(1000 - q) * (1000 - q) * (1000 - q) / 1000000), y;
        y = sg == 0u ? q : sg == 1u ? 1000 - (1000 - s) * e / 1000 : sg == 2u ? s : s - s * e / 1000;
        if (sg == 0u)                                   /* (ATK: slow or fast as it is) */
            y = 1000 - (int32_t)((int64_t)(1000 - q) * (1000 - q) / 1000) * (1000 - v / 2) / 1000 - (1000 - q) * v / 2000;
        y = 122 - y * 66 / 1000;
        if (x)
            cv_line_t(10 + x - 1, py, 10 + x, y, settings.palette == UI_JIANT_INDEX ? heat_y(y, 56, 122, T_THEME) : T_THEME, 2);
        py = y;
    }
    cv_rect(10, 124, 217, 1, T_RAISE);
    ed_cap(10, 50, SN[sg], T_THEME);
    param_format(&TP[id], t->p[id], b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_text_r(226, 50 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_SURF);
    cv_text(10, 128 - AF_X_CAP_Y, &AF_X, "0", T_DIM);
    cv_text_r(226, 128 - AF_X_CAP_Y, &AF_X, sg == 2u ? "HELD" : b, T_DIM, T_SURF);
}
/* the lens's panel for ENV's page pg (1), else 0 (its own) */
static int graph_env_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_STAGE)
        graph_stage(t);
    else if (pd_level() == 0u)
        graph_env_tracks(pg->id[0] != P_ATK);
    else
        return 0;
    return 1;
}

/* LFO . MOD  MOD: the matrix as a net, the sources down the left (those a slot reads lit), a curve from each slot's
 *            source to its box D1..D4 on the right (DEST, AMT as a bar), the slot KNOB 1 picked framed, a pulse
 *            running along its curve; LFO: graph_lfo under "RATE . WAVE"; LFO DEST: the LFO small on the left, the four
 *            amounts as rows, each lit by what it sends now; MSEQ, MSEQ 2: graph_mseq under its caption */
static void graph_mod_net(const track_t *t)
{
    uint32_t k;
    for (k = 0; k < NMSLOT; k++) {
        const int16_t *p = &t->p[P_M1SRC + 3u * k];
        int32_t y = 4 + (int32_t)k * 33, on = p[0] && p[1], sel = k == mod_ui_slot, sy = y + 11, w;
        uint16_t bg = sel ? T_TINT : T_RAISE, lc = sel ? T_THEME : on ? T_MID : T_DIM;
        char b[16];
        const char *u = "";
        cv_text(6, sy - AF_X_CAP_Y, &AF_X, N_MSRC[clamp(p[0], 0, MS_N - 1)], on ? T_TEXT : T_DIM);
        if (on) {
            cv_rect(58, sy, 100, 1, lc);
            if (sel && !ANIM_OFF())
                og_node((58 + (int32_t)(anim_fr * 3u % 100u)) * 16, sy * 16, 2, T_TEXT);
        } else
            cv_text(124, y + 9 - AF_X_CAP_Y, &AF_X, "----", T_DIM);
        cv_rect(160, y, 72, 26, bg);
        if (sel) {
            cv_rect(160, y, 72, 1, T_THEME); cv_rect(160, y + 25, 72, 1, T_THEME);
            cv_rect(160, y, 1, 26, T_THEME); cv_rect(231, y, 1, 26, T_THEME);
        }
        b[0] = 'D'; b[1] = (char)('1' + k); b[2] = ' '; b[3] = 0;
        str_cpy(b + 3, on ? mod_dst_name(t, p[1]) : "OFF", 13);
        cv_clip_text(165, y + 5 - AF_X_CAP_Y, &AF_X, b, on ? T_TEXT : T_DIM, bg, 64);
        param_format(&TP[P_M1AMT], p[2], b, &u);
        w = p[2] * 30 / 128;
        cv_rect(166, y + 17, 60, 2, T_LINE);
        if (on && w)
            cv_rect(w < 0 ? 196 + w : 196, y + 16, w < 0 ? -w : w, 4, p[2] < 0 ? T_ACCENT : T_THEME);
    }
}
static void lfo_cap(const track_t *t, int32_t x, int32_t y, int mode)   /* "1.18Hz . SIN" (mode: ". FREE" / ". TRIG") */
{
    const char *u = "";
    char b[32], w[12];
    param_format(&TP[P_LRATE], t->p[P_LRATE], b, &u);
    str_cpy(b + str_len(b), u, 32 - str_len(b));
    str_cpy(b + str_len(b), " \xB7 ", 32 - str_len(b));
    param_format(&TP[P_LWAVE], t->p[P_LWAVE], w, &u);
    str_cpy(b + str_len(b), w, 32 - str_len(b));
    if (mode)                                           /* (0.9.18) LFO, LFO 2: MODE too (LFO DEST: no room) */
        str_cpy(b + str_len(b), t->p[P_LPHASE] < 0 ? " \xB7 FREE" : " \xB7 TRIG", 32 - str_len(b));
    ed_cap(x, y, b, T_TEXT);
}
static __attribute__((noinline)) void graph_lfodest(const track_t *t, const page_t *pg)
{
    int32_t x, py = 0, src = mulq15(t->lfo_val, t->lfo_fade), k;
    uint32_t span = 0xFFFFFFFFu / 40u, ph = t->lfo_ph - 20u * span;
    lfo_cap(t, 10, 6, 0);
    cv_rect(10, 70, 80, 1, T_RAISE);
    for (x = 0; x < 80; x++) {                          /* (two cycles, the phase now a third in) */
        int32_t y = 70 - lfo_wave((track_t *)t, ph + (uint32_t)x * span) * 44 / 32768;
        if (x)
            cv_line_t(10 + x - 1, py, 10 + x, y, T_THEME, 2);
        py = y;
    }
    cv_rect(30, 22, 1, 96, T_MID);
    og_node(30 * 16, (70 - src * 44 / 32768) * 16, 3, T_TEXT);
    for (k = 0; k < 4; k++) {                           /* the amounts, lit by the LFO now */
        int16_t *vp;
        const param_desc_t *d = page_desc(pg, (uint32_t)k, &vp);
        int32_t y = 8 + k * 32, w = *vp * 46 / 100, live = w * src / 32767;
        const char *u = "";
        char b[16];
        if (!d)
            continue;
        cv_rect(100, y, 132, 26, T_RAISE);
        ed_cap(105, y + 4, d->label, T_MID);
        param_format(d, *vp, b, &u);
        str_cpy(b + str_len(b), u, 16 - str_len(b));
        cv_text_r(228, y + 4 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_RAISE);
        cv_rect(106, y + 17, 120, 2, T_LINE);
        if (w)
            cv_rect(w < 0 ? 166 + w : 166, y + 16, w < 0 ? -w : w, 4, ux_mix(T_RAISE, T_THEME, 45));
        if (live)
            cv_rect(live < 0 ? 166 + live : 166, y + 16, live < 0 ? -live : live, 4, settings.palette == UI_JIANT_INDEX ?
                    heat_col(110 + (live < 0 ? -live : live) * 3) : T_THEME);
    }
}
static void mseq_cap(const track_t *t, const page_t *pg)
{
    const char *u = "";
    char b[24];
    if (pg->id[0] == P_MSDIV) {                         /* MSEQ 2: "DIV 1/16" */
        str_cpy(b, "DIV ", 24);
        param_format(&TP[P_MSDIV], t->p[P_MSDIV], b + 4, &u);
        ed_cap(10, 6, b, T_ACCENT);
    } else
        ed_cap(10, 6, "MOD SRC: STEP", T_TEXT);
    str_cpy(b, "LEN ", 24);
    fmt_int(b + 4, t->p[P_MSLEN]);
    cv_text_r(228, 6 - AF_X_CAP_Y, &AF_X, b, T_DIM, T_SURF);
}
/* the lens's panel for LFO . MOD's page pg (1), else 0 (its own) */
static int graph_mod_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_MOD)
        graph_mod_net(t);
    else if (pg->graph == GR_LFO) {
        lfo_cap(t, 10, 6, 1);
        cv_oy = GOY + 12;
        graph_lfo(t, T_THEME);
    } else if (pg->id[0] == P_LD_PIT)
        graph_lfodest(t, pg);
    else if (pg->graph == GR_MSEQ || pg->id[0] == P_MSDIV) {
        mseq_cap(t, pg);
        cv_oy = GOY + 12;
        graph_mseq(t, T_THEME);
    } else
        return 0;
    return 1;
}

/* SCL . ARP  KEY: the twelve notes round a circle (C on top), the scale's lit and joined, ROOT in the accent, its name
 *            and the scale's in the middle; QUANT and ALL TR at the right; the scale on a keyboard under it. CHORD: the
 *            chord each degree of the key plays now (chord_build, as a key would), I..VII. ARP / ARP 2: "MODE . RATE .
 *            OCT" and the pattern it plays over a triad (OFF: "ARP OFF") */
static void key_val(int32_t x, int32_t y, const page_t *pg, uint32_t k)   /* a knob's label over its value */
{
    int16_t *vp;
    const param_desc_t *d = page_desc(pg, k, &vp);
    const char *u = "";
    char b[16];
    if (!d)
        return;
    param_format(d, *vp, b, &u);
    str_cpy(b + str_len(b), u, 16 - str_len(b));
    ed_cap(x, y, d->label, T_DIM);
    cv_text(x, y + 10 - AF_M_CAP_Y, &AF_M, b, T_TEXT);
}
/* (V10.6i) a note's colour: each of the twelve its own heat, C coldest .. B hottest (the user's pick B: the same note
 * the same colour on KEY, CHORD, ARP, SCENE); other palettes their theme */
static uint16_t note_col(uint32_t n)
{
    return settings.palette == UI_JIANT_INDEX ? heat_col(40 + (int32_t)(n % 12u) * 200 / 11) : T_THEME;
}
static __attribute__((noinline)) void graph_key(const track_t *t, const page_t *pg)
{
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const uint8_t BLK[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    uint32_t mask = scale_mask(t), i, rt = (uint32_t)clamp(t->p[P_ROOT], 0, 11), first = 12u, prev = 12u;
    int32_t px[12], py[12];
    const char *u = "";
    char b[16];
    for (i = 0; i < 12u; i++) {
        uint32_t a = (uint32_t)(i * 65536u / 12u) + 0xC000u;   /* (C on top, clockwise; a turn 65536, in decimal: the
                                                         * build's register lint reads 0x10000 as an SFR) */
        px[i] = 62 + (og_cos(a) * 42 >> 15);
        py[i] = 58 + (og_sin(a) * 42 >> 15);
    }
    for (i = 0; i < 12u; i++) {                         /* the scale joined */
        uint32_t n = (rt + i) % 12u;
        if (!(mask >> i & 1u))
            continue;
        if (prev < 12u)
            og_line(px[prev] * 16, py[prev] * 16, px[n] * 16, py[n] * 16, note_col(prev));
        else
            first = n;
        prev = n;
    }
    if (first < 12u && prev != first)
        og_line(px[prev] * 16, py[prev] * 16, px[first] * 16, py[first] * 16, note_col(prev));
    for (i = 0; i < 12u; i++) {
        uint32_t deg = (i + 12u - rt) % 12u, in = mask >> deg & 1u;
        uint16_t c = in ? note_col(i) : T_RAISE;
        if (deg == 0u)                                  /* the ROOT: its colour in a ring of text */
            og_node(px[i] * 16, py[i] * 16, 9, T_TEXT);
        og_node(px[i] * 16, py[i] * 16, in ? 7 : 4, c);
        if (in)
            cv_text_in(px[i] - 7, py[i] - 3 - AF_X_CAP_Y, 14, &AF_X, NN[i], T_INK, c);
    }
    cv_text_in(42, 48 - AF_M_CAP_Y, 40, &AF_M, NN[rt], note_col(rt), T_SURF);
    param_format(&TP[P_SCALE], t->p[P_SCALE], b, &u);
    cv_text_in(32, 66 - AF_X_CAP_Y, 60, &AF_X, b, T_MID, T_SURF);
    key_val(130, 10, pg, 2);
    key_val(130, 48, pg, 3);
    for (i = 0; i < 24u; i++) {                         /* two octaves from C */
        uint32_t deg = (i + 24u - rt) % 12u;
        cv_rect(8 + (int32_t)i * 9, BLK[i % 12u] ? 112 : 122, 8, BLK[i % 12u] ? 10 : 12,
                mask >> deg & 1u ? note_col(i) : BLK[i % 12u] ? T_LINE : T_RAISE);
        if (deg == 0u)
            cv_rect(8 + (int32_t)i * 9, BLK[i % 12u] ? 110 : 120, 8, 1, T_TEXT);
    }
}
static __attribute__((noinline)) void graph_chord(const track_t *t)
{
    static const char *const RN[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    uint32_t mask = scale_mask(t), deg[12], c = 0, i, d, rt = (uint32_t)clamp(t->p[P_ROOT], 0, 11);
    const char *u = "";
    char b[32], v[12];
    for (i = 0; i < 12u; i++)
        if (mask >> i & 1u && c < 7u)
            deg[c++] = i;
    if (t->p[P_CHRD]) {
        param_format(&CHORD_DESC[0], t->p[P_CHRD], b, &u);
        str_cpy(b + str_len(b), " \xB7 ", 32 - str_len(b));
        param_format(&CHORD_DESC[1], t->p[P_CVOI], v, &u);
        str_cpy(b + str_len(b), v, 32 - str_len(b));
    } else
        str_cpy(b, "CHORD OFF \xB7 ONE NOTE A KEY", 32);
    ed_cap(10, 6, b, T_TEXT);
    for (d = 0; d < c; d++) {
        uint8_t nt[8];
        int32_t x = 14 + (int32_t)d * 31;
        uint32_t n = chord_peek(t, 48u + rt + deg[d], nt), k;
        cv_rect(x - 2, 18, 27, 100, d == 0u ? T_TINT : T_SURF);
        for (k = 0; k < n && k < 8u; k++) {
            int32_t y = 112 - clamp((int32_t)nt[k] - 48, 0, 31) * 3;
            cv_rect(x + 2, y, 21, 3, note_col(nt[k]));
            if (k == 0u)                                /* the chord's root: a tick of text */
                cv_rect(x - 1, y, 2, 3, T_TEXT);
        }
        cv_text_in(x - 2, 125 - AF_X_CAP_Y, 27, &AF_X, RN[d], d == 0u ? T_THEME : T_DIM, T_SURF);
    }
}
static void graph_arp(const track_t *t)
{
    static const uint8_t TRI[3] = {0, 4, 7};
    const char *u = "";
    char b[40], v[12];
    uint32_t i, md = (uint32_t)t->p[P_AMODE], oc = (uint32_t)clamp(t->p[P_AOCT], 1, 4), n = 3u * oc;
    param_format(&TP[P_AMODE], t->p[P_AMODE], b, &u);
    str_cpy(b + str_len(b), " \xB7 ", 40 - str_len(b));
    param_format(&TP[P_ARATE], t->p[P_ARATE], v, &u);
    str_cpy(b + str_len(b), v, 40 - str_len(b));
    str_cpy(b + str_len(b), " \xB7 OCT ", 40 - str_len(b));
    fmt_int(b + str_len(b), (int32_t)oc);
    ed_cap(10, 6, b, T_TEXT);
    param_format(&TP[P_AORDER], t->p[P_AORDER], b, &u);
    cv_text_r(228, 6 - AF_X_CAP_Y, &AF_X, b, T_DIM, T_SURF);
    if (!md) {
        cv_text_in(4, 56 - AF_M_CAP_Y, 232, &AF_M, "ARP OFF", T_MID, T_SURF);
        cv_text_in(4, 76 - AF_X_CAP_Y, 232, &AF_X, "KEYS PLAY STRAIGHT", T_DIM, T_SURF);
        return;
    }
    for (i = 0; i < 16u; i++) {                         /* the pattern over a triad, OCT octaves */
        uint32_t pp = n > 1u ? 2u * n - 2u : 1u, j = i % pp;
        uint32_t k = md == 2u ? n - 1u - i % n : md == 3u ? (j < n ? j : pp - j) : md == 4u ? (i * 2654435761u >> 16) % n :
                     md == 6u ? i / 2u % n : i % n;
        int32_t semi = TRI[k % 3u] + 12 * (int32_t)(k / 3u), x = 10 + (int32_t)i * 14 + (i & 1u ? t->p[P_ASWING] * 6 / 127 : 0);
        int32_t w = 2 + t->p[P_AGATE] * 10 / 127, y = 118 - semi * 2;
        uint32_t on = (i * 40503u >> 8) % 100u < (uint32_t)t->p[P_APROB] * 100u / 127u;
        cv_rect(x, y, w, 4, on ? note_col((uint32_t)(clamp(t->p[P_ROOT], 0, 11) + semi)) : T_RAISE);
    }
    cv_rect(10, 124, 220, 1, T_RAISE);
}
/* the lens's panel for SCL . ARP's page pg (1), else 0 */
static int graph_key_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_SCALE)
        graph_key(t, pg);
    else if (pg->id[0] == P_CHRD)
        graph_chord(t);
    else if (pg->fam == FAM_ARP)
        graph_arp(t);
    else
        return 0;
    return 1;
}

/* GLO  MIX: the six strips (draw_tracks, track_col6; SENDS: the four sends as bars); GLOBAL keeps its bar of sixteenths;
 *      SYSTEM: the tracks that play as chips, LOAD's three settings (the one set lit), MIDI's route, the CPU load over
 *      the last seconds as a line */
static __attribute__((noinline)) void graph_system(void)
{
    static const char *const LD[3] = {"ECO", "LITE", "FULL"};
    static uint8_t cpu[56], cpu_at;
    static uint32_t cpu_ms;
    uint32_t c, k, ld = 3;
    int32_t py = 0;
    const char *u = "";
    char b[24];
    int16_t *vp;
    const param_desc_t *dl = page_desc(cur_page(), 1, &vp);   /* (LOAD, as its card names it) */
    if (dl) {
        param_format(dl, *vp, b, &u);
        for (k = 0; k < 3u; k++)
            if (str_eq(b, LD[k]))
                ld = k;
    }
    for (c = 0; c < NTRK; c++) {
        char n[3] = {'T', (char)('1' + c), 0};
        uint16_t bg = c < trk_live ? T_THEME : T_RAISE;
        cv_rect(8 + (int32_t)c * 22, 8, 20, 16, bg);
        cv_text_in(8 + (int32_t)c * 22, 12 - AF_X_CAP_Y, 20, &AF_X, n, c < trk_live ? T_INK : T_DIM, bg);
    }
    fmt_int(b, (int32_t)trk_live);
    str_cpy(b + str_len(b), " TRACKS PLAY", 24 - str_len(b));
    ed_cap(144, 13, b, T_TEXT);
    for (k = 0; k < 3u; k++) {
        uint16_t bg = k == ld ? T_THEME : T_RAISE;
        cv_rect(8 + (int32_t)k * 40, 34, 38, 14, bg);
        cv_text_in(8 + (int32_t)k * 40, 38 - AF_X_CAP_Y, 38, &AF_X, LD[k], k == ld ? T_INK : T_MID, bg);
    }
    param_format(&GP[G_ROUTE], song.g[G_ROUTE], b + 5, &u);
    b[0] = 'M'; b[1] = 'I'; b[2] = 'D'; b[3] = 'I'; b[4] = ' ';
    ed_cap(134, 38, b, T_DIM);
    if (fm1_ms - cpu_ms >= 250u) {                      /* (a sample each quarter second) */
        cpu_ms = fm1_ms;
        cpu[cpu_at++ % 56u] = (uint8_t)clamp((int32_t)(song.cpu_q8 * 100u / 256u), 0, 100);
    }
    for (k = 0; k < 56u; k++) {
        int32_t y = 126 - cpu[(cpu_at + k) % 56u] * 56 / 100;
        if (k)
            cv_line_t(8 + (int32_t)(k - 1u) * 4, py, 8 + (int32_t)k * 4, y, T_THEME, 2);
        py = y;
    }
    cv_rect(8, 127, 224, 1, T_RAISE);
    str_cpy(b, "CPU ", 24);
    fmt_int(b + 4, (int32_t)(song.cpu_q8 * 100u / 256u));
    str_cpy(b + str_len(b), "%", 2);
    cv_text_r(230, 62 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_SURF);
}

