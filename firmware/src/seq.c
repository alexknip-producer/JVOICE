/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Optional keyboard split. Kept in the pool so DSP globals retain their addresses.
 * on, physical cut (0=13), low/high track, low/high signed octaves, MIDI channel, MIDI cut (0=60). */
static uint8_t split_cfg[8] __attribute__((section(".pool")));
static uint32_t split_cut(uint32_t midi) { return split_cfg[midi ? 7 : 1] ? split_cfg[midi ? 7 : 1] : (midi ? 60u : 13u); }
static uint32_t split_track(uint32_t high) { return split_cfg[2u + high] % NTRK; }
static int32_t split_oct(uint32_t high) { return (int8_t)split_cfg[4u + high]; }
static uint8_t shift_valid[2][NTRK];
/* Keyboard, scale, arpeggiator, sequencer and transport.
 * Everything here runs in the audio ISR, once per CTL-sample block (64 samples since 0.9.9 ctl64; the note-ons it
 * fires carry the sample within the block they fall on: ev_off, voice.c), and
 * ends in trk_note_on / trk_note_off: engines never see where a note came from.
 * Four tracks, one transport: every track's pattern loops on its own LEN / DIV /
 * SWING / GATE (polymeter). The keys play the selected track; MIDI IN by GLO > SYSTEM ROUT (G_ROUTE):
 * CH1-6 (0) channels 1..6 play parts 1..6, channels 7..16 are ignored (midi_control.c midi_event); SEL (1)
 * every channel the selected track. Their CC1 (mod wheel), CC11 (expression) and channel aftertouch go to
 * the same track's modulation matrix (mod.c; the selected track: the one selected then).
 * A note into an armed track (song.rec) while
 * the transport runs is recorded into its pattern, quantised to its (swung) steps, with its
 * held length as TIE steps (rec_note, rec_hold, rec_release). With the ARP on, the notes the arp
 * plays are recorded, not the keys held (what plays back is what was heard). A key or MIDI note plays a
 * chord when the track's CHRD is on (chord.c): its notes go through the same input as so many keys. */
static const uint16_t SCALE_MASK[] = {
    0xFFF,                                   /* CHR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MAJ */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* MIN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* DOR */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 10),   /* MIX */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7) | (1 << 9),                          /* PEN */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 10),                         /* MPEN */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 11),   /* HARM */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 8) | (1 << 10),   /* PHRY */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 11),   /* LYD */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 10),   /* LOC */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 7) | (1 << 9) | (1 << 11),   /* MEL (ascending) */
    (1 << 0) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 7) | (1 << 10),              /* BLUES (minor) */
    (1 << 0) | (1 << 2) | (1 << 4) | (1 << 6) | (1 << 8) | (1 << 10),              /* WHOLE */
    (1 << 0) | (1 << 1) | (1 << 3) | (1 << 4) | (1 << 6) | (1 << 7) | (1 << 9) | (1 << 10), /* DIMHW */
    (1 << 0) | (1 << 2) | (1 << 3) | (1 << 5) | (1 << 6) | (1 << 8) | (1 << 9) | (1 << 11), /* DIMWH */
};

#define KB_SILENT 255u
static uint32_t kb_prev;
static volatile uint8_t kb_boot_hold;    /* #137: set from power-on to the end of the splash (main.c): the keys sound nothing,
                                          * and one held across its end stays silent until it is let go and pressed again */
static uint8_t kb_note[27], kb_trk[27];  /* per key: the note it started and on which track */
static int8_t kb_oct[27];                /* (0.8.5) .. on a DRUM track, the kit's octave it plays (OCT then: seq.c dxo_on) */
static volatile uint8_t kb_asleep;       /* MENU > SCREEN OFF (ui.c): the screen dark or waking, a key pressed now only
                                          * wakes it (silent, no layer, its release nothing) */
static uint8_t last_note = 60;
static volatile uint8_t transport_req;   /* 1 start, 2 stop, 3 restart (from the UI) */
static volatile uint8_t panic_req;       /* bit per track: release every sounding note (preset / engine change) */
static volatile uint8_t midi_hint;       /* MIDI IN played track n - 1, which is not the selected one (the UI says so) */

static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }

static uint32_t trk_midi_ch(uint32_t i) { return i % NTRK; }   /* MIDI channel 0..15 of track i (keys -> MIDI out) */

static int drum_track(const track_t *t) { return ENGINES[eng_idx(t->eng_req)] == &ENG_DRUM; }

/* (JIANT 0.6.72) MENU > CONTROL > MIDI OUT (ui.c midi_out_mode, over USB): KEYS (as ever: the keys' notes), CLOCK (and
 * the clock, 24 a beat, with START / STOP while the tempo is the FM-1's own), ALL (and every note the FM-1 plays: the
 * sequencer, the arp, FUGUE, the punch-in repeats, OCT and RANDOM on them, each track on its channel; the keys then
 * only where no arp plays them), OFF. The notes sent are remembered (mo_out): each gets its note-off, all of them at
 * STOP, a panic or a change of setting */
enum { MO_KEYS, MO_CLOCK, MO_ALL, MO_OFF };
static volatile uint8_t mo_lvl;                    /* the setting (ui.c sets it every frame) */
static uint8_t mo_last;                            /* .. as the ISR last saw it */
static uint8_t mo_out[NTRK][128];                  /* a played note's note sent on, + 1 (0: none) */
static uint32_t mo_pulse_acc;                      /* (samples x 24 towards the next clock: V10.33, a beat's 24 pulses exact) */
static void mo_send(uint32_t st, uint32_t d1, uint32_t d2)
{
    midi_out_event((st >> 4) | st << 8 | d1 << 16 | d2 << 24);
}
static int mo_plays(const track_t *t, uint32_t note)   /* trk_note_on would sound it (voice.c: the same tests) */
{
    return trk_index(t) < trk_live && !t->p[P_MUTE] && !((trk_hush >> trk_index(t)) & 1u) &&
           !(t->engine == ENGI_DRUM && drum_muted_t(t, note));
}
/* (0.9.9 ctl64) the block being handled (events_block: its samples) and the sample within it an event falls on (core.h
 * ev_off; voice.c starts or releases the voice there). Set around the note-ons and -offs fired inside the block, 0
 * between (a 1/2 TEMPO punch-in counts half the samples: its events land about where they would, within the block) */
static uint32_t blk_n = CTL;
static void ev_at(uint32_t k) { ev_off = k < blk_n ? k : blk_n; }   /* (blk_n: the next block's first) */
/* a note the FM-1 plays (not a key's): sounded, and with ALL sent as it sounds: the punch-ins' OCT- / OCT+ and RANDOM
 * on it (pfx.c pfx_pit, pfx_rpit: drawn by trk_note_on) */
static int dxp_overrides(const track_t *t);
static void seq_note_on(track_t *t, uint32_t note, uint32_t vel)
{
    if(dxp_overrides(t)) return; /* held fill owns this track; direct/live notes stay independent */
    uint32_t i = trk_index(t) % NTRK, send = mo_lvl == MO_ALL && note < 128u && mo_plays(t, note) && !pfx_holds(t);
    trk_note_on(t, note, vel);
    if (send) {
        uint32_t x = (uint32_t)clamp((int32_t)note + (t->pfx_pit + t->pfx_rpit) / 16, 0, 127);
        if (mo_out[i][note])
            mo_send(0x80u | trk_midi_ch(i), mo_out[i][note] - 1u, 0);   /* (struck again: its last one ends first) */
        mo_out[i][note] = (uint8_t)(x + 1u);
        mo_send(0x90u | trk_midi_ch(i), x, vel ? vel : 1u);
    }
}
static void seq_note_off(track_t *t, uint32_t note)
{
    uint32_t i = trk_index(t) % NTRK;
    if (note < 128u && mo_out[i][note]) {
        mo_send(0x80u | trk_midi_ch(i), mo_out[i][note] - 1u, 0);
        mo_out[i][note] = 0;
    }
    trk_note_off(t, note);
}
static __attribute__((noinline)) void mo_flush_of(uint32_t mask)   /* the notes sent on, of these tracks: off */
{
    uint32_t i, n;
    for (i = 0; i < NTRK; i++)
        if ((mask >> i) & 1u)
            for (n = 0; n < 128u; n++)
                if (mo_out[i][n]) {
                    mo_send(0x80u | trk_midi_ch(i), mo_out[i][n] - 1u, 0);
                    mo_out[i][n] = 0;
                }
}
#define mo_flush() mo_flush_of(0xFFu)
static void mo_key(const track_t *t, uint32_t pkt)   /* a key's note: not with OFF; with ALL, an arp sends its own */
{
    if (mo_lvl != MO_OFF && !(mo_lvl == MO_ALL && t->p[P_AMODE]))
        midi_out_event(pkt);
}
static int mo_clock_on(void) { return (mo_lvl == MO_CLOCK || mo_lvl == MO_ALL) && !song.g[G_CLOCK]; }
/* (V10.6z3, protocol 1.0) MIDI OUT ALL: a knob turned on the panel goes out as an NRPN (docs/NRPN.md): the UI queues
 * which (mo_knob: track k << 7 | P_* id; k 7: global id), the ISR sends the value it holds then, scaled to 14 bits over
 * the range DESC gives: CC99 0 (track, its channel) or 1 (global, channel 16), CC98 the id, CC6 / CC38 the value */
#define NRPN_G_NO (1u << G_CLOCK | 1u << G_MIDI | 1u << G_ROUTE | 1u << G_INFO | 1u << G_SLOT | 1u << G_LOAD | \
                   1u << G_SAVE | 1u << G_ENGSEL | 1u << G_CLRSEQ | 1u << G_INITSND)
static int motion_param(uint32_t id);   /* (motion.c) */
/* a global NRPN 1 reaches (in and out): not the transport, routing, load / save or engine switch, nor a fixed one */
static int nrpn_g_ok(uint32_t id)
{
    return id < G_COUNT && !(id < 32u && (NRPN_G_NO >> id) & 1u) && GP[id].max > GP[id].min;
}
static const uint8_t NRPN_CC[4] = {99, 98, 6, 38};
static uint16_t mo_kq[8];
static volatile uint8_t mo_kw, mo_kr;
static void mo_knob(uint32_t k, uint32_t id)
{
    uint32_t e = k << 7 | id;
    if (mo_lvl != MO_ALL || (k >= NTRK && !nrpn_g_ok(id)) || (k < NTRK && !motion_param(id)) || (uint8_t)(mo_kw - mo_kr) >= 8u ||
        ((uint8_t)(mo_kw - mo_kr) >= 2u && mo_kq[(mo_kw - 1u) & 7u] == e))   /* (queued, not taken yet: its latest value goes) */
        return;
    mo_kq[mo_kw & 7u] = (uint16_t)e;
    RING_PUBLISH();
    mo_kw++;
}
/* value v of a parameter of range d as 14 bits (docs/NRPN.md: the inverse of midi_nrpn's scaling) */
static uint32_t nrpn_of(const param_desc_t *d, int32_t v)
{
    int32_t r = d->max - d->min;
    return r > 0 ? (uint32_t)(((clamp(v, d->min, d->max) - d->min) * 16383 + r / 2) / r) : 0u;
}
static __attribute__((noinline)) void mo_knobs(void)
{
    while (mo_kr != mo_kw) {
        uint32_t e = mo_kq[mo_kr & 7u], k = e >> 7, id = e & 127u, ch = 15u, x;
        const param_desc_t *d;
        int32_t v;
        mo_kr++;
        if (k < NTRK) {
            d = track_desc(&trk[k], id);
            v = trk[k].p[id];
            ch = trk_midi_ch(k);
        } else {
            d = &GP[id];
            v = song.g[id];
        }
        if (d->max <= d->min)
            continue;
        x = nrpn_of(d, v);
        x = (uint32_t)(k >= NTRK) | id << 7 | (x >> 7) << 14 | (x & 127u) << 21;   /* CC99, CC98, CC6, CC38 */
        for (k = 0; k < 4u; k++, x >>= 7)
            mo_send(0xB0u | ch, NRPN_CC[k], x & 127u);
    }
}
static void kx_block(void);
static __attribute__((noinline)) void mo_block(uint32_t n)   /* events_block: the setting changed; the clock */
{
    kx_block();                                     /* (V10.6z4) the expression keys (from here: RAM code calls none
                                                     * of the image's last functions) */
    if (mo_kr != mo_kw)
        mo_knobs();
    if (mo_lvl != mo_last) {
        if (mo_last == MO_ALL)
            mo_flush();
        if ((mo_last == MO_CLOCK || mo_last == MO_ALL) && !(mo_lvl == MO_CLOCK || mo_lvl == MO_ALL) && song.playing &&
            !song.g[G_CLOCK])
            mo_send(0xFCu, 0, 0);                    /* (the clock leaves: whatever follows it stops) */
        mo_last = mo_lvl;
    }
    if (mo_clock_on()) {                             /* 24 a beat, also while stopped (a follower holds the tempo) */
        uint32_t beat = beat_samples(), t0 = fm1_ticks();   /* (V10.33) each pulse at its sample in the block, sent at its
                                                         * own TIMER4 tick (usb.c ep1_tx, bt_seg.c): steady to the
                                                         * receiver, not a burst a block (2.9 ms) */
        if (beat < 64u * 24u)
            beat = 64u * 24u;
        mo_pulse_acc += n * 24u;
        while (mo_pulse_acc >= beat) {
            uint32_t off;
            mo_pulse_acc -= beat;
            off = n - mo_pulse_acc / 24u;
            midi_out_event_at(0x0Fu | 0xF8u << 8, (t0 + off * 8000000u / ((uint32_t)FS / 3u)) | 1u);
        }
    }
}

/* (V10.6z4, the owner) the expression keys: OCT- or OCT+ held while a key's note sounds (ui_input.c oct_key; with no
 * note held they are the octave, as ever). OCT- a vibrato faded in as a mod wheel raised and out again (~200 ms), 5.5 Hz,
 * +-0.35 semitone; OCT+ a bend up along a fixed curve (smoothstep: eased in and landing, ~300 ms, and back so), up to
 * MENU > AUDIO > OCT BEND's semitones (kx_semi; OFF: no expression keys). Into the track's pitch bend (voice.c kx_q8,
 * beside MIDI's): every engine follows, DRUM none. Recording, they are automation of P_XPR (the gates: the same curves
 * play back); MIDI OUT ALL sends them as CC1 (the vibrato's depth) and pitch bend (full: the range; set the receiver's
 * bend range to OCT BEND's) */
#include "vel_kb.h"                                /* (V10.6z6) the keys' velocity and pressure */
_Static_assert(VK_NTRK == NTRK, "vel_kb.h VK_NTRK: the tracks");
static uint8_t kb_on[27];                           /* (below: how many of a key's notes sound) */
#define KX_BSS __attribute__((section(".bss.kx")))   /* (out of the merged statics, as fx.c's djf: theirs keep their places) */
static volatile uint8_t kx_g[2] KX_BSS;            /* the keys' gates: tracks (bit i) with OCT- (vibrato), OCT+ (bend) held */
static volatile uint8_t kx_semi KX_BSS;            /* the bend's range, semitones (0: OFF) */
static uint16_t kx_amt[NTRK][2] KX_BSS, kx_ph[NTRK] KX_BSS;   /* the ramps (vibrato depth, bend: 0..65535), the vibrato's phase */
static uint8_t kx_mo[NTRK][2] KX_BSS;              /* MIDI OUT's last CC1, bend MSB - 64 */
/* (V10.6z6) the keys' pressure (vel_kb.h vk[].at, MENU > CONTROL > AFTERTOUCH): a track's is its held keys' highest
 * (vs.atl), or what its recording plays (P_XPR bits 2..4: 0..7), slewed (~12 ms) into t->at, the matrix's AT (mod.c;
 * MIDI's channel aftertouch writes it too, the last one wins); MIDI OUT ALL sends it as channel aftertouch. Nothing
 * pressed and nothing recorded: t->at untouched (bit for bit as before) */
#define KX_STEP(ms) (int32_t)(65536u * CTL / ((uint32_t)FS / 1000u * (ms)))   /* a block's step of a ramp of ms */
static __attribute__((noinline)) void kx_block(void)
{
    uint32_t i, k, g;
    for (i = 0; i < NTRK; i++)
        vs.atl[i] = 0;
    for (k = 0; k < VK_NK; k++)                     /* (V10.6z6) the held keys' pressure, per track */
        if (kb_on[k] && vk[k].at > vs.atl[kb_trk[k] % NTRK])
            vs.atl[kb_trk[k] % NTRK] = vk[k].at;
    g = kx_g[0] | kx_g[1];                          /* (V10.7.6) nothing held or recorded, every ramp and pressure at 0 */
    for (i = 0; i < NTRK; i++)                      /* (as the pass below leaves them then): not run, ~10 % of the tick */
        g |= vs.atl[i] | ((uint32_t)trk[i].p[P_XPR] ^ 127u) | kx_amt[i][0] | kx_amt[i][1] | vs.at8[i];
    if (!g)
        return;
    for (i = 0; i < NTRK; i++) {
        int32_t x, q, v;
        g = 127u - (uint32_t)trk[i].p[P_XPR];
        g = g < 32u && !drum_track(&trk[i]) ? g : 0u;   /* (bits 0, 1 the gates; V10.6z6: 2..4 the pressure) */
        x = (int32_t)(g >> 2) * 18;
        x = (x > vs.atl[i] ? x : vs.atl[i]) << 8;
        if (x != (q = vs.at8[i])) {                 /* (V10.6z6) the pressure, slewed */
            q += (x - q + (x > q ? 7 : -7)) / 8;
            vs.at8[i] = (uint16_t)q;
            trk[i].at = (uint8_t)(q >> 8);
            if (mo_lvl == MO_ALL && (q >> 9) != vs.ato[i])   /* (MIDI OUT: in steps of 2, 0 at the end) */
                mo_send(0xD0u | i, (uint32_t)(vs.ato[i] = (uint8_t)(q >> 9)) << 1, 0);
        }
        g = ((kx_g[0] >> i) & 1u) | ((kx_g[1] >> i) & 1u) << 1 | (g & 3u);
        if (drum_track(&trk[i]))
            g = 0;
        for (k = 0; k < 2u; k++)
            kx_amt[i][k] = (uint16_t)clamp(kx_amt[i][k] + ((g >> k) & 1u ? 1 : -1) * (k ? KX_STEP(300) : KX_STEP(200)), 0, 65535);
        kx_ph[i] = (uint16_t)(kx_amt[i][0] ? kx_ph[i] + 65536u * 11u * CTL / 2u / (uint32_t)FS : 0u);   /* 5.5 Hz, from its middle */
        q = (int16_t)kx_ph[i];
        x = (kx_amt[i][1] + 8) >> 4;                /* Q12 (0..4096) */
        x = (x * x >> 12) * (3 * 4096 - 2 * x) >> 12;   /* smoothstep */
        v = q * (32768 - (q < 0 ? -q : q)) >> 13;   /* a sine, near enough (Q15) */
        kx_q8[i] = ((90 * (int32_t)kx_amt[i][0] >> 16) * v >> 15) + ((int32_t)kx_semi * x >> 4);
        if (mo_lvl != MO_ALL)
            continue;
        q = kx_amt[i][0] >> 9;
        v = kx_semi ? x * 63 >> 12 : 0;             /* (the bend's MSB above the middle: 0..63) */
        if (q != kx_mo[i][0])
            mo_send(0xB0u | i, 1u, (uint32_t)(kx_mo[i][0] = (uint8_t)q));   /* (track i: channel i, trk_midi_ch) */
        if (v != kx_mo[i][1])
            mo_send(0xE0u | i, 0u, (uint32_t)(kx_mo[i][1] = (uint8_t)v) + 64u);
    }
}

/* the 27 keys from F: black or white, and the key's place among the keys of its colour (white 0..15, black
 * 0..10). The DRUM grid: white keys are steps, black keys 1..8 lanes, 9 ACC, 10 / 11 the page */
static int key_black(uint32_t k) { return (int)((0x54Au >> ((k + 5u) % 12u)) & 1u); }
static uint32_t key_place(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < k; i++)
        n += (uint32_t)(key_black(i) == key_black(k));
    return n;
}
enum { GK_ACC = NLANE, GK_PGDN, GK_PGUP };       /* black keys 9..11 on the grid */

static uint32_t scale_mask(const track_t *t)
{
    return SCALE_MASK[clamp(t->p[P_SCALE], 0, sizeof SCALE_MASK / sizeof SCALE_MASK[0] - 1)];
}

/* note n moved down onto the track's scale (ROOT, SCALE): QNT SNAP's keys and QNT SEQ's sequenced notes */
static int32_t scale_snap(const track_t *t, int32_t n)
{
    uint32_t mask = scale_mask(t), guard = 12;
    while (guard-- && !((mask >> (uint32_t)((n - t->p[P_ROOT] + 120) % 12)) & 1u))
        n--;
    return n;
}

enum { QN_OFF, QN_SNAP, QN_WHITE, QN_SEQ };      /* P_QUANT */

/* (JIANT 0.4) G_STRN: note n onto the track's scale (down), then d steps of the scale up (d > 0) or down */
static int32_t scale_steps(const track_t *t, int32_t n, int32_t d)
{
    uint32_t mask = scale_mask(t);
    int32_t r = t->p[P_ROOT];
    n = scale_snap(t, n);
    if (!(mask & 0xFFFu))
        return n;
    for (; d > 0; d--)
        do n++; while (!((mask >> (uint32_t)((n - r + 120) % 12)) & 1u));
    for (; d < 0; d++)
        do n--; while (!((mask >> (uint32_t)((n - r + 120) % 12)) & 1u));
    return n;
}

static uint32_t kb_map(const track_t *t, uint32_t k)
{
    static const int8_t DEGREE[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];   /* (the engine it switches to) */
    int32_t n;
    if (e->keys && (n = e->keys(t, k)) >= 0)           /* the engine's own key map (DRUM's GM map, slices) */
        return (uint32_t)n;
    n = 53 + (int32_t)k;
    if (t->p[P_QUANT] == QN_SNAP || t->p[P_QUANT] == QN_SEQ)   /* SNAP (and SEQ): every key, rounded down */
        return (uint32_t)clamp(scale_snap(t, n + 12 * song.octave), 0, 127);   /* (0.6.81: TRANS as it sounds, voice.c) */
    if (t->p[P_QUANT] == QN_WHITE) {                    /* WHITE: white keys walk the scale, black keys are silent */
        uint32_t mask = scale_mask(t), i;
        int32_t count = 0, degree = DEGREE[n % 12], oct;
        if (degree < 0)
            return KB_SILENT;
        /* C4 is the root. Walk scale degrees on successive white keys, including
         * below C4; scales with 5, 6, 8 or 12 notes still have no duplicated degrees. */
        degree += (n / 12 - 5) * 7;
        for (i = 0; i < 12u; i++)
            count += (mask >> i) & 1u;
        oct = degree / count;
        degree %= count;
        if (degree < 0) {
            degree += count;
            oct--;
        }
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (!degree)
                    break;
                degree--;
            }
        n = 60 + t->p[P_ROOT] + 12 * oct + (int32_t)i;
    }
    return (uint32_t)clamp(n + 12 * song.octave, 0, 127);
}

/* the notes key k plays (kb_sn: how many, its chord; kb_on: how many of them started, the rest strummed in later) */
#define KB_CH 4u
static uint8_t kb_snd[27][KB_CH], kb_sn[27], kb_on[27];
static uint16_t kb_sdue[27];                    /* (0.6.81) samples to the strum's next note */
/* (V10.6z6) key velocity (vel_kb.h KB_VEL: each key's press's, 100 with VELOCITY OFF; its chord, strum, recording and
 * MIDI OUT use it) */
#define ARP_VEL(t) (vs.arpv[trk_index(t)] ^ 100u)    /* (stored ^ 100: 0, as zeroed, 100) */
/* (VEL, core.h P_TVEL) a velocity as track t's sound takes it: its distance from 100 scaled by VEL / 127 (127, every
 * store before: as played; 0: 100). Applied where a velocity enters, before what the FM-1 adds to it: the keys and MIDI
 * in (input_on, after the recording: the step keeps the key's own), the arp's notes, a step's own (STEP_VEL); a
 * step's ACCENT (127), HUMAN's spread and RATCHET's DECAY act after it, as they would on any note */
static inline uint32_t tvel(const track_t *t, uint32_t v)
{
    return FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL ? v : (uint32_t)(100 + ((int32_t)v - 100) * t->p[P_TVEL] / 127);
}
#define STEP_VEL(t, s) ((s)->flags & SF_ACCENT ? 127u : tvel((t), (s)->vel ? (s)->vel : 96u))

