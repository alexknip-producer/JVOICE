/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* USB full-speed device on USB0: a
 * class-compliant USB-MIDI interface. All SIE traffic happens in one place,
 * usb_poll(), called from the TIMER5 ISR at 2 kHz (uac_service at 10 kHz), so INDEX is never shared
 * and no USB IRQ is needed. MIDI in goes to a ring the audio ISR drains;
 * MIDI out (the panel keys) comes from a ring the audio ISR fills.
 * SysEx F0 22 24 35 7D F7 (the stock soft key, tools/fm1_softkey.py) asks
 * the main loop to enter UBOOT. VID 0x1209 / PID 0x0001 is the pid.codes
 * TEST id: fine for the bench, must be replaced before any release.
 * FELUCCA_CDC=1 adds a CDC-ACM serial function (IAD composite: EP2 notify,
 * EP3 bulk data) for the console in console.c; FELUCCA_USB_LAYOUT picks how
 * the composite presents itself, usb_cdc_switch / FELUCCA_CDC_DEFAULT=0 leave
 * the console out (descriptors, below).
 * FELUCCA_UAC=1 adds a USB audio input (UAC1, class compliant): the master
 * output as 16-bit stereo at 44.1 kHz on an isochronous asynchronous IN
 * endpoint (EP4), so a computer can record the FM-1 over the cable. audio.c
 * fills a ring (uac_render_start, uac_tap); uac_service sends one packet per
 * USB frame from the TIMER5 ISR, nested in the render too (main.c). The
 * update loader leaves both off: MIDI only.
 * FELUCCA_UAC_48K=1 (the default with FELUCCA_UAC) offers 48 kHz too: the host picks the rate with
 * SET_CUR (UAC1 sampling frequency control, two discrete rates in the one alt setting). The engines stay
 * at 44.1 kHz; at 48 kHz uac_tap resamples (polyphase FIR 160 / 147, uac_fir.h) into the ring. 44.1 kHz,
 * the rate until a host asks for another, sends the samples as before, bit for bit. (0.8.9) With CORES 2 the
 * second core resamples each half after the render (uac_render_end), off the first core's render; one core: each
 * block in the ISR as before (uac_rs_run, in RAM either way). An underrun never drops or shortens a packet. */
#include "../hal/fm1_usb.h"   /* registers; relative, so the loader and the host tests find it too */
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 0
#endif
#ifndef FELUCCA_UAC
#define FELUCCA_UAC 0
#endif
#ifndef FELUCCA_UAC_48K
#define FELUCCA_UAC_48K 1
#endif
#if !FELUCCA_UAC
#undef FELUCCA_UAC_48K
#define FELUCCA_UAC_48K 0
#endif
enum { S_FADDR = 0, S_POWER = 1, S_INTRTX1 = 2, S_INTRTX2 = 3, S_INTRRX1 = 4, S_INTRRX2 = 5, S_INTRUSB = 6,
       S_INTRTX1E = 7, S_INTRTX2E = 8, S_INTRRX1E = 9, S_INTRRX2E = 10, S_INTRUSBE = 11, S_FRAME1 = 12,
       S_FRAME2 = 13, S_INDEX = 14,
       S_TXMAXP = 16, S_CSR0 = 17, S_TXCSR1 = 17, S_TXCSR2 = 18, S_RXMAXP = 19, S_RXCSR1 = 20, S_RXCSR2 = 21,
       S_COUNT0 = 22, S_RXCOUNT1 = 22, S_RXCOUNT2 = 23 };

static uint8_t ep0buf[64 + 4] __attribute__((aligned(4)));
static uint8_t ep1tx[64] __attribute__((aligned(4)));
static uint8_t ep1rx[64 + 4] __attribute__((aligned(4)));
#if FELUCCA_CDC
static uint8_t ep2tx[8] __attribute__((aligned(4)));
static uint8_t ep3tx[64] __attribute__((aligned(4)));
static uint8_t ep3rx[64 + 4] __attribute__((aligned(4)));
/* serial rings: in = host -> console (ISR writes), out = console -> host (ISR reads) */
#define CI_N 256u
#define CO_N 2048u
static uint8_t cdc_in[CI_N], cdc_out[CO_N];
static volatile uint32_t ci_w, ci_r, co_w, co_r;
static struct {
    uint8_t line[7];             /* line coding: just stored and echoed back */
    volatile uint8_t dtr;        /* host has the port open */
    uint8_t e0_rx;               /* SET_LINE_CODING data stage pending */
    uint8_t rx_pend;             /* EP3 OUT packet seen, not yet taken (the ring was full) */
    uint32_t rx_pkts, tx_pkts;
} cdc = {{0x00, 0xC2, 0x01, 0x00, 0, 0, 8}};   /* 115200 8N1 */
#endif

#if FELUCCA_UAC
/* USB audio input. The I2S clock is not locked to USB (about 44,117.6 Hz against the host's 44,100):
 * the endpoint is asynchronous, so each 1 ms packet carries what the ring holds, 44 or 45 frames
 * (the 44.1 pattern) and one more or one fewer while the ring's fill drifts out of its band.
 * The render writes HALF_FRAMES (128) frames every 2.9 ms; the fill measured when a render starts is
 * the low point of that saw tooth, and is kept in UA_LO..UA_HI. A frame waits in the ring for that fill
 * plus its place in the render: 80..256 frames, ~3.8 ms on average (256-frame halves and a 96..160
 * band: ~5.8 ms). Measured under load (65 % CPU): a 64..112 band started renders at
 * 48..119 with no glitch; one render was once seen 29 frames under the floor: a packet (46) must
 * still be in hand then. */
#define UA_N 512u                /* ring, frames (16-bit L | R << 16); a power of two */
/* (0.8.9) the band 0.7 ms higher: a render now catches up at once after a late one (audio.c), so the next one may
 * start up to ~1 ms late (a render of 131 %) and the ring must hold a packet beyond that (was 80..128, prime 104) */
#define UA_PRIME 136u            /* silence ahead of the first rendered frame */
#define UA_LO 112u               /* a render may start late: keep a packet and more in hand */
#define UA_HI 160u
/* (0.9.8) STEMS / STEMS+IN: the band 1.1 ms higher still (a repeated frame is a click in a soloed stem; a few renders
 * late in a row emptied the ring now and then); the app measures the latency it lines the voice up by in each take */
#define UA_PAD_STEMS 48u
#define UAC_RATE 44100u
#if FELUCCA_UAC_48K
/* 48 kHz: the same times in 48 kHz frames (x 160 / 147); a render of HALF_FRAMES gives 139..140 frames.
 * (0.8.9) With CORES 2 the half is resampled on core 1 after the render (uac_render_end) and enters the ring when the
 * next half starts: the fill measured then (before it) is the low point, the prime one half (UA_REND48) longer */
#define UA_PRIME48 152u
#define UA_LO48 128u
#define UA_HI48 176u
#define UA_REND48 ((HALF_FRAMES * 160u + 146u) / 147u)
#define UA_MAXF 49u              /* frames in the largest packet: 48 + 1 */
_Static_assert(UA_PRIME48 + 2u * UA_REND48 + UA_MAXF < UA_N, "the ring holds the band, two renders and a packet (48 kHz)");
#else
#define UA_MAXF 46u              /* frames in the largest packet */
#endif
#define UAC_MAXP (UA_MAXF * 4u)
_Static_assert(UA_HI + HALF_FRAMES + UA_MAXF < UA_N, "the ring holds the band, a render and a packet");
#define UA_W 5u                  /* (0.7.1) words a frame in the ring: 1 stereo (L | R << 16), 4 the stems (8 x 16 bit);
                                  * (0.8.0) 5 the stems with the host's audio in (10 x 16 bit: STEMS+IN) */
#define UA_NF10 409u             /* (0.8.0) STEMS+IN: the ring's frames, the 8-channel ring's words / 5 (the pool kept) */
_Static_assert(UA_HI + UA_PAD_STEMS + HALF_FRAMES + UA_MAXF < UA_NF10 && UA_NF10 * 5u <= UA_N * 4u, "the 10-channel ring");
static uint32_t ua_ring[UA_N * 4u] __attribute__((section(".pool")));   /* (8 KiB: in the pool, zeroed at boot) */
#ifndef UAC_STEMS
#define UAC_STEMS() 0            /* (a build without fx.c: the host tests of USB alone) */
#endif
#define UA_FW (UAC_STEMS() ? 3u + UAC_STEMS() : 1u) /* this mode's words a frame (UAC_STEMS: 1 the 8, 2 the 10) */
#define UA_NF (UA_FW == 5u ? UA_NF10 : UA_N)        /* this mode's frames in the ring */
/* a frame's slot in the ring: a power of two masked; STEMS+IN's 409 a modulo by a constant (a multiply, no divide;
 * the free-running index wraps every 2^32 frames, 27 hours of streaming: one glitch then) */
static inline uint32_t ua_slot(uint32_t f) { return UA_FW == 5u ? f % UA_NF10 : f & (UA_N - 1u); }
static volatile uint32_t ua_w, ua_r;
static uint32_t ep4tx[UA_MAXF * UA_W] __attribute__((aligned(4)));
static struct {
    volatile uint8_t alt;        /* streaming interface setting: 1 = the host records */
    volatile uint8_t r48;        /* the host's rate (SET_CUR): 1 = 48 kHz, 0 = 44.1 (bus reset) */
    uint8_t ring48;              /* the rate the ring was primed for (the render's; a change re-primes) */
    volatile uint8_t go;         /* the ring is primed: the consumer reads it (0: silence packets) */
    volatile uint8_t flowing;    /* the host takes our packets (it may set alt 1 well before it reads) */
    uint8_t queued;              /* a packet was queued since the stream (re)started */
    uint8_t feed;                /* this render feeds the ring (sampled when it starts) */
    uint8_t wait;                /* polls the queued packet has waited */
    uint8_t e0_rx;               /* SET_CUR sampling frequency: data stage pending */
    volatile uint32_t fill_min;  /* ring fill when the last render started */
    uint32_t acc;                /* 44.1 frames per ms: 0.1 frame steps */
    uint32_t last[UA_W];         /* the last frame sent, repeated on an underrun (0.7.1: up to 4 words) */
    uint32_t starts, pkts, frames;               /* frames in packets sent from the ring */
    uint32_t underruns, overruns, missed;        /* glitches: ring empty (repeat), ring full (drop), no packet ready */
    uint32_t stalls;                             /* the host stopped reading for > 20 ms (alt still 1) */
    uint32_t adj_up, adj_down;                   /* packets one frame longer / shorter than the pattern */
    uint32_t owe, repaid;                        /* (0.8.9) frames an underrun repeated, not yet made up; made up */
    uint32_t fill_lo, fill_hi;                   /* range of fill_min since the stream started */
    uint32_t rate_sets;                          /* SET_CUR sampling frequency requests */
} uac;
/* (0.7.5) MENU > SYSTEM > USB IN+OUT: the FM-1 is the host's audio output too (UAC1, 2 ch 16 bit 44.1 kHz, EP4 OUT
 * isochronous adaptive): TIMER5 takes each 1 ms packet into ui_ring (uin_rx), the audio ISR reads it at its own clock
 * (uac_in_fill, a linear interpolation whose step follows the ring's fill: the host's 44,100 Hz against the I2S's
 * ~44,117.6) into fx.c's uin_l / uin_r, which the last live track mixes in (its LEVEL, PAN, sends, MUTE) */
#define UI_N 512u                /* ring, frames (L | R << 16); a power of two */
#define UI_TARGET 160u           /* the fill the step steers to: ~3.6 ms (a render's 128 and a packet's 45 in hand) */
#define UI_MAXF 46u              /* frames in the largest packet a host sends at 44.1 kHz (45, one spare) */
#define UI_STEP_MAX 650          /* the step's range: +-1 % of 65536 (a host clock far off: the ring re-primes) */
static uint32_t ui_ring[UI_N] __attribute__((section(".pool")));   /* (2 KiB) */
static volatile uint32_t ui_w, ui_r;
#define UI_MAXF_RX 50u           /* (V10.57) frames a packet taken at most: a 48 kHz interface on the host port sends 48 + 1 */
static uint32_t ep4rx[UI_MAXF_RX + 2u] __attribute__((aligned(4)));
static volatile uint8_t usb_duplex;                      /* the device presents the audio output too (usb_duplex_switch) */
static volatile uint8_t uh_aud_on;                       /* (V10.53) USB > HOST: an audio interface's input plays into the
                                                         * ring, as IN+OUT's host does (usb_host.c uh_poll) */
static uint8_t uin_r48;                                  /* (V10.57) the host port's interface plays at 48 kHz: the reader's
                                                         * step from 48,000 / 44,100 (usb_host.c uh_probe) */
static volatile uint8_t uin_feed;                        /* (V10.53) usb_duplex | uh_aud_on, set where either changes: the
                                                         * audio ISR tests one byte (its cost as before) */
