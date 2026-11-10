/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The reverb bus's two models (src/fx.c: REVERB TYPE, G_RTYPE) on the Mac, through hostsim.c as regress.c.
 *   build/host/reverb_test [DEMODIR]          (run_tests.sh: build/fx_demo)
 * 1. ROOM bit-identical: fx_buses against a copy of the buses as they were before SPRING (chorus, delay and the
 *    4-comb room in one loop), on noise sends with SIZE / DAMP / the delay and chorus settings changing, sample
 *    for sample. (The goldens of regress.c, all rendered with ROOM, say the same for the whole mix.)
 * 2. SPRING decay: the impulse response's RT60 (Schroeder integral, -5 .. -35 dB) rises with SIZE, within
 *    0.15 .. 1 s at SIZE 0 and 2 .. 6 s at 127.
 * 3. SPRING dispersion: the group delay of the first arrival rises with frequency (1 .. 5 kHz, each band later
 *    than the one below, 5 kHz at least 2 ms after 1 kHz; 500 Hz printed: the loop's low cut delays it a
 *    little), and the second arrival (one more pass round the loop) is more spread than the first: the
 *    chirp grows with each echo.
 * 4. SPRING stable at the corners: SIZE 127 with DAMP 0 and 127, 2 s of full-scale noise or square waves (the
 *    sum of four tracks' largest sends) then silence: bounded, and the tail dies away (no limit cycle, no
 *    offset).
 * 5. level: SPRING's tail within 6 dB of ROOM's (RMS of the first 1.5 s after a noise burst, defaults).
 * 6. a model change while the bus rings: no click (the old one's block fades out), the new one starts silent; (0.8.4)
 *    every change between any two models and through a third, PRE on with ROOM and SPRING: the new one starts silent,
 *    never playing what another left in the line they share (fx.c rev_x: CLOUD's, SHIMMER's, the pre-delay's).
 * 7. cost: host instructions per sample of the reverb alone (rev_room, rev_spring): SPRING at most ROOM + 80 % (0.6.83:
 *    was + 30 %; a Mac's clang measured + 72 % while the FM-1's own code has SPRING cheaper than ROOM, tests/
 *    target_budget.txt rev_spring 127 against rev_room 218: the host ratio is the compiler's, the device's is watched
 *    there and by tests/cpu_vg.py);
 *    and of the whole bus stage (fx_buses), with the device estimate (1.7 % per 100, drum_test's ratio).
 * 8. (JIANT 0.3) the GRAIN delay's PITCH (an octave up / down) and SPRY (bounded with feedback), the reverb's FILT
 *    and WIDE, MOD's depth (~5.7 ms), DUCK's depth and hold.
 * 9. PRE: a plain delay at the full rate, no alias (src/fx.c: the half-rate line folded the send's highs down).
 * Demos (WAV) into DEMODIR: a drum pattern and a pluck through SPRING, the drums through ROOM to compare. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("reverb: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
#endif
    return 0;
}
static uint32_t xs = 0x1234567u;
static int32_t noise(int32_t amp)
{
    xs ^= xs << 13;
    xs ^= xs >> 17;
    xs ^= xs << 5;
    return (int32_t)(((int64_t)(int32_t)xs * amp) >> 31);
}

/* ------------------------------------------------- the buses before SPRING --- */
static int16_t ref_dly[DLY_LEN], ref_cho[CHO_LEN], ref_comb[1116 + 1188 + 1277 + 1356], ref_ap[556 + 441];
static struct {
    uint32_t dly_w, cho_w, cho_ph;
    int32_t dly_lp, dly_hp, wm;          /* (0.6.86) the loop's low cut; the tape's drift (Q8) */
    uint32_t wph, fph;
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    uint32_t dl_now, dl_old, dl_xf;      /* (0.6.81) TIME's crossfade; DEPTH and MIX gliding (fx.c) */
    int32_t cd, dm;
} rf;
static int32_t ref_rd(uint32_t d, int32_t m) { return dly_rd_of(ref_dly, rf.dly_w, d, m); }
static void ref_buses(const int32_t *cho_in, const int32_t *dly_in, const int32_t *rev_in, int32_t *wet, uint32_t n)
{
    uint32_t i, k, dl = delay_samples();
    int32_t fb = dly_fb(song.g[G_DFDBK]), tn = song.g[G_DCOLOR] - 64;   /* (JIANT 0.4 TONE, its low-pass half) */
    int32_t hk = 5, m0, dwm, mw;                                /* (0.6.89: the loop's DC block only; TONE 64 here) */
    (void)tn;
    rf.wph += (uint32_t)(0.6 * 4294967296.0 / FS) * n;
    rf.fph += (uint32_t)(5.3 * 4294967296.0 / FS) * n;
    mw = ((osc_sine(rf.wph) * song.g[G_DMOD]) >> 12) + ((osc_sine(rf.fph) * song.g[G_DMOD]) >> 15);
    m0 = rf.wm; dwm = (mw - rf.wm) / (int32_t)n; rf.wm = m0 + dwm * (int32_t)n;
    int32_t dmix = song.g[G_DMIX] * 258;
    int32_t size = 25000 + song.g[G_RSIZE] * 40 + song.g[G_RSIZE] * song.g[G_RSIZE] / 7, damp = 32767 - song.g[G_RDAMP] * 200;
    int32_t cdepth = song.g[G_CDEPTH] * 6;
    uint32_t cinc = LFO_INC[song.g[G_CRATE] & 127] / CTL;
    int32_t dcd = fx_glide(rf.cd, cdepth, n), ddm = fx_glide(rf.dm, dmix, n);
    if (!rf.dl_now) { rf.dl_now = dl; rf.cd = cdepth; rf.dm = dmix; }   /* (the first block: fx.primed) */
    if (!rf.dl_xf && dl != rf.dl_now) { rf.dl_old = rf.dl_now; rf.dl_now = dl; rf.dl_xf = DXF_LEN; }
    int32_t cr[3], cdr[3] = {0, 0, 0};                          /* (0.9.9-snd2: the chorus's LFO a segment, fx.c) */
    uint32_t clg = song.g[G_CRATE] < 64 ? 6u : song.g[G_CRATE] < 96 ? 4u : song.g[G_CRATE] < 120 ? 3u : 2u;
    cho_lfo(rf.cho_ph, rf.cd, cr);
    for (i = 0; i < n; i++) {
        int32_t y = 0, x, r, a;
        rf.cd += dcd; rf.dm += ddm;
        ref_cho[rf.cho_w & (CHO_LEN - 1u)] = (int16_t)clamp(cho_in[i] >> 1, -32768, 32767);
        rf.cho_ph += cinc;
        {                                                       /* (0.5.1: the chorus is the ensemble, its middle) */
            int32_t sd;
            if (!(i & ((1u << clg) - 1u))) {
                int32_t ce[3];
                cho_lfo(rf.cho_ph - cinc + (cinc << clg), rf.cd - dcd + dcd * (1 << clg), ce);
                for (k = 0; k < 3u; k++) cdr[k] = (ce[k] - cr[k]) >> clg;
            }
            for (k = 0; k < 3u; k++) cr[k] += cdr[k];
            y += cho_ens(ref_cho, rf.cho_w, cr, &sd);
            (void)r;
        }
        rf.cho_w++;
        m0 += dwm;
        x = ref_rd(rf.dl_now, m0);
        if (rf.dl_xf) {
            int32_t xo = ref_rd(rf.dl_old, m0);
            x += ((xo - x) * (int32_t)rf.dl_xf) >> DXF_LOG2;
            rf.dl_xf--;
        }
        rf.dly_lp = x;
        rf.dly_hp += (((x << 8) - rf.dly_hp) * hk + 16384) >> 15;
        ref_dly[rf.dly_w & (DLY_LEN - 1u)] = (int16_t)dly_sat((dly_in[i] >> 1) + mulq15(clamp(x - ((rf.dly_hp + 128) >> 8), -32767, 32767), fb));
        rf.dly_w++;
        y += mulq15(x << 1, rf.dm);
        a = 0;
        {
            int16_t *c = ref_comb;
            int32_t in = mulq15(rev_in[i], 2580);
            for (k = 0; k < 4u; k++) {
                int32_t o = c[rf.comb_i[k]];
                rf.comb_lp[k] = o + mulq15(rf.comb_lp[k] - o, 32767 - damp);
                c[rf.comb_i[k]] = (int16_t)clamp(in + mulq15(rf.comb_lp[k], size), -32768, 32767);
                if (++rf.comb_i[k] >= REV_COMB[k])
                    rf.comb_i[k] = 0;
                a += o;
                c += REV_COMB[k];
            }
            c = ref_ap;
            for (k = 0; k < 2u; k++) {
                int32_t o = c[rf.ap_i[k]];
                int32_t v = a + (o >> 1);
                c[rf.ap_i[k]] = (int16_t)clamp(v, -32768, 32767);
                a = o - a;
                if (++rf.ap_i[k] >= REV_AP[k])
                    rf.ap_i[k] = 0;
                c += REV_AP[k];
            }
        }
        y += a;
        wet[i] = y;
    }
}

static void test_room_identical(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], w0[CTL], w1[CTL];
    uint32_t b, i, diff = 0, nb = 30u * FS / CTL;
    host_tracks_init();
    for (b = 0; b < nb; b++) {
        if (b % 700u == 0u) {                                   /* the settings move now and then */
            song.g[G_RSIZE] = (int16_t)((uint32_t)noise(1 << 30) % 128u);
            song.g[G_RDAMP] = (int16_t)((uint32_t)noise(1 << 30) % 128u);
            song.g[G_DTIME] = (int16_t)((uint32_t)noise(1 << 30) % 6u);
            song.g[G_DFDBK] = (int16_t)((uint32_t)noise(1 << 30) % 121u);
            song.g[G_CDEPTH] = (int16_t)((uint32_t)noise(1 << 30) % 128u);
        }
        for (i = 0; i < CTL; i++) {
            int32_t on = (b / 300u) % 3u != 2u;                 /* bursts and silences: the tails too */
            c[i] = on ? noise(60000) : 0;
            d[i] = on ? noise(60000) : 0;
            r[i] = on ? noise(b % 2000u < 1000u ? 90000 : 4000) : 0;
        }
        fx_buses(c, d, r, w0, CTL);
        ref_buses(c, d, r, w1, CTL);
        for (i = 0; i < CTL; i++) {
            diff += w0[i] != w1[i];
        }
    }
    check("ROOM: the buses bit for bit as before SPRING (30 s of noise sends, settings changing)", !diff && !fx.rtype);
}

/* ------------------------------------------------------------- SPRING --- */
#define IRN (12u * FS / CTL * CTL)
static int32_t ir[IRN];
static void spring_reset(int32_t size, int32_t damp)
{
    song.g[G_RTYPE] = 1;
    song.g[G_RSIZE] = (int16_t)size;
    song.g[G_RDAMP] = (int16_t)damp;
    rev_clear();
    fx.rtype = 1;
    fx.sp_w = 0;
    fx.sp_size = 0;
    fx.sp_ph = 0;
}
/* the spring's response to x[] (len samples, the rest silence) into ir[0 .. n) */
static void spring_run(const int32_t *x, uint32_t len, uint32_t n)
{
    static int32_t in[CTL];
    uint32_t t, i;
    for (t = 0; t < n; t += CTL) {
        for (i = 0; i < CTL; i++) {
            in[i] = t + i < len ? x[t + i] : 0;
            ir[t + i] = 0;
        }
        rev_spring(in, ir + t, CTL);
    }
}
static double rt60(const int32_t *h, uint32_t n)       /* Schroeder: -5 .. -35 dB, x2 */
{
    static double e[IRN];
    double s = 0, t5 = -1, t35 = -1;
    uint32_t i;
    for (i = n; i-- > 0;) {
        s += (double)h[i] * h[i];
        e[i] = s;
    }
    for (i = 0; i < n; i++) {
        double db = 10 * log10(e[i] / e[0] + 1e-30);
        if (t5 < 0 && db <= -5)
            t5 = i;
        if (t35 < 0 && db <= -35) {
            t35 = i;
            break;
        }
    }
    return t35 < 0 ? 99 : (t35 - t5) * 2.0 / FS;
}
/* group delay (samples) of h[a .. b) at f Hz: Re(sum n h e^-jwn / sum h e^-jwn) */
static double gdelay(const int32_t *h, uint32_t a, uint32_t b, double f)
{
    double w = 2 * M_PI * f / FS, ar = 0, ai = 0, br = 0, bi = 0;
    uint32_t i;
    for (i = a; i < b; i++) {
        double c = cos(w * i), s = -sin(w * i), v = h[i];
        ar += v * c;
        ai += v * s;
        br += i * v * c;
        bi += i * v * s;
    }
    return (br * ar + bi * ai) / (ar * ar + ai * ai);
}

static void test_spring(void)
{
    static int32_t imp[1] = {200000}, burst[2u * FS];
    static const int32_t SZ[3] = {0, 64, 127};
    double r[3];
    char what[200];
    uint32_t k, i;
    host_tracks_init();
    for (k = 0; k < 3u; k++) {
        spring_reset(SZ[k], 60);
        spring_run(imp, 1, IRN);
        r[k] = rt60(ir, IRN);
    }
    snprintf(what, sizeof what, "SPRING decay rises with SIZE: RT60 %.2f s (SIZE 0), %.2f (64), %.2f (127)", r[0], r[1], r[2]);
    check(what, r[0] < r[1] && r[1] < r[2] && r[0] >= 0.15 && r[0] <= 1.0 && r[2] >= 2.0 && r[2] <= 6.0);
    {   /* the chirp: the first arrival (main pickup at L / 2, before the second at 3 L / 4), and the next one */
        static const double F[6] = {500, 1000, 2000, 3000, 4000, 5000};
        double g1[6], g2[6];
        uint32_t L = 1323u + ((127u * 1323u) >> 7), a = L / 2u - 64u, b = (L * 3u) / 4u - 16u, rise = 1;
        spring_reset(127, 0);
        spring_run(imp, 1, 3u * L);
        for (i = 0; i < 6u; i++) {
            g1[i] = gdelay(ir, a, b, F[i]) - L / 2.0;
            g2[i] = gdelay(ir, a + L, b + L, F[i]) - L * 1.5;
            if (i >= 2u && (g1[i] <= g1[i - 1] + 0.5 || g2[i] <= g2[i - 1] + 0.5))
                rise = 0;
        }
        snprintf(what, sizeof what, "SPRING chirp: 1st arrival's group delay %.0f %.0f %.0f %.0f %.0f %.0f samples (0.5 1 2 3 4 5 kHz)",
                 g1[0], g1[1], g1[2], g1[3], g1[4], g1[5]);
        check(what, rise && (g1[5] - g1[1]) * 1000 / FS >= 2.0);
        snprintf(what, sizeof what, "  the next pass more spread: 2nd arrival 0.5 -> 5 kHz %.1f ms (1st %.1f)",
                 (g2[5] - g2[0]) * 1000 / FS, (g1[5] - g1[0]) * 1000 / FS);
        check(what, g2[5] - g2[0] > 1.5 * (g1[5] - g1[0]));
    }
    for (k = 0; k < 4u; k++) {   /* the corners: full-scale noise or square waves, then silence */
        int32_t pk = 0, tail = 0;
        for (i = 0; i < 2u * FS; i++)                   /* beyond any send (4 tracks' at most ~2^17 each) */
            burst[i] = k < 2u ? noise(1 << 19) : (i / (k == 2u ? 7u : 53u)) & 1u ? 1 << 19 : -(1 << 19);
        spring_reset(127, k & 1u ? 127 : 0);
        spring_run(burst, 2u * FS, IRN);
        for (i = 0; i < IRN; i++) {
            int32_t v = abs(ir[i]);
            pk = v > pk ? v : pk;
            if (i >= IRN - FS)
                tail = v > tail ? v : tail;
        }
        snprintf(what, sizeof what, "SPRING at SIZE 127 DAMP %d: 2 s of %s: peak %d, the last second %d",
                 k & 1u ? 127 : 0, k < 2u ? "full-scale noise" : k == 2u ? "a full-scale 3.2 kHz square" : "a 416 Hz square",
                 pk, tail);
        check(what, pk <= 6 * 32768 && tail <= 4);
    }
    {   /* the level against ROOM: the same noise burst, defaults */
        static int32_t in[CTL], o[CTL];
        double er = 0, es = 0;
        uint32_t t, m;
        for (m = 0; m < 2u; m++) {
            host_tracks_init();
            rev_clear();
            fx.sp_size = 0;
            xs = 99;
            for (t = 0; t < 2u * FS; t += CTL) {
                for (i = 0; i < CTL; i++) {
                    in[i] = t < FS / 4u ? noise(20000) : 0;
                    o[i] = 0;
                }
                if (m)
                    rev_spring(in, o, CTL);
                else
                    rev_room(in, o, CTL);
                for (i = 0; i < CTL; i++)
                    *(m ? &es : &er) += (double)o[i] * o[i];
            }
        }
        snprintf(what, sizeof what, "level: SPRING's tail %.1f dB from ROOM's (the same burst, SIZE 90 DAMP 60)", 10 * log10(es / er));
        check(what, fabs(10 * log10(es / er)) <= 6.0);
        rev_clear();
    }
}

/* --------------------------------------------------------- model change --- */
/* rev_clear: every sample of both buffers silent (rev_u's int16 view has an odd count, 556 + 441: clearing it
 * through its int32 view left the last sample, which ROOM then played back after a model change) */
static void test_clear(void)
{
    uint32_t i, left = 0;
    for (i = 0; i < sizeof rev_u.ap / 2u; i++)
        rev_u.ap[i] = 1000;
    for (i = 0; i < sizeof rev_comb / 2u; i++)
        rev_comb[i] = 1000;
    rev_clear();
    for (i = 0; i < sizeof rev_u.ap / 2u; i++)
        left += rev_u.ap[i] != 0;
    for (i = 0; i < sizeof rev_comb / 2u; i++)
        left += rev_comb[i] != 0;
    check("rev_clear: every sample of the combs and the allpasses silent", !left);
}

/* (0.6.86) DLY FEEDBK as an analog delay's: 0..80 the gain it was; 100 a long tail (still there after 6 s); past it the
 * loop grows into itself and holds, bounded by its saturation (never a hard clip, never past full scale) */
static double dly_tail_t(int32_t v, int32_t tone, double *late, int32_t *pk);
static double dly_tail(int32_t v, double *late, int32_t *pk) { return dly_tail_t(v, 64, late, pk); }
static double dly_tail_t(int32_t v, int32_t tone, double *late, int32_t *pk)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    double e1 = 0, e6 = 0;
    uint32_t b, i;
    memset(dly_buf, 0, sizeof dly_buf); fx.dly_lp = fx.dly_hp = 0; fx.primed = 0;
    song.g[G_DTIME] = 2; song.g[G_DFDBK] = (int16_t)v; song.g[G_DCOLOR] = (int16_t)tone; song.g[G_DMIX] = 127; song.g[G_WIDTH] = 0;
    song.g[G_DPIT] = 0; song.g[G_DSPRY] = 0; song.g[G_DMOD] = 0; fx.dt1 = fx.dt2 = 0;
    *pk = 0;
    for (b = 0; b < 7u * FS / CTL; b++) {
        for (i = 0; i < CTL; i++) {
            uint32_t t = b * CTL + i;
            c[i] = r[i] = 0;
            d[i] = t < FS / 4 ? (int32_t)(40000 * exp(-(double)t / 2000.0) * sin(2 * M_PI * 330 * t / FS)) : 0;
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            uint32_t t = b * CTL + i;
            if (t >= FS && t < 2u * FS) e1 += (double)w[i] * w[i];
            if (t >= 6u * FS) e6 += (double)w[i] * w[i];
            if (abs(dly_buf[(fx.dly_w - 1u) & (DLY_LEN - 1u)]) > *pk) *pk = abs(dly_buf[(fx.dly_w - 1u) & (DLY_LEN - 1u)]);
        }
    }
    *late = 10 * log10((e6 + 1) / (e1 + 1));
    return e1;
}
static void test_dly_feedback(void)
{
    double l100, l120, l60;
    int32_t p100, p120, p60, v, same = 1;
    for (v = 0; v <= 80; v++) same &= dly_fb(v) == v * 230;
    dly_tail(60, &l60, &p60);
    dly_tail(100, &l100, &p100);
    dly_tail(120, &l120, &p120);
    printf("reverb:   (DLY tails, 6 s after 1 s: FEEDBK 60 %.1f dB, 100 %.1f dB, 120 %.1f dB; the line's peak 120: %d)\n",
           l60, l100, l120, p120);
    check("DLY FEEDBK: 0..80 as before, 100 a long tail (never cut off), 120 grows and holds, saturating within full scale",
          same && l60 < -40 && l100 > -4 && l100 < 0.5 && l120 > 0 && l120 < 8 && p120 <= 32767 && p120 > 16000);
    {   /* (0.6.89) TONE in the middle: each repeat the very copy of what went in (a sine at half scale, FEEDBK 100: its
         * repeats' peaks within 0.5 % over 2 s); TONE at either end, FEEDBK 100: the band at the cutoff grows, held */
        static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
        uint32_t b, i, dl;
        int32_t p1 = 0, p2 = 0;
        double lo, hi;
        memset(dly_buf, 0, sizeof dly_buf); fx.dly_lp = fx.dly_hp = 0; fx.primed = 0; fx.dt1 = fx.dt2 = 0;
        song.g[G_DTIME] = 2; song.g[G_DFDBK] = 100; song.g[G_DCOLOR] = 64; song.g[G_DMIX] = 127; song.g[G_DMOD] = 0;
        dl = delay_samples();
        for (b = 0; b < 3u * FS / CTL; b++) {
            for (i = 0; i < CTL; i++) {
                uint32_t t = b * CTL + i;
                c[i] = r[i] = 0;
                d[i] = t < dl / 2u ? (int32_t)(30000 * sin(2 * M_PI * 440 * t / FS)) : 0;   /* (sent >> 1 into the line) */
            }
            fx_buses(c, d, r, w, CTL);
            for (i = 0; i < CTL; i++) {
                uint32_t t = b * CTL + i, k = t / dl, a2 = abs(dly_buf[(fx.dly_w - 1u) & (DLY_LEN - 1u)]);
                if (k == 1u && a2 > (uint32_t)p1) p1 = (int32_t)a2;
                if (t >= 2u * FS && a2 > (uint32_t)p2 && t % dl < dl / 2u) p2 = (int32_t)a2;
            }
        }
        dly_tail_t(100, 0, &lo, &p60);
        dly_tail_t(100, 127, &hi, &p60);
        printf("reverb:   (DLY TONE 64: first repeat %d, after 2 s %d; FEEDBK 100 TONE 0: %+.1f dB, TONE 127: %+.1f dB)\n", p1, p2, lo, hi);
        check("DLY TONE in the middle: the repeats copies (within 3 %); at either end the loop self-oscillates (FEEDBK 100)",
              p1 > 14000 && abs(p2 - p1) * 33 <= p1 && lo > 0 && hi > 0);
    }
    {   /* (0.6.90) PING-PONG: a click's repeats left, right, left, right (each side's echoes TIME * 2 apart, the right's
         * TIME later); DIGITAL: FEEDBK past 100 % holds (never grows), TONE at an end never self-oscillates */
        static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
        uint32_t b, i, dl, k;
        int32_t sl[8] = {0}, sr[8] = {0};
        double g120, gt;
        memset(dly_buf, 0, sizeof dly_buf); fx.dly_lp = fx.dly_hp = 0; fx.primed = 0; fx.dt1 = fx.dt2 = 0;
        song.g[G_DTIME] = 2; song.g[G_DFDBK] = 90; song.g[G_DCOLOR] = 64; song.g[G_DMIX] = 127; song.g[G_DMOD] = 0;
        song.g[G_DPING] = 1; song.g[G_DTYPE] = 0; song.g[G_WIDTH] = 0;
        dl = delay_samples();
        for (b = 0; b < 9u * dl / CTL; b++) {
            for (i = 0; i < CTL; i++) { c[i] = r[i] = 0; d[i] = b * CTL + i < 64u ? 40000 : 0; fx_sl[i] = fx_sr[i] = 0; }
            fx_buses(c, d, r, w, CTL);
            for (i = 0; i < CTL; i++) {
                uint32_t t = b * CTL + i;
                int32_t L = w[i] + fx_sl[i], R = w[i] + fx_sr[i];
                k = (t + dl / 2u) / dl;                 /* (repeat k, around k * TIME) */
                if (k > 0u && k < 8u) { if (abs(L) > sl[k]) sl[k] = abs(L); if (abs(R) > sr[k]) sr[k] = abs(R); }
            }
        }
        printf("reverb:   (PING-PONG repeats L/R:");
        for (k = 1; k < 7u; k++) printf(" %d/%d", sl[k], sr[k]);
        printf(")\n");
        check("DLY PING-PONG: the repeats left, right, left, right, each softer",
              sl[1] > 4 * sr[1] && sr[2] > 4 * sl[2] && sl[3] > 4 * sr[3] && sr[4] > 4 * sl[4] && sl[3] < sl[1] && sr[4] < sr[2]);
        song.g[G_DPING] = 0; song.g[G_DTYPE] = 1;
        dly_tail_t(120, 64, &g120, &p60);
        dly_tail_t(100, 0, &gt, &p60);
        printf("reverb:   (DIGITAL: FEEDBK 120 %+.1f dB over 5 s, TONE 0 FEEDBK 100 %+.1f dB)\n", g120, gt);
        check("DLY DIGITAL: FEEDBK 100 % at most (held, never growing), TONE plain (no self-oscillation)", g120 < 0.5 && g120 > -3 && gt < 0);
        song.g[G_DTYPE] = 0;
    }
}

static void test_switch(void)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i;
    int32_t prev = 0, step = 0, own = 0, after = 0;
    char what[200];
    host_tracks_init();
    rev_clear();
    memset(dly_buf, 0, sizeof dly_buf);                         /* (the delay and chorus quiet: only the reverb) */
    memset(cho_buf, 0, sizeof cho_buf);
    fx.dly_lp = fx.dly_hp = 0;                                  /* (0.6.86: its loop's filters too) */
    fx.rtype = 0;
    for (b = 0; b < 4u * FS / CTL; b++) {
        if (b == 2u * FS / CTL)
            song.g[G_RTYPE] = 1;
        for (i = 0; i < CTL; i++) {
            c[i] = d[i] = 0;
            r[i] = b < FS / CTL ? (int32_t)(30000 * sin(2 * M_PI * 220 * (b * CTL + i) / FS)) : 0;
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t s = abs(w[i] - prev);
            if (b >= 2u * FS / CTL - 4u && b <= 2u * FS / CTL + 4u)
                step = s > step ? s : step;
            else if (b > FS / CTL + 100u && b < 2u * FS / CTL - 4u)
                own = s > own ? s : own;
            if (b > 2u * FS / CTL)
                after = abs(w[i]) > after ? abs(w[i]) : after;
            prev = w[i];
        }
    }
    snprintf(what, sizeof what, "ROOM -> SPRING while the tail rings: largest step %d (the tail's own %d), silent after: %d",
             step, own, after);
    check(what, step <= 2 * own + 64 && after == 0 && fx.rtype == 1);
    song.g[G_RTYPE] = 0;
    fx_buses(c, d, r, w, CTL);
    check("  and back to ROOM", fx.rtype == 0);
}

/* (0.8.4) the models' lines share rev_x (CLOUD's, SHIMMER's, the pre-delay's): every change between any two models (and
 * through a third: CLOUD -> RESO -> ROOM, CLOUD -> ROOM -> CLOUD), PRE on with ROOM and SPRING, the old one ringing: no
 * click (the old one's block fades out, as test_switch), and after it, the input stopped before, nothing at all: the new
 * one starts from silence, never playing what another left in the shared line (nor CLOUD its own line from before) */
static void switch_chain(const uint8_t *seq, uint32_t nseq)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    static const char *const NM[5] = {"ROOM", "SPRING", "SHIMMER", "RESO", "CLOUD"};
    uint32_t b, i, k = 0, sw = (FS + FS / 5u) / CTL, ph = FS * 3u / 20u / CTL;   /* (the input 1 s; the first change 0.2 s
                                                             * later, then one every 0.15 s) */
    int32_t prev = 0, step = 0, own = 0, after = 0;
    char what[200];
    host_tracks_init();
    rev_clear();
    memset(dly_buf, 0, sizeof dly_buf);
    memset(cho_buf, 0, sizeof cho_buf);
    fx.dly_lp = fx.dly_hp = 0;
    song.g[G_RSIZE] = 110; song.g[G_RDAMP] = 40; song.g[G_RMOD] = 40; song.g[G_RRATE] = 64; song.g[G_RWIDE] = 0;
    song.g[G_RFILT] = 0;
    song.g[G_RTYPE] = seq[0]; song.g[G_RPRE] = seq[0] <= 1u ? 60 : 0;
    fx.rtype = seq[0];
    for (b = 0; b < sw + nseq * ph; b++) {
        if (b >= sw && (b - sw) % ph == 0u && ++k < nseq) {
            song.g[G_RTYPE] = seq[k];
            song.g[G_RPRE] = seq[k] <= 1u ? 60 : 0;
        }
        for (i = 0; i < CTL; i++) {
            c[i] = d[i] = 0;
            r[i] = b < FS / CTL ? (int32_t)(30000 * sin(2 * M_PI * 220 * (b * CTL + i) / FS)) + noise(6000) : 0;
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t s = abs(w[i] - prev);
            if (b >= sw - 4u && b <= sw + 4u)
                step = s > step ? s : step;
            else if (b > FS / CTL + 100u && b < sw - 4u)
                own = s > own ? s : own;
            if (b > sw)
                after = abs(w[i]) > after ? abs(w[i]) : after;
            prev = w[i];
        }
    }
    snprintf(what, sizeof what, "  %s", NM[seq[0]]);
    for (k = 1; k < nseq; k++)
        snprintf(what + strlen(what), sizeof what - strlen(what), " -> %s", NM[seq[k]]);
    snprintf(what + strlen(what), sizeof what - strlen(what), ": step %d (the tail's own %d), after %d", step, own, after);
    check(what, step <= 2 * own + 64 && after == 0 && fx.rtype == seq[nseq - 1u]);
}
static void test_switch_all(void)
{
    static const uint8_t CH[4][3] = {{4, 3, 0}, {4, 0, 4}, {2, 1, 2}, {0, 4, 1}};
    uint8_t s[2];
    uint32_t a, b;
    printf("reverb: (0.8.4) model changes through the shared line (rev_x):\n");
    for (a = 0; a < 5u; a++)
        for (b = 0; b < 5u; b++)
            if (a != b) {
                s[0] = (uint8_t)a; s[1] = (uint8_t)b;
                switch_chain(s, 2);
            }
    for (a = 0; a < 4u; a++)
        switch_chain(CH[a], 3);
    song.g[G_RTYPE] = 0; song.g[G_RPRE] = 0; song.g[G_RMOD] = 0;
    rev_clear();
    fx.rtype = 0;
}

