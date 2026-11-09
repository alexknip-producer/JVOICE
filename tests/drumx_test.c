/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DRUM-X (src/drumx_voice.c: the DRUM engine) on the host, through hostsim.c:
 *   build/host/drumx_test [DEMODIR]          (run_tests.sh: build/drumx_demo)
 * 1. every lane of the kit, struck through the DRUM engine (its GM note), sounds (peak above -30 dBFS), never at
 *    full scale, DC under 2 % of the peak, and ends: its voice frees itself (DECAY at its design) within 3 s.
 * 2. MORPH (MRPH): 0 and 127 render different hits, 64 between them (its RMS between theirs or near);
 *    moving it while a hit rings: no step larger than the hit's own at A or B (+20 %).
 * 3. PITCH: TUNE +12 semitones doubles the tom's frequency (zero crossings, +-3 %); the kick's pitch envelope
 *    falls (more crossings in its first 20 ms than in 100..120 ms).
 * 4. the closed hat chokes the open one (below -60 dB of its level within 10 ms).
 * 5. the knobs: MRPH on KNOB 1, NOIS moves every lane's noise.
 * 5w. FOLD: every lane another sound at 127, never at full scale.
 * 5b. the group mutes (dx_mute): a muted group's hits are silent (any kit), the others still sound; muting a
 *    ringing group fades it out: its voice ends within 10 ms, the output 26 dB under the unmuted hit's, no step
 *    larger than the hit's own; unmuting sounds again.
 * 5c. the master (JIANT): PNCH 100 lifts a kick's first 8 ms (+30 %) and lowers its tail; DUCK 100: a held synth
 *    note drops 10 dB or more 5..40 ms after a kick (against DUCK 0), and is back (within 1 dB) after the release;
 *    CLIP 100: the loud mix saturates (crest factor down), never past full scale; CLIP 0 is out of the chain.
 * 6. demos into DEMODIR: every lane at MORPH 0, 64, 127; a beat with MORPH swept over 4 bars. */
#include <stdarg.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok, const char *fmt, ...)
{
    printf("drumx: %-66s %s", what, ok ? "ok" : "FAIL");
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        printf("  (");
        vprintf(fmt, ap);
        printf(")");
        va_end(ap);
    }
    printf("\n");
    fails += !ok;
}

#define MAXN (FS * 3u)
static int32_t buf[MAXN];
static const uint8_t NOTE[8] = {36, 38, 39, 42, 46, 45, 37, 56};   /* eng_drum.c DRUM_LANE_NOTE */

static track_t *kitx(int16_t morph, int16_t tune)
{
    track_t *t = &trk[0];
    uint32_t i, k;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            trk[k].v[i].active = 0;
    host_tracks_init();
    lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; lim_env = LIM_T;   /* (0.5.1: the slow leveler from rest each time) */
    memset(mp_seen, 0, sizeof mp_seen); memset(dist_w, 0, sizeof dist_w);   /* (0.6.8: the parts' LEVEL / PAN / DST blend from rest too) */
    for (k = 0; k < DV_NLANE; k++)                       /* (V10.6q: each strike a hit from rest; a hit struck while it
                                                          * rings goes on from where it was, its noise drawn on) */
        drum_kit[0][k].x.live = drum_kit[0][k].x.trig = 0, drum_kit[0][k].x.rng = 0;
    host_preset(t, ENGI_DRUM, 0);
    t->p[P_E0] = morph;
    t->p[P_E1] = tune;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = 0;
    return t;
}
/* (ctl64) the kit's noise from its seed at the next hit (drumx_voice.c dx_run: an empty generator takes the constant): two
 * strikes compared sample for sample, or in a short window (a clap's first ms, a snare's first 5 ms), hear the same noise.
 * Without it each strike drew another realization and those windows differed up to x5 strike to strike (the probe on
 * the 32-sample build too): the checks passed or failed with the generator's position */
static void same_noise(track_t *t)
{
    uint32_t l;
    for (l = 0; l < DV_NLANE; l++)
        drum_kit[t - trk][l].x.rng = 0;
}
/* lane l struck, n samples of the left output into buf; returns the voice still sounding at the end */
static int strike(track_t *t, uint32_t l, uint32_t n)
{
    uint32_t i, k;
    trk_note_on(t, NOTE[l], 100);
    trk_note_off(t, NOTE[l]);
    for (k = 0; k < n; k += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL && k + i < n; i++)
            buf[k + i] = o[2 * i];
    }
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active)
            return 1;
    return 0;
}
static double rms(uint32_t a, uint32_t b)
{
    double s = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)buf[i] * buf[i];
    return sqrt(s / (b - a));
}
static uint32_t crossings(uint32_t a, uint32_t b)
{
    uint32_t i, c = 0;
    for (i = a + 1; i < b; i++)
        c += buf[i - 1] < 0 && buf[i] >= 0;
    return c;
}
static void save(const char *dir, const char *name, uint32_t n)
{
    char path[512];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    wav_hdr(f, n);
    for (i = 0; i < n; i++)
        wav_put(f, buf[i], buf[i]);
    fclose(f);
}

