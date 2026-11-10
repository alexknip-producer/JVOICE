/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.8.5) EDIT's lens panels (ui_lens.c), as the approved mock-up's; included by ui_graph.c. The canvas is the
 * lens's panel (240 x 140, x 4..236).
 *   SOUND   (EDIT 1 / EDIT 2 of a synth) the engine's being on the left (graph_being's, alive with the sound), the
 *           engine's page name over it and the sound's name under it; on the right the scope in a well and the eight
 *           E parameters as chips, the page's four lit. DRUM's KIT keeps its colony (graph_colony)
 *   FILTER  the response of TYPE at CUT with RES (LP HP BP COMB), CUT a line, DRIVE a row of cells
 *   VOICE   the track's voices one a row, each as full as its envelope now; VOICE and PRIO (VOICE 2: ALLOC); GLIDE's
 *           curve
 *   FOCUS   the parameter as a gauge, its value large, the MOD slot that moves it ("M1 -> +63%"); E1..E8 as tabs
 * The others (MORPH, RETRIG, ROTOR, the kit's, the operators') keep their panels. */
#define ED_CH 17                                        /* a chip's height (two lines of AF_X) */
/* a chip: the stripe, the label, the value (on: the page's) */
static void ed_chip(int32_t x, int32_t y, int32_t w, const param_desc_t *d, int16_t v, int on)
{
    char b[24];
    const char *u = "";
    uint16_t bg = on ? T_TINT : T_RAISE;
    cv_rect(x, y, w, ED_CH - 1, bg);
    cv_rect(x, y, 2, ED_CH - 1, on ? T_THEME : T_LINE);
    if (!d || !d->label || d->label[0] == '-')
        return;
    param_format(d, v, b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_clip_text(x + 5, y - AF_X_CAP_Y + 1, &AF_X, d->label, on ? T_MID : T_DIM, bg, w - 6);
    cv_clip_text(x + 5, y - AF_X_CAP_Y + 9, &AF_X, b, on ? T_TEXT : T_DIM, bg, w - 6);
}
static void ed_cap(int32_t x, int32_t y, const char *s, uint16_t c)   /* a caption, its capitals from y */
{
    cv_text(x, y - AF_X_CAP_Y, &AF_X, s, c);
}
static __attribute__((noinline)) void graph_sound_lz(const track_t *t)
{
    const engine_t *e = trk_engp(t);
    uint32_t two = cur_page()->id[0] != P_E0;
    int32_t x = 122, w = 106, ty = 2, bare = ux.skin != 0u;
    uint16_t tb = lz_tile_bg();
    char b[40];
    /* (V10.7.15, the owner) no parameter list beside the being: the knobs' cards above say the same. SCOPE OFF (the
     * default): the creature (or the generic being) on the whole panel; SCOPE ON: the being left, the scope tall right */
    if (!ui_scope) {
        if (!vx_draw(t, 4, 2, 232, (int32_t)cv_h - 4, -1))
            graph_being_at(t, 120 * 16, ((int32_t)cv_h / 2) * 16, 34 * 16);
    } else {
        if (!vx_draw(t, 8, 14, 108, 112, -1))          /* (0.9.4 / 0.9.6: the engines' own creatures: ui_engvis.c) */
            graph_being_at(t, 60 * 16, 68 * 16, 22 * 16);
        cv_rect(0, 0, 4, (int32_t)cv_h, T_BG);
        cv_rect(116, 0, 60, (int32_t)cv_h, T_SURF);
        lz_tile(x, ty, w, 124, 0, T_THEME);
        GFX_HOOK_ALIGN(0, ty + 4, 0, ty + 11, AL_V | AL_PASS, "EDIT scope title");
        cv_text_on(x + (bare ? 0 : 4), ty + 4 - AF_X_CAP_Y, &AF_X, "SCOPE", bare ? T_MID : T_TEXT, tb);
        scope_at(x + 4, w - 8, ty + 68, 48, T_THEME);
    }
    cv_halo = (uint8_t)bare;                            /* (0.9.0) BOTTOM, HUD: outlined, as HUD's */
    ed_cap(10, 6, e->page_title[two] && e->page_title[two][0] != '-' ? e->page_title[two] : e->name, T_THEME);
    sound_name(t, b);
    cv_clip_text(10, (int32_t)cv_h - 12 - AF_X_CAP_Y, &AF_X, b[0] ? b : e->name, bare ? T_TEXT : T_MID, T_SURF, 104);
    cv_halo = 0;
}
/* FILTER: y of the response at x (px, 10..226), the cut at xc, res 0..1000; the unity line at y 62 */
static int32_t ed_resp(uint32_t ty, int32_t x, int32_t xc, int32_t res)
{
    int32_t d = x - xc, a = d < 0 ? -d : d, pk = res * 46 / 1000, y;
    int32_t bump = pk * 64 / (64 + d * d / 2);
    if (ty == 3u || ty == 4u)                           /* COMB: teeth CUT apart, as deep as RES (COMB-: between) */
        return 62 - (int32_t)(((ty == 4u ? -1 : 1) * og_cos((uint32_t)x * (uint32_t)(4096 + xc * 160)) * (8 + pk / 2)) >> 15);
    if (ty == 5u)                                       /* (snd1) MS20: a low-pass, its peak higher */
        bump += bump / 2;
    if (ty == 6u)                                       /* (V10.22) LP24: twice as steep */
        return clamp(62 + (d > 0 ? 4 * d : 0) - bump, 8, 104);
    y = ty == 0u || ty == 5u ? (d > 0 ? 2 * d : 0) : ty == 1u ? (d < 0 ? 2 * a : 0) : 2 * a / 3;   /* LP HP BP */
    return clamp(62 + y - bump - (ty == 2u ? pk / 3 : 0), 8, 104);
}
static void graph_filter_lz(const track_t *t, const page_t *pg)
{
    static const char *const FX[3] = {"100", "1k", "10k"};
    int16_t *vp;
    const param_desc_t *d;
    int32_t r[4], x, py = 0, xc, k, nd;
    uint32_t ty = ft_of(t->p);
    char b[32];
    for (k = 0; k < 4; k++) {
        d = page_desc(pg, (uint32_t)k, &vp);
        r[k] = d ? clamp(RATIO(d, enum_rank(d, *vp)), 0, 1000) : 0;
    }
    xc = 10 + r[1] * 216 / 1000;
    for (k = 0; k < 3; k++)
        cv_rect(64 + k * 54, 6, 1, 99, T_RAISE);
    for (x = 10; x <= 226; x++) {
        int32_t y = ed_resp(ty, x, xc, r[2]);
        cv_rect(x, y, 1, 105 - y, ux_mix(T_SURF, settings.palette == UI_JIANT_INDEX ? heat_col(40 + x * 160 / 226) : T_THEME, 30));
        if (x > 10)
            cv_line_t(x - 1, py, x, y, T_THEME, 2);
        py = y;
    }
    for (k = 0; k < 3; k++)
        cv_text(67 + k * 54, 108 - AF_X_CAP_Y, &AF_X, FX[k], T_DIM);
    cv_rect(xc, 6, 1, 99, T_ACCENT);
    og_node(xc * 16, ed_resp(ty, xc, xc, r[2]) * 16, 4, T_ACCENT);
    str_cpy(b, N_FTYPE[ty], 32);
    str_cpy(b + str_len(b), trk_eng(t) == 0u ? " \xB7 PER VOICE" : " \xB7 ON THE PART", 32 - str_len(b));
    ed_cap(10, 6, b, T_TEXT);
    d = page_desc(pg, 3, &vp);
    if (d) {
        const char *u = "";
        str_cpy(b, d->label, 32);
        str_cpy(b + str_len(b), " ", 2);
        param_format(d, *vp, b + str_len(b), &u);
        str_cpy(b + str_len(b), u, 32 - str_len(b));
        ed_cap(10, 120, b, T_MID);
    }
    nd = r[3] * 20 / 1000;
    for (k = 0; k < 20; k++)
        cv_rect(110 + k * 6, 116, 4, 12, k < nd ? T_THEME : T_RAISE);
}
static void graph_voice_lz(const track_t *t, const page_t *pg)
{
    uint32_t i, nv = trk_nvoice(t) ? trk_nvoice(t) : NVOICE;
    int32_t glide = t->p[P_GLIDE], py = 0, x;
    const char *u = "";
    char b[40];
    fmt_int(b, (int32_t)nv);
    str_cpy(b + str_len(b), nv == 1u ? " VOICE" : " VOICES", 40 - str_len(b));
    ed_cap(10, 6, b, T_DIM);
    for (i = 0; i < NVOICE; i++) {
        int32_t y = 17 + (int32_t)i * 9, lv = 0;
        char n[2] = {(char)('1' + i), 0};
        if (t->v[i].stage && t->v[i].stage < 4u)
            lv = (int32_t)(((int64_t)t->v[i].env * 196) >> 24);
        ed_cap(14, y, n, i < nv ? T_MID : T_DIM);
        cv_rect(26, y, 202, 7, i < nv ? T_RAISE : T_LINE);
        if (lv > 0)
            cv_rect(26, y, lv, 7, settings.palette == UI_JIANT_INDEX ? heat_col(60 + lv) : T_ACCENT);
    }
    param_format(track_desc(t, P_VOICE), t->p[P_VOICE], b, &u);
    str_cpy(b + str_len(b), pg->id[0] == P_ALLOC ? " \xB7 ALLOC " : " \xB7 PRIO ", 40 - str_len(b));
    param_format(track_desc(t, pg->id[0] == P_ALLOC ? P_ALLOC : P_PRIO), t->p[pg->id[0] == P_ALLOC ? P_ALLOC : P_PRIO],
                 b + str_len(b), &u);
    ed_cap(10, 95, b, T_TEXT);
    str_cpy(b, "GLIDE ", 40);
    param_format(track_desc(t, P_GLIDE), t->p[P_GLIDE], b + 6, &u);
    str_cpy(b + str_len(b), u, 40 - str_len(b));
    ed_cap(10, 120, b, T_MID);
    for (x = 0; x <= 104; x++) {                        /* the glide: low note to high, as slow as GLIDE */
        int32_t tm = glide ? x * 127 * 6 / (glide * 4 + 1) : 1000, k = tm > 255 ? 255 : tm;
        int32_t y = x < 16 ? 130 : 130 - 22 * (k * (255 - (255 - k) * (255 - k) / 255)) / 65025;
        if (x)
            cv_line_t(122 + x - 1, py, 122 + x, y, T_THEME, 2);
        py = y;
    }
}
static void graph_focus_lz(const track_t *t)
{
    const engine_t *e = trk_engp(t);
    uint32_t k = focus_k(), i;
    int32_t j = focus_slot(k);
    const param_desc_t *d = track_desc(t, P_E0 + k);
    const char *u = "", *pt = e->page_title[k >> 2];
    char b[24];
    param_format(d, t->p[P_E0 + k], b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    lz_arc(120, 58, 44, 4, clamp(RATIO(d, enum_rank(d, t->p[P_E0 + k])), 0, 1000), T_RAISE, T_THEME);
    GFX_HOOK_ALIGN(80, 0, 160, 0, AL_H, "FOCUS value centred in its gauge");
    cv_text_in(80, 40 - AF_X_CAP_Y, 80, &AF_X, d->label, T_MID, T_SURF);
    cv_text_in(70, 52 - AF_M_CAP_Y, 100, &AF_M, b, T_TEXT, T_SURF);
    if (j >= 0) {
        const param_desc_t *da = track_desc(t, P_M1AMT + 3u * (uint32_t)j);
        char a[16];
        str_cpy(b, N_MSRC[clamp(t->p[P_M1SRC + 3u * (uint32_t)j], 0, MS_N - 1)], 24);
        str_cpy(b + str_len(b), " > ", 4);
        param_format(da, t->p[P_M1AMT + 3u * (uint32_t)j], a, &u);
        str_cpy(b + str_len(b), a, 24 - str_len(b));
        str_cpy(b + str_len(b), u, 24 - str_len(b));
    } else
        str_cpy(b, "NO MOD", 24);
    cv_text_in(70, 74 - AF_X_CAP_Y, 100, &AF_X, b, j >= 0 ? T_THEME : T_DIM, T_SURF);
    str_cpy(b, pt && pt[0] != '-' ? pt : e->name, 16);
    str_cpy(b + str_len(b), " ", 2);
    fmt_int(b + str_len(b), (int32_t)(k & 3u) + 1);
    str_cpy(b + str_len(b), "/4", 3);
    ed_cap(10, 6, b, T_DIM);
    for (i = 0; i < 8u; i++) {                          /* E1..E8 */
        const param_desc_t *di = track_desc(t, P_E0 + i);
        int32_t x = 8 + (int32_t)i * 28;
        uint16_t bg = i == k ? T_THEME : T_RAISE;
        cv_rect(x, 118, 26, 13, bg);
        if (di && di->label && di->label[0] != '-')
            cv_clip_text(x + 2, 121 - AF_X_CAP_Y, &AF_X, di->label, i == k ? T_INK : T_DIM, bg, 23);
    }
}
/* the lens's panel for EDIT's page pg (1), else 0 (its own) */
static int graph_edit_lz(const track_t *t, const page_t *pg)
{
    uint32_t e = trk_eng(t);
    if ((pg->scope == SC_ENGINE && e != ENGI_DRUM && !(FELUCCA_FM4 && e == ENGI_DIGITAL)) || pg->graph == GR_SWARM)
        graph_sound_lz(t);
    else if (pg->id[0] == P_FTYPE)
        graph_filter_lz(t, pg);
    else if (pg->id[0] == P_VOICE || pg->id[0] == P_ALLOC)
        graph_voice_lz(t, pg);
    else if (pg->graph == GR_FOCUS)
        graph_focus_lz(t);
    else
        return 0;
    return 1;
}
