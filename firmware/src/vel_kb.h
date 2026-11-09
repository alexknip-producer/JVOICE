/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z6) Key velocity and pressure as the keys' block (seq.c) reads them: written by the key scan's ISR (vel_diag.c
 * vd_sample, vel.c), the settings by the main loop (vel.c vel_poll). No HAL here: seq.c and the host tests include it
 * on its own (everything off: velocity 100, no pressure, as before V10.6z6). */
#pragma once
#define VK_BSS __attribute__((section(".bss.kx")))   /* (out of the merged statics, as seq.c's KX_BSS: theirs keep their
                                                    * places) */
#define VK_NTRK 6u                      /* the tracks (core.h NTRK: seq.c checks) */
#define VK_NK 20u                       /* the keys with a converter: F3..C5 (the top row C#5..G5 has none) */
#define VK_WAIT_T 22u                   /* (V10.7.10) a press waits for its strike's window at most this many scan ticks
                                         * (100 us each: 2.2 ms, one sample period of its key) from its debounce, whatever
                                         * its readings do (a conversion timed out, none at all): vel.c vel_tick */
typedef struct {
    uint8_t vel;                        /* the strike's velocity 1..127 so far (0: no driven reading yet) */
    uint8_t at;                         /* the key's pressure 0..127, smoothed (0 when let go) */
    uint8_t pend;                       /* touched, the strike's window still open (the note may wait for it) */
} vk_t;
/* the settings (main loop: menu_items.c, vel.c vel_poll): favorites.factory[15][13] VELOCITY (bits 0-1, its place ^ 2:
 * 0, every older setting, NORMAL) and VEL MIN (bits 2-4, its place ^ 2: 20); [15][14] AFTERTOUCH (bits 0-1, its place
 * ^ 2: MID), bits 2-3 VEL LEARN (0 ON, 1 OFF, 2 RESET: the seeds put back when picked, then as ON) */
#define VEL_SET (favorites.factory[15][13])
#define VEL_SET2 (favorites.factory[15][14])
/* the keys' velocity state, one block (one base address for the ISR's code; out of the merged statics, VK_BSS) */
typedef struct {                        /* (the bytes first: short offsets) */
    uint8_t on;                         /* bit 0 VELOCITY not OFF, bit 1 AFTERTOUCH not OFF (vel.c vel_poll) */
    uint8_t mean;                       /* the mean of the recent velocities: the top row's (no converter); 0: 100 */
    uint8_t cal;                        /* the main loop's calibration (vel.c vel_poll): 0 to read from the settings
                                         * (power-on, a restore: settings_persist.c), 1 read, 2 to reset (VEL LEARN RESET) */
    uint8_t curve, min, atr, learn;     /* (vel.c) the settings as the ISR reads them: curve 0..3, MIN, AFTERTOUCH 0..3 */
    uint8_t dirty, last, rn;            /* (vel.c) learnt since the last save; the last strike's key; the strikes */
    uint8_t diag, dlv;                  /* (vel_diag.c) the diagnostic on (its presses and captures queued), its drive */
    uint8_t atl[VK_NTRK], ato[VK_NTRK];       /* (seq.c kx_block) each track's held keys' pressure; MIDI OUT's last / 2 */
    uint8_t arpv[VK_NTRK];              /* (seq.c) each track's arp notes' velocity: its keys' last, ^ 100 */
    vk_t k[VK_NK];
    uint8_t cnt[VK_NK];                 /* a waiting press: the scan ticks it may still wait (VK_WAIT_T at its debounce) */
    uint8_t km[VK_NK];                  /* (V10.7.10, vel.c) each key's recent strikes' mean (0: none yet): a press with no
                                         * reading of its own plays it */
    uint8_t n[VK_NK];                   /* (vel.c) strikes learnt since a reset (to 24) */
    uint8_t velx[27];                   /* each key's press's velocity ^ 100 (0, as zeroed: 100) */
    uint32_t hold;                      /* the keys whose press waits for its strike's window (the scan's ISR, vel.c
                                         * vel_note_on): the keys' block (seq.c) sees them pressed once let through */
    uint32_t t;                         /* (vel.c) the last strike learnt, fm1_ms */
    uint32_t now;                       /* (V10.7.10, vel.c vel_poll: seq.c kb_drum_keys) the keys whose press never waits:
                                         * those going to a DRUM track (a drum needs its hit on time; no velocity yet: the
                                         * key's recent mean) */
    uint16_t at8[VK_NTRK];                 /* (seq.c kx_block) each track's pressure, Q8 */
    uint16_t strike[VK_NK];             /* (vel_diag.c) each key's strike: the lowest driven reading from the touch to
                                         * the first driven sample after it seated (VD_NONE: the window still open) */
    uint16_t vd[VK_NK];                 /* (vel.c) the reading each key's last velocity came from */
    uint16_t ref[VK_NK][3];             /* (vel.c) each key's HARD, NORMAL, SOFT strike, 1/16 count */
} vel_t;
static vel_t vs VK_BSS;
#define vk (vs.k)
#define vk_on (vs.on)
#define vk_mean (vs.mean)
#define vk_hold (vs.hold)
#define vk_cnt (vs.cnt)
#define vk_cal (vs.cal)
#define kb_velx (vs.velx)
#define KB_VEL(k) (kb_velx[k] ^ 100u)
/* the velocity of key k's press now: 100 with VELOCITY OFF (bit for bit as before); its strike's so far, else (V10.7.10)
 * the key's recent mean, else every key's */
static inline uint32_t vk_vel(uint32_t k)
{
    if (!(vk_on & 1u))
        return 100u;
    if (k < VK_NK && (vk[k].vel || vs.km[k]))
        return vk[k].vel ? vk[k].vel : vs.km[k];
    return vk_mean ? vk_mean : 100u;
}
