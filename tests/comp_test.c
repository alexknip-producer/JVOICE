/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* STEP COMPONENTS v2 (0.9.9-seq2d: core.h CA_* CC_*, seq.c comp_run / comp_migrate, project.c x bit 7, SEQ > COMP):
 * on the real sequencer, project and UI sources (with the stubs of tests/ui_test.c):
 *   CYCLE      four notes muted !1:4 .. !4:4 and a TRANSPOSE +1 R4 at 1:4 over the bar: 16 bars, each its own, then
 *              the same 16 again; the SEQ walker: a note further each pass.
 *   ACTIONS    RATCHET over its LENGTH (and its DECAY), STUTTER and MUTE replacing their steps, ARP over its LENGTH,
 *              OCT; conditions N:M, FIRST, PRE.
 *   MONO/POLY  C2's condition ELSE (MONO: C1 wins, C2 when C1 did not act) against POLY (both stack).
 *   FORMAT     a v2 project round trip; a project of before seq2d (its CP_* and EVERY, no x bit 7) loads migrated:
 *              ARP over its TIEs, ARP SEQ the walker, TRIG 1:2 MUTE !1:2, OCT with EVERY 1:4 OCT+ 1:4, ECHO RATCHET.
 *   TRIG       a MUTE of the inverted condition (VALUE CV_MUTE_TRIG): 4:4, 1:2, 50 %, LENGTH, = MUTE !c, FILL 1ST PRE,
 *              MONO / POLY, the format, its card, the global lane.
 *   UI         SEQ > COMP / COMP 2: KNOB 1 ACTION, KNOB 2 LENGTH, KNOB 3 CONDITION (ELSE on COMP 2 alone), KNOB 4 VALUE.
 *   GLOBAL     (0.9.9-seq2f: seq.c glane, GLO > COMPS) the mock-up's example (d): a STUTTER of 2 steps every 4 bars on all
 *              the tracks and a MUTE of the drum track in bar 8, over 8 bars of the whole sequencer (events_block); MONO
 *              against POLY with a track's ARP; an empty lane (or one masked off) changes nothing; the save round trip,
 *              a project without the block an empty lane; COMPS / COMPS 2's keys and knobs.
 * Built and run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* track 1 alone: a pattern of len steps of the notes nts (0: a REST), C major, GATE 64, no swing; the transport on */
static track_t *pat(uint32_t len, const uint8_t *nts)
{
    track_t *t;
    uint32_t i;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    song.g[G_SWING] = 0;
    t->p[P_ROOT] = 0; t->p[P_SCALE] = 1;
    for (i = 0; i < len; i++)
        t->step[i] = nts[i] ? (step_t){{nts[i]}, 1, ST_NOTE, 0, 100} : (step_t){{0}, 0, ST_REST, 0, 0};
    seq_start();
    return t;
}
static uint32_t step_len(const track_t *t) { return step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), 0); }
/* the notes the voices start over `steps` steps, in order (st: the step each started in) */
static uint32_t run_at(track_t *t, uint32_t steps, uint8_t *nt, uint16_t *st, uint32_t max)
{
    uint32_t b, n = 0, a, k, blocks = steps * step_len(t) / CTL - 1u;
    for (b = 0; b < blocks; b++) {
        uint32_t v0 = vage;
        seq_tick(t, CTL);
        for (a = v0 + 1u; a <= vage; a++)
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].age == a && n < max) {
                    if (st) st[n] = (uint16_t)(b * CTL / step_len(t));
                    nt[n++] = t->v[k].note;
                }
    }
    return n;
}
static uint32_t run(track_t *t, uint32_t steps, uint8_t *nt, uint32_t max) { return run_at(t, steps, nt, 0, max); }
static int seq_is(const uint8_t *nt, uint32_t n, const uint8_t *want, uint32_t m)
{
    uint32_t i;
    if (n != m) {
        printf("ui:   %u notes:", n);
        for (i = 0; i < n && i < 24u; i++) printf(" %u", nt[i]);
        printf("\n");
        return 0;
    }
    for (i = 0; i < n; i++)
        if (nt[i] != want[i]) return 0;
    return 1;
}
static void comp_set(step_t *s, uint32_t k, uint32_t a, uint32_t c, uint32_t nm, uint32_t len, uint32_t v)
{
    if (k) { s->comp2 = CO_MAKE(a, c); s->carg2 = (uint8_t)v; }
    else { s->comp = CO_MAKE(a, c); s->carg = (uint8_t)v; }
    s->clen[k] = (uint8_t)(len - 1u);
    s->cnm[k] = (uint8_t)nm;
}

static int cycle(void)
{
    int bad = 0, ok = 1;
    static uint8_t nt[512];
    static uint16_t at[512];
    uint32_t n, i, b;
    track_t *t;
    /* (the mock-up's example (a)) steps 1 5 9 13: C4 G4 D5 A5 (degrees 0 4 8 12 of C major), each MUTE'd but on its own
     * bar of four (!1:4 .. !4:4); step 1's C2 TRANSPOSE +1 degree, RANGE 4, at 1:4, over the bar (LENGTH 16; POLY) */
    {
        static const uint8_t P[16] = {60, 0, 0, 0, 67, 0, 0, 0, 74, 0, 0, 0, 81, 0, 0, 0};
        uint8_t bar[32];
        t = pat(16, P);
        for (i = 0; i < 4u; i++)
            comp_set(&t->step[4u * i], 0, CA_MUTE, CC_NNM, CNM_MAKE(i + 1u, 4u), 1, 0);
        comp_set(&t->step[0], 1, CA_TRANS, CC_NM, CNM_MAKE(1u, 4u), 16, CV_TR_MAKE(1, 4u));
        seq_start();
        n = run_at(t, 32u * 16u, nt, at, sizeof nt);
        ok = n == 32u;
        for (b = 0; b < 32u && b < n; b++) {
            bar[b] = nt[b];
            ok &= at[b] / 16u == b;                     /* (one note a bar) */
            ok &= nt[b] == (uint8_t)scale_steps(t, 60, (int32_t)(4u * (b % 4u) + (b / 4u) % 4u));
        }
        for (b = 0; b < 16u && ok; b++)
            for (i = 0; i < b; i++)
                ok &= bar[i] != bar[b];
        for (b = 16; b < 32u && ok; b++)
            ok &= bar[b] == bar[b - 16u];
        if (!ok) { printf("ui:   bars:"); for (b = 0; b < n && b < 32u; b++) printf(" %u", nt[b]); printf("\n"); }
        bad += check("CYCLE: four notes !1:4 .. !4:4 + TRANSPOSE +1 R4 at 1:4: 16 bars, all different, then again", ok);
    }
    {   /* SEQ: step 1 walks the pattern's notes, a note a pass (60 64 67 71: pass 2 plays 64 there ..); BOUNCE back */
        static const uint8_t P[4] = {60, 64, 67, 71};
        static const uint8_t W[16] = {60, 64, 67, 71, 64, 64, 67, 71, 67, 64, 67, 71, 71, 64, 67, 71};
        static const uint8_t WB[24] = {60, 64, 67, 71, 64, 64, 67, 71, 67, 64, 67, 71, 71, 64, 67, 71, 67, 64, 67, 71,
                                       64, 64, 67, 71};
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_SEQ, CC_ALWAYS, 0, 1, 0);
        seq_start();
        n = run(t, 16, nt, sizeof nt);
        ok = seq_is(nt, n, W, 16);
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_SEQ, CC_ALWAYS, 0, 1, CV_SEQ_BOUNCE);
        seq_start();
        n = run(t, 24, nt, sizeof nt);
        ok &= seq_is(nt, n, WB, 24);
        bad += check("SEQ walker: a note of the pattern further each pass (LOOP; BOUNCE turns back at the last)", ok);
    }
    return bad;
}