/* ---------------------------------------------------------------- cost --- */
static double cost_of(int what)              /* 0 rev_room, 1 rev_spring, 2 fx_buses ROOM, 3 fx_buses SPRING */
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t b, i, nb = 4u * FS / CTL;
    uint64_t i0;
    host_tracks_init();
    rev_clear();
    song.g[G_RTYPE] = (int16_t)(what & 1);
    fx.rtype = (uint8_t)(what & 1);
    for (i = 0; i < CTL; i++) {
        c[i] = noise(30000);
        d[i] = noise(30000);
        r[i] = noise(30000);
    }
    i0 = instr_now();
    for (b = 0; b < nb; b++) {
        if (what >= 2)
            fx_buses(c, d, r, w, CTL);
        else if (what)
            rev_spring(r, w, CTL);
        else
            rev_room(r, w, CTL);
    }
    return i0 ? (double)(instr_now() - i0) / (nb * CTL) : 0;
}
static void test_cost(void)
{
    double room = cost_of(0), spr = cost_of(1), broom = cost_of(2), bspr = cost_of(3);
    char what[200];
    if (!room) {
        printf("reverb: cost: no instruction counter on this host\n");
        return;
    }
    snprintf(what, sizeof what, "cost: the reverb alone ROOM %.0f, SPRING %.0f instructions / sample (+%.0f %%, limit +80 %%)",
             room, spr, (spr / room - 1) * 100);
    check(what, spr <= room * 1.80);
    snprintf(what, sizeof what, "  the bus stage (chorus, delay, reverb) ROOM %.0f (~%.1f %%), SPRING %.0f (~%.1f %%): +%.0f (~%.2f %% CPU)",
             broom, broom * 0.017, bspr, bspr * 0.017, bspr - broom, (bspr - broom) * 0.017);
    check(what, (bspr - broom) * 0.017 <= 2.0);
}

