/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* VOICE: formant oscillator, sung vowels from the keyboard. 4 voices.
 * After D. H. Klatt, "Software for a cascade/parallel formant synthesizer",
 * JASA 67 (1980): a glottal source (derivative of a Rosenberg (1971) pulse,
 * no DC) into four two-pole resonators in cascade, F1 -> F2 -> F3 -> F4.
 * Design follows klattsch by Tony Gies (MIT, design reference only, no code
 * copied). Formant data from Klatt (1980) and Hillenbrand et al. (1995).
 *
 *   source  rising half-sine while opening (Tp), falling quarter-sine while
 *           closing (Tn), 0 while closed; the closure step goes through blep()
 *   BUZZ    open quotient 0.9 .. 0.35 and a one-pole spectral tilt
 *   BREATH  noise, louder while the glottis is open, mixed before the
 *           resonators; at the top the voicing fades out (whisper)
 *   F1..F4  y = A x + B y1 + C y2, C = -r^2, B = 2 r cos(2 pi F / FS),
 *           A = 1 - B - C (unity gain at DC), Q30 coefficients per block;
 *           F4 is fixed at 3300 Hz and moves only with SHIFT / key tracking
 *   PRES    out = y + k (y - y[-1]), a first-difference shelf; k follows BUZZ
 *
 * Voice state: ph[0] glottal phase, ph[1..2] F4 y1 / y2, s[0..5] F1..F3
 * y1 / y2, s[6] tilt, s[7] TALK progress (Q24, bits 0..24) and the note's
 * random vowel (bits 25..31). Formant key tracking is fixed at 30 / 127.
 *
 * (JIANT 0.8.5) WORDS: with LANG on (EDIT > WORDS) the track sings syllables, words and lyrics (sing.c): the formants
 * and the source's gains come from the track's articulation (sing_block, the block hook), the same source and
 * cascade render them (formant_word_render), with noise for the consonants: aspiration into the cascade, frication
 * into F4 (tuned to the consonant while it sounds). The EDIT values read POS THEME ARTIC SHIFT | BUZZ BREATH Q LOOP
 * then (formant_desc). LANG OFF: every sound as before, bit for bit.
 *
 * (0.9.8) A singer: WORDS' frication band-passed (1 - z^-2) and softer, a vibrato on held notes, jitter and shimmer each
 * glottal period, a softer source; VOICE's glide (LANG OFF too) an S-curve in pitch (glide_sung, fmt_sung). */
#define FMT_FOLOW 30
#include "sing.c"
#define FMT_TALK_MASK 0x01FFFFFF

/* formants (Hz) of A E I O U: adult male means, Hillenbrand et al., JASA 97
 * (1995) (/a/ hod, /E/ head, /i/ heed, /o/ hoed, /u/ who'd); bandwidths (Hz)
 * in the range of Klatt (1980) and of measured vowel bandwidths */
static const uint16_t VOWEL_F[5][3] = {
    {768, 1333, 2522}, {580, 1799, 2605}, {342, 2322, 3000}, {497, 910, 2459}, {378, 997, 2343},
};
static const uint8_t VOWEL_B[5][3] = {
    {90, 100, 160}, {70, 100, 170}, {50, 120, 200}, {70, 80, 150}, {60, 80, 150},
};
/* 2^(k/12), Q16, k = 0..12 */
static const uint32_t SEMI_Q16[13] = {
    65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772, 123715, 131072,
};

/* 2^(st16 / 192) in Q16 (st16 in 1/16 semitone), clamped to 1/16 .. 16.
 * Divisions by constants are written as multiplies: -Os keeps them as divides */
static uint32_t fmt_ratio(int32_t st16)
{
    int32_t o, k;
    uint32_t r;
    st16 = clamp(st16, -4 * 192, 4 * 192 - 1) + 4 * 192;   /* 0 .. 8 octaves */
    o = (st16 * 2731) >> 19;                            /* / 192, exact for 0 .. 1535 */
    k = st16 - o * 192;
    r = SEMI_Q16[k >> 4] + (((SEMI_Q16[(k >> 4) + 1] - SEMI_Q16[k >> 4]) * (uint32_t)(k & 15)) >> 4);
    return o >= 4 ? r << (o - 4) : r >> (4 - o);
}

/* the three formants of vowel position pos (0..127 << 8, A E I O U), Hz * 16 */
static void vowel_at(int32_t pos, int32_t *f, int32_t *b)
{
    int32_t x = (clamp(pos, 0, 127 << 8) * 33027) >> 20, i = x >> 8, fr = x & 255, j;   /* * 4 / 127 */
    if (i >= 4) {
        i = 3;
        fr = 256;
    }
    for (j = 0; j < 3; j++) {
        f[j] = (VOWEL_F[i][j] << 4) + (((VOWEL_F[i + 1][j] - VOWEL_F[i][j]) * fr) >> 4);
        b[j] = (VOWEL_B[i][j] << 4) + (((VOWEL_B[i + 1][j] - VOWEL_B[i][j]) * fr) >> 4);
    }
}

