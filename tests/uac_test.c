/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the USB audio input in src/usb.c (FELUCCA_UAC):
 *   descriptors  the configuration parsed as a host does: lengths, interface and endpoint counts,
 *                class codes, the UAC1 chain (AC header collection, terminals, AS general, type I
 *                format: 44100 and 48000 Hz, the isochronous endpoint: 49 frames), the IADs; with and
 *                without CDC (-DT_CDC=0/1)
 *   ring         uac_render_start / uac_tap (the audio ISR) against uac_packet (TIMER5): renders of 256
 *                frames at the I2S rate, as late as the load makes them, packets at 1 kHz: sizes 43..46,
 *                44.1 on average, every frame delivered in order (bit for bit: 44.1 kHz is not resampled),
 *                then an underrun (repeats), an overrun (drops, the ring never overfills) and a restart.
 *   rate         SET_CUR's value -> the rate (uac_rate_set), GET_CUR / MIN / MAX answers.
 *   resampler    44.1 -> 48 kHz (uac_tap48, uac_fir.h): every output frame against a double-precision
 *                resampler with the prototype computed here (sinc x Kaiser, not read from the table);
 *                the response (20 Hz .. 20 kHz), SNR of sines at -1 dBFS (in band <= 20 kHz: the error
 *                after a fitted sine, through an FFT), the cost (instructions / ns per frame).
 *   ring 48      the same stream tests at 48 kHz: packets of 47..49 (48 on average), every resampled frame
 *                in order (against the reference), a fast and a slow I2S clock (adj up / down), the switch
 *                44.1 -> 48 -> 44.1 mid-stream (silence, primed again, then clean).
 * The SIE is never touched (uac_service and usb_poll are not called). */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#include <unistd.h>
#endif
#ifndef HALF_FRAMES
#error "-DHALF_FRAMES=n (src/core.h; run_tests.sh passes it)"
#endif
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")
#define FELUCCA_OTA 0
#define FELUCCA_CDC T_CDC
#define FELUCCA_UAC 1
static void fm1_delay_ms(uint32_t ms) { (void)ms; }
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"   /* SIE register macros (never touched here) */
/* (0.7.1) MENU > USB STEMS: fx.c's mode flag and stem buffers stood in for (usb.c uac_tap8, the 8-channel descriptors) */
static uint8_t t_stems;
#define UAC_STEMS() t_stems
#define UAC_STEMS_SRC 1
static int32_t stem_buf[7][HALF_FRAMES], stem_fx[2][HALF_FRAMES];   /* ((0.8.0) [6]: STEMS+IN's host audio) */
/* (0.7.5) MENU > USB IN+OUT: fx.c's input block stood in for (usb.c uac_in_fill writes it) */
static int32_t uin_l[HALF_FRAMES > 64 ? HALF_FRAMES : 64], uin_r[HALF_FRAMES > 64 ? HALF_FRAMES : 64];
static volatile uint8_t uin_on;
#define UIN_SRC 1
#include "../firmware/src/usb.c"

static int fails;
static int check(const char *what, int ok)
{
    printf("%-64s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        fails++;
    return ok;
}

static uint32_t le16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }

/* ------------------------------------------------------------------ descriptors --- */
/* (0.7.1) the stems: the 8-channel configuration (one rate, 736-byte packets: 0.9.8, was 720 while 46-frame packets went out; bcdDevice + 0x40), four words a frame
 * in the ring and the packet, the six parts then the FX return, each 16 bit */
static void test_stems(void)
{
    const uint8_t *d; uint16_t l; uint32_t off, it_ok = 0, fmt_ok = 0, ep_ok = 0, i, n, pk[UA_MAXF * UA_W];
    t_stems = 1; usb_stems = 1;                      /* (the firmware sets both: usb_serial_apply) */
    get_desc(0x0200, &d, &l);
    check("stems: the configuration is the 8-channel one, its length as declared", d == CFG_DESC_PLAIN8 && l == sizeof CFG_DESC_PLAIN8 && le16(d + 2) == l);
    for (off = 0; off + 1u < l; off += d[off]) {
        const uint8_t *p = d + off;
        if (p[1] == 0x24 && p[2] == 2 && p[0] == 12) it_ok = p[7] == 8 && p[8] == 0 && p[9] == 0;   /* input terminal: 8 ch, no config */
        if (p[1] == 0x24 && p[2] == 2 && p[0] == 11) fmt_ok = p[4] == 8 && p[5] == 2 && p[6] == 16 && p[7] == 1 && p[8] == 0x44 && p[9] == 0xAC && p[10] == 0;
        if (p[1] == 5 && p[2] == 0x84) ep_ok = le16(p + 4) == 736 && p[3] == 0x05;
    }
    check("stems: input terminal 8 channels, type I 8 ch 16 bit 44100 only, EP4 isochronous 736 bytes (46 frames)", it_ok && fmt_ok && ep_ok);
    get_desc(0x0100, &d, &l);
    check("stems: the device descriptor's bcdDevice + 0x40 (a host never reuses the stereo one's)", d == DEV_DESC_PLAIN8 && l == 18 && d[12] == (BCD_LO(0) + 0x40));
    memset(&uac, 0, sizeof uac);
    uac.feed = 1; uac.go = 1; uac.fill_min = UA_HI; ua_r = ua_w = 0;
    for (i = 0; i < HALF_FRAMES; i++) {
        uint32_t k;
        for (k = 0; k < 6u; k++) stem_buf[k][i] = (int32_t)(k + 1u) * 1000 + (int32_t)i;
        stem_fx[0][i] = -7000 - (int32_t)i; stem_fx[1][i] = 8000 + (int32_t)i;
    }
    uac_tap8(HALF_FRAMES);
    n = uac_packet(pk);
    check("stems: a block tapped as 8 x 16-bit frames of four words, a packet of 43..46 of them",
          ua_w == HALF_FRAMES && n >= 43u && n <= 46u && ua_r == n &&
          pk[0] == (1000u | 2000u << 16) && pk[1] == (3000u | 4000u << 16) && pk[2] == (5000u | 6000u << 16) &&
          pk[3] == ((uint32_t)(uint16_t)-7000 | 8000u << 16) && pk[4] == (1001u | 2001u << 16));
    t_stems = 0; usb_stems = 0;
    memset(&uac, 0, sizeof uac);
    ua_r = ua_w = 0;
}

/* (0.7.5) MENU > USB IN+OUT: the configuration with the host's audio output (four interfaces, the AC header's collection
 * of three, terminals 3 -> 4, EP4 OUT isochronous adaptive 184 bytes, bcdDevice + 0x80); then the stream: packets of
 * the host's 44.1 kHz (44 / 45 frames a ms) taken into the ring (uin_take, as TIMER5's uin_rx) against blocks of 32
 * read at the I2S clock (uac_in_fill): fast (44,117.6 Hz, the FM-1's) and slow (44,060): primed, faded in, the fill
 * held near UI_TARGET (no underrun or overrun once settled), a 1 kHz sine through it within -40 dB of a pure one at
 * its real frequency; the host stopping (a fade out from the last frame, primed again when it comes back), alt 0 */
