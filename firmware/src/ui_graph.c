/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI: the panel (Y_GRAPH .. Y_GRAPH + H_GRAPH) between the cards and the footer: a SURF area
 * (rounded, on BG) holding the page graphs (ADSR, LFO, steps, piano roll, drum grid, scale, FX, SLICER,
 * MOD, lists: presets, user presets, patterns, project slots, song), the HOME oscilloscope, the
 * instrument diagrams; the MIXER page draws four SURF columns instead. Each graph is redrawn only when
 * graph_signature() changes. The look:
 * curves THEME (2 px), guides and empty marks RAISE, captions MID / DIM, the active thing ACCENT,
 * bars rounded; a list's selected row is a THEME bar with INK text. */
#define PANEL_X0 10                                  /* the graphs' inner area: x 10..230 */
#define PANEL_W 220
#define GOY 11                                       /* graphs drawn on a 100 px scale sit at y 11..111 */
/* (0.8.9) SKIN HUD's HOME (ui.c hud_home): every strip begins on the beings (ui_lzhome.c eco_backdrop: the strip at screen
 * x, y), its words outlined (gfx.c cv_halo); elsewhere a strip cleared to BG */
static void eco_backdrop(int32_t x0, int32_t y0, int words);
static void lz_begin(uint32_t w, uint32_t h, uint32_t x, uint32_t y)
{
    cv_begin(w, h, T_BG);
    if (hud_home()) {
        eco_backdrop((int32_t)x, (int32_t)y, 0);
        cv_halo = 1;
    }
}
static void lz_blit(uint32_t x, uint32_t y)
{
    cv_halo = 0;
    if (hud_home())                                  /* (0.9.0) what changed only (gfx.c) */
        cv_blit_cells(x, y);
    else
        cv_blit(x, y);
}
/* (0.8.9) HUD: the knob row while KNOB 1..4 rest: whole for 3 s after a turn (0.9.0: or the page, level or set shown
 * changing), then down to 22 % in 0.6 s (steps of 13 %); every other case 100 (0.9.0: every page but HUD's HOME) */
static uint32_t hud_fade(void)
{
    uint32_t el = fm1_ms - ui.knob_ms;
    if (!hud_home() || el < 3000u)                   /* (0.9.0: HUD's HOME only, over the beings) */
        return 100u;
    return el >= 3600u ? 22u : 100u - (el - 3000u) * 78u / 600u / 13u * 13u;
}

/* (0.8.9) a tile in a lens's panel (FX's modules, the menu's tabs, ENV's tracks, MOD's slots): COLORS a rounded RAISE box
 * (r 4), the one picked framed in fc (the mock-up's card and its stroke); BOTTOM and HUD none (the words on the background;
 * the one picked: its name underlined, lz_uline). What lies under its words: lz_tile_bg */
static uint16_t lz_tile_bg(void) { return ux.skin ? T_SURF : T_RAISE; }
static void lz_tile(int32_t x, int32_t y, int32_t w, int32_t h, int sel, uint16_t fc)
{
    if (ux.skin)
        return;
    if (sel) {
        cv_rrect(x, y, w, h, 4, fc, cv_bg);
        cv_rrect(x + 1, y + 1, w - 2, h - 2, 3, T_RAISE, fc);
    } else
        cv_rrect(x, y, w, h, 4, T_RAISE, cv_bg);
}
static void lz_uline(int32_t x, int32_t y, const aafont_t *f, const char *s, uint16_t c)   /* BOTTOM, HUD: under a name */
{
    if (ux.skin)
        cv_rect(x, y, text_w(f, s) - 1, 1, c);
}

/* the height ADSR and LFO are drawn on: 100 px, or (MENU > LARGE's strip: draw_graph) the strip's ~ 56 */
static int32_t graph_ht = 100;
#include "ui_organic.c"                              /* JIANT FM's organic line art (drawn by code) */
static uint16_t heat_col(int32_t v);
static uint16_t note_col(uint32_t n);                /* (V10.6i) ui_lzpage.c: a note's own colour */
#include "ui_scene.c"                                /* (V10.25) SCL > SCENE: the multitimbral page */
#include "ui_euclid.c"                               /* (V10.6) SEQ > EUCLID / MODES: the generator, its graph */
#include "ui_fm6op.c"                                /* (0.6.10, from Felucca 1.5) EDIT > OPERATOR, OP ENV, OPERATOR 2 */
static uint16_t heat_col(int32_t v);
/* (0.9.5, as the 0.6.36) JIANT (thermal): a card's colour its value as heat (ratio 0..1000, -1 none); bipolar (its range
 * across 0): by the distance from the centre, cold in the middle */
static int heat_on(int32_t ratio) { return settings.palette == UI_JIANT_INDEX && ratio >= 0; }
/* (V10.7.7) a level bar filled w of its full length fw: in the thermal palette cold to hot along the length (PROX's and
 * SWARM's FAR .. NEAR), else plain c */
static __attribute__((noinline)) void heat_bar(int32_t x, int32_t y, int32_t w, int32_t fw, int32_t h, uint16_t c)
{
    int32_t i;
    if (settings.palette != UI_JIANT_INDEX || fw <= 0) { if (w > 0) cv_rect(x, y, w, h, c); return; }
    for (i = 0; i < w; i += 2)
        cv_rect(x + i, y, w - i < 2 ? w - i : 2, h, heat_col(24 + i * 232 / fw));
}
static uint16_t card_heat(int32_t ratio, int bip)
{
    int32_t r = clamp(ratio, 0, 1000);
    if (bip)
        r = (r > 500 ? r - 500 : 500 - r) * 2;
    return heat_col(r * 256 / 1000);
}
/* knob c's colour on a lens card, a layer's KNOBS, EDIT's chips: JIANT its value's heat; another palette the knob's own
 * (UI_KNOBS) */
static uint16_t knob_col(uint32_t c, int32_t ratio, int bip)
{
    return heat_on(ratio) ? card_heat(ratio, bip) : T_K(c);
}
/* JIANT (thermal): a curve's colour at canvas row y, hot at the top (y0) and cold at the bottom (y1); else c */
static uint16_t heat_y(int32_t y, int32_t y0, int32_t y1, uint16_t c)
{
    return settings.palette == UI_JIANT_INDEX && y1 > y0 ? heat_col(24 + (y1 - y) * 232 / (y1 - y0)) : c;
}
#include "ui_being.c"                                /* (JIANT 0.5) the beings: the engine pages' creatures */
#include "ui_engvis.c"                               /* (0.9.4) VOICE's head, ANIMAL's creature, DRUM-X's kit */

/* Matches voice.c: attack is linear, decay and release are exponential
 * (env += (target - env) * k each tick, ~99 % after the set time). Time
 * axis is the parameter value (the times themselves are exponential). */
static void graph_adsr(const track_t *t, uint16_t c)
{
    const page_t *pg = cur_page();
    int32_t a = 4 + t->p[pg->id[0]] * 50 / 127, d = 6 + t->p[pg->id[1]] * 50 / 127, r = 6 + t->p[pg->id[3]] * 60 / 127;
    int32_t top = 6 * graph_ht / 100, bot = 88 * graph_ht / 100, sus = t->p[pg->id[2]] * 1000 / 127;   /* 0..1000 */
    int32_t x0 = 12, x1 = x0 + a, x3 = 226 - r, i, px, py;
    int32_t e = 32768;                                                  /* exp(-4.6 u), Q15 */
#define EGY(lvl) (bot - (lvl) * (bot - top) / 1000)
    cv_rect(PANEL_X0, bot + 2, PANEL_W, 1, T_RAISE);
    cv_line_t(x0, bot, x1, top, heat_y((bot + top) / 2, top, bot, c), 2);                                  /* attack: linear */
    px = x1;
    py = top;
    for (i = 1; i <= d; i++) {                                          /* decay: exponential to SUS */
        int32_t lvl;
        e = (e * (32768 - 150733 / d)) >> 15;           /* k^d = exp(-4.6) */
        lvl = sus + ((1000 - sus) * e >> 15);
        cv_line_t(px, py, x1 + i, EGY(lvl), heat_y(py, top, bot, c), 2);
        px = x1 + i;
        py = EGY(lvl);
    }
    cv_line_t(px, py, x3, EGY(sus), heat_y(py, top, bot, c), 2);                               /* sustain */
    px = x3;
    py = EGY(sus);
    e = 32768;
    for (i = 1; i <= r; i++) {                                          /* release: exponential to 0 */
        e = (e * (32768 - 150733 / r)) >> 15;
        cv_line_t(px, py, x3 + i, EGY(sus * e >> 15), heat_y(py, top, bot, c), 2);
        px = x3 + i;
        py = EGY(sus * e >> 15);
    }
    if (pg->id[0] == P_ATK) {                           /* (JIANT 0.5) the track's envelope now: the loudest voice's */
        const voice_t *bv = 0;                          /* level as a dot on the curve, a line down to the axis */
        int32_t lvl, x;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].stage && t->v[i].stage < 4u && (!bv || t->v[i].env > bv->env))
                bv = &t->v[i];
        if (bv) {
            lvl = (int32_t)(((int64_t)bv->env * 1000) >> 24);
            if (bv->stage == 1u)
                x = x0 + a * lvl / 1000;
            else if (bv->stage == 2u)
                x = x1 + (sus < 1000 ? clamp((1000 - lvl) * d / (1000 - sus), 0, d) : 0);
            else
                x = x3 + (sus > 0 ? clamp((sus - lvl) * r / sus, 0, r) : r);
            cv_rect(x, EGY(lvl), 1, bot + 2 - EGY(lvl), T_MID);
            cv_rrect(x - 3, EGY(lvl) - 3, 7, 7, 3, T_TEXT, T_SURF);
        }
    }
#undef EGY
}

/* LFO: the wave as it runs (JIANT 0.5): the phase now at the playhead (a quarter in), two cycles across, scrolling
 * at the LFO's own rate; behind the playhead dimmer (what it played), the level now a dot, its depth after FADE */
static void graph_lfo(const track_t *t, uint16_t c)
{
    int32_t x, cy = graph_ht / 2, a = 38 * graph_ht / 100, py = cy, xp = PANEL_W / 4;
    uint32_t span = 0xFFFFFFFFu / (PANEL_W / 2u), ph = t->lfo_ph - (uint32_t)xp * span;
    int32_t now = cy - mulq15(t->lfo_val, t->lfo_fade) * a / 32768;
    cv_rect(PANEL_X0, cy, PANEL_W, 1, T_RAISE);
    for (x = 0; x < PANEL_W; x++) {                  /* (lfo_wave only reads the track) */
        uint32_t px = ph + (uint32_t)x * span;
        int32_t y = cy - lfo_wave((track_t *)t, px) * a / 32768;
        if (t->p[P_LWAVE] == 4)                      /* S&H: a value per cycle part, held */
            y = cy - ((int32_t)((((px >> 29) + 1u) * 2654435761u) >> 16) - 32768) * a / 32768;
        if (x)
            cv_line_t(PANEL_X0 + x - 1, py, PANEL_X0 + x, y,
                      x < xp ? ux_mix(T_SURF, heat_y(y, cy - a, cy + a, c), 45) : heat_y(y, cy - a, cy + a, c), 2);
        py = y;
    }
    cv_rect(PANEL_X0 + xp, cy - a - 4, 1, 2 * a + 8, T_MID);   /* the playhead */
    cv_rrect(PANEL_X0 + xp - 3, now - 3, 7, 7, 3, T_TEXT, T_SURF);
}


/* (0.6.45) EDIT > MORPH: a cycle of the wave as it morphs (from WAVE towards the next ones), heat coloured by its
 * height; under it the two waves it is between */
/* (0.6.48) EDIT > IMAGE (a DRUM track): the eight sounds from left to right as IMAGE places them (as wide as DRV,
 * DIST and the FILTER let it play: dx_img_k), a row each, the kick on top, on a line from the middle */