static struct {
    volatile uint8_t alt;        /* the output's streaming setting: 1 = the host plays to us */
    uint8_t go;                  /* the reader runs (primed: UI_TARGET frames were in hand) */
    uint8_t e0_rx;               /* SET_CUR to EP4 OUT: its data stage pending (taken, ignored: one rate) */
    volatile uint8_t gen;        /* stream starts and stops (TIMER5); the reader drops the ring on a new one */
    uint8_t gen_seen;            /* (the reader's: only it moves ui_r) */
    uint32_t ph;                 /* the reader's phase between ring frames (Q16) */
    int32_t step;                /* frames a sample, Q16 (65536: the host's rate is ours) */
    int32_t fs;                  /* the fill, smoothed (Q8 frames) */
    int32_t g;                   /* the fade in / out, Q15 */
    int32_t last_l, last_r;      /* the last frame read (an underrun fades from it) */
    uint32_t pkts, frames, overruns, underruns, starts, bad;   /* bad: a packet not of whole frames / too long */
} uin;
#define USB_DUPLEX usb_duplex
#if FELUCCA_UAC_48K
#include "uac_fir.h"
/* the resampler's input history: the last 32 frames, written twice so a window of UAC_FIR_N never wraps */
#define RS_H 32u
_Static_assert(RS_H >= UAC_FIR_N && (RS_H & (RS_H - 1u)) == 0 && UAC_FIR_N % 4u == 0u, "the history holds a window");
static struct {
    int16_t l[2u * RS_H], r[2u * RS_H];   /* r right after l (the FIR loop reads both from one pointer) */
    uint32_t w;                  /* input frames written */
    uint32_t ph;                 /* the next output's phase, 0..159 (its newest input frame: the next written) */
#if FELUCCA_UAC_TONE
    uint32_t tone;
#endif
} rs;
/* (0.8.9) all 160 phases in RAM (the pool, built from the 80 in the flash the first time the ring primes at 48 kHz):
 * one forward loop, no flash reads (the table no longer pushes the render's code out of the cache), and core 1 can
 * run it outside the audio ISR, with the flash off */
static int32_t rs_tab[UAC_FIR_L][UAC_FIR_N] __attribute__((section(".pool")));   /* (15 KiB) */
static uint32_t rs_in[HALF_FRAMES] __attribute__((section(".pool")));            /* a half (CORES 2) or a block, 16-bit L | R */
static struct {
    uint8_t c1;                  /* this stream resamples on core 1, a half at a time (chosen when it primes) */
    volatile uint8_t busy;       /* a half handed to core 1, not yet in the ring */
    uint32_t nin;                /* frames of this half in rs_in */
    uint32_t n, w0, job;         /* the half handed over: its frames, its place in the ring, core 1's job number */
    volatile uint32_t k;         /* core 1's: the frames it wrote */
} rq;
#if !defined(UAC_C1_LIVE) && FELUCCA_DUAL_CORE
/* core 1 (hal/fm1_multicore.h): one job at a time; mix_block's first job of the next half queues behind this one and
 * its wait covers it */
#define UAC_C1_LIVE() (fm1_mc_on && !mc_off)
#define UAC_C1_RUN(f) fm1_mc_run(f, 0)
#define UAC_C1_JOB() fm1_mc_box.job
#define UAC_C1_DONE(j) fm1_mc_done(j)
#define UAC_C1_WAIT() fm1_mc_wait()
#define UAC_C1_TEXT __attribute__((section(".c1_text"), noinline, used))
#endif
#ifndef UAC_C1_LIVE              /* one core (host builds, FELUCCA_DUAL_CORE=0): every block resampled at once */
#define UAC_C1_LIVE() 0
#define UAC_C1_RUN(f) 0
#define UAC_C1_JOB() 0u
#define UAC_C1_DONE(j) 1
#define UAC_C1_WAIT() 0
#endif
#ifndef UAC_C1_TEXT
#define UAC_C1_TEXT __attribute__((noinline))
#endif
#endif
#endif

static struct {
    uint8_t up, config, pend_addr, has_pend_addr, e0_tx, e0_zlp;
    const uint8_t *e0_src;
    uint16_t e0_left;
    uint32_t resets, setups, rx_pkts, tx_pkts, sof_seen, timeouts, no_sof;
    uint32_t suspends, max_gap, retries, frame_stalls;   /* diagnostics: suspend events, longest run of polls without SOF */
    uint16_t frame, frame_same;  /* last SOF frame number and how long it has not moved (x 10 ms) */
    uint8_t detached;            /* usb_detach() was called: no retry */
    volatile uint8_t suspended;  /* bus idle > 3 ms (unplugged or host asleep): the UI hides "USB" */
    uint8_t last_setup[8];
    uint8_t sysex[16];
    uint8_t sx_len, sx_on;
    uint8_t rx_pend;             /* EP1 OUT packet seen, not yet taken (the MIDI ring was too full) */
    volatile uint8_t uboot_req;
    volatile uint8_t ota_req;    /* F0 22 24 35 7F F7: M-UPGRADE upgrade command (FELUCCA_OTA) */
    uint8_t replug;              /* (0.9.10) a mode switch dropped off the bus: usb_retry waits USB_REPLUG_MS */
} usb;

#if FELUCCA_OTA
/* M-UPGRADE SysEx (ota.c): one received frame at a time (7-bit bytes between
 * F0 and F7; frames arriving while one is pending are dropped, the host
 * retries), and a TX ring of SysEx event packets sent before any MIDI. */
static uint8_t sx_frame[640];
static uint32_t sx_pos;
static volatile uint32_t sx_frame_len;
static volatile uint8_t sx_ready, sx_collect, sx_busy;
#define SXQ 64u
static uint32_t sx_out_q[SXQ];
static volatile uint32_t so_w, so_r;
#endif

/* MIDI rings: 4-byte USB-MIDI event packets */
#define MQ 64u
static uint32_t midi_in_q[MQ], midi_out_q[MQ];
static uint32_t midi_in_ms[MQ];
#ifndef fm1_ticks
static uint32_t fm1_ticks(void);                     /* (hal/fm1_time.h; the host tests' own) */
#endif
static uint32_t midi_out_due[MQ];                     /* (V10.33) the TIMER4 tick a packet goes at (0: at once): the clock's
                                                         * pulses at their own time, not the render's (ep1_tx holds them) */
static uint8_t midi_in_source[MQ];                    /* 1 USB, 2 TRS */
static volatile uint32_t fm1_ms;                    /* also declared by core.h; parser-only host tests */
static volatile uint32_t mi_w, mi_r, mo_w, mo_r;
static volatile uint8_t midi_in_overflow;               /* audio ISR discards a broken stream and releases notes */

static int midi_enqueue(uint32_t pkt, uint32_t source)
{
    uint32_t at = mi_w % MQ;
    if (midi_in_overflow)
        return 0;
    if (mi_w - mi_r >= MQ) {
        midi_in_overflow = 1;
        return 0;
    }
    midi_in_q[at] = pkt;
    midi_in_ms[at] = fm1_ms;
    midi_in_source[at] = (uint8_t)source;
    RING_PUBLISH();
    mi_w++;
    return 1;
}

#if FELUCCA_BT
#include "../bt/bt_midi.h"
struct bt_oq bt_oq;                                     /* (V10.13) MIDI OUT over Bluetooth too (bt_seg.c takes it) */
#endif
static void midi_out_event_at(uint32_t pkt, uint32_t due)   /* from the audio ISR; due: fm1_ticks() it goes at, 0 now */
{
    if (usb.config && mo_w - mo_r < MQ) {
        midi_out_q[mo_w % MQ] = pkt;
        midi_out_due[mo_w % MQ] = due;
        RING_PUBLISH();
        mo_w++;
    }
#if FELUCCA_BT
    bt_oq_put_at(&bt_oq, pkt, due);                     /* (V10.13: while a central listens; else nothing) */
#endif
}
static void midi_out_event(uint32_t pkt) { midi_out_event_at(pkt, 0); }

/* ------------------------------------------------------- descriptors --- */
/* Two functions: audio + MIDI (0 audio control, 1 MIDI streaming; with FELUCCA_UAC 1 audio streaming, 2 MIDI: V10.7.12)
 * and, with FELUCCA_CDC, the CDC-ACM console (communication + data). The endpoints never move:
 * EP1 MIDI, EP2 CDC notify, EP3 CDC data, EP4 audio.
 * bcdDevice: 3.00 MIDI (4.xx with FELUCCA_UAC since V10.7.12, MIDI last), +0.10 the audio input (+0.20 since 1.1: 44.1 and 48 kHz, FELUCCA_UAC_48K), +0.01 CDC
 * (+0.02 per FELUCCA_USB_LAYOUT step, so a host never reuses what it learnt about another layout).
 * FELUCCA_USB_LAYOUT, how the CDC build presents itself (#67: macOS 13-15 attach Apple's CDC composite
 * driver to a misc / IAD device that has a CDC function, and their kernel audio driver then never
 * takes the audio interfaces; macOS 26 / 27 (usbaudiod, matched per interface) and iOS / iPadOS work):
 *   0  1.0: device class EF 02 01 (misc / IAD), audio + MIDI IF 0.., CDC last         (the default)
 *   1  CDC first: CDC IF 0-1, then audio + MIDI, both with IADs, EF 02 01
 *   2  as 0 with device class 00 00 01: outside AppleUSBCDC's device personalities
 *      (02 * *, EF 02 01, 00 00 00); the IADs stay for Windows / Linux (test build)
 *   3  as 0 with device class 00 00 00 (test build: does AppleUSBCDC's "Vendor" personality take it?)
 * usb_cdc_on (FELUCCA_CDC_DEFAULT at boot; usb_cdc_switch re-enumerates): 0 presents the device without
 * the console, byte for byte as a FELUCCA_CDC=0 build: device class 0, audio + MIDI, no IAD. */
#ifndef FELUCCA_USB_LAYOUT
#define FELUCCA_USB_LAYOUT 0
#endif
#ifndef FELUCCA_CDC_DEFAULT
#define FELUCCA_CDC_DEFAULT 1
#endif
/* (0.9.12) iProduct's string index. Windows' USB audio driver keeps a MIDI port's name in the registry
 * (MediaCategories\{GUID}\Name) under a GUID made of VID, PID and this index only (ksmedia.h
 * INIT_USBAUDIO_PRODUCT_NAME), written the first time it sees the device and never again: every FM-1 that had
 * Felucca (1209:0001, index 2) kept showing "Felucca" in Live / Windows after JIANT's rename. A new index is a new
 * GUID, so the host reads the name again. Index 2 is no longer answered. Rename the product again: move it again.
 * (2026-10-30) "FM-1" (was "JIANT", index 4): the apps made for the FM-1 (FM1 Swap, Firmware Hub, ...) look for its name,
 * as Melodee's "Melodee FM-1"; on index 5, so Windows reads it afresh. */
#define USB_STR_PRODUCT 5u
#ifndef FELUCCA_USB_PID
#define FELUCCA_USB_PID 0x0001   /* the update loader is 0x0002 */
#endif
#if FELUCCA_USB_LAYOUT < 0 || FELUCCA_USB_LAYOUT > 3
#error "FELUCCA_USB_LAYOUT is 0..3"
#endif

#define D_CFG(len, nif) 9, 2, (len) & 0xFF, (len) >> 8, (nif), 1, 0, 0x80, 50
#define D_IAD(first, n, cls, sub, proto) 8, 0x0B, (first), (n), (cls), (sub), (proto), 0
#if FELUCCA_UAC_48K
#define D_FMT 14, 0x24, 2, 1, 2, 2, 16, 2, 0x44, 0xAC, 0x00, 0x80, 0xBB, 0x00,   /* 44100, 48000 */
#define FMT_LEN 14
#elif FELUCCA_UAC
#define D_FMT 11, 0x24, 2, 1, 2, 2, 16, 1, UAC_RATE & 0xFF, (UAC_RATE >> 8) & 0xFF, UAC_RATE >> 16,
#define FMT_LEN 11
#else
#define FMT_LEN 0
#endif
#if FELUCCA_UAC
#define D_AC_CH(a, ch, cfg)                                                                          \
    9, 4, (a), 0, 0, 1, 1, 0, 0,                                                                     \
    10, 0x24, 1, 0x00, 0x01, 31, 0, 2, (a) + 1, (a) + 2,   /* AC header 1.00: audio + MIDI streaming */ \
    12, 0x24, 2, 1, 0x03, 0x06, 0, (ch), (cfg), 0x00, 0, 0, /* input terminal 1: line, ch channels */    \
    9, 0x24, 3, 2, 0x01, 0x01, 0, 1, 0,                    /* output terminal 2: USB streaming, from 1 */
#define D_AC(a) D_AC_CH(a, 2, 0x03)                            /* 2 ch (L R) */
#define D_AS_FMT(s, fmt, maxp)                                                                       \
    9, 4, (s), 0, 0, 1, 2, 0, 0,                           /* audio streaming, alt 0: no bandwidth */    \
    9, 4, (s), 1, 1, 1, 2, 0, 0,                           /* alt 1: the stream */                      \
    7, 0x24, 1, 2, 1, 0x01, 0x00,                          /* AS general: terminal 2, delay 1, PCM */   \
    fmt                                                    /* type I: the channels, 16 bit, the rates */ \
    9, 5, 0x84, 0x05, (maxp) & 0xFF, (maxp) >> 8, 1, 0, 0,   /* EP4 IN isochronous async, 1 ms */       \
    7, 0x25, 1, 0x01, 0, 0, 0,                             /* CS endpoint: sampling frequency control */
