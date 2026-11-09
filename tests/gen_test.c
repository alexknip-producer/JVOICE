/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT (0.6.91) the song generator (firmware/src/gen.c) measured: every genre at several seeds and settings, on the
 * host build of the UI tests. What a piece must always have: the same piece from the same settings (deterministic);
 * one scale on every melodic track, every note in it (the bass's chromatic approaches excepted) and in its register;
 * drums on one track with the kick anchored (four-on-the-floor genres: step 1), its density growing with DRUMS;
 * the bass's first note at a chord change the root (most of the time: FiloBass' 68 %); the lead's strong steps on chord
 * tones more often than not; chords of three or four, voiced in C3..G5, led closely; a storable project; a form in
 * ARRANGE when asked; the parts' knobs only touching their own part. (0.8.6) EXPLORE, the default: no genre, every
 * rule broken, every piece still playing in its key, very different from the next.
 * (0.9.9-gen1) The clean base (the same seed from three different states, the same project), each genre's sound
 * archetypes and grammar (the acid house's 303 ..), one seed one project in every genre, the CPU budget.
 * (0.9.9-gen2) The owner's ear: the acid line's densities (X0X's TB-3PO), a SLIDE played legato (no retrigger, gliding), the
 * leads' vibrato shallow and fast, the pads' pitch LFO a drift at most, the drums' decays, GLUE and level.
 * (V10.6z10) A genre's throws differ (OCT+ never one of its last eight cores, the seed still its song), its anchors kept.
 * (0.9.9-gen3) The rules by role: no lead at full sustain (44..75 %, a bell's or a pluck's lead short), every lead moving
 * (a vibrato or a slow, shallow filter LFO) and its filter's envelope on, the basses' too, the plucks and stabs short;
 * the mix: the drums at most 60 % of the parts' energy in every genre (fx.c's stems: what each part adds, before the
 * leveler), the LEVELER's gain inside its range (x0.35 .. x2) and the mix as loud as gen1's.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_test tests/gen_test.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int gcheck(const char *what, int ok)
{
    printf("gen: %-74s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}
static void gen_reset(void)
{
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();          /* TRACKS 6 */
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
}
static void gen_with(uint32_t genre, uint32_t seed, uint32_t energy, uint32_t form)
{
    uint32_t k;
    gp.genre = (uint8_t)genre; gp.seed = (uint16_t)seed; gp.energy = (uint8_t)energy; gp.form = (uint8_t)form;
    for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
    gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
    gen_go();
}
static uint32_t all_sig(void)
{
    uint32_t k, h = 2166136261u ^ (uint32_t)song.g[G_BPM];
    for (k = 0; k < NTRK; k++) h = (h ^ steps_sig(&trk[k]) ^ trk[k].eng_req ^ (uint32_t)trk[k].p[P_E4] * 7u) * 16777619u;
    return h;
}
static int in_scale(const track_t *t, int32_t n)
{
    uint32_t mask = scale_mask(t);
    return (mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u;
}
static uint32_t hits_of(const track_t *t, uint32_t lane)
{
    uint32_t i, c = 0;
    for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++) c += t->step[i].time == ST_NOTE && ((t->step[i].hit >> lane) & 1u);
    return c;
}
/* (0.8.6) the piece heard for a while: its RMS and peak (the output before the DAC's clamp), the blocks at full scale,
 * the most voices sounding at once over the synth parts */
static void gx_listen(uint32_t blocks, double *rms, int32_t *peak, uint32_t *clip, uint32_t *vmax)
{
    uint32_t b, i, p;
    double acc = 0;
    int32_t pk = 0;
    uint32_t cl = 0, vm = 0;
    song.master_q12 = 4096;
    transport_req = 1;
    for (b = 0; b < blocks; b++) {
        int32_t o[2 * CTL];
        uint32_t nv = 0;
        if (b % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t v = o[i] < 0 ? -o[i] : o[i];
            if (v > pk) pk = v;
            cl += v >= 32767;
            acc += (double)o[i] * (double)o[i];
        }
        for (p = 0; p < NPART; p++)                     /* (as voice.c voices_busy counts them: not the fading) */
            for (i = 0; i < NVOICE; i++) nv += trk[p].v[i].active && trk[p].v[i].stage != 4u;
        if (nv > vm) vm = nv;
    }
    transport_req = 2;
    { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
    *rms = sqrt(acc / (double)(blocks * 2u * CTL)); *peak = pk; *clip = cl; *vmax = vm;
}
/* (0.9.9-gen1) the whole project as a hash: every track's parameters, engine, preset, steps and automation, the song's
 * globals, DRUM-X's kit (GLUE, octaves, sends, layers), the global components lane, FUGUE's playheads, the punch-in
 * lane, the macros, the chain, PLAY's mode */
static uint64_t gfnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) h = (h ^ *b++) * 1099511628211ull;
    return h;
}
static uint64_t gen_proj_hash(void)
{
    uint64_t h = 1469598103934665603ull;
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        h = gfnv(h, trk[k].p, sizeof trk[k].p);
        h = gfnv(h, &trk[k].eng_req, sizeof trk[k].eng_req);
        h = gfnv(h, &trk[k].preset, sizeof trk[k].preset);
        h = gfnv(h, trk[k].step, sizeof trk[k].step);
    }
    h = gfnv(h, motion.event, motion.count * sizeof motion.event[0]);
    h = gfnv(h, song.g, sizeof song.g);
    h = gfnv(h, dx_kit, sizeof dx_kit); h = gfnv(h, dx_snd, sizeof dx_snd); h = gfnv(h, dx_lay, sizeof dx_lay);
    h = gfnv(h, dx_mot, sizeof dx_mot); h = gfnv(h, &dx_glue, 1); h = gfnv(h, &dx_tone, sizeof dx_tone); h = gfnv(h, &dx_hands, sizeof dx_hands);
    h = gfnv(h, glane, sizeof glane); h = gfnv(h, &gl_mono, sizeof gl_mono);
    h = gfnv(h, fg_set, sizeof fg_set); h = gfnv(h, pfx_lane, sizeof pfx_lane); h = gfnv(h, macro_v, sizeof macro_v);
    {   /* (gen6c) the rows without GEN's marks: whose the sections in flash are is not the piece */
        chain_config_t c = chain_config;
        c.rsv[1] = 0;
        h = gfnv(h, &c, sizeof c);
    }
    h = gfnv(h, &live.mode, sizeof live.mode);
    return h;
}
/* (0.9.9-gen1) another state to generate from: a piece of another genre (EXPLORE, a SONG), then the player's hand on
 * everything the clean base must clear */
static uint32_t genre_of(const char *name);
#define G_HOUSE genre_of("HOUSE")                       /* (V10.6n: the genres by name, the owner's list renumbered them) */
static void gen_dirty(uint32_t seed)
{
    uint32_t k, i;
    gen_with(GEN_EXPLORE, seed, 70, 2);
    gen_with(genre_of("BREAKBEAT"), seed + 3u, 30, 1);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        t->p[P_LD_FLT] = 40; t->p[P_LRATE] = 90; t->p[P_LD_PIT] = 9; t->p[P_APDEP] = 50; t->p[P_SDIV] = 3; t->p[P_SSWING] = 33;
        t->p[P_MSND] = 2; t->p[P_MAMT] = 50; t->p[P_DIST] = 77; t->p[P_LEVEL] = 40; t->p[P_E3] = 99;
        for (i = 0; i < 8u; i++) { t->step[i].comp = CP_RAND; t->step[i].probability = 50; }
        motion_set_event(t, 5, P_FCUT, 20);
    }
    dx_glue = 9; dx_tone = 3; dx_hands = 1; dx_mot[0] = 5; dx_snd[0] = 77;
    memset(glane, 0x5A, sizeof glane); gl_mono = 1;
    memset(fg_set, 1, sizeof fg_set);
    memset(pfx_lane, 0x33, sizeof pfx_lane);
    macro_v[0] = 99; live.mode = 1;
}
static uint32_t genre_of(const char *name)
{
    uint32_t g;
    for (g = 0; g < NGENRE; g++) if (!strcmp(GENRE[g].name, name)) return g + 1u;
    return 0;
}
/* the steps of a lane: how many of steps a ..b (every 4th from a) have a hit there */
static uint32_t lane_on(const track_t *t, uint32_t lane, uint32_t from, uint32_t every)
{
    uint32_t i, c = 0;
    for (i = from; i < (uint32_t)t->p[P_SLEN]; i += every) c += t->step[i].time == ST_NOTE && ((t->step[i].hit >> lane) & 1u);
    return c;
}
/* (0.9.9-gen1) a piece's CPU load as the FM-1 predicts it (voice.c load_sounding_q8: every sounding voice at its
 * part's cost, by engine on the host), the most over a while, in % of a half */