static uint32_t vx_on_mask(const track_t *t)            /* (V10.49) the part's voices sounding, a bit each; (V10.52) their
                                                         * levels (a step each 1/8) and notes: the blobs move */
{
    uint32_t m = 0, i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active)
            m = (m ^ (1u << i) ^ (uint32_t)(clamp(t->v[i].env_out, 0, 32767) >> 12) << (i * 3u % 24u) ^ t->v[i].note << 8) * 16777619u;
    return m;
}
static void graph_dximg(const track_t *t)
{
    static const char *const AB[DV_NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    int32_t img = clamp(t->p[P_DXIMG], 0, 127), k = img ? dx_img_k(t) : 0, cx = PANEL_X0 + PANEL_W / 2;
    uint32_t l;
    if (trk_eng(t) != ENGI_DRUM) {                       /* (V10.52) a synth: the stereo field, L .. R; each voice sounding a
                                                          * blob at its place (IMAGE: voice.c), its size its level now, its
                                                          * note over it; the silent ones a tick where they would sit; the
                                                          * span IMAGE opens as a bar under the field (POLY only) */
        static const int8_t VP[8] = {-64, 64, -32, 32, -48, 48, -16, 16};
        int poly = t->p[P_VOICE] == V_POLY;
        int32_t half = PANEL_W / 2 - 16, fy = 62, sp;
        char b[8];
        img = poly ? 127 - clamp(t->p[P_SIMG], 0, 127) : 0;   /* (P_SIMG: stored inverted) */
        sp = img * half / 127;
        cv_text(PANEL_X0, 4 - AF_S_CAP_Y, &AF_S, "L", T_DIM);
        cv_text_r(PANEL_X0 + PANEL_W, 4 - AF_S_CAP_Y, &AF_S, "R", T_DIM, T_SURF);
        cv_rect(PANEL_X0 + 6, fy, PANEL_W - 12, 1, T_LINE);   /* the field */
        cv_rect(cx, fy - 22, 1, 44, T_RAISE);                  /* the middle */
        if (sp)
            cv_rect(cx - sp, fy + 26, 2 * sp + 1, 3, heat_col(60u + (uint32_t)img));   /* the span */
        for (l = 0; l < 8u && l < NVOICE; l++) {
            const voice_t *v = &t->v[l];
            int32_t x = cx + VP[l] * sp / 64;
            if (!v->active) {
                cv_rect(x, fy + 3, 1, 4, T_RAISE);
                continue;
            }
            {
                int32_t a = clamp(v->env_out, 0, 32767), r = 3 + a * 9 / 32767;
                uint16_t c = heat_col(110u + (uint32_t)(a >> 8));
                cv_rrect(x - r, fy - r, 2 * r + 1, 2 * r + 1, r, c, T_SURF);
                note_name(b, v->note);
                if (sp)                                  /* (in the middle, IMAGE 0: the notes in a row over it) */
                    cv_text_in(x - 14, fy - r - 12, 28, &AF_S, b, T_TEXT, T_SURF);
                else
                    cv_text_in(PANEL_X0 + 6 + (int32_t)l * 26, 22, 26, &AF_S, b, T_TEXT, T_SURF);
            }
        }
        cv_text(PANEL_X0, 100 - AF_S_CAP_Y, &AF_S, poly ? (img ? "IMAGE: VOICES SPREAD" : "IMAGE 0: ALL CENTRED") : "POLY ONLY",
                poly && img ? T_MID : T_DIM);
        return;
    }
    cv_rect(cx, 0, 1, 96, T_RAISE);
    for (l = 0; l < DV_NLANE; l++) {
        int32_t p = img ? dx_img_pan(l, img) * k >> 8 : 0, y = (int32_t)l * 12;
        int32_t x = cx + p * (PANEL_W / 2 - 14) / 64;
        uint16_t c = k < 256 && img ? T_MID : heat_col(60u + l * 22u);   /* (narrowed by DRV DIST FILTER: grey) */
        cv_rect(p < 0 ? x : cx, y + 6, p < 0 ? cx - x : x - cx, 1, c);
        cv_rrect(x - 13, y, 26, 12, 2, c, T_SURF);
        cv_text_in(x - 13, y + CAP_IN(S, 12), 26, &AF_S, AB[l], T_INK, c);
    }
}

/* (0.8.5) VOICE's words (sing.c sing_view): what the track sings. The lyric's source (the language, TEXT / W n / S n,
 * the theme) and the syllables sung; the word sounding, large; its syllable's phonemes, the one sounding lit; the
 * lyric's text from the word on (TEXT); at the right the vowel chart (F2 across, high to low; F1 down: the tongue's
 * place, as a sagittal head would show it) with the formants' dot, filled while voiced, ringed in noise; under it the
 * voicing and the noise */
static void sing_up(char *d, const char *s, uint32_t n)
{
    uint32_t i;
    for (i = 0; s[i] && i + 1u < n; i++)
        d[i] = (char)(s[i] >= 'a' && s[i] <= 'z' ? s[i] - 32 : s[i]);
    d[i] = 0;
}
static void graph_sing(const track_t *t)
{
    /* (0.8.7) in EDIT's lens (TONE . WORDS): no frame, the captions and chips in the 5 x 7 face as the lens panels'
     * (ui_lzedit.c), the word in M; the vowel chart a square well at the right, the voicing and noise under it */
    static const uint8_t VREF[5] = {PH_A, PH_E, PH_I, PH_O, PH_U};
    const sing_t *s = sing_view(t);
    uint32_t lang = (uint32_t)clamp(t->p[P_VLANG], 0, LG_N - 1), lyr = (uint32_t)clamp(t->p[P_VLYR], 0, LYR_N - 1), k;
    int32_t x, cx0 = 152, cy0 = 4, cw = 80, ch = 84, nv, nn, lw = cx0 - 10 - PANEL_X0;
    uint16_t bg = cv_bg;
    char b[64];
    if (!lang) {
        cv_text_in(PANEL_X0, 44, PANEL_W, &AF_X, "LANG OFF: VOWELS", T_MID, bg);
        cv_text_in(PANEL_X0, 58, PANEL_W, &AF_X, "LANG ES EN JA: WORDS", T_DIM, bg);
        return;
    }
    str_cpy(b, N_LANG[lang], sizeof b);
    str_cpy(b + str_len(b), " \xB7 ", 4);
    str_cpy(b + str_len(b), lyr == 0u && !lyr_get((uint32_t)(t - trk))[0] ? "W1" : N_LYRIC[lyr], 8);
    if (lyr <= LYR_NW && (lyr || !lyr_get((uint32_t)(t - trk))[0])) {
        str_cpy(b + str_len(b), " \xB7 ", 4);
        str_cpy(b + str_len(b), N_THEME[(uint32_t)clamp(t->p[P_E1], 0, 127) >> 4], 8);
    }
    cv_clip_text(PANEL_X0, 6 - AF_X_CAP_Y, &AF_X, b, T_TEXT, bg, lw - 30);   /* the source */
    fmt_int(b, s->nsy);
    str_cpy(b + str_len(b), " SYL", 5);
    cv_text_r(cx0 - 10, 6 - AF_X_CAP_Y, &AF_X, b, T_DIM, bg);
    sing_up(b, s->w[0] ? s->w : "-", sizeof b);          /* the word */
    cv_free_text(PANEL_X0, 20, &AF_M, b, T_TEXT, bg, lw);
    for (k = 0, x = PANEL_X0; k < s->np && k < SY_N; k++) {   /* its syllable's phonemes: chips */
        int32_t w;
        uint16_t f = k == s->i ? T_THEME : T_RAISE;
        sing_up(b, PH_NAME[s->ph[k] % PH_COUNT], 4);
        w = text_w(&AF_X, b) + 8;
        if (x + w > cx0 - 10)
            break;
        cv_rect(x, 46, w, 13, f);
        cv_text_in(x, 49 - AF_X_CAP_Y, w, &AF_X, b, k == s->i ? T_INK : k < s->i ? T_DIM : T_TEXT, f);
        x += w + 2;
    }
    if (!lyr && lyr_get((uint32_t)(t - trk))[0]) {       /* the text from the word on */
        sing_up(b, lyr_get((uint32_t)(t - trk)) + (s->to < LYR_LEN ? s->to : 0u), sizeof b);
        cv_free_text(PANEL_X0, 68 - AF_X_CAP_Y, &AF_X, b, T_DIM, bg, lw);
    }
    /* the vowel chart: F2 2500 .. 600 Hz across, F1 250 .. 850 Hz down */
    cv_rect(cx0, cy0, cw, ch, T_RAISE);
    cv_rect(cx0 + 1, cy0 + 1, cw - 2, ch - 2, bg);
    for (k = 0; k < 5u; k++) {
        const phon_t *P = &PHON[VREF[k]];
        int32_t vx = cx0 + 4 + clamp((2500 - P->f2) * (cw - 14) / 1900, 0, cw - 14);
        int32_t vy = cy0 + 4 + clamp(((int32_t)P->f1 - 250) * (ch - 16) / 600, 0, ch - 16);
        sing_up(b, PH_NAME[VREF[k]], 4);
        cv_text(vx, vy - AF_X_CAP_Y, &AF_X, b, T_DIM);
    }
    nv = s->gv * 100 / 32767;
    nn = (s->ga > s->gf ? s->ga : s->gf) * 100 / 32767;
    {
        int32_t dx = cx0 + 6 + clamp((2500 - s->f[1]) * (cw - 14) / 1900, 0, cw - 14);
        int32_t dy = cy0 + 8 + clamp((s->f[0] - 250) * (ch - 16) / 600, 0, ch - 16);
        uint16_t dc = nv > 8 ? T_THEME : nn > 8 ? T_ACCENT : T_DIM;   /* (plain squares: no cell for the lint) */
        cv_rect(dx - 3, dy - 2, 7, 5, dc);
        cv_rect(dx - 2, dy - 3, 5, 7, dc);
    }
    cv_text(cx0, 96 - AF_X_CAP_Y, &AF_X, "V", T_DIM);    /* the voicing, the noise */
    cv_rect(cx0 + 10, 98, cw - 10, 3, T_RAISE);
    cv_rect(cx0 + 10, 98, (cw - 10) * clamp(nv, 0, 100) / 100, 3, T_THEME);
    cv_text(cx0, 108 - AF_X_CAP_Y, &AF_X, "N", T_DIM);
    cv_rect(cx0 + 10, 110, cw - 10, 3, T_RAISE);
    cv_rect(cx0 + 10, 110, (cw - 10) * clamp(nn, 0, 100) / 100, 3, T_ACCENT);
}

/* (V10.45) MORPH on ANALOG's PWM SYNC RING SAW3 PLS3: the wave itself over two cycles, as MORPH bends it (the user: it was
 * a ramp, MORPH's amount): PWM / PLS3 the pulses' width, SYNC the slave's ratio (1..4), RING the product of two
 * triangles (1..4), SAW3 three saws spread apart. In the Bluetooth segment (the app slot full) */
static BTAPP_FN int32_t morph_tri(uint32_t u)       /* a triangle, -32768..32767, u a phase in 1/65536 */
{
    u &= 65535u;
    return (int32_t)(u < 32768u ? u * 2u : (65535u - u) * 2u) - 32768;
}
static BTAPP_FN void morph_wave(uint32_t mode, int32_t m)
{
    int32_t i, px = 0, py = 0, r = 256 + m * 768 / 127, w = 32768 + m * 26000 / 127;
    cv_rect(PANEL_X0, 46, PANEL_W, 1, T_RAISE);
    for (i = 0; i <= 110; i++) {
        uint32_t u = (uint32_t)i * 131072u / 110u, c = u & 65535u;
        int32_t s, x = PANEL_X0 + i * 2, y;
        switch (mode) {
        case 0: s = (int32_t)c < w ? 26000 : -26000; break;                                   /* PWM */
        case 1: s = (int32_t)((c * (uint32_t)r >> 8) & 65535u) - 32768; break;               /* SYNC */
        case 2: s = morph_tri(u + 16384u) * morph_tri((u * (uint32_t)r >> 8) + 16384u) >> 15; break;   /* RING */
        case 3: {                                                                           /* SAW3 */
            uint32_t d = u / 256u * (uint32_t)m / 6u;                /* (the spread: up to ~8 % a voice) */
            s = ((int32_t)c - 32768 + (int32_t)((u + d) & 65535u) - 32768 + (int32_t)((u - d) & 65535u) - 32768) / 3;
            break;
        }
        default: s = ((int32_t)c < w ? 9000 : -9000) + ((int32_t)((c + 21845u) & 65535u) < w ? 9000 : -9000) +
                     ((int32_t)((c + 43690u) & 65535u) < w * 7 / 8 ? 9000 : -9000); break;   /* PLS3 */
        }
        y = 46 - s * 18 / 32768;
        if (i) cv_line_t(px, py, x, y, heat_col(80 + (uint32_t)m), 2);
        px = x; py = y;
    }
}

static __attribute__((noinline)) void graph_morph(const track_t *t)
{
    const int16_t *p = t->p;
    uint32_t e = trk_eng(t), i, a0 = 0, a1 = 0;
    int32_t px = 0, py = 0, pos, f;
    const char *n0 = "", *n1 = "";
    char b[24];
    pd_t pa, pb;
    if (ENGINES[e] == &ENG_PHASE) {
        uint32_t dcw = (uint32_t)(p[P_E2] << 8) * 65535u / (127u << 8);
        pos = (int32_t)(p[P_E0] & 7) * 256 + p[P_MORPH] * 768 / 127;
        dcw = (dcw * 56000u) >> 16;
        a0 = ((uint32_t)pos >> 8) & 7u; a1 = (a0 + 1u) & 7u;
        pd_setup(&pa, a0, dcw); pd_setup(&pb, a1, dcw);
        n0 = N_PD_WAVE[a0]; n1 = N_PD_WAVE[a1];
    } else {
        if (p[P_E0] >= 4) {                             /* (0.6.81) PWM SYNC RING SAW3: the WAVE, what MORPH moves */
            static const char *const WHAT[5] = {"PWM  WIDTH", "SYNC  RATIO", "RING  RATIO", "SAW3  SPREAD", "PLS3  WIDTH"};
            str_cpy(b, WHAT[clamp(p[P_E0] - 4, 0, 4)], sizeof b);
            if (bt_hw_seg())                            /* (V10.45) the wave as MORPH bends it */
                morph_wave((uint32_t)clamp(p[P_E0] - 4, 0, 4), p[P_MORPH]);
            else {
            cv_rect(PANEL_X0, 46, PANEL_W, 1, T_RAISE);
            for (i = 0; i <= 110u; i++) {
                int32_t x = PANEL_X0 + (int32_t)i * 2, y = 46 - (int32_t)((i * 34u * (uint32_t)p[P_MORPH]) / (110u * 127u)) + 17;
                if (i) cv_line_t(px, py, x, y, heat_col(80 + (uint32_t)(46 - y + 34) * 2u), 2);
                px = x; py = y;
            }
            }
            cv_text(PANEL_X0 + 2, 90, &AF_S, b, T_MID);
            return;
        }
        pos = clamp(p[P_E0] * 256 + p[P_MORPH] * 768 / 127, 0, 1536);
        a0 = ((uint32_t)pos >> 8) & 3u; a1 = (a0 + 1u) & 3u;
        n0 = N_ANALOG_WAVE[a0]; n1 = N_ANALOG_WAVE[a1];
    }
    f = pos & 255;
    cv_rect(PANEL_X0, 46, PANEL_W, 1, T_RAISE);
    for (i = 0; i <= 110u; i++) {
        uint32_t ph = (uint32_t)(i % 110u) * (0xFFFFFFFFu / 110u), inc = 0xFFFFFFFFu / 220u;
        int32_t s, x = PANEL_X0 + (int32_t)i * 2, y;
        if (ENGINES[e] == &ENG_PHASE) {
            s = pd_wave(&pa, ph >> 16);
            if (f) s += ((pd_wave(&pb, ph >> 16) - s) * f) >> 8;
        } else
            s = an_morph(pos, ph, inc);
        y = 46 - s * 34 / 32768;
        if (i) cv_line_t(px, py, x, y, heat_col(80 + (uint32_t)(46 - y + 34) * 2u), 2);
        px = x; py = y;
    }
    str_cpy(b, n0, sizeof b);
    if (f && a1 != a0) { str_cpy(b + str_len(b), " > ", 4); str_cpy(b + str_len(b), n1, 8); }
    cv_text(PANEL_X0 + 2, 90, &AF_S, b, T_MID);
}

/* (0.8.5) the piano roll of the cursor's bar, as the approved mock-up (LENTE's BAR, under the bars' strip): its 16
 * steps as columns (0.9.9-seq2b: x 22 .. 230, 13 px, the keys at the left; was 4 .. 236), PR_ROWS semitone rows of PR_RH px (4), the
 * highest on top, on a BG area; rows in the track's scale (SCL ROOT / SCALE; CHR: the white keys) tinted LANE; step
 * lines GRID, every 4th LINE. A note is a bar in its column, a px in from either side, PR_RH - 1 tall, as hot as it
 * plays (an accent: the highlight; another palette: notes ACCENT, accents TEXT); chords stacked; lane hits as their
 * notes; a RATCH step's bars in its 2..4 parts; a TIE carries the bars of the note before through its column; a REST
 * is empty; a slide reaches 2 px into the next column. On SEQ > NOTE the note in focus is the accent (the chord, CHORD:
 * all of it). A step with a component: a 3 px mark at the column's top (an arpeggio ACCENT, the others THEME). Notes
 * outside the view: a 1 px mark on the edge. The cursor: a THEME frame round its column; the playhead: a 1 px ACCENT
 * line at its column's left; a parameter lock: an ACCENT mark at the column's foot; a key held on the track: its row
 * a 2 px ACCENT mark at the left. The view (proll.lo, the bottom row) fits the notes of the page, else centres on the
 * cursor's note, and follows in steps (a third of the way per frame); graph_signature() holds it. (Until 0.8.4 a
 * keyboard strip and the C rows' names at the left, 21 rows of 5 px, 12 px columns.) */
#define PR_ROWS ((int32_t)proll.rows)               /* semitones shown: 30 (2.5 octaves) */
#define PR_RH 4                                     /* px per row */
#define PR_Y0 ((int32_t)proll.y0)                   /* the top row (canvas y): 20, under the bars' strip */
#define PR_RX 22                                    /* (0.9.9-seq2b) the roll: x 22 .. 230, 13 px a step; the keys at its left */
#define PR_RW 208
#define PR_KX 7                                     /* .. the keys: x 7 .. 19, a row a key (white MID, black BG), the */
#define PR_KW 13                                    /* track's root a THEME tooth at x 4 .. 5 */
#define PR_SX 4                                     /* the bars' strip over it: the panel's width, 4 .. 236 */
#define PR_SW 232
#define PR_MAXR 64                                  /* (rows lit on the keys: at most) */
#define PR_CX(i) (PR_RX + (int32_t)(i) * PR_RW / 16)   /* column i's left */
#define PR_CWI(i) (PR_CX((i) + 1) - PR_CX(i))          /* .. its width */
static struct {
    uint8_t lo, trk, init, moving;                    /* lo: the note of the bottom row; moving: on its way */
    uint8_t rows, y0;                                 /* PR_ROWS, PR_Y0: pr_geom */
    uint32_t frame;
} proll = {0, 0, 0, 0, 30, 20, 0};
static int lens_strip(void);
static void pr_geom(void)                             /* the roll's rows: under the bars' strip (a layer's map: the panel) */
{
    int s = lens_strip();
    proll.y0 = s ? 20u : 2u;
    proll.rows = (uint8_t)(((int32_t)graph_h() - (int32_t)proll.y0 - (s ? 0 : 2)) / PR_RH);
}
static int32_t pr_note(const step_t *st, uint32_t j)   /* the step's note j (0..3), then its lane hits (-1: none) */
{
    if (j < 4u)
        return j < st->n ? st->note[j] : -1;
    return (st->hit >> (j - 4u)) & 1u ? DRUM_LANE_NOTE[j - 4u] : -1;
}
static uint32_t pr_src(const track_t *t, uint32_t si, uint32_t len)   /* the step whose notes sound at si (NSTEP: none) */
{
    uint32_t k;
    for (k = 0; k < len && seq_steps(t)[si].time == ST_TIE; k++)
        si = (si + len - 1u) % len;
    return step_on(&seq_steps(t)[si]) ? si : NSTEP;
}
/* the rows lit by keys held on the selected track (bit r: row r from the top) */
static uint32_t pr_held(void)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_on[k] && kb_trk[k] == song.sel) {
            uint32_t i;
            for (i = 0; i < kb_on[k]; i++) {            /* (0.6.81: a chord's every note) */
                int32_t r = (int32_t)proll.lo + PR_ROWS - 1 - kb_snd[k][i];
                if (r >= 0 && r < PR_ROWS)
                    m |= 1u << r;
            }
        }
    return m;
}
/* once a frame: the view toward the page's notes (centred; once there it stays while they fit); at once on a redraw
 * forced, a gap in the frames, another track or MENU > ANIM OFF */
static __attribute__((noinline)) void pr_follow(const track_t *t)
{
    uint32_t i, j, len = (uint32_t)t->p[P_SLEN], base = ui.bank * 16u;
    int32_t lo = 127, hi = -1, tgt, d, cur = proll.lo;
    int snap;
    pr_geom();
    snap = ui.force || !proll.init || proll.trk != song.sel || ui.frame != proll.frame + 1u || ANIM_SNAP();
    if (proll.init && proll.frame == ui.frame)
        return;
    for (i = 0; i < 16u && base + i < len; i++) {
        uint32_t s = pr_src(t, base + i, len);
        for (j = 0; s < NSTEP && j < 4u + NLANE; j++) {
            int32_t n = pr_note(&seq_steps(t)[s], j);
            if (n < 0)
                continue;
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    }
    if (hi < 0)
        lo = hi = last_note;
    if (hi - lo <= PR_ROWS - 1)                     /* all fit: centred, then kept while they fit */
        tgt = !snap && !proll.moving && lo >= cur && hi <= cur + PR_ROWS - 1 ? cur : (lo + hi + 1) / 2 - PR_ROWS / 2;
    else {                                            /* too wide: the cursor's (lowest) note in the middle */
        const step_t *st = &seq_steps(t)[ui.cursor];
        tgt = (st->n ? st->note[0] : (lo + hi) / 2) - PR_ROWS / 2;
    }
    tgt = clamp(tgt, 0, 128 - PR_ROWS);
    d = tgt - cur;
    proll.lo = (uint8_t)(snap ? tgt : cur + (d / 3 ? d / 3 : d > 0 ? 1 : d < 0 ? -1 : 0));
    proll.moving = proll.lo != tgt;
    proll.trk = (uint8_t)song.sel;
    proll.frame = ui.frame;
    proll.init = 1;
}
static int32_t pr_row_y(int32_t n) { return PR_Y0 + ((int32_t)proll.lo + PR_ROWS - 1 - n) * PR_RH; }
/* a bar w px wide in `hits` equal parts, 1 px apart (a RATCH step; 1: the bar) */
static void pr_split(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, uint32_t hits)
{
    int32_t k, pw = (w - (int32_t)hits + 1) / (int32_t)hits;
    if (hits < 2u) {
        cv_rect(x, y, w, h, c);
        return;
    }
    for (k = 0; k < (int32_t)hits; k++)
        cv_rect(x + k * (pw + 1), y, pw, h, c);
}
/* a note's colour in the lens's pictures, as the mock-up's: JIANT on the heat ramp, a note warm (115; a chord's a
 * little warmer), an accent near white (243); another palette the notes ACCENT, an accent TEXT (the highlight).
 * (V10.6w, the owner: "the whole system one visual language") JIANT's notes (n: the note; a kit's lane -1) their pitch
 * class's own heat, as SCL's keys (ui_lzpage.c note_col), an accent toward white */
static uint16_t lz_note(const step_t *st, int acc, int32_t n)
{
    if (settings.palette == UI_JIANT_INDEX) {
        if (n >= 0 && !ux.mono)
            return ux_mix(note_col((uint32_t)n), T_TEXT, acc ? 55 : 0);
        return heat_col(acc ? 243 : st->n > 1u ? 146 : 115);
    }
    return acc ? T_TEXT : T_ACCENT;
}
/* (seq2d) a component's colour by its family, as the mock-up's: the pitch (ARP SEQ OCT TRANSPOSE RANDOM BEND) ACCENT,
 * the time (RATCHET STUTTER) THEME, the flow (MUTE) MID, the sound (LOCK SOUND RAMP) TEXT */
static uint16_t comp_col(uint32_t a)
{
    return a == CA_RATCH || a == CA_STUT ? T_THEME : a == CA_MUTE ? T_MID :
           a == CA_LOCK || a == CA_SOUND || a == CA_RAMP ? T_TEXT : T_ACCENT;
}
/* a step's component marks (3 x 3 at y, centred on x): C1's, C2's beside it; none: nothing */
static void lz_comp_mark(const step_t *st, int32_t x, int32_t y)
{
    uint32_t a1 = CO_ACT(st->comp), a2 = CO_ACT(st->comp2);
    if (a1 && a2) {
        cv_rect(x - 3, y, 3, 3, comp_col(a1));
        cv_rect(x + 1, y, 3, 3, comp_col(a2));
    } else if (a1 || a2)
        cv_rect(x - 1, y, 3, 3, comp_col(a1 ? a1 : a2));
}
/* (seq2d) a component's LENGTH over the steps after it: the slot of step k that covers step si (0: none; 1 C1, 2 C2),
 * the pattern's steps in order (len), round the loop */
static uint32_t comp_span_at(const track_t *t, uint32_t si, uint32_t len)
{
    uint32_t j, k;
    for (j = 1; j < CL_MAX && j < len; j++) {
        const step_t *st = &seq_steps(t)[(si + len - j) % len];
        for (k = 0; k < 2u; k++) {
            uint32_t a = CO_ACT(k ? st->comp2 : st->comp), l = st->clen[k];
            if (a && a != CA_RAMP && l < CL_MAX && l >= j)   /* (V10.6v: a RAMP's its curve, seq_overlay) */
                return k + 1u + (a << 2);
        }
    }
    return 0;
}
/* (0.8.3) LENTE's bars; (0.8.5) drawn as the approved mock-up has them: a bar of the pattern as a cell (RAISE; the bar
 * edited TINT), its sixteen steps as a tiny roll (each note-on step's first note a 2 px dash over the pattern's range;
 * a kit: its lanes' hits, the kick on top), its number at the top left (DIM; the bar edited THEME, the bar playing
 * ACCENT); a bar past LEN an empty dark cell. Two sizes: PATTERN's four big cells and BAR's strip (16 px) */
static void pat_range(const track_t *t, int32_t *plo, int32_t *phi)   /* the pattern's notes, 30 semitones at least */
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, j;
    int32_t lo = 127, hi = -1;
    for (i = 0; i < len; i++)
        for (j = 0; j < seq_steps(t)[i].n && seq_steps(t)[i].time == ST_NOTE; j++) {
            int32_t n = seq_steps(t)[i].note[j];
            lo = n < lo ? n : lo;
            hi = n > hi ? n : hi;
        }
    if (hi < lo)
        lo = hi = 60;
    if (hi - lo < 29) {                              /* (centred in the window) */
        lo -= (29 - (hi - lo)) / 2;
        hi = lo + 29;
    }
    *plo = lo;
    *phi = hi;
}
static void bar_cell(const track_t *t, uint32_t b, int32_t x, int32_t y, int32_t w, int32_t h, int32_t lo, int32_t hi,
                     uint16_t c)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i, l, dr = drum_track(t);
    int sel = b == ui.bank;
    int32_t sw = w - 4, top = y + (h > 20 ? 14 : 3), nh = h - (h > 20 ? 17 : 5);
    uint16_t bg = ux.skin ? T_SURF : T_RAISE, nc;
    char s[2];
    if (b * 16u >= len) {                            /* past LEN: a dark cell */
        cv_rect(x, y, w, h, T_BG);
        return;
    }
    if (ux.skin)                                     /* (0.8.9) BOTTOM, HUD: bare, a line under it (the bar edited THEME) */
        cv_rect(x, y + h - 1, w, 1, sel ? T_THEME : T_LINE);
    else if (sel) {                                  /* COLORS: a rounded card, the bar edited framed THEME */
        cv_rrect(x, y, w, h, 3, T_THEME, cv_bg);
        cv_rrect(x + 1, y + 1, w - 2, h - 2, 2, T_RAISE, T_THEME);
    } else
        cv_rrect(x, y, w, h, 3, T_RAISE, cv_bg);
    for (i = 0; i < 16u && b * 16u + i < len; i++) {
        const step_t *st = &seq_steps(t)[b * 16u + i];
        int32_t sx = x + 2 + (int32_t)i * sw / 16, mw = sw / 16 - 1 > 1 ? sw / 16 - 1 : 1;
        if (!step_on(st))
            continue;
        if (dr) {
            for (l = 0; l < NLANE; l++)
                if ((step_lanes_of(TSEL,st) >> l) & 1u)
                    cv_rect(sx, top + (int32_t)l * nh / (int32_t)NLANE, mw, nh / (int32_t)NLANE > 2 ? 2 : 1,
                            lz_note(st, (step_accents_of(TSEL,st) >> l) & 1u, -1));
            continue;
        }
        cv_rect(sx, top + nh - 2 - ((int32_t)st->note[0] - lo) * (nh - 2) / (hi - lo), mw, 2,
                lz_note(st, (st->flags & SF_ACCENT) != 0u, st->note[0]));
    }
    s[0] = (char)('1' + b);
    s[1] = 0;
    nc = song.playing && seq_src(t, t->seq_idx) / 16u == b ? T_ACCENT : sel ? T_THEME : T_MID;
    GFX_HOOK_ALIGN(0, y, 0, y + (h > 20 ? 15 : h), AL_V | AL_PASS, "LENTE bar number at its cell's top");
    cv_text_on(x + 3, y + (h > 20 ? 3 : CAP_IN(X, h)), &AF_X, s, nc, bg);
}
/* the bars side by side from x0 (4 cells over w, 2 px apart), y .. y + h */
static void bar_cells(const track_t *t, int32_t x0, int32_t w, int32_t y, int32_t h, uint16_t c)
{
    int32_t lo, hi, bw = w / 4;
    uint32_t b;
    pat_range(t, &lo, &hi);
    for (b = 0; b < 4u; b++)
        bar_cell(t, b, x0 + (int32_t)b * bw + 1, y, bw - 2, h, lo, hi, c);
}
/* (0.8.5) BAR's strip over the roll and the grid (lens_strip; 0.9.9-seq2b, as 0.9.17: every SEQ page's): the bars small,
 * 16 px at the panel's top */