static int actions(void)
{
    int bad = 0, ok;
    uint8_t nt[64];
    uint32_t n;
    track_t *t;
    static const uint8_t P[4] = {60, 62, 64, 65};
    {   /* RATCHET x2 over 3 steps: the three split, the fourth as written; x3 at 1:2: every other pass */
        static const uint8_t W[7] = {60, 60, 62, 62, 64, 64, 65};
        static const uint8_t W2[10] = {60, 60, 60, 62, 64, 65, 60, 62, 64, 65};
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_RATCH, CC_ALWAYS, 0, 3, 0);
        seq_start();
        n = run(t, 4, nt, sizeof nt);
        ok = seq_is(nt, n, W, 7);
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_RATCH, CC_NM, CNM_MAKE(1u, 2u), 1, 1);
        seq_start();
        n = run(t, 8, nt, sizeof nt);
        ok &= seq_is(nt, n, W2, 10);
        bad += check("RATCHET x2 over its LENGTH of 3 steps (each split); x3 on 1:2 (every other pass)", ok);
    }
    {   /* DECAY: x4 DOWN, each part softer; UP louder */
        uint32_t b, k, nv = 0, a;
        uint8_t vel[8];
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_RATCH, CC_ALWAYS, 0, 1, 2u | 1u << 3);
        seq_start();
        for (b = 0; b + 1u < step_len(t) / CTL; b++) {
            uint32_t v0 = vage;
            seq_tick(t, CTL);
            for (a = v0 + 1u; a <= vage; a++)
                for (k = 0; k < NVOICE; k++)
                    if (t->v[k].age == a && nv < 8u) vel[nv++] = t->v[k].vel;
        }
        ok = nv == 4u && vel[0] > vel[1] && vel[1] > vel[2] && vel[2] > vel[3];
        bad += check("RATCHET x4 DECAY DOWN: four parts, each softer", ok);
    }
    {   /* STUTTER: step 1 again over 3 steps (60 60 60 65); every 2 steps over 4 (60 . 60 .) */
        static const uint8_t W[4] = {60, 60, 60, 65}, W2[2] = {60, 60};
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_STUT, CC_ALWAYS, 0, 3, 0);
        seq_start();
        n = run(t, 4, nt, sizeof nt);
        ok = seq_is(nt, n, W, 4);
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_STUT, CC_ALWAYS, 0, 4, 1);
        seq_start();
        n = run(t, 4, nt, sizeof nt);
        ok &= seq_is(nt, n, W2, 2);
        bad += check("STUTTER: its step replacing the next ones over its LENGTH (every step; every other step)", ok);
    }
    {   /* MUTE: steps 2 and 3 silent (LENGTH 2); FIRST: only the first pass; PRE: step 2 follows step 1's 1:2 */
        static const uint8_t W[2] = {60, 65}, W2[6] = {60, 65, 60, 62, 64, 65}, W3[6] = {64, 65, 60, 62, 64, 65};
        t = pat(4, P);
        comp_set(&t->step[1], 0, CA_MUTE, CC_ALWAYS, 0, 2, 0);
        seq_start();
        n = run(t, 4, nt, sizeof nt);
        ok = seq_is(nt, n, W, 2);
        t = pat(4, P);
        comp_set(&t->step[1], 0, CA_MUTE, CC_FIRST, 0, 2, 0);
        seq_start();
        n = run(t, 8, nt, sizeof nt);
        ok &= seq_is(nt, n, W2, 6);
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_MUTE, CC_NM, CNM_MAKE(1u, 2u), 1, 0);
        comp_set(&t->step[1], 0, CA_MUTE, CC_PRE, 0, 1, 0);
        seq_start();
        n = run(t, 8, nt, sizeof nt);
        ok &= seq_is(nt, n, W3, 6);
        bad += check("MUTE: its LENGTH of steps silent; FIRST the first pass only; PRE after the step before's 1:2", ok);
    }
    {   /* ARP: a chord's x2 UP over 2 steps (the second replaced): 60 64 | 67 60, then 64 65 */
        static const uint8_t W[6] = {60, 64, 67, 60, 64, 65};
        t = pat(4, P);
        t->step[0] = (step_t){{60, 64, 67}, 3, ST_NOTE, 0, 100};
        comp_set(&t->step[0], 0, CA_ARP, CC_ALWAYS, 0, 2, CP_ARG(0u, 0u, 0u));
        seq_start();
        n = run(t, 4, nt, sizeof nt);
        ok = seq_is(nt, n, W, 6);
        bad += check("ARP x2 UP over its LENGTH of 2 (the step after replaced), the rest as written", ok);
    }
    {   /* OCT+ 2 octaves, OCT- 1 (V10.6v: JUMP gone, RAMP its code; tests/ramp_test.c) */
        static const uint8_t W2[4] = {84, 50, 64, 65};
        ok = 1;
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_OCTU, CC_ALWAYS, 0, 1, 1);
        comp_set(&t->step[1], 0, CA_OCTD, CC_ALWAYS, 0, 1, 0);
        seq_start();
        n = run(t, 4, nt, sizeof nt);
        ok &= seq_is(nt, n, W2, 4);
        bad += check("OCT+ 2 octaves, OCT- 1 and back", ok);
    }
    return bad;
}

