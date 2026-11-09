/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Felucca UI input: LEDs, knobs and buttons, SEQ step entry, panel setup. */
#include "ui_name.c"                                    /* NAME: naming user presets and projects */
/* ----------------------------------------------------------- LEDs --- */
/* The LED picture is built off-line and copied one byte per column: clearing
 * and relighting would let the 10 kHz scan catch the dark gap and flicker. */
static uint8_t led_pos[41];                        /* (col << 3) | row bit, 0xFF = none */

static void led_pos_init(void)
{
    uint32_t id, p, r;
    for (id = 0; id < 41u; id++) {
        led_pos[id] = 0xFF;
        for (p = 0; p < FM1_NCOL; p++)
            for (r = 1; r < 5u; r++)
                if (FM1_KEYMAP[r][p] == (int8_t)id)
                    led_pos[id] = (uint8_t)((p << 3) | r);
    }
}

static void led_put(uint8_t *nl, uint32_t id, int on)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF && on)
        nl[q >> 3] |= (uint8_t)(1u << (q & 7u));
}

/* PLAY's second, green LED: not in the key matrix (no key there), found on the hardware at column 8, row PA9 (bit 1) */
#define LED_PLAY_GREEN ((8u << 3) | 1u)
static void led_clear(uint8_t *nl, uint32_t id)
{
    uint8_t q = led_pos[id];
    if (q != 0xFF)
        nl[q >> 3] &= (uint8_t)~(1u << (q & 7u));
}

static const uint8_t FAM_BTN[FAM_COUNT] = {B_HOME, B_ENV, B_LFO, B_FX, B_SCL, B_EDIT, B_GLO, B_SAVE,
                                           B_ARP, B_SEQ, B_GLO};   /* GLO: mixer + global settings; REC is transport */

static uint32_t cur_fam(void) { return ui.home ? FAM_HOME : cur_page()->fam; }

/* (0.9.2) a section's button held + SELECT (ui_lens.c nav_jump, the dice, the tempo): the button let go stays there (its
 * tap, its layer: not now) */
static uint8_t ly_used;                                  /* the held layer's knobs or keys used: PRESETS stays its */
static int song_open(uint32_t n);                         /* (song_main.c: SAVE > PROJECT's KNOB 1) */
static uint8_t pj_btn = 0xFFu;                           /* the button a SELECT combo was made with: no layer while held */
static uint32_t held_family(void)
{
    uint32_t f;
    for (f = FAM_ENV; f < FAM_COUNT; f++)
        if (f != FAM_TRK && (fm1_in.buttons >> panel.btn[FAM_BTN[f]]) & 1u)
            return f;
    return FAM_COUNT;
}
/* (0.6.67) a button whose SELECT combo was used: no next page when let go, no tap, its layer shut until let go */
static void btn_used(uint32_t b)
{
    ui.pg_down &= (uint16_t)~(1u << panel.btn[b]);
    if (b == B_SEQ) ui.seq_t0 |= 2u;
    if (b == B_SAVE) ui.save_t0 |= 2u;
    pj_btn = (uint8_t)b;
}
/* every pass: while the button of a page jump is held its layer stays shut (no map at HOLD, no tap when let go), its
 * page taps too; let go: free again */
static void pj_hold(void)
{
    if (pj_btn == 0xFFu)
        return;
    if (!((fm1_in.buttons >> panel.btn[pj_btn]) & 1u)) {
        pj_btn = 0xFFu;
        return;
    }
    ui.pg_down &= (uint16_t)~(1u << panel.btn[pj_btn]);
    if (pj_btn == B_SEQ) ui.seq_t0 |= 2u;
    if (pj_btn == B_SAVE) ui.save_t0 |= 2u;
    if (ui.ly)
        ui.ly_t0 = (ui.ly_t0 & ~2u) | 8u;               /* (ui_layer.c: LY_OPEN off, LY_DEAD) */
}

static int layer_set_open(void);                       /* (ui_layer.c) */
/* the OCT LEDs, bit 0 OCT- lit, bit 1 OCT+ lit, OCT_BREATH OCT+ breathing (can be pressed: dark .. ~60 %,
 * hal/fm1_input.h fm1_led_breath, #119). In the dialogs and on action pages OCT- (back) is lit and OCT+ breathes
 * while it would do something; the menu: on a value row OCT- and OCT+ both breathe while that way has a value to go to,
 * on CALIBRATION / ABOUT OCT+ breathing alone (opens), in ABOUT OCT- lit (back); elsewhere the octave shift */
#define OCT_BREATH 4u
#define OCT_BREATH_DN 8u                                /* OCT- breathing (the menu: the previous value) */
static uint32_t oct_leds(void)
{
    if (name_on() && !ui.confirm && !ui.menu)           /* NAME: OCT- cancels, OCT+ (breathing) writes */
        return 1u | OCT_BREATH;
    if (layer_set_open())                               /* a SET layer: OCT± do nothing (TONIC: no put back); */
        return ui.layer == LAYER_FX ? 1u : 0u;          /* FX LATCH: OCT- all off, lit */
    if (ui.menu && !ui.confirm)
        return ui.menu == 3u || (ui.menu_sel == MI_BT && bt_prox_ok()) ? (ui.menu == 3u ? OCT_BREATH_DN : 1u) | OCT_BREATH :
               ui.menu == 2u ? 1u :                    /* (ABOUT: OCT- back, lit; V10.14 PROX's page: OCT- FAR, OCT+ NEAR;
                                                        * BLUETOOTH ON: OCT- OFF lit, OCT+ opens PROX) */
               ui.menu_sel >= MI_VALUES ? OCT_BREATH :  /* CALIBRATION / ABOUT: OCT+ opens */
               (menu_step(ui.menu_sel, -1) != menu_get(ui.menu_sel) ? OCT_BREATH_DN : 0u) |   /* a value: each */
               (menu_step(ui.menu_sel, 1) != menu_get(ui.menu_sel) ? OCT_BREATH : 0u);         /* way it can go */
    if (ui.confirm || act_cols())
        return 1u | (ui.confirm || act_ready() ? OCT_BREATH : 0u);
    if (live_page())                                    /* (p2) LIVE: OCT- RETURN lit, OCT+ COMMIT breathing, while */
        return lv.busy ? 1u | OCT_BREATH : 0u;          /* a track is modified */
    return (song.octave < 0 ? 1u : 0u) | (song.octave > 0 ? 2u : 0u);
}

/* the key LEDs on the grid, bit k = key k: the white keys show where the selected lane hits on the page
 * shown (its accents while ACC is held), the step playing inverted (a light walks over them); the black keys
 * the lane selected, ACC while held, the page keys while there is more than one page */
static uint32_t grid_leds(void)
{
    const track_t *t = TSEL;
    uint32_t k, m = 0, len = (uint32_t)t->p[P_SLEN], b = 1u << ui.lane, acc = (uint32_t)black_held(GK_ACC);
    uint32_t ph = song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank ? seq_src(t, t->seq_idx) % 16u : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), on;
        if (!key_black(k)) {
            uint32_t i = ui.bank * 16u + p;
            on = i < len && ((acc ? step_accents_of(TSEL,&seq_steps(t)[i]) : step_lanes_of(TSEL,&seq_steps(t)[i])) & b) != 0u;
            on ^= (uint32_t)(p == ph);
        } else {
            on = p < NLANE ? p == ui.lane : p == GK_ACC ? acc : len > 16u;
        }
        m |= on << k;
    }
    return m;
}

/* the DRUM grid's beats, bit k = key k (ui_leds: a steady brighter glow than the other steps', hal/fm1_input.h
 * fm1_led_mid): the first step of each group of four on the page shown, steps 1 5 9 13 of 16 ("5111 5111 5111 5111"),
 * within LEN, where grid_leds shows no hit (the lane's, its accents while ACC is held) and not the playhead. Always
 * groups of four from the pattern's first step, whatever LEN or DIV (a 3/4 or a DIV 1/8 pattern too): the steps are
 * counted, not the time */
static uint32_t grid_beats(void)
{
    const track_t *t = TSEL;
    uint32_t k, m = 0, len = (uint32_t)t->p[P_SLEN], b = 1u << ui.lane, acc = (uint32_t)black_held(GK_ACC);
    uint32_t ph = song.playing && seq_src(t, t->seq_idx) < len && seq_src(t, t->seq_idx) / 16u == ui.bank ? seq_src(t, t->seq_idx) % 16u : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), i = ui.bank * 16u + p;
        if (key_black(k) || p % 4u || i >= len || p == ph)
            continue;
        m |= (uint32_t)(((acc ? step_accents_of(TSEL,&seq_steps(t)[i]) : step_lanes_of(TSEL,&seq_steps(t)[i])) & b) == 0u) << k;
    }
    return m;
}

/* the DRUM grid's keys that do something, bit k = key k (they glow with the idle LEDs, ui_leds): the page's steps
 * within LEN, the lane keys, ACC, the page keys while there is more than one page */
static uint32_t grid_glow(void)
{
    uint32_t k, m = 0, len = (uint32_t)TSEL->p[P_SLEN];
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        m |= (uint32_t)(!key_black(k) ? ui.bank * 16u + p < len : p < NLANE || p == GK_ACC || len > 16u) << k;
    }
    return m;
}

/* an ARP playing on any part flashes the ARP button on the beat, the bar's first beat longer: 1 lit, 0 dark,
 * 2 no ARP playing */
static uint32_t arp_led(void)
{
    uint32_t k, on = 0, b = beat_samples();
    for (k = 0; k < NPART; k++)
        on |= trk[k].p[P_AMODE] && trk[k].nheld;
    return !on ? 2u : beat_pos < (beat_n ? b / 6u : b / 2u);
}

/* Discussion #81: the keys of the notes MIDI IN (USB and TRS, routed by ROUT) holds on track t, bit k = key k: as
 * play_leds, where the keys play that note at the octave now, the lowest key that gives it; a note no key plays is
 * not shown. Read from midi_control.c's own state (midi_note_held: the notes each channel holds for the track, the
 * pedal's too, and the tones of MIDI chords): no state here; nothing to do while MIDI holds none of the track's */
static uint32_t midi_leds(const track_t *t)
{
    uint32_t k, note, m = 0, seen[4] = {0, 0, 0, 0};
    if (!midi_owners[trk_index(t)])
        return 0;
    for (k = 0; k < 27u; k++) {
        note = kb_map(t, k);
        if (note > 127u || ((seen[note >> 5] >> (note & 31u)) & 1u))
            continue;                                   /* (silent, or a lower key gives it) */
        seen[note >> 5] |= 1u << (note & 31u);
        m |= (uint32_t)midi_note_held(t, note) << k;
    }
    return m;
}

/* the keys of the notes the selected track's sequencer and ARP sound now (#38), bit k = key k: where the keys
 * play that note (kb_map: the octave, TRN, QNT, an engine's own map), the lowest key that gives it (QNT SNAP
 * rounds the keys above down onto it); a note no key plays is not shown. A snapshot of the ISR's seq_notes /
 * arp_note (arp_ch): no state of its own, nothing to do while nothing sounds */
static uint32_t play_leds(void)
{
    const track_t *t = TSEL;
    uint8_t s[4 + NLANE + 4];
    uint32_t n = t->seq_n < 4u + NLANE ? t->seq_n : 4u + NLANE, i, k, note, used = 0, m = 0, hit;
    for (i = 0; i < n; i++)
        s[i] = t->seq_notes[i];
    if (t->arp_note)
        s[n++] = t->arp_note;
    for (i = 0; i < 3u; i++)                            /* (the ARP's CHORD) */
        if (t->arp_ch[i])
            s[n++] = t->arp_ch[i];
    for (k = 0; n && k < 27u; k++) {
        note = kb_map(t, k);
        for (i = 0, hit = 0; i < n; i++)
            if (s[i] == note && !((used >> i) & 1u)) {
                used |= 1u << i;
                hit = 1;
            }
        m |= hit << k;
    }
    return m | midi_leds(t);
}

/* (0.6.83) the keys of every note the selected track's voices hold now, bit k = key k: a chord of SCL > CHORD, LATCH, a
 * chord gliding, MIDI, all of what sounds is shown (its released tails are not), as play_leds: the lowest key that
 * gives the note at the octave now; a kit's lanes not (its keys are its sounds). Read from the voices: no state */
static uint32_t voice_leds(void)
{
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t i, k, m = 0, seen[4] = {0, 0, 0, 0}, have[4] = {0, 0, 0, 0}, any = 0, note;
    if (e->oneshot || (e->keys && e->keys(t, 0) >= 0))
        return 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note < 128u) {
            have[t->v[i].note >> 5] |= 1u << (t->v[i].note & 31u);
            any = 1;
        }
    for (k = 0; any && k < 27u; k++) {
        note = kb_map(t, k);
        if (note > 127u || ((seen[note >> 5] >> (note & 31u)) & 1u))
            continue;                                   /* (silent, or a lower key gives it) */
        seen[note >> 5] |= 1u << (note & 31u);
        m |= ((have[note >> 5] >> (note & 31u)) & 1u) << k;
    }
    return m;
}

/* Discussion #89: on SEQ > STEP (the piano roll; the DRUM grid has its own map) while stopped, the keys of the
 * notes stored in the cursor step, bit k = key k: as play_leds, where the keys play that note now (kb_map: the
 * octave, TRN, QNT), the lowest key that gives it; a note no key plays is not shown, nor a REST step's. Playing,
 * the keys show what plays (play_leds) instead. Read from the step: no state of its own */
static uint32_t step_leds(void)
{
    const track_t *t = TSEL;
    const step_t *st;
    uint32_t i, k, note, used = 0, m = 0;
    if (ui.home || ui.menu || ui.confirm || !song.seq_mode || cur_page()->graph != GR_ROLL || song.playing ||
        chain_busy() || ui.cursor >= NSTEP)
        return 0;
    st = &t->step[ui.cursor];
    if (!st->n || st->time == ST_REST)
        return 0;
    for (k = 0; k < 27u; k++) {
        note = kb_map(t, k);
        for (i = 0; i < st->n && i < 4u; i++)
            if (st->note[i] == note && !((used >> i) & 1u)) {
                used |= 1u << i;
                m |= 1u << k;
            }
    }
    return m;
}

/* 1: the keys show a map of their own (NAME, a layer's map: SCL's scale, FX; the DRUM grid), lit or
 * dark; 0: the keys held and the notes playing, over the idle glow */
static int keys_own(void)
{
    return (name_on() && !ui.menu) || ui.layer || grid_on() || live_mano();
}

/* Discussion #127 (1.2): MENU > SCALE LEDS ON, the keys of the selected track's scale (ROOT, SCALE), bit k = key k, as
 * the keys play now: the note a key gives with TRN (QNT SNAP / SEQ: the key's own note; the others round down onto
 * the scale); QNT WHITE every white key (they walk the scale; the black ones are silent). *root: the keys of the
 * scale's root. None where the engine maps the keys itself (DRUM's GM map, the slices): 0 there, and OFF */
static uint32_t scale_leds(uint32_t *root)
{
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t k, m = 0, r = 0, mask = scale_mask(t), note;
    int32_t pc;
    *root = 0;
    if (!(ui_rec_prefs & (PREF_SCALE_LEDS >> 8)))
        return 0;
    for (k = 0; k < 27u; k++) {
        if (e->keys && e->keys(t, k) >= 0)
            continue;
        if (t->p[P_QUANT] == QN_WHITE) {
            if ((note = kb_map(t, k)) > 127u)
                continue;                               /* (a black key: silent) */
            pc = (int32_t)note + t->p[P_TRANS];         /* (0.6.81: the note it sounds: TRANS added as it sounds,
                                                         * voice.c trk_trans; WHITE walks the scale before it) */
        } else {
            pc = 53 + (int32_t)k + t->p[P_TRANS];
        }
        pc = ((pc - t->p[P_ROOT]) % 12 + 12) % 12;
        m |= ((mask >> pc) & 1u) << k;
        r |= (uint32_t)(pc == 0) << k;
    }
    *root = r & m;
    return m;
}

/* the key LEDs, bit k = key k: NAME's keys, the layer's map, the DRUM grid, else the keys held and the notes
 * the selected track's sequencer, ARP and MIDI IN play, on STEP while stopped the cursor step's notes (#89), and
 * (0.6.83) every note its voices hold (a CHORD's whole chord) */
static uint32_t key_leds(uint32_t *br)                 /* (*br: a layer's keys that breathe, layer_leds) */
{
    uint32_t c;
    if (br)
        *br = 0;
    c = name_on() && !ui.menu ? name_leds() : ui.layer ? layer_leds(br) : grid_on() ? grid_leds() : live_mano() ? live_leds() :
                 (fm1_in.notes & ~kb_layer) | play_leds() | step_leds() | voice_leds();
    return c;
}

/* The LEDs: lit = active (the page's family, PLAY / REC running, the keys held or playing, a map's keys), the ARP
 * beat flashes, the ones that can be pressed breathe (#119: the open layer's button, OCT+ when it would act, a
 * layer's keys; dark .. ~60 % of lit and back, hal/fm1_input.h fm1_led_breath, in every mode: never a hard blink in
 * a dark room; DIM LO up to ~30 %, DIM HI OFF INV ~60 %; with LEDS OFF it is the only glow; INV leaves it as it is,
 * between the dark active ones and the lit idle ones), every other button and key glows dim (#35: the
 * buttons of the black FM-1 can be found in the dark; hal/fm1_input.h fm1_led_dim, a short pulse each frame).
 * MENU > LEDS: DIM HI (default) that glow, DIM LO a darker one (fm1_led_dim_level), OFF no glow (as 1.0); INV
 * turns it around, as the stock firmware: the idle ones fully lit, the active ones dark, no glow (the ARP beat:
 * lit / dark). The keys' own maps (keys_own) stay lit or dark in every mode: their dark keys read as dark; only the
 * DRUM grid's keys that do something (grid_glow) glow under it, so STEP on a DRUM track is never a dark
 * keyboard (the steps of an empty pattern); on it the first step of each four (grid_beats: 1 5 9 13 of a page, where
 * no hit shows) glows steadier and brighter, so a bar's beats can be counted: DIM HI / DIM LO the mid level
 * (hal/fm1_input.h fm1_led_mid: lit 1 frame in 8, DIM LO in 12, over the glow), OFF and INV the glow alone (no
 * other key glows there). 1.2, MENU > SCALE LEDS ON (scale_leds; not over a map of the keys' own, nor on a DRUM or
 * SLICE track): the keys of the selected track's scale glow and the others are dark, the root's at the mid level, in
 * DIM HI, DIM LO and OFF alike (asked for: OFF only takes the other glow away); INV turns it around as it does the
 * rest: the root's keys lit, the scale's at the mid level, the others glowing; the keys held and playing as ever
 * (lit, INV dark). Each picture is built off-line and copied one byte per column, the
 * glow first: an LED going from lit to dim never has a dark frame */
static __attribute__((noinline)) void ui_leds(void)
{
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0}, own[FM1_NCOL] = {0}, og[FM1_NCOL] = {0}, nb[FM1_NCOL] = {0};
    uint8_t nm[FM1_NCOL] = {0}, sg[FM1_NCOL] = {0}, sm[FM1_NCOL] = {0};
    uint32_t k, c, g, br, beat, sc, sr = 0;
    uint32_t fam = cur_fam(), mode = settings_leds;
    int keys_map = keys_own();
    static uint8_t ready;
    if (fm1_led_anim_on())                              /* the power-on sweep has them (main.c boot_leds) */
        return;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    if (!ui.layer || FAM_BTN[fam] != layer_btn())
        led_put(nl, panel.btn[FAM_BTN[fam]], 1);
    if ((k = arp_led()) != 2u && (!ui.layer || layer_btn() != B_ARP))
        led_put(nl, panel.btn[B_ARP], FAM_BTN[fam] == B_ARP ? !k : (int)k);   /* (on ARP's page: dark flashes) */
    if (!ui.layer && (perf_latched || perf_k[0] || perf_k[1] || perf_k[2] || perf_k[3]))
        led_put(nl, panel.btn[B_FX], 1);                /* FX LATCH: lit while an effect or a macro is on */
    if (ui.layer)                                       /* the layer's button breathes while its map is up */
        led_put(nb, panel.btn[layer_btn()], 1);
    led_put(nl, panel.btn[B_REC], song.rec != 0u && ui.layer != LAYER_REC);   /* (its layer: it breathes) */
    k = oct_leds();
    led_put(nl, panel.btn[B_OCTDN], (int)(k & 1u));
    led_put(nl, panel.btn[B_OCTUP], (int)((k >> 1) & 1u));
    led_put(nb, panel.btn[B_OCTUP], (int)(k & OCT_BREATH));
    led_put(nb, panel.btn[B_OCTDN], (k & OCT_BREATH_DN) != 0u);
    c = key_leds(&br);
    g = !keys_map ? 0u : grid_on() && !ui.layer && !name_on() ? grid_glow() : 0u;   /* (key_leds: the grid's map) */
    beat = g ? grid_beats() & ~c : 0u;                  /* (the grid's empty beat starts: brighter than the glow) */
    sc = keys_map ? 0u : scale_leds(&sr);               /* SCALE LEDS: the scale's keys glow, the root's brighter */
    for (k = 0; k < 27u; k++) {
        uint32_t in = (sc >> k) & 1u, rt = (sr >> k) & 1u, idle = !((c >> k) & 1u);
        led_put(nm, 14u + k, (int)((beat >> k) & 1u));
        led_put(keys_map ? own : nl, 14u + k, (int)((c >> k) & 1u));
        led_put(keys_map ? og : nd, 14u + k, keys_map ? (int)((g >> k) & 1u) : !sc || (mode == LEDS_INV && rt));
        led_put(nb, 14u + k, (int)((br >> k) & 1u));
        if (!sc)
            continue;
        led_put(sg, 14u + k, mode == LEDS_INV ? !in && idle : (int)in);   /* (INV: the active ones dark) */
        led_put(sm, 14u + k, mode == LEDS_INV ? in && !rt && idle : (int)rt);
    }
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    if (song.playing || seq_counting())                 /* playing: PLAY's green, its own LED dark in every mode */
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++) {
        if (mode == LEDS_INV)                           /* INV: the active ones dark, the rest lit */
            nl[c] = (uint8_t)(nd[c] & ~nl[c]);
        nd[c] = mode == LEDS_DIM || mode == LEDS_DIM_LO ? (uint8_t)(nd[c] | og[c]) : nm[c];   /* OFF, INV: no glow but */
        if (mode != LEDS_DIM && mode != LEDS_DIM_LO)    /* the grid's beats (the one level above dark there); DIM HI */
            nm[c] = 0;                                  /* / LO: the beats at the mid level over the glow */
        nd[c] |= sg[c];                                 /* SCALE LEDS: in every mode (INV: inverted above) */
        nm[c] |= sm[c];
        nm[c] &= (uint8_t)~(nb[c] | own[c]);
        nl[c] |= own[c];
        nl[c] &= (uint8_t)~nb[c];                       /* breathing: neither lit (INV's idle) nor the glow */
        nd[c] &= (uint8_t)~nb[c];
    }
    if (song.playing || (seq_counting() && cin_pos < click_beat_len() / 4u))   /* counting in: green on each beat */
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(mode == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {                    /* (0.6.96) DIM HI: the idle glow at the mid level (the pulse
                                                         * alone was too dim to find the keys: ~1/30 of lit; now ~15 %),
                                                         * the beats and the scale's root a level above (fm1_led_hi) */
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = nb[c];
        fm1_led_hi[c] = mode == LEDS_DIM ? nm[c] : 0u;
        fm1_led_mid[c] = mode == LEDS_DIM ? (uint8_t)((nd[c] | nm[c]) & ~(nb[c] | nl[c])) : nm[c];
    }
    for (c = 0; c < FM1_NCOL; c++)
        fm1_led[c] = nl[c];
}

