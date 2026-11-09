/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* RATCH, a step's ratchet (core.h SF_RATCH, seq.c seq_step / seq_ratchet), on the real sequencer, project, user
 * preset and UI sources (with the stubs of tests/ui_test.c):
 *   PLAY       x1..x4: that many note-ons in one pass of the step, in equal parts of it, each retriggered after its
 *              gate; chords and the DRUM grid's hits repeat whole; x1 slides and ties out as before, x2..x4 never;
 *              the chance is rolled once per pass for all its parts; swing; STOP and an edit to fewer end them.
 *   PROJECTS   FUN8 round trip of x1..x4 (bit 7 of the velocity and chance bytes), an x1 project packs those bytes
 *              as before, FUN7 images load x1 (FUN6: retired, V10.6s, refused).
 *   PRESETS    a user preset's note pattern keeps it (flags 8 | 16, UP_PUT too); a drum grid record has no room: x1.
 *   UI         SEQ > CHANCE KNOB 3 (RATCH x1..x4, clamped, the cursor step; dimmed on a REST, a TIE, an empty
 *              step); the piano roll and the DRUM grid draw a ratcheted step in its parts, the roll no slide line
 *              out of one.
 * The editor protocol (STEP_SET / TRACK_STEP, INFO 52 01 04) is in tests/editor_test.c. Built and run by
 * tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#define PROJ_FUNB_WRITER 1                               /* (JIANT 0.6) the byte checks below read a FUNB */
#include "ui_test.c"

/* track 1 alone, a one-step loop: step 0 NOTE 60 (flags), RATCH hits, GATE 64, no swing; the transport started */
static track_t *one_step(uint32_t hits, uint32_t flags)
{
    track_t *t;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    t->p[P_SLEN] = 1;
    t->p[P_SGATE] = 64;
    t->p[P_SSWING] = 0;
    song.g[G_SWING] = 0;
    t->step[0] = (step_t){{60}, 1, ST_NOTE, (uint8_t)flags, 100};
    step_set_ratchet(&t->step[0], hits);
    seq_start();
    return t;
}
static uint32_t step_len(const track_t *t, uint32_t idx) { return step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), idx); }

/* note-ons (voice starts) over `blocks` CTL blocks; at[k]: the sample of the k-th; rel: releases seen between */
static uint32_t run(track_t *t, uint32_t blocks, uint32_t *at, uint32_t nat, uint32_t *rel)
{
    uint32_t b, n = 0, v0, was = 0;
    for (b = 0; b < blocks; b++) {
        v0 = vage;
        seq_tick(t, CTL);
        for (; v0 < vage; v0++, n++)
            if (at && n < nat)
                at[n] = b * CTL;
        if (rel && was && !t->seq_n)
            (*rel)++;
        was = t->seq_n;
    }
    return n;
}
/* blocks in `passes` passes of a one-step loop, the last block before the next pass left out */
static uint32_t passes(const track_t *t, uint32_t n) { return n * step_len(t, 0) / CTL - 1u; }