/* (0.9.8) VOICE glides as a singer does: the glide's course (pitch_cur, linear in semitones: log frequency, its time
 * GLIDE's; with TIME's fraction carried, gacc) eased in and out, an S-curve (3u^2 - 2u^3 of the way from gfrom), so the
 * pitch leaves the note and lands on the next one gently. In 1/4096 semitone (1/16 st << 8: fmt_sung puts the fraction
 * in the fine tune, no 1/16 st staircase). The other engines: pitch_cur, as before (voice.c, untouched) */
static int32_t glide_sung(const voice_t *v)
{
    int32_t d = v->pitch16 - v->gfrom, u;
    if (!d || v->pitch_cur == v->pitch16)
        return v->pitch_cur << 8;
    u = ((((v->pitch_cur - v->gfrom) << 8) + (d > 0 ? v->gacc : -v->gacc)) << 4) / d;   /* Q12 (|d| < 2048) */
    if ((uint32_t)u > 4096u)                            /* (not this glide's: as it is) */
        return v->pitch_cur << 8;
    u = (((u * u) >> 12) * (3 * 4096 - 2 * u)) >> 12;
    return (v->gfrom << 8) + ((d * u) >> 4);
}
/* the phase increment of the pitch as sung: m's (voice.c: the glide's linear course and the rest) moved by the S-curve's
 * offset from that course; *p16 the pitch (1/16 st). Only while the voice glides (the renders' block header) */
static __attribute__((noinline)) uint32_t fmt_sung(const voice_t *v, const vmod_t *m, int32_t *p16)
{
    int32_t g = glide_sung(v) - (v->pitch_cur << 8), fine = m->fine + (((g & 255) * 237) >> 12);   /* (1/16 st: 14.8 / 4096) */
    uint32_t inc;
    *p16 = clamp(m->pitch16 + (g >> 8), 0, 2047);
    inc = pitch_inc(*p16);
    return fine ? inc + (uint32_t)((int32_t)(inc >> 12) * fine) : inc;
}

typedef struct { int32_t a, b, c; } fres_t;
static int32_t formant_nz = 0x2545F491;                 /* breath noise, one xorshift for all voices */

/* Klatt resonator coefficients (Q30) for F, BW in Hz * 16 */
static void fres_coef(fres_t *r, uint32_t f16, uint32_t bw16)
{
    int32_t s = sine_i(f16 * 3044u);                    /* sin(pi F / FS), Q15 (2^32 / (2 FS) / 16) */
    int32_t x = (int32_t)(bw16 * 4781u);                /* pi BW / FS, Q30 (<= 0.2) */
    int32_t x2 = (int32_t)(mul64(x, x) >> 30), x3 = (int32_t)(mul64(x2, x) >> 30);
    int32_t x4 = (int32_t)(mul64(x3, x) >> 30);
    int32_t e = (1 << 30) - x + (x2 >> 1) - (int32_t)(mul64(x3, 178956971) >> 30)   /* exp(-x): x^3 / 6, */
                + (int32_t)(mul64(x4, 44739243) >> 30);                            /* x^4 / 24 */
    int32_t cs = (1 << 30) - 2 * s * s;                 /* cos(2 pi F / FS) = 1 - 2 sin^2, Q30 */
    r->b = (int32_t)(mul64(e, cs) >> 29);               /* 2 r cos */
    r->c = -(int32_t)(mul64(e, e) >> 30);               /* -r^2 */
    r->a = (1 << 30) - r->b - r->c;
}

/* (V10.40) WORDS' F5 (formant_word_render), each voice's y1 y2 (voice_t has no room; VOICE's 4 voices: slots 0..3) */
static int32_t fmt_f5[NTRK][4][2] __attribute__((section(".bss.fmt_f5")));   /* (its own: out of the merged globals) */
#define FMT_FRESH (-128)                                /* vjit: the resonators from rest (voice_start keeps ph[1..2]) */

/* source: BUZZ (+ SHP modulation, + velocity) -> the open quotient: g[] the closure (te, the end of the open phase),
 * the opening's end (tp: 65 % of it, closing 35 %) and their phase scales; returns BUZZ. (V10.40) both renders' (one
 * copy: the flash) */
static __attribute__((noinline)) int32_t fmt_glot(const int16_t *p, const voice_t *v, const vmod_t *m, uint32_t *g)
{
    int32_t buzz = clamp((p[P_E4] << 8) + m->shape - (64 << 8) + (v->vel - 96) * 40, 0, 127 << 8);
    uint32_t te = (uint32_t)(58982 - ((buzz * 284) >> 8)) << 16, tp = (te >> 8) * 166u;   /* (Q16: 0.90 .. 0.35) */
    g[0] = te;
    g[1] = tp;
    g[2] = (1u << 30) / ((tp >> 16) | 1u);
    g[3] = (1u << 30) / (((te - tp) >> 16) | 1u);
    return buzz;
}

