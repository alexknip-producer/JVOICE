/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z6) Key velocity and pressure: stage 2 of the velocity diagnostic (vel_diag.c), after Velo (Charles Vestal,
 * github.com/charlesvestal/fm1-velocity, GPL-3.0: the drive and the reading of the carbon pill are his findings).
 * The 20 keys F3..C5 (rows PA7 / PA8); the top row C#5..G5 has no converter and plays the mean of the recent velocities.
 *
 * The strike (vd_sample, the scan's ISR): a key touched is sampled every 2.2 ms, every other sample with its row driven
 * high; its velocity comes from the lowest driven reading of an early window: from the touch to the first driven sample
 * after the key seated, or after its press was debounced, whichever comes first (the owner's two captures,
 * docs/VELOCITY_DIAG.es.md: the minimum in an early window is the reliable measure, lower = harder; a sample that
 * catches the key still falling reads high). The digital scan debounces a press 1.1..2.2 ms after its contact closes;
 * a press whose window is still open then waits (vel_note_on: its bit held back from the keys' block, vk_hold) and its
 * key's next sample is taken driven: the window closes there, at most one sample period later. (V10.7.10, a user: "drums
 * almost impossible to use") The wait is capped in scan ticks, not in samples: VK_WAIT_T (22 ticks, 2.2 ms) after the
 * debounce the press plays whatever its readings did (a conversion that timed out, fm1_an.timeouts, drops its reading:
 * before, the wait then ran on to the key's next one, 4.4 ms or more), with the velocity so far, else the key's recent
 * mean (vel_tick). A key whose touch has not been sampled yet waits the same. A key going to a DRUM track (seq.c
 * kb_drum_keys, as keyboard_block routes it: the selected track or KEY SPLIT's half; the DRUM grid) never waits: it
 * plays at its debounce with the velocity read so far, else its recent mean; its strike is still read and learnt.
 * tests/vel_test.c: 400 modelled presses, with conversions timing out and with none at all: the wait never over 2.2 ms;
 * on a DRUM track none. No setting: the synth tracks keep the (capped) wait. The pressure's zero is the lowest driven
 * reading once seated.
 *
 * The velocity (vel_map), continuous: each key keeps three references, its HARD, NORMAL and SOFT strikes (vs.ref);
 * the strike's reading maps piecewise-linearly through them, HARD 112, NORMAL 76, SOFT 36, on at half the slope past
 * either end (clamped 1..127); then MENU > CONTROL > VELOCITY's curve (SOFT: louder for less, v + v (127 - v) / 254;
 * NORMAL as it is; HARD: v - v (127 - v) / 254) and VEL MIN's floor (1..127 scaled onto MIN..127, no step).
 * VELOCITY OFF: every key 100, as before V10.6z6 (and with AFTERTOUCH OFF too the scan samples nothing: bit for bit).
 *
 * The calibration (vel_lift, at the key's release; MENU > CONTROL > VEL LEARN ON, the default): each seated strike's
 * reading (the one its velocity came from) moves the key's references a step towards its own running quantiles, sign steps that no freak strike can throw (HARD the
 * 15th percentile: down 34 / up 6 sixteenths of a count; NORMAL the median, 16 / 16; SOFT the 85th, up 34 / down 6;
 * four times that for a key's first 24 strikes after a reset), kept at least 8 (HARD..NORMAL) and 16 (NORMAL..SOFT)
 * counts apart. Seeded from the owner's measured medians (HARD 560, NORMAL 594, SOFT 675, DRIVE 2) moved by each key's
 * offset where the captures had it (VC_OFS: its mean against the others'). Kept in the settings (favorites row 4, the
 * retired SAMPLE engine's: no engine reads it; 12 bits a key, NORMAL 400..904 by 8, SOFT - NORMAL 16..128 by 16,
 * NORMAL - HARD 8..64 by 8), written 30 s after the last strike learnt with no key held and the transport
 * stopped (project.c settings_poll waits for that, as for any setting); VEL LEARN RESET puts the seeds back.
 *
 * The pressure (vel_driven; MENU > CONTROL > AFTERTOUCH LOW / MID / HIGH, MID the default; OFF none): a held key's
 * driven reading under its strike, past a dead zone (a finger resting does not move it): (strike - reading) x 1 / 2 / 4
 * - 16, 0..127 (the dead zone 16 / 8 / 4 counts, full pressure 143 / 72 / 36 counts under the strike); smoothed a
 * quarter of the way a driven sample (~18 ms), 0 when the key is let go. seq.c kx_block: a track's is its held keys'
 * highest, slewed, into the matrix's AT. */
#define VC_NOM_H 560
#define VC_NOM_N 594
#define VC_NOM_S 675
#define VC_MARK 0xA7u                   /* favorites.factory[4][1]: the calibration saved */
#define VEL_BARRIER() __asm__ volatile("" ::: "memory")
static const int8_t VC_OFS[VK_NK] = {0, 0, 0, 0, 0, 0, 0, 15, 0, 77, -1, 0, 5, 0, 34, 0, 0, -31, 0, 0};
static const uint8_t VC_MIN[8] = {1, 10, 20, 30, 40, 50, 64, 80};
static uint32_t kb_drum_keys(void);             /* (V10.7.10, seq.c) the keys going to a DRUM track */

static __attribute__((noinline)) void vel_cal_reset(void)
{
    uint32_t k;
    for (k = 0; k < VK_NK; k++) {
        int32_t o = VC_OFS[k];
        vs.ref[k][0] = (uint16_t)((VC_NOM_H + o) << 4);
        vs.ref[k][1] = (uint16_t)((VC_NOM_N + o) << 4);
        vs.ref[k][2] = (uint16_t)((VC_NOM_S + o) << 4);
        vs.n[k] = 0;
    }
}

/* the velocity of driven reading d on key key (top): HARD 112, NORMAL 76, SOFT 36, linear between, half the slope past
 * either end; the curve; the floor */
static __attribute__((noinline)) uint32_t vel_map(uint32_t key, uint32_t d)
{
    const uint16_t *r = vs.ref[key];
    int32_t x = (int32_t)d, j = x > r[1] >> 4, lo = r[j] >> 4, hi = r[j + 1] >> 4, ylo = j ? 76 : 112, yhi = j ? 36 : 76, v;
    v = ylo + (yhi - ylo) * (x - lo) / (hi - lo);
    if (x < lo || x > hi)
        v = (v + (x < lo ? ylo : yhi)) / 2;
    v = v < 1 ? 1 : v > 127 ? 127 : v;
    if (vs.curve & 1u)                          /* SOFT 1, HARD 3 */
        v += (vs.curve == 1u ? 1 : -1) * v * (127 - v) / 254;
    return (uint32_t)(vs.min + (v - 1) * (127 - vs.min) / 126);
}

/* (ISR) key k's press let through to the keys' block (seq.c) with its velocity */
static __attribute__((noinline)) void vel_take(uint32_t k)
{
    kb_velx[k] = (uint8_t)(vk_vel(k) ^ 100u);
    VEL_BARRIER();                              /* (the velocity before the press: the audio ISR may come between) */
    vk_hold &= ~(1u << k);
}
/* (the scan's ISR, hal/fm1_input.h fm1__key) key k's press debounced, before its bit sets: its strike's window still
 * open, it waits (at most VK_WAIT_T scan ticks: vel_tick), else it plays now; (V10.7.10) a key going to a DRUM track
 * (vs.now) never waits: its velocity as far as it is read, else the key's recent mean */
static __attribute__((noinline)) void vel_note_on(uint32_t k)
{
    if ((vk_on & 1u) && k < VK_NK && !((vs.now >> k) & 1u) && (vk[k].pend || vd_key[k].st == VD_OPEN)) {   /* (open:
                                                 * its touch not sampled yet) */
        vk_cnt[k] = VK_WAIT_T;
        vk_hold |= 1u << k;
        if (vd_key[k].st != VD_OPEN)
            vd_key[k].dnext = 1;                /* (its next sample driven: the window closes there) */
    } else {
        vel_take(k);
    }
    VEL_BARRIER();                              /* (before the caller sets the key's bit) */
}
/* (V10.7.10; the scan's ISR, every tick, after the sample taken: hal/fm1_input.h FM1_INPUT_TICK) a press waiting past
 * VK_WAIT_T ticks plays with the velocity so far (a conversion timed out, its reading dropped: no longer than that) */
static __attribute__((noinline)) void vel_tick(void)
{
    uint32_t k;
    if (!vk_hold)
        return;
    for (k = 0; k < VK_NK; k++)
        if ((vk_hold >> k) & 1u && !--vk_cnt[k])
            vel_take(k);
}
/* (ISR, vd_sample) a driven reading v of key key, touched: the strike's window (its velocity so far from k's lowest
 * driven reading; it closes once seated, or once its press waits: vel_note_on), the pressure's zero once seated, then
 * the pressure (top) */
static __attribute__((noinline)) void vel_driven(uint32_t key, const vd_key_t *k, uint32_t v)
{
    uint32_t d = k->dmin, s = vs.strike[key];
    int32_t t, a = vk[key].at;
    if (vk[key].pend && vk_on) {
        uint32_t x = vel_map(key, d);
        vk[key].vel = (uint8_t)x;
        if (k->st == VD_DOWN || ((vk_hold >> key) & 1u)) {   /* (closed) */
            vk[key].pend = 0;
            vs.vd[key] = (uint16_t)d;
            if ((vk_hold >> key) & 1u)
                vel_take(key);
            vk_mean = (uint8_t)(((vk_mean ? vk_mean : 100u) * 7u + x + 4u) / 8u);
            vs.km[key] = (uint8_t)(vs.km[key] ? (vs.km[key] * 3u + x + 2u) / 4u : x);   /* (V10.7.10: the key's own) */
            vs.last = (uint8_t)key;
            vs.rn++;
        }
    }
    if (s == VD_NONE) {
        if (k->st == VD_DOWN)
            vs.strike[key] = (uint16_t)d;       /* (settled: the pressure's zero) */
        return;
    }
    if (!vs.atr)
        return;
    t = ((int32_t)s - (int32_t)v) * (1 << vs.atr >> 1) - 16;   /* (LOW 1, MID 2, HIGH 4 a count, the dead zone 16 / 8 / 4) */
    t = t < 0 ? 0 : t > 127 ? 127 : t;
    vk[key].at = (uint8_t)(a + (t - a + (t > a ? 3 : -3)) / 4);   /* (smoothed) */
}

/* (ISR) key let go: no pressure; a seated press (strike VD_NONE: a graze) moves its references by the reading its
 * velocity came from (top) */
static __attribute__((noinline)) void vel_lift(uint32_t key, uint32_t strike)
{
    static const uint8_t DN[3] = {34, 16, 6}, UP[3] = {6, 16, 34};
    uint16_t *r = vs.ref[key];
    int32_t m = (int32_t)vs.vd[key] << 4, f = vs.n[key] < 24u ? 4 : 1, j, n;
    vk[key].at = 0;
    if (strike == VD_NONE || vk[key].pend || !vs.learn || !(vk_on & 1u) || fm1_an.drive != 2u)
        return;
    for (j = 0; j < 3; j++)
        r[j] = (uint16_t)(r[j] + (m < r[j] ? -DN[j] : UP[j]) * f);
    n = r[1];
    if (r[0] > n - (8 << 4))
        r[0] = (uint16_t)(n - (8 << 4));
    if (r[2] < n + (16 << 4))
        r[2] = (uint16_t)(n + (16 << 4));
    vs.n[key] += vs.n[key] < 24u;
    vs.dirty = 1;
    vs.t = fm1_ms;
}

/* ---------------------------------------------------------------- main loop --- */
/* the settings (vel_kb.h VEL_SET, VEL_SET2) and the calibration's bytes: favorites row 4 (top) */
#define VC_ROW (favorites.factory[ENGI_SAMPLE])
/* x (counts) as a field: (x - lo) / 2^sh rounded, 0..top */
static int32_t vc_q(int32_t x, int32_t lo, int32_t sh, int32_t top)
{
    x = (x - lo + (1 << sh >> 1)) >> sh;
    return x < 0 ? 0 : x > top ? top : x;
}
/* the calibration into the settings (put 1; 2: the seeds first), or out of them (0; none saved: the seeds): 12 bits a
 * key, little-endian from
 * byte 2: NORMAL 400 + 8 n (6 bits), SOFT - NORMAL 16 + 16 s, NORMAL - HARD 8 + 8 h (3 bits each) */
static __attribute__((noinline)) void vc_io(uint32_t put)
{
    uint32_t k;
    if (put != 1u)                              /* (0: read, 2: reset, 1: write) */
        vel_cal_reset();
    if (put) {
        memset(&VC_ROW[1], 0, 31);
        VC_ROW[1] = VC_MARK;
    }
    for (k = 0; k < VK_NK && VC_ROW[1] == VC_MARK; k++) {
        uint16_t *r = vs.ref[k];
        uint8_t *c = &VC_ROW[2u + k * 3u / 2u];
        uint32_t sh = (k & 1u) * 4u, w = c[0] | (uint32_t)c[1] << 8, b;
        int32_t n = r[1] >> 4;
        if (put) {
            b = (uint32_t)(vc_q(n, 400, 3, 63) | vc_q((r[2] >> 4) - n, 16, 4, 7) << 6 | vc_q(n - (r[0] >> 4), 8, 3, 7) << 9);
            w |= b << sh;
            c[0] = (uint8_t)w;
            c[1] = (uint8_t)(w >> 8);
        } else {
            b = w >> sh;
            n = (int32_t)(400u + ((b & 63u) << 3)) << 4;
            r[1] = (uint16_t)n;
            r[2] = (uint16_t)(n + ((16 + (int32_t)((b >> 2) & 0x70u)) << 4));
            r[0] = (uint16_t)(n - ((8 + (int32_t)((b >> 6) & 0x38u)) << 4));
            vs.n[k] = 24;
        }
    }
    vk_cal = 1;
}
/* every pass: the settings into the ISR's copies, the sampling as they want it (vel_diag.c vd_apply); the calibration
 * read again after a restore, saved once learnt and quiet */
static void vel_poll(void)
{
    uint32_t c = VEL_SET ^ 0x0Au, a = VEL_SET2 ^ 2u, on;   /* (each field's place ^ 2) */
    vs.curve = (uint8_t)(c & 3u);
    vs.min = VC_MIN[(c >> 2) & 7u];
    vs.atr = (uint8_t)(a & 3u);
    vs.learn = (a & 12u) != 4u;                 /* (VEL LEARN ON 0, OFF 1, RESET 2: learning) */
    on = (vs.curve != 0u) | (uint32_t)(vs.atr != 0u) << 1;
    vs.now = kb_drum_keys();                    /* (V10.7.10) the keys going to a DRUM track: no wait */
    if (vk_cal != 1u || (vs.dirty && fm1_ms - vs.t >= 30000u && !fm1_in.notes)) {   /* (power-on, a restore: read;
                                                 * RESET: the seeds; learnt, no key held: saved, settings_poll once
                                                 * the transport stops) */
        uint32_t m = vk_cal;
        vs.dirty = 0;
        vc_io(m);
        if (m)
            settings_save();
    }
    if (on != vk_on) {
        vk_on = (uint8_t)on;
        vk_hold = 0;                            /* (no press left waiting) */
        vd_apply();
    }
}
