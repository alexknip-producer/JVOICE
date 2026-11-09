/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (0.9.1) the master DJ FILTER (src/fx.c djf_block, G_DJF: GLO > MIXER KNOB 4) through the real sources (ui_test.c's).
 *   build/host/djf_test
 * 1. centred: a four-part mix with its FX, sequenced, rendered with the knob in the middle: bit for bit 0.9.0's (the
 *    hash of the same render from 0.9.0's sources, DJF_CENTRE_090), and djf_block never runs (out of the chain).
 * 2. the response (sines through djf_block alone): +-1 within 0.5 dB at 1 kHz; each side at a quarter, a half and the whole
 *    of its range: a quarter / four times the cutoff passed / at least 18 dB down (12 dB an octave), the peak at the cutoff
 *    at most +2 dB; the cutoffs monotonic (LP down, HP up), the ends (LP ~70 Hz, HP ~6 kHz) at least 30 dB down 3 octaves
 *    away; left and right the same filter, apart.
 * 3. no click: a tone while the knob crosses the middle (LP -> 0 -> HP, a jump LP -30 -> HP +30, automation's jumps), and
 *    while it is swept end to end a detent a block: the output's second difference never past 4 x the tone's own (a hard
 *    switch, measured the same way, is far past it: the measure sees a click).
 * 4. automation: KNOB 4 turned while recording writes MOT_DJF records on the selected track; they play back at their
 *    steps, a stop brings the knob's value back; saved (FUNC: the main motion without them, block 'M' 'G' after the
 *    lyrics, so firmware before 0.9.1 loads the rest) and loaded back the same; AUTO LIST shows them; no lock.
 * 5. a project of before (G_DJF's id was G_NAME, always 0): the filter in the middle; a stored value out of range clamped. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("djf: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* ---- 1. centred: the render whose hash 0.9.0's sources give (the same code, built against them) ---- */
#define DJF_CENTRE_090 0xa884ede6u   /* (V10.7.6: 0x643f7d41 before FM6 BELL's REL 5, its tail in the mix; 2026-11-09 bug review: 0xe800cf98 until the sounds since V10.55 (presets that
                                         * move V10.6i, levelled V10.6t, DRUM-X V10.6q / z9, V10.7's mix, VEL); the
                                         * check that matters, djf_block never ran, below. V10.54: the factory sounds and the master since V10.42; 0.9.9 ctl64: the mix in blocks of 64; the point is the filter centred leaves it
                                         * untouched, checked against the same build's render) */
static int32_t centre_peak;
static uint32_t centre_render(void)
{
    static int32_t o[2u * CTL];
    uint32_t b, i, h = 2166136261u, k;
    ui_power_on();
    song.master_q12 = 4096;
    trk_live = NTRK;
    for (k = 0; k < 4u; k++) {
        track_t *t = &trk[k];
        set_engine_of(t, k == 3u ? ENGI_DRUM : k == 1u ? ENGI_FM6 : ENGI_ANALOG);
        t->engine = t->eng_req;
        apply_preset_to(t, k);
        t->p[P_MUTE] = 0; t->p[P_SLEN] = 16; t->p[P_DLY] = 50; t->p[P_REV] = 60; t->p[P_CHOR] = 30;
        for (i = 0; i < 16u; i++) {
            step_t *s = &t->step[i];
            memset(s, 0, sizeof *s);
            s->time = (i + k) % 3u ? ST_NOTE : ST_REST;
            s->vel = 100;
            if (k == 3u) s->hit = (uint8_t)(1u << (i % 4u));
            else { s->n = 2; s->note[0] = (uint8_t)(40 + 12 * k + i % 7u); s->note[1] = (uint8_t)(s->note[0] + 7u); }
        }
    }
    song.g[G_CLIP] = 30;
    transport_req = 1; ui_input();
    for (b = 0; b < 2400u; b++) {                       /* (~1.7 s) */
        memset(o, 0, sizeof o);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            h = (h ^ (uint32_t)o[i]) * 16777619u;
            if (o[i] > centre_peak) centre_peak = o[i];
        }
    }
    transport_req = 2; ui_input();
    return h;
}
#ifndef DJF_BASE_ONLY
static void test_centre(void)
{
    uint32_t h = centre_render();
    char what[120];
    snprintf(what, sizeof what, "centred: a sequenced four-part mix (peak %d) bit for bit 0.9.0's (%08x)", centre_peak, h);
    ck(what, h == DJF_CENTRE_090 && centre_peak > 2000);
    ck("centred: djf_block never ran (out of the chain)", !djf.mode && !djf.w);
}

/* ---- 2. the response ---- */
static double djf_hz(int32_t v)                         /* its cutoff (CUTOFF_HZ, between its points) */
{
    int32_t c = djf_cut(v), i = c >> 8;
    return i >= 127 ? CUTOFF_HZ[127] : CUTOFF_HZ[i] + (CUTOFF_HZ[i + 1] - CUTOFF_HZ[i]) * (c & 255) / 256.0;
}
/* a sine of hz (amplitude 40000 in the mix's scale, left only when right_off) through djf_block at v: the output's
 * level against the input's, dB; *rl: the right's (dB, against the input) */
static double djf_gain(int32_t v, double hz, int right_off, double *rl)
{
    uint32_t b, i, n = 0;
    double ph = 0, si = 0, so = 0, sr = 0;
    memset(&djf, 0, sizeof djf);
    song.g[G_DJF] = (int16_t)v;
    for (b = 0; b < 400u; b++) {
        for (i = 0; i < CTL; i++) {
            int32_t x = (int32_t)(40000.0 * sin(ph));
            ph += 2.0 * M_PI * hz / FS;
            mix_l[i] = x;
            mix_r[i] = right_off ? 0 : x;
            wet[i] = 0;
            if (b >= 100u) si += (double)x * x;
        }
        djf_block(CTL);
        if (b >= 100u)
            for (i = 0; i < CTL; i++, n++) {
                so += (double)mix_l[i] * mix_l[i];
                sr += (double)mix_r[i] * mix_r[i];
            }
    }
    if (rl) *rl = 10.0 * log10((sr + 1e-9) / si);
    return 10.0 * log10((so + 1e-9) / si);
}
static void test_response(void)
{
    static const int32_t V[6] = {-16, -32, -64, 16, 32, 63};
    char what[160];
    double g0, g1, gr;
    uint32_t k;
    int32_t v, mono = 1;
    g0 = djf_gain(-1, 1000, 0, 0);
    g1 = djf_gain(1, 1000, 0, 0);
    snprintf(what, sizeof what, "+-1 (the knob a detent off the middle): 1 kHz %.2f dB (LP), %.2f dB (HP): within 0.5 dB", g0, g1);
    ck(what, fabs(g0) < 0.5 && fabs(g1) < 0.5);
    for (k = 0; k < 6u; k++) {
        double fc = djf_hz(V[k]), lp = V[k] < 0, pass, stop, peak;
        pass = djf_gain(V[k], lp ? fc / 4 : fc * 4, 0, 0);
        stop = fc * 4 < 18000 && fc / 4 > 15 ? djf_gain(V[k], lp ? fc * 4 : fc / 4, 0, 0) : -99;
        peak = djf_gain(V[k], fc, 0, 0);
        snprintf(what, sizeof what, "%s %+d: cutoff %.0f Hz; %s %.2f dB, %s %.1f dB, at the cutoff %+.2f dB", lp ? "LP" : "HP", V[k], fc,
                 lp ? "fc/4" : "4fc", pass, lp ? "4fc" : "fc/4", stop, peak);
        ck(what, fabs(pass) < 1.0 && stop < -18.0 && peak < 2.0 && peak > -4.0);
    }
    for (v = -63; v <= 63; v++)                         /* LP down as the knob goes left, HP up as it goes right */
        if (v && v != 1 && djf_cut(v) <= djf_cut(v - 1)) mono = 0;
    ck("the cutoffs: LP lower a detent left, HP higher a detent right, all the way", mono && djf_cut(-1) > djf_cut(-64) && djf_cut(63) > djf_cut(1));
    g0 = djf_gain(-64, djf_hz(-64) * 8, 0, 0);
    g1 = djf_gain(63, djf_hz(63) / 8, 0, 0);
    snprintf(what, sizeof what, "the ends: LP -64 (%.0f Hz) %.1f dB 3 octaves above, HP 63 (%.0f Hz) %.1f dB 3 below", djf_hz(-64), g0,
             djf_hz(63), g1);
    ck(what, djf_hz(-64) < 90 && djf_hz(-64) > 50 && djf_hz(63) > 5000 && djf_hz(63) < 7500 && g0 < -30.0 && g1 < -30.0);
    g0 = djf_gain(-24, 500, 0, &gr);
    g1 = djf_gain(-24, 500, 1, 0);
    {
        double r1;
        (void)djf_gain(-24, 500, 1, &r1);
        snprintf(what, sizeof what, "stereo: left and right the same filter (%.3f / %.3f dB), apart (the right silent: %.0f dB)", g0, gr, r1);
        ck(what, fabs(g0 - gr) < 0.001 && fabs(g0 - g1) < 0.001 && r1 < -150.0);
    }
    song.g[G_DJF] = 0;
    memset(&djf, 0, sizeof djf);
}

/* ---- 3. no click ---- */
/* a 330 Hz tone (40000) through djf_block for nb blocks, the knob at knob(b) each block; the largest second difference of
 * the output (left) from block 4 on; hard: the filter's share switched at once (a click, to see the measure catch it) */
static double tone_d2(int32_t (*knob)(uint32_t), uint32_t nb, int hard)
{
    static double ph;
    int32_t p1 = 0, p2 = 0;
    double m = 0;
    uint32_t b, i;
    memset(&djf, 0, sizeof djf);
    ph = 0;
    for (b = 0; b < nb; b++) {
        song.g[G_DJF] = (int16_t)knob(b);
        for (i = 0; i < CTL; i++) {
            mix_l[i] = mix_r[i] = (int32_t)(40000.0 * sin(ph));
            ph += 2.0 * M_PI * 330.0 / FS;
            wet[i] = 0;
        }
        if (hard && djf.mode && djf.w)
            djf.w = 4096;                               /* (no fade) */
        if (song.g[G_DJF] || djf.mode)
            djf_block(CTL);
        for (i = 0; i < CTL; i++) {
            int32_t y = mix_l[i], d2 = y - 2 * p1 + p2;
            if (b >= 4u && fabs((double)d2) > m) m = fabs((double)d2);
            p2 = p1;
            p1 = y;
        }
    }
    song.g[G_DJF] = 0;
    memset(&djf, 0, sizeof djf);
    return m;
}
static int32_t k_cross(uint32_t b) { return b < 100u ? -10 : b < 200u ? 0 : b < 300u ? 10 : b < 400u ? 0 : -10; }
static int32_t k_jump(uint32_t b) { return b < 100u ? -30 : b < 200u ? 30 : b < 300u ? -30 : 30; }
static int32_t k_sweep(uint32_t b) { return b < 128u ? -64 + (int32_t)b : b < 256u ? 63 - (int32_t)(b - 128u) : -64; }
static int32_t k_auto(uint32_t b) { return (b / 23u) % 4u == 0u ? -40 : (b / 23u) % 4u == 1u ? 25 : (b / 23u) % 4u == 2u ? 0 : -5; }
static int32_t k_hard(uint32_t b) { return b < 50u ? 0 : 50; }
static void test_clicks(void)
{
    double tone = 40000.0 * pow(2.0 * M_PI * 330.0 / FS, 2.0), lim = 4.0 * tone, c, j, s, a, h;
    char what[160];
    c = tone_d2(k_cross, 500, 0);
    j = tone_d2(k_jump, 400, 0);
    s = tone_d2(k_sweep, 400, 0);
    a = tone_d2(k_auto, 400, 0);
    h = tone_d2(k_hard, 100, 1);
    snprintf(what, sizeof what, "LP -> middle -> HP -> middle -> LP: second difference %.0f (the tone's %.0f, at most x4)", c, tone);
    ck(what, c < lim);
    snprintf(what, sizeof what, "a jump LP -30 <-> HP +30 (through the middle, faded): %.0f", j);
    ck(what, j < lim);
    snprintf(what, sizeof what, "swept end to end a detent a block (no zipper): %.0f", s);
    ck(what, s < lim);
    snprintf(what, sizeof what, "automation's jumps (-40, +25, 0, -5 every 23 blocks): %.0f", a);
    ck(what, a < lim);
    snprintf(what, sizeof what, "(the measure: the HP at +50 switched in at once is a click: %.0f, past x4)", h);
    ck(what, h > lim);
}

/* ---- 4. automation ---- */
static void test_automation(void)
{
    track_t *t;
    project_t q, r;
    project_store_t st;
    uint32_t i, n = 0, k, main_n;
    int32_t mg = -1, pos;
    ui_power_on();
    t = &trk[0];
    song.sel = 0;
    song.g[G_DJF] = 5;                                  /* the knob before */
    go_title("MANO");                                   /* (V10.45: LIVE > MANO KNOB 4; MIXER's is BPM since V10.37) */
    song.rec = 1;
    seq_start();
    seq_tick(t, CTL);                                   /* step 1 */
    motion_djf_set(t, -20);
    for (i = 0; i < 4u; i++) {                          /* (to step 5: 4 steps of a 1/16 at 120) */
        uint32_t s = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx);
        seq_tick(t, s / 2u);
        seq_tick(t, s - s / 2u);
    }
    turn(EN_K4, 3);                                     /* KNOB 4, recording */
    for (i = 0; i < motion.count; i++)
        n += MOTION_ID(&motion.event[i]) == MOT_DJF && MOTION_TRK(&motion.event[i]) == 0u;
    ck("recording: the knob writes DJ FILTER records (MOT_DJF) on the selected track", n == 2u && song.g[G_DJF] == -17);
    seq_stop();
    ck("stop: the knob's value before the recording back (5)", song.g[G_DJF] == 5);
    seq_start();
    seq_tick(t, CTL);
    ck("playback: step 1's record sets it (-20)", song.g[G_DJF] == -20);
    for (i = 0; i < 4u; i++) {
        uint32_t s = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx);
        seq_tick(t, s);
    }
    ck("playback: step 5's (-17)", song.g[G_DJF] == -17);
    project_capture(&q);
    ck("a save keeps the knob's value (5), not the records'", q.g[G_DJF] == 5);
    seq_stop();
    ck("stop again: 5", song.g[G_DJF] == 5);
    ck("no lock for it (the editor's MOTION op 5 refused)", motion_set_lock(t, 3, MOT_DJF, 10) == 1 && motion_param(MOT_DJF));
    ck("AUTO LIST lists it, named", ev_id_ok(t, MOT_DJF) && str_eq(track_desc(t, MOT_DJF)->label, "DJ FILT"));
    (void)motion_set_event(t, 7, P_REV, 90);            /* (another record, a track parameter's) */
    project_capture(&q);
    ck("packed (FUNC)", proj_pack(&st, &q));
    {   /* the main motion holds only the track parameters' records; the block 'M' 'G' after the lyrics holds the DJ FILTER's */
        const uint8_t *b = st.raw;
        pos = 68;
        for (k = 0; k < NTRK; k++) {                     /* (walk the tracks as proj_unpack_c does, to the motion) */
            uint32_t p, np = b[66], nb = 0;
            pos += 2;
            if (b[pos++]) pos += FM6_PACKED;
            p = (uint32_t)pos; pos += (int32_t)((np + 7u) / 8u);
            for (i = 0; i < np; i++) nb += (b[p + i / 8u] >> (i & 7u)) & 1u;
            pos += (int32_t)nb;
            p = (uint32_t)pos; pos += 8;
            for (i = 0; i < NSTEP; i++)
                if ((b[p + i / 8u] >> (i & 7u)) & 1u) {
                    uint32_t meta = b[pos++], x = 0, nn = meta & 7u;
                    if (meta & 128u) { x = b[pos++]; if (x & 8u) nn = 4u; }
                    pos += (int32_t)(nn + 1u + (x & 1u ? 2u : 0u) + (x & 2u ? 1u : 0u) + (x & 16u ? 2u : 0u) + (x & 32u ? 4u : 0u) +
                                     (x & 64u ? 3u : 0u));
                }
        }
        pos += (int32_t)sizeof q.chain;
        main_n = b[pos];
        for (i = 0; i < main_n; i++)
            if ((b[pos + 4 + 4 * (int32_t)i + 1] & 0x7Fu) == MOT_DJF) main_n = 999u;
        for (k = (uint32_t)pos; k + 2u < PROJ_STORE_SIZE - 200u; k++)
            if (b[k] == 'M' && b[k + 1u] == 'G' && b[k + 2u] == 2u) { mg = (int32_t)k; break; }
        ck("FUNC: the main motion without the DJ FILTER's records (firmware before reads it), 'M' 'G' 2 after it",
           main_n == 1u && mg > pos);
    }
    ck("loaded back: the same records, the knob's value", proj_import(&r, &st, sizeof st) && r.motion.count == q.motion.count &&
       r.g[G_DJF] == 5 && proj_djf_events(&r.motion) == 2u);
    {
        uint32_t same = 1;
        for (i = 0; i < q.motion.count; i++) {
            int found = 0;
            for (k = 0; k < r.motion.count; k++)
                found |= !memcmp(&q.motion.event[i], &r.motion.event[k], sizeof q.motion.event[0]);
            same &= (uint32_t)found;
        }
        ck("loaded back: every record as it was", same);
    }
    song.rec = 0;
    motion_clear(t);
    ck("cleared: the knob's value", song.g[G_DJF] == 5 && !proj_djf_events(&motion));
}

