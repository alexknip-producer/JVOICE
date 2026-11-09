/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.9.2) The navigation, as the approved mock-up (navegacion.html): six rules, the same in every section.
 *   1 SELECT walks the section's pages as one chain (no levels, no zoom), stopping at its first and last page.
 *   2 The section's button: from another section, the page last shown there; already inside, the first page of the
 *     next group (round).
 *   3 PRESETS loads presets: the sounds everywhere, the patterns in SEQ (the PHRASES loader), the genre in GEN; never a
 *     page, a track or a module (ui_input.c presets_turn).
 *   4 A page that picks something picks it with KNOB 1 (the step, the bar, the slot, the kit's sound ..); the track is
 *     ALGORITHM's.
 *   5 A button held is its layer, as before (ui_layer.c); held + SELECT: HOME GEN, EDIT SEQ LFO FX their dice, GLO the
 *     tempo, ENV SCL ARP SAVE their chain (nav_jump).
 *   6 The screen says where: on top the section's groups as tabs (the one shown lit, a dot a page), at the foot
 *     SECTION . GROUP . page and n/N of the chain (ui_draw.c draw_tabs, draw_crumbs).
 * Each section's chain is a table (CH_*): the page (its PAGES title; HOME's are its own), the name shown, the group
 * and the level its pictures were drawn for (0.8.5's lenses: the panels, the cards and the knobs as they were, read
 * through lens_level / save_level / pd_level / home_level). Each section remembers the page last shown (ui.nmem, EDIT
 * one per kind of track); a page put by code brings its entry (nav_cur). Included by ui.c. */
enum { LZ_SEQ, LZ_SAVE, LZ_FX, LZ_HOME, LZ_EDIT, LZ_ENV, LZ_MOD, LZ_KEY, LZ_GLO, LZ_MENU, LZ_LAYER, LZ_DLG, LZ_NONE };
enum { PD_EDIT, PD_ENV, PD_MOD, PD_KEY, PD_GLO, PD_N };
static uint32_t pd_of_fam(uint32_t f)
{
    return f == FAM_EDIT ? PD_EDIT : f == FAM_ENV ? PD_ENV : f == FAM_LFO ? PD_MOD : f == FAM_SCL || f == FAM_ARP ? PD_KEY :
           f == FAM_GLO || f == FAM_TRK ? PD_GLO : PD_N;
}
static uint32_t edit_kind(void)                          /* 0 a synth, 1 FM6, 2 DRUM, 3 DIGITAL (FELUCCA_FM4) */
{
    uint32_t e = trk_eng(TSEL);
    return e == ENGI_FM6 ? 1u : e == ENGI_DRUM ? 2u : FELUCCA_FM4 && e == ENGI_DIGITAL ? 3u : 0u;
}
static int pd_on(void) { return !ui.home && !page_over && pd_of_fam(PAGES[ui.page].fam) < PD_N; }
static uint32_t pd_cur(void) { return pd_of_fam(PAGES[ui.page].fam); }

/* ------------------------------------------------------------------------------------------- the chains --- */
enum { SEC_HOME, SEC_EDIT, SEC_SEQ, SEC_FX, SEC_GLO, SEC_ENV, SEC_LFO, SEC_SCL, SEC_ARP, SEC_SAVE, SEC_N };
static const char *const SEC_NAME[SEC_N] = {"HOME", "EDIT", "SEQ", "FX", "GLO", "ENV", "LFO", "SCL", "ARP", "SAVE"};
static const uint8_t SEC_FAM[SEC_N] = {FAM_HOME, FAM_EDIT, FAM_SEQ, FAM_FX, FAM_GLO, FAM_ENV, FAM_LFO, FAM_SCL, FAM_ARP,
                                       FAM_SAVE};
typedef struct { const char *t, *nm; uint8_t g, lv; } nv_t;   /* PAGES title (HOME: 0), the name shown (0: t), group, level */
typedef struct { const char *nm, *tab; } ng_t;                /* a group: its name, its tab's when the name is too wide */
typedef struct { const nv_t *e; const ng_t *g; uint8_t n, ng; } ch_t;
#define NV(t, nm, g, lv) {t, nm, g, lv}
static const nv_t CE_HOME[] = {NV(0, "ECO", 0, 0), NV(0, "MACROS", 0, 0), NV(0, "BEING", 1, 1), NV(0, "ROUTES", 1, 1),
                               NV(0, "GENE 1", 2, 2), NV(0, "GENE 2", 2, 2), NV(0, "GENE 3", 2, 2)};