/* ---------------------------------------------------------- input --- */
/* Predictable hardware response (#23): each decoded detent is one value step; a fast turn keeps its full signed
 * detent count. KNOB ACCEL (#52; JIANT: ON by default, MENU > KNOB ACCEL OFF turns it off) multiplies a fast turn of a
 * wide value (range > 16, not a list of names; the FX and GLO layers' knobs too, #126): the faster the turn the
 * wider the sweep, a slow turn is fine tuning, one step per detent. The main loop reads the knobs many times a frame
 * (main.c), so a read holds one detent as a rule: the speed is the time per detent, ACC_RATE / ms -> 40 ms x2,
 * 27 ms x3, 20 ms x4, 10 ms x8, 5 ms x16; capped by the range: x2 up to 32, x4 up to 64, x8 up to 100, x16 above (a
 * flick across 0..127 in a dozen detents). Only the longer of this read's and the previous read's time counts, and
 * only while the turn goes on (both under ACC_GAP ms) in one direction: a slow turn, the first two detents of a turn,
 * a single quick detent (a bounce) and a reversal are one step per detent, and the sign is always the detents'.
 * ui.enc_t[role]: bits 0..23 the ms of its last read, bit 24 its direction (+1), 25..31 its ms per detent (127 slow) */
#define ACC_GAP 60u
#define ACC_RATE 80u
static int32_t accel(uint32_t role, int32_t s, int32_t range)
{
    uint32_t now = fm1_ms & 0xFFFFFFu, st = ui.enc_t[role], up = s > 0, pi = st >> 25, a, i, m = 1;
    if ((ui_prefs & PREF_ACCEL_OFF) || range <= 16 || !s)
        return s;
    a = (uint32_t)(s < 0 ? -s : s);
    i = ((now - st) & 0xFFFFFFu) / a;                   /* ms per detent of this read */
    if (!st || ((st >> 24) & 1u) != up || i >= ACC_GAP)
        i = 127u;                                       /* a new turn, or reversed */
    else if (pi < ACC_GAP) {
        m = ACC_RATE / (i > pi ? i : pi ? pi : 1u);
        a = range > 100 ? 16u : range > 64 ? 8u : range > 32 ? 4u : 2u;   /* (the cap) */
        m = m < 1u ? 1u : m > a ? a : m;
    }
    ui.enc_t[role] = now | up << 24 | (i ? i : 1u) << 25;
    return s * (int32_t)m;
}

/* (0.9.18) the LFO's MODE (mode 1) or PHASE turned to v (params.c ID_LMODE, ID_LPHS): FREE stores PHASE -1, keeping
 * TRIG's phase in lfo_ph_mem for TRIG again; FREE begun fades the LFO in from 0 (its FADE, once: it is turned on) */
static void lfo_view_put(track_t *t, int mode, int32_t v)
{
    uint32_t k = (uint32_t)(t - trk) % NTRK;
    if (!mode) {
        if (t->p[P_LPHASE] >= 0)
            t->p[P_LPHASE] = (int16_t)v;
        else
            lfo_ph_mem[k] = (uint8_t)v;
    } else if (v && t->p[P_LPHASE] >= 0) {
        lfo_ph_mem[k] = (uint8_t)t->p[P_LPHASE];
        t->p[P_LPHASE] = -1;
        lfo_fade_restart(1u << k);
    } else if (!v && t->p[P_LPHASE] < 0)
        t->p[P_LPHASE] = lfo_ph_mem[k];
}

/* MIXER page: KNOB 1 LEVEL, 2 PAN, 3 REV send of the selected track (the track itself is ALGORITHM's, on every page),
 * (0.9.1) KNOB 4 the master DJ FILTER (G_DJF: was the track's MUTE, which the GLO layer's black keys latch, as before).
 * Pattern length stays on SEQ. */
/* a knob turned parameter id of track t: recorded (AUTOMATION), and picked by MIDI LEARN (p1, main 0.9.12, midi_learn.c) */
static void knob_cap(track_t *t, uint32_t id)
{
    motion_capture(t, id, t->p[id]);
    ml_knob(t, id);
    mo_knob(trk_index(t), id);                         /* (V10.6z3) MIDI OUT ALL: the move as an NRPN (seq.c) */
}
/* (V10.23, a tester: "the STRING modes don't take until a knob goes up") STRING's EXCITE into or out of FDBK: EDIT 2's
 * four knobs change meaning (STRETCH BODY SIZE BOW / GAIN DIST SPEAKR BLOOM), so each side keeps its own four: the
 * other side's come back as they were left (the first time, their defaults: FDBK's GAIN no longer starts at STRETCH's 0) */
static void excite_turn(track_t *t, const int16_t *vp, int32_t old)
{
    static int16_t side[NTRK][2][4];
    static uint8_t had[NTRK];
    uint32_t k = (uint32_t)(t - trk) % NTRK, i, was = old == 5, now = t->p[P_E0] == 5;   /* (eng_string.c SX_FDBK) */
    if (vp == &t->p[P_E0] && t->eng_req % NENGINES == 0u &&   /* (V10.39) ANALOG into / out of SAW3 PLS3: NOISE <-> INT 3 */
        (old == 7 || old == 8) != AN_OSC3(t->p)) {      /* (the knob changes meaning: from 0, no noise, unison) */
        t->p[P_E3] = 0;
        knob_cap(t, P_E3);
        return;
    }
    if (vp != &t->p[P_E0] || t->eng_req % NENGINES != ENGI_STRING || was == now)
        return;
    for (i = 0; i < 4u; i++) {
        side[k][was][i] = t->p[P_E4 + i];
        t->p[P_E4 + i] = (had[k] >> now) & 1u ? side[k][now][i] : track_desc(t, P_E4 + i)->def;
        knob_cap(t, P_E4 + i);
    }
    had[k] |= (uint8_t)(1u << was);
}
static void tracks_edit(uint32_t slot, int32_t steps)
{
    track_t *t = TSEL;
    int16_t *vp;
    const param_desc_t *d;
    switch (slot) {
    case 3:                                             /* (V10.37) the tempo (was the DJ FILTER: LIVE > MANO) */
        song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_K4, steps, 200), GP[G_BPM].min, GP[G_BPM].max);
        ui.bpm_t = 40;
        return;
    case 4: {                                           /* (V10.45) LIVE > MANO KNOB 4: the DJ FILTER (V10.37 sent it to the
                                                         * tempo, MIXER's new KNOB 4); no acceleration across the middle */
        int32_t v = song.g[G_DJF], to = clamp(v + accel(EN_K4, steps, 127), -64, 63);
        if ((v < 0 && to > 0) || (v > 0 && to < 0))
            to = 0;
        motion_djf_set(t, (int16_t)to);
        return;
    }
    case 0:
        vp = &t->p[P_LEVEL];
        d = &TP[P_LEVEL];
        break;
    case 2:
        vp = &t->p[P_REV];
        d = &TP[P_REV];
        break;
    default:
        vp = &t->p[P_PAN];
        d = &TP[P_PAN];
        break;
    }
    *vp = (int16_t)clamp(*vp + accel(EN_K1 + slot, steps, d->max - d->min), d->min, d->max);
    knob_cap(t, (uint32_t)(vp - t->p));
}

/* SEQ > STEP's piano roll (not the DRUM grid, whose white keys stay its steps): where step recording writes */
static int roll_on(void)
{
    return !ui.home && !ui.menu && !ui.confirm && song.seq_mode && cur_page()->graph == GR_ROLL && !drum_track(TSEL);
}

/* REC tap on every page: arm / disarm recording on the selected track without navigating; arming while stopped
 * never starts or stops the transport. On STEP's piano roll it arms step
 * recording: the keys write the cursor step (seq_entry); PLAY separately starts live recording.
 * Armed and playing, STEP's keys record live (at the play head) instead of writing the cursor step */
static int cp_paste_rec(void);
static int rec_tap(void)                                /* 1: the arming changed */
{
    uint8_t bit = (uint8_t)(1u << song.sel);
    if (cp_paste_rec())                                 /* (seq2h) a step key held, the clipboard full: PASTE */
        return 0;
    if (!ui.home && SONG_CLIPS(cur_page())) {          /* (V10.6s) CLIPS: REC records the launches as the song's rows */
        song_rec_toggle();
        return 1;
    }
    if (chain_busy()) {
        ui_message("STOP TO RECORD");
        return 0;
    }
    if (!ui.home && cur_page()->graph == GR_SONG && !(song.rec & bit)) {
        ui_message("[SEQ] TO RECORD");
        return 0;
    }
    if (!(song.rec & bit)) undo_mark();  /* the music before arming this take */
    song.rec ^= bit;
    ui.force = 1;                                     /* also refresh the status on MENU / ABOUT */
    if ((song.rec & bit) && !song.playing && !seq_counting() && roll_on()) {
        ui_message("KEYS WRITE STEPS");               /* step recording: stopped, at the cursor */
        return 1;
    }
    if ((song.rec & bit) && grid_on())
        ui_message("LANE KEYS RECORD");               /* (the white keys stay the steps) */
    else if ((song.rec & bit) && !ui.home && cur_page()->graph == GR_ROLL)
        ui_message("KEYS RECORD LIVE");               /* (seq_entry pauses while armed and playing) */
    return 1;
}

/* live recording into the selected track now: the STEP page's key entry pauses meanwhile */
static int live_rec_sel(void)
{
    return ((song.rec >> song.sel) & 1u) && (song.playing || transport_req == 1u || seq_counting());
}

/* the OCT- / OCT+ dialog (ui_draw.c draws it); trk: the track or the slot it is about */
static void confirm_open(uint32_t kind, uint32_t trk)
{
    ui.confirm = (uint8_t)kind;
    ui.confirm_trk = (uint8_t)trk;
    ui.act = 0;
    ui.force = 1;
}

/* the grid's page down (-1) / up (+1): the cursor to the same place on it (at most the last step) */
static void page_go(int32_t d)
{
    uint32_t len = (uint32_t)TSEL->p[P_SLEN], pages = (len + 15u) / 16u, b;
    if (pages < 2u)
        return;
    b = (ui.bank + pages + (uint32_t)d) % pages;
    cursor_set((int32_t)(b * 16u + ui.cursor % 16u < len ? b * 16u + ui.cursor % 16u : len - 1u));
}

/* the grid's knobs: 1 STEP (the cursor), 2 LANE, 3 HIT and 4 ACC of the lane at the cursor (right on, left off; 0.8.5:
 * HIT on along its modes, ui.c GRID_HIT) */
static void grid_edit(uint32_t slot, int32_t steps)
{
    if (slot == 0u)
        cursor_set(ui.cursor + steps);
    else if (slot == 1u)
        ui.lane = (uint8_t)clamp((int32_t)ui.lane + (steps > 0 ? 1 : -1), 0, NLANE - 1);
    else if (slot == 2u)
        grid_hit_set(TSEL, ui.cursor, ui.lane,
                     (uint32_t)clamp((int32_t)grid_hit_of(&TSEL->step[ui.cursor], ui.lane) + (steps > 0 ? 1 : -1), 0, LM_N));
    else
        grid_acc(TSEL, ui.cursor, ui.lane, steps > 0);
}

/* the keys on the grid (presses): a white key toggles the selected lane at its step of the page (its accent
 * while ACC is held) and puts the cursor there; a lane key selects the lane (seq.c plays it); the page keys */
static void grid_keys(uint32_t pressed)
{
    uint32_t k, len = (uint32_t)TSEL->p[P_SLEN];
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k);
        if (!((pressed >> k) & 1u))
            continue;
        if (!key_black(k)) {
            uint32_t i = ui.bank * 16u + p;
            if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); continue; }
            if (i >= len)
                continue;                               /* past LEN: no step there */
            {   /* a hit (an accent) there goes when the key is let go, unless it was held for a lock (lock_keys) */
                const step_t *st = &TSEL->step[i];
                uint64_t b = (uint64_t)1 << i;
                uint32_t acc = (uint32_t)black_held(GK_ACC);
                if (((acc ? step_accents_of(TSEL,st) : step_lanes_of(TSEL,st)) >> ui.lane) & 1u) {
                    ui.plk_rm |= b;
                    ui.plk_acc = acc ? ui.plk_acc | b : ui.plk_acc & ~b;
                    ui.plk_lane = ui.lane;
                    ui.plk_trk = song.sel;
                } else if (acc) {
                    grid_acc(TSEL, i, ui.lane, 1);
                    cp_add |= b, cp_add_acc |= b, cp_add_lane = ui.lane;   /* (seq2h: a copy with the key takes it back) */
                } else {
                    grid_hit(TSEL, i, ui.lane, 1);
                    cp_add |= b, cp_add_acc &= ~b, cp_add_lane = ui.lane;
                }
            }
            cursor_set((int32_t)i);
        } else if (p < NLANE) {
            ui.lane = (uint8_t)p;
        } else if (p != GK_ACC) {
            page_go(p == GK_PGUP ? 1 : -1);
        }
    }
}

/* (0.9.9-seq2g) SEQ > COMP / COMP 2: a white key picks its step of the bar shown (the footer's strip; DRUM the same), black
 * keys 10 / 11 the bar down / up; silent (ui_name.c keys_mode). KNOB 1..4 then edit that step's component */
static void comp_keys(uint32_t pressed)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), i = ui.bank * 16u + p;
        char b[12];
        if (!((pressed >> k) & 1u))
            continue;
        if (key_black(k)) {
            if (p == GK_PGUP || p == GK_PGDN)
                page_go(p == GK_PGUP ? 1 : -1);
            continue;
        }
        if (i >= (uint32_t)TSEL->p[P_SLEN])
            continue;
        cursor_set((int32_t)i);
        str_cpy(b, "STEP ", sizeof b);
        fmt_int(b + 5, (int32_t)i + 1);
        ui_message(b);
    }
}

/* ------------------------------------------------- parameter locks --- */
/* 1.1 (Discussions #75, #53): on SEQ > STEP, hold a step and turn KNOB 1..4: the step gets a lock of the parameter
 * that knob edits on the sound page shown last (ui.plk_src: HOME's four, ENV, LFO, FX, EDIT 1 / 2, OP ENV, ..; the
 * cards show those four while a step is held, a locked value bright, the sound's own DIM). The step held: on the
 * DRUM grid its white keys (several at once: each gets the lock; a press on a hit takes it away only when let go,
 * and not at all when it was held for a lock), on the roll the step being entered (its note keys held). [EDIT]
 * tapped meanwhile clears the held steps' locks (EDIT alone still clears the step, its locks with it). Not while a
 * song plays (STOP TO EDIT). A lock is a motion record (motion.c): AUTOMATION's PLAY plays it, CLEAR clears it */
static uint64_t lock_held(void)
{
    const track_t *t = TSEL;
    uint32_t k, len = (uint32_t)t->p[P_SLEN], keys = fm1_in.notes & ~kb_layer;
    uint64_t m = 0;
    if (ui.home || ui.menu || ui.confirm || ui.layer || name_on() || !song.seq_mode || cur_page()->graph != GR_ROLL ||
        !keys)
        return 0;
    if (!grid_on())                                     /* the roll: the step being entered */
        return ui.entry_open && !live_rec_sel() && ui.cursor < len ? (uint64_t)1 << ui.cursor : 0u;
    for (k = 0; k < 27u; k++)
        if (((keys >> k) & 1u) && !key_black(k) && ui.bank * 16u + key_place(k) < len)
            m |= (uint64_t)1 << (ui.bank * 16u + key_place(k));
    return m;
}

/* each pass, before the grid's keys: a hit pressed on goes once its key is let go (plk_rm, grid_keys) */
static void lock_keys(void)
{
    uint64_t gone = ui.plk_rm & ~lock_held();
    uint32_t i;
    cp_add &= lock_held();                              /* (seq2h) a key let go: the hit it put there stays */
    for (i = 0; gone && i < NSTEP; i++)
        if ((gone >> i) & 1u) {
            if ((ui.plk_acc >> i) & 1u)
                grid_acc(&trk[ui.plk_trk % NTRK], i, ui.plk_lane, 0);
            else
                grid_hit(&trk[ui.plk_trk % NTRK], i, ui.plk_lane, 0);
        }
    ui.plk_rm &= ~gone;
}

/* KNOB k turned s with the steps `held` held: each gets (or changes) its lock of lock_id(k), from its lock or the
 * sound's own value */
static void lock_turn(uint32_t k, int32_t s, uint64_t held)
{
    track_t *t = TSEL;
    uint32_t id = lock_id(k), i;
    const param_desc_t *d;
    if (chain_busy()) {
        ui_message("SONG PLAYS: STOP IT TO EDIT");
        return;
    }
    if (id == 0xFFu)
        return;                                         /* (an empty card: nothing to lock) */
    ui.plk_rm &= ~held;                                 /* (held for a lock: its hit stays) */
    d = track_desc(t, id);
    s = accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min);
    for (i = 0; i < NSTEP; i++) {
        int16_t v;
        if (!((held >> i) & 1u))
            continue;
        if (!motion_lock_get(t, i, id, &v))
            v = motion_base_value(t, id);
        if (motion_set_lock(t, i, id, (int16_t)param_turn(d, v, s)) == 2)
            break;                                      /* (full: ui_notices says so) */
    }
}

/* [EDIT] with the steps `held` held: their locks go */
static void lock_clear(uint64_t held)
{
    uint32_t i, n = 0;
    ui.plk_rm &= ~held;
    if (!(motion_lock_steps(song.sel) & held)) {
        ui_message("NO LOCKS");
        return;
    }
    for (i = 0; i < NSTEP; i++)
        if ((held >> i) & 1u)
            n += motion_clear_locks(TSEL, i);
    ui_message(n ? "LOCKS CLEARED" : "NO LOCKS");
}

/* ------------------------------------------------- copy / paste --- */
/* (0.9.9-seq2h, the approved mock-up's CP / CP2) on SEQ's roll pages (the DRUM grid too), stopped or playing, the white
 * keys are the steps of the bar shown (ui.bank, as the grid's and COMP's): a step key held + SELECT turned COPIES that
 * step, two held (the first and the last) the range between them, keys 1 and 16 the whole bar; a step key held + REC
 * tapped PASTES the clipboard from that step on (round within LEN). REC tapped with no step key, or the clipboard empty,
 * arms / disarms as ever. A melodic track armed: its keys write or record notes, no copy or paste (DRUM: the white
 * keys stay the steps). DRUM <-> melodic refused. UNDO (GLO + G4, project.c) takes the paste back */
static uint64_t cp_held(void)
{
    const track_t *t = TSEL;
    uint32_t k, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), keys = fm1_in.notes & ~kb_layer;
    uint64_t m = 0;
    if (!keys || ui.home || ui.menu || ui.confirm || name_on() || !song.seq_mode ||
        cur_page()->graph != GR_ROLL || (!drum_track(t) && ((song.rec >> song.sel) & 1u)))
        return 0;
    for (k = 0; k < 27u; k++)
        if (((keys >> k) & 1u) && !key_black(k) && ui.bank * 16u + key_place(k) < len)
            m |= (uint64_t)1 << (ui.bank * 16u + key_place(k));
    return m;
}

/* the held keys' own doing on the grid undone (the hit a press put there; a hit pressed stays when let go) */
static void cp_keys_used(track_t *t, uint64_t held)
{
    uint32_t i;
    ui.plk_rm &= ~held;
    for (i = 0; i < NSTEP; i++)
        if (((cp_add & held) >> i) & 1u) {
            if ((cp_add_acc >> i) & 1u)
                grid_acc(t, i, cp_add_lane, 0);
            else
                grid_hit(t, i, cp_add_lane, 0);
        }
    cp_add &= ~held;
}

static void cp_say(const char *what, uint32_t n)
{
    char b[24];
    str_cpy(b, what, sizeof b);
    fmt_int(b + str_len(b), (int32_t)n);
    str_cpy(b + str_len(b), n == 1u ? " STEP" : " STEPS", 8);
    ui_message(b);
}

static void cp_copy(uint64_t held)
{
    track_t *t = TSEL;
    uint32_t lo = (uint32_t)__builtin_ctzll(held), hi = 63u - (uint32_t)__builtin_clzll(held), i, k = trk_index(t), f;
    cp_keys_used(t, held);
    clip.n = (uint8_t)(hi - lo + 1u);
    clip.drum = (uint8_t)drum_track(t);
    clip.engine = (uint8_t)eng_idx(t->eng_req);
    memcpy(clip.st, &t->step[lo], clip.n * sizeof clip.st[0]);
    clip.nl = 0;
    f = motion_guard();
    for (i = 0; i < motion.count && clip.nl < CLIP_LOCKS; i++) {
        const motion_event_t *e = &motion.event[i];
        if (MOTION_TRK(e) != k || !(e->param & MOTION_LOCK) || MOTION_STEP(e) < lo || MOTION_STEP(e) > hi)
            continue;
        clip.lk[clip.nl].off = (uint8_t)(MOTION_STEP(e) - lo);
        clip.lk[clip.nl].id = (uint8_t)MOTION_ID(e);
        clip.lk[clip.nl++].v = MOTION_VAL(e);
    }
    motion_unguard(f);
    cp_say("COPIED ", clip.n);
    ui.force = 1;
}