/* (JIANT 0.6.81) CHORD (SCL > CHORD; after the Omnichord and the Orchid, light): a key plays a chord on the track.
 * TRIAD 7TH ADD9 SUS2 SUS4 are built in the track's scale on the key (every tone in key; a key outside it takes the
 * scale note below; CHR: the major scale of ROOT; 0.8.3: a scale of five, six or eight notes takes each tone as the
 * interval the chord asks for where the scale has it, else the nearest it offers: sus chords and sixths in the
 * pentatonics, augmented in whole tone, diminished in the diminished scales), MAJ MIN 7 MAJ7 MIN7 POWER that shape on
 * any key. VOICING CLOSE
 * (root position), LEAD (each chord the inversion nearest the last one the track played: voice leading), OPEN (the
 * second tone an octave up), +BASS (the root an octave down added). STRUM: the notes one after the other, low to
 * high. The root alone on a kit and on MONO / LEGATO / UNISON. The keys only (MIDI notes play as they come); the
 * notes go to the sequencer's recording and the ARP as the key's own would */
enum { CD_OFF, CD_TRI, CD_SEV, CD_ADD9, CD_SUS2, CD_SUS4, CD_MAJ, CD_MIN, CD_DOM7, CD_MAJ7, CD_MIN7, CD_POW };
enum { CV_CLOSE, CV_LEAD, CV_OPEN, CV_BASS };
static uint8_t chord_last[NTRK][KB_CH], chord_nlast[NTRK];   /* the last chord each track played (LEAD) */
/* (0.8.3) the first of the intervals p (n of them) the scale (mask) has over a root rel semitones above ROOT; -1 none */
static int32_t chord_pick(uint32_t mask, int32_t rel, const int8_t *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if ((mask >> (uint32_t)((rel + p[i]) % 12)) & 1u)
            return p[i];
    return -1;
}
static uint32_t chord_build_mode(const track_t *t, uint32_t root, uint8_t *out, uint32_t mode)
{
    static const int8_t DIA[5][4] = {{0, 2, 4, -1}, {0, 2, 4, 6}, {0, 2, 4, 8}, {0, 1, 4, -1}, {0, 3, 4, -1}};
    static const int8_t SHP[6][4] = {{0, 4, 7, -1}, {0, 3, 7, -1}, {0, 4, 7, 10}, {0, 4, 7, 11}, {0, 3, 7, 10},
                                     {0, 7, 12, -1}};
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t vc = (uint32_t)t->p[P_CVOI], n = 0, i, j, ti = trk_index(t) % NTRK, any = mode & 0x80u, peek = mode & 0x40u;
    int32_t nt[KB_CH + 1], r = (int32_t)root;           /* (V10.25 SCENE any: any voice; V10.27 peek: the screen's, */
    mode &= 0x3Fu;                                      /* LEAD's last chord not written: drawing changed the voicing) */
    if (!mode || mode > CD_POW || e->oneshot || (e->keys && e->keys(t, 0) >= 0) || (t->p[P_VOICE] != V_POLY && !any)) {
        out[0] = (uint8_t)root;
        return 1;
    }
    if (mode >= CD_MAJ) {
        for (i = 0; i < 4u && SHP[mode - CD_MAJ][i] >= 0; i++)
            nt[n++] = r + SHP[mode - CD_MAJ][i];
    } else {                                    /* every other note of the scale from the key's */
        uint32_t mask = scale_mask(t), deg[12], c = 0, at = 0, guard = 12;
        int32_t rt = t->p[P_ROOT];
        if ((mask & 0xFFFu) == 0xFFFu || !(mask & 0xFFFu))
            mask = SCALE_MASK[1];               /* (CHR: the major scale of ROOT) */
        while (guard-- && !((mask >> (uint32_t)((r - rt + 120) % 12)) & 1u))
            r--;
        for (i = 0; i < 12u; i++)
            if ((mask >> i) & 1u) {
                if (i == (uint32_t)((r - rt + 120) % 12)) at = c;
                deg[c++] = i;
            }
        if (c == 7u) {                          /* a mode of seven: its own chord on the degree (diatonic) */
            for (i = 0; i < 4u && DIA[mode - CD_TRI][i] >= 0; i++) {
                uint32_t k = at + (uint32_t)DIA[mode - CD_TRI][i];
                nt[n++] = r + (int32_t)deg[k % c] + 12 * (int32_t)(k / c) - (int32_t)deg[at];
            }
        } else {                                /* (0.8.3) five, six or eight notes (PEN MPEN BLUES WHOLE DIMHW DIMWH):
                                                 * every other note of such a scale is a quartal or odd stack, so each
                                                 * tone is the interval the chord asks for where the scale has it, else
                                                 * the nearest it offers: a third major or minor (none: a suspension),
                                                 * the fifth perfect (else diminished over a minor third, augmented over
                                                 * a major), the seventh minor, major or sixth; pentatonic keys give
                                                 * triads, sus chords and sixths, whole tone augmented, diminished
                                                 * diminished and dominant sevenths: every tone in the scale */
            static const int8_t T3[4] = {4, 3, 5, 2}, F5[3] = {7, 8, 6}, F5M[3] = {7, 6, 8}, S7[3] = {10, 11, 9}, S7D[2] = {9, 10},
                                S9[2] = {14, 15}, S2[2] = {2, 1}, S4[2] = {5, 6};
            int32_t rel = (r - rt + 120) % 12, th, fi, x;
#define CPK(P) chord_pick(mask, rel, P, sizeof P)
            th = mode == CD_SUS2 ? CPK(S2) : mode == CD_SUS4 ? CPK(S4) : -1;
            if (th < 0) th = CPK(T3);
            fi = th == 3 ? CPK(F5M) : CPK(F5);
            nt[n++] = r;
            if (th >= 0) nt[n++] = r + th;
            if (fi >= 0) nt[n++] = r + fi;
            if (mode == CD_SEV && (x = th == 3 && fi == 6 ? CPK(S7D) : CPK(S7)) >= 0) nt[n++] = r + x;
            if (mode == CD_ADD9 && (x = CPK(S9)) >= 0) nt[n++] = r + x;
#undef CPK
        }
    }
    if (vc == CV_OPEN && n > 2u)
        nt[1] += 12;
    if (vc == CV_LEAD && chord_nlast[ti]) {     /* each tone the octave nearest the last chord's middle */
        int32_t mid = 0;
        for (i = 0; i < chord_nlast[ti]; i++) mid += chord_last[ti][i];
        mid /= (int32_t)chord_nlast[ti];
        for (i = 0; i < n; i++) {
            while (nt[i] - mid > 6 && nt[i] - 12 >= 24) nt[i] -= 12;
            while (mid - nt[i] > 6 && nt[i] + 12 <= 108) nt[i] += 12;
        }
    }
    if (vc == CV_BASS) {                        /* the root an octave down (a four-note chord leaves its fifth out) */
        if (n == 4u) { nt[2] = nt[3]; n = 3; }
        nt[n++] = r - 12;
    }
    for (i = 1; i < n; i++)                     /* low to high, no unisons */
        for (j = i; j > 0 && nt[j] < nt[j - 1]; j--) { int32_t x = nt[j]; nt[j] = nt[j - 1]; nt[j - 1] = x; }
    for (i = 1; i < n; i++)
        if (nt[i] <= nt[i - 1]) nt[i] = nt[i - 1] + 12;
    for (i = 0, j = 0; i < n && j < KB_CH; i++)
        if (nt[i] >= 0 && nt[i] <= 127) out[j++] = (uint8_t)nt[i];
    if (!j) out[j++] = (uint8_t)root;
    if (peek) return j;
    for (i = 0; i < j; i++) chord_last[ti][i] = out[i];
    chord_nlast[ti] = (uint8_t)j;
    return j;
}
static uint32_t chord_build(const track_t *t, uint32_t root, uint8_t *out)
{
    return chord_build_mode(t, root, out, (uint32_t)t->p[P_CHRD]);
}
static uint32_t chord_peek(const track_t *t, uint32_t root, uint8_t *out)   /* (V10.27) the screen's: no voicing state */
{
    return t->p[P_CHRD] ? chord_build_mode(t, root, out, (uint32_t)t->p[P_CHRD] | 0x40u) : chord_build(t, root, out);
}
static inline __attribute__((always_inline)) uint32_t chord_gap(const track_t *t)   /* (~0.5 ms a step to 64 (32 ms); V10.27, a tester: past it up to ~1 s) */
{
    uint32_t v = (uint32_t)clamp(t->p[P_CSTR], 0, 127);
    return v <= 64u ? v * (FS / 2000u) : 64u * (FS / 2000u) + (v - 64u) * (v - 64u) * 11u;
}

/* ------------------------------------------------------------- arp --- */
static void arp_add(track_t *t, uint32_t note)
{
    uint32_t i;
    if (t->p[P_AHOLD] && t->arp_phys == 0u)
        t->nheld = 0;                               /* new chord replaces the latched one */
    t->arp_phys++;                                  /* every key-down: arp_remove counts every key-up */
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] == note)
            return;                                 /* repeated note-on: not a new note */
    if (t->nheld < 16u)
        t->held[t->nheld++] = (uint8_t)note;
    if (t->nheld == 1u) {
        t->arp_pos = 0xFFFFFFF;                     /* fire on this block */
        t->arp_idx = 0xFFFFFFFFu;
    }
}

static void arp_remove(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    if (t->arp_phys)
        t->arp_phys--;
    if (t->p[P_AHOLD])
        return;
    for (i = 0; i < t->nheld; i++)
        if (t->held[i] != note)
            t->held[k++] = t->held[i];
    t->nheld = (uint8_t)k;
}

static void rec_note(track_t *t, uint32_t note, uint32_t vel);
static void rec_release(track_t *t, uint32_t note);
/* (V10.25) SCENE (SCL > SCENE, ui_scene.c): the keys play every track given a role at once, from the one key: CHORD the
 * key's chord of the scale (its CHORD type, TRI if off), ARP that chord into the track's arp, BASS the key an octave
 * down, LEAD an octave up; KEEP: the track as it is (its sequence plays on, the keys do not reach it). Armed, REC
 * records every track with a role into its own steps. The roles are the session's (RAM) */
enum { SC_KEEP, SC_CHORD, SC_ARP, SC_BASS, SC_LEAD, SC_DRUM, SC_N };
static uint8_t scn_on, scn_role[NTRK];   /* (V10.26: all KEEP until set or diced) */
/* (V10.6j) BASS and LEAD play a pattern over the key's chord (the track's arp: notes, MODE, RATE (N_DIV), GATE, PROB);
 * the kit's DRUM writes a beat of its own a key (ui_scene.c sc_beat: the main loop, scn_dreq the key + 1) */
static uint8_t scn_pat[NTRK];
enum { SC_GEN = 8, SC_NPAT, SC_DGEN = 4 };            /* (V10.6r) DRUM's 5th: GEN, GENE's drums (gen_track.c sc_gen) */                       /* (V10.6m) BASS LEAD's 9th: GEN, the generator's line (ui_scene.c sc_gen) */
static volatile uint8_t scn_dreq;
enum { SN_ROOT, SN_R5, SN_ROCT, SN_TRI, SN_R5O, SN_SEV };
static const uint8_t SC_PAT[2][8][5] = {
    {{SN_ROOT, 1, 0, 70, 127}, {SN_ROOT, 1, 1, 40, 127}, {SN_ROCT, 1, 1, 50, 127}, {SN_R5, 1, 1, 55, 127},
     {SN_TRI, 13, 0, 75, 127}, {SN_ROOT, 1, 2, 35, 127}, {SN_R5O, 11, 1, 50, 127}, {SN_ROCT, 4, 2, 45, 112}},
    {{SN_TRI, 11, 1, 60, 127}, {SN_SEV, 3, 1, 55, 127}, {SN_TRI, 8, 2, 45, 127}, {SN_SEV, 13, 1, 60, 127},
     {SN_TRI, 1, 0, 80, 90}, {SN_TRI, 12, 2, 40, 127}, {SN_SEV, 9, 1, 55, 127}, {SN_SEV, 4, 2, 45, 100}}};
static uint32_t scn_keys;                           /* the keys down that play the scene (their key-up too) */
static int rec_on(const track_t *t)
{
    uint32_t i = trk_index(t);                       /* (not modulo: the internal parts past NTRK never record) */
    return (((song.rec >> i) & 1u) || (scn_on && song.rec && i < NTRK && scn_role[i])) && song.playing && !chain.running;
}
static int pfx_rec_ok(void) { return song.rec && song.playing && !chain.running; }   /* (pfx.c: the punch-in lane) */

#include "motion.c"

/* the ARP's modes (P_AMODE). Append-only: a stored value keeps its meaning (ORD plays as UP, the ORD switch of
 * ARP 2 orders the notes). 1.2 (Discussion #95), over the note list (the held notes low to high, or as played,
 * across OCT octaves), C E G held: DNUP G E C E; UP+8 C E G C' (the first note again an octave above the top);
 * CONV C G E (outside in); DIVG E G C (inside out); PINKY C G E G (the top note between the others); THUMB
 * C E C G (the bottom one); WALK a random step up or down the list (bouncing off its ends); CHORD all the held
 * notes together (the first 4), an octave higher each step through OCT */
enum { AM_OFF, AM_UP, AM_DN, AM_UPDN, AM_RND, AM_ORD, AM_REPEAT, AM_DNUP, AM_UP8, AM_CONV, AM_DIVG, AM_PINKY,
       AM_THUMB, AM_WALK, AM_CHORD,
       AM_TRNS };   /* (JIANT) TRNS: a key no note but the track's sequence transposed by its interval from C4 (t->trn, kept
                     * when let go: the next key moves it), MIDI in too; the steps' notes, never the lane hits */

static void arp_forget(track_t *t)               /* (its notes ended otherwise: trk_all_off) */
{
    t->arp_note = t->arp_ch[0] = t->arp_ch[1] = t->arp_ch[2] = 0;
}

/* CHORD's notes. Not inlined: they run once an arp step, and kept out of the audio ISR's loop over the tracks
 * (tests/target_budget.py) */
static __attribute__((noinline)) void arp_chord_off(track_t *t, int rec)   /* the other notes end */
{
    uint32_t i;
    for (i = 0; i < 3u; i++)
        if (t->arp_ch[i]) {
            if (rec)
                rec_release(t, t->arp_ch[i]);
            seq_note_off(t, t->arp_ch[i]);
            t->arp_ch[i] = 0;
        }
}

static __attribute__((noinline)) void arp_chord(track_t *t, const uint32_t *list, uint32_t n)
{
    uint32_t i, nt, top = 0, o = t->arp_idx % (uint32_t)(t->p[P_AOCT] > 0 ? t->p[P_AOCT] : 1);
    uint32_t av = ARP_VEL(t);                       /* (V10.6z6: its keys' velocity) */
    if (n > 4u)
        n = 4u;
    for (i = 0; i < n; i++) {                       /* the held notes (sorted or as played), o octaves up */
        nt = (uint32_t)clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
        if (nt == 127u && top++)
            continue;                               /* (notes clamped onto the top one: once) */
        if (!t->arp_note)
            t->arp_note = (uint8_t)nt;
        else
            t->arp_ch[i - 1u] = (uint8_t)nt;
        if (rec_on(t))
            rec_note(t, nt, av);
        seq_note_on(t, nt, tvel(t, av));
    }
}

static void arp_silence(track_t *t)              /* the sounding arp note(s) end */
{
    if (t->arp_ch[0] | t->arp_ch[1] | t->arp_ch[2])
        arp_chord_off(t, 0);
    seq_note_off(t, t->arp_note);
    t->arp_note = 0;
}

static void arp_off(track_t *t)                  /* the sounding arp note(s) end (a recorded one too) */
{
    if (t->arp_ch[0] | t->arp_ch[1] | t->arp_ch[2])
        arp_chord_off(t, rec_on(t));
    seq_note_off(t, t->arp_note);
    if (rec_on(t))
        rec_release(t, t->arp_note);
    t->arp_note = 0;
}

/* the place in the note list (len notes) the step arp_idx plays (not inlined, as arp_chord); UP+8 adds its note */
static __attribute__((noinline)) uint32_t arp_pick(track_t *t, uint32_t *list, uint32_t len)
{
    uint32_t i = t->arp_idx, m = (uint32_t)t->p[P_AMODE], cyc, k, w;
    if (m == AM_UP8 && len < 64u)                   /* UP+8: the first note again over the top */
        list[len++] = (uint32_t)clamp((int32_t)list[0] + 12 * t->p[P_AOCT], 0, 127);
    cyc = len > 1u ? 2u * len - 2u : 1u;
    k = i % cyc;
    switch (m) {
    case AM_DN:
        return len - 1u - i % len;
    case AM_UPDN:
        return k < len ? k : cyc - k;
    case AM_DNUP:
        return k < len ? len - 1u - k : k - (len - 1u);
    case AM_RND:
        return rng() % len;
    case AM_CONV:
    case AM_DIVG:
        k = i % len;
        if (m == AM_DIVG)
            k = len - 1u - k;
        return (k & 1u) ? len - 1u - k / 2u : k / 2u;
    case AM_PINKY:
        return len < 2u ? 0u : (k & 1u) ? len - 1u : k / 2u;
    case AM_THUMB:
        return len < 2u ? 0u : (k & 1u) ? 1u + k / 2u : 0u;
    case AM_WALK:
        w = t->arp_walk;
        if (!i)
            w = 0;                                  /* (from the bottom) */
        else if (w >= len)
            w = len - 1u;                           /* (notes let go) */
        else if (len > 1u)
            w = !w ? 1u : w == len - 1u ? w - 1u : (rng() >> 7) & 1u ? w + 1u : w - 1u;
        t->arp_walk = (uint8_t)w;
        return w;
    default:
        return i % len;
    }
}

/* n: samples the arp advances by (the external clock's while it runs, 0 stopped); gate_n: samples its
 * gate counts down by (real time when the external transport stops: a tapped or latched note still ends) */
static volatile uint8_t fx_lock_on;              /* (0.6.44) ui_layer.c: the FX layer locked open, no other layer armed:
                                                 * the keys are its effects without FX held */
static volatile uint32_t beat_pos, beat_n;      /* samples into the beat, the beat of the bar (0..3) */
static volatile uint32_t bar_n;                 /* (0.6.43) the bars since PLAY (MUTATE: project.c mutate_poll) */
static uint8_t mut_n[NTRK];                     /* (V10.6z3) each track's EUCLID MUTATEs, counted (ui_euclid.c eu_mutate; the
                                                 * editor's MUTATE push, editor.c) */

/* the metronome (click.c) and the count-in (1.1, Discussion #131). The click follows the song tempo's quarter notes
 * (BPM, or the external clock's 24 pulses), 4 to the bar, the first accented, whatever a track's DIV or SWING: a beat is
 * four 1/16 steps as the sequencer counts them (click_beat_len: 4 x div_samples(1/16), which may be a few samples
 * shorter than beat_samples, so the click never drifts from the 1/16 tracks it is recorded against). clk_pos and
 * clk_step count the 1/16 steps as seq_tick counts a 1/16 track's (and midi_clock_pulse rescales clk_pos with them when
 * an external clock's tempo moves): CLK_START at seq_start clicks the first beat in the block step 0 starts in, and
 * beat k in the block a 1/16 track's step 4k starts in (every 4th step; step 0 of 16, the bar's first, accented).
 * The count-in: with MENU > COUNT-IN on, PLAY from stop with a track armed (REC while stopped starts PLAY too) on
 * the internal clock counts cin_total beats first (1 or 2 bars, clicked whatever CLICK is), then
 * starts the sequencer at step 1 exactly that many beats later (song.playing stays 0 until then: nothing plays or
 * records; the keys sound). A note into an armed track in the count-in's last eighth (the second half of its last
 * beat) is recorded onto step 1 when it starts (cin_keep, cin_flush), held on or let go as it was. An external clock
 * never counts in (its Start starts). STOP (seq_stop) cancels it. */
/* (clk_pos, clk_step, clk_n: mod.c, read by the SLICER and the FX layer before this file) */
static volatile uint8_t cin_left, cin_total;    /* the count-in's beats still to come (0: none), of how many */
static volatile uint32_t cin_pos;               /* samples into its beat playing */
static uint8_t cin_flush;                       /* the count-in ended in this block: record what was kept */
static uint8_t cin_note[NTRK][4], cin_vel[NTRK][4], cin_n[NTRK], cin_up[NTRK];   /* kept notes; cin_up bit k: let go */
static uint8_t cin_oct[NTRK][4];                /* (0.8.5) .. a kit's octave: its hits a step (bits 0-3), TOUCH (4-5) */
static int seq_counting(void) { return cin_left != 0u; }
static uint32_t click_beat_len(void) { return 4u * div_samples(2); }   /* DIV 2: 1/16 */
static int click_wanted(void)                  /* CLICK REC: while a track is armed (not in a song: no recording) */
{
    return click_mode == CLICK_ON || (click_mode == CLICK_REC && song.rec && !chain.running);
}
static void arp_step_do(track_t *t, uint32_t n);
static void arp_step(track_t *t, uint32_t n, uint32_t gate_n)
{
    arp_step_do(t, n);
    if (t->arp_note) {                                 /* (ctl64) the gate's end, at its sample, after the step (a step
                                                        * at its sample sets it from there) */
        if (t->arp_off <= gate_n) {
            ev_at(t->arp_off);
            arp_off(t);
            ev_off = 0;
        } else
            t->arp_off -= gate_n;
    }
}
static void arp_step_do(track_t *t, uint32_t n)
{
    uint32_t period, cnt, list[64], len = 0, i, j, o;
    int32_t sw;
    if (!t->p[P_AMODE] || !t->nheld || t->p[P_AMODE] == AM_TRNS) {   /* (TRNS: no arpeggio) */
        if ((!t->nheld || t->p[P_AMODE] == AM_TRNS) && t->arp_note)
            arp_off(t);
        return;
    }
    period = div_samples((uint32_t)t->p[P_ARATE]);
    sw = t->p[P_ASWING] * (int32_t)period / 250;
    t->arp_pos += n;
    {
        uint32_t per = period + (uint32_t)((t->arp_idx & 1u) ? sw : -sw);
        if (t->arp_pos < per && t->arp_pos != 0xFFFFFFF + n)
            return;
        if (t->arp_pos == 0xFFFFFFF + n) {               /* the start: now */
            t->arp_pos = 0;
            ev_off = 0;
        } else {                                        /* (ctl64) the step at its sample; the samples past it kept
                                                         * (they were dropped: an ARP ran a block late a step) */
            t->arp_pos -= per;
            if (t->arp_pos >= n)
                t->arp_pos = 0;
            ev_at(n - t->arp_pos);
        }
    }
    /* build the note list: held notes (sorted or as played) over OCT octaves */
    for (i = 0; i < t->nheld; i++)
        list[i] = t->held[i];
    cnt = t->nheld;
    if (!t->p[P_AORDER])
        for (i = 1; i < cnt; i++)
            for (j = i; j > 0 && list[j - 1] > list[j]; j--) {
                uint32_t x = list[j];
                list[j] = list[j - 1];
                list[j - 1] = x;
            }
    for (o = 0; o < (uint32_t)t->p[P_AOCT]; o++)
        for (i = 0; i < cnt && len < 64u; i++)
            list[len++] = clamp((int32_t)list[i] + 12 * (int32_t)o, 0, 127);
    if (t->p[P_AMODE] == AM_REPEAT) { list[0] = t->held[t->nheld - 1u]; len = 1; } /* REPEAT: last played note, no octave traversal */
    t->arp_idx++;
    j = t->p[P_AMODE] == AM_CHORD ? 0u : arp_pick(t, list, len);
    if (t->arp_note)
        arp_off(t);
    if ((uint32_t)(rng() & 127u) <= (uint32_t)t->p[P_APROB]) {
        t->arp_off = period * (uint32_t)t->p[P_AGATE] / 128u + ev_off;   /* (from the step's sample) */
        if (t->p[P_AMODE] == AM_CHORD) {            /* CHORD: the first 4 notes, an octave up each step */
            arp_chord(t, list, cnt);
            ev_off = 0;
            return;
        }
        uint32_t av = ARP_VEL(t);                   /* (V10.6z6: its keys' velocity) */
        t->arp_note = (uint8_t)list[j];
        if (rec_on(t))
            rec_note(t, t->arp_note, av);            /* recording: what the arp plays */
        seq_note_on(t, t->arp_note, tvel(t, av));
    }
    ev_off = 0;
}

static void arp_tick(track_t *t, uint32_t n) { arp_step(t, n, n); }

/* -------------------------------------------------------- note input --- */
/* the length of step idx in samples: SWING (the track's + the global, at most 100) makes the even steps longer
 * and the odd ones shorter, so every odd step starts late */
#include "seq_fit.c"
static __attribute__((noinline)) uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx)
{
    if (t->seq_fit & 128u) return fit_step_samples(t, period, idx);
    return swing_step_len(t, period, idx);   /* core.h: own + global, at most 100 */
}