#define LS_Y 0
#define LS_H 16
static int seq_roll_view(const page_t *pg);
static int lens_strip(void) { return !ui.home && lz_on() && !ui.layer && seq_roll_view(cur_page()); }

/* the bars of step s in column x (in its RATCH parts, hits; a TIE's carried bars: 1): a 1 px edge mark for a note
 * out of view; first: row y of the step's first note; fc: the note in focus's colour (0: none) and its note */
static void pr_bars(const step_t *st, int32_t x, int32_t w, uint16_t c, int acc_all, int32_t *first, uint32_t hits)
{
    uint32_t j;
    int32_t ybot = PR_Y0 + PR_ROWS * PR_RH;
    for (j = 0; j < 4u + NLANE; j++) {
        int32_t n = pr_note(st, j), y;
        int acc;
        if (n < 0)
            continue;
        acc = acc_all || (j >= 4u && ((st->acc >> (j - 4u)) & 1u));
        y = pr_row_y(n);
        if (*first < -1000)
            *first = y;
        if (y < PR_Y0)
            pr_split(x, PR_Y0, w, 1, c ? c : lz_note(st, acc, j < 4u ? n : -1), hits);
        else if (y >= ybot)
            pr_split(x, ybot - 1, w, 1, c ? c : lz_note(st, acc, j < 4u ? n : -1), hits);
        else
            pr_split(x, y, w, PR_RH - 1, c ? c : lz_note(st, acc, j < 4u ? n : -1), hits);
    }
}
/* (0.9.9-seq2b, the approved mock-up's R1 .. R3) the playheads' colours: the sequencer's ACCENT, FUGUE's heads 2 .. 4 the
 * knobs' colours 1, 2 and 4 (the palette's own: GREY grey, MONO white) */
static uint16_t pr_head_col(uint32_t k) { return k ? T_K(k == 3u ? 3u : k - 1u) : T_ACCENT; }
/* the step a head is on (0: the sequencer's, 1 .. 3 FUGUE's; NSTEP: none, stopped or the head off) */
static uint32_t pr_head_step(const track_t *t, uint32_t k)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), ti = song.sel % NTRK, i;
    if (!song.playing)
        return NSTEP;
    if (!k)
        return seq_src(t, t->seq_idx);
    i = fg_head[ti][k - 1u].idx;
    return fg_set[ti][k - 1u].rate && i < len ? seq_src(t, i) : NSTEP;
}
/* the keys lit: the rows of the notes step si sounds (a TIE's: the note before's), moved ivl degrees (FUGUE's INTERVAL) */
static void pr_lit(const track_t *t, uint32_t si, int32_t ivl, uint16_t c, uint16_t *lit)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), s, j;
    if (si >= len || (s = pr_src(t, si, len)) == NSTEP)
        return;
    for (j = 0; j < seq_steps(t)[s].n && j < 4u; j++) {
        int32_t n = seq_steps(t)[s].note[j], r;
        if (ivl)
            n = scale_steps(t, n, ivl);
        r = (int32_t)proll.lo + PR_ROWS - 1 - n;
        if (r >= 0 && r < PR_ROWS && r < PR_MAXR)
            lit[r] = c;
    }
}
static const uint8_t PR_KB[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};   /* (a black key) */
/* the keys at the roll's left, a row a key: white MID, black BG, the track's root a THEME tooth; lit: the cursor's step's
 * notes TEXT, the notes each playhead sounds its colour, the keys held (the step being entered) ACCENT */
static void pr_keys(const track_t *t, uint32_t held)
{
    uint16_t lit[PR_MAXR];
    uint32_t k, ti = song.sel % NTRK;
    int32_t r;
    for (r = 0; r < PR_MAXR; r++)
        lit[r] = 0;
    pr_lit(t, ui.cursor, 0, T_TEXT, lit);
    for (k = FG_HEADS + 1u; k-- > 0u;)
        pr_lit(t, pr_head_step(t, k), k ? fg_set[ti][k - 1u].ivl : 0, pr_head_col(k), lit);
    for (r = 0; r < PR_ROWS && r < PR_MAXR; r++) {
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint16_t kc = (held >> r) & 1u ? T_ACCENT : lit[r] ? lit[r] : PR_KB[n % 12] ? T_BG : T_MID;
        cv_rect(PR_KX, y, PR_KW, PR_RH - 1, kc);
        if ((n - t->p[P_ROOT] + 120) % 12 == 0)
            cv_rect(PR_KX - 3, y, 2, PR_RH - 1, T_THEME);
    }
}
/* the playheads as lines down the bar shown, a step at a time: the sequencer's 1 px ACCENT at its column's left (as
 * before), FUGUE's heads 2 .. 4 in their colours a quarter, a half, three quarters into theirs (PLAYHEADS: the head edited
 * 2 px); dr: the DRUM grid's columns */
static void seq_col(uint32_t dr, uint32_t i, int32_t *x, int32_t *w);
static void pr_heads(const track_t *t, int32_t top, int32_t bot, uint32_t dr)
{
    uint32_t k;
    for (k = dr ? 1u : 0u; k <= FG_HEADS; k++) {      /* (the grid: its own bar over the sequencer's step) */
        uint32_t s = pr_head_step(t, k);
        int32_t x, w;
        if (s >= NSTEP || s / 16u != ui.bank)
            continue;
        seq_col(dr, s % 16u, &x, &w);
        if (k)
            x += (w + 1) * (int32_t)k / 4;
        cv_rect(x, top, k && cur_page()->graph == GR_FUGUE && k - 1u == fg_ui % FG_HEADS ? 2 : 1, bot - top, pr_head_col(k));
    }
}
/* (V10.47) the roll and the grid follow the playhead (a user's ask: 64 steps, the view stayed on the first 16): while
 * it plays and nothing was touched for 3 s, the bar shown is the one playing; a key or a knob stops it there for 3 s
 * (what is shown is what the keys edit) */
static void pr_follow_bar(const track_t *t)
{
    uint32_t s = pr_head_step(t, 0);
    if (song.playing && (int32_t)(fm1_ms - scrn.idle) > 3000 && s < NSTEP && (uint32_t)t->p[P_SLEN] > 16u)
        ui.bank = (uint8_t)(s / 16u);
}
static void graph_roll(const track_t *t, uint16_t c)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN], base = (pr_follow_bar(t), ui.bank * 16u), ncol, mask = scale_mask(t), held = pr_held();
    uint64_t locks = motion_lock_steps(trk_index(t));
    int32_t r, ybot, trw = 0, note_pg = cur_page()->id[0] == 4u;
    char tb[16];
    (void)c;
    pr_geom();
    ybot = PR_Y0 + PR_ROWS * PR_RH;
    if (song.g[G_STRN]) {                             /* (0.6.45) ALL TR (SCL KNOB 4, 0.7.6): the notes sound moved, shown */
        str_cpy(tb, "ALL TR ", sizeof tb);
        tb[7] = song.g[G_STRN] < 0 ? '-' : '+'; tb[8] = 0;
        fmt_int(tb + 8, song.g[G_STRN] < 0 ? -song.g[G_STRN] : song.g[G_STRN]);
        trw = text_w(&AF_S, tb) + 6;
    }
    if (lens_strip()) {                               /* LENTE's BAR: the bars over the roll (ALL TR after them) */
        bar_cells(t, PR_SX, PR_SW - trw, LS_Y, LS_H, ACC);
        if (trw) {
            GFX_HOOK_ALIGN(0, LS_Y, 0, LS_Y + LS_H, AL_V, "ALL TR on the bars' line");
            cv_text_r(PR_SX + PR_SW, LS_Y + CAP_IN(S, LS_H), &AF_S, tb, T_ACCENT, T_SURF);
        }
    }
    ncol = base < len ? (len - base < 16u ? len - base : 16u) : 0u;
    cv_rect(PR_RX, PR_Y0, PR_RW, ybot - PR_Y0, T_BG);
    for (r = 0; r < PR_ROWS; r++) {                 /* rows: the scale's tinted */
        int32_t n = (int32_t)proll.lo + PR_ROWS - 1 - r, y = PR_Y0 + r * PR_RH;
        uint32_t pc = (uint32_t)n % 12u;
        int in = mask == 0xFFFu ? !PR_KB[pc] : (int)((mask >> ((uint32_t)(n - t->p[P_ROOT] + 120) % 12u)) & 1u);
        if (in && !ux.skin)                          /* ((0.8.9) BOTTOM, HUD: no rows, the beats only) */
            cv_rect(PR_RX, y, PR_CX(ncol) - PR_RX, PR_RH, T_LANE);
    }
    for (i = 0; i < 16u; i++)                         /* step lines, the beats brighter */
        if (!ux.skin || !(i % 4u))
            cv_rect(PR_CX(i), PR_Y0, 1, ybot - PR_Y0, i % 4u ? T_GRID : T_LINE);
    if (ncol < 16u)                                   /* past LEN: dark */
        cv_rect(PR_CX(ncol), PR_Y0, PR_RX + PR_RW - PR_CX(ncol), ybot - PR_Y0, T_BG);
    pr_keys(t, held);                                 /* (0.9.9-seq2b) the keys at the left (was: a key held, its row
                                                       * marked at the roll's left) */
    for (i = 0; i < ncol; i++) {                      /* the notes */
        uint32_t si = base + i, s = pr_src(t, si, len), nx = (si + 1u) % len;
        const step_t *st, *ns = &seq_steps(t)[nx];
        int32_t x = PR_CX(i), w = PR_CWI(i), y = -10000;
        int slide;
        if (s == NSTEP)
            continue;                                 /* a REST, empty, or a TIE after one */
        st = &seq_steps(t)[s];
        slide = (st->flags & SF_SLIDE) && step_ratchet(st) == 1u && ns->time == ST_NOTE && ns->n;
        if (s == ui.cursor && note_pg && ui.pr_focus && ui.pr_focus <= st->n) {   /* (0.6.81) SEQ > NOTE: the note in
                                                         * focus the accent */
            step_t one = *st;
            int32_t yy = -10000;
            pr_bars(st, s == si ? x + 1 : x - 1, s == si ? w - 2 : w + 1, 0, (st->flags & SF_ACCENT) != 0u, &y,
                    s == si ? step_ratchet(st) : 1u);
            one.n = 1; one.note[0] = st->note[ui.pr_focus - 1u]; one.hit = 0;
            pr_bars(&one, s == si ? x + 1 : x - 1, s == si ? w - 2 : w + 1, T_ACCENT, 0, &yy, s == si ? step_ratchet(st) : 1u);
        } else if (s == si) {                         /* (0.6.88: its TIMING off the grid, shown off it) */
            int32_t mo = ST_MICRO(st) * w / 64;     /* (0.9.1: 64ths) */
            pr_bars(st, x + 1 + mo < PR_RX ? PR_RX : x + 1 + mo, w - 2 + (slide ? 2 : 0),
                    s == ui.cursor && note_pg ? T_ACCENT : 0, (st->flags & SF_ACCENT) != 0u, &y, step_ratchet(st));
        } else                                        /* a TIE: the bars on through the gap before */
            pr_bars(st, x - 1, w + (slide && (si + 1u) % len != nx ? 0 : 0), s == ui.cursor && note_pg ? T_ACCENT : 0,
                    (st->flags & SF_ACCENT) != 0u, &y, 1u);
        if (s == si)
            lz_comp_mark(st, x + w / 2, PR_Y0 + 1);
    }
    for (i = 0; i < ncol; i++) {                      /* (seq2d) a component's marks on any step, its LENGTH a line on */
        uint32_t si = base + i, sp;
        int32_t x = PR_CX(i), w = PR_CWI(i);
        if (si >= len)
            break;
        if (seq_steps(t)[si].time != ST_NOTE || (!seq_steps(t)[si].n && !seq_steps(t)[si].hit))
            lz_comp_mark(&seq_steps(t)[si], x + w / 2, PR_Y0 + 1);
        if ((sp = comp_span_at(t, si, len)) != 0u)
            cv_rect(x, PR_Y0 + 2, w, 1, comp_col(sp >> 2));
    }
    for (i = 0; i < ncol; i++) {
        uint32_t si = base + i;
        int32_t x = PR_CX(i);
        if ((locks >> si) & 1u)                       /* a parameter lock: a mark at the column's foot */
            cv_rect(x + 3, ybot - 3, PR_CWI(i) - 6, 2, T_ACCENT);
        if (si == ui.cursor)                          /* the cursor: a frame round its column (0.8.9, the skins' mock-up:
                                                       * TEXT) */
            cv_frame(x, PR_Y0, PR_CWI(i) + 1, ybot - PR_Y0, T_TEXT);
    }
    pr_heads(t, PR_Y0, ybot, 0);
    if (ui.entry_open && !ui.ent_arp && ui.ent_nn && ui.ent_len > 1u) {   /* (0.6.81) a chord held, KNOB 1 its length:
                                                         * the length to be written, outlined from the cursor on */
        uint32_t k, j;
        for (k = 0; k < ui.ent_len && k < len; k++) {
            uint32_t si = (ui.cursor + k) % len;
            int32_t x;
            if (si < base || si >= base + ncol) continue;
            x = PR_CX(si - base);
            for (j = 0; j < ui.ent_nn; j++) {
                int32_t y = pr_row_y(ui.ent_notes[j]);
                if (y < PR_Y0 || y >= ybot) continue;
                cv_frame(k ? x - 1 : x + 1, y, k ? PR_CWI(si - base) + 1 : PR_CWI(si - base) - 1, PR_RH, T_ACCENT);
            }
        }
    }
}
/* SEQ > STEP on a DRUM track: the grid, 8 lanes x the 16 steps of the page shown. Lanes by their two-letter
 * names (BD SD CP CH OH TM RS CB; CG CL CY on the other kits). A hit is a rounded square (accented: the
 * accent; a RATCH step's: its 2..4 parts as bars), an empty step a dot (brighter on the beats and on the selected lane); the selected lane is
 * underlaid RAISE, the cursor framed in TEXT; a TEXT bar over the step playing. (0.8.5) LENTE's BAR: the bars' strip
 * over it, the lanes 15 px under it */
static void graph_grid(const track_t *t, uint16_t c)
{
    uint32_t l, i, len = (uint32_t)t->p[P_SLEN], base = (pr_follow_bar(t), ui.bank * 16u);
    uint64_t locks = motion_lock_steps(trk_index(t));
    int s = lens_strip();
    int32_t y0 = s ? 19 : 6, lp = s ? 15 : 14, hy = 2;   /* (a lane's pitch; its hits' row in it) */
    if (s)
        bar_cells(t, PR_SX, PR_SW, LS_Y, LS_H, c);
    for (i = 0; i < 16u && base + i < len; i++)     /* a parameter lock: a mark over the step (under the bars: under it) */
        if ((locks >> (base + i)) & 1u)
            cv_rect(41 + (int32_t)i * 12, s ? y0 + 4 + 8 * lp - 1 : 1, 8, 2, T_ACCENT);
    for (l = 0; l < NLANE; l++) {
        int32_t y = y0 + (int32_t)l * lp;
        int sel = l == ui.lane;
        uint16_t row = sel ? T_RAISE : T_SURF;
        if (sel) {
            GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H | AL_CELLS, "drum lane fill centred");
            cv_rect(6, y, 228, lp - 1, T_RAISE);        /* (6 .. 234, as a list row; the hits 40 .. 230) */
        }
        GFX_HOOK_ALIGN(0, y + hy, 0, y + hy + 10, AL_V, "drum lane label on its hits' line");
        cv_text_on(10, y + hy - 3, &AF_S, drum_lane_abbr(t, l), sel ? T_ACCENT : T_MID, row);   /* ink as the hits' rows */
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t b = 1u << l;
            int32_t x = 40 + (int32_t)i * 12;
            if ((step_lanes_of(TSEL,st) & b) && step_ratchet(st) > 1u)
                pr_split(x, y + hy, 10, 10, lz_note(st, step_accents_of(TSEL,st) & b, -1), step_ratchet(st));
            else if (step_lanes_of(TSEL,st) & b)
                cv_rect(x, y + hy, 10, 10, lz_note(st, step_accents_of(TSEL,st) & b, -1));
            else
                cv_rect(x + 4, y + hy + 4, 2, 2, i % 4u == 0u || sel ? T_MID : T_DIM);
            if (sel && base + i == ui.cursor)          /* the cursor: a 1 px frame */
                cv_frame(x - 1, y + hy - 1, 12, 12, T_THEME);
        }
    }
    if (song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank)
        cv_rect(40 + (int32_t)(seq_src(t, t->seq_idx) % 16u) * 12, y0 - 2, 10, 2, T_ACCENT);
}

