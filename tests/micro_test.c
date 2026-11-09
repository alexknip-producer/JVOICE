/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.9.1) MICRO in 64ths of a step (core.h ST_MICRO: the sixteenth in cwhen, the 64ths above it in flags' SF_MFINE), live
 * recording off the grid (seq.c rec_note, REC Q), its playback (seq_tick: late by a queue, early by the step before
 * ending sooner), FUNC's block 'M' 'F' (project.c), the editor and SEQ > NOTE's knob, on the real sources (ui_test.c's).
 *   build/host/micro_test
 * 1. the value: every one of -32 .. +31 kept, the step's other bits untouched; what firmware before 0.9.1 reads of it (the
 *    sixteenth below); a step of before (its sixteenth, no 64ths): x 4 exactly.
 * 2. a project of before: steps with MICRO in sixteenths (early and late, a kit too) play sample for sample as 0.9.0 played
 *    them (a sequenced mix's hash from 0.9.0's sources, MICRO_OLD_090) and pack to 0.9.0's bytes (MICRO_PACK_090, no 'M'
 *    'F'); loaded, every MICRO its sixteenths x 4.
 * 3. live recording, REC Q 0 %: notes played early and late at 72 / 120 / 174 BPM on 1/8, 1/16, 1/32 and 16T steps (no
 *    swing; a sample's resolution), each recorded within 1/64 of a step of where it was played, into its own step;
 * 4. .. played back where they were recorded (the note-on within a sample of MICRO's place) and so within 1/64 of a step
 *    of where they were played; with SWING too.
 * 5. REC Q 50 %: half way to the grid (within a 64th of half the offset); 100 %: on it.
 * 6. saved and loaded: every value on 64 steps of a synth and a kit's, the block 'M' 'F' there; without it (what firmware
 *    before 0.9.1 reads: the rest of the project as it is) the sixteenth below.
 * 7. SEQ > NOTE's KNOB 4 a 64th a detent, -32 .. +31, "+5/64" (the editor's STEP_SET keeping a step's MICRO: editor_test.c). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("micro: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t mfnv(uint32_t h, const void *p, uint32_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}

/* ---- 2. a project of before: the steps' MICRO set as 0.9.0 stored it (cwhen's bits 4-7, sixteenths) ---- */
#define MICRO_OLD_090 0x6377849cu   /* (2026-11-09 bug review: 0xe7ee65d3 until the sounds and defaults since V10.6
                                         * (GATE 100 % for new tracks V10.6z4, the levelled presets V10.6t, V10.7's mix):
                                         * the steps' places, checked below, as before. 0.9.9 ctl64: the render in blocks of 64 differs from 0.9.0's bit for bit
                                         * (the notes at their samples, the fades); was 0xd5c5b5c5: the steps' places as read
                                         * from the project are unchanged, the second check below. Then 0x249e3fdc, rewritten
                                         * by 0.9.9-snd2's chorus (da051a3: 0xd8795d59) and gen6e's notes in a release
                                         * (b587793), the goldens they rewrote; the steps' places as before) */
#define MICRO_PACK_090 0xd49924b3u           /* (V10.7.6: 0x49120182 before FUNC's VEL mark, byte 65 'V'; was 0x55f3c173: the project's defaults since, e.g. GATE 100 % (V10.6z4);
                                         * still no 'M' 'F' block and every MICRO read back x 4, below) */
static void old_steps(void)
{
    static const int8_t M16[8] = {0, 3, -2, 7, -8, 1, -5, 4};
    uint32_t k, i;
    ui_power_on();
    song.master_q12 = 4096;
    for (k = 0; k < 3u; k++) {
        track_t *t = &trk[k];
        set_engine_of(t, k == 2u ? ENGI_DRUM : ENGI_ANALOG);
        t->engine = t->eng_req;
        apply_preset_to(t, 0);
        t->p[P_MUTE] = 0; t->p[P_SLEN] = 16; t->p[P_SDIV] = (int16_t)(k == 1u ? 3 : 2); t->p[P_SSWING] = (int16_t)(k * 20u);
        for (i = 0; i < 16u; i++) {
            step_t *s = &t->step[i];
            memset(s, 0, sizeof *s);
            s->time = ST_NOTE;
            s->vel = 100;
            if (k == 2u) s->hit = (uint8_t)(1u << (i % 3u));
            else { s->n = 1; s->note[0] = (uint8_t)(48 + 7 * k + i % 5u); }
            s->cwhen = (uint8_t)(((uint32_t)M16[(i + k) % 8u] & 15u) << 4);   /* (0.9.0's TIMING: sixteenths) */
        }
    }
}
static int32_t old_peak;
static uint32_t old_render(void)
{
    static int32_t o[2u * CTL];
    uint32_t b, i, h = 2166136261u;
    old_steps();
    transport_req = 1; ui_input();
    for (b = 0; b < 1800u; b++) {
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            h = (h ^ (uint32_t)o[i]) * 16777619u;
            if (o[i] > old_peak) old_peak = o[i];
        }
    }
    transport_req = 2; ui_input();
    return h;
}
static uint32_t old_pack(project_store_t *st, project_t *q)
{
    old_steps();
    project_capture(q);
    if (!proj_pack(st, q))
        return 0;
    return mfnv(2166136261u, st->raw, sizeof st->raw);
}