static int mono_poly(void)
{
    int bad = 0, ok;
    uint8_t nt[64];
    uint32_t n;
    track_t *t;
    static const uint8_t P[4] = {60, 62, 64, 65};
    static const uint8_t WP[5] = {72, 72, 62, 64, 65}, WM[4] = {72, 62, 64, 65}, WE[10] = {60, 60, 62, 64, 65, 72, 62, 64,
                                                                                          65, 60};
    /* POLY: C1 OCT+ and C2 RATCHET x2 on step 1: both, two notes an octave up */
    t = pat(4, P);
    comp_set(&t->step[0], 0, CA_OCTU, CC_ALWAYS, 0, 1, 0);
    comp_set(&t->step[0], 1, CA_RATCH, CC_ALWAYS, 0, 1, 0);
    seq_start();
    n = run(t, 4, nt, sizeof nt);
    ok = seq_is(nt, n, WP, 5);
    bad += check("POLY: C1 OCT+ and C2 RATCHET x2 on a step stack (72 72)", ok);
    /* MONO (C2 ELSE): C1 wins (72 once); C1 at 2:2: on the first pass C2's RATCHET (60 60), on the second C1's */
    t = pat(4, P);
    comp_set(&t->step[0], 0, CA_OCTU, CC_ALWAYS, 0, 1, 0);
    comp_set(&t->step[0], 1, CA_RATCH, CC_ELSE, 0, 1, 0);
    seq_start();
    n = run(t, 4, nt, sizeof nt);
    ok = seq_is(nt, n, WM, 4);
    t = pat(4, P);
    comp_set(&t->step[0], 0, CA_OCTU, CC_NM, CNM_MAKE(2u, 2u), 1, 0);
    comp_set(&t->step[0], 1, CA_RATCH, CC_ELSE, 0, 1, 0);
    seq_start();
    n = run(t, 9, nt, sizeof nt);
    ok &= n >= 9u && !memcmp(nt, WE, 9);
    bad += check("MONO (C2 ELSE): one acts, C1 first (72); C2 when C1's condition fails (60 60, then 72)", ok);
    /* the UI: COMP's footer column says MONO */
    ui_power_on();
    TSEL->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    comp_set(&TSEL->step[0], 0, CA_OCTU, CC_ALWAYS, 0, 1, 0);
    comp_set(&TSEL->step[0], 1, CA_RATCH, CC_ELSE, 0, 1, 0);
    go_page(GR_COMP); frame();
    bad += check("  COMP shows the step MONO beside its CONDITION", strstr(ui.col[2], "MONO") != 0);
    return bad;
}