static void formant_note_on(track_t *t, voice_t *v)
{
    if (t->p[P_VLANG]) {                                /* WORDS: the next syllable (sing_block); a fresh voice from */
        sing_note(t);                                   /* rest (s[7] unused); (V10.40) a retrigger near silence (under
                                                         * -42 dB) too: its resonators rang the last vowel at full level
                                                         * under the release, heard ~10 ms again as the attack came */
        if (v->env_out < 256) {
            v->vjit = FMT_FRESH;                        /* (F4 F5 zeroed at the render: ph[1..2] kept by voice_start) */
            v->ph[0] = v->ph[1] = v->ph[2] = 0;
            v->s[0] = v->s[1] = v->s[2] = v->s[3] = v->s[4] = v->s[5] = v->s[6] = v->s[7] = 0;
        }
        return;
    }
    v->s[7] = (int32_t)((vs_rnd >> 25) << 25);          /* TALK starts over; a random vowel (RAND; ctl64: drawn of
                                                         * formant_nz when the note was asked for, voice.c voice_begin) */
    if (!v->env && !v->env_out) {                       /* a fresh voice (not a retrigger): from rest */
        uint32_t i;
        v->ph[0] = 0;                                   /* the opening starts at zero: no click */
        v->ph[1] = v->ph[2] = 0;
        for (i = 0; i < 7u; i++)
            v->s[i] = 0;
    }
}

/* WORDS (sing.c): the articulation's formants and gains through the same source and cascade; aspiration noise into
 * F1 with the breath's, frication (first-difference noise: high-passed) into F4, F4 at the consonant's resonance
 * while it sounds (3300 Hz between). The gains ramp across the block (sing_t pv -> gv): no step, no click */