static int play(void)
{
    int bad = 0, ok = 1;
    uint32_t h, n, at[8], rel, len;
    track_t *t;
    for (h = 1; h <= 4u; h++) {
        t = one_step(h, 0);
        rel = 0;
        n = run(t, passes(t, 1), at, 8, &rel);
        len = step_len(t, 0);
        ok &= n == h && rel >= h - 1u;
        for (uint32_t k = 0; k < n && k < 8u; k++) {   /* part k at k / h of the step, to the block */
            int32_t d = (int32_t)at[k] - (int32_t)(k * (len / h));
            ok &= d > -(int32_t)CTL && d <= (int32_t)CTL;
        }
        if (n != h)
            printf("ui:   RATCH x%u: %u note-ons in one pass\n", h, n);
    }
    bad += check("RATCH x1..x4: that many note-ons in a pass, in equal parts, each ended by its gate", ok);
    t = one_step(4, 0);
    bad += check("  x4 over 8 passes: 32 note-ons", run(t, passes(t, 8), 0, 0, 0) == 32u);
    t = one_step(3, 0);
    t->p[P_VOICE] = V_POLY;
    t->step[0].n = 3; t->step[0].note[1] = 64; t->step[0].note[2] = 67;
    bad += check("  a chord of 3 at x3: 9 note-ons", run(t, passes(t, 1), 0, 0, 0) == 9u);
    t = one_step(1, SF_SLIDE);
    run(t, 4, 0, 0, 0);
    ok = t->seq_hold == 1u;
    t = one_step(2, SF_SLIDE);
    run(t, passes(t, 1), 0, 0, 0);
    bad += check("  x1 slides out as before; x2 never (its last part ends at its gate)", ok && !t->seq_hold && !t->seq_n);
    t = one_step(4, 0);
    t->p[P_SLEN] = 2;
    t->step[1] = (step_t){{0}, 0, ST_TIE};
    seq_start();
    run(t, step_len(t, 0) / CTL + step_len(t, 1) / CTL - 1u, 0, 0, 0);
    bad += check("  nor ties out: a TIE after a ratcheted step holds nothing", !t->seq_n);

    t = one_step(4, 0);                               /* the DRUM grid: hits of BD (accented) and CH */
    set_engine_of(t, ENGI_DRUM);
    t->engine = t->eng_req;
    t->step[0] = (step_t){{0}, 0, ST_NOTE, 0, 100, 1u << DV_KICK | 1u << DV_HATC, 1u << DV_KICK};
    step_set_ratchet(&t->step[0], 3);
    seq_start();
    n = run(t, passes(t, 1), 0, 0, 0);
    ok = 0;
    for (h = 0; h < NVOICE; h++)
        ok |= t->v[h].note == DRUM_LANE_NOTE[DV_KICK] && t->v[h].vel == 127u;
    bad += check("  DRUM grid: BD + CH at x3, 6 hits, BD accented", n == 6u && ok);
    step_set_chance(&t->step[0], 0);
    seq_start();
    bad += check("  chance 0: no part plays", run(t, passes(t, 2), 0, 0, 0) == 0u);

    t = one_step(4, 0);                               /* chance: one roll for all four parts */
    step_set_chance(&t->step[0], 50);
    {   /* a pass starts where the step's position wraps (seq_pos below a block) */
        uint32_t heard = 0, p = 0, odd = 0, v0;
        n = 0;
        while (p < 400u) {
            v0 = vage;
            seq_tick(t, CTL);
            if (t->seq_pos < CTL) {                  /* (this block started a pass: count the one before) */
                odd += p && n != 0u && n != 4u;
                heard += p ? n : 0u;
                p++;
                n = 0;
            }
            n += vage - v0;
        }
        bad += check("  chance 50 % at x4: each pass all 4 parts or none, about half", !odd && heard > 4u * 140u &&
                     heard < 4u * 260u);
    }

    t = one_step(2, 0);                               /* swing: a step's parts stay inside its swung length */
    t->p[P_SLEN] = 2;
    t->p[P_SSWING] = SWING_MAX;
    t->step[1] = t->step[0];
    seq_start();
    n = run(t, (step_len(t, 0) + step_len(t, 1)) / CTL - 1u, at, 8, 0);
    bad += check("  SWING 100: x2 on a long and a short step, 4 note-ons, the short step's within it",
                 n == 4u && at[1] < step_len(t, 0) && at[2] >= step_len(t, 0) - CTL &&
                 at[3] - at[2] <= step_len(t, 1) / 2u + CTL);

    t = one_step(4, 0);
    run(t, 2, 0, 0, 0);
    ok = t->rat_left == 3u;
    seq_stop();
    bad += check("  STOP: no part left", ok && !t->rat_left && !t->seq_n);
    t = one_step(4, 0);
    run(t, 2, 0, 0, 0);
    step_set_ratchet(&t->step[0], 1);
    bad += check("  an edit to x1 while it plays ends the parts", run(t, passes(t, 1) - 2u, 0, 0, 0) == 0u && !t->rat_left);
    seq_stop();
    return bad;
}