/* (JIANT) SHIFT OFS: the step whose notes play at place idx, the sequence OFS steps later (left with OFS < 0,
 * wrapping inside LEN). Live recording writes there too, so a note recorded plays where it was heard */
static uint32_t seq_src(const track_t *t, uint32_t idx)
{
    int32_t len = t->p[P_SLEN] > 0 ? t->p[P_SLEN] : 1, o = t->p[P_SOFS] % len;
    return (uint32_t)(((int32_t)(idx % (uint32_t)len) - o + len) % len);
}

#include "seq_live.c"                              /* (p2, main 0.9.11) GLO > LIVE: the live modifiers, the ghost playhead */

/* (0.8.5) a recorded hit's octave (dx_in_rat / dx_in_tch: the key's or MIDI note's, eng_drum.c) as its lane's mode on
 * the step (core.h LM_*): x2 / x4 / x8 above the kit's octave, SHORT / GHOST / FLAM below, none in it; that lane only
 * (its hit and its notes), the rest of the step as it was. The last hit recorded on a lane sets it */
static uint32_t rec_lmode(void)
{
    if (dx_in_tch)
        return LM_SHORT + (dx_in_tch - 1u) % 3u;
    return dx_in_rat >= 8u ? LM_X8 : dx_in_rat >= 4u ? LM_X4 : dx_in_rat >= 2u ? LM_X2 : LM_NONE;
}
static uint32_t rec_repeats(const track_t *t, const step_t *s);
/* a hit (a GM note) into step s of a DRUM track (rec_note, a held ratchet's next step: dxo_rec_hold) */
static void rec_drum(track_t *t, step_t *s, uint32_t note, uint32_t vel, int32_t mic)
{
    uint32_t l = drum_note_lane(t, note), k, m = rec_lmode(), was;
    if (s->time != ST_NOTE || (!s->n && !s->hit)) {
        s->n = s->hit = s->acc = 0;
        s->flags = 0;
        s->vel = 0;
        ST_MICRO_SET(s, mic);                           /* (0.6.88) its place off the grid (REC Q) */
        if (CO_ACT(s->comp) == CA_SOUND)                /* (0.8.5: an empty step's TOUCH left over: not this hit's) */
            s->comp = s->carg = 0;
        if (CO_ACT(s->comp2) == CA_SOUND)
            s->comp2 = s->carg2 = 0;
        s->lm[0] = s->lm[1] = s->lm[2] = 0;
    }
    s->time = ST_NOTE;
    if (note == DRUM_LANE_NOTE[l]) {                    /* a lane's note: its hit */
        was = (s->hit >> l) & 1u;                       /* (p1) the sound in the step already (it rolls with it) */
        s->hit |= (uint8_t)(1u << l);
        if (vel > 110)
            s->acc |= (uint8_t)(1u << l);
    } else {                                            /* another GM drum (a low tom, a crash): a note */
        for (k = 0; k < s->n && s->note[k] != note; k++)
            ;
        if (!(was = k < s->n)) {
            if (s->n < 4u)
                s->n++;
            s->note[s->n - 1u] = (uint8_t)note;
        }
        if (vel > 110)
            s->flags |= SF_ACCENT;
    }
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
    /* (p1, main 0.9.16: Danil's "random rolls") a plain hit on a lane new to a step that repeats: once, as played (the
     * step's next parts replayed it at once, x2..x8, and on every pass after); played there again, still once */
    if (!m && (was ? step_lm_raw(s, l) == LM_ONCE : rec_repeats(t, s)))
        m = LM_ONCE;
    step_set_lmode(s, l, m);
    t->seq_active = 1;
}
static uint8_t rec_at, rec_late;                        /* (0.8.5) the step (play index + 1) the last hit went into;
                                                         * 1: the next one (it was played late in the one before) */
/* live recording: the note goes into the nearest step, as swung (the one playing, or
 * the next one when it is past the middle of the playing one). Overdub: a step that
 * holds notes gets this one added (a chord of up to 4; when full, the last note is
 * replaced); MONO / LEGATO / UNISON parts keep one note per step, as step entry does.
 * Held on (synth parts): each further step the sequencer enters while the note is
 * held becomes a TIE (rec_hold), up to the pattern length; a release before the middle
 * of the last one puts that step back (rec_release), so a short note stays one step.
 * A note recorded into another step ends the hold before (the step model ties the
 * notes of one step only). */
#if FELUCCA_DUAL_CORE                                    /* (p8) ~5.8 ms on the FM-1: the output's and the key's latency */
#define REC_LAT ((int32_t)(HALF_FRAMES + CTL + HALF_FRAMES / 2u))
#else
#define REC_LAT 0                                        /* (the host's tests, the emulator: no DAC between) */
#endif
/* (V10.6z4, the owner: "the feel of how I play is never recorded") a held note's end as played: its length (from where
 * it was played, rh_p; its start where it was recorded, MICRO) into the LEN of the step it ends in (n/16, CT_LEN; a TIE's
 * too), the last TIE given back when it ends in that one's first 1/32 (or before it: a quantised start); held to the
 * step's end, the track's GATE (it reaches the next step: 0 at 120 and up, else 15/16). A legato (the next note down
 * before this one is up, rec_legato): held to it, and on LEGATO / UNISON with SLIDE, so it plays back as one envelope
 * gliding (GLIDE's time; 0: SLIDE's ~45 ms) as the keys played it; MONO and POLY retrigger, touching */
static void rec_len(track_t *t, step_t *s, int32_t e, int32_t l)
{
    int32_t v = l > 0 ? (e * 16 + l / 2) / l : 16;
    s->ctime = (uint8_t)((s->ctime & 7u) | (uint32_t)(v >= 16 ? (t->p[P_SGATE] >= 120 ? 0 : 15) : v < 1 ? 1 : v) << 3);
}
static __attribute__((noinline)) void rec_end(track_t *t)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, period = div_samples((uint32_t)t->p[P_SDIV]);
    uint32_t i = t->seq_idx % len, s0 = t->rh_start % len, tie = t->rh_ties, j;
    int32_t l = (int32_t)step_samples(t, period, i), e = (int32_t)t->seq_pos - (int32_t)t->ext - REC_LAT - t->rh_p, o = 0;
    if (tie)                                        /* in its last tie: where it started as recorded (MICRO) */
        o = ST_MICRO(&t->step[seq_src(t, s0)]) * (int32_t)step_samples(t, period, s0) / 64;
    else if (i != s0) {                             /* (recorded into the next step, not in it yet) */
        e -= l;
        i = s0;
        l = (int32_t)step_samples(t, period, i);
    }
    if (tie && (e + o) * 32 < l) {                  /* it ends in that tie's first 1/32 (or before: a quantised
                                                     * start): the tie as it was, the step before to its end */
        if (seq_src(t, i) == t->rh_last)
            t->step[t->rh_last] = t->rh_bak;
        tie = --t->rh_ties;
        i = (i ? i : len) - 1u;
        l = (int32_t)step_samples(t, period, i);
        e += l;
    }
    if (tie)
        e += o;                                     /* (a tie: from its place; the note's own step: its length) */
    j = seq_src(t, i);
    if (j < NSTEP && (t->step[j].time == ST_TIE || (t->step[j].time == ST_NOTE && i == s0)))
        rec_len(t, &t->step[j], e, l);
}
static void rec_legato(track_t *t, uint32_t p)  /* p: the step before the next note's (play index), the held one's last */
{
    step_t *s = &t->step[seq_src(t, p)];
    uint32_t m = trk_vmode(t);
    if (p != t->rh_start && (!t->rh_ties || s->time != ST_TIE))
        return;
    rec_len(t, s, 1, 0);
    if (m == V_LEGATO || m == V_UNISON)
        s->flags |= SF_SLIDE;
}
static void rec_note_at(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, idx = t->seq_idx % len, k;
    uint32_t period = div_samples((uint32_t)t->p[P_SDIV]), sl = step_samples(t, period, t->seq_idx);
    int32_t gp = (int32_t)t->seq_pos - (int32_t)t->ext   /* (0.6.88: from the step's place; an early one began before) */
                 - REC_LAT;                         /* (p8, the owner: live notes landed late) what was heard: the
                                                         * sequencer a half buffer and a block ahead of the DAC, the key
                                                         * up to a half more: the press placed where its sound was heard */
    uint32_t next = gp > (int32_t)(sl / 2u);       /* (V10.6z4: the block PLAY starts in, before its first step,
                                                     * seq_pos 0x7FFFFFFF: into step 1, was the last step) */
    int32_t mic = 0;
    step_t *s;
    if (next)
        idx = (idx + 1u) % len;
    s = &t->step[seq_src(t, idx)];
    if (t->seq_pos < 0x7FFFFFFFu && t->p[P_HRQ] < 127) {   /* (0.6.88) REC Q under 100 %: where it was played, kept */
        int32_t off = next ? gp - (int32_t)sl : gp, l = (int32_t)(next ? step_samples(t, period, idx) : sl);
        int32_t q = 127 - clamp(t->p[P_HRQ], 0, 127);
        mic = (off * 64 + (off < 0 ? -l / 2 : l / 2)) / (l ? l : 1);   /* (0.9.1: 64ths of the step, rounded; was 16ths) */
        mic = clamp((mic * q + (mic < 0 ? -63 : 63)) / 127, MICRO_MIN, MICRO_MAX);   /* (REC Q's share of it, rounded) */
    }
    if (drum_track(t)) {                            /* DRUM: into the grid, no holds (hits) */
        rec_drum(t, s, note, vel, mic);
        rec_at = (uint8_t)(idx + 1u);
        rec_late = (uint8_t)next;
        if (next) {
            if (t->rskip_idx != idx)
                t->rskip_n = 0;
            t->rskip_idx = (uint8_t)idx;
            if (t->rskip_n < NLANE)
                t->rskip[t->rskip_n++] = (uint8_t)note;
        }
        return;
    }
    if (s->time != ST_NOTE || !s->n || trk_vmode(t) != V_POLY) {
        s->n = 0;                                   /* a fresh step */
        s->flags = 0;
        s->vel = 0;
        s->lm[0] = s->lm[1] = s->lm[2] = 0;         /* (p1: no note of it once) */
        s->ctime &= 7u;                             /* (V10.6z4) LEN: the track's GATE until the release says (rec_end) */
        ST_MICRO_SET(s, mic);                       /* (0.6.88) its place off the grid (REC Q) */
    }
    for (k = 0; k < s->n && s->note[k] != note; k++)
        ;
    if (k == s->n) {
        /* (p1, main 0.9.18) a note added to a POLY step that repeats: once, as played, not rolled with the step's own
         * (its slot LM_ONCE) */
        uint32_t rep = s->n && rec_repeats(t, s);
        if (s->n < 4u)
            s->n++;
        s->note[s->n - 1u] = (uint8_t)note;
        step_set_lmode(s, s->n - 1u, rep ? LM_ONCE : LM_NONE);
    }
    s->time = ST_NOTE;
    if (vel > 126)                                  /* (V10.6z4: an accent plays 127; 111..126 came back 127) */
        s->flags |= SF_ACCENT;
    if (vel > s->vel)
        s->vel = (uint8_t)vel;
    t->seq_active = 1;
    if (next) {                                     /* it sounds now: the step must not trigger it again */
        if (t->rskip_idx != idx)
            t->rskip_n = 0;
        t->rskip_idx = (uint8_t)idx;
        if (t->rskip_n < 4u)
            t->rskip[t->rskip_n++] = (uint8_t)note;
    }
    if (!t->rh_n || t->rh_start != idx) {           /* a new hold (one in another step ends) */
        if (t->rh_n)                                /* (V10.6z4) .. still held: a legato, into this one */
            rec_legato(t, (idx ? idx : len) - 1u);
        t->rh_n = 0;
        t->rh_start = (uint8_t)idx;
        t->rh_ties = 0;
        t->rh_p = (int16_t)(t->seq_pos < 0x7FFFFFFFu ? clamp(next ? gp - (int32_t)sl : gp, -32768, 32767) : 0);
    }
    for (k = 0; k < t->rh_n && t->rh_note[k] != note; k++)
        ;
    if (k == t->rh_n && t->rh_n < 4u)
        t->rh_note[t->rh_n++] = (uint8_t)note;      /* a chord: held until its last key is up */
}

/* the sequencer enters step idx (before playing it): a recorded note still held ties into it */
static void rec_hold(track_t *t, uint32_t idx, uint32_t len)
{
    step_t *s;
    uint32_t k;
    if (!t->rh_n)
        return;
    if (!((song.rec >> trk_index(t)) & 1u) || t->rh_ties + 1u >= len) {
        t->rh_n = 0;                                /* disarmed, or the whole pattern is this note */
        return;
    }
    if (idx == t->rh_start)
        return;                                     /* (recorded ahead into the step now starting) */
    s = &t->step[seq_src(t, idx)];
    t->rh_bak = *s;
    t->rh_last = (uint8_t)seq_src(t, idx);
    t->rh_ties++;
    for (k = 0; k < 4u; k++)
        s->note[k] = 0;
    s->n = 0;
    s->time = ST_TIE;
    s->flags &= SF_MFINE;                           /* (0.9.1: MICRO kept whole, as its sixteenth in cwhen) */
    s->vel = 0;
    s->ctime = 0;                                   /* (V10.6z4: a TIE's LEN is where the note ends: rec_end) */
    s->hit = s->acc = 0;
    s->lm[0] = s->lm[1] = s->lm[2] = 0;
}

/* (p2, main 0.9.11) LIVE: a modified track records at its ghost's place (seq_live.c), where the step is in the written
 * pattern */
static void rec_note(track_t *t, uint32_t note, uint32_t vel)
{
    uint16_t e = t->ext;
    int s = live_swap(t);
    if (s)
        t->ext = 0;
    rec_note_at(t, note, vel);
    if (s)
        live_swap(t);
    t->ext = e;
}
/* a key of a recorded note is up: the hold ends with the last one */
static void rec_release_at(track_t *t, uint32_t note)
{
    uint32_t i, k = 0;
    for (i = 0; i < t->rh_n; i++)
        if (t->rh_note[i] != note)
            t->rh_note[k++] = t->rh_note[i];
    if (k == t->rh_n || (t->rh_n = (uint8_t)k))
        return;                                     /* not one of them, or others still held */
    rec_end(t);                                     /* (V10.6z4) its length, as held (was: the steps it reached, a
                                                     * tie kept past its middle; one step, the track's GATE) */
}
static void rec_release(track_t *t, uint32_t note)
{
    int s = live_swap(t);
    rec_release_at(t, note);
    if (s)
        live_swap(t);
}

/* a note into armed track t in the count-in's last eighth: kept for step 1 (cin_flush); once each, 4 per track */
static __attribute__((noinline)) void cin_keep(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t i = trk_index(t), k;
    if (cin_left != 1u || cin_pos < click_beat_len() / 2u || !((song.rec >> i) & 1u) || t->p[P_AMODE])
        return;
    for (k = 0; k < cin_n[i] && cin_note[i][k] != note; k++)
        ;
    if (k == cin_n[i]) {
        if (k == 4u)
            return;
        cin_n[i]++;
    }
    cin_note[i][k] = (uint8_t)note;
    cin_vel[i][k] = (uint8_t)vel;
    cin_oct[i][k] = (uint8_t)((dx_in_rat & 15u) | (dx_in_tch & 3u) << 4);
    cin_up[i] &= (uint8_t)~(1u << k);
}

static uint8_t in_norec;                            /* (0.6.84) input_on plays without recording (key_next) */
static void input_on(track_t *t, uint32_t note, uint32_t vel)
{
    last_note = (uint8_t)note;
    if (cin_left)
        cin_keep(t, note, vel);
    if (t->p[P_AMODE] == AM_TRNS) {                 /* ARP TRNS: the key transposes the sequence */
        t->trn = (int8_t)clamp((int32_t)note - 60, -36, 36);
        return;
    }
    if (rec_on(t) && !t->p[P_AMODE] && !in_norec)  /* (ARP on: arp_tick records its notes; 0.6.84: a key's chord was
                                                     * recorded whole when the key went down) */
        rec_note(t, note, vel);
    if (t->p[P_AMODE])
        arp_add(t, note);
    else
        trk_note_on(t, note, tvel(t, vel));         /* (VEL: the sound's share of it; recorded as played) */
}

static int midi_note_held(const track_t *t, uint32_t note);
static int midi_local_held(const track_t *t, uint32_t note);
static void input_off(track_t *t, uint32_t note)
{
    if (midi_note_held(t, note))
        return;
    if (cin_left) {                                 /* a kept note let go before step 1: recorded, then let go */
        uint32_t i = trk_index(t), k;
        for (k = 0; k < cin_n[i]; k++)
            if (cin_note[i][k] == note)
                cin_up[i] |= (uint8_t)(1u << k);
    }
    rec_release(t, note);
    arp_remove(t, note);                            /* both: the note may have started in the */
    trk_note_off(t, note);                          /* other mode (ARP switched while held) */
}

/* ------------------------------------------- (0.8.5) the kit's octaves --- */
/* A DRUM track's note in another octave than the kit's (eng_drum.c DXO_*): a key at OCT +-n (kb_oct), a MIDI note of a
 * band around the GM map (drum_oct_of). Below: the hit with TOUCH n (dx_in_tch on its note-on: SHORT GHOST FLAM).
 * Above: the hit, then again every other step / once / x2 a step while held (dxo_tick; p1, as main 0.9.17: was x2 x4
 * x8): playing, on the grid the sequencer's lane
 * ratchets use (a part every step / n from each step's start, swung as the steps are: seq_lane_rat), stopped, every
 * step / n from the press. Recording, playing (rec_on), the step it goes into gets the hit and its lane's mode
 * (rec_lmode: that lane only); every step entered while it is held too, before it plays (dxo_rec_hold: the sequencer
 * plays those, the held note is silent there), so the pass after plays what was heard; let go before the middle of
 * such a step, that step is put back (dxo_bak), as a held synth note's TIE. The ARP on: the notes go to it plain. Each
 * held one: its key or MIDI note (id), up to DXO_N a track (one more replaces the last) */
#define DXO_N 4u
#define DXO_KEY 0x100u                                  /* id: a key (+ k), a MIDI note (+ ch << 7 + note) */
#define DXO_MIDI 0x1000u
typedef struct {
    uint16_t id, idx;                                   /* who holds it; the step (play index) part counts in */
    uint8_t note, vel, r, part;                         /* the hit, its hits a step, the part struck last (255 none) */
    uint8_t quiet, rec;                                 /* a step (+ 1) it is silent in (the sequencer plays it); the
                                                         * step (+ 1) its press was recorded into */
    uint8_t half, ph;                                   /* (p1, main 0.9.17) OCT +1: every other step; ph the steps since
                                                         * the press, odd: none */
    uint32_t cnt;                                       /* stopped: samples since its last hit */
} dxo_t;
static dxo_t dxo[NPART][DXO_N];
static uint8_t dxo_n[NPART];
static int dxp_overrides(const track_t *t)
{
    uint32_t ti=trk_index(t);
    return ti<NPART&&dxo_n[ti]&&dxo[ti][0].half>=2u&&dxo[ti][0].ph<16u;
}
static step_t dxo_bak[NPART];                           /* the step a held one was last recorded into, as it was */
static uint8_t dxo_bak_at[NPART];                       /* .. which (its source step + 1; 0 none), entered at play idx */
static uint16_t dxo_bak_idx[NPART];
static uint32_t dxo_part(const track_t *t, uint32_t r)  /* the part of the playing step it is in */
{
    uint32_t sl = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / (r ? r : 1u), p;
    p = t->seq_pos / (sl ? sl : 1u);
    return p < r ? p : r - 1u;
}
/* Extended held performances use the existing four held-key slots. Each cell
 * describes actual GM hits and a recordable per-lane 1/2/4 rate, never a jump
 * of the playhead. No additional voice pool or per-track buffer. */
/* (V10.6r) FILLS (OCT +1): 27 drum fills, nine kinds (the key's place mod 9) in three lengths (the key / 9: half a bar,
 * a bar, a bar over four kicks), on the step grid from the press: 0 the snare's roll building (8ths, 16ths, 32nds:
 * the lane's x2), 1 the toms high to low (GM 50 .. 41: the tom's pitches), 2 the snare then the toms, 3 snare + kick
 * FLAMs (TOUCH FLAM; 16ths at the third length), 4 kick + snare in unison (4ths, 8ths, 16ths; 32nds at the third), 5 the clap's roll, 6 32nds over the toms on the
 * beats, 7 3-3-2 accents, 8 hats and rims; held past its end, an open hat (a crash at the third length) and the kick
 * on the downbeat after it (e->rec 255: it went round once), then again. A hit's TOUCH: dx_in_tch (dxp_fire) */
static const uint8_t DXP_TOM[6] = {50, 48, 47, 45, 43, 41};
static uint32_t dxp_note(const dxo_t *e, uint32_t phase, uint32_t slot, uint32_t *rate)
{
    uint32_t key=e->note%27u;
    *rate=1;
    if (e->half==2u) {
        uint32_t ty=key%9u, v=key/9u, L=v?16u:8u, p=phase&(L-1u), q=p*4u/L, tom=DXP_TOM[p*6u/L], n=0;
        if (!p && (phase>=L || e->rec==255u)) return slot==0u?(v==2u?49u:46u):slot==2u?36u:0u;
        switch (ty) {
        case 0: case 5:
            if (!slot && (q>=2u || !(p&1u))) { n=ty?39u:38u; *rate=q==3u?2u:1u; }
            break;
        case 1: if (!slot) n=tom; break;
        case 2: if (!slot) n=q<2u?38u:DXP_TOM[(p*2u-L)*6u/L]; break;
        case 3:
            if (slot!=1u && (q==3u || v==2u || !(p&1u))) { n=slot?36u:38u; dx_in_tch=3; }
            break;
        case 4: if (slot!=1u && !(p&(q<2u?3u:q==2u?1u:0u))) { n=slot?36u:38u; *rate=v==2u&&q==3u?2u:1u; } break;
        case 6: if (!slot) { n=38u; *rate=2u; } else if (slot==1u && !(p&3u)) n=tom; break;
        case 7: if (slot!=1u ? (p&7u)%3u==0u : (p&1u)) n=slot==1u?tom:slot?36u:38u; break;
        default:
            if (!slot) { n=42u; *rate=q==3u?2u:1u; } else if (slot==1u && (p&3u)==2u) n=37u;
            break;
        }
        if (slot==2u && !n && (!p || (v==2u && !(p&3u)))) n=36u;
        return n;
    }
    /* Sets vary rhythm, lane and density independently. */
    if (slot==2u && (phase&3u)) return 0;
    *rate=1u<<((key+slot)%3u);
    return slot==0u?(key&1u?42u:38u):slot==1u?(key%3u==0u?45u:37u):36u;
}
static void dxp_fire(track_t *t, const dxo_t *e, uint32_t phase, uint32_t sub, uint32_t how)
{
    uint32_t j, rate, note, sh=e->note%27u>=9u?4u:3u, p=phase&((1u<<sh)-1u);
    if(phase>=16u) return; /* queued: no off-grid first hit */
    uint32_t vel=(uint32_t)clamp((int32_t)e->vel+(int32_t)(e->r>>4)-4,1,127);
    if(e->half==2u&&p) vel=(vel*((3u<<sh)+p)+(2u<<sh))>>(sh+2u);   /* (a fill builds: 3/4 of the level to all of it) */
    for(j=0;j<3u;j++) {
        note=dxp_note(e,phase,j,&rate);
        if(note && !(sub%(4u/rate))) {
            dx_in_rat=(uint8_t)rate;
            if(how==2u) rec_drum(t,&t->step[seq_src(t,t->seq_idx)],note,vel,0);
            else trk_note_on(t,note,vel);
        }
        dx_in_tch=0;
    }
    dx_in_rat=0;
}
/* One active performance and at most one replacement, using existing slots.
 * The last pressed key wins on the next step; releasing an old key cannot
 * cancel its replacement and old held keys never restart by themselves. */