static void __attribute__((noinline)) formant_word_render(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                          const vmod_t *m)
{
    static const uint8_t BW0[3] = {70, 90, 140};       /* the bandwidths of F1 F2 F3 at x1 (Hz; V10.40: were 80 100 160,
                                                         * the vowels peakier, open, less muffled) */
    const int16_t *p = t->p;
    const sing_t *sg = sing_view(t);
    sing_x_t *sx = &sing_x[(uint32_t)(t - trk) % NTRK];
    fres_t r1, r2, r3, r4, r5;
    int32_t *f5 = fmt_f5[(uint32_t)(t - trk) % NTRK][(uint32_t)(v - t->v) & 3u];
    int32_t fa[3], ba[3], j, buzz, tilt, gvb, gn, og, ogk, gv, dgv, ga, dga, gf, dgf, fw, hum, jd, jr, shim, bm, bw[3];
    uint32_t inc = m->inc, incj, i, te, tp, rp, rn, ratio, bws, f0, r4r;
    uint32_t g[4];
    uint32_t ph = v->ph[0];
    int32_t y1 = v->s[0], y2 = v->s[1], y3 = v->s[2], y4 = v->s[3], y5 = v->s[4], y6 = v->s[5];
    int32_t y7 = (int32_t)v->ph[1], y8 = (int32_t)v->ph[2], lp = v->s[6], nst = formant_nz, nzp = 0, nzq = 0;
    int32_t y9 = f5[0], y10 = f5[1];
    int32_t p16 = m->pitch16, incq;
    /* (0.9.8) the singer: a vibrato on a held note (~5.5 Hz, +-~20 ct, after ~0.3 s, in over ~0.4 s; none while the
     * pitch glides, then it waits again), each voice of a choir at its own phase; each glottal period its own length
     * (jitter, ~0.3 %) and level (shimmer, ~0.4 dB). BUZZ at its top (the last ~12 %) steadies them away: a machine */
    hum = clamp((127 - p[P_E4]) * 4, 0, 64);
    if (v->pitch_cur != v->pitch16) {                   /* gliding: as sung (an S-curve), no vibrato */
        inc = fmt_sung(v, m, &p16);
        sx->vb = 0;
    }
    {
        int32_t vd = (clamp((int32_t)sx->vb - 400, 0, 512) * hum) >> 9;   /* 0 .. 64 */
        int32_t vib = (sine_i((uint32_t)(sx->vph + (uint32_t)(v - t->v) * 0x3A00u) << 16) * vd * 3) >> 17;
        inc += (uint32_t)((int32_t)(inc >> 12) * vib);   /* (+-48 / 4096: +-20 ct) */
    }
    jd = (hum * 3) >> 4;                                 /* +-12 / 4096 at the most */
    jr = v->vjit;                                        /* (the period under way: as it began) */
    if (jr == FMT_FRESH) {                               /* (V10.40: a retrigger near silence, formant_note_on) */
        jr = 0;
        y7 = y8 = y9 = y10 = 0;
    }
    incq = (int32_t)(inc >> 12);
    incj = inc + (uint32_t)(incq * jr);
    shim = 32768 + (jr << 7);
    ratio = fmt_ratio(p[P_E3] * 16 + ((FMT_FOLOW * (p16 - 60 * 16) * 516) >> 16));   /* (the pitch sung: its glide) */
    bws = fmt_ratio((64 - p[P_E6]) * 4);
    f0 = (uint32_t)(umul64(inc, 705600u) >> 32);
    bm = clamp(sg->bm, 8, 64);
    bw[0] = BW0[0] * (bm < 24 ? bm : 24);               /* (0.9.8) a nasal's, a lateral's F1 less widened than F2 F3: */
    bw[1] = BW0[1] * bm;                                /* the murmur low and round, the mouth's formants damped */
    bw[2] = BW0[2] * bm;
    for (j = 0; j < 3; j++) {
        int32_t f = (int32_t)(mul64_us(ratio, sg->f[j] << 4) >> 16);
        int32_t b = (int32_t)(mul64_us(bws, (int32_t)(mul64_us(ratio, bw[j]) >> 16)) >> 16);
        if (j == 0 && (uint32_t)f < f0 + (f0 >> 4) && f > 0) {
            b = (int32_t)(mul64(b, (int32_t)(f0 + (f0 >> 4))) / f);
            f = (int32_t)(f0 + (f0 >> 4));
        }
        fa[j] = clamp(f, 60 * 16, 7000 * 16);
        ba[j] = clamp(b, 20 * 16, 1500 * 16);
    }
    fres_coef(&r1, (uint32_t)fa[0], (uint32_t)ba[0]);
    fres_coef(&r2, (uint32_t)fa[1], (uint32_t)ba[1]);
    fres_coef(&r3, (uint32_t)fa[2], (uint32_t)ba[2]);
    /* (V10.40) F5: 4300 Hz, 300 Hz wide, moved as F1 .. F3 (ratio: a SHIFT up kept it over F4; at most ~x5, under
     * FS / 2) */
    fres_coef(&r5, ((ratio >> 4) * 4300u) >> 8, ((ratio >> 4) * 300u) >> 8);   /* (Hz x 16; 32 bits: ratio < 2^18) */
    fw = clamp(sg->gf * 16, 0, 32767);                  /* frication's weight: F4 its resonance (SHIFT not on it), */
    r4r = ratio + (uint32_t)((((int32_t)65536 - (int32_t)ratio) * fw) >> 15);   /* no presence on the noise */
    fres_coef(&r4, (uint32_t)clamp((int32_t)(umul64((uint32_t)clamp(sg->ff, 200, 7600) * 16u, r4r) >> 16),
                                   60 * 16, 7600 * 16),
              (uint32_t)clamp((int32_t)(umul64((uint32_t)clamp(sg->fb, 50, 5000) * 16u, r4r) >> 16), 20 * 16,
                              5000 * 16));
    buzz = fmt_glot(p, v, m, g);
    tilt = 1900 + ((buzz * 96) >> 8);                   /* (0.9.8) a softer source than the vowels' (~6 kHz at 80 %) */
    te = g[0];
    tp = g[1];
    rp = g[2];
    rn = g[3];
    gvb = 32767 - p[P_E5] * p[P_E5] * 2;                /* BREATH: voicing fades at the top, noise comes in; */
    gn = (p[P_E5] * 258 * sg->gv) >> 15;                /* (V10.40) as the voicing does: a closure silent (the breath */
                                                        /* through F1 filled the gap of p t k: -17 dB, now -30 and under) */
    gv = (sg->pv * gvb) >> 15;                          /* the articulation's gains, ramped over the block */
    dgv = (((sg->gv * gvb) >> 15) - gv) >> CTL_LOG2;
    ga = sg->pa;
    dga = (sg->ga - ga) >> CTL_LOG2;
    gf = sg->pf;
    dgf = (sg->gf - gf) >> CTL_LOG2;
    og = 7800 - ((p[P_E6] - 64) * 30);                 /* (V10.40: x 0.6, the peakier F1 F2 F3 and F5 as loud as before) */
    og = (int32_t)(mul64_us(fmt_ratio((60 * 16 - p16) / 4), og) >> 16);
    ogk = mul64_sh15((og >> 2) * (6 + (buzz >> 11)), 32767 - fw);   /* (0.9.8: less presence) */
    for (i = 0; i < n; i++) {
        int32_t e, x, a, s, nz, fz;
        if (ph < tp)
            e = (sine_i(((ph >> 16) * rp) << 1) * 17644) >> 15;
        else if (ph < te)
            e = -sine_i(((ph - tp) >> 16) * rn);
        else
            e = 0;
        e += blep(ph - te, incj) >> 1;
        e = (e * shim) >> 15;
        lp += ((e - lp) * tilt) >> 14;
        x = (lp * gv) >> 15;
        gv += dgv;
        nz = (int32_t)(noise32(&nst) >> 16) - 32768;
        if (gn)
            x += ph < te ? (nz * gn) >> 15 : (nz * gn) >> 16;   /* breath: with the glottis open */
        x += (nz * ga) >> 14;                           /* aspiration: h, after p t k (V10.40: x 2, the output's x 0.6) */
        ga += dga;
        fz = ((nz - nzq) * gf) >> 15;
        fz += fz >> 1;                                  /* (V10.40: x 1.5, the output's x 0.6: s ~-17 dB) frication: noise, (0.9.8) band-passed (1 - z^-2: none at DC */
        nzq = nzp;                                      /* nor at the top, no fizz past ~12 kHz), into F4 */
        nzp = nz;
        gf += dgf;
        a = (int32_t)((mac2_64(r1.a, x << 6, r1.b, y1) + mul64(r1.c, y2) + (1 << 29)) >> 30);
        y2 = y1;
        y1 = a;
        a = (int32_t)((mac2_64(r2.a, a, r2.b, y3) + mul64(r2.c, y4) + (1 << 29)) >> 30);
        y4 = y3;
        y3 = a;
        a = (int32_t)((mac2_64(r3.a, a, r3.b, y5) + mul64(r3.c, y6) + (1 << 29)) >> 30);
        y6 = y5;
        y5 = a;
        /* (V10.40) F5 between F3 and F4: the cascade's top as Klatt's (four poles fell -48 dB / oct past F4: a voice
         * heard through a wall), F3 +3 dB, 3 .. 4 kHz +6, 4 .. 5 kHz +20, as a mouth's; not on the frication */
        a = (int32_t)((mac2_64(r5.a, a, r5.b, y9) + mul64(r5.c, y10) + (1 << 29)) >> 30);
        y10 = y9;
        y9 = a;
        a = (int32_t)((mac2_64(r4.a, a + (fz << 6), r4.b, y7) + mul64(r4.c, y8) + (1 << 29)) >> 30);
        y8 = y7;
        y7 = clamp(a, -(1 << 28), 1 << 28);
        s = soft_knee(clamp((int32_t)(mac2_64(y7, og, y7 - y8, ogk) >> 23), -200000, 200000), 24000);   /* (V10.6y: as
                                                         * formant_render's, a quarter: under the knee) */
        ph += incj;
        if (ph < incj) {                                /* a glottal period begins (the glottis shut: no step) */
            jr = (nz * jd) >> 15;
            incj = inc + (uint32_t)(incq * jr);
            shim = 32768 + (jr << 7);
        }
        out[i] += voice_amp(s, m, i) * 23 >> 3;   /* (V10.6y: x 2.875, formant_render's) */
    }
    v->ph[0] = ph;
    v->vjit = (int8_t)jr;
    v->ph[1] = (uint32_t)y7;
    v->ph[2] = (uint32_t)y8;
    f5[0] = y9;
    f5[1] = y10;
    formant_nz = nst;
    v->s[0] = y1;
    v->s[1] = y2;
    v->s[2] = y3;
    v->s[3] = y4;
    v->s[4] = y5;
    v->s[5] = y6;
    v->s[6] = lp;
}