static uint32_t steps_hash(const track_t *t)
{
    uint32_t h = 2166136261u, i;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &seq_steps(t)[i];
        h = (h ^ (st->note[0] + st->n * 128u + st->time * 1024u + st->flags * 4096u + st->note[1] * 65536u)) *
            16777619u;
        h = (h ^ (st->hit | (uint32_t)st->acc << 8 | (uint32_t)st->note[2] << 16 | (uint32_t)st->note[3] << 24)) * 16777619u;
        h = (h ^ ((uint32_t)st->cwhen >> 4)) * 16777619u;   /* (0.9.1: MICRO's sixteenth; its 64ths are in the flags) */
        if (st->comp || st->comp2)                      /* (seq2d) the components' marks and LENGTHs */
            h = (h ^ ((uint32_t)st->comp | (uint32_t)st->comp2 << 8 | (uint32_t)st->clen[0] << 16 |
                      (uint32_t)st->clen[1] << 24)) * 16777619u;
    }
    return h;
}

static uint32_t str_hash(uint32_t h, const char *s)
{
    while (*s)
        h = (h ^ (uint8_t)*s++) * 16777619u;
    return h;
}



/* The FM charts' parts: an operator box 21 x 17 (rows 23 px apart, columns 24), junction dots, Manhattan routes */
#define FM_BH 17
static void fm_dot(int32_t x, int32_t y, uint16_t c) { cv_rect(x - 1, y - 1, 3, 3, c); }
/* a Manhattan route, 1 px: down from (x0, y0) to the jog row jy, across to x1, down to y1 */
static void fm_route(int32_t x0, int32_t y0, int32_t x1, int32_t jy, int32_t y1, uint16_t c)
{
    cv_rect(x0, y0, 1, jy - y0, c);
    cv_rect(x0 < x1 ? x0 : x1, jy, (x0 < x1 ? x1 - x0 : x0 - x1) + 1, 1, c);
    cv_rect(x1, jy, 1, y1 - jy, c);
}
/* a feedback loop on the operator box at (x, y): out of its right side, up, back in on top with an arrow */
static void fm_loop(int32_t x, int32_t y, uint16_t c)
{
    cv_rect(x + 11, y + 5, 5, 1, c);
    cv_rect(x + 15, y - 4, 1, 9, c);
    cv_rect(x, y - 4, 15, 1, c);
    cv_rect(x, y - 4, 1, 4, c);
    cv_line(x - 2, y - 3, x - 1, y - 2, c); cv_line(x + 2, y - 3, x + 1, y - 2, c);
}
/* the output bus under the carriers (the leftmost at cl, the rightmost at cr) at y bus, an arrow out on the right */
static void fm_bus(int32_t cl, int32_t cr, int32_t bus)
{
    cv_rect(cl, bus, cr + 24 - cl, 1, T_MID);
    cv_line(cr + 20, bus - 3, cr + 23, bus, T_MID); cv_line(cr + 20, bus + 3, cr + 23, bus, T_MID);
}

/* FM6's 32 algorithms as charts (0.9.6: as the colony, ui_engvis.c vx_fm6). FM6_CELL: per operator 1..6 its cell, the column in 24 px steps (bits 0..2) and
 * the row above the output bus (bits 4..5; row 0 = the carriers). The routes, the carriers and the feedback
 * operator are fm6_core.c's FM6_ALG itself (fm6_routes, fm6_car_ops, fm6_fb_op). ui_test.c checks the cells
 * against them (a modulator one row above what it modulates, the carriers on the bottom row and only they, one
 * operator per cell) and the routes against the 32 algorithms written out; ui_render.c lints every colony as drawn
 * (no two bichos overlapping, all inside the panel). The cells:
 * a search for the fewest and shortest bends, the operators numbered left to right. */
static const uint8_t FM6_CELL[32][6] = {
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x31}, {0x00, 0x10, 0x02, 0x12, 0x22, 0x32}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x21},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x21}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12}, {0x00, 0x10, 0x01, 0x11, 0x02, 0x12},
    {0x00, 0x10, 0x01, 0x11, 0x12, 0x22}, {0x00, 0x10, 0x01, 0x12, 0x11, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x13, 0x23},
    {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x20, 0x01, 0x11, 0x12}, {0x00, 0x10, 0x03, 0x12, 0x13, 0x14},
    {0x00, 0x10, 0x02, 0x11, 0x12, 0x13}, {0x00, 0x10, 0x01, 0x11, 0x20, 0x21}, {0x00, 0x10, 0x02, 0x12, 0x21, 0x22},
    {0x01, 0x10, 0x11, 0x21, 0x12, 0x22}, {0x01, 0x12, 0x10, 0x20, 0x11, 0x21}, {0x01, 0x10, 0x12, 0x11, 0x21, 0x31},
    {0x00, 0x10, 0x20, 0x01, 0x02, 0x11}, {0x00, 0x01, 0x10, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x10, 0x02, 0x03, 0x12},
    {0x00, 0x10, 0x01, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x11, 0x02, 0x03, 0x12}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x13},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x13}, {0x00, 0x01, 0x11, 0x02, 0x12, 0x13}, {0x00, 0x01, 0x11, 0x03, 0x13, 0x14},
    {0x00, 0x10, 0x01, 0x11, 0x21, 0x02}, {0x00, 0x01, 0x02, 0x12, 0x03, 0x13}, {0x00, 0x01, 0x02, 0x12, 0x22, 0x03},
    {0x00, 0x01, 0x02, 0x03, 0x04, 0x14}, {0x00, 0x01, 0x02, 0x03, 0x04, 0x05}};
/* algorithm a's routes: m[i] bit j: operator j + 1 modulates operator i + 1. FM6_ALG runs the operators sixth
 * first, each reading a bus (or none) and writing one (or adding to it): who wrote a bus modulates its reader */
static void fm6_routes(uint32_t a, uint8_t *m)
{
    const uint8_t *f = FM6_ALG[a & 31u];
    uint8_t bus[4] = {0, 0, 0, 0};
    uint32_t k;
    for (k = 0; k < 6u; k++) {
        uint32_t op = 5u - k, in = (f[k] >> 4) & 3u, o = f[k] & 3u;
        m[op] = in ? bus[in] : 0u;
        bus[o] = (uint8_t)(((f[k] & FM6_OADD) ? bus[o] : 0u) | 1u << op);
    }
}
/* the operator with feedback in algorithm a (0..5: operator 1..6) */
static uint32_t fm6_fb_op(uint32_t a)
{
    uint32_t k;
    for (k = 0; k < 5u && (FM6_ALG[a & 31u][5u - k] & 0xC0u) != 0xC0u; k++)
        ;
    return k;
}
/* the carriers of algorithm a: bit k operator k + 1 (fm6_carriers counts the sixth first) */
static uint32_t fm6_car_ops(uint32_t a)
{
    uint32_t c = fm6_carriers(a), r = 0, k;
    for (k = 0; k < 6u; k++)
        r |= (c >> (5u - k) & 1u) << k;
    return r;
}
/* the algorithm FM6 plays on track t (0..31): ALG, or the patch's at PAT */
static uint32_t fm6_alg_of(const track_t *t)
{
    return t->p[P_E0] >= 1 && t->p[P_E0] <= 32 ? (uint32_t)t->p[P_E0] - 1u : fm6_patch[(t - trk) % NTRK][FP_ALG] & 31u;
}
#if FELUCCA_FM4
/* DIGITAL's eight algorithms as FM charts, read from src/eng_digital.c's switch (alg) (ui_test.c checks
 * these tables against it). FM_CELL: per operator 1..4 its grid cell, the column in 24 px steps (bits 0..2)
 * and the row above the output bus (bits 4..5; row 0 = the carriers). FM_MOD: per destination operator d a
 * nibble at bit 4 d of the operators modulating it (bit k = operator k + 1). Operator 4 feeds itself (FB). */
static const uint8_t FM_CELL[8][4] = {
    {0x00, 0x10, 0x20, 0x30}, {0x01, 0x11, 0x20, 0x22}, {0x01, 0x10, 0x20, 0x12}, {0x01, 0x12, 0x10, 0x20},
    {0x00, 0x10, 0x02, 0x12}, {0x00, 0x02, 0x04, 0x12}, {0x00, 0x02, 0x04, 0x14}, {0x00, 0x02, 0x04, 0x06}};
static const uint16_t FM_MOD[8] = {0x0842, 0x00C2, 0x004A, 0x0806, 0x0802, 0x0888, 0x0800, 0x0000};
/* Modulators above what they modulate, the carriers on the bottom row over the output bus (an arrow out).
 * Carriers THEME with INK numerals, modulators RAISE with THEME ones; an operator at LEVEL 0: DIM numeral.
 * Routes by IDX: DIM at 0, MID, THEME from 64; op 4's feedback loop MID (DIM at FB 0). The knob just turned
 * (an operator's ratio or level, IDX, FB): ACCENT. Junction dots where routes split or merge. */
static void graph_fm(const track_t *t, uint16_t c)
{
    uint32_t alg = (uint32_t)t->p[P_E0] & 7u, k, j, mods = FM_MOD[alg], car = 0, hop = 9;
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    int32_t x[4], y[4], lo = 7, hi = 0, top = 0, bus, cl = 240, cr = 0, idx = t->p[P_E4];
    uint16_t ec = hot == P_E4 ? T_ACCENT : !idx ? T_DIM : idx < 64 ? T_MID : c;
    if (hot >= P_E1 && hot <= P_E3) hop = hot - P_E1 + 1u;
    else if (hot >= P_FM1_ATK && hot < P_E0) hop = (hot - P_FM1_ATK) / 5u;
    for (k = 0; k < 4u; k++) {
        int32_t col = FM_CELL[alg][k] & 7, row = FM_CELL[alg][k] >> 4;
        lo = col < lo ? col : lo; hi = col > hi ? col : hi; top = row > top ? row : top;
    }
    bus = 50 + (27 + 23 * top) / 2;                  /* the chart centred on the 100 px scale */
    for (k = 0; k < 4u; k++) {
        x[k] = 120 + (FM_CELL[alg][k] & 7) * 24 - (lo + hi) * 12;
        y[k] = bus - 6 - FM_BH - 23 * (FM_CELL[alg][k] >> 4);
        if (FM_CELL[alg][k] < 0x10) { car |= 1u << k; cl = x[k] < cl ? x[k] : cl; cr = x[k] > cr ? x[k] : cr; }
    }
    for (k = 0; k < 4u; k++) {                       /* into k: sources down to its jog row, across, down */
        uint32_t m = (mods >> (4u * k)) & 15u;
        for (j = 0; j < 4u; j++) if (m >> j & 1u) {
            uint32_t fan = mods & (0x1111u << j);
            fm_route(x[j], y[j] + FM_BH, x[k], y[k] - 3, y[k], ec);
            if (fan & (fan - 1u)) fm_dot(x[j], y[k] - 3, ec);   /* one operator modulating several */
        }
        if (m & (m - 1u)) fm_dot(x[k], y[k] - 3, ec);           /* several modulating one */
        if (car >> k & 1u) {
            cv_rect(x[k], y[k] + FM_BH, 1, bus - y[k] - FM_BH, T_MID);
            if (x[k] != cl) fm_dot(x[k], bus, T_MID);
        }
    }
    fm_bus(cl, cr, bus);                             /* the output bus and its arrow */
    fm_loop(x[3], y[3], hot == P_E6 ? T_ACCENT : t->p[P_E6] ? T_MID : T_DIM);   /* op 4's feedback */
    for (k = 0; k < 4u; k++) {
        char b[2] = {(char)('1' + k), 0};
        uint32_t on = t->p[P_FM1_LEVEL + k * 5u] != 0, fill_c = (car >> k & 1u) && on;
        uint16_t f = k == hop ? T_ACCENT : c, fill = fill_c ? f : T_RAISE;
        cv_rrect(x[k] - 10, y[k], 21, FM_BH, 3, fill, T_SURF);
        GFX_HOOK_ALIGN(x[k] - 10, y[k], x[k] + 11, y[k] + FM_BH, AL_HV, "DIGITAL operator number in its box");
        cv_text_in(x[k] - 10, y[k] + CAP_IN(S, FM_BH), 21, &AF_S, b, fill_c ? T_INK : on ? f : T_DIM, fill);
    }
}
#endif
/* Save-bank validity is expensive (CRC/import). Refresh it once per redraw or
 * twice per second, rather than parsing four projects on every UI frame. */
static char graph_pname[4][13];                       /* .. and the slots' names ("" none) */
static uint32_t graph_pname_sig;
static int graph_project_used(uint32_t slot)
{
    static uint32_t ms, frame;
    static uint8_t mask, ready;
    if (!ready || fm1_ms - ms >= 500u || (ui.force && frame != ui.frame)) {
        uint32_t i; mask = 0;
        for (i = 0; i < 4u; i++) {
            mask |= (uint8_t)((project_name(i, graph_pname[i]) != 0) << i);
            part_play.b[i] = (uint8_t)((mask >> i) & 1u ? part_scan(i) : 0u);   /* (V10.6o) PARTS, TIMELINE: its tracks
                                                         * (proj_scratch as read) */
        }
        graph_pname_sig = fnv(2166136261u, graph_pname, sizeof graph_pname);
        ready = 1; ms = fm1_ms; frame = ui.frame;
    }
    return (mask >> (slot & 3u)) & 1u;
}
static const char *graph_project_name(uint32_t slot)  /* (after graph_project_used) */
{
    return graph_pname[slot & 3u];
}

/* MENU > LARGE's strip under the tall cards (ui.c LK_TALL): HOME's scope, ENV's ADSR, LFO's wave, PATTERN's steps,
 * MIXER's tracks drawn small; on the other pages the page's title and number (their charts need the whole panel) */
enum { SK_NONE, SK_SCOPE, SK_ADSR, SK_LFO, SK_STEPS, SK_TRK, SK_TITLE };
static uint32_t strip_kind(void)
{
    uint32_t g;
    if (large_kind() != LK_TALL)
        return SK_NONE;
    if (ui.home)
        return SK_SCOPE;
    g = cur_page()->graph;
    return g == GR_ADSR ? SK_ADSR : g == GR_LFO ? SK_LFO : g == GR_STEPS ? SK_STEPS : g == GR_TRK ? SK_TRK : SK_TITLE;
}
/* the page's title and its number in its family ("ENV DEST 2/2", EDIT: the engine's "OSC 1/2"); the piano roll: its
 * scale ("C MIN"); ti holds 20 (the footer, the strip) */
static int roll_view(const page_t *pg);
static void page_title(char *ti)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    const engine_t *e = trk_engp(t);
    uint32_t i, n = 0, k = 0;
    char nb[24];
    const char *pt = pg->scope == SC_ENGINE ? e->page_title[pg->id[0] != P_E0] : 0;   /* EDIT: the engine's */
    str_cpy(nb, pg->title, sizeof nb);
    if (!page_over && !ui.home) {                       /* (0.9.2) the page within its group ("ENV DEST 2/2", FX: the
                                                         * effect's name, "DELAY 2/3") */
        uint8_t cnt[8], gi[8];
        uint32_t g, m;
        (void)nav_groups(cnt, gi, &g, &k, &i, &m);
        n = cnt[g];
        k++;
        if (pg->fam == FAM_FX && fx_level() == FXL_EFFECT)
            str_cpy(nb, nav_group_name(gi[g]), sizeof nb);
        else
            str_cpy(nb, nav_page_name(), sizeof nb);
    } else
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == pg->fam && page_visible(i)) {
                n++;
                if (i == ui.page)
                    k = n;
            }
    str_cpy(ti, pt ? pt : grid_on() && roll_view(pg) ? "GRID" : nb, 12);
    if (n > 1) {
        str_cpy(ti + str_len(ti), " ", 4);
        fmt_frac(ti + str_len(ti), (int32_t)k, (int32_t)n);
    }
    if (roll_view(pg) && !drum_track(t)) {          /* the piano roll: its tinted rows' scale ("C MIN") */
        str_cpy(ti, N_NOTE[(uint32_t)t->p[P_ROOT] % 12u], 4);
        str_cpy(ti + str_len(ti), " ", 4);
        str_cpy(ti + str_len(ti), N_SCALE[clamp(t->p[P_SCALE], 0, (int32_t)(sizeof N_SCALE / sizeof N_SCALE[0]) - 1)], 8);
    }
}
/* the strip's title: L centred (M when L lacks a glyph or is too wide) */
/* the thermal ramp (JIANT): a value 0..256 as heat, as a thermal camera sees it: cyan (cold) .. blue .. violet .. red
 * .. orange .. yellow .. white (other palettes: their surface toward their accent and text) */
static uint16_t heat_col(int32_t v)
{
    static const uint16_t H[7] = {0x3955u, 0x82DFu, 0xFA09u, 0xFBA3u, 0xFDEBu, 0xFF99u, 0xFFFFu};   /* (0.8.9, the skins' mock-up: the 0.6's
                                                         * ramp, indigo violet red orange amber cream white; was cyan blue violet
                                                         * red orange yellow white) */
    int32_t k;
    v = clamp(v, 0, 256);
    if (settings.palette != UI_JIANT_INDEX)
        return v < 128 ? ux_mix(T_SURF, T_ACCENT, v * 100 / 128) : ux_mix(T_ACCENT, T_TEXT, (v - 128) * 100 / 128);
    k = v * 6 / 257;
    return ux_mix(H[k], H[k + 1], (v * 6 - k * 256) * 100 / 256);
}

/* DRUM (DRUM-X): the eight sounds as columns, each as high and as hot as its hit now (a muted one crossed); the MORPH
 * A .. B above. sel: the sound edited (EDIT > SOUND), its name in the theme colour; -1 none */