static void dxp_choke(uint32_t ti)
{
    if(dxo_n[ti]>1u&&dxo[ti][1].half>=2u&&dxo[ti][1].ph==255u) {
        dxo[ti][0]=dxo[ti][1];dxo_n[ti]=1;
    }
}
static void dxp_step(dxo_t *e,uint32_t idx)
{
    if(e->ph==15u) e->rec=255u;                          /* (a fill went round: its downbeat next) */
    e->idx=(uint16_t)idx;e->part=255u;e->ph=(uint8_t)((e->ph+1u)&15u);
    /* A small, shared per-step velocity change is representable in step_t.
     * Keep all hit times on the tempo grid; don't jitter ratchets separately. */
    e->r=(uint8_t)(4u|((rng()&7u)<<4));
}
static void dxo_on(track_t *t, uint32_t note, int32_t o, uint32_t vel, uint32_t id)
{
    uint32_t ti = trk_index(t), k;
    dxo_t *e;
    uint32_t ext=drum_track(t)&&drum_ext(t)&&note>=86u&&note<=127u, extcnt=0, extpart=0;
    if(ext) o=1;
    if (!o || t->p[P_AMODE] || ti >= NPART || !drum_track(t)) {
        input_on(t, note, vel);
        return;
    }
    if (o < 0) {
        dx_in_tch = (uint8_t)(o < -3 ? 3 : -o);
        input_on(t, note, vel);
        dx_in_tch = 0;
        return;
    }
    dx_in_rat = DXO_RAT[o > 3 ? 3 : o];
    rec_at = 0;
    if(!ext) input_on(t, note, vel);                    /* (its first hit; recorded with the RATCH) */
    if(ext) {
        if(dxo_n[ti]) {extcnt=dxo[ti][0].cnt;extpart=dxo[ti][0].part;}
        k=dxo_n[ti]&&dxo[ti][0].half>=2u&&dxo[ti][0].ph<16u?1u:0u;
        dxo_n[ti]=(uint8_t)(k+1u);
    } else {
        for (k = 0; k < dxo_n[ti] && dxo[ti][k].id != id; k++);
        if (k == dxo_n[ti] && dxo_n[ti] < DXO_N) dxo_n[ti]++;
    }
    e = &dxo[ti][k < DXO_N ? k : DXO_N - 1u];
    e->id = (uint16_t)id;
    e->note = (uint8_t)note;
    e->vel = (uint8_t)vel;
    e->r = dx_in_rat;
    e->half = o == 1;
    e->ph = 0;
    e->cnt = 0;
    e->idx = t->seq_idx;
    e->part = song.playing && t->seq_pos < 0x7FFFFFFFu ? (uint8_t)dxo_part(t, e->r) : 255u;
    e->quiet = e->rec = 0;
    if(ext) {
        e->half=note<113u?2u:3u;
        e->note=(uint8_t)(note<113u?note-86u:(note-113u)%12u);
        e->r=4;
        e->part=song.playing&&t->seq_pos<0x7FFFFFFFu?(uint8_t)dxo_part(t,4):(uint8_t)extpart;
        e->cnt=extcnt;e->ph=255u;
        dx_in_rat=0;return;
    }
    if (cin_left)                                       /* (the count-in keeps it for step 1) */
        e->rec = 1;
    else if (rec_at && rec_on(t)) {
        e->rec = rec_at;
        if (rec_late)                                   /* into the next step: silent to its end, as it plays back */
            e->quiet = (uint8_t)(t->seq_idx + 1u);
    }
    dx_in_rat = 0;
}
static void dxo_off(track_t *t, uint32_t id)
{
    uint32_t ti = trk_index(t), k, j = 0, ext=0;
    if (ti >= NPART || !dxo_n[ti])
        return;
    for (k = 0; k < dxo_n[ti]; k++)
        if (dxo[ti][k].id != id)
            dxo[ti][j++] = dxo[ti][k];
        else ext|=dxo[ti][k].half>=2u;
    if (j == dxo_n[ti])
        return;
    dxo_n[ti] = (uint8_t)j;
    if (!j && dxo_bak_at[ti]) {                         /* let go early in a step it was held into: that step back */
        if (!ext && rec_on(t) && dxo_bak_idx[ti] == t->seq_idx && seq_src(t, t->seq_idx) == dxo_bak_at[ti] - 1u &&
            t->seq_pos < step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / 2u)
            t->step[dxo_bak_at[ti] - 1u] = dxo_bak[ti];
        dxo_bak_at[ti] = 0;
    }
}
/* the sequencer enters step idx of track t (before it plays it): recording, the held ones go into it */
static __attribute__((noinline)) void dxo_rec_hold(track_t *t, uint32_t idx)
{
    uint32_t ti = trk_index(t), k, src = seq_src(t, idx), bak = 0;
    dxp_choke(ti);
    for (k = 0; k < dxo_n[ti]; k++) {
        dxo_t *e = &dxo[ti][k];
        if(e->half>=2u&&(e->idx!=idx||e->ph==255u)) dxp_step(e,idx);
        if (e->rec == idx + 1u) {                       /* (its press went into this one: it plays it) */
            e->quiet = (uint8_t)(idx + 1u);
            continue;
        }
        if (!rec_on(t) || !drum_track(t) || (e->half==1u && !e->ph))   /* (p1: OCT +1, every other step) */
            continue;
        if (!bak) {
            dxo_bak[ti] = t->step[src];
            dxo_bak_at[ti] = (uint8_t)(src + 1u);
            dxo_bak_idx[ti] = (uint16_t)idx;
            bak = 1;
        }
        if(e->half>=2u) {
            step_t *s=&t->step[src];
            /* Replace this step's musical contents, including rests in a fill.
             * Both components, conditions, probability and automation remain. */
            memset(s,0,__builtin_offsetof(step_t,probability));s->time=ST_REST;memset(s->lm,0,sizeof s->lm);
            dxp_fire(t,e,e->ph,0,2);
            s->comp=dxo_bak[ti].comp;s->carg=dxo_bak[ti].carg;
            s->comp2=dxo_bak[ti].comp2;s->carg2=dxo_bak[ti].carg2;
            continue; /* hear the live performance once, not its sequencer copy */
        } else {
            dx_in_rat = e->r;
            rec_drum(t, &t->step[src], e->note, e->vel, 0);
            dx_in_rat = 0;
        }
        e->quiet = (uint8_t)(idx + 1u);
    }
}
static void dxo_hit(track_t *t, const dxo_t *e)
{
    if(e->half>=2u) dxp_fire(t,e,e->ph,e->part,0);
    else trk_note_on(t, e->note, e->vel);
}
/* a block on (events_block, after the sequencer): the held ones' next hits */
static __attribute__((noinline)) void dxo_tick(track_t *t, uint32_t n)
{
    uint32_t ti = trk_index(t), k;
    if (!drum_track(t) || t->p[P_AMODE]) {
        dxo_n[ti] = 0;
        return;
    }
    if(dxo_n[ti]>1u&&(!song.playing||t->seq_pos>=0x7FFFFFFFu)&&
       dxo[ti][0].half>=2u&&dxo[ti][0].part==3u&&
       dxo[ti][0].cnt+n>=div_samples((uint32_t)t->p[P_SDIV])/4u) dxp_choke(ti);
    for (k = 0; k < dxo_n[ti]; k++) {
        dxo_t *e = &dxo[ti][k];
        uint32_t r = e->half>=2u?4u:e->r ? e->r : 1u, part;
        if(e->half>=2u&&cin_left) continue;
        if (!song.playing || t->seq_pos >= 0x7FFFFFFFu) {   /* stopped: from the press */
            uint32_t per = div_samples((uint32_t)t->p[P_SDIV]) / r << (e->half==1u);
            if(e->half<2u) e->part = 255u;
            e->idx = 0xFFFFu;
            e->cnt += n;
            if (per && e->cnt >= per) {               /* (ctl64: the hit at its sample) */
                e->cnt -= per;
                if (e->cnt >= per)
                    e->cnt = 0;
                ev_at(e->cnt >= n ? 0u : n - e->cnt);
                if(e->half>=2u) {
                    e->part=(uint8_t)((e->part+1u)&3u);
                    if(!e->part) {dxp_step(e,0xFFFFu);e->part=0;}
                }
                dxo_hit(t, e);
                ev_off = 0;
            }
            continue;
        }
        e->cnt = 0;
        part = dxo_part(t, r);
        if (t->seq_idx != e->idx) {
            if(e->half>=2u) dxp_step(e,t->seq_idx);
            else {e->idx=t->seq_idx;e->part=255u;e->ph^=1u;}
        }
        if (e->quiet == t->seq_idx + 1u) {
            e->part = (uint8_t)part;
            continue;
        }
        if (part != e->part) {
            uint32_t sl = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx) / r, past;
            e->part = (uint8_t)part;
            past = t->seq_pos - part * sl;              /* (ctl64) the part's first sample in this block */
            ev_at(past >= n ? 0u : n - past);
            if (e->half!=1u || !e->ph)                 /* (p1: OCT +1, every other step) */
                dxo_hit(t, e);
            ev_off = 0;
        }
    }
}

/* the effect of key k in the FX layer (PF_N none): the first PF_M1 white keys (F3 .. A4) in order, black keys
 * 1..4 (F#3 G#3 A#3 C#4) the track mutes */
static uint32_t perf_key(uint32_t k)
{
    uint32_t p = key_place(k);
    static const uint8_t BK[5] = {PF_LP3, PF_TGT, PF_LP5, PF_LP7, PF_GLT};   /* (snd2) black keys 7..11: G#4 A#4 C#5 D#5 F#5 */
    return !key_black(k) ? (p < PF_M1 ? p : p == PF_M1 ? PF_CLR : PF_N) : p < NTRK ? PF_M1 + p : p - NTRK < 5u ? BK[p - NTRK] : PF_N;
}

/* (0.6.14) LATCH: ARP OFF and ARP 2's HOLD ON, the keys' notes keep sounding when let go; the first key of the next
 * chord (no key down) lets the latched ones go. In the arp's note list (held / nheld / arp_phys, unused with ARP OFF);
 * HOLD turned off lets them go (events_block) */
static int key_latch(const track_t *t) { return t->p[P_AHOLD] && !t->p[P_AMODE]; }
static void latch_release(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->nheld; i++) {
        trk_note_off(t, t->held[i]);
        mo_key(t, 0x08u | (0x80u | trk_midi_ch(trk_index(t))) << 8 | (uint32_t)t->held[i] << 16);
    }
    t->nheld = 0;
}

/* (0.6.19) the UI (main loop) lets a track's held / latched notes go through here, in events_block: SEQ TOOLS CLEAR,
 * REC CLEAR ALL (they zeroed the list from the main loop: the latched voices kept sounding, nothing left to end them) */
static volatile uint8_t latch_drop_req;
static uint8_t latch_was;                           /* bit i: track i was in LATCH last block */
static void latch_drop(track_t *t)
{
    latch_release(t);                               /* (their voices and MIDI OUT's notes end) */
    pfx_hush(t);                                    /* (0.6.28) the punch-in repeats end and forget the notes struck */
    t->pfx_lnn = 0;                                 /* (STUTTER / ARP replayed the cleared steps' last notes) */
    t->arp_phys = 0;
    if (t->arp_note)
        arp_silence(t);
}

/* key k plays kb_note[k] on track t. A note another key holds already sounds: it is not started again (nor sent
 * to MIDI OUT); the key keeps its note */
/* (0.6.81) the next note of key k's chord in (a note another key holds already sounds: not started again) */
static void key_next(uint32_t k, track_t *t)
{
    uint32_t x = kb_snd[k][kb_on[k]], mc = trk_midi_ch(trk_index(t));
    if(drum_track(t)&&drum_ext(t)&&kb_oct[k] && kb_oct[k]>=-2) {
        uint32_t key=(x+128u-29u)%128u;
        x=kb_oct[k]==-1?key%27u:kb_oct[k]==-2?drum_fx_note(key):
          kb_oct[k]==1?86u+key%27u:113u+key%12u;
        kb_snd[k][kb_on[k]]=(uint8_t)x;
        kb_oct[k]=0;                                  /* retain the exact MIDI note until release */
    }
    if (!midi_local_held(t, x)) {
        in_norec = kb_sn[k] > 1u;                   /* (0.6.84: the chord's notes recorded at once, key_on) */
        vs.arpv[trk_index(t)] = kb_velx[k];            /* (V10.6z6: the key's velocity, its arp's too) */
        dxo_on(t, x, kb_oct[k], KB_VEL(k), DXO_KEY + (x>=86u&&x<=127u?x:k));
        in_norec = 0;
        mo_key(t, 0x09u | (0x90u | mc) << 8 | drum_oct_note(x, kb_oct[k]) << 16 | (uint32_t)KB_VEL(k) << 24);
    }
    kb_on[k]++;
}
static void key_on(uint32_t k, track_t *t)
{
    uint32_t n = chord_build(t, kb_note[k], kb_snd[k]), gap = n > 1u ? chord_gap(t) : 0u, i;
    if (key_latch(t)) {                             /* LATCH: a new chord lets the last one go */
        uint32_t j;
        if (!t->arp_phys)
            latch_release(t);
        t->arp_phys++;
        for (j = 0; j < n; j++) {
            for (i = 0; i < t->nheld && t->held[i] != kb_snd[k][j]; i++)
                ;
            if (i == t->nheld && t->nheld < 16u)
                t->held[t->nheld++] = kb_snd[k][j];
        }
    }
    kb_on[k] = 0;
    kb_sn[k] = (uint8_t)n;
    if (n > 1u && rec_on(t) && !t->p[P_AMODE])      /* (0.6.84) recording: the whole chord into one step, where the
                                                     * key went down (the strum's later notes would fall into the next
                                                     * step); the sequencer strums it back as it plays (seq_step) */
        for (i = 0; i < n; i++)
            rec_note(t, kb_snd[k][i], KB_VEL(k));
    for (i = 0; i < n && (!gap || !i); i++)        /* (STRUM: the first now, the others by keys_strum) */
        key_next(k, t);
    kb_sdue[k] = (uint16_t)(gap < 65535u ? gap : 65535u);
    kb_sdue[k] = (uint16_t)(kb_sdue[k] > blk_n ? kb_sdue[k] - blk_n : 0u);   /* (ctl64: keys_strum checks it from the
                                                     * next block on; the key came at this one's first sample) */
    last_note = kb_note[k];                         /* (step entry, the SAMPLE zone: the key's note) */
}
/* (0.6.81) STRUM: the keys' chords still coming in, a block on */
static void keys_strum(uint32_t n)
{
    uint32_t k;
    for (k = 0; k < 27u; k++) {
        if (kb_on[k] >= kb_sn[k] || !kb_on[k])
            continue;
        if (kb_sdue[k] > n) {
            kb_sdue[k] = (uint16_t)(kb_sdue[k] - n);
            continue;
        }
        ev_at(kb_sdue[k]);                            /* (ctl64) at its sample; the next gap samples after it */
        key_next(k, &trk[kb_trk[k] % NTRK]);
        ev_off = 0;
        kb_sdue[k] = (uint16_t)clamp((int32_t)kb_sdue[k] + (int32_t)chord_gap(&trk[kb_trk[k] % NTRK]), 1, 65535);
        kb_sdue[k] = (uint16_t)(kb_sdue[k] > n ? kb_sdue[k] - n : 0u);   /* (within this block still: the next's first) */
    }
}

/* key k is up: the note it started ends, unless another key still holds it */
static void key_off(uint32_t k, track_t *t)
{
    uint32_t n = kb_on[k], i, mc = trk_midi_ch(trk_index(t));
    if (!n)
        return;
    dxo_off(t, DXO_KEY + k);                         /* the held performance ends (LATCH too) */
    kb_on[k] = 0;                                   /* (the strum's notes not in yet: never) */
    kb_sn[k] = 0;
    /* Repeated physical octaves share a performance note. Its scheduler is
     * released by the last owner, even if the key map/octave or LATCH changed. */
    uint32_t control=kb_snd[k][0];
    if(control>=86u&&control<=127u&&!midi_local_held(t,control)) dxo_off(t,DXO_KEY+control);
    if (key_latch(t)) {                             /* LATCH: it keeps sounding (recorded as let go) */
        if (t->arp_phys)
            t->arp_phys--;
        for (i = 0; i < n; i++)
            rec_release(t, kb_snd[k][i]);
        return;
    }
    for (i = 0; i < n; i++) {
        uint32_t x = kb_snd[k][i];
        if (midi_local_held(t, x))
            continue;
        input_off(t, x);
        mo_key(t, 0x08u | (0x80u | mc) << 8 | drum_oct_note(x, kb_oct[k]) << 16);
    }
}

/* (V10.25) SCENE: key k down (on) or up on every track with a role; the notes again from the key at the key-up */
static uint32_t scn_notes(const track_t *t, uint32_t r, uint32_t root, uint8_t *o, int pk)
{
    if (r == SC_BASS || r == SC_LEAD) {                 /* (V10.6j) the pattern's notes: the arp plays them */
        uint32_t set = SC_PAT[r == SC_LEAD][scn_pat[trk_index(t) % NTRK] & 7u][0], n, i;
        int32_t b = (int32_t)root - 12;
        if (set >= SN_TRI && set != SN_R5O) {
            n = chord_build_mode(t, (uint32_t)clamp(r == SC_LEAD ? (int32_t)root + 12 : b, 0, 127), o,
                                 (set == SN_SEV ? CD_SEV : CD_TRI) | 0x80u | (pk ? 0x40u : 0u));
            return n;
        }
        o[0] = (uint8_t)clamp(b, 0, 127);
        n = 1;
        if (set == SN_R5 || set == SN_R5O) o[n++] = (uint8_t)clamp(b + 7, 0, 127);
        if (set == SN_ROCT || set == SN_R5O) o[n++] = (uint8_t)clamp(b + 12, 0, 127);
        for (i = 0; r == SC_LEAD && i < n; i++) o[i] = (uint8_t)clamp(o[i] + 24, 0, 127);
        return n;
    }
    return chord_build_mode(t, root, o, (t->p[P_CHRD] ? (uint32_t)t->p[P_CHRD] : CD_TRI) | 0x80u | (pk ? 0x40u : 0u));
}
static void scene_key(uint32_t k, int on)
{
    uint32_t i, j, n, mc;
    uint8_t o[KB_CH + 1];
    for (i = 0; i < trk_live && i < NTRK; i++) {
        track_t *t = &trk[i];
        if (drum_track(t)) {                            /* (V10.6j) the kit's DRUM: a beat of its own for the key */
            if (on && scn_role[i] == SC_DRUM)
                scn_dreq = (uint8_t)(k + 1u);
            continue;
        }
        if (!scn_role[i] || scn_role[i] == SC_DRUM)      /* (DRUM: a kit's alone) */
            continue;
        if ((scn_role[i] == SC_BASS || scn_role[i] == SC_LEAD) && scn_pat[i] % SC_NPAT == SC_GEN) {
            if (on)                                     /* (V10.6m) GEN: the key's chord moves the line (the root's
                                                         * interval from the key's, the nearest way; QUANT SEQ: in scale) */
                t->trn = (int8_t)(((kb_map(t, k) + 132u - (uint32_t)t->p[P_ROOT]) % 12u + 5u) % 12u - 5);
            continue;
        }
        n = scn_notes(t, scn_role[i], kb_map(t, k), o, 0);
        mc = trk_midi_ch(i);
        if (on && n > 1u && rec_on(t) && !t->p[P_AMODE])   /* (a chord: into one step, as key_on) */
            for (j = 0; j < n; j++)
                rec_note(t, o[j], KB_VEL(k));
        in_norec = (uint8_t)(on && n > 1u);
        for (j = 0; j < n; j++) {
            if (midi_local_held(t, o[j]))
                continue;
            if (on) {
                input_on(t, o[j], KB_VEL(k));
                mo_key(t, 0x09u | (0x90u | mc) << 8 | (uint32_t)o[j] << 16 | (uint32_t)KB_VEL(k) << 24);
            } else {
                input_off(t, o[j]);
                mo_key(t, 0x08u | (0x80u | mc) << 8 | (uint32_t)o[j] << 16);
            }
        }
        in_norec = 0;
    }
}

/* (the keys' block is RAM code: SCENE's call goes through a pointer, out of the direct calls' +-4 MiB reach check;
 * a key event, not every block) */
static void (*volatile scene_fp)(uint32_t, int) = scene_key;

#ifdef FM1_INPUT_LAT
/* the press latency stats (hal/fm1_input.h fm1_in_stat, console `inp`): from the first scan that saw
 * key k closed to its note-on here, and to the note's first sample leaving the DMA (kb_out_tick:
 * audio.c, the TIMER4 tick at which the block being rendered starts to play) */
static uint32_t kb_out_tick;
static __attribute__((noinline)) void kb_lat(uint32_t k)
{
    uint32_t t0 = fm1_in.note_t0[k], d = fm1_ticks() - t0, o = kb_out_tick - t0;
    fm1_in_stat.kb_n++;
    fm1_in_stat.kb_sum += d;
    fm1_in_stat.dac_sum += o;
    if (d > fm1_in_stat.kb_max)
        fm1_in_stat.kb_max = d;
    if (o > fm1_in_stat.dac_max)
        fm1_in_stat.dac_max = o;
}
#endif

static void keyboard_block(void)
{
    uint32_t cur = fm1_in.notes & ~vk_hold, ch, k;   /* (V10.6z6: a press waiting for its strike, vel_kb.h: not yet) */
    uint32_t lay, fx;
    keys_strum(CTL);                              /* (0.6.81) the chords' strums */
    ch = cur ^ kb_prev;                           /* keys also sound while entering steps */
    if (!ch)
        return;
    if (kb_boot_hold) {                           /* #137: under the splash; their release is silent too */
        for (k = 0; k < 27u; k++)
            if ((ch & cur) >> k & 1u)
                kb_note[k] = KB_SILENT;
        kb_prev = cur;
        return;
    }
    lay = (fm1_in.buttons & kb_mask) != 0u || fx_lock_on;
    fx = (fm1_in.buttons & perf_mask) != 0u || fx_lock_on;   /* (0.6.44: the FX layer locked open) */
    for (k = 0; k < 27u; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u) {                    /* the selected track; the key-up goes to the same one */
            kb_trk[k] = song.sel;
            if (kb_asleep) {                      /* (SCREEN OFF: the wake press is swallowed) */
                kb_note[k] = KB_SILENT;
                continue;
            }
            if (lay) {                            /* a layer's button held (or locked): the key is the layer's */
                kb_note[k] = KB_SILENT;           /* (ui_layer.c), with FX an effect (perform.c) */
                kb_layer |= 1u << k;
                if (fx)
                    perf_press(perf_key(k), 1);
                continue;
            }
            if (song.grid >= 2u) {                /* NAME (ui_name.c): every key types, none sounds; (p2) 3: GLO > LIVE
                                                   * MANO, the keys its board (seq_live.c, ui_live.c) */
                kb_note[k] = KB_SILENT;
                if (song.grid == 3u)
                    lv.kb |= 1u << k;
            } else if (song.grid)                   /* the DRUM grid: a lane key plays its lane, the rest are the UI's */
                kb_note[k] = key_black(k) && key_place(k) < NLANE ? DRUM_LANE_NOTE[key_place(k)] : KB_SILENT;
            else {
                uint32_t high = k >= split_cut(0);
                if (split_cfg[0]) kb_trk[k] = (uint8_t)split_track(high);
                kb_note[k] = (uint8_t)kb_map(&trk[kb_trk[k]], k);
                if (split_cfg[0] && kb_note[k] != KB_SILENT && !drum_track(&trk[kb_trk[k]]))
                    kb_note[k] = (uint8_t)clamp((int32_t)kb_note[k] + 12 * split_oct(high), 0, 127);
            }
            kb_oct[k] = (int8_t)(!song.grid && drum_track(&trk[kb_trk[k]]) ? song.octave + (split_cfg[0] ? split_oct(k >= split_cut(0)) : 0) : 0);   /* split drum octave retained until release */
            if (kb_note[k] == KB_SILENT)
                continue;
            if (scn_on && !song.grid && !split_cfg[0]) {           /* (V10.25) SCENE: every track with a role */
                scn_keys |= 1u << k;
                scene_fp(k, 1);
                continue;
            }
            key_on(k, &trk[kb_trk[k]]);
#ifdef FM1_INPUT_LAT
            kb_lat(k);
#endif
        } else {
            if ((kb_layer >> k) & 1u) {
                kb_layer &= ~(1u << k);
                perf_press(perf_key(k), 0);
                continue;
            }
            if (kb_note[k] == KB_SILENT) {
                lv.kb &= ~(1u << k);
                continue;
            }
            if ((scn_keys >> k) & 1u) {           /* (V10.25) a SCENE key: its notes on every role */
                scn_keys &= ~(1u << k);
                scene_fp(k, 0);
                continue;
            }
            key_off(k, &trk[kb_trk[k] % NTRK]);
        }
    }
    kb_prev = cur;
}

/* (V10.7.10; main loop, vel.c vel_poll) the keys 0..19 whose press never waits for its strike's velocity (vel_kb.h
 * vs.now): those keyboard_block sends to a DRUM track (DRUM-X: its kit's key map), the selected one or, with KEY SPLIT,
 * their half's; the DRUM grid's lanes too (NAME, MANO: no note, nothing to wait for) */