static void formant_block(track_t *t) { sing_block(t); }   /* (LANG OFF: nothing) */
/* WORDS: the release sings the coda (the last consonants) at the level the note had, not faded with the release: the
 * level at the release (s[7]) held while a timed coda phoneme sounds (a held voiced one, n l m, fades as the ADSR
 * does). LANG OFF: the ADSR as it is */
static int32_t formant_amp(track_t *t, voice_t *v, int32_t adsr)
{
    const sing_t *s;
    if (!t->p[P_VLANG])
        return adsr;
    if (v->gate) {
        v->s[7] = adsr;
        return adsr;
    }
    s = sing_view(t);
    return s->cdon && s->i > s->nu && s->i < s->np && s->tb && v->s[7] > adsr ? v->s[7] : adsr;
}

/* WORDS: the EDIT values relabelled (the same ranges and defaults): POS (0 AUTO: each note the next syllable; 1..127
 * that syllable of the lyric at the next note, as it changes: a lock, automation), THEME (the dice's words), ARTIC (the
 * consonants: 100 % crisp .. 0 % slurred; stored the other way, 0 = 100 %), LOOP (the lyric from its start every 1 2 4
 * 8 loops of the pattern, FREE: on through it) */
static const param_desc_t FMT_WDESC[8] = {
    {"POS", F_INT, 0, 127, 0, 0, 0}, {"THEME", F_INT, 0, 127, 64, N_THEME, 0}, {"ARTIC", F_PCTINV, 0, 127, 0, 0, 0},
    {0}, {0}, {0}, {0}, {"LOOP", F_INT, 0, 127, 0, N_LOOP, 0},
};
static const param_desc_t *formant_desc(const track_t *t, uint32_t k)
{
    const param_desc_t *d = &FMT_WDESC[k & 7u];
    return t->p[P_VLANG] && d->label ? d : 0;
}