#define D_AS(s) D_AS_FMT(s, D_FMT, UAC_MAXP)
/* (0.7.1) MENU > USB STEMS: 8 channels (the six parts mono, the FX return L R), 44.1 kHz only, 46 frames a packet at
 * most: 736 bytes. (0.9.8) It said 45 (720 bytes) while uac_packet sends 46 whenever it holds the ring's fill (+1 on a
 * 45-frame millisecond): the host took each such packet as babble (the FM-1's clock is fast, so +1 comes often), dropped
 * it and stalled the endpoint, the ring filled and whole blocks were dropped, ~100 times a second: stems full of clicks */
#define D_FMT8 11, 0x24, 2, 1, 8, 2, 16, 1, 0x44, 0xAC, 0x00,
#define FMT_LEN8 11
#define UAC_MAXP8 (46u * 16u)
#define D_AC8(a) D_AC_CH(a, 8, 0x00)
#define D_AS8(s) D_AS_FMT(s, D_FMT8, UAC_MAXP8)
/* (0.8.0) MENU > USB STEMS+IN: 10 channels (the stems, then the host's audio as the mix took it and as it came), 46
 * frames a packet at most: 920 bytes, within a full-speed isochronous packet's 1023 */
#define D_FMT10 11, 0x24, 2, 1, 10, 2, 16, 1, 0x44, 0xAC, 0x00,
#define UAC_MAXP10 (46u * 20u)
#define D_AS10(s) D_AS_FMT(s, D_FMT10, UAC_MAXP10)
/* (0.9.8) every packet uac_packet can send fits the endpoint's wMaxPacketSize, in every mode: 44.1 kHz's 45 + 1 frames */
_Static_assert(UAC_MAXP8 >= (45u + 1u) * 16u && UAC_MAXP10 >= (45u + 1u) * 20u && UAC_MAXP >= UA_MAXF * 4u &&
               UAC_MAXP8 <= 1023u && UAC_MAXP10 <= 1023u, "the largest packet fits wMaxPacketSize (full speed: 1023)");
/* (0.7.5) MENU > USB IN+OUT: the input as D_AC's, plus the host's output: input terminal 3 (USB streaming, L R) ->
 * output terminal 4 (speaker), and its streaming interface (EP4 OUT isochronous adaptive, 44.1 kHz only) */
#define UIN_MAXP (UI_MAXF * 4u)
#define D_AC_DUP_CH(a, ch, cfg)                                                                      \
    9, 4, (a), 0, 0, 1, 1, 0, 0,                                                                     \
    11, 0x24, 1, 0x00, 0x01, 53, 0, 3, (a) + 1, (a) + 2, (a) + 3,   /* AC header: input, output, MIDI */ \
    12, 0x24, 2, 1, 0x03, 0x06, 0, (ch), (cfg), 0x00, 0, 0, /* input terminal 1: line, L R (or the stems) */ \
    9, 0x24, 3, 2, 0x01, 0x01, 0, 1, 0,                    /* output terminal 2: USB streaming, from 1 */ \
    12, 0x24, 2, 3, 0x01, 0x01, 0, 2, 0x03, 0x00, 0, 0,    /* input terminal 3: USB streaming, L R */   \
    9, 0x24, 3, 4, 0x01, 0x03, 0, 3, 0,                    /* output terminal 4: speaker, from 3 */
#define D_AC_DUP(a) D_AC_DUP_CH(a, 2, 0x03)
#define D_AS_OUT(s)                                                                                  \
    9, 4, (s), 0, 0, 1, 2, 0, 0,                           /* the output's streaming, alt 0 */          \
    9, 4, (s), 1, 1, 1, 2, 0, 0,                           /* alt 1: the host plays */                  \
    7, 0x24, 1, 3, 1, 0x01, 0x00,                          /* AS general: terminal 3, PCM */            \
    11, 0x24, 2, 1, 2, 2, 16, 1, 0x44, 0xAC, 0x00,         /* type I: 2 ch, 16 bit, 44100 */            \
    9, 5, 0x04, 0x09, UIN_MAXP & 0xFF, UIN_MAXP >> 8, 1, 0, 0,   /* EP4 OUT isochronous adaptive, 1 ms */ \
    7, 0x25, 1, 0x00, 0, 0, 0,                             /* CS endpoint: no controls */
#else
#define D_AC(a) 9, 4, (a), 0, 0, 1, 1, 0, 0, 9, 0x24, 1, 0x00, 0x01, 9, 0, 1, (a) + 1,
#define D_AS(s)
#endif
#define D_MIDI(m)                                                                                    \
    9, 4, (m), 0, 2, 1, 3, 0, 0,                                                                     \
    7, 0x24, 1, 0x00, 0x01, 0x41, 0x00,                                                              \
    6, 0x24, 2, 1, 1, 0,                                                                             \
    6, 0x24, 2, 2, 2, 0,                                                                             \
    9, 0x24, 3, 1, 3, 1, 2, 1, 0,                                                                    \
    9, 0x24, 3, 2, 4, 1, 1, 1, 0,                                                                    \
    9, 5, 0x01, 2, 64, 0, 0, 0, 0,                                                                   \
    5, 0x25, 1, 1, 1,                                                                                \
    9, 5, 0x81, 2, 64, 0, 0, 0, 0,                                                                   \
    5, 0x25, 1, 1, 3,
/* (V10.7.12) the MIDI streaming interface last (Felucca's felucca-bsp cd76d43, Skiff 2.2, Skiff #59 / PR #60): macOS 13-15's
 * AppleUSBAudio (AUAConfigurationDictionary::parseConfigurationDescriptor) shows no USB audio device while the MIDI
 * interface sits between the audio control and the audio streaming ones (MIDI still works); Linux and Windows take any
 * order. 0 audio control, 1 audio streaming (2 the output's, IN+OUT / STEMS+IN), then MIDI; the AC header lists them so,
 * MIDI last (it stays listed: Linux's snd-usb-audio makes a MIDI port only for a listed one) */
#if FELUCCA_UAC
#define D_AUDIO(a) D_AC(a) D_AS((a) + 1) D_MIDI((a) + 2)   /* audio + MIDI from interface a */
#else
#define D_AUDIO(a) D_AC(a) D_MIDI((a) + 1)
#endif
#define AUD_NIF (2 + FELUCCA_UAC)
#define AUD_LEN (92 + (63 + FMT_LEN) * FELUCCA_UAC)
#if FELUCCA_UAC
#define D_AUDIO8(a) D_AC8(a) D_AS8((a) + 1) D_MIDI((a) + 2)   /* (0.7.1) the stems: 8 channels */
#define AUD_LEN8 (92 + 63 + FMT_LEN8)
#define D_AUDIO_DUP(a) D_AC_DUP(a) D_AS((a) + 1) D_AS_OUT((a) + 2) D_MIDI((a) + 3)   /* (0.7.5) in + out */
#define AUD_LEN_DUP (92 + 63 + FMT_LEN + 22 + 52)
#define D_AUDIO_STIN(a) D_AC_DUP_CH(a, 10, 0x00) D_AS10((a) + 1) D_AS_OUT((a) + 2) D_MIDI((a) + 3)   /* (0.8.0) */
#define AUD_LEN_STIN (92 + 63 + 11 + 22 + 52)
#endif
#define D_CDC(c)                                                                                     \
    D_IAD((c), 2, 2, 2, 1),                                /* IAD: CDC ACM */                           \
    9, 4, (c), 0, 1, 2, 2, 1, 0,                           /* communication */                          \
    5, 0x24, 0x00, 0x10, 0x01,                             /* header 1.10 */                            \
    5, 0x24, 0x01, 0x00, (c) + 1,                          /* call management: the data IF */           \
    4, 0x24, 0x02, 0x02,                                   /* ACM: line coding + state */               \
    5, 0x24, 0x06, (c), (c) + 1,                           /* union */                                  \
    7, 5, 0x82, 3, 8, 0, 16,                               /* EP2 IN interrupt (never sent) */          \
    9, 4, (c) + 1, 0, 2, 0x0A, 0, 0, 0,                    /* data */                                   \
    7, 5, 0x03, 2, 64, 0, 0,                               /* EP3 OUT bulk */                           \
    7, 5, 0x83, 2, 64, 0, 0,                               /* EP3 IN bulk */
#define CDC_LEN 66                                         /* with its IAD */
#define BCD_LO(cdc) (0x10 * (FELUCCA_UAC + FELUCCA_UAC_48K) + ((cdc) ? 0x01 + 2 * FELUCCA_USB_LAYOUT : 0))
#define D_DEV_LO(bcdusb, cls, sub, proto, lo)                                                        \
    18, 1, (bcdusb) & 0xFF, (bcdusb) >> 8, (cls), (sub), (proto), 64, 0x09, 0x12, FELUCCA_USB_PID & 0xFF, \
    FELUCCA_USB_PID >> 8, (lo), 0x03 + FELUCCA_UAC, 1, USB_STR_PRODUCT, 0, 1
#define D_DEV(bcdusb, cls, sub, proto, cdc) D_DEV_LO(bcdusb, cls, sub, proto, BCD_LO(cdc))

/* without the console (FELUCCA_CDC=0, or usb_cdc_on = 0): class 0, no IAD, as a plain USB-MIDI / audio device */
#define CFG_LEN_PLAIN (9 + AUD_LEN)
static const uint8_t DEV_DESC_PLAIN[18] = {D_DEV(0x0110, 0, 0, 0, 0)};
static const uint8_t CFG_DESC_PLAIN[] = {D_CFG(CFG_LEN_PLAIN, AUD_NIF), D_AUDIO(0)};
typedef char cfg_plain_len_ok[sizeof CFG_DESC_PLAIN == CFG_LEN_PLAIN ? 1 : -1];
#if FELUCCA_UAC
/* (0.7.1) MENU > USB STEMS: the plain device with the 8-channel audio function (bcdDevice + 0x40: a host never reuses
 * what it learnt about the stereo one). No console with it (usb_stems_switch turns it off) */
#define CFG_LEN_PLAIN8 (9 + AUD_LEN8)
static const uint8_t DEV_DESC_PLAIN8[18] = {D_DEV_LO(0x0110, 0, 0, 0, BCD_LO(0) + 0x40)};
static const uint8_t CFG_DESC_PLAIN8[] = {D_CFG(CFG_LEN_PLAIN8, AUD_NIF), D_AUDIO8(0)};
typedef char cfg_plain8_len_ok[sizeof CFG_DESC_PLAIN8 == CFG_LEN_PLAIN8 ? 1 : -1];
static volatile uint8_t usb_stems;                         /* the device presents the 8 channels (fx.c fx_stems feeds them) */
#define USB_STEMS usb_stems
/* (0.7.5) MENU > USB IN+OUT: the plain device with the audio output as well (bcdDevice + 0x80), no console */
#define CFG_LEN_DUP (9 + AUD_LEN_DUP)
static const uint8_t DEV_DESC_DUP[18] = {D_DEV_LO(0x0110, 0, 0, 0, BCD_LO(0) + 0x80)};
static const uint8_t CFG_DESC_DUP[] = {D_CFG(CFG_LEN_DUP, AUD_NIF + 1), D_AUDIO_DUP(0)};
typedef char cfg_dup_len_ok[sizeof CFG_DESC_DUP == CFG_LEN_DUP ? 1 : -1];
/* (0.8.0) MENU > USB STEMS+IN: the stems (10 channels) and the host's audio output, both flags set (bcdDevice + 0xC0) */
#define CFG_LEN_STIN (9 + AUD_LEN_STIN)
static const uint8_t DEV_DESC_STIN[18] = {D_DEV_LO(0x0110, 0, 0, 0, BCD_LO(0) + 0xC0)};
static const uint8_t CFG_DESC_STIN[] = {D_CFG(CFG_LEN_STIN, AUD_NIF + 1), D_AUDIO_STIN(0)};
typedef char cfg_stin_len_ok[sizeof CFG_DESC_STIN == CFG_LEN_STIN ? 1 : -1];
#else
#define USB_STEMS 0
#define USB_DUPLEX 0
#endif
#if FELUCCA_CDC
#define CFG_LEN (9 + 8 + AUD_LEN + CDC_LEN)
#define USB_NIF (AUD_NIF + 2)
#if FELUCCA_USB_LAYOUT == 1
#define AUD_IF_CDC 2u                                      /* the audio function's first interface */
#define CDC_IF_CDC 0u
#else
#define AUD_IF_CDC 0u
#define CDC_IF_CDC AUD_NIF
#endif
#if FELUCCA_USB_LAYOUT == 2
static const uint8_t DEV_DESC[18] = {D_DEV(0x0200, 0x00, 0x00, 0x01, 1)};
#elif FELUCCA_USB_LAYOUT == 3
static const uint8_t DEV_DESC[18] = {D_DEV(0x0200, 0x00, 0x00, 0x00, 1)};
#else
static const uint8_t DEV_DESC[18] = {D_DEV(0x0200, 0xEF, 0x02, 0x01, 1)};   /* misc / IAD */
#endif
static const uint8_t CFG_DESC[] = {
    D_CFG(CFG_LEN, USB_NIF),
#if FELUCCA_USB_LAYOUT == 1
    D_CDC(CDC_IF_CDC)
#endif
    D_IAD(AUD_IF_CDC, AUD_NIF, 1, 1, 0),                  /* IAD: audio + MIDI */
    D_AUDIO(AUD_IF_CDC)
#if FELUCCA_USB_LAYOUT != 1
    D_CDC(CDC_IF_CDC)
#endif
};
typedef char cfg_len_ok[sizeof CFG_DESC == CFG_LEN ? 1 : -1];
static uint8_t usb_cdc_on = FELUCCA_CDC_DEFAULT != 0;      /* the console is presented (read by the TIMER5 ISR) */
#define USB_CDC_ON usb_cdc_on
#define AUD_IF (usb_cdc_on ? AUD_IF_CDC : 0u)
#else
#define DEV_DESC DEV_DESC_PLAIN
#define CFG_DESC CFG_DESC_PLAIN
#define USB_CDC_ON 0
#define AUD_IF 0u
#endif
#define UAC_AS_IF (AUD_IF + 1u)                           /* (V10.7.12: MIDI last) */
static const uint8_t STR0[4] = {4, 3, 0x09, 0x04};
static const uint8_t STR1[] = {22, 3, 'J', 0, 'u', 0, 'a', 0, 'n', 0, ' ', 0, 'J', 0, 'i', 0, 'a', 0, 'n', 0, 't', 0};   /* (0.7.0) the names JIANT's; the editor and the installer match /jiant|felucca/ */
/* the product (USB_STR_PRODUCT): Windows names the MIDI port after it, macOS the device and its port; no interface
 * or jack has a name of its own (iInterface / iJack 0). 0.7.0..0.9.11: "JIANT FM" (index 2); Felucca: "Felucca" */