static int format(void)
{
    int bad = 0, ok;
    uint8_t nt[64];
    uint32_t n;
    track_t *t;
    project_store_t st;
    /* v2: a round trip keeps ACTION, CONDITION, VALUE, LENGTH and N:M of both */
    ui_power_on();
    trk[0].step[3] = (step_t){{60, 64}, 2, ST_NOTE, 0, 100};
    comp_set(&trk[0].step[3], 0, CA_TRANS, CC_NM, CNM_MAKE(3u, 16u), 16, CV_TR_MAKE(-2, 5u));
    comp_set(&trk[0].step[3], 1, CA_LOCK, CC_ELSE, 0, 7, 0);
    trk[1].step[5] = (step_t){{0}, 0, ST_REST, 0, 0};
    comp_set(&trk[1].step[5], 0, CA_MUTE, CC_P25, 0, 32, 1);
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    memset(&trk[0].step[3], 0, sizeof trk[0].step[3]);
    memset(&trk[1].step[5], 0, sizeof trk[1].step[5]);
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    ok &= trk[0].step[3].comp == CO_MAKE(CA_TRANS, CC_NM) && trk[0].step[3].carg == CV_TR_MAKE(-2, 5u) &&
          trk[0].step[3].clen[0] == 15u && trk[0].step[3].cnm[0] == CNM_MAKE(3u, 16u) &&
          trk[0].step[3].comp2 == CO_MAKE(CA_LOCK, CC_ELSE) && trk[0].step[3].clen[1] == 6u &&
          trk[1].step[5].comp == CO_MAKE(CA_MUTE, CC_P25) && trk[1].step[5].carg == 1u && trk[1].step[5].clen[0] == 31u;
    bad += check("FORMAT: a project keeps components v2 (x bit 7: LENGTH and N:M of both), a REST's too", ok);
    /* before seq2d: CP_* as such (no x bit 7) migrate as the project loads */
    ui_power_on();
    trk[0].p[P_SLEN] = 8;
    trk[0].step[0] = (step_t){{60, 64, 67}, 3, ST_NOTE, 0, 100};
    trk[0].step[0].comp = CP_ARPN; trk[0].step[0].carg = CP_ARG(0u, 0u, 0u);   /* x2 UP over the two TIEs after it */
    trk[0].step[1] = (step_t){{0}, 0, ST_TIE, 0, 0};
    trk[0].step[2] = (step_t){{0}, 0, ST_TIE, 0, 0};
    trk[0].step[3] = (step_t){{62}, 1, ST_NOTE, 0, 100};
    trk[0].step[3].comp = CP_TRIG; trk[0].step[3].carg = 0;                     /* 1:2 */
    trk[0].step[4] = (step_t){{64}, 1, ST_NOTE, 0, 100};
    trk[0].step[4].comp = CP_OCT; trk[0].step[4].carg = 0; ST_EVERY_SET(&trk[0].step[4], EV_1_4);
    trk[0].step[5] = (step_t){{65}, 1, ST_NOTE, 0, 100};
    trk[0].step[5].comp = CP_ARPQ; trk[0].step[5].carg = CP_ARG(0u, 0u, 0u);
    trk[0].step[5].comp2 = CP_ECHO; trk[0].step[5].carg2 = 1;                 /* x3, fading */
    trk[0].step[6] = (step_t){{67}, 1, ST_NOTE, 0, 100};
    trk[0].step[6].comp = CP_FILL;
    trk[0].step[7] = (step_t){{69}, 1, ST_NOTE, 0, 100};
    trk[0].step[7].comp = CP_WALK; trk[0].step[7].carg = CP_WARG(1u, 0u, 0u);  /* UP 3 degrees, a move every 2 loops */
    proj_comp_legacy = 1;
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    proj_comp_legacy = 0;
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    t = &trk[0];
    ok &= t->step[0].comp == CO_MAKE(CA_ARP, CC_ALWAYS) && t->step[0].carg == CP_ARG(0u, 0u, 0u) && t->step[0].clen[0] == 2u;
    ok &= t->step[3].comp == CO_MAKE(CA_MUTE, CC_NNM) && t->step[3].cnm[0] == CNM_MAKE(1u, 2u) && !t->step[3].clen[0];
    ok &= t->step[4].comp == CO_MAKE(CA_OCTU, CC_NM) && t->step[4].cnm[0] == CNM_MAKE(1u, 4u) && !(t->step[4].cwhen & 15u);
    ok &= t->step[5].comp == CO_MAKE(CA_SEQ, CC_ALWAYS) && t->step[5].comp2 == CO_MAKE(CA_RATCH, CC_ALWAYS) &&
          CV_RAT_HITS(t->step[5].carg2) == 3u && CV_RAT_DEC(t->step[5].carg2) == 1u;
    ok &= t->step[6].comp == CO_MAKE(CA_MUTE, CC_NFILL);
    ok &= t->step[7].comp == CO_MAKE(CA_TRANS, CC_NM) && t->step[7].cnm[0] == CNM_MAKE(1u, 2u) &&
          CV_TR_MOVE(t->step[7].carg) == 1 && CV_TR_RANGE(t->step[7].carg) == 4u;
    bad += check("FORMAT: a project of before seq2d migrates: ARP over its TIEs, TRIG -> MUTE !1:2, EVERY 1:4 -> OCT+ 1:4, "
                 "ARP SEQ -> SEQ, ECHO -> RATCHET x3 DOWN, FILL -> MUTE !FILL, WALK -> TRANSPOSE R4 at 1:2", ok);
    {   /* .. and plays as it did: the arpeggio 60 64 | 67 60 | 64 67 over the TIEs, 62 on the first pass (TRIG 1:2), 76 */
        static const uint8_t W[7] = {60, 64, 67, 60, 64, 67, 62};
        t->p[P_SGATE] = 64; t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
        seq_start();
        n = run(t, 5, nt, sizeof nt);
        ok = n == 8u && !memcmp(nt, W, 7) && nt[7] == 76u;
        bad += check("  .. and plays the arpeggio over its TIEs, the TRIG's step, the OCT on loop 1 of 4 as before", ok);
    }
    {   /* in memory (GEN, DICE): comp_legacy then comp_migrate */
        step_t s[4];
        memset(s, 0, sizeof s);
        s[0].time = ST_NOTE; s[0].n = 1; s[0].note[0] = 60;
        comp_legacy(&s[0], 0, CP_OCT, 4);                 /* ALT: up on odd loops, down on the others, both slots */
        comp_legacy(&s[1], 0, CP_TONAL, 9);
        comp_migrate(s, 4);
        ok = s[0].comp == CO_MAKE(CA_OCTU, CC_NM) && s[0].cnm[0] == CNM_MAKE(1u, 2u) &&
             s[0].comp2 == CO_MAKE(CA_OCTD, CC_NM) && s[0].cnm[1] == CNM_MAKE(2u, 2u) &&
             s[1].comp == CO_MAKE(CA_TRANS, CC_ALWAYS) && CV_TR_MOVE(s[1].carg) == 2 && CV_TR_RANGE(s[1].carg) == 1u &&
             s[1].clen[0] == 2u;
        bad += check("  in memory: OCT ALT into both slots (1:2 up, 2:2 down), TONAL a FIX TRANSPOSE to the loop's end", ok);
    }
    return bad;
}

static int comp_ui(void)
{
    int bad = 0, ok;
    track_t *t;
    ui_power_on();
    t = TSEL;
    t->step[0] = (step_t){{60, 64, 67}, 3, ST_NOTE, 0, 100};
    go_page(GR_COMP); frame();
    ok = str_eq(cur_page()->title, "COMP") && comp_view() == CA_NONE;
    turn(EN_K1, 1);                                       /* OFF -> ARP: four steps, UP x4 1 OCT */
    ok &= CO_ACT(t->step[0].comp) == CA_ARP && t->step[0].clen[0] == 3u && t->step[0].carg == CP_ARG(0u, 2u, 0u);
    turn(EN_K2, 2);                                       /* LENGTH 6 */
    ok &= t->step[0].clen[0] == 5u;
    turn(EN_K3, 1);                                       /* 25 % */
    ok &= CO_COND(t->step[0].comp) == CC_P25 && strstr(ui.col[2], "25%") != 0;
    turn(EN_K3, 11);                                      /* past ELSE (COMP 2's): 1:2 */
    ok &= CO_COND(t->step[0].comp) == CC_NM && t->step[0].cnm[0] == CNM_MAKE(1u, 2u);
    turn(EN_K3, 3);                                       /* 2:2 1:3 2:3 */
    ok &= t->step[0].cnm[0] == CNM_MAKE(2u, 3u) && strstr(ui.col[2], "2:3") != 0;
    turn(EN_K4, 1);                                       /* x4 -> x6 (RATE first) */
    ok &= CP_RATE(t->step[0].carg) == 3u && CP_ORD(t->step[0].carg) == 0u;
    turn(EN_K1, 4);                                       /* .. RATCH: the VALUE its own, the CONDITION kept */
    ok &= CO_ACT(t->step[0].comp) == CA_RATCH && CO_COND(t->step[0].comp) == CC_NM && t->step[0].carg == 0u;
    turn(EN_K1, -4); turn(EN_K1, -1);                     /* OFF: gone */
    ok &= !t->step[0].comp && !t->step[0].clen[0];
    bad += check("SEQ > COMP: KNOB 1 ACTION, KNOB 2 LENGTH, KNOB 3 CONDITION (ELSE skipped), KNOB 4 VALUE, OFF clears", ok);
    ui.page++; page_entered(); frame();
    ok = str_eq(cur_page()->title, "COMP 2") && comp_slot() == 1u;
    turn(EN_K1, 4); turn(EN_K1, 3);                       /* MUTE */
    turn(EN_K3, 4); turn(EN_K3, 4); turn(EN_K3, 4);   /* ELSE: MONO */
    ok &= CO_ACT(t->step[0].comp2) == CA_MUTE && CO_COND(t->step[0].comp2) == CC_ELSE && !t->step[0].comp;
    bad += check("SEQ > COMP 2: the step's C2 (MUTE), ELSE its condition (MONO)", ok);
    return bad;
}