/* Shared by range paste and SEQ > PASTE SEQ. One clipboard and one undo level. */
static void cp_put(track_t *t, uint32_t a, uint32_t whole)
{
    uint32_t j, len = whole ? clip.n : (uint32_t)clamp(t->p[P_SLEN],1,NSTEP), n = clip.n < len ? clip.n : len;
    if (clip.drum != (uint8_t)drum_track(t)) { ui_message("DRUM <-> NOTES: NO"); return; }
    undo_mark();
    if (whole) {
        motion_clear_locks(t,NSTEP);
        for(j=0;j<NSTEP;j++) step_clear(&t->step[j]);
        t->p[P_SLEN]=(int16_t)len;
        cursor_set(0);
    }
    for(j=0;j<n;j++) {
        uint32_t d=(a+j)%len;
        if(!whole) motion_clear_locks(t,d);
        t->step[d]=clip.st[j];
    }
    for(j=0;j<clip.nl;j++)
        if(clip.lk[j].off<n && (clip.engine==eng_idx(t->eng_req) || clip.lk[j].id<P_E0) &&
           motion_set_lock(t,(a+clip.lk[j].off)%len,clip.lk[j].id,clip.lk[j].v)==2)break;
    cp_say("PASTED ",n);str_cpy(ui.msg2,"UNDO: GLO + G4",sizeof ui.msg2);ui.force=1;
}
static void cp_seq(uint32_t paste)
{
    if(paste) {
        if(!clip.n) ui_message("NOTHING COPIED");
        else if(chain_busy()) ui_message("SONG PLAYS: STOP IT TO EDIT");
        else cp_put(TSEL,0,1);
    } else {
        uint32_t len=(uint32_t)clamp(TSEL->p[P_SLEN],1,NSTEP);
        cp_add=cp_add_acc=0;
        cp_copy(~(uint64_t)0 >> (NSTEP-len));
    }
}
/* REC tapped: 1 = it pasted (or refused), not an arming tap. */
static int cp_paste_rec(void)
{
    uint64_t held = clip.n ? cp_held() : 0u;
    if(!held) return 0;
    if(chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return 1; }
    cp_keys_used(TSEL,held);
    cp_put(TSEL,(uint32_t)__builtin_ctzll(held),0);
    return 1;
}

/* (0.6.81) SEQ > NOTE, the piano roll's edits on the cursor's step: FOCUS the chord or one of its notes (lit), PITCH
 * moves what is in focus up or down, MOVE moves it a step left or right (the chord with its length: it changes places
 * with the step beside it; a note: into the step beside, a NOTE with room or an empty one), LEN the chord's length (its
 * TIEs, over empty steps only; 0.8.4: STEP's KNOB 3, len_turn). EDIT deletes what is in focus (page_tap) */
/* (0.8.4) a note's length d steps longer or shorter, its notes kept (a tester: no way to lengthen a recorded note but
 * TIE by TIE): longer, the steps after it TIEs (over RESTs and empty steps; the next note stops it: NEXT NOTE), up to
 * 16 steps or LEN, round past LEN's end as it plays (as step recording writes them); shorter, its last TIEs RESTs. Its
 * length after */
static uint32_t len_turn(track_t *t, uint32_t c, int32_t d)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, l = note_len(t, c, len), max = len < 16u ? len : 16u;
    if (c >= len)
        return 1;                                       /* (past LEN: nothing plays it) */
    for (; d > 0 && l < max; d--, l++) {
        step_t *nx = &t->step[(c + l) % len];
        if (step_on(nx)) { ui_message("NEXT NOTE"); break; }
        memset(nx, 0, sizeof *nx);
        nx->time = ST_TIE;
        nx->vel = t->step[c].vel;
    }
    for (; d < 0 && l > 1u; d++) {
        step_t *e = &t->step[(c + --l) % len];
        memset(e, 0, sizeof *e);
        e->time = ST_REST;
    }
    return l;
}
static void note_drop(step_t *st, uint32_t j)           /* note j out of the step (empty: a REST, its TIEs nothing) */
{
    uint32_t i;
    for (i = j; i + 1u < st->n; i++)
        st->note[i] = st->note[i + 1u];
    if (st->n) st->note[--st->n] = 0;
    if (!st->n && !st->hit) st->time = ST_REST;
}
static __attribute__((noinline)) void note_edit(uint32_t slot, int32_t steps)
{
    track_t *t = TSEL;
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, c = ui.cursor, i;
    step_t *st = &t->step[c];
    uint32_t f = ui.pr_focus <= st->n ? ui.pr_focus : 0u;
    if (st->time != ST_NOTE)
        f = 0;
    switch (slot) {
    case 0:                                             /* FOCUS: ALL, 1 .. the chord's notes */
        ui.pr_focus = (uint8_t)clamp((int32_t)f + (steps > 0 ? 1 : -1), 0, st->time == ST_NOTE ? (int32_t)st->n : 0);
        break;
    case 1:                                             /* PITCH */
        if (st->time != ST_NOTE || !st->n) { ui_message("NO NOTE HERE"); break; }
        for (i = 0; i < st->n; i++)
            if (!f || i == f - 1u)
                st->note[i] = (uint8_t)clamp(st->note[i] + steps, 1, 127);
        last_note = st->note[f ? f - 1u : 0u];
        break;
    case 2: {                                           /* MOVE, a step a detent */
        int32_t dir = steps > 0 ? 1 : -1;
        if (st->time != ST_NOTE || !st->n) { ui_message("NO NOTE HERE"); break; }
        if (!f) {                                       /* the chord and its TIEs change places with the step beside */
            uint32_t l = note_len(t, c, len), k;
            step_t blk[NSTEP], other;
            if (l >= len) break;
            for (k = 0; k < l; k++) blk[k] = t->step[(c + k) % len];
            if (dir > 0) {
                uint32_t nx = (c + l) % len, nl = note_len(t, nx, len);
                if (t->step[nx].time == ST_NOTE && nl > 1u) { ui_message("BLOCKED: A LONG NOTE"); break; }
                other = t->step[nx];
                if (other.time == ST_TIE) { other.time = ST_REST; other.n = 0; }
                t->step[c] = other;
                for (k = 0; k < l; k++) t->step[(c + 1u + k) % len] = blk[k];
                ui.cursor = (uint8_t)((c + 1u) % len);
            } else {
                uint32_t pv = (c + len - 1u) % len;
                if (t->step[pv].time == ST_TIE) { ui_message("BLOCKED: A LONG NOTE"); break; }
                other = t->step[pv];
                for (k = 0; k < l; k++) t->step[(pv + k) % len] = blk[k];
                t->step[(pv + l) % len] = other;
                ui.cursor = (uint8_t)pv;
            }
        } else {                                        /* one note: into the step beside */
            uint32_t to = (c + len + (uint32_t)dir) % len;
            step_t *d = &t->step[to];
            uint8_t n = st->note[f - 1u];
            if (d->time == ST_TIE) { ui_message("BLOCKED: A LONG NOTE"); break; }
            if (d->time == ST_NOTE && d->n >= 4u) { ui_message("BLOCKED: 4 NOTES THERE"); break; }
            if (d->time != ST_NOTE) { memset(d, 0, sizeof *d); d->time = ST_NOTE; d->vel = st->vel; }
            note_drop(st, f - 1u);
            d->note[d->n++] = n;
            ui.cursor = (uint8_t)to;
            ui.pr_focus = d->n;
        }
        ui.bank = (uint8_t)(ui.cursor / 16u);
        ui.entry_open = 0;
        break;
    }
    default:                                            /* LEN: the chord's TIEs (0.8.4: STEP's KNOB 3 the same) */
        if (st->time != ST_NOTE || !st->n) { ui_message("NO NOTE HERE"); break; }
        len_turn(t, c, steps > 0 ? 1 : -1);
        break;
    }
}

/* (0.6.88) SEQ > TIMING: KNOB 1 the step, KNOB 2 its OFFSET off the grid (-8 .. +7 sixteenths: early / late; a step
 * with nothing to play keeps 0), KNOB 3 the track's REC Q (how much live recording snaps to the grid). EDIT: back on it */
/* (0.6.88) a step's own place off the grid (0.6.92: SEQ > NOTE's KNOB 4, MICRO; a DRUM track's too); (0.9.1) -32 .. +31
 * 64ths of a step, a detent each (faster turned: knob acceleration) */
static void micro_edit(int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    if (st->time != ST_NOTE || (!st->n && !st->hit)) { ui_message("NO NOTE HERE"); return; }
    ST_MICRO_SET(st, clamp(ST_MICRO(st) + accel(EN_K4, steps, MICRO_MAX - MICRO_MIN), MICRO_MIN, MICRO_MAX));
}
/* (V10.8) SEQ > STEP's KNOB 4: the step's velocity 1..127 (none recorded: 96, as it plays), 128 ACC, 129 SLD, 130 A+S */
static int32_t step_velpos(const step_t *st)
{
    return (st->flags & SF_SLIDE) ? (st->flags & SF_ACCENT ? 130 : 129) : (st->flags & SF_ACCENT) ? 128 : st->vel ? st->vel : 96;
}
static __attribute__((noinline)) void step_edit(uint32_t slot, int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    uint32_t i;
    if (cur_page()->id[0] == 4u) {                      /* (0.6.81) SEQ > NOTE; (0.6.92) KNOB 4 MICRO, on a kit too */
        if (slot == 3u) micro_edit(steps);
        else if (drum_track(TSEL)) { if (slot == 0u) cursor_set(ui.cursor + steps); else ui_message("A KIT: STEP + MICRO"); }
        else note_edit(slot, steps);
        return;
    }
    if (drum_track(TSEL)) {
        grid_edit(slot, steps);
        return;
    }
    switch (slot) {
    case 0:                                               /* STEP: the cursor */
        cursor_set(ui.cursor + steps);
        break;
    case 1:                                               /* NOTE: transpose the step */
        if (!st->n) {
            st->note[0] = last_note;
            st->n = 1;
            st->time = ST_NOTE;
            break;
        }
        for (i = 0; i < st->n; i++)
            st->note[i] = (uint8_t)clamp(st->note[i] + steps, 1, 127);
        st->time = ST_NOTE;
        last_note = st->note[0];
        break;
    case 2: {                                             /* TIME (0.8.4), one range: REST < TIE < NOTE < 2 ST .. 16 ST;
                                                           * right a NOTE longer (len_turn), left shorter, then a TIE, a
                                                           * REST (one detent at a time past NOTE: a fast turn stops there) */
        uint32_t len = TSEL->p[P_SLEN] > 0 ? (uint32_t)TSEL->p[P_SLEN] : 1u;
        if (st->time == ST_NOTE && (steps > 0 || note_len(TSEL, ui.cursor, len) > 1u)) {
            if (!st->n && steps > 0) ui_message("NO NOTE HERE");   /* (an empty NOTE: nothing to hold) */
            else len_turn(TSEL, ui.cursor, steps);
        } else if (steps > 0)
            st->time = (uint8_t)(st->time - 1u);          /* REST > TIE > NOTE (x1) */
        else if (st->time < ST_REST) {
            st->time = (uint8_t)(st->time + 1u);          /* NOTE x1 > TIE > REST */
            if (st->time == ST_REST) {                    /* (0.9.2) a REST clears the step (its notes, locks: EDIT's
                                                         * clear until 0.9.1; SEQ + G5 too) */
                step_clear(st);
                (void)motion_clear_locks(TSEL, ui.cursor);
            }
        }
        break;
    }
    default: {                                            /* (V10.8) VEL 1..127, then ACC (127), SLD, A+S (was FLAG
                                                           * - / ACC / SLD / A+S; testers: a step's velocity) */
        int32_t pos = step_velpos(st);
        pos = pos < 127 || (pos == 127 && steps < 0) ? clamp(pos + steps, 1, 127)   /* (a turn stops at 127 on its way
                                                           * up or down: the flags past it on their own) */
            : clamp(pos + steps, 127, 130);
        if (pos <= 127) st->vel = (uint8_t)pos;
        else if (!st->vel) st->vel = 96;
        st->flags = (uint8_t)((st->flags & ~(SF_ACCENT | SF_SLIDE)) | (pos == 128 || pos == 130 ? SF_ACCENT : 0u) |
                              (pos >= 129 ? SF_SLIDE : 0u));
        break;
    }
    }
}

/* (0.9.9-seq2d) SEQ > COMP / COMP 2: the cursor step's C1 / C2 (the step picked on the roll, SELECT here): KNOB 1
 * ACTION (OFF and the twelve, then BEND; a new one its VALUE's default, LENGTH 1), KNOB 2 LENGTH 1..32 (a RAMP's: 1..32
 * steps, then 1..16 bars), KNOB 3 CONDITION (ALWAYS 25 50 75 % FIRST !FIRST FILL !FILL PRE !PRE NEI !NEI, ELSE on COMP 2 alone: MONO; then
 * 1:2 .. 16:16, !1:2 .. !16:16), KNOB 4 VALUE (core.h). OFF clears the component. The step's CHANCE and RATCH: CHANCE */
static __attribute__((noinline)) void comp_knob(uint32_t slot, int32_t steps)
{
    step_t *st = &TSEL->step[ui.cursor];
    uint32_t k = comp_slot(), a = comp_view();
    int32_t d = clamp(steps, -4, 4);
    uint8_t *cp = k == 1u ? &st->comp2 : &st->comp, *ap = k == 1u ? &st->carg2 : &st->carg;
    uint8_t *lp = &st->clen[k & 1u], *np = &st->cnm[k & 1u];
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    if (slot == 0u) {
        uint32_t na = (uint32_t)clamp((int32_t)a + d, 0, CA_N - 1);
        if (na == a) return;
        if (!na) { *cp = *ap = *lp = *np = 0; return; }
        *cp = CO_MAKE(na, a && na != CA_SHIFT ? CO_COND(*cp) : CC_ALWAYS);
        *ap = CA_VDEF[na];
        if (!a) { *lp = 0; *np = CNM_MAKE(1u, 2u); }
        if (na == CA_ARP || na == CA_STUT) *lp = 3;     /* (an ARP, a STUTTER: four steps to start with) */
        if (na == CA_RAMP) {                            /* (V10.6v: a RAMP 16 steps; the step's LOCK is what it moves) */
            *lp = 15;
            if (!motion_lock_at(TSEL, ui.cursor % NSTEP)) ui_message("LOCK A PARAM");
        }
        return;
    }
    if (!a) { ui_message("ACTION FIRST"); return; }
    if (slot == 1u) {
        if (a == CA_RAMP) {                             /* (V10.6v) 1..32 steps, then 1..16 bars */
            int32_t i = clamp((int32_t)(*ap & CV_RAMP_BARS ? CL_MAX + *lp : *lp) + d, 0, CL_MAX + 15);
            *ap = (uint8_t)((*ap & ~CV_RAMP_BARS) | (i >= (int32_t)CL_MAX ? CV_RAMP_BARS : 0u));
            *lp = (uint8_t)(i >= (int32_t)CL_MAX ? i - (int32_t)CL_MAX : i);
        } else
            *lp = (uint8_t)clamp((int32_t)*lp + d, 0, CL_MAX - 1);
    } else if (slot == 2u) {
        uint32_t c, nm;
        int32_t i = clamp((int32_t)(a == CA_SHIFT ? shift_cond_idx(CO_COND(*cp), *np) : cond_idx(CO_COND(*cp), *np)) + steps,
                          0, (int32_t)(a == CA_SHIFT ? 3u + 2u * CCL_NM : CCL_N) - 1);
        if (a != CA_SHIFT && !k && i == CC_ELSE)                         /* (ELSE: C2's, MONO) */
            i += steps > 0 ? 1 : -1;
        if (a == CA_SHIFT) shift_cond_at((uint32_t)i, &c, &nm);
        else cond_at((uint32_t)i, &c, &nm);
        *cp = CO_MAKE(a, c);
        if (c >= CC_NM) *np = (uint8_t)nm;
    } else {
        uint32_t b = a == CA_RAMP ? *ap & CV_RAMP_BARS : 0u;   /* (V10.6v: a RAMP's BARS, KNOB 2's) */
        int32_t i = clamp((int32_t)comp_vidx(a, *ap ^ b) + steps, 0, CA_VMAX[a]);
        *ap = (uint8_t)(comp_vat(a, (uint32_t)i) | b);
    }
}
/* (0.6.85) SEQ > STRETCH: KNOB 1 STRETCH /8 .. /2, x1, x2 .. x8; KNOB 2 REPEAT OFF, x2 .. x8, TO 64; (0.6.92) done as the
 * knob turns, always from the sequence as it was before the first turn (its base: the steps, LEN and automation,
 * kept while the result stays untouched; back to the middle = back to it; UNDO: GLO + G4); KNOB 3 NUDGE: the whole
 * sequence a step left / right; KNOB 4 ROTATE: played from another step (P_SOFS). ui_tools.c tl_stretch, tl_repeat,
 * tl_shift */
static uint32_t tl_len(const track_t *t);
static void tl_take(const track_t *t);
static void tl_shift(track_t *t, uint32_t len, uint32_t d);
static void tl_order(track_t *t, uint32_t len, const uint8_t *src);
static uint32_t tl_stretch(track_t *t, int32_t k);
static uint32_t tl_repeat(track_t *t, uint32_t k);
static struct {
    uint8_t valid, trk, which;                          /* which: the knob the base belongs to (0.6.96: the other knob
                                                         * takes the result as its base, so REPEAT then STRETCH
                                                         * stretches the repeats) */
    int16_t len;
    uint32_t sig;                                       /* the result's steps: another sig, the user edited: a new base */
    step_t step[NSTEP];
    motion_store_t mo;
} shp_base;
static void shape_apply(uint32_t which)
{
    track_t *t = TSEL;
    uint32_t was, n, f;
    char m[24];
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    if (!shp_base.valid || shp_base.trk != song.sel || shp_base.sig != steps_sig(t) || shp_base.which != which) {   /* the
                                                         * base: as it is now (the other knob's result included) */
        undo_mark();
        shp_base.valid = 1; shp_base.trk = (uint8_t)song.sel; shp_base.len = t->p[P_SLEN]; shp_base.which = (uint8_t)which;
        memcpy(shp_base.step, t->step, sizeof shp_base.step);
        f = motion_guard(); memcpy(&shp_base.mo, &motion, sizeof shp_base.mo); motion_unguard(f);
        if (which == 0u) shp_r = 0; else shp_i = 7;    /* (the other knob's pick starts over) */
    } else {                                            /* back to the base, then the new pick (as the tools write:
                                                         * straight into the steps, the automation with them) */
        memcpy(t->step, shp_base.step, sizeof t->step);
        t->p[P_SLEN] = shp_base.len;
        f = motion_guard(); memcpy(&motion, &shp_base.mo, sizeof motion); RING_PUBLISH(); motion_unguard(f);
    }
    was = (uint32_t)clamp(shp_base.len, 1, NSTEP);
    n = which == 0u ? (shp_k() == 1 ? was : tl_stretch(t, shp_k())) : (shp_rep() == 1u ? was : tl_repeat(t, shp_rep()));
    shp_base.sig = steps_sig(t);
    if (!n) { ui_message("LEN 64: NO ROOM"); return; }
    str_cpy(m, "LEN ", sizeof m); fmt_int(m + 4, (int32_t)was); str_cpy(m + str_len(m), " > ", 4);
    fmt_int(m + str_len(m), (int32_t)n);
    if (which == 0u && shp_k() > 1 && was * (uint32_t)shp_k() > NSTEP) str_cpy(m + str_len(m), " CUT", 5);
    ui_message(m);
}
/* (NUDGE's step) a note's first step: back from i over TIEs; one past its last: on from i over TIEs, inside len */
static __attribute__((noinline)) uint32_t ndg_note(const step_t *s, uint32_t i) { while (i && s[i].time == ST_TIE) i--; return i; }
static __attribute__((noinline)) uint32_t ndg_end(const step_t *s, uint32_t i, uint32_t len)
{
    while (++i < len && s[i].time == ST_TIE) {}
    return i;
}
static void shape_knob(uint32_t slot, int32_t steps)
{
    int32_t d = steps > 0 ? 1 : -1;
    if (slot == 0u) { shp_i = (uint8_t)clamp((int32_t)shp_i + d, 0, 14); shape_apply(0); }
    else if (slot == 1u) { shp_r = (uint8_t)clamp((int32_t)shp_r + d, 0, 8); shape_apply(1); }
    else if (slot == 2u && ndg_step == 1u) {            /* (V10.25) the cursor step off the grid, 64ths of a step */
        micro_edit(steps);
    } else if (slot == 2u && ndg_step) {                /* (V10.7.4) OCT+ again: the cursor step itself a step left / right
                                                         * (traded with its neighbour), the cursor with it. A note moves with
                                                         * its TIEs and trades with the neighbour's note whole (V10.7.4
                                                         * swapped single steps: a moved note lost its length, the TIE left
                                                         * behind lengthened the note before it); at step 1 / LEN it stays
                                                         * (V10.7.4 wrapped: a note's TIEs cannot) */
        track_t *t = TSEL;
        uint8_t src[NSTEP];
        uint32_t len = tl_len(t), c = ui.cursor % len, a, b, x, y, z, j;
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        a = ndg_note(t->step, c);                       /* the cursor's note [a, b): back over its TIEs, past them */
        b = ndg_end(t->step, a, len);
        if (d > 0 ? b >= len : !a)                      /* (an end: it stays) */
            return;
        if (d > 0) { x = a; y = b; z = ndg_end(t->step, b, len); }
        else { y = a; z = b; x = ndg_note(t->step, a - 1u); }
        for (j = 0; j < len; j++)                       /* [x, y) and [y, z) trade places */
            src[j] = (uint8_t)(j < x || j >= z ? j : j - x < z - y ? y + j - x : j - (z - y));
        x = d > 0 ? x + z - y : x;                      /* (the note's new place) */
        undo_mark();
        tl_take(t);
        tl_order(t, len, src);
        ui.cursor = (uint8_t)x;
    } else if (slot == 2u) {
        track_t *t = TSEL;
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        tl_take(t);
        tl_shift(t, tl_len(t), d > 0 ? 1u : tl_len(t) - 1u);
        ui_message(d > 0 ? "NUDGED RIGHT" : "NUDGED LEFT");
    } else {
        TSEL->p[P_SOFS] = (int16_t)clamp(TSEL->p[P_SOFS] + steps, -32, 32);
        knob_cap(TSEL, P_SOFS);
    }
}
/* (0.8.5) EDIT > FOCUS: KNOB 1 the parameter (E1..E8 with a name, no wrap), 2 its value, 3 the SRC of the MOD slot aimed
 * at it (from OFF: a free slot takes it, its DEST E1..E8; back to OFF: the slot free again), 4 that slot's AMT */