static int32_t dup_smin, dup_smax;
static int dup_src48;                                /* (V10.57) dup_run: the source at 48 kHz (USB > HOST) */
static double dup_run(double fd, uint32_t secs, uint32_t *under, uint32_t *over, int32_t *fmin, int32_t *fmax,
                      double *snr, int32_t *jump, int gap)
{
    static uint32_t pk[UI_MAXF_RX];
    double th = 0, t = 0, tp = 0, tb = 0, f0 = 1000.0;
    uint32_t acc = 0, blocks = 0, i, n0 = 0;
    int32_t prev = 0;
    double *ys = malloc(sizeof(double) * 44200u * secs), *ts = malloc(sizeof(double) * 44200u * secs);
    uint32_t ny = 0;
    memset(&uin, 0, sizeof uin);
    ui_w = ui_r = 0;
    usb.config = 1;
    uin.alt = 1;
    *fmin = 1 << 30; *fmax = 0; *jump = 0;
    while (t < secs) {
        if (tp <= tb) {                                 /* a host packet (1 ms: 44 or 45 frames of 44.1 kHz) */
            uint32_t f = 44u;
            acc += 100u;
            if (acc >= 1000u) { acc -= 1000u; f = 45u; }
            if (dup_src48) f = 48u;                     /* (V10.57) an interface at 48 kHz on the host port */
            for (i = 0; i < f; i++, th += 1.0 / (dup_src48 ? 48000.0 : 44100.0)) {
                int16_t l = (int16_t)lrint(20000.0 * sin(2.0 * M_PI * f0 * th)), r = (int16_t)lrint(10000.0 * sin(2.0 * M_PI * f0 * th));
                pk[i] = (uint32_t)(uint16_t)l | (uint32_t)(uint16_t)r << 16;
            }
            if (!(gap && tp > 3.0 && tp < 3.2))         /* (gap: the host silent for 200 ms at 3 s) */
                uin_take(pk, f * 4u);
            tp += 0.001;
            t = tp;
        } else {                                        /* a block of 32 at the I2S clock */
            int32_t fill;
            uac_in_fill(32u);
            tb += 32.0 / fd;
            t = tb;
            blocks++;
            fill = (int32_t)(ui_w - ui_r);
            if (t > secs / 2.0 && uin_on) {
                if (fill < *fmin) *fmin = fill;
                if (fill > *fmax) *fmax = fill;
                if (!n0) { n0 = 1; *under = uin.underruns; *over = uin.overruns; dup_smin = dup_smax = uin.step; }
                if (uin.step < dup_smin) dup_smin = uin.step;
                if (uin.step > dup_smax) dup_smax = uin.step;
            }
            for (i = 0; i < 32u && uin_on; i++) {
                int32_t d = uin_l[i] - prev, a = d < 0 ? -d : d;
                if (a > *jump) *jump = a;
                prev = uin_l[i];
                if (t > secs / 2.0) { ys[ny] = uin_l[i]; ts[ny] = tb - (32.0 - i) / fd; ny++; }
            }
        }
    }
    *under = uin.underruns - *under;
    *over = uin.overruns - *over;
    /* a fitted sine at 1 kHz of real time over each 1024 samples (~23 ms: the delay, which drifts by a frame or two
     * over seconds as the step settles, is that window's own), the residuals against them, summed */
    {
        double e = 0, p = 0;
        uint32_t w0;
        for (w0 = 0; w0 + 1024u <= ny; w0 += 1024u) {
            double scc = 0, sss = 0, ssc = 0, sxc = 0, sxs = 0, det, a, b;
            for (i = w0; i < w0 + 1024u; i++) {
                double c = cos(2 * M_PI * f0 * ts[i]), s2 = sin(2 * M_PI * f0 * ts[i]);
                scc += c * c; sss += s2 * s2; ssc += s2 * c; sxc += ys[i] * c; sxs += ys[i] * s2;
            }
            det = scc * sss - ssc * ssc; a = (sxc * sss - sxs * ssc) / det; b = (sxs * scc - sxc * ssc) / det;
            for (i = w0; i < w0 + 1024u; i++) {
                double y = a * cos(2 * M_PI * f0 * ts[i]) + b * sin(2 * M_PI * f0 * ts[i]);
                e += (ys[i] - y) * (ys[i] - y);
                p += y * y;
            }
        }
        *snr = 10.0 * log10(p / (e + 1e-9));
    }
    free(ys); free(ts);
    return blocks;
}
static void test_duplex(void)
{
    const uint8_t *d; uint16_t l; uint32_t off, nif = 0, under, over;
    int ac_ok = 0, it3 = 0, ot4 = 0, asg = 0, fmt = 0, ep = 0, cs_total = 0, in_ac = 0, ac_wt = 0;
    int32_t fmin, fmax, jump;
    double snr;
    usb_duplex = 1; uin_feed = usb_duplex;
    get_desc(0x0200, &d, &l);
    check("in+out: the configuration is the duplex one, its length as declared, 4 interfaces",
          d == CFG_DESC_DUP && l == sizeof CFG_DESC_DUP && le16(d + 2) == l && d[4] == 4);
    for (off = 0; off + 1u < l; off += d[off]) {
        const uint8_t *p = d + off;
        if (p[1] == 4) { nif += p[3] == 0; in_ac = p[5] == 1 && p[6] == 1; }
        if (in_ac && p[1] == 0x24) {
            cs_total += p[0];
            if (p[2] == 1) { ac_ok = p[0] == 11 && p[7] == 3 && p[8] == 1 && p[9] == 2 && p[10] == 3; ac_wt = le16(p + 5); }
            if (p[2] == 2 && p[3] == 3) it3 = le16(p + 4) == 0x0101 && p[7] == 2;
            if (p[2] == 3 && p[3] == 4) ot4 = le16(p + 4) == 0x0301 && p[7] == 3;
        }
        if (p[1] == 0x24 && p[2] == 1 && p[0] == 7 && p[3] == 3) asg = 1;
        if (p[1] == 0x24 && p[2] == 2 && p[0] == 11) fmt = p[4] == 2 && p[5] == 2 && p[6] == 16 && p[7] == 1 && p[8] == 0x44 && p[9] == 0xAC;
        if (p[1] == 5 && p[2] == 0x04) ep = p[3] == 0x09 && le16(p + 4) == UI_MAXF * 4u && p[6] == 1;
    }
    check("in+out: AC header of 3 (MIDI, input, output), its total the class descriptors' sum", ac_ok && ac_wt == cs_total);
    check("in+out: input terminal 3 USB streaming L R -> output terminal 4 speaker, AS general on 3", it3 && ot4 && asg);
    check("in+out: type I 2 ch 16 bit 44100, EP4 OUT isochronous adaptive 184 bytes, 1 ms", fmt && ep && nif == 4u);
    get_desc(0x0100, &d, &l);
    check("in+out: the device descriptor's bcdDevice + 0x80", d == DEV_DESC_DUP && d[12] == (BCD_LO(0) + 0x80));
    dup_run(44117.6, 12, &under, &over, &fmin, &fmax, &snr, &jump, 0);
    printf("  fast clock: fill %d..%d (target %u), step %d..%d, SNR %.1f dB, largest step %d\n", fmin, fmax, UI_TARGET, dup_smin, dup_smax, snr, jump);
    check("in+out: an I2S clock 0.04 % fast: settled, no underrun or overrun, the fill near the target, step < 65536",
          !under && !over && fmin > 40 && fmax < (int32_t)UI_N - 64 && uin.step < 65536 && uin.step > 65536 - 100);
    check("in+out: a 1 kHz sine through it within -40 dB of a pure 1 kHz (no click: steps < 4000)", snr > 40.0 && jump < 4000);
    check("in+out: settled, the step steady within 0.01 % (no audible wow)", dup_smax - dup_smin <= 7);
    dup_run(44060.0, 12, &under, &over, &fmin, &fmax, &snr, &jump, 0);
    printf("  slow clock: fill %d..%d, step %d..%d, SNR %.1f dB\n", fmin, fmax, dup_smin, dup_smax, snr);
    check("in+out: an I2S clock 0.09 % slow: settled, no glitch, step > 65536", !under && !over && fmin > 40 && uin.step > 65536 && snr > 40.0);
    dup_run(44117.6, 6, &under, &over, &fmin, &fmax, &snr, &jump, 1);
    printf("  host silent 200 ms: underruns %u, starts primed again %d, largest step %d\n", uin.underruns, uin.go, jump);
    check("in+out: the host silent 200 ms: one underrun (a fade out, no click), primed again and clean after", uin.underruns == 1u && uin.go && jump < 4000);
    {   /* a new stream (uin_stream: gen + 1) with the last one's frames still in the ring: dropped, primed afresh */
        static uint32_t pk[UI_MAXF];
        uint32_t k;
        memset(pk, 0x11, sizeof pk);
        for (k = 0; k < 6u; k++) uin_take(pk, 45u * 4u);
        uin.gen++;
        uac_in_fill(32u);
        check("in+out: a new stream drops what the last one left in the ring (only the reader moves its index)", ui_r == ui_w && !uin_on && !uin.go);
    }
    {   /* (V10.57) USB > HOST: an interface at 48 kHz, read at the I2S clock from a step of 48 / 44.1 (no 44,100 to set) */
        uin.alt = 0;
        uh_aud_on = 1; uin_r48 = 1; dup_src48 = 1;
        dup_run(44117.6, 12, &under, &over, &fmin, &fmax, &snr, &jump, 0);
        printf("  48 kHz on the host port: fill %d..%d, step %d..%d, SNR %.1f dB, largest step %d\n", fmin, fmax, dup_smin, dup_smax, snr, jump);
        check("host 48 kHz: settled, no underrun or overrun, the step about 48 / 44.1 (71331)",
              !under && !over && fmin > 40 && fmax < (int32_t)UI_N - 64 && uin.step > 71331 - 700 && uin.step < 71331 + 700);
        check("host 48 kHz: a 1 kHz sine comes out a 1 kHz sine (within -40 dB), no click", snr > 40.0 && jump < 4000);
        uh_aud_on = 0; uin_r48 = 0; dup_src48 = 0;
    }
    uin.alt = 0;
    uac_in_fill(32u);
    check("in+out: alt 0: nothing in the mix", !uin_on && !uin.go);
    usb_duplex = 0; uin_feed = usb_duplex;
    get_desc(0x0200, &d, &l);
    check("in+out off: the configuration as before", d != CFG_DESC_DUP);
    usb.config = 0;
}
static void test_descriptors(void)
{
    const uint8_t *c = CFG_DESC;
    uint32_t total = le16(c + 2), n = sizeof CFG_DESC, off, nif = 0, i;
    int lens_ok = 1, eps_ok = 1, ac_ok = 0, it_ok = 0, ot_ok = 0, asg_ok = 0, fmt_ok = 0, iso_ok = 0, csep_ok = 0;
    int alt0_ok = 0, iad_ok = 1, midi_ok = 0, dup_ok = 1, coll_ok = 0;
    uint8_t seen_if[16] = {0}, if_class[16] = {0}, if_sub[16] = {0};
    int cur_if = -1, cur_alt = -1, cur_neps = 0, got_eps = 0, ac_len_left = -1, ac_total = 0, ac_wt = -1;
    uint8_t coll[8], ncoll = 0, iad_n = 0;
    uint32_t ep_seen[32] = {0};
    char name[96];

    check("device descriptor: 18 bytes, type 1, EP0 64", DEV_DESC[0] == 18 && DEV_DESC[1] == 1 && DEV_DESC[7] == 64);
    check("bcdDevice bumped for the audio input at 44.1 / 48 kHz (x.2x), 4.xx with MIDI last (V10.7.12)",
          (DEV_DESC[12] & 0xF0u) == 0x20u && DEV_DESC[13] == 4);
    check(T_CDC ? "device class misc / IAD (EF 02 01)" : "device class 0 (per interface)",
          T_CDC ? DEV_DESC[4] == 0xEF && DEV_DESC[5] == 2 && DEV_DESC[6] == 1 : DEV_DESC[4] == 0);
    check("configuration: type 2, wTotalLength = the bytes sent", c[1] == 2 && total == n);

    for (off = 0; off < n;) {
        const uint8_t *d = c + off;
        if (d[0] < 2 || off + d[0] > n) {
            lens_ok = 0;
            break;
        }
        if (ac_len_left > 0 && d[1] == 0x24) {
            ac_total += d[0];
            ac_len_left -= d[0];
        } else if (ac_len_left > 0) {
            ac_len_left = -2;                          /* the CS AC block ended early */
        }
        switch (d[1]) {
        case 0x0B:                                     /* IAD */
            iad_n++;
            if (d[0] != 8 || d[2] + d[3] > 16)
                iad_ok = 0;
            if (iad_n == 1 && !(d[2] == 0 && d[3] == 3 && d[4] == 1))
                iad_ok = 0;                            /* audio: IF 0..2 */
            if (iad_n == 2 && !(d[2] == 3 && d[3] == 2 && d[4] == 2))
                iad_ok = 0;                            /* CDC: IF 3..4 */
            break;
        case 4:
            if (cur_if >= 0 && got_eps != cur_neps)
                eps_ok = 0;
            cur_if = d[2];
            cur_alt = d[3];
            cur_neps = d[4];
            got_eps = 0;
            if (cur_if < 16) {
                if (!seen_if[cur_if])
                    nif++;
                seen_if[cur_if] = 1;
                if_class[cur_if] = d[5];
                if_sub[cur_if] = d[6];
            }
            if (d[5] == 1 && d[6] == 2 && cur_alt == 0 && d[4] == 0)
                alt0_ok = 1;
            break;
        case 5:
            got_eps++;
            if (d[2] == 0x84) {
                iso_ok = d[0] == 9 && d[3] == 0x05 && le16(d + 4) == UA_MAXF * 4u && le16(d + 4) <= 1023u &&
                         d[6] == 1 && cur_alt == 1 && if_sub[cur_if] == 2;
            }
            if ((d[2] == 0x01 || d[2] == 0x81) && d[3] == 2 && if_sub[cur_if] == 3)
                midi_ok++;
            if (ep_seen[(d[2] & 15u) + (d[2] >> 7) * 16u]++)
                dup_ok = 0;                            /* each address once (no alt shares one here) */
            break;
        case 0x24:
            if (if_class[cur_if] == 1 && if_sub[cur_if] == 1) {      /* audio control */
                if (d[2] == 1) {
                    ac_ok = d[0] == 8u + d[7] && le16(d + 3) == 0x0100u;
                    ac_len_left = ac_wt = (int)le16(d + 5);
                    ac_total = 0;
                    ac_len_left -= d[0];
                    ac_total += d[0];
                    ncoll = d[7];
                    for (i = 0; i < ncoll && i < sizeof coll; i++)
                        coll[i] = d[8 + i];
                } else if (d[2] == 2) {
                    it_ok = d[0] == 12 && d[3] == 1 && d[7] == 2 && le16(d + 8) == 3u && le16(d + 4) != 0x0101u;
                } else if (d[2] == 3) {
                    ot_ok = d[0] == 9 && d[3] == 2 && le16(d + 4) == 0x0101u && d[7] == 1;
                }
            } else if (if_class[cur_if] == 1 && if_sub[cur_if] == 2) {   /* audio streaming */
                if (d[2] == 1)
                    asg_ok = d[0] == 7 && d[3] == 2 && le16(d + 5) == 1u;
                else if (d[2] == 2)
                    fmt_ok = d[0] == 14 && d[3] == 1 && d[4] == 2 && d[5] == 2 && d[6] == 16 && d[7] == 2 &&
                             (d[8] | d[9] << 8 | d[10] << 16) == 44100 && (d[11] | d[12] << 8 | d[13] << 16) == 48000;
            }
            break;
        case 0x25:
            if (d[2] == 1 && if_sub[cur_if] == 2)
                csep_ok = d[0] == 7 && (d[3] & 1u);
            break;
        }
        off += d[0];
    }
    if (cur_if >= 0 && got_eps != cur_neps)
        eps_ok = 0;
    coll_ok = ncoll == 2;
    for (i = 0; i < ncoll && i < sizeof coll; i++)
        if (coll[i] >= 16 || if_class[coll[i]] != 1 || (if_sub[coll[i]] != 2 && if_sub[coll[i]] != 3))
            coll_ok = 0;

    check("descriptor lengths add up to wTotalLength", lens_ok && off == n);
    check("bNumInterfaces = the interfaces present", c[4] == nif && nif == (T_CDC ? 5u : 3u));
    check("each interface setting has bNumEndpoints endpoints", eps_ok);
    check("endpoint addresses unique", dup_ok);
    check("AC header: UAC 1.00, length 8 + collection", ac_ok);
    check("AC header wTotalLength = the class-specific AC descriptors", ac_len_left == 0 && ac_total == ac_wt);
    check("AC collection: the MIDI and audio streaming interfaces", coll_ok);
    check("input terminal 1: 2 ch, L R, not USB streaming", it_ok);
    check("output terminal 2: USB streaming, source 1", ot_ok);
    check("AS alt 0 has no endpoint", alt0_ok);
    check("AS general: terminal 2, PCM", asg_ok);
    check("type I format: 2 ch, 2-byte subframe, 16 bit, 44100 and 48000 Hz", fmt_ok);
    check("EP 0x84: isochronous async, 196 B (49 frames), every frame, alt 1", iso_ok && UA_MAXF == 49u);
    check("CS endpoint: sampling frequency control", csep_ok);
    check("MIDI bulk endpoints 0x01 / 0x81 unchanged", midi_ok == 2);
    snprintf(name, sizeof name, "IADs: %s", T_CDC ? "audio IF 0-2, CDC IF 3-4" : "none");
    check(name, iad_ok && iad_n == (T_CDC ? 2 : 0));
}