static int projects(void)
{
    int bad = 0, ok = 1;
    project_t before, after;
    project_store_t packed;
    uint32_t i, k, pos;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    for (i = 0; i < 8u; i++) {
        t->step[i] = (step_t){{(uint8_t)(60 + i)}, 1, ST_NOTE, i & 1u ? SF_ACCENT : 0u, i & 2u ? 127u : 90u};
        step_set_ratchet(&t->step[i], 1u + i % 4u);
        step_set_chance(&t->step[i], i < 4u ? 100u : i == 4u ? 0u : 30u);
    }
    trk[3].step[5].hit = 0x81; trk[3].step[5].acc = 0x80; trk[3].step[5].time = ST_NOTE;
    step_set_ratchet(&trk[3].step[5], 4);
    project_capture(&before);
    ok = proj_pack(&packed, &before) && proj_import(&after, &packed, sizeof packed) && !memcmp(&before, &after, sizeof before);
    for (i = 0; ok && i < 8u; i++)
        ok &= step_ratchet(&after.t[0].step[i]) == 1u + i % 4u && step_chance(&after.t[0].step[i]) == step_chance(&before.t[0].step[i]) &&
              after.t[0].step[i].vel == before.t[0].step[i].vel;
    bad += check("FUN8: x1..x4 round trip with their velocity, accent, chance (0 and 30 %) and lane hits", ok &&
                 step_ratchet(&after.t[3].step[5]) == 4u && after.t[3].step[5].hit == 0x81);
    proj_pack_vb(&packed, &before);                  /* (FUNB: fixed places) */
    pos = 68u + P_COUNT + 2u + 3u * 9u;              /* step 3 of track 1: x4 -> bit 7 of its velocity and chance */
    bad += check("  x4 is bit 7 of the velocity and the chance bytes", (packed.raw[pos + 5] & 128u) && (packed.raw[pos + 8] & 128u) &&
                 (packed.raw[pos + 5] & 127u) == before.t[0].step[3].vel);
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NSTEP; i++)
            trk[k].step[i].flags &= (uint8_t)~SF_RATCH;
    project_capture(&before);
    proj_pack_vb(&packed, &before);
    ok = 1;
    for (k = 0, pos = 68u; k < PROJ_LT; k++, pos += P_COUNT + 2u + NSTEP * 9u)
        for (i = 0; i < NSTEP; i++)
            ok &= packed.raw[pos + P_COUNT + 2u + i * 9u + 5u] < 128u && packed.raw[pos + P_COUNT + 2u + i * 9u + 8u] < 128u;
    bad += check("  a project without ratchets: every velocity and chance byte below 128, as firmware before wrote", ok);
    {   /* a FUN7 image (its tracks without patches) as firmware before RATCH wrote it: x1 */
        project_store_t old;
        uint32_t sum, magic = PROJ_MAGIC_V7, size = PROJ_STORE_V7;
        project_t fun7;
        memcpy(&old, &packed, sizeof old);
        memcpy(old.raw, &magic, 4); memcpy(old.raw + 4, &size, 4);
        memset(old.raw + PROJ_STORE_V7 - 4u - PROJ_NAME_LEN, 0, PROJ_NAME_LEN);
        sum = proj_hash(old.raw, PROJ_STORE_V7 - 4u); memcpy(old.raw + PROJ_STORE_V7 - 4u, &sum, 4);
        bad += check("  FUN7 of before: refused (V10.7.13+: JIANT 0.1 .. 0.5's formats not read)",
                     !proj_import(&fun7, &old, PROJ_STORE_V7) && proj_old(&old, PROJ_STORE_V7));
    }
    {   /* FUN6 (10-byte steps): (V10.6s) retired, refused */
        project_v6_t v6;
        memset(&v6, 0, sizeof v6);
        v6.magic = PROJ_MAGIC_V6; v6.size = sizeof v6; v6.parts = NPART; v6.phys = PROJ_PHYS;
        memcpy(v6.g, before.g, sizeof v6.g);
        chain_defaults(&v6.chain);
        for (k = 0; k < PROJ_LT; k++) {
            for (i = 0; i < 69u; i++) v6.t[k].p[i] = TP[i < 61u ? i : i - 61u + P_E0].def;
            v6.t[k].step[0] = (step10_t){{60}, 1, ST_NOTE, 0xFBu, 100, 0, 0};   /* (stray high bits) */
        }
        v6.sum = proj_hash(&v6, sizeof v6 - 4u);
        bad += check("  FUN6 (retired): refused", !proj_import(&after, &v6, sizeof v6));
    }
    ui_power_on();                                   /* a real save and load */
    step_set_ratchet(&trk[1].step[7], 3);
    trk[1].step[7].time = ST_NOTE; trk[1].step[7].n = 1; trk[1].step[7].note[0] = 50;
    project_save(2);
    step_set_ratchet(&trk[1].step[7], 1);
    project_load(2);
    bad += check("  project save and load keep the ratchet", step_ratchet(&trk[1].step[7]) == 3u);
    return bad;
}