/* ---- (0.9.9-seq2f) the global lane: the whole sequencer (events_block), every track's notes by global step ---- */
static void gl_clear(void) { memset(glane, 0, sizeof glane); gl_mono = 0; gl_touch(); }
static void gl_set(uint32_t i, uint32_t a, uint32_t c, uint32_t nm, uint32_t len, uint32_t v, uint32_t mask)
{
    glane[i] = (gcomp_t){CO_MAKE(a, c), (uint8_t)v, (uint8_t)(len - 1u), (uint8_t)nm, (uint8_t)mask};
    gl_touch();
}
/* every track REST over 16 steps, 1/16, no swing; the transport stopped */
static void gl_blank(void)
{
    uint32_t k, i;
    ui_power_on();
    gl_clear();
    song.g[G_SWING] = 0;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        track_defaults_steps(t);
        t->p[P_SLEN] = 16; t->p[P_SGATE] = 64; t->p[P_SSWING] = 0;
        t->p[P_ROOT] = 0; t->p[P_SCALE] = 1;
        for (i = 0; i < NSTEP; i++) t->step[i] = (step_t){{0}, 0, ST_REST, 0, 0};
    }
}
#define GRUN_MAX 512u
static uint8_t g_trk[GRUN_MAX], g_note[GRUN_MAX];
static uint16_t g_at[GRUN_MAX];
/* `steps` 1/16 steps of the transport from PLAY: the notes every voice starts (track, global step, note) */
static uint32_t grun(uint32_t steps)
{
    uint32_t b, n = 0, a, k, v, sl = div_samples(2), blocks = steps * sl / CTL - 1u;
    seq_start();
    for (b = 0; b < blocks; b++) {
        uint32_t v0 = vage;
        events_block(CTL);
        for (a = v0 + 1u; a <= vage; a++)
            for (k = 0; k < NTRK; k++)
                for (v = 0; v < NVOICE; v++)
                    if (trk[k].v[v].age == a && n < GRUN_MAX) {
                        g_trk[n] = (uint8_t)k; g_at[n] = (uint16_t)(b * CTL / sl); g_note[n++] = trk[k].v[v].note;
                    }
    }
    song.playing = 0;
    return n;
}
static const uint8_t GMEL[16] = {60, 62, 64, 65, 67, 69, 71, 72, 74, 76, 77, 79, 81, 83, 84, 86};
static int gl_example(void)
{
    int bad = 0, ok = 1;
    uint32_t n, i, bar, st, hits[8] = {0}, mel = 0, bad_mel = 0;
    gl_blank();
    for (i = 0; i < 16u; i++) {                         /* T1 (DRUM-X) BD on every step, T4 the scale up */
        trk[0].step[i] = (step_t){{0}, 0, ST_NOTE, 0, 100}; trk[0].step[i].hit = 1;
        trk[3].step[i] = (step_t){{GMEL[i]}, 1, ST_NOTE, 0, 100};
    }
    gl_set(0, CA_STUT, CC_NM, CNM_MAKE(4u, 4u), 2, 0, 63);    /* step 1: STUTTER 2 steps, 4:4, all */
    gl_set(2, CA_MUTE, CC_NM, CNM_MAKE(8u, 8u), 14, 0, 1);    /* step 3: MUTE 14 steps, 8:8, T1 */
    n = grun(8u * 16u);
    for (i = 0; i < n; i++) {
        bar = g_at[i] / 16u; st = g_at[i] % 16u;
        if (bar >= 8u) continue;
        if (g_trk[i] == 0u) hits[bar]++;
        else if (g_trk[i] == 3u) {
            uint8_t want = st == 1u && bar % 4u == 3u ? GMEL[0] : GMEL[st];
            mel++;
            bad_mel += g_note[i] != want;
        } else
            ok = 0;
    }
    for (bar = 0; bar < 8u; bar++)
        ok &= hits[bar] == (bar == 7u ? 2u : 16u);
    ok &= mel == 128u && !bad_mel;
    if (!ok) {
        printf("ui:   mel %u (wrong %u), hits", mel, bad_mel);
        for (bar = 0; bar < 8u; bar++) printf(" %u", hits[bar]);
        printf("\n");
    }
    bad += check("GLOBAL (d): STUTTER 2 steps at 4:4 on all (bars 4, 8: step 2 plays step 1 again), MUTE 14 of T1 at 8:8 "
                 "(bar 8: its two first hits alone), the other tracks untouched", ok);
    return bad;
}
static int gl_mono_poly(void)
{
    int bad = 0, ok;
    uint32_t n0, n1, n2, n, i;
    static uint8_t a0[64], a1[64], a2[64];
    gl_blank();
    trk[3].step[0] = (step_t){{60, 64, 67}, 3, ST_NOTE, 0, 100};
    comp_set(&trk[3].step[0], 0, CA_ARP, CC_ALWAYS, 0, 4, CA_VDEF[CA_ARP]);
    n0 = grun(4);                                       /* the ARP alone */
    for (i = 0; i < n0 && i < 64u; i++) a0[i] = g_note[i];
    gl_set(0, CA_STUT, CC_ALWAYS, 0, 2, 0, 1u << 3);    /* a global STUTTER over steps 1-2 on T4 */
    gl_mono = 0;
    n1 = grun(4);                                       /* POLY: the ARP inside the stutter */
    for (i = 0; i < n1 && i < 64u; i++) a1[i] = g_note[i];
    gl_mono = 1;
    n2 = grun(4);                                       /* MONO: the stutter alone, the chord twice */
    for (i = 0; i < n2 && i < 64u; i++) a2[i] = g_note[i];
    trk[3].step[0].comp = 0;                            /* the reference: the chord stuttered, no ARP */
    n = grun(4);
    ok = n0 > 3u && a0[0] == 60u && a0[1] != 60u;       /* (the arpeggio: one note at a time) */
    ok &= n1 == n0 && !memcmp(a0, a1, n0);
    ok &= n2 == n && n2 >= 4u && !memcmp(a2, g_note, n2) && (n2 != n1 || memcmp(a1, a2, n2));
    bad += check("GLOBAL POLY: a track's ARP runs inside the global STUTTER; MONO: the global alone (the chord twice)", ok);
    return bad;
}
static int gl_empty(void)
{
    int bad = 0, ok;
    uint32_t n0, n1, i;
    static uint8_t a0[GRUN_MAX];
    static uint16_t t0[GRUN_MAX];
    gl_blank();
    for (i = 0; i < 16u; i += 2u) trk[3].step[i] = (step_t){{GMEL[i]}, 1, ST_NOTE, 0, 100};
    comp_set(&trk[3].step[0], 0, CA_OCTU, CC_NM, CNM_MAKE(1u, 2u), 1, 0);
    comp_set(&trk[3].step[4], 0, CA_RATCH, CC_P50, 0, 1, 1);
    comp_set(&trk[3].step[8], 1, CA_TRANS, CC_ALWAYS, 0, 4, CV_TR_MAKE(2, 3u));
    rng_state = 0x1234567u;
    n0 = grun(64);
    for (i = 0; i < n0; i++) a0[i] = g_note[i], t0[i] = g_at[i];
    ok = n0 > 30u && !gsp_n && !gl_ser && !gl_on;       /* (empty: no span, no state) */
    gl_set(3, CA_STUT, CC_P50, 0, 4, 1, 1u << 2);       /* a lane for T3 alone: T4 as before */
    gl_set(9, CA_OCTU, CC_ALWAYS, 0, 2, 1, 1u << 1);
    rng_state = 0x1234567u;
    n1 = grun(64);
    ok &= n1 == n0 && !memcmp(a0, g_note, n0) && !memcmp(t0, g_at, n0 * sizeof t0[0]);
    bad += check("GLOBAL: an empty lane changes nothing (no state, no roll); one masked to other tracks neither (its own dice)", ok);
    return bad;
}
static int gl_format(void)
{
    int bad = 0, ok;
    project_store_t st;
    gcomp_t keep[GL_N];
    ui_power_on();
    gl_clear();
    gl_set(0, CA_STUT, CC_NM, CNM_MAKE(1u, 4u), 2, 1, 63);
    gl_set(8, CA_MUTE, CC_NM, CNM_MAKE(8u, 8u), 16, 1, 8);
    gl_set(15, CA_TRANS, CC_P75, 0, 32, CV_TR_MAKE(-3, 4u), 0x22);
    gl_mono = 1;
    memcpy(keep, glane, sizeof keep);
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    gl_clear();
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    ok &= !memcmp(keep, glane, sizeof keep) && gl_mono == 1u && gl_on;
    bad += check("GLOBAL FORMAT: the lane (ACTION CONDITION VALUE LENGTH N:M TRACKS) and MONO round trip ('G' 'C')", ok);
    /* a project without the block (as every one before seq2f): the lane empty, POLY */
    gl_clear();
    project_capture(&proj_scratch);
    ok = proj_pack(&st, &proj_scratch);
    gl_set(4, CA_MUTE, CC_ALWAYS, 0, 1, 0, 63); gl_mono = 1;
    ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
    ok &= !gl_on && !gl_mono && !glane[4].comp;
    bad += check("  a project without the global block loads with the lane empty, POLY", ok);
    return bad;
}
static int gl_ui(void)
{
    int bad = 0, ok;
    ui_power_on();
    gl_clear();
    go_page(GR_GCOMP); frame();
    ok = str_eq(cur_page()->title, "COMPS") && gco_on() && !gco_slot();
    tap_key(key_at(0, 2));                              /* step 3 */
    turn(EN_K1, 2);                                     /* OFF MUTE STUTTER */
    ok &= gl_cur == 2u && CO_ACT(glane[2].comp) == CA_STUT && glane[2].len == 1u && glane[2].mask == 63u && gl_on;
    turn(EN_K2, 2);                                     /* LENGTH 4 */
    turn(EN_K3, 13);                                    /* past NEI !NEI ELSE: 1:2 */
    ok &= glane[2].len == 3u && CO_COND(glane[2].comp) == CC_NM && glane[2].nm == CNM_MAKE(1u, 2u);
    turn(EN_K4, 1);                                     /* EVERY 2 */
    ok &= glane[2].val == 1u && strstr(ui.col[2], "1:2") != 0;
    tap_key(key_at(1, 0));                              /* black key 1: T1 off */
    ok &= glane[2].mask == 62u;
    bad += check("GLO > COMPS: a white key the step, KNOB 1..4 ACTION LENGTH CONDITION (no NEI, no ELSE) VALUE, black keys "
                 "the TRACKS", ok);
    ui.page++; page_entered(); frame();
    ok = str_eq(cur_page()->title, "COMPS 2") && gco_slot();
    turn(EN_K2, 1); turn(EN_K3, 1);                     /* T2 .. ON (already) */
    ok &= gl_trk == 1u && glane[2].mask == 62u;
    turn(EN_K3, -1);
    ok &= glane[2].mask == 60u;
    turn(EN_K4, 1);
    ok &= gl_mono == 1u && strstr(ui.col[3], "MONO") != 0;
    bad += check("GLO > COMPS 2: STEP, TRACK, its ON / OFF, MODE MONO / POLY", ok);
    return bad;
}