static __attribute__((noinline)) void focus_knob(uint32_t slot, int32_t steps)
{
    uint32_t k = focus_k(), f = k, n;
    int32_t j = focus_slot(k);
    int16_t *p;
    const param_desc_t *d;
    if (slot == 0u) {                                   /* (a named one a detent) */
        int32_t m = steps < 0 ? -steps : steps;
        for (n = 0; n < 8u && m > 0; n++) {
            if ((steps > 0 && f == 7u) || (steps < 0 && f == 0u))
                break;
            f = steps > 0 ? f + 1u : f - 1u;
            if (focus_named(f)) {
                ui.focus = (uint8_t)f;
                m--;
            }
        }
        return;
    }
    if (slot == 1u) {
        int32_t was = TSEL->p[P_E0 + k];
        d = track_desc(TSEL, P_E0 + k);
        TSEL->p[P_E0 + k] = (int16_t)param_turn(d, was, accel(EN_K1 + 1u, steps, d->fmt == F_ENUM ? 0 : d->max - d->min));
        knob_cap(TSEL, P_E0 + k);
        excite_turn(TSEL, &TSEL->p[P_E0 + k], was);
        return;
    }
    if (slot == 2u && j < 0 && !mod_dst_offer(TSEL, MD_E1 + k)) {
        ui_message("NO MOD DEST"); return;
    }
    if (slot == 2u && j < 0) {                          /* (no slot aims at it: a free one, turned up) */
        uint32_t m;
        if (steps <= 0)
            return;
        for (m = 0; m < 4u && TSEL->p[P_M1SRC + 3u * m]; m++)
            ;
        if (m == 4u) {
            ui_message("MOD: 4 SLOTS USED");
            return;
        }
        j = (int32_t)m;
        TSEL->p[P_M1DST + 3u * m] = (int16_t)(MD_E1 + k);
    }
    if (j < 0)
        return;
    p = &TSEL->p[P_M1SRC + 3u * (uint32_t)j + (slot == 2u ? 0u : 2u)];
    d = track_desc(TSEL, P_M1SRC + 3u * (uint32_t)j + (slot == 2u ? 0u : 2u));
    *p = (int16_t)param_turn(d, *p, slot == 2u ? (steps > 0 ? 1 : -1) : accel(EN_K1 + 3u, steps, d->max - d->min));
}
/* Preserve fractional progress when FIT/SPAN changes during playback; never catch up thousands of tiny steps.
 * This runs only on a knob turn, outside the audio ISR. */
static void fit_ui_edit(track_t *t, int16_t *param, int16_t value)
{
    uint32_t old, now, ti = trk_index(t), oldghost = 0;
    if (param ? *param == value : t->seq_fit == fit_valid((uint8_t)value)) return;
    if (!param && !(t->seq_fit & 128u) && !((uint8_t)value & 128u)) {
        t->seq_fit = fit_valid((uint8_t)value);          /* editing an inactive SPAN must not disturb microtiming */
        return;
    }
    fm1_irq_off();
    if (LV_ON(ti) && lv.gpos[ti] < 0x7FFFFFFFu)
        oldghost = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), lv.gidx[ti]);
    old = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx);
    if (param) *param = value;
    else t->seq_fit = fit_valid((uint8_t)value);
    if (t->seq_pos < 0x7FFFFFFFu) t->seq_idx %= (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    now = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx);
    if (t->seq_pos < 0x7FFFFFFFu) {
        uint32_t pos = old ? (uint32_t)((uint64_t)t->seq_pos * now / old) : 0u;
        t->seq_pos = pos < now ? pos : now - 1u;
        t->ext = 0;
    }
    if (oldghost) {
        lv.gidx[ti] %= (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        now = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), lv.gidx[ti]);
        uint32_t pos = (uint32_t)((uint64_t)lv.gpos[ti] * now / oldghost);
        lv.gpos[ti] = pos < now ? pos : now - 1u;
    }
    fm1_irq_on();
}
static void fit_ui_set(track_t *t, uint8_t value) { fit_ui_edit(t, 0, value); }

/* (V10.6o) SAVE > PARTS and TIMELINE (the owner's mock-ups A and B; ui_lzsave.c graph_parts graph_tl) */
/* song row d becomes row s (its scene, tracks, variations with it; ARRANGE's and TIMELINE's ADD, DELETE ROW); s past the
 * rows: an empty one (A, 4 bars) */
static void row_copy(uint32_t d, uint32_t s)
{
    if (s < CHAIN_ROWS) {
        chain_config.row[d] = chain_config.row[s];
        song_idx.scene[song_cur][d] = song_idx.scene[song_cur][s];
        song_idx.tsrc[song_cur][d] = song_idx.tsrc[song_cur][s];
        song_idx.tvar[song_cur][d] = song_idx.tvar[song_cur][s];
    } else {
        chain_config.row[d] = (chain_row_t){0, 4};
        memset(&song_idx.scene[song_cur][d], 0, sizeof(scene_t));
        song_idx.tsrc[song_cur][d] = song_idx.tvar[song_cur][d] = 0;
    }
}
/* the song's row of part s: d = 0 the cursor's if it is s's, else s's first; else s's next / previous (round); none: the
 * cursor's */
static uint32_t part_row(uint32_t s, int32_t d)
{
    uint32_t n = chain_config.count, r = ui.song_row, k;
    if (!d && r < n && chain_config.row[r].slot == s)
        return r;
    if (!d) r = n - 1u;                                 /* (its first) */
    for (k = 0; k < n; k++)
        if (chain_config.row[r = (r + (d < 0 ? n - 1u : 1u)) % n].slot == s)
            return r;
    return ui.song_row;
}
/* OCT+ / F3..B3: part s loops (what plays: in on the next bar, the song cut; stopped: now, and PLAY) */
static void parts_audition(uint32_t s)
{
    ui.part = (uint8_t)s;
    chain_cut();
    live.mode = 0;
    if (!section_jump(s) && !song.playing && !transport_busy())
        transport_req = 1;
}
/* OCT-: the song again, from the row picked of the part (its first: KNOB 2) */
static void parts_song(void)
{
    live.mode = 1;
    ui.song_row = (uint8_t)part_row(ui.part, 0);
    if (!chain_goto(ui.song_row, 0))
        ui_message("THE SONG");
}
/* TIMELINE's PLAY: stopped, the cursor's row round and round; held, PLAY on it lets the song go on, on another row
 * that row held; else (a song going on, a loop) 0: PLAY stops as anywhere */
static int tl_play(void)
{
    uint32_t r = ui.song_row;
    if (ui.home || !SONG_TRK(cur_page()) || seq_counting() || (song.playing && !(chain.running && chain.hold)))
        return 0;
    if (song.playing && chain.row == r && !chain.loop) {
        fm1_irq_off();
        chain.hold = 0;
        sec_stage.ready = 0;                            /* (song_poll stages the next row) */
        fm1_irq_on();
        ui_message("THE SONG GOES ON");
    } else if (!chain_goto(r, 1))
        ui_message("LOOP ROW");
    return 1;
}
/* TIMELINE's keys: F3..D4 T1..T6 muted / sounding in the cursor's row (its scene: none yet, the state now, as
 * ARRANGE's SCENE; the row playing: at once), F4..B4 the ALGORITHM track's FROM A..D (its letter again: the row's own) */
static __attribute__((noinline)) void tl_keys(uint32_t pressed)
{
    uint32_t k, p, r = ui.song_row;
    for (k = 0; k < 27u; k++) {
        if (!((pressed >> k) & 1u) || key_black(k))
            continue;
        p = key_place(k);
        if (SONG_PARTS(cur_page()) || SONG_CLIPS(cur_page())) {
            if (p < 4u && SONG_CLIPS(cur_page())) {      /* (V10.6s) CLIPS: the whole part, every track */
                ui.part = (uint8_t)p;
                clip_launch(NTRK, p);
            } else if (p < 4u) parts_audition(p);
            continue;
        }
        if (r >= chain_config.count)
            continue;
        if (p < NTRK) {
            scene_t *sc = &song_idx.scene[song_cur][r];
            uint32_t b;
            if (!sc->on)
                scene_capture(sc);
            if (p < 4u) sc->mute ^= (uint8_t)(1u << p);
            else sc->on ^= (uint8_t)(1u << (p - 3u));
            b = p < 4u ? (sc->mute >> p) & 1u : (sc->on >> (p - 3u)) & 1u;
            if (chain.running && !chain.loop && chain.row == r)
                trk[p].p[P_MUTE] = (int16_t)b;
            ui_message(b ? "MUTED IN THE ROW" : "PLAYS IN THE ROW");
        } else if (p >= 7u && p <= 10u && !chain_busy()) {
            uint32_t *m = &song_idx.tsrc[song_cur][r], sh = 3u * (song.sel % NTRK), v = p - 6u;
            *m = (*m & ~(7u << sh)) | ((*m >> sh & 7u) == v || chain_config.row[r].slot == v - 1u ? 0u : v) << sh;
        } else
            continue;
        song_idx_touch();
    }
}

static void edit_param(uint32_t slot, int32_t steps)
{
    int16_t *vp;
    const page_t *pg = cur_page();
    const param_desc_t *d;
    int32_t v;
    if (pg->graph == GR_LIVE) {                         /* (p2) GLO > LIVE (ui_live.c) */
        live_knob(slot, steps);
        return;
    }
    if (BAR_PAGE(pg) && slot == 0u) {                    /* (0.9.2) SEQ > BAR: KNOB 1 the bar (no wrap), the cursor at its
                                                         * place in it */
        uint32_t len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP), nb = (len + 15u) / 16u;
        uint32_t b = (uint32_t)clamp((int32_t)ui.bank + steps, 0, (int32_t)nb - 1), at = b * 16u + ui.cursor % 16u;
        cursor_set((int32_t)(at < len ? at : len - 1u));
        return;
    }
    if (pg->graph == GR_COMP) {
        comp_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_GCOMP) {                        /* (0.9.9-seq2f) GLO > COMPS / COMPS 2 (ui.c) */
        gco_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_SHAPE) {                        /* (0.6.85) STRETCH, REPEAT (picked, OCT+ does it), NUDGE (now) */
        shape_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_FUGUE) {                        /* (0.6.52) HEAD, RATE, DIR, INTERVAL */
        fg_set_t *f = &fg_set[song.sel % NTRK][fg_ui % FG_HEADS];
        int32_t d = steps > 0 ? 1 : -1;
        if (slot == 0u) fg_ui = (uint8_t)clamp((int32_t)fg_ui + d, 0, FG_HEADS - 1);
        else if (slot == 1u) {                          /* (V10.27: over FG_ORD, 1/16 1/8 after OFF) */
            uint32_t r;
            for (r = 0; r < FG_NRATE && FG_ORD[r] != f->rate; r++) {}
            f->rate = FG_ORD[clamp((int32_t)r + d, 0, FG_NRATE - 1)];
        }
        else if (slot == 2u) f->dir = (uint8_t)clamp((int32_t)f->dir + d, 0, 3);
        else f->ivl = (int8_t)clamp((int32_t)f->ivl + d, -14, 14);
        return;
    }
    if (pg->graph == GR_CHANCE) {
        if (slot == 0u) cursor_set(ui.cursor + steps);
        else if (slot == 1u || slot == 2u) {
            if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
            step_t *st = &TSEL->step[ui.cursor];
            if (slot == 1u)
                step_set_chance(st, (uint32_t)clamp((int32_t)step_chance(st) + steps, 0, 100));
            else                                      /* RATCH x1..x4 */
                step_set_ratchet(st, (uint32_t)clamp((int32_t)step_ratchet(st) + steps, 1, 4));
        }
        return;
    }
    if (pg->graph == GR_DXSND) {                          /* EDIT > SOUND: the DRUM-X kit (ui_dx.c) */
        dx_knob(pg, slot, steps);
        return;
    }
    if (pg->graph == GR_SWARM) {                          /* (V10.6z6) EDIT > SWARM: FAR / NEAR (ui_dx.c) */
        sw_ui_knob(pg, slot, steps);
        return;
    }
    if (pg->graph == GR_FOCUS) {                          /* (0.8.5) EDIT > FOCUS */
        focus_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_STAGE && slot < 2u) {             /* (0.8.5) ENV > STAGE: the stage, its value */
        if (slot == 0u)
            ui.stage = (uint8_t)clamp((int32_t)(ui.stage & 3u) + (steps > 0 ? 1 : -1), 0, 3);
        else {
            uint32_t id = P_ATK + (ui.stage & 3u);
            TSEL->p[id] = (int16_t)param_turn(&TP[id], TSEL->p[id], accel(EN_K1 + 1u, steps, 127));
            knob_cap(TSEL, id);
        }
        return;
    }
    if (DX_KIT3(pg)) {                                    /* (0.6.54) KIT 3: BP, FEEDBK (0.6.58: IMAGE on STEREO) */
        dx_kit3_knob(slot, steps);
        return;
    }
    if(pg->graph==GR_LYRSTEP) {
        if(!slot) cursor_set((int32_t)ui.cursor+steps);
        if(slot==1u) {lyric_word=(uint8_t)clamp((int32_t)lyric_word+steps,0,31);lyric_syl=1;}
        if(slot==2u) lyric_syl=(uint8_t)clamp((int32_t)lyric_syl+steps,0,19);
        return;
    }
    if(pg->graph==GR_TRACKGEN) {trackgen_knob(slot,steps);return;}
    if (pg->graph == GR_EVENTS) {                         /* AUTO LIST (ui_events.c) */
        ev_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_MOTION) {
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        if (slot == 0u) motion_set_enabled(TSEL, steps > 0);
        else if (slot == 3u) ui.act = steps > 0 ? 4u : 0u;
        return;
    }
    if (SONG_PARTS(pg)) {                               /* (V10.6o) PARTS: KNOB 1 the part, 2 the song's row of it (OCT-
                                                         * plays from there), 4 what PLAY plays (LOOP / SONG); 3 nothing */
        if (slot == 0u)
            ui.part = (uint8_t)clamp((int32_t)ui.part + steps, 0, 3);
        else if (slot == 1u)
            ui.song_row = (uint8_t)part_row(ui.part, steps);
        else if (slot == 3u)
            live.mode = steps > 0;
        return;
    }
    if (SONG_CLIPS(pg)) {                               /* (V10.6s) CLIPS: KNOB 1 the track, 2 the part (the cell) */
        if (slot == 0u)
            track_select((uint32_t)clamp((int32_t)song.sel + steps, 0, NTRK - 1));
        else if (slot == 1u)
            ui.part = (uint8_t)clamp((int32_t)ui.part + steps, 0, 3);
        return;
    }
    if (SONG_TRK(pg)) {                                 /* (V10.6o) TIMELINE: ROW, its SECT, its BARS, the VAR of the track
                                                         * ALGORITHM picks (its mute: the keys) */
        chain_row_t *r;
        if (slot == 0u) {
            ui.song_row = (uint8_t)clamp((int32_t)ui.song_row + steps, 0, chain_config.count ? chain_config.count - 1 : 0);
            return;
        }
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        if (!chain_config.count) { ui_message("NO ROW"); return; }
        if (ui.song_row >= chain_config.count) ui.song_row = chain_config.count - 1u;   /* (ARRANGE's ADD / END) */
        r = &chain_config.row[ui.song_row];
        if (slot == 1u) r->slot = (uint8_t)clamp((int32_t)r->slot + steps, 0, 3);
        else if (slot == 2u) r->bars = (uint8_t)clamp((int32_t)r->bars + steps, 1, (int32_t)CHAIN_BARS);
        else {
            uint32_t *w = &song_idx.tvar[song_cur][ui.song_row], sh = 4u * (song.sel % NTRK);
            *w = (*w & ~(15u << sh)) | (uint32_t)clamp((int32_t)(*w >> sh & 15u) + steps, 0, RV_N - 1) << sh;
        }
        song_idx_touch();
        return;
    }
    if (pg->graph == GR_SONG) {
        uint32_t end = song_end_row();                  /* (0.6.22) the rows, + ADD, then the song's END */
        if (slot == 0u) {
            ui.song_row = (uint8_t)clamp((int32_t)ui.song_row + steps, 0, (int32_t)end);
            return;
        }
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        if (ui.song_row >= end) {                       /* END: KNOB 2 STOP / x2 .. x8 / LOOP (the song's times) */
            if (slot == 1u) {
                SONG_END(&chain_config) = (uint8_t)song_end_step(SONG_END(&chain_config), steps > 0 ? 1 : -1);
                song_idx_touch();
            }
            return;
        }
        if (slot == 3u) {                     /* (JIANT) SCENE: right stores the state now (mutes, macros, punch-in), */
            scene_t *sc = &song_idx.scene[song_cur][ui.song_row % CHAIN_ROWS];   /* left takes it away */
            if (ui.song_row >= chain_config.count) { ui_message("NO ROW"); return; }
            if (steps > 0) { scene_capture(sc); ui_message("SCENE STORED"); }
            else if (sc->on) { memset(sc, 0, sizeof *sc); ui_message("SCENE CLEARED"); }
            song_idx_touch();
            return;
        }
        if (ui.song_row >= chain_config.count) {
            chain_row_t *r = &chain_config.row[ui.song_row];
            r->slot = ui.song_row ? chain_config.row[ui.song_row - 1u].slot : 0u;
            r->bars = 1;
            song_idx.tsrc[song_cur][ui.song_row] = 0;
            song_idx.tvar[song_cur][ui.song_row] = 0;
            chain_config.count = ui.song_row + 1u;
            if (slot == 1u) return;
        }
        if (slot == 1u && steps < 0 && chain_config.row[ui.song_row].slot == 0u) {
            confirm_open(CF_DEL_ROW, ui.song_row);      /* (0.6.22) SECT left of A: DELETE ROW? (the dialog) */
            return;
        }
        if (slot == 1u)
            chain_config.row[ui.song_row].slot = (uint8_t)clamp((int32_t)chain_config.row[ui.song_row].slot + steps, 0, 3);
        if (slot == 2u)
            chain_config.row[ui.song_row].bars = (uint8_t)clamp((int32_t)chain_config.row[ui.song_row].bars + steps, 1,
                                                                (int32_t)CHAIN_BARS);
        return;
    }
    if (chain_busy() && (pg->scope == SC_STEP || pg->graph == GR_STEPS ||
        0)) {
        ui_message("SONG PLAYS: STOP IT TO EDIT"); return;
    }
    if (pg->scope == SC_STEP) {
        step_edit(slot, steps);
        return;
    }
    if (pg->scope == SC_TRK) {
        tracks_edit(slot, steps);
        return;
    }
    if (pg->graph == GR_BROWSE) {                         /* KNOB 1: one preset, KNOB 2: the next / previous engine */
        if (slot < 2u && !br_armed) {                    /* (V10.24) the sound before the browse: OCT- brings it back */
            undo_mark();
            br_armed = 1;
        }
        if (slot == 0u) {
            preset_step(steps);
        } else if (slot == 1u) {
            select_engine(eng_step(TSEL->eng_req, steps));
        } else if (slot == 2u) {
            preset_mark(steps > 0);
        } else if (slot == 3u) {                         /* (p1, main 0.9.12) LIST: ALL FAV, then the types */
            list_turn(steps);
        }
        return;
    }
    if (pg->graph == GR_TOOLS) {                          /* (0.6.25) TOOLS: a list, any knob (and PRESETS) moves the
                                                         * pick down / up; OCT+ does it (its dialog first) */
        int32_t a = ui.act ? (int32_t)ui.act - 1 : 0, sc = TL_SC[a];   /* (V10.24) KNOB 1 the scope (its first), the */
        if (slot == 0u && ui.act) {                      /* others the action (across the scopes) */
            sc = clamp(sc + (steps > 0 ? 1 : -1), 0, 3);
            for (a = 0; TL_SC[a] != sc; a++) {}
        } else if (ui.act)
            a = clamp(a + (steps > 0 ? 1 : -1), 0, 5);
        ui.act = (uint8_t)(a + 1);
        return;
    }
    if (SPLIT_PAGE(pg)) { split_edit(slot, steps); return; }
    if (FIT_PAGE(pg) && (slot == 1u || slot == 2u)) {
        uint32_t v = TSEL->seq_fit, span = fit_span(TSEL);
        if (slot == 1u) span = (uint32_t)clamp((int32_t)span + steps, 1, 64);
        else v = (v & 127u) | (steps > 0 ? 128u : 0u);
        fit_ui_set(TSEL, (uint8_t)((v & 128u) | span));
        ui.force = 1;
        return;
    }
    if (pg->graph == GR_SONGSAVE && slot == 1u) {
        song_save_loop = (uint8_t)clamp((int32_t)song_save_loop + steps, 0, 1); return;
    }
    if (pg->graph == GR_SONGSAVE && slot == 0u) {
        song_save_target = (uint8_t)clamp((int32_t)song_save_target + steps, 0, NSONG - 1);
        return;
    }
    if (pg->graph == GR_SLOTS && save_on() && save_level() == SV_PROJ) {   /* (0.8.5) SAVE > PROJECT's SECTIONS: KNOB 1
                                                         * the section, KNOB 2 its name (shown; EDIT renames it) */
        if (slot == 1u) {                                 /* (V10.17) KNOB 2 ERASE: right picks it, left drops it */
            ui.act = steps > 0 ? 2u : ui.act == 2u ? 0u : ui.act;
            return;
        }
        if (slot == 0u) {
            slot = 1u;                                    /* (KNOB 1 the section: SECT's slot, not an action) */
            goto sect;
        }
    }
    if ((act_cols() >> slot) & 1u) {                      /* an action's knob picks it (right) or drops it (left); */
        if (pg->graph != GR_PATS)                         /* OCT+ does it (act_do) */
            ui.act = steps > 0 ? (uint8_t)(slot + 1u) : ui.act == slot + 1u ? 0u : ui.act;
        return;
    }
