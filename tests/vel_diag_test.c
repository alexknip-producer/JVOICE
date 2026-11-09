/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z5) Host test of the velocity diagnostic (firmware/src/vel_diag.c) and of its sampling in the key scan
 * (hal/fm1_input.h fm1_input_tick, fm1__an_take / fm1__an_start; hal/fm1_adc.h), on synthetic ADC traces. The GPIO and
 * the converter are memory here; a conversion started on a tick is "done" at the next with the reading of the model
 * key under it (pulled up, or driven when its row's pin is an output driven high). Checked:
 *   - off (the default): the tick never touches the converter or the drive registers, the rows stay inputs (its only
 *     difference from V10.6z4's tick: two tests of fm1_an.on), fm1_adc_read converts as before;
 *   - on: the slots: in one frame PA7's eleven columns, in the next PA8's nine note columns, the MASTER pot (column 0,
 *     OCT-) and the battery (column 1, OCT+), every key of the two rows once every two frames; fm1_adc_read hands the
 *     pot and the battery to the main loop from those slots (it starts no conversion of its own); the MASTER follows
 *     the knob;
 *   - a driven sample only for a touched key, its row an input with its pull-up again before the rows are read (the
 *     digital scan sees what it saw), the strength in HD0 / HD and cleared after;
 *   - soft / normal / hard presses on F3, C4 and G4 (the model's driven reading 700 / 560 / 420, its fall 3..12 ms):
 *     one press each, its key, touch -> press within a sample period of the model's fall, the lowest driven and
 *     pulled-up readings exact; a graze (never under PRESS) reported as one; drive 0: no driven sample;
 *   - the capture of the first key touched: 8 samples before the touch (open), 48 from it, driven ones flagged and
 *     alternating while held; a second key pressed meanwhile reported, not captured; a full queue counted as lost;
 *   - off again: the scan lets go at its next tick (no conversion pending, the rows inputs).
 *   cc -O1 -Ibuild/gen -Ifirmware/src tests/vel_diag_test.c -o build/vel_diag_test */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
static uint32_t host_now;                          /* TIMER4 ticks (24 MHz) */
#define FM1_INPUT_NOW() host_now
#include "../firmware/hal/fm1_time.h"
#include "../firmware/hal/fm1_gpio.h"
static volatile uint32_t host_reg[16][64];         /* the GPIO registers, as memory */
static uint32_t host_rows_bad;                     /* rows read while one of PA7 / PA8 was not an input */
static volatile uint32_t *host_pr(uint32_t p, uint32_t r)
{
    if (p == FM1_PA && r == FM1_IN && (~host_reg[FM1_PA][FM1_DIR / 4u] & ((1u << 7) | (1u << 8))))
        host_rows_bad++;
    return &host_reg[p & 15u][(r / 4u) & 63u];
}
#undef FM1_PR
#define FM1_PR(p, r) (*host_pr((p), (r)))
static uint32_t host_ticks(void) { return host_now; }
#define fm1_ticks host_ticks
/* the converter as memory: CON / RES, the writes counted */
static uint32_t adc_con, adc_res, adc_wla, adc_writes, adc_instant;
static uint32_t *adc_reg(void)                     /* (adc_instant: a conversion done at once, the main loop's reads) */
{
    adc_writes++;
    if (adc_instant && (adc_con & 0x10u))
        adc_con |= 0x80u;
    return &adc_con;
}
#define FM1_ADC_CON (*adc_reg())
#define FM1_ADC_RES adc_res
#define FM1_WLA_CON0 adc_wla
static void vd_sample(uint32_t col, uint32_t rowbit, uint32_t v, uint32_t driven);
static int vd_drive(uint32_t col, uint32_t rowbit);
#define FM1_INPUT_ANALOG(col, rowbit, v, driven) vd_sample(col, rowbit, v, driven)
#define FM1_INPUT_DRIVE(col, rowbit) vd_drive(col, rowbit)
#include "../firmware/hal/fm1_input.h"
static unsigned str_len(const char *s) { unsigned n = 0; while (s[n]) n++; return n; }
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    for (; max && i + 1u < max && s[i]; i++)
        d[i] = s[i];
    if (max)
        d[i] = 0;
}
static char host_msg[48];
static void ui_message(const char *s) { str_cpy(host_msg, s, sizeof host_msg); }
#include "../firmware/src/vel_diag.c"
/* (V10.6z6) the keys' velocity (vel.c), off here (vk_on 0: the diagnostic alone, as V10.6z5): its main-loop needs */
static struct { uint8_t factory[16][32]; } favorites;
#define ENGI_SAMPLE 4u
static volatile uint32_t fm1_ms;
static void settings_save(void) {}
static uint32_t kb_drum_keys(void) { return 0; }   /* (V10.7.10: seq.c's) */
#include "../firmware/src/vel.c"

static int fails;
static int check(const char *what, int ok)
{
    printf("vel_diag: %-96s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
    return ok;
}

/* ---- the model: each key's pulled-up and driven readings at time t (us) */
typedef struct { int on; uint32_t t0, fall, hold; uint32_t pseat, dseat, graze; } press_t;
static press_t model[VD_NK];
static uint32_t pot = 700, batt = 620, sim_us;
static uint32_t model_v(uint32_t key, uint32_t t, int driven)
{
    const press_t *m = &model[key];
    uint32_t lo, x;
    if (!m->on || t < m->t0 || t >= m->t0 + m->fall + m->hold)
        return 1023;
    lo = m->graze ? m->graze : driven ? m->dseat : m->pseat;
    if (t >= m->t0 + m->fall)
        return lo;
    x = t - m->t0;                                  /* a straight fall from 880 (just touched) to the seat */
    return 880u - (880u - lo) * x / m->fall;
}
/* the conversion started on the last tick, done now: the reading under it */
static uint32_t conv_n, conv_ch[64], conv_col[64], driven_n;
static void adc_finish(void)
{
    uint32_t ch = (adc_con >> 8) & 15u, col = fm1_an.col, rb = fm1_an.rowbit, v = 0;
    if (!(adc_con & 0x10u))
        return;                                     /* (none started) */
    if (conv_n < 64u) {
        conv_ch[conv_n] = ch;
        conv_col[conv_n] = col;
    }
    conv_n++;
    if (ch == FM1_ADC_MASTER)
        v = pot;
    else if (ch == FM1_ADC_BATT)
        v = batt;
    else {
        uint32_t key = vd_note(col, rb), m = 1u << (rb == 3u ? 7 : 8);
        int driven = !(host_reg[FM1_PA][FM1_DIR / 4u] & m) && (host_reg[FM1_PA][FM1_OUT / 4u] & m);
        driven_n += driven;
        v = key < VD_NK ? model_v(key, sim_us, driven) : 1023u;
    }
    adc_res = v;
    adc_con |= 0x80u;
}
static void tick(void)
{
    adc_finish();
    sim_us += 100u;
    host_now = sim_us * 24u;
    fm1_input_tick();
}
static void run_ms(uint32_t ms)
{
    uint32_t i;
    for (i = 0; i < ms * 10u; i++)
        tick();
}
static void poll_all(vd_ev_t *ev, uint32_t *nev, vd_cap_t *cap, uint32_t *ncap)
{
    vd_cap_t c;
    vd_ev_t e;
    uint32_t k;
    while ((k = vd_next(&c, &e)) != 0u) {
        if (k == 1u && *nev < 64u) ev[(*nev)++] = e;
        if (k == 2u && *ncap < 4u) cap[(*ncap)++] = c;
    }
}
static void press(uint32_t key, uint32_t at_ms, uint32_t fall_ms10, uint32_t hold_ms, uint32_t pseat, uint32_t dseat)
{
    press_t *m = &model[key];
    m->on = 1;
    m->t0 = at_ms * 1000u;
    m->fall = fall_ms10 * 100u;
    m->hold = hold_ms * 1000u;
    m->pseat = pseat;
    m->dseat = dseat;
    m->graze = 0;
}

int main(void)
{
    uint32_t i, j;
    vd_ev_t ev[64];
    vd_cap_t cap[4];
    uint32_t nev = 0, ncap = 0;
    host_reg[FM1_PA][FM1_IN / 4u] = host_reg[FM1_PB][FM1_IN / 4u] = 0xFFFFFFFFu;   /* every row open */
    host_reg[FM1_PA][FM1_DIR / 4u] = 0xFFFFFFFFu;                                    /* inputs (fm1_input_init) */
    check("the notes on PA8 / PA7: F3 col 3, C4 col 9 (PA8), G4 col 5 (PA7); OCT- / OCT+ and PA6 none",
          vd_note(3, 4) == 0 && vd_note(9, 4) == 7 && vd_note(5, 3) == 14 && vd_note(0, 4) == VD_NK &&
          vd_note(1, 4) == VD_NK && vd_note(0, 2) == VD_NK && vd_note(10, 3) == 19);

    /* ---- off: no converter, no drive, the same GPIO writes as the tick without the diagnostic */
    {
        uint32_t hd0 = host_reg[FM1_PA][FM1_HD0 / 4u], hd = host_reg[FM1_PA][FM1_HD / 4u];
        adc_writes = 0;
        for (i = 0; i < 2200u; i++)
            tick();
        check("off: 2200 ticks, no converter access (fm1_an.on 0 by default)", adc_writes == 0 && !fm1_an.on);
        check("off: the drive registers untouched, the rows inputs", host_reg[FM1_PA][FM1_HD0 / 4u] == hd0 &&
              host_reg[FM1_PA][FM1_HD / 4u] == hd && !host_rows_bad);
        adc_instant = 1;
        adc_res = 512u;
        check("off: fm1_adc_read converts itself (the main loop's, as before)", fm1_adc_read(FM1_ADC_MASTER) == 512 &&
              adc_writes > 0);
        adc_instant = 0;
    }

    /* ---- on: the slots, MASTER and the battery */
    vd_set(1, 2);
    adc_writes = 0;
    check("on: fm1_adc_read hands -1 until the scan's first readings", fm1_adc_read(FM1_ADC_MASTER) == -1 &&
          adc_writes == 0);
    while (fm1__tick_col != 10u)
        tick();                                     /* (to a frame's start) */
    conv_n = 0;
    for (i = 0; i < 2u * FM1_NCOL; i++)
        tick();
    {
        uint32_t pa7 = 0, pa8 = 0, mst = 0, bat = 0, cols7 = 0, cols8 = 0;
        for (i = 0; i < conv_n && i < 64u; i++) {
            if (conv_ch[i] == FM1_ADC_ROW_PA7) pa7++, cols7 |= 1u << conv_col[i];
            if (conv_ch[i] == FM1_ADC_ROW_PA8) pa8++, cols8 |= 1u << conv_col[i];
            if (conv_ch[i] == FM1_ADC_MASTER) mst += conv_col[i] == 0u;
            if (conv_ch[i] == FM1_ADC_BATT) bat += conv_col[i] == 1u;
        }
        check("on: two frames, a conversion each tick: PA7 x 11 (every column), PA8 x 9 (columns 2..10), MASTER at "
              "column 0, the battery at column 1", conv_n == 22u && pa7 == 11u && pa8 == 9u && mst == 1u && bat == 1u &&
              cols7 == 0x7FFu && cols8 == 0x7FCu);
    }
    check("on: fm1_adc_read: the pot and the battery from their slots, no conversion of its own",
          (adc_writes = 0, fm1_adc_read(FM1_ADC_MASTER) == 700 && fm1_adc_read(FM1_ADC_BATT) == 620 && adc_writes == 0));
    pot = 300;
    run_ms(5);
    check("on: the MASTER knob turned: fm1_adc_read follows it within ~2.2 ms", fm1_adc_read(FM1_ADC_MASTER) == 300);
    check("on: nothing touched: no drive, no press", driven_n == 0 && vd_ev_r == vd_ev_w && !vd_cready);

    /* ---- presses: soft / normal / hard on F3, C4, G4, 10 each, one at a time */
    {
        static const uint32_t KEY[3] = {0, 7, 14}, DS[3] = {700, 560, 420}, PS[3] = {240, 205, 186}, FALL[3] = {120, 60, 30};
        uint32_t t = sim_us / 1000u + 10u, ok_n = 1, ok_dt = 1, ok_v = 1, n = 0;
        nev = ncap = 0;
        for (j = 0; j < 30u; j++) {
            uint32_t k = KEY[j % 3u], f = j / 10u;
            press(k, t, FALL[f], 80u + 7u * j, PS[f], DS[f]);
            run_ms(80u + 7u * j + FALL[f] / 10u + 40u);
            poll_all(ev, &nev, cap, &ncap);
            if (nev != ++n) { ok_n = 0; break; }
            {
                const vd_ev_t *e = &ev[nev - 1u];
                /* touch -> press: the model crosses 400 at fall x (880-400)/(880-seat); the samples 2.2 ms apart, 4.4
                 * after the touch (the next is driven) */
                uint32_t want = FALL[f] * (880u - 400u) / (880u - PS[f]);
                if (e->key != k || e->dt == VD_NONE || e->dt + 23u < want || e->dt > want + 45u) ok_dt = 0;
                if (e->dmin != DS[f] || e->pmin != PS[f]) ok_v = 0;
            }
            model[k].on = 0;
            t = sim_us / 1000u + 20u;
        }
        check("30 presses (10 soft, 10 normal, 10 hard on F3 C4 G4): one report each", ok_n && nev == 30u);
        check("each: its key, touch -> press within the sampling's step (-2.2 .. +4.4 ms) of the model's", ok_dt);
        check("each: the lowest driven reading (700 / 560 / 420) and the lowest pulled-up (240 / 205 / 186) exact", ok_v);
        check("soft > normal > hard in the driven reading by 140 counts each: what the diagnostic is for",
              ev[0].dmin > ev[10].dmin && ev[10].dmin > ev[20].dmin);
        check("a capture of each press played alone (not read in time: the rest counted lost)", ncap >= 1u);
        if (ncap) {
            const vd_cap_t *c = &cap[0];
            uint32_t pre_open = 1, drv = 0, alt = 1;
            for (i = 0; i < VD_PRE; i++) pre_open &= c->v[i] == 1023u;
            for (i = VD_PRE; i < VD_PRE + 36u; i++) drv += (c->v[i] & VD_DRIVEN) != 0u;
            for (i = VD_PRE + 1u; i + 1u < VD_PRE + 36u; i++)
                if (((c->v[i] & VD_DRIVEN) != 0u) == ((c->v[i + 1u] & VD_DRIVEN) != 0u)) alt = 0;
            check("the capture: F3, 8 samples before the touch all open, 48 from it", c->key == 0 && c->n == VD_CAP &&
                  pre_open && !(c->v[VD_PRE] & VD_DRIVEN) && c->v[VD_PRE] < VD_TOUCH);
            check("the capture: driven and pulled-up samples alternate from the touch on while the key is held", alt &&
                  drv == 18u);
        }
        check("the message line: the last press (\"VEL G4 D420 P186 ...MS\")",
              (vd_poll(), !strncmp(host_msg, "VEL G4 D420 P186 ", 17)) && strstr(host_msg, "MS"));
        check("driven samples were taken; every row read with PA7 / PA8 inputs again (the digital scan as before)",
              driven_n > 0 && !host_rows_bad);
        check("the drive (Velo's DRIVE 2, strength 1): HD0 / HD cleared once the row is let go",
              !(host_reg[FM1_PA][FM1_HD0 / 4u] & ((1u << 7) | (1u << 8))) && !(host_reg[FM1_PA][FM1_HD / 4u] & ((1u << 7) | (1u << 8))));
    }

    /* ---- a graze, a chord, drive 0 */
    {
        uint32_t t = sim_us / 1000u + 10u;
        nev = ncap = 0;
        press(14, t, 50, 60, 0, 0);
        model[14].graze = 650;                      /* touched, never under PRESS */
        run_ms(120);
        poll_all(ev, &nev, cap, &ncap);
        check("a graze (to 650 and back): reported as one (GRAZE, no touch -> press)",
              nev == 1u && ev[0].key == 14 && ev[0].dt == VD_NONE && ev[0].pmin == 650u &&
              (vd_line(host_msg, &ev[0]), strstr(host_msg, "GRAZE") != NULL));
        model[14].on = 0;
        nev = ncap = 0;
        t = sim_us / 1000u + 10u;
        press(9, t, 40, 200, 200, 500);             /* D4 first: captured */
        press(19, t + 30u, 40, 150, 210, 600);      /* C5 during it */
        run_ms(300);
        poll_all(ev, &nev, cap, &ncap);
        check("a chord: both pressed reported, the first one's capture only", nev == 2u && ncap == 1u &&
              cap[0].key == 9 && ((ev[0].key == 9 && ev[1].key == 19) || (ev[0].key == 19 && ev[1].key == 9)));
        model[9].on = model[19].on = 0;
        vd_set(1, 0);
        driven_n = 0;
        nev = ncap = 0;
        press(3, sim_us / 1000u + 10u, 40, 100, 220, 500);
        run_ms(200);
        poll_all(ev, &nev, cap, &ncap);
        check("drive 0: no driven sample, the press with no driven reading (D-)", driven_n == 0 && nev == 1u &&
              ev[0].dmin == VD_NONE && ev[0].pmin == 220u);
        model[3].on = 0;
        vd_set(1, 2);
    }

    /* ---- nobody reading: the queue full, counted */
    {
        uint32_t lost0 = vd_lost;
        for (j = 0; j < 12u; j++) {
            press(j, sim_us / 1000u + 5u, 30, 30, 200, 500);
            run_ms(80);
            model[j].on = 0;
        }
        check("twelve presses unread: eight kept, the rest and the captures counted lost", (uint8_t)(vd_ev_w - vd_ev_r) == VD_NEV &&
              vd_lost >= lost0 + 4u);
        poll_all(ev, &nev, cap, &ncap);
    }

    /* ---- off again */
    vd_set(0, 0);
    tick();
    adc_writes = 0;
    check("off: the scan lets go at its next tick (nothing pending, the rows inputs)", !fm1_an.on && !fm1_an.pend &&
          !fm1_an.driven && (host_reg[FM1_PA][FM1_DIR / 4u] & ((1u << 7) | (1u << 8))) == ((1u << 7) | (1u << 8)));
    run_ms(10);
    check("off: no converter access by the scan, the main loop's fm1_adc_read its own again",
          adc_writes == 0 && (adc_instant = 1, adc_res = 333u, fm1_adc_read(FM1_ADC_MASTER) == 333));
    printf("vel_diag: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