#ifdef FELUCCA_LOADER
static const uint8_t STR_PROD[] = {24, 3, 'F', 0, 'M', 0, '-', 0, '1', 0, ' ', 0, 'U', 0, 'p', 0, 'd', 0, 'a', 0, 't', 0, 'e', 0};
#else
static const uint8_t STR_PROD[] = {10, 3, 'F', 0, 'M', 0, '-', 0, '1', 0};
#endif

static int get_desc(uint32_t wvalue, const uint8_t **d, uint16_t *l)
{
    switch (wvalue >> 8) {
    case 1:
#if FELUCCA_UAC
        if (USB_STEMS && USB_DUPLEX) { *d = DEV_DESC_STIN; *l = 18; return 1; }
        if (USB_STEMS) { *d = DEV_DESC_PLAIN8; *l = 18; return 1; }
        if (USB_DUPLEX) { *d = DEV_DESC_DUP; *l = 18; return 1; }
#endif
        *d = USB_CDC_ON ? DEV_DESC : DEV_DESC_PLAIN;
        *l = 18;
        return 1;
    case 2:
#if FELUCCA_UAC
        if (USB_STEMS && USB_DUPLEX) { *d = CFG_DESC_STIN; *l = (uint16_t)sizeof CFG_DESC_STIN; return 1; }
        if (USB_STEMS) { *d = CFG_DESC_PLAIN8; *l = (uint16_t)sizeof CFG_DESC_PLAIN8; return 1; }
        if (USB_DUPLEX) { *d = CFG_DESC_DUP; *l = (uint16_t)sizeof CFG_DESC_DUP; return 1; }
#endif
        *d = USB_CDC_ON ? CFG_DESC : CFG_DESC_PLAIN;
        *l = (uint16_t)(USB_CDC_ON ? sizeof CFG_DESC : sizeof CFG_DESC_PLAIN);
        return 1;
    case 3:
        switch (wvalue & 0xFFu) {
        case 0:
            *d = STR0;
            *l = sizeof STR0;
            return 1;
        case 1:
            *d = STR1;
            *l = sizeof STR1;
            return 1;
        case USB_STR_PRODUCT:
            *d = STR_PROD;
            *l = sizeof STR_PROD;
            return 1;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

/* ---------------------------------------------------------- the SIE --- */
static void sie_wr(uint32_t r, uint32_t v)
{
    uint32_t n = 20000;
    if (!fm1_usb_sie_on())
        return;
    fm1_usb_sie_wr_start(r, v);
    while (!fm1_usb_sie_done() && --n)
        ;
    if (n)
        usb.timeouts = 0;                               /* consecutive failures only */
    else if (++usb.timeouts > 50u)
        usb.up = 0;                                     /* SIE dead (no clock?): stop polling it */
}

static uint32_t sie_rd(uint32_t r)
{
    uint32_t n = 20000;
    if (!fm1_usb_sie_on())
        return 0;
    fm1_usb_sie_rd_start(r);
    while (!fm1_usb_sie_done())
        if (!--n) {
            if (++usb.timeouts > 50u)
                usb.up = 0;
            return 0;
        }
    usb.timeouts = 0;
    return fm1_usb_sie_data();
}

#if FELUCCA_UAC
static void uac_ep4_reset(void)                         /* drop a queued packet; isochronous mode */
{
    sie_wr(S_INDEX, 4);
    sie_wr(S_TXCSR1, 0x48);                             /* ClrDataTog + FlushFIFO (SDK usb_g_ep_config) */
    sie_wr(S_TXCSR2, 0);
    sie_wr(S_TXCSR1, 0);
    sie_wr(S_TXCSR2, 0x40);                             /* ISO */
}

static void uac_stream(uint32_t alt)                    /* SET_INTERFACE, configuration, bus reset */
{
    uac.go = 0;                                         /* the next render primes the ring again */
    uac.flowing = 0;                                    /* ... once the host has taken a packet */
    uac.queued = 0;
    uac.wait = 0;
    RING_PUBLISH();
    uac.alt = (uint8_t)alt;
    uac.acc = 0;
    memset(uac.last, 0, sizeof uac.last);
    if (alt) {                                          /* (the fill range of the last stream stays readable) */
        uac.starts++;
        uac.fill_lo = 0xFFFFFFFFu;
        uac.fill_hi = 0;
    }
}

/* (0.7.5) MENU > USB IN+OUT: EP4 OUT's buffer, isochronous mode, a fresh toggle (configuration; out of line: the SIE
 * writes' waits are not in the TIMER5 body) */
static __attribute__((noinline)) void uin_ep_config(void)
{
    fm1_usb_ep4_rxbuf(ep4rx);
    sie_wr(S_INDEX, 4);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);
    sie_wr(S_RXCSR2, 0x40);
}
/* (0.7.5) MENU > USB IN+OUT: the host starts (alt 1) or stops (alt 0) playing to us: the ring empties, the reader
 * waits for UI_TARGET frames again (uac_in_fill) */
static __attribute__((noinline)) void uin_stream(uint32_t alt)   /* (its SIE writes: out of the TIMER5 body) */
{
    uin.alt = 0;
    RING_PUBLISH();
    uin.gen++;                                          /* the reader drops what the ring holds (ui_r is its own) */
    if (alt) {
        uin.starts++;
        sie_wr(S_INDEX, 4);
        sie_wr(S_RXCSR1, 0x90);                         /* ClrDataTog + flush, as EP1 / EP3 (ep1_config) */
        sie_wr(S_RXCSR2, 0x40);                         /* ISO */
    }
    RING_PUBLISH();
    uin.alt = (uint8_t)(alt != 0u);
}

/* SET_CUR sampling frequency (any time: before or after SET_INTERFACE; a bus reset goes back to 44.1).
 * 48 kHz when built with it and asked for (the nearer of the two), else 44.1. A change restarts the
 * stream: silence packets until the next render primes the ring at the new rate. */
static void uac_rate_set(uint32_t hz)
{
    uint8_t r48 = FELUCCA_UAC_48K && !USB_STEMS && hz >= (UAC_RATE + 48000u) / 2u;   /* (0.7.1: the stems at 44.1 only) */
    if (r48 == uac.r48)
        return;
    uac.go = 0;
    RING_PUBLISH();
    uac.r48 = r48;
    uac.acc = 0;
    uac.pkts = uac.frames = 0;                          /* the measured rate (console) starts again */
}
#endif

static void ep1_config(void)
{
    fm1_usb_ep_txbuf(1, ep1tx);
    fm1_usb_ep_rxbuf(1, ep1rx);
    sie_wr(S_INDEX, 1);
    sie_wr(S_TXMAXP, 0xFF);
    sie_wr(S_TXCSR1, 0x48);
    sie_wr(S_TXCSR2, 0);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);
    sie_wr(S_RXCSR2, 0);
    sie_wr(S_INTRRX1E, 0x02);
    fm1_usb_ep_enable(1u << 1);
#if FELUCCA_CDC
    if (!usb_cdc_on)
        goto no_cdc;                                    /* presented without the console: EP2 / EP3 unused */
    fm1_usb_ep_txbuf(2, ep2tx);
    sie_wr(S_INDEX, 2);
    sie_wr(S_TXMAXP, 0xFF);
    sie_wr(S_TXCSR1, 0x48);
    sie_wr(S_TXCSR2, 0);
    fm1_usb_ep_txbuf(3, ep3tx);
    fm1_usb_ep_rxbuf(3, ep3rx);
    sie_wr(S_INDEX, 3);
    sie_wr(S_TXMAXP, 0xFF);
    sie_wr(S_TXCSR1, 0x48);
    sie_wr(S_TXCSR2, 0);
    sie_wr(S_RXMAXP, 0xFF);
    sie_wr(S_RXCSR1, 0x90);
    sie_wr(S_RXCSR2, 0);
    sie_wr(S_INTRRX1E, 0x0A);
    fm1_usb_ep_enable((1u << 2) | (1u << 3));
    cdc.rx_pend = 1;                                    /* look once: a packet may already wait */
no_cdc:;
#endif
#if FELUCCA_UAC
    fm1_usb_ep4_txbuf(ep4tx);
    sie_wr(S_INDEX, 4);
    sie_wr(S_TXMAXP, 0xFF);
    uac_ep4_reset();
    if (USB_DUPLEX)                                     /* (0.7.5) EP4 OUT: the host's audio, isochronous */
        uin_ep_config();
    fm1_usb_ep_enable(1u << 4);
    uac_stream(0);
    uin_stream(0);
#endif
}

static void e0_chunk(void)
{
    uint32_t n = usb.e0_left > 64u ? 64u : usb.e0_left, i;
    int last;
    for (i = 0; i < n; i++)
        ep0buf[i] = usb.e0_src[i];
    fm1_usb_ep0_send(ep0buf, n);
    usb.e0_src += n;
    usb.e0_left = (uint16_t)(usb.e0_left - n);
    last = usb.e0_left == 0 && !(n == 64u && usb.e0_zlp);
    if (usb.e0_left == 0 && n == 64u)
        usb.e0_zlp = 0;
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, last ? 0x0A : 0x02);
    if (last)
        usb.e0_tx = 0;
}

static void e0_send(const uint8_t *p, uint16_t len, uint16_t wlen)
{
    if (len > wlen)
        len = wlen;
    usb.e0_src = p;
    usb.e0_left = len;
    usb.e0_zlp = (len < wlen) && (len % 64u == 0);
    usb.e0_tx = 1;
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x40);
    e0_chunk();
}

