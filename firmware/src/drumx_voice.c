/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT (FELUCCA TONIC): DRUM-X, after the idea of Sonic Charge's Microtonic (a tuned oscillator with a pitch
 * envelope and FM, a filtered noise generator, their envelopes, two patches and a MORPH between them), built anew
 * small and in fixed point for the FM-1. */
/* A DRUM-X sound (a lane of a kit) is dx_lane_t, 11 bytes: the mode and five values for each of its patches A and B:
 *   PITCH  the oscillator, MIDI 24..119 (33 Hz .. 7.9 kHz), 3/4 semitone a step
 *   PMOD   the pitch envelope: up to +4 octaves at the hit, falling with a sixth of the decay's tau (1..40 ms)
 *   DECAY  the amplitude's tau, 3 ms .. 1.5 s (exponential)
 *   NOISE  the mix: 0 the oscillator only .. 127 the noise only
 *   COLOR  the oscillator's FM index (SINE: its drive) and the noise filter's cutoff (30 Hz .. 16 kHz)
 * The mode: the oscillator (SINE, FM at 1:1, METAL at 1:1.47, BELL at 1:2.76), the noise filter (LP, BP, HP), the
 * noise's envelope (as the oscillator's, or SNAP: a quarter of its tau); (JIANT 0.4, the mode's free bits 5..7) the
 * pitch modulation, as Microtonic's: DECAY (the envelope above), LONG (its tau half the decay's, up to 250 ms: the
 * 808 boom), NOISE (a random pitch each block, scaled by the envelope: glitch, grit), SINE (a sine at 1/8 of the
 * pitch, scaled by the envelope: wobble, laser); and DRIVE (the oscillator x3 into a soft clip: dense kicks, fat toms).
 * MORPH (0 A .. 127 B) mixes the two patches value by value, read every block: it may move while a hit rings.
 * Envelopes per block (Q30 values, Q16 factors, dv_kb), ramped linearly inside it; the pitch ramps too. A hit
 * that has rung out (-84 dB) stops computing (live 0) and writes zeros. Only 32-bit products. */
enum { DV_KICK, DV_SNARE, DV_CLAP, DV_HATC, DV_HATO, DV_TOM, DV_RIM, DV_BELL, DV_NLANE };   /* the kit's lanes */
enum { DXW_SINE, DXW_FM, DXW_METAL, DXW_BELL };
enum { DXF_LP, DXF_BP, DXF_HP, DXF_GR };                 /* (V10.7.8) GR: the guiro's scrape (dx_run_fx) */
#define DX_MODE(w, f, snap) ((uint8_t)((w) | (f) << 2 | (snap) << 4))
enum { DXM_DECAY, DXM_LONG, DXM_NOISE, DXM_SINE };       /* (JIANT 0.4) the pitch modulation: mode bits 5..6 */
#define DX_DRIVE 0x80u                                   /* .. the oscillator driven: bit 7 */
enum { DXP_PITCH, DXP_PMOD, DXP_DECAY, DXP_NOISE, DXP_COLOR, DXP_N };
typedef struct {
    uint8_t mode;
    uint8_t a[DXP_N], b[DXP_N];                          /* 0..127 */
} dx_lane_t;
typedef struct {                                         /* a hit */
    uint32_t ph, ph2, inc;                               /* the phases; the last block's end increment */
    int32_t ea, en, ep;                                  /* amplitude, noise (Q30), pitch (Q15) envelopes */
    int32_t ic1, ic2, rng;                               /* the noise filter's state, the noise */
    int32_t mo;                                          /* WARP: the modulator's last output (its feedback) */
    uint32_t ph3;                                        /* (JIANT 0.4) the SINE pitch modulation's phase */
    int32_t hl;                                          /* (0.6.17) the last half-rate sample (dx_run's upsampling) */
    int32_t b1, b2;                                      /* (0.6.36) TONE's band-pass state */
    uint32_t ph4;                                        /* (0.6.49) LAYER's oscillator phase */
    uint8_t live, trig, choke;
    uint8_t sv;                                          /* (V10.7.8) the hit's velocity (eng_drum.c; 0 a full one): GR's
                                                          * stroke, long or short (in the struct's padding) */
    uint16_t hpos, hsum, hseed;
    int16_t mo1;                                         /* (0.9.9-snd1) FEEDBK: the output a sample before mo (the mean
                                                          * of the two fed back; in the struct's padding) */                          /* (0.7.9) CLAP's HANDS: half-rate samples since the hit, the
                                                          * hands come in so far (Q12, over the hit's own), this hit's dice */
} dx_voice_t;

/* (V10.6q) the SINE's drive: 1 / tanh(its gain), Q12, every 8 COLOR (0..128): the driven sine's peak back to the sine's */
static const uint16_t DX_NRM[17] = {16724, 11014, 8422, 6987, 6105, 5526, 5131, 4854, 4655, 4511, 4406, 4328, 4270, 4249, 4249,
                                    4249, 4249};
/* the oscillator's modulator ratios (Q12), by wave */
static const uint16_t DX_RATIO[4] = {4096, 4096, 6021, 11305};

/* 2^(x / 4096), 0 <= x < 16 * 4096, Q16 (a cubic for the fraction: within 0.01 %) */
static __attribute__((noinline)) uint32_t dv_exp2(uint32_t x)
{
    uint32_t f = x & 4095u, m;
    m = 65536u + ((f * (45594u + ((f * (14851u + ((f * 5092u) >> 12))) >> 12))) >> 12);
    return m << (x >> 12);
}
/* (0.9.9-snd1) the owner: FEEDBK, FM ENV "too extreme, harsh noise, unusable"; "a SOPHIE-like palette: hard but
 * musical, metallic, rubbery, clean transients". The knobs on curves, most of the travel musical, the extreme only in
 * the last steps. FEEDBK (the kit's KIT 3 + the sound's SOUND 4, 0..15): the phase fed back per unit of the output, Q15
 * x this = phase (2^32 a turn); beta (rad) 1.6 (k / 13)^1.5 up to 13 (0.77 at 8, 1.25 at 11, 1.6 at 13: a sine
 * growing a saw's edge, rubber, metal), then 2.2 and 3.0 (noise, as most of it was: k x 4096, beta 1.2 at 6, 1.57 at 8);
 * and fed back the mean of the last two
 * outputs (the DX7's: the period-2 hunting of a one-sample loop gone: clean, no fizz) */
static const uint16_t DX_FB_CURVE[16] = {0, 712, 2013, 3699, 5697, 7961, 10465, 13188, 16113, 19230, 22521, 25981, 29600,
                                         33377, 37300, 41370};   /* (V10.6q: the curve on to 15, 1.98 rad; its 2.2 and 3.0 rad,
                                                          * noise and 8 % DC, gone) */