static int presets(void)
{
    int bad = 0;
    up_rec_t r;
    uint8_t a[400];
    uint32_t slot, i, k, n;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    for (i = 0; i < 16u; i++)
        t->step[i] = (step_t){{(uint8_t)(48 + i)}, 1, ST_NOTE, 0, 100};
    step_set_ratchet(&t->step[2], 3);
    t->step[5].flags = SF_ACCENT;
    step_set_ratchet(&t->step[5], 4);
    memset(&r, 0, sizeof r);
    up_pat_from(&r, t->step);
    track_defaults_steps(t);
    load_pat16(t, r.note, r.flags);
    bad += check("user preset note pattern: x3 and an accented x4 stored in its flags and loaded back",
                 step_ratchet(&t->step[2]) == 3u && step_ratchet(&t->step[5]) == 4u && t->step[5].flags & SF_ACCENT &&
                 step_ratchet(&t->step[0]) == 1u && r.flags[2] == 2u << SF_RATCH_SH);
    k = 0;                                           /* UP_PUT: slot, engine, name, values, 16 x (note, flags) */
    a[k++] = 3; a[k++] = (uint8_t)t->eng_req;
    memcpy(a + k, "RAT", 4); k += 4;
    for (i = 0; i < P_COUNT; i++) { a[k++] = (uint8_t)((t->p[i] + 8192) & 127); a[k++] = (uint8_t)((t->p[i] + 8192) >> 7); }
    for (i = 0; i < 16u; i++) { a[k++] = r.note[i]; a[k++] = r.flags[i]; }
    n = k;
    memset(&r, 0, sizeof r);
    bad += check("  UP_PUT keeps a pattern's ratchet", !up_parse(a, n, &r, &slot) && r.flags[2] == 2u << SF_RATCH_SH &&
                 r.flags[5] == (SF_ACCENT | 3u << SF_RATCH_SH));
    set_engine_of(t, ENGI_DRUM);                     /* a drum grid record: no room, x1 */
    t->engine = t->eng_req;
    track_defaults_steps(t);
    t->step[0] = (step_t){{0}, 0, ST_NOTE, 0, 100, 1u << DV_KICK};
    step_set_ratchet(&t->step[0], 2);
    up_store(4, "GRID");
    up_pat_load(t, 4);
    bad += check("  a DRUM grid record loads its hits x1", up_grid(up_rec(4)) && t->step[0].hit == 1u << DV_KICK &&
                 step_ratchet(&t->step[0]) == 1u);
    return bad;
}

/* pixels of colour c in card slot k (the cards' row) */
static uint32_t card_px(uint32_t k, uint16_t c)
{
    uint32_t n = 0;
    int32_t x, y;
    for (y = (int32_t)card_y(); y < (int32_t)(card_y() + card_h()); y++)   /* (0.8.5: the page's cards, a lens page's too) */
        for (x = CARD_X(k); x < CARD_X(k) + CARD_W; x++)
            n += swap16(host_screen[y * 240 + x]) == c;
    return n;
}
/* (0.8.5) a slide: the bar of step column i reaches into the next column's first px (its row: note n) */
static int pr_slide_reach(uint32_t i, int32_t n)
{
    return pr_is_bar((uint32_t)PR_CX(i + 1u), graph_y() + (uint32_t)pr_row_y(n) + 1u);
}