sect:
    if (pg->graph == GR_SLOTS && slot == 0u) {            /* (0.6.12) KNOB 1 the project: its sections, the music kept */
        int32_t n = clamp((int32_t)(song_draft ? song_save_target : song_cur) + (steps > 0 ? 1 : -1), 0, (int32_t)NSONG - 1);
        if (song_draft) song_save_target = (uint8_t)n;
        else if ((uint32_t)n != song_cur) (void)song_open((uint32_t)n);
        return;
    }
    if (pg->graph == GR_USER) {                           /* KNOB 1 the slot */
        if (slot == 0u)
            ui.uslot = (uint8_t)clamp((int32_t)ui.uslot + steps, 0, UP_SLOTS - 1);
        return;
    }
    if (pg->graph == GR_PATS) {                          /* KNOB 1 the pattern */
        if (slot == 0u)
            ui.ppick = (uint8_t)clamp((int32_t)pat_pick() + steps, 0, (int32_t)pat_count() - 1);
        return;
    }
    if (pg->graph == GR_SLPIT && slot == 0u) {           /* (JIANT 0.6.3) SL PITCH: KNOB 1 the step, 2 its pitch */
        sl_ui_step = (uint8_t)clamp((int32_t)sl_ui_step + steps, 0, 15);
        return;
    }
    if (pg->graph == GR_MSEQ && slot == 0u) {            /* (JIANT) MSEQ: KNOB 1 the step, 2 its level */
        ms_ui_step = (uint8_t)clamp((int32_t)ms_ui_step + steps, 0, 15);
        return;
    }
    if (fop_page(pg->graph)) {                           /* (0.6.10) FM6's operators: KNOB 1 OP, 2..4 its bytes (ui_fm6op.c) */
        fop_knob(slot, steps);
        return;
    }
    if (pg->graph == GR_MOD && slot == 0u) {             /* MOD: KNOB 1 the slot, 2..4 its SRC DST AMT */
        mod_ui_slot = (uint8_t)clamp((int32_t)mod_ui_slot + (steps > 0 ? 1 : -1), 0, 3);
        return;
    }
    if (pg->graph == GR_MOD && slot == 2u) {
        int16_t *dst = &TSEL->p[P_M1DST + 3u * mod_ui_slot];
        *dst = (int16_t)mod_dst_turn(TSEL, *dst, steps);
        knob_cap(TSEL, P_M1DST + 3u * mod_ui_slot);
        return;
    }
    if (pg->id[0] == G_MIDI && pg->scope == SC_GLOBAL) {   /* (V10.6z8, a Discord user) GLO > SYSTEM 2 shows, edits nothing */
        ui_message("EDIT: MENU > BLUETOOTH");
        str_cpy(ui.msg2, "HOLD HOME, TAB SYSTEM", sizeof ui.msg2);
        return;
    }
    d = page_desc(pg, slot, &vp);
    if (!d || !vp || d->max == d->min)
        return;
    v = param_turn(d, *vp, accel(EN_K1 + slot, steps, d->fmt == F_ENUM ? 0 : d->max - d->min));
    {
        int32_t was = *vp;
        if ((TSEL->seq_fit & 128u) && (vp == &TSEL->p[P_SLEN] || vp == &TSEL->p[P_SDIV]))
            fit_ui_edit(TSEL, vp, (int16_t)v);
        else *vp = (int16_t)v;
        excite_turn(TSEL, vp, was);
    }
    if (vp >= sc_v && vp < sc_v + 4) {                  /* (V10.25) SCENE's SCENE TRACK ROLE; (V10.6j) PATTERN STYLE */
        sc_put((uint32_t)(vp - sc_v), v);
        return;
    }
    if (vp >= eu_v && vp < eu_v + EK_N) {               /* (V10.6) EUCLID / MODES: the setting, the pattern again */
        eu_put((uint32_t)(vp - eu_v), v);
        return;
    }
    if (vp == &trks_ui) {                               /* (0.6.18) GLO > SYSTEM's TRKS: the MENU's setting, saved */
        trks_put(v);
        ui_message(v ? "6 TRACKS: MORE CPU" : "4 TRACKS: T5-T6 ASLEEP");
        settings_save();
        return;
    }
    if (vp == &lmode_ui || vp == &lphase_ui) {          /* (0.9.18) LFO MODE / PHASE: P_LPHASE written back */
        lfo_view_put(TSEL, vp == &lmode_ui, v);
        if (!mom_on)
            knob_cap(TSEL, P_LPHASE);
        return;
    }
    if (vp == &load_ui) {                               /* (0.6.30) GLO > SYSTEM's LOAD: the same */
        load_put(v);
        ui_message(v == 2 ? "LOAD FULL: ALL FX" : v ? "LOAD LITE: LIGHTER FX" : "LOAD ECO: NO SLICER/CHORUS");
        settings_save();
        return;
    }
    if (!mom_on && pg->scope != SC_GLOBAL && vp >= TSEL->p && vp < TSEL->p + P_COUNT)   /* (0.7.6: SCL's ALL TR is a
                                                         * global; 0.9.9-seq2e: MOMENTARY records nothing) */
        knob_cap(TSEL, (uint32_t)(vp - TSEL->p));
    else if (vp >= song.g && vp < song.g + G_COUNT)
        mo_knob(7, (uint32_t)(vp - song.g));            /* (V10.6z3) a global: NRPN 1 on channel 16 (MIDI OUT ALL) */
}


/* (V10.24) SAVE > TOOLS: OCT+ pressed on the picked action: held TL_HOLD it is done (no dialog) */
static void tools_arm(void)
{
    uint32_t c = act_col();
    if (!c--) return;
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    if (!act_ready()) {                               /* nothing there to clear or delete */
        ui_message(c == 2u ? "NOTHING TO DELETE" : "NOTHING TO CLEAR");
        return;
    }
    tl_kind = (uint8_t)(c == 0u ? CF_CLEAR_SEQ : c == 1u ? CF_INIT_SOUND : c == 2u ? CF_DEL_ROW : c == 4u ? CF_INIT_SYS :
                        c == 5u ? CF_CPU_TEST : CF_CLEAR_SONG);
    tl_trk = (uint8_t)(c == 2u ? ui.song_row : song.sel);
    tl_t0 = fm1_ms | 1u;
}
/* OCT+ on an action page: the picked action. A load stays picked (browse and load again); the others
 * are dropped once done. Flash writes only while stopped; over the user's data: the dialog */
static __attribute__((noinline)) void act_do(void)
{
    uint32_t c = act_col(), id, k = (uint32_t)song.g[G_SLOT] - 1u;
    if (!c--)
        return;
    if(cur_page()->graph==GR_TRACKGEN) {trackgen_make();return;}
    if (cur_page()->graph == GR_MOTION) {
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        confirm_open(CF_CLEAR_MOTION, song.sel);
        return;
    }
    if (cur_page()->graph == GR_LYRIC) {                /* (0.8.5) WORDS: the lyric, typed (the NAME screen) */
        name_open(NK_LYRIC, song.sel);
        return;
    }
    if(cur_page()->graph==GR_LYRSTEP) {
        char label[20];uint32_t pos=lyric_pick(label);
        if(chain_busy()) {ui_message("SONG PLAYS: STOP IT TO EDIT");return;}
        if(!label[0]) {ui_message("TYPE A LYRIC FIRST");return;}
        undo_mark();
        uint32_t err=motion_set_lock(TSEL,ui.cursor,P_E0,(int16_t)pos);
        if(err) {ui_message(err==2u?"AUTOMATION FULL":"CANNOT PLACE LYRIC");return;}
        TSEL->p[P_VLYR]=LYR_FIXED;
        ui_message(pos?"SYLLABLE PLACED":"AUTO ADVANCE PLACED");
        return;
    }
    if (cur_page()->graph == GR_PUNCH) {                /* (0.6.99) the punch-in lane cleared (a dialog) */
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        confirm_open(CF_CLEAR_PFX, song.sel);
        return;
    }
    if (cur_page()->graph == GR_EVENTS) {
        ev_oct();
        return;
    }
    if (cur_page()->graph == GR_TOOLS)                   /* (V10.24) OCT+ held: tools_arm, on the press */
        return;
    if (SONG_PARTS(cur_page())) {                       /* (V10.6o) PARTS: the part picked loops */
        parts_audition(ui.part);
        return;
    }
    if (SONG_CLIPS(cur_page())) {                       /* (V10.6s) CLIPS: the cell in on the next bar */
        clip_launch(song.sel, ui.part);
        return;
    }
    if (cur_page()->graph == GR_SONG) {                 /* (0.8.5) SAVE > PROJECT's ARRANGE: OCT+ adds a row; (V10.6o) after
                                                         * the one picked, a copy of it (its scene, tracks, variations; on
                                                         * ADD / END: of the last; none: A, 4 bars), the new one selected */
        uint32_t n = chain_config.count, c = ui.song_row < n ? ui.song_row : n - 1u, i;
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        if (n >= CHAIN_ROWS) { ui_message("16 ROWS"); return; }
        if (!n) {
            row_copy(0, CHAIN_ROWS);                    /* (no scene yet) */
            c = (uint32_t)-1;
        } else
            for (i = n; i > c; i--)
                row_copy(i, i - 1u);
        ui.song_row = (uint8_t)(c + 1u);
        chain_config.count++;
        song_idx_touch();
        ui_message("ROW ADDED");
        return;
    }
    if (cur_page()->graph == GR_PATS) {
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
        if (pat_needs_confirm(TSEL))                      /* the user's steps: the dialog */
            confirm_open(CF_LOAD_PAT, song.sel);
        else {                                            /* empty, or a pattern loaded and untouched */
            undo_mark();
            pat_load_ui(TSEL, pat_pick());
        }
        return;
    }
    if (cur_page()->graph == GR_USER) {                   /* 1 LOAD, 2 ERASE, 3 SAVE (the NAME screen first) */
        if (c > 1u)
            ui.act = 0;
        if (c == 2u && up_used(ui.uslot) && !transport_busy())
            confirm_open(CF_ERASE_USER, ui.uslot);        /* ERASE: the dialog first */
        else if (c != 3u)
            up_ui(c - 1u, ui.uslot);
        else if (transport_busy())
            ui_message("STOP TO SAVE");
        else if (up_used(ui.uslot))
            confirm_open(CF_OVR_USER, ui.uslot);
        else
            name_open(NK_USER_SAVE, ui.uslot);
        return;
    }
    if (cur_page()->graph == GR_SONGSAVE) {
        char title[13];
        ui.act = 0;
        if (transport_busy()) ui_message("STOP TO SAVE");
        else if (lib_scan(song_save_target, title) || ((!song_draft || song_save_target != song_cur) && song_idx.rows[song_save_target].count))
            confirm_open(CF_OVR_SONG, song_save_target);
        else name_open(NK_SONG_SAVE, song_save_target);
        return;
    }
    if (cur_page()->graph == GR_SLOTS && save_level() == SV_LIB) {
        uint32_t target = song_draft ? song_save_target : song_cur;
        if (c == 1u) {
            ui.act = 0;
            if (transport_busy()) ui_message("STOP TO ERASE");
            else confirm_open(CF_ERASE_SONG, target);
        } else if (c == 2u && song_draft) (void)song_select(target);
        else if (c == 3u) nav_put(SEC_SAVE, "SAVE SONG AS");
        else goto slot_action;
        return;
    }
    if (cur_page()->graph == GR_SLOTS && c == 1u) {     /* (V10.17) SECTIONS: ERASE, the dialog first */
        ui.act = 0;
        if (transport_busy()) ui_message("STOP TO ERASE");
        else if (!project_used(k)) ui_message("EMPTY SECTION");
        else confirm_open(CF_ERASE_SECT, (song_cur << 2u) | (k & 3u));
        return;
    }
slot_action:
    id = cur_page()->id[c & 3u];
    if (id != G_LOAD)
        ui.act = 0;
    switch (id) {
    case G_LOAD:
        undo_mark();                                      /* (0.6.21) UNDO: the music before the load */
        project_load(k);
        break;
    case G_SAVE:                                          /* the NAME screen writes it */
        if (transport_busy())
            ui_message("STOP TO SAVE");
        else if (project_used(k))
            confirm_open(CF_OVR_PROJ, k);
        else
            name_open(NK_PROJ_SAVE, k);
        break;
    case G_CLRSEQ:
        if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); break; }
        confirm_open(CF_CLEAR_SEQ, song.sel);
        break;
    default:                                              /* G_INITSND */
        set_engine(TSEL->eng_req);                        /* engine defaults + its first preset (the steps stay) */
        vel_at_route(TSEL);                               /* (V10.6z6: the keys' pressure audible, vel.c) */
        ui_message("SOUND INIT");
        ui.force = 1;
        break;
    }
}

/* (V10.6z4) the expression keys (seq.c kx_block): OCT- / OCT+ pressed while a key's note sounds (MENU > AUDIO > OCT
 * BEND not OFF), where OCT would change the octave: the tracks the keys hold notes of vibrato / bend while it is held,
 * the octave as it was; with no note held, the octave as ever. Both held so: no UPDATE MODE (main.c) */
static const uint8_t XB_SEMI[8] = {0, 1, 2, 3, 4, 5, 7, 12};
static uint8_t kx_recd KX_BSS;                          /* the selected track's gates as last recorded */
/* OCT- / OCT+ pressed where they are the octave: the octave (both: back to 0), or with a note held the expression */
static __attribute__((noinline)) void oct_key(uint32_t b, uint32_t both)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        if (kb_on[k])
            m |= 1u << (kb_trk[k] % NTRK);
    if (kx_semi && m) {
        kx_g[b == B_OCTUP] |= (uint8_t)m;
        return;
    }
    if ((fm1_in.buttons & both) == both)
        song.octave = 0;
    else
        song.octave += b == B_OCTDN ? (song.octave > -3 ? -1 : 0) : (song.octave < 3 ? 1 : 0);
    if (drum_track(TSEL) && !song.grid)         /* (0.8.5) the kit's octaves: what this one does */
        ui_message(drum_oct_label(TSEL,song.octave));
}
/* a frame: OCT let go ends its gates; recording, the selected track's into its automation (motion.c) */
static __attribute__((noinline)) void kx_frame(void)
{
    uint32_t g, i = song.sel;
    kx_semi = XB_SEMI[(oct_xp ^ 2u) & 7u];
    for (g = 0; g < 2u; g++)
        if (!((fm1_in.buttons >> panel.btn[g ? B_OCTUP : B_OCTDN]) & 1u))
            kx_g[g] = 0;
    g = ((kx_g[0] >> i) & 1u) | ((kx_g[1] >> i) & 1u) << 1 | (uint32_t)(vs.atl[i % NTRK] >> 4) << 2;   /* (V10.6z6: and
                                                         * the keys' pressure, 0..7, seq.c kx_block) */
    if (g != kx_recd && rec_on(TSEL) && !drum_track(TSEL))   /* (only recording: else it would become the base) */
        (void)motion_capture(TSEL, P_XPR, (int16_t)(127 - (int32_t)g));
    kx_recd = (uint8_t)g;
}

/* OCT- / OCT+ where they answer (the dialogs, the menu, action pages): on release, and only a press
 * that began there; both down together (UPDATE MODE, main.c) is no tap. Bit 0 OCT-, bit 1 OCT+ */
static uint32_t oct_taps(uint32_t pressed, int here)
{
    static uint8_t down, chord;
    uint32_t dn = panel.btn[B_OCTDN], up = panel.btn[B_OCTUP];
    uint32_t now = ((fm1_in.buttons >> dn) & 1u) | ((fm1_in.buttons >> up) & 1u) << 1, tap;
    if (here)
        down |= (uint8_t)(((pressed >> dn) & 1u) | ((pressed >> up) & 1u) << 1);
    if (now == 3u)
        chord = 1;
    tap = down & ~now;
    down &= (uint8_t)now;
    if (chord) {
        tap = 0;
        chord = now != 0u;
    }
    return tap;
}

/* (JIANT) REC held, then a page button: that part's automation goes, both kinds (the knob moves and the
 * step locks), the stored values stay. FX: the selected track's FX and SLICER and the punch-in lane; EDIT ENV LFO SCL
 * ARP: the selected track's parameters of those pages; GLO: every track's LEVEL and PAN. No dialog (SEQ + REC, which
 * clears every sequence, asks). The other order is REC in a layer: it arms, as always (FX: to record the punch-in lane) */
static int rec_combo_btn(uint32_t b)
{
    return b == B_FX || b == B_EDIT || b == B_ENV || b == B_LFO || b == B_SCL || b == B_ARP || b == B_GLO;
}
/* REC held, then page button b pressed: REC's tap and layer dead (1 = rec_clear_auto's) */
static int rec_first(uint32_t b)
{
    if (!rec_combo_btn(b) || ui.menu || ui.confirm || !((fm1_in.buttons >> panel.btn[B_REC]) & 1u))
        return 0;
    ui.rec_t0 |= 2u;
    if (ui.ly == LAYER_REC)
        ui.ly_t0 |= 8u;                                 /* (LY_DEAD: no REC map, no arming tap) */
    return 1;
}
static void rec_clear_auto(uint32_t b)
{
    uint32_t m[(P_COUNT + 31u) / 32u], i, f, n = 0, k;
    char msg[28];
    const char *name = b == B_FX ? "FX" : b == B_EDIT ? "EDIT" : b == B_ENV ? "ENV" : b == B_LFO ? "LFO" :
                       b == B_SCL ? "SCL" : b == B_ARP ? "ARP" : "MIX";
    if (chain_busy()) {
        ui_message("SONG PLAYS: STOP IT TO EDIT");
        return;
    }
    memset(m, 0, sizeof m);
    if (b == B_GLO) {
        m[P_LEVEL / 32u] |= 1u << (P_LEVEL % 32u);
        m[P_PAN / 32u] |= 1u << (P_PAN % 32u);
    } else {
        for (f = FAM_HOME + 1u; f < FAM_COUNT && FAM_BTN[f] != b; f++)
            ;
        for (i = 0; i < NPAGES; i++)
            if (PAGES[i].fam == f && (PAGES[i].scope == SC_TRACK || PAGES[i].scope == SC_ENGINE))
                for (k = 0; k < 4u; k++)
                    if (PAGES[i].id[k] < P_COUNT && !(f == FAM_EDIT && PAGES[i].id[k] == P_DIST))   /* (EDIT > FILTER's
                                                         * DRV is the part's DIST: FX's) */
                        m[PAGES[i].id[k] / 32u] |= 1u << (PAGES[i].id[k] % 32u);
    }
    for (k = 0; k < NTRK; k++)
        if (b == B_GLO || k == song.sel)
            n += motion_clear_ids(&trk[k], m);
    if (b == B_FX)
        for (i = 0; i < sizeof pfx_lane; i++) {
            n += pfx_lane[i] != 0;
            pfx_lane[i] = 0;
        }
    str_cpy(msg, n ? "" : "NO ", sizeof msg);
    str_cpy(msg + str_len(msg), name, sizeof msg - str_len(msg));
    str_cpy(msg + str_len(msg), n ? " AUTOMATION CLEARED" : " AUTOMATION", sizeof msg - str_len(msg));
    ui_message(msg);
    ui.force = 1;
}

/* SEQ step entry = step recording (1.2, Discussion #133: only with the track armed and the transport stopped), acid
 * style: the keys pressed together (POLY: up to 4 notes, MONO: the last one) become the cursor step; releasing all
 * keys moves on one step (wrapping inside LEN) */
/* (0.6.37) the entry let go: a chord held for LEN steps (TIEs after it), or arpeggiated (ARP pressed while held): its
 * notes over the octaves in the ORDER, one every RES steps; the cursor after it */
static const char *const ENT_ORD[4] = {"UP", "DOWN", "UP-DN", "RND"};
static const char *const ENT_RES[4] = {"1/16", "1/8", "1/8.", "1/4"};
static void ent_message(void)
{
    char a[24], b[24];
    if (ui.ent_arp) {
        str_cpy(a, "ARP ", sizeof a); str_cpy(a + str_len(a), ENT_ORD[ui.ent_ord & 3u], 8);
        str_cpy(b, ENT_RES[(ui.ent_res - 1u) & 3u], 8); str_cpy(b + str_len(b), " x", 4);
        fmt_int(b + str_len(b), ui.ent_oct); str_cpy(b + str_len(b), " OCT", 6);
    } else {
        str_cpy(a, "LEN ", sizeof a); fmt_int(a + str_len(a), ui.ent_len);
        str_cpy(a + str_len(a), ui.ent_len > 1u ? " STEPS" : " STEP", 8);
        str_cpy(b, "ARP: ARPEGGIATE", sizeof b);
    }
    str_cpy(ui.msg, a, sizeof ui.msg); str_cpy(ui.msg2, b, sizeof ui.msg2); ui.msg_t = 40;
}
static void ent_knob(uint32_t k, int32_t s)
{
    int32_t d = clamp(s, -4, 4);                        /* (detents, not accelerated: small ranges) */
    if (k == 0u && !ui.ent_arp) ui.ent_len = (uint8_t)clamp(ui.ent_len + d, 1, TSEL->p[P_SLEN] > 16 ? 16 : TSEL->p[P_SLEN]);
    else if (k == 0u) ui.ent_res = (uint8_t)clamp(ui.ent_res + d, 1, 4);
    else if (k == 1u) ui.ent_ord = (uint8_t)clamp(ui.ent_ord + d, 0, 3);
    else ui.ent_oct = (uint8_t)clamp(ui.ent_oct + d, 1, 3);
    ent_message();
}
static __attribute__((noinline)) void ent_commit(void)
{
    track_t *t = TSEL;
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, cur = ui.cursor, i, j, n = 0, span;
    uint8_t vel = t->step[cur].vel, list[24];
    if (!ui.ent_arp || !ui.ent_nn) {
        span = ui.ent_len < len ? ui.ent_len : len;
        for (i = 1; i < span; i++) {
            step_t *s = &t->step[(cur + i) % len];
            memset(s, 0, sizeof *s);
            s->time = ST_TIE; s->vel = vel;
        }
        cursor_set((int32_t)(cur + (span ? span : 1u)));
        return;
    }
    for (j = 0; j < ui.ent_oct; j++)                    /* the notes over the octaves, low to high */
        for (i = 0; i < ui.ent_nn; i++)
            if (ui.ent_notes[i] + 12u * j <= 127u) list[n++] = (uint8_t)(ui.ent_notes[i] + 12u * j);
    if (ui.ent_ord == 1u)                               /* DOWN */
        for (i = 0; i < n / 2u; i++) { uint8_t x = list[i]; list[i] = list[n - 1u - i]; list[n - 1u - i] = x; }
    else if (ui.ent_ord == 2u && n > 2u)                /* UP-DN: back down without the ends twice */
        for (i = n - 2u; i > 0u && n < sizeof list; i--) list[n++] = list[i];
    else if (ui.ent_ord == 3u)                          /* RND */
        for (i = n; i > 1u; i--) { uint32_t r = rng() % i; uint8_t x = list[i - 1u]; list[i - 1u] = list[r]; list[r] = x; }
    if (n * ui.ent_res > len) n = len / ui.ent_res ? len / ui.ent_res : 1u;
    span = n * ui.ent_res;
    for (i = 0; i < span; i++) {
        step_t *s = &t->step[(cur + i) % len];
        memset(s, 0, sizeof *s);
        s->vel = vel;
        if (i % ui.ent_res == 0u) { s->time = ST_NOTE; s->n = 1; s->note[0] = list[i / ui.ent_res]; }
        else s->time = ST_TIE;
    }
    cursor_set((int32_t)(cur + span));
}
static void ent_note(track_t *t, step_t *st, uint32_t note)   /* one note into the entry (the step, the arpeggio's) */
{
    uint32_t j;
    if (!ui.entry_open) {
        ui.entry_open = 1;
        st->n = 0;
        st->time = ST_NOTE;
        ui.ent_len = ui.ent_res = ui.ent_oct = 1; ui.ent_arp = ui.ent_ord = ui.ent_nn = 0;
    }
    for (j = 0; j < ui.ent_nn && ui.ent_notes[j] < note; j++)   /* (the arpeggio's notes, in order of pitch) */
        ;
    if (ui.ent_nn < sizeof ui.ent_notes && (j == ui.ent_nn || ui.ent_notes[j] != note)) {
        uint32_t m;
        for (m = ui.ent_nn; m > j; m--) ui.ent_notes[m] = ui.ent_notes[m - 1u];
        ui.ent_notes[j] = (uint8_t)note; ui.ent_nn++;
    }
    if (t->p[P_VOICE] && !ENGINES[t->engine]->oneshot) {   /* (drums: hits stack as a chord) */
        st->note[0] = (uint8_t)note;
        st->n = 1;
    } else if (st->n < 4u) {
        for (j = 0; j < st->n && st->note[j] != note; j++)
            ;
        if (j == st->n)
            st->note[st->n++] = (uint8_t)note;
    }
}
static void seq_entry(uint32_t pressed)
{
    track_t *t = TSEL;
    step_t *st = &t->step[ui.cursor];
    uint32_t k, ti = song.sel % NTRK;
    for (k = 0; k < 27u; k++) {
        uint32_t note, c, nc;
        uint8_t cn[KB_CH];
        if (!((pressed >> k) & 1u))
            continue;
        note = kb_map(t, k);
        if (note == KB_SILENT)
            continue;
        if (kb_sn[k] && kb_trk[k] == ti) {             /* (0.6.83) SCL > CHORD: the chord the key sounds (seq.c key_on), */
            nc = kb_sn[k];                              /* all of it into the step, as live recording does */
            memcpy(cn, kb_snd[k], KB_CH);
        } else                                          /* (the key not in yet: built here, LEAD's memory untouched) */
            nc = chord_peek(t, note, cn);
        for (c = 0; c < nc && c < KB_CH; c++)
            ent_note(t, st, cn[c]);
        last_note = (uint8_t)note;
    }
    if (ui.entry_open && !(fm1_in.notes & ~kb_layer))
        ent_commit();                                   /* (let go: LEN / the arpeggio written, the cursor on) */
}