/* FM ENV (DRUM's E5, 0..127) -> the FM index the hit strikes with (dx_run's ix: 1 = 0.0245 rad): 3.26 (2^(6 x) - 1)
 * up to 114 (x = FM ENV / 127: 0.56 rad at 64, ~3.3 at 114: a beater, a click, a metal edge), then a line to the old
 * top (381, 9.3 rad: the noise) at 127; was 3 x FM ENV (4.7 rad at 64: a crunch) */
/* (V10.6q) x^2 (x + 0.5) up to 114 (0.84 rad at 64, 2.3 at 96, 3.7 at 114: half the knob heard, it was ~0 up to ~100),
 * then the line to the top */
static __attribute__((noinline)) int32_t dx_fm_env(int32_t fm)
{
    fm = clamp(fm, 0, 127);
    return fm > 114 ? 150 + (fm - 114) * (381 - 150) / 13 : fm * fm * (fm + 64) / 15422;
}
/* .. and kept under the half rate's Nyquist (11 kHz): the hit's highest sideband (fc + (I + 1) fm, the carrier fc,
 * the modulator fm = fc ratio / 4096) at 10.5 kHz at most. (V10.6q) A ceiling, on COLOR's index too (it was FM ENV's
 * alone, and soft, e lim / (e + lim), which took ~40 % off a rim far under it): below it every index as asked, above
 * it no sideband folded back (the hats' FM was 77 % aliasing) */
static __attribute__((noinline)) int32_t dx_fm_lim(int32_t e, uint32_t inc, uint32_t ratio)
{
    int32_t fc = (int32_t)(((inc >> 16) * 44100u) >> 16), fmh = (int32_t)(((uint32_t)fc * ratio) >> 12), lim;
    lim = fc >= 10500 ? 0 : (10500 - fc) * 41 / (fmh > 0 ? fmh : 1) - 41;
    return e < lim ? e : lim > 0 ? lim : 0;
}
/* tau (us) -> per-block decay, Q16: e^(-CTL / (tau fs)), to the fourth order (2^16 CTL / FS us) */
static uint32_t dv_kb(uint32_t us)
{
    uint32_t x = (uint32_t)(65536.0 * CTL / FS * 1000000.0) / (us | 1u), x2, x3, x4, k;
    if (x > 46000u)
        x = 46000u;
    x2 = (x * x) >> 16;
    x3 = (x2 * x) >> 16;
    x4 = (x3 * x) >> 16;
    k = 65536u - x + (x2 >> 1) - x3 / 6u + x4 / 24u;
    return k > 65535u ? 65535u : k;
}

static void dx_trigger(dx_voice_t *v) { v->trig = 1; v->choke = 0; }
static void dx_choke(dx_voice_t *v) { if (v->live) v->choke = 1; }

/* a decay value (0..127) -> its tau in us: 3 ms x 2^(9 d / 127) */
static uint32_t dx_tau(uint32_t d) { return ((dv_exp2(d * 9u * 4096u / 127u) >> 8) * 3000u) >> 8; }

/* (0.6.49) LAYER's oscillator: a triangle (no table, a few operations; its odd partials add to the clang) */
static inline int32_t dx_tri(uint32_t ph)
{
    int32_t s = (int32_t)(ph >> 15);                     /* 0 .. 131071 */
    return s < 65536 ? s - 32768 : 98303 - s;
}

/* (V10.6q) the cowbell's square: a triangle clipped (its edge k, Q8: ~2 samples) */
static __attribute__((noinline)) int32_t dx_sq(uint32_t ph, int32_t k) { return clamp((dx_tri(ph) * k) >> 8, -32767, 32767); }

/* (0.6.49) the oscillator's loop with LAYER (dx_run's FM ENV / FEEDBK loop and a triangle at sqrt 2 of it, frequency
 * modulated by it, its index up as LAYER's square; up to ~70 % of the mix): a function of its own (dx_run's loops as
 * they were) */
static __attribute__((noinline)) void dx_layer(dx_voice_t *v, int32_t *y, uint32_t n, uint32_t *php, uint32_t *ph2p,
                                               int32_t *op, int32_t *o1p, int32_t inc, int32_t dinc, int32_t ga, int32_t dga,
                                               int32_t ix, int32_t dix, uint32_t ratio, int32_t fbk, int32_t lay)
{
    uint32_t i, ph = *php, ph2 = *ph2p, ph4 = v->ph4;
    int32_t o = *op, o1 = *o1p, lx = lay * lay * 600, lm = lay * 3, fh = fbk >> 1;   /* (snd1: FEEDBK's mean of two) */
    uint32_t st4 = lay ? 1448u : 0u;                     /* (LAYER 0: its phase kept) */
    if (!ix && !dix)                                     /* (no FM on it: the modulator stands still, its phase kept) */
        ratio = 0;
    for (i = 0; i < n; i++, inc += dinc, ga += dga, ix += dix) {
        int32_t o2, f = o + o1;
        o1 = o;
        o = sine_i(ph + (uint32_t)(sine_i(ph2) * ix) * 512u + (uint32_t)(f * fh));
        o2 = dx_tri(ph4 + (uint32_t)(o * lx));
        y[i] = mulq15(o + (((o2 - o) * lm) >> 6), ga >> 8);
        ph += (uint32_t)inc << 4;
        ph2 += ((uint32_t)inc >> 8) * ratio;
        ph4 += ((uint32_t)inc >> 6) * st4;               /* (x 22.6: x 16 the step, sqrt 2) */
    }
    *php = ph;
    *ph2p = ph2;
    *op = o;
    *o1p = o1;
    v->ph4 = ph4;
}

/* one block of lane L's hit v into y (n <= CTL): Q15, peaks near full scale. morph 0..127; tune16 1/16 semitones;
 * dofs / cofs / nofs move DECAY / COLOR / NOISE (-64..63). warp (DRUM's FOLD since JIANT 0.5, was WARP, 0..127): the
 * oscillator through a wavefolder after its envelope, x1 .. x9 into three folds at +-T: the hit's loud start folds
 * into bright partials, its tail unfolds back to the plain wave (a West Coast drum); 0: the patch as it is.
 * fm (DRUM's FM ENV, JIANT 0.6.36; was FM, 0..127): how much FM the hit strikes with, its index following the pitch
 * envelope (1..40 ms) down to the lane's own: an attack's beater / click / metal edge, the tail as the kit has it
 * (the old FM, a fixed harmonic FM over the whole hit, muddied every sound). The oscillator and the noise each run their own loop (the wave and the
 * filter picked once a block), skipped when the mix leaves them out; gains and the pitch step per sample */