static int screens(void)
{
    int bad = 0, ok;
    track_t *t;
    uint32_t x, y;
    ui_power_on();
    t = TSEL;
    go_page(GR_CHANCE);
    ui.cursor = 0;
    turn(EN_K3, 2);
    ok = step_ratchet(&t->step[0]) == 3u && step_ratchet(&t->step[1]) == 1u;
    turn(EN_K3, 100);
    ok &= step_ratchet(&t->step[0]) == 4u;
    turn(EN_K3, -100);
    bad += check("SEQ > CHANCE KNOB 3: RATCH of the cursor step, x1..x4", ok && step_ratchet(&t->step[0]) == 1u &&
                 step_chance(&t->step[0]) == 100u);
    {                                                /* RATCH dimmed where it does nothing: REST, TIE, empty */
        uint32_t dim[4], thm[4], k;
        static const step_t kind[4] = {
            {{60}, 1, ST_NOTE, 0, 100}, {{0}, 0, ST_REST, 0, 0}, {{0}, 0, ST_TIE, 0, 0}, {{0}, 0, ST_NOTE, 0, 0}};
        for (k = 0; k < 4u; k++) {
            t->step[0] = kind[k];
            step_set_ratchet(&t->step[0], 2);
            memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
            dim[k] = card_px(2, T_DIM); thm[k] = card_px(2, T_TEXT);   /* (0.8.5: a value TEXT) */
        }
        t->step[0] = kind[3]; t->step[0].hit = 1u;   /* a NOTE step of lane hits only: it plays */
        step_set_ratchet(&t->step[0], 2);
        memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
        ok = card_px(2, T_DIM) == dim[0] && card_px(2, T_TEXT) == thm[0];
        for (k = 1; k < 4u; k++)
            ok &= dim[k] > dim[0] && thm[k] < thm[0];
        bad += check("  RATCH shows dimmed on a REST, a TIE or an empty step (a note or a hit: not)", ok);
        t->step[0] = (step_t){{0}, 0, ST_REST, 0, 0};
    }
    ui_power_on();                                   /* the piano roll: x2 in two bars, the middle column empty */
    t = TSEL;
    track_defaults_steps(t);
    t->p[P_SLEN] = 16;
    t->step[1] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    t->step[3] = t->step[1];
    step_set_ratchet(&t->step[1], 2);
    go_page(GR_ROLL); ui.cursor = 8; ui.bank = 0;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    y = graph_y() + (uint32_t)pr_row_y(60) + 1u;
    {   /* (V10.54: the bars' runs counted across the column, whatever its width: 13 px since 0.9.9-seq2b) */
        uint32_t c, runs[2] = {0, 0};
        for (c = 0; c < 2u; c++) {
            uint32_t x0 = (uint32_t)PR_CX(c ? 3 : 1), x1 = (uint32_t)PR_CX(c ? 4 : 2), on = 0;
            for (x = x0; x < x1; x++) {
                uint32_t b = (uint32_t)pr_is_bar(x, y);
                runs[c] += b && !on;
                on = b;
            }
        }
        bad += check("  the piano roll draws an x2 step in two parts (x1 whole)", runs[0] == 2u && runs[1] == 1u);
    }
    t->step[4] = (step_t){{64}, 1, ST_NOTE, 0, 100};   /* SLIDE into a note: the line, but not from x2 */
    t->step[3].flags = SF_SLIDE;
    t->step[1].flags |= SF_SLIDE;                     /* (x2 kept) */
    t->step[2] = t->step[4];
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    bad += check("  an x1 SLIDE step's bar reaches into the next column, an x2 one's not", pr_slide_reach(3, 60) &&
                 !pr_slide_reach(1, 60));
    t->step[1].flags &= (uint8_t)~SF_SLIDE; t->step[3].flags = 0;
    t->step[2] = t->step[4] = (step_t){{0}, 0, ST_REST, 0, 0};
    set_engine_of(t, ENGI_DRUM);                     /* the grid: x3 in three bars */
    t->engine = t->eng_req;
    t->step[1] = (step_t){{0}, 0, ST_NOTE, 0, 100, 1u};
    step_set_ratchet(&t->step[1], 3);
    ui.lane = 1;
    memset(host_screen, 0, sizeof host_screen); ui.force = 1; ui_draw();
    y = graph_y() + 19u + 2u + 5u;                   /* lane 0's hits: y 21 .. 30 of the panel (0.8.5: under LENTE's bars) */
    x = 40u + 12u;
    bad += check("  the DRUM grid draws an x3 hit in three parts", pr_is_bar(x, y) && !pr_is_bar(x + 2u, y) &&
                 pr_is_bar(x + 3u, y) && !pr_is_bar(x + 5u, y) && pr_is_bar(x + 6u, y));
    return bad;
}

/* (0.6.39) the step components (core.h CP_*, seq.c cp_arp_note / cp_trig_ok, SEQ > COMP): the notes the voices start
 * over `blocks` blocks, in order */