#ifndef MICRO_BASE_ONLY
static void test_micro_value(void)
{
    int32_t m, c, ok = 1, old = 1, ex = 1;
    for (m = MICRO_MIN; m <= MICRO_MAX; m++) {
        step_t s;
        memset(&s, 0, sizeof s);
        s.flags = SF_ACCENT | SF_SLIDE | (2u << SF_RATCH_SH);
        s.cwhen = 0x0Bu;                                /* (EVERY in the low bits) */
        ST_MICRO_SET(&s, m);
        ok &= ST_MICRO(&s) == m && (s.flags & ~SF_MFINE) == (SF_ACCENT | SF_SLIDE | (2u << SF_RATCH_SH)) &&
              (s.cwhen & 15u) == 0x0Bu;
        old &= ((int32_t)(int8_t)s.cwhen >> 4) == (m >= 0 ? m / 4 : -((-m + 3) / 4));   /* (0.9.0's ST_MICRO: the 16th below) */
    }
    ck("every value -32 .. +31 kept, accent / slide / RATCH / EVERY untouched", ok);
    ck("what firmware before 0.9.1 reads of it: the sixteenth at or below (floor of m / 4)", old);
    for (c = -8; c <= 7; c++) {
        step_t s;
        memset(&s, 0, sizeof s);
        s.cwhen = (uint8_t)(((uint32_t)c & 15u) << 4);
        ex &= ST_MICRO(&s) == 4 * c;
    }
    ck("a step of before (-8 .. +7 sixteenths, no 64ths): x 4 exactly", ex);
}

static void test_micro_old(void)
{
    project_store_t st;
    project_t q, r;
    uint32_t h = old_render(), hp, k, i, ok = 1;
    char what[120];
    snprintf(what, sizeof what, "a project of before (MICRO in sixteenths, early and late, a kit, swing): sample for sample 0.9.0's (%08x)", h);
    ck(what, h == MICRO_OLD_090 && old_peak > 2000);
    hp = old_pack(&st, &q);
    snprintf(what, sizeof what, "  saved: 0.9.0's bytes (%08x), no 'M' 'F'", hp);
    ck(what, hp == MICRO_PACK_090);
    ok = proj_import(&r, &st, sizeof st);
    for (k = 0; k < 3u; k++)
        for (i = 0; i < 16u; i++)
            ok &= ST_MICRO(&r.t[k].step[i]) == 4 * ((int32_t)(int8_t)q.t[k].step[i].cwhen >> 4) && !(r.t[k].step[i].flags & SF_MFINE);
    ck("  loaded: every MICRO its sixteenths x 4", ok);
}

/* ---- 3, 4, 5: live recording and its playback, a sample at a time ---- */
static const double OFF[6] = {-0.45, -0.21, -0.03, 0.02, 0.27, 0.49};   /* (of a step: before / after its place) */
static uint32_t slen(const track_t *t, uint32_t i) { return step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), i); }
static uint32_t gridp(const track_t *t, uint32_t i)     /* where step i (of the loop) begins on the grid */
{
    uint32_t k, p = 0;
    for (k = 0; k < i; k++)
        p += slen(t, k);
    return p;
}
static void tick_to(track_t *t, uint32_t *now, uint32_t at)
{
    while (*now < at) {
        uint32_t n = at - *now > 16u ? 16u : at - *now;
        seq_tick(t, n);
        *now += n;
    }
}
/* track 1 (ANALOG), LEN 8, at bpm / div / swing, REC Q hrq: notes 61.. played OFF[k] of a step from steps 1..6's places;
 * the MICROs recorded (m[k], in the step it went to: *into 1 when each in its own); the playback's note-ons, a sample
 * at a time: at[k], against the grid (the steps' places) */