/* (JIANT 0.6.36) EDIT > TONE, the kit's (project.c keeps it in the kit's reserved byte 7: 0 in older projects, off):
 * low nibble BP, a band-pass on the metal sounds (the hats, the rim, the cowbell), 1..15 its centre ~1.5 .. 9 kHz;
 * high nibble FEEDBK, every sound's oscillator fed back into its own phase (1..15: warm, growl, grit, noise) */
static uint8_t dx_tone;
/* (JIANT 0.7.9) KIT 3 > HANDS, the kit's (project.c keeps it in the high nibble of the kit's reserved byte 0, the
 * mute groups' low nibble: 0 in older projects, off): 1..15 more hands on the CLAP, each the hit's noise struck again
 * at a moment of its own (~3 .. 45 ms after it, the dice thrown again each hit: no tempo in it) and at a level of its
 * own, then dying as the hit does: a few people clapping, then a crowd. 0: the clap as it was */
static uint8_t dx_hands;
static dx_lane_t dx_kit[8] __attribute__((section(".data.dx_kit")));                              /* (the music's kit: DX_KITS below; V10.6o: no initializer, the
                                                          * power-on and a project set it) */
/* (JIANT 0.6.49) EDIT > LAYER, each sound's (the kit's: project.c keeps it, FUNC's 'D' 'L' block; 0 before it): low
 * nibble LAYER, a second oscillator (a triangle) at sqrt 2 of the pitch (the most out of tune with it there is) frequency-modulated
 * by the first, more as it goes up (1..15: a metal edge .. a clang with no note: the sound melts into the rhythm and
 * stays out of the harmony); high nibble FEEDBK, the sound's own oscillator fed back (on top of TONE's, the kit's) */
static uint8_t dx_lay[8] __attribute__((section(".data.dx_lay")));                                /* (V10.6o: no initializer; the power-on and a project set it: the
                                                          * factory kit's, DX_LAY_DEF) */
#define DX_LAY_L(x) ((uint32_t)(x) & 15u)
#define DX_LAY_FB(x) ((uint32_t)(x) >> 4)
#define DX_TONE_BP(t) ((uint32_t)(t) & 15u)
#define DX_TONE_FB(t) ((uint32_t)(t) >> 4)
/* (0.7.9) CLAP's HANDS, once a half-rate block (~0.7 ms: each hand's onset on its block, its rise the block's ramp):
 * the noise's gain at the block's start and end, gn and *g1 (Q23), times 1 + the hands in. Hand k strikes at its own
 * moment (this hit's dice, v->hseed, 3 .. 45 ms), at its own level, then decays as the hit: in the sum it is the hit's
 * envelope times its level over where that envelope was when it struck, so one sum carries them all (v->hsum, Q12) */
static __attribute__((noinline)) int32_t dx_hands_step(dx_voice_t *v, uint32_t n, int32_t gn, int32_t *g1, int32_t en1)
{
    uint32_t k, nh = dx_hands ? 1u + (uint32_t)dx_hands * 5u / 15u : 0u, span = 60u + (uint32_t)dx_hands * 60u, p0 = v->hpos;
    int32_t s0 = v->hsum, s = s0, w = 2 * 23 / (int32_t)(2u * nh + 1u);   /* (V10.6q: the more hands the softer each: the
                                                          * crowd about as loud as a few, was +10.6 dB at 15) */
    if (p0 < 768u) {                                     /* (V10.6q) the 808's: three claps 11.6 ms apart, each a falling
                                                          * saw (squared) on the noise, then the tail */
        int32_t a = 256 - (int32_t)(p0 & 255u), b = 255 - (int32_t)((p0 + n - 1u) & 255u);
        gn = (gn >> 16) * a * a;
        if (p0 + n < 768u)
            *g1 = (*g1 >> 16) * b * b;
    }
    for (k = 0; k < nh && p0 < 1200u; k++) {             /* (all in by ~54 ms: then the sum alone) */
        uint32_t h = ((uint32_t)v->hseed + k * 0x9E37u) * 2654435761u, on = 66u + (h >> 8) % span;
        if (on >= p0 && on < p0 + n) {
            int32_t f = (int32_t)((1u << 30) / (uint32_t)((en1 >> 12) | 1));   /* 1 / the envelope there, Q12 */
            if (f > 16 << 12) f = 16 << 12;
            s += (((int32_t)(1200u + ((h >> 20) & 2047u)) * w / 23) * f) >> 12;   /* its level, ~0.3 .. 0.8 */
        }
    }
    s = clamp(s, 0, 12 << 12);
    v->hsum = (uint16_t)(s > 65535 ? 65535 : s);
    v->hpos = (uint16_t)(p0 + n < 65535u ? p0 + n : 65535u);
    *g1 = (int32_t)(mul64(*g1, 4096 + s) >> 12);
    return (int32_t)(mul64(gn, 4096 + s0) >> 12);
}
/* (V10.7.8) the filter GR's stroke (dx_run_fx), on a half-rate block y[n] from the hit's sample sp: the guiro's ridges
 * ticks (a phase step sinc, 2^16 a tick), each falling as a square, the first eight growing (the stroke's swell); none
 * past the tick holding the stroke's end slen (the hit over by then: 1 back; ~120 ms at most past the stroke) */