/* ---- 5. a project of before ---- */
static void test_old(void)
{
    project_t q, r;
    project_store_t st;
    uint32_t sum;
    int16_t v = 500;
    ui_power_on();
    song.g[G_DJF] = 0;
    project_capture(&q);
    ck("a project with the filter in the middle stores 0 where 0.9.0 kept G_NAME's 0 (header id 17), no 'M' 'G'",
       proj_pack(&st, &q) && !st.raw[8 + 2 * G_DJF] && !st.raw[9 + 2 * G_DJF] && !proj_djf_events(&q.motion));
    song.g[G_DJF] = 40;
    ck("loaded (0.9.0's bytes): the filter in the middle", proj_import(&r, &st, sizeof st) && !project_restore_runtime(&r) &&
       song.g[G_DJF] == 0);
    memcpy(st.raw + 8 + 2 * G_DJF, &v, 2);              /* (a value no firmware wrote: clamped) */
    sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
    memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
    ck("a value out of range there: clamped (63)", proj_import(&r, &st, sizeof st) && !project_restore_runtime(&r) &&
       song.g[G_DJF] == 63);
    song.g[G_DJF] = 0;
}

int main(void)
{
    test_centre();
    test_response();
    test_clicks();
    test_automation();
    test_old();
    printf(bad ? "djf: %d FAILED\n" : "djf: all checks ok\n", bad);
    return bad != 0;
}
#else
int main(void)
{
    printf("%08x\n", centre_render());
    return 0;
}
#endif
