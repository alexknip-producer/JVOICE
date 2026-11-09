/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA boot and main loop. Boot order: WDT first, boot-loop guard, fatal vectors,
 * guards; then LCD, input (TIMER5 IRQ, 10 kHz), audio (ALNK0 IRQ). */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];
#if FELUCCA_DUAL_CORE
extern uint32_t _c1t_start[], _c1t_end[], _c1t_load[];   /* (0.6.71) the second core's loop (app.ld .c1_text) */
extern uint32_t _hot_start[], _hot_end[], _hot_load[];   /* (0.6.78) core 0's hottest audio code (app.ld .hot_text) */
extern uint32_t _low_start[], _low_end[];                /* (0.6.78) main-loop buffers below the RAM code (app.ld) */
#endif
extern char _etext[];                                    /* (V10.6z5) .text's end: their load images from here, */
#include "boot_lz4.h"                                    /* packed (tools/build.py, app.ld PACKED) */


/* TIMER5 outranks ALNK0, so the scan keeps its 100 us pace while a half buffer renders: before,
 * the ticks stopped for the whole render (0.7 ms idle, several ms loaded), the column lit when it
 * began stayed lit that long (a ~16 Hz flicker over all LEDs, beating with the scan) and the
 * encoders lost frames. Nested in ALNK0 it only scans (GPIO + fm1_in, nothing the audio ISR touches)
 * and counts ms; USB and UART polls wait for the first tick after the render, as they always did,
 * and the time spent nested is handed to the audio ISR so its load figures stay render-only.
 * The USB audio stream cannot wait for the render (one packet per 1 ms frame, a render takes up to
 * ~5 ms): uac_service and EP4 OUT reception also run nested. They touch only EP4 (INDEX is set on every access) and the
 * consumer side of the audio ring, and usb_poll never runs nested, so the two never interleave.
 * (0.9.10) uac_service runs on every tick, nested or not (in usb_poll on its ticks), not every 5th: a
 * stems packet is on the wire up to ~0.65 ms, the next must be queued in the ~0.35 ms after it (usb.c). */
void fm1_timer5_irq(void)
{
    static uint32_t sub, owed;
    uint32_t t0 = fm1_ticks(), usb_due = sub % 5u == 0u;
    fm1_timer5_ack();
    felucca_dbg.timer_irqs++;
    fm1_input_tick();
    {   /* milliseconds from the 24 MHz TIMER4 (robust to a late tick) */
        static uint32_t last, acc;
        acc += t0 - last;
        last = t0;
        while (acc >= 1000u * FM1_TICKS_PER_US) {
            acc -= 1000u * FM1_TICKS_PER_US;
            fm1_ms++;
        }
    }
    if (usb_due)
        owed |= 1u;                             /* 2 kHz: all USB SIE traffic lives here */
#if FELUCCA_UART
    if (sub % 5u == 2u)
        owed |= 2u;                             /* 2 kHz: <= ~7 bytes per call at 31250 baud */
#endif
    if (++sub == 10u)
        sub = 0;
    if (felucca_dbg.in_audio) {
        felucca_dbg.nested++;
#if FELUCCA_UAC
        uac_service();
        /* The host's OUT packets cannot wait for a render either. Only EP4 RX,
         * on its 2 kHz slot; full USB/control/MIDI polling stays outside audio. */
        if (usb_due && usb.up && usb.config && uin.alt)
            uin_rx();
#endif
        t5_nested_ticks += fm1_ticks() - t0;
        return;
    }
    if (owed & 1u)
        usb_poll();                             /* (uac_service at its end) */
#if FELUCCA_UAC
    else
        uac_service();
#endif
#if FELUCCA_UART
    if (owed & 2u)
        uart_midi_poll();
#endif
    owed = 0;
}
extern void isr_timer5(void);

static void timer5_start(void)                 /* OSC /4 = 6 MHz, PRD 600 -> 10 kHz */
{
    fm1_timer5_start(isr_timer5, 4);   /* above ALNK0 (3): the scan nests into the render (see fm1_timer5_irq) */
}

static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}