static uint32_t kb_drum_keys(void)
{
    uint32_t c;
    if (song.grid)
        return ~0u;
    if (!split_cfg[0])
        return drum_track(&trk[song.sel % NTRK]) ? ~0u : 0u;
    c = split_cut(0);
    c = c < 32u ? ~0u << c : 0u;                    /* (the high half: keys from the cut up) */
    return (drum_track(&trk[split_track(1)]) ? c : 0u) | (drum_track(&trk[split_track(0)]) ? ~c : 0u);
}

/* (V10.6v) RAMP (core.h CV_RAMP_*): a track's one gliding parameter (the RAMP launched last): its id + 1 (0 none), the
 * step that launched it, VALUE, LENGTH - 1; how far, Q24 of the way (past RAMP_ONE: the knob's value coming back, in
 * samples); the lock's value it goes to */
typedef struct { uint8_t id, st, val, len; uint32_t pos; int16_t to; } ramp_t;
static ramp_t rmp[NTRK] __attribute__((section(".bss.swarm")));   /* (out of the merged statics, as fx.c's djf) */
#define RAMP_ONE (1u << 24)
#define RAMP_BACK 1024u                             /* (~23 ms: the knob's value back without a click) */
/* -------------------------------------------------------- sequencer --- */
static void fg_reset(void);                         /* (fugue.c) */
static void gl_reset(void);                         /* (seq2f: the global lane) */
static void seq_start(void)
{
    memset(shift_valid, 0, sizeof shift_valid);
    uint32_t i;
    ms_clock = 0;                                  /* (JIANT) the modulation sequences from their step 0 */
    if (mo_clock_on()) {                           /* (0.6.72) MIDI OUT CLOCK / ALL: START, the clock from here */
        mo_send(0xFAu, 0, 0);
        mo_pulse_acc = 0;
    }
    motion_begin();
    chain_start();
    for (i = 0; i < NTRK; i++) {                   /* every track from its step 0, together */
        track_t *t = &trk[i];
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFF;                   /* step 0 fires on the first block */
        t->ext = 0;
        t->rskip_n = 0;
        t->rh_n = 0;
        t->rat_left = 0;
        t->arp_from = 0; t->loops = 0;             /* (0.6.39) the components: the first loop, no arpeggio */
        t->jump_to = 0;                            /* (0.6.42) no JUMP (LIVE's) */
        t->csp_n = t->c_pre = t->c_acc = 0;        /* (seq2d) no component span, PRE false, TRANSPOSE from its place */
        t->cpos = t->cv_on = 0;                    /* (0.6.47) C.LEN's lane from its first step */
    }
    fg_reset();                                    /* (0.6.52) FUGUE's heads from their first step */
    memset(rmp, 0, sizeof rmp);                    /* (V10.6v) no RAMP (a stop: motion_end the knob's values back) */
    lv.act = 0;                                    /* (p2) LIVE: the ghosts from step 0 too (live_q) */
    gl_reset();                                    /* (seq2f) the global lane from its first step, its first pass */
    song.tick = 0;
    beat_pos = 0; beat_n = 0;                      /* the ARP LED's beat from the top too */
    bar_n = 0;
    clk_pos = CLK_START;                           /* the metronome's first beat with step 0 */
    song.playing = 1;
    slicer_start();                                /* slicer.c: its step 0 with the sequencer's */
    perf_start();                                  /* perform.c: its 1/16 grid too */
}

/* (0.6.37) a step's chance as the track plays it: its own, scaled by SHIFT > CHANCE (P_TCHAN 0: as it is) */
static uint32_t trk_chance(const track_t *t, const step_t *s)
{
    return step_chance(s) * (127u - (uint32_t)clamp(t->p[P_TCHAN], 0, 127)) / 127u;
}
/* (0.6.42) FILL: the FX button held (ui_input.c sets it each pass) */
static volatile uint8_t seq_fill;
/* the step's one roll: its chance (the track's scaling it). (seq2d: TRIG and FILL are conditions of a component now) */
static int step_rolls(const track_t *t, const step_t *s)
{
    uint32_t c = trk_chance(t, s);
    if (live_drop(t))                               /* (p2) LIVE THIN */
        return 0;
    return c >= 100u || rng() % 100u < c;
}
/* (0.9.9-seq2d) STEP COMPONENTS v2 (core.h CA_* CC_*). Component k's CONDITION on step s this pass (t->loops: the
 * pattern's passes since PLAY, 0 the first); acted: C1 acted (C2's ELSE, MONO). Every condition but ALWAYS, ELSE, PRE
 * and NEI sets PRE (the track's: what the next PRE, and the next track's NEI, look at) */
static int comp_cond(track_t *t, const step_t *s, uint32_t k, uint32_t acted)
{
    uint32_t c = CO_COND(k ? s->comp2 : s->comp), nm = s->cnm[k & 1u], n = CNM_N(nm), m = CNM_M(nm), p = t->loops;
    const track_t *nb = &trk[(trk_index(t) + NTRK - 1u) % NTRK];
    int r;
    switch (c) {
    case CC_ALWAYS: return 1;
    case CC_ELSE: return k ? !acted : 1;
    case CC_PRE: return t->c_pre;
    case CC_NPRE: return !t->c_pre;
    case CC_NEI: return nb->c_pre;
    case CC_NNEI: return !nb->c_pre;
    case CC_P25: case CC_P50: case CC_P75: r = rng() % 100u < 25u * (c - CC_P25 + 1u); break;
    case CC_FIRST: r = !p; break;
    case CC_NFIRST: r = p != 0u; break;
    case CC_FILL: r = seq_fill != 0u; break;
    case CC_NFILL: r = !seq_fill; break;
    case CC_NM: r = n <= m && p % m == n - 1u; break;
    case CC_NNM: r = !(n <= m && p % m == n - 1u); break;
    default: r = 1; break;
    }
    t->c_pre = (uint8_t)r;
    return r;
}
/* .. the passes so far (this one too) its condition held: TRANSPOSE's count (N:M, FIRST, ALWAYS from the pass; the
 * others, by chance or by hand, counted as they come: t->c_acc, the track's) */
static uint32_t comp_count(const track_t *t, const step_t *s, uint32_t k)
{
    uint32_t c = CO_COND(k ? s->comp2 : s->comp), nm = s->cnm[k & 1u], n = CNM_N(nm), m = CNM_M(nm), p = t->loops, h;
    switch (c) {
    case CC_ALWAYS: return p + 1u;
    case CC_FIRST: return 1u;
    case CC_NFIRST: return p;
    case CC_NM: case CC_NNM:
        h = n <= m ? (p + m + 1u - n) / m : 0u;
        return c == CC_NM ? h : p + 1u - h;
    default: return t->c_acc;
    }
}
/* SHIFT: a circular, non-destructive playback map. Conditions are pass based so
 * earlier destinations can be known before their source step is reached. */
static uint8_t shift_map[2][NTRK][NSTEP], shift_len[2][NTRK];
static uint16_t shift_pass[2][NTRK];
static int shift_cond(const track_t *t, const step_t *s, uint32_t k, uint32_t p)
{
    uint32_t c = CO_COND(k ? s->comp2 : s->comp), nm = s->cnm[k];
    if (c == CC_ALWAYS) return 1;
    if (c == CC_FIRST) return !p;
    if (c == CC_NFIRST) return p != 0u;
    if (c == CC_NM || c == CC_NNM) {
        uint32_t n = CNM_N(nm), m = CNM_M(nm), ok = n <= m && p % m == n - 1u;
        return c == CC_NM ? ok : !ok;
    }
    return 0; /* Conditions depending on future live events are not offered for SHIFT. */
}
static void shift_prepare_pass(const track_t *t, uint32_t len, uint32_t pass)
{
    uint32_t ti = trk_index(t) % NTRK, bank = pass & 1u, i, k, j;
    len = (uint32_t)clamp((int32_t)len, 1, NSTEP);
    if (shift_valid[bank][ti] && shift_pass[bank][ti] == pass && shift_len[bank][ti] == len) return;   /* (V10.7.6: the
                                                     * map kept first: seq_early asks every block and track) */
    int16_t move[NSTEP] = {0};
    const step_t *st = seq_steps(t);
    shift_valid[bank][ti] = 1; shift_pass[bank][ti] = (uint16_t)pass; shift_len[bank][ti] = (uint8_t)len;
    for (i = 0; i < len; i++) for (k = 0; k < 2u; k++) {
        uint32_t a = CO_ACT(k ? st[i].comp2 : st[i].comp);
        if (a == CA_SHIFT && shift_cond(t, &st[i], k, pass)) {
            int32_t d = (int32_t)((k ? st[i].carg2 : st[i].carg) % 127u) - 63;
            for (j = 0; j <= st[i].clen[k] && j < CL_MAX; j++)
                move[(i + j) % len] += (int16_t)d;
        }
    }
    for (i = 0; i < len; i++) {
        move[i] %= (int32_t)len;
        shift_map[bank][ti][i] = (uint8_t)(move[i] ? 255u : i);
    }
    for (i = 0; i < len; i++) if (move[i]) {
        int32_t dst = (int32_t)i + move[i];
        if (dst < 0) dst += (int32_t)len;
        if (dst >= (int32_t)len) dst -= (int32_t)len;
        shift_map[bank][ti][dst] = (uint8_t)i; /* Later moved source wins a collision. */
    }
}
static const step_t *shift_step_pass(const track_t *t, uint32_t idx, uint32_t pass)
{
    static const step_t rest = {{0}, 0, ST_REST};
    uint32_t ti = trk_index(t) % NTRK, bank = pass & 1u, src = seq_src(t, idx);
    if (shift_valid[bank][ti]) src = shift_map[bank][ti][src];
    return src == 255u ? &rest : &seq_steps(t)[src];
}

static const step_t *shift_step(const track_t *t, uint32_t idx)
{
    return shift_step_pass(t, idx, t->loops);
}
static const step_t *shift_ahead(const track_t *t, uint32_t offset, uint32_t len)
{
    uint32_t index = t->seq_idx + offset, pass;
    len = len ? len : 1u;
    pass = t->loops;
    if (index >= len) { index -= len; if (pass < 65535u) pass++; }
    shift_prepare_pass(t, len, pass);
    return shift_step_pass(t, index, pass);
}
/* SEQ's notes: the pattern's steps with notes (or hits), in order; the k-th of them (*cnt: how many) */
static const step_t *seq_unit(const track_t *t, uint32_t len, uint32_t k, uint32_t *cnt)
{
    const step_t *st = seq_steps(t), *r = 0;
    uint32_t i, n = 0;
    for (i = 0; i < len && i < NSTEP; i++)
        if (st[i].time == ST_NOTE && (st[i].n || st[i].hit)) {
            if (n == k) r = &st[i];
            n++;
        }
    *cnt = n;
    return r;
}
/* (0.9.9-seq2f) GLOBAL STEP COMPONENTS (GLO > COMPS, the approved mock-up's G1): a lane of GL_N steps on the transport's
 * 1/16 grid (clk_n: one bar, looping), one component a step (ACTION | CONDITION << 4, VALUE, LENGTH - 1, N:M as the
 * tracks', core.h CA_* CC_*) and the TRACKS it applies to (mask, bit k track k). Each global step's CONDITION is
 * evaluated once (gl_eval, from comp_run: the first track step on or past it), with its own passes (the bars since
 * PLAY) and PRE; one that holds starts a span of its LENGTH in global steps (gsp, GSP_N at once, the oldest goes),
 * which every masked track playing a step inside it takes once into its own spans (CSF_GLO; its LENGTH still counted
 * on the global grid). POLY (gl_mono 0): the global first, then the track's C1 / C2 over the result; MONO: while a
 * global acts on the track, its own components do not. An empty lane (gl_on 0): nothing, no state, no roll */
#define GL_N 16u
#define GSP_N 2u
#define CSF_GLO 0x80u                                   /* (cspan_t.act: a global's; base: its end, the global step & 255) */
typedef struct { uint8_t comp, val, len, nm, mask; } gcomp_t;
typedef struct { uint32_t g0; uint8_t act, val, len, mask, ser; int8_t mv; } gspan_t;
static gcomp_t glane[GL_N];
static uint8_t gl_mono, gl_on;                          /* MONO (1) / POLY (0); any step of the lane set */
static gspan_t gsp[GSP_N];
static uint8_t gsp_n, gl_ser, gl_pre, gl_acc, gl_took[NTRK];
static uint32_t gl_next;                                /* the next global step to evaluate */
static uint32_t gl_rs;                                  /* its own dice (the tracks' rolls untouched by it) */
static void gl_touch(void)                              /* (after an edit or a load) */
{
    uint32_t i, on = 0;
    for (i = 0; i < GL_N; i++) {
        if (CO_ACT(glane[i].comp) >= CA_N || CO_COND(glane[i].comp) >= CC_N || CO_ACT(glane[i].comp) == CA_RAMP)
            glane[i].comp = 0;                          /* (V10.6v: a JUMP of before, RAMP's code: a track's alone) */
        if (glane[i].len >= CL_MAX) glane[i].len = CL_MAX - 1u;
        glane[i].mask &= (1u << NTRK) - 1u;
        on |= CO_ACT(glane[i].comp) != 0u;
    }
    gl_mono &= 1u;
    gl_on = (uint8_t)on;
}
static void gl_reset(void) { gsp_n = gl_ser = gl_pre = gl_acc = 0; gl_next = 0; gl_rs = 0x2545F491u; memset(gl_took, 0, sizeof gl_took); }
/* (p1) a live recording into step s (rec_drum, rec_note): does it repeat as it plays? Its RATCH, a RATCHET of its own,
 * one acting on it now (a span of an earlier step's, the C.LEN lane's, the global lane's: comp_run's cx_rat), one of
 * the track's spanning more steps, or the global lane's on the track */
static __attribute__((noinline)) uint32_t rec_repeats(const track_t *t, const step_t *s)
{
    uint32_t i, k, len = t->p[P_SLEN] > 0 ? (uint32_t)t->p[P_SLEN] : 1u, ti = trk_index(t) % NTRK;
    if (step_ratchet(s) > 1u || CO_ACT(s->comp) == CA_RATCH || CO_ACT(s->comp2) == CA_RATCH)
        return 1;
    if (t->cx_rat > 1u && s == &t->step[seq_src(t, t->seq_idx % len)])
        return 1;
    for (i = 0; i < NSTEP; i++)
        for (k = 0; k < 2u; k++)
            if (CO_ACT(k ? t->step[i].comp2 : t->step[i].comp) == CA_RATCH && t->step[i].clen[k] &&
                t->step[i].clen[k] != CL_LEGACY)
                return 1;
    for (i = 0; i < GL_N; i++)
        if (CO_ACT(glane[i].comp) == CA_RATCH && ((glane[i].mask >> ti) & 1u))
            return 1;
    return 0;
}
/* the global step a track's step starting off samples into this block falls on (the nearest 1/16 of the transport) */
static uint32_t gl_step_at(uint32_t off)
{
    uint32_t p = div_samples(2);
    if (!p) p = 1u;
    return clk_pos == CLK_START ? (off + p / 2u) / p : clk_n + (clk_pos + off + p / 2u) / p;
}
static int gl_cond(const gcomp_t *c, uint32_t p)       /* its CONDITION on pass p (NEI: PRE; ELSE: always) */
{
    uint32_t k = CO_COND(c->comp), n = CNM_N(c->nm), m = CNM_M(c->nm);
    int r;
    switch (k) {
    case CC_ALWAYS: case CC_ELSE: return 1;
    case CC_PRE: case CC_NEI: return gl_pre;
    case CC_NPRE: case CC_NNEI: return !gl_pre;
    case CC_P25: case CC_P50: case CC_P75:
        gl_rs ^= gl_rs << 13; gl_rs ^= gl_rs >> 17; gl_rs ^= gl_rs << 5;
        r = (gl_rs >> 8) % 100u < 25u * (k - CC_P25 + 1u);
        break;
    case CC_FIRST: r = !p; break;
    case CC_NFIRST: r = p != 0u; break;
    case CC_FILL: r = seq_fill != 0u; break;
    case CC_NFILL: r = !seq_fill; break;
    case CC_NM: r = n <= m && p % m == n - 1u; break;
    case CC_NNM: r = !(n <= m && p % m == n - 1u); break;
    default: r = 1; break;
    }
    gl_pre = (uint8_t)r;
    return r;
}
static void gl_eval(uint32_t g)                         /* every global step up to g, once */
{
    if (g >= gl_next + 64u)                             /* (turned on while playing: from near here) */
        gl_next = g - 63u;
    for (; gl_next <= g; gl_next++) {
        const gcomp_t *c = &glane[gl_next % GL_N];
        uint32_t a = CO_ACT(c->comp), v = c->val, p = gl_next / GL_N, ok, i, n = 0;
        int32_t mv = 0;
        if (!a || !c->mask)
            continue;
        ok = (uint32_t)gl_cond(c, p);
        if (a == CA_TRANS) {
            mv = CV_TR_MOVE(v);
            if (CV_TR_RANGE(v) > 1u) {                  /* adding up, as a track's (its passes: the bars) */
                uint32_t k = CO_COND(c->comp), h, nn = CNM_N(c->nm), m = CNM_M(c->nm);
                gl_acc = (uint8_t)(gl_acc + ok);
                h = k == CC_ALWAYS ? p + 1u : k == CC_FIRST ? 1u : k == CC_NFIRST ? p :
                    k == CC_NM || k == CC_NNM ? 0u : gl_acc;
                if (k == CC_NM || k == CC_NNM) {
                    h = nn <= m ? (p + m + 1u - nn) / m : 0u;
                    if (k == CC_NNM) h = p + 1u - h;
                }
                mv *= (int32_t)(h ? (h - 1u) % CV_TR_RANGE(v) : 0u);
                ok = 1;
            }
        }
        if (!ok)
            continue;
        for (i = 0; i < gsp_n; i++)                     /* (the ones over: gone) */
            if (gsp[i].g0 + gsp[i].len > gl_next) gsp[n++] = gsp[i];
        gsp_n = (uint8_t)n;
        if (gsp_n == GSP_N) {
            gsp[0] = gsp[1];
            gsp_n--;
        }
        gsp[gsp_n].g0 = gl_next; gsp[gsp_n].act = (uint8_t)a; gsp[gsp_n].val = (uint8_t)v;
        gsp[gsp_n].len = (uint8_t)(c->len + 1u); gsp[gsp_n].mask = c->mask;
        gsp[gsp_n].ser = ++gl_ser; gsp[gsp_n].mv = (int8_t)clamp(mv, -100, 100);
        gsp_n++;
    }
}
/* track t (index ti) playing step s on global step g: the global spans it is under and has not taken, into its own */
static void gl_take(track_t *t, const step_t *s, uint32_t ti, uint32_t g)
{
    uint32_t i;
    for (i = 0; i < gsp_n; i++) {
        const gspan_t *G = &gsp[i];
        cspan_t *c;
        if (!((G->mask >> ti) & 1u) || g < G->g0 || g >= G->g0 + G->len || (int8_t)(G->ser - gl_took[ti]) <= 0)
            continue;
        gl_took[ti] = G->ser;
        if (G->act == CA_MUTE && G->val)
            trk_fade_out(t);
        if (G->act == CA_STUT)
            t->cvp = *s;
        if (t->csp_n == CSP_N) {
            uint32_t k;
            for (k = 1; k < CSP_N; k++) t->csp[k - 1u] = t->csp[k];
            t->csp_n--;
        }
        c = &t->csp[t->csp_n++];
        c->act = (uint8_t)(G->act | CSF_GLO); c->val = G->val; c->mv = G->mv; c->len = G->len;
        c->left = (uint8_t)(G->g0 + G->len - g);
        c->base = (uint8_t)(G->g0 + G->len);
    }
}
/* the step the track plays now, with its components: the spans before it a step older (gone after their LENGTH), this
 * step's own (C1, then C2: those whose CONDITION holds start a span of their LENGTH, this step its first), then what
 * every span does here, the oldest first: s itself when nothing replaces or silences it, else t->cvs (cv_on: a RATCH
 * part reads it again); the moves of its notes in t->cx_* (seq_step). Nothing on the track: s, no roll, no state */
static __attribute__((noinline)) const step_t *comp_run(track_t *t, const step_t *s, uint32_t len)   /* (flash: not
                                                    * in mix_block's RAM code) */
{
    uint32_t i, n = 0, k, acted = 0, rep = 0, rq = 0, rb = 0, arp = 0, av = 0, mute = 0, ti = trk_index(t) % NTRK, cnt;
    uint32_t g = 0, gm = 0, ps, grep = 0, chord = 0;
    const step_t *src = s;
    uint8_t probability = s->probability;   /* save before cvs can replace an aliased C.LEN input */
    t->cx_deg = t->cx_oct = 0;
    t->cx_rnd = t->cx_rat = t->cx_dec = t->cx_tch = t->cx_bend = t->cx_arp = t->cx_lock = 0;
    if (gl_on || gsp_n) {                               /* (seq2f) the global lane: evaluated up to here */
        g = gl_step_at(ev_off);
        gl_eval(g);
    }
    for (i = 0; i < t->csp_n; i++)
        if (t->csp[i].act & CSF_GLO) {                  /* (seq2f) a global's: its LENGTH on the global grid */
            uint32_t l = (uint8_t)(t->csp[i].base - (uint8_t)g);
            if (l >= 1u && l <= t->csp[i].len) {
                t->csp[n] = t->csp[i];
                t->csp[n++].left = (uint8_t)l;
            }
        } else if (t->csp[i].left > 1u) {
            t->csp[n] = t->csp[i];
            t->csp[n++].left--;
        }
    t->csp_n = (uint8_t)n;
    if (gsp_n)
        gl_take(t, s, ti, g);
    for (i = 0; i < t->csp_n; i++)
        gm |= (t->csp[i].act & CSF_GLO) != 0u;
    gm = gm && gl_mono;                                 /* MONO: a global acting here, the track's own do not */
    n = t->csp_n;
    if (!n && !comp_any(s)) {
        t->arp_from = 0;                            /* (an ARP's LENGTH over: a TIE after it holds its last note) */
        return s;
    }
    for (k = 0; k < 2u && !gm; k++) {
        uint32_t a = CO_ACT(k ? s->comp2 : s->comp), v = k ? s->carg2 : s->carg, l = s->clen[k], ok;
        int32_t mv = 0;
        cspan_t *c;
        if (!a || a >= CA_N)
            continue;
        if (a == CA_SHIFT) { acted |= shift_cond(t, s, k, t->loops); continue; }
        if (a == CA_RAMP)                           /* (V10.6v: no condition, no span: seq_tick's ramp_run) */
            continue;
        ok = (uint32_t)comp_cond(t, s, k, acted);
        if (a == CA_TRANS) {
            mv = CV_TR_MOVE(v);
            if (CV_TR_RANGE(v) > 1u) {              /* adding up: every pass, a place further each time it held */
                uint32_t h;
                t->c_acc = (uint8_t)(t->c_acc + ok);
                h = comp_count(t, s, k);
                mv *= (int32_t)(h ? (h - 1u) % CV_TR_RANGE(v) : 0u);
                ok = 1;
            }
        }
        if (a == CA_LOCK && !ok)
            t->cx_lock = 2;                         /* (its step's locks not this pass) */
        if (!ok)
            continue;
        acted = 1;
        if (a == CA_MUTE && v)
            trk_fade_out(t);                        /* CUT: what sounds goes (~3 ms) */
        if (a == CA_STUT)
            t->cvp = *s;
        if (t->csp_n == CSP_N) {                    /* (full: the oldest goes) */
            for (i = 1; i < CSP_N; i++) t->csp[i - 1u] = t->csp[i];
            t->csp_n--;
        }
        c = &t->csp[t->csp_n++];
        c->act = (uint8_t)a; c->val = (uint8_t)v; c->mv = (int8_t)mv;
        c->len = c->left = (uint8_t)(l < CL_MAX ? l + 1u : 1u);
        c->base = 0;
        if (a == CA_SEQ) {                          /* the note it starts from, a note further each pass */
            uint32_t per;
            seq_unit(t, len, 0, &cnt);
            per = (v & CV_SEQ_BOUNCE) && cnt > 1u ? 2u * cnt - 2u : cnt;
            c->base = (uint8_t)(per ? ((v & 63u) + t->loops) % per : 0u);
        }
    }
    for (ps = 0; ps < 2u; ps++)                     /* (seq2f) the globals first, then the track's (POLY: over them) */
    for (i = 0; i < t->csp_n; i++) {                /* what each does here, the oldest first (C1 before C2) */
        const cspan_t *c = &t->csp[i];
        uint32_t j = (uint32_t)(c->len - c->left), v = c->val;
        if (ps != !(c->act & CSF_GLO) || (ps && gm))
            continue;
        if (ps && !grep) grep = rep | 0x100u;           /* (what the globals left: an ARP of the track's keeps it) */
        switch (c->act & ~CSF_GLO) {
        case CA_ARP: arp = j ? 2u : 1u; av = v; rep = ps ? grep & 0xFFu : 0u; break;
        case CA_SEQ: rep = 1; rq = (uint32_t)c->base + j; rb = (v & CV_SEQ_BOUNCE) != 0u; arp = 0; break;
        case CA_STUT: rep = j % (v % 4u + 1u) ? 2u : 3u; arp = 0; break;
        case CA_OCTU: t->cx_oct = (int8_t)(t->cx_oct + 12 * (int32_t)(v % 3u + 1u)); break;
        case CA_OCTD: t->cx_oct = (int8_t)(t->cx_oct - 12 * (int32_t)(v % 3u + 1u)); break;
        case CA_RATCH: t->cx_rat = (uint8_t)CV_RAT_HITS(v); t->cx_dec = (uint8_t)CV_RAT_DEC(v); break;
        case CA_MUTE: mute = 1; break;
        case CA_TRANS: t->cx_deg = (int8_t)clamp(t->cx_deg + c->mv, -100, 100); break;
        case CA_RAND: if (v % 7u + 1u > t->cx_rnd) t->cx_rnd = (uint8_t)(v % 7u + 1u); break;
        case CA_LOCK: if (j && t->cx_lock != 2u) t->cx_lock = 1; break;
        case CA_SOUND: t->cx_tch = (uint8_t)(1u + v % 3u); break;
        case CA_BEND: t->cx_bend = (uint8_t)(1u + v % 25u); break;
        case CA_CHORD: chord = 1u + v % CD_POW; break;
        default: break;
        }
    }
    if (!arp)
        t->arp_from = 0;
    if (!rep && !arp && !mute && !chord)
        return s;
    if (rep == 2u || rep == 3u)                     /* STUTTER: its step again (3), or nothing between (2) */
        src = rep == 3u ? &t->cvp : s;
    else if (rep) {                                 /* SEQ: the pattern's k-th note here */
        uint32_t q = rq, per;
        seq_unit(t, len, 0, &cnt);
        per = rb && cnt > 1u ? 2u * cnt - 2u : cnt;
        if (per) {
            q %= per;
            src = seq_unit(t, len, q < cnt ? q : per - q, &cnt);
            if (!src) src = s;
        }
    }
    if (src != &t->cvs)
        t->cvs = *src;
    if (rep) {                                      /* the replacement keeps this position's timing and probability */
        t->cvs.cwhen = s->cwhen;
        t->cvs.flags = (uint8_t)((t->cvs.flags & ~SF_MFINE) | (s->flags & SF_MFINE));
        t->cvs.probability = probability;
        if (rep == 2u) { t->cvs.time = ST_REST; t->cvs.n = 0; t->cvs.hit = 0; }
    }
    /* CHORD changes the rendered copy only. The track's CHORD page, root and source stay intact. */
    if (chord && t->cvs.time == ST_NOTE && t->cvs.n)
        t->cvs.n = (uint8_t)chord_build_mode(t, t->cvs.note[0], t->cvs.note, chord);
    t->cvs.comp = t->cvs.carg = t->cvs.comp2 = t->cvs.carg2 = 0;
    if (arp == 1u) {                                /* ARP: from here (seq_step: cva, arp_from) */
        t->cvs.comp = CO_MAKE(CA_ARP, 0u);
        t->cvs.carg = (uint8_t)av;
        t->cx_arp = 1;
    } else if (arp == 2u) {                         /* .. the steps of its LENGTH: TIEs it goes on over */
        t->cvs.time = ST_TIE;
        t->cvs.probability = 0;
    }
    if (mute) {
        t->cvs.time = ST_REST; t->cvs.n = 0; t->cvs.hit = 0;
        t->cx_lock = 2;
    }
    t->cv_on = 1;
    return &t->cvs;
}
/* (V10.6v) RAMP's shape: x of the way (Q15, 0..32768) -> how far the value is (Q15): LIN, EXP (slow, then fast), LOG
 * (fast, then slow), S, STEPS (8 stairs), ZIGZAG (rising, small bounces back) */