static uint32_t run_notes(track_t *t, uint32_t blocks, uint8_t *nt, uint32_t max)
{
    uint32_t b, n = 0, a, k;
    for (b = 0; b < blocks; b++) {
        uint32_t v0 = vage;
        seq_tick(t, CTL);
        for (a = v0 + 1u; a <= vage; a++)
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].age == a && n < max) nt[n++] = t->v[k].note;
    }
    return n;
}
/* (0.6.42) the second set: a pattern of LEN len with the notes nts (0: a REST), run for `steps` steps */
static track_t *pat(uint32_t len, const uint8_t *nts)
{
    track_t *t = one_step(1, 0);
    uint32_t i;
    t->p[P_SLEN] = (int16_t)len;
    t->p[P_ROOT] = 0; t->p[P_SCALE] = 1;                  /* C major */
    for (i = 0; i < len; i++) {
        t->step[i] = nts[i] ? (step_t){{nts[i]}, 1, ST_NOTE, 0, 100} : (step_t){{0}, 0, ST_REST, 0, 0};
    }
    seq_start();
    return t;
}
static uint32_t run_steps(track_t *t, uint32_t steps, uint8_t *nt, uint32_t max)
{
    return run_notes(t, steps * step_len(t, 0) / CTL - 1u, nt, max);
}
/* (0.6.82) two components a step, WHEN, DIV, LEN */
static uint32_t onsets(track_t *t, uint32_t *at, uint32_t max)   /* one step: the blocks its note-ons start on */
{
    uint32_t b, n = 0, k;
    for (b = 0; b + 1u < step_len(t, 0) / CTL; b++) {
        uint32_t v0 = vage;
        seq_tick(t, CTL);
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].age > v0 && n < max) { at[n++] = b; break; }
    }
    return n;
}
/* (0.6.82) a step's RATCH DIV and LEN (ctime; seq2d: the components are tests/comp_test.c) */
static int ratch_div_len(void)
{
    int bad = 0, ok;
    uint8_t nt[64];
    uint32_t n, at[8], nb;
    track_t *t;
    static const uint8_t P1[1] = {60};
    /* DIV: RATCH x4 at 1/2: two */
    nb = step_len(pat(1, P1), 0) / CTL;
    t = pat(1, P1); step_set_ratchet(&t->step[0], 4); t->step[0].ctime = 1;
    n = onsets(t, at, 8);
    ok = n == 2u;
    bad += check("DIV: RATCH x4 at 1/2 plays two", ok);
    /* LEN 4/16: the note off by half the step (the track's GATE holds it longer) */
    t = pat(1, P1); t->p[P_SGATE] = 120; t->step[0].ctime = 4u << 3;
    run_notes(t, nb / 2u, nt, sizeof nt);
    ok = t->seq_n == 0u;
    t = pat(1, P1); t->p[P_SGATE] = 120;
    run_notes(t, nb / 2u, nt, sizeof nt);
    ok &= t->seq_n == 1u;
    bad += check("LEN 4/16: the note's gate a quarter step (GATE 120 holds it on)", ok);
    return bad;
}

/* (0.6.87) SEQ > HUMAN: at 0 % (stored 127, every project before it) each note as written, on the step; VEL varies each
 * note's velocity (+-38 at 100 %), TIME starts it up to 30 ms late (a third of the step at most), never early, GATE
 * varies its length; a chord stays together */
static int human(void)
{
    int bad = 0;
    uint32_t pass, k;
    for (pass = 0; pass < 3u; pass++) {
        track_t *t = one_step(1, 0);
        uint32_t len = step_len(t, 0), b, n = 0, lo = 999999, hi = 0, vlo = 127, vhi = 0, cmax = len / 3u, early = 0;
        uint32_t cap = FS * 40u / 1000u < cmax ? FS * 40u / 1000u : cmax, apart = 0, lo_b = 0;   /* (0.6.92: 40 ms) */
        if (pass == 2u) { t->p[P_VOICE] = V_POLY; t->step[0].n = 3; t->step[0].note[1] = 64; t->step[0].note[2] = 67; }
        t->p[P_GLIDE] = 0;                              /* (the power-on sound's GLIDE, V10.6d: POLY + GLIDE slides the
                                                         * chord into the next step's on the same voices, seq.c chord_auto:
                                                         * no note-on to count) */
        t->p[P_HVEL] = pass ? 0 : 127;                  /* (100 %, 0 %) */
        t->p[P_HTIM] = pass ? 0 : 127;
        uint32_t st = 0;
        for (b = 0; b < 64u * len / CTL; b++) {
            uint32_t v0 = vage, first = 1;
            seq_tick(t, CTL);
            if (t->seq_pos < CTL) st = b;              /* (a step began in this block) */
            for (; v0 < vage; v0++) {
                uint32_t off = (b - st) * CTL, vel = 0;
                for (k = 0; k < NVOICE; k++)
                    if (t->v[k].active && t->v[k].age == v0 + 1u) vel = t->v[k].vel;
                if (off > len / 2u) early++;           /* (a note just before a step: early) */
                if (off < lo) lo = off;
                if (off > hi) hi = off;
                if (vel < vlo) vlo = vel;
                if (vel > vhi) vhi = vel;
                if (!first) apart += off != lo_b;
                lo_b = off;
                first = 0;
                n++;
            }
        }
        if (pass == 0u)
            bad += check("HUMAN 0 %: every note on its step, as written (velocity 100)", n >= 60u && hi == 0u && vlo == 100u && vhi == 100u);
        else if (pass == 1u) {
            printf("  human: start %u .. %u samples late (cap %u), velocity %u .. %u\n", lo, hi, cap, vlo, vhi);
            bad += check("HUMAN 100 %: notes up to 40 ms late (a third of a step), never early; velocities spread +-50",
                         n >= 60u && !early && hi > cap / 2u && hi <= cap + CTL && lo < cap / 3u && vlo >= 50u && vhi <= 127u &&
                         vhi - vlo > 40u);
        } else
            printf("  chord: n %u apart %u early %u\n", n, apart, early), bad += check("HUMAN 100 % on a chord: its notes together (one start), each its own velocity", n >= 180u && !apart && !early);
    }
    return bad;
}

