/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.12) MENU > SYSTEM > BLUETOOTH OFF / ON: Bluetooth LE MIDI, as a peripheral that advertises "JIANT FM-1" with the
 * BLE-MIDI service (phones and computers list it as a MIDI device); docs/BLE_FEASIBILITY.md stage 2. MIDI goes
 * (V10.13) both ways: what a central sends plays as USB-MIDI does (notes, CC, PC, bend, the clock and START / STOP,
 * no SysEx); MIDI OUT's messages (MENU > CONTROL > MIDI OUT: KEYS CLOCK ALL OFF) go to it too while it listens.
 *   OFF (the default)  the radio is never started
 *   ON                 started at once if the music is stopped (else "STOP FIRST", the row stays OFF), or at power-on
 *                      when ON was saved; the sound is quiet for the start (~0.3 s: the interrupts off while the radio
 *                      calibrates itself). Its value then reads ADV (advertising) or CONN (a central connected), with
 *                      the connections since power-on after it; MISSING (V10.7.10; was NO SEG): the Bluetooth segment at
 *                      0xEA000 is not this build's (the update loader writes it with the app: install the package
 *                      again; other flashing tools write the app alone), said as "BLUETOOTH PART MISSING", then
 *                      "REINSTALL: JIANT INSTALLER" (ui.c seg_missing; GEN, the songs' saving, USB HOST alike); the radio's
 *                      start-up returned unfinished: "BLUETOOTH FAILED TO START", OFF (no retry until the next power-on)
 *   OFF again          advertising stops and a link is closed; the radio stays set up (no second calibration)
 * The stack and the radio drivers live in the segment (firmware/bt/bt_seg.c, app.ld BTSEG), called through bt_hw.c only
 * after the segment's CRC-32 matched this build's.
 * Boot guard: a word in .noinit (bt_crumb) says the radio's start is running; a reset in it (a hang: the watchdog, or a
 * crash) leaves it set, and the next power-on turns BLUETOOTH OFF, saves that, and says so. A crash or a hang while it
 * ran (bt_crumb BT_RUN) does the same.
 * The settings bytes: favorites.factory[6][24..31] (TRIO's retired favourites row: never selectable, nothing reads
 * its stars; its presets were < 64, so bytes 8.. were always 0): [24] BT_MARK (else none: OFF, no address), [25] bit 0
 * ON, [26..31] the static random address (LSB first, generated once at the first start, top bits 11); (V10.14) [22] PROX's FAR,
 * [23] its NEAR (bt_prox_step). */
#define BT_ROW (&favorites.factory[6][24])
#define BT_MARK 0xB7u
enum { BTS_NONE, BT_RUN, BT_FAIL, BT_NOSEG };
enum { BT_CR_START = 0xB7570001u, BT_CR_RUN = 0xB7570002u };
static uint8_t bt_up;                                   /* BT_*: this power-on's radio */
static uint32_t bt_crumb __attribute__((section(".noinit.zbt")));   /* (after every other .noinit: none moves) */
#if FELUCCA_BT
static int bt_hw_start(uint8_t addr[6]);                /* bt_hw.c: 0 started, 1 the radio's start-up failed, 2 no segment */
static void bt_hw_enable(int on);
static uint32_t bt_hw_status(void);
static uint32_t bt_hw_rssi(uint16_t *w, uint32_t *rd);   /* bt_hw.c: the RSSI words since *rd (V10.14) */
static uint32_t bt_hw_rxdiag(void);                     /* bt_hw.c: (V10.19) RSSI hook / good packets, 16 bits each */
/* (V10.13) code and constants of the app's own that live in the segment (app.ld .bt: flash 0xEA000, outside the full
 * app slot), cold and never needed by the sound: read or called only once bt_hw_seg() said the segment is this build's
 * (its CRC-32 covers them), or from the segment itself; without it they are missing (MENU > ABOUT's credits), never
 * read */
/* (BTAPP_RO / BTAPP_FN and bt_hw_seg's prototype: core.h, V10.60) */
#else
static int bt_hw_start(uint8_t addr[6]) { (void)addr; return 2; }   /* (the host, the emulator: no radio) */
static void bt_hw_enable(int on) { (void)on; }
static uint32_t bt_hw_status(void) { return 0; }
static uint32_t bt_hw_rssi(uint16_t *w, uint32_t *rd) { (void)w, (void)rd; return 0; }
static uint32_t bt_hw_rxdiag(void) { return 0; }
#endif
#if FELUCCA_BT
/* (V10.13) a BLE-MIDI message received (bt_seg.c ble_app_midi_in: channel messages, the clock, START CONTINUE STOP) into
 * the MIDI input ring as USB's (source 1: CLK USB follows its clock; LEARN, ROUT, MIDI MAP alike). The BLE interrupts
 * (priority 2) are preempted by TIMER5, the ring's other producer (USB, TRS): the interrupts off around it (core 1 takes
 * none). In the segment: only its interrupts call it */