/* (0.9.9-stab) the crash screen, labelled (every number in hex): the firmware and the core that faulted of how many ran;
 * VEC PC EMU DBG RETS as every crash screen since Felucca (in this order, now named); then (0.9.19's record,
 * hal/fm1_irq.h) SP USP SSP (which stack the main loop runs on: crash audit, cause 6), C2 PC where the second core was
 * (when it ran), and the debug unit's culprits: WR / RD the bus master of a bad write / read (AXI_WR_INV_ID / RD), P the
 * DMA that wrote into a closed window (PRP_WR_LIMIT_ID), N the windows hit by core 0 / core 1 / the DMA
 * (_WR_LIMIT_ERR_NUM). 10 s, then a reset (was 4: time for a photo); MENU > ABOUT repeats the main numbers until the
 * unit is switched off */
#if FELUCCA_DUAL_CORE
/* (p1, main 0.9.12) the crash's CR_* flags for the next boot (cpu_boot); out of fm1_fault (its room before the sound's
 * first pinned run is full) */
static __attribute__((noinline)) void fault_flags(uint32_t two)
{
    uint32_t fl = (fm1_clk_320() ? CR_320 : 0u) | (two ? CR_2C : 0u);
    fm1_mc_bar.barred = (fm1_mc_bar.barred & ~0xFF00u) | fl << 8;
}
#endif
/* the screen's last two lines (the bus culprits; p1: the short code, cpu_safe.c crash_code), 10 s, the reset */
static __attribute__((noinline)) void fault_tail(const fm1_crash_t *c, char *b)
{
    uint32_t t0, i;
    str_cpy(b, "WR ", 4); hexs(b + 3, c->bus[5]);
    str_cpy(b + 11, " RD ", 5); hexs(b + 15, c->bus[4]);
    draw_text_line(10, 48 + 17 * 9, 220, &AF_S, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    str_cpy(b, "P ", 3); hexs(b + 2, c->bus[3]);
    str_cpy(b + 10, " N ", 4);
    for (i = 0; i < 3u; i++) {                        /* (a window's number, 0..15, a digit each; '-' past 9) */
        uint32_t n = c->bus[i] & 15u;
        b[13 + i] = n < 10u ? (char)('0' + n) : '-';
    }
    str_cpy(b + 16, "  ", 3);                         /* (p1, main 0.9.12) the short code (the cause, then the vector), as
                                                       * MENU > ABOUT > LAST CRASH keeps it: "IL-01" */
    crash_code(b + 18, c->emu, c->dbg, 0);
    b[20] = '-';
    hexs(b + 21, c->vec);
    b[21] = b[27];
    b[22] = b[28];
    b[23] = 0;
    draw_text_line(10, 48 + 17 * 10, 220, &AF_S, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 10000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}
static void fm1_fault(const fm1_crash_t *c)
{
    static const char L[9][6] = {"VEC ", "PC ", "EMU ", "DBG ", "RETS ", "SP ", "USP ", "SSP ", "C2 PC "};
    const uint32_t v[9] = {c->vec, c->pc, c->emu, c->dbg, c->rets, c->sp, c->usp, c->ssp, c->other_pc};
    char b[40];
    uint32_t i, two = 0;
#if FELUCCA_DUAL_CORE
    if (fm1_mc_on) {
        fm1_mc_bar_set();                             /* (0.6.71) a crash beside the second core: one core until power-off */
        two = 1;
    }
    fault_flags(two);
    fm1_mc_hold();
#endif
    fm1_crash.app = (felucca_dbg.in_audio ? 1u : 0u) | (uint32_t)felucca_dbg.stage << 8 | (uint32_t)felucca_dbg.page << 16;
    fm1_audio_stop();
    scr_wake_now();                                   /* (MENU > SCREEN OFF: the panel on first) */
    lcd_fill(0, 0, 240, 240, UI_CRASH_BG);            /* fixed, outside the palettes */
    draw_text_line(0, 6, 240, &AF_M, "JIANT CRASH", UI_CRASH_INK, UI_CRASH_BG, 1);
    str_cpy(b, FELUCCA_VERSION "  CORE ", 24);
    i = str_len(b);
    b[i] = (char)('0' + (c->cnum & 1u));
    str_cpy(b + i + 1u, two ? " OF 2" : " OF 1", 8);
    draw_text_line(10, 30, 220, &AF_S, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    for (i = 0; i < (c->other ? 9u : 8u); i++) {
        str_cpy(b, L[i], 7);
        hexs(b + str_len(b), v[i]);
        draw_text_line(10, 48 + 17 * i, 220, &AF_S, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    }
    fault_tail(c, b);
}

/* power-on: the parts with their power-on sounds (V10.6d: T1 DRUM-X, T2 ANALOG, T3 PHASE, T4 FM6, T5 VOICE, T6 ANIMAL,
 * engines.c TRK_DEF; as INIT SYSTEM: ui.c power_sound_of); the sequencers empty. RESTORE LAST ON brings the last session back after (autosave_boot) */
static void felucca_init(void)
{
    uint32_t i;
    if (song_idx.magic == SONG_MAGIC && song_cur < NSONG && chain_valid(&song_idx.rows[song_cur]))
        chain_config = song_idx.rows[song_cur];       /* (JIANT 0.6.81) the song's rows as persist_boot read them (it
                                                       * runs first): reset here, the main loop then saved the empty
                                                       * rows over them, the ARRANGE lost at every power-on */
    else
        chain_defaults(&chain_config);
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    sing_init();                              /* (0.9.0) VOICE's English generator words, resolved once (sing.c) */
    load_depth++;                             /* (the power-on loads: one outer load) */
    fm6_init();                               /* every track's FM6 patch: the init voice */
    power_on_parts();                         /* (0.9.9: ui.c, TRK_DEF's sounds; the tests run it too) */
    load_depth--;
    song.sel = 0;
    song.master_q12 = 2048;                   /* (the pot's level replaces it before the audio starts: master_boot) */
    ui.home = 1;
    ui.force = 1;
}

/* The power-on LED sweep (1.1, hal/fm1_led_anim.h: a soft light over the keys left to right, then the buttons swell
 * and settle on the glow; ~0.70 s), started before the scan and run by it: nothing waits for it, the splash's ~2 s (JIANT 0.5: the signature written, then held)
 * and the first ~0.27 s of the UI go by under it (the UI's LEDs from its end, or at once on a key or a button down).
 * Every LEDS setting: it is no idle glow; with DIM HI / DIM LO it ends on that glow (and is drawn over that glow
 * level), with OFF / INV dark. Not with MENU > ANIM OFF (no motion: the LEDs as the UI has them at once), not after a crash (a crash record newer than the last boot saw: the crash screen's
 * reset), a hang (a WDT reset) or a failed boot (bootguard: the last one died within its first 30 s): straight to the
 * UI. The UBOOT paths never get here. */
static uint32_t crash_seen __attribute__((section(".noinit")));   /* fm1_crash.count at the last boot */
/* (V10.46) the supply setup's boot guard (hal/fm1_power.h, since V10.11): PW_CR_START while it runs and until the UI
 * is up; a reset before that (the watchdog of a hang: .noinit kept) finds it set and the next start skips the setup
 * (PW_CR_SKIP: the boot loader's supplies, as JIANT up to V10.10), and keeps skipping it until the power goes off. A
 * power-off loses .noinit: the next power-on tries again */
#define PW_CR_START 0x5057A001u
#define PW_CR_SKIP 0x5057A002u
static uint32_t pw_crumb __attribute__((section(".noinit.zbt")));
static uint8_t boot_clean;                    /* no crash, hang (WDT) or failed boot before this one (boot_leds):
                                               * the sweep runs and the autosave is restored (project.c autosave_boot) */
static void boot_leds(void)
{
    uint32_t crashed = fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != crash_seen;
    crash_seen = fm1_crash.count;
    boot_clean = !crashed && !bootguard.failed && !(fm1_boot.p3_rst & 4u);
    fm1_led_dim_level(settings_leds == LEDS_DIM_LO);
    if (boot_clean && !(ui_prefs & PREF_ANIM_OFF))
        fm1_led_anim_start(settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO);
}

/* (p1, main 0.9.12 / 0.9.13) what the last run left (cpu_safe.c): a crash (a record newer than the last boot saw,
 * boot_leds) or a hang (a watchdog reset: the main loop's breadcrumbs in PC / RETS, felucca_dbg) is logged into the
 * settings (MENU > ABOUT LAST CRASH, the editor's backup: kept across power-off); with both cores at work then, MENU >
 * SYSTEM > CPU CORES goes to 1 CORE for three power-ons, a second time for good. 1: that (the UI says so at its start) */
static __attribute__((noinline)) uint32_t cpu_boot(void)
{
    uint32_t fl = 0, cfg = cpu_cfg;
#if FELUCCA_DUAL_CORE
    uint32_t bar = fm1_mc_bar.barred;
    fm1_mc_bar.barred = bar & ~0xFF02u;                 /* (this run's own from here) */
    if (fm1_crash.magic == FM1_CRASH_MAGIC && fm1_crash.count != crash_seen) {
        fl = (bar >> 8) & 0xFFu;
        crash_log_put(fl, fm1_crash.vec, fm1_crash.pc, fm1_crash.rets, fm1_crash.emu, fm1_crash.dbg);
    } else if (fm1_boot.p3_rst & 4u) {
        fl = CR_WDT | (bar & 2u ? CR_2C : 0u) | (fm1_clk_320() ? CR_320 : 0u);
        if (fl & CR_2C)
            fm1_mc_bar_set();                           /* (a hang beside the second core: as a crash, fm1_fault) */
        crash_log_put(fl, 0xFFu, felucca_dbg.prev_stage, felucca_dbg.prev_page, 0, 0);
    } else {
        cpu_cfg = (uint8_t)cpu_retry(cfg);              /* (0.9.13) a clean power-on: after a crash, both cores again
                                                         * on the third */
        if (cpu_cfg != cfg)
            settings_save();
        return 0;
    }
    cpu_cfg = (uint8_t)cpu_after(cfg, fl);
    settings_save();
    return 0;                                           /* (p2b: a crash no longer turns 2 CORES off) */
#else
    (void)fl; (void)cfg;
    return 0;
#endif
}

/* (0.9.9-stab, as 0.9.12) MENU > SYSTEM > CPU CLOCK 320 MHZ: from the boot stage's 360 to JieLi's rated 320 (hal/fm1_clk320.h,
 * curist's, as Skiff runs it), the screen up, on one core, before the scan, the second core, the audio and USB. Not after
 * a failed boot, nor again until power-off after a reset in it (fm1_cstart); a soft reset keeps the PLL (the check:
 * already 320). Registers not as the boot stage leaves them: not switched (ABOUT: 360 MHZ). The default stays 360 MHZ:
 * with the core rails at V15's levels (fm1_rails_boot) that is M-VAVE's own configuration */
static __attribute__((noinline)) void cpu_clock(void)
{
#if FELUCCA_DUAL_CORE
    fm1_clk320_t k;
    uint32_t f;
    if (!(cpu_cfg & CPU_320) || bootguard.failed || (fm1_mc_bar.barred & (FM1_MC_CLK_ARMED | FM1_MC_CLK_TRIP)))
        return;
    lcd_sync();                                         /* (no LCD DMA through it) */
    fm1_mc_bar.barred |= FM1_MC_CLK_ARMED;
    fm1_wdt_feed();
    f = irq_save();
    if (!fm1_clk320_check(&k))
        FL_FAR(fm1_clk320_switch)(&k);
    irq_restore(f);
    fm1_mc_bar.barred &= ~FM1_MC_CLK_ARMED;
#endif
}

/* the debug record at boot (its breadcrumbs: a watchdog reset's), then (p1) cpu_boot; out of fm1_main (its room is
 * pinned: tools/audio_layout.txt) */
static __attribute__((noinline)) uint32_t dbg_boot(void)
{
    if (felucca_dbg.magic != DBG_MAGIC) {
        memset(&felucca_dbg, 0, sizeof felucca_dbg);
        felucca_dbg.magic = DBG_MAGIC;
    }
    felucca_dbg.boots++;
    felucca_dbg.max_us = 0;
    felucca_dbg.in_audio = 0;                       /* .noinit: a reset inside the audio ISR left it set, and
                                                       TIMER5 would treat every tick as nested (no USB poll) */
    felucca_dbg.prev_stage = felucca_dbg.stage;     /* a WDT reset leaves the last breadcrumb here */
    felucca_dbg.prev_page = felucca_dbg.page;
    felucca_dbg.prev_home = felucca_dbg.home;
    felucca_dbg.prev_frames = felucca_dbg.ui_frames;
    felucca_dbg.prev_rst = fm1_boot.p3_rst;
    return cpu_boot();
}
#if FELUCCA_DUAL_CORE
/* (p1, main 0.9.12) the second core given up while it plays (a misread, no answer, a fault): said once (p2b: one core
 * until the power-off, not for good) */
static __attribute__((noinline)) uint32_t cpu_lost(uint32_t said)
{
    if (fm1_mc_lost && said != 2u) {
        said = 2;
        cpu_off("CORE 2 FAILED: 1 CORE");
    }
    return said;
}
#endif
/* the splash, its signature written (JIANT 0.5: was 400 ms); the pot followed (MIDI IN plays under it). (0.5.2) On the
 * clock: drawing the beings does not make it longer */
static __attribute__((noinline)) void boot_splash(void)
{
    uint32_t t0 = fm1_ms, n, ms;
    for (n = 0; n < 600u && (ms = fm1_ms - t0) < SPL_WRITE + SPL_HOLD; n++) {   /* (600: a stalled clock still ends it,
                                                                               * in ~2.4 s) */
        splash_write(ms);
        fm1_delay_ms(4);
        master_poll();
    }
}
static void fm1_main(void)
{
    uint32_t said;
    persist_boot();
#if FELUCCA_OTA
    if (flash_ok)
        ota_boot_cleanup();                             /* staging area left by an update */
#endif
    settings_init();
    prefs_update();                                     /* (JIANT 0.6.44) FX LATCH ON by default, once */
    usb_serial_apply();                                 /* (#67: the saved USB SERIAL before usb_start) */
    lcd_init();
    spl_anim = 1;                                       /* (JIANT 0.5) the signature written: splash_write */
    draw_splash();                                      /* ui_draw.c (1.1.5) */
    said = dbg_boot();                                  /* (p1) the breadcrumbs; the last run's crash or hang kept, CPU
                                                         * CORES after it */
    cpu_clock();                                        /* (0.9.9-stab) MENU > SYSTEM > CPU CLOCK 320 MHZ, if set */
    fm1_input_init();
    fm1_adc_init();
    panel_init();
#if FELUCCA_BT
    ly_en_ok = (uint8_t)bt_hw_seg();                    /* (V10.15) the segment checked (the plain XIP window set: its
                                                         * interrupts-off moment under the splash) before sing_init reads
                                                         * VOICE's English dictionary there (sing.c) */
#endif
    felucca_init();
    master_boot();
    kb_boot_hold = 1;                         /* #137: no notes from the keys before the UI (seq.c keyboard_block) */
    audio_init();
    usb_start();
#if FELUCCA_UART
    uart_midi_init();
#endif
    boot_leds();
    timer5_start();
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    fm1_delay_ms(30);
    if ((fm1_in.buttons & 3u) == 3u) {
        panel_setup();                        /* OCT- + OCT+ held at power-on */
        settings_save();
    }
#if FELUCCA_DUAL_CORE
    /* (0.6.71) the second core, unless REC is held at power-on (one core this session: mc_off), a boot failed (a second
     * would enter UBOOT), or it was given up / beside a crash since power-on (hal/fm1_multicore.h fm1_mc_bar). Its entry
     * word is in the protected top of RAM: opened for it, interrupts off (up to 20 ms: under the splash) */
    mc_off = (uint8_t)((fm1_in.buttons >> panel.btn[B_REC]) & 1u);
    if (!mc_off && !bootguard.failed && !fm1_mc_barred() && !(cpu_cfg & CPU_1CORE) && fm1_power_ready) {
                                                /* (p1: MENU > CPU CORES) (V10.6z) and only on the stock supplies
                                                 * (fm1_power_init read back; not after a SAFE START): with the radio
                                                 * on and only the two core levels raised, some units' second core
                                                 * jumps to addresses with bits 23/24 set (fm1-dock src/cpu1.c) */
        fm1_irq_off();
        fm1_guard_unlock_top();
        fm1_mc_start();
        fm1_guard_lock_top();
        fm1_irq_on();
    }
#endif
    autosave_boot(boot_clean);                /* (1.2) the last session's music, under the splash */
    session_dice(fm1_ticks() ^ (uint32_t)fm1_adc_read(FM1_ADC_BATT) << 16);   /* (JIANT 0.4) this session's macros */
    boot_splash();                            /* (p1: out of line, fm1_main's room is pinned) */
    kb_boot_hold = 0;
    lcd_fill(0, 0, 240, 240, T_BG);
    scrn.idle = fm1_ms;                        /* (MENU > SCREEN OFF: from here) */
    if (said)                                  /* (p1) cpu_boot */
        cpu_off("2 CORES OFF AFTER A CRASH");
    if (pw_crumb == PW_CR_SKIP) ui_message("SAFE START: POWER SETUP SKIPPED");
    else pw_crumb = 0;                         /* (V10.46) the UI is up: the supply setup went well */
    bt_boot(boot_clean);                       /* (V10.12) BLUETOOTH's boot guard (bt.c) */

    for (;;) {
        uint32_t m = fm1_ms;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
#if FELUCCA_USB_HOST
        if (uh.want && !bt_hw_seg()) {                  /* (V10.41) the host's code is in the Bluetooth segment */
            uh.want = 0;
            usb_host_want = 0;
            seg_missing();                              /* (V10.7.10: was "USB HOST: INSTALL AGAIN (NO SEG)") */
        } else if (uh.want || uh.on)
            uh_task(fm1_ms);                            /* (V10.41) USB > HOST: the keyboard found, kept, lost */
        if (uh.msg) {
            static const char *const UHM[8] = {"", "USB HOST: PLUG A MIDI KEYBOARD", "USB HOST OFF", "USB MIDI KEYBOARD READY",
                                               "NOT USB MIDI OR AUDIO 16BIT 44.1-48K", "LOW SPEED: NOT SUPPORTED", "USB DEVICE LOST",
                                               "USB AUDIO IN READY: USE INPUT"};   /* (V10.53; V10.56: an INPUT track takes it) */
            ui_message(UHM[uh.msg % 8u]);
            uh.msg = 0;
        }
#endif
        if (fm1_ms > 30000u && bootguard.pending) {     /* a crash or hang in the first 30 s counts */
            bootguard.pending = 0;
            bootguard.failed = 0;
        }
        {
            int32_t b = fm1_adc_read(FM1_ADC_BATT);     /* battery: slow IIR */
            if (b > 0)
                song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
        }
        master_poll();
#ifdef VD_NK
        vd_poll();                                      /* (V10.6z5) the velocity diagnostic's last press, when on */
        vel_poll();                                     /* (V10.6z6) key velocity and pressure: the settings, the sampling */
#endif
#if FELUCCA_DUAL_CORE
        said = cpu_lost(said);                          /* (p1) the second core given up: 1 CORE for good */
#endif
        {   /* OCT- + OCT+ held 5 s: enter UBOOT with RAM intact (debug / update); a countdown
             * shows from 2 s over the whole screen (the menu and the dialogs too: ui_draw), letting
             * go cancels it */
            static uint32_t t0;
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) != both || kx_g[0] || kx_g[1]) {   /* (V10.6z4: on a note, the expression keys) */
                if (ui.uboot) {
                    ui.uboot = 0;
                    ui.force = 1;                       /* what the countdown covered */
                    ui_message("UPDATE CANCELLED");
                }
                t0 = fm1_ms;
            } else if (fm1_ms - t0 > 2000u && fm1_ms - t0 <= 5000u) {
                uint32_t left = (5000u - (fm1_ms - t0) + 999u) / 1000u;
                if (left != ui.uboot) {
                    ui.uboot = (uint8_t)left;
                    ui.force = 1;
                }
            } else if (fm1_ms - t0 > 5000u) {
                fm1_audio_stop();
                scr_wake_now();
                lcd_fill(0, 0, 240, 240, T_BG);
                draw_text_box(0, 110, 240, &AF_M, "UBOOT", T_THEME, 1);
                usb_detach();
                fm1_delay_ms(30);
                bootguard.pending = 0;                  /* intentional reset: not a failed boot */
                fm1_enter_uboot();
            }
        }
#if FELUCCA_OTA
        ed_service();                                   /* web editor SysEx */
        ota_service();                                  /* M-UPGRADE handshake */
        if (usb.ota_req) {                              /* M-UPGRADE upgrade command */
            usb.ota_req = 0;
            panic_req = (1u << NTRK) - 1u;               /* every track (bit per track) */
            scr_wake_now();                             /* (its progress on the screen) */
            if (flash_ok)
                ota_session();                          /* returns only if nothing was committed */
            lcd_fill(0, 0, 240, 240, T_BG);
            ui.force = 1;
        }
#endif
        if (usb.uboot_req) {                            /* SysEx F0 22 24 35 7D F7 from the host */
            fm1_audio_stop();
            scr_wake_now();
            lcd_fill(0, 0, 240, 240, T_BG);
            draw_text_box(0, 110, 240, &AF_M, "UBOOT (USB)", T_THEME, 1);
            fm1_delay_ms(20);
            usb_detach();
            fm1_delay_ms(30);
            bootguard.pending = 0;
            fm1_enter_uboot();
        }
#if FELUCCA_CDC
        cdc_task();
#endif
        felucca_dbg.ui_frames++;
        felucca_dbg.page = ui.page;
        felucca_dbg.home = ui.home;
        felucca_dbg.stage = 1;
        ui_input();
        settings_poll();                              /* queued settings save: only while stopped */
        bt_tick();                                    /* (V10.12) BLUETOOTH ON saved: the radio, once stopped (bt.c) */
        autosave_poll();                              /* (1.2) the music, stopped and idle: project.c */
        song_poll();                                  /* the song: the next section staged, the music back after it */
        felucca_dbg.stage = 2;
        ui_leds();
        ui_hold_poll();                               /* (0.9.9-beta4) playing past 60 % of the render: the UI held (ui.c) */
        ui_draw();
        felucca_dbg.stage = 9;
        while (fm1_ms - m < (ui_hold ? 50u : 15u)) {             /* ~60 UI frames/s at most; held: 20 */
            ui_input();
#if FELUCCA_OTA
            ed_service();                       /* editor replies without waiting for the next frame */
#endif
        }
    }
}

void fm1_cstart(void)
{
    uint32_t *d, p3, src, wdt;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    fm1_core1_stop();                           /* (0.6.71) a second core a reset left running would read the RAM
                                                 * cleared below */
    if (bootguard.magic != BOOTGUARD_MAGIC) {
        bootguard.magic = BOOTGUARD_MAGIC;
        bootguard.failed = 0;
        bootguard.pending = 0;
    }
#if FELUCCA_DUAL_CORE
    if (fm1_mc_bar.magic != FM1_MC_BAR_MAGIC) {         /* (0.9.9-stab, as 0.9.12: its other bits too; garbage after a power-off) */
        fm1_mc_bar.magic = FM1_MC_BAR_MAGIC;
        fm1_mc_bar.barred = 0;
    }
    if (fm1_mc_bar.barred & FM1_MC_CLK_ARMED) {         /* a reset in the 320 MHz switch (fm1_main cpu_clock): no failed
                                                         * boot, and no switch again until power-off */
        fm1_mc_bar.barred ^= FM1_MC_CLK_ARMED | FM1_MC_CLK_TRIP;
        bootguard.pending = 0;
    }
#endif
    if (bootguard.pending)
        bootguard.failed++;
    bootguard.pending = 1;
    if (bootguard.failed >= 2u) {
        bootguard.failed = 0;
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (d = _low_start; d < _low_end; d++)
        *d = 0;
    {                                           /* (V10.6z5) unpacked, not copied (boot_lz4.h): from _etext on (the */
        const uint8_t *z = (const uint8_t *)_etext;     /* image's end), one LZ4 block each, in this order (build.py) */
        z = boot_lz4(z, (uint8_t *)_rt_start, (uint8_t *)_rt_end);     /* flash driver code that must run from RAM */
#if FELUCCA_DUAL_CORE                                                  /* (0.6.71) the second core's waiting loop */
        z = boot_lz4(z, (uint8_t *)_c1t_start, (uint8_t *)_c1t_end);   /* (hal/fm1_multicore.h; without it no block) */
#endif
        z = boot_lz4(z, (uint8_t *)_hot_start, (uint8_t *)_hot_end);   /* (0.6.78) core 0's hottest audio code */
        boot_lz4(z, (uint8_t *)_data_start, (uint8_t *)_data_end);
    }
    if (pw_crumb == PW_CR_START || pw_crumb == PW_CR_SKIP)
        pw_crumb = PW_CR_SKIP;                  /* (V10.46) the last start hung here or before the UI: not again */
    else {
        pw_crumb = PW_CR_START;
        fm1_power_ready = (uint8_t)fm1_power_init();
    }
                                                /* (V10.11) the supplies as stock V15 sets them (the DC-DC on:
                                                 * hal/fm1_power.h, Melodee's sequence), the radio's prerequisite */
    fm1_rails_boot();                           /* (0.9.9-stab) the core supplies to M-VAVE's own levels (VDC14 1.60 V,
                                                 * SYSVDD 1.38 V, hal/fm1_sys.h) before the second core and the audio:
                                                 * up to here the chip ran on the boot loader's 1.26 V at 360 MHz */
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    fm1_main();
    for (;;)
        ;
}