static void graph_drum(const track_t *t, int32_t sel)
{
    const drum_lane_t *K = drum_kit_of(t);
    int32_t morph = clamp(t->p[P_E0] + dx_mot_v, 0, 127), mx = 30 + morph * 180 / 127;   /* (0.5.1: X-MOD live) */
    uint32_t l;
    cv_rect(30, 12, 180, 1, T_LINE);
    cv_rect(30, 11, mx - 30, 3, heat_col(morph * 2));
    cv_text(12, 8, &AF_S, "A", morph < 64 ? T_THEME : T_DIM);
    cv_text_r(226, 8, &AF_S, "B", morph >= 64 ? T_THEME : T_DIM, T_SURF);
    for (l = 0; l < 8u; l++) {
        int32_t x = 18 + (int32_t)l * 26, e = K && K[l].x.live ? K[l].x.ea >> 22 : 0, h = 4 + e * 66 / 256;
        int muted = dx_lane_muted(l);
        cv_rect(x, 94 - 70, 16, 70, T_SURF);
        cv_rect(x + 7, 24, 2, 70, T_LINE);
        if (!muted)
            cv_rect(x, 94 - h, 16, h, e ? heat_col(e) : T_DIM);
        else {
            cv_rect(x + 2, 58, 12, 1, T_DIM);
        }
        cv_text_in(x - 4, 100, 24, &AF_S, drum_lane_abbr(t, l), sel == (int32_t)l ? T_THEME : muted ? T_DIM : T_MID, T_SURF);
    }
}

static void graph_title(void)
{
    char ti[20];
    const aafont_t *f = &AF_L;
    int32_t y;
    page_title(ti);
    if (!large_face_has(ti) || text_w(f, ti) > 220)
        f = &AF_M;
    y = f == &AF_L ? CAP_IN(L, LG_H_GRAPH) : CAP_IN(M, LG_H_GRAPH);
    GFX_HOOK_ALIGN(0, 0, 240, LG_H_GRAPH, AL_HV, "large strip title centred");
    cv_text_in(0, y, 240, f, ti, T_THEME, T_SURF);
}


/* (0.9.9-seq2b, the approved mock-up's chapter 1; ported from 0.9.17's commit "every page the same piano roll") every SEQ
 * page draws the same picture: the bars' strip and the roll of the cursor's bar with the keys at its left and every
 * playhead (a kit: the grid), then the page's own marks over it (seq_overlay); its knobs stay the page's. (0.8.3 ..
 * 0.9.9-seq2a: PATTERN STRETCH TRACK the big bars, COMP the step large, PLAYHEADS MUTATE AUTO LIST their own panels.)
 * PHRASES and SONG keep their lists. TODO (the mock-up's slice 9): AUTOMATION's lane under a shorter roll (lane2), a value
 * a step, once motion.c keeps one; till then its records as marks at the roll's foot */
static int seq_roll_view(const page_t *pg)
{
    return pg == &PAGES[ui.page] && lens_on() && pg->graph != GR_PATS && pg->graph != GR_SONG && pg->graph != GR_EUCLID;
}
static int roll_view(const page_t *pg)
{
    return pg->fam == FAM_SEQ && pg->graph == GR_ROLL;
}
static void seq_col(uint32_t dr, uint32_t i, int32_t *x, int32_t *w)
{
    *x = dr ? 40 + (int32_t)i * 12 : PR_CX(i);
    *w = dr ? 10 : PR_CWI(i);
}
/* (V10.6v) SEQ > COMP, a RAMP on the cursor step (column k of the bar shown): its CURVE over the steps it lasts (cold UI:
 * in the Bluetooth segment, as gen.c's cards) */
BTAPP_FN static void ramp_draw_bt(const track_t *t, uint32_t dr, int32_t top, uint32_t k)
{
    const step_t *cs = &seq_steps(t)[ui.cursor % NSTEP];
    uint32_t v = comp_arg(cs), nc = v & CV_RAMP_BARS ? 16u : cs->clen[comp_slot()] + 1u;
    int32_t x, w, x1, w1, px;
    nc = nc < 16u - k ? nc : 16u - k;
    seq_col(dr, k, &x, &w);
    seq_col(dr, k + nc - 1u, &x1, &w1);
    w = x1 + w1 - x;
    for (px = 0; px < w; px++)
        cv_rect(x + px, top + 12 - (ramp_curve(CV_RAMP_CURVE(v), px * 32768 / w) * 12 >> 15), 1, 2, T_TEXT);
}
/* the page's marks over the roll / the grid (0.9.17's): PLAYHEADS FUGUE's heads on the grid too (the roll draws them on
 * every page); COMP the steps with the cursor's component a bar on top ACCENT, another component a thin MID one;
 * AUTOMATION / AUTO LIST the records at the foot (a lock ACCENT, automation TEXT; AUTO LIST the row picked framed THEME);
 * MUTATE a chip, the next mutation; TRACK the notes PITCH moves them to, MID outlines */
static __attribute__((noinline)) void seq_overlay(const track_t *t, const page_t *pg)
{
    uint32_t dr = drum_track(t), len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), base = ui.bank * 16u, i, k;
    int32_t top = dr ? 19 : PR_Y0, bot = dr ? 19 + 8 * 15 - 1 : PR_Y0 + PR_ROWS * PR_RH, x, w;
    if (dr)
        pr_heads(t, top, bot, dr);
    if (pg->graph == GR_COMP) {
        uint32_t mine = comp_type(&seq_steps(t)[ui.cursor % NSTEP]);
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            uint32_t cp = comp_type(st);
            if (!cp || !step_on(st))
                continue;
            seq_col(dr, i, &x, &w);
            if (cp == mine)
                cv_rect(x + 1, top - 3, w - 2, 3, T_ACCENT);
            else
                cv_rect(x + 2, top - 2, w - 4, 1, T_MID);
        }
        k = ui.cursor % NSTEP;
        if (mine == CA_RAMP && k >= base && k < base + 16u && bt_hw_seg())
            ramp_draw_bt(t, dr, top, k - base);
    } else if (pg->graph == GR_MOTION || pg->graph == GR_EVENTS) {
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx), cur = ev_row(n);
        for (k = 0; k < n; k++) {
            const motion_event_t *e = &motion.event[idx[k]];
            uint32_t sx = MOTION_STEP(e);
            if (sx < base || sx >= base + 16u || sx >= len)
                continue;
            seq_col(dr, sx - base, &x, &w);
            cv_rect(x + 2, bot - 3, w - 4, 2, (e->param & MOTION_LOCK) ? T_ACCENT : T_TEXT);
            if (pg->graph == GR_EVENTS && k == cur)
                cv_frame(x, top, w + 1, bot - top, T_THEME);
        }
    } else if (pg->id[0] == P_POFS && !dr && t->p[P_POFS]) {
        for (i = 0; i < 16u && base + i < len; i++) {
            const step_t *st = &seq_steps(t)[base + i];
            if (st->time != ST_NOTE)
                continue;
            seq_col(dr, i, &x, &w);
            for (k = 0; k < st->n && k < 4u; k++) {
                int32_t y = pr_row_y(st->note[k] + t->p[P_POFS]);
                if (y >= PR_Y0 && y < bot)
                    cv_frame(x + 1, y, w - 2, PR_RH - 1, T_MID);
            }
        }
    }
}

/* (0.6.51) the page's name big (page_flash). (0.6.61) In the header, not over the cards or the graph (covering them
 * lost the sense of what a turn did): for PF_MS the header shows the page's name (M) and, smaller at the right, the
 * track and its engine; on HOME (a track picked there) the track's engine big and "HOME" small. Then the header comes
 * back (ui.force). (0.6.64) Over the footer instead, which says the same: the header (load, battery) stays */
static void draw_crumbs(int32_t y, uint16_t fg, uint16_t last);   /* (ui_draw.c) */
#define CRUMB_Y 4                                    /* (ui_draw.c: a lens footer's breadcrumb line) */
static void page_flash_draw(void)
{
    static uint32_t drawn;
    uint32_t e = fm1_ms - pf_ms, ph, dim;
    char nm[20], sub[20] = {'T', 0, 0};
    const page_t *pg = cur_page();
    const char *pt = pg->scope == SC_ENGINE ? trk_engp(TSEL)->page_title[pg->id[0] != P_E0] : 0;
    const char *en = trk_engp(TSEL)->name;
    if (e >= PF_MS) {
        pf_on = 0;
        ui.foot_sig ^= 1u;                           /* (0.6.64) the footer, whole again: that alone redrawn */
        return;
    }
    dim = e > PF_MS - 120u;
    ph = (pf_ms << 1 | dim) + 1u;                     /* (a new flash or its dimming; else every 4th frame: cheap) */
    if (!ui.force && ph == drawn && (ui.frame & 3u))
        return;
    drawn = ph;
    sub[1] = (char)('1' + song.sel % NTRK);
    sub[2] = ' ';
    if (ui.home) {                                   /* HOME: the engine big, "T2 HOME" small */
        str_cpy(nm, en, sizeof nm);
        str_cpy(sub + 3, "HOME", sizeof sub - 3u);
    } else {
        str_cpy(nm, pt && pt[0] != '-' ? pt : pg->title, sizeof nm);
        str_cpy(sub + 3, en, sizeof sub - 3u);
    }
    if (lz_on()) {                                   /* (0.8.5) a lens page: its breadcrumb lit (the tabs say the level) */
        lz_begin(240, foot_h(), 0, foot_y());
        draw_crumbs(CRUMB_Y + LZ_FOOT_Y - (int32_t)foot_y(), dim ? T_MID : T_TEXT, dim ? T_MID : T_TEXT);
        lz_blit(0, foot_y());
        return;
    }
    {   /* (0.6.64) at the bottom: the footer says the same */
        int32_t fh = H_FOOT;
        cv_begin(240, (uint32_t)fh, T_BG);
        cv_bg = T_BG;
        cv_text(8, (fh - (int32_t)AF_M.h) / 2, &AF_M, nm, dim ? T_MID : T_TEXT);
        cv_text_r(232, (fh - (int32_t)AF_S.h) / 2, &AF_S, sub, dim ? T_DIM : T_ACCENT, T_BG);
        cv_blit(0, foot_y());
    }
}

/* (0.6.61) the panels' cytoplasm: a few granules drifting behind the graph, as HOME's, so the screen breathes. LOAD
 * FULL and ANIM ON only, not on HOME (its own), the lists and the dense grids (the roll, the steps); (0.6.64) no
 * redraw of their own: they move when the panel redraws anyway (a knob, the LFO, the envelope), so they cost nothing */
#ifndef UI_NO_DRIFT
#define UI_NO_DRIFT 0                                /* (tests/ui_test.c: 1, its frames compared pixel by pixel) */
#endif
static int drift_on(void)
{
    uint32_t g = cur_page()->graph;
    if (UI_NO_DRIFT)
        return 0;
    return !ui.home && !sys_load && !ANIM_OFF() && strip_kind() == SK_NONE && !roll_view(cur_page()) &&
           !seq_roll_view(cur_page()) &&
           g != GR_ROLL && g != GR_SHAPE && g != GR_COMP && g != GR_FUGUE && g != GR_LIVE &&   /* (0.8.3: LENTE's step, the
                                                         * heads; p2: LIVE's rails) */
           g != GR_STEPS && g != GR_CHANCE && g != GR_BROWSE && g != GR_USER && g != GR_SLOTS && g != GR_EVENTS &&
           g != GR_SONGSAVE && g != GR_SONG && g != GR_TOOLS && g != GR_PATS;
}
static void graph_drift(void)
{
    uint32_t k, gh = graph_h();
    for (k = 0; k < 14u; k++) {
        uint32_t h = k * 2654435761u + 0x9E37u;
        int32_t gx = 10 + (int32_t)((h >> 8) % 220u + ui.frame * (1u + (h >> 28)) / 8u) % 220;
        int32_t gy = 4 + (int32_t)(((h >> 16) % (gh - 8u)) + (uint32_t)(sine_i((ui.frame * 200u + h) << 16) >> 13)) % (int32_t)(gh - 8u);
        cv_rect(gx, gy, 1 + (int32_t)(h >> 31), 1 + (int32_t)(h >> 31), ux_mix(T_SURF, T_THEME, 18 + (int32_t)(h >> 29) * 3));
    }
}
/* (0.8.5) SAVE > LIBRARY's picture of the other projects (ui_lzsave.c) */
static struct {
    uint32_t gen;                                       /* lib_gen() when the scan began */
    uint8_t next;                                       /* the next project to scan (NSONG: done) */
    uint8_t mask[NSONG];                                /* sections stored, a bit each */
    char name[NSONG][13u];
} lib = {0xFFFFFFFFu, 0, {0}, {{0}}};
static void lib_poll(void)                              /* (graph_signature, once a frame) */
{
    if (lib.gen != lib_gen()) {
        lib.gen = lib_gen();
        lib.next = 0;
    }
    if (lib.next < NSONG) {
#if FELUCCA_FLASH
        lib.mask[lib.next] = (uint8_t)lib_scan(lib.next, lib.name[lib.next]);
#endif
        lib.next++;
    }
}
/* (0.9.5) a toast (ui_say) on a lens page: a pill of BG behind its words alone, near the panel's foot, over the page
 * drawn as ever (its graph, roll, beings live behind it); HOME and the layers: the header says it.
 * The pill keeps clear of the page's own words (gfx.c cv_txt: every text drawn on the panel this frame): near the foot
 * if nothing is written there, else the nearest rows lower, then higher, with none; a page written all over (MUTATE's
 * lines, AUTO LIST's rows where no gap is a pill high): no pill, the header says it alone, as on HOME */