static void rec_play(int32_t bpm, int32_t div, int32_t sw, int32_t hrq, int32_t *m, int *into, int32_t *at)
{
    track_t *t;
    uint32_t now = 0, k, i, v0;
    ui_power_on();
    t = &trk[0];
    track_defaults_steps(t);
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    t->p[P_SLEN] = 8; t->p[P_SDIV] = (int16_t)div; t->p[P_SSWING] = (int16_t)sw; song.g[G_SWING] = 0;
    t->p[P_HRQ] = (int16_t)hrq; t->p[P_VOICE] = V_POLY; t->p[P_SGATE] = 40;
    t->p[P_GLIDE] = 0;                                  /* (track 1's power-on sound since V10.6d glides: POLY + GLIDE slides each
                                                         * step into the next on the same voice, seq.c chord_auto; no new note-on) */
    song.g[G_BPM] = (int16_t)bpm;
    seq_start();
    song.rec = 1;
    for (k = 0; k < 6u; k++) {
        uint32_t g = gridp(t, k + 1u);
        int32_t when = (int32_t)g + (int32_t)(OFF[k] * (OFF[k] < 0 ? slen(t, k + 1u) : slen(t, k + 1u)));
        tick_to(t, &now, (uint32_t)when);
        input_on(t, 61 + k, 100);
        trk_note_off(t, 61 + k);
    }
    tick_to(t, &now, gridp(t, 8));
    song.rec = 0;
    *into = 1;
    for (k = 0; k < 6u; k++) {
        const step_t *s = &t->step[k + 1u];
        *into &= s->time == ST_NOTE && s->n == 1u && s->note[0] == 61 + k;
        m[k] = ST_MICRO(s);
    }
    seq_stop();
    for (k = 0; k < NVOICE; k++) t->v[k].active = 0;
    seq_start();                                        /* the playback: a sample at a time */
    for (k = 0; k < 6u; k++) at[k] = -1000000;
    now = 0;
    while (now < gridp(t, 8)) {
        v0 = vage;
        seq_tick(t, 1);
        for (; v0 < vage; v0++)
            for (k = 0; k < NVOICE; k++) {
                /* (voice.c voice_begin, ctl64: a note on a voice still sounding (the last one's release) is kept in
                 * pn_* until track_render, which this sample-by-sample run never calls: read the kept start too; the
                 * note was asked for here either way. Without it only the first note was ever seen) */
                const voice_t *v = &t->v[k];
                uint32_t nt = v->age == v0 + 1u ? v->note : (v->pn_flags & PN_START) && v->pn_age == v0 + 1u ? v->pn_note : 0u;
                if (v->active && nt >= 61 && nt <= 66 && at[nt - 61] == -1000000)
                    at[nt - 61] = (int32_t)now - (int32_t)gridp(t, nt - 60u);
            }
        now++;
    }
    seq_stop();
}
static void test_micro_rec(void)
{
    static const int32_t BPM[3] = {72, 120, 174}, DIV[4] = {1, 2, 3, 5};   /* 1/8 1/16 1/32 16T */
    static const char *const DN[4] = {"1/8", "1/16", "1/32", "16T"};
    int32_t m[6], at[6], b, d, k, into;
    double worst_rec = 0, worst_play = 0, worst_place = 0;
    int ok_into = 1;
    char what[160];
    for (b = 0; b < 3; b++)
        for (d = 0; d < 4; d++) {
            double e_rec = 0, e_play = 0, e_place = 0;
            track_t *t = &trk[0];
            rec_play(BPM[b], DIV[d], 0, 0, m, &into, at);
            ok_into &= into;
            for (k = 0; k < 6; k++) {
                double l = (double)slen(t, (uint32_t)k + 1u), want = OFF[k] * 64.0;
                double er = fabs(m[k] - want) / 64.0;                         /* (of a step) */
                double ep = fabs(at[k] - (double)m[k] * l / 64.0);            /* (samples: against MICRO's place) */
                double el = fabs(at[k] / l - OFF[k]);                         /* (of a step: against where it was played) */
                if (er > e_rec) e_rec = er;
                if (ep > e_play) e_play = ep;
                if (el > e_place) e_place = el;
            }
            if (e_rec > worst_rec) worst_rec = e_rec;
            if (e_play > worst_play) worst_play = e_play;
            if (e_place > worst_place) worst_place = e_place;
            if (e_rec > 1.0 / 64 + 1e-9 || e_play > 1.5 || e_place > 1.0 / 64 + 0.0005)
                printf("  %d BPM %s: recorded %d %d %d %d %d %d, note-ons %d %d %d %d %d %d (errors %.4f %.1f %.4f)\n", BPM[b],
                       DN[d], m[0], m[1], m[2], m[3], m[4], m[5], at[0], at[1], at[2], at[3], at[4], at[5], e_rec, e_play, e_place);
        }
    ck("REC Q 0 %: each note into its own step (early ones into the step after: theirs)", ok_into);
    snprintf(what, sizeof what, "  recorded within 1/64 of a step of where played, 72 / 120 / 174 BPM x 1/8 1/16 1/32 16T (worst %.4f)",
             worst_rec);
    ck(what, worst_rec <= 1.0 / 64 + 1e-9);
    snprintf(what, sizeof what, "  played back at MICRO's place, within a sample or so (worst %.1f samples)", worst_play);
    ck(what, worst_play <= 1.5);
    snprintf(what, sizeof what, "  so within 1/64 of a step of where they were played (worst %.4f of a step)", worst_place);
    ck(what, worst_place <= 1.0 / 64 + 0.0005);
    {   /* with SWING: the late / early places on the swung steps */
        double e = 0;
        track_t *t = &trk[0];
        rec_play(120, 2, 60, 0, m, &into, at);
        for (k = 0; k < 6; k++) {
            double l = (double)slen(t, (uint32_t)k + 1u), x = fabs(at[k] / l - OFF[k]);
            if (x > e) e = x;
        }
        snprintf(what, sizeof what, "  SWING 60: recorded and played within 1/64 of a step of where played (worst %.4f)", e);
        ck(what, into && e <= 1.0 / 64 + 0.0005);
    }
    {   /* REC Q 50 % (64 of 127): half way to the grid; 100 %: on it */
        int ok = 1, ok100 = 1;
        rec_play(120, 2, 0, 64, m, &into, at);
        for (k = 0; k < 6; k++)
            ok &= fabs(m[k] - OFF[k] * 64.0 * 63.0 / 127.0) <= 1.0;
        printf("  REC Q 50 %%: %d %d %d %d %d %d (of %.1f %.1f %.1f %.1f %.1f %.1f)\n", m[0], m[1], m[2], m[3], m[4], m[5],
               OFF[0] * 64, OFF[1] * 64, OFF[2] * 64, OFF[3] * 64, OFF[4] * 64, OFF[5] * 64);
        ck("REC Q 50 %: each half way to the grid (within a 64th)", into && ok);
        rec_play(120, 2, 0, 127, m, &into, at);
        for (k = 0; k < 6; k++)
            ok100 &= m[k] == 0 && abs(at[k]) <= 1;
        ck("REC Q 100 %: all on the grid, played there", into && ok100);
    }
}