/* (0.6.88) TIMING: a step's own place off the grid. Late (+n/64; 0.9.1, sixteenths before): its notes n 64ths of the step
 * after its place; early (-n/64): before it (the step before ends sooner, the one after keeps its place: the loop's
 * length as it was);
 * recorded live with REC Q under 100 %: the step keeps where the note was played (sooner than the middle of a step:
 * the next step, early); 100 %: on the grid; stored with the project */
static int timing(void)
{
    int bad = 0, ok;
    track_t *t;
    uint32_t b, k, len, at[64], n, st = 0;
    int32_t off[64];
    for (k = 0; k < 2u; k++) {                          /* (k 0: steps 1 and 3 late +4 / early -4; k 1: all on the grid) */
        ui_power_on();
        t = &trk[0];
        track_defaults_steps(t);
        t->p[P_SLEN] = 4; t->p[P_SGATE] = 32; t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
        for (b = 0; b < 4u; b++) t->step[b] = (step_t){{(uint8_t)(60 + b)}, 1, ST_NOTE, 0, 100};
        if (!k) { ST_MICRO_SET(&t->step[1], 16); ST_MICRO_SET(&t->step[3], -16); }   /* (0.9.1: 64ths, +4/16 -4/16) */
        seq_start();
        len = step_len(t, 0);
        n = 0;
        for (b = 0; b < 40u * len / CTL && n < 64u; b++) {
            uint32_t v0 = vage;
            seq_tick(t, CTL);
            for (; v0 < vage && n < 64u; v0++, n++) at[n] = b * CTL;
        }
        for (b = 0; b < n; b++) off[b] = (int32_t)at[b] - (int32_t)(b * len);   /* (note b on the grid: step b) */
        if (k) {
            ok = n >= 32u;
            for (b = 0; b < n; b++) ok &= off[b] > -(int32_t)CTL && off[b] < (int32_t)CTL;
            bad += check("TIMING 0: every step on the grid", ok);
        } else {
            int32_t q = (int32_t)len / 4;
            ok = n >= 32u;
            for (b = 0; b < n; b++) {
                int32_t want = b % 4u == 1u ? q : b % 4u == 3u ? -q : 0;
                ok &= off[b] > want - 2 * (int32_t)CTL && off[b] < want + 2 * (int32_t)CTL;
            }
            if (!ok) for (b = 0; b < 8u; b++) printf("  timing: note %u at %d (step %u)\n", b, off[b], len);
            bad += check("TIMING +16/64 and -16/64: the steps a quarter late / early, the rest on the grid, the loop's length kept", ok);
        }
    }
    {   /* live recording: a note played 5/16 into step 2 and one 3/16 before step 4, REC Q 0 % and 100 % */
        uint32_t pass;
        for (pass = 0; pass < 2u; pass++) {
            ui_power_on();
            t = &trk[0];
            track_defaults_steps(t);
            for (b = 0; b < NSTEP; b++) step_clear(&t->step[b]);
            t->p[P_SLEN] = 8; t->p[P_SSWING] = 0; song.g[G_SWING] = 0;
            t->p[P_HRQ] = pass ? 127 : 0;
            seq_start();
            song.rec = 1;
            len = step_len(t, 0);
            for (b = 0; b < 8u * len / CTL; b++) {
                uint32_t p0 = b * CTL, p1 = p0 + CTL;
                seq_tick(t, CTL);
                if (p0 <= len + 5u * len / 16u && p1 > len + 5u * len / 16u) { input_on(t, 64, 100); trk_note_off(t, 64); }
                if (p0 <= 3u * len - 3u * len / 16u && p1 > 3u * len - 3u * len / 16u) { input_on(t, 67, 100); trk_note_off(t, 67); }
            }
            song.rec = 0;
            if (pass)
                ok = t->step[1].note[0] == 64 && ST_MICRO(&t->step[1]) == 0 && t->step[3].note[0] == 67 && ST_MICRO(&t->step[3]) == 0;
            else {
                printf("  timing: recorded %d/64 (step 2, %u) and %d/64 (step 4, %u)\n", ST_MICRO(&t->step[1]), t->step[1].note[0],
                       ST_MICRO(&t->step[3]), t->step[3].note[0]);
                ok = t->step[1].note[0] == 64 && ST_MICRO(&t->step[1]) >= 19 && ST_MICRO(&t->step[1]) <= 21 &&   /* (0.9.1: 64ths,
                                                         * within one of 20 and -12 now, the block's sample apart) */
                     t->step[3].note[0] == 67 && ST_MICRO(&t->step[3]) >= -13 && ST_MICRO(&t->step[3]) <= -11;
            }
            bad += check(pass ? "  REC Q 100 %: recorded on the grid" : "  REC Q 0 %: recorded where played (+20/64 late, -12/64 early)", ok);
        }
    }
    {   /* stored with the project */
        project_t a, c;
        project_store_t p;
        ui_power_on();
        trk[0].step[2] = (step_t){{62}, 1, ST_NOTE, 0, 100};
        ST_MICRO_SET(&trk[0].step[2], -27);             /* (0.9.1: 64ths, -7/16 and a 64th more) */
        trk[0].p[P_HRQ] = 40;
        project_capture(&a);
        ok = proj_pack(&p, &a) && proj_import(&c, &p, sizeof p) && ST_MICRO(&c.t[0].step[2]) == -27 && c.t[0].p[P_HRQ] == 40;
        bad += check("  TIMING and REC Q kept with the project", ok);
    }
    (void)st;
    return bad;
}