static __attribute__((noinline)) int dx_scrape(dx_voice_t *v, int32_t *y, uint32_t n, uint32_t sp, uint32_t slen, uint32_t sinc)
{
    uint32_t kl = (slen * sinc) >> 16, i;
    for (i = 0; i < n; i++) {
        uint32_t u = (sp + i) * sinc, d = (65535u - (u & 65535u)) >> 1, t = u >> 16;
        y[i] = (y[i] * (int32_t)(t > kl ? 0u : ((d * d) >> 19) * (t < 8u ? 8u + t : 16u))) >> 15;
    }
    if (v->hpos == sp)                                   /* (the CLAP's HANDS count it too) */
        v->hpos = (uint16_t)(sp + n);
    return ((sp + n) * sinc) >> 16 > kl;
}
static __attribute__((noinline)) void dx_run_fx(const dx_lane_t *L, dx_voice_t *v, int32_t morph, int32_t tune16,
                                             int32_t dofs, int32_t cofs, int32_t nofs, int32_t warp, int32_t fm, int32_t *y,
                                             uint32_t n, uint32_t feedback)
{
    int32_t q[DXP_N], p16, depth, ea0, ea1, en1, ep1, og, ng, ga, dga, gn, dgn, kd, inc, dinc, mt, cw, cx;
    uint32_t i, k, wave = L->mode & 3u, flt = (L->mode >> 2) & 3u, ta, tn, tp, kb, inc1, sp, gr = flt == DXF_GR, slen = 0, sinc = 0;
    tsvf_t c;
    uint32_t nf = n, ln = L >= dx_kit && L < dx_kit + 8 ? (uint32_t)(L - dx_kit) : 0u;
    int32_t lay = (int32_t)DX_LAY_L(dx_lay[ln]), lfb = (int32_t)DX_TONE_FB(dx_tone) + (int32_t)DX_LAY_FB(dx_lay[ln]) + (int32_t)feedback;
    if (!v->live && !v->trig) {
        for (i = 0; i < n; i++)
            y[i] = 0;
        return;
    }
    if (nf < 2u) {                                       /* (ctl64) one sample (a hit starting at a block's last sample,
                                                          * voice.c track_render): the last one held, nothing moves */
        y[0] = v->hl;
        return;
    }
    n >>= 1;                                             /* (0.6.17) at half rate (22 kHz); the block back to full rate
                                                          * at the end, linearly (half the work of the heaviest lanes).
                                                          * (V10.6q) no one-pole after it: at 22 kHz its "8 kHz" was a
                                                          * 1.8 dB shelf, 12 % of a hit's work */
    morph = clamp(morph, 0, 127);
    for (k = 0; k < DXP_N; k++)
        q[k] = L->a[k] + ((L->b[k] - L->a[k]) * morph) / 127;
    q[DXP_DECAY] = clamp(q[DXP_DECAY] + dofs, 0, 127);
    for (k = 0; k < 2u; k++) {                           /* (V10.6q) COLOR, NOISE: the offset as it is where it fits, scaled
                                                          * to the room where it would clip (was clipped: half a knob dead
                                                          * on a sound near an end); DECAY stays in octaves (SHORT: half) */
        int32_t o = clamp(k ? nofs : cofs, -64, 63), *x = &q[k ? DXP_NOISE : DXP_COLOR];
        *x += o > 0 ? o * (127 - *x < 63 ? 127 - *x : 63) / 63 : o * (*x < 64 ? *x : 64) / 64;
    }
    if (!ln)                                             /* (V10.6z8, the owner: "the kick sounds like a snare") the
                                                          * KICK's NOISE on a cube: the kit's 40 (V10.6q) or 4..24 (before)
                                                          * as good as none, the body whole; the knob up to 127 still there
                                                          * (64: an eighth, 100: a half) */
        q[DXP_NOISE] = (q[DXP_NOISE] * q[DXP_NOISE] * q[DXP_NOISE]) >> 14;
    ta = dx_tau((uint32_t)q[DXP_DECAY]);
    tn = (L->mode >> 4) & 1u ? (ta >> 2 < 8000u ? ta >> 2 : 8000u) : ta;   /* SNAP: a quarter, 8 ms at most (V10.6q: a click) */
    tp = ((L->mode >> 5) & 3u) == DXM_LONG ? (uint32_t)clamp((int32_t)(ta / 2u), 4000, 250000)
                                           : (uint32_t)clamp((int32_t)(ta / (ln ? 8u : 6u)), 1000, ln ? 25000 : 40000);
                                                         /* (V10.6q: / 12, 15 ms: thin; / 8, 25 ms: the click and the
                                                          * body; V10.6z8: the KICK's / 6, 40 ms again, as before V10.6q:
                                                          * the big sweep the owner asked back) */
    p16 = 384 + q[DXP_PITCH] * 12 + tune16;
    depth = gr ? 0 : q[DXP_PMOD] * 6;                    /* (GR: PMOD the SCRAPE, below) */
    ea0 = v->live ? v->ea : 1 << 30;                     /* (V10.6q) the block's start: a hit struck again while it rings
                                                          * rises from where it was over the block, its phases and
                                                          * filters as they were (a jump to 0 clicked: x35 a step) */
    if (v->trig) {                                       /* the hit: from the top */
        if (!v->live) {
            v->ph = v->ph2 = v->ph4 = 0;
            v->ic1 = v->ic2 = 0;
            v->b1 = v->b2 = 0;
            v->mo = 0;
            v->mo1 = 0;
        }
        v->trig = 0;
        v->live = 1;
        v->ea = v->en = 1 << 30;
        v->ep = 32767;
        if (!v->rng)
            v->rng = 0x2545F491;
        v->hpos = v->hsum = 0;
        v->hseed = (uint16_t)(((uint32_t)v->rng * 2654435761u) >> 16);   /* (not drawn from the noise: OFF as it was) */
        v->inc = pitch_inc((uint32_t)clamp(p16 + depth, 0, 2047));
    }
    sp = v->hpos;                                        /* (V10.7.8) GR: half-rate samples since the hit */
    if (gr) {                                            /* (V10.7.8, the owner: a guiro) the filter GR, a band-pass whose
                                                          * hit is a stroke over the guiro's ridges: ticks at 24 .. 64 Hz
                                                          * (PITCH), held and swelling for the SCRAPE (PMOD's place: 0 .. 173
                                                          * ms) times the velocity's 4th power (an accent the long stroke,
                                                          * a soft hit a "chk"), the last tick decaying with DECAY */
        int32_t sv = v->sv ? v->sv : 127;
        sv = (sv * sv * sv * sv) >> 14;                  /* (Q14 of 127^4 / 2^14) */
        slen = (uint32_t)((q[DXP_PMOD] * 31 * sv) >> 14);
        sinc = (uint32_t)((3048 + 40 * q[DXP_PITCH]) * 3) >> 7;   /* (a tick's phase step, 2^16 a tick) */
        flt = DXF_BP;
    }
    k = 11890000u / ((pitch_inc((uint32_t)clamp(p16, 0, 2047)) >> 12) | 1u);   /* (V10.6q) half its period (us): no shorter */
    kb = v->choke ? dv_kb(1500u) : dv_kb(ta > k ? ta : k);   /* (a choked hit: gone in ~5 ms; a blip shorter than its own
                                                          * half wave was all DC) */
    ea1 = mulq16(v->ea, kb);
    en1 = mulq16(v->en, v->choke ? kb : dv_kb(tn));
    ep1 = (int32_t)(((uint32_t)v->ep * dv_kb(tp)) >> 16);
    if (sp < slen) {                                     /* (GR: the stroke held) */
        ea1 = v->ea;
        en1 = v->en;
    }
    {
        uint32_t pm = (L->mode >> 5) & 3u;
        int32_t off = (depth * ep1) >> 15;              /* DECAY, LONG: the envelope */
        if (pm == DXM_NOISE) {                          /* (JIANT 0.4) a random pitch each block, the envelope's depth */
            int32_t r = v->rng ? v->rng : 0x2545F491;
            off = (off * ((int32_t)noise32(&r) >> 17)) >> 14;
            v->rng = r;
        } else if (pm == DXM_SINE) {                    /* .. a sine at 1/8 of the pitch */
            v->ph3 += v->inc << 3;                      /* (V10.6q: << 3, 1/8 at CTL 64; was 1/16) */
            off = (off * sine_i(v->ph3)) >> 15;
        }
        inc1 = pitch_inc((uint32_t)clamp(p16 + off, 0, 2047));
    }
    ng = q[DXP_NOISE] * 258;
    og = ((32767 - ng) * 11) >> 4;                       /* (x 11 / 16: about the DRUM kits' level) */
    ng = (ng * 11) >> 4;
    cw = wave == DXW_METAL && ln == DV_BELL;             /* (V10.6q) the cowbell: the CMOS one (below), its band on its
                                                          * pitch, between its two squares (its COLOR the timbre) */
    cx = cw ? clamp(((p16 - 360) * 19 >> 8) + 4, 0, 113) : q[DXP_COLOR] + (!ln << 5);   /* (the filter's cutoff, as a
                                                          * COLOR; V10.6z8: the KICK's 2 1/4 octaves up, its noise turned
                                                          * up a knock, a beater: not a rumble under the body) */
    if (flt < DXF_HP && cx < 96)                         /* (V10.6q) LP, BP: the band's power goes with its cutoff, the
                                                          * gain back as 1 / sqrt(fc) to COLOR 96's (x8 at most): a dark
                                                          * snare or clap as loud as a bright one (were up to 18 dB under) */
        ng = (ng * (int32_t)(dv_exp2((uint32_t)(cx < 12 ? 84 : 96 - cx) * 146u) >> 4)) >> 12;
    ga = mulq15(og, cw ? (ea0 >> 16) + (v->en >> 16) : ea0 >> 15) << 8;   /* the oscillator's gain, Q23, stepped (the
                                                          * cowbell's in two stages: half on the noise's SNAP, a click,
                                                          * half on the decay, the tail) */
    dga = ((mulq15(og, cw ? (ea1 >> 16) + (en1 >> 16) : ea1 >> 15) << 8) - ga) / (int32_t)n;
    gn = mulq15(ng >> 3, v->en >> 15) << 11;             /* the noise's (ng up to x8: >> 3) */
    {
        int32_t g1 = mulq15(ng >> 3, en1 >> 15) << 11;
        if (ln == DV_CLAP && (v->hpos < 1200u || v->hsum))   /* (V10.6q) the 808's claps; (0.7.9) HANDS: more hands */
            gn = dx_hands_step(v, n, gn, &g1, en1);
        dgn = (g1 - gn) / (int32_t)n;
    }
    inc = (int32_t)(v->inc >> 4) * 2;                    /* (>> 4: the step fits; x 2: half rate) */
    dinc = ((int32_t)(inc1 >> 4) * 2 - inc) / (int32_t)n;
    if (og) {                                            /* the oscillator */
        uint32_t ph = v->ph;
        int32_t fold = clamp(warp, 0, 127);
        fm = clamp(fm, 0, 127);
        warp = fm || lfb || lay ? 0x100 : 0;            /* (FM ENV, FEEDBK, LAYER: the loop below; the flag stripped there) */
        if (cw) {                                        /* (V10.6q) the 808's cowbell: two squares at the pitch and x1.47
                                                          * (540 / 800 Hz at PITCH 64), each a triangle clipped to an edge
                                                          * of ~2 samples (no wrap folded back: as good as PolyBLEP under
                                                          * the band), through the band-pass (below); COLOR the timbre: 0
                                                          * their mix .. 64 their ring (A B) .. 127 A's phase driven by B
                                                          * (FM) */
            uint32_t ph2 = v->ph2, st = (uint32_t)inc << 4;
            int32_t u = (q[DXP_COLOR] - 64) * 520, t = u < 0 ? u + 33280 : 32767;   /* (ring 0 .. 1 up to 64, FM 0 .. 1 over it) */
            int32_t ka = (int32_t)((1u << 29) / ((st >> 8) | 1u)), kb = (ka * 2787) >> 12;   /* (the edges' gains, Q8) */
            int32_t a = v->mo, fbk = DX_FB_CURVE[lfb > 15 ? 15 : lfb], um;
            u = u < 0 ? 0 : u;
            um = u + ((fm ? dx_fm_env(fm) * v->ep >> 15 : 0) * 43);   /* (FM ENV: A's modulation struck with the hit) */
            for (i = 0; i < n; i++, inc += dinc, ga += dga) {
                int32_t b = dx_sq(ph2, kb), o;
                a = dx_sq(ph + ((uint32_t)(dx_tri(ph2) * um) << 1) + (uint32_t)(a * fbk), ka);   /* (A's phase driven by B's
                                                          * triangle, up to pi: no phase jumps; FEEDBK its own) */
                o = (a + b) >> 2;                        /* (half scale: the sums fit) */
                o += mulq15(((mulq15(a, b) * 3) >> 2) - o, t);   /* (the ring x1.5: its sum and difference off the band) */
                o += mulq15(((a * 3) >> 2) - o, u);
                y[i] = mulq15(o << 1, ga >> 8);
                st = (uint32_t)inc << 4;
                ph += st;
                ph2 += (st >> 12) * 6021u;
            }
            v->mo = a;
            v->ph2 = ph2;
        } else if (wave == DXW_SINE && !warp) {          /* drive x0.25 (as good as clean) .. x2.5 into the soft clip;
                                                          * (V10.6q) on one line from COLOR 0 (was clean at 0, then x1 ..
                                                          * x2.5: a step), its peak back to the sine's: a colour, not a level */
            int32_t g = 1024 + q[DXP_COLOR] * 72, j = q[DXP_COLOR] >> 3, nrm = DX_NRM[j] + (((DX_NRM[j + 1] - DX_NRM[j]) * (q[DXP_COLOR] & 7)) >> 3);
            for (i = 0; i < n; i++, inc += dinc, ga += dga) {
                y[i] = mulq15((softclip((sine_i(ph) * g) >> 12) * nrm) >> 12, ga >> 8);
                ph += (uint32_t)inc << 4;
            }
        } else {                                         /* FM (up to ~1.5 cycles of phase: COLOR's index), and (0.6.36)
                                                          * FM ENV: the FM struck with the hit and gone with the pitch
                                                          * envelope (1..40 ms): a beater, a click, a metal edge on the
                                                          * attack, the tail clean; a SINE gets its octave as the
                                                          * modulator, the other waves their own ratio and COLOR's index
                                                          * under it. (V10.6o: one loop; without FM ENV, FEEDBK and LAYER
                                                          * its index steady, nothing fed back, as the plain FM loop was) */
            uint32_t ph2 = v->ph2, ratio = wave == DXW_SINE ? 8192u : DX_RATIO[wave];
            int32_t base = wave == DXW_SINE ? 0 : dx_fm_lim(q[DXP_COLOR] * 3, inc1, ratio), fe = fm ? dx_fm_lim(dx_fm_env(fm), inc1, ratio) : 0;   /* (snd1) */
            int32_t ix = base + ((fe * v->ep) >> 15), dix = ((base + ((fe * ep1) >> 15)) - ix) / (int32_t)n;
            int32_t o = v->mo, o1 = v->mo1, fbk = DX_FB_CURVE[lfb > 15 ? 15 : lfb];   /* (FEEDBK: on its curve, snd1) */
            dx_layer(v, y, n, &ph, &ph2, &o, &o1, inc, dinc, ga, dga, ix, dix, ratio, fbk, lay);   /* (0.6.49) LAYER: a second
                                                          * one at sqrt 2, FM'd by the first (V10.6q: its loop the only
                                                          * one, LAYER 0 none of it) */
            v->ph2 = ph2;
            if (warp) {                                  /* (the feedback's memory: only its own loop's) */
                v->mo = o;
                v->mo1 = (int16_t)o1;
            }
        }
        v->ph = ph;
        if (L->mode & DX_DRIVE) {                       /* (JIANT 0.4) DRIVE: x3 into the soft clip, the level near */
            int32_t dl = 14000 + !ln * 8000;             /* (V10.6q: 14000, was 22000: +4 dB louder; V10.6z8: the KICK's
                                                          * 22000 again, its punch as before V10.6q) */
            for (i = 0; i < n; i++)
                y[i] = (softclip(y[i] * 3) * dl) >> 15;
        }
        if (fold) {                                      /* (JIANT 0.5) FOLD: after the envelope, so it follows it */
            int32_t g = 16 + ((fold * fold * 5) >> 10), c = g * 6144 / (8 + g);   /* (Q4: x1 .. x5.9 on fold^2; V10.6q,
                                                          * was on fold: +5..+14 dB at the first steps); out x 1.5 g / (0.5
                                                          * + g), Q12: x1 at the start (was x1.5 at once) */
            for (i = 0; i < n; i++) {
                int32_t v = clamp((y[i] * g) >> 4, -5 * 16384, 5 * 16384), a;
                for (a = 0; a < 3; a++)
                    v = v > 16384 ? 32768 - v : v < -16384 ? -32768 - v : v;
                y[i] = (v * c) >> 12;
            }
        }
    } else {
        for (i = 0; i < n; i++)
            y[i] = 0;
    }
    mt = wave == DXW_METAL && (0x98u >> ln) & 1u ? -1 : 0;   /* (V10.6q) the hats' METAL through the filter
                                                          * with the noise, in their band (its clank at 1 .. 2.5 kHz was
                                                          * 13 % of a hat) */
    if (ng || mt) {                                      /* the noise (and METAL) through its filter */
        int32_t ic1 = v->ic1, ic2 = v->ic2, rng = v->rng;
        int32_t mr, ck, cv;                              /* (V10.6o: one loop) the output r & mr - k v1 - cv v2: LP v2,
                                                          * BP v1, HP r - k v1 - v2 (each exactly as its own loop was) */
        kd = flt == DXF_BP ? 2400 : 6000;                /* (BP a little resonant) */
        tsvf_coef_k(&c, (cx + 14) << 8, kd);   /* (+ an octave of the table: the same Hz at half rate) */
        mr = flt == DXF_LP || flt == DXF_BP ? 0 : -1;
        ck = flt == DXF_LP ? 0 : flt == DXF_BP ? -4096 : kd;
        cv = flt == DXF_LP ? -1 : flt == DXF_BP ? 0 : 1;
        for (i = 0; i < n; i++, gn += dgn) {
            int32_t r = mulq15((((int32_t)noise32(&rng) >> 17) * 23170) >> 15, gn >> 8) + ((y[i] & mt) >> 1), v3 = r - ic2, v1, v2;
                                                         /* (+-11585: 1/sqrt2, half rate: the same power in half the
                                                          * band, +3 dB; its gain before the filter, V10.6q) */
            v1 = (c.a1 * ic1 + c.a2 * v3) >> 13;
            v2 = ic2 + ((c.a2 * ic1 + c.a3 * v3) >> 13);
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            y[i] = (y[i] & ~mt) + clamp(((r & mr) - ((ck * v1) >> 12) - cv * v2) << 1, -32767, 32767);
        }
        v->ic1 = ic1;
        v->ic2 = ic2;
        v->rng = rng;
    }
    if (gr && dx_scrape(v, y, n, sp, slen, sinc))      /* (V10.7.8) GR: past its last tick, the hit is over */
        ea1 = en1 = 0;
    if (DX_TONE_BP(dx_tone) && (0xD8u >> ln) & 1u) {     /* (0.6.36) TONE's BP on the metal sounds (the hats, the rim,
                                                          * the cowbell); (V10.6q) around the sound's own COLOR, an
                                                          * octave under it .. over it, half dry (was 1.5 .. 9 kHz for all
                                                          * four and the band alone: 14 dB apart, the hats darkened) */
        int32_t b1 = v->b1, b2 = v->b2;
        tsvf_t cb;
        tsvf_coef_k(&cb, (q[DXP_COLOR] + 2 * (int32_t)DX_TONE_BP(dx_tone) - 2) << 8, 1800);   /* (half rate: + an octave) */
        for (i = 0; i < n; i++) {
            int32_t v3 = y[i] - b2, v1 = (cb.a1 * b1 + cb.a2 * v3) >> 13, v2 = b2 + ((cb.a2 * b1 + cb.a3 * v3) >> 13);
            b1 = clamp(2 * v1 - b1, -150000, 150000);
            b2 = clamp(2 * v2 - b2, -150000, 150000);
            y[i] = clamp((y[i] >> 1) + v1 - (v1 >> 2), -32767, 32767);   /* (its centre a little louder: the band sings) */
        }
        v->b1 = b1;
        v->b2 = b2;
    }
    for (i = n; i-- > 0;) {                              /* (0.6.17) back to full rate: each half-rate sample and the
                                                          * mean of it and the one before (the last block's: v->hl) */
        int32_t a = y[i], b = i ? y[i - 1] : v->hl;
        y[2u * i + 1u] = a;
        y[2u * i] = (a + b) >> 1;
    }
    if (nf & 1u)                                         /* (an odd block: its last sample held) */
        y[nf - 1u] = y[nf - 2u];
    v->hl = y[nf - 1u];
    v->ea = ea1;
    v->en = en1;
    v->ep = ep1;
    v->inc = inc1;
    if (v->ea < (1 << 19) && v->en < (1 << 19)) {       /* -66 dB (0.6.14: was -84: a tail computed below hearing) */
        v->live = 0;
        v->hl = 0;
    }
}