static const ng_t CG_HOME[] = {{"ECO", 0}, {"BEING", 0}, {"GENE", 0}};
static const nv_t CE_SYN[] = {NV("EDIT 1", 0, 0, 0), NV("EDIT 2", 0, 0, 0), NV("FILTER", 0, 1, 1), NV("MORPH", 0, 1, 1),
                              NV("RETRIG", 0, 1, 1), NV("ROTOR", 0, 1, 1), NV("WORDS", 0, 1, 1), NV("STEREO", 0, 1, 1),
                              NV("SWARM", 0, 1, 1), NV("SWARM 2", 0, 1, 1), NV("FOCUS", 0, 2, 2), NV("VOICE", 0, 3, 1), NV("VOICE 2", 0, 3, 1)};   /* (V10.49: STEREO; the
                                                         * voices' pages last, after the sound's (the user)) */
static const ng_t CG_SYN[] = {{"SOUND", 0}, {"TONE", 0}, {"FOCUS", 0}, {"VOICE", 0}};
static const nv_t CE_FM6[] = {NV("EDIT 1", 0, 0, 0), NV("EDIT 2", 0, 0, 0), NV("FILTER", 0, 1, 1), NV("STEREO", 0, 1, 1),
                              NV("OPERATOR", 0, 2, 2), NV("OP ENV", 0, 2, 2), NV("OPERATOR 2", 0, 2, 2), NV("VOICE", 0, 3, 1),
                              NV("VOICE 2", 0, 3, 1)};
static const ng_t CG_FM6[] = {{"SOUND", 0}, {"TONE", 0}, {"OPERATOR", "OPS"}, {"VOICE", 0}};
static const nv_t CE_DRM[] = {NV("EDIT 1", "KIT", 0, 0), NV("EDIT 2", "KIT 2", 0, 0), NV("KIT 3", 0, 0, 0), NV("KIT 4", 0, 0, 0),
                              NV("SOUND", 0, 1, 2), NV("SOUND 2", 0, 1, 2), NV("SOUND 3", 0, 1, 2), NV("SOUND 4", 0, 1, 2),
                              NV("LEVELS", 0, 2, 1), NV("LEVELS 2", 0, 2, 1), NV("X-MOD", 0, 3, 1), NV("FILTER", 0, 3, 1),
                              NV("STEREO", 0, 3, 1), NV("VOICE", 0, 3, 1)};
static const ng_t CG_DRM[] = {{"KIT", 0}, {"SOUND", 0}, {"LEVELS", "LEVEL"}, {"TONE", 0}};
static const nv_t CE_DIG[] = {NV("EDIT 1", 0, 0, 0), NV("EDIT 2", 0, 0, 0), NV("FILTER", 0, 1, 1), NV("VOICE", 0, 1, 1),
                              NV("VOICE 2", 0, 1, 1), NV("OP1 ENV", 0, 2, 2), NV("OP2 ENV", 0, 2, 2), NV("OP3 ENV", 0, 2, 2),
                              NV("OP4 ENV", 0, 2, 2), NV("OP LEVEL", 0, 2, 2)};
static const ng_t CG_DIG[] = {{"SOUND", 0}, {"TONE", 0}, {"OPERATOR", "OPS"}};
enum { LV_PAT, LV_BAR, LV_STEP, LV_N };                 /* SEQ's pictures: the bars, the roll, the step large */
/* (0.9.9-seq2, the approved mock-up) SEQ's chain in seven tabs: STEP . LENGTH (the PATTERN page, then TRACK) . MUTATE .
 * PLAYHEADS . STRETCH . COMP (COMP 2 after it, by SELECT as before) . AUTO (AUTOMATION, AUTO LIST). BAR and NOTE stay pages
 * (code puts them) but out of the chain, until the step held opens its own (the mock-up's slice 5) */