/* --------------------------------------------------------------------- the ring --- */
/* time in ns. The producer renders HALF_FRAMES frames at FS_DEV, a render taking `rend` ns in
 * HALF_FRAMES / 32 blocks of 32 (the samples count up: L = n, R = ~n, so the order can be checked); the consumer
 * takes one packet per ms. */
static double fs_dev = 44117.6;                       /* the I2S rate (DAC_TICKS: 24 MHz / 544) */
#define FS_DEV fs_dev
static uint32_t prod_n, cons_n, cons_bad, cons_rep, cons_zero, sizes[64], npk;
static uint32_t last_frame;
static int32_t blk[64];

static void render_block(void)
{
    uint32_t i;
    for (i = 0; i < 32u; i++, prod_n++) {
        blk[2 * i] = (int16_t)prod_n;
        blk[2 * i + 1] = (int16_t)~prod_n;
    }
    uac_tap(blk, 32);
}

static void take_packet48(void);
static void take_packet(void)
{
    uint32_t d[UA_MAXF + 2], n, i;
    if (uac.r48) {
        take_packet48();
        return;
    }
    n = uac_packet(d);
    sizes[n]++;
    npk++;
    for (i = 0; i < n; i++) {
        uint16_t l = (uint16_t)d[i], r = (uint16_t)(d[i] >> 16);
        if (d[i] == 0 && !uac.go) {
            cons_zero++;
            continue;
        }
        if ((uint16_t)~l != r) {
            if (d[i] == 0) {                           /* primed silence ahead of the first render */
                cons_zero++;
                continue;
            }
            cons_bad++;
            continue;
        }
        if (cons_n && l == (uint16_t)last_frame)
            cons_rep++;                                 /* a repeated frame (underrun) */
        else if (cons_n && l != (uint16_t)(last_frame + 1u))
            cons_bad++;
        last_frame = l;
        cons_n++;
    }
}