static void formant_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    fres_t r1, r2, r3, r4;
    int32_t fa[3], ba[3], fb[3], bb[3], k, j, buzz, tilt, gv, gn, og, ogk;
    uint32_t inc = m->inc, i, te, tp, rp, rn, ratio, bws, f0, g[4];
    uint32_t ph = v->ph[0];
    int32_t y1 = v->s[0], y2 = v->s[1], y3 = v->s[2], y4 = v->s[3], y5 = v->s[4], y6 = v->s[5];
    int32_t y7 = (int32_t)v->ph[1], y8 = (int32_t)v->ph[2], lp = v->s[6], nst = 0;

    if (p[P_VLANG]) {                                   /* (0.8.5) WORDS */
        formant_word_render(t, v, out, n, m);
        return;
    }
    if (v->pitch_cur != v->pitch16) {                   /* (0.9.8) gliding: as sung, an S-curve */
        int32_t p16;
        inc = fmt_sung(v, m, &p16);
    }
    buzz = fmt_glot(p, v, m, g);                        /* (the source's, below) */
    /* vowel: VOWEL -> VOWL2 over TALK after note-on; ENV / LFO -> FLT move both */
    k = 0;
    if (p[P_E2]) {
        uint32_t g = ((uint32_t)v->s[7] & FMT_TALK_MASK) + ENV_LIN[p[P_E2] & 127];
        g = g > (1u << 24) ? 1u << 24 : g;
        v->s[7] = (int32_t)(((uint32_t)v->s[7] & ~(uint32_t)FMT_TALK_MASK) | g);
        k = (int32_t)(g >> 9);                          /* Q15 */
        k = (k * (65536 - k)) >> 15;                    /* ease out: the mouth opens fast, then settles */
    }
    {   /* RAND: both vowels move towards this note's random vowel (0 = off, 127 = fully random) */
        int32_t rv = (int32_t)(((uint32_t)v->s[7] >> 25) << 8), v0 = p[P_E0] << 8, v1 = p[P_E1] << 8;
        v0 += ((rv - v0) * p[P_E7] * 258) >> 15;
        v1 += ((rv - v1) * p[P_E7] * 258) >> 15;
        vowel_at(v0 + m->cutoff, fa, ba);
        /* TALK=0 (or its exact onset): only the first vowel contributes.
         * Avoid preparing and blending a second unused formant set. */
        if (k) {
            vowel_at(v1 + m->cutoff, fb, bb);
            for (j = 0; j < 3; j++) {
                fa[j] += ((fb[j] - fa[j]) * k) >> 15;
                ba[j] += ((bb[j] - ba[j]) * k) >> 15;
            }
        }
    }
    /* SHIFT (semitones) and FOLOW (key tracking around C4, fixed), one ratio for all four */
    ratio = fmt_ratio(p[P_E3] * 16 + ((FMT_FOLOW * (v->pitch_cur - 60 * 16) * 516) >> 16));   /* FOLOW / 127 */
    bws = fmt_ratio((64 - p[P_E6]) * 4);                /* Q: bandwidth x2.5 .. x0.4 */
    f0 = (uint32_t)(umul64(inc, 705600u) >> 32);   /* F0 in Hz * 16 */
    for (j = 0; j < 3; j++) {
        int32_t f = fa[j], b = ba[j];
        f = (int32_t)(mul64_us(ratio, f) >> 16);
        b = (int32_t)(mul64_us(bws, (int32_t)(mul64_us(ratio, b) >> 16)) >> 16);
        if (j == 0 && (uint32_t)f < f0 + (f0 >> 4) && f > 0) {   /* sung high: F1 follows F0 (sopranos do), */
            b = (int32_t)(mul64(b, (int32_t)(f0 + (f0 >> 4))) / f);   /* same Q: no boom on the fundamental */
            f = (int32_t)(f0 + (f0 >> 4));
        }
        fa[j] = clamp(f, 60 * 16, 7000 * 16);
        ba[j] = clamp(b, 20 * 16, 1500 * 16);
    }
    fres_coef(&r1, (uint32_t)fa[0], (uint32_t)ba[0]);
    fres_coef(&r2, (uint32_t)fa[1], (uint32_t)ba[1]);
    fres_coef(&r3, (uint32_t)fa[2], (uint32_t)ba[2]);
    fres_coef(&r4, (uint32_t)clamp((int32_t)(umul64(3300u * 16u, ratio) >> 16), 60 * 16, 7000 * 16),
              (uint32_t)clamp((int32_t)(umul64((uint32_t)(umul64(250u * 16u, ratio) >> 16), bws) >> 16), 20 * 16, 1500 * 16));

    /* source: BUZZ (+ SHP modulation, + velocity) -> open quotient and tilt */
    tilt = 2500 + ((buzz * 109) >> 8);                  /* one-pole coefficient Q14: dark .. open */
    te = g[0];
    tp = g[1];
    rp = g[2];
    rn = g[3];
    gv = 32767 - p[P_E5] * p[P_E5] * 2;                 /* BREATH: voicing fades at the top */
    gn = p[P_E5] * 258;                                 /* noise: Q15, up to 1.0 */
    if (gn) nst = formant_nz;
    og = 13000 - ((p[P_E6] - 64) * 50);                 /* output: narrower bands ring louder */
    og = (int32_t)(mul64_us(fmt_ratio((60 * 16 - v->pitch_cur) / 4), og) >> 16);   /* 1.5 dB / oct: low notes are sparse pulses */
    ogk = (og >> 2) * (4 + (buzz >> 10));               /* presence k = 1 .. 4.9 */

    for (i = 0; i < n; i++) {
        int32_t e, x, a, s;
        if (ph < tp)                                    /* opening: + sin(pi u) * Tn / Tp */
            e = (sine_i(((ph >> 16) * rp) << 1) * 17644) >> 15;
        else if (ph < te)                               /* closing: - sin(pi / 2 u) */
            e = -sine_i(((ph - tp) >> 16) * rn);
        else
            e = 0;
        e += blep(ph - te, inc) >> 1;                   /* the closure step, band-limited */
        lp += ((e - lp) * tilt) >> 14;
        x = (lp * gv) >> 15;
        if (gn) {
            int32_t nz = (((int32_t)(noise32(&nst) >> 16) - 32768) * gn) >> 15;
            x += ph < te ? nz : nz >> 1;                /* aspiration follows the glottal opening */
        }
        /* F1 -> F2 -> F3 -> F4, Q30, rounded; the signal x 64 inside: the rounding
         * dead band of F1 (+-0.5 / A, ~340 at 270 Hz) stays below 6 LSB */
        a = (int32_t)((mac2_64(r1.a, x << 6, r1.b, y1) + mul64(r1.c, y2) + (1 << 29)) >> 30);
        y2 = y1;
        y1 = a;
        a = (int32_t)((mac2_64(r2.a, a, r2.b, y3) + mul64(r2.c, y4) + (1 << 29)) >> 30);
        y4 = y3;
        y3 = a;
        a = (int32_t)((mac2_64(r3.a, a, r3.b, y5) + mul64(r3.c, y6) + (1 << 29)) >> 30);
        y6 = y5;
        y5 = a;
        a = (int32_t)((mac2_64(r4.a, a, r4.b, y7) + mul64(r4.c, y8) + (1 << 29)) >> 30);
        y8 = y7;
        y7 = clamp(a, -(1 << 28), 1 << 28);             /* int32 state stays far from overflow */
        /* (V10.6y, alex.knip: "AH" clipped) a quarter before the knee (>> 23, was >> 21: an open A at C4 reached 2.3x
         * the knee, a third of its samples saturated) and x 2.875 after it (was x 2): a sung vowel peaks as ANALOG's
         * first preset, -8.7 dB (was ~+9 dB over the other engines), no sample in the knee but CHANT's high notes */
        s = soft_knee(clamp((int32_t)(mac2_64(y7, og, y7 - y8, ogk) >> 23), -200000, 200000), 24000);
        ph += inc;
        out[i] += voice_amp(s, m, i) * 23 >> 3;
    }
    v->ph[0] = ph;
    v->ph[1] = (uint32_t)y7;
    v->ph[2] = (uint32_t)y8;
    if (gn) formant_nz = nst;
    v->s[0] = y1;
    v->s[1] = y2;
    v->s[2] = y3;
    v->s[3] = y4;
    v->s[4] = y5;
    v->s[5] = y6;
    v->s[6] = lp;
}