static const nv_t CE_SEQ[] = {NV("STEP", 0, 0, LV_BAR), NV("PATTERN", "LENGTH", 1, LV_PAT), NV("RHYTHM", 0, 1, LV_PAT), NV("TRACK", 0, 1, LV_PAT),
                              NV("EUCLID", 0, 2, LV_PAT), NV("MODES", 0, 2, LV_PAT), NV("MUTATE", 0, 2, LV_PAT),
                              NV("PLAYHEADS", 0, 3, LV_PAT), NV("STRETCH", 0, 4, LV_PAT),
                              NV("COMP", 0, 5, LV_STEP), NV("COMP 2", 0, 5, LV_STEP), NV("AUTOMATION", 0, 6, LV_BAR),
                              NV("AUTO LIST", 0, 6, LV_BAR), NV("LYRIC STEP",0,6,LV_BAR),NV("GENERATE",0,7,LV_BAR)};
static const ng_t CG_SEQ[] = {{"STEP", 0}, {"LENGTH", "LEN"}, {"EUCLID", "EUCL"}, {"PLAYHEADS", "HEAD"}, {"STRETCH", "STR"},
                              {"COMP", 0}, {"AUTO", 0}, {"GENERATE","GEN"}};
enum { FXL_TRACK, FXL_BUSES, FXL_EFFECT };
static const nv_t CE_FX[] = {NV("FX", "SENDS", 0, FXL_TRACK), NV("DIST", 0, 0, FXL_TRACK), NV("AUTO PAN", 0, 0, FXL_TRACK),
                             NV("BUSES", 0, 1, FXL_BUSES), NV("DLY", 0, 2, FXL_EFFECT), NV("DLY 2", 0, 2, FXL_EFFECT),
                             NV("DLY 3", 0, 2, FXL_EFFECT), NV("REVERB", 0, 3, FXL_EFFECT), NV("REVERB 2", 0, 3, FXL_EFFECT),
                             NV("CHORUS", 0, 4, FXL_EFFECT), NV("SLICER", 0, 5, FXL_EFFECT), NV("SL BUS", 0, 5, FXL_EFFECT),
                             NV("SL BUS 2", 0, 5, FXL_EFFECT), NV("SL PITCH", 0, 5, FXL_EFFECT), NV("PUNCH", 0, 6, FXL_EFFECT),
                             NV("MASTER", 0, 7, FXL_EFFECT), NV("TAPE", 0, 7, FXL_EFFECT),
                             NV("EQ", 0, 7, FXL_EFFECT)};   /* (p9: the master's EQ) */   /* (0.9.9-snd2: MASTER last, the
                                                         * end of the chain; snd8: its TAPE) */
static const ng_t CG_FX[] = {{"TRACK", "TRK"}, {"BUSES", "BUS"}, {"DELAY", "DLY"}, {"REVERB", "REV"}, {"CHORUS", "CHO"},
                             {"SLICER", "SLC"}, {"PUNCH", "PCH"}, {"MASTER", "MST"}};
static const nv_t CE_GLO[] = {NV("MANO", 0, 0, 0), NV("RIELES", 0, 0, 0), NV("MIXER", 0, 1, 0), NV("SENDS", 0, 1, 0),
                              NV("COMPS", 0, 2, 3), NV("COMPS 2", 0, 2, 3), NV("GLOBAL", 0, 3, 1),
                              NV("SYSTEM", 0, 4, 2), NV("SYSTEM 2", 0, 4, 2), NV("KEY SPLIT", 0, 4, 2), NV("KEY SPLIT 2", 0, 4, 2)};   /* (V10.37) the performing pages first, the
                                                         * settings (GLOBAL, SYSTEM) last */   /* (0.9.9-seq2f) the global lane; (p2) LIVE first
                                                         * (ui_live.c) */
static const ng_t CG_GLO[] = {{"LIVE", 0}, {"MIX", 0}, {"COMPS", "CMP"}, {"GLOBAL", 0}, {"SYSTEM", 0}};
static const nv_t CE_ENV[] = {NV("ENV", "TRACKS", 0, 0), NV("ENV DEST", "TRACKS DEST", 0, 0), NV("ENV", 0, 1, 1),
                              NV("ENV DEST", 0, 1, 1), NV("ENV 2", 0, 1, 1), NV("STAGE", 0, 2, 2)};   /* (V10.22: ENV 2) */