/* ------------------------------------------------------------- demos --- */
static void demo(const char *dir, const char *name, int rtype, int pluck)
{
    static const uint8_t PL[16] = {64, 0, 67, 0, 71, 0, 0, 74, 72, 0, 67, 0, 64, 0, 62, 0};
    char path[512];
    FILE *f;
    uint32_t t, i, frames = 8u * FS;
    int32_t o[2 * CTL];
    track_t *tr = &trk[0];
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    rev_clear();
    fx.rtype = (uint8_t)rtype;
    song.g[G_RTYPE] = (int16_t)rtype;
    song.g[G_BPM] = 100;
    if (pluck) {
        host_preset(tr, 0, 8);                                     /* ANALOG PLUCK */
        for (i = 0; i < 16u; i++)
            put_step(tr, i, PL[i] ? 1u : 0u, &PL[i], PL[i] ? ST_NOTE : ST_REST, 0);
    } else {
        host_drums(tr);                                            /* the GM map: DRUM KIT (SAMPLE PERC until 1.0.2) */
        for (i = 0; i < 16u; i++) {
            uint8_t dn[3], k = 0;
            if (i % 4u == 0u || i == 10u) dn[k++] = 36;
            if (i == 4u || i == 12u) dn[k++] = 38;
            if (i % 2u == 0u) dn[k++] = 42;
            put_step(tr, i, k, dn, k ? ST_NOTE : ST_REST, 0);
        }
    }
    tr->p[P_REV] = 90;
    tr->p[P_DLY] = tr->p[P_CHOR] = 0;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    wav_hdr(f, frames);
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t == 6u * FS)
            transport_req = 2;                                     /* the last 2 s: the tail alone */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    fclose(f);
    printf("reverb: demo %s\n", path);
}