static const preset_t FORMANT_PRESETS[] = {
    /* VOWEL VOWL2 TALK SHIFT | BUZZ BREATH Q RAND. (V10.6i, reworked in place: the stored numbers kept, PROGRAM CHANGE
     * too; browsed in VOICE_ORDER) LANG OFF: ENV / LFO -> FILTER and MOD CUT move the vowel (A E I O U), SHP the BUZZ */
    {"CHOIR AAH", {0, 0, 0, 0, 40, 22, 60, 0}, {85, 90, 115, 95}, 0, TAG(VOX), FX(0, 70, 15, 90), PAT(5),
     MODS(P_LWAVE, LW_DRIFT, P_LRATE, 36, P_LD_FLT, 6, MMOD(1, PS_MODW, PD_CUT, 40))},   /* the vowel breathes; MODW: OOH */
    {"VOX LEAD", {32, 0, 0, 2, 90, 8, 72, 0}, {6, 70, 105, 55}, 0, 1 | TAG(VOX), FX(0, 20, 45, 40), PAT(4),
     MODS(P_LRATE, 88, P_LFADE, 70, MMOD(1, PS_MODW, PD_VIB, 2))},          /* the wheel brings a vibrato in */
    {"WOW BASS", {95, 0, 68, 0, 100, 0, 80, 0}, {0, 60, 80, 28}, 0, 1 | TAG(BASS), FX(10, 0, 10, 10), PAT(8),
     MODS(P_GLIDE, 18, MMOD(1, PS_VEL, PD_SHP, 30))},                                     /* a harder note buzzes */
    {"INIT VOWEL", {0, 64, 0, 0, 64, 10, 64, 0}, {2, 64, 127, 30}, 0, TAG(VOX), FX(0, 0, 0, 0)},
    {"OOH PAD", {80, 100, 50, 0, 55, 25, 55, 0}, {70, 90, 115, 95}, 0, TAG(VOX), FX(0, 60, 25, 85), PAT(5),
     MODS(P_LWAVE, LW_TRI, P_LRATE, 30, P_LD_FLT, 10, MMOD(1, PS_AT, PD_SHP, 40))},       /* O .. U, slowly; AT: buzz */
    {"CHANT", {20, 60, 100, -12, 70, 15, 65, 10}, {30, 90, 110, 80}, 0, TAG(VOX), FX(0, 30, 35, 75), PAT(5),
     MODS(P_LWAVE, LW_SH, P_LRATE, -5, P_LD_FLT, 18, MMOD(1, PS_RAND, PD_PAN, 30))},     /* a new vowel each beat */
    {"TALK BOX", {10, 110, 40, 0, 110, 4, 85, 0}, {2, 65, 95, 40}, 0, 1 | TAG(VOX), FX(10, 10, 35, 30), PAT(4),
     MODS(P_GLIDE, 12, MMOD(1, PS_MODW, PD_CUT, -40))},                                   /* MODW: the mouth */
    /* (0.8.5) WORDS (sing.c): LANG and LYRIC in WORDS(); the EDIT values POS THEME ARTIC SHIFT | BUZZ BREATH Q LOOP.
     * (0.9.8) the singers' REL longer (~190 ms to -40 dB, was ~70): a syllable's vowel carries on into the next note's
     * consonant, as sung, not cut off by the step's gate; their ATK ~5 ms (was ~1.5): a voice's onset, no tick.
     * (V10.6i) their own vibrato kept (none on PITCH from here): AT / MODW on the voice (BUZZ BREATH Q) */
    {"CANTOR ES", {0, 64, 0, 0, 80, 6, 72, 0}, {24, 70, 112, 72}, 0, 1 | TAG(VOX), FX(0, 10, 30, 35), PAT(4), WORDS(LG_ES, 1),
     MODS(MMOD(1, PS_AT, PD_SHP, 30))},
    {"CANTORA ES", {0, 0, 0, 4, 70, 10, 74, 0}, {26, 70, 112, 72}, 0, 1 | TAG(VOX), FX(0, 15, 30, 45), PAT(4), WORDS(LG_ES, 2),
     MODS(MMOD(1, PS_MODW, PD_E(5), 40))},
    {"SINGER EN", {0, 64, 0, 0, 84, 6, 72, 0}, {24, 70, 112, 72}, 0, 1 | TAG(VOX), FX(0, 10, 30, 35), PAT(4), WORDS(LG_EN, 3),
     MODS(MMOD(1, PS_AT, PD_SHP, 30))},
    {"UTA JA", {0, 48, 0, 5, 76, 8, 74, 0}, {24, 70, 112, 70}, 0, 1 | TAG(VOX), FX(0, 15, 30, 40), PAT(4), WORDS(LG_JA, 4),
     MODS(MMOD(1, PS_MODW, PD_E(6), 40))},
    {"WORD CHOIR", {0, 64, 16, 0, 45, 20, 62, 0}, {60, 90, 115, 85}, 0, TAG(VOX), FX(0, 70, 15, 90), PAT(5), WORDS(LG_ES, 5),
     MODS(MMOD(1, PS_RAND, PD_PAN, 40))},   /* each note elsewhere */
    {"SCAT EN", {0, 64, 0, 0, 96, 4, 76, 0}, {16, 60, 100, 60}, 0, 1 | TAG(VOX), FX(0, 0, 35, 30), PAT(4), WORDS(LG_EN, 17),
     MODS(MMOD(1, PS_RAND, PD_SHP, 20))},   /* each syllable its colour */
    {"ROBOT EN", {0, 64, 0, -3, 127, 0, 90, 0}, {2, 65, 120, 56}, 0, 1 | TAG(VOX), FX(10, 10, 35, 30), PAT(4), WORDS(LG_EN, 6),
     MODS(P_LWAVE, LW_SH, P_LRATE, -10, MMOD(1, PS_LFO, PD_SHP, 30), MMOD(2, PS_LFO, PD_PAN, 30))},   /* S&H 1/16 */
    {"WHISPER JA", {0, 64, 0, 3, 50, 120, 52, 0}, {40, 90, 110, 80}, 0, TAG(VOX), FX(0, 40, 30, 70), PAT(5), WORDS(LG_JA, 7),
     MODS(MMOD(1, PS_RAND, PD_PAN, 40), MMOD(2, PS_RAND, PD_REV, 30))},                  /* each word elsewhere */
};
/* (V10.6i) the browsing order: to play, then the experiments, then INIT */
static const uint8_t VOICE_ORDER[] = {0, 4, 1, 6, 2, 7, 8, 9, 10,   /* to play */
                                      11, 5, 12, 13, 14,            /* experiments */
                                      3};                           /* INIT */
_Static_assert(sizeof VOICE_ORDER == NELEM(FORMANT_PRESETS), "VOICE's browsing order: every preset once");

static const engine_t ENG_FORMANT = {
    .name = "VOICE",
    .page_title = {"VOWL", "TONE"},
    .edit = {
        {"VOWEL", F_INT, 0, 127, 0, 0, 0},
        {"VOWEL2", F_INT, 0, 127, 64, 0, 0},
        {"TALK", F_TIME, 0, 127, 0, 0, 0},
        {"SHIFT", F_SEMI, -12, 12, 0, 0, 0},
        {"BUZZ", F_PCT, 0, 127, 64, 0, 0},
        {"BREATH", F_PCT, 0, 127, 10, 0, 0},
        {"Q", F_PCT, 0, 127, 64, 0, 0},
        {"RAND", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = FORMANT_PRESETS,
    .npresets = NELEM(FORMANT_PRESETS),
    .note_on = formant_note_on,
    .render = formant_render,
    .knob = {P_E0, P_E4, P_E5, P_E2},
    .poly = 4,
    .desc = formant_desc,
    .block = formant_block,
    .amp = formant_amp,
};