/* (0.9.2, rule 3) PRESETS loads presets, on every page (the menu, a dialog, NAME and the layers: ui_input before this;
 * GEN: its GENRE, there): the selected track's sound, one list over every engine, then the used user presets
 * (preset_step; a sound load keeps the steps); in SEQ the patterns: the PHRASES loader over the page (KNOB 1 the pattern
 * too, OCT+ LOAD, OCT- back to the page), each detent the next one. Never a page, a track, a module, a step or a row
 * (until 0.9.1 it was the bar, the step, the row, the module, the track, ROOT ..: KNOB 1's now, rule 4) */
/* PRESETS: the sounds; (0.9.9-seq2i, the owner) on SEQ's pages the step picked, a step a detent (round in LEN), the same
 * step every SEQ page edits (STEP's cursor, COMP's, the locks'): the factory PATTERNS' loader it opened is gone (the
 * generator, MUTATE and the components make patterns enough) */
static void presets_turn(int32_t s)
{
    if (gco_on()) {
        gl_cur = (uint8_t)(((int32_t)gl_cur + s) & (GL_N - 1u));
        ui.force = 1;
        return;
    }

    if (!ui.home && PAGES[ui.page].fam == FAM_SEQ) {
        char b[12];
        cursor_set((int32_t)ui.cursor + s);
        str_cpy(b, "STEP ", sizeof b);
        fmt_int(b + 5, (int32_t)ui.cursor + 1);
        ui_message(b);
        return;
    }
    if (!ui.home && cur_page()->graph == GR_BROWSE && !br_armed) {   /* (V10.24) as KNOB 1 there */
        undo_mark();
        br_armed = 1;
    }
    preset_step(s);
}

/* (JIANT 0.5) SELECT off HOME: the selected track's sound diced. DRUM-X: both patches (A, B) of every sound, each
 * value moved up to +-40 from the factory kit's (COLOR up 12 at most: no knife edges), the wave one time in five
 * SINE or FM; a synth: one of its engine's
 * factory presets, then each engine parameter moved up to a quarter of its range (an ENUM: another value, one time in
 * three). Musical by construction: always near a sound that works */
static int32_t dice_by(int32_t v, int32_t lo, int32_t hi, int32_t span)
{
    return clamp(v + (int32_t)(rng() % (uint32_t)(2 * span + 1)) - span, lo, hi);
}
static void sound_dice_of(track_t *t, int quiet)
{
    uint32_t e = trk_eng(t), i, l;
    if (e == ENGI_DRUM && drum_elsewhere(t)) {          /* (0.8.3, reported again) the section's one kit is another
                                                         * track's: its drums stay as they are */
        static char m[16] = "KIT OF T? KEPT";
        m[8] = (char)('0' + drum_elsewhere(t));
        ui_message(m);
        return;
    }
    if (e == ENGI_DRUM) {
        for (l = 0; l < 8u; l++) {
            const dx_lane_t *D = &DX_KIT_DEF[l];
            dx_lane_t *L = &dx_kit[l];
            for (i = 0; i < DXP_N; i++) {               /* (COLOR: the brightness, down 40 but up 12 at most) */
                int32_t up = i == DXP_COLOR ? 12 : 127;
                L->a[i] = (uint8_t)dice_by(D->a[i], 0, D->a[i] + up < 127 ? D->a[i] + up : 127, 40);
                L->b[i] = (uint8_t)dice_by(D->b[i], 0, D->b[i] + up < 127 ? D->b[i] + up : 127, 40);
            }                                            /* (a new wave: SINE or FM, the gentle two) */
            L->mode = (uint8_t)((D->mode & ~3u) | (rng() % 5u ? (D->mode & 3u) : rng() % 2u));
        }
        for (l = 0; l < 8u; l++) {                      /* (0.6.49) LAYER strong where a note is in the way (snare,
                                                         * hats, rim, bell), light on the kick, clap and tom; FEEDBK
                                                         * one sound in three */
            static const uint8_t LO[8] = {0, 4, 0, 6, 6, 0, 3, 8}, HI[8] = {6, 15, 8, 15, 15, 7, 12, 15};
            uint32_t la = LO[l] + rng() % (HI[l] - LO[l] + 1u), fb = rng() % 3u ? 0u : 1u + rng() % 9u;
            dx_lay[l] = (uint8_t)(la | fb << 4);
        }
        if (!quiet) ui_message("KIT DICED");
        return;
    }
    if (ENGINES[e]->npresets)
        apply_preset_to(t, rng() % ENGINES[e]->npresets);
    for (i = 0; i < 8u; i++) {
        const param_desc_t *d = track_desc(t, P_E0 + i);
        int32_t span = (d->max - d->min) / 4;
        if (!d->label || d->label[0] == '-' || d->max <= d->min)
            continue;
        if (d->fmt == F_ENUM)
            t->p[P_E0 + i] = (int16_t)(rng() % 3u ? t->p[P_E0 + i] : d->min + (int32_t)(rng() % (uint32_t)(d->max - d->min + 1)));
        else if (d->fmt == F_SEMI)                      /* (V10.38) an interval: 0, 4th, 5th, octave, up or down */
            t->p[P_E0 + i] = (int16_t)semi_pick(rng(), d->min, d->max);
        else
            t->p[P_E0 + i] = (int16_t)dice_by(t->p[P_E0 + i], d->min, d->max, span ? span : 1);
    }
    if (!quiet) ui_message("SOUND DICED");
}
/* (0.6.67) a button held + SELECT: that part of the selected track diced (EDIT the sound, SEQ the sequence, LFO the
 * LFO, FX its effects and the shared reverb and delay), at once so it is heard, under a dialog: OCT+ OK keeps it, OCT-
 * CANCEL brings back what it was before the first dice (only that part: the music plays on), SELECT again dices again.
 * (0.6.65: the sound first.) UNDO (GLO + G4) still brings back what was before */
static const uint8_t DK_LFO[8] = {P_LRATE, P_LWAVE, P_LPHASE, P_LFADE, P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP};
static const uint8_t DK_FXT[9] = {P_DIST, P_DTYPE, P_DTONE, P_CHOR, P_DLY, P_REV, P_APDEP, P_APRATE, P_APWAVE};
static const uint8_t DK_FXG[6] = {G_RTYPE, G_RSIZE, G_RDAMP, G_DTIME, G_DFDBK, G_DPIT};
static struct {                                         /* what CANCEL puts back (the sound: undo_sound) */
    step_t step[NSTEP];
    int16_t slen, v[9], g[6];
    const char *msg;
} dk;
static const char *dice_seq(track_t *t);                /* gen.c */
static int32_t dk_rr(int32_t lo, int32_t hi) { return hi <= lo ? lo : lo + (int32_t)(rng() % (uint32_t)(hi - lo + 1)); }
static int32_t dk_frac(const param_desc_t *d, int32_t a, int32_t b)   /* a value a% .. b% up the range */
{
    int32_t span = d->max - d->min;
    return dk_rr(d->min + span * a / 100, d->min + span * b / 100);
}
/* (0.8.5) the DICE dialog's BEFORE: the sound's being as it was (its HOME knobs' features; the steps, LFO and FX before
 * are dk's) */
static struct { const genome_t *g; bfeat_t f; uint8_t roll; } dkv;
static const genome_t *dk_genome(const track_t *t)
{
    uint32_t e = trk_eng(t);
    return e == ENGI_DRUM ? &LANE_G_HOME : &GENOME[EGEN[e]];
}
static void dk_feat(const track_t *t, bfeat_t *f)
{
    uint8_t ids[4];
    uint32_t v;
    for (v = 0; v < 4u; v++) ids[v] = (uint8_t)trk_engp(t)->knob[v];
    bg_feat(t, dk_genome(t), ids, f);
}
static void dk_vis_take(void)
{
    dkv.g = dk_genome(TSEL);
    dk_feat(TSEL, &dkv.f);
    dkv.roll = 0;
}
static void dice_side(uint32_t now, int32_t x0)          /* one half of the panel: BEFORE (0) or NOW */
{
    const track_t *t = TSEL;
    uint32_t i, kind = ui.confirm;
    cv_text_in(x0, 6 - AF_X_CAP_Y, 112, &AF_X, now ? "NOW" : "BEFORE", T_DIM, T_SURF);
    if (kind == CF_DICE_SOUND) {
        bfeat_t f;
        if (!dkv.g)                                     /* (opened without a dice: as it is) */
            dk_vis_take();
        if (now)
            dk_feat(t, &f);
        being_lines(now ? dk_genome(t) : dkv.g, now ? &f : &dkv.f, (x0 + 56) * 16, 66 * 16, 14 * 16, 0, 0);
    } else if (kind == CF_DICE_SEQ) {                   /* the first 16 steps' notes as dots */
        const step_t *st = now ? t->step : dk.step;
        for (i = 0; i < 16u; i++)
            if (step_on(&st[i]))
                cv_rect(x0 + 4 + (int32_t)i * 7, 96 - clamp((int32_t)st[i].note[0] - 36, 0, 40) * 2, 5, 3,
                        now ? T_ACCENT : T_MID);
    } else {                                            /* LFO / FX: their four first values as bars */
        const uint8_t *ids = kind == CF_DICE_LFO ? DK_LFO : DK_FXT;
        for (i = 0; i < 4u; i++) {
            int16_t v = now ? t->p[ids[i]] : dk.v[i];
            const param_desc_t *d = track_desc(t, ids[i]);
            int32_t w = d ? clamp(RATIO(d, enum_rank(d, v)), 0, 1000) * 96 / 1000 : 0;
            cv_text(x0 + 8, 20 + (int32_t)i * 20 - AF_X_CAP_Y, &AF_X, d ? d->label : "", T_DIM);
            cv_rect(x0 + 8, 29 + (int32_t)i * 20, 96, 3, T_LINE);
            cv_rect(x0 + 8, 29 + (int32_t)i * 20, w ? w : 1, 3, now ? T_ACCENT : T_MID);
        }
    }
}
static void dice_panel(void)
{
    cv_begin(240, 148, T_BG);
    cv_rect(4, 0, 232, 148, T_SURF);
    cv_bg = T_SURF;
    cv_rect(119, 8, 1, 132, T_LINE);
    dice_side(0, 4);
    cv_rect(117, 0, 6, 148, T_SURF);                    /* (BEFORE's tendrils kept on its side) */
    cv_rect(119, 8, 1, 132, T_LINE);
    dice_side(1, 124);
    cv_rect(0, 0, 4, 148, T_BG);
    cv_rect(236, 0, 4, 148, T_BG);
    {   /* (0.8.9 COLORS: the panel's corners rounded, as the lens's) */
        int32_t j, i;
        for (j = 0; j < 4; j++)
            for (i = 0; i < 4 - j - (j == 0); i++) {
                cv_pset(4 + i, j, T_BG); cv_pset(235 - i, j, T_BG);
                cv_pset(4 + i, 147 - j, T_BG); cv_pset(235 - i, 147 - j, T_BG);
            }
    }
    cv_blit(0, LZ_CARD_Y);
}
static void dk_keep(uint32_t kind)
{
    track_t *t = TSEL;
    uint32_t i;
    if (kind == CF_DICE_SEQ) {
        for (i = 0; i < NSTEP; i++) dk.step[i] = t->step[i];
        dk.slen = t->p[P_SLEN];
    } else if (kind == CF_DICE_LFO) {
        for (i = 0; i < 8u; i++) dk.v[i] = t->p[DK_LFO[i]];
    } else if (kind == CF_DICE_FX) {
        for (i = 0; i < 9u; i++) dk.v[i] = t->p[DK_FXT[i]];
        for (i = 0; i < 6u; i++) dk.g[i] = song.g[DK_FXG[i]];
    }
}
static int dk_back(uint32_t kind, uint32_t k)           /* CANCEL: 0 done */
{
    track_t *t = &trk[k % NTRK];
    uint32_t i;
    if (kind == CF_DICE_SOUND)
        return undo_sound(k);
    if (kind == CF_DICE_SEQ) {
        load_begin(t, LOAD_PAT);
        for (i = 0; i < NSTEP; i++) t->step[i] = dk.step[i];
        t->p[P_SLEN] = dk.slen;
        load_end(t);
        latch_drop_req |= (uint8_t)(1u << (k % NTRK));
    } else if (kind == CF_DICE_LFO) {
        for (i = 0; i < 8u; i++) t->p[DK_LFO[i]] = dk.v[i];
    } else {
        for (i = 0; i < 9u; i++) t->p[DK_FXT[i]] = dk.v[i];
        for (i = 0; i < 6u; i++) song.g[DK_FXG[i]] = dk.g[i];
    }
    return 0;
}
/* the LFO: its rate in the middle of the range, any wave, MODE FREE or TRIG (any phase) alike, a fade now and then; aimed mostly at the filter,
 * a little at the shape, seldom at the pitch or the level (a wobble, not a siren) */
static void lfo_dice(track_t *t)
{
    const param_desc_t *d;
    d = track_desc(t, P_LRATE); t->p[P_LRATE] = (int16_t)dk_frac(d, 25, 80);
    d = track_desc(t, P_LWAVE); t->p[P_LWAVE] = (int16_t)dk_frac(d, 0, 100);
    d = track_desc(t, P_LPHASE); t->p[P_LPHASE] = (int16_t)(rng() >> 31 ? -1 : dk_rr(0, d->max));   /* (0.9.18: MODE FREE
                                                         * half the time, was 2 in 3; TRIG at a phase the rest, 0..127) */
    d = track_desc(t, P_LFADE); t->p[P_LFADE] = (int16_t)(rng() % 3u ? d->min : dk_frac(d, 0, 50));
    d = track_desc(t, P_LD_PIT); t->p[P_LD_PIT] = (int16_t)(rng() % 4u ? 0 : dk_frac(d, 44, 56));
    d = track_desc(t, P_LD_FLT); t->p[P_LD_FLT] = (int16_t)dk_frac(d, d->min < 0 ? 15 : 20, d->min < 0 ? 85 : 70);
    d = track_desc(t, P_LD_SHP); t->p[P_LD_SHP] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, d->min < 0 ? 30 : 0, d->min < 0 ? 70 : 40));
    d = track_desc(t, P_LD_AMP); t->p[P_LD_AMP] = (int16_t)(rng() % 3u ? 0 : dk_frac(d, 0, 35));
}
/* the effects: the track's drive (half the time none), its sends, an AUTOPAN one time in three; the shared reverb's
 * kind and size and the delay's time and feedback, a pitched delay one time in five */
static void fx_dice(track_t *t)
{
    const param_desc_t *d;
    d = track_desc(t, P_DIST); t->p[P_DIST] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, 8, 55));
    d = track_desc(t, P_DTYPE); t->p[P_DTYPE] = (int16_t)dk_frac(d, 0, 100);
    d = track_desc(t, P_DTONE); t->p[P_DTONE] = (int16_t)dk_frac(d, 30, 80);
    d = track_desc(t, P_CHOR); t->p[P_CHOR] = (int16_t)(rng() % 2u ? 0 : dk_frac(d, 15, 70));
    d = track_desc(t, P_DLY); t->p[P_DLY] = (int16_t)dk_frac(d, 0, 65);
    d = track_desc(t, P_REV); t->p[P_REV] = (int16_t)dk_frac(d, 10, 85);
    d = track_desc(t, P_APDEP); t->p[P_APDEP] = (int16_t)(rng() % 3u ? 0 : dk_frac(d, 25, 80));
    d = track_desc(t, P_APRATE); t->p[P_APRATE] = (int16_t)dk_frac(d, 20, 80);
    d = track_desc(t, P_APWAVE); t->p[P_APWAVE] = (int16_t)dk_frac(d, 0, 100);
    song.g[G_RTYPE] = (int16_t)dk_rr(GP[G_RTYPE].min, GP[G_RTYPE].max);
    song.g[G_RSIZE] = (int16_t)dk_rr(GP[G_RSIZE].min + (GP[G_RSIZE].max - GP[G_RSIZE].min) / 3, GP[G_RSIZE].max);
    song.g[G_RDAMP] = (int16_t)dk_rr(GP[G_RDAMP].min, GP[G_RDAMP].max);
    song.g[G_DTIME] = (int16_t)dk_rr(GP[G_DTIME].min, GP[G_DTIME].max);
    song.g[G_DFDBK] = (int16_t)dk_rr(20, 70);
    song.g[G_DPIT] = (int16_t)(rng() % 10u ? 0 : (rng() & 1u ? 12 : -12));   /* (V10.8: one in ten, octaves; was 1/5 with
                                                         * fifths and fourths) */
}
static void part_dice(uint32_t kind)
{
    static uint32_t last_ms;                            /* (once a turn burst, the dialog's turns too) */
    uint32_t again = last_ms && fm1_ms - last_ms <= 400u;
    last_ms = fm1_ms | 1u;
    if (again && ui.confirm == kind)
        return;
    if (chain_busy()) {
        ui_message("SONG PLAYS: STOP IT TO EDIT");
        return;
    }
    if (ui.confirm != kind) {
        undo_mark();
        dk_keep(kind);
        dk_vis_take();
        confirm_open(kind, song.sel);
    }
    dk.msg = kind == CF_DICE_SEQ ? dice_seq(TSEL) : kind == CF_DICE_LFO ? "LFO DICED" : kind == CF_DICE_FX ? "FX DICED" :
             drum_track(TSEL) ? "KIT DICED" : "SOUND DICED";
    if (kind == CF_DICE_SOUND) sound_dice_of(TSEL, 1);
    else if (kind == CF_DICE_LFO) lfo_dice(TSEL);
    else if (kind == CF_DICE_FX) fx_dice(TSEL);
}
static int dice_kind(uint32_t c) { return c == CF_DICE_SOUND || c == CF_DICE_SEQ || c == CF_DICE_LFO || c == CF_DICE_FX; }
/* (0.6.21) HOME + SELECT (after the dialog): everything diced into a new piece. One scale for the song (ROOT, a
 * musical SCALE: never CHR / WHOLE / DIM; the RESO reverb and the SLICER follow it), each melodic track an engine, a
 * sound (dice of a factory preset) and a sequence in that scale by its role (T1 the bass; the others a lead, a pad or an
 * arpeggio), a DRUM track a diced kit and a beat; the macros' routes rolled for every track; the delay, reverb and
 * chorus their settings within musical ranges. Tempo, mixer levels, mutes and the master are left alone. UNDO brings
 * the music before it back */
static void macro_dice(uint32_t mode);
static int16_t sl_root = -1, sl_scale, sl_quant;      /* (scale_link: the song's scale) */
#include "song_dice.c"                             /* (0.6.26) DICE ALL: a song-idea generator */
#include "gen.c"                                   /* (0.6.91) GEN: the song generator and its screen (HOME + SELECT) */

/* HOME / REC / SAVE: tap on release, hold 0.7 s fires once. t0 = press time | 1,
 * bit 1 = fired (or swallowed: then the release is no tap either) */
enum { BT_NONE, BT_TAP, BT_HOLD };
static uint32_t btn_hold(uint32_t *t0, uint32_t label, uint32_t now, int hold_ok)
{
    uint32_t tap;
    if ((fm1_in.buttons >> panel.btn[label]) & 1u) {
        if (!*t0)
            *t0 = (now | 1u) & ~2u;
        else if (hold_ok && !(*t0 & 2u) && now - (*t0 & ~3u) > 700u * 1000u * FM1_TICKS_PER_US) {
            *t0 |= 2u;
            return BT_HOLD;
        }
        return BT_NONE;
    }
    tap = *t0 && !(*t0 & 2u);
    *t0 = 0;
    return tap ? BT_TAP : BT_NONE;
}

/* the quick layers: ui_layer.c (included after this file) */
static void layer_masks(void);
static void macro_dice(uint32_t mode);                  /* (JIANT 0.4) */
static void layer_arm(uint32_t pressed, uint32_t now);
static uint32_t layer_held(void);
static int layer_knobs_quiet(void);
static uint32_t layer_gesture(uint32_t now, uint32_t combo);
static void layer_show(void);
static void layer_tap(uint32_t l);
static void layer_keys(uint32_t keys);
static void layer_knob(uint32_t k, int32_t s);
static int layer_play(void);
static int layer_set_open(void);
static uint32_t layer_oct(uint32_t pressed, uint32_t oct);
static void layer_oct_open(uint32_t pressed);
static int layer_allowed(void);
static void mom_knob(uint32_t k, int32_t s);            /* (0.9.9-seq2e) MOMENTARY: EDIT held + KNOB 1..4 */
static void mom_poll(void);
static void mom_forget(uint32_t t);
static uint32_t ly_bit(uint32_t l);

/* a page button let go (they act on release; a layer's own button: layer_gesture): its section (rule 2; 0.9.2: EDIT is
 * always EDIT: a step's clear and AUTO LIST's delete are SEQ + G5, a slot's rename SAVE + C5, ui_layer.c). 1: it acted
 * (a step held on STEP + EDIT: its locks go, the step stays) instead of opening a page */
static int page_tap(uint32_t b)
{
    uint32_t f;
    if (b == B_EDIT && song.seq_mode && !ui.home && cur_page()->graph == GR_ROLL && lock_held()) {
        if (chain_busy()) ui_message("SONG PLAYS: STOP IT TO EDIT");
        else lock_clear(lock_held());
        return 1;
    }
    for (f = FAM_HOME + 1u; f < FAM_COUNT; f++)
        if (FAM_BTN[f] == b) {
            open_family(f);
            return 0;
        }
    return 0;
}
/* (0.9.2) SEQ + G5, CLR STEP (EDIT's until 0.9.1): on AUTO LIST the row's record goes; elsewhere in SEQ the cursor step
 * is cleared, its locks with it, and the cursor goes on to the next (NOTE with a note in focus: that note alone) */