static void ep0_service(void)
{
    static const uint8_t zero2[2];
    uint32_t csr, i;
    uint16_t wvalue, wlength;
    uint8_t *s = usb.last_setup;
    if (usb.has_pend_addr) {
        sie_wr(S_FADDR, usb.pend_addr);
        usb.has_pend_addr = 0;
    }
    sie_wr(S_INDEX, 0);
    csr = sie_rd(S_CSR0);
    if (csr & 0x04u) {                                  /* SentStall */
        sie_wr(S_CSR0, 0);
        usb.e0_tx = 0;
#if FELUCCA_CDC
        cdc.e0_rx = 0;
#endif
#if FELUCCA_UAC
        uac.e0_rx = 0;
        uin.e0_rx = 0;
#endif
        return;
    }
    if (csr & 0x10u) {                                  /* SetupEnd: the host abandoned the transfer */
        sie_wr(S_CSR0, 0x80);
        usb.e0_tx = 0;
#if FELUCCA_CDC
        cdc.e0_rx = 0;                                  /* else the next SETUP is taken as line coding */
#endif
#if FELUCCA_UAC
        uac.e0_rx = 0;
        uin.e0_rx = 0;
#endif
    }
    if (usb.e0_tx) {
        if (!(csr & 0x02u))
            e0_chunk();
        return;
    }
    if (!(csr & 0x01u))
        return;
    fm1_usb_rx_sync();
#if FELUCCA_CDC
    if (cdc.e0_rx) {                                    /* SET_LINE_CODING data stage */
        uint32_t n = sie_rd(S_COUNT0);
        for (i = 0; i < 7u && i < n; i++)
            cdc.line[i] = ep0buf[i];
        cdc.e0_rx = 0;
        goto ack;
    }
#endif
#if FELUCCA_UAC
    if (uac.e0_rx) {                                    /* SET_CUR sampling frequency: 3 bytes, Hz */
        uac.rate_sets++;
        if (sie_rd(S_COUNT0) >= 3u)
            uac_rate_set(ep0buf[0] | (uint32_t)ep0buf[1] << 8 | (uint32_t)ep0buf[2] << 16);
        uac.e0_rx = 0;
        goto ack;
    }
    if (uin.e0_rx) {                                    /* (0.7.5) SET_CUR to EP4 OUT: one rate, taken and ignored */
        uin.e0_rx = 0;
        goto ack;
    }
#endif
    for (i = 0; i < 8u; i++)
        s[i] = ep0buf[i];
    usb.setups++;
    wvalue = (uint16_t)(s[2] | s[3] << 8);
    wlength = (uint16_t)(s[6] | s[7] << 8);
    switch ((uint32_t)s[0] << 8 | s[1]) {
    case 0x0005:                                        /* SET_ADDRESS */
        usb.pend_addr = s[2] & 0x7Fu;
        usb.has_pend_addr = 1;
        goto ack;
    case 0x8006: {                                      /* GET_DESCRIPTOR */
        const uint8_t *d;
        uint16_t l;
        if (get_desc(wvalue, &d, &l)) {
            if (!wlength)
                goto ack;
            e0_send(d, l, wlength);
            return;
        }
        goto stall;
    }
    case 0x0009:                                        /* SET_CONFIGURATION: 0 or 1 only */
        if (s[2] > 1u)
            goto stall;
        usb.config = s[2];
        if (usb.config == 1u)
            ep1_config();
        else
            sie_wr(S_INTRRX1E, 0);
#if FELUCCA_UAC
        if (usb.config != 1u) {
            uac_stream(0);
            uin_stream(0);
        }
        uac_rate_set(UAC_RATE);                         /* a new configuration starts at 44.1 */
#endif
        goto ack;
    case 0x8008:
        e0_send(&usb.config, 1, wlength);
        return;
    case 0x8000:
    case 0x8100:
    case 0x8200:
        e0_send(zero2, 2, wlength);
        return;
    case 0x010B:                                        /* SET_INTERFACE: alt 0 (alt 1 for the audio stream) */
#if FELUCCA_UAC
        if (s[4] == UAC_AS_IF && wvalue <= 1u && usb.config) {
            uac_ep4_reset();
            uac_stream(wvalue);
            goto ack;
        }
        if (USB_DUPLEX && s[4] == UAC_AS_IF + 1u && wvalue <= 1u && usb.config) {   /* (0.7.5) the output */
            uin_stream(wvalue);
            goto ack;
        }
#endif
        if (wvalue == 0)
            goto ack;
        goto stall;
    case 0x810A:
#if FELUCCA_UAC
        if (s[4] == UAC_AS_IF || (USB_DUPLEX && s[4] == UAC_AS_IF + 1u)) {
            static uint8_t alt;
            alt = s[4] == UAC_AS_IF ? uac.alt : uin.alt;
            e0_send(&alt, 1, wlength);
            return;
        }
#endif
        e0_send(zero2, 1, wlength);
        return;
    case 0x0201: {                                      /* CLEAR_FEATURE(ENDPOINT_HALT): data toggle reset */
        uint32_t ep = s[4] & 0x0Fu, last = USB_CDC_ON ? 3u : 1u;
#if FELUCCA_UAC
        if (wvalue == 0 && (s[4] == 0x84u || (USB_DUPLEX && s[4] == 0x04u)))
            goto ack;                                   /* isochronous: no halt, no toggle */
#endif
        if (wvalue != 0 || ep > last)
            goto stall;                                 /* not an endpoint we have */
        if (ep) {                                       /* (EP0 has no halt to clear) */
            sie_wr(S_INDEX, ep);
            if (s[4] & 0x80u)
                sie_wr(S_TXCSR1, 0x40);
            else
                sie_wr(S_RXCSR1, 0x80);
        }
        goto ack;
    }
#if FELUCCA_CDC
    case 0x2120:                                        /* SET_LINE_CODING: 7 bytes follow */
        if (!usb_cdc_on)
            goto stall;
        if (wlength) {
            cdc.e0_rx = 1;
            sie_wr(S_INDEX, 0);
            sie_wr(S_CSR0, 0x40);                       /* ServicedRxPktRdy, no DataEnd yet */
            return;
        }
        goto ack;
    case 0xA121:                                        /* GET_LINE_CODING */
        if (!usb_cdc_on)
            goto stall;
        e0_send(cdc.line, 7, wlength);
        return;
    case 0x2122:                                        /* SET_CONTROL_LINE_STATE */
        if (!usb_cdc_on)
            goto stall;
        cdc.dtr = s[2] & 1u;
        goto ack;
    case 0x2123:                                        /* SEND_BREAK */
        if (!usb_cdc_on)
            goto stall;
        goto ack;
#endif
#if FELUCCA_UAC
    case 0x2201:                                        /* SET_CUR, endpoint: sampling frequency */
        if ((s[4] != 0x84u && !(USB_DUPLEX && s[4] == 0x04u)) || s[3] != 1u)
            goto stall;
        if (wlength) {
            if (s[4] == 0x04u)                          /* (0.7.5) the output: 44.1 only, the value ignored */
                uin.e0_rx = 1;
            else
                uac.e0_rx = 1;
            sie_wr(S_INDEX, 0);
            sie_wr(S_CSR0, 0x40);                       /* the 3 bytes follow */
            return;
        }
        goto ack;
    case 0xA281:                                        /* GET_CUR / MIN / MAX: the rate / the range */
    case 0xA282:
    case 0xA283: {
        static const uint8_t RATE[3] = {UAC_RATE & 0xFF, (UAC_RATE >> 8) & 0xFF, UAC_RATE >> 16};
#if FELUCCA_UAC_48K
        static const uint8_t RATE48[3] = {48000u & 0xFF, (48000u >> 8) & 0xFF, 48000u >> 16};
#endif
        if ((s[4] != 0x84u && !(USB_DUPLEX && s[4] == 0x04u)) || s[3] != 1u)   /* (0.7.5: the output, 44.1) */
            goto stall;
#if FELUCCA_UAC_48K
        if (s[4] == 0x84u && (s[1] == 0x83u || (s[1] == 0x81u && uac.r48))) {
            e0_send(RATE48, 3, wlength);
            return;
        }
#endif
        e0_send(RATE, 3, wlength);
        return;
    }
#endif
    default:
        goto stall;
    }
ack:
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x48);
    return;
stall:
    sie_wr(S_INDEX, 0);
    sie_wr(S_CSR0, 0x60);
}

/* SysEx assembly: only short commands matter here */
static void sysex_byte(uint8_t b)
{
    static const uint8_t UBOOT_KEY[6] = {0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7};
    if (b >= 0xF8u)
        return;                                        /* realtime may occur anywhere in SysEx */
    if ((b & 0x80u) && b != 0xF0u && b != 0xF7u) {
        usb.sx_on = 0;
#if FELUCCA_OTA
        sx_collect = 0;
#endif
        return;                                        /* another status aborts the unfinished frame */
    }
    if (b == 0xF0) {
        usb.sx_on = 1;
        usb.sx_len = 0;
#if FELUCCA_OTA
        sx_collect = !sx_ready;
        sx_pos = 0;
#endif
    }
    if (!usb.sx_on)
        return;
#if FELUCCA_OTA
    if (sx_collect && b != 0xF0 && b != 0xF7) {
        if (sx_pos < sizeof sx_frame)
            sx_frame[sx_pos++] = b;
        else
            sx_collect = 0;                             /* too long: not ours */
    }
#endif
    if (usb.sx_len < sizeof usb.sysex)
        usb.sysex[usb.sx_len++] = b;
    if (b == 0xF7) {
        uint32_t i, ok = usb.sx_len == 6u;
        for (i = 0; ok && i < 6u; i++)
            ok = usb.sysex[i] == UBOOT_KEY[i];
        if (ok)
            usb.uboot_req = 1;
#if FELUCCA_OTA
        else if (usb.sx_len == 6u && usb.sysex[1] == 0x22 && usb.sysex[2] == 0x24 && usb.sysex[3] == 0x35 &&
                 usb.sysex[4] == 0x7F)
            usb.ota_req = 1;
        else if (sx_collect && sx_pos) {
            sx_frame_len = sx_pos;
            RING_PUBLISH();
            sx_ready = 1;                               /* main loop: ota_take() / ed_service() */
        }
        sx_collect = 0;
#endif
        usb.sx_on = 0;
    }
}

static void midi_in_event(uint32_t pkt)                 /* one USB-MIDI event packet */
{
    uint32_t cin = pkt & 15u, st = (pkt >> 8) & 0xFFu;
    if (cin >= 4u && cin <= 7u) {
        uint32_t k, nb = cin == 4u || cin == 7u ? 3u : cin == 6u ? 2u : 1u;
        for (k = 0; k < nb; k++)
            sysex_byte((uint8_t)(pkt >> (8u * (k + 1u))));
        return;
    }
    if (st >= 0x80u && st < 0xF8u) {
        usb.sx_on = 0;                                 /* channel/system common also ends SysEx */
#if FELUCCA_OTA
        sx_collect = 0;
#endif
    }
    if (cin == 0xFu && (st == 0xF8u || st == 0xFAu || st == 0xFBu || st == 0xFCu)) {
        midi_enqueue(0xFu | st << 8, 1u);
        return;
    }
    if (cin >= 8u && cin <= 0xEu && (st >> 4) == cin && !((pkt >> 16) & 0x80u) &&
        ((cin == 0xCu || cin == 0xDu) || !(pkt & 0x80000000u))) {
        midi_enqueue(pkt, 1u);
    }
}

/* one EP1 OUT packet (<= 16 events) into the MIDI ring, or 0: fewer than 16 + 8 slots free, so the packet
 * stays (the host is NAKed) and is retried next poll; a burst never overflows the ring (that panics), and
 * 8 slots stay for TRS MIDI, which shares the ring and cannot wait (31250 baud: about 3 messages a half).
 * The update loader never drains the ring (it wants SysEx only): no back-pressure there. */
#define EP1_ROOM (16u + 8u)
static int ep1_take(const uint8_t *b, uint32_t n)
{
    uint32_t i;
#ifndef FELUCCA_LOADER
    if (MQ - (mi_w - mi_r) < EP1_ROOM)
        return 0;
#endif
    for (i = 0; i + 3u < n; i += 4u)
        midi_in_event((uint32_t)b[i] | (uint32_t)b[i + 1] << 8 | (uint32_t)b[i + 2] << 16 | (uint32_t)b[i + 3] << 24);
    return 1;
}

static void ep1_rx(void)                                /* leaves the packet (NAK) while the ring is too full */
{
    uint32_t csr, n;
    sie_wr(S_INDEX, 1);
    csr = sie_rd(S_RXCSR1) | (sie_rd(S_RXCSR2) << 8);
    if (!(csr & 1u)) {
        usb.rx_pend = 0;
        return;
    }
    n = sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8);
    if (n > 64u)
        n = 64u;
    fm1_usb_rx_sync();
    if (!ep1_take(ep1rx, n))
        return;                                         /* rx_pend stays: retried next poll */
    usb.rx_pend = 0;
    usb.rx_pkts++;
    csr = (csr & ~0x164u) | 0x10u;
    sie_wr(S_RXCSR1, csr & 0xFFu);
    sie_wr(S_RXCSR2, csr >> 8);
}

#if FELUCCA_OTA
/* ---- SysEx frames for the main loop (ota.c / editor.c hooks; felucca.c and the
 * update loader supply ota_now_ms / ota_idle) ---- */
static uint32_t ota_now_ms(void);
static void ota_idle(void);

static int ota_wire_send(const uint8_t *p, uint32_t n)   /* F0..F7 -> USB-MIDI SysEx packets */
{
    uint32_t i = 0, t0 = ota_now_ms();
    sx_busy = 1;
    while (i < n) {
        uint32_t k = n - i >= 3u ? 3u : n - i, pkt;
        uint32_t cin = k == 3u && i + 3u < n ? 4u : k == 3u ? 7u : 4u + k;   /* 4 continues; 5/6/7 end */
        pkt = cin | (uint32_t)p[i] << 8 | (k > 1u ? (uint32_t)p[i + 1] << 16 : 0u) |
              (k > 2u ? (uint32_t)p[i + 2] << 24 : 0u);
        while (so_w - so_r >= SXQ) {
            if (!*(volatile uint8_t *)&usb.config || ota_now_ms() - t0 > 200u) {
                sx_busy = 0;
                return -1;
            }
            ota_idle();
        }
        sx_out_q[so_w % SXQ] = pkt;
        RING_PUBLISH();
        so_w++;
        i += k;
    }
    sx_busy = 0;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!sx_ready)
        return 0;
    RING_PUBLISH();                                     /* read the frame only after the flag */
    *p = sx_frame;
    *n = sx_frame_len;
    return 1;
}
static void ota_frame_done(void)
{
    RING_PUBLISH();                                     /* done with the frame before the ISR may refill it */
    sx_ready = 0;
}
#endif