/* ------------------------------------------------- JIANT 0.3: the new FX --- */
/* the buses on sends (chorus 0, delay d(t), reverb r(t)) for nb blocks; out: wet into w (n samples), mix_l - mix_r
 * (the side) energy into *side */
static int32_t jw[FS * 2];
static void jrun(int32_t (*d)(uint32_t), int32_t (*r)(uint32_t), uint32_t n, double *side)
{
    static int32_t c[CTL], dd[CTL], rr[CTL];
    uint32_t t, i;
    *side = 0;
    for (t = 0; t + CTL <= n; t += CTL) {
        for (i = 0; i < CTL; i++) {
            c[i] = 0;
            dd[i] = d ? d(t + i) : 0;
            rr[i] = r ? r(t + i) : 0;
            fx_sl[i] = fx_sr[i] = 0;
        }
        fx_buses(c, dd, rr, jw + t, CTL);
        for (i = 0; i < CTL; i++)
            *side += (double)(fx_sl[i] - fx_sr[i]) * (fx_sl[i] - fx_sr[i]);
    }
}
static int32_t sine500(uint32_t t) { return (int32_t)(40000.0 * sin(2.0 * M_PI * 500.0 * t / FS)); }
static int32_t burst(uint32_t t) { return t < FS / 10u ? noise(80000) : 0; }
static int32_t noise_on(uint32_t t) { (void)t; return noise(120000); }
static void jreset(void)
{
    uint32_t i;
    host_tracks_init();
    for (i = 0; i < DLY_LEN; i++)
        dly_buf[i] = 0;
    rev_clear();
    fx.rtype = 0;
    memset(fx.rm_m, 0, sizeof fx.rm_m);
    fx.rf_lp = 0;
    song.g[G_DMIX] = 127;
    song.g[G_DFDBK] = 0;
    song.g[G_DCOLOR] = 64;                         /* (TONE open) */
    song.g[G_DHPF] = 0;
    song.g[G_WIDTH] = 0;
    song.g[G_RPRE] = 0;
    xs = 0x1234567u;
}
static double band_e(uint32_t a, uint32_t b) { double e = 0; for (; a < b; a++) e += (double)jw[a] * jw[a]; return e; }
static uint32_t jcross(uint32_t a, uint32_t b) { uint32_t c = 0; for (a++; a < b; a++) c += jw[a - 1] < 0 && jw[a] >= 0; return c; }
static void test_jiant(void)
{
    double side, e0, e1, e2;
    uint32_t c0, c1, c2, k;
    int32_t pk = 0;
    /* GRAIN: PITCH +12 an octave up, -12 down (the echo's zero crossings), SPRY scatters it, bounded */
    jreset();
    jrun(sine500, 0, FS, &side);
    c0 = jcross(FS / 2u, FS);
    jreset();
    song.g[G_DPIT] = 12;
    jrun(sine500, 0, FS, &side);
    c1 = jcross(FS / 2u, FS);
    jreset();
    song.g[G_DPIT] = -12;
    jrun(sine500, 0, FS, &side);
    c2 = jcross(FS / 2u, FS);
    check("GRAIN delay: PITCH +12 the echo an octave up, -12 down (its zero crossings: x2, x0.5 +-15 %)",
          c0 > 200 && c1 > c0 * 1.7 && c1 < c0 * 2.3 && c2 > c0 * 0.35 && c2 < c0 * 0.65);
    printf("reverb:   (crossings in 0.5 s: plain %u, +12 %u, -12 %u)\n", c0, c1, c2);
    jreset();
    song.g[G_DPIT] = 7;
    song.g[G_DSPRY] = 127;
    song.g[G_DFDBK] = 120;
    jrun(burst, 0, FS * 2u, &side);
    for (k = 0; k < FS * 2u; k++)
        pk = abs(jw[k]) > pk ? abs(jw[k]) : pk;
    e0 = band_e(FS, FS * 2u);
    check("GRAIN delay: PITCH 7, SPRY 127, feedback 120: bounded, still repeating after 1 s", pk < 1 << 20 && e0 > 0);
    /* the reverb's FILT (darker / thinner: less energy from white noise) and WIDE (a side, none at 0) */
    for (k = 0; k < 3u; k++) {
        double *e = k == 0u ? &e0 : k == 1u ? &e1 : &e2;
        jreset();
        song.g[G_DMIX] = 0;
        song.g[G_RFILT] = (int16_t)(k == 0u ? 0 : k == 1u ? -64 : 63);
        jrun(0, burst, FS, &side);
        *e = band_e(0, FS);
    }
    check("reverb FILT: -64 (low-pass) and 63 (low cut) each take energy out of a noise burst's tail",
          e1 < e0 * 0.7 && e2 < e0 * 0.9 && e1 > 0 && e2 > 0);
    {
        double sd;
        jreset();
        song.g[G_DMIX] = 0;
        song.g[G_RTYPE] = 0;
        song.g[G_RWIDE] = 127;
        jrun(0, burst, FS, &side);
        sd = side;
        jreset();
        song.g[G_DMIX] = 0;
        song.g[G_RWIDE] = 0;
        jrun(0, burst, FS, &side);
        check("reverb WIDE: ROOM 127 a side (left and right apart), 0 none", sd > band_e(0, FS) * 0.05 && side == 0);
    }
    jreset();
    song.g[G_DMIX] = 0;
    song.g[G_RTYPE] = 1;
    fx.rtype = 1;
    song.g[G_RWIDE] = 127;
    jrun(0, burst, FS, &side);
    check("reverb WIDE: SPRING 127 a side too", side > 0);
    /* MOD: the combs' read offsets swing up to ~5.7 ms at 127 (0.25 .. 1.3 ms before 0.3), no step between samples */
    {
        uint32_t b, mx[2] = {0, 0}, kk;
        for (kk = 0; kk < 2u; kk++) {
            jreset();
            song.g[G_DMIX] = 0;
            song.g[G_RMOD] = (int16_t)(kk ? 127 : 16);
            song.g[G_RRATE] = 127;
            for (b = 0; b < 4u * FS / CTL; b++) {
                static int32_t z[CTL], w[CTL];
                rev_room_mod(z, w, CTL, 0);
                mx[kk] = (uint32_t)fx.rm_m[0] > mx[kk] ? (uint32_t)fx.rm_m[0] : mx[kk];
            }
        }
        printf("reverb:   (MOD: the offset's swing 16: %u, 127: %u samples)\n", mx[0] >> 8, mx[1] >> 8);
        check("reverb MOD: 127 swings the combs ~5.7 ms (240 samples or more), 16 a few", mx[1] >> 8 >= 240u && mx[0] >> 8 < 8u);
    }
    /* DUCK 100: the gain down past -26 dB within 3 ms, held there at 20 ms, back up after the release */
    {
        int32_t g3, g20, gend;
        uint32_t b;
        host_tracks_init();
        song.g[G_DUCK] = 100;
        song.g[G_DREL] = 0;
        duck_env = 0;
        duck_g1 = 32767;
        duck_hit = 1;
        for (b = 0; b < 4u * 32u / CTL; b++)             /* (the moments in blocks of 32: 3 ms, 20 ms, 0.4 s) */
            duck_block();
        g3 = duck_g1;
        for (; b < 28u * 32u / CTL; b++)
            duck_block();
        g20 = duck_g1;
        for (; b < 600u * 32u / CTL; b++)
            duck_block();
        gend = duck_g1;
        check("DUCK 100: below -26 dB in 3 ms, still there at 20 ms (the hold), back to unity after the release",
              g3 < 1640 && g20 < 1640 && gend > 32000);
        printf("reverb:   (DUCK gain: %d at 3 ms, %d at 20 ms, %d at 0.4 s)\n", g3, g20, gend);
        song.g[G_DUCK] = 0;
    }
}