static int32_t ramp_curve(uint32_t c, int32_t x)
{
    int32_t y = 32768 - x, f;
    switch (c) {
    case 1: return ((x * x >> 15) * x) >> 15;
    case 2: return 32768 - (((y * y >> 15) * y) >> 15);
    case 3: return (x * x >> 15) * ((98304 - 2 * x) >> 1) >> 14;
    case 4: f = x >> 12; return f >= 7 ? 32768 : f * 4681;
    case 5: f = (x * 6) & 32767; return clamp(x + (f < 16384 ? f : 32768 - f) / 4, 0, 32768);
    default: return x;
    }
}
/* (V10.6v) RAMP on track t: s the step starting now (play: it plays; its RAMP launches the glide of the step's LOCK, the
 * first one, unless that glide still runs from this step: it goes on), 0 none; then n samples on: the value into its
 * parameter (motion.c's records and locks move it, this after them: the ramp's value wins), at its end the knob's back */
static __attribute__((noinline)) void ramp_run(track_t *t, const step_t *s, uint32_t play, uint32_t n)
{
    uint32_t ti = trk_index(t) % NTRK, k, id, u, b, st = t->seq_idx % NSTEP, bars;
    ramp_t *r = &rmp[ti];
    const motion_event_t *e;
    int32_t from, to, v, base;
    if (!motion_enabled(t)) {                       /* (its records off: so are its locks; motion_restore the values) */
        r->id = 0;
        return;
    }
    for (k = 0; s && play && k < 2u; k++) {
        uint32_t val = k ? s->carg2 : s->carg, l = s->clen[k];
        if (CO_ACT(k ? s->comp2 : s->comp) != CA_RAMP)
            continue;
        if (!(e = motion_lock_at(t, st)))           /* (no lock: nothing) */
            break;
        id = MOTION_ID(e) + 1u;
        if (r->id == id && r->st == st &&           /* (running from here, more than half a step to go: on) */
            r->pos + RAMP_ONE / (2u * (val & CV_RAMP_BARS ? (l % 16u + 1u) << 4 : l + 1u)) < RAMP_ONE)
            break;
        if (r->id && r->id != id)
            t->p[r->id - 1u] = motion_base[ti][r->id - 1u];   /* (another one: its knob's value back) */
        r->id = (uint8_t)id; r->st = (uint8_t)st; r->val = (uint8_t)val; r->len = (uint8_t)l;
        r->pos = 0; r->to = MOTION_VAL(e);
        break;
    }
    if (!r->id)
        return;
    id = r->id - 1u;
    motion_active[ti][id / 32u] |= 1u << (id % 32u);   /* (a stop puts the knob's value back; seq_tick runs this after
                                                     * each step's motion_step: its value over the lock's unlock) */
    base = motion_base[ti][id];
    to = r->to;
    b = r->pos - RAMP_ONE;
    if (r->pos < RAMP_ONE) {                        /* (on its way; the values between its ends: no fit) */
        const param_desc_t *d = param_desc_of(eng_idx(t->eng_req), id);
        bars = (r->val & CV_RAMP_BARS) != 0u;
        u = div_samples(bars ? 2u : (uint32_t)t->p[P_SDIV]) * (bars ? (r->len % 16u + 1u) << 4 : r->len + 1u);
        r->pos += (n << 16) / ((u >> 8) | 1u);
        if (r->pos > RAMP_ONE)
            r->pos = RAMP_ONE;
        from = CV_RAMP_OPP(r->val) ? (to >= (d->min + d->max) >> 1 ? d->min : d->max) : base;
        b = (uint32_t)ramp_curve(CV_RAMP_CURVE(r->val), (int32_t)(r->pos >> 9)) >> 5;
    } else if (b >= RAMP_BACK) {                    /* (its end: the knob's) */
        t->p[id] = (int16_t)base;
        r->id = 0;
        return;
    } else {                                        /* (then the knob's value back, RAMP_BACK samples) */
        r->pos += n;
        from = to;
        to = base;
    }
    v = from + ((to - from) * (int32_t)b >> 10);
    t->p[id] = (int16_t)v;
}
/* (called through a pointer: seq_tick is inlined in RAM code, out of a direct call's reach of the image's end) */
static void (*const volatile RAMP_RUN)(track_t *, const step_t *, uint32_t, uint32_t) = ramp_run;
/* (seq2d) a legacy component (CP_* c, its argument a; projects before seq2d, GEN, DICE) into slot k of step i of st as
 * ACTION LENGTH CONDITION VALUE: ev the step's EVERY (both components'), len the pattern's; other: the other slot is
 * free (OCT's ALT takes it). LENGTH: an ARP's over the TIEs after it, TONAL's to the loop's end, PULSE's over the
 * empty steps after it. Lossy (the manual): EVERY beside a TRIG / FILL / an alternating OCT / a WALK at a RATE (their
 * own condition wins), OCT RND (50 %), ALT without a free slot, WALK / CLIMB UP-DN and RND (UP), CLIMB as 7 degrees,
 * KILL (MUTE CUT: the step silent now), ARP SEQ (the SEQ walker), JUMP x2 (once a pass) */
static void cm_one(step_t *st, uint32_t i, uint32_t k, uint32_t c, uint32_t a, uint32_t ev, uint32_t len, int other)
{
    step_t *s = &st[i];
    uint32_t act = CA_NONE, cond = CC_ALWAYS, nm = 0, l = 0, v = 0, own = 0, j;
    const uint8_t *e = EV_AB[ev % EV_N];
    switch (c) {
    case CP_ARPN: case CP_ARPS: case CP_ARPQ:
        act = c == CP_ARPQ ? CA_SEQ : CA_ARP;
        v = c == CP_ARPQ ? 0u : (a & 0x7Fu) | (c == CP_ARPS ? CV_ARP_SCALE : 0u);
        for (j = 1; i < len && j < len && j < CL_MAX && st[(i + j) % len].time == ST_TIE; j++) l++;
        break;
    case CP_TRIG: {
        const uint8_t *g = CP_TRIGS[a % CP_NTRIG];
        act = CA_MUTE; own = 1;
        if (!g[0]) cond = g[1] == 1u ? CC_NFIRST : CC_FIRST;
        else { cond = CC_NNM; nm = CNM_MAKE(g[0], g[1]); }
        break;
    }
    case CP_RAND: act = CA_RAND; v = (a % 8u ? a % 8u : 1u) - 1u; break;
    case CP_OCT:
        act = a % 6u == 1u || a % 6u == 3u ? CA_OCTD : CA_OCTU;
        if (a % 6u >= 2u) {
            own = 1;
            cond = a % 6u == 5u ? CC_P50 : CC_NM;
            nm = a % 6u == 4u ? CNM_MAKE(1u, 2u) : CNM_MAKE(2u, 2u);
            if (a % 6u == 4u && other) {            /* ALT: down on the other passes, in the free slot */
                if (k) { s->comp = CO_MAKE(CA_OCTD, CC_NM); s->carg = 0; s->clen[0] = 0; s->cnm[0] = CNM_MAKE(2u, 2u); }
                else { s->comp2 = CO_MAKE(CA_OCTD, CC_NM); s->carg2 = 0; s->clen[1] = 0; s->cnm[1] = CNM_MAKE(2u, 2u); }
            }
        }
        break;
    case CP_TONAL: act = CA_TRANS; v = CV_TR_MAKE((int32_t)(a % 15u) - 7, 1u); l = i < len ? len - 1u - i : 0u; break;
    case CP_BEND: act = CA_BEND; v = a % 25u; break;
    case CP_ECHO: act = CA_RATCH; v = ((a & 127u) % 7u) | (a & CP_ECHO_EVEN ? 0u : 1u << 3); break;
    case CP_PULSE:
        act = CA_STUT; v = a % 4u;
        for (j = 1; i < len && j < len && j < CL_MAX; j++, l++) {
            const step_t *o = &st[(i + j) % len];
            if (o->time == ST_NOTE && (o->n || o->hit)) break;
        }
        break;
    case CP_FILL: act = CA_MUTE; own = 1; cond = CC_NFILL; break;
    case CP_KILL: act = CA_MUTE; v = 1; break;
    case CP_WALK: case CP_CLIMB: {
        uint32_t r = CP_WRATES[CP_WRATE(a) % CP_NWRATE], g = CP_WRNG(a);
        int32_t m = c == CP_WALK ? 1 : 7;
        act = CA_TRANS;
        v = CV_TR_MAKE(CP_WMODE(a) == 1u ? -m : m, (c == CP_WALK ? CP_WDEGS[g] : g + 1u) + 1u);
        if (r > 1u) { own = 1; cond = CC_NM; nm = CNM_MAKE(1u, r); }
        break;
    }
    case CP_TOUCH: act = CA_SOUND; v = a % 3u; break;
    default: break;
    }
    if (e[0] == 0u && e[1] == 3u)                   /* EVERY SKIP: never */
        act = CA_NONE;
    else if (!own && e[1]) {
        if (e[0]) { cond = CC_NM; nm = CNM_MAKE(e[0], e[1]); }
        else cond = e[1] == 1u ? CC_FIRST : CC_NFIRST;
    }
    if (l >= CL_MAX) l = CL_MAX - 1u;
    if (k) { s->comp2 = act ? CO_MAKE(act, cond) : 0u; s->carg2 = act ? (uint8_t)v : 0u; s->clen[1] = (uint8_t)l; s->cnm[1] = (uint8_t)nm; }
    else { s->comp = act ? CO_MAKE(act, cond) : 0u; s->carg = act ? (uint8_t)v : 0u; s->clen[0] = (uint8_t)l; s->cnm[0] = (uint8_t)nm; }
}
/* every step of st (a pattern of len) with a slot marked CL_LEGACY: its CP_* as v2; EVERY (cwhen's bits 0-3) gone */
static void comp_migrate(step_t *st, uint32_t len)
{
    uint32_t i;
    if (!len || len > NSTEP) len = NSTEP;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &st[i];
        uint32_t c1 = s->comp, a1 = s->carg, c2 = s->comp2, a2 = s->carg2, ev = s->cwhen & 15u;
        int l1 = s->clen[0] == CL_LEGACY, l2 = s->clen[1] == CL_LEGACY;
        if (!l1 && CO_ACT(c1) == CA_RAMP && !CV_RAMP_OK(a1))   /* (V10.6v) a JUMP of before whose VALUE is no RAMP: */
            s->comp = 0;                            /* none */
        if (!l2 && CO_ACT(c2) == CA_RAMP && !CV_RAMP_OK(a2))
            s->comp2 = 0;
        if (!l1 && !l2)
            continue;
        if (l1) { s->comp = s->carg = 0; s->clen[0] = s->cnm[0] = 0; }
        if (l2) { s->comp2 = s->carg2 = 0; s->clen[1] = s->cnm[1] = 0; }
        if (l1) cm_one(st, i, 0, c1 < CP_N ? c1 : 0u, a1, ev, len, l2 ? !c2 : !CO_ACT(s->comp2));
        if (l2) cm_one(st, i, 1, c2 < CP_N ? c2 : 0u, a2, ev, len, !CO_ACT(s->comp));
        s->cwhen &= 0xF0u;
    }
}
/* a legacy component into slot k of s (GEN, DICE: comp_migrate after the pattern is written) */
static void comp_legacy(step_t *s, uint32_t k, uint32_t c, uint32_t a)
{
    if (k) { s->comp2 = (uint8_t)c; s->carg2 = (uint8_t)a; s->clen[1] = CL_LEGACY; }
    else { s->comp = (uint8_t)c; s->carg = (uint8_t)a; s->clen[0] = CL_LEGACY; }
}
/* (0.6.39) the step whose ARP component plays at s: s itself (a NOTE step with one), or the one a TIE continues
 * (arp_from); 0: none (a drum kit, the slices: never) */
static int trk_kit(const track_t *t)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    return e->oneshot || (e->keys && e->keys(t, 0) >= 0);
}
/* (0.6.47) C.LEN (P_CLEN 1..64): the step s with the component of the lane's next step (steps 1..C.LEN in turn, one a
 * step the track plays, apart from where the steps are: the components drift over a pattern of another length);
 * OFF: s as it is. The copy is the track's (cvs: a RATCH repeats it); an ARP or a PULSE keeps theirs (CV_IDX) */
#define CV_IDX 255u
/* (0.6.88) TIMING early: how much before its place the step after the playing one starts (its ST_MICRO < 0), so the
 * playing one ends that much sooner (seq_tick); 0 when the next step is not known yet (a JUMP or REV coming, RANDOM
 * held, a song's section about to change) */
static uint32_t seq_early(const track_t *t, uint32_t period, uint32_t len)
{
    uint32_t ni = (t->seq_idx + 1u) % (len ? len : 1u);
    const step_t *s;
    int32_t m;
    if (t->jump_to || (t->pfx & PFX_RND) || (!ni && (chain.running || chain.armed)))
        return 0;
    s = shift_ahead(t, 1u, len);
    if (s->time != ST_NOTE || (!s->n && !s->hit))       /* (nothing to play there: never early) */
        return 0;
    m = ST_MICRO(s);
    if (m < 0) {
        uint32_t early = fit_scale_samples(step_samples(t, period, ni), (uint32_t)-m, 6u);
        return (t->seq_fit & 128u) && early > 65535u ? 65535u : early;
    }
    return 0u;   /* (0.9.1: 64ths) */
}
static const step_t *comp_lane(track_t *t, const step_t *s)
{
    uint32_t cl = (uint32_t)clamp(t->p[P_CLEN], 0, NSTEP);
    const step_t *c;
    t->cv_on = 0;
    if (!cl)
        return s;
    c = &seq_steps(t)[t->cpos % cl];
    t->cpos = (uint8_t)((t->cpos + 1u) % cl);
    t->cvs = *s;
    t->cvs.comp = c->comp;
    t->cvs.carg = c->carg;
    t->cvs.comp2 = c->comp2;                        /* (0.6.82) the second one, their WHENs, DIV and LEN */
    t->cvs.carg2 = c->carg2;
    t->cvs.cwhen = (uint8_t)((c->cwhen & 15u) | (s->cwhen & 0xF0u));   /* (0.6.88: the step's own TIMING kept) */
    t->cvs.ctime = c->ctime;
    t->cvs.clen[0] = c->clen[0]; t->cvs.clen[1] = c->clen[1];   /* (seq2d) their LENGTHs and N:Ms */
    t->cvs.cnm[0] = c->cnm[0]; t->cvs.cnm[1] = c->cnm[1];
    t->cv_on = 1;
    return &t->cvs;
}
static const step_t *arp_owner(const track_t *t, const step_t *s)
{
    const step_t *o = s;
    if (s->time == ST_TIE && t->arp_from == CV_IDX)
        o = &t->cva;
    else if (s != &t->cvs || !t->cx_arp)            /* (seq2d: the ARP comp_run started here, its step in cvs) */
        return 0;
    return o->time == ST_NOTE && (o->n || (o->hit && trk_kit(t))) && CO_ACT(o->comp) == CA_ARP ? o : 0;   /* (0.6.82:
                                                         * a kit's hits too) */
}
/* the parts a step plays (*div: in how many equal parts of its length they fall, one each from the start): an ARP's
 * RATE, else its ECHO (0.6.42: x2..x4; 0.6.82: .. x8; an ARP wins over it), else its RATCH. (0.6.82) DIV: ECHO's and
 * RATCH's parts a step/2 .. /8 apart, as many as fit (0: the step shared out) */
static uint32_t step_parts(const track_t *t, const step_t *s, uint32_t *div)
{
    const step_t *o = arp_owner(t, s);
    uint32_t h, d;
    if (o)
        return *div = CP_RATES[CP_RATE(o->carg) % CP_NRATE];
    h = s->time == ST_NOTE && t->cx_rat ? t->cx_rat : step_ratchet(s);   /* (seq2d: a RATCHET, x2..x8) */
    if ((d = live_rat(t)) > h)                      /* (p2) LIVE RATCH: every step at least that many parts, inside it */
        return *div = d;
    d = CT_DIVS[CT_DIV(s) % CT_NDIV];
    if (h < 2u || !d)
        return *div = h;
    *div = d;
    return h < d ? h : d;
}
/* an ARP's k-th of n in its ORDER: UP, DOWN, UP-DN, RND */
static uint32_t cp_order(uint32_t o, uint32_t k, uint32_t n)
{
    if (!n) return 0;
    if (o == 1u) return n - 1u - k % n;                 /* DOWN */
    if (o == 2u && n > 1u) { k %= 2u * n - 2u; return k >= n ? 2u * n - 2u - k : k; }   /* UP-DN */
    if (o == 3u) return rng() % n;                      /* RND */
    return k % n;                                       /* UP */
}
/* (0.6.82) an ARP on a drum kit, its k-th part: NOTES the step's sounds one after another, SEQ the pattern's in the
 * order it plays them, SCALE all of the step's at once, each part a degree further up the scale (*deg); into nn */
static uint32_t cp_kit_arp(const track_t *t, const step_t *s, uint32_t c, uint32_t a, uint32_t k, uint8_t *nn,
                           int32_t *deg)
{
    uint8_t l[16];
    uint32_t n = 0, i, m, cnt = 1;
    const step_t *src = s;
    *deg = 0;
    if (c == CP_ARPQ) {
        src = seq_steps(t);
        cnt = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    }
    for (m = 0; m < cnt && m < NSTEP; m++) {
        if (src[m].time != ST_NOTE) continue;
        for (i = 0; i < src[m].n && i < 4u; i++)
            if (n < sizeof l && (!n || l[n - 1u] != src[m].note[i])) l[n++] = src[m].note[i];
        for (i = 0; i < NLANE; i++)
            if (((src[m].hit >> i) & 1u) && n < sizeof l && (!n || l[n - 1u] != DRUM_LANE_NOTE[i]))
                l[n++] = DRUM_LANE_NOTE[i];
    }
    if (!n) return 0;
    if (c == CP_ARPS) {
        *deg = (int32_t)cp_order(CP_ORD(a), k, CP_DEGS[CP_RNG(a)]);
        for (i = 0; i < n && i < 4u + NLANE; i++) nn[i] = l[i];
        return n < 4u + NLANE ? n : 4u + NLANE;
    }
    nn[0] = l[cp_order(CP_ORD(a), k, n)];
    return 1;
}
/* (0.6.39) an ARP component's next note: its notes (NOTES: the step's, low to high; SEQ: the pattern's, 0.6.82: in the
 * order the pattern plays them, its melody fast; both over RANGE octaves; SCALE: the scale's degrees up from the step's lowest note), the k-th in its ORDER */
static uint32_t cp_arp_note(const track_t *t, const step_t *s, uint32_t c, uint32_t a, uint32_t k)
{
    uint8_t base[16];
    uint32_t nb = 0, n, i, j, m, o2, o = CP_ORD(a), len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    if (c == CP_ARPS) {
        n = CP_DEGS[CP_RNG(a)];
    } else {
        const step_t *src = c == CP_ARPQ ? seq_steps(t) : s;
        uint32_t cnt = c == CP_ARPQ ? len : 1u;
        for (m = 0; m < cnt; m++)
            for (i = 0; i < src[m].n && i < 4u && src[m].time == ST_NOTE; i++) {
                uint8_t x = src[m].note[i];
                if (c == CP_ARPQ) {                     /* (0.6.82) SEQ: the pattern's melody as it plays (a note */
                    if (nb < sizeof base && (!nb || base[nb - 1u] != x))   /* again at once: one), not sorted */
                        base[nb++] = x;
                    continue;
                }
                for (j = 0; j < nb && base[j] < x; j++)
                    ;
                if (j < nb && base[j] == x) continue;
                if (nb == sizeof base) break;
                for (o2 = nb; o2 > j; o2--) base[o2] = base[o2 - 1u];   /* (no memmove on the target) */
                base[j] = x; nb++;
            }
        if (!nb) return s->n ? s->note[0] : 60u;
        n = nb * (CP_RNG(a) + 1u);
    }
    k = cp_order(o, k, n);
    if (c == CP_ARPS) {
        uint32_t lo = 127u;
        for (i = 0; i < s->n && i < 4u; i++) if (s->note[i] < lo) lo = s->note[i];
        return (uint32_t)clamp(scale_steps(t, (int32_t)lo, (int32_t)k), 0, 127);
    }
    return (uint32_t)clamp(base[k % nb] + 12 * (int32_t)(k / nb), 0, 127);
}
static void seq_release(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->seq_n; i++)
        seq_note_off(t, t->seq_notes[i]);
    for (i = t->ss_i; i < t->ss_n; i++)             /* (V10.6z4) a slide's note-off still queued (seq_step) */
        if (!t->ss_vv[i])
            seq_note_off(t, t->ss_nn[i]);
    t->seq_n = 0;
    t->seq_hold = 0;
    t->slide_glide = 0;                             /* live MONO / LEG keys must not glide after it */
    t->ss_n = t->ss_i = 0;                          /* (0.6.84) a strum still coming in: never */
}