static int toast_on(void) { return lz_on() && ui.msg_t && ui.msg[0] && !ui.layer && !ui.home; }
#define TOAST_H 17
#define TOAST_GAP_X 3                                /* (the clearance round a word: its 2 px halo and a pixel) */
#define TOAST_GAP_Y 2
static int16_t toast_at[4];                          /* the pill drawn last (screen x0 y0 x1 y1; x1 == x0: none) */
static int toast_clear(int32_t x, int32_t y, int32_t w)
{
    return !cv_txt_in(x - TOAST_GAP_X, y - TOAST_GAP_Y, x + w + TOAST_GAP_X, y + TOAST_H + TOAST_GAP_Y);
}
/* the pill's top row on the panel (canvas rows), -1: nowhere clear */
static int32_t toast_row(int32_t x, int32_t w)
{
    int32_t gh = (int32_t)graph_h(), y0 = gh >= 3 * TOAST_H ? gh - TOAST_H - 5 : (gh - TOAST_H) / 2, y;
    for (y = y0; y <= gh - TOAST_H - 1; y++)         /* where it was, or lower (nearer the foot) */
        if (toast_clear(x, y, w))
            return y;
    for (y = y0 - 1; y >= 1; y--)                    /* .. else higher */
        if (toast_clear(x, y, w))
            return y;
    return -1;
}
static void toast_pill(void)
{
    int32_t w = clamp(text_w(&AF_S, ui.msg) + 16, 40, 224), x = 120 - w / 2, y = toast_row(x, w);
    uint8_t hl = cv_halo;
    char f[40];
    toast_at[0] = toast_at[2] = 0;
    if (y < 0)                                       /* (no clear rows: the header's words alone) */
        return;
    toast_at[0] = (int16_t)x; toast_at[1] = (int16_t)(y + (int32_t)graph_y());
    toast_at[2] = (int16_t)(x + w); toast_at[3] = (int16_t)(toast_at[1] + TOAST_H);
    text_fit(f, sizeof f, ui.msg, &AF_S, w - 12);
    GFX_HOOK_COVER(x, y + cv_oy, x + w, y + cv_oy + TOAST_H);   /* (what the page has under it: hidden) */
    cv_halo = 1;                                     /* (its round ends blended onto what is there) */
    cv_rrect(x, y, w, TOAST_H, -(TOAST_H / 2), ux_mix(T_BG, T_ACCENT, 70), T_BG);
    cv_rrect(x + 1, y + 1, w - 2, TOAST_H - 2, -(TOAST_H / 2 - 1), T_BG, T_BG);
    cv_halo = 0;
    GFX_HOOK_ALIGN(x, y, x + w, y + TOAST_H, AL_HV, "toast message centred in its pill");
    cv_text_flags(ink_in(&AF_S, f, w) + x, y + CAP_IN(S, TOAST_H), &AF_S, f, T_ACCENT, T_BG, 8u | (f[str_len(f) - 1u] == ELLIPSIS));
    cv_halo = hl;
}
static void sound_name(const track_t *t, char *b);   /* (ui_draw.c) */
static void gen_panel(void);                         /* (0.8.5) gen.c: GEN in HOME's lens */
static uint32_t gen_draw_sig(void);
static __attribute__((noinline)) uint32_t graph_signature(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint32_t h = 2166136261u, i;
    if (toast_on())                                  /* (0.8.5) the toast over the panel (0.9.5: a pill on the page) */
        h = str_hash(h ^ 0x70A57u, ui.msg);
    if (ui.confirm == CF_DICE_ALL)                   /* (0.8.5) GEN: its settings, its page (0.9.0: MENU > LARGE's strip is
                                                      * HOME's scope there: live) */
        return h ^ gen_draw_sig() ^ (strip_kind() == SK_SCOPE ? anim_fr / 2u * 2654435761u : 0u);
    if (ui.home)                                     /* the beings: every other frame (0.9.9-beta4: of the animation's, anim_fr:
                                                      * held, or ANIM OFF, the band is not drawn again) */
        return h ^ (anim_fr / 2u) ^ (home_level() * 7u + home_set()) * 104729u;
    h ^= (uint32_t)pg->graph * 131u + TSEL->eng_req + song.sel * 7777u + ui.page * 1291u;
    if (strip_kind() == SK_TITLE)                    /* MENU > LARGE: the page's title (its engine, its number); (V10.8) an
                                                      * EDIT page: the engine's creature, alive */
        return h ^ 0x5A17u ^ (pg->fam == FAM_EDIT ? anim_fr / 2u * 2654435761u : 0u);
    if ((pg->graph == GR_NONE && !fx_on()) || SPLIT_PAGE(pg) || pg->graph == GR_ARP ||
        (pg->graph == GR_MOTION && !seq_roll_view(pg))) h ^= anim_fr / 2u;   /* (0.9.9-seq2b: AUTOMATION the roll, still) */   /* (0.8.5:
                                                         * FX's lens pictures change with their values, */
    if (fx_on() && (song.playing || bg_energy() > 2))   /* .. and with the sound while it sounds: FX live) */
        h ^= anim_fr / 2u * 2246822519u;
    if (pg->graph == GR_LFO && !ui_hold)             /* (JIANT 0.5) the LFO runs: its phase, its level (0.9.9-beta4: held under load) */
        h ^= (t->lfo_ph >> 23) * 2654435761u ^ (uint32_t)(t->lfo_val * t->lfo_fade >> 22);
    if (pg->graph == GR_ADSR && !ui_hold)            /* the envelope's dot: every voice's stage and level */
        for (i = 0; i < NVOICE; i++)
            h = (h ^ ((uint32_t)t->v[i].stage << 24 | (uint32_t)t->v[i].env >> 16)) * 16777619u;
    for (i = 0; i < P_COUNT; i++)
        h = (h ^ (uint32_t)t->p[i]) * 16777619u;
    h ^= (uint32_t)TSEL->preset * 7u + (uint32_t)song.g[G_SLOT] * 13u + TSEL->user * 257u + up_gen * 7919u + ui.uslot * 104729u +
         ui.ppick * 1299709u;
    if (pg->graph == GR_MSEQ)                        /* (JIANT) the step picked, the one playing, the level now */
        h ^= (ms_ui_step + 1u) * 40503u + (song.playing ? anim_fr * 2654435761u : 0u);
    if (pg->graph == GR_SONG) {
        for (uint32_t r = 0; r < CHAIN_ROWS; r++)
            h = (h ^ (song_idx.scene[song_cur][r].on * (r + 1u))) * 16777619u;
        h ^= ui.song_row * 40503u + chain_config.count * 7919u;
        for (i = 0; i < CHAIN_ROWS; i++)
            h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
        h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u + chain.pass * 15485863u : 0u;
        h ^= SONG_END(&chain_config) * 2246822519u;      /* (0.6.22: the END) */
        for (i = 0; i < 4u; i++) h ^= (uint32_t)graph_project_used(i) << (24u + i);
        for (i = 0; i < NTRK; i++) h = (h ^ (clip_now[i] | clip_go[i] << 4)) * 16777619u;   /* (V10.6s) CLIPS */
        h ^= live.srec * 3266489917u + ui.part * 668265263u;
        h += graph_pname_sig;
    }
    if (pg->graph == GR_MOD)
        h ^= (mod_ui_slot + 1u) * 40503u;
    if (pg->graph == GR_FOCUS)                       /* (0.8.5) EDIT > FOCUS: the parameter */
        h ^= (ui.focus + 1u) * 2246822519u;
    if (pg->graph == GR_STAGE)                       /* .. ENV > STAGE: the stage */
        h ^= (ui.stage + 1u) * 3266489917u;
    if (pd_on()) {                                   /* .. a lens of ui_lens.c: its level (ENV's TRACKS: every track live) */
        h ^= (pd_level() + 1u) * 668265263u;
        if ((pd_cur() == PD_ENV && pd_level() == 0u) || (pd_cur() == PD_MOD && pg->graph != GR_MSEQ))   /* (MOD's pulse,
                                                         * LFO DEST's LFO) */
            h ^= anim_fr / 2u * 2654435761u;
    }
    if ((pg->graph == GR_SLCR || pg->graph == GR_SLPIT) && song.g[G_SLMODE])   /* the SLICER's step playing */
        h ^= (sl.idx + 1u) * 2654435761u;
    if (pg->graph == GR_SLCR || pg->graph == GR_SLPIT)   /* (its globals: the page's knobs and the bus) */
        for (i = 0; i < G_SL_N; i++)
            h = (h ^ (uint32_t)song.g[G_SLMODE + i]) * 16777619u;
    if (pg->graph == GR_SLPIT)
        h ^= (sl_ui_step + 1u) * 40503u;
    if (FIT_PAGE(pg)) h ^= (t->seq_fit + 1u) * 104729u;
    if (pg->graph == GR_SONGSAVE) h ^= (song_save_target + 1u) * 2654435761u;
    if (pg->graph == GR_TOOLS)                       /* (0.6.12) the action picked, the song's row */
        h ^= ui.act * 7919u + ui.song_row * 104729u + (tl_t0 ? (fm1_ms - tl_t0 + 1u) / 40u + 1u : 0u) * 2654435761u;
    if (pg->graph == GR_SLOTS) {                     /* (a checksum over each slot; the project, the music's section, the song) */
        for (i = 0; i < 4u; i++)
            h ^= (uint32_t)graph_project_used(i) << (20u + i);
        h += graph_pname_sig + song_cur * 2654435761u + (uint32_t)(live.cur + 1) * 40503u + chain_config.count * 7919u;
        for (i = 0; i < CHAIN_ROWS; i++)
            h = (h ^ (chain_config.row[i].slot + 4u * chain_config.row[i].bars)) * 16777619u;
    }
    if (pg->graph == GR_MOTION) h ^= motion_count(t) * 131u + motion_enabled(t);
    if (pg->graph == GR_EVENTS) {                    /* AUTO LIST: the rows, the one selected, + ADD LOCK's place */
        uint8_t idx[MOTION_MAX];
        uint32_t n = ev_rows(idx);
        for (i = 0; i < n; i++)
            h = (h ^ ((uint32_t)motion.event[idx[i]].place << 24 | (uint32_t)motion.event[idx[i]].param << 16 |
                      (uint16_t)motion.event[idx[i]].value)) * 16777619u;
        h = (h ^ (n + 1u) * 40503u ^ ev_row(n) * 2654435761u ^ ((uint32_t)ui.ev_step << 8 | ui.ev_id) * 104729u) * 16777619u;
    }
    if (pg->graph == GR_CHANCE) h ^= ui.cursor * 40503u + step_chance(&t->step[ui.cursor]);
    if (pg->graph == GR_LIVE)                        /* (p2) GLO > LIVE: the playheads, the ghosts, the keys */
        h = live_sig(h);
    if (pg->graph == GR_FUGUE) {                     /* (0.6.52) the heads, their settings, where they are */
        uint32_t ti = song.sel % NTRK, k;
        h = (h ^ (steps_hash(t) + (song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu) * 31u + fg_ui * 7u)) * 16777619u;
        for (k = 0; k < FG_HEADS; k++)
            h = (h ^ ((uint32_t)fg_set[ti][k].rate | (uint32_t)fg_set[ti][k].dir << 4 | (uint32_t)(uint8_t)fg_set[ti][k].ivl << 8 |
                      (song.playing ? (uint32_t)fg_head[ti][k].idx << 16 : 0u))) * 16777619u;
    }
    if (pg->graph == GR_LYRIC || (pg->scope == SC_ENGINE && trk_engp(t) == &ENG_FORMANT &&
                                  t->p[P_VLANG])) {   /* (0.8.5) the syllable, the phoneme, the formants, the gains */
        const sing_t *s = sing_view(t);
        h = (h ^ ((uint32_t)s->nsy << 16 | (uint32_t)s->i << 8 | (uint32_t)s->np)) * 16777619u;
        h = (h ^ ((uint32_t)(s->f[0] >> 4) << 20 | (uint32_t)(s->f[1] >> 5) << 8 | (uint32_t)(s->gv >> 11) << 4 |
                  (uint32_t)((s->ga | s->gf) >> 12))) * 16777619u;
        h = (h ^ str_hash(lyr_gen[(t - trk) % NTRK], s->w)) * 16777619u;
    }
    if (pg->graph == GR_DXIMG)                       /* (0.6.48) IMAGE and how wide it plays */
        h = (h ^ ((uint32_t)t->p[P_DXIMG] << 16 | (uint32_t)dx_img_k(t) | (uint32_t)t->p[P_SIMG] << 24 |
                  (trk_eng(t) != ENGI_DRUM ? vx_on_mask(t) << 9 : 0u))) * 16777619u;   /* (V10.49: a synth's, its voices lit) */
    if (pg->fam == FAM_EDIT && trk_engp(t) == &ENG_INPUT)   /* (V10.54) INPUT: its meters live */
        h = (h ^ ((uint32_t)(ain_pk[0] >> 10) | (uint32_t)(ain_pk[1] >> 10) << 6 | (uint32_t)(mod_follow >> 10) << 12 |
                  (uint32_t)(ain_gg[(uint32_t)(t - trk) % NTRK] >> 12) << 18)) * 16777619u;
    if (pg->fam == FAM_EDIT && trk_engp(t) == &ENG_INPUT && t->p[P_E0] == AIN_SW)   /* (SWARM: it moves every frame) */
        h ^= ui.frame * 2654435761u;
    if (DX_KIT3(pg))                                 /* (0.8.8) KIT 3's colony lives every frame, as KIT's (was still) */
        h ^= ui.frame * 2654435761u;
    if (pg->graph >= GR_FMOP && pg->graph <= GR_FMENV)   /* (0.9.6) FM6's operators: the colony lives, as the beings */
        h ^= ui.frame / 2u * 2246822519u;
    if (pg->graph == GR_MORPH)                       /* (0.6.45) the wave, MORPH */
        h = (h ^ ((uint32_t)t->p[P_MORPH] << 16 | (uint32_t)t->p[P_E0] << 8 | (uint32_t)t->p[P_E1] |
                  (uint32_t)t->p[P_E2] << 24)) * 16777619u;
    if (pg->graph == GR_SCENE) {                     /* (V10.25) the roles, the track picked, the last key */
        for (i = 0; i < NTRK; i++) h = (h ^ scn_role[i] ^ (uint32_t)drum_track(&trk[i]) << 4 ^ (uint32_t)trk[i].p[P_CHRD] << 8) * 16777619u;
        h = (h ^ scn_on ^ (uint32_t)sc_trk << 2 ^ (uint32_t)last_note << 8) * 16777619u;
    }
    if (pg->graph == GR_EUCLID) {                    /* (V10.6) the pattern, the lane, the key, the step playing */
        h = (h ^ steps_sig(t) ^ (uint32_t)t->p[P_SLEN] << 24) * 16777619u;
        h = (h ^ eu[song.sel % NTRK].lane ^ (uint32_t)eu[song.sel % NTRK].src << 4 ^ (uint32_t)eu[song.sel % NTRK].root << 8 ^
             (song.playing ? t->seq_idx + 1u : 0u) << 16) * 16777619u;
    }
    if (pg->graph == GR_GCOMP) {                     /* (0.9.9-seq2f) the global lane, its cursor, MONO, the step playing */
        h = (h ^ (gl_cur * 7919u + gl_trk * 31u + gl_mono * 977u + (song.playing ? gl_step_at(0) % GL_N + 1u : 0u))) *
            16777619u;
        for (i = 0; i < GL_N; i++)
            h = (h ^ ((uint32_t)glane[i].comp | (uint32_t)glane[i].val << 8 | (uint32_t)glane[i].len << 16 |
                      (uint32_t)glane[i].mask << 24) ^ glane[i].nm * 131u) * 16777619u;
    }
    if (pg->graph == GR_COMP) {                      /* (0.6.39) the steps, their components, the arpeggio and the loop */
        const step_t *cs = &t->step[ui.cursor];
        uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu;
        h = (h ^ (steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.bank * 613u)) * 16777619u;
        h = (h ^ ((uint32_t)t->p[P_CLEN] << 8 | (song.playing ? t->cpos : 0xFFu))) * 16777619u;   /* (0.6.47) C.LEN */
        for (i = ui.bank * 16u; i < ui.bank * 16u + 16u && i < NSTEP; i++)
            h = (h ^ ((uint32_t)t->step[i].comp << 8 | t->step[i].carg | (uint32_t)t->step[i].probability << 16 |
                      (uint32_t)t->step[i].flags << 24 | (uint32_t)t->step[i].comp2 << 4 | (uint32_t)t->step[i].clen[0] << 12 |
                      (uint32_t)t->step[i].clen[1] << 20)) * 16777619u;
        h = (h ^ (comp_view() * 977u + cs->carg + (song.playing ? t->arp_k * 131u + t->loops * 7u + t->arp_from : 0u))) *
            16777619u;
        h = (h ^ ((uint32_t)cs->carg2 | (uint32_t)cs->cnm[0] << 8 | (uint32_t)cs->cnm[1] << 16 | comp_slot() << 24)) *
            16777619u;                                  /* (0.6.82) COMP 2; (seq2d) the N:Ms */
    }
    if (pg->scope == SC_ENGINE && (trk_engp(t) == &ENG_WHEEL || trk_eng(t) == ENGI_FM6 ||
                                   (FELUCCA_FM4 && trk_eng(t) == ENGI_DIGITAL)))
        h ^= (ui.hot_t ? ui.hot_col + 1u : 0u) * 65537u;
    if (pg->scope == SC_ENGINE && trk_eng(t) == ENGI_DRUM)
        h ^= ui.frame * 2654435761u;                 /* DRUM-X: the specimen moves every frame */
    if (pg->graph == GR_DXSND) h ^= ui.frame * 2654435761u ^ dx_sig();
    if (pg->graph == GR_SWARM) h ^= ui.frame * 2654435761u;   /* (V10.6z6: the morph, live) */
    if (pg->scope == SC_ENGINE && trk_eng(t) == ENGI_FM6)   /* the patch (PAT's algorithm, levels, FB) */
        h ^= (fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u;
    if (fop_page(pg->graph))                         /* (0.6.77) OPERATOR, OP ENV (track pages): the patch, the operator */
        h = (h ^ ((fm6_pgen[(t - trk) % NTRK] + 1u) * 2246822519u ^ (uint32_t)fop_op << 8 ^ fop_stg)) * 16777619u;
    if (save_on()) {                                 /* (0.8.5) SAVE's lens: the level, the library's scan, the being */
        lib_poll();
        h = (h ^ (save_level() + 1u) * 2654435761u ^ lib.next * 40503u ^ lib.gen) * 16777619u;
        if (pg->graph == GR_USER || pg->graph == GR_BROWSE)
            h ^= bg_energy() > 2 ? anim_fr * 2654435761u : 0u;   /* (the being moves with the sound) */
        if (pg->graph == GR_SONG) {                  /* (ARRANGE: as SLOTS', the rows and where the song is) */
            for (i = 0; i < CHAIN_ROWS; i++)         /* (add15: ARRANGE 2's tracks; V10.6o: the rows' mutes) */
                h = (h ^ song_idx.tsrc[song_cur][i] ^ song_idx.tvar[song_cur][i] * 31u ^
                     (uint32_t)song_idx.scene[song_cur][i].mute << 7 ^ (uint32_t)song_idx.scene[song_cur][i].on << 3) * 16777619u;
            h ^= (ui.part + 1u) * 2654435761u + song.sel * 977u + (uint32_t)(live.req + 2) * 7919u + live.mode * 131u +   /* (PARTS: */
                 chain.hold * 31u + song.playing * 3u + (song.playing ? anim_fr / 8u * 40503u : 0u) +   /* the part, what */
                 part_play.w * 104729u;               /* loops, pulsing; its tracks) */
            for (i = 0; i < 4u; i++) h ^= (uint32_t)graph_project_used(i) << (20u + i);
            h += graph_pname_sig + (uint32_t)(live.cur + 1) * 40503u;
        }
        if (pg->graph == GR_SLOTS) {
            h ^= SONG_END(&chain_config) * 2246822519u + ui.song_row * 7919u;
            h ^= chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u : 0u;
            for (i = 0; i < CHAIN_ROWS; i++) h = (h ^ (uint32_t)song_idx.scene[song_cur][i].on * (i + 1u)) * 16777619u;
        }
        if (pg->graph == GR_BROWSE)
            h ^= list_get() * 977u + preset_favorite() * 31u;
    }
    if (fx_on()) {                                   /* (0.8.5) FX's lens: the level, the module, the buses' values, the
                                                         * slicer's step, the output (MASTER), the punch lane */
        h = (h ^ (fx_level() + 1u) * 2654435761u ^ ui.fxmod * 40503u) * 16777619u;
        for (i = 0; i < G_COUNT; i++)
            h = (h ^ (uint32_t)song.g[i]) * 16777619u;
        if (fx_level() == FXL_BUSES || (fx_level() == FXL_EFFECT && fx_mod_of(ui.page) == 3u))   /* (the SLICER drawn) */
            h ^= (song.playing && song.g[G_SLMODE] ? sl.idx + 1u : 0u) * 2654435761u;
        h ^= sl_ui_step * 7919u + (uint32_t)pfx_rec_ok() * 31u;
        if (fx_level() == FXL_BUSES || fx_mod_of(ui.page) == 5u)   /* (MASTER: last, snd2) */
            h ^= (uint32_t)(bg_energy() / 16) * 104729u;   /* (MASTER's level bar: 16 steps, not every frame) */
        for (i = 0; i < 32u; i++)
            h = (h ^ pfx_lane[i]) * 16777619u;
    }
    if (lens_on()) {                                 /* (0.8.3) LENTE: the level; its STEP, the cursor step's every byte */
        h = (h ^ (lens_level() + 1u) * 2654435761u) * 16777619u;
        h = (h ^ (song.playing ? seq_src(t, t->seq_idx) / 16u + 1u : 0u) * 40503u) * 16777619u;   /* (0.8.5) the bar
                                                         * playing: its number lit in the bars */
        if (seq_roll_view(pg)) {                     /* (0.9.9-seq2b) the roll: every playhead's step (the keys light
                                                       * what each sounds, in any bar: a step at a time), the page's marks */
            uint32_t k;
            for (k = 0; k <= FG_HEADS; k++)
                h = (h ^ (pr_head_step(t, k) + 1u)) * 16777619u;
            h = (h ^ (pg->graph == GR_FUGUE ? fg_ui + 1u : 0u) ^ (uint32_t)(uint8_t)t->p[P_POFS] << 8) * 16777619u;
            for (k = 0; k < FG_HEADS; k++)
                h = (h ^ (uint32_t)(uint8_t)fg_set[song.sel % NTRK][k].ivl) * 16777619u;
            if (pg->graph == GR_COMP)
                h = (h ^ (comp_slot() + 1u) * 40503u) * 16777619u;
            if (pg->graph == GR_MOTION || pg->graph == GR_EVENTS) {
                uint8_t idx[MOTION_MAX];
                uint32_t n = ev_rows(idx);
                for (k = 0; k < n; k++)
                    h = (h ^ motion.event[idx[k]].place ^ (uint32_t)motion.event[idx[k]].param << 8) * 16777619u;
                h = (h ^ (n + 1u) ^ ev_row(n) << 8) * 16777619u;
            }
        }
    }
    if (pg->graph == GR_STEPS || pg->graph == GR_ROLL || pg->graph == GR_CHANCE || pg->graph == GR_SHAPE || roll_view(pg) ||
        seq_roll_view(pg)) {
        uint32_t ph = song.playing ? seq_src(t, t->seq_idx) : 0xFFFFu, rl = roll_view(pg) || seq_roll_view(pg) ||
                                                                          pg->graph == GR_CHANCE;
        if (rl)                                      /* (V10.51) the playhead followed here (V10.47 did it in the drawing,
                                                      * which a playhead out of the bar shown never asked for: no redraw) */
            pr_follow_bar(t);
        if (rl && ph / 16u != ui.bank)
            ph = 0xFFFFu;                            /* the roll shows the cursor's bank only */
        h ^= steps_hash(t) + ph * 31u + ui.cursor * 7919u + ui.lane * 104723u + ui.bank * 613u;
        if (rl) {                                    /* the parameter locks' marks (the roll, the grid) */
            uint64_t lk = motion_lock_steps(trk_index(t));
            h = (h ^ (uint32_t)lk ^ (uint32_t)(lk >> 32) * 2654435761u) * 16777619u;
        }
        if (rl && !drum_track(t)) {                  /* the roll: its view and the keys held */
            pr_follow(t);
            h = (h ^ (proll.lo + 1u)) * 16777619u;
            h = (h ^ pr_held()) * 16777619u;
            h = (h ^ (ui.pr_focus | (uint32_t)pg->id[0] << 4 | (ui.entry_open ? (uint32_t)ui.ent_len << 8 : 0u) |
                      (uint32_t)ui.ent_nn << 16)) * 16777619u;   /* (0.6.81: NOTE's focus, the length being held) */
        }
    }
    return h;
}
/* 4-letter engine tags of the preset list */
static const char *eng_abbr(const char *name)
{
    static const char *const A[][2] = {{"ANALOG", "ANLG"},
#if FELUCCA_FM4
                                       {"DIGITAL", "DGTL"},
#endif
                                       {"PHASE", "PHAS"}, {"MATH", "MATH"},
                                       {"SAMPLE", "SMPL"}, {"VOICE", "VOCL"}, {"TRIO", "TRIO"}, {"WHEEL", "WHEL"},
                                       {"GRAIN", "GRAN"}, {"PHYS", "PHYS"}, {"DRUM", "DRUM"},
                                       {"NOISE", "NOIS"}, {"FM6", "FM6"}, {"SLICE", "SLCE"},
                                       {"ANIMAL", "ANML"}, {"STRING", "STRG"}, {"ADD", "ADD"}, {"EXT", "EXT"}};
    uint32_t i;
    for (i = 0; i < sizeof A / sizeof A[0]; i++)
        if (str_eq(name, A[i][0]))
            return A[i][1];
    return name;
}

/* a list row (17 px): the selected one a THEME bar with INK text; tag at x 14, free text from x 54 to x1 */
#define LIST_Y(k) (3 + 17 * (int32_t)(k))
static void list_row(int32_t y, int sel, const char *tag, uint16_t tc, const char *name, uint16_t nc, int32_t x1)
{
    uint16_t bg = sel ? T_THEME : T_SURF;
    if (sel)
        cv_rrect(6, y, 228, 16, 4, T_THEME, T_SURF);
    GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_text_on(14, y + CAP_IN(S, 16), &AF_S, tag, sel ? T_INK : tc, bg);
    if (name[0])
        GFX_HOOK_ALIGN(0, y, 0, y + 16, AL_V, "list row text centred up/down");
    cv_free_text(54, y + CAP_IN(S, 16), &AF_S, name, sel ? T_INK : nc, bg, x1 - 54);
}

/* SEQ > PATTERNS: the pattern list around the one picked ("03  MELODY", "U07  MY BASS") */
static void graph_pats(void)
{
    uint32_t n = pat_count(), cur = pat_pick();
    int32_t row, first = clamp((int32_t)cur - 3, 0, (int32_t)n > 7 ? (int32_t)n - 7 : 0);
    for (row = 0; row < 7 && (uint32_t)(first + row) < n; row++) {
        uint32_t k = (uint32_t)(first + row);
        char tag[4], nm[13];
        pat_label(k, tag, nm);
        list_row(LIST_Y(row), k == cur, tag, T_MID, nm, T_TEXT, 232);
    }
}
/* MIXER page: four SURF columns, one under each card: the track number on its cushion (in the accent:
 * the selected track) with a REC / ARM / MUTE badge (P_MUTE: the GLO layer's black keys), the sound's short name (a MUTE badge
 * when armed and muted), the LEVEL knob (dB inside) with the output meter beside it, then the PAN and REV
 * knobs with their values. Knobs: a 270 degree ring, the track RAISE, the value arc THEME (the knob just
 * turned: ACCENT; muted: DIM); PAN from 12 o'clock. Each column is its own canvas with its own signature:
 * while the transport runs only the meters move. */
#define TS_MY 39                                     /* meter slot: column rows 39 .. 70 */
#define TS_MH 32
#define KB_X 4                                       /* LEVEL knob: box 4..37 x 37..70, centre (21, 54) */
#define KB_Y 37
#define KS_Y 86                                      /* PAN / REV knobs: boxes 86..105, centres x 15 and 42 */
static struct {
    uint32_t col[NTRK];                              /* (the cards: JIANT's six columns, else the four from trk_base4) */
    uint8_t meter[NTRK];
    uint32_t frame;                                  /* (0.9.0) ui.frame of the meters' last step */
} ts;

static int32_t meter_px(int32_t a)                   /* |sample| (Q15) -> px: 60 dB over the slot */
{
    int32_t lg = 0, v;
    if (a < 64)
        return 0;
    while ((a >> lg) > 1)
        lg++;
    v = lg * 8 + (((a << 3) >> lg) & 7);             /* 8 log2(a): 48 (-54 dB) .. 128 (+6 dB) */
    return clamp((v - 48) * (TS_MH - 2) / 80, 0, TS_MH - 2);
}

/* a knob's ring: 270 degrees (7:30 .. 4:30 o'clock), 2 px, anti-aliased, from the quadrant mask of its size
 * (tools/gen_aa_keycaps.py: coverage and angle; the other three quadrants mirror it, no trigonometry here).
 * Box top-left (x, y), outer radius r. Angles: 1/1024 turn from 12 o'clock, clockwise, -384 .. 384; the value
 * arc is lo .. hi (in vc), the rest of the ring the track (tr); bg lies under the ring */
#define KA_END 384
static void knob_arc(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t lo, int32_t hi,
                     uint16_t tr, uint16_t vc, uint16_t bg)
{
    const uint16_t *rt = ramp(tr, bg), *rv = ramp(vc, bg);
    int32_t i, j, m;
    for (j = 0; j < r; j++)
        for (i = 0; i < r; i++) {
            uint32_t k = (uint32_t)(j * r + i), a = (cov[k >> 1] >> ((k & 1u) ? 0 : 4)) & 15u;
            int32_t q = ang[k];
            if (!a)
                continue;
            for (m = 0; m < 4; m++) {                /* top right, bottom right, bottom left, top left */
                int32_t s = m == 0 ? q : m == 1 ? 512 - q : m == 2 ? q - 512 : -q;
                int32_t px = m < 2 ? x + r + i : x + r - 1 - i, py = (m == 0 || m == 3 ? y + r - 1 - j : y + r + j) + cv_oy;
                if (s < -KA_END || s > KA_END || (uint32_t)px >= cv_w || (uint32_t)py >= cv_h)
                    continue;
                cv_px[(uint32_t)py * cv_w + (uint32_t)px] = (s >= lo && s <= hi ? rv : rt)[a];
            }
        }
}
/* value v of lo..hi as an arc: from the start, or (bipolar, lo < 0) from 12 o'clock with a 1 px nub at 0 */
static void knob(int32_t x, int32_t y, int32_t r, const uint8_t *cov, const uint8_t *ang, int32_t v, int32_t lo,
                 int32_t hi, uint16_t vc)
{
    int32_t a0, a1;
    if (lo < 0) {
        int32_t s = v * KA_END / (v < 0 ? -lo : hi);
        a0 = (s < 0 ? s : 0) - 8;
        a1 = (s > 0 ? s : 0) + 8;
    } else {
        a0 = -KA_END;
        a1 = v > lo ? -KA_END + (v - lo) * 2 * KA_END / (hi - lo) : -KA_END - 1;
    }
    knob_arc(x, y, r, cov, ang, a0, a1, T_RAISE, vc, T_SURF);
}

static uint32_t trk_level(uint32_t c) { return (uint32_t)trk[c].p[P_LEVEL] & 127u; }   /* LEVEL 0..127 */

static void trk_short_name(uint32_t c, char *b)      /* the track's sound, b holds 13 */
{
    const track_t *t = &trk[c];
    const engine_t *e = trk_engp(t);
    if (user_of(t) < UP_SLOTS)
        up_name(user_of(t), b);
    else if (e->npresets)
        str_cpy(b, e->presets[t->preset % e->npresets].name, 13);
    else
        str_cpy(b, e->name, 13);
}

/* MENU > LARGE: the four tracks in the strip under the tall cards (57 x LG_H_GRAPH each): the cushion (the selected
 * one the accent), REC / ARM / MUTE as on the full mixer, the sound's name, LEVEL as a gauge (THEME: the cards'),
 * the output meter under it (MID); a muted track DIM */
#define TSS_GY 40                                    /* the LEVEL gauge, 3 px */
#define TSS_MY 48                                    /* the meter, 3 px */
static void track_strip(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t arm, uint32_t hot, uint32_t lvl,
                        const char *b)
{
    int32_t gw = CARD_W - 10, m = ts.meter[c] * gw / (TS_MH - 2), fx = (int32_t)lvl * gw / 127;
    uint16_t vc = mute ? T_DIM : T_THEME;
    cv_begin(CARD_W, LG_H_GRAPH, T_BG);
    cv_rrect(0, 0, CARD_W, LG_H_GRAPH, 5, T_SURF, T_BG);
    cv_trk(4, 4, 16, c, sel ? T_ACCENT : T_MID, T_SURF);
    if (st)
        GFX_HOOK_ALIGN(0, 4, 0, 20, AL_V, "mixer badge on the cushion's line");
    if (st == 1u || st == 2u)
        cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 5, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                  T_INK, T_SURF);
    else if (st)
        cv_keycap(53 - kc_w(KC_MUTE), 5, KC_MUTE, T_KEY, T_INK, T_SURF);
    if (mute && arm)                                 /* armed and muted: MUTE in place of the name */
        GFX_HOOK_ALIGN(0, 22 + AF_S_CAP_Y, 0, 22 + AF_S_CAP_Y + AF_S_CAP_H, AL_V, "mixer MUTE badge on the name's line");
    if (mute && arm)
        cv_keycap(5, 22 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, T_KEY, T_INK, T_SURF);
    else
        cv_free_text(5, 22, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
    cv_rrect(5, TSS_GY, gw, 3, 1, ux.style ? T_LINE : T_BG, T_SURF);
    cv_rrect(5, TSS_GY, fx < 3 ? 3 : fx, 3, 1, hot == 1u ? T_ACCENT : vc, T_BG);
    cv_rrect(5, TSS_MY, gw, 3, 1, T_RAISE, T_SURF);
    if (m)
        cv_rrect(5, TSS_MY, m < 3 ? 3 : m, 3, 1, T_MID, T_RAISE);
    cv_blit((uint32_t)CARD_X(c - trk_base4()), graph_y());
}
/* (JIANT 0.6) the JIANT style's mixer: the six tracks side by side, 38 px each. The track's cushion (armed: a dot,
 * red recording), the sound's name (muted: MUTE), LEVEL as the thermal column with the meter brighter over it, its dB
 * beside it, PAN (from the middle) and the REV send as the two bars under it */
#define T6_W 38
#define T6_X(c) (3 + 39 * (int32_t)(c))
static __attribute__((noinline)) void track_col6(uint32_t c, uint32_t sel, uint32_t st, uint32_t mute, uint32_t hot, uint32_t lvl, int32_t pan,
                       int32_t rv, const char *b)
{
    char v[8];
    int32_t k, lv = (int32_t)lvl * 14 / 127, mt = ts.meter[c] * 14 / TS_MH, pw = T6_W - 8;
    uint16_t lc = hot == 1u ? T_ACCENT : lvl ? (mute ? T_DIM : T_TEXT) : T_DIM;
    cv_begin(T6_W, H_GRAPH, T_BG);
    cv_rrect(0, 0, T6_W, H_GRAPH, 5, T_SURF, T_BG);
    cv_trk(4, 5, 16, c, sel ? T_ACCENT : mute ? T_DIM : T_MID, T_SURF);
    if (st == 1u || st == 2u)                        /* armed: a dot (recording: red) */
        cv_rrect(T6_W - 11, 10, 6, 6, 3, st == 1u ? T_REC : T_ACCENT, T_SURF);
    if (mute == 2u)                                  /* asleep (TRACKS 4) */
        cv_text_in(2, 22, T6_W - 4, &AF_S, "OFF", T_DIM, T_SURF);
    else {                                           /* (0.6.24) the sound's name cut to the column (its first word, as
                                                      * many letters as fit, no "..."); muted: in MUTE's colour, still
                                                      * there (it said MUTE instead: which sound, gone) */
        char sn[13];
        uint32_t n;
        for (n = 0; b[n] && b[n] != ' ' && n < 12u; n++)
            sn[n] = b[n];
        sn[n] = 0;
        while (n > 1u && text_w(&AF_S, sn) > T6_W - 7)
            sn[--n] = 0;
        cv_text_on(4, 22, &AF_S, sn, mute ? T_ACCENT : sel ? T_TEXT : T_MID, T_SURF);
    }
    if (cur_page()->id[0] == P_DIST) {               /* (0.8.5) GLO > MIX > SENDS: DIST CHORUS DLY REV as bars */
        static const char *const SN[4] = {"D", "C", "D", "R"};
        for (k = 0; k < 4; k++) {
            int32_t v = trk[c].p[P_DIST + (uint32_t)k] * 60 / 127;   /* (P_DIST P_CHOR P_DLY P_REV: in a row) */
            cv_rect(4 + k * 8, 38, 6, 62, T_RAISE);
            if (v)
                cv_rect(4 + k * 8, 100 - v, 6, v, mute ? T_DIM : sel && hot == (uint32_t)k + 1u ? T_ACCENT :
                        settings.palette == UI_JIANT_INDEX ? heat_col(60 + v * 3) : T_THEME);
            cv_text(4 + k * 8, 106 - AF_X_CAP_Y, &AF_X, SN[k], T_DIM);
        }
        cv_blit((uint32_t)T6_X(c), lz_on() ? graph_y() + 9u : Y_GRAPH);
        return;
    }
    for (k = 0; k < 14; k++) {                       /* LEVEL: cold at the bottom, hot at the top */
        int32_t sy = 95 - k * 4;
        uint16_t sc = heat_col(20 + k * 220 / 13);
        sc = mute ? (k < lv ? T_DIM : T_RAISE) : k < mt ? sc : k < lv ? ux_mix(T_SURF, sc, 42) : T_RAISE;
        cv_rect(4, sy, 12, 3, sc);
    }
    if (lvl) {
        int32_t d = LEVEL_DB_X10_AT(lvl);
        fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
    } else {
        str_cpy(v, "OFF", sizeof v);
    }
    cv_text_in(17, 44, T6_W - 18, &AF_S, v, lc, T_SURF);
    cv_text_in(17, 58, T6_W - 18, &AF_S, "dB", T_DIM, T_SURF);
    cv_rect(4, 104, pw, 3, T_RAISE);                 /* PAN, from the middle */
    cv_rect(4 + pw / 2, 102, 1, 7, hot == 2u ? T_ACCENT : T_MID);
    if (pan)
        cv_rect(pan > 0 ? 4 + pw / 2 + 1 : 4 + pw / 2 + pan * (pw / 2) / 64, 104, (pan > 0 ? pan : -pan) * (pw / 2) / 64 + 1,
                3, mute ? T_DIM : hot == 2u ? T_ACCENT : heat_col(70 + (pan > 0 ? pan : -pan) * 2));
    cv_rect(4, 113, pw, 3, T_RAISE);                 /* the REV send */
    if (rv)
        cv_rect(4, 113, rv * pw / 127 + 1, 3, mute ? T_DIM : hot == 3u ? T_ACCENT : heat_col(40 + rv * 3 / 2));
    cv_blit((uint32_t)T6_X(c), lz_on() ? graph_y() + 9u : Y_GRAPH);   /* (0.8.5: GLO's lens, the panel under the cards) */
}
static __attribute__((noinline)) void draw_tracks(void)
{
    uint32_t c, sk = strip_kind() == SK_TRK, six = !sk;   /* (0.6.80: the six columns with every COLOR and STYLE;
                                                         * MENU > LARGE: four, bigger) */
    uint32_t b4 = six ? 0u : trk_base4(), n = six ? NTRK : 4u;
    if (ui.force) {
        lcd_fill(0, graph_y(), 240, graph_h(), T_BG);
        if (ux.style && !six)                        /* LINE: the strips divided as the cards above (four) */
            draw_rules(graph_y(), graph_h());
        if (ui.frame - ts.frame > 1u)                /* (0.9.0) a redraw while they fall (a toast, a knob's page)
                                                      * keeps them falling: only meters left long ago start at 0
                                                      * (they jumped to 0 and the panel kept the old segments lit) */
            for (c = 0; c < NTRK; c++)
                ts.meter[c] = 0;
    }
    ts.frame = ui.frame;
    for (c = b4; c < b4 + n; c++) {                  /* (JIANT 0.6: the six, or four of them) */
        track_t *t = &trk[c];
        uint32_t sel = c == song.sel, lvl = trk_level(c), mute = t->p[P_MUTE] != 0;
        uint32_t arm = (song.rec >> c) & 1u, st = arm ? (song.playing ? 1u : 2u) : mute ? 3u : 0u, sig;
        if (c >= trk_live)                           /* (0.6.22) TRACKS 4: asleep, grey as muted, "OFF" (mute 2) */
            mute = 2u;
        uint32_t hot = sel && ui.hot_t ? ui.hot_col + 1u : 0u;   /* the knob just turned (hot_col + 1): 1 LEVEL, 2 PAN,
                                                                * 3 REV (4: the DJ FILTER, on its card) */
        int32_t pk = t->peak, m, pan = clamp(t->p[P_PAN], -64, 63), rv = clamp(t->p[P_REV], 0, 127);
        uint16_t vc = mute ? T_DIM : T_THEME;
        char b[16];
        t->peak = 0;
        trk_short_name(c, b);
        m = mute ? 0 : meter_px(pk);
        if (m < ts.meter[c] - 1)
            m = ts.meter[c] - 1;                     /* falls ~2 dB a frame */
        ts.meter[c] = (uint8_t)(m < 0 ? 0 : m);
        sig = str_hash(1u + sel + st * 2u + (mute && arm) * 16u + hot * 32u, b) + lvl * 7919u +
              ts.meter[c] * 131u + (uint32_t)(pan + 128) * 104729u + (uint32_t)rv * 1299709u + mute * 3u + sk * 0x9E37u + c * 0x51u +
              six * 0x3C6Fu + (cur_page()->id[0] == P_DIST ? 0x7F4Au + (uint32_t)t->p[P_DIST] * 3u + (uint32_t)t->p[P_CHOR] * 389u +
                                (uint32_t)t->p[P_DLY] * 6151u + (uint32_t)t->p[P_REV] * 98317u : 0u);   /* (SENDS) */
        if (!ui.force && sig == ts.col[c - b4])
            continue;
        ts.col[c - b4] = sig;
        if (sk) {
            track_strip(c, sel, st, mute, arm, hot, lvl, b);
            continue;
        }
        if (six) {
            track_col6(c, sel, st, mute, hot, lvl, pan, rv, b);
            continue;
        }
        cv_begin(CARD_W, H_GRAPH, T_BG);
        cv_rrect(0, 0, CARD_W, H_GRAPH, 5, T_SURF, T_BG);
        cv_trk(4, 5, 16, c, sel ? T_ACCENT : T_MID, T_SURF);
        if (st)
            GFX_HOOK_ALIGN(0, 5, 0, 21, AL_V, "mixer badge on the cushion's line");
        if (st == 1u || st == 2u)                    /* REC (recording) / ARM (armed, stopped) */
            cv_keycap(53 - kc_w(st == 1u ? KC_REC : KC_ARM), 6, st == 1u ? KC_REC : KC_ARM, st == 1u ? T_REC : T_ACCENT,
                      T_INK, T_SURF);
        else if (st)                                 /* muted */
            cv_keycap(53 - kc_w(KC_MUTE), 6, KC_MUTE, T_KEY, T_INK, T_SURF);
        if (mute && arm)                             /* armed and muted: MUTE in place of the name */
            GFX_HOOK_ALIGN(0, 21 + AF_S_CAP_Y, 0, 21 + AF_S_CAP_Y + AF_S_CAP_H, AL_V,
                           "mixer MUTE badge on the name's line");
        if (mute && arm)                             /* (on the name's capitals) */
            cv_keycap(5, 21 + AF_S_CAP_Y + HALF_UP(AF_S_CAP_H - KC_H), KC_MUTE, T_KEY, T_INK, T_SURF);
        else
            cv_free_text(5, 21, &AF_S, b, mute ? T_DIM : sel ? T_TEXT : T_MID, T_SURF, CARD_W - 10);
        {   /* (the JIANT style: track_col6) */
        {   /* LEVEL: the knob, its dB inside (OFF at 0), and the meter of the output */
            char v[8];
            knob(KB_X, KB_Y, KNOB_BIG_R, KNOB_BIG_COV, KNOB_BIG_ANG, (int32_t)lvl, 0, 127, hot == 1u ? T_ACCENT : vc);
            if (lvl) {
                int32_t d = LEVEL_DB_X10_AT(lvl);
                fmt_int(v, (d + (d < 0 ? -5 : 5)) / 10);
                GFX_HOOK_ALIGN(KB_X, 0, KB_X + 2 * KNOB_BIG_R, 0, AL_H, "mixer level text centred across");
                cv_text_in(KB_X, KB_Y + KNOB_BIG_R + 5, 2 * KNOB_BIG_R, &AF_S, "dB", T_DIM, T_SURF);
            } else {
                str_cpy(v, "OFF", sizeof v);
            }
            GFX_HOOK_ALIGN(KB_X, KB_Y, KB_X + 2 * KNOB_BIG_R, KB_Y + 2 * KNOB_BIG_R, AL_HV, "mixer level value in its knob");
            cv_text_in(KB_X, KB_Y + CAP_IN(S, 2 * KNOB_BIG_R), 2 * KNOB_BIG_R, &AF_S, v, hot == 1u ? T_ACCENT : lvl ? vc : T_DIM, T_SURF);
            cv_rrect(45, TS_MY, 4, TS_MH, 2, T_RAISE, T_SURF);
            if (ts.meter[c])
                cv_rrect(45, TS_MY + TS_MH - 1 - ts.meter[c], 4, ts.meter[c] + 1, ts.meter[c] >= 4 ? 2 : 0, T_MID, T_RAISE);
        }
        {   /* PAN (from the centre) and the REV send: captions, knobs, values */
            char v[8];
            uint16_t pc = hot == 2u ? T_ACCENT : vc, rc = hot == 3u ? T_ACCENT : vc;
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(15 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "PAN", T_DIM, T_SURF);   /* (by their ink) */
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer caption over its knob");
            cv_text_in(42 - KNOB_SMALL_R, 72, 2 * KNOB_SMALL_R, &AF_S, "REV", T_DIM, T_SURF);
            knob(15 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, pan, -64, 63, pc);
            knob(42 - KNOB_SMALL_R, KS_Y, KNOB_SMALL_R, KNOB_SMALL_COV, KNOB_SMALL_ANG, rv, 0, 127, rc);
            v[0] = '+';                              /* the cards' numbers, without the % */
            fmt_int(pan > 0 ? v + 1 : v, pan * 100 / 64);
            GFX_HOOK_ALIGN(15 - KNOB_SMALL_R, 0, 15 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(15 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, pc, T_SURF);
            fmt_int(v, (rv * 100 + 63) / 127);
            GFX_HOOK_ALIGN(42 - KNOB_SMALL_R, 0, 42 + KNOB_SMALL_R, 0, AL_H, "mixer value under its knob");
            cv_text_in(42 - KNOB_SMALL_R, KS_Y + 17, 2 * KNOB_SMALL_R, &AF_S, v, rc, T_SURF);
        }
        }
        cv_blit((uint32_t)CARD_X(c - b4), Y_GRAPH);
    }
}
/* oscilloscope of the output, triggered on a rising zero crossing: a RAISE centre line, the trace 2 px (in silence
 * a flat line on it; MENU > SCREEN OFF keeps it from staying on the panel for hours) */
static void scope_at(int32_t x0, int32_t pw, int32_t cy, int32_t a, uint16_t c);
static void graph_scope(uint16_t c)
{
    scope_at(PANEL_X0, PANEL_W, (int32_t)cv_h / 2, cv_h == H_GRAPH ? 46 : (int32_t)cv_h / 2 - 6, c);   /* (LARGE: the strip) */
}
/* .. pw px wide from x0, about the line cy, +-a px (0.8.5: EDIT's SOUND, in its well) */
static void scope_at(int32_t x0, int32_t pw, int32_t cy, int32_t a, uint16_t c)
{
    int16_t *snap = mscr.snap;                       /* (seq2h) core.h mscr: this draw's */
    uint32_t w = scope_w, i, trig = 0;
    int32_t py = cy, x, peak = 1500;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak)
            peak = snap[i];
        else if (-snap[i] > peak)
            peak = -snap[i];
    }
    for (i = 1; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) {
            trig = i;
            break;
        }
    cv_rect(x0, cy, pw, 1, T_RAISE);
    for (x = 0; x < pw; x++) {
        int32_t y = cy - snap[trig + (uint32_t)x] * a / peak;    /* auto-scaled */
        if (x)                                                   /* JIANT: each stroke as hot as it is loud */
            cv_line_t(x0 - 1 + x, py, x0 + x, y, settings.palette == UI_JIANT_INDEX ?
                      heat_col(40 + (y > cy ? y - cy : cy - y) * 216 / a) : c, 2);
        py = y;
    }
}