/* (p1, main 0.9.18) a POLY note recorded into a ratcheted synth step: once, live and on the passes after, not rolled with
 * the step's own (its lm slot LM_ONCE); into a plain step: no mark */
static int poly_once(void)
{
    uint32_t n60 = 0, n64 = 0, n, k;
    uint8_t nt[32];
    int ok;
    track_t *t = one_step(2, 0);
    t->p[P_VOICE] = V_POLY;
    run(t, 2, 0, 0, 0);                                   /* (early in the step) */
    rec_note(t, 64, 100);
    rec_release(t, 64);
    ok = t->step[0].n == 2u && t->step[0].note[1] == 64u && step_lm_raw(&t->step[0], 1u) == LM_ONCE &&
         step_lm_raw(&t->step[0], 0u) == LM_NONE;
    seq_start();
    n = run_notes(t, passes(t, 1), nt, sizeof nt);
    for (k = 0; k < n; k++) { n60 += nt[k] == 60u; n64 += nt[k] == 64u; }
    if (n60 != 2u || n64 != 1u) printf("ui:   POLY overdub into x2: 60 x%u, 64 x%u\n", n60, n64);
    ok &= n60 == 2u && n64 == 1u;
    t = one_step(1, 0);
    t->p[P_VOICE] = V_POLY;
    run(t, 2, 0, 0, 0);
    rec_note(t, 64, 100);
    rec_release(t, 64);
    ok &= t->step[0].n == 2u && !step_lm_any(&t->step[0]);
    return check("  a POLY note recorded into an x2 step: once (60 x2, 64 x1); into an x1 step: no mark", ok);
}
int main(void)
{
    int bad = play() + poly_once() + projects() + presets() + screens() + ratch_div_len() + human() + timing();
    printf("%s\n", bad ? "RATCHET TEST FAILED" : "ratchet tests passed");
    return bad != 0;
}