static void seq_stop(void)
{
    uint32_t i;
    if (song.playing && mo_clock_on())             /* (0.6.72) MIDI OUT CLOCK / ALL: STOP */
        mo_send(0xFCu, 0, 0);
    song.playing = 0;
    cin_left = 0;                                  /* (a count-in: cancelled, nothing recorded) */
    cin_flush = 0;
    memset(cin_n, 0, sizeof cin_n);
    if (song.g[G_CLOCK])                           /* external clock: the arp stops with the transport, */
        for (i = 0; i < NPART; i++)                /* its sounding note too (HOLD keeps the latched chord) */
            if (trk[i].arp_note)
                arp_off(&trk[i]);
    for (i = 0; i < NTRK; i++) {
        seq_release(&trk[i]);
        trk[i].rh_n = 0;                           /* a recorded note held over the stop: as far as it got */
        trk[i].rat_left = 0;
        trk[i].arp_from = 0;
        trk[i].csp_n = trk[i].jump_to = 0;   /* (seq2d: no component span) */
        trk[i].cpos = trk[i].cv_on = 0;
    }
    fg_reset();                                    /* (0.6.52) FUGUE's heads silent */
    mo_flush();                                    /* (0.6.72) MIDI OUT ALL: nothing left hanging */
    chain_stop();
    motion_end();
}

/* play one step: TIE extends, REST releases, NOTE (re)triggers its notes, then its lane hits (their GM
 * notes, DRUM_LANE_NOTE; an accented hit at 127); a SLIDE on the previous step makes this one legato with a
 * glide (acid style). skip: bit k = note k, bit 8 + l = the hit of lane l already sounds from live
 * recording (not triggered, not released here); SEQ_REP: a RATCH repeat (seq_ratchet).
 * RATCH x2..x4 (core.h step_ratchet): a NOTE step plays all of it again in equal parts of its swung length, each
 * part retriggered with GATE of the part; the chance is rolled once for the whole step (a failed roll: no part),
 * a slide into it glides into the first part only, and it never slides or ties out (its last part ends at its
 * gate). x1, what every older pattern holds, is the step exactly as before. */
#define SEQ_REP (1u << 16)
#define SEQ_ROLLED (1u << 17)   /* seq_tick rolled the chance already (before the step's parameter locks): it passed */
#define SEQ_MISS (1u << 18)     /* .. it failed: nothing plays */
/* (JIANT 0.6.81) a chord's slide on a POLY track (SLD on a chord step): its notes glide to the next chord's, low to
 * high, none struck again; the next chord with more notes: each extra one rises from the nearest of the old (its
 * octave nearest), fading in; with fewer: each old one left glides to the nearest new one, fading out as it goes (its
 * release). The glide over three quarters of the step */
static int32_t cg_near(int32_t x, const uint8_t *set, uint32_t n, int other)   /* the note of set, in the octave
                                                         * nearest x (other: never x itself, 0.6.82) */
{
    uint32_t i, o;
    int32_t best = x, bd = 1000;
    for (i = 0; i < n; i++)
        for (o = 0; o < 2u; o++) {                      /* (its octave nearest, and the next nearest) */
            int32_t y = set[i] + 12 * ((x - set[i] + 6 + 1200) / 12 - 100), d;
            if (o) y += y > x ? -12 : 12;
            d = y > x ? y - x : x - y;
            if (other && !d) continue;
            if (d < bd) { bd = d; best = y; }
        }
    return clamp(best, 0, 127);
}
static int32_t glide_q8(int32_t d, uint32_t ticks)   /* (0.6.82) a glide's step over ticks: gstep, Q8 */
{
    int32_t g = ((d < 0 ? -d : d) * 256 + (int32_t)ticks - 1) / (int32_t)(ticks ? ticks : 1u);   /* (rounded up: there in time) */
    return g ? g : 1;
}
/* (0.6.82) the samples from this step to the next that is not a TIE: a chord's slide lasts as long as the chord */
static uint32_t tie_span(const track_t *t, uint32_t period, uint32_t len)
{
    uint32_t n = step_samples(t, period, t->seq_idx), k;
    for (k = 1; k < len; k++) {
        uint32_t i = (t->seq_idx + k) % len;
        if (shift_ahead(t, k, len)->time != ST_TIE)
            break;
        n += step_samples(t, period, i);
    }
    return n;
}
static void chord_glide(track_t *t, const uint8_t *nn, const uint8_t *vv, uint32_t m, uint32_t sk, uint32_t ssamp)
{
    uint8_t o[8], n[8], nv[8];
    uint32_t a = 0, b = 0, i, j, k, ti = trk_index(t) % NTRK;
    uint32_t ticks = (uint32_t)((t->p[P_GLIDE] > 0 ? (uint64_t)ssamp * (uint32_t)t->p[P_GLIDE] / 127u :
                                 (uint64_t)ssamp * 3u / 4u) / CTL);   /* (GLIDE: that much of the chord, its TIEs
                                                         * too (0.6.82); SLD alone: three quarters) */
    int32_t tr = trk_trans(t);
    if (!ticks) ticks = 1;
    for (i = 0; i < t->seq_n && a < 8u; i++)            /* (0.6.81) the old chord: its notes still sounding (a voice
                                                         * taken by another note is not one) */
        for (k = 0; k < NVOICE; k++)
            if (t->v[k].active && t->v[k].gate && t->v[k].note == t->seq_notes[i]) { o[a++] = t->seq_notes[i]; break; }
    for (i = 0; i < m && b < 8u; i++)
        if (!((sk >> i) & 1u)) { n[b] = nn[i]; nv[b++] = vv[i]; }
    for (i = 1; i < a; i++) for (j = i; j > 0 && o[j] < o[j - 1]; j--) { uint8_t x = o[j]; o[j] = o[j - 1]; o[j - 1] = x; }
    for (i = 1; i < b; i++) for (j = i; j > 0 && n[j] < n[j - 1]; j--) {
        uint8_t x = n[j]; n[j] = n[j - 1]; n[j - 1] = x; x = nv[j]; nv[j] = nv[j - 1]; nv[j - 1] = x; }
    for (i = 0; i < a || i < b; i++) {
        voice_t *v = 0;
        if (i < a)
            for (k = 0; k < NVOICE; k++)
                if (t->v[k].active && t->v[k].gate && t->v[k].note == o[i]) { v = &t->v[k]; break; }
        if (i < a && i < b && v) {                      /* a pair: the old note glides to the new */
            int32_t d;
            if (mo_out[ti][o[i]]) { mo_send(0x80u | trk_midi_ch(ti), mo_out[ti][o[i]] - 1u, 0); mo_out[ti][o[i]] = 0; }
            v->note = n[i];
            glide_from_trk(t, v);                       /* (0.9.8) VOICE's S-curve from where it sounds */
            v->pitch16 = ((int32_t)n[i] + tr) * 16 + t->pfx_rpit;
            d = v->pitch16 - v->pitch_cur;
            v->gstep = glide_q8(d, ticks);
            if (mo_lvl == MO_ALL && mo_plays(t, n[i])) {
                mo_out[ti][n[i]] = (uint8_t)(n[i] + 1u);
                mo_send(0x90u | trk_midi_ch(ti), n[i], nv[i] ? nv[i] : 1u);
            }
        } else if (i < b) {                             /* a new one: from the nearest old note, fading in */
            uint32_t a0 = vage;
            seq_note_on(t, n[i], nv[i]);
            if (vage != a0 && a)
                for (k = 0; k < NVOICE; k++)
                    if (t->v[k].age == vage && t->v[k].active) {
                        voice_t *w = &t->v[k];
                        int32_t d, pc = (cg_near(n[i], o, a, 1) + tr) * 16 + t->pfx_rpit;
                        d = ((int32_t)n[i] + tr) * 16 + t->pfx_rpit - pc;   /* (pitch16 - pitch_cur, as voice_start sets it) */
                        if (w->sub && (w->pn_flags & PN_OLD)) {   /* (ctl64) restarting later in the block: with it */
                            w->pn_flags |= PN_GLIDE;
                            w->pn_pcur = pc;
                            w->pn_gstep = glide_q8(d, ticks);
                            w->pn_fin = (uint16_t)(d < 0 ? -d : d);
                            continue;
                        }
                        w->pitch_cur = pc;
                        w->gfrom = (int16_t)w->pitch_cur;   /* (0.9.8: VOICE's glide from there) */
                        w->gstep = glide_q8(d, ticks);
                        w->gacc = 0;
                        w->fin_d = (uint16_t)(d < 0 ? -d : d);   /* (in as it glides; 0.6.82) */
                    }
        } else if (v) {                                 /* an old one left: to the nearest new note, fading out */
            int32_t d;
            glide_from_trk(t, v);
            v->pitch16 = (cg_near(o[i], n, b, 0) + tr) * 16 + t->pfx_rpit;
            d = v->pitch16 - v->pitch_cur;
            v->gstep = glide_q8(d, ticks);
            seq_note_off(t, o[i]);
        }
    }
    t->slide_glide = 0;
}

/* (0.6.81) GLIDE on a POLY track: every chord glides into the next by itself (no SLD needed): its notes held to the next
 * step when that one plays notes */
static int chord_auto(const track_t *t, uint32_t len)
{
    const step_t *nx;
    if (t->p[P_VOICE] != V_POLY || t->p[P_GLIDE] <= 0)
        return 0;
    nx = shift_ahead(t, 1u, len);
    return nx->time == ST_NOTE && nx->n;
}

/* (0.6.87) HUMAN: SEQ > TRACK's HUMAN amount (0..127; stored 127 - it in P_HVEL: 127 none; 0.6.92: one knob, it moves
 * each note's velocity, start, gate, decay and release); its own random numbers (never libc's rng: the UI's dice keep
 * theirs) */
static uint32_t hum_rs = 0x2545F491u;
static uint32_t hum_rnd(void) { hum_rs ^= hum_rs << 13; hum_rs ^= hum_rs >> 17; hum_rs ^= hum_rs << 5; return hum_rs; }
static uint32_t hum_amt(const track_t *t, uint32_t id)
{
    if (FELUCCA_FM4 && t->eng_req % NENGINES == ENGI_DIGITAL) return 0;
    return 127u - (uint32_t)clamp(t->p[id], 0, 127);
}
static int32_t hum_pm(uint32_t r) { return (int32_t)(hum_rnd() % (2u * r + 1u)) - (int32_t)r; }   /* -r .. r */
#define HUM_TMAX (FS * 40u / 1000u)                 /* at 100 %: up to 40 ms late (0.6.92: was 30) */
/* (p1, main 0.9.18) an entry of a step left out of its repeats: on a kit lane l recorded once (LM_ONCE) or with its own
 * x2 x4 x8; on a synth note l (0..3) overdubbed once (its lm slot l LM_ONCE: rec_note) */
static int lm_once(const step_t *s, uint32_t l, uint32_t kit)
{
    uint32_t r = step_lm_raw(s, l % NLANE);
    return r == LM_ONCE || (kit && lm_hits(r) > 1u);
}
static __attribute__((noinline)) void seq_step(track_t *t, const step_t *s, uint32_t period, uint32_t skip)
{
    uint32_t unit = t->seq_fit & 128u ? step_samples(t, period, t->seq_idx) : period;
    uint32_t i, j, gate = fit_scale_samples(unit, (uint32_t)t->p[P_SGATE], 7u);
    uint32_t vel = STEP_VEL(t, s);
    uint32_t slide_in = t->seq_hold && t->seq_n && !(skip & SEQ_REP);
    uint32_t len = t->p[P_SLEN] ? (uint32_t)t->p[P_SLEN] : 1u;
    uint32_t next_tie = shift_ahead(t, 1u, len)->time == ST_TIE;
    uint32_t div, hits = step_parts(t, s, &div);
    const step_t *arp = arp_owner(t, s);            /* (0.6.39) an ARP component: one of its notes a part */
    uint8_t aa = 0, ra = t->cx_rnd, ba = (uint8_t)(t->cx_bend ? t->cx_bend - 1u : 0u);   /* (seq2d: comp_run's) */
    uint32_t ac = 0, rnd = t->cx_rnd, bend = t->cx_bend;
    int32_t octs = t->cx_oct;
    int cont = 0;                                   /* .. going on over a TIE */
    uint8_t nn[4 + NLANE], vv[4 + NLANE];           /* the notes it plays: its notes, then its hits */
    uint32_t m = 0, sk = 0, sk0, hdel = 0, once = 0;
    /* QNT SEQ: the step's notes (not the lane hits) snap to the scale as they play; never on a drum kit, the slices
     * or another engine that maps the keys itself. seq_notes keeps the notes that sound: their note-offs match */
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t kit = e->oneshot || (e->keys && e->keys(t, 0) >= 0);
    uint32_t qseq = t->p[P_QUANT] == QN_SEQ && !kit;
    int32_t up = (kit ? 0 : t->p[P_POFS] + live_up(t)) +   /* SHIFT PIT: the notes up (a kit keeps its lanes); LIVE OCT */
                 (t->p[P_AMODE] == AM_TRNS ? t->trn : 0);   /* ARP TRNS: the keys' interval */
    if (!(skip & SEQ_REP)) {
        t->rat_left = 0;
        t->lr_on = 0;
        if ((skip & SEQ_MISS) ||
            (!(skip & SEQ_ROLLED) && !step_rolls(t, s))) {
            seq_release(t);
            return;
        }
    }
    if (s->time == ST_TIE && arp) {
        s = arp;                                    /* (0.6.39) the arpeggio goes on over the TIE */
        cont = 1;
        vel = STEP_VEL(t, s);
    } else if (s->time == ST_TIE) {
        if (t->seq_n) {
            t->seq_off = (CT_LEN(s) ? unit * CT_LEN(s) / 16u : gate + unit / 2u) + ev_off;   /* (V10.6z4) LEN: where the
                                                         * held note ends in it (rec_end) */
            t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie || chord_auto(t, len);   /* chains hold at any GATE /
                                                         * swing; (0.6.81) GLIDE on POLY: to the next chord */
        }
        return;
    }
    if (!(skip & SEQ_REP) && !cont) {
        t->arp_from = 0;                            /* (0.6.39) a step of its own: no arpeggio, or a new one */
        if (arp) {
            uint32_t ai = (uint32_t)(arp - seq_steps(t));
            t->arp_from = (uint8_t)(ai < NSTEP ? ai + 1u : 0u);
            if (arp == &t->cvs) {                       /* (0.6.47) C.LEN: the lane's step, kept over the TIEs */
                t->cva = t->cvs;
                t->arp_from = CV_IDX;
            }
            t->arp_k = 0;
        }
    }
    if (s->time == ST_REST || (!s->n && !s->hit)) {
        seq_release(t);
        return;
    }
    if (arp) {                                      /* (seq2d) the ARP's VALUE: its NOTES or SCALE, ORDER RATE RANGE */
        aa = arp->carg;
        ac = aa & CV_ARP_SCALE ? CP_ARPS : CP_ARPN;
    }
    if (CT_LEN(s))                                  /* (0.6.82) LEN: the step's own gate, n/16 */
        gate = unit * CT_LEN(s) / 16u;
    if (hits > 1u) {                                /* RATCH: the gate of a part, no slide or tie out */
        gate = fit_scale_samples(step_samples(t, period, t->seq_idx) / div,
                                CT_LEN(s) ? CT_LEN(s) * 8u : (uint32_t)t->p[P_SGATE], 7u);
        if (!(skip & SEQ_REP))
            t->rat_left = (uint8_t)(hits - 1u);
        if (!arp && t->cx_rat && t->cx_dec) {       /* (seq2d) RATCHET's DECAY: each part softer (1) or louder (2)
                                                         * (0.6.42 ECHO's; flat: a ratchet, all at one level) */
            uint32_t part = (skip & SEQ_REP) ? hits - 1u - t->rat_left : 0u;
            vel = vel * (t->cx_dec == 2u ? part + 1u : hits - part) / hits;
            if (vel < 8u) vel = 8u;
        }
    }
    if ((t->seq_fit & 128u) && !gate) gate = 1u;
    for (i = 0; i < (arp ? 1u : s->n) && i < 4u && !(kit && arp); i++) {
        uint32_t x = arp ? cp_arp_note(t, arp, ac, aa, t->arp_k++) : s->note[i];
        x = (uint32_t)clamp((int32_t)x + up, 0, 127);
        if (!kit) {                                 /* (seq2d) TRANSPOSE, RANDOM, OCT+ / OCT- (comp_run) */
            if (t->cx_deg)
                x = (uint32_t)clamp(scale_steps(t, (int32_t)x, t->cx_deg), 0, 127);
            if (rnd) {
                int32_t r = ra % 8u ? (int32_t)(ra % 8u) : 1;
                x = (uint32_t)clamp(scale_steps(t, (int32_t)x, (int32_t)(rng() % (uint32_t)(2 * r + 1)) - r), 0, 127);
            }
            if (octs)                               /* (0.6.82: with a RAND too) */
                x = (uint32_t)clamp((int32_t)x + octs, 0, 127);
        }
        if (song.g[G_STRN] && !kit)                 /* (JIANT 0.4) STRN: up / down the scale, every sequence */
            x = (uint32_t)clamp(scale_steps(t, (int32_t)x, song.g[G_STRN]), 0, 127);
        if (qseq) {                                 /* QNT SEQ: onto the scale now; two notes snapping */
            x = (uint32_t)clamp(scale_snap(t, (int32_t)x), 0, 127);   /* together play once */
            for (j = 0; j < m && nn[j] != x; j++)
                ;
            if (j < m)
                continue;
        }
        nn[m] = (uint8_t)x;
        vv[m] = (uint8_t)vel;
        sk |= ((skip >> i) & 1u) << m;
        once |= (uint32_t)(arp ? 0 : lm_once(s, kit ? drum_note_lane(t, s->note[i]) : i, kit)) << m;
        m++;
    }
    for (i = 0; i < NLANE && !arp; i++)
        if ((s->hit >> i) & 1u) {
            nn[m] = DRUM_LANE_NOTE[i];
            vv[m] = (uint8_t)((s->acc >> i) & 1u ? 127u : vel);
            sk |= ((skip >> (8u + i)) & 1u) << m;
            once |= (uint32_t)lm_once(s, i, 1u) << m;
            m++;
        }
    if (skip & SEQ_REP)                             /* (p1) the step's repeats: not what was recorded once, nor a lane
                                                     * with its own x2 x4 x8 (no repeats of repeats: seq_lane_rat) */
        sk |= once;
    if (kit) {                                      /* (0.6.82) on a drum kit: an ARP plays its sounds (cp_kit_arp); */
        int32_t dp = 0, deg = 0;                    /* OCT RAND TONAL and ARP SCALE tune the hits, BEND bends them */
        if (arp) {
            uint8_t kn[4 + NLANE];
            uint32_t c2 = cp_kit_arp(t, arp, ac, aa, t->arp_k++, kn, &deg);
            for (i = 0; i < c2 && m < 4u + NLANE; i++) {
                nn[m] = kn[i];
                vv[m++] = (uint8_t)vel;
            }
        }
        if (t->cx_deg) dp += scale_steps(t, 60, t->cx_deg) - 60;
        if (deg) dp += scale_steps(t, 60, deg) - 60;
        if (rnd) {
            int32_t r = ra % 8u ? (int32_t)(ra % 8u) : 1;
            dp += scale_steps(t, 60, (int32_t)(rng() % (uint32_t)(2 * r + 1)) - r) - 60;
        }
        if (octs) dp += octs;
        t->dpit = (int8_t)clamp(dp, -48, 48);
        {                                           /* (0.8.5) TOUCH: SHORT GHOST FLAM on the hits (eng_drum.c) */
            t->dtch = t->cx_tch;                    /* (seq2d: SOUND) */
        }
        t->dbend = (int16_t)(bend ? ((int32_t)(ba % 25u) - 12) * 16 : 0);
        t->dbt = (uint16_t)clamp((int32_t)(step_samples(t, period, t->seq_idx) / CTL), 1, 65535);
    }
    if (slide_in && !kit && !arp && hits == 1u && m && t->p[P_VOICE] == V_POLY) {   /* (0.6.81) a POLY chord sliding into
                                                         * this one: its notes glide to these */
        chord_glide(t, nn, vv, m, sk, tie_span(t, period, len));
        t->seq_n = 0;
        for (i = 0; i < m; i++)
            if (!((sk >> i) & 1u))
                t->seq_notes[t->seq_n++] = nn[i];
        t->seq_off = gate + ev_off;                 /* (ctl64: from the step's sample) */
        t->seq_hold = (s->flags & SF_SLIDE) != 0 || next_tie || chord_auto(t, len);
        return;
    }
    t->slide_glide = (uint8_t)slide_in;
    if (!slide_in)
        seq_release(t);
    t->ss_n = t->ss_i = 0;
    {   /* (0.6.87) HUMAN (0.6.92: one amount): each note's velocity up to +-50 at 100 %, the gate +-40 %, the step up to
         * 40 ms late, its DEC and REL up to +-20 steps of their knobs (voice_start takes hum_env) */
        uint32_t hv = hum_amt(t, P_HVEL), hg = hv, ht = hv;
        t->hum_env = (int8_t)(hv ? hum_pm(hv * 20u / 127u) : 0);
        if (hv)
            for (i = 0; i < m; i++)
                vv[i] = (uint8_t)clamp((int32_t)vv[i] + hum_pm(hv * 50u / 127u), 1, 127);
        if (hg && hits == 1u)
            gate = (uint32_t)clamp((int32_t)gate + (int32_t)gate / 100 * hum_pm(hg * 40u / 127u), 16, 0x7FFFFFFF);
        if (ht && hits == 1u && !bend && !slide_in) {   /* (late, never early: the step can't play before it starts) */
            uint32_t d = hum_rnd() % (HUM_TMAX * ht / 127u + 1u), cap = unit / 3u;
            hdel = d < cap ? d : cap;
        }
        if (t->mic > 0 && hits == 1u && !bend && !(skip & SEQ_REP))   /* (0.6.88) TIMING late: on top (V10.6z4: a SLIDE
                                                         * into it too: the note before held to it, below) */
            hdel += fit_scale_samples(step_samples(t, period, t->seq_idx), (uint32_t)t->mic, 6u);   /* (0.9.1: 64ths) */
    }
    sk0 = sk;                                       /* (the notes the step sounds, strummed or not: seq_notes) */
    if (!kit && !arp && m > 1u && t->p[P_CSTR] > 0 && t->p[P_VOICE] == V_POLY) {   /* (0.6.84) STRUM: the chord in
                                                         * one note after another, low to high, as the keys strum it
                                                         * (the first now, the others by seq_tick) at the keys' gaps
                                                         * (V10.6o: it was squeezed into half the gate: a long STRUM
                                                         * recorded came back a fast one); the gate held on past the
                                                         * last note's entry by half itself */
        uint32_t g = chord_gap(t), q, w = 0;
        uint8_t ord[4 + NLANE];
        for (q = 0; q < m; q++)
            if (!((sk >> q) & 1u)) {
                for (j = w; j > 0 && nn[ord[j - 1u]] > nn[q]; j--) ord[j] = ord[j - 1u];
                ord[j] = (uint8_t)q; w++;
            }
        for (q = 1; q < w && t->ss_n < 4u; q++) {       /* (the rest, queued; marked done here) */
            t->ss_nn[t->ss_n] = nn[ord[q]];
            t->ss_vv[t->ss_n++] = vv[ord[q]];
            sk |= 1u << ord[q];
        }
        t->ss_gap = (uint32_t)clamp((int32_t)g, 1, 65535);
        if (w > 1u && g * (w - 1u) > gate / 2u)
            gate = g * (w - 1u) + gate / 2u;
        t->ss_due = t->ss_gap + ev_off;             /* (ctl64: from the step's sample) */
    }
    if (hdel) {                                     /* (0.6.87) HUMAN TIME: every note of the step queued, hdel late
                                                     * (a strum's after it, its gaps kept; else all at once) */
        uint32_t q, gap = t->ss_n ? t->ss_gap : 0u;
        for (q = 0; q < m; q++)                     /* (the strum's first, the ones not queued yet, to the front) */
            if (!((sk >> q) & 1u) && t->ss_n < sizeof t->ss_nn) {
                for (j = t->ss_n; j > 0u; j--) { t->ss_nn[j] = t->ss_nn[j - 1u]; t->ss_vv[j] = t->ss_vv[j - 1u]; }
                t->ss_nn[0] = nn[q]; t->ss_vv[0] = vv[q];
                t->ss_n++;
                sk |= 1u << q;
            }
        t->ss_gap = gap;
        t->ss_due = (hdel ? hdel : 1u) + ev_off;    /* (ctl64: from the step's sample) */
        t->ss_tch = t->dtch;                        /* (0.8.5: the queued hits keep their TOUCH) */
        gate += hdel;
    }
    {   /* (0.8.5) a kit: each lane's own mode on its hits (TOUCH now, the ratchets after: seq_lane_rat) */
        uint32_t l;
        for (l = 0; kit && !(skip & SEQ_REP) && l < NLANE; l++)
            if (lm_hits(step_lmode(s, l)) > 1u)
                t->lr_on = 1;
    }
    for (i = 0; i < m; i++)
        if (!((sk >> i) & 1u)) {
            uint32_t a0 = vage, dt0 = t->dtch, lt = kit ? lm_touch(step_lmode(s, drum_note_lane(t, nn[i]))) : 0u;
            if (lt)
                t->dtch = (uint8_t)lt;
            seq_note_on(t, nn[i], vv[i]);
            t->dtch = (uint8_t)dt0;
            if (bend && !kit && vage != a0) {   /* (0.6.42) BEND: its voice glides over the step */
                uint32_t k2, ticks = step_samples(t, period, t->seq_idx) / CTL;
                int32_t d = ((int32_t)(ba % 25u) - 12) * 16;
                for (k2 = 0; k2 < NVOICE; k2++) {
                    voice_t *v = &t->v[k2];
                    if (v->age == vage && v->active) {
                        if (v->sub && (v->pn_flags & PN_OLD)) {   /* (ctl64) restarting later in the block: with it */
                            v->pn_flags |= PN_BEND;
                            v->pn_bend = (int16_t)d;
                            v->pn_gstep = glide_q8(d, ticks);
                            continue;
                        }
                        v->pitch_cur = v->pitch16;
                        v->pitch16 = clamp(v->pitch16 + d, 0, 2047);
                        v->gstep = glide_q8(d, ticks);
                    }
                }
            }
        }
    t->dpit = 0;                                    /* (0.6.82: the keys' hits as they are) */
    t->dbend = 0;
    t->dtch = 0;
    if (slide_in)                                   /* release what is not held over */
        for (i = 0; i < t->seq_n; i++) {
            for (j = 0; j < m && nn[j] != t->seq_notes[i]; j++)
                ;
            if (j < m)
                continue;
            if (hdel && t->ss_n < sizeof t->ss_nn) {   /* (V10.6z4) the slide's note late: this one up as it comes
                                                         * in (seq_due: a velocity 0 is a note-off), legato */
                t->ss_nn[t->ss_n] = t->seq_notes[i];
                t->ss_vv[t->ss_n++] = 0;
            } else
                seq_note_off(t, t->seq_notes[i]);
        }
    t->seq_n = 0;
    for (i = 0; i < m; i++)
        if (!((sk0 >> i) & 1u))
            t->seq_notes[t->seq_n++] = nn[i];
    t->seq_off = gate + ev_off;                     /* (ctl64: from the step's sample) */
    t->seq_hold = hits == 1u && ((s->flags & SF_SLIDE) != 0 || next_tie ||   /* next step a TIE: keep the notes to it */
                                 (!kit && !arp && chord_auto(t, len)));   /* (0.6.81) GLIDE on POLY: to the next chord */
}