static void seq_clr_step(void)
{
    const page_t *pg = cur_page();
    if (ui.home || pg->fam != FAM_SEQ)
        return;
    if (chain_busy()) {
        ui_message("SONG PLAYS: STOP IT TO EDIT");
        return;
    }
    if (pg->graph == GR_EVENTS) {
        ev_delete();
        return;
    }
    if (pg->graph == GR_ROLL && pg->id[0] == 4u && ui.pr_focus && TSEL->step[ui.cursor].time == ST_NOTE &&
        ui.pr_focus <= TSEL->step[ui.cursor].n) {       /* (0.6.81) SEQ > NOTE: the note in focus alone */
        note_drop(&TSEL->step[ui.cursor], ui.pr_focus - 1u);
        ui.pr_focus = 0;
        ui_message("NOTE DELETED");
        return;
    }
    step_clear(&TSEL->step[ui.cursor]);
    (void)motion_clear_locks(TSEL, ui.cursor);
    cursor_set(ui.cursor + 1);
    ui_message("STEP CLEARED");
}

/* a track whose sound's sample is missing (engines.c snd_missing: an empty user slot, a built-in set this build lacks;
 * it plays a sine): the no-file icon and NO SAMPLE (ui.c MSG_NO_SAMPLE). Once per track and source: at power-on, a sound
 * or project load, SRC / SET turned, a slot erased; again after another source or a project load. One a frame; after
 * a message already showing (LOADED) as the second message. Not on notes: nothing in the audio path */

static void hints_reset(void);                          /* (ui_layer.c) */
/* messages of things that happened elsewhere (a load, the editor, MIDI in): after this frame's own */
static void ui_notices(void)
{
    static uint32_t midi_t, midi_last;
    if (motion_full) { motion_full = 0; ui_message("AUTOMATION FULL"); }
    if (motion_recd && (!song.playing || motion.count > MOTION_MAX - 8u))   /* (V10.5) what was recorded thinned: at */
        (void)motion_thin(motion_recd);                 /* the stop, and while recording before the 64 run out */
    if (midi_hint) {                                    /* MIDI notes into a track that is not selected */
        uint32_t h = midi_hint;
        midi_hint = 0;
        if (!ui.msg_t && (h != midi_last || fm1_ms - midi_t > 4000u)) {
            char b[4] = {'T', (char)('0' + h), 0, 0};
            ui_say("MIDI IN -> ", b);
            midi_last = h;
            midi_t = fm1_ms;
        }
    }
}

/* (JIANT 0.4) one scale for the whole instrument. (0.6.19) Strictly: ROOT, SCALE and QNT are the song's, held here;
 * the selected track's (a knob, the SCL layer, the editor, MIDI, automation, a project load) is the one that counts,
 * and every melodic track is kept on it each frame (any other way a track got its own scale undone: a sequence
 * never plays in another scale). A DRUM track has none to follow. Each track keeps its TRN */
static void scale_link(void)
{
    const track_t *t = TSEL;
    uint32_t k;
    for (k = 0; k < NTRK && drum_track(t); k++) t = &trk[k];   /* (0.8.3) a DRUM track selected: the first melodic
                                                         * track's counts (a section the song brings in its own key:
                                                         * its key, not the one before it) */
    if (!drum_track(t) && (t->p[P_ROOT] != sl_root || t->p[P_SCALE] != sl_scale || t->p[P_QUANT] != sl_quant)) {
        sl_root = t->p[P_ROOT];
        sl_scale = t->p[P_SCALE];
        sl_quant = t->p[P_QUANT];
    }
    if (sl_root < 0)
        return;                                         /* (a DRUM track selected from the start: nothing yet) */
    for (k = 0; k < NTRK; k++)
        if (!drum_track(&trk[k])) {
            trk[k].p[P_ROOT] = sl_root;
            trk[k].p[P_SCALE] = sl_scale;
            trk[k].p[P_QUANT] = sl_quant;
        }
}

static void ui_input_frame(void);
static void ui_input(void)
{
    ui_input_frame();
    eu_tick();                                          /* (V10.16) EUCLID's MUTATE, on the bar */
    scale_link();                                       /* (JIANT 0.4) one scale for every melodic track (after the
                                                         * frame's edits) */
}
static void ui_input_frame(void)
{
    uint32_t pressed = fm1_input_edges(0), notes = fm1_input_note_edges(), now = fm1_ticks(), id, b, k;
    uint32_t home, rec, seq, save;
    uint32_t oct;
    uint32_t lay, combo = 0, lytap, lkeys, glo, kq = 0;
    int32_t s, sel = 0, ks[4] = {0, 0, 0, 0};
    fm6_poll();                                         /* FM6: PTCH turned -> its patch */
    sc_beat();                                          /* (V10.6j) SCENE's DRUM: a key's beat into the kit */
    mom_poll();                                         /* (0.9.9-seq2e) MOMENTARY's glide back, its header tag */
    {   /* (JIANT 0.6.5) RESO's PITCH and FOLW live in PRE's and DAMP's slots: into RESO a ROOM's values become theirs
         * (DAMP past FOLW's T6: OFF; PRE past 14: 14), out of it the reverb's own defaults back */
        static uint8_t rt0 = 0xFFu;
        uint32_t rt = (uint32_t)song.g[G_RTYPE];
        if (rt == 3u) {
            if (song.g[G_RDAMP] > 6) song.g[G_RDAMP] = 0;
            if (song.g[G_RPRE] > 14) song.g[G_RPRE] = 14;
            if (song.g[G_RPRE] < 0) song.g[G_RPRE] = 0;
        } else if (rt0 == 3u) {
            song.g[G_RDAMP] = GP[G_RDAMP].def;
            song.g[G_RPRE] = GP[G_RPRE].def;
        }   /* (0.6.12) PRE (ROOM, SPRING) and AIR (SHIMMER, CLOUD) share the slot: from one to the other it starts at 0 */
        if (rt0 != 0xFFu && rt0 != rt && (rt == 2u || rt == 4u) != (rt0 == 2u || rt0 == 4u) && rt != 3u && rt0 != 3u)
            song.g[G_RPRE] = 0;
        if (rt != 2u && rt != 4u && song.g[G_RPRE] < 0) song.g[G_RPRE] = 0;
        if ((rt == 2u || rt == 4u) && song.g[G_RPRE] > 24) song.g[G_RPRE] = 24;
        rt0 = (uint8_t)rt;
    }
#if !FELUCCA_FM4
    for (k = 0; k < NTRK; k++)                          /* a DIGITAL sound any other way (the paths convert it */
        if (trk[k].eng_req == ENGI_DIGITAL)             /* already): FM6 (fm4_convert.c) */
            fm4_track(&trk[k]);
#endif
    for (k = 0; k < NTRK; k++)                          /* a retired engine's sound any other way (core.h): ANALOG */
        if (eng_gone(trk[k].eng_req))
            set_engine_of(&trk[k], ENGI_PHYS_TO);
    perf_latch_on = fx_latch & 1u;                      /* (MENU > FX LATCH; a settings load sets it too) */
    fx_usb_fixed = (ui_prefs & PREF_USB_FIXED) != 0u;   /* (MENU > USB LEVEL: fx.c, audio.c) */
    fx_usb_gain = USB_GAIN[fx_usb_fixed ? usb_lvl % 5u : 0u];   /* (0.7.0) .. and its attenuation (usb.c uac_tap) */
    mo_lvl = (uint8_t)(midi_out_mode & 3u);             /* (0.6.72) MENU > MIDI OUT (seq.c) */
    midi_pc_poll();                                     /* (0.6.72) PROGRAM CHANGE in: the sounds it asked for */
    ml_poll();                                          /* (p1) MIDI LEARN: a CC came for the picked parameter */
    rp_apply();                                        /* (MENU > CLICK, CLICK LEVEL, COUNT-IN: click.c, seq.c) */
    {   /* (0.6.55) TRACKS 6 -> 4 (MENU or GLO > SYSTEM) on T5 / T6: T4 selected (ALGORITHM stops there) */
        static uint8_t live0 = NTRK;
        if (trk_live < live0 && song.sel >= trk_live)
            track_select(trk_live - 1u);
        live0 = trk_live;
    }
    {   /* (0.6.18) TRACKS 4 LOW: T5 / T6 picked, the screen says how to wake them (once a pick). (0.6.28) Also when a
         * note is sent to them (a key, MIDI, their steps) and when PLAY starts with steps on them: they were silent and
         * people did not know why (at most every 4 s) */
        static uint8_t sel0 = 0xFFu, play0;
        static uint32_t said_ms;
        uint32_t k, said = song.sel != sel0 && song.sel >= trk_live;
        int quiet = !said_ms || fm1_ms - said_ms > 4000u;
        if (song.playing && !play0 && song.g[G_STRN]) {   /* (0.6.45) PLAY with every sequence moved: say so (it
                                                         * looked like a bug: notes lower than the roll shows) */
            char b[24];
            str_cpy(b, "ALL TR ", sizeof b);
            if (song.g[G_STRN] < 0) str_cpy(b + 7, "-", 2);
            fmt_int(b + str_len(b), song.g[G_STRN] < 0 ? -song.g[G_STRN] : song.g[G_STRN]);
            ui_say(b, " DEG");
            str_cpy(ui.msg2, "SCL > ALL TR: 0 = OFF", sizeof ui.msg2);   /* (0.7.6: SCL KNOB 4, was SCL 2) */
        }
        if (song.playing && !play0 && quiet)
            for (k = trk_live; k < NTRK; k++)
                if (!seq_is_empty(&trk[k]) && !trk[k].p[P_MUTE]) said = 1;
        if (asleep_hit) { asleep_hit = 0; said |= quiet; }
        if (said) {
            ui_say("T5-T6 ASLEEP: ", "");
            str_cpy(ui.msg2, "GLO>SYSTEM>TRACKS 6", sizeof ui.msg2);
            said_ms = fm1_ms | 1u;
        }
        sel0 = song.sel;
        play0 = song.playing;
    }
    if (!ui.menu)
        usb_serial_apply();                             /* (MENU > USB SERIAL: when the menu has closed) */
    if (scr_input(pressed, notes))                      /* MENU > SCREEN OFF: dark, or the wake gesture: swallowed */
        return;                                         /* (before the buttons' timers: no tap or hold after it) */
    home = btn_hold(&ui.home_t0, B_HOME, now, 1);
    rec = btn_hold(&ui.rec_t0, B_REC, now, 0);          /* (a tap; held, the REC layer: ui_layer.c) */
    seq = btn_hold(&ui.seq_t0, B_SEQ, now, !ui.menu && !dlg_on());
    save = btn_hold(&ui.save_t0, B_SAVE, now, 0);       /* (a tap; held, the song layer: ui_layer.c) */
    layer_arm(pressed, now);
    if (((pressed >> panel.btn[B_REC]) & 1u) && ui.ly == LAYER_REC)
        ui.rec_t0 |= 2u;                                /* (REC is its layer's button now: its tap is layer_tap's) */
    if (((pressed >> panel.btn[B_SAVE]) & 1u) && ui.ly == LAYER_SAVE)
        ui.save_t0 |= 2u;                               /* (SAVE too: layer_tap opens its pages) */
    layer_oct_open(pressed);                            /* (OCT± with a SET layer's button down: it opens now) */
    oct = oct_taps(pressed, ui.menu || ui.confirm || act_cols() || name_on() || layer_set_open());
    oct = layer_oct(pressed, oct);                      /* (a SET layer's OCT-: put back) */
    if (!ui.home && !ui.menu && !ui.confirm && !ui.ly && cur_page()->graph == GR_TOOLS && ((pressed >> panel.btn[B_OCTUP]) & 1u))
        tools_arm();
    if (tl_t0) {                                        /* (V10.24) TOOLS: OCT+ held TL_HOLD does the action */
        if (!((fm1_in.buttons >> panel.btn[B_OCTUP]) & 1u) || cur_page()->graph != GR_TOOLS || ui.home) {
            if ((int32_t)(fm1_ms - tl_t0) < (int32_t)TL_HOLD) ui_message("HOLD OCT+ TO DO IT");
            tl_t0 = 0;
        } else if ((int32_t)(fm1_ms - tl_t0) >= (int32_t)TL_HOLD) {
            uint8_t a = ui.act;
            confirm_open(tl_kind, tl_trk);
            ui.act = a;
            tl_t0 = 0;
            oct |= 2u;                                  /* (the dialog's OK, below, this frame) */
        }
    }
    kx_frame();                                         /* (V10.6z4) the expression keys let go, recorded */
    layer_masks();                                      /* seq.c: keys pressed with a layer's button are its own */
    live_frame();                                       /* (p2) GLO > LIVE: MANO's keys, OCT- held, the modifiers */
    {   /* (0.6.67) a button held + SELECT (rule 5): its dice (HOME the generator, GEN; EDIT the sound, SEQ the sequence,
         * LFO the LFO, FX the effects; GLO + SELECT the tempo), ENV SCL ARP SAVE their chain (nav_jump); the button's tap
         * and layer swallowed, and the turn's last detents for a moment after it is let go (no page moved by them). Not in
         * the menu, a dialog, NAME. (0.9.2) A button held + PRESETS: nothing (rule 3; until 0.9.1 its pages) */
        static uint32_t pj_sel_ms;
        uint32_t f = held_family(), hb = (fm1_in.buttons >> panel.btn[B_HOME]) & 1u;
        int32_t ps;
        if (!ui.menu && !dlg_on() && !name_on() && (f != FAM_COUNT || hb) && f != FAM_GLO &&
            (ps = panel_enc(EN_SELECT)) != 0) {
            uint32_t k = f == FAM_EDIT ? CF_DICE_SOUND : f == FAM_SEQ ? CF_DICE_SEQ : f == FAM_LFO ? CF_DICE_LFO :
                         f == FAM_FX ? CF_DICE_FX : 0u;
            if (f == FAM_COUNT) {                       /* HOME held: GEN, HOME's generator */
                ui.home_t0 |= 2u;
                if (!pj_sel_ms || fm1_ms - pj_sel_ms > 400u)
                    nav_go(SEC_HOME, 4u + gen_page(), 1);
            } else if (k) {
                if (f == FAM_EDIT)                      /* (0.9.9-seq2f) EDIT + SELECT: the sound dice again (SELECT is
                                                         * no sound parameter; the presets: PRESETS only) */
                    mom_forget(song.sel);               /* (a MOMENTARY value of the sound replaced: not put back) */
                part_dice(k);
                btn_used(FAM_BTN[f]);
            } else {
                nav_jump(sec_of_fam(f), ps);
                btn_used(FAM_BTN[f]);
            }
            pj_sel_ms = fm1_ms | 1u;
        } else if (f != FAM_COUNT && !ui.menu && !dlg_on() && !name_on() && !ly_used && (s = panel_enc(EN_PRESET)) != 0) {
            btn_used(FAM_BTN[f]);                       /* (0.9.2, rule 3) a button held + PRESETS: nothing (until 0.9.1
                                                         * its pages); no tap after, no map while held. (A layer already
                                                         * used: PRESETS stays its, dropped) */
            if (f == FAM_EDIT && !mom_on)               /* (V10.7) but EDIT + PRESETS: the next / previous engine (not
                                                         * amid a MOMENTARY tweak: its values go back to this sound) */
                preset_engine_jump(s);
        } else if (pj_sel_ms && fm1_ms - pj_sel_ms < 400u) {
            if (panel_enc(EN_SELECT))
                pj_sel_ms = fm1_ms | 1u;                /* (dropped: still the paging turn) */
        } else {
            pj_sel_ms = 0;
        }
        pj_hold();
    }
    lay = layer_held();
    glo = lay && ui.ly == LAYER_GLO;                    /* GLO held: SELECT is the tempo, BPM LOCK or not (#58) */
    if (!layer_allowed()) {
        perf_kill = 1;                                  /* (effects off until their keys are let go) */
        perf_latched = 0;                               /* (FX LATCH: the latched ones and the macros off) */
        perf_k[0] = perf_k[1] = perf_k[2] = perf_k[3] = 0;
    }
    else if (!kb_layer)
        perf_kill = 0;
    {   /* a key pressed with the button: a combo (its edge, or the ISR already took it); then not the grid's or a step's */
        static uint32_t kb_seen;
        lkeys = kb_layer & ~kb_seen;
        combo = lay && (notes || lkeys);
        kb_seen = kb_layer;
    }
    if (lay)
        lkeys |= notes & ~fm1_in.notes;                 /* (tapped and let go already) */
    notes &= ~kb_layer;
    if (lay) {                                          /* a layer's button held: keys, knobs and buttons are combos */
        combo |= (pressed & ~ly_bit(ui.ly) &            /* (REC + PLAY: PLAY plays and REC let go soon is still */
                  ~(ui.ly == LAYER_REC ? 1u << panel.btn[B_PLAY] : 0u)) != 0u;   /* a tap, it arms: as before) */
        notes = 0;                                      /* (the keys are the layer's, not the grid's or a step's) */
        if (pressed & (1u << panel.btn[B_SAVE]))       /* no UNDO, no page */
            ui.save_t0 |= 2u;
        if (pressed & (1u << panel.btn[B_HOME]))       /* no menu, no HOME */
            ui.home_t0 |= 2u;
        if (pressed & (1u << panel.btn[B_SEQ]))
            ui.seq_t0 |= 2u;
        for (k = 0; k < 4u; k++)                        /* KNOB 1..4: the layer's (ui_layer.c layer_knob); (0.9.9-seq2e)
                                                         * EDIT's: MOMENTARY, the page under (ui_layer.c mom_knob) */
            if ((ks[k] = panel_enc(EN_K1 + k)) != 0) {
                if (ui.ly == LAYER_EDIT && (mom_on || !(ui.ly_t0 & 8u))) {   /* (LY_DEAD: not a dead press) */
                    mom_knob(k, ks[k]);
                    ks[k] = 0;
                } else
                    combo = 1;
            }
        panel_enc(EN_PRESET);                           /* (a stray turn would load another sound) */
        {   /* (0.8.5) ALGORITHM: the layer's lens, MAP <-> KNOBS <-> STATE (no wrap; was dropped: another track there,
             * OCT- put back the layer's track only) */
            int32_t a = panel_enc(EN_ALGO);
            if (a && ui.layer) {
                ui.lylv = (uint8_t)clamp((int32_t)ui.lylv + a, 0, 2);
                ui.force = 1;
                combo = 1;
            }
        }
        if (glo && (sel = panel_enc(EN_SELECT)) != 0)   /* GLO + SELECT: the tempo, a combo (OCT- puts it back) */
            combo = 1;
    } else if (fx_lock_on) {                            /* (0.6.44) the FX layer locked open: KNOB 1..4 its macros */
        for (k = 0; k < 4u; k++)
            ks[k] = panel_enc(EN_K1 + k);
        panel_enc(EN_PRESET);
        kq = 1;
    } else if ((kq = (uint32_t)layer_knobs_quiet()) != 0) {   /* a layer letting go: KNOB 1..4 are nobody's (#39) */
        for (k = 0; k < 4u; k++)
            if (panel_enc(EN_K1 + k) != 0)
                combo = 1;                              /* (with the button let go this frame: no tap) */
    }
    if (!lay)
        ly_used = 0;
    else if (combo)
        ly_used = 1;
    lytap = layer_gesture(now, combo);
    layer_show();
    layer_keys(lkeys);
    for (k = 0; k < 4u; k++)
        if (ks[k]) {
            layer_knob(k, ks[k]);
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40, ui.knob_ms = fm1_ms;
        }
    song.grid = (uint8_t)keys_mode();                 /* (the menu, a dialog: the keys play again; NAME: silent) */
    if (home == BT_TAP && ui.menu) {                    /* HOME (pressed, or held) closes the menu, from ABOUT too; */
        menu_close();                                   /* the release of the hold that opened it is no tap */
        home = BT_NONE;                                 /* (menu_close went HOME already) */
    } else if (home == BT_HOLD) {                       /* HOME held: open the menu, or leave it */
        if (ui.menu) {
            menu_close();
        } else {
            ui.menu = 1;                                /* (at the tab and row it was left at) */
            ui.mlv = 1;                                 /* (0.8.5: the lens at TAB, its rows) */
            ui.confirm = 0;                             /* (a clear dialog is cancelled, NAME too) */
            name_close();
            ui.force = 1;
            song.seq_mode = 0;
        }
    }
    if (rec == BT_TAP && !ui.menu && !dlg_on() && !name_on())   /* REC pressed in another layer (REC alone: */
        rec_tap();                                      /* its layer's tap); nothing in the menu, a dialog or NAME */
    if (ui.menu) {                                      /* SAVE / REC taps do nothing here (HOME: above) */
        if (ui.save_t0)
            ui.save_t0 |= 2u;
        ui.pg_down = 0;
        if (!ui.home_t0)
            menu_input(oct);
        return;
    }
    if (name_on() && !ui.confirm) {                     /* NAME: the keys type, KNOB 1 / 2, OCT+ / OCT- (ui_name.c); */
        if (ui.save_t0)                                 /* SAVE does nothing */
            ui.save_t0 |= 2u;
        if (((pressed >> panel.btn[B_PLAY]) & 1u) && (song.playing || chain_busy() || seq_counting()))
            transport_req = 2;                          /* PLAY stops a transport started meanwhile (MIDI Start, the
                                                         * editor) so the name can be saved; it never starts one */
        ui.pg_down = 0;
        name_input(notes, oct);
        return;
    }
    if (save == BT_TAP && !dlg_on())
        layer_tap(LAYER_SAVE);
    if (dlg_on()) {                                     /* OCT- cancels, OCT+ does it; nothing else reacts (GEN: a page,
                                                         * below) */
        if (oct & 2u) {
            uint32_t kind = ui.confirm;
            ui.confirm = 0;
            ui.force = 1;
            if (kind != CF_OVR_PROJ && kind != CF_OVR_USER && kind != CF_ERASE_USER && kind != CF_ERASE_SECT && kind != CF_OVR_SONG && kind != CF_ERASE_SONG && kind != CF_DICE_ALL &&
                !dice_kind(kind) && kind != CF_DEL_ROW && kind != CF_CLEAR_SONG && !chain_busy())
                undo_mark();                            /* (0.6.21) the music before a clear, INIT, RECALL, pattern */
            if (kind == CF_OVR_PROJ) {                  /* overwrite: the NAME screen writes it */
                name_open(NK_PROJ_SAVE, ui.confirm_trk & 3u);
            } else if (kind == CF_OVR_SONG) {
                name_open(NK_SONG_SAVE, ui.confirm_trk);
            } else if (kind == CF_OVR_USER) {
                name_open(NK_USER_SAVE, ui.confirm_trk);
            } else if (kind == CF_ERASE_USER) {
                up_ui(1u, ui.confirm_trk);
            } else if (kind == CF_ERASE_SONG) {
                song_erase_parts(ui.confirm_trk, 0);
            } else if (kind == CF_ERASE_SECT) {
                if ((ui.confirm_trk >> 2u) == song_cur) project_erase(ui.confirm_trk & 3u);
                else ui_message("PROJECT CHANGED");
            } else if (dice_kind(kind)) {               /* (0.6.65) OK: the diced part stays */
                ui_message(dk.msg);
            } else if (kind == CF_LOAD_PAT) {
                pat_load_ui(&trk[ui.confirm_trk % NTRK], pat_pick());
            } else if (kind == CF_CLEAR_PFX) {         /* (0.6.99) FX > PUNCH > CLEAR */
                if (!chain_busy()) { memset(pfx_lane, 0, sizeof pfx_lane); pfx_ltgt = 0; ui_message("PUNCH LANE CLEARED"); }
            } else if (kind == CF_CLEAR_MOTION) {
                track_t *t = &trk[ui.confirm_trk % NTRK];
                if (!chain_busy()) { load_begin(t, LOAD_PAT); motion_clear(t); load_end(t); ui_message("AUTOMATION CLEARED"); }
            } else if (kind == CF_DEL_ROW) {
                uint32_t r = ui.confirm_trk;
                if (!chain_busy() && r < chain_config.count) {
                    for (; r + 1u < chain_config.count; r++)
                        row_copy(r, r + 1u);            /* (its scene, its tracks' with it) */
                    row_copy(r, CHAIN_ROWS);
                    song_idx_touch();
                    chain_config.count--;
                    if (ui.song_row > chain_config.count) ui.song_row = chain_config.count;
                    ui_message("ROW DELETED");
                }
            } else if (kind == CF_CLEAR_SONG) {
                if (!chain_busy()) {
                    chain_defaults(&chain_config);
                    memset(song_idx.scene[song_cur], 0, sizeof song_idx.scene[song_cur]);
                    memset(song_idx.tsrc[song_cur], 0, sizeof song_idx.tsrc[song_cur]);
                    memset(song_idx.tvar[song_cur], 0, sizeof song_idx.tvar[song_cur]);
                    song_idx_touch();
                    ui.song_row = 0;
                    ui_message("SONG CLEARED");
                }
            } else if (kind == CF_LIVE_COMMIT) {        /* (p2) LIVE: the modifiers written (UNDO: marked above) */
                live_commit(ui.confirm_trk);
            } else if (kind == CF_CPU_TEST) {           /* (0.6.83) SAVE > TOOLS 6: the heaviest song (UNDO: back) */
                if (!chain_busy()) {
                    cpu_test_song();
                    ui_message("CPU TEST: PRESS PLAY");
                }
            } else if (kind == CF_INIT_SYS) {           /* (0.6.40) SAVE > TOOLS 5: everything as at power-on */
                if (!chain_busy()) {
                    undo_mark();
                    if (song.playing) transport_req = 2;
                    system_init();
                    hints_reset();                      /* (0.9.5) the teaching hints start over */
                    gp.genre = GEN_EXPLORE;             /* (0.8.6) GEN's GENRE as at power-on: EXPLORE */
                    ui_message("NEW PROJECT");
                }
            } else if (kind == CF_CLEAR_ALL) {          /* SEQ + REC: every track's steps and automation, the lane */
                if (!chain_busy()) {
                    uint32_t k;
                    for (k = 0; k < NTRK; k++) {
                        load_begin(&trk[k], LOAD_PAT);
                        track_defaults_steps(&trk[k]);
                        motion_clear(&trk[k]);
                        load_end(&trk[k]);
                        latch_drop_req |= (uint8_t)(1u << k);   /* (the held / latched notes: events_block) */
                    }
                    memset(pfx_lane, 0, sizeof pfx_lane);
                    ui_message("ALL CLEARED");
                }
            } else if (kind == CF_INIT_SOUND) {         /* (DRUM: the factory DRUM-X kit too) */
                if (!chain_busy()) {
                    init_sound_of(TSEL, TSEL->eng_req);
                    if (drum_track(TSEL) && !drum_elsewhere(TSEL)) {   /* (0.8.3: not another track's kit) */
                        memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
                        memset(dx_mot, 0, sizeof dx_mot);   /* (0.5.1: X-MOD off) */
                        memset(dx_snd, 0, sizeof dx_snd);   /* (0.6.48: no SEND) */
                        memcpy(dx_lay, DX_LAY_DEF, sizeof dx_lay);   /* (0.6.49: the factory LAYERs) */
                    }
                    ui_message(drum_track(TSEL) && drum_elsewhere(TSEL) ? "INIT, KIT KEPT" : "SOUND INIT");
                }
            } else if (kind == CF_RECALL_SOUND) {       /* the section's stored sound (ui_layer.c EDIT); (V10.6z8) a song
                                                         * playing too: the track's sound follows the sections again */
                int sl = project_recall_slot();
                if (sl < 0 || project_recall_sound((uint32_t)sl, song.sel)) ui_message("NOTHING SAVED");
                else ui_message("SOUND RECALLED");
            } else {
                track_t *t = &trk[ui.confirm_trk % NTRK];
                load_begin(t, LOAD_PAT);
                track_defaults_steps(t);
                load_end(t);
                latch_drop_req |= (uint8_t)(1u << (t - trk));   /* and the latched chord (events_block) */
                if (kind == CF_CLEAR_TRK) {
                    char b[12] = "1 CLEARED";
                    b[0] = (char)('1' + ui.confirm_trk);
                    ui_say("TRACK ", b);
                } else {
                    {   /* (0.6.28) which track: the others play on (SEQ + REC clears them all) */
                        char cb[16] = "T1 CLEARED";
                        cb[1] = (char)('1' + ui.confirm_trk % NTRK);
                        ui_say(cb, "");
                        str_cpy(ui.msg2, "SEQ+REC: ALL TRACKS", sizeof ui.msg2);
                    }
                }
            }
        } else if (oct & 1u) {
            if (dice_kind(ui.confirm))                  /* (0.6.65) CANCEL: the part before the dice */
                ui_message(dk_back(ui.confirm, ui.confirm_trk) ? "CAN'T UNDO" :
                           ui.confirm == CF_DICE_SEQ ? "SEQ BACK" : ui.confirm == CF_DICE_LFO ? "LFO BACK" :
                           ui.confirm == CF_DICE_FX ? "FX BACK" : "SOUND BACK");
            ui.confirm = 0;
            ui.force = 1;
        } else if (dice_kind(ui.confirm) && panel_enc(EN_SELECT)) {   /* (0.6.65) SELECT again: another dice
                                                         * (once a turn burst) */
            part_dice(ui.confirm);
        }
        ui.pg_down = 0;
        enc_drop();
        return;
    }
    if (lytap)                                          /* a layer's button acts on release (held: the layer) */
        layer_tap(lytap);
    if (seq == BT_TAP || seq == BT_HOLD)                /* (SONG: SAVE held, G5) */
        open_family(FAM_SEQ);
    if (home == BT_TAP)                                 /* HOME acts on release (a hold opens the menu): rule 2, HOME's */
        nav_button(SEC_HOME);                           /* page last shown; on HOME the next group (ECO BEING GEN) */
    cursor_fix();                                       /* LEN may have changed (knob, editor, load) */
    for (id = 0; id < 14u; id++) {
        if (!((pressed >> id) & 1u))
            continue;
        b = panel_btn_of(id);
        switch (b) {
        case B_PLAY:
            if (layer_play() || tl_play())              /* (GLO held: RESTART; V10.6o TIMELINE: the row loops) */
                break;
            if (song.playing || chain_busy() || seq_counting())   /* (a count-in: PLAY stops it) */
                transport_req = 2;
            else if (!song.rec && ((!ui.home && cur_page()->graph == GR_SONG && !SONG_PARTS(cur_page()) &&
                                    !SONG_CLIPS(cur_page())) || live.mode))   /* (the song layer's SONG mode; PARTS: as
                                                         * KNOB 4 says. A track armed: the loop, recorded; a song does
                                                         * not record, chain_start) */
                chain_play_ui();
            else
                transport_req = 1;
            break;
        case B_REC:                                     /* (JIANT) with SEQ held: CLEAR ALL? (every track's steps) */
            if ((fm1_in.buttons >> panel.btn[B_SEQ]) & 1u && !ui.menu && !ui.confirm) {
                ui.seq_t0 |= 2u;                        /* (SEQ: neither its tap nor its hold now) */
                if (ui.ly)
                    ui.ly_t0 |= 2u | 4u;                 /* (LY_OPEN LY_COMBO: the armed layer, no tap when let go) */
                if (chain_busy())
                    ui_message("SONG PLAYS: STOP IT TO EDIT");
                else
                    confirm_open(CF_CLEAR_ALL, song.sel);
            }
            break;
        case B_SEQ:                                     /* tap: above; held, its layer (ui_layer.c) */
        case B_SAVE:
        case B_HOME:
            break;
        case B_FX:                                      /* the layers' buttons: ui_layer.c; REC held first: */
        case B_GLO:                                     /* their automation (rec_clear_auto) */
        case B_SCL:
        case B_EDIT:
            if (rec_first(b))
                rec_clear_auto(b);
            break;
        case B_OCTDN:
        case B_OCTUP: {
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if (act_cols() || layer_set_open() || gen_on())   /* action pages: enter / back (below); SET layers: OCT-; GEN */
                break;
            if (live_page())                            /* (p2) LIVE: OCT- RETURN / held BYPASS, OCT+ COMMIT */
                break;                                  /* (ui_live.c live_frame) */
            if (!ui.home && !ui.menu && !ui.confirm && cur_page()->graph == GR_BROWSE) {   /* (V10.24) PRESETS: OCT+ keeps
                                                         * the sound, OCT- the one before the browse */
                if (!br_armed) ui_message(b == B_OCTUP ? "TURN KNOB 1 TO BROWSE" : "NOTHING TO GO BACK TO");
                else if (b == B_OCTUP) ui_message("SOUND KEPT");
                else ui_message(undo_sound(song.sel) ? "NOTHING TO GO BACK TO" : "SOUND BEFORE");
                br_armed = 0;
                break;
            }
            if (!ui.home && !ui.menu && !ui.confirm && cur_page()->graph == GR_SCENE) {   /* (V10.26) SCENE: OCT+ dices
                                                         * the roles (each one track's), OCT- all KEEP */
                if (b == B_OCTUP) sc_dice(); else memset(scn_role, 0, sizeof scn_role);
                ui_message(b == B_OCTUP ? "SCENE DICED" : "ALL TRACKS KEEP");
                ui.force = 1;
                break;
            }
            if (!ui.home && !ui.menu && !ui.confirm && cur_page()->graph == GR_SHAPE) {   /* (V10.25) STRETCH: NUDGE's */
                ndg_step = b != B_OCTUP ? 0u : ndg_step == 1u ? 2u : 1u;   /* mode, OCT- the sequence (blue), OCT+ the
                                                         * step's MICRO (red), OCT+ again the step itself (V10.7.4) */
                ui_message(ndg_step == 2u ? "NUDGE: THE STEP" : ndg_step ? "NUDGE: THE STEP'S MICRO TIMING" : "NUDGE: THE WHOLE SEQUENCE");
                ui.force = 1;
                break;
            }
            if (!ui.home && !ui.menu && !ui.confirm && cur_page()->graph == GR_EUCLID) {   /* (V10.6) EUCLID / MODES: */
                eu_oct(b == B_OCTUP, cur_page()->id[0] == ID_MU0);   /* OCT+ a new variation, OCT- the one before;
                                                         * (V10.16) MUTATE: a mutation now, back to the anchor */
                break;
            }
            oct_key(b, both);                           /* (V10.6z4: a note held, the expression keys) */
            break;
        }
        default:                                        /* page buttons (GLO SCL ENV LFO EDIT ARP): when let go */
            if (rec_first(b)) {                         /* (REC held first: their automation) */
                rec_clear_auto(b);
                break;
            }
            if (b == B_ARP && ui.entry_open && !ui.home && cur_page()->graph == GR_ROLL) {
                ui.ent_arp ^= 1u;                       /* (0.6.37) a chord held in step recording: arpeggiate it */
                ent_message();
                break;
            }
            if (!lay)                                   /* (with FX held: swallowed) */
                ui.pg_down |= (uint16_t)(1u << id);
            break;
        }
    }
    b = ui.pg_down & ~fm1_in.buttons;
    ui.pg_down &= (uint16_t)~b;
    for (id = 0; b; id++, b >>= 1)
        if (b & 1u)
            page_tap(panel_btn_of(id));
    if (gen_on() && (oct & 2u)) {                       /* (0.9.2) GEN, HOME's pages: OCT+ the piece (the screen stays, */
        gen_press();                 /* OCT+ again: another), OCT- back to ECO */
    } else if (gen_on() && (oct & 1u)) {
        nav_go(SEC_HOME, 0, 0);
    } else if (act_cols() && (oct & 2u)) {              /* action pages: OCT+ does the picked action, */
        act_do();
    } else if (act_cols() && (oct & 1u)) {              /* OCT- drops it, or (none picked) goes HOME; PHRASES: back to */
        if (cur_page()->graph == GR_PATS && !ui.home)   /* the SEQ page it was opened on */
            nav_go(SEC_SEQ, nav_cur(), 0);
        else if (SONG_PARTS(cur_page()))                 /* (V10.6o) PARTS: back to the song */
            parts_song();
        else if (SONG_CLIPS(cur_page()))                 /* (V10.6s) CLIPS: the track stops on the next bar */
            clip_launch(song.sel, 4u);
        else if (cur_page()->graph == GR_SONG) {         /* (V10.4) ARRANGE, TIMELINE: OCT+ adds a row, OCT- deletes
                                                         * the one picked (DELETE ROW?, as TOOLS) */
            if (chain_busy()) ui_message("SONG PLAYS: STOP IT TO EDIT");
            else if (ui.song_row < chain_config.count) confirm_open(CF_DEL_ROW, ui.song_row);
            else ui_message("NOTHING TO DELETE");
        } else if (ui.act)
            ui.act = 0;
        else
            go_home();
    }
    song.grid = (uint8_t)keys_mode();                 /* (seq.c: the keys are the grid's) */
    seq_fill = (uint8_t)((fm1_in.buttons >> panel.btn[B_FX]) & 1u);   /* (0.6.42) FILL steps: FX held */
    lock_keys();                                        /* (a grid key on a hit let go: the hit goes) */
    if (gco_on()) {                                     /* (0.9.9-seq2f) GLO > COMPS: the keys are the lane's */
        gco_keys(notes);
    } else if (ckeys_on()) {                            /* (seq2g) SEQ > COMP: the keys pick the step */
        comp_keys(notes);
    } else if (skeys_on()) {                            /* (V10.6o) PARTS, TIMELINE: theirs */
        tl_keys(notes);
    } else if (song.grid && song.grid != 3u) {         /* (p2: 3, LIVE's MANO: its board, ui_live.c) */
        grid_keys(notes);
    } else if (song.seq_mode && cur_page()->graph == GR_ROLL) {   /* STEP (not CHANCE: its knobs only) */
        if (live_rec_sel() || !((song.rec >> song.sel) & 1u))   /* armed and playing: the keys record live, not
                                                         * into the cursor step too; not armed (1.2, #133): they
                                                         * only play, being on STEP writes nothing */
            ui.entry_open = 0;
        else if (!chain_busy())
            seq_entry(notes);                           /* armed and stopped: step recording at the cursor */
        else if (notes)
            ui_message("SONG PLAYS: STOP IT TO EDIT");
    }

    /* #102: the knobs a layer took above are not read again here. The TIMER5 scan (10 kHz) counts detents while this
     * pass runs, so one landing after the layer's read went to the page as well: KNOB 1..4 edited the layer's control
     * and the page under it (HOME's sound, motion recorded), PRESETS could load a sound. They stay for the next pass */
    kq |= lay;
    if (!lay && (s = panel_enc(EN_PRESET)) != 0) {
        if (gen_on())
            gen_knob(0, s);                             /* (GEN: its GENRE, the generator's preset) */
        else
            presets_turn(s);
    }
    if (!lay && (s = panel_enc(EN_ALGO)) != 0)     /* ALGORITHM: the selected track, on every page. (0.6.55) Only
                                                    * the tracks that play: TRACKS 4 stops at T4 (T5 / T6 asleep) */
        track_select((uint32_t)clamp((int32_t)song.sel + (s > 0 ? 1 : -1), 0, (int32_t)trk_live - 1));
    if (glo) {                                          /* GLO + SELECT: the tempo (0.6.67: the only way, with TAP) */
        if (sel) {
            song.g[G_BPM] = (int16_t)clamp(song.g[G_BPM] + accel(EN_SELECT, sel, 200), GP[G_BPM].min, GP[G_BPM].max);
            ui.bpm_t = 40;                              /* the header's BPM lights up; no message over the header */
        }
    } else if ((s = panel_enc(EN_SELECT)) != 0 && !lay && !ui.menu) {   /* (0.9.2, rule 1) SELECT alone: the section's
                                                        * chain, a page a detent (HOME's: ECO MACROS BEING ROUTES GEN 1..3) */
        uint64_t cph = cp_held();
        if (cph)                                        /* (seq2h) step keys held: COPY them (the range between) */
            cp_copy(cph);
        else
            nav_select(s);
    }
    for (k = 0; k < 4u && !kq; k++) {
        const page_t *pg = cur_page();
        int16_t *hv;
        uint64_t held;
        if ((s = panel_enc(EN_K1 + k)) == 0)
            continue;
        if (gen_on()) {                                 /* GEN: its settings */
            gen_knob(k, s);
            continue;
        }
        if (mac_latch && !ui.menu && !ui.confirm) {     /* (JIANT 0.4) the MACRO layer latched: M1..M4 */
            macro_v[k] = (uint8_t)clamp(macro_v[k] + accel(EN_K1 + k, s, 127), 0, 127);
            continue;
        }
        if (ui.entry_open && !ui.home && cur_page()->graph == GR_ROLL && !grid_on() && lock_held() &&
            (k == 0u || (ui.ent_arp && k < 3u))) {      /* (0.6.37) a chord being entered: KNOB 1 its LEN (ARP: RES), */
            ent_knob(k, s);                             /* 2 ORDER, 3 OCT; the others still lock */
            continue;
        }
        if ((held = lock_held()) != 0u) {               /* a step held on STEP: KNOB k locks (parameter locks) */
            lock_turn(k, s, held);
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40, ui.knob_ms = fm1_ms;
            continue;
        }
        if (ui.home || pg->scope == SC_STEP || pg->scope == SC_TRK || page_desc(pg, k, &hv) ||
            ((pg->graph == GR_USER || pg->graph == GR_MOD || pg->graph == GR_MSEQ || pg->graph == GR_PATS || BAR_PAGE(pg)) &&
             k == 0u)
            || FIT_PAGE(pg) || SPLIT_PAGE(pg) || pg->graph == GR_SONG || pg->graph == GR_DXSND || pg->graph == GR_SWARM || pg->graph == GR_FOCUS || pg->graph == GR_STAGE ||
            pg->graph == GR_GCOMP || pg->graph == GR_LIVE) {   /* (not an empty column) */
            ui.hot_col = (uint8_t)k;
            ui.hot_t = 40, ui.knob_ms = fm1_ms;
        }
        if (home_macros()) {                            /* (0.8.5) HOME's MACROS / ROUTES: M1..M4 */
            macro_v[k] = (uint8_t)clamp(macro_v[k] + accel(EN_K1 + k, s, 127), 0, 127);
        } else if (ui.home) {
            int16_t *vp;
            const param_desc_t *d = home_param(k, &vp);
            int32_t was = *vp;
            *vp = (int16_t)param_turn(d, *vp, accel(EN_K1 + k, s, d->fmt == F_ENUM ? 0 : d->max - d->min));
            knob_cap(TSEL, (uint32_t)(vp - TSEL->p));
            excite_turn(TSEL, vp, was);
        } else {
            edit_param(k, s);
        }
    }
    ui_notices();
}