/* (JIANT 0.5) SHIMMER RESO CLOUD: a burst rings on (a tail), never runs away (the peak bounded), and dies (RESO's
 * combs and SHIMMER's octave loop below 1; CLOUD's grains read only the last ~0.5 s); CLOUD FREEZE (SIZE full) holds */
static void test_new_types(void)
{
    static const char *const NM[3] = {"SHIMMER", "RESO", "CLOUD"};
    uint32_t k, i;
    for (k = 0; k < 3u; k++) {
        double side, early, late;
        int32_t pk = 0;
        jreset();
        song.g[G_RTYPE] = (int16_t)(2 + k);
        song.g[G_RSIZE] = 90; song.g[G_RDAMP] = 40; song.g[G_RMOD] = 80; song.g[G_RRATE] = 60; song.g[G_RWIDE] = 60;
        jrun(0, burst, FS * 2u, &side);
        for (i = 0; i < FS * 2u; i++) if (abs(jw[i]) > pk) pk = abs(jw[i]);
        early = band_e(FS / 5u, FS * 3u / 5u);
        late = band_e(FS * 8u / 5u, FS * 2u);
        printf("reverb: %-8s peak %d, energy 0.2-0.6 s %.3g, 1.6-2 s %.3g, side %.3g\n", NM[k], pk, early, late, side);
        {
            char what[96];
            snprintf(what, sizeof what, "%s: a burst rings on, bounded, and dies away", NM[k]);
            check(what, early > 1e9 && pk < (1 << 20) && late < early * 0.25);
        }
    }
    {   /* CLOUD FREEZE: SIZE full, the line stops being written: the cloud still sounds after 2 s */
        double side;
        jreset();
        song.g[G_RTYPE] = 4; song.g[G_RSIZE] = 120; song.g[G_RRATE] = 80; song.g[G_RMOD] = 0; song.g[G_RWIDE] = 0;
        jrun(0, burst, FS / 2u, &side);
        song.g[G_RSIZE] = 127;                          /* (freeze once the burst is in) */
        jrun(0, 0, FS * 2u, &side);
        check("CLOUD: SIZE full freezes the cloud (it still sounds 2 s on)", band_e(FS * 3u / 2u, FS * 2u) > 1e8);
    }
    {   /* (0.5.1) SIZE 126, every model: 2 s of loud noise stay bounded; then 2 s alone: the tail long but dying */
        static const char *const N5[5] = {"ROOM", "SPRING", "SHIMMER", "RESO", "CLOUD"};
        int ok = 1;
        for (k = 0; k < 5u; k++) {
            double side, e0, e1;
            int32_t pk = 0;
            jreset();
            song.g[G_RTYPE] = (int16_t)k;
            song.g[G_RSIZE] = 126; song.g[G_RDAMP] = 30; song.g[G_RMOD] = 60; song.g[G_RRATE] = 60; song.g[G_RWIDE] = 80;
            jrun(0, noise_on, FS * 2u, &side);
            for (i = 0; i < FS * 2u; i++) if (abs(jw[i]) > pk) pk = abs(jw[i]);
            jrun(0, 0, FS * 2u, &side);
            e0 = band_e(0, FS / 2u);
            e1 = band_e(FS * 3u / 2u, FS * 2u);
            printf("reverb: SIZE 126 %-8s peak %d under noise, tail 0-0.5 s %.3g, 1.5-2 s %.3g (%.1f dB)\n", N5[k], pk, e0, e1,
                   10 * log10((e1 + 1) / (e0 + 1)));
            ok &= pk < (1 << 20) && e0 > 1e9 && e1 < e0;
        }
        check("SIZE 126: every model bounded under 2 s of loud noise, its tail long and dying", ok);
    }
    {   /* (0.6.5) RESO's PITCH (PRE's slot): the chord up the scale; FOLW (DAMP's slot): built on a track's note; a glide */
        double side;
        uint32_t p0, p2, pf, pg;
        jreset();
        song.g[G_RTYPE] = 3; song.g[G_RSIZE] = 90; song.g[G_RMOD] = 0; song.g[G_RPRE] = 0; song.g[G_RDAMP] = 0;
        trk[0].p[P_ROOT] = 0; trk[0].p[P_SCALE] = 1;    /* (C major) */
        jrun(0, burst, FS / 2u, &side);
        p0 = fx.rs_per[0];
        song.g[G_RPRE] = 2;                             /* two steps up: E */
        jrun(0, burst, FS / 2u, &side);
        p2 = fx.rs_per[0];
        song.g[G_RPRE] = 0; song.g[G_RDAMP] = 1;        /* FOLW T1: its note G (67) */
        trk[0].v[0].active = 1; trk[0].v[0].gate = 1; trk[0].v[0].note = 67;
        jrun(0, burst, FS / 2u, &side);
        pf = fx.rs_per[0];
        trk[0].v[0].active = trk[0].v[0].gate = 0;      /* let go: the chord stays */
        jrun(0, burst, FS / 2u, &side);
        pg = fx.rs_per[0];
        printf("reverb: RESO bass string period C %u, +2 steps %u (%.3f), FOLW G %u (%.3f), held %u\n", p0, p2,
               (double)p0 / p2, pf, (double)p0 / pf, pg);
        check("RESO: PITCH +2 steps on C major = E (x1.26), FOLW T1 on G = G (x1.50), held after the note ends",
              fabs((double)p0 / p2 - 1.2599) < 0.02 && fabs((double)p0 / pf - 1.4983) < 0.02 && pg == pf && fx.rs_root == 68);
        song.g[G_RDAMP] = 0;
        fx.rs_root = 0;
    }
    song.g[G_RTYPE] = 0; song.g[G_RWIDE] = 0; song.g[G_RMOD] = 0;
}
/* (0.6.76) core 1 reads a word wrong now and then (0x00200000 for 0). The buses' lines are 16-bit and clamped, so a
 * wrong word in their state gives a short burst, never a blow-up; ROOM's modulation offsets, wrong, are put back (they
 * read far outside the combs); a 32-bit value out of range in the output (fx_insane) mutes the block, silences every
 * line and state, and gives core 1 up if it ran the buses (fx_trip) */