BTAPP_FN void bt_midi_rx(uint32_t pkt)
{
    uint32_t ie = fm1_icfg() & 0x200u;
    fm1_irq_off();
    midi_enqueue(pkt, 1u);                              /* (full: midi_in_overflow, every note let go, as USB / TRS) */
    if (ie)
        fm1_irq_on();
}
#endif
/* 0 OFF, 1 ON, (V10.35) 2 AIR: [25] bit 0 the radio on, bit 1 AIR (the scanner: no MIDI, every advertisement heard
 * around feeds PROX) */
static uint32_t bt_get(void) { return BT_ROW[0] == BT_MARK && (BT_ROW[1] & 1u) ? 1u + (BT_ROW[1] >> 1 & 1u) : 0u; }
static void bt_set(uint32_t on)
{
    uint8_t *r = BT_ROW;
    if (r[0] != BT_MARK) {
        memset(r, 0, 8);
        r[0] = BT_MARK;
    }
    r[1] = (uint8_t)((r[1] & ~3u) | (on ? 1u | (on == 2u) << 1 : 0u));
}
static void bt_start(void)
{
    int rc;
    if (bt_up == BT_RUN) {
        bt_hw_enable((int)bt_get());
        bt_crumb = BT_CR_RUN;
        return;
    }
    if (bt_up)                                          /* (failed or missing this power-on: not again) */
        return;
    bt_crumb = BT_CR_START;
    rc = bt_hw_start(BT_ROW + 2);
    bt_crumb = rc ? 0u : BT_CR_RUN;
    if (!rc && bt_get() == 2u)
        bt_hw_enable(2);                                /* (V10.35) AIR: the scanner instead of the advertising */
    bt_up = (uint8_t)(rc == 2 ? BT_NOSEG : rc ? BT_FAIL : BT_RUN);
    if (rc == 2)
        seg_missing();                                  /* (V10.7.10: what to do, not NO SEG alone) */
    if (rc == 1) {                                      /* (the start-up returned unfinished: OFF, not again at power-on) */
        bt_set(0);
        ui_message("BLUETOOTH FAILED TO START");
    }
    settings_save();                                    /* (the address, made at the first start) */
}
static void bt_put(uint32_t v)
{
    if (v && song.playing) {
        ui_message("STOP FIRST");
        return;
    }
    bt_set(v);
    if (v && bt_up == BT_NOSEG)
        seg_missing();                                  /* (V10.7.10: turned on again, the segment still missing) */
    if (v)
        bt_start();
    else if (bt_up == BT_RUN) {
        bt_hw_enable(0);
        bt_crumb = 0;
    }
}
static char bt_val[12];
static const char *bt_vname(uint32_t v)
{
    uint32_t st = bt_hw_status(), n = st >> 8 & 255u;
    if (!v)
        return "OFF";
    if (bt_up != BT_RUN)
        return bt_up == BT_NOSEG ? "MISSING" : bt_up == BT_FAIL ? "FAILED" : "ON";   /* (V10.7.10: was NO SEG; REINSTALL
                                                         * is wider than a card's value, 54 px of 49) */
    str_cpy(bt_val, v == 2u ? "AIR" : st & 1u ? "CONN" : "ADV", sizeof bt_val);
    if (v == 2u)
        return bt_val;
    if (n) {
        str_cpy(bt_val + str_len(bt_val), " ", 2);
        fmt_int(bt_val + str_len(bt_val), (int32_t)n);
    }
    return bt_val;
}
/* power-on (fm1_main, after the splash: the last run's crash or hang known, boot_clean): the guard, then the saved ON
 * starts from bt_tick */
static void bt_boot(uint32_t clean)
{
    bt_hw_seg();                                        /* (V10.13) the segment checked here, the music stopped: the plain
                                                         * window's set-up has the interrupts off a moment (ABOUT's texts
                                                         * there) */
    if (bt_crumb == BT_CR_START || (bt_crumb == BT_CR_RUN && !clean)) {
        if (bt_get()) {
            bt_set(0);
            settings_save();
        }
        ui_message(bt_crumb == BT_CR_START ? "BLUETOOTH OFF: START FAILED" : "BLUETOOTH OFF AFTER A CRASH");
    }
    bt_crumb = 0;
}
/* (V10.14) PROX, the proximity source (mod.c MS_PROX, mod_prox 0..32767): the RSSI of every good packet on the link
 * (bt_seg.c, 89..133 a second while a central is connected) as a level, (w & 63) + (w >> 8 & 63) (0..126: the stack's
 * stand-in, ble_scan_strength; larger is taken as weaker, [I]: the gain table is unknown), the median of the last five,
 * then one pole over ~6 packets (~50-70 ms); mapped by the two points the user stores on MENU > SYSTEM > BLUETOOTH's
 * page (ui_menu.c bt_prox_panel: OCT- FAR, the hand away; OCT+ NEAR, the hand on the back over the antenna, top left)
 * to 0..1, clamped, whichever way the level goes. No packets (a flash write): held. No central, BLUETOOTH OFF: back to 0
 * (31/32 every 10 ms, ~0.3 s); never started: 0. The points: favorites.factory[6][22] FAR, [23] NEAR (TRIO's retired
 * row, always 0 before: equal, not calibrated, PROX 0), the level x 2. All of it in the segment (BTAPP_FN): run only
 * while the radio runs (bt_up BT_RUN: the segment checked) */
