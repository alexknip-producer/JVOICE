/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca quick layers: while a page button is held, the keys and KNOB 1..4
 * are its shortcuts and its map shows over the page. One table (LAYERS) drives the gesture, the keys, the knobs,
 * the LEDs and the overlay:
 *   FX   HOLD  the performance effects (perform.c) and the track mutes while held; KNOB 1..4 its macros
 *   GLO  SET   black keys 1..4 (F#3 G#3 A#3 C#4) T1..T4 MUTE (latched; lit = sounding), F3..B3 SOLO T1..T4 while
 *              held (HOLD cells, a corner triangle), C4 UNMUTE ALL, F4 TAP tempo; KNOB 1..4 T1..T4 LEVEL;
 *              black keys 5..8 (D#4 F#4 G#4 A#4) the DRUM group mutes KICK SNARE HAT PERC (eng_drum.c dx_mute; the
 *              row shows while a track plays DRUM; C4 unmutes them too);
 *              GLO + PLAY: from the top without stopping
 *   SCL  SET   any key: its note name is ROOT; KNOB 1..4 ROOT SCL CHRD VOIC (LY_SCL: the SCL page's first two,
 *              the CHORD page's two; QNT TRN stay on SCL); the LEDs show the root lit and the scale's notes breathing
 *   EDIT SET   (JIANT: Felucca's engine keys are gone, PRESETS picks engines and sounds) F3 INIT: the track's sound
 *              to its engine's first preset, a DRUM track's DRUM-X kit to the factory one; G3 RECALL: the track's
 *              sound (and a DRUM track's kit) as the section playing stored it, the safe state (project.c
 *              project_recall_sound). Each asks first (the dialog closes the layer); the steps stay. KNOB 1..4 the
 *              EDIT page's four (the engine's first); on a DRUM track black keys 1..8 mute each DRUM-X sound
 *              (eng_drum.c DXM_LANE, saved with the section; lit = sounding)
 *   SEQ  SET   (1.2, on the SEQ pages that show the pattern; 0.8.3: every page of LENTE's three levels, not PHRASES and
 *              SONG; elsewhere SEQ held is a tap) TOOLS: the white keys from F3 the sequence tools (ui_tools.c: CLEAR,
 *              REVERSE, SHIFT < >, RANDOM, COOK, CLR AUTO (0.6.8: the track's automation); on a DRUM track BEAT and the lane's CLEAR REVERSE FILL RANDOM), black
 *              keys 1..8 the lane (DRUM); KNOB 1..4 the PATTERN page's LEN DIV SWING GATE. No undo (TONIC: the song
 *              layer's RECALL)
 *   REC  SET   (1.1.5, on every page; REC's tap still arms / disarms) the white keys from F3: CLEAR (the selected
 *              track's steps and automation, no dialog), CLICK (OFF REC ON, each press the next value); KNOB 1 the
 *              same (TONIC: COUNT-IN and CLICK LEVEL only in MENU > AUDIO). The settings
 *              are MENU > AUDIO's (ui.c rp_get / rp_put: one store), saved when the layer closes.
 *              Replaces the old REC held on SEQ (Discussion #91:
 *              the CLEAR Tn SEQUENCE? dialog and its "HOLD: CLEAR" hint)
 * The gesture: let go before HOLD (the menu: 0.3 .. 0.6 s) with nothing else touched: a tap, the button's page.
 * Held past HOLD alone: the map (a peek), letting go does nothing. A key, a knob or a button meanwhile: a combo,
 * the map at once, no tap. Keys pressed with the button down are the layer's (seq.c keyboard_block): silent, no
 * MIDI, never recorded; keys held before stay notes. Only one layer at a time: a second layer button is ignored.
 * PLAY and REC work in every layer (GLO + PLAY: RESTART); SAVE, HOME, SEQ and the other page buttons are swallowed. In SET
 * layers OCT± do not shift the octave (TONIC: OCT- no longer puts back what the layer changed: no undo). A HOLD key's effect lasts until the key is let go, the map with it. No layer in the menu, a dialog,
 * NAME or the UPDATE MODE countdown. A tap says nothing of the layer (0.9.5: the "HOLD [GLO] QUICK" hint is gone).
 * TONIC: no lock (Felucca's double tap, Discussion #83, is gone): a layer is open while its button is held. */
#include "ui_tools.c"                                   /* SEQ TOOLS' actions */
/* the song layer's actions (project.c: song_main.c) */
static int section_jump(uint32_t s);
static void section_store(uint32_t s);
static void section_recall(void);
static void quick_chain(const uint8_t *s, uint32_t n);
static void song_rec_toggle(void);
static int song_select(uint32_t n);
static int project_used(uint32_t slot);
enum { LK_HOLD, LK_SET };
typedef struct {
    uint8_t btn, kind, fam;            /* the button, HOLD / SET, the family whose first page KNOB 1..4 edit */
    const char *head;                  /* (FAM_HOME: the layer's own knobs) the header over the map */
    khint_t foot[3];                   /* the footer's key hints (a third with no word: none) */
} layer_t;
static const layer_t LAYERS[LAYER_N] = {
    {0, 0, 0, "", {{0, 0}, {0, 0}, {0, 0}}},
    {B_FX, LK_HOLD, FAM_HOME, "[FX] HOLD", {{KC_KEYS, "EFFECTS"}, {KC_K14, "MACROS"}, {0, 0}}},   /* (LET GO: the header's HOLD) */
    {B_GLO, LK_SET, FAM_HOME, "[GLO] SET", {{KC_PLAY, "RESTART"}, {KC_GLO, "DONE"}, {0, 0}}},
    {B_SCL, LK_SET, FAM_SCL, "[SCL] SET", {{KC_KEYS, "ROOT"}, {KC_SCL, "DONE"}, {0, 0}}},
    {B_EDIT, LK_SET, FAM_EDIT, "[EDIT] SET", {{KC_SELECT, "DICE"}, {KC_K14, "TWEAK"}, {KC_KEYS, "INIT / RECALL"}}},   /* (0.9.9-seq2e:
                                                                     * KNOB 1..4 MOMENTARY: mom_knob; seq2f: SELECT the sound dice) */
    {B_SEQ, LK_SET, FAM_SEQ, "[SEQ] TOOLS", {{KC_REC, "CLEAR ALL"}, {KC_SEQ, "DONE"}, {0, 0}}},   /* (0.6.14: REC, all six
                                                                     * tracks cleared, shown: it was nowhere) */
    {B_REC, LK_SET, FAM_HOME, "[REC] SET", {{KC_KEYS, "RECORDING"}, {KC_REC, "DONE"}, {0, 0}}},
    {B_SAVE, LK_SET, FAM_HOME, "[SAVE] SONG", {{KC_KEYS, "PLAY"}, {KC_OCTUP, "STORE"}, {KC_OCTDN, "RECALL"}}},
    {B_LFO, LK_SET, FAM_HOME, "[LFO] MACRO", {{KC_K14, "M1-M4"}, {KC_SELECT, "DICE"}, {KC_KEYS, "DICE / CLEAR"}}},   /* JIANT (mod.c macro_v; SELECT, F3: macro_dice, G3 clears) */
};
/* SCL's knobs: the key and the sequence offset (cur_page() while the layer edits or draws them: page_over) */
static const page_t LY_SCL = {"SCL", FAM_SCL, SC_TRACK, GR_SCALE, {P_ROOT, P_SCALE, P_SOFS, P_POFS}};
/* SEQ TOOLS' knobs: the PATTERN page's (the length the tools work in) */
static const page_t LY_SEQ = {"PATTERN", FAM_SEQ, SC_TRACK, GR_STEPS, {P_SLEN, P_SDIV, P_SSWING, P_SGATE}};
static const page_t *ly_page(uint32_t l) { return l == LAYER_SCL ? &LY_SCL : l == LAYER_SEQ ? &LY_SEQ : 0; }
#define LY_OPEN 2u                     /* ui.ly_t0: the map opened (no tap any more) */
#define LY_COMBO 4u                    /* .. by a combo */
#define LY_DEAD 8u                     /* .. pressed where there is no layer: does nothing */
enum { LY_INIT, LY_RECALL };          /* EDIT: the white keys F3 G3 */
static const khint_t FX_LATCH_FOOT[3] = {{KC_KEYS, "ON / OFF"}, {KC_K14, "MACROS"}, {KC_OCTDN, "ALL OFF"}};
static const khint_t MAC_LATCH_FOOT[3] = {{KC_K14, "M1-M4"}, {KC_SELECT, "DICE"}, {KC_LFO, "DONE"}};   /* (MACRO latched) */

static struct {
    uint8_t l, oct;                    /* the layer open; OCT- / OCT+ pressed in a SET layer (bits) */
    uint8_t cook;                      /* SEQ: COOK pressed since it opened */
    uint8_t qc[8], nqc;                /* SAVE: the sections tapped in this hold (two or more: a quick chain) */
    uint8_t song;                      /* SAVE: the song KNOB 1 picked (+ 1; 0 none): in when SAVE lets go */
    uint8_t rp_dirty;                  /* REC: a setting changed (settings_save when the layer lets go) */
    uint32_t solo;                     /* GLO: the keys held that solo */
    uint32_t tap[4];                   /* GLO TAP: the last taps (fm1_ms) */
    uint8_t ntap;
    uint8_t quiet;                     /* #39: a layer closed after it opened: KNOB 1..4 do nothing until quiet_t + */
    uint32_t quiet_t;                  /* LY_QUIET_MS (fm1_ms) */
    uint8_t used;                      /* (0.6.44) a key, a knob or OCT used in the layer since it was armed */
} lys;
#define LY_QUIET_MS 250u

/* (0.6.44) FX LATCH: FX held open and let go locks its layer open, as the macros latch: its keys, knobs and map stay
 * until FX is pressed again (or another layer is held over it, for as long as it is) */
static uint8_t fx_lock;
static int layer_allowed(void) { return !ui.menu && !dlg_on() && !ui.uboot && !name_on(); }   /* (GEN: a page) */
static uint32_t ly_bit(uint32_t l) { return 1u << panel.btn[LAYERS[l].btn]; }
/* SEQ's layer is there on the SEQ pages only (elsewhere SEQ held is a tap: ui_input) */
static int tools_page(void)
{
    const page_t *pg = &PAGES[ui.page];
    return !ui.home && pg->fam == FAM_SEQ && pg->graph != GR_PATS && pg->graph != GR_SONG;   /* (0.8.3: every level
                                                         * of LENTE's, STRETCH PLAYHEADS MUTATE AUTO LIST too) */
}
static int ly_avail(uint32_t l) { return l != LAYER_SEQ || tools_page(); }   /* (REC: on every page) */
static uint32_t ly_down(uint32_t l) { return l && (fm1_in.buttons & ly_bit(l)) != 0u; }
static uint32_t layer_bits(void)
{
    uint32_t l, m = 0;
    for (l = LAYER_FX; l < LAYER_N; l++)
        m |= ly_avail(l) ? ly_bit(l) : 0u;
    return m;
}
static uint32_t layer_btn(void) { return LAYERS[ui.layer % LAYER_N].btn; }
static uint32_t layer_open(void);
static const char *layer_head(void)                     /* FX: LATCH, and the MIDI effects' tracks (A#4) */
{
    static const char *const H[3][3] = {{"[FX] HOLD", "[FX] HOLD SYN", "[FX] HOLD DRM"},
                                        {"[FX] LATCH", "[FX] LATCH SYN", "[FX] LATCH DRM"},
                                        {"[FX] LOCKED", "[FX] LOCK SYN", "[FX] LOCK DRM"}};
    if (ui.layer == LAYER_MACRO && !layer_open())       /* (JIANT 0.4) the MACRO layer latched */
        return "[LFO] LATCH";
    return ui.layer == LAYER_FX ? H[fx_lock && !ui.ly ? 2 : perf_latch_on ? 1 : 0][pfx_tgt % 3u] : LAYERS[ui.layer % LAYER_N].head;
}
static uint32_t layer_open(void)
{
    if (ui.ly && (ui.ly_t0 & LY_OPEN) && ly_down(ui.ly) && layer_allowed())
        return ui.ly;
    return fx_lock && !ui.ly && layer_allowed() ? LAYER_FX : 0u;
}
static int layer_set_open(void)                 /* (FX LATCH: FX is one too, OCT- turns all off) */
{
    return layer_open() && (LAYERS[layer_open()].kind == LK_SET || (layer_open() == LAYER_FX && perf_latch_on));
}
static uint32_t layer_held(void) { return ui.ly && ly_down(ui.ly) && layer_allowed(); }

/* each pass, before the keys: what the ISR (seq.c keyboard_block) gives to a layer. Armed: its button; none
 * armed: any layer button not already down (one pressed meanwhile is armed below; one held over from before
 * is ignored, its keys stay notes); armed but dead (pressed under a dialog, a menu or NAME that has closed
 * since): nothing, its keys stay notes and FX starts no effect with no map shown; FX's effects only with FX */
static void layer_masks(void)
{
    uint32_t m = !layer_allowed() || (ui.ly && (ui.ly_t0 & LY_DEAD)) ? 0u
               : ui.ly ? ly_bit(ui.ly) : layer_bits() & ~fm1_in.buttons;
    kb_mask = m;
    perf_mask = m & ly_bit(LAYER_FX);
    fx_lock_on = (uint8_t)(fx_lock && !ui.ly && layer_allowed());
}

/* a layer button pressed (its edge) while none is armed: it is now, a dead one where no layer opens */
static void layer_arm(uint32_t pressed, uint32_t now)
{
    uint32_t l;
    if (ui.ly == LAYER_REC && (pressed & ~ly_bit(LAYER_REC)))
        lys.used = 1;                                   /* (V10.7) REC held and another button: no UNDO at its let go */
    if (ui.ly)
        return;
    if (fx_lock && (pressed & ly_bit(LAYER_FX))) {      /* (0.6.44) FX pressed again: the lock off, nothing else */
        fx_lock = 0;
        ui.force = 1;
        ui.ly = LAYER_FX;
        ui.ly_t0 = (now & ~15u) | 1u | LY_DEAD;
        ui_message("FX UNLOCKED");
        return;
    }
    if (mac_latch && (pressed & ly_bit(LAYER_MACRO))) { /* (JIANT 0.4) LFO pressed again: the latch off, nothing else */
        mac_latch = 0;
        ui.force = 1;
        ui.ly = LAYER_MACRO;
        ui.ly_t0 = (now & ~15u) | 1u | LY_DEAD;
        return;
    }
    for (l = LAYER_FX; l < LAYER_N; l++)
        if ((pressed & ly_bit(l)) && ly_avail(l)) {
            lys.used = 0;
            ui.ly = (uint8_t)l;
            ui.ly_t0 = (now & ~15u) | 1u | (layer_allowed() ? 0u : LY_DEAD);
            return;
        }
}

/* (0.9.9-seq2e) MOMENTARY, the owner's three EDIT gestures: a tap, EDIT's pages; held, its layer; held and KNOB 1..4
 * turned (before the layer shows or after), the knob edits the page under it (the page shown before EDIT went down),
 * live, recorded nowhere (no motion), and EDIT let go every parameter touched in the hold glides back over MOM_GLIDE_MS
 * to its value when first touched (motion.c mom[]: up to MOM_N); a save meanwhile stores those values. The page under
 * shows, "MOMENTARY" in the header; no tap, no layer after it. SELECT is the sound dice (0.9.9-seq2f; CF_DICE_SOUND) */
#define MOM_GLIDE_MS 32u
static void mom_release(void);
static int16_t *mom_ptr(const mom_t *m) { return m->t == MOM_G ? &song.g[m->id] : &trk[m->t % NTRK].p[m->id]; }
static void mom_finish(void)                            /* every value back, now */
{
    uint32_t i;
    for (i = 0; i < mom_n; i++)
        *mom_ptr(&mom[i]) = mom[i].v;
    mom_n = 0;
    mom_glide = 0;
}
static void mom_forget(uint32_t t)                      /* track t's sound replaced (EDIT + SELECT's dice): its own kept */
{
    uint32_t i, n = 0;
    for (i = 0; i < mom_n; i++)
        if (mom[i].t != t)
            mom[n++] = mom[i];
    mom_n = (uint8_t)n;
}
/* the knob k's value on the page under, if a plain parameter (a track's or a song global; not a list, an action, a step,
 * the kit, an operator: those are not momentary) */
static __attribute__((noinline)) int16_t *mom_vp(uint32_t k, const param_desc_t **d)
{
    const page_t *pg = cur_page();
    int16_t *vp = 0;
    uint32_t i, g = pg->graph;
    *d = 0;
    if (ui.menu || ui.confirm || gen_on() || mac_latch || lock_held())
        return 0;
    if (ui.home) {
        if (home_macros())
            return 0;
        *d = home_param(k, &vp);
    } else {
        if (FIT_PAGE(pg) || pg->scope == SC_STEP || pg->scope == SC_TRK || ((act_cols() >> k) & 1u) || g == GR_COMP || g == GR_GCOMP || g == GR_SHAPE ||
            g == GR_FUGUE || g == GR_CHANCE || g == GR_DXSND || g == GR_SWARM || g == GR_FOCUS || (g == GR_STAGE && k < 2u) ||
            g == GR_EVENTS || g == GR_MOTION || g == GR_SONG || g == GR_BROWSE || g == GR_TOOLS || g == GR_SLOTS ||
            g == GR_SONGSAVE || g == GR_USER || g == GR_PATS || DX_KIT3(pg) || fop_page(g) ||
            (k == 0u && (BAR_PAGE(pg) || g == GR_SLPIT || g == GR_MSEQ || g == GR_MOD)))
            return 0;
        *d = page_desc(pg, k, &vp);
    }
    if (!*d || !vp || (*d)->max == (*d)->min)
        return 0;
    for (i = 0; i < NTRK; i++)
        if (vp >= trk[i].p && vp < trk[i].p + P_COUNT)
            return vp;
    return vp >= song.g && vp < song.g + G_COUNT && vp != &song.g[G_SLOT] && vp != &song.g[G_LOAD] &&
           vp != &song.g[G_SAVE] ? vp : 0;
}
static int mom_keep(uint32_t t, uint32_t id)           /* (t, id) kept as it is now, once a hold; 0: no room */
{
    uint32_t i;
    for (i = 0; i < mom_n; i++)
        if (mom[i].t == t && mom[i].id == id)
            return 1;
    if (mom_n >= MOM_N)
        return 0;
    mom[i].t = (uint8_t)t;
    mom[i].id = (uint8_t)id;
    mom[i].v = t == MOM_G ? song.g[id] : trk[t % NTRK].p[id];
    mom_n++;
    return 1;
}
static __attribute__((noinline)) void mom_knob(uint32_t k, int32_t s)
{
    const param_desc_t *d;
    int16_t *vp;
    uint32_t t, id;
    if (!mom_on) {                                      /* the hold becomes MOMENTARY: no layer, no tap */
        mom_finish();                                   /* (a glide still going: there now) */
        mom_on = 1;
        ui.ly_t0 = (ui.ly_t0 & ~LY_OPEN) | LY_DEAD;
        ui_message("MOMENTARY");
        ui.force = 1;
    }
    ui.hot_col = (uint8_t)k;
    ui.hot_t = 40, ui.knob_ms = fm1_ms;
    if ((vp = mom_vp(k, &d)) == 0) {
        ui_message("NOT MOMENTARY HERE");
        return;
    }
    for (t = 0; t < NTRK && !(vp >= trk[t].p && vp < trk[t].p + P_COUNT); t++)
        ;
    id = t < NTRK ? (uint32_t)(vp - trk[t].p) : (uint32_t)(vp - song.g);
    if (t >= NTRK)
        t = MOM_G;
    if (t == MOM_G && id == G_RTYPE && mom_n + 3u > MOM_N) {
        ui_message("MOMENTARY: 16 AT MOST");
        return;
    }
    if (t == MOM_G && id == G_RTYPE) {                  /* (REVERB TYPE: PRE and DAMP change with it, ui_input.c) */
        (void)mom_keep(MOM_G, G_RPRE);
        (void)mom_keep(MOM_G, G_RDAMP);
    }
    if (!mom_keep(t, id)) {
        ui_message("MOMENTARY: 16 AT MOST");
        return;
    }
    if (ui.home)
        *vp = (int16_t)param_turn(d, *vp, accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
    else
        edit_param(k, s);
}
static void mom_release(void)                           /* EDIT let go: the glide back begins (a list's value: at once) */
{
    uint32_t i;
    mom_on = 0;
    for (i = 0; i < mom_n; i++)
        mom[i].from = *mom_ptr(&mom[i]);
    mom_glide = mom_n != 0u;
    mom_t0 = fm1_ms;
    if (ui.msg_t && str_eq(ui.msg, "MOMENTARY"))
        ui.msg_t = 0;
    ui.force = 1;
    mom_poll();
}
static void mom_poll(void)                              /* every pass */
{
    uint32_t i, dt;
    if (mom_on) {
        if (ui.msg_t < 8u)                              /* the header's tag, as long as it lasts */
            ui_message("MOMENTARY");
        return;
    }
    if (!mom_glide)
        return;
    if ((dt = fm1_ms - mom_t0) >= MOM_GLIDE_MS) {
        mom_finish();
        return;
    }
    for (i = 0; i < mom_n; i++) {
        const mom_t *m = &mom[i];
        const param_desc_t *d = m->t == MOM_G ? &GP[m->id] : track_desc(&trk[m->t % NTRK], m->id);
        *mom_ptr(m) = d->fmt == F_ENUM ? m->v :
                      (int16_t)(m->from + ((int32_t)m->v - m->from) * (int32_t)dt / (int32_t)MOM_GLIDE_MS);
    }
}

static void undo_ui(void);
/* the armed layer lets go (its button up): FX's macros snap back (FX LATCH: they stay); one that
 * opened, or a combo: KNOB 1..4 quiet for a while (#39: the knob still turning is not the page's) */
static __attribute__((noinline)) void layer_let_go(uint32_t quiet)
{
    if (ui.ly == LAYER_SAVE && lys.nqc >= 2u)           /* SAVE: two or more sections tapped: a quick chain */
        quick_chain(lys.qc, lys.nqc);
    if (ui.ly == LAYER_SAVE && lys.song && lys.song - 1u != song_cur)   /* SAVE: KNOB 1 picked another song */
        (void)song_select(lys.song - 1u);
    lys.nqc = 0;
    lys.song = 0;
    if (ui.ly == LAYER_FX && !perf_latch_on)
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    if (ui.ly == LAYER_FX && (ui.ly_t0 & LY_OPEN) && !(ui.ly_t0 & (LY_DEAD | LY_COMBO)) &&
        !lys.used && !kb_layer && layer_allowed()) {
        fx_lock = 1;                                    /* (0.6.44) held open, nothing used, let go: locked open;
                                                         * (0.6.64) whatever FX LATCH (the effects' own: momentary by
                                                         * default) */
        ui_message("FX LOCKED: [FX] TO UNLOCK");
        quiet = 0;
        ui.force = 1;
    }
    if (ui.ly == LAYER_REC && (ui.ly_t0 & LY_OPEN) && !(ui.ly_t0 & (LY_DEAD | LY_COMBO)) && !lys.used &&
        layer_allowed()) {                              /* (V10.7) REC held alone and let go: UNDO (again: REDO) */
        undo_ui();
        quiet = 0;
    }
    if (ui.ly == LAYER_MACRO && (ui.ly_t0 & LY_OPEN) && !(ui.ly_t0 & LY_DEAD)) {   /* (JIANT 0.4) MACRO opened, let go: */
        mac_latch = 1;                                  /* latched (its knobs at once: no quiet time) */
        quiet = 0;
        ui.force = 1;
    }
    if (ui.ly == LAYER_EDIT && mom_on)                  /* (0.9.9-seq2e) MOMENTARY: what was touched glides back */
        mom_release();
    if (lys.rp_dirty) {                                 /* REC: its settings kept, as MENU does when it closes */
        lys.rp_dirty = 0;
        settings_save();                                /* (deferred while playing) */
    }
    if (quiet) {
        lys.quiet = 1;
        lys.quiet_t = fm1_ms;
    }
    ui.ly_t0 = 0;
    ui.ly = 0;
}

/* the layer opened */
static void layer_opened(uint32_t l)
{
    lys.l = (uint8_t)l;
    lys.nqc = 0;
    lys.song = 0;
}

/* the armed button: 0, or the layer whose button was tapped (let go) */
static uint32_t layer_gesture(uint32_t now, uint32_t combo)
{
    uint32_t *t0 = &ui.ly_t0, l = ui.ly;
    if (!l)
        return 0;
    if (!layer_allowed())
        *t0 |= LY_DEAD;
    if (ly_down(l)) {
        if (!(*t0 & (LY_OPEN | LY_DEAD)) &&
            (combo || now - (*t0 & ~15u) >= (uint32_t)HOLD_MS[settings_hold % 4u] * 1000u * FM1_TICKS_PER_US)) {
            *t0 |= LY_OPEN | (combo ? LY_COMBO : 0u);
            layer_opened(l);
        }
        return 0;
    }
    l = *t0 & (LY_OPEN | LY_DEAD) || combo ? 0u : l;   /* (a knob turned as it was let go: a combo, no tap) */
    layer_let_go(*t0 & LY_OPEN || combo || mom_on);   /* (MOMENTARY: its knobs quiet too) */
    return l;
}

/* #39: KNOB 1..4 belong to no page while a layer lets go: the frame its button is let go (the turns read then
 * were made with it held), while its map still shows (its keys held after it), and LY_QUIET_MS after it closed.
 * Their turns are dropped there (ui_input); before, they edited the page under the layer (an ARP page: ARP on) */
static int layer_knobs_quiet(void)
{
    if (lys.quiet && fm1_ms - lys.quiet_t >= LY_QUIET_MS)
        lys.quiet = 0;
    return layer_allowed() && (ui.ly || ui.layer || lys.quiet);
}

/* the map shows while the button is held open, and after it while its keys are still held */
static void layer_show(void)
{
    uint32_t show = layer_allowed() ? (layer_open() ? layer_open() : kb_layer ? ui.layer : 0u) : 0u;
    if (show != ui.layer) {
        ui.layer = (uint8_t)show;
        ui.force = 1;
    }
}

/* a tap: the button's page (0.9.5: no "HOLD [X] QUICK" hint any more: the layers show themselves when held) */
static void layer_tap(uint32_t l)
{
    if (l == LAYER_REC)                                 /* REC: arms / disarms (no page) */
        (void)rec_tap();
    else if (l == LAYER_SAVE) {                         /* GEN's SAVE keeps the whole song. */
        if (gen_on()) nav_put(SEC_SAVE, "SAVE SONG AS");
        else open_family(FAM_SAVE);
    }
    else
        (void)page_tap(LAYERS[l].btn);
}
typedef char hk_room_t[HK_N <= 20 ? 1 : -1];   /* (bytes 20.. of engine 15's: other settings) */
/* (0.9.5) INIT SYSTEM: the teaching hints start over (none given) */
static void hints_reset(void)
{
    uint32_t k;
    for (k = 0; k < HK_N; k++)
        hint_given(k) = 0;
    ui.hint_k = 0;
    settings_save();
}
/* (0.9.5) a frame: the page's hint waiting (page_entered: MOTION, PUNCH, MUTATE) given once its page has been shown HINT_DWELL_MS (gone with
 * another page: walking past with SELECT says nothing), when nothing else is said or open over it, at most HINT_MAX times
 * ever (hint_given, kept with the settings); on its page only (ui.hint_pg) */
static void hint_tick(void)
{
    uint32_t k = ui.hint_k, at = ui.home ? 255u : ui.page + 1u;
    if (!k)
        return;
    if (ui.hint_at != at) {
        ui.hint_k = 0;
        return;
    }
    if (fm1_ms - ui.hint_ms < HINT_DWELL_MS || ui.msg_t || ui.menu || ui.confirm || ui.layer || name_on() || ui.uboot)
        return;
    ui.hint_k = 0;
    if (hint_given(k) >= HINT_MAX)
        return;
    hint_given(k)++;
    settings_save();                                    /* (the count kept: deferred while playing) */
    if (k == HK_MOTION)
        ui_message("REC + PLAY, TURN A KNOB");          /* (0.6.92) how it is recorded */
    else if (k == HK_PUNCH)
        ui_message("REC + PLAY, HOLD FX + A KEY");      /* (0.6.99) the lane's recording */
    else {                                              /* (V10.6) HK_EUCLID */
        int mu = cur_page()->id[0] == ID_MU0;           /* (V10.16) MUTATE's keys */
        ui_message(mu ? "EVERY: MUTATE ON THE BAR" : "TURN A KNOB: A PATTERN");
        str_cpy(ui.msg2, mu ? "OCT+ MUTATE  OCT- ANCHOR" : "OCT+ NEW  OCT- BEFORE", sizeof ui.msg2);
    }
    ui.hint_pg = (uint8_t)at;                           /* (0.9.0: this page's only) */
}

static uint32_t snd_id(void) { return TSEL->eng_req | (uint32_t)TSEL->preset << 8 | (uint32_t)TSEL->user << 16; }
/* EDIT RECALL: the section playing has a stored sound to come back to */
static int edit_recallable(void) { return project_recall_slot() >= 0; }

/* GLO TAP: from the third tap the tempo of the taps (the last 4); 2 s without one starts over. INT clock only */
static void glo_tap(void)
{
    uint32_t n = lys.ntap, i;
    if (song.g[G_CLOCK]) {
        ui_message("TAP: CLK IS EXT");
        return;
    }
    if (n && fm1_ms - lys.tap[n - 1u] > 2000u)
        n = 0;
    if (n == 4u) {
        for (i = 0; i < 3u; i++)
            lys.tap[i] = lys.tap[i + 1u];
        n = 3;
    }
    lys.tap[n++] = fm1_ms;
    lys.ntap = (uint8_t)n;
    if (n >= 3u && lys.tap[n - 1u] != lys.tap[0]) {
        song.g[G_BPM] = (int16_t)clamp((int32_t)(60000u * (n - 1u) / (lys.tap[n - 1u] - lys.tap[0])),
                                       GP[G_BPM].min, GP[G_BPM].max);
        ui.bpm_t = 40;
    }
}

/* SEQ TOOLS: white key place p's action on the selected track (ui_tools.c), "SONG PLAYS: STOP IT TO EDIT" while a song plays */
static void tools_key(uint32_t p)
{
    const char *m;
    if (!tl_cell(TSEL, p))
        return;
    if (chain_busy()) {
        ui_message("SONG PLAYS: STOP IT TO EDIT");
        return;
    }
    if (!(m = tl_do(TSEL, p, ui.lane)))
        return;
    ui_message(m);
    ui.force = 1;
}

/* Shared by GLO + G4 and REC + A3. One global undo/redo snapshot. */
static void undo_ui(void)
{
    if (chain_busy())
        ui_message("SONG PLAYS: STOP IT TO EDIT");
    else {
        uint32_t g = gls.u && gls_sig == gls_hash();    /* (GEN's FROM LOOP: the loop as it was, in LOOP mode) */
        if (undo_do()) { ui_message("NOTHING TO UNDO"); return; }
        if (g) { chain_config.count = 0; live.mode = 0; gls.u = 0; }
        if (ui.ly == LAYER_REC) song.rec = 0; /* keep the recovered take safe */
        ui_message("UNDONE (AGAIN: REDO)");
    }
}

/* REC: CLEAR, the selected track's steps and automation (and ARP's latched chord), no dialog, as SEQ TOOLS' CLEAR
 * (up to 1.1.4 REC held on SEQ asked first, CF_CLEAR_SEQ) */
static void rec_clear(void)
{
    track_t *t = TSEL;
    if (chain_busy()) {
        ui_message("SONG PLAYS: STOP IT TO EDIT");
        return;
    }
    if (seq_is_empty(t) && !motion_count(t)) {
        ui_message("NOTHING TO CLEAR");
        return;
    }
    undo_mark();                                        /* (0.6.21) UNDO: GLO + G4 */
    load_begin(t, LOAD_PAT);
    track_defaults_steps(t);
    load_end(t);
    latch_drop_req |= (uint8_t)(1u << (t - trk));       /* (0.6.19) the latched chord too (events_block) */
    {   /* (0.6.28) which track: the others play on */
        char cb[16] = "T1 CLEARED";
        cb[1] = (char)('1' + (t - trk));
        ui_say(cb, "");
        str_cpy(ui.msg2, "SEQ+REC: ALL TRACKS", sizeof ui.msg2);
    }
    ui.force = 1;
}
/* REC's settings, row r (G3, KNOB 1): CLICK as MENU > AUDIO steps it (ui.c rp_*). TONIC: COUNT-IN and CLICK LEVEL
 * left the layer (MENU > AUDIO still has them) */
#define RL_N 1u
static const uint8_t RL_F[RL_N] = {RP_CLICK};
static const char *const RL_LABEL[RL_N] = {"CLICK"};
static const uint8_t RL_ICON[RL_N] = {ICON_TEMPO};
static const char *rl_name(uint32_t r, uint32_t v) { (void)r; return CLICK_N[v % 3u]; }
static void rec_set(uint32_t r, uint32_t v)
{
    if (v == rp_get(RL_F[r]))
        return;
    rp_put(RL_F[r], v);
    lys.rp_dirty = 1;
}
static void rec_key(uint32_t p)
{
    uint32_t r = p - 1u, v;
    if (p == 2u) { undo_ui(); return; }       /* A3: UNDO / REDO */
    if (p == 3u) {                         /* B3: hard quantization off / on */
        TSEL->p[P_HRQ] = TSEL->p[P_HRQ] ? 0 : 127;
        return;
    }
    if (p == 0u) {
        rec_clear();
        return;
    }
    v = (rp_get(RL_F[r]) + 1u) % 3u;                    /* the next value, round */
    rec_set(r, v);
    ui_say("CLICK ", rl_name(r, v));
}

/* SAVE, the song layer (song_chain.c): white key p. 1..4 (F3..B3) play section A..D (on the next bar; tapped in a row
 * in one hold: a quick chain, layer_let_go), 5..8 (C4..F4) store the music into A..D, 13 (D5) LOOP / SONG (what PLAY
 * plays), 14 (E5) SONG REC, 16 (G5) the SONG page */
#define SK_PLAY 0u                                       /* the song layer's keys (white key places) */
#define SK_STORE 4u
#define SK_MODE 12u
#define SK_REC 13u
#define SK_PAGE 15u
#define SK_NAME 11u                                     /* (0.9.2) C5: NAME, the slot's rename (EDIT's until 0.9.1) */
#define TL_COPY 13u
#define TL_PASTE 14u
#define TL_STEP 15u                                     /* (0.9.2) SEQ's G5: CLR STEP (ui_input.c seq_clr_step) */
static void song_key(uint32_t p)
{
    uint32_t k;
    if (p < SK_STORE) {
        if (lys.nqc < sizeof lys.qc)
            lys.qc[lys.nqc++] = (uint8_t)p;
        if (lys.nqc == 1u)
            (void)section_jump(p);                       /* (the first: at once, as a single tap) */
        else {
            char b[24] = "CHAIN";
            for (k = 0; k < lys.nqc && k < 8u; k++) {
                char c[3] = {' ', (char)('A' + lys.qc[k]), 0};
                str_cpy(b + str_len(b), c, sizeof b - str_len(b));
            }
            ui_message(b);
        }
    } else if (p < SK_STORE + 4u) {
        section_store(p - SK_STORE);
    } else if (p == SK_MODE) {
        live.mode ^= 1u;
        ui_message(live.mode ? "PLAY: THE SONG" : "PLAY: THE LOOP");
    } else if (p == SK_REC) {
        song_rec_toggle();
    } else if (p == SK_NAME) {                          /* (0.9.2) SAVE + C5: USER's slot, PROJECT's project or section */
        if (!ui.home && (cur_page()->graph == GR_USER || cur_page()->graph == GR_SLOTS))
            name_rename();
        else
            ui_message("NAME: ON USER OR PROJECT");
    } else if (p == SK_PAGE) {
        for (k = 0; k < NPAGES && PAGES[k].graph != GR_SONG; k++)
            ;
        if (k < NPAGES) {
            ui.home = 0;
            ui.page = (uint8_t)k;
            page_entered();
        }
    }
}

/* a key pressed in layer l (k: 0 = F3 .. 26 = G5) */
/* (JIANT 0.4) MACRO DICE: random routes from M1..M4 to what each track's sound has (its engine's parameters, CUT,
 * SHP, the sends, a little PITCH), two a track into its free MOD slots, the macros spread so each moves two tracks:
 * the instrument invites a turn from the start, the routes there to tune (MOD) or keep (saved with the project).
 * mode 0 (each power-on, project.c autosave_boot then main.c): only a track with no macro route yet, so a session
 * restored keeps its own; 1 (MACRO layer F3): the macros' routes rolled again; 2 (G3): the macros' routes cleared */
/* (JIANT 0.5) a track that sounds now: not muted, and a voice of it ringing, or (playing) a step of its pattern on */
static int track_sounding(const track_t *t)
{
    uint32_t i, len = (uint32_t)t->p[P_SLEN];
    if (t->p[P_MUTE])
        return 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].stage && t->v[i].stage < 4u)
            return 1;
    if (song.playing)
        for (i = 0; i < len && i < NSTEP; i++)
            if (step_on(&seq_steps(t)[i]))
                return 1;
    return 0;
}
static void macro_dice(uint32_t mode)
{
    static const uint8_t DST[] = {MD_CUT, MD_SHP, MD_E1, MD_E1 + 1, MD_E1 + 2, MD_E1 + 3, MD_E1 + 4, MD_E1 + 5, MD_E1 + 6,
                                  MD_E1 + 7, MD_E1, MD_E1 + 1, MD_E1 + 2, MD_E1 + 3, MD_DLY, MD_REV, MD_DIST};   /* (V10.8: no PITCH, the
                                                         * macros' drift wobbled whole tracks out of tune; the
                                                         * engine's, its first four twice as likely) */
    uint32_t k, j, n;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        uint32_t has = 0, used = 0;
        for (j = 0; j < 4u; j++) {
            int16_t *m = &t->p[P_M1SRC + 3u * j];
            if (m[0] >= MS_M1 && m[0] < MS_M1 + 4) {
                if (mode) m[0] = m[1] = m[2] = 0;
                else has = 1;
            }
            if (m[0]) used |= 1u << m[1];
        }
        if (has || mode == 2u)
            continue;
        if (mode == 1u && !track_sounding(t))           /* (JIANT 0.5) a reroll: only the tracks that sound now */
            continue;
        if (mode == 3u && t->p[P_MUTE])                 /* (0.6.21) DICE ALL: every track not muted */
            continue;
        for (j = 0, n = 0; j < 4u && n < 2u; j++) {
            int16_t *m = &t->p[P_M1SRC + 3u * j];
            uint32_t d, tries;
            if (m[0])
                continue;
            for (tries = 0; tries < 16u; tries++) {     /* a destination the sound has, not used on this track */
                const param_desc_t *pd;
                d = DST[rng() % sizeof DST];
                if (((used >> d) & 1u) || !mod_dst_offer(t, d))
                    continue;
                if (d >= MD_E1 && d < MD_E1 + 8u &&
                    ((pd = track_desc(t, P_E0 + (d - MD_E1))) == 0 || pd->max <= pd->min || pd->label[0] == '-' ||
                     pd->fmt == F_SEMI))                /* (V10.38: never an interval, INT: out of tune) */
                    continue;
                break;
            }
            if (tries == 16u)
                continue;
            used |= 1u << d;
            m[0] = (int16_t)(MS_M1 + (k + 2u * n) % 4u);
            m[1] = (int16_t)d;
            m[2] = (int16_t)((d == MD_PITCH ? 4 + (int32_t)(rng() % 9u) : 24 + (int32_t)(rng() % 40u)) * (rng() & 1u ? 1 : -1));
            n++;
        }
    }
}

/* (JIANT 0.4) the session's macros (main.c after autosave_boot; the emulator too): the random numbers seeded by
 * something of this power-on, then the routes the tracks have none of */
static void session_dice(uint32_t seed)
{
    rng_state ^= seed * 2654435761u;
    if (!rng_state)
        rng_state = 0x1234567u;
    macro_dice(0);
}

/* GLO's white keys after the SOLOs (F3 ..): unmute all, tap tempo; KNOB 1..4: the levels from track trk_base4() */
#define GLO_ALL ((uint32_t)NTRK)
#define GLO_TAP 7u
#define GLO_UNDO 8u                                     /* (0.6.21) G4: UNDO the last big change (project.c undo_do) */
                                                        /* (V10.6) A4 FREEZE, B4 REVERT ALL and SEQ's D5 REVERT gone with MUTATE */
static __attribute__((noinline)) void layer_key(uint32_t l, uint32_t k)
{
    uint32_t p = key_place(k), i;
    if (l == LAYER_GLO) {
        if (key_black(k)) {
            if (p < NTRK)
                trk[p].p[P_MUTE] = (int16_t)!trk[p].p[P_MUTE];
            else if (p < NTRK + 4u)
                dx_mute_set(dx_mute ^ (1u << (p - NTRK)));
        } else if (p < NTRK) {
            lys.solo |= 1u << k;                        /* (layer_masks: perf_solo while held) */
        } else if (p == GLO_ALL) {
            for (i = 0; i < NTRK; i++)
                trk[i].p[P_MUTE] = 0;
            dx_mute_set(0);
        } else if (p == GLO_TAP) {
            glo_tap();
        } else if (p == GLO_UNDO) {
            undo_ui();
        }
    } else if (l == LAYER_SCL) {
        TSEL->p[P_ROOT] = (int16_t)((k + 5u) % 12u);    /* the key's note name (F3 = F) */
    } else if (l == LAYER_MACRO && !key_black(k) && p < 2u) {   /* (JIANT 0.4) F3 DICE, G3 CLEAR */
        if (chain_busy()) {
            ui_message("SONG PLAYS: STOP IT TO EDIT");
        } else {
            macro_dice(p ? 2u : 1u);
            ui_message(p ? "MACROS CLEARED" : "MACROS DICED");
        }
    } else if (l == LAYER_EDIT && key_black(k) && p < NLANE && drum_track(TSEL)) {
        dx_mute_set(dx_mute ^ DXM_LANE(p));             /* DRUM: black keys 1..8 mute each sound (lit = sounding) */
    } else if (l == LAYER_EDIT && !key_black(k) && p <= LY_RECALL) {
        if (chain_busy())
            ui_message("SONG PLAYS: STOP IT TO EDIT");
        else if (p == LY_RECALL && !edit_recallable())
            ui_message("NOTHING SAVED");
        else
            confirm_open(p == LY_INIT ? CF_INIT_SOUND : CF_RECALL_SOUND, song.sel);   /* (the dialog closes the layer) */
    } else if (l == LAYER_REC) {
        if (!key_black(k) && p < 4u)
            rec_key(p);
    } else if (l == LAYER_SEQ) {
        if (!key_black(k) && (p == TL_COPY || p == TL_PASTE))
            cp_seq(p == TL_PASTE);
        else if (!key_black(k) && p == TL_STEP)
            seq_clr_step();                             /* (0.9.2) G5: CLR STEP (AUTO LIST: the record) */
        else if (!key_black(k))
            tools_key(p);
        else if (p < NLANE && drum_track(TSEL))
            ui.lane = (uint8_t)p;                       /* (black keys 1..8: the lane, as on the grid) */
    } else if (l == LAYER_SAVE && !key_black(k)) {
        song_key(p);
    }
}
/* each pass: the keys the layer got now; then GLO's solo: the tracks of its keys still held (perform.c) */
static void layer_keys(uint32_t keys)
{
    uint32_t k, l = layer_open() ? layer_open() : ui.layer, s = 0;
    if (keys) lys.used = 1;
    for (k = 0; keys && k < 27u; k++)
        if ((keys >> k) & 1u)
            layer_key(l, k);
    lys.solo &= kb_layer;
    for (k = 0; k < 27u; k++)
        if ((lys.solo >> k) & 1u)
            s |= 1u << key_place(k);
    perf_solo = (uint8_t)s;
}

/* KNOB 1..4 in the open layer: its own (FX macros, GLO levels, EDIT's sound), else the four of its page */
static void layer_knob(uint32_t k, int32_t s)
{
    uint32_t l = layer_open();
    lys.used = 1;
    if (l == LAYER_FX) {                                /* perform.c perf_k, not recorded */
        if (k == 3u)
            s = -s;                                     /* DEPTH (100 - perf_k[3]) rises to the right (#40: it fell) */
        perf_k[k] = (int8_t)clamp(perf_k[k] + accel(EN_K1 + k, s, k ? 100 : 200), k ? 0 : -100, 100);   /* (#126) */
    } else if (l == LAYER_GLO) {                        /* T1..T4 LEVEL (T5, T6 selected: T3..T6), recorded as on MIXER */
        track_t *t = &trk[k + trk_base4()];
        int16_t *vp = &t->p[P_LEVEL];
        *vp = (int16_t)clamp(*vp + accel(EN_K1 + k, s, TP[P_LEVEL].max - TP[P_LEVEL].min), TP[P_LEVEL].min,
                             TP[P_LEVEL].max);
        motion_capture(t, P_LEVEL, *vp);
    } else if (l == LAYER_SAVE) {                       /* KNOB 1: the song (in when SAVE lets go); 2..4: none */
        if (k == 0u)
            lys.song = (uint8_t)(clamp((int32_t)(lys.song ? lys.song - 1u : song_cur) + (s > 0 ? 1 : -1), 0,
                                       NSONG - 1) + 1);
    } else if (l == LAYER_MACRO) {                      /* M1..M4 (the project's; the MOD slots that name them) */
        macro_v[k] = (uint8_t)clamp(macro_v[k] + accel(EN_K1 + k, s, 127), 0, 127);
    } else if (l == LAYER_REC) {                        /* K1 CLICK, K2 REC Q */
        if (k == 1u)
            TSEL->p[P_HRQ] = (int16_t)clamp(TSEL->p[P_HRQ] + accel(EN_K2, s, 127), 0, 127);
        else if (k < RL_N)
            rec_set(k, (uint32_t)clamp((int32_t)rp_get(RL_F[k]) + s, 0, 2));
    } else if (l) {                                     /* the page's knobs, wherever the page is */
        uint8_t h = ui.home, pg = ui.page, nm[sizeof ui.nmem];
        memcpy(nm, ui.nmem, sizeof nm);                 /* (the sections' memories: not this borrowed page's) */
        ui.home = 0;
        ui.page = (uint8_t)page_first(LAYERS[l].fam);
        page_over = ly_page(l);
        edit_param(k, s);
        page_over = 0;
        ui.home = h;
        ui.page = pg;
        memcpy(ui.nmem, nm, sizeof nm);
    }
}

/* PLAY with GLO held open: RESTART (from the top, playing on); stopped: PLAY. 1 = done here */
static int layer_play(void)
{
    if (layer_open() != LAYER_GLO || chain_busy())
        return 0;
    if (song.g[G_CLOCK] && song.playing) {
        ui_message("RESTART: CLK IS EXT");
        return 1;
    }
    transport_req = song.playing ? 3u : 1u;
    return 1;
}

/* OCT- / OCT+ pressed with a SET layer's button down (FX LATCH: FX's) before it opened: a combo, so it opens now,
 * before oct_taps and layer_oct read it (else the press was nobody's: no put back / ALL OFF, no octave) */
static void layer_oct_open(uint32_t pressed)
{
    uint32_t l = ui.ly, oct = 1u << panel.btn[B_OCTDN] | 1u << panel.btn[B_OCTUP];
    if ((pressed & oct) && l && !(ui.ly_t0 & (LY_OPEN | LY_DEAD)) && layer_held() &&
        (LAYERS[l].kind == LK_SET || (l == LAYER_FX && perf_latch_on))) {
        ui.ly_t0 |= LY_OPEN | LY_COMBO;
        layer_opened(l);
    }
}

/* OCT- / OCT+ in a SET layer: no octave (TONIC: no put back; FX LATCH: OCT- all off). Returns the taps left */
static uint32_t layer_oct(uint32_t pressed, uint32_t oct)
{
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP], mine;
    if (layer_set_open())
        lys.oct |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    if (ui.ly && (pressed & (1u << dn | 1u << up)))
        lys.used = 1;                                   /* (0.6.44: an OCT with it: no FX lock) */
    mine = oct & lys.oct;
    if ((mine & 1u) && lys.l == LAYER_FX) {             /* OCT- (FX LATCH): every effect and macro off */
        perf_latched = 0;
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
        ui_message("FX ALL OFF");
    }
    if (lys.l == LAYER_SAVE && (mine & 2u)) {           /* SAVE + OCT+: STORE the music into the section playing */
        if (live.cur < 0)
            ui_message("PLAY A SECTION FIRST");
        else
            section_store((uint32_t)live.cur);
    }
    if (lys.l == LAYER_SAVE && (mine & 1u))             /* SAVE + OCT-: RECALL it as it was stored */
        section_recall();
    lys.oct &= (uint8_t)(((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1);
    return oct & ~mine;
}

/* --------------------------------------------------------- the LEDs --- */
/* lit = in effect now (held, the value, a track sounding), breathing = can be pressed (*br: dark .. ~60 % and
 * back, ~1.1 s, hal/fm1_input.h fm1_led_breath; was a hard 250 ms blink, #119), dark = nothing there */
static int glo_sounding(uint32_t t) { return !trk[t].p[P_MUTE] && (!perf_solo || ((perf_solo >> t) & 1u)); }
static int glo_drums(void)                              /* a track plays DRUM: GLO shows its group mutes */
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        if (drum_track(&trk[i]))
            return 1;
    return 0;
}
static int rec_clearable(void) { return !seq_is_empty(TSEL) || motion_count(TSEL) != 0u; }
static uint32_t layer_leds(uint32_t *br)
{
    uint32_t k, m = 0, n = 0, l = ui.layer, held = perf_held | perf_latched, ok = perf_avail();
    uint32_t mask = scale_mask(TSEL), root = (uint32_t)TSEL->p[P_ROOT] % 12u;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), b = (uint32_t)key_black(k), on = 0, can = 0, e;
        if (l == LAYER_FX) {                            /* effects breathe, held lit, a too-long REPEAT dark */
            e = perf_key(k);
            can = e < PF_N && ((ok >> e) & 1u);
            on = can && ((held >> e) & 1u);
        } else if (l == LAYER_GLO) {                    /* sounding lit; SOLO held lit, the others, C4, F4 breathe */
            on = b ? (p < NTRK && glo_sounding(p)) || (p >= NTRK && p < NTRK + 4u && !((dx_mute >> (p - NTRK)) & 1u))
                   : p < NTRK && ((lys.solo >> k) & 1u);      /* (a group sounding: lit) */
            can = !b && ((p < NTRK && p < trk_live) || p == GLO_ALL || (p == GLO_TAP && !song.g[G_CLOCK]) ||
                         (p == GLO_UNDO && undo_ok));
                                                        /* (the keys that do something breathe: not an asleep track's) */
        } else if (l == LAYER_SCL) {                    /* the root lit, the scale's notes breathe */
            e = (k + 5u + 12u - root) % 12u;
            on = e == 0u;
            can = (mask >> e) & 1u;
        } else if (l == LAYER_EDIT && !b) {             /* INIT and RECALL breathe (RECALL: a stored section) */
            can = (p == LY_INIT && !chain_busy()) || (p == LY_RECALL && edit_recallable());   /* (V10.6z8: RECALL in a song) */
        } else if (l == LAYER_EDIT && p < NLANE && drum_track(TSEL)) {   /* DRUM: a sound sounding lit, muted dark */
            on = !dx_lane_muted(p);
        } else if (l == LAYER_SEQ) {                    /* the tools breathe (not while a song plays); DRUM: the lane */
            on = b && p < NLANE && drum_track(TSEL) && p == ui.lane;   /* lit, the other lanes breathe */
            can = b ? p < NLANE && drum_track(TSEL) : p == TL_COPY ? 1u : p == TL_PASTE ? clip.n && !chain_busy() : tl_cell(TSEL, p) && !chain_busy() &&
                  (p != TL_CAUTO || motion_count(TSEL));   /* (CLR AUTO: automation to clear; REVERT: an anchor) */
        } else if (l == LAYER_SAVE && !b) {             /* the section playing lit, the saved ones and the */
            on = (p < SK_STORE && live.cur == (int8_t)p) || (p == SK_REC && live.srec) ||   /* actions breathe */
                 (p == SK_MODE && live.mode);
            can = p < SK_STORE ? project_used(p) : p < SK_STORE + 4u || p == SK_MODE || p == SK_REC || p == SK_PAGE;
        } else if (l == LAYER_REC && !b && p < 4u) {   /* the click on: lit, else it breathes; CLEAR: */
            on = (p == 1u && rp_get(RP_CLICK) != 0u) || (p == 3u && !TSEL->p[P_HRQ]);    /* something to clear, not while a song plays */
            can = p == 2u ? !chain_busy() && undo_ok : p ? 1u : !chain_busy() && rec_clearable();
        }
        m |= on << k;
        n |= (can & !on) << k;
    }
    if (br)
        *br = n;
    return m;
}

/* ------------------------------------------------------ the overlay --- */
/* (0.8.9) The layers as the approved LENTE mock-up's CAPAS (MAP . KNOBS . STATE) in the skins' language (the mock-up
 * opciones_skin.html): the header "[GLO] SET" in plain words; the cards KNOB 1..4 in their colours (COLORS on top,
 * BOTTOM / HUD the knob row); MAP the keyboard itself, F3 .. G5, 16 white keys and 11 black, each named upright by what it
 * does there (none: an empty key): a key that can be pressed RAISE (white) or tinted (black), the value now THEME, held
 * the accent, a muted track KEY, unavailable its name DIM, its note under it; KNOBS the four as gauges in their knobs'
 * colours over the layer's keys (the keycaps' words); STATE what is on, as chips. (Until 0.8.8: cells with note names,
 * the Felucca map.) The keys and knobs do what they did: only the drawing is new */
static const char *const PF_NAME[PF_M1] = {"RPT 1/8", "RPT 1/16", "RPT 1/32", "LPF", "HPF",   /* the audio ones (perform.c) */
    "OCT-", "OCT+", "1/2 TEMPO", "DEC-", "DEC+", "STUT 1/8", "STUT 1/16", "ATK+", "ARP", "RANDOM"};   /* the MIDI ones (pfx.c) */
static const char W_NOTE[16] = {'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'A', 'B', 'C', 'D', 'E', 'F', 'G'};
static const char B_NOTE[11] = {'F', 'G', 'A', 'C', 'D', 'F', 'G', 'A', 'C', 'D', 'F'};   /* black keys 1..11: F# G# .. F#5 */
static const uint8_t PK_BLK[11] = {1, 2, 3, 5, 6, 8, 9, 10, 12, 13, 15};   /* a black key: left of white key n */
enum { LS_OFF, LS_SEL, LS_HELD, LS_WAIT, LS_DIM, LS_MUTE };
static const char *const GLO_GROUP[4] = {"KICK", "SNARE", "HAT", "PERC"};
static const char *const TL_NAME[TL_N] = {"CLEAR", "REVERSE", "SHIFT <", "SHIFT >", "RANDOM", "COOK", "BEAT", "CLR AUTO",
    "CLR", "REV", "FILL", "RND"};
#define LMC_TW 140                                       /* MACRO's routes: their width (x 92 .. 232) */
static uint32_t macro_routes(uint32_t m, char *b, uint32_t n)
{
    uint32_t i, k, c = 0;
    if (b)
        b[0] = 0;
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < 4u; k++) {
            const int16_t *p = &trk[i].p[P_M1SRC + 3u * k];
            if (p[0] != (int16_t)(MS_M1 + m) || !p[1] || !p[2])
                continue;
            c++;
            if (b && str_len(b) + 12u < n) {           /* (as many as fit in LMC_TW; "+" when more) */
                char e[16] = {'T', (char)('1' + i), ' ', 0};
                uint32_t l0 = str_len(b);
                str_cpy(e + 3, mod_dst_name(&trk[i], p[1]), sizeof e - 3);
                str_cpy(e + str_len(e), p[2] > 0 ? "+" : "", sizeof e - str_len(e));
                fmt_int(e + str_len(e), p[2] * 100 / 64);
                if (l0)
                    str_cpy(b + l0, " ", n - l0);
                str_cpy(b + str_len(b), e, n - str_len(b));
                if (text_w(&AF_S, b) > LMC_TW) {
                    b[l0] = 0;
                    str_cpy(b + l0, " +", n - l0);
                    n = l0 + 3u;                        /* (nothing more goes in) */
                }
            }
        }
    return c;
}
static uint32_t macro_sig(void)
{
    uint32_t i, k, h = (uint32_t)macro_v[0] | (uint32_t)macro_v[1] << 8 | (uint32_t)macro_v[2] << 16 | (uint32_t)macro_v[3] << 24;
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < 12u; k++)
            h = (h ^ (uint32_t)trk[i].p[P_M1SRC + k]) * 16777619u;
    return h + snd_id() * 31u;
}
/* key p (white, or black: blk) of layer l: its name into nm (12; "" none), its state */
static uint32_t ly_keyname(uint32_t l, int blk, uint32_t p, char *nm)
{
    uint32_t busy = chain_busy() ? LS_DIM : LS_OFF;
    nm[0] = 0;
    if (l == LAYER_FX) {
        uint32_t held = perf_kill ? 0u : perf_held | perf_latched, e = blk ? PF_M1 + p : p;
        static const char *const TG[3] = {"TRK ALL", "TRK SYN", "TRK DRM"};
        if (blk && p == 7u) {
            str_cpy(nm, TG[pfx_tgt % 3u], 12);
            return LS_OFF;
        }
        if (!blk && p == PF_M1) {                       /* (snd2) G5: CLEAR AUTOMATION, the punch-in lane's */
            str_cpy(nm, "CLR AUTO", 12);
            return pfx_clr ? LS_HELD : LS_OFF;
        }
        if (blk && p >= NTRK && p != 7u && p <= 10u) {  /* (snd2) G#4 LOOP 3, C#5 LOOP 5, D#5 LOOP 7, F#5 GLITCH */
            static const char *const BN[4] = {"LOOP 3", "LOOP 5", "LOOP 7", "GLITCH"};
            static const uint8_t BE[4] = {PF_LP3, PF_LP5, PF_LP7, PF_GLT};
            uint32_t j = p == 6u ? 0u : p - 7u;
            e = BE[j];
            str_cpy(nm, BN[j], 12);
            return !((held >> e) & 1u) ? LS_OFF : ((perf_act >> e) & 1u) || ((PF_LOOPS >> e) & 1u) ? LS_HELD : LS_WAIT;
        }
        if (blk ? p >= NTRK : p >= PF_M1)
            return LS_OFF;
        if (blk) {
            str_cpy(nm, "MUTE T1", 12);
            nm[6] = (char)('1' + p);
            return (held >> e) & 1u ? LS_MUTE : LS_OFF;
        }
        str_cpy(nm, PF_NAME[p], 12);
        return !((perf_avail() >> e) & 1u) ? LS_DIM : !((held >> e) & 1u) ? LS_OFF :
               ((perf_act >> e) & 1u) || ((PF_MIDI >> e) & 1u) ? LS_HELD : LS_WAIT;
    }
    if (l == LAYER_GLO) {
        if (blk && p < NTRK) {
            str_cpy(nm, "MUTE T1", 12);
            nm[6] = (char)('1' + p);
            return trk[p].p[P_MUTE] ? LS_MUTE : p >= trk_live ? LS_DIM : LS_OFF;
        }
        if (blk && p < NTRK + 4u && glo_drums()) {
            str_cpy(nm, GLO_GROUP[p - NTRK], 12);
            return (dx_mute >> (p - NTRK)) & 1u ? LS_MUTE : LS_OFF;
        }
        if (blk)
            return LS_OFF;
        if (p < NTRK) {
            str_cpy(nm, "SOLO T1", 12);
            nm[6] = (char)('1' + p);
            return (perf_solo >> p) & 1u ? LS_HELD : p >= trk_live ? LS_DIM : LS_OFF;
        }
        str_cpy(nm, p == GLO_ALL ? "UNMUTE" : p == GLO_TAP ? "TAP" : p == GLO_UNDO ? "UNDO" : "", 12);
        return p == GLO_TAP && song.g[G_CLOCK] ? LS_DIM : p == GLO_UNDO && !undo_ok ? LS_DIM : LS_OFF;
    }
    if (l == LAYER_SCL) {                               /* every key its note: the root lit, the scale's plain, the rest dim */
        static const uint8_t WN[16] = {5, 7, 9, 11, 0, 2, 4, 5, 7, 9, 11, 0, 2, 4, 5, 7};
        uint32_t n = blk ? (uint32_t)(WN[PK_BLK[p] - 1u] + 1u) % 12u : WN[p], deg;
        nm[0] = blk ? B_NOTE[p] : W_NOTE[p];
        nm[1] = blk ? '#' : 0;
        nm[2] = 0;
        deg = (n + 12u - (uint32_t)clamp(TSEL->p[P_ROOT], 0, 11)) % 12u;
        return !deg ? LS_SEL : (scale_mask(TSEL) >> deg) & 1u ? LS_OFF : LS_DIM;
    }
    if (l == LAYER_EDIT) {
        if (blk) {
            if (p < NLANE && drum_track(TSEL)) {
                str_cpy(nm, drum_lane_abbr(TSEL, p), 12);
                return dx_lane_muted(p) ? LS_MUTE : LS_OFF;
            }
            return LS_OFF;
        }
        if (p > LY_RECALL)
            return LS_OFF;
        str_cpy(nm, p == LY_INIT ? (drum_track(TSEL) ? "INIT+KIT" : "INIT") : drum_track(TSEL) ? "RCL+KIT" : "RECALL", 12);
        return p == LY_RECALL ? (edit_recallable() ? LS_OFF : LS_DIM) : busy;   /* (V10.6z8: RECALL in a song too) */
    }
    if (l == LAYER_SEQ) {
        if (blk) {
            if (p < NLANE && drum_track(TSEL)) {
                str_cpy(nm, drum_lane_abbr(TSEL, p), 12);
                return p == ui.lane % NLANE ? LS_SEL : LS_OFF;
            }
            return LS_OFF;
        }
        if (p == TL_COPY || p == TL_PASTE) {
            str_cpy(nm,p == TL_COPY ? "COPY SEQ" : "PASTE SEQ",12);
            return p == TL_PASTE && (!clip.n || chain_busy()) ? LS_DIM : LS_OFF;
        }
        if (p == TL_STEP) {                             /* (0.9.2) G5 */
            str_cpy(nm, cur_page()->graph == GR_EVENTS ? "DEL REC" : "CLR STEP", 12);
            return busy;
        }
        if (!tl_cell(TSEL, p))
            return LS_OFF;
        str_cpy(nm, TL_NAME[p], 12);
        if (p >= TL_LCLR) {                             /* the lane's: "CLR BD" */
            str_cpy(nm + str_len(nm), " ", 2);
            str_cpy(nm + str_len(nm), drum_lane_abbr(TSEL, ui.lane), 12 - str_len(nm));
        }
        return busy;
    }
    if (l == LAYER_REC) {
        if (blk || p > 3u)
            return LS_OFF;
        if (p >= 2u) {
            str_cpy(nm, p == 2u ? "UNDO" : TSEL->p[P_HRQ] ? "Q ON" : "Q OFF", 12);
            return p == 2u ? (!undo_ok || chain_busy() ? LS_DIM : LS_OFF) : !TSEL->p[P_HRQ] ? LS_SEL : LS_OFF;
        }
        str_cpy(nm, p ? "CLICK " : "CLEAR", 12);
        if (p)
            str_cpy(nm + 6, rl_name(0, rp_get(RL_F[0])), 6);
        return p ? (rp_get(RL_F[0]) ? LS_SEL : LS_OFF) : chain_busy() || !rec_clearable() ? LS_DIM : LS_OFF;
    }
    if (l == LAYER_SAVE) {
        if (blk)
            return LS_OFF;
        if (p < SK_STORE) {
            str_cpy(nm, "PLAY A", 12);
            nm[5] = (char)('A' + p);
            return !project_used(p) ? LS_DIM : live.req == (int8_t)p ? LS_WAIT : live.cur == (int8_t)p ? LS_SEL : LS_OFF;
        }
        if (p < SK_STORE + 4u) {
            str_cpy(nm, "STORE A", 12);
            nm[6] = (char)('A' + p - SK_STORE);
            return (live.dirty >> (p - SK_STORE)) & 1u ? LS_WAIT : LS_OFF;
        }
        str_cpy(nm, p == SK_MODE ? (live.mode ? "SONG" : "LOOP") : p == SK_REC ? "SONG REC" : p == SK_PAGE ? "ARRANGE" :
                    p == SK_NAME ? "NAME" : "", 12);
        if (p == SK_NAME)
            return !ui.home && (cur_page()->graph == GR_USER || cur_page()->graph == GR_SLOTS) ? LS_OFF : LS_DIM;
        return p == SK_MODE && live.mode ? LS_SEL : p == SK_REC ? (live.srec == 2u ? LS_HELD : live.srec ? LS_WAIT : LS_OFF) : LS_OFF;
    }
    if (l == LAYER_MACRO && !blk && p < 2u) {            /* F3 DICE, G3 CLEAR */
        str_cpy(nm, p ? "CLEAR" : "DICE", 12);
        return busy;
    }
    return LS_OFF;
}
/* a key's fill and its name's ink by its state (named: n) */
static uint16_t pk_fill(uint32_t st, int blk, int n, uint16_t *ink)
{
    *ink = st == LS_DIM ? T_DIM : blk ? T_TEXT : T_TEXT;
    if (st == LS_HELD) { *ink = T_INK; return T_ACCENT; }
    if (st == LS_SEL || st == LS_WAIT) { *ink = T_INK; return st == LS_WAIT ? ux_mix(T_RAISE, T_THEME, 55) : T_THEME; }
    if (st == LS_MUTE) { *ink = T_INK; return T_KEY; }
    if (!n)
        return blk ? T_BG : ux_mix(T_BG, T_TEXT, 7);
    return blk ? ux_mix(T_BG, T_THEME, 34) : ux_mix(T_BG, T_TEXT, 17);
}
static void ly_piano(uint32_t l)
{
    int32_t h = (int32_t)graph_h() - 6, y = 3, bh = 54, i;
    char nm[12], nt[2] = {0, 0};
    for (i = 0; i < 16; i++) {                          /* the white keys */
        int32_t x0 = 4 + i * 232 / 16, w = 4 + (i + 1) * 232 / 16 - x0 - 1;
        uint16_t ink, fill = pk_fill(ly_keyname(l, 0, (uint32_t)i, nm), 0, nm[0] != 0, &ink);
        cv_rect(x0, y, w, h, fill);
        if (nm[0])
            cv_text_up(x0 + HALF_UP(w - AF_X_CAP_H) - AF_X_CAP_Y, y + h - 14, &AF_X, nm, ink, fill, h - 14 - bh - 6);
        nt[0] = W_NOTE[i];                              /* (its note, the letter) */
        GFX_HOOK_ALIGN(x0, 0, x0 + w, 0, AL_H, "layer key's note centred");
        cv_text_in(x0, y + h - 9 - AF_X_CAP_Y, w, &AF_X, nt, ink == T_INK ? ink : T_DIM, fill);
    }
    for (i = 0; i < 11; i++) {                          /* the black keys over them */
        int32_t x0 = 4 + PK_BLK[i] * 232 / 16 - 5;
        uint16_t ink, fill = pk_fill(ly_keyname(l, 1, (uint32_t)i, nm), 1, nm[0] != 0, &ink);
        cv_rect(x0 - 1, y, 12, bh + 1, T_BG);
        cv_rect(x0, y, 10, bh, fill);
        if (nm[0])
            cv_text_up(x0 + HALF_UP(10 - AF_X_CAP_H) - AF_X_CAP_Y, y + bh - 4, &AF_X, nm, fill == T_BG ? T_DIM : ink, fill, bh - 8);
    }
}
/* Draw REC controls separately so the pinned UI/audio boundary stays unchanged. */
static __attribute__((noinline)) void rec_columns(void)
{
    char val[12];
    uint32_t c;
    int32_t raw = TSEL->p[P_HRQ], q = raw * 100 / 127;
    fmt_int(val, q);
    for (c = 0; c < 4u; c++) {
        if (c == 1u)
            draw_column(c, "REC Q", raw ? val : "OFF", raw ? "%" : "", VAL(c), q * 10, ICON_NONE);
        else if (c == 0u) {
            uint32_t v = rp_get(RP_CLICK);
            draw_column(c, RL_LABEL[0], rl_name(0, v), "", v ? VAL(c) : T_DIM, v * 500, RL_ICON[0]);
        } else
            empty_column(c);
    }
}
static __attribute__((noinline)) void layer_cards(uint32_t l)
{
    char val[12];
    const char *unit;
    uint32_t c;
    if (l == LAYER_FX) {                                /* FILTER CRUSH THROW DEPTH */
        int32_t m = perf_k[0];
        fmt_int(val, m < 0 ? -m : m);
        draw_column(0, "FILTER", m ? val : "OFF", m < 0 ? "LP" : m > 0 ? "HP" : "", m ? VAL(0u) : T_DIM, (m + 100) * 5,
                    ICON_CUTOFF);
        fmt_int(val, perf_k[1]);
        draw_column(1, "CRUSH", perf_k[1] ? val : "OFF", perf_k[1] ? "%" : "", perf_k[1] ? VAL(1u) : T_DIM,
                    perf_k[1] * 10, ICON_BITS);
        fmt_int(val, perf_k[2]);
        draw_column(2, "THROW", perf_k[2] ? val : "OFF", perf_k[2] ? "%" : "", perf_k[2] ? VAL(2u) : T_DIM,
                    perf_k[2] * 10, ICON_DELAY);
        fmt_int(val, 100 - perf_k[3]);
        draw_column(3, "DEPTH", val, "%", VAL(3u), (100 - perf_k[3]) * 10, ICON_MIX);
    } else if (l == LAYER_GLO) {                        /* T1..T4 LEVEL (T5, T6 selected: T3..T6; the icon; muted: dim) */
        for (c = 0; c < 4u; c++) {
            uint32_t t = c + trk_base4();
            param_format(&TP[P_LEVEL], trk[t].p[P_LEVEL], val, &unit);
            draw_column(c, "LEVEL", val, unit, glo_sounding(t) ? VAL(c) : T_DIM, RATIO(&TP[P_LEVEL], trk[t].p[P_LEVEL]),
                        trk_icon(t, 1));
        }
    } else if (l == LAYER_SAVE) {                       /* the section, LOOP / SONG, SONG REC, the song row */
        uint32_t n = lys.song ? lys.song - 1u : song_cur;
        fmt_int(val, (int32_t)n + 1);
        draw_column(0, "SONG", val, n != song_cur ? "NEXT" : "", VAL(0u), (int32_t)(n * 1000u / (NSONG - 1u)),
                    ICON_X_SONG);
        val[0] = live.cur >= 0 ? (char)('A' + live.cur) : '-'; val[1] = 0;
        text_column(1, "SECT", val, live.cur >= 0 ? VAL(1u) : T_DIM);
        hot_text_column(2, "PLAY", live.mode ? "SONG" : "LOOP");
        text_column(3, "REC", live.srec == 2u ? "ON" : live.srec ? "ARM" : "OFF", live.srec ? VAL(3u) : T_DIM);
    } else if (l == LAYER_MACRO) {                      /* M1..M4, 0..100 % */
        static const char *const M[4] = {"M1", "M2", "M3", "M4"};
        for (c = 0; c < 4u; c++) {
            fmt_int(val, macro_v[c] * 100 / 127);
            draw_column(c, M[c], val, "%", macro_routes(c, 0, 0) ? VAL(c) : T_DIM, macro_v[c] * 1000 / 127, ICON_MIX);
        }
    } else if (l == LAYER_REC) {                        /* CLICK, then empty cards */
        rec_columns();
    } else if (l == LAYER_EDIT) {                       /* (0.9.9-seq2f) ENG, No. (PRESETS), FAV (ui_draw.c) */
        engine_columns();
    } else {                                            /* the page's four (SCL: LY_SCL) */
        uint8_t h = ui.home, pg = ui.page, nm[sizeof ui.nmem];
        memcpy(nm, ui.nmem, sizeof nm);
        ui.home = 0;
        ui.page = (uint8_t)page_first(LAYERS[l].fam);
        page_over = ly_page(l);
        draw_columns();
        page_over = 0;
        ui.home = h;
        ui.page = pg;
        memcpy(ui.nmem, nm, sizeof nm);
    }
}