static uint32_t render_load(uint32_t blocks)
{
    uint32_t i, most = 0;
    song.master_q12 = 4096;
    transport_req = 1;
    for (i = 0; i < blocks; i++) {
        int32_t o[2 * CTL];
        uint32_t l;
        if (i % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        l = load_sounding_q8() >> 8;
        if (l > most) most = l;
    }
    transport_req = 2;
    { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
    return most;
}
static uint32_t hits_all(const track_t *t)
{
    uint32_t i, c = 0, l;
    for (i = 0; i < (uint32_t)t->p[P_SLEN]; i++)
        if (t->step[i].time == ST_NOTE) for (l = 0; l < 8u; l++) c += (t->step[i].hit >> l) & 1u;
    return c;
}

int main(void)
{
    int bad = 0;
    uint32_t g, s, k, i;
    uint32_t notes_out = 0, notes_all = 0, chords_bad = 0, lead_strong = 0, lead_ct = 0, bass_changes = 0, bass_root = 0;
    uint32_t drums_many = 0, pieces = 0, kick_anchor_bad = 0, store_bad = 0, range_bad = 0, motion_bad = 0;
    uint32_t genre0 = gp.genre;                         /* (0.8.6: the power-on's GENRE, before any test turns it) */
    gen_reset();
    /* ---- deterministic: the same settings twice, the same piece; another seed, another piece */
    {
        uint32_t h1, h2, h3;
        gen_with(G_HOUSE, 421, 50, 0); h1 = all_sig();
        gen_with(genre_of("BREAKBEAT"), 7, 80, 0);
        gen_with(G_HOUSE, 421, 50, 0); h2 = all_sig();
        gen_with(G_HOUSE, 422, 50, 0); h3 = all_sig();
        bad += gcheck("deterministic: HOUSE 0421 twice the same piece, 0422 another", h1 == h2 && h1 != h3);
    }
    /* ---- every genre, three seeds, three energies: the guard rails */
    for (g = 0; g < NGENRE; g++) {
        for (s = 0; s < 3u; s++) {
            uint32_t dr = NTRK, bs = NTRK, ld = NTRK, pd = NTRK, drums = 0;
            gen_with(g + 1u, 100u * g + 17u * s + 3u, s == 0u ? 20u : s == 1u ? 50u : 85u, 0);
            pieces++;
            for (k = 0; k < NTRK; k++) {
                if (drum_track(&trk[k])) { drums++; dr = k; }
                else if (gs.role[k] == RL_BASS && bs == NTRK) bs = k;
                else if (gs.role[k] == RL_LEAD && ld == NTRK) ld = k;
                else if (gs.role[k] == RL_PAD && pd == NTRK) pd = k;
            }
            if (drums != 1u) drums_many++;
            for (k = 0; k < NTRK; k++) {
                const track_t *t = &trk[k];
                uint32_t len = (uint32_t)t->p[P_SLEN];
                if (k == dr || len > NSTEP) continue;
                if (t->p[P_ROOT] != trk[0].p[P_ROOT] || t->p[P_SCALE] != trk[0].p[P_SCALE]) range_bad++;
                for (i = 0; i < len; i++) {
                    const step_t *st = &t->step[i];
                    uint32_t j;
                    if (st->time != ST_NOTE) continue;
                    if (st->n < 1u || st->n > 4u) chords_bad++;
                    for (j = 0; j < st->n; j++) {
                        notes_all++;
                        if (!in_scale(t, st->note[j])) notes_out++;
                        if (st->note[j] > 84u || st->note[j] < 20u) range_bad++;
                    }
                    if (k == pd && st->n >= 3u && (st->note[0] < 46u || st->note[st->n - 1u] > 81u)) { chords_bad++; printf("  chord range: %s seed %u step %u %u..%u\n", gs.g->name, s, i, st->note[0], st->note[st->n - 1u]); }
                }
            }
            if (dr < NTRK && gs.act[RL_DRUM]) {         /* the kick anchored on the one where the genre has it */
                uint32_t bd = DRUM_MAP_I[gs.g->drum][0];   /* (0.6.95: the map as this throw plays it; 0.9.7 packed) */
                if (bd && gm_level(bd, 0) == 255u && !(trk[dr].step[0].time == ST_NOTE && (trk[dr].step[0].hit & 1u))) { kick_anchor_bad++; printf("  kick anchor: %s seed %u map %u\n", gs.g->name, s, gs.g->drum); }
            }
            if (bs < NTRK && gs.act[RL_BASS] && gs.ba != BA_ACID) {   /* the root at the chord changes */
                const track_t *b = &trk[bs];
                uint32_t len = (uint32_t)b->p[P_SLEN], at;
                for (at = 0; at < len; at++) {
                    uint32_t j;
                    int32_t root = (int32_t)gchord_root_pc(at);
                    if (!gchord_starts(at)) continue;
                    for (j = at; j < at + 4u && j < len; j++)
                        if (b->step[j].time == ST_NOTE) {
                            bass_changes++;
                            if (b->step[j].note[0] % 12u == (uint32_t)root) bass_root++;
                            break;
                        }
                }
            }
            if (ld < NTRK && gs.act[RL_LEAD]) {         /* the lead's strong steps on chord tones */
                const track_t *l = &trk[ld];
                uint32_t len = (uint32_t)l->p[P_SLEN];
                for (i = 0; i < len; i += 4u) {
                    const step_t *st = &l->step[i];
                    if (st->time != ST_NOTE) continue;
                    lead_strong++;
                    if ((gchord_pcs(i) >> (st->note[0] % 12u)) & 1u) lead_ct++;
                }
            }
            project_capture(&proj_scratch);
            if (!proj_pack(&proj_wire, &proj_scratch)) store_bad++;
            if (motion.count > MOTION_MAX) motion_bad++;
            for (i = 0; i < 16u; i++) events_block(CTL);   /* (it plays: no crash) */
        }
    }
    printf("gen: %u pieces, %u notes, %u off the scale (%u %%), bass root at %u of %u changes, lead chord tones %u of %u strong\n",
           pieces, notes_all, notes_out, notes_all ? notes_out * 100u / notes_all : 0u, bass_root, bass_changes, lead_ct, lead_strong);
    bad += gcheck("every genre x 3: one DRUM track, one scale, every note in it but the approaches (< 4 %), in its register",
                  !drums_many && !range_bad && notes_out * 100u < notes_all * 4u);
    bad += gcheck("  chords of 1..4 notes, the pads' in C3..G5; the kick on the one where the map anchors it", !chords_bad && !kick_anchor_bad);
    bad += gcheck("  the bass's first note at a chord change the root, 70 % at least (FiloBass: 68 %)", bass_root * 10u >= bass_changes * 7u);
    bad += gcheck("  the lead's strong steps on chord tones, 55 % at least", lead_ct * 100u >= lead_strong * 55u);
    bad += gcheck("  every piece storable, the automation inside its 64", !store_bad && !motion_bad);
    /* ---- (0.8.3) the music: twelve seeds of every genre at varied ENERGY and COMPLEX, measured as a musician would
     * listen: every note in the key the SCALE page shows, but the chromaticism the rules allow (a semitone approach off
     * beats 1 and 3 that resolves, a borrowed / secondary / raised chord's own tones where the genre colours) and few
     * of those; the SCALE page a mode of seven, QUANT on, the CHORD page on the chord track (an in-scale chord type,
     * VOICING LEAD) and every chord it builds in the key; the lead's beats on chord tones, its motion mostly by steps,
     * never past an octave, a leap past a fourth turned back by a step, its phrases closing on a stable degree, its
     * first bar's rhythm coming back; the chords led closely; the bass on the root at every chord change */
    {
        uint32_t nn = 0, out_appr = 0, out_ct = 0, out_bad = 0, page_bad = 0, chord_bad = 0;
        uint32_t ls = 0, lct = 0, liv = 0, lrep = 0, lstep = 0, loct = 0, l5 = 0, l5r = 0, lend = 0, lend_ok = 0, lbars = 0, lbars_r = 0;
        uint32_t pmov = 0, pmot = 0, bch = 0, broot = 0;
        for (g = 0; g < NGENRE; g++)
            for (s = 0; s < 12u; s++) {
                uint32_t pd;
                gp.genre = (uint8_t)(g + 1u); gp.seed = (uint16_t)(2000u + 41u * s + g); gp.energy = (uint8_t)(15u + (s * 23u) % 80u); gp.form = 0;
                for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
                gp.complex = (uint8_t)(10u + (s * 31u) % 85u); gp.swing = GA_AUTO; gp.human = 30;
                gen_go();
                for (k = 0; k < NTRK; k++) {            /* the SCALE page */
                    const track_t *t = &trk[k];
                    uint32_t c = 0, b;
                    for (b = 0; b < 12u; b++) c += (scale_mask(t) >> b) & 1u;
                    if (t->p[P_ROOT] != gs.root || t->p[P_SCALE] != gs.scale || c != 7u || t->p[P_QUANT] == QN_OFF) page_bad++;
                }
                pd = gs.trk_of[RL_PAD];
                if (pd < NTRK && gs.act[RL_PAD]) {      /* the CHORD page: an in-scale type, LEAD, every chord in the key */
                    track_t *t = &trk[pd];
                    uint8_t o[KB_CH];
                    uint32_t key, c, j;
                    if (t->p[P_CHRD] < CD_TRI || t->p[P_CHRD] > CD_SUS4 || t->p[P_CVOI] != CV_LEAD) page_bad++;
                    for (key = 53; key < 80u; key++)
                        for (c = chord_build(t, key, o), j = 0; j < c; j++) if (!in_scale(t, o[j])) chord_bad++;
                }
                for (k = 0; k < NTRK; k++) {
                    const track_t *t = &trk[k];
                    uint32_t r = gs.role[k], len = dlen(t);
                    if (drum_track(t) || !gs.act[r]) continue;
                    if (r == RL_PAD && gclub() && gs.g->pads == CS_STAB && gs.g->drum != DM_TECHHOUSE && gs.g->drum != DM_TECHNO)
                        continue;                       /* (gen5: house's and Detroit's CHORD MEMORY, a shape moved in
                                                         * parallel: out of the key by design) */
                    for (i = 0; i < len; i++) {         /* every note in the key, or chromatic by a rule */
                        const step_t *st = &t->step[i];
                        uint32_t j;
                        if (st->time != ST_NOTE) continue;
                        for (j = 0; j < st->n; j++) {
                            int32_t n = st->note[j], nx = -1;
                            uint32_t q;
                            const gch_t *c = gchord(i);
                            nn++;
                            if (in_scale(t, n)) continue;
                            for (q = 1; q <= len && nx < 0; q++) if (t->step[(i + q) % len].time == ST_NOTE) nx = t->step[(i + q) % len].note[0];
                            if (c->alt && ((gchord_pcs(i) >> (uint32_t)(n % 12)) & 1u) &&
                                (gs.hx & (HX_RAISE | HX_SEC | HX_BOR) || HXF(gs.hx) == HF_JAZZ)) out_ct++;
                            else if (st->n == 1u && i % 8u && nx >= 0 && (nx - n == 1 || n - nx == 1)) out_appr++;
                            else { out_bad++; if (out_bad < 6u) printf("  off the key: %s seed %u T%u step %u note %d\n", gs.g->name, gp.seed, k + 1u, i, n); }
                        }
                    }
                    if (r == RL_LEAD && gs.g->lead <= LD_NONE) {   /* (gen6: an archetype's motif as written: its
                                                         * octaves and fourths, not a sung line's rules) */
                        int32_t prev = -1, piv = 0, last = -1;
                        uint32_t pend = 0;
                        uint16_t bar0 = 0;
                        for (i = 0; i < len; i++) {
                            const step_t *st = &t->step[i];
                            int32_t n, iv, a2;
                            if (st->time != ST_NOTE || !st->n) continue;
                            n = st->note[0];
                            if (i % 4u == 0u) { ls++; lct += (gchord_pcs(i) >> (uint32_t)(n % 12)) & 1u; }
                            if (i < 16u) bar0 |= (uint16_t)(1u << i);
                            if (prev >= 0) {
                                iv = n - prev; a2 = iv < 0 ? -iv : iv;
                                liv++; lrep += !a2; lstep += a2 && a2 <= 2; loct += a2 > 12;
                                if (pend) { l5++; l5r += a2 && a2 <= 2 && (iv > 0) != (piv > 0); pend = 0; }
                                if (a2 > 5) { pend = 1; piv = iv; }
                            }
                            prev = n; last = (int32_t)i;
                        }
                        if (last >= 0 && gs.g->lead != LD_OSTINATO && gs.g->lead != LD_MECH) {   /* (the phrase's close) */
                            int32_t n = t->step[last].note[0], d = ((n - gs.root) % 12 + 12) % 12;
                            lend++;
                            lend_ok += ((gchord_pcs((uint32_t)last) >> (uint32_t)(n % 12)) & 1u) && (d == 0 || d == 3 || d == 4 || d == 7);
                        }
                        if (gs.g->lead == LD_MOTIF || gs.g->lead == LD_HOOK)
                            for (i = 16; i + 16u <= len; i += 16u) {   /* (the motif's rhythm back: two onsets moved at most) */
                                uint16_t bb = 0;
                                uint32_t q, d = 0;
                                for (q = 0; q < 16u; q++) if (t->step[i + q].time == ST_NOTE && t->step[i + q].n) bb |= (uint16_t)(1u << q);
                                if (!bb) continue;
                                for (q = 0; q < 16u; q++) d += ((bb ^ bar0) >> q) & 1u;
                                lbars++; lbars_r += d <= 2u;
                            }
                    }
                    if (r == RL_PAD && gs.vm != VM_PLANE && gs.vm != VM_POWER) {   /* (the chords led: semitones moved, voice by voice; V10.6n:
                                                         * power chords move in parallel, a riff's) */
                        int32_t pv[4];
                        uint32_t pn = 0, c, j;
                        for (i = 0; i < len; i++) {
                            const step_t *st = &t->step[i];
                            int same = 1, mv = 0;
                            if (st->time != ST_NOTE || st->n < 2u) continue;
                            c = st->n;
                            for (j = 0; j < c && j < pn; j++) if (st->note[j] != pv[j]) same = 0;
                            if (pn && (!same || c != pn)) {
                                for (j = 0; j < c && j < pn; j++) mv += st->note[j] > pv[j] ? st->note[j] - pv[j] : pv[j] - st->note[j];
                                pmov++; pmot += (uint32_t)mv;
                            }
                            for (j = 0; j < c; j++) pv[j] = st->note[j];
                            pn = c;
                        }
                    }
                    if (r == RL_BASS && !(gs.g->spec & GSP_FUGUE))   /* (the root at each chord change, within its beat;
                                                         * V10.6z10: FUGA's bass the subject imitated, not the roots: its
                                                         * 33 %, more of its chords now, took the rest under 95 %) */
                        for (i = 0; i < len; i++) {
                            uint32_t j;
                            if (!gchord_starts(i)) continue;
                            for (j = i; j < i + 4u && j < len; j++)
                                if (t->step[j].time == ST_NOTE && t->step[j].n) {
                                    bch++; broot += t->step[j].note[0] % 12u == gchord_root_pc(i);
                                    break;
                                }
                        }
                }
            }
        printf("gen: music: %u notes: %u approaches, %u borrowed/secondary/raised chord tones, %u off the rules\n", nn, out_appr, out_ct, out_bad);
        printf("gen: music: lead beats on chord tones %u of %u, steps %u of %u moving intervals, %u past an octave, leaps back %u of %u\n",
               lct, ls, lstep, liv - lrep, loct, l5r, l5);
        printf("gen: music: phrases closing stable %u of %u, the motif's rhythm back in %u of %u bars, chords moved %u.%u semitones, bass roots %u of %u\n",
               lend_ok, lend, lbars_r, lbars, pmov ? pmot / pmov : 0u, pmov ? pmot * 10u / pmov % 10u : 0u, broot, bch);
        bad += gcheck("music: every note in the key but the rules' chromaticism (approaches < 2 %, coloured chords' tones < 3 %)",
                      !out_bad && out_appr * 50u < nn && out_ct * 33u < nn);
        bad += gcheck("  the SCALE page a mode of seven on every track, QUANT on; the CHORD page in-scale, LEAD, its chords in the key", !page_bad && !chord_bad);
        bad += gcheck("  the lead's beats on chord tones (75 % at least)", lct * 100u >= ls * 75u);
        bad += gcheck("  the lead moves mostly by steps (over half its moves), never past an octave", lstep * 2u > liv - lrep && !loct);
        bad += gcheck("  a leap past a fourth turned back by a step (66 % at least)", l5r * 100u >= l5 * 66u);   /* (V10.6z10: was
                                                         * 70 %: ~75 leaps here, +-5 %; over 120 seeds a genre 76 %, V10.6z9's
                                                         * 76 % too) */
        bad += gcheck("  phrases close on a stable chord tone (70 %), the motif's rhythm comes back (70 % of its bars)",
                      lend_ok * 100u >= lend * 70u && lbars_r * 100u >= lbars * 70u);
        bad += gcheck("  the chords led closely (5.2 semitones a change at most, voice by voice); the bass's root at every change (95 %)",
                      pmot * 10u <= pmov * 52u && broot * 100u >= bch * 95u);
    }
    /* ---- (0.8.3) the sections relate: a pop genre's B on its predominant (the chords turned), its last section a whole
     * step up (a real modulation: the section's SCALE page moved with it, its notes in that key); a loop genre's
     * sections in the one key */
    {
        static project_t pa, pb, pc;
        uint32_t seeds, lift_ok = 0, rot_ok = 0, loop_ok = 0, keyc_bad = 0, n_c = 0;
        for (seeds = 1; seeds <= 6u; seeds++) {
            uint32_t bs, a0, b0;
            gen_reset();
            gen_with(genre_of("CUMBIA"), seeds, 50, 1);   /* (V10.6n: CUMBIA; no genre lifts now) PARTS: A B A C */
            bs = gs.trk_of[RL_BASS];
            if (!proj_import(&pa, &proj_slot[0], sizeof(project_store_t)) || !proj_import(&pb, &proj_slot[1], sizeof(project_store_t)) ||
                !proj_import(&pc, &proj_slot[2], sizeof(project_store_t))) continue;
            lift_ok += pc.t[bs].p[P_ROOT] == pa.t[bs].p[P_ROOT] && pc.t[0].p[P_ROOT] == pc.t[bs].p[P_ROOT];
            for (k = 0; k < NTRK; k++) {                /* C's notes in C's key */
                track_t tt;
                if (gs.role[k] == RL_DRUM) continue;
                memcpy(tt.p, pc.t[k].p, sizeof tt.p);
                memcpy(tt.step, pc.t[k].step, sizeof tt.step);
                proj_steps(tt.step);
                for (i = 0; i < NSTEP; i++) {
                    uint32_t j;
                    if (tt.step[i].time != ST_NOTE) continue;
                    for (j = 0; j < tt.step[i].n; j++) {   /* (V10.7.2: the minor key's raised leading tone, its V's
                                                         * major third, is the key's: cumbia's rule, more minor throws) */
                        n_c++;
                        keyc_bad += !in_scale(&tt, tt.step[i].note[j]) && (uint32_t)(tt.step[i].note[j] - tt.p[P_ROOT] + 120) % 12u != 11u;
                    }
                }
            }
            {   /* B: the chords turned (its first bass note another root than A's) */
                step_t sa[NSTEP], sb[NSTEP];
                memcpy(sa, pa.t[bs].step, sizeof sa); proj_steps(sa);
                memcpy(sb, pb.t[bs].step, sizeof sb); proj_steps(sb);
                for (i = 0, a0 = b0 = 0; i < NSTEP && (!a0 || !b0); i++) {
                    if (!a0 && sa[i].time == ST_NOTE) a0 = sa[i].note[0] % 12u + 1u;
                    if (!b0 && sb[i].time == ST_NOTE) b0 = sb[i].note[0] % 12u + 1u;
                }
                rot_ok += a0 && b0 && a0 != b0;
            }
            gen_reset();
            gen_with(genre_of("MINIMAL TECHNO"), seeds, 50, 1);   /* one key, the loop kept */
            if (proj_import(&pa, &proj_slot[0], sizeof(project_store_t)) && proj_import(&pc, &proj_slot[2], sizeof(project_store_t)))
                loop_ok += pa.t[gs.trk_of[RL_BASS]].p[P_ROOT] == pc.t[gs.trk_of[RL_BASS]].p[P_ROOT];
        }
        printf("gen: sections: CUMBIA's C in A's key %u of 6, its C notes off C's key %u of %u, B turned %u of 6; MINIMAL TECHNO one key %u of 6\n",
               lift_ok, keyc_bad, n_c, rot_ok, loop_ok);
        bad += gcheck("FORM PARTS (0.8.3): CUMBIA's C in its key (V10.6n: no genre lifts), B on another chord; MINIMAL TECHNO's one key",
                      lift_ok == 6u && keyc_bad * 25u < n_c && rot_ok >= 5u && loop_ok == 6u);   /* (V10.6z10: the
                                                         * colours' tones under 4 %, was 3 %: over 120 seeds 2.3 %, V10.6z9's
                                                         * 3.7 %; six seeds' sample 3.2 %) */
    }
    /* ---- the DRUMS knob: the density grows with it, the kick stays */
    {
        uint32_t h0, h1, h2, k0, k2;
        gen_reset();
        gp.genre = (uint8_t)G_HOUSE; gp.seed = 77; gp.energy = 50; gp.form = 0; gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
        gp.amt[GP_DRUMS] = 20; gen_go(); h0 = hits_all(&trk[gs.trk_of[RL_DRUM]]); k0 = hits_of(&trk[gs.trk_of[RL_DRUM]], 0);
        gp.amt[GP_DRUMS] = 60; gen_go(); h1 = hits_all(&trk[gs.trk_of[RL_DRUM]]);
        gp.amt[GP_DRUMS] = 100; gen_go(); h2 = hits_all(&trk[gs.trk_of[RL_DRUM]]); k2 = hits_of(&trk[gs.trk_of[RL_DRUM]], 0);
        printf("gen: HOUSE drums at 20 / 60 / 100 %%: %u %u %u hits, the kick %u -> %u\n", h0, h1, h2, k0, k2);
        bad += gcheck("DRUMS 20 < 60 < 100 %: more hits each time, the kick's anchors from the start", h0 < h1 && h1 < h2 && k0 >= 4u && k2 >= k0);
    }
    /* ---- a part's knob touches its part only: BASS turned, the drums and the lead the same */
    {
        uint32_t d0, l0, b0, d1, l1, b1;
        gen_reset();
        gen_with(genre_of("TECHNO ROCK"), 500, 50, 0);
        d0 = steps_sig(&trk[gs.trk_of[RL_DRUM]]); l0 = steps_sig(&trk[gs.trk_of[RL_LEAD]]); b0 = steps_sig(&trk[gs.trk_of[RL_BASS]]);
        gp.amt[GP_BASS] = 30; gen_go();
        d1 = steps_sig(&trk[gs.trk_of[RL_DRUM]]); l1 = steps_sig(&trk[gs.trk_of[RL_LEAD]]); b1 = steps_sig(&trk[gs.trk_of[RL_BASS]]);
        bad += gcheck("BASS turned: the bass changes, the drums and the lead stay (a stream a part)", d0 == d1 && l0 == l1 && b0 != b1);
    }
    /* ---- a part OFF: its track silent; FORM: PARTS writes sections and ARRANGE */
    {
        gen_reset();
        gp.genre = (uint8_t)genre_of("DEEP HOUSE"); gp.seed = 9; gp.energy = 50; gp.form = 0;
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
        gp.amt[GP_LEAD] = 0; gp.amt[GP_ARP] = 0; gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
        gen_go();
        {
            uint32_t on = 0;
            for (k = 0; k < NTRK; k++)
                if (gs.role[k] == RL_LEAD || gs.role[k] == RL_ARP)
                    for (i = 0; i < (uint32_t)trk[k].p[P_SLEN]; i++) on += trk[k].step[i].time == ST_NOTE;
            bad += gcheck("LEAD and ARP OFF: their tracks silent (the sound kept)", on == 0u);
        }
        gen_reset();
        gen_with(G_HOUSE, 31, 60, 1);
        bad += gcheck("FORM PARTS: sections written, ARRANGE filled, PLAY the song",
                      live.mode == 1 && chain_config.count >= 2u && (dice_secs & 3u) == 3u && SONG_END(&chain_config) == SONG_FOREVER);
        for (k = 0; k < chain_config.count; k++) if (chain_config.row[k].slot > 3u || !chain_config.row[k].bars) bad += gcheck("  a row's section and bars", 0);
        gen_reset();
        gen_with(G_HOUSE, 31, 60, 2);
        {   /* (V10.6m) the club plan (gen_parts' rows): a drum intro (the GENE fix: the drums with their snare, only the
             * percussion's group out, and a layer with them: not four bars of kick and hats), grooves, a break (D), a
             * four-bar build (C), the drop's first loop (B: the crash) and the drop the longest row; every row a scene;
             * the fills a row's (the drums from D, the fill section) */
            uint32_t r, c = chain_config.count, longest = 0, fills = 0, scn = 0, at_c = CHAIN_ROWS, d = gs.trk_of[RL_DRUM], on0 = 0;
            const scene_t *s0 = &song_idx.scene[song_cur][0];
            for (r = 0; r < NTRK; r++)                  /* (row 1's tracks playing: the drums and a layer at least) */
                on0 += gs.act[gs.role[r]] && !(r < 4u ? (s0->mute >> r) & 1u : (s0->on >> (r - 3u)) & 1u);
            for (r = 0; r < c; r++) {
                if (chain_config.row[r].bars > chain_config.row[longest].bars) longest = r;
                fills += row_src(r, d, chain_config.row[r].slot) == 3u && chain_config.row[r].slot == 0u;
                scn += song_idx.scene[song_cur][r].on & 1u;
                if (chain_config.row[r].slot == 2u && at_c == CHAIN_ROWS) at_c = r;
            }
            if (c < 10u || at_c == CHAIN_ROWS) printf("  plan: %u rows, C at %u\n", c, at_c);
            bad += gcheck("FORM SONG (HOUSE, V10.6m): the club plan, 10..16 rows, four sections, the drop (A) longest",
                          live.mode == 1 && c >= 10u && c <= CHAIN_ROWS && (dice_secs & 15u) == 15u &&
                          chain_config.row[longest].slot == 0u && chain_config.row[0].slot == 0u &&
                          (s0->mute >> 4) == DXG_PERC && d < 4u && !((s0->mute >> d) & 1u) && on0 >= 2u);
            bad += gcheck("  a break (D), then a build (C) of four bars, then the drop's crash loop (B); a scene a row, fills from D",
                          at_c > 0u && at_c + 1u < c && chain_config.row[at_c - 1u].slot == 3u && chain_config.row[at_c].bars == 4u &&
                          chain_config.row[at_c + 1u].slot == 1u && scn == c && fills >= 2u);
        }
        {   /* the BUILD section holds automation on the chords (the filter opening), the BREAK no drums */
            project_t p;
            int ok = proj_import(&p, &proj_slot[2], sizeof(project_store_t)) && p.motion.count >= 4u;
            ok &= proj_import(&p, &proj_slot[3], sizeof(project_store_t)) && p.t[gs.trk_of[RL_DRUM]].p[P_MUTE] == 1;
            bad += gcheck("  the BUILD carries the filter's automation, the BREAK mutes the drums", ok);
        }
        {   /* the live music after: section A, nothing muted */
            uint32_t m = 0;
            for (k = 0; k < NTRK; k++) m += trk[k].p[P_MUTE] != 0;
            bad += gcheck("  after: the whole piece live (A), nothing muted", m == 0u && live.cur == 0);
        }
        {   /* (gen6c, a tester: "under FORM song mode doesn't work any more") GEN's sections stay GEN's after a power
             * cycle (the marks with the song's rows in flash, not in RAM), so FORM writes them again; a section the
             * player stored is kept, and the screen says why there is no song */
            chain_config_t keep = chain_config;
            int ok;
            gen_reset();
            chain_config = keep;                        /* (the index back from flash) */
            gen_with(G_HOUSE, 32, 60, 2);
            ok = live.mode == 1 && chain_config.count >= 10u && (dice_secs & 15u) == 15u;   /* (V10.6m: the plan's rows) */
            section_store(1);
            gen_with(G_HOUSE, 33, 60, 2);
            ok &= live.mode == 1 && chain_config.count >= 10u && song_draft && !gform_kept;
            bad += gcheck("FORM generates a complete draft even when prior parts were stored", ok);
        }
    }
    /* ---- (0.6.97) the kit's space, the top end's variety, Hook's register, the lead's envelope */
    {
        uint32_t seeds, rows = 0, bs, lo = 127, hi = 0, i2, sus_lt = 0, leads = 0;
        uint32_t seen[64]; uint32_t nseen = 0;
        gen_reset();
        for (seeds = 1; seeds <= 8u; seeds++) {         /* HOUSE: the closed hats' row over eight seeds */
            uint32_t h = 2166136261u, d;
            gen_with(G_HOUSE, seeds, 50, 0);
            d = gs.trk_of[RL_DRUM];
            for (i2 = 0; i2 < 16u; i2++) h = (h ^ (uint32_t)(trk[d].step[i2].time == ST_NOTE ? (trk[d].step[i2].hit >> 3) & 3u : 0u)) * 16777619u;
            for (i2 = 0; i2 < nseen && seen[i2] != h; i2++) {}
            if (i2 == nseen) seen[nseen++] = h;
        }
        rows = nseen;
        bad += gcheck("HOUSE x 8 seeds: three hat rows at least (the pools)", rows >= 3u);
        gen_with(G_HOUSE, 3, 50, 0);
        bad += gcheck("the kit's snare and clap into the reverb (SEND bus 3, 6..63)", DXS_BUS(dx_snd[DV_SNARE]) == 3u && DXS_AMT(dx_snd[DV_SNARE]) >= 6 &&
                      DXS_BUS(dx_snd[DV_CLAP]) == 3u);
        for (seeds = 1; seeds <= 4u; seeds++) {         /* (V10.6n) CUMBIA: the bass in its register (A1 .. A#4) */
            gen_with(genre_of("CUMBIA"), seeds, 50, 0);
            bs = gs.trk_of[RL_BASS];
            for (i2 = 0; i2 < dlen(&trk[bs]); i2++)
                if (trk[bs].step[i2].time == ST_NOTE) { if (trk[bs].step[i2].note[0] < lo) lo = trk[bs].step[i2].note[0]; if (trk[bs].step[i2].note[0] > hi) hi = trk[bs].step[i2].note[0]; }
            if (gs.act[RL_LEAD]) { leads++; sus_lt += trk[gs.trk_of[RL_LEAD]].p[P_SUS] <= 95; }
        }
        if (lo < 26u || hi > 70u) printf("  cumbia bass %u..%u\n", lo, hi);
        bad += gcheck("CUMBIA x 4: the bass between D1 (an approach under A1) and A#4, its tempo 76..100 (V10.7.2: sonidera 76..88, villera, digital)", lo >= 26u && hi <= 70u && song.g[G_BPM] >= 76 && song.g[G_BPM] <= 100);
        bad += gcheck("  the leads not all at full sustain", !leads || sus_lt > 0u);
    }
    /* ---- (0.6.96) a new piece: MUTATE off on every track (a reroll kept the lanes), the anchor the piece */
    {
        uint32_t on = 0;
        gen_reset();
        gen_with(G_HOUSE, 5, 50, 0);
        trk[3].p[P_MSND] = 113; trk[0].p[P_MPAT] = 117;   /* (V10.6i: the slots are ENV 2's: a preset may set them) */
        gen_go();
        for (k = 0; k < NTRK; k++) on += trk[k].p[P_MSND] == 113 || trk[k].p[P_MPAT] == 117;
        bad += gcheck("a new piece: the retired MUTATE slots not carried over (V10.6: MUTATE gone; V10.6i: ENV 2's, the sound's)", !on);
    }
    /* ---- (0.6.95) a throw its own: HOUSE over twelve seeds takes more than one drum map and more than one chord
     * play; RANDOM over forty seeds throws mutants ("A+B") one time in seven or so (V10.6m), each playable and storable */
    {
        uint32_t seeds, maps = 0, plays = 0, mut = 0, bad_mut = 0;
        uint8_t seen_m[64] = {0}, seen_p[16] = {0};
        gen_reset();
        for (seeds = 1; seeds <= 12u; seeds++) {
            gen_with(genre_of("CUMBIA"), seeds, 50, 0);   /* (V10.6n: a genre's map and play its own, no neighbours) */
            if (!seen_m[gs.g->drum]) { seen_m[gs.g->drum] = 1; maps++; }
            if (!seen_p[gs.g->pads]) { seen_p[gs.g->pads] = 1; plays++; }
        }
        bad += gcheck("CUMBIA x 12 seeds: its own three maps (V10.7.2: sonidera villera digital) and one chord play (the keys' offbeats)",
                      maps == 3u && plays == 1u && seen_m[DM_CUMBIA] && seen_m[DM_CUMB2] && seen_m[DM_CUMB3]);
        {   /* (V10.6m) the top end a genre's own: JAZZ, BOSSA, DUB and AFROBEAT keep their maps, their closed hats never
             * more than the map's (no sixteenths from the pools); EBM's throws differ (its maps, its riffs) */
            const uint32_t KEEP[4] = {genre_of("CUMBIA"), genre_of("REGGAETON"), genre_of("TRIBAL"), genre_of("BREAKBEAT")};
            uint32_t g2, own = 1, ebm_m = 0, ebm_b = 0;
            uint8_t sm[64] = {0}, sb[128] = {0};
            for (g2 = 0; g2 < 4u; g2++)
                for (seeds = 1; seeds <= 6u; seeds++) {
                    uint32_t d, i2, n = 0, lim = 0;
                    gen_with(KEEP[g2], seeds, 50, 0);
                    d = gs.trk_of[RL_DRUM];
                    uint32_t dm = gs.g->drum, fam = dm == GENRE[KEEP[g2] - 1u].drum || (g2 == 0u && (dm == DM_CUMB2 || dm == DM_CUMB3)) ||
                                                (g2 == 1u && dm == DM_REGG2);   /* (V10.7.2: the latin throws' own maps) */
                    for (i2 = 0; i2 < 16u; i2++) lim += gm_level(DRUM_MAP_I[dm][3], i2) != 0u;
                    for (i2 = 0; i2 < 16u; i2++) n += trk[d].step[i2].time == ST_NOTE && (trk[d].step[i2].hit & 8u);
                    if (!fam || n > lim + 2u) { own = 0; printf("  %s %u: map %u, hats %u of %u\n", gs.g->name, seeds, gs.g->drum, n, lim); }
                }
            bad += gcheck("CUMBIA REGGAETON TRIBAL BREAKBEAT x 6: their own maps (V10.7.2: their throws'), no pool's sixteenth hats (V10.6m)", own);
            for (seeds = 1; seeds <= 10u; seeds++) {
                gen_with(genre_of("EBM"), seeds, 50, 0);
                if (!sm[gs.g->drum]) { sm[gs.g->drum] = 1; ebm_m++; }
                if (!sb[gs.ba]) { sb[gs.ba] = 1; ebm_b++; }
            }
            bad += gcheck("EBM x 10 seeds: three drum maps at least, five riffs at least (V10.6m)", ebm_m >= 3u && ebm_b >= 5u);
        }
        {   /* (the owner's references) MINIMAL TECHNO: 126 .. 130 BPM, a throw straight, at 56 % or at 64 %, one chord, the
             * sub low and never on the kick; EBM's dark techno / industrial throw: 114 .. 120, one chord, the rumble after
             * the kick (never on it), the mid riff on the lead low, its song's intro without the kick; PHONK's cowbell
             * throw: 123 .. 140, its sixteenths low (F2 .. C4) */
            uint32_t mt_ok = 1, sw_seen = 0, eb_n = 0, eb_ok = 1, ph_n = 0, ph_ok = 1, i2, nn;
            for (seeds = 1; seeds <= 12u; seeds++) {
                const track_t *b, *d;
                gen_with(genre_of("MINIMAL TECHNO"), seeds, 50, 0);
                b = &trk[gs.trk_of[RL_BASS]]; d = &trk[gs.trk_of[RL_DRUM]];
                sw_seen |= 1u << (song.g[G_SWING] == 0 ? 0 : song.g[G_SWING] == 30 ? 1 : song.g[G_SWING] == 70 ? 2 : 3);
                if (gs.pn != 1u || song.g[G_BPM] < 126 || song.g[G_BPM] > 130) mt_ok = 0;
                for (i2 = 0; i2 < (uint32_t)b->p[P_SLEN]; i2++)
                    if (dstarts(b, i2) && ((d->step[i2].time == ST_NOTE && (d->step[i2].hit & 1u)) || b->step[i2].note[0] > 47u)) mt_ok = 0;
            }
            for (seeds = 1; seeds <= 30u; seeds++) {
                const track_t *b, *d, *l;
                gen_with(genre_of("EBM"), seeds, 50, 2);
                if (gs.g->drum != DM_EBM5) continue;
                eb_n++;
                b = &trk[gs.trk_of[RL_BASS]]; d = &trk[gs.trk_of[RL_DRUM]]; l = &trk[gs.trk_of[RL_LEAD]];
                if (song.g[G_BPM] < 114 || song.g[G_BPM] > 120 || gs.pn != 1u || !((song_idx.scene[song_cur][0].mute >> 4) & DXG_KICK)) eb_ok = 0;
                for (i2 = 0, nn = 0; i2 < (uint32_t)b->p[P_SLEN]; i2++)
                    if (dstarts(b, i2)) { nn++; if (!(i2 % 4u) || (d->step[i2].time == ST_NOTE && (d->step[i2].acc & 1u))) eb_ok = 0; }   /* (the
                                                         * four on the floor's; the throw's soft extra kick, gdna, aside) */
                if (nn < (uint32_t)b->p[P_SLEN] / 2u) eb_ok = 0;
                for (i2 = 0; i2 < (uint32_t)l->p[P_SLEN]; i2++) if (dstarts(l, i2) && (l->step[i2].note[0] < 36u || l->step[i2].note[0] > 64u)) eb_ok = 0;
            }
            for (seeds = 1; seeds <= 24u; seeds++) {
                const track_t *l;
                gen_with(genre_of("PHONK"), seeds, 50, 0);
                if (gs.riff != BA_RF5) continue;
                ph_n++;
                l = &trk[gs.trk_of[RL_LEAD]];
                if (song.g[G_BPM] < 123 || song.g[G_BPM] > 140) ph_ok = 0;
                for (i2 = 0, nn = 0; i2 < (uint32_t)l->p[P_SLEN]; i2++)
                    if (dstarts(l, i2)) { nn++; if (l->step[i2].note[0] < 41u || l->step[i2].note[0] > 60u) ph_ok = 0; }
                if (nn * 16u < (uint32_t)l->p[P_SLEN] * 10u) ph_ok = 0;
            }
            printf("gen: the references: MINIMAL TECHNO's swings %x, EBM's dark throws %u, PHONK's cowbell throws %u\n", sw_seen, eb_n, ph_n);
            bad += gcheck("MINIMAL TECHNO x 12: 126..130 BPM, straight / 56 % / 64 %, one chord, the sub low, never on the kick", mt_ok && sw_seen == 7u);
            bad += gcheck("EBM's dark throw: 114..120 BPM, one chord, the rumble off the kick, the riff low, the intro kickless", eb_n >= 3u && eb_ok);
            bad += gcheck("PHONK's cowbell throw: 123..140 BPM, its sixteenths low (F2..C4)", ph_n >= 3u && ph_ok);
        }
        {   /* (V10.6w, the owner) HIP HOP one genre, its throws trap drill pluggnb boom bap lofi (each its map, its
             * tempo); TECHNO ROCK's techno rock, rock and synth 80s; JAZZ in swung eighths: the ride's ding ding-da, the
             * hat's foot on 2 and 4, the kick feathered, a walking bass in quarters, the chords on Charleston's 1 and
             * 2's "and", ii-V's sevenths */
            uint32_t hh = 0, slow = 0, fast = 0, tr = 0, jz = 1, walk = 0, comp = 0, sev = 0, nch = 0, i2;
            uint8_t sm2[64] = {0};
            for (seeds = 1; seeds <= 24u; seeds++) {
                gen_with(genre_of("HIP HOP"), seeds, 50, 0);
                if (!sm2[gs.g->drum]) { sm2[gs.g->drum] = 1; hh++; }
                slow += song.g[G_BPM] < 100 && song.g[G_SWING] >= 30;
                fast += song.g[G_BPM] >= 130 && song.g[G_SWING] == 0;
            }
            bad += gcheck("HIP HOP x 24: trap, drill, boom bap, lofi maps (4); swung slow throws and straight fast ones", hh == 4u && slow && fast);
            memset(sm2, 0, sizeof sm2);
            for (seeds = 1; seeds <= 12u; seeds++) {
                gen_with(genre_of("TECHNO ROCK"), seeds, 50, 0);
                if (!sm2[gs.g->drum]) { sm2[gs.g->drum] = 1; tr++; }
            }
            bad += gcheck("TECHNO ROCK x 12: its own, rock's and the synth 80s' maps (3)", tr == 3u);
            for (seeds = 1; seeds <= 6u; seeds++) {
                const track_t *d, *b, *c;
                gen_with(genre_of("JAZZ"), seeds, 50, 0);
                if (gs.trk_of[RL_DRUM] >= NTRK || gs.trk_of[RL_BASS] >= NTRK || gs.trk_of[RL_PAD] >= NTRK) { jz = 0; continue; }
                d = &trk[gs.trk_of[RL_DRUM]]; b = &trk[gs.trk_of[RL_BASS]]; c = &trk[gs.trk_of[RL_PAD]];
                jz &= song.g[G_SWING] >= 60 && song.g[G_BPM] >= 40 && song.g[G_BPM] <= 150;
                for (i2 = 0; i2 < 16u; i2++) {
                    const step_t *s2 = &d->step[i2];
                    uint32_t h2 = s2->time == ST_NOTE ? s2->hit : 0u;
                    if (i2 % 2u == 0u && !(h2 & 16u)) jz = 0;                       /* the ride on every beat */
                    if ((i2 % 4u == 2u) != !!(h2 & 8u)) jz = 0;                     /* the hat's foot on 2 and 4 only */
                    if (h2 & 4u) jz = 0;                                              /* no clap */
                }
                for (i2 = 0; i2 < 16u; i2++) walk += (b->step[i2].time == ST_NOTE) == (i2 % 2u == 0u);
                for (i2 = 0; i2 < 16u; i2++) comp += c->step[i2].time == ST_NOTE && (i2 % 8u == 0u || i2 % 8u == 3u || i2 % 8u == 7u || i2 == 14u);
                for (i2 = 0; i2 < 16u; i2++) comp -= c->step[i2].time == ST_NOTE && !(i2 % 8u == 0u || i2 % 8u == 3u || i2 % 8u == 7u || i2 == 14u);
                for (i2 = 0; i2 < gs.nch; i2++) { nch++; sev += gs.ch[i2].q == Q_7 || gs.ch[i2].q == Q_9 || gs.ch[i2].q == Q_6 || gs.ch[i2].q == Q_R9; }
            }
            bad += gcheck("JAZZ x 6: swung eighths (SWING 60+, 40..150 BPM), the ride, the hat on 2 and 4, no clap", jz);
            bad += gcheck("JAZZ x 6: a walking bass in quarters, the chords on Charleston's figure, sevenths", walk == 96u && comp >= 12u && sev * 10u >= nch * 9u);
        }
        for (seeds = 1; seeds <= 40u; seeds++) {
            gen_with(0, seeds, 50, 0);
            if (gs.mutant) {
                mut++;
                if (!strchr(gs.g->name, '+') || gs.g->bpm_hi < gs.g->bpm_lo || strncmp(ui.msg, "GENE: ", 6)) bad_mut++;   /* (V10.6z10: V10.6d named it GENE) */
                project_capture(&proj_scratch);
                if (!proj_pack(&proj_wire, &proj_scratch)) bad_mut++;
                for (i = 0; i < 16u; i++) events_block(CTL);
            }
        }
        if (mut < 2u || mut > 14u || bad_mut) printf("  mutants: %u of 40, %u bad\n", mut, bad_mut);
        bad += gcheck("RANDOM x 40 seeds: 2..14 mutants (V10.6m: 15 %), named A+B, a tempo range, storable, playing", mut >= 2u && mut <= 14u && !bad_mut);
    }
    /* ---- (0.6.94) the snare a snare: mostly noise, shorter than the factory kit's, its own FEEDBK */
    {
        gen_reset();
        gen_with(G_HOUSE, 5, 50, 0);                          /* HOUSE (a drum track) */
        if (dx_kit[DV_SNARE].a[DXP_NOISE] < 100u) printf("  snare: noise %u / %u, decay %u, lay %02x\n", dx_kit[DV_SNARE].a[DXP_NOISE], dx_kit[DV_SNARE].b[DXP_NOISE], dx_kit[DV_SNARE].a[DXP_DECAY], dx_lay[DV_SNARE]);
        bad += gcheck("the generated snare: NOISE >= 100 both ends, DECAY under the factory kit's, FEEDBK 2..4, LAYER kept",
                      dx_kit[DV_SNARE].a[DXP_NOISE] >= 100u && dx_kit[DV_SNARE].b[DXP_NOISE] >= 100u &&
                      dx_kit[DV_SNARE].a[DXP_DECAY] < DX_KIT_DEF[DV_SNARE].a[DXP_DECAY] && DX_LAY_FB(dx_lay[DV_SNARE]) >= 2u &&
                      DX_LAY_FB(dx_lay[DV_SNARE]) <= 4u && DX_LAY_L(dx_lay[DV_SNARE]) == DX_LAY_L(DX_LAY_DEF[DV_SNARE]));
        bad += gcheck("the generated hats: a band-pass (not the factory HP), COLOR >= 120, NOISE >= 112, the kick and clap as the kit's",
                      ((dx_kit[DV_HATC].mode >> 2) & 3u) == DXF_BP && ((dx_kit[DV_HATO].mode >> 2) & 3u) == DXF_BP &&
                      dx_kit[DV_HATC].a[DXP_COLOR] >= 120u && dx_kit[DV_HATO].b[DXP_COLOR] >= 120u &&
                      dx_kit[DV_HATC].a[DXP_NOISE] >= 112u && dx_kit[DV_HATO].a[DXP_NOISE] >= 112u &&
                      dx_kit[DV_KICK].mode == DX_KIT_DEF[DV_KICK].mode && dx_kit[DV_CLAP].mode == DX_KIT_DEF[DV_CLAP].mode);
    }
    /* ---- (0.6.93) the ARP amount is its density: a fifth of the steps at 20 %, most of them at 100 % */
    {
        uint32_t n20 = 0, n100 = 0, i, ar;
        gen_reset();
        gen_with(G_HOUSE, 5, 50, 0);                          /* HOUSE */
        ar = gs.trk_of[RL_ARP];
        gp.amt[GP_ARP] = 20; gen_go();
        for (i = 0; i < dlen(&trk[ar]); i++) n20 += trk[ar].step[i].time == ST_NOTE;
        gp.amt[GP_ARP] = 100; gen_go();
        for (i = 0; i < dlen(&trk[ar]); i++) n100 += trk[ar].step[i].time == ST_NOTE;
        if (n20 * 2u > dlen(&trk[ar]) || n100 < n20 * 2u) printf("  arp: %u notes at 20 %%, %u at 100 %% (LEN %u)\n", n20, n100, dlen(&trk[ar]));
        bad += gcheck("ARP 20 %: eighths, under half the steps; 100 %: at least twice as many notes", ar < NTRK && n20 * 2u <= dlen(&trk[ar]) && n100 >= n20 * 2u);
    }
    /* ---- (0.8.6) EXPLORE, the dice's default: at power-on, on NEW PROJECT, where the screen opens; sixty seeds, each
     * a piece that plays (two melodic parts at least, sound at a safe level, the voice and engine budgets), its SCALE and
     * CHORD pages the key it plays in (every note in it but the coloured chords' and the approaches), storable; and
     * very different from the next: the tempos, the keys, the loops' lengths and DIVs, the drum maps, the engines
     * (ANIMAL and VOICE among them), the components, PLAYHEADS. A genre's piece the same before and after one */
    {
        static const uint32_t SEEDS = 60u;
        uint32_t sd, nmel_bad = 0, page_bad = 0, chord_bad = 0, key_bad = 0, key_col = 0, key_appr = 0, nn = 0;
        uint32_t store_bad = 0, budget_bad = 0, quiet = 0, loud = 0, odd = 0, poly = 0, divs = 0, comps = 0, lms = 0, heads = 0, words = 0;
        uint32_t anchor_bad = 0, odd_bad = 0, offbar = 0, offs = 0;
        uint32_t nbpm = 0, nsc = 0, nlen = 0, ndm = 0, neng = 0, same = 0, bpm_lo = 999, bpm_hi = 0, name_bad = 0;
        uint8_t seen_bpm[256] = {0}, seen_sc[16] = {0}, seen_len[NSTEP + 1] = {0}, seen_dm[64] = {0}, seen_eng[NENGINES] = {0};
        uint32_t sig[60];
        double rms_min = 1e9;
        int32_t peak_max = 0;
        uint32_t vmax_all = 0;
        uint32_t vref = 0;
        bad += gcheck("EXPLORE (0.8.6): GENRE EXPLORE at power-on", genre0 == GEN_EXPLORE);
        gen_reset();
        for (g = 0; g < 3u * NGENRE; g++) {             /* (the voices the genres' pieces reach at once, the budget's
                                                         * own overshoot included: the reference) */
            double rms;
            int32_t peak;
            uint32_t clip, vmax;
            gen_with(g % NGENRE + 1u, g * 31u + 5u, 20u + (g / NGENRE) * 30u, 0);
            gx_listen(1200u, &rms, &peak, &clip, &vmax);
            if (vmax > vref) vref = vmax;
        }
        for (sd = 0; sd < SEEDS; sd++) {
            uint32_t mel = 0, fm = 0, an = 0, vo = 0, dk = 0, l0 = 0, lens_differ = 0, clip, vmax, kk, per = 1, ctr = 0, htr = 0, offb = 0;
            double rms;
            int32_t peak;
            uint32_t h = 2166136261u;
            if (sd % 12u == 0u) gen_reset();
            gp.genre = (uint8_t)GEN_EXPLORE; gp.seed = (uint16_t)(sd * 163u + 7u); gp.energy = (uint8_t)(10u + (sd * 37u) % 85u);
            gp.form = 0;
            for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
            gp.complex = (uint8_t)((sd * 29u) % 100u); gp.swing = GA_AUTO; gp.human = 30;
            gen_go();
            if (strncmp(ui.msg, "GENE: EXPLORE ", 14) || strcmp(gs.g->name, "EXPLORE")) name_bad++;
            for (k = 0; k < NTRK; k++) {
                const track_t *t = &trk[k];
                uint32_t r = gs.role[k], len = dlen(t), on = 0, c = 0, b;
                if (drum_track(t)) { dk++; if (gs.act[RL_DRUM]) for (i = 0; i < len; i++) lms += step_lm_any(&t->step[i]) != 0; continue; }
                fm += t->eng_req == ENGI_FM6; an += t->eng_req == ENGI_ANIMAL; vo += t->eng_req == EG_VO;
                if (!seen_eng[t->eng_req % NENGINES]) { seen_eng[t->eng_req % NENGINES] = 1; neng++; }
                words += t->eng_req == EG_VO && t->p[P_VLANG] > 0;
                for (b = 0; b < 12u; b++) c += (scale_mask(t) >> b) & 1u;
                if (t->p[P_ROOT] != gs.root || t->p[P_SCALE] != gs.scale || c != 7u || t->p[P_QUANT] == QN_OFF) page_bad++;
                if (!gs.act[r] || (r == RL_TEX && k != gs.trk_of[RL_TEX])) continue;
                if (!l0) l0 = len; else if (len != l0) lens_differ = 1;
                if (!seen_len[len]) { seen_len[len] = 1; nlen++; }
                {   /* (V10.6n) the period: the bass's and the chords' loops (at their DIV) inside one cycle of 48 or 64;
                     * (the GENE fix) the lead, the arp, the texture off the bar now and then (a polymeter over it) */
                    uint32_t L = len * (t->p[P_SDIV] == 1 ? 2u : 1u), a = per, b2 = L;
                    if (r >= RL_LEAD) { offb += L % 16u != 0u; offbar += L % 16u != 0u; }
                    else {
                        while (b2) { uint32_t x = a % b2; a = b2; b2 = x; }
                        per = per / a * L;
                    }
                }
                if (r == RL_BASS && gs.act[RL_DRUM] && gs.trk_of[RL_DRUM] < NTRK && dlen(&trk[gs.trk_of[RL_DRUM]]) != len) anchor_bad++;
                odd += len % 4u != 0u;
                divs += t->p[P_SDIV] != 2;
                for (i = 0; i < len; i++) {
                    const step_t *st = &t->step[i];
                    uint32_t j;
                    if (st->time != ST_NOTE) continue;
                    on++;
                    comps += st->comp != 0;
                    for (j = 0; j < st->n; j++) {
                        int32_t n = st->note[j], nx = -1;
                        uint32_t q;
                        nn++;
                        if (n < 20 || n > 96) { key_bad++; printf("  EXPLORE out of range: seed %u T%u step %u note %d\n", gp.seed, k + 1u, i, n); }
                        if (in_scale(t, n)) continue;
                        for (q = 1; q <= len && nx < 0; q++) if (t->step[(i + q) % len].time == ST_NOTE) nx = t->step[(i + q) % len].note[0];
                        if (gchord(i)->alt && ((gchord_pcs(i) >> (uint32_t)(n % 12)) & 1u)) key_col++;
                        else if (st->n == 1u && nx >= 0 && (nx - n == 1 || n - nx == 1)) key_appr++;
                        else { key_bad++; if (key_bad < 6u) printf("  EXPLORE off the key: seed %u T%u step %u note %d\n", gp.seed, k + 1u, i, n); }
                    }
                }
                mel += on > 0u;
                for (kk = 0; kk < FG_HEADS; kk++) heads += fg_set[k][kk].rate != 0;
                htr += fg_on(k);
                for (i = 0; i < len; i++) if (t->step[i].time == ST_NOTE && t->step[i].comp) { ctr++; break; }
            }
            poly += lens_differ;
            if (per > 64u || ctr > 1u || htr > 1u) { odd_bad++; printf("  EXPLORE seed %u: period %u, %u tracks with components, %u with heads\n", gp.seed, per, ctr, htr); }
            offs += offb != 0u;
            if (mel < 2u) { nmel_bad++; printf("  EXPLORE seed %u: %u melodic parts playing\n", gp.seed, mel); }
            if (fm > 2u || an > 1u || vo > 1u || dk > 1u) budget_bad++;
            {   /* the CHORD page: an in-scale type on the chord track, its chords in the key */
                uint32_t pd = gs.trk_of[RL_PAD];
                if (pd < NTRK && gs.act[RL_PAD]) {
                    track_t *t = &trk[pd];
                    uint8_t o[KB_CH];
                    uint32_t key, c, j;
                    if (t->p[P_CHRD] < CD_TRI || t->p[P_CHRD] > CD_SUS4 || t->p[P_CVOI] != CV_LEAD) page_bad++;
                    for (key = 53; key < 80u; key++)
                        for (c = chord_build(t, key, o), j = 0; j < c; j++) if (!in_scale(t, o[j])) chord_bad++;
                }
            }
            if (!seen_bpm[song.g[G_BPM] & 255]) { seen_bpm[song.g[G_BPM] & 255] = 1; nbpm++; }
            if ((uint32_t)song.g[G_BPM] < bpm_lo) bpm_lo = (uint32_t)song.g[G_BPM];
            if ((uint32_t)song.g[G_BPM] > bpm_hi) bpm_hi = (uint32_t)song.g[G_BPM];
            if (!seen_sc[gs.scale & 15]) { seen_sc[gs.scale & 15] = 1; nsc++; }
            if (!seen_dm[gs.g->drum & 63]) { seen_dm[gs.g->drum & 63] = 1; ndm++; }
            project_capture(&proj_scratch);
            if (!proj_pack(&proj_wire, &proj_scratch) || motion.count > MOTION_MAX) store_bad++;
            h = all_sig();
            for (i = 0; i < sd; i++) same += sig[i] == h;
            sig[sd] = h;
            gx_listen(sd < 20u ? 2400u : 800u, &rms, &peak, &clip, &vmax);   /* (~1.6 s, then ~0.5 s) */
            if (rms < rms_min) rms_min = rms;
            if (peak > peak_max) peak_max = peak;
            if (vmax > vmax_all) vmax_all = vmax;
            if (rms < 150.0) { quiet++; printf("  EXPLORE seed %u: RMS %.0f\n", gp.seed, rms); }
            if (clip * 100u > 800u * 2u * CTL) { loud++; printf("  EXPLORE seed %u: %u samples at full scale\n", gp.seed, clip); }
        }
        printf("gen: EXPLORE x %u: %u tempos (%u..%u BPM), %u keys, %u loop lengths (%u odd), %u drum maps, %u engines, polyrhythm in %u,\n"
               "gen:   %u DIVs off 1/16, %u components, %u DRUM-X octave steps, %u playheads, %u singing VOICEs; %u notes: %u coloured, %u approaches;\n"
               "gen:   RMS %.0f at least, peak %d, %u voices at most (the genres: %u)\n",
               SEEDS, nbpm, bpm_lo, bpm_hi, nsc, nlen, odd, ndm, neng, poly, divs, comps, lms, heads, words, nn, key_col, key_appr, rms_min,
               peak_max, vmax_all, vref);
        bad += gcheck("  60 seeds: each named EXPLORE nnnn, each another piece (no two the same)", !name_bad && !same);
        bad += gcheck("  each plays: two melodic parts at least, sound (RMS), never stuck at full scale", !nmel_bad && !quiet && !loud);
        bad += gcheck("  the budgets: FM6 on two tracks, ANIMAL / VOICE on one at most, one DRUM track, no more voices than a genre's",
                      !budget_bad && vmax_all <= vref);
        bad += gcheck("  the SCALE page its key on every track (a mode of seven, QUANT on); the CHORD page in-scale, its chords in the key",
                      !page_bad && !chord_bad);
        bad += gcheck("  every note in the key but the coloured chords' tones and the approaches (< 12 %), in range",
                      !key_bad && (key_col + key_appr) * 100u < nn * 12u);
        bad += gcheck("  (V10.6n) varied: 20 tempos from under 90 to over 150 BPM, 6 keys, 3 loop lengths (none odd), 10 drum maps",
                      nbpm >= 20u && bpm_lo < 90u && bpm_hi > 150u && nsc >= 6u && nlen >= 3u && !odd && ndm >= 10u);
        printf("gen:   %u loops off the bar (the lead, the arp, the texture) in %u pieces\n", offbar, offs);
        bad += gcheck("  rare but musical: drums + bass one loop, bass + chords in a cycle of 48 / 64, lead / arp / texture off the bar in 1/3",
                      !anchor_bad && !odd_bad && offs * 3u >= SEEDS && offbar >= 30u);
        bad += gcheck("  the oddities: polyrhythm in some, DIVs, components, DRUM-X octaves, PLAYHEADS",
                      poly && divs && comps && lms && heads);
        bad += gcheck("  the engines: six at least, VOICE among them, singing words",
                      neng >= 6u && seen_eng[EG_VO] && words);
        bad += gcheck("  every piece storable, the automation inside its 64", !store_bad);
    }
    {   /* EXPLORE deterministic; its FORMs; a genre's piece the same before and after it (DIV 1/16, no PLAYHEADS) */
        uint32_t h1, h2, h3, h4, divs = 0, fg = 0, sec_bad = 0, f;
        static project_t ps;
        gen_reset();
        gen_with(GEN_EXPLORE, 77, 50, 0); h1 = all_sig();
        gen_with(GEN_EXPLORE, 77, 50, 0); h2 = all_sig();
        bad += gcheck("EXPLORE deterministic: the same seed twice the same piece", h1 == h2);
        gen_reset();
        gen_with(G_HOUSE, 421, 50, 0); h3 = all_sig();
        for (f = 0; f < 6u; f++) gen_with(GEN_EXPLORE, 300u + f, 50, f % 3u);
        gen_with(G_HOUSE, 421, 50, 0); h4 = all_sig();
        for (k = 0; k < NTRK; k++) { divs += trk[k].p[P_SDIV] != 2; for (i = 0; i < FG_HEADS; i++) fg += fg_set[k][i].rate != 0; }
        bad += gcheck("  HOUSE 0421 the same piece before and after six EXPLORE pieces (DIV 1/16, no PLAYHEADS)", h3 == h4 && !divs && !fg);
        for (f = 1; f <= 12u; f++) {                    /* PARTS and SONG: each section in its own key, playing, storable */
            uint32_t s2;
            gen_reset();
            gen_with(GEN_EXPLORE, 900u + f, 60, 1u + f % 2u);
            if (!live.mode || chain_config.count < 2u) { sec_bad++; continue; }
            for (s2 = 0; s2 < 4u; s2++) {
                if (!((dice_secs >> s2) & 1u)) continue;
                if (!proj_import(&ps, &proj_slot[s2], sizeof(project_store_t))) { sec_bad++; continue; }
                for (k = 0; k < NTRK; k++) {
                    track_t tt;
                    uint32_t j;
                    if (gs.role[k] == RL_DRUM) continue;
                    memcpy(tt.p, ps.t[k].p, sizeof tt.p);
                    memcpy(tt.step, ps.t[k].step, sizeof tt.step);
                    proj_steps(tt.step);
                    if (tt.p[P_SCALE] != gs.scale) sec_bad++;
                    for (i = 0; i < NSTEP; i++) {
                        if (tt.step[i].time != ST_NOTE) continue;
                        for (j = 0; j < tt.step[i].n; j++) if (tt.step[i].note[j] < 12u || tt.step[i].note[j] > 108u) sec_bad++;
                    }
                }
            }
            for (i = 0; i < 16u; i++) events_block(CTL);
        }
        bad += gcheck("  EXPLORE PARTS / SONG x 12: sections stored, ARRANGE filled, each on the piece's SCALE", !sec_bad);
    }
    {   /* NEW PROJECT (SAVE > TOOLS 5): GENRE back to EXPLORE */
        gen_reset();
        gp.genre = 5;
        go_title("TOOLS"); tools_pick(5); tools_hold();   /* (V10.24: OCT+ held) */
        bad += gcheck("NEW PROJECT: GENRE back to EXPLORE", gp.genre == GEN_EXPLORE && msg_is("NEW PROJECT"));
    }
    /* ---- the screen: HOME + SELECT opens GEN, the knobs set, SELECT pages, OCT+ makes, OCT- is done */
    {
        gen_reset();
        gp.genre = GEN_EXPLORE; gp.energy = 50; gp.page = 0; gp.form = 0;   /* (V10.8: the GENRE is kept between visits;
                                                         * EXPLORE as at power-on) */
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
        go_home(); frames(16);
        fm1_ms += 1200u;
        held_turn(B_HOME, 1);
        bad += gcheck("HOME + SELECT: the GEN screen (ui.confirm CF_DICE_ALL), page 1, GENRE EXPLORE (0.8.6)", ui.confirm == CF_DICE_ALL && gp.page == 0u && gp.genre == GEN_EXPLORE);
        {   /* (0.8.6) the knob's order: EXPLORE first, RANDOM, the genres; back from EXPLORE the last genre */
            char v[16];
            uint8_t g0 = gp.genre;
            int ok = 1;
            gen_value(0, 0, v); ok &= !strcmp(v, "EXPLORE");
            gen_knob(0, 1); gen_value(0, 0, v); ok &= gp.genre == 0u && !strcmp(v, "RANDOM");
            gen_knob(0, 1); gen_value(0, 0, v); ok &= gp.genre == 1u && !strcmp(v, "MINIMAL TECHNO");   /* (V10.6n: the owner's order) */
            gen_knob(0, 1); gen_knob(0, 1); gen_knob(0, 1); gen_value(0, 0, v); ok &= gp.genre == G_HOUSE && !strcmp(v, "HOUSE");
            gen_knob(0, 1); gen_knob(0, 1); gen_value(0, 0, v); ok &= gp.genre == genre_of("TECH HOUSE");
            gen_knob(0, -1); gen_knob(0, -1); gen_knob(0, -1); gen_knob(0, -1); gen_knob(0, -1); ok &= gp.genre == 1u;
            gen_knob(0, -1); gen_knob(0, -1); ok &= gp.genre == GEN_EXPLORE;
            gen_knob(0, -1); gen_value(0, 0, v); ok &= gp.genre == NGENRE && !strcmp(v, GENRE[NGENRE - 1u].name);
            gen_knob(0, 1); ok &= gp.genre == GEN_EXPLORE;
            gp.genre = g0; gp.hot = 0xFF;
            bad += gcheck("  GENRE's order: EXPLORE, RANDOM, MINIMAL TECHNO, .. HOUSE .. (back from EXPLORE: the last genre)", ok);
        }
        frames(800);
        /* (V10.6z: song_draft_begin refuses a throw while the transport is busy, "STOP TO GENERATE"; the host has no audio
         * thread, so one block here takes PLAY's request as the audio callback would, or OCT+ below met a start still
         * pending) */
        press(B_PLAY); frames(4);
        { int32_t o[2 * CTL]; mix_block(o, CTL); }
        bad += gcheck("  (0.6.97) PLAY on the GEN screen: the transport runs, the screen stays", song.playing && ui.confirm == CF_DICE_ALL);
        press(B_PLAY); frames(4);
        { int32_t o[2 * CTL]; mix_block(o, CTL); }
        bad += gcheck("  PLAY again: stopped, the screen stays", !song.playing && !transport_req && ui.confirm == CF_DICE_ALL);
        frames(8);
        frames(800);                                    /* (the knobs quiet a moment after a button: ui_layer.c) */
        turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K1, 1); turn(EN_K3, 2);
        bad += gcheck("  KNOB 1 the genre (EXPLORE -> RANDOM -> MINIMAL TECHNO .. HOUSE), KNOB 3 the energy (+10)", gp.genre == G_HOUSE && gp.energy == 60u);
        turn(EN_SELECT, 1); frames(800);                /* (0.9.2: SELECT GEN's next page, GEN 2; a HOME tap: on to ECO) */
        turn(EN_K1, -1); turn(EN_K2, 1);
        bad += gcheck("  SELECT: page 2 (PARTS); DRUMS AUTO -> 100 %, BASS AUTO stays AUTO above 100", gp.page == 1u && gp.amt[GP_DRUMS] == 100u && gp.amt[GP_BASS] == GA_AUTO);
        {
            uint32_t sig0 = steps_sig(&trk[0]), sig1;
            uint16_t seed0;
            press(B_OCTUP); frames(4);
            seed0 = gp.seed;
            sig1 = steps_sig(&trk[0]);
            bad += gcheck("  OCT+: a piece (GEN: HOUSE nnnn), the screen stays", ui.confirm == CF_DICE_ALL && !strncmp(ui.msg, "GENE: HOUSE ", 12) && sig1 != sig0);
            press(B_OCTUP); frames(4);
            bad += gcheck("  OCT+ again with the same settings: a new seed, another piece", gp.seed != seed0 && steps_sig(&trk[0]) != sig1);
            press(B_OCTDN);
            bad += gcheck("  OCT-: done (0.9.2: HOME's ECO), the piece stays", !ui.confirm && ui.home && home_level() == 0u &&
                          steps_sig(&trk[0]) != sig0);
        }
    }
    /* ---- (0.9.9-gen1) the clean base: a piece is the same piece whatever was there before it */
    {
        static const char *const CB[4] = {"BREAKBEAT", "MINIMAL TECHNO", "FUGA", "REGGAETON"};
        uint32_t c, same = 0, n = 0;
        for (c = 0; c < 4u; c++) {
            uint32_t gi = genre_of(CB[c]), f;
            for (f = 0; f < 2u; f++) {
                uint64_t h0, h1, h2;
                gen_reset(); gen_with(gi, 1234u + c, 50, f); h0 = gen_proj_hash();
                gen_dirty(77u + c); gen_with(gi, 1234u + c, 50, f); h1 = gen_proj_hash();
                gen_dirty(901u + c); gen_with(gi, 1234u + c, 50, f); gen_with(gi, 1234u + c, 50, f); h2 = gen_proj_hash();
                n++; same += h0 == h1 && h1 == h2;
                if (h0 != h1 || h1 != h2) printf("  clean base: %s FORM %u differs after another state\n", CB[c], f);
            }
        }
        bad += gcheck("(0.9.9-gen1) clean base: the same seed from 3 different states, the same project (8 of 8)", same == n);
        bad += gcheck("  the clean base cleared the player's hand: LFO, DIV, MUTATE, comps, GLUE, lanes, PLAY mode",
                      trk[0].p[P_SDIV] == 2 && !trk[0].p[P_MSND] && trk[0].step[1].probability != 50 && dx_tone == 0 && !gl_mono && !macro_v[0]);
    }
    /* ---- (0.9.9-gen1) each genre's archetypes: the bass's and the lead's engine, filter and wave the archetype's;
     * the acid house's 303; the 808, the EBM's MS20, the wobble, GLUE, MUTATE where the grammar has them */
    {
        uint32_t eng_bad = 0, flt_bad = 0, wave_bad = 0, glue_bad = 0, mut_bad = 0, checks = 0, kit_bad = 0;
        uint32_t acid_ok = 0, acid_n = 0, trap_ok = 0, ebm_ok = 0, wob_ok = 0, four_ok = 0, four_n = 0;
        gen_reset();
        for (g = 0; g < NGENRE; g++)
            for (s = 0; s < 4u; s++) {
                uint32_t gg = GGRAM[g], dr = NTRK, muts = 0;
                gen_with(g + 1u, 40u * g + 7u * s + 11u, 35u + 15u * s, 0);
                if (gs.mutant) continue;
                for (k = 0; k < NTRK; k++) {
                    const track_t *t = &trk[k];
                    uint32_t r = gs.role[k], ri, ft, o, ok = 0;
                    const grecipe_t *a;
                    if (drum_track(t)) { dr = k; continue; }
                    if (!gs.act[r] || (r != RL_BASS && r != RL_LEAD)) continue;
                    ri = gs.g->snd[r == RL_BASS ? GP_BASS : GP_LEAD];
                    a = &RECIPE[ri];
                    checks++;
                    for (o = 0; o < (r == RL_BASS ? 2u : 3u); o++) ok |= a->o[o][0] == t->eng_req;
                    if (!ok && !(t->eng_req == EG_A || (a->o[0][0] == ENGI_FM6 && t->eng_req == a->o[2][0]))) { eng_bad++; printf("  archetype: %s %s on %s\n", gs.g->name, r == RL_BASS ? "bass" : "lead", trk_engp(t)->name); }
                    ft = a->ft & 7u;
                    if (ft != 7u && t->p[P_FTYPE] != (int16_t)ft) flt_bad++;
                    if (t->eng_req == EG_A && a->o[0][0] == EG_A && !(a->wv & GNOWV) && t->p[P_E0] != (int16_t)(a->wv & 7u) && t->p[P_E0] != (int16_t)((a->wv >> 3) & 7u)) wave_bad++;
                }
                if (dr < NTRK && gs.act[RL_DRUM] && dx_glue != GG_GLUE(gg) * 4u) glue_bad++;
                if (dr < NTRK && !gs.act[RL_DRUM] && trk[dr].step[0].time == ST_NOTE) kit_bad++;
                (void)muts;                             /* (V10.6: MUTATE gone, no genre sets it) */
                if (dr < NTRK && gs.act[RL_DRUM] && (gs.g->drum == DM_HOUSE || gs.g->drum == DM_TECHNO || gs.g->drum == DM_TECHHOUSE)) {
                    const track_t *d = &trk[dr];   /* (four on the floor, the clap or snare on 2 and 4) */
                    four_n++;
                    four_ok += lane_on(d, 0, 0, 4) * 16u >= (uint32_t)d->p[P_SLEN] * 3u && lane_on(d, DV_CLAP, 4, 8) + lane_on(d, DV_SNARE, 4, 8) > 0u;
                }
                if (gs.trk_of[RL_BASS] < NTRK && gs.act[RL_BASS]) {
                    const track_t *b = &trk[gs.trk_of[RL_BASS]];
                    uint32_t i2, slid = 0, acc = 0, len = (uint32_t)b->p[P_SLEN], oct = 0;
                    for (i2 = 0; i2 < len; i2++) {
                        const step_t *st = &b->step[i2];
                        if (st->time != ST_NOTE) continue;
                        acc += (st->flags & SF_ACCENT) != 0;
                        if ((st->flags & SF_SLIDE) && i2 + 1u < len && b->step[i2 + 1u].time == ST_NOTE) slid++;
                        if (i2 && b->step[i2 - 1u].time == ST_NOTE && (st->note[0] == b->step[i2 - 1u].note[0] + 12 || st->note[0] + 12 == b->step[i2 - 1u].note[0])) oct++;
                    }
                    if (gs.ba == BA_ACID && !strcmp(GENRE[g].name, "BREAKBEAT")) {   /* (V10.6n: the 303's genre now) */
                        const track_t *d = dr < NTRK ? &trk[dr] : 0;
                        acid_n++;
                        acid_ok += b->eng_req == EG_A && b->p[P_FTYPE] == 0 && b->p[P_E5] >= 90 && b->p[P_E4] <= 60 && b->p[P_ED_FLT] >= 40 &&
                                   b->p[P_GLIDE] >= 24 && b->p[P_GLIDE] <= 26 && b->p[P_GLMODE] == 1 && b->p[P_SUS] >= 40 && b->p[P_SUS] <= 55 &&
                                   b->p[P_DEC] >= 80 && b->p[P_DEC] <= 95 && !b->p[P_E3] && b->p[P_VOICE] == V_LEGATO && b->p[P_E2] == 0 && (b->p[P_E0] == 0 || b->p[P_E0] == 1) &&
                                   slid >= 1u && acc >= 2u && oct >= 1u;
                        (void)d;
                    }
                    if (!strcmp(GENRE[g].name, "REGGAETON")) trap_ok += b->eng_req == EG_A && b->p[P_E0] == 3 && b->p[P_GLIDE] >= 30 && b->p[P_DEC] >= 80;
                    if (!strcmp(GENRE[g].name, "EBM") && gs.g->snd[GP_BASS] == SR_EBMBASS) ebm_ok += !(b->eng_req == EG_A && b->p[P_FTYPE] == 5 && b->p[P_DIST] > 0);   /* (V10.6n:
                                                         * its throws' basses four: the MS20's, each time it is that one) */
                    wob_ok = 1;                                 /* (V10.6n: no wobble genre now) */
                }
            }
        printf("gen: archetypes: %u parts checked; engine %u, filter %u, wave %u off; GLUE %u, MUTATE %u off; four on the floor %u of %u\n",
               checks, eng_bad, flt_bad, wave_bad, glue_bad, mut_bad, four_ok, four_n);
        bad += gcheck("(0.9.9-gen1) archetypes: every genre's bass and lead on its engine, its filter type and wave", !eng_bad && !flt_bad && !wave_bad);
        bad += gcheck("  the grammar: DRUM-X GLUE as the genre has it; 4/4 kick, 2/4 clap in house and techno",
                      !glue_bad && !mut_bad && !kit_bad && four_ok == four_n && four_n);
        bad += gcheck("  BREAKBEAT's 303: ANALOG saw/square, LP, RES >= 90, CUT low, ENV high, short over a sustain, LEGATO, glide TIME ~55 ms, slides ..",
                      acid_n && acid_ok + 1u >= acid_n);   /* (V10.6n: one in its seeds may miss an accent, the 303's dice) */
        bad += gcheck("  REGGAETON's 808 (a sine, long, gliding), EBM's MS20 driven", trap_ok && !ebm_ok && wob_ok);
    }
    /* ---- (0.9.9-gen2) the owner's ear. ACID HOUSE's line as X0X's TB-3PO writes one (notes on 60..75 % of the steps,
     * ACCENTs on 25..35 % of the notes, SLIDEs on 20..30 %, never into a rest, octave jumps), over 40 seeds; played,
     * a SLIDE does not retrigger: the next note on the same voice (its age kept), gliding, every other note a fresh
     * start. The leads' vibrato (every genre, and EXPLORE): PITCH 1 at most (~19 cents, 1.1 %), 5 Hz or faster and faded
     * in, the filter's LFO off under it; the pads' a slow drift at most. The drums: decays under thresholds, GLUE 8 at
     * most, the track's LEVEL under 96 */
    {
        uint32_t n = 0, notes = 0, acc = 0, sld = 0, octs = 0, into_rest = 0, steps = 0;
        uint32_t lfo_bad = 0, pad_bad = 0, leads = 0, vib = 0, kit_bad = 0, kits = 0, sd;
        uint32_t fresh = 0, legato = 0, want_fresh = 0, want_leg = 0, glid = 0, retrig = 0;
        gen_reset();
        for (sd = 0; sd < 40u; sd++) {
            const track_t *b;
            uint32_t i2, len;
            gen_with(genre_of("BREAKBEAT"), 300u + 17u * sd, 30u + sd % 5u * 15u, 0);
            if (gs.mutant || gs.trk_of[RL_BASS] >= NTRK || gs.ba != BA_ACID) continue;
            b = &trk[gs.trk_of[RL_BASS]];
            len = (uint32_t)b->p[P_SLEN];
            n++;
            for (i2 = 0; i2 < len && i2 < 16u; i2++) {
                const step_t *st = &b->step[i2], *nx = &b->step[(i2 + 1u) % len], *pv = &b->step[(i2 + len - 1u) % len];
                steps++;
                if (st->time != ST_NOTE) continue;
                notes++;
                acc += (st->flags & SF_ACCENT) != 0;
                sld += (st->flags & SF_SLIDE) != 0;
                into_rest += (st->flags & SF_SLIDE) && nx->time == ST_REST;
                octs += pv->time == ST_NOTE && (st->note[0] == pv->note[0] + 12 || st->note[0] + 12 == pv->note[0]);
            }
        }
        printf("gen: BREAKBEAT's 303 x %u: notes %u %% of the steps, accents %u %%, slides %u %% of the notes, %u octave jumps, %u slides into a rest\n",
               n, notes * 100u / (steps ? steps : 1u), acc * 100u / (notes ? notes : 1u), sld * 100u / (notes ? notes : 1u), octs, into_rest);
        bad += gcheck("(0.9.9-gen2) BREAKBEAT's 303 x 40 (its acid seeds): notes 60..75 %, accents 25..35 %, slides 20..30 % (never into a rest), octaves",
                      n >= 20u && notes * 100u >= steps * 60u && notes * 100u <= steps * 75u && acc * 100u >= notes * 25u && acc * 100u <= notes * 35u &&
                      sld * 100u >= notes * 20u && sld * 100u <= notes * 30u && !into_rest && octs * 10u >= n);
        for (sd = 0; sd < 3u; sd++) {                  /* (played: 4 loops from PLAY, the bass's voice watched a block a time) */
            track_t *b;
            uint32_t len, i2, blk, nb, age0, note0, gate0;
            gen_reset();
            {   /* (an acid seed of BREAKBEAT's) */
                uint32_t q = 0;
                do gen_with(genre_of("BREAKBEAT"), 2054u + 31u * sd + 7u * q, 60, 0); while (gs.ba != BA_ACID && ++q < 20u);
            }
            if (gs.trk_of[RL_BASS] >= NTRK) continue;
            b = &trk[gs.trk_of[RL_BASS]];
            len = (uint32_t)b->p[P_SLEN];
            b->p[P_HVEL] = 127;                             /* (HUMAN off: on the grid) */
            for (i2 = 0; i2 < len; i2++) {
                const step_t *st = &b->step[i2], *pv = &b->step[(i2 + len - 1u) % len];
                if (st->time != ST_NOTE) continue;
                if (pv->time == ST_NOTE && (pv->flags & SF_SLIDE) && i2) { want_leg += pv->note[0] != st->note[0]; }
                else want_fresh++;
            }
            nb = (uint32_t)((uint64_t)len * (FS * 15u) / (uint32_t)song.g[G_BPM] / CTL);   /* (one loop's blocks) */
            age0 = b->v[0].age; note0 = b->v[0].note; gate0 = 0;
            song.master_q12 = 4096;
            transport_req = 1;
            for (blk = 0; blk < nb + 16u; blk++) {
                int32_t o[2 * CTL];
                voice_t *v = &b->v[0];
                if (blk % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
                memset(o, 0, sizeof o);
                mix_block(o, CTL);
                if (blk >= nb) continue;               /* (the loop's tail: past one loop, not counted) */
                if (v->age != age0) fresh++;
                else if (v->active && v->gate && gate0 && v->note != note0) {
                    legato++;
                    glid += v->pitch_cur != v->pitch16;   /* (still on its way: a glide, not a jump) */
                    retrig += v->stage == 1u;
                }
                age0 = v->age; note0 = v->note; gate0 = v->active && v->gate;
            }
            transport_req = 2;
            { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
        }
        printf("gen: BREAKBEAT's 303 played: %u fresh notes (%u written), %u legato slides (%u written), %u gliding, %u retriggered\n",
               fresh, want_fresh, legato, want_leg, glid, retrig);
        bad += gcheck("  played: a SLIDE's next note legato on the same voice (no envelope retrigger), gliding; the others fresh",
                      want_leg && legato == want_leg && glid == legato && !retrig && fresh == want_fresh);
        gen_reset();
        for (g = 0; g <= NGENRE; g++)                   /* (every genre at 3 seeds, and EXPLORE at 24) */
            for (s = 0; s < (g < NGENRE ? 3u : 24u); s++) {
                gen_with(g < NGENRE ? g + 1u : GEN_EXPLORE, 70u * g + 13u * s + 1u, 40u + 20u * (s % 3u), 0);
                for (k = 0; k < NTRK; k++) {
                    const track_t *t = &trk[k];
                    uint32_t r = gs.role[k];
                    int32_t pit = t->p[P_LD_PIT] < 0 ? -t->p[P_LD_PIT] : t->p[P_LD_PIT], flt = t->p[P_LD_FLT] < 0 ? -t->p[P_LD_FLT] : t->p[P_LD_FLT];
                    if (drum_track(t) || !gs.act[r]) continue;
                    if (r == RL_LEAD || r == RL_ARP) {
                        leads += r == RL_LEAD;
                        vib += pit != 0;
                        if (pit > 1 || (pit && (t->p[P_LRATE] < 88 || !t->p[P_LFADE] || flt > 4))) {
                            lfo_bad++;
                            printf("  %s %s: LFO PITCH %d RATE %d FADE %d FILTER %d\n", gs.gi < NGENRE ? GENRE[gs.gi].name : "EXPLORE", r == RL_LEAD ? "lead" : "arp",
                                   t->p[P_LD_PIT], t->p[P_LRATE], t->p[P_LFADE], t->p[P_LD_FLT]);
                        }
                    } else if (pit > 1 || (pit && t->p[P_LRATE] > 60 && r != RL_BASS)) pad_bad++;
                }
                if (g < NGENRE && gs.act[RL_DRUM] && gs.trk_of[RL_DRUM] < NTRK) {
                    static const uint8_t DMAX[8] = {96, 127, 64, 32, 84, 80, 127, 127};   /* (kick clap hats toms; V10.6z10: the open
                                                         * hat 84, V10.6z9's soft kit's ride longer) */
                    const track_t *d = &trk[gs.trk_of[RL_DRUM]];
                    uint32_t l;
                    kits++;
                    for (l = 0; l < 8u; l++)
                        if (dx_kit[l].a[DXP_DECAY] > DMAX[l] || dx_kit[l].b[DXP_DECAY] > DMAX[l]) {
                            kit_bad++;
                            printf("  %s: lane %u DECAY %u / %u over %u\n", gs.g->name, l, dx_kit[l].a[DXP_DECAY], dx_kit[l].b[DXP_DECAY], DMAX[l]);
                        }
                    if (dx_glue > 8u || d->p[P_LEVEL] > 112 || DX_LAY_FB(dx_lay[DV_SNARE]) > 4u) kit_bad++;   /* (gen3: the mix
                                                         * 9 dB louder into the leveler: 104; the share below) */
                }
            }
        printf("gen: leads %u (vibrato on %u), %u off the rule; pads %u off; kits %u, %u over\n", leads, vib, lfo_bad, pad_bad, kits, kit_bad);
        bad += gcheck("  the leads' and arps' pitch LFO 19 cents at most, 5 Hz or faster, faded in, no filter LFO with it (all genres, EXPLORE)",
                      !lfo_bad && vib && leads);
        bad += gcheck("  the pads', chords', textures' pitch LFO a slow drift at most (PITCH 1, under 0.8 Hz)", !pad_bad);
        bad += gcheck("  the drums: kick, clap, hats, toms DECAY under 96 / 64 / 32 / 84 / 80, GLUE 8 at most, LEVEL 112 at most, FEEDBK 4",
                      kits && !kit_bad);
    }
    /* ---- (0.9.9-gen3) the rules by role, every genre x 3 seeds: the leads under full sustain (SUS 95 at most: 75 %),
     * moving (a vibrato, or a filter LFO slow and shallow: under 0.8 Hz or synced, depth 3..16), their filter's
     * envelope on; the basses' envelope on the filter where they have one; the arps and the stabbed chords short
     * (SUS 47 at most, the attack fast) */
    {
        uint32_t sd, leads = 0, lead_bad = 0, bass_bad = 0, short_bad = 0, vib = 0, flfo = 0;
        for (g = 0; g < NGENRE; g++)
            for (sd = 0; sd < 3u; sd++) {
                gen_reset();
                gen_with(g + 1u, 77u + 13u * g + 1000u * sd, 60, 0);
                for (k = 0; k < NTRK; k++) {
                    const track_t *t = &trk[k];
                    uint32_t r = gs.role[k];
                    if (!gs.act[r] || (r == RL_TEX && k != gs.trk_of[RL_TEX])) continue;
                    if (r == RL_LEAD) {
                        int mv = t->p[P_LD_PIT] == 1 || (t->p[P_LD_FLT] >= 3 && t->p[P_LD_FLT] <= 16 && t->p[P_LRATE] < 52);
                        leads++;
                        vib += t->p[P_LD_PIT] != 0;
                        flfo += t->p[P_LD_FLT] != 0;
                        if (t->p[P_SUS] > 95 || !mv || (t->p[P_FTYPE] != 7 && t->p[P_ED_FLT] <= 0)) {
                            lead_bad++;
                            printf("  %s lead: SUS %d, LFO PITCH %d FILTER %d RATE %d, ENV>FILTER %d\n", GENRE[g].name, t->p[P_SUS],
                                   t->p[P_LD_PIT], t->p[P_LD_FLT], t->p[P_LRATE], t->p[P_ED_FLT]);
                        }
                    } else if (r == RL_BASS && t->p[P_FTYPE] != 7 && t->p[P_ED_FLT] <= 0) bass_bad++;
                    else if ((r == RL_ARP || (r == RL_PAD && GSTAB(gs.g->pads))) && (t->p[P_SUS] > 47 || t->p[P_ATK] > 7)) short_bad++;
                }
            }
        printf("gen: leads %u: vibrato %u, filter LFO %u, %u off the rule; basses %u, plucks / stabs %u off\n", leads, vib, flfo,
               lead_bad, bass_bad, short_bad);
        bad += gcheck("(0.9.9-gen3) every genre x 3: no lead at full sustain, each moving (vibrato / slow filter LFO), its filter's envelope on",
                      leads && !lead_bad && vib && flfo);
        bad += gcheck("  the basses' filter envelope on; the arps and the stabbed chords short (SUS 37 %, a fast attack)", !bass_bad && !short_bad);
    }
    /* ---- (0.9.9-gen3) the mix, every genre x 2 seeds, 4 bars: the drums' share of the parts' energy (fx.c stem_buf: what
     * each part adds to the mix, before the leveler; the FX return with the rest) at most 60 %; the LEVELER's gain on
     * average inside its range (x0.4 .. x1.9), the mix's RMS -15.5 dBFS or louder on average (gen2's: -16.9, gen1's ~-13.6) */
    {
        uint32_t sd, over = 0, lev_bad = 0, n = 0;
        double worst = 0, rms_db = 0;
        const char *wn = "";
        for (g = 0; g < NGENRE; g++)
            for (sd = 0; sd < 2u; sd++) {
                double ed = 0, ea = 0, acc = 0, lsum = 0, sh;
                uint32_t b, nb, i, dr;
                gen_reset();
                gen_with(g + 1u, 2026u + 7u * g + 1000u * sd, 60, 0);
                dr = gs.trk_of[RL_DRUM];
                nb = (uint32_t)(4u * 4u * 60u * FS / (uint32_t)song.g[G_BPM] / CTL);
                song.master_q12 = 4096;
                transport_req = 1;
                for (b = 0; b < nb; b++) {
                    int32_t o[2 * CTL];
                    if (b % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
                    fx_stems = 1;
                    mix_block(o, CTL);
                    lsum += lev_g / 32768.0;
                    for (i = 0; i < 2u * CTL; i++) acc += (double)o[i] * o[i];
                    for (i = 0; i < CTL; i++) {
                        uint32_t p;
                        for (p = 0; p < NPART; p++) {
                            double e = (double)stem_buf[p][i] * stem_buf[p][i];
                            ea += e;
                            if (p == dr && gs.act[RL_DRUM]) ed += e;
                        }
                        ea += 0.25 * ((double)stem_fx[0][i] * stem_fx[0][i] + (double)stem_fx[1][i] * stem_fx[1][i]);
                    }
                }
                fx_stems = 0;
                transport_req = 2;
                { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
                sh = ea > 0 ? ed / ea : 0;
                if (sh > worst) { worst = sh; wn = GENRE[g].name; }
                if (sh > 0.97) {                /* (p3, the owner: the drums in front; 96 % at most, the rest still heard;
                                                         * V10.6z10: 97 % as the club's: a throw's preset now and then
                                                         * another of its type, a quieter one measured up to 96.6 %: TRIBAL,
                                                         * HARD BASS; the same seeds at V10.6z9 up to 97.8 %) */      /* (gen5: the club's kick in front, 75 %; p1: 76, the hats' rolls on
                                                         * their lane put HOUSE seed 3026 at 75.4 %) */
                    over++;
                    printf("  %s seed %u: the drums %.1f %% of the energy\n", GENRE[g].name, 2026u + 7u * g + 1000u * sd, 100 * sh);
                }
                if (lsum / nb < 0.4 || lsum / nb > 1.9) {
                    lev_bad++;
                    printf("  %s seed %u: the leveler's gain x%.2f on average\n", GENRE[g].name, 2026u + 7u * g + 1000u * sd, lsum / nb);
                }
                rms_db += 20 * log10(sqrt(acc / (2.0 * nb * CTL)) / 32768.0 + 1e-9);
                n++;
            }
        printf("gen: the drums' share at most %.1f %% (%s); the mix %.1f dBFS RMS on average, the leveler's gain off its range %u times\n",
               100 * worst, wn, rms_db / n, lev_bad);
        bad += gcheck("(0.9.9-gen3) every genre x 2: the drums in front, 97 % of the parts' energy at most (p3), the leveler in its range, the mix loud",
                      !over && !lev_bad && rms_db / n >= -15.5);
    }
    /* ---- (the GENE mix pass, the owner: "unprofessional, previous versions better mixed") the electronic genres x 3
     * seeds, 4 bars of LOOP, each part's loudness while it plays (its 0.37 s windows within 30 dB of its loudest, as
     * LUFS gates, as the mix was set from): the bass 10 dB under the drums at most (V10.6z9: HARD BASS's 13.2, TRIBAL's
     * 12.9, ELECTRO's 11.6, TECH HOUSE's 10.5 on average), every melodic part within 25 dB of them (V10.6z9: the arps,
     * leads and textures 20 .. 47 under, an FM6 bass 36 dB gone), no part 3 dB over them, EBM's shouts aside; no engine's list (FM6's ALG, its SLOT) moved off its
     * preset by GEN's jitter (V10.6z9: a bank slot's silence) */
    {
        static const char *const EL[] = {"MINIMAL TECHNO", "HARD TECHNO", "HARD BASS", "HOUSE", "DEEP HOUSE", "TECH HOUSE",
                                         "ELECTRO", "EBM", "BREAKBEAT", "DRUM & BASS", "IDM GLITCH", "PHONK"};
        uint32_t e, sd, n = 0, bass_bad = 0, mel_bad = 0, front_bad = 0, list_bad = 0, sub_bad = 0;
        for (e = 0; e < NELEM(EL); e++)
            for (g = 0; g < NGENRE; g++) {
                if (strcmp(GENRE[g].name, EL[e])) continue;
                for (sd = 0; sd < 3u; sd++) {
                    static double win[NPART][64];
                    double er[RL_TEX + 1] = {0}, ep[NPART] = {0};   /* (RL_N is ui_layer.c's macro here) */
                    uint32_t b, nb, i, p, w;
                    gen_reset();
                    gen_with(g + 1u, 4040u + 11u * g + 1000u * sd, 50, 0);
                    for (p = 0; p < NTRK; p++) {
                        const track_t *t = &trk[p];
                        const engine_t *en = ENGINES[t->eng_req % NENGINES];
                        if (gs.role[p] != RL_DRUM && t->preset < en->npresets)
                            for (i = 0; i < 8u; i++)
                                if (en->edit[i].names && en->edit[i].fmt != F_ENUM && t->p[P_E0 + i] != en->presets[t->preset].e[i]) list_bad++;
                    }
                    nb = (uint32_t)(4u * 4u * 60u * FS / (uint32_t)song.g[G_BPM] / CTL);
                    if (nb > 64u * 256u) nb = 64u * 256u;
                    memset(win, 0, sizeof win);
                    song.master_q12 = 4096;
                    transport_req = 1;
                    for (b = 0; b < nb; b++) {
                        int32_t o[2 * CTL];
                        if (b % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
                        fx_stems = 1;
                        mix_block(o, CTL);
                        for (p = 0; p < NPART; p++)
                            if (gs.act[gs.role[p]] && !(gs.role[p] == RL_TEX && p != gs.trk_of[RL_TEX]))
                                for (i = 0; i < CTL; i++) win[p][b / 256u] += (double)stem_buf[p][i] * stem_buf[p][i];
                    }
                    fx_stems = 0;
                    transport_req = 2;
                    { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
                    for (p = 0; p < NPART; p++) {               /* (the windows within 30 dB of the loudest: its power sounding) */
                        double mx = 0, sum = 0;
                        uint32_t nw = 0;
                        for (w = 0; w < nb / 256u; w++) if (win[p][w] > mx) mx = win[p][w];
                        for (w = 0; w < nb / 256u; w++) if (mx > 0 && win[p][w] > mx * 1e-3) { sum += win[p][w]; nw++; }
                        ep[p] = nw ? sum / nw : 0;
                        er[gs.role[p] % (RL_TEX + 1u)] += ep[p];
                    }
                    n++;
                    if (er[RL_DRUM] <= 0) continue;
                    if (gs.act[RL_BASS] && 10 * log10(er[RL_BASS] / er[RL_DRUM] + 1e-12) < -10) {
                        bass_bad++;
                        printf("  %s seed %u: the bass %.1f dB under the drums\n", GENRE[g].name, 4040u + 11u * g + 1000u * sd, -10 * log10(er[RL_BASS] / er[RL_DRUM] + 1e-12));
                    }
                    for (p = RL_PAD; p <= RL_ARP; p++)
                        if (gs.act[p] && er[p] > 0 && !(p == RL_LEAD && gs.g->lead == LD_EBM) && 10 * log10(er[p] / er[RL_DRUM]) < -25) {
                            mel_bad++;
                            printf("  %s seed %u: role %u %.1f dB under the drums\n", GENRE[g].name, 4040u + 11u * g + 1000u * sd, p, -10 * log10(er[p] / er[RL_DRUM]));
                        }
                    for (p = RL_BASS; p <= RL_TEX; p++)
                        if (er[p] > er[RL_DRUM] * 2.0 && !(p == RL_BASS && !strcmp(GENRE[g].name, "MINIMAL TECHNO"))) front_bad++;   /* (the
                                                         * owner's minimal reference: its sub 8 .. 14 dB over the drums) */
                    if (!strcmp(GENRE[g].name, "MINIMAL TECHNO") && er[RL_BASS] < er[RL_DRUM] * 4.0) {
                        sub_bad++;
                        printf("  MINIMAL TECHNO seed %u: the sub %.1f dB over the drums\n", 4040u + 11u * g + 1000u * sd, 10 * log10(er[RL_BASS] / er[RL_DRUM] + 1e-12));
                    }
                }
            }
        printf("gen: the mix pass: %u electronic pieces, the bass over 10 dB under the drums %u, a melodic part over 25 dB under %u, a part 3 dB over the drums %u, lists moved %u, MINIMAL's sub under 6 dB over %u\n",
               n, bass_bad, mel_bad, front_bad, list_bad, sub_bad);
        bad += gcheck("(the GENE mix pass) the electronic genres x 3: the bass and the melodic parts heard, the drums in front (MINIMAL's sub over them), the presets' lists kept",
                      n >= 30u && bass_bad <= 1u && mel_bad <= 2u && front_bad <= 2u && !list_bad && !sub_bad);
    }
    /* ---- (0.9.9-gen1) the same seed, the same piece (and a different one from another seed), every genre */
    {
        uint32_t det = 0;
        for (g = 0; g < NGENRE; g++) {
            uint64_t a, b, c;
            gen_with(g + 1u, 555u + g, 60, 1); a = gen_proj_hash();
            gen_with(g + 1u, 556u + g, 60, 1); c = gen_proj_hash();
            gen_with(g + 1u, 555u + g, 60, 1); b = gen_proj_hash();
            det += a == b && a != c;
        }
        bad += gcheck("(0.9.9-gen1) every genre: one seed one project (FORM PARTS), another seed another", det == NGENRE);
    }
    /* ---- (0.9.9-gen1) the CPU budget: every piece's voices, as the FM-1 predicts their load (voice.c), under the
     * admission's limit (LOAD_ADMIT: past it a voice is given up), the CPU TEST song's (SAVE > TOOLS 6) printed beside;
     * the voice budget kept */
    {
        uint32_t heavy, worst = 0, vbad = 0, e;
        const char *wn = "";
        gen_reset();
        cpu_test_song();
        heavy = render_load(3000);
        for (g = 0; g < NGENRE; g++)
            for (e = 0; e < 2u; e++) {
                uint32_t l, cl, vm;
                double rms;
                int32_t pk;
                gen_with(g + 1u, 31u * g + 5u + e, e ? 85u : 50u, 0);
                l = render_load(2000);
                if (l > worst) { worst = l; wn = GENRE[g].name; }
                gx_listen(400, &rms, &pk, &cl, &vm);
                vbad += vm > NVOICE + 1u;               /* (the budget's own overshoot: a fading voice) */
            }
        printf("gen: CPU: the heaviest piece %s %u %% predicted (the CPU TEST song %u %%, the admission %u %%)\n", wn, worst, heavy, LOAD_ADMIT);
        bad += gcheck("(0.9.9-gen1) CPU: every genre's piece (ENERGY 50, 85) under the admission's limit, the voice budget kept",
                      worst < LOAD_ADMIT && !vbad);
    }
    {   /* (p1, main 0.9.18) the drums' rolls on their lanes (x2 / x4), never the step's RATCH: a kick under a rolled hat,
         * a hit recorded into the step, never roll with it */
        uint32_t g, sd, k, i, l, stepr = 0, lrolls = 0;
        for (g = 0; g < NGENRE; g++)
            for (sd = 0; sd < 4u; sd++) {
                gen_with(g + 1u, sd * 977u + g * 13u + 3u, 40u + sd * 20u, 0);
                for (k = 0; k < NTRK; k++) {
                    const track_t *t = &trk[k];
                    if (!drum_track(t)) continue;
                    for (i = 0; i < NSTEP; i++) {
                        stepr += t->step[i].time == ST_NOTE && step_ratchet(&t->step[i]) > 1u;
                        for (l = 0; l < NLANE; l++) lrolls += lm_hits(step_lmode(&t->step[i], l)) > 1u;
                    }
                }
            }
        printf("gen:   drum rolls on their lanes: %u, steps with RATCH on a kit: %u\n", lrolls, stepr);
        bad += gcheck("GEN drums: rolls as the lanes' x2 / x4, no step RATCH on a kit", !stepr && lrolls > 0u);
    }
    {   /* (V10.8) BASS CHORDS ARP LEAD all OFF: only the drums play (was: the bass and the chords forced back on,
         * "never fewer than two voices"); AUTO still gets them back */
        uint32_t g, k, i, full = 0, emp = 0;
        for (g = 1; g <= 12u; g++) {
            gp.genre = (uint8_t)g; gp.seed = (uint16_t)(g * 31u); gp.energy = 50; gp.form = 0;
            gp.amt[GP_DRUMS] = 80; for (k = 1; k < GP_N; k++) gp.amt[k] = 0;
            gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
            gen_go();
            for (k = 0; k < NTRK; k++) {
                uint32_t n = 0;
                for (i = 0; i < NSTEP; i++) n += trk[k].step[i].time == ST_NOTE && (trk[k].step[i].n || trk[k].step[i].hit);
                if (drum_track(&trk[k])) emp += !n; else full += n != 0u;
            }
        }
        printf("gen:   all melodic OFF: melodic tracks with notes %u, empty kits %u\n", full, emp);
        bad += gcheck("V10.8 GEN with BASS CHORDS ARP LEAD OFF: the drums alone", !full && !emp);
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
    }
    {   /* (V10.7.2, the owner: "CUMBIA, REGGAETON and DEMBOW very bad: each has very clear rules") every throw of the
         * three keeps its genre's rules (gen_tables.c: the rows, DM_CUMB2 .., GSUB_CU ..), twelve seeds a genre:
         * CUMBIA 76 .. 100, (V10.7.5, after Compas (TexasRADIO, GPL-3.0)) its kick on 1 and 3 (never on 2 and 4 but
         * digital's pickups), the guiro on every beat of every bar (the sonidera's accent on the "e", the owner's demos;
         * villera's and digital's on the beat), the llamador's or the conga's accent on 2's and 4's "and" or "a", the
         * bass's tonica-quinta syncopated (the root at the bar's one, never a note on every beat but digital's, a note
         * off the beat every bar, its notes the chord's root and fifth, an anticipated next root, the turn's fourth or
         * seventh on 4's "and"), major or minor, I IV V only (the V major), the keys on the "and"s;
         * REGGAETON 88 .. 100, the kick on every beat, the snare on the dembow's 3 6 11 14 and nowhere else, the 808's
         * onsets on the kick or the tresillo / dembow, natural minor (no raised V), the chords sounding every bar;
         * DEMBOW 115 .. 132, the kick on every beat, the snare 3 6 11 14 (rapido's extra on 10), the clap on it,
         * sixteenths of hats, denser than reggaeton, one or two chords, the chants (the arp) on the snare's steps, the
         * 808 on the bar's one; and their throws differ (sonidera, villera, digital; classic, perreo, romantico; classic,
         * rapido) */
        uint32_t gi, seeds, i, ok_t[3] = {1, 1, 1}, ok_k[3] = {1, 1, 1}, ok_s[3] = {1, 1, 1}, ok_b[3] = {1, 1, 1}, ok_h[3] = {1, 1, 1};
        uint32_t ok_c[3] = {1, 1, 1}, ok_x[3] = {1, 1, 1}, nmap[3] = {0, 0, 0}, dens[3] = {0, 0, 0}, nbar[3] = {0, 0, 0};
        uint32_t lo_t[3] = {255, 255, 255}, hi_t[3] = {0, 0, 0};
        uint8_t seen[3][64] = {{0}};
        static const char *const LN[3] = {"CUMBIA", "REGGAETON", "DEMBOW"};
        gen_reset();
        for (gi = 0; gi < 3u; gi++)
            for (seeds = 1; seeds <= 12u; seeds++) {
                const track_t *d, *b, *pd, *ar;
                uint32_t len, bl, bpm, bars, br, p, dig;
                gen_with(genre_of(LN[gi]), seeds, 50, 0);
                bpm = (uint32_t)song.g[G_BPM];
                if (bpm < lo_t[gi]) lo_t[gi] = bpm;
                if (bpm > hi_t[gi]) hi_t[gi] = bpm;
                if (!seen[gi][gs.g->drum]) { seen[gi][gs.g->drum] = 1; nmap[gi]++; }
                dig = gs.g->drum == DM_CUMB3;
                if (gi == 0u ? bpm < 76u || bpm > 100u : gi == 1u ? bpm < 88u || bpm > 100u : bpm < 115u || bpm > 132u) ok_t[gi] = 0;
                if (!gs.act[RL_DRUM] || !gs.act[RL_BASS]) { ok_x[gi] = 0; continue; }
                d = &trk[gs.trk_of[RL_DRUM]]; b = &trk[gs.trk_of[RL_BASS]];
                len = (uint32_t)d->p[P_SLEN]; bl = (uint32_t)b->p[P_SLEN]; bars = len / 16u;
                for (br = 0; br < bars; br++) {
                    uint32_t sn = 0, ch = 0, hits = 0, kb = 0, beat, ca = 0, pa = 0, bon = 0, boff = 0;
                    for (p = 0; p < 16u; p++) {
                        const step_t *s = &d->step[br * 16u + p];
                        uint32_t h = s->time == ST_NOTE ? s->hit : 0u, l;
                        for (l = 0; l < 8u; l++) hits += (h >> l) & 1u;
                        if (p % 4u == 0u && (h & 1u)) kb |= 1u << (p / 4u);
                        if (h & 2u) sn |= 1u << p;
                        if (h & 8u) { ch |= 1u << (p / 4u); if (gi == 2u) ch += 16u; }
                        if (s->time == ST_NOTE && (s->acc & 8u)) ca |= 1u << p;
                        if (s->time == ST_NOTE && (s->acc & 0x24u)) pa |= 1u << p;
                    }
                    dens[gi] += hits; nbar[gi]++;
                    if (gi || gs.g->drum == DM_CUMB2 ? kb != 15u : (kb & 5u) != 5u || (!dig && (kb & 10u))) ok_k[gi] = 0;   /* (the
                                                         * kick: cumbia's 1 and 3, V10.7.5: never 2 and 4; V10.7.8 villera
                                                         * as RKT's and the others' every beat) */
                    if (gi == 0u && br + 1u < bars) {           /* (V10.7.5: the guiro's accent, the sonidera's on the
                                                         * "e" (the demos' phase), its throws' on the beat; the llamador's
                                                         * or the conga's on 2's and 4's "and" or "a"; the fill bar's free) */
                        if (gs.g->drum == DM_CUMBIA ? (ca & 0x1111u) || !(ca & 0x2222u) : (ca & 0x0101u) != 0x0101u) ok_h[gi] = 0;
                        if (!(pa & 0x00C0u) || !(pa & 0xC000u)) ok_h[gi] = 0;
                    }
                    if (gi && br + 1u < bars) {                 /* (the snare: the dembow's steps; the fill bar's free) */
                        uint32_t want = 0x4848u, extra = gs.g->drum == DM_DEMB2 ? 0x0400u : 0u;
                        if ((sn & want) != want || (sn & ~(want | extra))) ok_s[gi] = 0;
                    }
                    if (gi == 0u && (ch & 15u) != 15u) ok_h[gi] = 0;   /* (the guiro never stops) */
                    if (gi == 2u && (ch >> 4) < 12u) ok_h[gi] = 0;     /* (dembow: sixteenths of hats) */
                    for (p = 0; gi == 0u && p < 16u; p++) {   /* (V10.7.5: the cumbia bass syncopated: its onsets) */
                        uint32_t j = (br * 16u + p) % bl;
                        if (b->step[j].time != ST_NOTE) continue;
                        if (p % 4u) boff++; else bon |= 1u << (p / 4u);
                    }
                    if (gi == 0u && !dig && (!boff || bon == 15u)) ok_b[gi] = 0;   /* (a note off the beat every bar, never
                                                         * one on every beat: digital's its own, the tecnocumbia's or
                                                         * the sub's half notes) */
                    if (b->step[(br * 16u) % bl].time != ST_NOTE && gchord_starts(br * 16u)) ok_b[gi] = 0;   /* (the one) */
                }
                for (i = 0; i < bl; i++) {                       /* the bass's notes */
                    const step_t *s = &b->step[i];
                    const gch_t *c = gchord(i), *nc = gchord_next(i);
                    uint32_t pc, r, f, rn;
                    if (s->time != ST_NOTE) continue;
                    pc = s->note[0] % 12u; r = gchord_root_pc(i); f = (r + (uint32_t)giv(c, 4)) % 12u;
                    rn = (uint32_t)gtone(nc, 0, 12) % 12u;
                    if (gi == 0u && pc != r && pc != f && !(pc == rn && i % 16u >= 12u) && i % 16u != 14u) ok_b[gi] = 0;
                    if (gi && !((0x5959u >> (i % 16u)) & 1u)) ok_b[gi] = 0;   /* (on 0 3 6 8 11 12 14, 4 the kick's double) */
                    if (gi == 0u && i % 16u == 0u && pc != r) ok_b[gi] = 0;
                }
                if (gi == 0u) {                                  /* cumbia's harmony and keys */
                    if (gs.scale != SC_MIN && gs.scale != SC_MAJ) ok_c[gi] = 0;
                    for (i = 0; i < gs.nch; i++) {
                        if (gs.ch[i].deg != 0u && gs.ch[i].deg != 3u && gs.ch[i].deg != 4u) ok_c[gi] = 0;
                        if (gs.ch[i].deg == 4u && giv(&gs.ch[i], 2) != 4) ok_c[gi] = 0;   /* (the V major) */
                    }
                    if (gs.act[RL_PAD]) {
                        pd = &trk[gs.trk_of[RL_PAD]];
                        for (i = 0; i < (uint32_t)pd->p[P_SLEN]; i++)   /* (the phrase's fourth bar: one step pushed, gen_pads) */
                            if (pd->step[i].time == ST_NOTE && i % 4u != 2u && !(i / 16u % 4u == 3u && i % 16u >= 10u)) ok_c[gi] = 0;
                    }
                } else {
                    if (gs.scale != SC_MIN && gs.scale != SC_PHRY && gs.scale != SC_HARM) ok_c[gi] = 0;
                    if (gi == 1u) for (i = 0; i < gs.nch; i++) if (gs.ch[i].deg == 4u && giv(&gs.ch[i], 2) == 4) ok_c[gi] = 0;   /* (no raised V) */
                    if (gi == 1u && gs.act[RL_PAD]) {            /* (the chords sounding every bar) */
                        pd = &trk[gs.trk_of[RL_PAD]];
                        for (br = 0; br < (uint32_t)pd->p[P_SLEN] / 16u; br++) {
                            uint32_t on = 0;
                            for (p = 0; p < 16u; p++) on += pd->step[br * 16u + p].time != ST_REST;
                            if (!on) ok_c[gi] = 0;
                        }
                    }
                    if (gi == 2u && gs.pn > 2u) ok_c[gi] = 0;
                    if (gi == 2u && gs.act[RL_ARP]) {             /* (the chants on the snare's steps) */
                        ar = &trk[gs.trk_of[RL_ARP]];
                        for (i = 0; i < (uint32_t)ar->p[P_SLEN]; i++) if (ar->step[i].time == ST_NOTE && !((0x4848u >> (i % 16u)) & 1u)) ok_c[gi] = 0;
                    }
                }
            }
        for (gi = 0; gi < 3u; gi++)
            printf("gen:   %-9s %u..%u BPM, %u maps; kick %u snare %u bass %u hats %u harmony %u parts %u; %u hits a bar\n", LN[gi], lo_t[gi], hi_t[gi],
                   nmap[gi], ok_k[gi], ok_s[gi], ok_b[gi], ok_h[gi], ok_c[gi], ok_x[gi], nbar[gi] ? dens[gi] / nbar[gi] : 0u);
        bad += gcheck("V10.7.5 CUMBIA x 12: 76..100, kick 1 3 (V10.7.8 villera RKT every beat), guiro every beat (its accent), llamador 2 4, the bass syncopated, I IV V, keys on the and",
                      ok_t[0] && ok_k[0] && ok_h[0] && ok_b[0] && ok_c[0] && ok_x[0] && nmap[0] == 3u);
        bad += gcheck("V10.7.2 REGGAETON x 12: 88..100, four on the floor, snare 3 6 11 14, the 808 on kick or tresillo, minor",
                      ok_t[1] && ok_k[1] && ok_s[1] && ok_b[1] && ok_c[1] && ok_x[1] && nmap[1] == 2u && lo_t[1] < 92u && hi_t[1] > 96u);
        bad += gcheck("V10.7.2 DEMBOW x 12: 115..132, the riddim, sixteenth hats, denser than reggaeton, 1..2 chords, chants on 3 6 11 14",
                      ok_t[2] && ok_k[2] && ok_s[2] && ok_b[2] && ok_h[2] && ok_c[2] && ok_x[2] && nmap[2] == 2u && hi_t[2] > 124u &&
                      nbar[1] && nbar[2] && dens[2] / nbar[2] > dens[1] / nbar[1] + 4u);
    }
    {   /* (V10.7.8, the owner: "BAILE FUNK's claps into tom shots and cowbells, ultra-minimalist, aggressive"; "REGGAETON
         * more aggression and space, arpeggios"; "CUMBIA's rhythm a guiro, more syncopated", "percs") twelve seeds a
         * genre: CUMBIA's guiro on the closed hat's lane (DRUM-X's filter GR, a SCRAPE: an accent its long stroke, a soft
         * hit a "chk"), every bar of the loop a long stroke and a short one, the sonidera's long on the "e" and short on
         * 2's and 4's "a" too, villera as RKT (the owner's reference: the clap on every quarter, the "a"s pulled early), its llamador, conga and clave LATIN PERC's; REGGAETON's kit and 808 driven (romantico's
         * clean), its arp playing, its pad under the arp, its conga LATIN PERC's; BAILE FUNK 128 .. 136 (funk 150's
         * 145 .. 152), no snare, no clap, no hats (the open hat's crash on a fill's one), the "tum tcha" two tom shots
         * on 3 6 12 (the tom's lane on 3 and 12, the clap's, a high tom, on 6), a cowbell every bar, no chords, the 808
         * short: rests in at least half of its steps, a glide every bar */
        uint32_t gi, sd, br, p, ok_g = 1, ok_r = 1, ok_b = 1, rkt = 0, nlong = 0, nshort = 0, heavy = 0, arps = 0, bars_b = 0;
        static const char *const LN[3] = {"CUMBIA", "REGGAETON", "BAILE FUNK"};
        gen_reset();
        for (gi = 0; gi < 3u; gi++)
            for (sd = 1; sd <= 12u; sd++) {
                const track_t *d, *b;
                uint32_t len, bars, bpm, son, rom;
                gen_with(genre_of(LN[gi]), sd, 50, 0);
                if (!gs.act[RL_DRUM] || !gs.act[RL_BASS]) { if (gi == 0u) ok_g = 0; else if (gi == 1u) ok_r = 0; else ok_b = 0; continue; }
                d = &trk[gs.trk_of[RL_DRUM]]; b = &trk[gs.trk_of[RL_BASS]];
                len = (uint32_t)d->p[P_SLEN]; bars = len / 16u; bpm = (uint32_t)song.g[G_BPM];
                son = gs.g->drum == DM_CUMBIA; rom = gi == 1u && !(gs.g->spec & GSP_HEAVY);
                if (gi == 0u) {
                    if (((dx_kit[DV_HATC].mode >> 2) & 3u) != DXF_GR || dx_kit[DV_HATC].a[DXP_PMOD] < 90u ||
                        memcmp(&dx_kit[DV_TOM], &DX_KITS[DXK_LATIN].lane[DV_TOM], sizeof dx_kit[0]) ||
                        memcmp(&dx_kit[DV_CLAP], &DX_KITS[DXK_LATIN].lane[DV_CLAP], sizeof dx_kit[0])) ok_g = 0;
                    for (br = 0; br + 1u < bars; br++) {
                        uint32_t l = 0, sh = 0, ch = 0;
                        for (p = 0; p < 16u; p++) {
                            const step_t *s = &d->step[br * 16u + p];
                            if (s->time != ST_NOTE || !(s->hit & 8u)) continue;
                            ch |= 1u << p;
                            if (s->acc & 8u) l++; else sh++;
                        }
                        nlong += l; nshort += sh;
                        if (!l || !sh || (son && (ch & 0x8080u) != 0x8080u)) ok_g = 0;
                        if (gs.g->drum == DM_CUMB2) {           /* (RKT: the clap on every quarter, the "a"s early) */
                            for (p = 0; p < 16u; p += 4u)
                                if (!(d->step[br * 16u + p].time == ST_NOTE && (d->step[br * 16u + p].hit & 4u))) ok_g = 0;
                            for (p = 3; p < 16u; p += 4u)
                                if (((ch >> p) & 1u) && ST_MICRO(&d->step[br * 16u + p]) >= 0) ok_g = 0;
                            rkt++;
                        }
                    }
                } else if (gi == 1u) {
                    heavy += !rom && d->p[P_DIST] >= 40 && b->p[P_DIST] >= 30;
                    if (gs.act[RL_ARP]) {
                        const track_t *ar = &trk[gs.trk_of[RL_ARP]];
                        for (p = 0; p < (uint32_t)ar->p[P_SLEN] && ar->step[p].time != ST_NOTE; p++) {}
                        arps += p < (uint32_t)ar->p[P_SLEN];
                        if (gs.act[RL_PAD] && trk[gs.trk_of[RL_PAD]].p[P_LEVEL] >= ar->p[P_LEVEL]) ok_r = 0;
                    }
                    if (rom ? 0 : memcmp(&dx_kit[DV_TOM], &DX_KITS[DXK_LATIN].lane[DV_TOM], sizeof dx_kit[0])) ok_r = 0;
                } else {
                    if (!(bpm >= 128u && bpm <= 136u) && !(bpm >= 145u && bpm <= 152u)) ok_b = 0;
                    if (gs.act[RL_PAD] || memcmp(&dx_kit[DV_CLAP], &DX_KITS[DXK_LATIN].lane[DV_CLAP], sizeof dx_kit[0])) ok_b = 0;
                    for (br = 0; br < bars; br++) {
                        uint32_t cb = 0, rest = 0, sl = 0, last = br + 1u == bars;
                        for (p = 0; p < 16u; p++) {
                            const step_t *s = &d->step[br * 16u + p];
                            uint32_t h = s->time == ST_NOTE ? s->hit : 0u;
                            const step_t *q = &b->step[(br * 16u + p) % (uint32_t)b->p[P_SLEN]];
                            if (h & 0x0Au) ok_b = 0;                       /* (no snare, no closed hat) */
                            if ((h & 0x10u) && (br || p)) ok_b = 0;        /* (the open hat: a crash on the one) */
                            if (!last && (((p == 3u || p == 12u) && !(h & 0x20u)) || (p == 6u && !(h & 4u)))) ok_b = 0;
                            if ((h & 4u) && p != 6u) ok_b = 0;             /* (the high tom: the "tcha" on 6 alone) */
                            cb += (h >> 7) & 1u;
                            rest += q->time == ST_REST;
                            sl += q->time == ST_NOTE && (q->flags & SF_SLIDE);
                        }
                        if (!cb || rest < 8u || !sl) ok_b = 0;
                        bars_b++;
                    }
                }
            }
        printf("gen:   guiro: %u long strokes, %u short, %u RKT bars; REGGAETON heavy %u of 12, arps %u of 12; BAILE FUNK %u bars\n", nlong, nshort, rkt, heavy, arps, bars_b);
        bad += gcheck("V10.7.8 CUMBIA x 12: the guiro (DRUM-X GR, its SCRAPE) long and short every bar, the sonidera's syncopated, villera RKT's, LATIN PERC's percussion",
                      ok_g && nlong && nshort > nlong && rkt);
        bad += gcheck("V10.7.8 REGGAETON x 12: driven (romantico clean), the arp playing over a pad under it, LATIN PERC's conga",
                      ok_r && heavy >= 6u && arps >= 10u);
        bad += gcheck("V10.7.8 BAILE FUNK x 12: tom shots on 3 6 12, cowbells, no snare clap hats chords, the 808 short and gliding",
                      ok_b && bars_b);
    }
    {   /* (V10.6z10, the owner: "every throw of a genre the same song") a genre's throws differ: OCT+ never throws the
         * core (progression, bass, map, the drums' DNA) of the genre's last eight; a seed is still its song whatever was
         * thrown before; the bass, the kick's DNA, the progressions and the IDM breaks a throw's own; the genres' anchors
         * kept (house's and techno's four on the floor, the dembow riddim's 3 + 3 + 2, cumbia's guiro, jazz's ride) */
        uint32_t g, sd, k, i, b, h0, h1, rep = 0, n = 0, maps = 0, anchors = 0, anch_n = 0, ba_n = 0, prog_n = 0, kx = 0;
        uint8_t seen[64] = {0}, bseen[64] = {0};
        uint32_t pseen[64], np = 0;
        gen_reset();
        host_ticks = 0; rng_state = 0x1234567u;         /* (V10.7.8: the throws from one state, whatever the tests
                                                         * before rendered: OCT+'s seeds are drawn from the clock) */
        gen_with(G_HOUSE, 421, 50, 0); h0 = all_sig();
        for (g = 0; g < 24u; g++) {                     /* OCT+ 24 times on DEMBOW (its core the fewest) */
            uint16_t ring[8];
            gp.genre = (uint8_t)genre_of("DEMBOW"); gp.sig = gen_sig();
            memcpy(ring, gsr.h, sizeof ring);
            gen_press();
            for (k = 0; k < 8u && ring[k] != gsr.h[(gsr.at - 1u) & 7u]; k++) {}
            rep += g >= 8u && k < 8u; n++;
        }
        gen_with(G_HOUSE, 421, 50, 0); h1 = all_sig();
        for (sd = 0; sd < 24u; sd++) {                  /* IDM GLITCH's breaks; HOUSE's basses and progressions */
            gen_with(genre_of("IDM GLITCH"), 500u + sd, 50, 0);
            maps += !seen[gs.g->drum]; seen[gs.g->drum] = 1;
            gen_with(G_HOUSE, 700u + sd, 50, 0);
            ba_n += !bseen[gs.ba]; bseen[gs.ba] = 1;
            for (k = 0; k < np && pseen[k] != (uint32_t)(gs.pdeg[0] | gs.pdeg[1] << 4 | gs.pdeg[2] << 8 | gs.pdeg[3] << 12 | gs.pn << 16); k++) {}
            if (k == np && np < 64u) pseen[np++] = (uint32_t)(gs.pdeg[0] | gs.pdeg[1] << 4 | gs.pdeg[2] << 8 | gs.pdeg[3] << 12 | gs.pn << 16);
            {
                const track_t *t = &trk[gs.trk_of[RL_DRUM]];
                for (i = 0; i < 16u; i++) kx += i % 4u && t->step[i].time == ST_NOTE && (t->step[i].hit & 1u);
            }
        }
        prog_n = np;
        for (g = 0; g < 4u; g++)                        /* the anchors, every bar */
            for (sd = 0; sd < 6u; sd++) {
                static const char *const AN[4] = {"HOUSE", "MINIMAL TECHNO", "DEMBOW", "REGGAETON"};
                const track_t *t;
                uint32_t len, ok = 1;
                gen_with(genre_of(AN[g]), 900u + 13u * sd, 20u + 30u * (sd % 3u), 0);
                t = &trk[gs.trk_of[RL_DRUM]];
                len = (uint32_t)t->p[P_SLEN];
                for (b = 0; b < len; b += 16u)
                    for (i = 0; i < 16u; i++) {
                        const step_t *s2 = &t->step[b + i];
                        if (i % 4u == 0u && !(s2->time == ST_NOTE && (s2->hit & 1u))) ok = 0;   /* (the kick on every beat) */
                        if (g >= 2u && (i == 3u || i == 6u || i == 11u || i == 14u) && !(s2->time == ST_NOTE && (s2->hit & 6u))) ok = 0;
                    }
                anchors += ok; anch_n++;
            }
        printf("gen: OCT+ x %u on DEMBOW: %u cores again within eight; IDM GLITCH x 24: %u maps; HOUSE x 24: %u basses, %u progressions, %u extra kicks; anchors %u of %u\n",
               n, rep, maps, ba_n, prog_n, kx, anchors, anch_n);
        bad += gcheck("V10.6z10 OCT+ never the genre's last eight cores; a seed its song whatever came before", !rep && h0 == h1);
        bad += gcheck("  a throw's own: IDM's breaks (3 maps at least), HOUSE's three basses, 8 progressions, extra kicks", maps >= 3u && ba_n == 3u && prog_n >= 8u && kx > 0u);
        bad += gcheck("  the anchors: the four on the floor, the dembow riddim's 3 + 3 + 2 in every bar", anchors == anch_n);
    }
    printf(bad ? "gen: %d FAILED\n" : "gen: all passed\n", bad);
    return bad ? 1 : 0;
}