/* RATCH: the next part of the playing step once the sequencer is that far into it (seq_step played the first).
 * The step is read again: an edit to fewer parts (or another source in a song) ends the repeats */
static __attribute__((noinline)) void seq_ratchet(track_t *t, uint32_t period)
{
    const step_t *s = t->cv_on ? &t->cvs : shift_step(t, t->seq_idx);   /* (0.6.47: C.LEN's) */
    uint32_t div, hits = step_parts(t, s, &div), q, at;
    if (t->rat_left >= hits) {
        t->rat_left = 0;
        return;
    }
    q = step_samples(t, period, t->seq_idx) / (div ? div : 1u);
    while (t->rat_left && t->seq_pos >= (at = (hits - t->rat_left) * q)) {   /* (ctl64) each part at its sample (every
                                                         * one due in this block; a part at the block's end: the next) */
        ev_at(t->seq_pos - at >= blk_n ? 0u : blk_n - (t->seq_pos - at));
        t->rat_left--;
        seq_step(t, s, period, SEQ_REP);
        ev_off = 0;
    }
}

/* (0.8.5) the lanes ratcheting on the playing step (LM_X2 / X4 / X8, seq_step struck their first part): each again at
 * every step / n (the grid RATCH's parts and the held keys' ratchets keep, dxo_tick), that lane's hit and notes alone,
 * the step read again (an edit ends it); n: the samples the step moved on in this block */
static __attribute__((noinline)) void seq_lane_rat(track_t *t, uint32_t period, uint32_t n)
{
    const step_t *s = t->cv_on ? &t->cvs : shift_step(t, t->seq_idx);
    uint32_t sl = step_samples(t, period, t->seq_idx), pos = t->seq_pos, prev, l, k, any = 0;
    uint32_t vel = STEP_VEL(t, s);
    if (pos >= 0x7FFFFFFFu || s->time != ST_NOTE) {
        t->lr_on = 0;
        return;
    }
    prev = pos > n ? pos - n : 0u;
    for (l = 0; l < NLANE; l++) {
        uint32_t r = lm_hits(step_lmode(s, l)), q, pn;
        if (r < 2u)
            continue;
        any = 1;
        q = sl / r;
        pn = pos / (q ? q : 1u);
        if (pn <= prev / (q ? q : 1u) || pn >= r)
            continue;
        ev_at(pos - pn * q >= n ? 0u : n - (pos - pn * q));   /* (ctl64) the part's first sample in this block */
        if ((s->hit >> l) & 1u)
            seq_note_on(t, DRUM_LANE_NOTE[l], (s->acc >> l) & 1u ? 127u : vel);
        for (k = 0; k < s->n && k < 4u; k++)
            if (drum_note_lane(t, s->note[k]) == l)
                seq_note_on(t, s->note[k], vel);
        ev_off = 0;
    }
    t->lr_on = (uint8_t)any;
}

#include "fugue.c"                                 /* (JIANT 0.6.52) SEQ > FUGUE: three more playheads a track */

/* (p2, snd6's for 1/2 TEMPO) a track back in phase from a slower LIVE RATE (or 1/2 TEMPO): its PLAYHEADS on by the
 * samples they missed, silently (live_q) */
static __attribute__((noinline)) void live_heads(track_t *t, uint32_t ti, uint32_t ln)
{
    uint32_t pd = div_samples((uint32_t)t->p[P_SDIV]), k;
    for (k = 0; k < FG_HEADS && ln; k++) {
        const fg_set_t *f = &fg_set[ti % NTRK][k];
        fg_head_t *h = &fg_head[ti % NTRK][k];
        uint32_t r = f->rate, sl, g;
        if (!r || r >= FG_NRATE || h->pos == 0x7FFFFFFFu)
            continue;
        sl = pd * FG_DEN[r] / FG_NUM[r];
        if (sl < CTL)
            sl = CTL;
        h->pos += lv.hd[ti % NTRK];
        for (g = 0; h->pos >= sl && g < 256u; g++) {
            h->pos -= sl;
            h->idx = (uint16_t)fg_next(h, f->dir, ln);
        }
        h->pos %= sl;
    }
    lv.hd[ti % NTRK] = 0;
}

/* (0.9.9 ctl64) what a step queued, due within this block: a STRUM's or HUMAN TIME's notes (ss_due, from this block's
 * first sample) and the gate's end (seq_off). After the steps of the block (seq_tick): a step at its sample queues from
 * there, and what falls in the rest of the block plays in it (it ran before the steps: a block late, as with blocks
 * of 32) */
static void seq_due(track_t *t, uint32_t n)
{
    if (t->ss_i < t->ss_n) {                         /* (0.6.84) STRUM: the step's chord coming in; (ctl64) each note
                                                     * at its sample (ss_due: from this block's start), the next gap
                                                     * samples after it */
        while (t->ss_i < t->ss_n && t->ss_due <= n) {   /* (0.6.87: HUMAN TIME without a strum: the chord together) */
            ev_at(t->ss_due);
            t->dtch = t->ss_tch;
            if (trk_kit(t)) {                       /* (0.8.5) its lane's TOUCH, from the step playing */
                const step_t *ps = t->cv_on ? &t->cvs : shift_step(t, t->seq_idx);
                uint32_t lt = lm_touch(step_lmode(ps, drum_note_lane(t, t->ss_nn[t->ss_i])));
                if (lt)
                    t->dtch = (uint8_t)lt;
            }
            if (t->ss_vv[t->ss_i])
                seq_note_on(t, t->ss_nn[t->ss_i], t->ss_vv[t->ss_i]);
            else
                seq_note_off(t, t->ss_nn[t->ss_i]);
            t->dtch = 0;
            t->ss_i++;
            t->ss_due += t->ss_gap;
        }
        ev_off = 0;
        t->ss_due = t->ss_due > n ? t->ss_due - n : 0u;
    }
    if (t->seq_n && !t->seq_hold) {
        if (t->seq_off <= n) {                       /* (ctl64) the gate ends at its sample (n: the next block's first) */
            ev_at(t->seq_off);
            seq_release(t);
            ev_off = 0;
        } else
            t->seq_off -= n;
    }
}

static void seq_tick(track_t *t, uint32_t n)
{
    uint32_t period, len, lon, nf;
    if (n > blk_n)
        blk_n = n;                                    /* (ctl64: ticked on its own, by a test, with more than a block) */
    if (!song.playing) {
        seq_due(t, n);
        return;
    }
    period = div_samples((uint32_t)t->p[P_SDIV]);
    len = (uint32_t)t->p[P_SLEN];
    nf = n;
    if ((lon = LV_ON(trk_index(t))) != 0u) {
        n = live_adv(t, n, period, len);            /* (p2) LIVE (and the FX layer's 1/2 TEMPO, LOOP): the ghost on, the
                                                     * playhead at its RATE, its next step (the window, the direction)
                                                     * as a JUMP's */
        nf = n < nf ? n : nf;                       /* (the PLAYHEADS: slower with it, never faster) */
    }
    t->seq_pos += n;
    for (;;) {
        uint32_t cur_len = step_samples(t, period, t->seq_idx) + t->ext, start, e = seq_early(t, period, len);
        if (e >= cur_len) e = cur_len - 1u;             /* (0.6.88) TIMING: the next step early, this one shorter */
        if (t->seq_pos < cur_len - e && t->seq_pos != 0x7FFFFFFFu + n)
            break;
        start = t->seq_pos >= 0x7FFFFFFFu;
        t->seq_pos = start ? chain.carry : t->seq_pos - (cur_len - e);   /* (a start: 0; a section in: its carry) */
        ev_at(start || t->seq_pos >= n ? 0u : n - t->seq_pos);   /* (ctl64) the step's first sample (seq_pos: the samples
                                                         * past it at the block's end; 0 past it: the next block's first) */
        t->ext = (uint16_t)(start ? 0u : e);            /* (the next one longer by as much: the grid kept) */
        if (t->jump_to && !start)                /* (0.6.42) JUMP: the step it names (LIVE's; V10.6y: REV, never set
                                                 * since LIVE's went, gone) */
            t->seq_idx = (uint16_t)((t->jump_to - 1u) % (len ? len : 1u));
        else
            t->seq_idx = (uint16_t)((t->seq_idx + 1u) % (len ? len : 1u));
        t->jump_to = 0;
        if (!t->seq_idx && !start && t->loops < 0xFFFFu)
            t->loops++;                                 /* (0.6.39) one more loop of the pattern (TRIG) */
        if (!lon)
            rec_hold(t, t->seq_idx, len ? len : 1u);   /* (LIVE: the ghost's steps, live_adv) */
        if (trk_index(t) < NPART && dxo_n[trk_index(t)])
            dxo_rec_hold(t, t->seq_idx);                /* (0.8.5) a held ratchet records into it */
        {
            shift_prepare_pass(t, len, t->loops);
            const step_t *s = (t->pfx & PFX_RND) && !(rng() & 1u)   /* (JIANT) RANDOM held: half the */
                                            ? &seq_steps(t)[rng() % (len ? len : 1u)] : shift_step(t, t->seq_idx);   /* steps, any */
            uint32_t skip = SEQ_ROLLED, i, k;
            const step_t *s0;
            t->mic = (int8_t)ST_MICRO(s);               /* (0.6.88) its own TIMING (late: seq_step) */
            s = comp_lane(t, s);                        /* (0.6.47) C.LEN: the lane's component on it */
            s0 = s;
            s = comp_run(t, s, len ? len : 1u);         /* (seq2d) the components: the step as they play it */
            /* the chance first (the one roll, as seq_step made it): a step that does not play applies no lock; the
             * automation and the locks before the notes, so a note-on reads them (eng_drum's KIT, ..) */
            if (!step_rolls(t, s))
                skip = SEQ_MISS;
            cx_lkhold = t->cx_lock == 1u;               /* (seq2d) LOCK: the locks held over its LENGTH, or none */
            motion_step(t, t->seq_idx, &motion, skip == SEQ_ROLLED && t->cx_lock != 2u);   /* (a song: its section's
                                                         * motion is the motion) */
            cx_lkhold = 0;
            if (comp_any(s0) || rmp[trk_index(t) % NTRK].id)   /* (V10.6v) RAMP: launched, its value */
                RAMP_RUN(t, s0, skip == SEQ_ROLLED && t->cx_lock != 2u, 0);
            if (t->rskip_n && t->rskip_idx == t->seq_idx) {
                for (k = 0; k < t->rskip_n; k++) {
                    for (i = 0; i < s->n; i++)
                        if (s->note[i] == t->rskip[k])
                            skip |= 1u << i;
                    for (i = 0; i < NLANE; i++)
                        if (((s->hit >> i) & 1u) && DRUM_LANE_NOTE[i] == t->rskip[k])
                            skip |= 1u << (8u + i);
                }
                t->rskip_n = 0;
            }
            seq_step(t, s, period, skip);
        }
        ev_off = 0;
    }
    if (rmp[trk_index(t) % NTRK].id)
        RAMP_RUN(t, 0, 0, n);                           /* (V10.6v) a RAMP on its way */
    if (t->rat_left)
        seq_ratchet(t, period);
    if (t->lr_on)
        seq_lane_rat(t, period, n);
    seq_due(t, n);                                      /* (ctl64: after the steps: what they queued, due in this block) */
    fugue_tick(t, nf, period, len);                     /* (0.6.52) */
}

/* MIDI in: the track a channel plays (0..15): G_ROUTE CH1-6 (0) channels 1..6 their parts (7..16 never get
 * here: midi_event drops them), SEL (1) every channel the selected track */
static uint8_t midi_route;                 /* the G_ROUTE events_block last saw (a change to CH1-6: midi_route_ch14) */
static track_t *midi_track(uint32_t ch) { return ch < NPART && !song.g[G_ROUTE] ? &trk[ch] : TSEL; }

#include "midi_control.c"
#include "midi_clock.c"

/* the count-in: its first beat now (accented) */
static __attribute__((noinline)) void cin_begin(void)
{
    cin_total = cin_left = (uint8_t)(4u * cin_bars);
    cin_pos = 0;
    memset(cin_n, 0, sizeof cin_n);
    click_req = 2;
}
/* n samples on: a beat ends; the last one starts the sequencer (in this block, before seq_tick: step 0 with it) */
static __attribute__((noinline)) void cin_block(uint32_t n)
{
    uint32_t b = click_beat_len();
    cin_pos += n;
    if (cin_pos < b)
        return;
    cin_pos -= b;
    cin_pos = cin_pos < b ? cin_pos : 0u;
    if (--cin_left) {
        click_req = (uint8_t)((cin_total - cin_left) % 4u ? 1u : 2u);
        return;
    }
    seq_start();
    cin_flush = 1;
}
/* the count-in's kept notes onto step 1, which has just started (events_block, after seq_tick): as if played now */
static __attribute__((noinline)) void cin_record(void)
{
    uint32_t i, k;
    cin_flush = 0;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        for (k = 0; k < cin_n[i]; k++)
            if (rec_on(t)) {
                dx_in_rat = (uint8_t)(cin_oct[i][k] & 15u);
                dx_in_tch = (uint8_t)(cin_oct[i][k] >> 4);
                rec_note(t, cin_note[i][k], cin_vel[i][k]);
                dx_in_rat = dx_in_tch = 0;
                if ((cin_up[i] >> k) & 1u)
                    rec_release(t, cin_note[i][k]);
            }
        cin_n[i] = 0;
    }
}
/* the metronome's beat: adv samples on while the transport runs (click.c sounds it in this block) */
static void click_tick(uint32_t adv)
{
    uint32_t p = div_samples(2), beat = 0;
    if (clk_pos == CLK_START) {
        clk_pos = 0;
        clk_step = 0;
        clk_n = 0;
        beat = 2;
    } else {
        clk_pos += adv;
        while (clk_pos >= p) {                      /* (as seq_tick: the remainder kept) */
            clk_pos -= p;
            clk_step = (clk_step + 1u) & 15u;
            clk_n++;
            if (!(clk_step & 3u))
                beat = clk_step ? 1u : 2u;
        }
    }
    if (beat && click_wanted())
        click_req = (uint8_t)beat;
}
/* the beat of the bar the click is on (0..3: 0 the accented first), -1 stopped: the count-in's, else the playing
 * transport's (clk_step, internal or external clock). It moves in the block a beat clicks in (the header's metronome,
 * ui_draw.c, swings on it; CLICK OFF it moves all the same) */
static int32_t click_beat_now(void)
{
    if (cin_left)
        return (int32_t)((uint32_t)(cin_total - cin_left) % 4u);
    if (!song.playing)
        return -1;
    return clk_pos == CLK_START ? 0 : (int32_t)(clk_step >> 2);
}

/* everything that happens between two rendered blocks */
static void events_block(uint32_t n)
{
    uint32_t i, pr, fo, seq_n = n;
    uint32_t clock_mode = (uint32_t)song.g[G_CLOCK];
    blk_n = n ? n : 1u;                                /* (ctl64: ev_at) */
    ev_off = 0;
    if (midi_clock.mode != clock_mode) {
        seq_stop();
        memset(&midi_clock, 0, sizeof midi_clock);
        midi_clock.mode = (uint8_t)clock_mode;
        midi_beat_samples = 0;
    }
    mo_block(n);                                    /* (0.6.72) MIDI OUT: its setting, the clock */
    if (cin_left && !clock_mode)
        cin_block(n);
    if (transport_req == 1u) {
        if (clock_mode)
            midi_clock_transport(0xFAu, fm1_ms);
        else if (cin_left)
            ;                                       /* counting in already (the UI's PLAY stops it: seq_counting) */
        else if (cin_bars && song.rec && !song.playing && !chain.armed)
            cin_begin();                            /* recording from stop: the count-in first */
        else
            seq_start();
        transport_req = 0;
    } else if (transport_req == 2u) {
        seq_stop();
        transport_req = 0;
    } else if (transport_req == 3u) {                 /* GLO + PLAY: from the top without stopping (INT clock) */
        if (!clock_mode) {
            seq_stop();
            seq_start();
        }
        transport_req = 0;
    }
    if (midi_route != (uint8_t)song.g[G_ROUTE]) {     /* ROUT changed: CH1-6 lets go of channels 7..16 */
        midi_route = (uint8_t)song.g[G_ROUTE];
        if (!midi_route)
            midi_route_ch14();
    }
    pr = panic_req;
    panic_req = 0;
    if (pr)
        mo_flush_of(pr);                               /* (0.6.72) their MIDI OUT notes too */
    fo = pr;                                           /* (0.6.13) sound loads: what sounds fades on the old sound */
    if (midi_in_overflow) {                            /* a lost note-off must never leave a held note */
        mi_r = mi_w;
        memset(midi_sel_on, 0, sizeof midi_sel_on);
        memset(midi_ch, 0, sizeof midi_ch);
        memset(midi_owners, 0, sizeof midi_owners);
        memset(&midi_map, 0, sizeof midi_map);
        midi_hint = 0;
        for (i = 0; i < NTRK; i++) {
            trk[i].rh_n = trk[i].rskip_n = 0;
            trk[i].seq_n = trk[i].seq_hold = trk[i].slide_glide = trk[i].rat_left = 0;
        }
        pr |= (1u << NTRK) - 1u;
        RING_PUBLISH();
        midi_in_overflow = 0;
    }
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if (((latch_drop_req >> i) & 1u) || (((latch_was >> i) & 1u) && !key_latch(t))) {
            latch_drop_req &= (uint8_t)~(1u << i);  /* (0.6.19) a clear, or LATCH left (HOLD off, ARP on, whatever
                                                     * the keys: a key down kept it all sounding, and stacking) */
            latch_drop(t);
        }
        latch_was = (uint8_t)(key_latch(t) ? latch_was | 1u << i : latch_was & ~(1u << i));
        if ((pr >> i) & 1u) {
            if (i < NPART)
                dxo_n[i] = 0;                       /* (0.8.5) the held ratchets stop */
            if ((latch_was >> i) & 1u)
                latch_release(t);                   /* (0.6.19: MIDI OUT's latched notes end too) */
            midi_forget_track(i);
            {   /* (0.6.41) another engine: what is held sounds on in it (kept past the release, which forgets) */
                uint8_t hn[4], hv[4];
                uint32_t hc = t->eng_req != t->engine ? xf_held(t, hn, hv) : 0u, h;
                trk_all_off(t);
                for (h = 0; h < hc; h++) { t->xp_note[h] = hn[h]; t->xp_vel[h] = hv[h]; }
                t->xp_n = (uint8_t)hc;
            }
            if ((fo >> i) & 1u)
                trk_fade_out(t);                        /* (0.6.13: a sound load: on the old values, ~3 ms; voice.c) */
            t->nheld = 0;
            t->arp_phys = 0;
            arp_forget(t);
        }
        engine_block(t);                              /* engine switch: fade, then switch (voice.c) */
        /* ARP turned off, or HOLD released with no key down: drop the latched chord */
        if ((t->armp && !t->p[P_AMODE]) || (t->aholdp && !t->p[P_AHOLD] && !t->arp_phys)) {
            if (!t->armp && t->aholdp && !t->p[P_AHOLD])
                latch_release(t);                       /* (0.6.14) LATCH off: its notes go */
            t->nheld = 0;
            if (!t->p[P_AMODE])
                t->arp_phys = 0;
            if (t->arp_note)
                arp_silence(t);
        }
        t->armp = t->p[P_AMODE];
        t->aholdp = t->p[P_AHOLD];
    }
    keyboard_block();
    while (mi_r != mi_w) {                            /* USB-MIDI (and TRS) in */
        uint32_t at = mi_r % MQ, pkt = midi_in_q[at], status = (pkt >> 8) & 0xFFu;
        uint32_t st = status & 0xF0u, ch = status & 0x0Fu;
        uint32_t d1 = (pkt >> 16) & 0x7Fu, d2 = (pkt >> 24) & 0x7Fu;
        uint32_t source = midi_in_source[at] ? midi_in_source[at] : 1u, ms = midi_in_ms[at];
        mi_r++;
        if (status >= 0xF8u) {
            if (clock_mode == source) {
                if (status == 0xF8u)
                    midi_clock_pulse(ms);
                else
                    midi_clock_transport(status, ms);
            }
        } else
            midi_event(st, ch, d1, d2);
    }
    if (clock_mode) {
        seq_n = 0;
        if (song.playing) {
            uint32_t last = midi_clock.have_pulse ? midi_clock.last_ms : midi_clock.start_ms;
            if (fm1_ms - last > 500u) {
                seq_stop();                         /* cable loss must not leave a running held note */
                midi_clock.tempo_valid = 0;
            } else
                seq_n = midi_clock_advance(fm1_ms);
        }
    }
    chain_tick(seq_n);
    live_q(seq_n);                                  /* (p2) LIVE: what goes in now (seq_live.c; idle: a test) */
    for (i = 0; i < NTRK; i++)
        seq_tick(&trk[i], seq_n);
    for (i = 0; i < NPART; i++)                       /* (0.8.5) the kit's octaves held: their ratchets */
        if (dxo_n[i])
            dxo_tick(&trk[i], clock_mode ? seq_n : n);
    if (cin_flush)
        cin_record();
    if (song.playing)
        click_tick(seq_n);
    for (i = 0; i < NPART; i++)
        arp_step(&trk[i], clock_mode ? seq_n : n, clock_mode && song.playing ? seq_n : n);
    beat_pos += clock_mode ? seq_n : n;               /* the beat the ARP LED flashes on (ui_leds) */
    if (beat_pos >= beat_samples()) {
        beat_pos -= beat_samples();
        beat_pos = beat_pos < beat_samples() ? beat_pos : 0u;
        beat_n = (beat_n + 1u) & 3u;
        if (!beat_n && song.playing)
            bar_n++;
    }
    if (song.playing)
        song.tick++;
    ev_off = 0;
}

#include "song_play.c"                           /* the song: the ISR's part (song_chain.c) */