/* ---- TRIG (an Elektron trig condition: core.h CV_MUTE_TRIG, a MUTE of the inverted condition) ---- */
static uint32_t trig_run(uint32_t c, uint32_t nm, uint32_t len, uint32_t steps, uint8_t *nt, uint16_t *at, uint32_t max,
                         uint32_t act, uint32_t v)
{
    static const uint8_t P[4] = {60, 62, 64, 65};
    track_t *t = pat(4, P);
    comp_set(&t->step[0], 0, act, c, nm, len, v);
    seq_start();
    return run_at(t, steps, nt, at, max);
}
/* the passes (of `passes` 4-step passes) step 1's 60 plays */
static uint32_t trig_passes(uint32_t c, uint32_t nm, uint32_t passes)
{
    static uint8_t nt[1024];
    static uint16_t at[1024];
    uint32_t n = trig_run(c, nm, 1, passes * 4u, nt, at, sizeof nt, CA_MUTE, CV_MUTE_TRIG), i, m = 0;
    for (i = 0; i < n; i++)
        if (nt[i] == 60u && at[i] % 4u == 0u && at[i] / 4u < 32u) m |= 1u << (at[i] / 4u);
        else if (nt[i] == 60u) m |= 1u << 31;
    return m;
}
static int trig(void)
{
    int bad = 0, ok, k;
    static uint8_t nt[1024], n2[1024];
    static uint16_t at[1024], a2[1024];
    uint32_t n, m, i, p;
    track_t *t;
    static const uint8_t P[4] = {60, 62, 64, 65};
    ok = trig_passes(CC_NM, CNM_MAKE(4u, 4u), 16) == 0x8888u;
    bad += check("TRIG 4:4: the step plays on pass 4 of every 4 alone (the others as written)", ok);
    ok = trig_passes(CC_NM, CNM_MAKE(1u, 2u), 16) == 0x5555u && trig_passes(CC_NNM, CNM_MAKE(1u, 2u), 16) == 0xAAAAu;
    bad += check("TRIG 1:2: every other pass (the first); !1:2 the others", ok);
    rng_state = 0x1234567u;
    n = trig_run(CC_P50, 0, 1, 800, nt, 0, sizeof nt, CA_MUTE, CV_MUTE_TRIG);
    for (i = m = 0; i < n; i++) m += nt[i] == 60u;
    ok = n == 600u + m && m >= 70u && m <= 130u;
    if (!ok) printf("ui:   50%%: %u of 200 (%u notes)\n", m, n);
    bad += check("TRIG 50%: about half the passes", ok);
    {   /* LENGTH 3 at 1:2: pass 1 the four, pass 2 the fourth alone */
        static const uint8_t W[10] = {60, 62, 64, 65, 65, 60, 62, 64, 65, 65};
        n = trig_run(CC_NM, CNM_MAKE(1u, 2u), 3, 16, nt, 0, sizeof nt, CA_MUTE, CV_MUTE_TRIG);
        bad += check("TRIG 1:2 LENGTH 3: three steps gated by it", seq_is(nt, n, W, 10));
    }
    {   /* TRIG c = MUTE !c, note for note */
        static const uint8_t C[][3] = {{CC_NM, CC_NNM, 0x31}, {CC_NNM, CC_NM, 0x10}, {CC_FIRST, CC_NFIRST, 0},
                                       {CC_NFIRST, CC_FIRST, 0}, {CC_FILL, CC_NFILL, 0}, {CC_NFILL, CC_FILL, 0},
                                       {CC_ALWAYS, CC_ALWAYS, 0}};
        ok = 1;
        for (k = 0; k < (int)(sizeof C / sizeof C[0]); k++) {
            uint32_t l = 1u + (uint32_t)k % 3u;
            n = trig_run(C[k][0], C[k][2], l, 32, nt, at, sizeof nt, CA_MUTE, CV_MUTE_TRIG);
            m = C[k][0] == CC_ALWAYS ? trig_run(C[k][0], 0, l, 32, n2, a2, sizeof n2, CA_NONE, 0)
                                     : trig_run(C[k][1], C[k][2], l, 32, n2, a2, sizeof n2, CA_MUTE, 0);
            ok &= n == m && !memcmp(nt, n2, n) && !memcmp(at, a2, n * sizeof at[0]);
        }
        bad += check("TRIG = MUTE of the inverted condition (N:M, !N:M, 1ST, !1ST, FILL, !FILL; ALWAYS: as written)", ok);
    }
    seq_fill = 1;
    ok = trig_passes(CC_FILL, 0, 4) == 0xFu && !trig_passes(CC_NFILL, 0, 4);
    seq_fill = 0;
    ok &= !trig_passes(CC_FILL, 0, 4) && trig_passes(CC_NFILL, 0, 4) == 0xFu;
    ok &= trig_passes(CC_FIRST, 0, 8) == 1u && trig_passes(CC_NFIRST, 0, 8) == 0xFEu;
    bad += check("TRIG FILL / !FILL (FX held or not), 1ST / !1ST", ok);
    {   /* PRE: step 2's TRIG PRE plays when step 1's TRIG 1:2 held (PRE reads the condition, not the mute) */
        static const uint8_t W[6] = {60, 62, 64, 65, 64, 65};
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_MUTE, CC_NM, CNM_MAKE(1u, 2u), 1, CV_MUTE_TRIG);
        comp_set(&t->step[1], 0, CA_MUTE, CC_PRE, 0, 1, CV_MUTE_TRIG);
        seq_start();
        n = run(t, 8, nt, sizeof nt);
        bad += check("TRIG PRE: after a TRIG 1:2, the passes it played", seq_is(nt, n, W, 6));
    }
    {   /* MONO: C1 TRIG 1:2, C2 OCT+ ELSE: the pass it plays, an octave up (it did not mute); POLY (C2 ALWAYS): the same;
         * C1 OCT+ 1:2, C2 TRIG ELSE: the step plays the passes C1 did not act */
        static const uint8_t W[7] = {72, 62, 64, 65, 62, 64, 65}, W2[7] = {62, 64, 65, 60, 62, 64, 65};
        for (p = 0; p < 2u; p++) {
            t = pat(4, P);
            comp_set(&t->step[0], 0, CA_MUTE, CC_NM, CNM_MAKE(1u, 2u), 1, CV_MUTE_TRIG);
            comp_set(&t->step[0], 1, CA_OCTU, p ? CC_ALWAYS : CC_ELSE, 0, 1, 0);
            seq_start();
            n = run(t, 8, nt, sizeof nt);
            ok = p ? ok & seq_is(nt, n, W, 7) : seq_is(nt, n, W, 7);
        }
        t = pat(4, P);
        comp_set(&t->step[0], 0, CA_OCTU, CC_NM, CNM_MAKE(1u, 2u), 1, 0);
        comp_set(&t->step[0], 1, CA_MUTE, CC_ELSE, 0, 1, CV_MUTE_TRIG);
        seq_start();
        n = run(t, 8, nt, sizeof nt);
        ok &= seq_is(nt, n, W2, 7);
        bad += check("TRIG MONO / POLY: C2 OCT+ ELSE over a TRIG 1:2 (up when it plays), C2 TRIG ELSE (plays when C1 did not act)", ok);
    }
    {   /* the format: kept (MUTE, VALUE TRIG), and read back a TRIG */
        project_store_t st;
        ui_power_on();
        trk[2].step[7] = (step_t){{60}, 1, ST_NOTE, 0, 100};
        comp_set(&trk[2].step[7], 1, CA_MUTE, CC_NM, CNM_MAKE(4u, 4u), 3, CV_MUTE_TRIG);
        glane[5] = (gcomp_t){CO_MAKE(CA_MUTE, CC_NM), CV_MUTE_TRIG, 0, CNM_MAKE(2u, 2u), 9};
        gl_touch();
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        memset(&trk[2].step[7], 0, sizeof trk[2].step[7]);
        memset(glane, 0, sizeof glane);
        ok &= proj_import(&proj_scratch, &st, sizeof st) && !project_restore_runtime(&proj_scratch);
        ok &= trk[2].step[7].comp2 == CO_MAKE(CA_MUTE, CC_NM) && trk[2].step[7].carg2 == CV_MUTE_TRIG &&
              trk[2].step[7].clen[1] == 2u && trk[2].step[7].cnm[1] == CNM_MAKE(4u, 4u) &&
              glane[5].comp == CO_MAKE(CA_MUTE, CC_NM) && glane[5].val == CV_MUTE_TRIG && glane[5].mask == 9u;
        bad += check("TRIG FORMAT: a step's and the lane's round trip (MUTE, VALUE TRIG: no new ACTION code)", ok);
    }
    {   /* the UI: KNOB 1's last, "TRIG" on the card, CONDITION as any (4:4), no VALUE; GLO > COMPS too */
        ui_power_on();
        t = TSEL;
        t->step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
        go_page(GR_COMP); frame();
        turn(EN_K1, 4); turn(EN_K1, 4); turn(EN_K1, 4); turn(EN_K1, 4); turn(EN_K1, 4); frame();
        ok = comp_view() == CA_TRIG && t->step[0].comp == CO_MAKE(CA_MUTE, CC_ALWAYS) && t->step[0].carg == CV_MUTE_TRIG &&
             strstr(ui.col[0], "TRIG") != 0;
        for (i = 0; i < 20u && !(CO_COND(t->step[0].comp) == CC_NM && t->step[0].cnm[0] == CNM_MAKE(4u, 4u)); i++)
            turn(EN_K3, 1);
        frame();
        ok &= CO_ACT(t->step[0].comp) == CA_MUTE && t->step[0].carg == CV_MUTE_TRIG && strstr(ui.col[2], "4:4") != 0;
        turn(EN_K4, 1);
        ok &= t->step[0].carg == CV_MUTE_TRIG;
        turn(EN_K1, -1); frame();
        ok &= comp_view() == CA_CHORD && strstr(ui.col[0], "CHORD") != 0;
        turn(EN_K1, 1); turn(EN_K1, -4); turn(EN_K1, -4); turn(EN_K1, -1);
        ok &= comp_view() == CA_MUTE && t->step[0].carg == 0u;
        if (!ok) printf("ui:   comp %02x carg %u view %u col0 %s col2 %s\n", t->step[0].comp, t->step[0].carg, comp_view(), ui.col[0], ui.col[2]);
        bad += check("SEQ > COMP: TRIG KNOB 1's last (MUTE, VALUE TRIG), its card TRIG, CONDITION 4:4, no VALUE", ok);
        gl_clear();
        go_page(GR_GCOMP); frame();
        turn(EN_K1, 4); turn(EN_K1, 4); turn(EN_K1, 4); frame();
        ok = glane[0].comp == CO_MAKE(CA_MUTE, CC_ALWAYS) && glane[0].val == CV_MUTE_TRIG && strstr(ui.col[0], "TRIG") != 0;
        turn(EN_K3, 1); turn(EN_K4, 1);
        ok &= glane[0].comp == CO_MAKE(CA_MUTE, CC_P25) && glane[0].val == CV_MUTE_TRIG;
        bad += check("GLO > COMPS: TRIG its last", ok);
    }
    {   /* the global lane: TRIG 2:2 over 16 steps on T1: its hits on bar 2 of 2 alone */
        uint32_t hits[4] = {0};
        gl_blank();
        for (i = 0; i < 16u; i++) { trk[0].step[i] = (step_t){{0}, 0, ST_NOTE, 0, 100}; trk[0].step[i].hit = 1; }
        gl_set(0, CA_MUTE, CC_NM, CNM_MAKE(2u, 2u), 16, CV_MUTE_TRIG, 1);
        n = grun(4u * 16u);
        for (i = 0; i < n; i++)
            if (g_trk[i] == 0u && g_at[i] / 16u < 4u) hits[g_at[i] / 16u]++;
        ok = !hits[0] && hits[1] == 16u && !hits[2] && hits[3] == 16u;
        if (!ok) printf("ui:   hits %u %u %u %u\n", hits[0], hits[1], hits[2], hits[3]);
        bad += check("GLOBAL TRIG 2:2 LENGTH 16 on T1: its bar 2 of every 2 alone", ok);
        gl_clear();
    }
    return bad;
}

static int original_comp_main(void)
{
    int bad = cycle() + actions() + mono_poly() + format() + comp_ui() + trig();
    bad += gl_example() + gl_mono_poly() + gl_empty() + gl_format() + gl_ui();
    printf("%s\n", bad ? "COMP TEST FAILED" : "comp tests passed");
    return bad != 0;
}


#ifndef STEP_EXTENSIONS_TEST
int main(void) { return original_comp_main(); }
#endif