/* ---- 6. saved and loaded ---- */
static void test_micro_store(void)
{
    project_store_t st;
    project_t q, r;
    uint32_t i, ok = 1, older = 1, k, sum;
    int32_t at = -1;
    ui_power_on();
    set_engine_of(&trk[3], ENGI_DRUM);
    trk[3].engine = trk[3].eng_req;
    for (i = 0; i < NSTEP; i++) {
        step_t *a = &trk[0].step[i], *d = &trk[3].step[i];
        memset(a, 0, sizeof *a); memset(d, 0, sizeof *d);
        a->time = d->time = ST_NOTE; a->vel = d->vel = 90;
        a->n = 1; a->note[0] = (uint8_t)(50 + i % 12u);
        d->hit = (uint8_t)(1u << (i % 8u));
        ST_MICRO_SET(a, MICRO_MIN + (int32_t)i);         /* (all 64 values) */
        ST_MICRO_SET(d, MICRO_MAX - (int32_t)i);
    }
    project_capture(&q);
    ok = proj_pack(&st, &q) && proj_import(&r, &st, sizeof st) && !project_restore_runtime(&r);
    for (i = 0; i < NSTEP; i++)
        ok &= ST_MICRO(&trk[0].step[i]) == MICRO_MIN + (int32_t)i && ST_MICRO(&trk[3].step[i]) == MICRO_MAX - (int32_t)i &&
              !memcmp(&trk[0].step[i], &q.t[0].step[i], sizeof(step_t)) && !memcmp(&trk[3].step[i], &q.t[3].step[i], sizeof(step_t));
    ck("saved and loaded: every value on a synth's 64 steps and a kit's, the steps byte for byte", ok);
    for (k = 68u; k + 2u < PROJC_PFX_OFF; k++)
        if (st.raw[k] == 'M' && st.raw[k + 1u] == 'F' && st.raw[k + 2u] == 9u) at = (int32_t)k;   /* (tracks 1 and 4) */
    ck("  the block 'M' 'F' (tracks 1 and 4) after the others", at > 0);
    if (at > 0) {                                       /* (as firmware before 0.9.1 reads it: the block not there) */
        st.raw[at] = st.raw[at + 1] = 0;
        sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
        memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
        older = proj_import(&r, &st, sizeof st);
        for (i = 0; i < NSTEP; i++) {
            int32_t a = MICRO_MIN + (int32_t)i, d = MICRO_MAX - (int32_t)i;
            older &= ST_MICRO(&r.t[0].step[i]) == (a >= 0 ? a / 4 : -((-a + 3) / 4)) * 4 &&
                     ST_MICRO(&r.t[3].step[i]) == (d >= 0 ? d / 4 : -((-d + 3) / 4)) * 4 && r.t[0].step[i].note[0] == q.t[0].step[i].note[0];
        }
        ck("  without it (what firmware before 0.9.1 reads): the sixteenth below, the rest as it is", older);
    }
}