static const ng_t CG_ENV[] = {{"TRACKS", 0}, {"ENV", 0}, {"STAGE", 0}};
static const nv_t CE_LFO[] = {NV("MOD", 0, 0, 0), NV("LFO", 0, 1, 1), NV("LFO 2", 0, 1, 1), NV("LFO DEST", 0, 1, 1),
                              NV("MSEQ", 0, 2, 2), NV("MSEQ 2", 0, 2, 2)};   /* (0.9.18: LFO 2, PHASE) */
static const ng_t CG_LFO[] = {{"MOD", 0}, {"LFO", 0}, {"MSEQ", 0}};
static const nv_t CE_SCL[] = {NV("SCL", 0, 0, 0), NV("CHORD", 0, 1, 1), NV("SCENE", 0, 2, 2)};   /* (V10.25 SCENE) */
static const ng_t CG_SCL[] = {{"KEY", 0}, {"CHORD", 0}, {"SCENE", 0}};
static const nv_t CE_ARP[] = {NV("ARP", 0, 0, 2), NV("ARP 2", 0, 0, 2)};
static const ng_t CG_ARP[] = {{"ARP", 0}};
enum { SV_LIB, SV_PROJ, SV_SOUND };
static const nv_t CE_SAVE[] = {NV("TIMELINE", 0, 0, SV_PROJ), NV("PARTS", 0, 0, SV_PROJ), NV("CLIPS", 0, 0, SV_PROJ),
                               NV("PROJECT", "SECTIONS", 0, SV_PROJ),   /* (V10.1) composing first; V10.6o PARTS; V10.6s CLIPS */
                               NV("PROJECT", "PROJECTS", 1, SV_LIB), NV("ARRANGE", 0, 1, SV_PROJ), NV("TOOLS", 0, 1, SV_LIB), NV("SAVE SONG AS", 0, 1, SV_PROJ),
                               NV("USER", 0, 2, SV_SOUND), NV("PRESETS", 0, 2, SV_SOUND)};
static const ng_t CG_SAVE[] = {{"SONG", 0}, {"PROJECT", 0}, {"SOUND", 0}};
#define CH(e, g) {e, g, (uint8_t)NELEM(e), (uint8_t)NELEM(g)}
#define NAV_KEYS (SEC_N + 3u)                           /* the sections' memories: EDIT's synth one, then FM6 DRUM DIGITAL */
static const ch_t CHAINS[NAV_KEYS] = {CH(CE_HOME, CG_HOME), CH(CE_SYN, CG_SYN), CH(CE_SEQ, CG_SEQ), CH(CE_FX, CG_FX),
                                      CH(CE_GLO, CG_GLO), CH(CE_ENV, CG_ENV), CH(CE_LFO, CG_LFO), CH(CE_SCL, CG_SCL),
                                      CH(CE_ARP, CG_ARP), CH(CE_SAVE, CG_SAVE), CH(CE_FM6, CG_FM6), CH(CE_DRM, CG_DRM),
                                      CH(CE_DIG, CG_DIG)};
/* the page each opens on at power-on (the mock-up's): SEQ its STEP, ENV its ENV, LFO its LFO, (p2) GLO its MIXER (LIVE's
 * pages before it: a GLO tap inside, rule 2); ui.nmem holds it ^ this */
