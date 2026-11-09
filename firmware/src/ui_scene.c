/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT V10.25: SCL > SCENE, the multitimbral mode's page (seq.c scene_key plays it). Included by ui_graph.c.
 * KNOB 1 SCENE OFF / ON (on: the keys play every track with a role), KNOB 2 the TRACK, KNOB 3 its ROLE (KEEP CHORD ARP
 * BASS LEAD), KNOB 4 its CHORD (the chord CHORD and ARP play; OFF: a triad of the scale). A role sets its track up:
 * ARP turns the track's arp on (UP) if it was off, CHORD BASS LEAD turn it off, CHORD makes the voice POLY. A kit
 * keeps its sequence (KEEP): the keys do not reach it. The graph: six columns, each track's role and the notes the
 * last key played makes on it */
static uint8_t sc_trk = 5;                               /* the track KNOB 2 picked (T6) */
static int16_t sc_v[4];
static param_desc_t sc_d;
static const char *const SC_ROLE[SC_N] = {"KEEP", "CHORD", "ARP", "BASS", "LEAD", "DRUM"};
/* (V10.6j) KNOB 4 by the role: BASS LEAD their pattern (seq.c SC_PAT), DRUM its beat's style */
static const char *const SC_PN[3][SC_NPAT] = {{"ROOT", "PUMP", "OCTAVE", "R-5", "WALK", "ROLL", "BOUNCE", "ACID", "GEN"},
                                         {"HOOK", "ARC", "RISE", "STEPS", "SPARSE", "BOUNCE", "CONVRG", "RANDOM", "GEN"},
                                         {"FOUR", "BREAK", "HALF", "SPARSE", "GEN"}};
static const param_desc_t *sc_desc(uint32_t k, int16_t **valp)
{
    static const char *const ONOFF[2] = {"OFF", "ON"};
    static const char *const TN[NTRK] = {"T1", "T2", "T3", "T4", "T5", "T6"};
    static const char *const KR[2] = {"KEEP", "DRUM"};
    track_t *t = &trk[sc_trk % NTRK];
    uint32_t r = scn_role[sc_trk % NTRK], dr = drum_track(t);
    k &= 3u;
    if (k == 3u && r != SC_BASS && r != SC_LEAD && !(dr && r == SC_DRUM)) {   /* the picked track's own CHORD */
        *valp = &t->p[P_CHRD];
        return track_desc(t, P_CHRD);
    }
    sc_d.fmt = F_ENUM; sc_d.min = 0; sc_d.def = 0; sc_d.unit = 0;
    if (k == 0u) { sc_d.label = "SCENE"; sc_d.max = 1; sc_d.names = ONOFF; sc_v[0] = scn_on; }
    else if (k == 1u) { sc_d.label = "TRACK"; sc_d.max = NTRK - 1; sc_d.names = TN; sc_v[1] = sc_trk; }
    else if (k == 2u && dr) { sc_d.label = "ROLE"; sc_d.max = 1; sc_d.names = KR; sc_v[2] = r == SC_DRUM; }   /* (V10.6j) a kit: KEEP, DRUM */
    else if (k == 2u) { sc_d.label = "ROLE"; sc_d.max = SC_DRUM - 1; sc_d.names = SC_ROLE; sc_v[2] = r < SC_DRUM ? r : SC_KEEP; }
    else { sc_d.label = r == SC_DRUM ? "STYLE" : "PATTERN"; sc_d.max = r == SC_DRUM ? SC_DGEN : SC_GEN;
           sc_d.names = SC_PN[r == SC_DRUM ? 2 : r == SC_LEAD]; sc_v[3] = (int16_t)(scn_pat[sc_trk % NTRK] % (r == SC_DRUM ? SC_DGEN + 1u : SC_NPAT)); }
    *valp = &sc_v[k];
    return &sc_d;
}
static void sc_setup(uint32_t i)                        /* track i set up for its role */
{
    track_t *t = &trk[i % NTRK];
    uint32_t r = scn_role[i % NTRK];
    if (drum_track(t) || r == SC_KEEP) return;
    if (r == SC_ARP) { if (!t->p[P_AMODE]) t->p[P_AMODE] = AM_UP; }
    else if ((r == SC_BASS || r == SC_LEAD) && scn_pat[i % NTRK] % SC_NPAT == SC_GEN) {   /* (V10.6m) GEN: the track's own line
                                                         * (sc_gen), each key's chord moving it in the scale */
        t->p[P_AMODE] = AM_TRNS; t->p[P_QUANT] = QN_SEQ; t->trn = 0;
    } else if (r == SC_BASS || r == SC_LEAD) {           /* (V10.6j) the pattern: the arp over the chord's notes */
        const uint8_t *q = SC_PAT[r == SC_LEAD][scn_pat[i % NTRK] & 7u];
        t->p[P_AMODE] = q[1]; t->p[P_ARATE] = q[2]; t->p[P_AOCT] = 1; t->p[P_AGATE] = q[3]; t->p[P_APROB] = q[4];
    }
    else t->p[P_AMODE] = AM_OFF;
    if (r == SC_CHORD) t->p[P_VOICE] = V_POLY;
}
static void sc_gen(uint32_t i);                         /* (V10.6m) gen_track.c */
static uint8_t sc_dk;                                   /* (V10.6r) DRUM GEN: the last key (its variation, sc_gen) */
/* (V10.26) the roles diced: one track for each of CHORD ARP BASS LEAD at most (never two doing the same, never two
 * playing chords: the polyphony), the melodic tracks awake only, the rest KEEP; at least two roles */