#define BT_CAL (&favorites.factory[6][22])
static struct {
    uint32_t rd, t;                                     /* the segment's ring read so far; the last 10 ms step */
    int32_t lvl;                                        /* the level, Q8 */
    uint16_t raw;                                       /* the last RSSI word */
    uint8_t win[9], wn, have, conn;                     /* the median's last nine (wn 0..17); a level; a central */
    uint16_t w4[4];                                     /* (V10.18) the packet's four RSSI words, last */
    uint8_t mn[12], mx[12], src, lock, seen;            /* (V10.18) each reading's range on this link; the one used; kept
                                                         * (a point stored); a link seen */
    int32_t alo, ahi;                                   /* (V10.6z6) SWARM's range of the level, not calibrated (Q8) */
    int32_t hl;                                         /* (V10.6z8) AIR: the level PROX maps, a level's backlash (Q8) */
} bp;
/* (V10.61, the user's ask: a narrower field, PROX let go without taking the phone far away) thresholds over the
 * calibrated FAR .. NEAR, in 5 % steps: [19] MIN 0..18 (0 .. 90 %: under it PROX is 0, so a phone a little away
 * already lets go), [20] MAX as 100 % - 5 % x [20] (0..18: over it PROX is full), always 10 % over MIN; [21] SPEED
 * 0..4: the level's smoothing 1/32 (as before, ~0.3 s) 1/16 1/8 1/4 1/2 (the fastest, the noisiest). 0 0 0 as V10.60 */
#define BT_TUNE (&favorites.factory[6][19])
#define BT_MIN() ((int32_t)(BT_TUNE[0] > 18u ? 0u : BT_TUNE[0]) * 5)
#define BT_MAX() (100 - (int32_t)(BT_TUNE[1] > 18u ? 0u : BT_TUNE[1]) * 5)
#define BT_SPEED() ((uint32_t)(BT_TUNE[2] > 4u ? 0u : BT_TUNE[2]))
BTAPP_FN static int32_t bt_prox_map(int32_t lvl)
{
    int32_t f = (int32_t)BT_CAL[0] << 7, n = (int32_t)BT_CAL[1] << 7, lo = BT_MIN(), hi = BT_MAX(), p;
    if (f == n)
        return 0;
    if (hi < lo + 10)
        hi = lo + 10;
    p = clamp((lvl - f) * 32767 / (n - f), 0, 32767);
    return clamp((p - 32767 * lo / 100) * 100 / (hi - lo), 0, 32767);
}
/* (V10.6z6) SWARM's closeness (eng_input.c sw_pxv): a central's link (ON, connected; not AIR's scan), as PROX maps it;
 * the points not stored, the level's own range (its lowest and highest, each relaxing towards it, ~10 s), larger nearer.
 * The points too, for AIR's devices */
BTAPP_FN static void bt_sw_prox(uint32_t conn)
{
    int32_t l = bp.lvl;
    sw_pxf = (int16_t)(BT_CAL[0] << 7);
    sw_pxn = (int16_t)(BT_CAL[1] << 7);
    if (!conn || !bp.have || bt_get() != 1u) {
        sw_pxon = 0;
        return;
    }
    if (!sw_pxon)
        bp.alo = bp.ahi = l;
    bp.alo = l < bp.alo ? l : bp.alo + ((l - bp.alo) >> 10);
    bp.ahi = l > bp.ahi ? l : bp.ahi - ((bp.ahi - l) >> 10);
    sw_pxv = (int16_t)(BT_CAL[0] != BT_CAL[1] ? mod_prox : clamp((l - bp.alo) * 32767 / (bp.ahi - bp.alo > 1536 ? bp.ahi - bp.alo : 1536), 0, 32767));
    sw_pxon = 1;
}
/* (V10.6z8, a Discord user: AIR's FAR / NEAR "jump chaotically") AIR's level (advertisements, every device's, far
 * noisier than a central's link): smoothed twice as long and, before PROX maps it, held within one level of it (a
 * backlash: the noise left under a level moves nothing) */