static void on_prime(void);
/* run for `ms`; renders take `rend_ns` (or vary with the load when 0); consumer / producer can be paused */
static uint64_t now_ns, next_half, next_pkt;
static uint32_t blocks_left;
static uint64_t next_block, rend_len;
static uint32_t lcg = 12345;
/* render times: 7 % .. 85 % of a half (the overload shedding starts above 85 %) */
#define REND_MIN ((uint64_t)(HALF_FRAMES * 1e9 / FS_DEV * 0.07))
#define REND_MAX ((uint64_t)(HALF_FRAMES * 1e9 / FS_DEV * 0.85))

static void run(uint32_t ms, uint64_t rend_ns, int produce, int consume)
{
    uint64_t end = now_ns + (uint64_t)ms * 1000000u;
    while (now_ns < end) {
        uint64_t t = end;
        if (next_half < t)
            t = next_half;
        if (blocks_left && next_block < t)
            t = next_block;
        if (next_pkt < t)
            t = next_pkt;
        now_ns = t;
        if (now_ns >= end)
            break;
        if (now_ns == next_half) {
            next_half += (uint64_t)((double)HALF_FRAMES / FS_DEV * 1e9);
            if (produce) {
                int primed = uac.go && uac.ring48 == uac.r48;
                lcg = lcg * 1103515245u + 12345u;
                rend_len = rend_ns ? rend_ns : REND_MIN + (lcg >> 8) % (REND_MAX - REND_MIN);
                uac_render_start();
                if (!primed && uac.go)
                    on_prime();
                blocks_left = HALF_FRAMES / 32u;
                next_block = now_ns + rend_len / blocks_left;
            }
        } else if (blocks_left && now_ns == next_block) {
            render_block();
            blocks_left--;
            next_block += rend_len / (HALF_FRAMES / 32u);
        } else if (now_ns == next_pkt) {
            lcg = lcg * 1103515245u + 12345u;
            next_pkt = (now_ns / 1000000u + 1u) * 1000000u + (lcg >> 8) % 500000u;   /* in the next frame */
            if (consume)
                take_packet();
        }
    }
}

static void test_ring(void)
{
    uint32_t i, sum, base_n, base_p;
    char s[120];
    usb.config = 1;
    memset(&uac, 0, sizeof uac);
    uac_stream(1);
    next_half = 300000;
    next_pkt = 1000000;
    run(3, 0, 1, 1);                                    /* alt 1 set: no IN token yet */
    check("not fed before the host reads (no overruns while it waits)", uac.overruns == 0 && ua_w == 0);
    uac.flowing = 1;                                    /* (uac_service: the first packet went) */
    run(2000, 0, 1, 1);
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    base_n = cons_n;
    base_p = uac.underruns + uac.overruns;
    run(10000, 0, 1, 1);
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    snprintf(s, sizeof s, "10 s, random render times: sizes 43..46 (44:%u 45:%u 46:%u 43:%u)", sizes[44], sizes[45],
             sizes[46], sizes[43]);
    check(s, sizes[44] + sizes[45] + sizes[46] + sizes[43] == npk);
    snprintf(s, sizeof s, "  mean packet %.4f frames (I2S %.1f Hz)", (double)sum / npk, FS_DEV);
    check(s, (double)sum / npk > 44.05 && (double)sum / npk < 44.2);
    check("  every frame once, in order; no underrun / overrun", cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p && cons_n - base_n == sum);
    snprintf(s, sizeof s, "  fill at render start stays %u..%u (band %u..%u)", uac.fill_lo, uac.fill_hi, UA_LO, UA_HI);
    check(s, uac.fill_lo >= 46u && uac.fill_hi + HALF_FRAMES <= UA_N);
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    for (i = 0; i < 1000u; i++) {                      /* 1000 frames worst case: renders of 85 % */
        run(1, REND_MAX, 1, 1);
    }
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    snprintf(s, sizeof s, "1000 packets, %.1f ms renders: mean %.3f, no glitch", REND_MAX / 1e6, (double)sum / npk);
    check(s, (double)sum / npk > 44.0 && (double)sum / npk < 44.25 && cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p);

    base_p = uac.underruns;
    run(30, 0, 0, 1);                                   /* the render stops (a crash would; a stuck ISR) */
    check("producer stopped 30 ms: underruns counted, packets keep their size", uac.underruns > base_p &&
          cons_bad == 0);
    check("  the empty ring repeats the last frame", cons_rep > 0);
    base_p = uac.overruns;
    run(40, 0, 1, 0);                                   /* the consumer stops (flowing still 1) */
    check("consumer stopped 40 ms: overruns counted, the ring never overfills", uac.overruns > base_p &&
          ua_w - ua_r <= UA_N);
    uac_stream(0);
    run(10, 0, 1, 1);
    check("alt 0: nothing fed, nothing taken", ua_w - ua_r <= UA_N && !uac.feed);
    uac_stream(1);
    uac.flowing = 1;
    cons_n = 0;
    cons_bad = cons_rep = 0;
    base_p = uac.underruns + uac.overruns;
    run(3000, 0, 1, 1);
    check("restart: primed again, in order, no glitch", cons_n > 3000u * 44u && cons_bad == 0 && cons_rep == 0 &&
          uac.underruns + uac.overruns == base_p);
}