/* (V10.6n) the scene's key: every melodic track in one mode (its ROOT kept); a track with no scale (CHR) made a
 * chord of the major scale: the scene sounded major whatever was played. sc < 0: CHR tracks only, minor */
static void sc_key(int32_t sc)
{
    static const uint8_t MD[6] = {2, 3, 8, 7, 4, 1}, MW[6] = {40, 20, 10, 5, 10, 15};   /* MIN DOR PHRY HARM MIX MAJ */
    uint32_t i, k = 0, x = rng() % 100u;
    while (k < 5u && x >= MW[k]) x -= MW[k++];
    for (i = 0; i < trk_live && i < NTRK; i++)
        if (!drum_track(&trk[i]) && (sc >= 0 || !trk[i].p[P_SCALE]))
            trk[i].p[P_SCALE] = (int16_t)(sc >= 0 ? MD[k] : 2);
}
static void sc_dice(void)
{
    uint32_t i, j, m[NTRK], nm = 0, r[4] = {SC_CHORD, SC_ARP, SC_BASS, SC_LEAD};
    for (i = 0; i < NTRK; i++) {
        scn_role[i] = SC_KEEP;
        if (i < trk_live && !drum_track(&trk[i])) m[nm++] = i;
    }
    for (i = nm; i > 1u; i--) { j = rng() % i; uint32_t x = m[i - 1u]; m[i - 1u] = m[j]; m[j] = x; }   /* (the tracks shuffled) */
    for (i = 4u; i > 1u; i--) { j = rng() % i; uint32_t x = r[i - 1u]; r[i - 1u] = r[j]; r[j] = x; }   /* (the roles too) */
    j = nm < 4u ? nm : 2u + rng() % 3u;                  /* (2 .. 4 roles, as many tracks as there are) */
    for (i = 0; i < j && i < nm; i++) { scn_role[m[i]] = (uint8_t)r[i]; sc_setup(m[i]); }
    sc_key(1);                                           /* (V10.6n) and a mode, mostly dark */
}
static __attribute__((noinline)) void sc_put(uint32_t k, int32_t v)
{
    uint32_t i, need = 0;
    if (k == 0u) {
        scn_on = (uint8_t)(v != 0);
        for (i = 0, need = 0; i < NTRK; i++) need |= scn_role[i];
        if (scn_on && !need) sc_dice();                  /* (all KEEP: a scene diced, so ON plays something) */
        if (scn_on) sc_key(-1);                          /* (V10.6n) no scale: minor, not the major of CHR */
        need = 0;
        for (i = 0; scn_on && i < NTRK; i++) {
            sc_setup(i);
            need |= (uint32_t)(i >= trk_live && scn_role[i] && !drum_track(&trk[i]));
        }
        ui_message(!scn_on ? "SCENE OFF: KEYS PLAY ONE TRACK" : need ? "SCENE: T5 T6 ASLEEP (MENU > 6 TRACKS)" : "SCENE: KEYS PLAY THE ROLES");
    } else if (k == 1u)
        sc_trk = (uint8_t)clamp(v, 0, NTRK - 1);
    else if (k == 3u) {                                  /* (V10.6j) BASS LEAD: the pattern; DRUM: the style */
        scn_pat[sc_trk % NTRK] = (uint8_t)clamp(v, 0, SC_GEN);
        sc_setup(sc_trk);
        sc_gen(sc_trk);
    } else if (k == 2u && drum_track(&trk[sc_trk % NTRK]))
        scn_role[sc_trk % NTRK] = v ? SC_DRUM : SC_KEEP;
    else if (k == 2u) {
        scn_role[sc_trk % NTRK] = (uint8_t)clamp(v, 0, SC_DRUM - 1);
        for (i = 0; v != SC_KEEP && i < NTRK; i++)        /* (V10.26) a role is one track's: another with it goes KEEP */
            if (i != sc_trk && scn_role[i] == (uint8_t)v) {
                char b[16] = "T  ROLE TO KEEP";
                b[1] = (char)('1' + i);
                scn_role[i] = SC_KEEP;
                ui_message(b);
            }
        sc_setup(sc_trk);
        sc_gen(sc_trk);
    }
    ui.force = 1;
}
static void graph_scene(void)
{
    uint32_t i, j, n, h = graph_h();
    int32_t cw = 38, x0 = 6, y0 = 30, yh = (int32_t)h - y0 - 16;
    uint8_t o[KB_CH + 1];
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        int32_t x = x0 + (int32_t)i * cw;
        uint32_t r = drum_track(t) && scn_role[i] != SC_DRUM ? SC_KEEP : scn_role[i];
        uint16_t c = r == SC_KEEP ? T_DIM : r == SC_CHORD ? T_THEME : r == SC_ARP ? T_MID : r == SC_BASS ? T_ACCENT : T_TEXT;
        char b[3] = {'T', (char)('1' + i), 0};
        cv_text_in(x, 2, cw - 4, &AF_X, b, i == sc_trk ? T_TEXT : T_DIM, T_SURF);
        cv_text_in(x - 2, 14, cw, &AF_X, drum_track(t) && r == SC_KEEP ? "KIT" : SC_ROLE[r], c, T_SURF);
        if (i == sc_trk) cv_frame(x - 3, 0, cw - 1, 25, T_ACCENT);
        if (i >= trk_live) continue;
        if (r == SC_KEEP || r == SC_DRUM) {              /* its own sequence: the steps, dim; DRUM: the beat's lanes lit */
            for (j = 0; j < 16u; j++) {
                uint32_t h = t->step[j].hit;
                int32_t y = y0 + (int32_t)j * yh / 16;
                if (r == SC_DRUM) {
                    if (h & 1u) cv_rect(x + 1, y, 9, 2, heat_col(60));
                    if (h & 6u) cv_rect(x + 12, y, 9, 2, heat_col(150));
                    if (h & 24u) cv_rect(x + 23, y, 7, 2, heat_col(235));
                } else if (t->step[j].time == ST_NOTE) cv_rect(x + 2, y, cw - 10, 2, T_DIM);
            }
            continue;
        }
        n = scn_notes(t, r, last_note, o, 1);               /* the last key's notes on it, by pitch */
        for (j = 0; j < n; j++) {
            int32_t y = y0 + yh - 4 - (clamp(o[j], 36, 96) - 36) * (yh - 6) / 60;
            cv_rect(x + 2 + (r == SC_ARP ? (int32_t)j * 7 : 0), y, r == SC_ARP ? 6 : cw - 10, 3, note_col(o[j]));
        }
    }
    cv_text_in(4, (int32_t)h - 11, 232, &AF_X, scn_on ? "OCT+ DICE  OCT- ALL KEEP" : "SCENE OFF: KNOB 1 TURNS IT ON",
               scn_on ? T_ACCENT : T_DIM, T_SURF);
}