/* (0.8.5) a layer as a lens, as the approved mock-up (MAP . KNOBS . STATE; ALGORITHM while it is held): MAP the keys'
 * map (the cells, no icons), KNOBS the four cards as gauges, STATE the key hints (the footer's until 0.8.4) and what is
 * on. The header "[FX] HELD"; the breadcrumb "LENTE . MAP . FX" */
static const khint_t *layer_foot(uint32_t l)
{
    return l == LAYER_FX && perf_latch_on ? FX_LATCH_FOOT : l == LAYER_MACRO && !layer_open() ? MAC_LATCH_FOOT : LAYERS[l].foot;
}
static void layer_knobs(uint32_t l)                     /* the four as gauges in their knobs' colours, the keys' words */
{
    const khint_t *ft = layer_foot(l);
    uint32_t c, k;
    char b[48];
    for (c = 0; c < 4u; c++) {
        int32_t cx = CARD_X(c) + 28;
        if (!ly_card[c].l[0]) {
            lz_arc(cx, 50, 25, 1, 0, T_LINE, T_LINE);
            continue;
        }
        lz_arc(cx, 50, 25, 4, ly_card[c].r < 0 ? 0 : ly_card[c].r, T_LINE,
               ly_card[c].r < 0 ? T_LINE : knob_col(c, ly_card[c].r, ly_card[c].b));   /* (0.9.5 JIANT: its heat) */
        GFX_HOOK_ALIGN(cx - 26, 0, cx + 26, 0, AL_H, "layer KNOBS gauge's words centred");
        cv_text_in(cx - 26, 44 - AF_X_CAP_Y, 52, &AF_X, ly_card[c].l, T_MID, T_SURF);
        cv_text_in(cx - 26, 54 - AF_X_CAP_Y, 52, &AF_X, ly_card[c].v, T_TEXT, T_SURF);
    }
    b[0] = 0;
    for (k = 0; k < 3u; k++)                            /* "PLAY RESTART . GLO DONE": the keycaps' words */
        if (ft[k].act) {
            if (b[0])
                str_cpy(b + str_len(b), " \xB7 ", sizeof b - str_len(b));
            str_cpy(b + str_len(b), KC[ft[k].key % KC_COUNT].label, sizeof b - str_len(b));
            str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
            str_cpy(b + str_len(b), ft[k].act, sizeof b - str_len(b));
        }
    GFX_HOOK_ALIGN(4, 0, 236, 0, AL_H, "layer KNOBS keys' words centred");
    cv_text_in(4, 100 - AF_X_CAP_Y, 232, &AF_X, b, T_DIM, T_SURF);
}
/* STATE: "GLO . HELD", then what is on as chips, two a row (on: a THEME stripe, TEXT; off: DIM) */
static uint32_t lst_n __attribute__((section(".pool")));
static void lst_chip(const char *a, const char *v, int on)
{
    int32_t x = 4 + (int32_t)(lst_n & 1u) * 117, y = 16 + (int32_t)(lst_n >> 1) * 15;
    uint16_t bg = ux.skin ? T_SURF : on ? T_TINT : T_RAISE;
    char b[24];
    if (lst_n >= 16u)
        return;
    lst_n++;
    str_cpy(b, a, sizeof b);
    if (v) {
        str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
        str_cpy(b + str_len(b), v, sizeof b - str_len(b));
    }
    if (!ux.skin)
        cv_rrect(x, y, 114, 13, 3, bg, T_SURF);
    if (on)
        cv_rect(x, y + 2, 2, 9, T_THEME);
    cv_clip_text(x + 6, y + 3 - AF_X_CAP_Y, &AF_X, b, on ? T_TEXT : T_DIM, bg, 104);
}
static __attribute__((noinline)) void layer_state(uint32_t l)
{
    static const char *const LN[LAYER_N] = {"", "FX", "GLO", "SCL", "EDIT", "SEQ", "REC", "SAVE", "LFO"};
    uint32_t k, e;
    char b[48], v[8];
    str_cpy(b, LN[l], sizeof b);
    str_cpy(b + str_len(b), l == LAYER_MACRO && !layer_open() ? " \xB7 LATCHED" : " \xB7 HELD", sizeof b - str_len(b));
    cv_text(6, 5 - AF_X_CAP_Y, &AF_X, b, T_THEME);
    if (l == LAYER_FX && fx_lock)
        cv_text_r(234, 5 - AF_X_CAP_Y, &AF_X, "[FX] LOCKED", T_ACCENT, T_SURF);
    lst_n = 0;
    if (l == LAYER_FX) {                                /* the lock, the tracks, the latch, the effects held */
        uint32_t held = perf_kill ? 0u : perf_held | perf_latched;
        static const char *const TG[3] = {"ALL", "SYN", "DRM"};
        lst_chip("LOCK", 0, fx_lock);
        lst_chip("TRACKS", TG[pfx_tgt % 3u], 1);
        lst_chip("FX LATCH", perf_latch_on ? "ON" : "OFF", perf_latch_on);
        for (e = 0; e < PF_M1; e++)
            lst_chip(PF_NAME[e], 0, (held >> e) & 1u);
    } else if (l == LAYER_GLO) {                        /* the tracks, the DRUM groups, UNDO */
        for (k = 0; k < NTRK; k++) {
            char t[3] = {'T', (char)('1' + k), 0};
            lst_chip(t, trk[k].p[P_MUTE] ? "MUTE" : k >= trk_live ? "ASLEEP" : "ON", !trk[k].p[P_MUTE] && k < trk_live);
        }
        for (k = 0; glo_drums() && k < 4u; k++)
            lst_chip(GLO_GROUP[k], (dx_mute >> k) & 1u ? "MUTE" : "ON", !((dx_mute >> k) & 1u));
        lst_chip("UNDO", undo_ok ? "READY" : "-", undo_ok);
    } else if (l == LAYER_MACRO) {                      /* the latch, each macro's routes */
        lst_chip("LATCH", mac_latch ? "ON" : "OFF", mac_latch);
        for (k = 0; k < 4u; k++) {
            char m[3] = {'M', (char)('1' + k), 0};
            uint32_t n = macro_routes(k, b, sizeof b);
            lst_chip(m, n ? b : "NO ROUTE", n != 0u);
        }
    } else if (l == LAYER_REC) {
        b[0] = 'T'; b[1] = (char)('1' + song.sel); b[2] = 0;
        lst_chip("ARMED", b, (song.rec >> song.sel) & 1u);
        lst_chip(b, rec_clearable() ? "SEQUENCE" : "EMPTY", rec_clearable());
        lst_chip("CLICK", rl_name(0, rp_get(RP_CLICK)), rp_get(RP_CLICK) != 0u);
        lst_chip("COUNT-IN", COUNTIN_N[rp_get(RP_COUNTIN) % 3u], rp_get(RP_COUNTIN) != 0u);
    } else if (l == LAYER_SAVE) {
        fmt_int(v, (int32_t)song_cur + 1);
        lst_chip("SONG", v, 1);
        v[0] = live.cur >= 0 ? (char)('A' + live.cur) : '-'; v[1] = 0;
        lst_chip("SECTION", v, live.cur >= 0);
        str_cpy(b, "CHAIN ", sizeof b);
        for (k = 0; k < lys.nqc && k < 8u; k++) { char c[2] = {(char)('A' + lys.qc[k]), 0}; str_cpy(b + str_len(b), c, sizeof b - str_len(b)); }
        lst_chip(lys.nqc >= 2u ? b : "CHAIN -", 0, lys.nqc >= 2u);
        lst_chip(live.mode ? "SONG" : "LOOP", 0, 1);
        lst_chip("SONG REC", live.srec == 2u ? "ON" : live.srec ? "ARM" : "OFF", live.srec != 0u);
        if (chain.running) {
            fmt_int(v, (int32_t)chain.row + 1);
            lst_chip("ROW", v, 1);
            fmt_int(v, chain.remaining);
            lst_chip("BARS LEFT", v, 1);
        }
    } else {                                            /* the four knobs' values; the track's sound; DRUM's sounds */
        for (k = 0; k < 4u; k++)
            if (ly_card[k].l[0])
                lst_chip(ly_card[k].l, ly_card[k].v, 1);
        b[0] = 'T'; b[1] = (char)('1' + song.sel); b[2] = ' ';
        str_cpy(b + 3, trk_engp(TSEL)->name, sizeof b - 3);
        lst_chip(b, 0, 1);
        sound_name(TSEL, b);
        if (b[0])
            lst_chip(b, 0, 1);
        for (k = 0; (l == LAYER_EDIT || l == LAYER_SEQ) && drum_track(TSEL) && k < NLANE; k++)
            lst_chip(drum_lane_abbr(TSEL, k), l == LAYER_SEQ ? (k == ui.lane % NLANE ? "LANE" : 0) : dx_lane_muted(k) ? "MUTE" : "ON",
                     l == LAYER_SEQ ? k == ui.lane % NLANE : !dx_lane_muted(k));
    }
}
static __attribute__((noinline)) void draw_layer(void)
{
    uint32_t l = ui.layer % LAYER_N, sig = l * 7919u + ux.gen * 977u;
    layer_cards(l);
    if (l == LAYER_FX)
        sig += (perf_kill ? 0u : perf_held | perf_latched) * 31u + perf_latch_on * 11u + perf_act * 131u + perf_avail() * 7u;
    else if (l == LAYER_GLO)
        sig += perf_solo * 31u + (uint32_t)song.g[G_CLOCK] * 5u +
               (uint32_t)(trk[0].p[P_MUTE] | trk[1].p[P_MUTE] << 1 | trk[2].p[P_MUTE] << 2 | trk[3].p[P_MUTE] << 3 |
                          trk[4].p[P_MUTE] << 4 | trk[5].p[P_MUTE] << 5) * 131u +
               ((uint32_t)dx_mute * 2u + (uint32_t)glo_drums()) * 4099u;
    else if (l == LAYER_SCL)
        sig += (uint32_t)TSEL->p[P_SCALE] * 31u;
    else if (l == LAYER_REC)
        sig += (uint32_t)ui_rec_prefs * 31u + (uint32_t)chain_busy() * 3u + (uint32_t)rec_clearable() * 5u + song.sel * 977u +
               (uint32_t)TSEL->p[P_HRQ] * 131u + (uint32_t)undo_ok * 4099u;
    else if (l == LAYER_SAVE)
        sig += (uint32_t)(live.cur + 1) * 31u + (uint32_t)(live.req + 1) * 131u + live.srec * 7u + live.mode * 11u +
               live.dirty * 977u + lys.nqc * 4099u + (chain.running ? chain.row * 13u + chain.remaining * 17u : 0u) +
               (lys.song * 8u + song_cur) * 65537u;
    else if (l == LAYER_MACRO)
        sig += macro_sig();
    else if (l == LAYER_SEQ)
        sig += (uint32_t)drum_track(TSEL) * 31u + ui.lane * 5u + (uint32_t)chain_busy() * 3u + (uint32_t)TSEL->p[P_E0] * 131u + song.sel * 977u;
    else
        sig += snd_id() * 31u + (uint32_t)preset_favorite() * 13u + (uint32_t)chain_busy() * 3u + (uint32_t)(project_recall_slot() + 1) * 5u + dx_mute * 7919u +
               (uint32_t)drum_track(TSEL) * 11u + up_gen * 101u;
    sig += ui.lylv * 0x9E3779B1u + (ui.lylv == 1u ? str_hash(7u, ly_card[0].v) + str_hash(9u, ly_card[1].v) * 3u +
                                     str_hash(11u, ly_card[2].v) * 5u + str_hash(13u, ly_card[3].v) * 7u : 0u);
    if (ui.force || sig != ui.layer_sig) {
        ui.layer_sig = sig;
        cv_begin(240, graph_h(), T_BG);
        if (!ux.skin)                                   /* (0.8.5: the lens's panel; 0.8.9 COLORS rounded, BOTTOM / HUD none) */
            cv_rrect(4, 0, 232, (int32_t)graph_h(), 6, T_SURF, T_BG);
        cv_bg = T_SURF;
        if (ui.lylv % 3u == 1u)                         /* KNOBS: the four as gauges */
            layer_knobs(l);
        else if (ui.lylv % 3u == 2u)                    /* STATE: what is on */
            layer_state(l);
        else                                            /* MAP: the keyboard */
            ly_piano(l);
        cv_blit(0, graph_y());
    }
    draw_tabs();                                        /* (0.8.5: MAP . KNOBS . STATE, the breadcrumb) */
    draw_foot();
}

#include "ui_live.c"                                    /* (p2, main 0.9.11) GLO > LIVE: MANO, RIELES */