static const uint8_t NAV_DEF[NAV_KEYS] = {0, 0, 0, 0, 2, 2, 1, 0, 0, 0, 0, 0, 0};
static uint32_t nav_key(uint32_t sec) { uint32_t k = sec == SEC_EDIT ? edit_kind() : 0u; return k ? SEC_N - 1u + k : sec; }
static const ch_t *nav_ch(uint32_t sec) { return &CHAINS[nav_key(sec)]; }
static uint32_t nav_mem(uint32_t key) { return (uint32_t)(ui.nmem[key] ^ NAV_DEF[key]); }
static void nav_mem_put(uint32_t key, uint32_t e) { ui.nmem[key] = (uint8_t)(e ^ NAV_DEF[key]); }
static uint32_t sec_of_fam(uint32_t f)
{
    uint32_t s;
    for (s = SEC_EDIT; s < SEC_N; s++)
        if (SEC_FAM[s] == f || (s == SEC_GLO && f == FAM_TRK))
            return s;
    return SEC_HOME;
}
static int gen_on(void) { return ui.confirm == CF_DICE_ALL; }   /* GEN: HOME's last group (the generator's screen) */
static int dlg_on(void) { return ui.confirm && ui.confirm != CF_DICE_ALL; }   /* a dialog over the page */
static uint32_t gen_page(void);                         /* (gen.c) */
static uint32_t nav_sec(void) { return ui.home || gen_on() ? SEC_HOME : sec_of_fam(PAGES[ui.page].fam); }
/* entry e of section sec: its PAGES index shown now (NPAGES: not on this track, LOAD ECO's, HOME's own) */
static uint32_t nv_page(uint32_t sec, uint32_t e)
{
    const ch_t *c = nav_ch(sec);
    uint32_t i, f = SEC_FAM[sec];
    if (e >= c->n || !c->e[e].t)
        return NPAGES;
    for (i = 0; i < NPAGES; i++)
        if ((PAGES[i].fam == f || (sec == SEC_GLO && PAGES[i].fam == FAM_TRK)) && str_eq(PAGES[i].title, c->e[e].t) &&
            page_visible(i))
            return i;
    return NPAGES;
}
static int nv_ok(uint32_t sec, uint32_t e) { return sec == SEC_HOME ? e < NELEM(CE_HOME) : nv_page(sec, e) < NPAGES; }
/* the entry shown: HOME's from its state (ui.hlv, ui.hset, GEN's page); the others the one remembered when it is the page
 * shown, else the page's own (put by code: in the group left, the default page's, then the chain's first), else
 * (PHRASES, CHANCE: out of the chain) the one left */
static uint32_t nav_cur(void)
{
    uint32_t sec = nav_sec(), key = nav_key(sec), e = nav_mem(key), k, pass, s;
    const ch_t *c = &CHAINS[key];
    if (sec == SEC_HOME)
        return gen_on() ? 4u + gen_page() : (ui.hlv ? 2u : 0u) + (ui.hset[ui.hlv ? 1 : 0] & 1u);
    s = ((((ui.page * 31u + key) * 131u + e) * 257u + TSEL->eng_req) * 8191u + (uint32_t)(uint16_t)TSEL->p[P_E1]) * 7u +
        sys_load + 1u;
    if (s == ui.nav_sig)
        return ui.nav_at;
    if (e >= c->n || nv_page(sec, e) != ui.page) {     /* (the group left, the default's, then any: ENV's ENV page in */
        uint32_t g0 = c->e[e < c->n ? e : 0].g, g1 = c->e[NAV_DEF[key]].g;   /* ENV, not TRACKS) */
        for (pass = 0; pass < 3u; pass++)
            for (k = 0; k < c->n; k++)
                if ((pass == 2u || c->e[k].g == (pass ? g1 : g0)) && nv_page(sec, k) == ui.page) {
                    nav_mem_put(key, k);
                    e = k;
                    pass = 3u;
                    break;
                }
        if (e >= c->n)
            e = 0;
    }
    ui.nav_sig = ((((ui.page * 31u + key) * 131u + e) * 257u + TSEL->eng_req) * 8191u + (uint32_t)(uint16_t)TSEL->p[P_E1]) * 7u +
                 sys_load + 1u;
    ui.nav_at = e;
    return e;
}
static uint32_t nav_lv(void) { return nav_ch(nav_sec())->e[nav_cur()].lv; }   /* the 0.8.5 lens level it is drawn as */
static uint32_t pd_level(void) { return nav_lv(); }
static uint32_t save_level(void) { return nav_lv(); }
static uint32_t home_level(void) { return gen_on() ? 2u : ui.hlv ? 1u : 0u; }
static uint32_t home_set(void) { return ui.hset[home_level() & 1u] & 1u; }
static int home_macros(void) { return ui.home && !ui.menu && !gen_on() && home_set(); }   /* MACROS / ROUTES: M1..M4 */
/* the entries shown now (on this track, at this LOAD), in order: their indices in v (NAV_MAX at most: the longest chain,
 * FX's 17 since snd8's TAPE, fits: ui_test checks every chain against it; snd8b's 17th page overran a 16 and crashed
 * FX); how many */