/* ---- 7. SEQ > NOTE's knob ---- */
static void test_micro_edit(void)
{
    step_t *st;
    uint32_t i;
    int ok;
    ui_power_on();
    st = &trk[0].step[2];
    memset(st, 0, sizeof *st);
    st->time = ST_NOTE; st->n = 1; st->note[0] = 60; st->vel = 90;
    song.sel = 0;
    open_family(FAM_SEQ);
    for (i = 0; i < NPAGES && !(PAGES[i].scope == SC_STEP && PAGES[i].id[0] == 4u); i++)
        ;
    ui.page = (uint8_t)i; page_entered();
    ui.cursor = 2;
    ST_MICRO_SET(st, 4);
    turn(EN_K4, 1);
    ok = ST_MICRO(st) == 5;
    turn(EN_K4, -10);
    ok &= ST_MICRO(st) == 4 || ST_MICRO(st) == -5 || ST_MICRO(st) < 4;
    for (i = 0; i < 80u; i++) turn(EN_K4, -1);
    ok &= ST_MICRO(st) == MICRO_MIN;
    for (i = 0; i < 80u; i++) turn(EN_K4, 1);
    ok &= ST_MICRO(st) == MICRO_MAX;
    ck("SEQ > NOTE's KNOB 4: a 64th a detent, -32 .. +31 at the ends", ok);
    ST_MICRO_SET(st, 5);
    ui.force = 1; frame();
    ck("  its card: \"+5/64\"", str_eq(ui.col[3] + str_len("MICRO|"), "+5/64||") || !memcmp(ui.col[3], "MICRO|+5/64|", 12));
}

int main(void)
{
    test_micro_value();
    test_micro_old();
    test_micro_rec();
    test_micro_store();
    test_micro_edit();
    printf(bad ? "micro: %d FAILED\n" : "micro: all checks ok\n", bad);
    return bad != 0;
}
#else
int main(void)
{
    project_store_t st;
    project_t q;
    printf("%08x %08x\n", old_render(), old_pack(&st, &q));
    return 0;
}
#endif