/* (V10.6j) SCENE's DRUM: key k's beat into the kit, 16 steps (main loop: ui_input.c's poll; the key in the audio side
 * only asks, seq.c scn_dreq). STYLE FOUR BREAK HALF SPARSE; the higher the key in its octave, the busier: hats in 16ths,
 * a clap, an open hat, a fill */
static __attribute__((noinline)) void sc_beat(void)
{
    static const uint16_t KICK[4] = {0x1111, 0x0481, 0x0801, 0x0101}, SNR[4] = {0x1010, 0x1010, 0x0100, 0x1000},
                          HAT[4] = {0x4444, 0x5555, 0x5555, 0x4040};
    uint8_t hit[16], acc[16];
    uint32_t k = scn_dreq, i, j, st, d, x;
    if (!k) return;
    scn_dreq = 0;
    for (i = 0; i < NTRK && !(drum_track(&trk[i]) && scn_role[i] == SC_DRUM); i++) ;
    if (i == NTRK) return;
    st = scn_pat[i] % (SC_DGEN + 1u); d = (k - 1u) % 12u; x = k * 2654435761u + st * 40503u;
    if (st == SC_DGEN) { sc_dk = (uint8_t)(k - 1u); sc_gen(i); return; }   /* (V10.6r) GEN: GENE's drums, the key's variation */
    for (j = 0; j < 16u; j++) {
        uint32_t b = 1u << j, h = 0;
        x = x * 1664525u + 1013904223u;
        if (KICK[st] & b || (d > 6u && st == 1u && j == 7u)) h |= 1u;
        if (d > 1u && SNR[st] & b) h |= 2u;
        if (d > 5u && st == 0u && SNR[0] & b) h |= 4u;
        if (HAT[st] & b || (d > 3u && (x >> 28) < d)) h |= 8u;
        if (d > 7u && j == 14u) h = (h & ~8u) | 16u;
        if (d > 9u && j >= 12u && (j & 1u)) h |= 2u;
        hit[j] = (uint8_t)h; acc[j] = (uint8_t)(j % 4u ? 0u : (h & 1u));
    }
    load_begin(&trk[i], LOAD_PAT);
    load_grid16(&trk[i], hit, acc);
    load_end(&trk[i]);
    ui.force = 1;
}