#define NAV_MAX 24u
static uint32_t nav_list(uint32_t sec, uint8_t *v)
{
    const ch_t *c = nav_ch(sec);
    uint32_t k, n = 0;
    for (k = 0; k < c->n; k++)
        if (nv_ok(sec, k))
            v[n++] = (uint8_t)k;
    return n;
}
static void go_home(void);
static void page_entered(void);
static void page_flash(void);
static void confirm_open(uint32_t kind, uint32_t trk);
static void gen_open(void);
static void gen_page_put(uint32_t p);
static int chain_busy(void);
static void ui_message(const char *s);
static void seg_missing(void);
static uint32_t fx_mod_of(uint32_t i);
/* entry e of section sec shown (one that is not here: the nearest after it, else before); outside: from another
 * section (SAVE: USER and SECTIONS come with their SAVE armed, SAVE . OCT+ . OCT+ as before) */
static void nav_go(uint32_t sec, uint32_t e, int outside)
{
    const ch_t *c = nav_ch(sec);
    uint32_t key = nav_key(sec), k, i;
    if (e >= c->n)
        e = 0;
    for (k = e; k < c->n && !nv_ok(sec, k); k++) {}
    if (k >= c->n)
        for (k = e; k-- > 0u && !nv_ok(sec, k);) {}
    if (k >= c->n)
        return;
    e = k;
    nav_mem_put(key, e);
    if (sec == SEC_HOME) {
        if (e >= 4u) {                                  /* GEN: the generator's screen, HOME's last group */
            if (!gen_on()) {
                confirm_open(CF_DICE_ALL, song.sel);
                gen_open();
                if (!bt_hw_seg())                       /* (V10.7.10: GEN's code is in the Bluetooth segment) */
                    seg_missing();
            }
            gen_page_put(e - 4u);
        } else {
            if (gen_on())
                ui.confirm = 0;
            ui.hlv = (uint8_t)(e >> 1);
            ui.hset[e >> 1] = (uint8_t)(e & 1u);
        }
        if (!ui.home)
            go_home();
        ui.force = 1;
        page_flash();
        return;
    }
    if (gen_on())
        ui.confirm = 0;
    i = nv_page(sec, e);
    ui.page = (uint8_t)i;
    ui.home = 0;
    page_entered();
    if (PAGES[i].graph == GR_SONGSAVE) ui.act = 4u;
    if (PAGES[i].graph == GR_USER || PAGES[i].graph == GR_SLOTS)
        ui.act = outside && (PAGES[i].graph == GR_USER || c->e[e].lv == SV_PROJ) ? 4u : 0u;
    page_flash();
}
/* rule 2: a section's button */
static void nav_button(uint32_t sec)
{
    const ch_t *c = nav_ch(sec);
    uint32_t e, g, j, k;
    if (nav_sec() != sec || (sec != SEC_HOME && ui.home)) {
        nav_go(sec, nav_mem(nav_key(sec)), 1);
        return;
    }
    e = nav_cur();
    g = c->e[e].g;
    for (j = 1; j <= c->ng; j++)                        /* (the next group with a page here; ARP's one: its first) */
        for (k = 0; k < c->n; k++)
            if (c->e[k].g == (g + j) % c->ng && nv_ok(sec, k)) {
                nav_go(sec, k, 0);
                return;
            }
}
/* rule 1: SELECT, s detents along the chain (no wrap) */
static void nav_select(int32_t s)
{
    uint8_t v[NAV_MAX];
    uint32_t sec = nav_sec(), n = nav_list(sec, v), e = nav_cur(), i;
    int32_t at = -1, to;
    for (i = 0; i < n; i++)
        if (v[i] == e)
            at = (int32_t)i;
    if (at < 0) {                                       /* (the entry left is not here: the nearest that is) */
        for (i = 0; i < n && v[i] < e; i++) {}
        at = (int32_t)i - (s > 0 ? 1 : 0);
    }
    to = clamp(at + s, 0, (int32_t)n - 1);
    if (n && (to != at || v[to] != e))
        nav_go(sec, v[to], 0);
}
/* rule 5: ENV / SCL / ARP / SAVE held + SELECT: into the section (its page last shown), then along its chain */
static void nav_jump(uint32_t sec, int32_t s)
{
    if (nav_sec() != sec || ui.home)
        nav_go(sec, nav_mem(nav_key(sec)), 1);
    if (nav_sec() == sec)
        nav_select(s);
}
/* rule 6: the groups shown (with a page here): their number n, the one shown (*g), its page's place in it (*p), each
 * one's pages (cnt) and chain index of the first (first); the chain's place *i of *m */