/* ------------------------------------------------------------------ the rate --- */
static void test_rate(void)
{
    static const struct { uint32_t hz; uint8_t r48; } T[] = {
        {44100, 0}, {48000, 1}, {44100, 0}, {32000, 0}, {46049, 0}, {46050, 1}, {96000, 1}, {0, 0}};
    uint32_t i, ok = 1;
    memset(&uac, 0, sizeof uac);
    for (i = 0; i < sizeof T / sizeof T[0]; i++) {
        uac.go = 1;
        uac.pkts = 7;
        uac_rate_set(T[i].hz);
        ok &= uac.r48 == T[i].r48;
        if (i && T[i].r48 != T[i - 1].r48)
            ok &= !uac.go && !uac.pkts;                 /* a change restarts the stream */
        else if (i)
            ok &= uac.go && uac.pkts == 7;              /* the same rate again: nothing happens */
    }
    check("SET_CUR: 44100 / 48000, the nearer of the two; a change re-primes", ok);
}

/* --------------------------------------------------------------- the resampler --- */
/* the prototype computed here (as tools/gen_uac_fir.py describes it), not read from the table */
#define RL 160
#define RN 24
static double H[RL * RN];

static double bessel_i0(double x)
{
    double s = 1, t = 1;
    int k;
    for (k = 1; k < 60; k++) {
        t *= (x / (2.0 * k)) * (x / (2.0 * k));
        s += t;
    }
    return s;
}

static void proto_init(void)
{
    const int n = RL * RN;
    const double fc = 22000.0 / (44100.0 * RL), beta = 9.0;
    double sum = 0;
    int i;
    for (i = 0; i < n; i++) {
        double t = i - (n - 1) / 2.0, a = 2.0 * i / (n - 1) - 1.0, x = 2 * fc * t;
        double sinc = x == 0 ? 1.0 : sin(M_PI * x) / (M_PI * x);
        H[i] = 2 * fc * sinc * bessel_i0(beta * sqrt(1 - a * a)) / bessel_i0(beta);
        sum += H[i];
    }
    for (i = 0; i < n; i++)
        H[i] *= RL / sum;
}

/* output frame k of the double-precision resampler on input frames x(0), x(1), .. (x(< 0) = 0) */
static double ref_out(uint64_t k, double (*x)(int64_t, void *), void *ctx)
{
    uint64_t t = k * 147u;
    int64_t base = (int64_t)(t / RL);
    uint32_t p = (uint32_t)(t % RL), j;
    double y = 0;
    for (j = 0; j < RN; j++)
        if (base - (int64_t)j >= 0)
            y += H[p + RL * j] * x(base - (int64_t)j, ctx);
    return y;
}

static int32_t s16r(double v)
{
    v = floor(v + 0.5);
    return v > 32767 ? 32767 : v < -32768 ? -32768 : (int32_t)v;
}

/* the C resampler alone (uac_tap with a ring primed for 48 kHz), drained after each block */
static void rs_reset(void)
{
    memset(&uac, 0, sizeof uac);
    memset(&rs, 0, sizeof rs);
    uac_rs_init();
    usb.config = 1;
    uac.feed = 1;
    uac.ring48 = uac.r48 = 1;
    uac.go = 1;
    ua_w = ua_r = 0;
}

static uint32_t rs_block(const int32_t *in, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t k = 0;
    uac_tap(in, n);
    while (ua_r != ua_w) {
        uint32_t f = ua_ring[ua_r++ & (UA_N - 1u)];
        if (l)
            l[k] = (int16_t)f;
        if (r)
            r[k] = (int16_t)(f >> 16);
        k++;
    }
    return k;
}

static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, len;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i];
            re[i] = re[j];
            re[j] = t;
            t = im[i];
            im[i] = im[j];
            im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len, wr = cos(a), wi = sin(a);
        for (i = 0; i < n; i += len) {
            double cr = 1, ci = 0;
            for (j = 0; j < len / 2; j++) {
                uint32_t u = i + j, v = i + j + len / 2;
                double xr = re[v] * cr - im[v] * ci, xi = re[v] * ci + im[v] * cr, t;
                re[v] = re[u] - xr;
                im[v] = im[u] - xi;
                re[u] += xr;
                im[u] += xi;
                t = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = t;
            }
        }
    }
}

/* a sine of f Hz in y (48 kHz): the fitted amplitude, the SNR in band (<= 20 kHz: the residual after the fit,
 * Hann-windowed through an FFT) and over the whole band (0..24 kHz) */
#define NF 65536u
static double fre[NF], fim[NF];
static double spur_db;                                   /* (0.8.9) the largest residual bin in band, dB under the input */
static void sine_fit(const int32_t *y, double f, double *amp, double *snr_band, double *snr_full)
{
    double m[3][4], c[3], w2 = 0, pb = 0, pf = 0, ps;
    uint32_t i, j, k, kmax = (uint32_t)(20000.0 * NF / 48000.0);
    memset(m, 0, sizeof m);
    for (i = 0; i < NF; i++) {
        double v[3] = {sin(2 * M_PI * f * i / 48000.0), cos(2 * M_PI * f * i / 48000.0), 1};
        for (j = 0; j < 3; j++) {
            for (k = 0; k < 3; k++)
                m[j][k] += v[j] * v[k];
            m[j][3] += v[j] * y[i];
        }
    }
    for (i = 0; i < 3; i++)                             /* Gauss-Jordan, 3 x 3 */
        for (j = 0; j < 3; j++)
            if (j != i) {
                double q = m[j][i] / m[i][i];
                for (k = 0; k < 4; k++)
                    m[j][k] -= q * m[i][k];
            }
    for (i = 0; i < 3; i++)
        c[i] = m[i][3] / m[i][i];
    for (i = 0; i < NF; i++) {
        double e = y[i] - (c[0] * sin(2 * M_PI * f * i / 48000.0) + c[1] * cos(2 * M_PI * f * i / 48000.0) + c[2]);
        double w = 0.5 - 0.5 * cos(2 * M_PI * i / NF);
        pf += e * e;
        fre[i] = e * w;
        fim[i] = 0;
        w2 += w * w;
    }
    fft(fre, fim, NF);
    spur_db = 0;
    for (k = 0; k <= kmax; k++) {
        double p = fre[k] * fre[k] + fim[k] * fim[k];
        pb += (k ? 2.0 : 1.0) * p;
        if (k > 2u && p > spur_db)
            spur_db = p;
    }
    spur_db = 10 * log10((spur_db + 1e-30) / pow(32767.0 * pow(10, -1 / 20.0) * NF / 4.0, 2));
    pb /= (double)NF * w2;
    pf /= NF;
    *amp = sqrt(c[0] * c[0] + c[1] * c[1]);
    ps = *amp * *amp / 2;
    *snr_band = 10 * log10(ps / pb);
    *snr_full = 10 * log10(ps / pf);
}