/* (0.7.2) the factory kits: DRUM's presets (eng_drum.c DRUM_PRESETS, one a kit, in this order), each the eight sounds
 * ({mode, A, B}, A and B {PITCH PMOD DECAY NOISE COLOR}), their LAYERs and their SENDs (bus << 6 | amount). The first
 * is the factory kit (DX_KIT_DEF, below). dx_kit is the music's kit: a section's (project.c keeps it in the project,
 * the song stages it with the section), edited on EDIT > SOUND (ui_input.c). Loaded whole by a preset (ui.c
 * apply_preset_to: dx_kit_load) into the section's kit; INIT SOUND and a new project take the first */
/* (V10.6z8, the owner: "the kicks sound like a snare", "the hats a giant low clap") every KICK as before V10.6q (LP,
 * its old NOISE and COLOR: V10.6q's HP noise at 40 was the snare in it); the 808 BOOM, LOFI DUST, ACOUSTIC, DUB ROOTS
 * and HIP HOP hats' COLOR up (their band at 1.2 .. 2.5 kHz under the preset's TONE: now ~5 .. 8 kHz) */
typedef struct { dx_lane_t lane[8]; uint8_t lay[8]; uint8_t snd[8]; } dx_kit_t;
static const dx_kit_t DX_KITS[] = {
    {   /* DRUM-X */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {9, 70, 72, 4, 20}, {14, 100, 88, 10, 60}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 0), {40, 30, 52, 80, 90}, {46, 18, 64, 104, 70}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 46, 127, 84}, {54, 0, 58, 127, 76}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {100, 0, 22, 96, 116}, {108, 0, 14, 112, 122}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {100, 0, 62, 96, 112}, {108, 0, 74, 112, 118}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {30, 40, 66, 14, 54}, {36, 60, 76, 20, 74}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {63, 10, 14, 16, 46}, {70, 24, 20, 30, 60}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 34, 6}, {68, 0, 54, 46, 28}},   /* BELL */
        },
        {0, 6, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* 909 CLUB */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1) | DX_DRIVE, {7, 90, 68, 4, 50}, {12, 120, 84, 10, 90}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 0), {40, 34, 60, 92, 104}, {46, 22, 72, 116, 84}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 52, 127, 88}, {54, 0, 64, 127, 80}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {106, 0, 16, 96, 126}, {114, 0, 8, 112, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {106, 0, 66, 96, 122}, {114, 0, 78, 112, 127}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {32, 54, 66, 14, 68}, {38, 74, 76, 20, 88}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {65, 10, 12, 16, 50}, {72, 24, 18, 30, 64}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 34, 6}, {68, 0, 54, 46, 28}},   /* BELL */
        },
        {0, 6, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* 808 BOOM */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1) | DXM_LONG << 5, {3, 40, 102, 0, 6}, {8, 70, 118, 6, 46}},   /* KICK */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {36, 18, 46, 90, 66}, {42, 6, 58, 114, 46}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {56, 0, 50, 127, 70}, {50, 0, 62, 127, 62}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {92, 0, 14, 96, 118}, {100, 0, 6, 112, 124}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {92, 0, 60, 96, 114}, {100, 0, 72, 112, 120}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1) | DXM_LONG << 5, {24, 26, 82, 14, 42}, {30, 46, 92, 20, 62}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {65, 10, 14, 16, 38}, {72, 24, 20, 30, 52}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {62, 0, 56, 34, 0}, {66, 0, 66, 46, 16}},   /* BELL */
        },
        {0, 4, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* LOFI DUST */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {11, 78, 58, 14, 0}, {16, 108, 74, 20, 40}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 1), {44, 30, 36, 104, 68}, {50, 18, 48, 127, 48}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {62, 0, 36, 127, 60}, {56, 0, 48, 127, 52}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {104, 0, 12, 96, 124}, {112, 0, 4, 112, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {104, 0, 48, 96, 120}, {112, 0, 60, 112, 126}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {32, 40, 52, 20, 34}, {38, 60, 62, 30, 54}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {69, 10, 8, 16, 30}, {76, 24, 14, 30, 44}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {66, 0, 32, 34, 10}, {70, 0, 42, 46, 36}},   /* BELL */
        },
        {0, 9, 0, 0, 0, 0, 5, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* INDUSTRIAL */
        {
            {DX_MODE(DXW_FM, DXF_LP, 1) | DX_DRIVE, {7, 104, 80, 18, 64}, {12, 127, 96, 24, 104}},   /* KICK */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {42, 54, 64, 102, 124}, {48, 42, 76, 126, 104}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 54, 127, 108}, {54, 0, 66, 127, 100}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {100, 0, 32, 106, 127}, {108, 0, 24, 122, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {100, 0, 76, 106, 127}, {108, 0, 88, 122, 127}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1) | DX_DRIVE, {30, 64, 76, 20, 88}, {36, 84, 86, 30, 108}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1) | DXM_NOISE << 5, {63, 22, 20, 28, 70}, {70, 36, 26, 42, 84}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1) | DXM_NOISE << 5, {64, 0, 48, 46, 96}, {68, 0, 58, 58, 120}},   /* BELL */
        },
        {0, 12, 0, 0, 0, 0, 6, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* ACOUSTIC */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {9, 60, 64, 12, 0}, {14, 90, 80, 18, 34}},   /* KICK */
            {DX_MODE(DXW_BELL, DXF_BP, 0), {38, 30, 56, 98, 74}, {44, 18, 68, 122, 54}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {58, 0, 48, 127, 66}, {52, 0, 60, 127, 58}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {96, 0, 18, 96, 116}, {104, 0, 10, 112, 122}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {96, 0, 66, 96, 112}, {104, 0, 78, 112, 118}},   /* HAT OP */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {28, 28, 70, 20, 8}, {34, 48, 80, 30, 28}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {61, 10, 16, 16, 26}, {68, 24, 22, 30, 40}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {62, 0, 46, 34, 20}, {66, 0, 56, 46, 44}},   /* BELL */
        },
        {0, 5, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* DUB ROOTS */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1) | DXM_LONG << 5, {3, 58, 92, 4, 0}, {8, 88, 108, 10, 38}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 0), {34, 30, 64, 88, 80}, {40, 18, 76, 112, 60}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {58, 0, 52, 127, 72}, {52, 0, 64, 127, 64}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {98, 0, 20, 96, 122}, {106, 0, 12, 112, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {98, 0, 68, 96, 118}, {106, 0, 80, 112, 124}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {26, 40, 76, 14, 44}, {32, 60, 86, 20, 64}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {59, 10, 24, 16, 46}, {66, 24, 30, 30, 60}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 52, 34, 8}, {68, 0, 62, 46, 40}},   /* BELL */
        },
        {0, 6, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* TECHNO RAW */
        {
            {DX_MODE(DXW_FM, DXF_LP, 1) | DX_DRIVE, {7, 96, 74, 10, 56}, {12, 126, 90, 16, 96}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_HP, 0), {40, 40, 46, 102, 110}, {46, 28, 58, 126, 90}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 48, 127, 94}, {54, 0, 60, 127, 86}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {104, 0, 14, 100, 127}, {112, 0, 6, 116, 127}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_HP, 0), {104, 0, 62, 100, 127}, {112, 0, 74, 116, 127}},   /* HAT OP */
            {DX_MODE(DXW_METAL, DXF_HP, 1), {30, 52, 68, 16, 46}, {36, 72, 78, 26, 66}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {65, 10, 10, 22, 58}, {72, 24, 16, 36, 72}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 40, 70}, {68, 0, 54, 52, 96}},   /* BELL */
        },
        {0, 7, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* BREAKS */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {11, 78, 68, 12, 32}, {16, 108, 84, 18, 72}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 1), {42, 38, 54, 96, 112}, {48, 26, 66, 120, 92}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {60, 0, 48, 127, 94}, {54, 0, 60, 127, 86}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {100, 0, 26, 102, 116}, {108, 0, 18, 118, 122}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {100, 0, 70, 102, 112}, {108, 0, 82, 118, 118}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {32, 48, 66, 18, 66}, {38, 68, 76, 28, 86}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {63, 10, 14, 20, 56}, {70, 24, 20, 34, 70}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 34, 6}, {68, 0, 54, 46, 28}},   /* BELL */
        },
        {0, 6, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* HIP HOP (V10.34, a user's ask): boom bap: a round kick with a knock, a fat cracking snare, a tight clap,
         * dusty hats (band-passed), a low tom, a woody rim, a shaker in the BELL's place */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {6, 84, 74, 8, 26}, {10, 112, 90, 16, 62}},   /* KICK */
            {DX_MODE(DXW_FM, DXF_BP, 1), {38, 34, 62, 106, 82}, {44, 22, 76, 124, 62}},   /* SNARE */
            {DX_MODE(DXW_SINE, DXF_BP, 0), {58, 0, 42, 127, 76}, {52, 0, 54, 127, 66}},   /* CLAP */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {98, 0, 12, 96, 116}, {106, 0, 6, 112, 122}},   /* HAT CL */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {98, 0, 54, 96, 110}, {106, 0, 66, 112, 116}},   /* HAT OP */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {26, 36, 70, 14, 40}, {32, 56, 80, 20, 60}},   /* TOM */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {61, 10, 10, 16, 34}, {68, 24, 16, 30, 48}},   /* RIM */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {84, 0, 20, 110, 70}, {92, 0, 28, 124, 80}},   /* SHAKER */
        },
        {0, 8, 0, 0, 0, 0, 4, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
    {   /* LATIN PERC (V10.7.8, the owner: a guiro, the percussion of cumbia, reggaeton, baile funk): a bombo, a timbal,
         * the llamador, the GUIRO on the closed hat (the filter GR: an accent its long stroke, a soft hit its "chk"),
         * maracas, a conga, the clave, the cowbell */
        {
            {DX_MODE(DXW_SINE, DXF_LP, 1), {9, 70, 66, 4, 20}, {14, 96, 80, 10, 50}},   /* BOMBO */
            {DX_MODE(DXW_BELL, DXF_BP, 0), {50, 20, 40, 70, 96}, {56, 12, 50, 90, 80}},   /* TIMBAL */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {54, 30, 40, 20, 60}, {60, 40, 48, 30, 70}},   /* LLAMADOR */
            {DX_MODE(DXW_METAL, DXF_GR, 0), {100, 110, 20, 127, 106}, {108, 122, 26, 127, 112}},   /* GUIRO */
            {DX_MODE(DXW_METAL, DXF_BP, 0), {84, 0, 22, 118, 110}, {92, 0, 30, 124, 116}},   /* MARACAS */
            {DX_MODE(DXW_SINE, DXF_HP, 1), {44, 30, 50, 12, 40}, {50, 44, 58, 18, 56}},   /* CONGA */
            {DX_MODE(DXW_BELL, DXF_HP, 1), {76, 6, 10, 8, 30}, {82, 14, 14, 16, 44}},   /* CLAVE */
            {DX_MODE(DXW_METAL, DXF_BP, 1), {64, 0, 44, 34, 6}, {68, 0, 54, 46, 28}},   /* COWBELL */
        },
        {0, 4, 0, 0, 0, 0, 0, 0}, {0, 0, 0, 0, 0, 0, 0, 0},   /* the layers; the sends (bus << 6 | amount: V10.7.10 none, every kit dry) */
    },
};
#define DX_NKIT ((uint32_t)(sizeof DX_KITS / sizeof DX_KITS[0]))
#define DXK_LATIN 10u                                    /* (V10.7.8) LATIN PERC (gen.c gen_kit: the latin genres' percussion) */
_Static_assert(DXK_LATIN + 1u == DX_NKIT, "LATIN PERC the last kit");
/* (V10.6o) the factory kit (DX_KIT_DEF: what a project without one gets) and (0.6.49) its LAYERs (FEEDBK 0: the snare,
 * the hats and the bell, no note to them; the rest clean, CPU): the first kit's, DRUM-X (were copies of it) */
#define DX_KIT_DEF (DX_KITS[0].lane)
#define DX_LAY_DEF (DX_KITS[0].lay)
/* a stored kit as the voice takes it: values 0..127, the mode's fields in range (wave 0..3, filter 0..2, snap) */
static int dx_kit_ok(const dx_lane_t *k)
{
    uint32_t l, i;
    for (l = 0; l < 8u; l++) {
        for (i = 0; i < DXP_N; i++)
            if (k[l].a[i] > 127u || k[l].b[i] > 127u)
                return 0;
    }
    return 1;
}


/* Classic callers retain exactly the original feedback path. */
static void dx_run(const dx_lane_t *L, dx_voice_t *v, int32_t morph, int32_t tune16,
                   int32_t dofs, int32_t cofs, int32_t nofs, int32_t warp, int32_t fm, int32_t *y, uint32_t n)
{ dx_run_fx(L,v,morph,tune16,dofs,cofs,nofs,warp,fm,y,n,0); }