/* SONG: the rows, each a section (the saved project A..D, sounds and steps) and its bars (song_chain.c) */
/* (JIANT) MSEQ: the 16 levels as bars (past LEN dim), the step KNOB 1 picked framed, the one playing in the accent, and
 * the level the matrix reads now (STEP, slewed) as a line */
static void graph_mseq(const track_t *t, uint16_t c)
{
    uint32_t i, len = t->p[P_MSLEN] > 0 ? (uint32_t)t->p[P_MSLEN] : 1u;
    uint32_t per = div_samples((uint32_t)t->p[P_MSDIV]), now = song.playing && per ? ms_clock / per % len : 0xFFu;
    int32_t y0 = 92, hmax = 80;
    for (i = 0; i < 16u; i++) {
        int32_t x = 13 + (int32_t)i * 13, h = t->p[P_MS0 + i] * hmax / 127;
        uint16_t col = i >= len ? T_RAISE : i == now ? T_ACCENT : settings.palette == UI_JIANT_INDEX ?
                       heat_col(48 + t->p[P_MS0 + i] * 200 / 127) : c;
        cv_rrect(x, y0 - hmax, 10, hmax, 2, T_RAISE, T_SURF);
        if (h > 0)
            cv_rrect(x, y0 - (h < 3 ? 3 : h), 10, h < 3 ? 3 : h, 2, col, T_RAISE);
        if (i == ms_ui_step)
            cv_rect(x, y0 + 3, 10, 2, T_TEXT);
    }
    if (song.playing)
        cv_rect(13, y0 - t->ms_v * hmax / 32767, 205, 1, T_TEXT);
}
/* (0.6.22) ARRANGE / SEQ > SONG, after SLOOP's arranger in JIANT's colours: the song as a column of rows, each a section
 * chip (A..D, each its own heat), its bars as blocks (one a bar, up to 16; past 16 each a 16th of it) filling as they
 * play, the bars' count, a dot for its scene; under them + ADD (the rows used of CHAIN_ROWS) and the song's END (STOP,
 * x2..x8, LOOP). On the right the whole song as a strip, each row's height its share of the bars, the play head on it,
 * the bars in all. KNOB 1 the row, 2 its section (on END: STOP / x2 .. / LOOP), 3 its bars, 4 its scene */