static double sine_x(int64_t i, void *ctx)               /* the 16-bit input of the reference (see sine_run) */
{
    const double *f = ctx;
    return floor(f[1] * sin(2 * M_PI * f[0] * (double)i / 44100.0) + 0.5);
}

#define SKIP 2000u                                       /* the start (silence ahead of the sine) */
static int32_t yl[NF + SKIP + 64], yr[NF + SKIP + 64];

/* a sine at -1 dBFS on L (R: the same, inverted) through the C resampler; reference = 1: the double one */
static uint32_t sine_run(double f, int reference)
{
    double ctx[2] = {f, 32767.0 * pow(10, -1 / 20.0)};
    int32_t blk[64];
    uint32_t n = 0, i, k = 0;
    rs_reset();
    if (reference) {
        for (k = 0; k < NF + SKIP; k++)
            yl[k] = s16r(ref_out(k, sine_x, ctx));
        return k;
    }
    while (k < NF + SKIP) {
        for (i = 0; i < 32u; i++, n++) {
            blk[2 * i] = (int32_t)sine_x(n, ctx);
            blk[2 * i + 1] = -blk[2 * i];
        }
        k += rs_block(blk, 32, yl + k, yr + k);
    }
    return k;
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

static double rnd_x(int64_t i, void *ctx)                 /* input frame i: noise of period 1000 at -6 dBFS */
{
    uint32_t seed = (uint32_t)(i % 1000) * 2654435761u + *(uint32_t *)ctx;
    return (double)((int16_t)(seed >> 16) / 2);
}

/* (0.8.9) the 0.7 .. 0.8.8 resampler (uac_tap48's loop: 80 phases, 80..159 read backwards; the table's 80 phases
 * checked exactly above), the one the 160-phase RAM table and the four-tap loop must equal bit for bit */
static int16_t old_l[64], old_r[64];
static uint32_t old_w, old_ph;
static uint32_t old_block(const int32_t *in, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t i, j, k = 0, h = old_w, ph = old_ph;
    for (i = 0; i < n; i++) {
        uint32_t s = h & 31u;
        old_l[s] = old_l[s + 32u] = (int16_t)uac_s16(in[2u * i]);
        old_r[s] = old_r[s + 32u] = (int16_t)uac_s16(in[2u * i + 1u]);
        h++;
        s = (h - UAC_FIR_N) & 31u;
        do {
            int64_t al = 0, ar = 0;
            for (j = 0; j < UAC_FIR_N; j++) {
                int32_t c = ph < UAC_FIR_L / 2u ? rs_tab[ph][j] : rs_tab[UAC_FIR_L - 1u - ph][UAC_FIR_N - 1u - j];
                al += (int64_t)c * old_l[s + j];
                ar += (int64_t)c * old_r[s + j];
            }
            l[k] = uac_clip((int32_t)((al + (1 << (UAC_FIR_Q - 1))) >> UAC_FIR_Q));
            r[k++] = uac_clip((int32_t)((ar + (1 << (UAC_FIR_Q - 1))) >> UAC_FIR_Q));
            ph += UAC_FIR_M;
        } while (ph < UAC_FIR_L);
        ph -= UAC_FIR_L;
    }
    old_w = h;
    old_ph = ph;
    return k;
}

static void test_resampler(void)
{
    static const double FQ[] = {20, 100, 1000, 5000, 10000, 15000, 18000, 20000};
    double amp, sb, sf, a0 = 32767.0 * pow(10, -1 / 20.0), worst = 0, gmin = 0, gmax = -99;
    uint32_t i, k, n, bad = 0, tab_ok = 1, sum_ok = 1;
    int32_t blk[64];
    uint32_t seed = 0x1234;
    char s[160];

    proto_init();
    /* the table (0.8.9: built in RAM from the flash's differences, all 160 phases): each phase sums to 1 << 30 and is
     * the prototype computed here (oldest input first); phases 0..79 the generator's exactly (its checksum) */
    rs_reset();
    {
        uint64_t chk = 0;
        for (i = 0; i < UAC_FIR_L; i++) {
            int64_t t = 0;
            for (k = 0; k < UAC_FIR_N; k++) {
                double want = H[i + RL * (RN - 1u - k)] * (1 << 30);
                t += rs_tab[i][k];
                if (fabs(rs_tab[i][k] - want) > 2e-5 * (1 << 30))
                    tab_ok = 0;
                if (i < UAC_FIR_L / 2u)
                    chk += (uint64_t)(i * UAC_FIR_N + k + 1u) * (uint64_t)(int64_t)rs_tab[i][k];
            }
            sum_ok &= t == (int64_t)1 << 30;
        }
        check("FIR table = sinc x Kaiser (24 x 160 taps, beta 9, 22 kHz); phases sum to 1.0", tab_ok && sum_ok);
        check("  the RAM table from the flash's third differences: exactly the generator's (UAC_FIR_CHK)", chk == UAC_FIR_CHK);
    }

    /* every frame against the double-precision resampler: noise (period 1000), +-1 LSB */
    rs_reset();
    for (k = n = 0; k < 48000u;) {
        int32_t l[40], r[40];
        uint32_t m, j;
        for (i = 0; i < 32u; i++, n++) {
            blk[2 * i] = (int32_t)rnd_x(n, &seed);
            blk[2 * i + 1] = -blk[2 * i];
        }
        m = rs_block(blk, 32, l, r);
        for (j = 0; j < m; j++, k++) {
            int32_t want = s16r(ref_out(k, rnd_x, &seed));
            if (abs(l[j] - want) > 1 || abs(r[j] + want) > 1)
                bad++;
        }
    }
    snprintf(s, sizeof s, "1 s of noise: every frame = the double-precision resampler +-1 LSB (%u off)", bad);
    check(s, bad == 0);
    {   /* (0.8.9) the same frames as 0.8.8's loop, bit for bit: noise at full scale and past it (the clip), 10 s */
        uint32_t m, mo, j, nn, diff = 0, tot = 0;
        int32_t l[40], r[40], ol[40], orr[40];
        rs_reset();
        memset(old_l, 0, sizeof old_l); memset(old_r, 0, sizeof old_r); old_w = old_ph = 0;
        for (nn = 0; nn < 441000u; nn += 32u) {
            for (i = 0; i < 64u; i++) {
                seed = seed * 1664525u + 1013904223u;
                blk[i] = (int32_t)(seed >> 15) - 65536;      /* +-2 x full scale: the clip too */
            }
            m = rs_block(blk, 32, l, r);
            mo = old_block(blk, 32, ol, orr);
            diff += m != mo;
            for (j = 0; j < m && j < mo; j++, tot++)
                diff += l[j] != ol[j] || r[j] != orr[j];
        }
        snprintf(s, sizeof s, "  bit for bit the 0.8.8 resampler (flash table, phases backwards): %u frames, %u differ", tot, diff);
        check(s, diff == 0 && tot > 470000u);
    }
    snprintf(s, sizeof s, "  frames out / in: %u / %u (160 / 147)", k, n);
    check(s, k == (160u * n - 1u) / 147u + 1u);          /* the outputs whose newest input frame is < n */

    /* response and SNR (in band <= 20 kHz; full band 0..24 kHz) of sines at -1 dBFS */
    for (i = 0; i < sizeof FQ / sizeof FQ[0]; i++) {
        double rb, rf, ra, g;
        sine_run(FQ[i], 1);
        sine_fit(yl + SKIP, FQ[i], &ra, &rb, &rf);
        sine_run(FQ[i], 0);
        sine_fit(yl + SKIP, FQ[i], &amp, &sb, &sf);
        g = 20 * log10(amp / a0);
        snprintf(s, sizeof s, "  %5.0f Hz -1 dBFS: gain %+.3f dB, SNR %.1f dB in band, %.1f full (exact taps %.1f)",
                 FQ[i], g, sb, sf, rb);
        if (FQ[i] <= 18000) {
            if (g < gmin)
                gmin = g;
            if (g > gmax)
                gmax = g;
        }
        if (FQ[i] >= 100 && FQ[i] <= 15000 && (worst == 0 || sb < worst))
            worst = sb;
        if (FQ[i] == 1000)
            check(s, sb >= 90.0);
        else if (FQ[i] <= 15000)
            check(s, sb >= 88.0);
        else if (FQ[i] == 18000)
            check(s, sb >= 80.0 && g > -0.2);
        else
            check(s, g > -1.5);                         /* 20 kHz: the transition band */
    }
    snprintf(s, sizeof s, "  flat 20 Hz .. 18 kHz within %+.3f .. %+.3f dB; SNR 100 Hz .. 15 kHz >= %.1f dB", gmin,
             gmax, worst);
    check(s, gmin > -0.2 && gmax < 0.05);
    {   /* (0.8.9) THD and aliasing: the largest single spur in 20 Hz .. 20 kHz (harmonics, images folded back) under a
         * -1 dBFS sine, at the bins of a 65536-point Hann FFT; and tones above 20 kHz, whose only content in band is
         * their aliases */
        static const double FS2[] = {1000, 6000, 15000, 19000, 20500, 21500, 22000};
        double worst_s = -999;
        for (i = 0; i < sizeof FS2 / sizeof FS2[0]; i++) {
            sine_run(FS2[i], 0);
            sine_fit(yl + SKIP, FS2[i], &amp, &sb, &sf);
            if (spur_db > worst_s)
                worst_s = spur_db;
            snprintf(s, sizeof s, "  %5.0f Hz -1 dBFS: the largest spur in band %.1f dB under it", FS2[i], -spur_db);
            check(s, spur_db < -95.0);
        }
        snprintf(s, sizeof s, "  THD + aliasing: no spur above %.1f dB of the tone (the 16-bit input's own quantisation spurs)", worst_s);
        check(s, worst_s < -95.0);
    }

    /* the cost: 10 s of stereo input through uac_tap (48 kHz) */
    {
        uint64_t i0, i1, t0;
        struct timespec a, b;
        uint32_t frames_in = 441000u, out = 0;
        rs_reset();
        for (i = 0; i < 64u; i++)
            blk[i] = (int32_t)(i * 997u % 30000u) - 15000;
        i0 = instr_now();
        clock_gettime(CLOCK_MONOTONIC, &a);
        for (n = 0; n < frames_in; n += 32u) {
            uac_tap(blk, 32);
            out += ua_w - ua_r;
            ua_r = ua_w;
            __asm__ volatile("" : : "r"(blk) : "memory");
        }
        clock_gettime(CLOCK_MONOTONIC, &b);
        i1 = instr_now();
        t0 = (uint64_t)(b.tv_sec - a.tv_sec) * 1000000000u + (uint64_t)(b.tv_nsec - a.tv_nsec);
        if (i0 && i1 > i0)
            printf("  cost (host): %.0f instructions / 44.1 kHz frame (%.0f / 48 kHz frame), %.1f ns / 44.1 kHz frame\n",
                   (double)(i1 - i0) / frames_in, (double)(i1 - i0) / out, (double)t0 / frames_in);
        else
            printf("  cost (host): %.1f ns / 44.1 kHz frame (no instruction counter)\n", (double)t0 / frames_in);
    }
}

/* ------------------------------------------------------------------ the ring at 48 kHz --- */
static uint64_t k48, n0_48;
static uint32_t bad48, zero48;

static void on_prime(void)
{
    k48 = 0;
    n0_48 = prod_n;
}

static double ring_x(int64_t i, void *ctx)              /* render_block's L (R = ~L), counted from the prime */
{
    (void)ctx;
    return (double)(int16_t)(uint16_t)(n0_48 + (uint64_t)i);
}

static double ring_xr(int64_t i, void *ctx)
{
    (void)ctx;
    return (double)(int16_t)~(uint16_t)(n0_48 + (uint64_t)i);
}

static void take_packet48(void)
{
    uint32_t d[UA_MAXF + 2], i, from = uac.go && uac.ring48 == uac.r48, n = uac_packet(d);
    sizes[n]++;
    npk++;
    for (i = 0; i < n; i++) {
        int32_t l = (int16_t)d[i], r = (int16_t)(d[i] >> 16);
        if (!from || k48 < UA_PRIME48) {                /* silence: not primed yet, the primed lead */
            zero48++;
            bad48 += d[i] != 0;
            k48 += from;
            continue;
        }
        if (abs(l - s16r(ref_out(k48 - UA_PRIME48, ring_x, 0))) > 1 ||
            abs(r - s16r(ref_out(k48 - UA_PRIME48, ring_xr, 0))) > 1)
            bad48++;
        k48++;
    }
}

static void test_ring48(void)
{
    uint32_t i, sum, base_p;
    char s[160];
    double mean;
    memset(&uac, 0, sizeof uac);
    usb.config = 1;
    uac_rate_set(48000);
    uac_stream(1);
    uac.flowing = 1;
    bad48 = 0;
    run(2000, 0, 1, 1);
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    base_p = uac.underruns + uac.overruns;
    run(10000, 0, 1, 1);
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    mean = (double)sum / npk;
    snprintf(s, sizeof s, "48 kHz, 10 s, random render times: sizes 47..49 (48:%u 49:%u 47:%u), mean %.4f", sizes[48],
             sizes[49], sizes[47], mean);
    check(s, sizes[47] + sizes[48] + sizes[49] == npk && mean > 48.0 && mean < 48.05);
    snprintf(s, sizeof s, "  every frame = the resampled render, in order (%u off); no underrun / overrun", bad48);
    check(s, bad48 == 0 && uac.underruns + uac.overruns == base_p);
    snprintf(s, sizeof s, "  fill at render start %u..%u (band %u..%u), adj up %u down %u (I2S %.1f Hz: fast)",
             uac.fill_lo, uac.fill_hi, UA_LO48, UA_HI48, uac.adj_up, uac.adj_down, FS_DEV);
    check(s, uac.fill_lo >= UA_MAXF && uac.fill_hi + UA_REND48 <= UA_N && uac.adj_up > uac.adj_down);
    for (i = 0; i < 1000u; i++)
        run(1, REND_MAX, 1, 1);
    snprintf(s, sizeof s, "  1000 packets, %.1f ms renders: no glitch", REND_MAX / 1e6);
    check(s, bad48 == 0 && uac.underruns + uac.overruns == base_p);

    /* a slow I2S clock: the ring drains, packets one frame shorter now and then */
    fs_dev = 44085.0;
    uac_stream(1);
    uac.flowing = 1;
    run(1000, 0, 1, 1);
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    bad48 = 0;
    base_p = uac.underruns + uac.overruns;
    uac.adj_up = uac.adj_down = 0;
    run(10000, 0, 1, 1);
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    mean = (double)sum / npk;
    snprintf(s, sizeof s, "48 kHz, I2S %.0f Hz (slow): mean %.4f, adj up %u down %u, no glitch", FS_DEV, mean,
             uac.adj_up, uac.adj_down);
    check(s, mean < 48.0 && mean > 47.95 && uac.adj_down > uac.adj_up && bad48 == 0 &&
                 uac.underruns + uac.overruns == base_p);
    fs_dev = 44117.6;

    /* 48 -> 44.1 -> 48 while streaming */
    uac_rate_set(44100);
    cons_n = cons_bad = cons_rep = 0;
    base_p = uac.underruns + uac.overruns;
    run(3000, 0, 1, 1);
    check("switch to 44.1 mid-stream: silence, primed again, every frame in order, bit for bit",
          cons_n > 2900u * 44u && cons_bad == 0 && cons_rep == 0 && uac.underruns + uac.overruns == base_p);
    uac_rate_set(48000);
    bad48 = zero48 = 0;
    memset(sizes, 0, sizeof sizes);
    npk = 0;
    run(3000, 0, 1, 1);
    for (i = sum = 0; i < 64u; i++)
        sum += sizes[i] * i;
    check("switch to 48 mid-stream: silence, primed again, every frame in order",
          bad48 == 0 && sum - zero48 > 2900u * 48u && uac.underruns + uac.overruns == base_p);
    uac_rate_set(44100);
}

/* (0.8.0) MENU > USB STEMS+IN: both flags (the stems and the output): the 10-channel configuration with the output
 * interface (five interfaces, bcdDevice + 0xC0), input terminal 10 channels, type I 10 ch, EP4 IN 920 bytes, EP4 OUT as
 * IN+OUT's; five words a frame: the stems, then the host's audio as mixed and as it came (silence while none comes) */
static void test_stems_in(void)
{
    const uint8_t *d; uint16_t l; uint32_t off, i, n, it_ok = 0, fmt_ok = 0, epi = 0, epo = 0, nif = 0, pk[UA_MAXF * UA_W];
    t_stems = 2; usb_stems = 1; usb_duplex = 1; uin_feed = usb_duplex;
    get_desc(0x0200, &d, &l);
    check("stems+in: the 10-channel configuration with the output, its length as declared, 4 interfaces",
          d == CFG_DESC_STIN && l == sizeof CFG_DESC_STIN && le16(d + 2) == l && d[4] == 4);
    for (off = 0; off + 1u < l; off += d[off]) {
        const uint8_t *p = d + off;
        if (p[1] == 4) nif += p[3] == 0;
        if (p[1] == 0x24 && p[2] == 2 && p[0] == 12 && p[3] == 1) it_ok = p[7] == 10 && p[8] == 0 && p[9] == 0;
        if (p[1] == 0x24 && p[2] == 2 && p[0] == 11 && p[4] == 10) fmt_ok = p[5] == 2 && p[6] == 16 && p[7] == 1 && p[8] == 0x44 && p[9] == 0xAC;
        if (p[1] == 5 && p[2] == 0x84) epi = le16(p + 4) == 920 && p[3] == 0x05;
        if (p[1] == 5 && p[2] == 0x04) epo = le16(p + 4) == UI_MAXF * 4u && p[3] == 0x09;
    }
    check("stems+in: input terminal 10 ch, type I 10 ch 16 bit 44100, EP4 IN 920 bytes, EP4 OUT adaptive, 4 interfaces",
          it_ok && fmt_ok && epi && epo && nif == 4u);
    get_desc(0x0100, &d, &l);
    check("stems+in: the device descriptor's bcdDevice + 0xC0", d == DEV_DESC_STIN && d[12] == (BCD_LO(0) + 0xC0));
    memset(&uac, 0, sizeof uac);
    uac.feed = 1; uac.go = 1; uac.fill_min = UA_HI; ua_r = ua_w = 0;
    for (i = 0; i < HALF_FRAMES; i++) {
        uint32_t k;
        for (k = 0; k < 7u; k++) stem_buf[k][i] = (int32_t)(k + 1u) * 1000 + (int32_t)i;
        stem_fx[0][i] = -7000 - (int32_t)i; stem_fx[1][i] = 8000 + (int32_t)i;
        uin_l[i] = 300 + (int32_t)i; uin_r[i] = 500 + (int32_t)i;
    }
    uin_on = 1;
    uac_tap8(HALF_FRAMES);
    n = uac_packet(pk);
    check("stems+in: a block as 10 x 16-bit frames of five words: the stems, the host's audio mixed, as it came",
          n >= 43u && n <= 46u && pk[0] == (1000u | 2000u << 16) && pk[3] == ((uint32_t)(uint16_t)-7000 | 8000u << 16) &&
          pk[4] == (7000u | 400u << 16) && pk[5] == (1001u | 2001u << 16) && pk[9] == (7001u | 401u << 16));
    uin_on = 0;
    ua_r = ua_w = 0;
    uac_tap8(HALF_FRAMES);
    n = uac_packet(pk);
    check("stems+in: no host audio this block: its raw channel silent", pk[4] == 7000u && pk[9] == 7001u);
    {   /* the 409-frame ring (the 8-channel one's words, five a frame) round many times: every frame once, in order */
        uint32_t b, seq = 0, want = 0, bad = 0, j;
        memset(&uac, 0, sizeof uac);
        uac.feed = 1; uac.go = 1; uac.fill_min = UA_HI; ua_r = ua_w = 0xFFFFFF00u;   /* (near the index's wrap too) */
        for (b = 0; b < 60u; b++) {
            for (i = 0; i < HALF_FRAMES; i++) stem_buf[0][i] = (int32_t)(seq++ & 0x7FFFu);
            uac_tap8(HALF_FRAMES);
            while (ua_w - ua_r > 60u) {
                uac.fill_min = UA_HI;
                n = uac_packet(pk);
                for (j = 0; j < n; j++, want++) bad += (pk[j * 5u] & 0xFFFFu) != (want & 0x7FFFu);
            }
        }
        check("stems+in: the 409-frame ring round 18 times: every frame once and in order", !bad && want > 7000u && !uac.overruns);
    }
    t_stems = 0; usb_stems = 0; usb_duplex = 0; uin_feed = usb_duplex;
    get_desc(0x0200, &d, &l);
    check("stems+in off: the configuration as before", d != CFG_DESC_STIN && d != CFG_DESC_PLAIN8);
    memset(&uac, 0, sizeof uac);
    ua_r = ua_w = 0;
}

static void test_duplex_full_scale(void)
{
    static const int16_t pairs[][2]={{-32768,32767},{32767,-32768},{-30000,30000},{30000,-30000},{-20000,20000},{20000,-20000}};
    unsigned k,f,bad=0,maxerr=0;
    for(k=0;k<sizeof pairs/sizeof pairs[0];k++) for(f=0;f<65536;f++){
        memset(&uin,0,sizeof uin);usb.config=1;uin.alt=1;uin.go=1;uin.g=32767;uin.step=65536;uin.ph=f;
        ui_r=0;ui_w=UI_TARGET;
        ui_ring[0]=(uint16_t)pairs[k][0]|(uint32_t)(uint16_t)pairs[k][1]<<16;
        ui_ring[1]=(uint16_t)pairs[k][1]|(uint32_t)(uint16_t)pairs[k][0]<<16;
        uac_in_fill(1);
        int32_t a=pairs[k][0],b=pairs[k][1];
        int32_t exact=a+(int32_t)(((int64_t)(b-a)*f)>>16);
        exact=(exact*32767)>>15;
        unsigned err=(unsigned)abs(uin_l[0]-exact);
        int32_t right=b+(int32_t)(((int64_t)(a-b)*f)>>16);
        right=(right*32767)>>15;
        unsigned er=(unsigned)abs(uin_r[0]-right);
        if(er>err)err=er;
        if(err>maxerr)maxerr=err;
        if(err>1){if(bad<3)printf("FAIL a=%d b=%d fraction=%u output=%d expected=%d\n",a,b,f,uin_l[0],exact);bad++;}
    }
    printf("INPUT full-scale interpolation: %u wrong samples, max error %u LSB\n",bad,maxerr);
    check("input: full-scale interpolation stays within 1 LSB on both channels", bad==0);
}

int main(void)
{
    printf("-- USB audio input, CDC %s\n", T_CDC ? "on" : "off");
    test_descriptors();
    test_stems();
    test_duplex();
    test_duplex_full_scale();
    test_stems_in();
    test_ring();
    test_rate();
    test_resampler();
    test_ring48();
    printf(fails ? "UAC TEST FAILED (%d)\n" : "uac: all ok\n", fails);
    return fails != 0;
}