static uint32_t nav_groups(uint8_t *cnt, uint8_t *gi, uint32_t *g, uint32_t *p, uint32_t *i, uint32_t *m)
{
    uint8_t v[NAV_MAX];
    uint32_t sec = nav_sec(), n = nav_list(sec, v), e = nav_cur(), k, ng = 0, last = 0xFFu;
    const ch_t *c = nav_ch(sec);
    *g = *p = *i = 0;
    *m = n;
    for (k = 0; k < n; k++) {
        uint32_t gg = c->e[v[k]].g;
        if (gg != last) {
            cnt[ng] = 0;
            gi[ng++] = (uint8_t)gg;
            last = gg;
        }
        if (v[k] == e) {
            *g = ng - 1u;
            *p = cnt[ng - 1u];
            *i = k;
        }
        cnt[ng - 1u]++;
    }
    return ng;
}
static const char *nav_group_name(uint32_t g) { const ch_t *c = nav_ch(nav_sec()); return c->g[g % c->ng].nm; }
static const char *nav_group_tab(uint32_t g)
{
    const ch_t *c = nav_ch(nav_sec());
    return c->g[g % c->ng].tab ? c->g[g % c->ng].tab : c->g[g % c->ng].nm;
}
/* the page's name in the chain: EDIT 1 . OSC (the engine's page name), KIT, TRACKS DEST, SENDS .. */
static __attribute__((noinline)) const char *nav_page_name(void)
{
    static char b[24];
    uint32_t sec = nav_sec(), e = nav_cur();
    const nv_t *v = &nav_ch(sec)->e[e];
    if (sec != SEC_HOME && v->t && !str_eq(PAGES[ui.page].title, v->t)) {   /* (PHRASES, CHANCE: their own) */
        str_cpy(b, PAGES[ui.page].title, 24);
        return b;
    }
    str_cpy(b, v->nm ? v->nm : v->t, 24);
    if (sec == SEC_EDIT && v->g == 0u && !v->nm && PAGES[ui.page].scope == SC_ENGINE) {   /* (EDIT 1 . OSC) */
        const char *pt = trk_engp(TSEL)->page_title[PAGES[ui.page].id[0] != P_E0];
        if (pt && pt[0] && pt[0] != '-') {
            str_cpy(b + str_len(b), " \xB7 ", 4);
            str_cpy(b + str_len(b), pt, 24 - str_len(b));
        }
    }
    return b;
}
/* (code, the tests, the renders) the page of section sec named nm in its chain */
static int nav_put(uint32_t sec, const char *nm)
{
    const ch_t *c = nav_ch(sec);
    uint32_t k;
    for (k = 0; k < c->n; k++)
        if (str_eq(c->e[k].nm ? c->e[k].nm : c->e[k].t, nm) && nv_ok(sec, k)) {
            nav_go(sec, k, 0);
            return 1;
        }
    return 0;
}

/* EDIT > FOCUS (a synth's detail): the parameter shown (E1..E8 that has a name: 0..7), the MOD slot aimed at it (its
 * source set; -1 none) */
static int focus_named(uint32_t k)
{
    const param_desc_t *d = track_desc(TSEL, P_E0 + (k & 7u));
    return d && d->label && d->label[0] != '-' && d->max > d->min;
}
static uint32_t focus_k(void)
{
    uint32_t k = ui.focus & 7u, n;
    for (n = 0; n < 8u && !focus_named(k); n++)
        k = (k + 1u) & 7u;
    return k;
}
static int32_t focus_slot(uint32_t k)
{
    uint32_t j;
    for (j = 0; j < 4u; j++)
        if (TSEL->p[P_M1SRC + 3u * j] && TSEL->p[P_M1DST + 3u * j] == (int16_t)(MD_E1 + k))
            return (int32_t)j;
    return -1;
}