BTAPP_FN static void bt_prox_step(const uint16_t *w, uint32_t n, uint32_t conn, uint32_t now)
{
    uint32_t i, j, k, m[9], air = bt_get() == 2u;
    if (!conn)
        bp.have = bp.wn = 0;
    bp.conn = (uint8_t)conn;
    for (i = 0; conn && i < n; i++) {
        bp.raw = w[i];
        bp.win[bp.wn % 9u] = (uint8_t)((w[i] & 63u) + (w[i] >> 8 & 63u));
        bp.wn = (uint8_t)(bp.wn < 17u ? bp.wn + 1u : 9u);
        k = bp.wn < 9u ? bp.wn : 9u;
        for (j = 0; j < k; j++) {                       /* (sorted in) */
            uint32_t x = bp.win[j], q = j;
            for (; q && m[q - 1u] > x; q--)
                m[q] = m[q - 1u];
            m[q] = x;
        }
        bp.lvl = bp.have ? bp.lvl + ((int32_t)(m[k / 2u] << 8) - bp.lvl) / ((32 << 2 * air) >> BT_SPEED()) : (int32_t)(m[k / 2u] << 8);   /* (V10.20:
                                                         * the median of nine, ~32 packets: ~0.3 s; was five, ~6: on
                                                         * the hardware a random sample and hold, the channels' hops) */
        bp.have = 1;
    }
    if (conn && bp.have) {
        bp.hl = air ? clamp(bp.hl, bp.lvl - 384, bp.lvl + 384) : bp.lvl;
        mod_prox = (int16_t)bt_prox_map(bp.hl);
        bp.t = now;
        if (n)
            bt_sw_prox(conn);                           /* (V10.6z6: SWARM's, a packet at a time) */
        return;
    }
    bt_sw_prox(0);
    if (now - bp.t > 1000u)
        bp.t = now - 1000u;
    for (; now - bp.t >= 10u; bp.t += 10u)
        mod_prox = (int16_t)(mod_prox - (mod_prox + (31 >> BT_SPEED())) / (32 >> BT_SPEED()));
}
/* (V10.18) twelve readings of a packet: each of its four words as V10.14's level ((w & 63) + (w >> 8 & 63)), its low
 * byte / 2, its high byte / 2. (V10.20) The one used is fixed again: word 0 (RSSI2 / 3 by the buffer) as the level, the
 * gain index and the raw value together. V10.18 took the reading whose range grew widest, which on the hardware was a
 * lone byte jumping at random (the noisiest, not the one that follows the distance); V10.17's still page was only the
 * redraw (V10.19). The four words stay on the page */
BTAPP_FN static uint32_t bt_prox_rd(const uint16_t *q, uint32_t c)
{
    uint32_t w = q[c / 3u];
    return c % 3u == 0u ? (w & 63u) + (w >> 8 & 63u) : c % 3u == 1u ? (w & 255u) >> 1 : w >> 9;   /* (0..127) */
}
BTAPP_FN static void bt_prox_feed(const uint16_t *w, uint32_t n, uint32_t conn, uint32_t now)   /* n packets of 4 */
{
    uint16_t v[16];
    uint32_t i, c, best;
    n = n > 16u ? 16u : n;
    if (!conn)
        bp.seen = 0;
    for (i = 0; conn && i < n; i++) {
        (void)best;                                     /* (V10.20: the reading fixed, see above) */
        bp.seen = 1;
        c = bt_prox_rd(&w[4u * i], bp.src);
        c = c > 126u ? 126u : c;                        /* (as bt_prox_step reads it: two halves of 0..63) */
        v[i] = (uint16_t)(c / 2u | (c - c / 2u) << 8);
        for (c = 0; c < 4u; c++)
            bp.w4[c] = w[4u * i + c];
    }
    bt_prox_step(v, conn ? n : 0u, conn, now);
    if (n && conn)
        bp.raw = bp.w4[bp.src / 3u];
}
BTAPP_FN static void bt_prox_tick(void)
{
    uint16_t w[64];
    uint32_t n = bt_hw_rssi(w, &bp.rd);
    bt_prox_feed(w, n, bt_get() && (bt_hw_status() & 5u), fm1_ms);   /* (a central's link, or AIR's scan) */
}
static uint32_t bt_prox_ok(void) { return bt_up == BT_RUN && bt_get(); }   /* (the PROX page opens: ui_menu.c) */
static void bt_tick(void)                               /* main loop: the saved ON, once the music is stopped */
{
    if (!bt_up && bt_get() && !song.playing && fm1_ms > 3000u)
        bt_start();
    if (bt_up == BT_RUN)
        bt_prox_tick();                                 /* (V10.14) PROX */
}