static void ep1_tx(void)
{
    uint32_t csr, n = 0;
#if FELUCCA_OTA
    if (mo_w == mo_r && so_w == so_r)
        return;
#else
    if (mo_w == mo_r)
        return;
#endif
    sie_wr(S_INDEX, 1);
    csr = sie_rd(S_TXCSR1);
    if (csr & 0x01u)
        return;                                         /* previous packet still pending */
    if (csr & 0x80u)
        sie_wr(S_TXCSR1, csr & ~0x80u);
#if FELUCCA_OTA
    while (so_r != so_w && n < 64u) {                   /* SysEx first, never split by notes */
        uint32_t pkt = sx_out_q[so_r % SXQ];
        ep1tx[n] = (uint8_t)pkt;
        ep1tx[n + 1] = (uint8_t)(pkt >> 8);
        ep1tx[n + 2] = (uint8_t)(pkt >> 16);
        ep1tx[n + 3] = (uint8_t)(pkt >> 24);
        n += 4u;
        so_r++;
    }
    while (!sx_busy && so_r == so_w && mo_r != mo_w && n < 64u) {
#else
    while (mo_r != mo_w && n < 64u) {
#endif
        uint32_t pkt, due;
        RING_PUBLISH();                                 /* the audio ISR (producer) can preempt us: */
        due = midi_out_due[mo_r % MQ];                  /* (V10.33) not its time yet: it and what follows wait */
        if (due && (int32_t)(fm1_ticks() - due) < 0)
            break;
        pkt = midi_out_q[mo_r % MQ];                    /* slot read strictly between the index checks */
        ep1tx[n] = (uint8_t)pkt;
        ep1tx[n + 1] = (uint8_t)(pkt >> 8);
        ep1tx[n + 2] = (uint8_t)(pkt >> 16);
        ep1tx[n + 3] = (uint8_t)(pkt >> 24);
        n += 4u;
        RING_PUBLISH();
        mo_r++;
    }
    if (!n)
        return;
    fm1_usb_ep_send(1, ep1tx, n);
    sie_wr(S_TXCSR1, sie_rd(S_TXCSR1) | 0x01u);
    usb.tx_pkts++;
}

#if FELUCCA_CDC
static void ep3_rx(void)                                /* leaves the packet (NAK) while the ring is full */
{
    uint32_t csr, n, i;
    sie_wr(S_INDEX, 3);
    csr = sie_rd(S_RXCSR1) | (sie_rd(S_RXCSR2) << 8);
    if (!(csr & 1u)) {
        cdc.rx_pend = 0;
        return;
    }
    n = sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8);
    if (n > 64u)
        n = 64u;
    if (CI_N - (ci_w - ci_r) < n)
        return;                                         /* rx_pend stays: retried next poll */
    cdc.rx_pend = 0;
    fm1_usb_rx_sync();
    for (i = 0; i < n; i++)
        cdc_in[(ci_w + i) % CI_N] = ep3rx[i];
    RING_PUBLISH();
    ci_w += n;
    cdc.rx_pkts++;
    csr = (csr & ~0x164u) | 0x10u;
    sie_wr(S_RXCSR1, csr & 0xFFu);
    sie_wr(S_RXCSR2, csr >> 8);
}

static void ep3_tx(void)                                /* <= 63 bytes per packet: never needs a ZLP */
{
    uint32_t csr, n = 0;
    if (co_w == co_r)
        return;
    sie_wr(S_INDEX, 3);
    csr = sie_rd(S_TXCSR1);
    if (csr & 0x01u)
        return;
    if (csr & 0x80u)
        sie_wr(S_TXCSR1, csr & ~0x80u);
    while (co_r != co_w && n < 63u)
        ep3tx[n++] = cdc_out[co_r++ % CO_N];
    fm1_usb_ep_send(3, ep3tx, n);
    sie_wr(S_TXCSR1, sie_rd(S_TXCSR1) | 0x01u);
    cdc.tx_pkts++;
}
#endif

#if FELUCCA_UAC
/* ---- the audio input: audio.c produces (render start, then each block), TIMER5 consumes ---- */
#if FELUCCA_UAC_48K
static void uac_collect(int wait);
static void uac_rs_init(void)                           /* the resampler from silence; its table built once */
{
    uint32_t i, p;
    for (i = 0; i < 2u * RS_H; i++)
        rs.l[i] = rs.r[i] = 0;
    rs.w = rs.ph = 0;
    if (rs_tab[0][UAC_FIR_N / 2u - 1u])                 /* (the pool is zeroed at boot) */
        return;
    for (i = 0; i < UAC_FIR_N; i++) {                   /* each tap along the phases: its differences summed back */
        int32_t x = UAC_FIR0[0][i], a = UAC_FIR0[1][i], b = UAC_FIR0[2][i], c = UAC_FIR3S[i];
        for (p = 0; p < UAC_FIR_L / 2u; p++) {
            rs_tab[p][i] = x;
            rs_tab[UAC_FIR_L - 1u - p][UAC_FIR_N - 1u - i] = x;   /* (phase 159 - p: phase p backwards) */
            x += a;
            a += b;
            if (p < UAC_FIR_L / 2u - 3u) {
                b += c;                                 /* (V10.6k: the third differences summed back from the fourth) */
                if (p < UAC_FIR_L / 2u - 4u)
                    c += UAC_FIR4[i][p];
            }
        }
    }
}
#endif
static __attribute__((noinline)) void uac_render_start(void)   /* audio ISR, before a half buffer renders */
{
    uint32_t fill;
    uac.feed = uac.alt && usb.config && uac.flowing;
#if FELUCCA_UAC_48K
    rq.nin = 0;
    fill = ua_w - ua_r;                                 /* (CORES 2: the low point, before the last half comes in) */
    uac_collect(0);
    if (uac.ring48 && rq.c1 != UAC_C1_LIVE())           /* (core 1 given up mid-stream, or back) */
        uac.go = 0;
#else
    fill = ua_w - ua_r;
#endif
    if (!uac.feed) {
        uac.go = 0;                                     /* not read (yet / any more): prime when it is */
        return;
    }
    if (!uac.go || uac.ring48 != uac.r48) {             /* (re)start: silence ahead, then this render */
        uint32_t r = ua_r, i, prime = UA_PRIME + (UA_FW > 1u ? UA_PAD_STEMS : 0u);
        uac.go = 0;
        uac.owe = 0;
#if FELUCCA_UAC_48K
        if (rq.busy)                                    /* (a half still on core 1: prime when it is in) */
            return;
        uac.ring48 = uac.r48;
        rq.c1 = 0;
        if (uac.ring48) {                               /* the resampler starts from silence */
            rq.c1 = (uint8_t)UAC_C1_LIVE();
            prime = UA_PRIME48 + (rq.c1 ? UA_REND48 : 0u);
            uac_rs_init();
        }
#endif
        for (i = 0; i < prime * UA_FW; i++)
            ua_ring[ua_slot(r + i / UA_FW) * UA_FW + i % UA_FW] = 0;
        RING_PUBLISH();
        ua_w = r + prime;
        uac.fill_min = prime;
        RING_PUBLISH();
        uac.go = 1;
        return;
    }
    uac.fill_min = fill;
    if (fill < uac.fill_lo)
        uac.fill_lo = fill;
    if (fill > uac.fill_hi)
        uac.fill_hi = fill;
}

static inline int32_t uac_clip(int32_t v) { return v > 32767 ? 32767 : v < -32768 ? -32768 : v; }
#ifndef UAC_GAIN
#define UAC_GAIN() 4096                /* (a build without fx.c: the host tests of USB alone) */
#endif
static inline int32_t uac_s16(int32_t v)
{
    v = (v * UAC_GAIN()) >> 12;                       /* (0.7.0) MENU > USB LEVEL -6 .. -24 dB (fx.c fx_usb_gain) */
    return v > 32767 ? 32767 : v < -32768 ? -32768 : v;
}

#if FELUCCA_UAC_48K
#define UAC_CLIP(v) ((v) > 32767 ? 32767 : (v) < -32768 ? -32768 : (v))
/* 44.1 -> 48 kHz (x 160 / 147): n input frames (16-bit L | R << 16) into the ring from frame w; each input frame enters
 * the history, then the 1 or 2 output frames whose newest input it is: phase ph, the window of UAC_FIR_N frames ending
 * at it, phase ph's taps (rs_tab), Q30 into 64-bit sums, four taps a pass. Returns the frames written. (0.8.9) In RAM
 * and calling nothing: core 1 runs it outside the audio ISR (uac_rs_job), core 0 with one core; bit for bit the
 * resampler of 0.7 .. 0.8.8 */
/* (0.9.9-stab) two taps' products, out of line in RAM beside it (core.h mul64: the pi32v2 fault beside 16-bit loads; the
 * FIR's history is int16 and its taps Q30, so 32-bit products cannot do). The same sums bit for bit (uac_test) */
static UAC_C1_TEXT int64_t uac_mac2(int32_t c0, int32_t x0, int32_t c1, int32_t x1) { return (int64_t)c0 * x0 + (int64_t)c1 * x1; }
static UAC_C1_TEXT uint32_t uac_rs_run(const uint32_t *in, uint32_t n, uint32_t w)
{
    uint32_t i, j, k = 0, h = rs.w, ph = rs.ph;
    if (ph >= UAC_FIR_L)                                /* (core 1 may misread a word: never past the table) */
        ph = 0;
    for (i = 0; i < n; i++) {
        uint32_t f = in[i], s = h & (RS_H - 1u);
        rs.l[s] = rs.l[s + RS_H] = (int16_t)f;
        rs.r[s] = rs.r[s + RS_H] = (int16_t)(f >> 16);
        h++;
        s = (h - UAC_FIR_N) & (RS_H - 1u);              /* the window: oldest .. this frame */
        do {
            const int16_t *x = &rs.l[s];                /* R: x[2 * RS_H] */
            const int32_t *c = rs_tab[ph];
            int64_t al = 0, ar = 0;
            int32_t yl, yr;
            for (j = UAC_FIR_N / 4u; j; j--, x += 4, c += 4) {
                int32_t c0 = c[0], c1 = c[1], c2 = c[2], c3 = c[3];
                al += uac_mac2(c0, x[0], c1, x[1]);
                ar += uac_mac2(c0, x[2u * RS_H], c1, x[2u * RS_H + 1u]);
                al += uac_mac2(c2, x[2], c3, x[3]);
                ar += uac_mac2(c2, x[2u * RS_H + 2u], c3, x[2u * RS_H + 3u]);
            }
            yl = (int32_t)((al + (1 << (UAC_FIR_Q - 1))) >> UAC_FIR_Q);
            yr = (int32_t)((ar + (1 << (UAC_FIR_Q - 1))) >> UAC_FIR_Q);
            yl = UAC_CLIP(yl);
            yr = UAC_CLIP(yr);
#if FELUCCA_UAC_TONE
            {   /* bench: the triangles at 48 kHz (not resampled), one per output frame */
                uint32_t t = rs.tone, q = t < 50u ? t : 100u - t, q2 = t < 50u ? 50u - t : t - 50u;
                rs.tone = t == 99u ? 0u : t + 1u;
                yl = (int32_t)q * 1000 - 25000;
                yr = (int32_t)q2 * 1000 - 25000;
            }
#endif
            ua_ring[(w + k++) & (UA_N - 1u)] = (uint32_t)(uint16_t)yl | (uint32_t)(uint16_t)yr << 16;
            ph += UAC_FIR_M;
        } while (ph < UAC_FIR_L);
        ph -= UAC_FIR_L;
    }
    rs.w = h;
    rs.ph = ph;
    return k;
}
static UAC_C1_TEXT void uac_rs_job(uint32_t a)          /* core 1: the half core 0 handed over (uac_render_end) */
{
    uint32_t n = rq.n;
    (void)a;
    rq.k = uac_rs_run(rs_in, n <= HALF_FRAMES ? n : 0u, rq.w0);
}
/* Publish core 1's half only after completion. The mix can skip its job (silent parts / buses), so its wait is not
 * guaranteed. Before reusing rs_in, wait here if necessary; only a stopped core can relinquish an unfinished half. */
static void uac_collect(int wait)
{
    uint32_t k;
    if (!rq.busy)
        return;
    if (!UAC_C1_DONE(rq.job) && UAC_C1_LIVE()) {
        if (!wait)
            return;
        (void)UAC_C1_WAIT();                            /* bounded, stops core 1 on a fault / timeout (fm1_multicore.h) */
        if (!UAC_C1_DONE(rq.job) && UAC_C1_LIVE())
            return;                                    /* keep ownership if a backend did not finish or stop it */
    }
    if (!UAC_C1_DONE(rq.job)) {
        rq.busy = uac.go = 0;                           /* core 1 stopped: the partial half is discarded */
        return;
    }
    k = rq.k;                                          /* after DONE's publication barrier, never before it */
    rq.busy = 0;
    if (k <= UA_REND48 + 1u) {                          /* (a wrong word from core 1: the half dropped) */
        RING_PUBLISH();
        ua_w = rq.w0 + k;
    }
}
/* the audio ISR, after each block: 16-bit frames, then (one core) resampled into the ring at once, or (CORES 2) kept
 * for the half's end */
static __attribute__((noinline)) void uac_tap48(const int32_t *out, uint32_t n)
{
    uint32_t i, *d;
    if (rq.c1) {
        uac_collect(1);                                 /* the mix may have skipped its core-1 job and wait */
        if (rq.busy || !uac.go || rq.nin + n > HALF_FRAMES)
            return;
        d = &rs_in[rq.nin];
        rq.nin += n;
    } else {
        if (UA_N - (ua_w - ua_r) < n + n / 8u + 2u) {   /* nobody reads: drop the block (<= n * 160 / 147 + 1) */
            uac.overruns++;
            return;
        }
        d = rs_in;
    }
    for (i = 0; i < n; i++)
        d[i] = (uint32_t)(uint16_t)uac_s16(out[2u * i]) | (uint32_t)(uint16_t)uac_s16(out[2u * i + 1u]) << 16;
    if (!rq.c1) {
        uint32_t w = ua_w, k = uac_rs_run(rs_in, n, w);
        RING_PUBLISH();
        ua_w = w + k;
    }
}
#endif

/* (0.8.9) the audio ISR, after the render: CORES 2 at 48 kHz, the half to core 1 (it resamples while core 0 goes back
 * to the main loop; the next half takes it in); core 1 gone, here */