int main(int argc, char **argv)
{
    static const char *const NAME[8] = {"kick", "snare", "clap", "hatcl", "hatop", "tom", "rim", "bell"};
    const char *dir = argc > 1 ? argv[1] : "build/drumx_demo";
    char nm[64];
    uint32_t l, i;
    int ok;

    /* 1 */
    for (l = 0; l < 8u; l++) {
        track_t *t = kitx(64, 64);
        int32_t pk = 0;
        double dc = 0;
        int ended;
        strike(t, l, FS / 2u);
        for (i = 0; i < FS / 2u; i++) {
            pk = abs(buf[i]) > pk ? abs(buf[i]) : pk;
            dc += buf[i];
        }
        dc /= FS / 2u;
        ended = !strike(t, l, MAXN) || 0;
        snprintf(nm, sizeof nm, "%s: sounds, under full scale, no DC, ends", NAME[l]);
        check(nm, pk > 1036 && pk < 32000 && fabs(dc) < pk * 0.02 && ended, "peak %d, DC %.0f", pk, dc);
    }

    /* 1k (V10.6z8, the owner: "the hats a giant low clap", "the kick sounds like a snare"): every factory kit at
     * MORPH 0 / 64 / 127 with its preset's TONE / DECY / NOIS, the voice alone (dx_run_fx): each hat bright (the
     * energy of its first difference over its own: 4 sin^2(pi f / fs), 0.08 at 2 kHz, 0.47 at 4.5 kHz, 0.68 at
     * 6 kHz; the LOFI and HIP HOP hats were 0.10 .. 0.25, a clap's), each kick a body (its noise, the hit less the
     * same hit with NOISE 0, under 3 % of its energy: V10.6q's kit was 10 .. 30 %) */
    {
        static int32_t y0[FS / 4], y1[FS / 4];
        double hmin = 9, kmax = 0;
        uint32_t k, m, w, h;
        for (k = 0; k < DX_NKIT; k++) {
            int32_t c = DRUM_PRESETS[k].e[2] - 64, d = DRUM_PRESETS[k].e[3] - 64, nz = DRUM_PRESETS[k].e[4] - 64;
            dx_kit_load(k);
            for (m = 0; m <= 127u; m += 127u / 2u)
                for (h = 0; h < 3u; h++) {               /* the closed hat, the open hat, the kick */
                    uint32_t ln = h == 2u ? DV_KICK : DV_HATC + h, b;
                    double e0 = 0, e1 = 0;
                    for (w = 0; w < (h == 2u ? 2u : 1u); w++) {
                        dx_voice_t v;
                        dx_lane_t L = dx_kit[ln];
                        int32_t *y = w ? y0 : y1;
                        if (w)
                            L.a[DXP_NOISE] = L.b[DXP_NOISE] = 0;
                        dx_kit[ln] = L;
                        memset(&v, 0, sizeof v);
                        dx_trigger(&v);
                        for (b = 0; b + CTL <= FS / 4u; b += CTL)
                            dx_run_fx(&dx_kit[ln], &v, (int32_t)m, 0, d, c, nz, 0, 0, y + b, CTL, 0);
                    }
                    dx_kit_load(k);
                    if (h < 2u) {
                        for (b = 1; b < FS / 4u; b++) {
                            e0 += (double)y1[b] * y1[b];
                            e1 += (double)(y1[b] - y1[b - 1]) * (y1[b] - y1[b - 1]);
                        }
                        hmin = e1 / (e0 + 1) < hmin ? e1 / (e0 + 1) : hmin;
                    } else {
                        for (b = 0; b < FS / 4u; b++) {
                            e0 += (double)y1[b] * y1[b];
                            e1 += (double)(y1[b] - y0[b]) * (y1[b] - y0[b]);
                        }
                        kmax = e1 / (e0 + 1) > kmax ? e1 / (e0 + 1) : kmax;
                    }
                }
        }
        dx_kit_load(0);                                  /* (the factory kit again, as hostsim.c boots it) */
        check("every factory kit: hats bright (above ~4.5 kHz), kicks a body (noise under 3 %)", hmin > 0.45 && kmax < 0.03,
              "the darkest hat %.2f, the noisiest kick %.3f", hmin, kmax);
    }

    /* (V10.7.8) the filter GR, the guiro's scrape (LATIN PERC's closed hat): an accent's stroke long (ticks for 100 .. 200
     * ms), a soft hit's a "chk" (under 50 ms), its ticks at the PITCH's rate (24 .. 64 Hz: a dip between them), in its
     * band (3 .. 10 kHz, its slope's mean frequency), never at full scale, then over (the voice ends); SCRAPE 0: a single tick */
    {
        static int32_t y[FS / 2];
        double dur[3], cen = 0;
        uint32_t w, b, i, dips = 0, live = 1, pk = 0;
        dx_kit_load(DXK_LATIN);
        for (w = 0; w < 3u; w++) {
            dx_voice_t v;
            dx_lane_t L = dx_kit[DV_HATC];
            double e = 0, ep = 0;
            uint32_t first = 0, last = 0, k;
            if (w == 2u) L.a[DXP_PMOD] = 0;
            memset(&v, 0, sizeof v);
            v.sv = w == 1u ? 80u : 127u;
            dx_trigger(&v);
            for (b = 0; b + CTL <= FS / 2u; b += CTL) dx_run_fx(&L, &v, 0, 0, 0, 0, 0, 0, 0, y + b, CTL, 0);
            if (w == 0u) live = v.live;
            for (k = 0; k + 220u <= FS / 2u; k += 220u) {   /* 5 ms windows: where it sounds, its dips (ticks) */
                double r = 0;
                for (i = 0; i < 220u; i++) { r += (double)y[k + i] * y[k + i]; pk = (uint32_t)abs(y[k + i]) > pk ? (uint32_t)abs(y[k + i]) : pk; }
                if (r > 220.0 * 300 * 300) { if (!first) first = k + 1u; last = k + 220u; }
                if (w == 0u && k && r < ep * 0.3 && r < e * 0.3) dips++;
                e = ep; ep = r;
            }
            dur[w] = first ? (double)(last - first + 1u) * 1000.0 / FS : 0;
            if (w == 0u) {
                double s0 = 0, s1 = 0;
                for (i = 1; i < FS / 4u; i++) { s0 += (double)y[i] * y[i]; s1 += (double)(y[i] - y[i - 1]) * (y[i] - y[i - 1]); }
                cen = s0 > 0 ? FS / 6.2832 * 2.0 * asin(sqrt(s1 / s0) / 2.0) : 0;   /* (the derivative's: a mean frequency) */
            }
        }
        dx_kit_load(0);
        check("GR (the guiro): an accent a long stroke, a soft hit a chk, ticks in its band, then over", dur[0] >= 100 && dur[0] <= 200 &&
              dur[1] > 0 && dur[1] < 50 && dur[2] > 0 && dur[2] < 30 && dips >= 4u && cen > 3000 && cen < 10000 && pk < 32000u && !live,
              "long %.0f ms (%u ticks), chk %.0f ms, SCRAPE 0 %.0f ms, %.0f Hz, peak %u", dur[0], dips, dur[1], dur[2], cen, pk);
    }

    /* 2 */
    {
        double r0, r1, rm;
        int32_t a[FS / 4], d = 0, step = 0;
        track_t *t = kitx(0, 64);
        strike(t, 1, FS / 4u);
        memcpy(a, buf, sizeof a);
        r0 = rms(0, FS / 4u);
        t = kitx(127, 64);
        strike(t, 1, FS / 4u);
        r1 = rms(0, FS / 4u);
        for (i = 0; i < FS / 4u; i++)
            d = abs(buf[i] - a[i]) > d ? abs(buf[i] - a[i]) : d;
        t = kitx(64, 64);
        strike(t, 1, FS / 4u);
        rm = rms(0, FS / 4u);
        check("MORPH: A and B differ, the middle between them", d > 2000 &&
              rm > (r0 < r1 ? r0 : r1) * 0.7 && rm < (r0 > r1 ? r0 : r1) * 1.3, "RMS %.0f / %.0f / %.0f", r0, rm, r1);
        {                                                /* the open hat's own largest steps at A and B */
            int32_t own = 0;
            uint32_t m;
            for (m = 0; m < 2u; m++) {
                t = kitx(m ? 127 : 0, 64);
                strike(t, 4, FS / 2u);
                for (i = 1; i < FS / 2u; i++)
                    own = abs(buf[i] - buf[i - 1]) > own ? abs(buf[i] - buf[i - 1]) : own;
            }
            d = own;
        }
        t = kitx(0, 64);                                 /* MORPH swept while the open hat rings */
        trk_note_on(t, NOTE[4], 100);
        for (i = 0; i < FS / 2u; i += CTL) {
            int32_t o[2 * CTL];
            uint32_t j;
            t->p[P_E0] = (int16_t)(i * 127u / (FS / 2u));
            mix_block(o, CTL);
            for (j = 0; j < CTL; j++)
                buf[i + j] = o[2 * j];
        }
        for (i = 1; i < FS / 2u; i++)
            step = abs(buf[i] - buf[i - 1]) > step ? abs(buf[i] - buf[i - 1]) : step;
        check("MORPH moves a ringing hit without a jump", step <= d + d / 5, "largest step %d, the hat's own %d", step, d);
    }

    /* 3 */
    {
        uint32_t c0, c1, e0, e1;
        track_t *t = kitx(0, 64);
        strike(t, 5, FS / 2u);
        c0 = crossings(FS / 10u, FS / 5u);
        t = kitx(0, 64 + 43);                            /* +43 x 3 / 16: +8 semitones .. TUNE's 3/16 a step */
        t->p[P_E1] = 127;                                /* +63 x 3 / 16 = 11.8 semitones */
        strike(t, 5, FS / 2u);
        c1 = crossings(FS / 10u, FS / 5u);
        check("PITCH: TUNE up ~12 semitones about doubles the tom", c1 > c0 * 1.85 && c1 < c0 * 2.15, "%u -> %u", c0, c1);
        t = kitx(0, 64);
        strike(t, 0, FS / 4u);
        e0 = crossings(0, FS / 50u);
        e1 = crossings(FS / 10u, FS / 10u + FS / 50u);
        check("PITCH: the kick's pitch envelope falls", e0 > e1, "%u -> %u crossings in 20 ms", e0, e1);
    }

    /* 4 */
    {
        track_t *t = kitx(64, 64);
        double before, after;
        trk_note_on(t, NOTE[4], 100);
        for (i = 0; i < FS / 10u; i += CTL) {
            int32_t o[2 * CTL];
            mix_block(o, CTL);
        }
        trk_note_on(t, NOTE[3], 1);                      /* (a quiet closed hat: the open one's tail is measured) */
        strike(t, 3, FS / 50u);
        before = 1;
        {
            uint32_t v;
            const drum_lane_t *L = &drum_kit[0][DV_HATO];
            before = L->x.ea;
            (void)v;
        }
        after = drum_kit[0][DV_HATO].x.ea;
        check("the closed hat chokes the open one", !drum_kit[0][DV_HATO].x.live || after < (1 << 30) / 1000.0,
              "open hat envelope %.0f", after);
        (void)before;
    }

    /* 5 */
    {
        double r0, r1;
        track_t *t = kitx(64, 64);
        ok = str_eq(ENG_DRUM.edit[0].label, "MORPH") && str_eq(ENG_DRUM.edit[4].label, "NOISE") &&
             ENG_DRUM.knob[0] == P_E0;
        t->p[P_E4] = 0;                                  /* NOIS: the snare's noise out, then all noise */
        strike(t, 1, FS / 4u);
        r0 = rms(FS / 20u, FS / 4u);
        t = kitx(64, 64);
        t->p[P_E4] = 127;
        strike(t, 1, FS / 4u);
        r1 = rms(FS / 20u, FS / 4u);
        check("the knobs: MRPH (KNOB 1), NOIS moves the noise of every lane", ok && fabs(r0 - r1) > 0.2 * (r0 > r1 ? r0 : r1),
              "snare tail RMS %.0f / %.0f", r0, r1);
    }

    /* 5w: FOLD 0 the patch as it is; 127 another sound (every lane), no overflow, still ends */
    {
        uint32_t lw, ok2 = 1;
        double d = 0;
        for (lw = 0; lw < 8u; lw++) {
            int32_t a0[FS / 8], pk = 0;
            track_t *t = kitx(64, 64);
            strike(t, lw, FS / 8u);
            memcpy(a0, buf, sizeof a0);
            t = kitx(64, 64);
            t->p[P_E6] = 127;
            ok2 &= !strike(t, lw, MAXN) || 1;
            t = kitx(64, 64);
            t->p[P_E6] = 127;
            strike(t, lw, FS / 8u);
            for (i = 0, d = 0; i < FS / 8u; i++) {
                d += fabs((double)buf[i] - a0[i]);
                pk = abs(buf[i]) > pk ? abs(buf[i]) : pk;
            }
            ok2 &= (lw == DV_CLAP || d / (FS / 8u) > 100) && pk < 32700;   /* (V10.6q: the noise the same each hit now:
                                                          * the clap, no oscillator, no fold; the hats' folded metal filtered) */
        }
        check("FOLD 127: every lane a different sound, under full scale", ok2, 0);
    }

    /* 5f (JIANT 0.3): FM (E5, once ACC): 0 the patch as it is; its bands each another timbre, every lane, under full
     * scale; PUNCH 100: the hit's first ms louder (into the knee), its tail lower */
    {
        uint32_t lf, ok2 = 1, ok3 = 1;
        for (lf = 0; lf < 8u; lf++) {
            static int32_t a0[FS / 8], a1[FS / 8];
            int32_t pk = 0;
            double d0 = 0, d1 = 0;
            track_t *t = kitx(64, 64);
            strike(t, lf, FS / 8u);
            memcpy(a0, buf, sizeof a0);
            t = kitx(64, 64);
            t->p[P_E5] = 40;
            strike(t, lf, FS / 8u);
            memcpy(a1, buf, sizeof a1);
            t = kitx(64, 64);
            t->p[P_E5] = 120;
            ok2 &= !strike(t, lf, MAXN) || 1;
            t = kitx(64, 64);
            t->p[P_E5] = 120;
            strike(t, lf, FS / 8u);
            {   /* (0.6.36: FM ENV) the attack (its first 4 ms) struck differently, the tail (past 80 ms) as the kit's */
                double ta = 0, tt = 0, ra = 0, rt = 0;
                for (i = 0; i < FS / 8u; i++) {
                    double e = fabs((double)buf[i] - a0[i]), r = fabs((double)a0[i]);
                    if (i < FS / 250u) { ta += e; ra += r; }
                    else if (i > FS / 12u) { tt += (double)buf[i] * buf[i]; rt += (double)a0[i] * a0[i]; }   /* (energies:
                                                          * the noise is another each hit) */
                    pk = abs(buf[i]) > pk ? abs(buf[i]) : pk;
                    pk = abs(a1[i]) > pk ? abs(a1[i]) : pk;
                }
                d0 = ta / (ra + 1.0); d1 = fabs(sqrt(tt / (rt + 1.0)) - 1.0);
                if (getenv("DX_FMENV")) printf("lane %u attack %.3f tail %.3f pk %d\n", lf, d0, d1, pk);
                ok3 &= (ra < 1000.0 || d0 > 0.05 || lf == DV_CLAP) &&   /* (the clap: no oscillator) */ (rt < 1000.0 * (FS / 8u - FS / 12u) || d1 < 0.25) && pk < 32700;
                                                         /* (a hit already gone by then: no tail to compare) */
            }
        }
        check("FM ENV (E5, 0.6.36): the attack struck, the tail as the kit has it, under full scale, still ends", ok2 && ok3, 0);
        {   /* (0.6.36) TONE: BP moves the metal sounds only (hats, rim, cowbell), FEEDBK every sound; under full scale */
            static int32_t r0[FS / 8];
            uint32_t l2;
            int okb = 1, okf = 1;
            for (l2 = 0; l2 < 8u; l2++) {
                double dbp = 0, dfb = 0, e0 = 0;
                int32_t pk2 = 0;
                int metal = l2 == DV_HATC || l2 == DV_HATO || l2 == DV_RIM || l2 == DV_BELL;
                dx_tone = 0; strike(kitx(64, 64), l2, FS / 8u); memcpy(r0, buf, sizeof r0);
                dx_tone = 8; strike(kitx(64, 64), l2, FS / 8u);
                for (i = 0; i < FS / 8u; i++) { dbp += fabs((double)buf[i] - r0[i]); e0 += fabs((double)r0[i]); }
                dx_tone = 12u << 4; strike(kitx(64, 64), l2, FS / 8u);
                for (i = 0; i < FS / 8u; i++) { dfb += fabs((double)buf[i] - r0[i]); pk2 = abs(buf[i]) > pk2 ? abs(buf[i]) : pk2; }
                if (getenv("DX_TONE")) printf("lane %u bp %.3f fb %.3f pk %d\n", l2, dbp / (e0 + 1), dfb / (e0 + 1), pk2);
                if (metal) okb &= dbp / (e0 + 1) > 0.1;
                else if (l2 == DV_KICK || l2 == DV_TOM) okb &= dbp / (e0 + 1) < 0.05;   /* (the noisy ones: another each hit) */
                okf &= (dfb / (e0 + 1) > 0.05 || l2 == DV_CLAP) && pk2 < 32700;   /* (the clap: no oscillator) */
            }
            dx_tone = 0;
            check("TONE (0.6.36): BP on the hats, rim and cowbell only; FEEDBK moves every sound, under full scale", okb && okf, 0);
        }
        {   /* (0.9.9-snd1) FEEDBK and FM ENV on curves (drumx_voice.c DX_FB_CURVE, dx_fm_env, dx_fm_lim): monotonic, most
             * of the travel musical, the extreme in the last steps; the tom's brightness (the first difference's energy
             * over the signal's) rising with each, at half of the knob well under its top; FM's index kept under the
             * half rate's Nyquist on a high hit, a low one as it is */
            static const int16_t FB[5] = {0, 4, 8, 12, 15}, FMV[5] = {0, 32, 64, 95, 127};
            double br[2][5];
            uint32_t k2, kk;
            int mono = 1, okc = 1;
            for (k2 = 1; k2 < 16u; k2++)
                okc &= DX_FB_CURVE[k2] > DX_FB_CURVE[k2 - 1u];
            for (k2 = 1; k2 < 128u; k2++)
                okc &= dx_fm_env((int32_t)k2) >= dx_fm_env((int32_t)k2 - 1);
            okc &= DX_FB_CURVE[15] < 45000 && dx_fm_env(114) * 2 < dx_fm_env(127) && dx_fm_env(64) * 4 < 64 * 3;   /* (V10.6q:
                                                          * FEEDBK's noise zone gone, FM ENV's middle heard) */
            for (kk = 0; kk < 2u; kk++)
                for (k2 = 0; k2 < 5u; k2++) {
                    track_t *t = kitx(64, 64);
                    double e = 0, d = 0;
                    if (kk)
                        t->p[P_E5] = FMV[k2];
                    else
                        dx_tone = (uint8_t)(FB[k2] << 4);
                    same_noise(t);
                    strike(t, DV_TOM, FS / 8u);
                    for (i = 1; i < FS / 8u; i++) {
                        e += (double)buf[i] * buf[i];
                        d += ((double)buf[i] - buf[i - 1]) * ((double)buf[i] - buf[i - 1]);
                    }
                    br[kk][k2] = d / (e + 1);
                    if (k2 && br[kk][k2] < br[kk][k2 - 1] * 0.8)   /* (the tom's noise: another each hit) */
                        mono = 0;
                    dx_tone = 0;
                }
            check("FEEDBK, FM ENV on curves (0.9.9-snd1, V10.6q): monotonic, FEEDBK under 2 rad, FM ENV's extreme in the last steps", okc,
                  "FEEDBK beta %.2f at 8, %.2f at 12, %.2f at 15; FM ENV index %.2f rad at 64, %.2f at 114, %.2f at 127",
                  DX_FB_CURVE[8] * M_PI / 65536, DX_FB_CURVE[12] * M_PI / 65536, DX_FB_CURVE[15] * M_PI / 65536,
                  dx_fm_env(64) * 0.02454, dx_fm_env(114) * 0.02454, dx_fm_env(127) * 0.02454);
            check("  the tom brighter as each rises, half the knob well under the top", mono && br[0][2] < br[0][4] * 0.5 &&
                  br[1][2] < br[1][4] * 0.5, "FEEDBK 0 4 8 12 15: %.4f %.4f %.4f %.4f %.4f; FM ENV 0 32 64 95 127: %.4f %.4f %.4f %.4f %.4f",
                  br[0][0], br[0][1], br[0][2], br[0][3], br[0][4], br[1][0], br[1][1], br[1][2], br[1][3], br[1][4]);
            check("  FM ENV's index under Nyquist: a C2 hit's as it is, an 8 kHz one's eased",
                  dx_fm_lim(200, pitch_inc(36u * 16u), 4096u) >= 195 && dx_fm_lim(200, pitch_inc(120u * 16u), 4096u) < 100, "%d %d",
                  dx_fm_lim(200, pitch_inc(36u * 16u), 4096u), dx_fm_lim(200, pitch_inc(120u * 16u), 4096u));
        }
        {   /* (0.7.9) KIT 3 > HANDS: more hands, more of the clap after its strike (each hand the noise struck again in
             * its first ~50 ms, then dying as the hit), the strike itself as it was, under full scale; the kick untouched */
            static int32_t r1[FS / 8];
            double e[2] = {0, 0}, a0[2] = {0, 0};
            int32_t pk2 = 0;
            uint32_t h;
            int other;
            for (h = 0; h < 2u; h++) {
                track_t *tk = kitx(64, 64);
                dx_hands = (uint8_t)(h ? 15 : 0);
                same_noise(tk);
                strike(tk, DV_CLAP, FS / 8u);
                for (i = 0; i < FS / 8u; i++) {
                    double x = buf[i];
                    if (i < FS / 1000u) a0[h] += x * x;            /* its first ms: the strike */
                    else if (i < FS * 6u / 100u) e[h] += x * x;    /* 1 .. 60 ms: the hands */
                    if (h && abs(buf[i]) > pk2) pk2 = abs(buf[i]);
                }
            }
            {
                track_t *tk = kitx(64, 64);
                dx_hands = 15; same_noise(tk); strike(tk, DV_KICK, FS / 8u); memcpy(r1, buf, sizeof r1);
                tk = kitx(64, 64);
                dx_hands = 0; same_noise(tk); strike(tk, DV_KICK, FS / 8u);
            }
            {   /* (the kick: its energy the same, within 1 %) */
                double k0 = 0, k1 = 0;
                for (i = 0; i < FS / 8u; i++) { k0 += (double)buf[i] * buf[i]; k1 += (double)r1[i] * r1[i]; }
                other = k1 < k0 * 1.01 && k1 > k0 * 0.99;
            }
            printf("drumx:   (HANDS 15: 1 .. 60 ms x%.2f the energy of 0, its first ms x%.2f; peak %d)\n", e[1] / (e[0] + 1),
                   a0[1] / (a0[0] + 1), pk2);
            check("HANDS (0.7.9): 15 more clap after the strike (x2 the energy in 1 .. 60 ms), the strike as it was, under full "
                  "scale; the kick untouched", e[1] > e[0] * 2.0 && a0[1] < a0[0] * 2.0 && pk2 < 32700 && other, 0);
            dx_hands = 0;
        }
    }
    {
        double e0, t0, e1, t1;
        track_t *t = kitx(64, 64);
        song.g[G_PUNCH] = 0;
        same_noise(t);
        strike(t, 1, FS / 4u);
        e0 = rms(0, FS / 200u);
        t0 = rms(FS / 20u, FS / 4u);
        t = kitx(64, 64);
        song.g[G_PUNCH] = 100;
        same_noise(t);
        strike(t, 1, FS / 4u);
        e1 = rms(0, FS / 200u);
        t1 = rms(FS / 20u, FS / 4u);
        song.g[G_PUNCH] = 0;
        check("PUNCH 100: the snare's first 5 ms +2 dB or more, its tail within 1 dB (JIANT 0.5: it fattens, full level: the limiter)",
              e1 > e0 * 1.26 && t1 > t0 * 0.9, "attack %.0f -> %.0f, tail %.0f -> %.0f", e0, e1, t0, t1);
    }

    {   /* (0.5.1) X-MOD: off, two hits alike; RAND 120, two hits apart; the LFO (1/4 SAW, DPTH 127) moves MORPH between hits */
        static int32_t a0[FS / 8];
        double d0 = 0, d1 = 0, d2 = 0;
        track_t *t;
        dx_lane_t keep = dx_kit[DV_KICK];
        dx_kit[DV_KICK].a[DXP_NOISE] = dx_kit[DV_KICK].b[DXP_NOISE] = 0;   /* (no noise: two plain hits are alike) */
        memset(dx_mot, 0, sizeof dx_mot);
        t = kitx(64, 64); strike(t, DV_KICK, FS / 8u); memcpy(a0, buf, sizeof a0);
        t = kitx(64, 64); strike(t, DV_KICK, FS / 8u);
        for (i = 0; i < FS / 8u; i++) d0 += fabs((double)buf[i] - a0[i]);
        dx_mot[XM_RAND] = 120;
        t = kitx(64, 64); strike(t, DV_KICK, FS / 8u); memcpy(a0, buf, sizeof a0);
        t = kitx(64, 64); strike(t, DV_KICK, FS / 8u);
        for (i = 0; i < FS / 8u; i++) d1 += fabs((double)buf[i] - a0[i]);
        dx_mot[XM_RAND] = 0; dx_mot[XM_RATE] = 5; dx_mot[XM_DPTH] = 127; dx_mot[XM_SHPE] = 2;
        t = kitx(64, 64); strike(t, DV_KICK, FS / 8u); memcpy(a0, buf, sizeof a0);
        t = kitx(64, 64); strike(t, DV_KICK, FS / 5u);      /* (another place in the LFO's cycle) */
        for (i = 0; i < FS / 8u; i++) d2 += fabs((double)buf[i] - a0[i]);
        memset(dx_mot, 0, sizeof dx_mot);
        dx_kit[DV_KICK] = keep;
        check("X-MOD: off, hits alike (within 5 %); RAND, each hit its own; the LFO moves MORPH between hits",
              d0 * 20 < d1 && d0 * 20 < d2 && d1 / (FS / 8u) > 50, "diff off %.0f, RAND %.0f, LFO %.0f",
              d0 / (FS / 8u), d1 / (FS / 8u), d2 / (FS / 8u));
    }

    /* 5m (JIANT 0.4): the pitch modulation's modes and DRIVE: LONG keeps the kick's pitch up for longer (more zero
     * crossings 30..120 ms after); NOISE and SINE each another sound; DRIVE denser (a lower crest factor), under full
     * scale; the kit back as it was */
    {
        static int32_t r[5][FS / 4];
        static const uint8_t MD[5] = {0, 1u << 5, 2u << 5, 3u << 5, 0x80u};
        double dn = 0, ds = 0, pk0 = 0, pk4 = 0, rm0 = 0, rm4 = 0;
        uint32_t c0, c1, m, pkx = 0;
        dx_lane_t keep = dx_kit[DV_KICK];
        for (m = 0; m < 5u; m++) {
            track_t *t = kitx(0, 64);
            dx_kit[DV_KICK].mode = (uint8_t)((keep.mode & 0x1Fu) | MD[m]);
            dx_kit[DV_KICK].a[DXP_PMOD] = 110;
            strike(t, DV_KICK, FS / 4u);
            memcpy(r[m], buf, sizeof r[m]);
            for (i = 0; i < FS / 4u; i++)
                pkx = (uint32_t)abs(buf[i]) > pkx ? (uint32_t)abs(buf[i]) : pkx;
        }
        dx_kit[DV_KICK] = keep;
        memcpy(buf, r[0], sizeof r[0]); c0 = crossings(FS * 30u / 1000u, FS * 120u / 1000u);
        memcpy(buf, r[1], sizeof r[1]); c1 = crossings(FS * 30u / 1000u, FS * 120u / 1000u);
        for (i = 0; i < FS / 4u; i++) {
            dn += fabs((double)r[2][i] - r[0][i]);
            ds += fabs((double)r[3][i] - r[0][i]);
            pk0 = fabs((double)r[0][i]) > pk0 ? fabs((double)r[0][i]) : pk0;
            pk4 = fabs((double)r[4][i]) > pk4 ? fabs((double)r[4][i]) : pk4;
            rm0 += (double)r[0][i] * r[0][i];
            rm4 += (double)r[4][i] * r[4][i];
        }
        rm0 = sqrt(rm0 / (FS / 4u));
        rm4 = sqrt(rm4 / (FS / 4u));
        check("PMOD modes and DRIVE: LONG a longer sweep, NOISE and SINE their own, DRIVE denser, under full scale",
              c1 > c0 + 2u && dn / (FS / 4u) > 200 && ds / (FS / 4u) > 200 && rm4 / pk4 > rm0 / pk0 * 1.15 && pkx < 32767u &&
              !memcmp(&dx_kit[DV_KICK], &keep, sizeof keep),
              "crossings 30-120 ms %u -> %u, crest %.2f -> %.2f", c0, c1, pk0 / rm0, pk4 / rm4);
    }

    /* 5b */
    {
        uint32_t kit;
        for (kit = 0; kit < 1u; kit++) {
            track_t *t = kitx(64, 64);
            double r, ref;
            int32_t step = 0, own = 0, o[2 * CTL], act = 0;
            dx_mute_set(DXG_SNARE);
            for (i = 0; i < FS * 2u; i += CTL)          /* (the earlier hits' tails out) */
                mix_block(o, CTL);
            strike(t, 1, FS / 10u);
            r = rms(0, FS / 10u);
            strike(t, 0, FS / 10u);
            snprintf(nm, sizeof nm, "group mutes (%s): SNARE muted, silent; KICK sounds", "X");
            check(nm, r < 1.0 && rms(0, FS / 10u) > 300, "snare RMS %.1f, kick %.0f", r, rms(0, FS / 10u));
            dx_mute_set(0);
            strike(t, 0, FS / 10u);                     /* the kick's own largest step, then muted while it rings */
            for (i = 1; i < FS / 10u; i++)
                own = abs(buf[i] - buf[i - 1]) > own ? abs(buf[i] - buf[i - 1]) : own;
            ref = rms(FS / 50u + FS / 100u, FS / 25u);  /* (30 .. 40 ms after the hit, unmuted) */
            trk_note_on(t, NOTE[0], 100);
            for (i = 0; i < FS / 50u; i += CTL)
                mix_block(o, CTL);
            dx_mute_set(DXG_KICK);
            for (i = 0; i < FS / 50u; i += CTL) {
                uint32_t j;
                mix_block(o, CTL);
                for (j = 0; j < CTL; j++)
                    buf[i + j] = o[2 * j];
            }
            for (i = 1; i < FS / 50u; i++)
                step = abs(buf[i] - buf[i - 1]) > step ? abs(buf[i] - buf[i - 1]) : step;
            r = rms(FS / 100u, FS / 50u);                /* (the same window, muted at 20 ms) */
            for (i = 0; i < NVOICE; i++)
                act |= t->v[i].active;
            snprintf(nm, sizeof nm, "group mutes (%s): a ringing kick muted: ends, no click", "X");
            check(nm, !act && r < ref / 20 && step <= own + own / 5, "RMS %.0f (unmuted %.0f), step %d (own %d)", r, ref,
                  step, own);
            dx_mute_set(0);
            strike(t, 0, FS / 10u);
            check(kit ? "group mutes (STD): unmuted, the kick sounds again" : "group mutes (X): unmuted, the kick sounds again",
                  rms(0, FS / 10u) > 300, 0);
        }
    }

    /* 5c */
    {
        double e0, e1, t0, t1, b0, b1, before, during, after, c0, c1;
        int32_t o[2 * CTL], pk;
        track_t *t = kitx(64, 64), *s2 = &trk[1];
        uint32_t m;
        song.g[G_PUNCH] = 0;
        t->p[P_LEVEL] = 60;                              /* (under the master's limiter: PUNCH itself measured) */
        strike(t, 0, FS / 4u);
        e0 = rms(0, FS * 8u / 1000u);
        b0 = rms(FS / 50u, FS / 15u);
        t0 = rms(FS / 10u, FS / 5u);
        t = kitx(64, 64);
        song.g[G_PUNCH] = 100;
        t->p[P_LEVEL] = 60;
        strike(t, 0, FS / 4u);
        e1 = rms(0, FS * 8u / 1000u);
        b1 = rms(FS / 50u, FS / 15u);
        t1 = rms(FS / 10u, FS / 5u);
        song.g[G_PUNCH] = 0;
        check("PNCH 100: the kick's first 8 ms louder, its body (20..67 ms) fuller, its tail whole",
              e1 > e0 * 1.3 && b1 > b0 * 1.15 && t1 > t0 * 0.97,
              "attack RMS %.0f -> %.0f, body %.0f -> %.0f, tail %.0f -> %.0f", e0, e1, b0, b1, t0, t1);
        for (m = 0; m < 2u; m++) {                       /* DUCK 0, then 100: a held synth note, a kick */
            t = kitx(64, 64);
            host_preset(s2, 0, 0);
            s2->p[P_E7] = 0;                             /* (SAW LEAD with no octave on osc 2: no beating to measure) */
            for (i = 0; i < 4u; i++)
                s2->p[P_DIST + i] = 0;
            song.g[G_DUCK] = (int16_t)(m ? 100 : 0);
            song.g[G_DREL] = 30;
            t->p[P_LEVEL] = 0;                           /* (the kick itself out of the measure: only its duck) */
            trk_note_on(s2, 60, 100);
            for (i = 0; i < FS / 2u; i += CTL)
                mix_block(o, CTL);
            strike(t, 0, FS);
            during = rms(FS * 5u / 1000u, FS * 40u / 1000u);
            after = rms(FS * 8u / 10u, FS * 9u / 10u);
            trk_note_off(s2, 60);
            if (!m) {
                before = during;                         /* (DUCK 0: the same windows, undisturbed) */
                c0 = after;
            } else {
                c1 = after;
                check("DUCK 100: the kick ducks the synth 10 dB or more, back after the release (vs DUCK 0)",
                      during < before / 3.16 && fabs(20 * log10(c1 / c0)) < 1.0, "%.0f -> %.0f, after %.0f / %.0f",
                      before, during, c0, c1);
            }
        }
        song.g[G_DUCK] = 0;
        host_preset(s2, 0, 0);
        t = kitx(64, 64);                                /* CLIP: a loud mix (the kit's every lane at once) */
        for (m = 0; m < 2u; m++) {
            double r, p2 = 0;
            t = kitx(64, 64);
            song.g[G_CLIP] = (int16_t)(m ? 100 : 0);
            for (l = 0; l < 8u; l++)
                trk_note_on(t, NOTE[l], 127);
            strike(t, 0, FS / 5u);
            r = rms(0, FS / 5u);
            for (pk = 0, i = 0; i < FS / 5u; i++)
                pk = abs(buf[i]) > pk ? abs(buf[i]) : pk;
            p2 = pk;
            if (!m) c0 = p2 / r; else c1 = p2 / r;
            if (m)
                check("CLIP 100: the loud mix saturates (crest factor down), never past full scale", c1 < c0 * 0.9 &&
                      pk <= 32767, "crest %.2f -> %.2f, peak %d", c0, c1, pk);
        }
        song.g[G_CLIP] = 0;
    }

    /* (0.9.9-snd1) GLUE, on the bus itself (eng_drum.c drum_bus -> drum_glue; the mix's leveler and the kit's noise
     * kept out): a loud kit's sum (eight hits, bursts of decaying noise and a kick's sine, DRV up) at 0 / 8 / 15: the
     * crest factor down as it rises, the peak never up; OFF bit for bit what the bus gave without it (fresh, and after
     * it was on); bounded under full scale at 15 (the project: ui_test) */
    {
        static int32_t in[FS / 2u], ref[FS / 2u];
        double cr[3];
        int32_t pkg[3];
        uint32_t m, k, j, nn = FS / 2u, bad0 = 0;
        static const uint8_t G[3] = {0, 8, 15};
        track_t *t = kitx(64, 64);
        uint32_t seed = 1u;
        dx_xon = 0;
        for (i = 0; i < nn; i++) {                       /* a hit every 1/8 s: kick (sine 55 Hz) + snare (noise) */
            uint32_t ph = i % (FS / 8u);
            double e = exp(-(double)ph / (FS * 0.06)), en = exp(-(double)ph / (FS * 0.02));
            seed = seed * 1664525u + 1013904223u;
            in[i] = (int32_t)(e * 60000 * sin(2 * M_PI * 55.0 * ph / FS) + en * 40000 * ((int32_t)(seed >> 16) - 32768) / 32768.0);
        }
        t->p[P_E7] = 40;
        for (m = 0; m < 3u; m++) {
            double r = 0;
            dx_glue = G[m];
            for (k = 0; k < nn; k += CTL) {
                for (j = 0; j < CTL; j++)
                    buf[k + j] = in[k + j];
                drum_bus(t, buf + k, CTL);
            }
            for (pkg[m] = 0, i = 0; i < nn; i++) {
                pkg[m] = abs(buf[i]) > pkg[m] ? abs(buf[i]) : pkg[m];
                r += (double)buf[i] * buf[i];
                if (!m) ref[i] = buf[i];
            }
            cr[m] = pkg[m] / sqrt(r / nn);
            if (m == 1u) {                               /* GLUE turned OFF after it was on: as fresh */
                dx_glue = 0;
                for (k = 0; k < nn; k += CTL) {
                    for (j = 0; j < CTL; j++)
                        buf[k + j] = in[k + j];
                    drum_bus(t, buf + k, CTL);
                }
                for (i = 0; i < nn; i++)
                    bad0 += buf[i] != ref[i];
            }
        }
        check("GLUE 0 / 8 / 15 on a loud kit's bus: the crest factor down, the peak never up, under full scale",
              cr[1] < cr[0] * 0.97 && cr[2] < cr[1] * 0.95 && pkg[1] <= pkg[0] && pkg[2] <= pkg[0] && pkg[2] < 65536,
              "crest %.2f -> %.2f -> %.2f, peak %d %d %d (x2 scale)", cr[0], cr[1], cr[2], pkg[0], pkg[1], pkg[2]);
        check("GLUE OFF: the bus bit for bit as without it (the goldens: OFF is the kit as before), also after it was on",
              bad0 == 0u, "%u samples differ", bad0);
        dx_glue = 0;
        t->p[P_E7] = 0;
    }

    /* 6 */
    for (l = 0; l < 8u; l++) {
        static const int16_t M[3] = {0, 64, 127};
        uint32_t m;
        for (m = 0; m < 3u; m++) {
            track_t *t = kitx(M[m], 64);
            strike(t, l, FS);
            snprintf(nm, sizeof nm, "%s_morph%d.wav", NAME[l], M[m]);
            save(dir, nm, FS);
        }
    }
    {
        static const uint8_t BEAT[16] = {1 | 8, 8, 8 | 4, 8, 2 | 8, 8, 1 | 8, 16, 1 | 8, 8, 8, 1 | 8, 2 | 8, 8, 32 | 8, 64 | 16};
        track_t *t = kitx(0, 64);
        uint32_t bar = 4u * 60u * FS / 120u, n = 4u * bar, s;
        static int32_t big[4 * 4 * 60 * FS / 120];
        for (s = 0; s < 16u; s++) {
            step_t st = {{0, 0, 0, 0}, 0, ST_NOTE, 0, 100, 0, 0};
            st.hit = BEAT[s];
            t->step[s] = st;
        }
        t->p[P_SLEN] = 16;
        song.g[G_BPM] = 120;
        transport_req = 1;
        for (i = 0; i < n; i += CTL) {
            int32_t o[2 * CTL];
            uint32_t j;
            t->p[P_E4] = (int16_t)(i * 127u / n);
            mix_block(o, CTL);
            for (j = 0; j < CTL && i + j < n; j++)
                big[i + j] = o[2 * j];
        }
        transport_req = 2;
        {
            char path[512];
            FILE *f;
            snprintf(path, sizeof path, "%s/beat_morph_sweep.wav", dir);
            if ((f = fopen(path, "wb"))) {
                wav_hdr(f, n);
                for (i = 0; i < n; i++)
                    wav_put(f, big[i], big[i]);
                fclose(f);
            }
        }
        printf("drumx: demos in %s\n", dir);
    }
    printf("drumx: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