static void test_trip(void)
{
    int32_t o[2 * CTL];
    uint32_t k, i, t0, quiet = 1, kept, in = 1;
    jreset();
    song.g[G_RTYPE] = 0;
    fx.rtype = 0;
    song.g[G_RMOD] = 60;
    song.g[G_RWIDE] = 127;
    trk[0].p[P_REV] = 127;
    trk[0].p[P_DLY] = 127;
    trk_note_on(&trk[0], 60, 100);
    mc_host = 1;
    t0 = fx_trips;
    for (k = 0; k < 20u; k++)
        mix_block(o, CTL);
    for (k = 0; k < 4u; k++)
        fx.rm_m[k] = k & 1u ? 0x00200000 : -0x00200000;
    for (k = 0; k < 20u; k++) {
        mix_block(o, CTL);
        for (i = 0; i < 4u; i++)
            in &= (uint32_t)fx.rm_m[i] <= 0xFFFFu;
    }
    check("ROOM MOD: offsets read wrong (0x00200000) put back in range, nothing out of range, core 1 kept",
          in && fx_trips == t0 && mc_host);
    for (i = 0; i < CTL; i++)
        fx_wet[i] = fx_sl[i] = 45000;
    k = fx_insane(CTL);
    fx_wet[7] = 0x00200000;
    check("the buses' check: 45000 in range, 0x00200000 out", !k && fx_insane(CTL));
    fx_bad = 1;                                                 /* (as fx_run on core 1, out of range) */
    mix_block(o, CTL);
    kept = mc_host;
    for (i = 0; i < CTL; i++)
        quiet &= fx_wet[i] == 0 && fx_sl[i] == 0 && fx_sr[i] == 0;
    for (i = 0; i < DLY_LEN; i++)
        quiet &= dly_buf[i] == 0;
    for (i = 0; i < sizeof rev_comb / 2u; i++)
        quiet &= rev_comb[i] == 0;
    trk_note_off(&trk[0], 60);
    trk[0].p[P_REV] = trk[0].p[P_DLY] = 0;
    mc_host = 0;
    printf("reverb: trip: %u, core 1 %s, buses %s\n", fx_trips - t0, kept ? "kept" : "given up", quiet ? "silenced" : "sounding");
    check("out of range: the block muted, the buses' lines silenced, core 1 given up (CORES 1)",
          fx_trips == t0 + 1u && !kept && quiet);
    song.g[G_RMOD] = song.g[G_RWIDE] = 0;
}
/* PRE: a plain delay of the send at the full rate (it was at half rate, without a filter, up to V10.7.15: a 12
 * kHz send came back as a 10 kHz alias, louder than itself): ROOM's output with PRE 20 is PRE 0's 800 samples later, but
 * for the line's rounding (12 kHz, where the alias was) */