static __attribute__((noinline)) void uac_render_end(void)
{
#if FELUCCA_UAC_48K
    uint32_t n = rq.nin, w = ua_w;
    rq.nin = 0;
    if (!n || rq.busy || !uac.feed)
        return;
    if (UA_N - (w - ua_r) < n + n / 8u + 2u) {          /* nobody reads: drop the half */
        uac.overruns++;
        return;
    }
    rq.n = n;
    rq.w0 = w;
    RING_PUBLISH();
    if (UAC_C1_LIVE() && UAC_C1_RUN(uac_rs_job)) {
        rq.job = UAC_C1_JOB();
        rq.busy = 1;
    } else {
        uac.go = 0;                                     /* (core 1 given up: one core from the next half) */
    }
#endif
}

static __attribute__((noinline)) void uac_tap(const int32_t *out, uint32_t n)   /* audio ISR: one block (Q15) */
{
    uint32_t w = ua_w, i;
    if (!uac.feed || !uac.go)                           /* ((0.8.9) not primed: a half on core 1 to wait for) */
        return;
#if FELUCCA_UAC_48K
    if (uac.ring48) {
        uac_tap48(out, n);
        return;
    }
#endif
    if (UA_N - (w - ua_r) < n) {                        /* nobody reads: drop the block */
        uac.overruns++;
        return;
    }
#if FELUCCA_UAC_TONE
    {   /* bench: triangles of period 100 frames instead of the music (L, R in antiphase): every
         * recorded frame must follow the one before, so a lost or repeated frame shows exactly */
        static uint32_t ph;
        for (i = 0; i < n; i++, ph = ph == 99u ? 0u : ph + 1u) {
            uint32_t q = ph < 50u ? ph : 100u - ph, q2 = ph < 50u ? 50u - ph : ph - 50u;
            ua_ring[(w + i) & (UA_N - 1u)] = (uint32_t)(uint16_t)(int16_t)((int32_t)q * 1000 - 25000) |
                                             (uint32_t)(uint16_t)(int16_t)((int32_t)q2 * 1000 - 25000) << 16;
        }
        (void)out;
    }
#else
    for (i = 0; i < n; i++)
        ua_ring[(w + i) & (UA_N - 1u)] = (uint32_t)(uint16_t)uac_s16(out[2u * i]) |
                                         (uint32_t)(uint16_t)uac_s16(out[2u * i + 1u]) << 16;
#endif
    RING_PUBLISH();
    ua_w = w + n;
}
#ifdef UAC_STEMS_SRC
/* (0.7.1) MENU > USB STEMS: a block as 8 channels (fx.c stem_buf: the six parts mono, stem_fx: the FX return L R),
 * 16 bit each, four words a frame; the ring, the priming and the packets as the stereo stream's (44.1 kHz only).
 * (0.8.0) STEMS+IN: two more, a fifth word: the host's audio as the mix took it (stem_buf[NPART]: its LEVEL, PAN and
 * MUTE, in its own channel so the host can move it by the latency) and as it came (uin_l / uin_r halved: the
 * reference the host finds its latency against) */
static __attribute__((noinline)) void uac_tap8(uint32_t n)
{
    /* USB mode changes stop/re-enumerate the stream outside the audio IRQ.
     * Snapshot it once: UA_FW reads volatile fx_stems, not a loop constant.
     * Walk slots rather than recomputing the 409-frame modulo for each sample. */
    uint32_t w = ua_w, i, k, fw = UA_FW;
    uint32_t nf = fw == 5u ? UA_NF10 : UA_N;
    uint32_t slot = fw == 5u ? w % UA_NF10 : w & (UA_N - 1u);
    if (!uac.feed)
        return;
    if (nf - (w - ua_r) < n) {
        uac.overruns++;
        return;
    }
    for (i = 0; i < n; i++) {
        if ((uint32_t)(w + i) == 0u) slot = 0;          /* preserve the free-running index's uint32 wrap */
        uint32_t *f = &ua_ring[slot * fw];
        for (k = 0; k < 3u; k++)
            f[k] = (uint32_t)(uint16_t)uac_s16(stem_buf[2u * k][i]) | (uint32_t)(uint16_t)uac_s16(stem_buf[2u * k + 1u][i]) << 16;
        f[3] = (uint32_t)(uint16_t)uac_s16(stem_fx[0][i]) | (uint32_t)(uint16_t)uac_s16(stem_fx[1][i]) << 16;
        if (fw == 5u)
            f[4] = (uint32_t)(uint16_t)uac_s16(stem_buf[6][i]) |
                   (uint32_t)(uint16_t)(uin_on ? (int16_t)((uin_l[i] + uin_r[i]) >> 1) : 0) << 16;
        if (++slot == nf) slot = 0;
    }
    RING_PUBLISH();
    ua_w = w + n;
}
#define UAC_TAP8 1                 /* (audio.c: a build with fx.c but without usb.c, audio_test, has no stems tap) */
#endif

/* (0.9.8) a frame made up after an underrun (uac_packet): the ring's n + 1 frames from r read into the packet's n, each
 * channel linearly between its two neighbours (the step 1 + 1 / (n + 1) frame), so the frame left out is spread over
 * the millisecond instead of skipped (a skip is a step in a soloed stem: a click). Each 16-bit channel of each word */
static __attribute__((noinline)) void uac_squeeze(uint32_t *d, uint32_t r, uint32_t n)
{
    uint32_t i, j, fw = UA_FW, st = (16384u << 8) / (n + 1u), q = st;
    uint32_t nf = fw == 5u ? UA_NF10 : UA_N;
    uint32_t slot = fw == 5u ? r % UA_NF10 : r & (UA_N - 1u);
    for (i = 0; i < n; i++, q += st) {
        uint32_t next = (uint32_t)(r + i + 1u) == 0u || slot + 1u == nf ? 0u : slot + 1u;
        const uint32_t *a = &ua_ring[slot * fw], *b = &ua_ring[next * fw];
        int32_t w = (int32_t)(q >> 8);                  /* (i + 1) / (n + 1), Q14 */
        for (j = 0; j < fw; j++) {
            int32_t al = (int16_t)a[j], ah = (int16_t)(a[j] >> 16);
            int32_t l = al + ((((int32_t)(int16_t)b[j] - al) * w + 8192) >> 14);
            int32_t h = ah + ((((int32_t)(int16_t)(b[j] >> 16) - ah) * w + 8192) >> 14);
            d[i * fw + j] = (uint32_t)(uint16_t)l | (uint32_t)(uint16_t)h << 16;
        }
        slot = next;
    }
}

/* the next packet into d: 44 / 45 frames (+-1 to hold the ring's fill), from the ring or, until it is
 * primed, silence. An empty ring repeats the last frame. Returns the frame count. */
static uint32_t uac_packet(uint32_t *d)
{
    uint32_t fw = UA_FW, n = 44u, r, fill, k, i;
    uint32_t pad = fw > 1u ? UA_PAD_STEMS : 0u, lo = UA_LO + pad, hi = UA_HI + pad;
#if FELUCCA_UAC_48K
    if (uac.r48) {                                      /* 48 frames a ms, no pattern */
        n = 48u;
        lo = UA_LO48;
        hi = UA_HI48;
    } else
#endif
    {
        uac.acc += UAC_RATE % 1000u;
        if (uac.acc >= 1000u) {
            uac.acc -= 1000u;
            n++;
        }
    }
    if (!uac.go || uac.ring48 != uac.r48) {             /* not primed (at this rate) yet */
        for (i = 0; i < n * fw; i++)
            d[i] = 0;
        return n;
    }
    if (uac.fill_min > hi) {
        n++;
        uac.adj_up++;
    } else if (uac.fill_min < lo) {
        n--;
        uac.adj_down++;
    }
    r = ua_r;
    fill = ua_w - r;
    RING_PUBLISH();                                     /* the slots after the index */
    if (uac.owe && uac.fill_min >= lo && fill > n) {    /* (0.8.9) a frame an underrun repeated, made up: one */
        uac.owe--;                                      /* frame more of the ring taken, so the stream keeps the */
        uac.repaid++;                                   /* render's time and length (a packet still carries n). */
        uac_squeeze(d, r, n);                           /* (0.9.8) n + 1 frames read into n, not one skipped: a */
        RING_PUBLISH();                                 /* skip was a step in every stem (a click soloed) */
        ua_r = r + n + 1u;
        uac.pkts++;
        uac.frames += n;
        for (i = 0; i < fw; i++)
            uac.last[i] = d[(n - 1u) * fw + i];
        return n;
    }
    k = fill < n ? fill : n;
    {
        uint32_t j, nf = fw == 5u ? UA_NF10 : UA_N;
        uint32_t slot = fw == 5u ? r % UA_NF10 : r & (UA_N - 1u);
        for (i = 0; i < k; i++) {
            if ((uint32_t)(r + i) == 0u) slot = 0;
            for (j = 0; j < fw; j++)
                d[i * fw + j] = ua_ring[slot * fw + j];
            if (++slot == nf) slot = 0;
        }
        if (k < n) {
            uac.underruns++;
            uac.owe += n - k;
            for (; i < n; i++)
                for (j = 0; j < fw; j++)
                    d[i * fw + j] = k ? d[(k - 1u) * fw + j] : uac.last[j];
        }
        for (j = 0; j < fw; j++)
            uac.last[j] = d[(n - 1u) * fw + j];
    }
    RING_PUBLISH();
    ua_r = r + k;
    uac.pkts++;
    uac.frames += n;
    return n;
}

/* TIMER5, every tick (10 kHz), also nested in the render: the host takes one packet per 1 ms frame; queue the
 * next one as soon as the last has gone (TxPktRdy clear). Not paced on SOF-pending (see usb_poll).
 * (0.9.10) Every tick, not every 5th: TxPktRdy stays set while the packet is on the wire ((bytes + 9) x 8 x ~1.04
 * / 12 MHz: STEMS+IN's 920 bytes ~0.65 ms, STEMS' 736 ~0.52 ms), so the next one could only be queued in the
 * ~0.35 ms before the next IN token, less than 2 kHz's 0.5 ms. Whenever TIMER5's phase drifted into it against the
 * host's frames, tokens found nothing, the ring overran and uac_tap8 dropped 32-frame blocks, in bursts, in every
 * stem (tests/stems_usb_test.c). Nothing to do costs the guard; streaming, two SIE round trips (ep4_tx).
 * The ring is fed only while the host reads: it often selects alt 1 long before its first IN token,
 * and a recorder may stop reading without going back to alt 0 (a packet waiting > 20 ms). */
#define UAC_POLL_HZ 10000u                              /* uac_service's rate (main.c fm1_timer5_irq; the tests' too) */
static __attribute__((noinline)) void ep4_fill(uint32_t csr)   /* (out of ep4_tx: its registers pushed once a ms) */
{
    uint32_t n;
    if (uac.queued) {
        if (csr & 0x04u)
            uac.missed++;                               /* UnderRun: an IN token found nothing queued */
        uac.flowing = 1;
    }
    uac.queued = 1;
    uac.wait = 0;
    n = uac_packet(ep4tx);
    fm1_usb_ep4_send(ep4tx, n * 4u * UA_FW);
    sie_wr(S_TXCSR1, 0x01);                             /* TxPktRdy (and UnderRun cleared) */
}
static __attribute__((noinline)) void ep4_tx(void)
{
    uint32_t csr;
    sie_wr(S_INDEX, 4);
    csr = sie_rd(S_TXCSR1);
    if (!(csr & 0x01u)) {
        ep4_fill(csr);
        return;
    }
    if (uac.wait < 255u && ++uac.wait == UAC_POLL_HZ / 50u && uac.flowing) {   /* still queued, 20 ms */
        uac.flowing = 0;
        uac.stalls++;
    }
}
/* (p1, main 0.9.12) a packet just queued is on its way: TxPktRdy stays set until the next IN token has taken it, >= ~0.9
 * ms after the fill (the token after the one that took the last, then its time on the wire), so the first UAC_SKIP
 * ticks after a fill are not polled (their wait still counted): ~4 SIE round trips a ms instead of ~10, inside the
 * render too (TIMER5 nests in it) */
#define UAC_SKIP 6u
static inline void uac_service(void)                   /* (inline: the guard alone in the tick, nothing pushed) */
{
    if (usb.up && usb.config && uac.alt) {
        if (uac.queued && uac.wait < UAC_SKIP)
            uac.wait++;
        else
            ep4_tx();
    }
}

/* (0.7.5) MENU > USB IN+OUT, TIMER5: a packet the host played to us (n bytes of L | R << 16 frames) into ui_ring;
 * a full ring drops it (the reader has stopped: it re-primes) */
static __attribute__((noinline)) void uin_take(const uint32_t *p, uint32_t n)   /* (not in the TIMER5 body: its copy loop) */
{
    uint32_t w = ui_w, i, f = n / 4u;
    if ((n & 3u) || f > UI_MAXF_RX) {
        uin.bad++;
        if (f > UI_MAXF_RX)
            f = UI_MAXF_RX;
    }
    if (UI_N - (w - ui_r) < f) {
        uin.overruns++;
        return;
    }
    for (i = 0; i < f; i++)
        ui_ring[(w + i) & (UI_N - 1u)] = p[i];
    RING_PUBLISH();
    ui_w = w + f;
    uin.pkts++;
    uin.frames += f;
}
static __attribute__((noinline)) void uin_rx(void)
{
    uint32_t csr, n;
    sie_wr(S_INDEX, 4);
    csr = sie_rd(S_RXCSR1) | (sie_rd(S_RXCSR2) << 8);
    if (!(csr & 1u))                                    /* RxPktRdy */
        return;
    n = sie_rd(S_RXCOUNT1) | (sie_rd(S_RXCOUNT2) << 8);
    if (n > sizeof ep4rx)
        n = sizeof ep4rx;
    fm1_usb_rx_sync();
    uin_take(ep4rx, n);
    csr = (csr & ~0x164u) | 0x10u;                      /* serviced, as EP1 / EP3 (ep1_rx); ISO kept */
    sie_wr(S_RXCSR1, csr & 0xFFu);
    sie_wr(S_RXCSR2, csr >> 8);
}