/* ---------------------------------------------------- panel setup --- */
/* 30 s without input: give up and keep the old table (a stuck key cannot hang the boot). A piano key cancels at
 * any time: the keys are not part of what is taught, so one works whatever the buttons' table says */
#define SETUP_IDLE_MS 30000u
#define SETUP_GIVE_UP(t0) (fm1_ms - (t0) > SETUP_IDLE_MS || fm1_in.notes != 0u)
static void setup_title(void)
{
    lcd_fill(0, 0, 240, 240, T_BG);
    {   /* the title with its icon (the menu row's), centred together; M from y 8 as before */
        const char *t = "HARDWARE CALIBRATION";
        int32_t x = (240 - text_w(&AF_M, t)) / 2;       /* (0.8.5: no icons) */
        cv_begin(240, 24, T_BG);
        GFX_HOOK_ALIGN(0, 0, 240, 0, AL_H, "calibration title centred");
        cv_text(x, HEAD_MY, &AF_M, t, T_TEXT);
        cv_blit(0, 5);
    }
    draw_text_box(0, 32, 240, &AF_S, "TEACH EACH BUTTON AND KNOB", T_MID, 1);
    lcd_fill(16, 56, 208, 1, T_LINE);
    draw_text_box(0, 206, 240, &AF_S, "ANY KEY: CANCEL", T_DIM, 1);
}
static void setup_show(const char *what, const char *name)     /* "PRESS" / "TURN RIGHT", the control */
{
    draw_text_box(0, 80, 240, &AF_S, what, T_MID, 1);
    draw_text_box(0, 100, 240, &AF_L, name, T_THEME, 1);
}
static void panel_setup(void)
{
    uint32_t i, used = 0, t0 = fm1_ms;
    const panel_t old = panel;
    setup_title();
    while (fm1_in.buttons) {                             /* wait for OCT-/OCT+ release */
        fm1_wdt_feed();
        if (SETUP_GIVE_UP(t0))
            goto timeout;
    }
    fm1_input_edges(0);
    for (i = 0; i < NB; i++) {
        uint32_t p = 0, id;
        setup_show("PRESS", B_NAME[i]);
        t0 = fm1_ms;
        while (!(p & ~used)) {
            fm1_wdt_feed();
            p |= fm1_input_edges(0);
            if (SETUP_GIVE_UP(t0))
                goto timeout;
        }
        for (id = 0; id < 14u; id++)
            if (((p & ~used) >> id) & 1u)
                break;
        panel.btn[i] = (uint8_t)id;
        used |= 1u << id;
    }
    used = 0;
    for (i = 0; i < NE; i++) {
        uint32_t e;
        int32_t st = 0;
        setup_show("TURN RIGHT", E_NAME[i]);
        for (e = 0; e < 7u; e++)
            fm1_enc_take(e);
        t0 = fm1_ms;
        for (;;) {
            fm1_wdt_feed();
            if (SETUP_GIVE_UP(t0))
                goto timeout;
            for (e = 0; e < 7u; e++)
                if (!((used >> e) & 1u) && (st = fm1_enc_take(e)) != 0)
                    break;
            if (e < 7u)
                break;
        }
        panel.enc[i] = (uint8_t)e;
        panel.dir[i] = (int8_t)(st > 0 ? 1 : -1);
        used |= 1u << e;
        fm1_delay_ms(300);
        fm1_enc_take(e);
    }
    panel.magic = PANEL_MAGIC;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    return;
timeout:
    panel = old;
    lcd_fill(0, 0, 240, 240, T_BG);
    ui.force = 1;
    ui_message("SETUP CANCELLED");
}

#include "ui_dx.c"                                  /* EDIT > SOUND / SOUND 2: the DRUM-X kit */