static int32_t tone12k(uint32_t t) { return (int32_t)(40000.0 * sin(2.0 * M_PI * 12000.0 * t / FS)); }
static void test_pre(void)
{
    static int32_t ref[FS];
    double side, e = 0, r = 0;
    uint32_t i;
    jreset();
    jrun(0, tone12k, FS, &side);
    memcpy(ref, jw, sizeof ref);
    jreset();
    song.g[G_RPRE] = 20;
    jrun(0, tone12k, FS, &side);
    for (i = FS / 4u; i < FS - CTL; i++) {
        double d = (double)jw[i] - ref[i - 800u];
        e += d * d;
        r += (double)ref[i - 800u] * ref[i - 800u];
    }
    char what[160];
    snprintf(what, sizeof what, "PRE 20 on ROOM: PRE 0's output 800 samples later, no alias (12 kHz: the rest %.1f dB)",
             10.0 * log10(e / r + 1e-30));
    check(what, r > 0 && e < r * 1e-5);
    song.g[G_RPRE] = 0;
}
int main(int argc, char **argv)
{
    test_room_identical();
    test_pre();
    test_spring();
    test_clear();
    test_switch();
    test_switch_all();
    test_dly_feedback();
    test_cost();
    test_jiant();
    test_new_types();
    test_trip();
    if (argc > 1) {
        demo(argv[1], "spring_drums", 1, 0);
        demo(argv[1], "spring_pluck", 1, 1);
        demo(argv[1], "room_drums", 0, 0);
    }
    printf("%s\n", bad ? "REVERB TEST FAILED" : "reverb test passed");
    return bad != 0;
}