#ifdef UIN_SRC
/* (0.7.5) the audio ISR, before the mix: n frames of the host's audio into fx.c's uin_l / uin_r (Q15), read at the
 * I2S clock: a linear interpolation between ring frames, its step steered by the ring's fill (smoothed over ~0.4 s)
 * toward UI_TARGET: the host's rate, whatever the clocks. Primed: UI_TARGET frames in hand; it fades in. Short of
 * frames (the host stopped, a glitch): it fades out from the last frame and primes again. uin_on: this block holds it */
static __attribute__((noinline)) void uac_in_fill(uint32_t n)
{
    uint32_t r = ui_r, fill = ui_w - r, i, need, ph = uin.ph;
    int32_t step = uin.step, g = uin.g, base = uh_aud_on && uin_r48 ? 71331 : 65536;   /* (V10.57: 48 kHz in, 65536 x 48 / 44.1) */
    if (!uh_aud_on && (!uin.alt || !usb.config)) {     /* (V10.53: or an audio interface on the host port) */
        uin.go = 0;
        uin_on = 0;
        return;
    }
    if (uin.gen_seen != uin.gen) {                      /* a new stream: nothing of the last one */
        uin.gen_seen = uin.gen;
        uin.go = 0;
        RING_PUBLISH();
        ui_r = r = ui_w;
        fill = 0;
    }
    if (!uin.go) {
        if (fill < UI_TARGET) {
            uin_on = 0;
            return;
        }
        uin.go = 1;
        ph = 0;
        step = base;
        g = 0;
        uin.fs = (int32_t)fill << 8;
    }
    need = (uint32_t)(((uint64_t)ph + (uint64_t)(uint32_t)step * n) >> 16) + 2u;
    RING_PUBLISH();                                     /* the slots after the index */
    if (fill < need) {                                  /* short: fade out from the last frame, prime again */
        int32_t d = g / (int32_t)n + 1;
        uin.underruns++;
        uin.go = 0;
        for (i = 0; i < n; i++) {
            g = g > d ? g - d : 0;
            uin_l[i] = (uin.last_l * g) >> 15;
            uin_r[i] = (uin.last_r * g) >> 15;
        }
        uin.g = 0;
        uin_on = 1;
        return;
    }
    for (i = 0; i < n; i++) {
        uint32_t a = ui_ring[r & (UI_N - 1u)], b = ui_ring[(r + 1u) & (UI_N - 1u)], f = ph >> 1;
        int32_t al = (int16_t)a, ar = (int16_t)(a >> 16), bl = (int16_t)b, br = (int16_t)(b >> 16);
        /* Full-scale neighbours differ by up to 65535: multiplying by Q16 f
         * overflows signed 32-bit above half phase, producing a 65536-LSB jump.
         * Q15 phase keeps both products in int32, with < 2 LSB interpolation error
         * and no 64-bit multiplies in the audio ISR. */
        /* ph is kept in 0..65535 at every iteration; f is its Q15 fraction. */
        int32_t l = al + (((bl - al) * (int32_t)f) >> 15), rr = ar + (((br - ar) * (int32_t)f) >> 15);
        if (g < 32767)
            g = g + 1024 < 32767 ? g + 1024 : 32767;    /* (a fade in over ~32 frames) */
        uin_l[i] = (l * g) >> 15;
        uin_r[i] = (rr * g) >> 15;
        uin.last_l = l;
        uin.last_r = rr;
        ph += (uint32_t)step;
        r += ph >> 16;
        ph &= 0xFFFFu;
    }
    RING_PUBLISH();
    ui_r = r;
    fill = ui_w - r;
    uin.fs += (((int32_t)fill << 8) - uin.fs) >> 7;     /* (a block a ~0.7 ms: ~90 ms; the packets' saw tooth averaged) */
    {
        int32_t e = (uin.fs >> 8) - (int32_t)UI_TARGET;   /* more in hand than wanted: read faster */
        step = base + (e > UI_STEP_MAX ? UI_STEP_MAX : e < -UI_STEP_MAX ? -UI_STEP_MAX : e);
    }
    uin.ph = ph;
    uin.step = step;
    uin.g = g;
    uin_on = 1;
}
#define UAC_UIN 1                  /* (audio.c: a build with fx.c and usb.c reads the host's audio before the mix) */
#endif
#endif

#ifndef FELUCCA_USB_HOST
#define FELUCCA_USB_HOST 0                              /* (the host tests: no host mode) */
#endif
#include "usb_host.c"                                   /* (V10.41) FELUCCA_USB_HOST: the port as a MIDI host (USB > HOST) */

static void usb_poll(void)                              /* TIMER5 ISR, 2 kHz */
{
    uint32_t iu, it, ir;
#if FELUCCA_USB_HOST
    if (uh.run) {                                       /* a host: one IN endpoint, nothing of the device side */
        uh_poll();
        return;
    }
#endif
    if (!usb.up)
        return;
    if (fm1_usb_sof_take()) {                           /* SOF pending: NOT a reliable "host is there" */
                                                        /* (it keeps firing with the cable out) */
        usb.sof_seen++;
        usb.no_sof = 0;
    } else if (++usb.no_sof > usb.max_gap) {
        usb.max_gap = usb.no_sof;
    }
    if (usb.config) {                                   /* the frame number only advances with a real host */
        static uint32_t tick;
        if (++tick >= 20u) {                            /* every 20 polls = 10 ms */
            uint16_t f;
            tick = 0;
            f = (uint16_t)(sie_rd(S_FRAME1) | (sie_rd(S_FRAME2) & 7u) << 8);
            if (f != usb.frame) {
                usb.frame = f;
                usb.frame_same = 0;
                usb.suspended = 0;
            } else if (++usb.frame_same >= 10u && !usb.suspended) {   /* 100 ms frozen: unplugged / host asleep */
                usb.suspended = 1;
                usb.frame_stalls++;
            }
        }
    }
    iu = sie_rd(S_INTRUSB);
#ifdef FELUCCA_LOADER
    it = sie_rd(S_INTRTX1) | sie_rd(S_INTRTX2) << 8;    /* the update loader reads both */
    ir = sie_rd(S_INTRRX1) | sie_rd(S_INTRRX2) << 8;
#else
    it = sie_rd(S_INTRTX1);                             /* EP0..7 only: skipping INTR*2 saves */
    ir = sie_rd(S_INTRRX1);                             /* 2 SIE round trips per poll */
#endif
    if (iu & 0x01u) {                                   /* suspend: the bus went idle */
        usb.suspends++;
        usb.suspended = 1;
    }
    if (iu & 0x02u)                                     /* resume */
        usb.suspended = 0;
    if (iu & 0x04u) {                                   /* bus reset */
        usb.resets++;
        sie_wr(S_FADDR, 0);
        usb.config = 0;
        usb.suspended = 0;
        mo_r = mo_w;                                    /* nothing stale for the next host */
        usb.sx_on = 0;
#if FELUCCA_OTA
        so_r = so_w;
        sx_collect = 0;
#endif
        usb.e0_tx = 0;
        usb.has_pend_addr = 0;
        usb.rx_pend = 0;
#if FELUCCA_CDC
        cdc.dtr = 0;
        cdc.e0_rx = 0;
        cdc.rx_pend = 0;
#endif
#if FELUCCA_UAC
        uac.e0_rx = 0;
        uac_stream(0);
        uac_rate_set(UAC_RATE);
        uin.e0_rx = 0;
        uin_stream(0);
#endif
        fm1_usb_ep0_buf(ep0buf);
        sie_wr(S_INTRUSBE, 0x07);
        sie_wr(S_INTRTX1E, 0x01);
        sie_wr(S_INTRRX1E, 0);
    }
    if (it & 0x01u)
        ep0_service();
    if (ir & 0x02u)
        usb.rx_pend = 1;
    if (usb.rx_pend)                                    /* not every poll: 3 SIE round trips each */
        ep1_rx();
    if (usb.config)
        ep1_tx();
#if FELUCCA_CDC
    if (usb.config && usb_cdc_on) {
        if (ir & 0x08u)
            cdc.rx_pend = 1;
        if (cdc.rx_pend)                                /* not every poll: 3 SIE round trips each */
            ep3_rx();
        if (cdc.dtr)
            ep3_tx();
    }
#endif
#if FELUCCA_UAC
    uac_service();
    if (usb.config && uin.alt)                          /* (0.7.5) the host's audio: every poll (a 1 ms packet each */
        uin_rx();                                       /* other poll: an isochronous one not taken is lost) */
#endif
}

static void usb_start(void)                             /* boot, or main-loop retry while usb.up == 0 */
{
    usb.timeouts = 0;
    fm1_usb_reset();                                    /* reset whatever the ROM left */
    fm1_delay_ms(25);
    fm1_usb_attach(ep0buf);
    sie_wr(S_POWER, 0x60);
    sie_wr(S_INTRUSBE, 0x07);                           /* suspend, resume, reset (polled) */
    sie_wr(S_INTRTX1E, 0x01);
    sie_wr(S_INTRTX2E, 0);
    sie_wr(S_INTRRX1E, 0);
    sie_wr(S_INTRRX2E, 0);
    fm1_usb_ep_enable(0x1Fu);
    usb.up = usb.timeouts < 3u;                         /* a dead SIE leaves USB off */
}

/* Powered on without a cable, the SIE does not answer and usb_start leaves
 * USB off: retry once a second from the main loop so a cable plugged in
 * later still enumerates.
 * (0.9.10) After a mode switch (usb_replug) the device stays off the bus USB_REPLUG_MS first: it came back on the
 * next pass (~25 ms, usb_start's reset), too short for some hosts (a phone, a hub polled every 256 ms) to see it
 * leave, and they kept the old function: MENU > SYSTEM > USB > STEMS took several tries to appear. */
#define USB_REPLUG_MS 500u
static int usb_retry_due(uint32_t now_ms)              /* (apart: the host tests run it) */
{
    static uint32_t t;
    if (usb.replug) {
        usb.replug = 0;
        t = now_ms - 1000u + USB_REPLUG_MS;             /* (the next retry USB_REPLUG_MS from now) */
        return 0;
    }
    if (usb.up || usb.detached || now_ms - t < 1000u)
        return 0;
#if FELUCCA_USB_HOST
    if (uh.on)                                          /* (the port is a host: uh_task's) */
        return 0;
#endif
    t = now_ms;
    return 1;
}
static void usb_retry(uint32_t now_ms)
{
    if (!usb_retry_due(now_ms))
        return;
    usb.retries++;
    usb_start();
}

static void usb_detach(void)
{
    usb.detached = 1;
    usb.up = 0;
    fm1_usb_off();
}

/* a mode switch (main loop): off the bus, attached again by usb_retry with the new descriptors. Two switches in one
 * apply (the console off, the stems on) drop it once: the second finds it off */
static __attribute__((unused)) void usb_replug(void)
{
#if FELUCCA_USB_HOST
    if (uh.on)                                          /* (a device setting while a host: applied when HOST is off) */
        return;
#endif
    if (usb.up) {
        usb.up = 0;                                     /* usb_poll (TIMER5) stops before the SIE goes */
        RING_PUBLISH();
        fm1_usb_off();
    }
    usb.config = 0;
    usb.replug = 1;
}

#if FELUCCA_CDC
/* Present the device with or without the serial console (main loop; at boot before usb_start, or later
 * from a setting). A change while attached drops off the bus; usb_retry attaches again 0.5 s later
 * with the other descriptors, and the host enumerates the device afresh. */
static __attribute__((unused)) void usb_cdc_switch(uint32_t on)
{
    on = on != 0;
    if (on == usb_cdc_on)
        return;
    usb_replug();
    cdc.dtr = 0;                                        /* the console stops writing */
    cdc.rx_pend = 0;
    usb_cdc_on = (uint8_t)on;
}
#endif
#if FELUCCA_UAC
/* (0.7.1) MENU > USB STEMS: the device re-enumerates with the 8-channel audio function (on) or the stereo one (off);
 * as usb_cdc_switch, the console off with the stems (menu_items.c usb_serial_apply picks the three) */
static __attribute__((unused)) void usb_stems_switch(uint32_t on)
{
    on = on != 0;                                       /* ((0.8.0) STEMS+IN: this and usb_duplex_switch both on) */
    if (on == usb_stems)
        return;
    usb_replug();
    uac.go = 0;                                         /* the ring restarts in the new frame size */
    usb_stems = (uint8_t)on;
}
/* (0.7.5) MENU > USB IN+OUT: re-enumerates with the audio output as well (on) or without it, as usb_stems_switch */
static __attribute__((unused)) void usb_duplex_switch(uint32_t on)
{
    on = on != 0;
    if (on == usb_duplex)
        return;
    usb_replug();
    uac.go = 0;
    uin.alt = 0;
    uin.gen++;
    usb_duplex = (uint8_t)on;
    uin_feed = (uint8_t)(on | uh_aud_on);
}
#endif