static uint16_t sec_col(uint32_t s)
{
    static const uint8_t HJ[4] = {40, 92, 140, 196}, HO[4] = {120, 160, 200, 240};   /* (V10.6i) A..D cold to hot: violet red
                                                         * orange amber (was indigo red amber cream) */   /* (0.8.5: JIANT the mock-up's cyan
                                                         * violet orange yellow; was 60 115 175 230) */
    return heat_col(settings.palette == UI_JIANT_INDEX ? HJ[s & 3u] : HO[s & 3u]);
}
#include "ui_tech.c"                                /* (JIANT 0.5) the technical graphs (FX, DEST, VOICE, GLOBAL) */
#include "ui_lzsave.c"                              /* (0.8.5) SAVE's lens: its levels' panels */
#include "ui_lzfx.c"                                /* (0.8.5) FX's lens: its levels' panels */
#include "ui_lzhome.c"                              /* (0.8.5) HOME's lens: ECO, BEING, the critters */
#include "ui_lzedit.c"                               /* (0.8.5) EDIT's lens panels */
#include "ui_lzpage.c"                               /* (0.8.5) ENV's, LFO . MOD's, SCL . ARP's, GLO's */
/* (0.9.9-seq2f) GLO > COMPS' roll: the lane's 16 steps as columns, each one's component mark on top (comp_col, as a
 * track's), its LENGTH a line after it, the six tracks it applies to as small marks in rows below, the cursor step
 * framed, the global step playing underlined; MONO / POLY at the right */
static __attribute__((noinline)) void graph_gcomp(void)
{
    uint32_t i, k, j, h = graph_h(), play = song.playing ? gl_step_at(0) % GL_N : GL_N;
    int32_t x0 = 12, cw = 13, y0 = 8;
    for (i = 0; i < GL_N; i++) {
        const gcomp_t *c = &glane[i];
        uint32_t a = CO_ACT(c->comp) % CA_N;
        int32_t x = x0 + (int32_t)i * cw;
        cv_rect(x + 1, (int32_t)h - 10, cw - 2, 1, (i & 3u) ? T_DIM : T_MID);   /* (the beats) */
        if (i == gl_cur % GL_N)
            cv_frame(x, y0 - 4, cw, (int32_t)h - y0 - 2, T_ACCENT);
        if (i == play)
            cv_rect(x + 1, (int32_t)h - 8, cw - 2, 3, T_THEME);
        if (!a)
            continue;
        cv_rect(x + cw / 2 - 2, y0, 4, 4, comp_col(a));
        for (j = 1; j <= c->len && j < GL_N; j++)   /* (its LENGTH, round the bar) */
            cv_rect(x0 + (int32_t)((i + j) % GL_N) * cw, y0 + 6, cw, 1, comp_col(a));
        for (k = 0; k < NTRK; k++)
            cv_rect(x + cw / 2 - 1, y0 + 12 + (int32_t)k * 9, 3, 3, (c->mask >> k) & 1u ? T_TEXT : T_DIM);
    }
    cv_text(x0, (int32_t)h - 24 - AF_X_CAP_Y, &AF_X, gl_mono ? "MONO" : "POLY", T_DIM);
}
static void draw_graph(void)
{
    const page_t *pg = cur_page();
    const track_t *t = TSEL;
    uint16_t c = ACC;
    uint32_t sig;
    if (!ui.home && pg->graph == GR_TRK) {
        draw_tracks();
        return;
    }
    sig = graph_signature();
    if (!ui.force && sig == ui.graph_sig)
        return;
    {   /* (0.6.57) LOAD LITE / ECO: the panel's animation (the beings, the scope, LFO, the envelope's dot, a turn
         * redrawing a curve) at most every 50 / 70 ms (~20 / 14 frames a second, FULL: as fast as it changes), so the
         * main loop keeps its time for the keys, the knobs and the LCD. A new page, track or engine: at once */
        static uint32_t gr_ms, gr_key;
        uint32_t key = ui.page * 7u + song.sel * 131u + ui.home * 3u + TSEL->eng_req * 1031u;
        if (sys_load && !ui.force && key == gr_key && fm1_ms - gr_ms < (sys_load == LOAD_ECO ? 70u : 50u))
            return;                                  /* (graph_sig stays: drawn on a later frame) */
        gr_key = key;
        gr_ms = fm1_ms;
    }
    ui.graph_sig = sig;
    cv_begin(240, graph_h(), T_BG);
    if (hud_home()) {                                /* (0.8.9) HUD's HOME: the middle of the screen's water, its numbers */
        cv_halo = 1;                                 /* (0.9.0) its words outlined too (M3) */
        eco_backdrop(0, (int32_t)graph_y(), 1);
        lz_blit(0, graph_y());
        return;
    }
    if (lz_low())                                    /* (0.8.9) BOTTOM, HUD: no panel, the picture on the background */
        ;
    else if (lz_on())                                /* (0.8.5) a lens page: the mock-up's panel, 4 .. 236 (0.8.9 COLORS: */
        cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);   /* rounded, as the 0.6's) */
    else
        cv_rrect(3, 0, 234, graph_h(), 5, T_SURF, T_BG);   /* the panel (MENU > LARGE: the strip) */
    cv_bg = T_SURF;                                  /* (text drawn with cv_text lands on it) */
    if (drift_on())
        graph_drift();
    cv_oy = GOY;                                     /* graphs on a 100 px scale */
    if (strip_kind() != SK_NONE) {            /* MENU > LARGE: the strip */
        uint32_t k = strip_kind();
        cv_oy = k == SK_ADSR || k == SK_LFO ? 3 : 0;
        graph_ht = LG_H_GRAPH - 6;
        if (k == SK_SCOPE) {                         /* (V10.8, testers: LARGE hid the animations) HOME: the tracks'
                                                      * creatures in a row, the scope only if none has one */
            uint32_t n = trk_live ? trk_live : 1u, i, w = 228u / n, any = 0;
            vx_small = 1;
            for (i = 0; i < n; i++)
                any |= (uint32_t)vx_draw(&trk[i], 6 + (int32_t)(i * w), 2, (int32_t)w - 4, (int32_t)graph_ht - 4, -1);
            vx_small = 0;
            if (!any) graph_scope(c);
        } else if (k == SK_ADSR) graph_adsr(t, c);
        else if (k == SK_LFO) graph_lfo(t, c);
        else {
            int ok;
            vx_small = 1;
            ok = pg->fam == FAM_EDIT && vx_draw(t, 8, 2, 224, (int32_t)graph_ht - 4, -1);   /* (V10.8) EDIT: the creature */
            vx_small = 0;
            if (!ok) graph_title();
        }
        graph_ht = 100;
    } else if (ui.confirm == CF_DICE_ALL) {          /* (0.8.5) GEN, HOME's lens's last level (gen.c) */
        cv_oy = 0;
        gen_panel();
    } else if (ui.home) {
        cv_oy = 0;
        graph_home_lz();                             /* (0.8.5: HOME's lens, ECO / BEING; JIANT 0.5: the system's
                                                      * beings, was the scope) */
    } else if (pg->graph == GR_LIVE) {               /* (p2) GLO > LIVE: the rails, MANO's board (ui_live.c) */
        cv_oy = 0;
        graph_live();
    } else if (pg->graph == GR_SCENE) {              /* (V10.25) SCL > SCENE: the six tracks' roles */
        cv_oy = 0;
        graph_scene();
    } else if (pg->graph == GR_EUCLID) {             /* (V10.6) SEQ > EUCLID / MODES: the ring, the lanes or the notes */
        cv_oy = 0;
        graph_euclid(t);
    } else if (seq_roll_view(pg)) {                  /* (0.9.9-seq2b) every SEQ page: the roll (a kit: the grid), its marks */
        cv_oy = 0;
        if (drum_track(t))
            graph_grid(t, c);
        else
            graph_roll(t, c);
        seq_overlay(t, pg);
    } else if (lz_on() && lz_fam() == LZ_SAVE) {     /* (0.8.5) SAVE's lens */
        cv_oy = 0;
        graph_save_lz(pg);
    } else if (lz_on() && lz_fam() == LZ_FX) {       /* (0.8.5) FX's lens */
        cv_oy = 0;
        graph_fx_lz(t, pg);
    } else if (lz_on() && lz_fam() == LZ_EDIT && (cv_oy = 0, graph_edit_lz(t, pg))) {   /* (0.8.5) EDIT's (ui_lzedit.c) */
    } else if (lz_on() && lz_fam() == LZ_ENV && (cv_oy = 0, graph_env_lz(t, pg))) {     /* .. ENV's (ui_lzpage.c) */
    } else if (lz_on() && lz_fam() == LZ_MOD && (cv_oy = 0, graph_mod_lz(t, pg))) {     /* .. LFO . MOD's */
    } else if (lz_on() && lz_fam() == LZ_KEY && (cv_oy = 0, graph_key_lz(t, pg))) {     /* .. SCL . ARP's */
    } else if (pg->graph == GR_GCOMP) {              /* (0.9.9-seq2f) GLO > COMPS: the global lane */
        cv_oy = 0;
        graph_gcomp();
    } else if (lz_on() && lz_fam() == LZ_GLO && pg->id[0] == G_TRKS) {               /* .. GLO's SYSTEM */
        cv_oy = 0;
        graph_system();
    } else {
        cv_oy = GOY;
        switch (pg->graph) {
        case GR_ADSR:
            graph_adsr(t, c);
            break;
        case GR_MORPH:
            graph_morph(t);
            break;
        case GR_DXIMG:                               /* (0.6.58) KIT 3: the colony; STEREO: IMAGE */
            if (DX_KIT3(pg)) {
                cv_oy = 0;
                graph_colony(t, -1);
            } else
                graph_dximg(t);
            break;
        case GR_ROLL:
        case GR_TRACKGEN:
        case GR_CHANCE:
        case GR_SHAPE:                               /* (0.6.85) STRETCH: the whole sequence, as the roll shows it */
            cv_oy = 0;
            if (drum_track(t))
                graph_grid(t, c);
            else
                graph_roll(t, c);
            break;
        case GR_PATS:
            cv_oy = 0;
            graph_pats();
            break;
        case GR_FMOP:                                /* (0.6.10) FM6's operators: (0.9.6) the colony (ui_engvis.c), the operator */
        case GR_FMOP2:                               /* picked lit */
            if (fop_ok()) vx_fm6_page(t, fop_op % 6u, pg->graph == GR_FMOP2);
            break;
        case GR_FMENV:                               /* .. its envelope, its bicho riding it */
            if (fop_ok()) vx_fm6env(t);
            break;
        case GR_DXSND:                               /* EDIT > SOUND: the kit's lanes, the sound edited singled out */
            cv_oy = 0;
            if (pg->title[0] == 'S')                 /* (0.9.4) SOUND 1..4: the drum kit, the sound edited lit (ui_engvis.c) */
                vx_kit_page(t, (int32_t)(dx_ui_lane & 7u));
            else
                graph_colony(t, dx_ui_lane);
            break;
        case GR_LYRIC:                               /* (0.8.5) EDIT > WORDS: what VOICE sings */
            graph_sing(t);
            break;
        default:
            if (pg->scope == SC_ENGINE && trk_engp(t) == &ENG_FORMANT && t->p[P_VLANG]) {
                graph_sing(t);                       /* (0.8.5) VOICE singing: its words on its pages */
                break;
            }
            if (pg->scope == SC_ENGINE && trk_eng(t) == ENGI_DRUM) {   /* (0.8.5: WHEEL's and FM6's EDIT 1
                                                      * and 2 their beings, as every synth's: EDIT's lens, SOUND) */
                cv_oy = 0;
                if (pg->id[0] == P_E0)               /* (0.9.4) KIT: the kit's beings (ui_engvis.c); KIT 2 the colony */
                    vx_kit_page(t, -1);
                else
                    graph_colony(t, -1);
            }
#if FELUCCA_FM4
            else if ((pg->scope == SC_ENGINE || pg->id[0] == P_FM1_LEVEL) && trk_eng(t) == ENGI_DIGITAL)
                graph_fm(t, c);                      /* (OP LEVEL too: the levels on the chart) */
#endif
            else if (pg->scope == SC_ENGINE) { cv_oy = 0; graph_being(t); }   /* (JIANT 0.5: the being, was the scope) */
            else {                                   /* the FX, DEST, VOICE, GLOBAL pages: their graphs (else the scope) */
                cv_oy = 0;
                if (!graph_tech(t, pg))
                    graph_scope(c);
            }
            break;
        }
    }
    cv_oy = 0;
    if (toast_on())                                  /* (0.9.5) a toast: a small pill near the panel's foot, over the page,
                                                      * which goes on drawing and moving behind it (0.8.5 .. 0.9.4: the
                                                      * panel cleared, the message boxed alone in it) */
        toast_pill();
    else
        toast_at[0] = toast_at[2] = 0;
    cv_blit(0, graph_y());
}

