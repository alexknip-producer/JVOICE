/* SPDX-License-Identifier: GPL-3.0-only
 * Adapted from MIDI control contribution by ChanceTheMaker (2026).
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Shared USB/TRS channel controls. Included by seq.c after its input helpers.
 * USB and TRS intentionally share channel state, matching the existing routing.
 * A synth part has one live bend/wheel state; channels assigned to the same part
 * share it (last controller wins). Drum hits ignore bend and sustain. CC1 keeps the existing matrix routing. */
typedef struct {
    int16_t bend;                           /* signed 14-bit value, zero = centre */
    uint8_t wheel, pedal, targets;
    uint8_t owned[NTRK];                    /* held/pedal notes per track, at most 128 per channel */
    uint8_t ready, semis, cents;
    uint8_t rpn_msb, rpn_lsb;
    uint8_t nrpn_msb, nrpn_lsb, nrpn_v;     /* (V10.6z3) the NRPN selected (127: none) and its data entry MSB */
} midi_channel_t;
static midi_channel_t midi_ch[16];
/* A nibble per note (JIANT: two to a byte, 1 KB): low bits track + 1, bit 3 key released, held by its channel's pedal */
static uint8_t midi_sel_on[16][64];
static uint32_t mn_get(uint32_t ch, uint32_t note) { return (midi_sel_on[ch & 15u][(note & 127u) >> 1] >> ((note & 1u) * 4u)) & 15u; }
static void mn_set(uint32_t ch, uint32_t note, uint32_t v)
{
    uint8_t *b = &midi_sel_on[ch & 15u][(note & 127u) >> 1];
    uint32_t sh = (note & 1u) * 4u;
    *b = (uint8_t)((*b & ~(15u << sh)) | (v & 15u) << sh);
}
/* Dense remaps only for transposed keys; normal MIDI keeps its existing nibble table.
 * At most 128 transposed presses, each retaining original channel/key until release.
 * LOWBUF has linker-checked room; preserve the pool's 8 KiB safety reserve. */
static struct {
    uint16_t n;
    struct { uint16_t key; uint8_t note, rsv; } row[128];
} midi_map __attribute__((section(".lowbuf")));
static uint32_t midi_mapped(uint32_t ch, uint32_t note, uint32_t forget)
{
    uint32_t key=(ch<<7)|note;
    for(uint32_t i=0;i<midi_map.n;i++) if(midi_map.row[i].key==key) {
        note=midi_map.row[i].note;
        if(forget) midi_map.row[i]=midi_map.row[--midi_map.n];
        break;
    }
    return note;
}
static int midi_remap(uint32_t ch,uint32_t note,uint32_t sound)
{
    if(note==sound) return 1;
    if(midi_map.n==128u) return 0; /* bounded admission; never lose a stored note-off */
    midi_map.row[midi_map.n].key=(uint16_t)((ch<<7)|note);
    midi_map.row[midi_map.n++].note=(uint8_t)sound;
    return 1;
}
static uint16_t midi_owners[NTRK];           /* avoids rescanning all 2048 entries for CC123 */
#define MIDI_PEDAL_NOTE 0x8u

static midi_channel_t *midi_channel(uint32_t ch)
{
    midi_channel_t *c = &midi_ch[ch];
    if (!c->ready) {
        c->semis = 2;
        c->rpn_msb = c->rpn_lsb = c->nrpn_msb = 127;
        c->ready = 1;
    }
    return c;
}

static uint32_t midi_targets(uint32_t ch)
{
    uint32_t mask = midi_channel(ch)->targets;
    if (split_cfg[0] && ch == split_cfg[6]) mask |= (1u << split_track(0)) | (1u << split_track(1));
    else mask |= 1u << trk_index(midi_track(ch));
    return mask;
}

static void midi_expression(track_t *t, const midi_channel_t *c)
{
    int32_t range = ((int32_t)c->semis * 100 + c->cents) * 256 / 100;
    if (drum_track(t))
        return;
    midi_bend_target[trk_index(t)] = (int32_t)c->bend * range / (c->bend < 0 ? 8192 : 8191);
}

static void midi_expression_channel(uint32_t ch)
{
    uint32_t i, mask = midi_targets(ch);
    for (i = 0; i < NPART; i++)
        if (mask & (1u << i))
            midi_expression(&trk[i], midi_channel(ch));
}

/* a MIDI note of track t sounds note */
static int midi_note_held(const track_t *t, uint32_t note)
{
    uint32_t id=trk_index(t)+1u;
    for(uint32_t ch=0;ch<16u;ch++)
        if((mn_get(ch,note)&7u)==id && midi_mapped(ch,note,0)==note) return 1;
    for(uint32_t i=0;i<midi_map.n;i++)
        if(midi_map.row[i].note==note && (mn_get(midi_map.row[i].key>>7,midi_map.row[i].key&127u)&7u)==id) return 1;
    return 0;
}

/* a key held on track t sounds note */
static int midi_local_held(const track_t *t, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (kb_on[k] && kb_trk[k] == trk_index(t)) {   /* (0.6.81: a key's chord, the notes started) */
            uint32_t i;
            for (i = 0; i < kb_on[k]; i++)
                if (kb_snd[k][i] == note)
                    return 1;
        }
    return 0;
}

static void midi_release(uint32_t ch, uint32_t note)
{
    uint32_t id = mn_get(ch, note) & 0x7u;
    uint32_t source = note;
    note = midi_mapped(ch, source, 1);
    mn_set(ch, source, 0);
    if (id) {
        midi_owners[id - 1u]--;
        if (!--midi_ch[ch].owned[id - 1u])
            midi_ch[ch].targets &= (uint8_t)~(1u << (id - 1u));
    }
    if(id && drum_track(&trk[id-1u])) dxo_off(&trk[id-1u],DXO_MIDI+(ch<<7)+source);
    if (id && drum_track(&trk[id - 1u]) && !drum_ext(&trk[id-1u]) && drum_oct_of(note)) {   /* classic octave bands */
        dxo_off(&trk[id - 1u], DXO_MIDI + (ch << 7) + source);
        if (!midi_local_held(&trk[id - 1u], drum_oct_base(note)))
            input_off(&trk[id - 1u], drum_oct_base(note));
    } else if (id && !midi_local_held(&trk[id - 1u], note))
        input_off(&trk[id - 1u], note);           /* input_off also checks other MIDI owners */
}

/* a MIDI note-on (ch, note) of track t. A note another key or MIDI note holds already sounds: not started again */
static void midi_play(track_t *t, uint32_t ch, uint32_t note, uint32_t vel)
{
    if(drum_track(t)&&drum_ext(t)) {
        dxo_on(t,note,0,vel,DXO_MIDI+(ch<<7)+note);
        return;
    }
    if (drum_track(t) && drum_oct_of(note)) {          /* (0.8.5) the kit in another octave: its sound, ratcheted /
                                                         * damped (seq.c dxo_on) */
        dxo_on(t, drum_oct_base(note), drum_oct_of(note), vel, DXO_MIDI + (ch << 7) + note);
        return;
    }
    if (!midi_note_held(t, note) && !midi_local_held(t, note))
        input_on(t, note, vel);
}

static void midi_note_event(uint32_t ch, uint32_t note, uint32_t vel)
{
    midi_channel_t *c = midi_channel(ch);
    uint32_t id = mn_get(ch, note) & 0x7u;
    if (vel) {
        uint32_t high = note >= split_cut(1), sound = note;
        int split = split_cfg[0] && ch == split_cfg[6];
        track_t *t = split ? &trk[split_track(high)] : midi_track(ch);
        if (split && !drum_track(t)) sound = (uint32_t)clamp((int32_t)note + 12 * split_oct(high), 0, 127);
        /* Repeated notes replace the previous press, including a pedal-held one. */
        if (id)
            midi_release(ch, note);
        if (!midi_remap(ch, note, sound)) return;
        midi_expression(t, c);
        if (t != TSEL)
            midi_hint = (uint8_t)(trk_index(t) + 1u);
        c->targets |= (uint8_t)(1u << trk_index(t));
        midi_play(t, ch, sound, vel);
        mn_set(ch, note, (uint8_t)(trk_index(t) + 1u));
        midi_owners[trk_index(t)]++;
        c->owned[trk_index(t)]++;
    } else if (id) {
        if (c->pedal && !drum_track(&trk[id - 1u]))
            mn_set(ch, note, mn_get(ch, note) | MIDI_PEDAL_NOTE);
        else
            midi_release(ch, note);
    }
}

static void midi_pedal_up(uint32_t ch)
{
    uint32_t note;
    midi_channel(ch)->pedal = 0;
    for (note = 0; note < 128u; note++)
        if (mn_get(ch, note) & MIDI_PEDAL_NOTE)
            midi_release(ch, note);
}

/* Preset/project panic and CC120 must discard ownership so a later pedal-up
 * or note-off cannot release notes subsequently started on another patch. */
static void __attribute__((noinline)) midi_forget_track(uint32_t track)
{
    uint32_t ch, note;
    for (ch = 0; ch < 16u; ch++) {
        if (!(midi_ch[ch].targets & (1u << track)))
            continue;
        for (note = 0; note < 128u; note++)
            if ((mn_get(ch, note) & 0x7u) == track + 1u) {
                midi_mapped(ch, note, 1);
                mn_set(ch, note, 0);
            }
        midi_ch[ch].targets &= (uint8_t)~(1u << track);
        midi_ch[ch].owned[track] = 0;
    }
    midi_owners[track] = 0;
    midi_bend_q8[track] = midi_bend_target[track] = 0;
}

static void midi_silence_track(uint32_t track)
{
    track_t *t = &trk[track];
    uint32_t i;
    trk_all_off(t);
    if (track < NPART)
        dxo_n[track] = 0;                     /* (0.8.5) a held ratchet stops */
    latch_release(t);                         /* (stuck notes: the latched notes' MIDI OUT note-offs; nheld 0) */
    t->arp_phys = t->rh_n = 0;
    if (t->arp_note)
        arp_silence(t);                       /* (stuck notes: and its MIDI OUT note-off, not only forgotten) */
    t->seq_n = t->seq_hold = t->slide_glide = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active)
            voice_kill(&t->v[i]);             /* one-block fade, regardless of RELEASE */
    slicer_drop();                            /* do not keep replaying captured sound (the SLICER's bus) */
    midi_forget_track(track);
}

static int midi_track_held(uint32_t track)
{
    uint32_t k;
    if (midi_owners[track])
        return 1;
    for (k = 0; k < 27u; k++)
        if ((fm1_in.notes & (1u << k)) && kb_trk[k] == track)
            return 1;
    return 0;
}

/* The standard CC map (#103): always on, these CCs only, on the track the channel plays (ROUT as notes). A CC
 * sets its parameter as a knob would, 0..127 over the parameter's range (64: the middle of a bipolar one), and
 * AUTOMATION records it as a knob move (motion_capture: the selected track, armed and playing). CC74 / CC71 are
 * the engine's own brightness / resonance (MIDI_CC_ENG); an engine without one ignores them. CC121 resets none
 * of them (RP-015: they are the sound's) */
#define MCC_CUT 0xFEu
#define MCC_RES 0xFFu
static const uint8_t MIDI_CC_MAP[][2] = {
    {5, P_GLIDE}, {7, P_LEVEL}, {10, P_PAN}, {71, MCC_RES}, {72, P_REL}, {73, P_ATK}, {74, MCC_CUT}, {75, P_DEC},
    {91, P_REV}, {93, P_CHOR}, {94, P_DLY},
    /* (0.6.96) the engine's own eight values (EDIT 1 / EDIT 2's knobs, whatever the engine: CC16..23 = E1..E8), the
     * post filter, drive and sustain (CC102..105) */
    {16, P_E0}, {17, P_E1}, {18, P_E2}, {19, P_E3}, {20, P_E4}, {21, P_E5}, {22, P_E6}, {23, P_E7},
    {102, P_FCUT}, {103, P_FRES}, {104, P_DIST}, {105, P_SUS},
};
/* per engine (ENGINES[] order): CC74's E index + 1 in the high nibble, CC71's in the low one, 0 = none */
static const uint8_t MIDI_CC_ENG[NENGINES] = {
    0x56, 0x00, 0x30, 0x00, 0x50, 0x07, 0x67, 0x00,   /* ANALOG CUT RES, -, PHASE DCW, MATH - (V10.6h: was E8, now its LOOP:
                                                       * its CUT RES are the FILTER page's), SAMPLE CUT, VOICE Q, TRIO
                                                       * CUT RES, WHEEL - */
    0x32, 0x30, 0x30, 0x34, 0x30,                     /* (snd5) STRING BRIGHT DECAY, PHYS BRIT, DRUM TONE, NOISE FREQ RES, FM6 MLVL */
    0x26,                                             /* (snd5) ADD BRIGHT FORMNT */
    0x23, 0x00,  /* (V10.54: then INPUT, none) */  /* ANIMAL PRESS (TOUCH CLICK BUZZ MOUTH), CHAOS (COUPLE Q DRIFT ROUGH) */
};
/* a CC's value 0..top (127, or an NRPN's 16383) over parameter id's range of track t (d), as a knob sets it;
 * AUTOMATION records it */
static void __attribute__((noinline)) midi_val_set(track_t *t, const param_desc_t *d, uint32_t id, uint32_t value, int32_t top)
{
    int32_t v = d->min + ((int32_t)value * (d->max - d->min) + top / 2) / top;
    if (d->max <= d->min || t->p[id] == v)
        return;
    t->p[id] = (int16_t)v;
    (void)motion_capture(t, id, (int16_t)v);
}
static void midi_cc_set(track_t *t, uint32_t id, uint32_t value)
{
    midi_val_set(t, param_desc_of(eng_idx(t->eng_req), id), id, value, 127);
}
/* (V10.6z3, editor protocol 1.0: docs/NRPN.md) NRPN on the channel a track plays (ROUT as notes; channel 16 also with
 * ROUT CH1-6, the selected track): CC99 0, CC98 a P_* id that automation records (motion_param), CC6 the value's top 7
 * bits (alone: a 7-bit value, as a CC's), CC38 its low 7 (a 14-bit value, after CC6), over the range DESC / TRACK_PARAM
 * give. CC99 1, CC98 a global id: the global (no transport, routing, load / save, engine switch: NRPN_G_NO). AUTOMATION
 * records a track's as a knob move (the DJ FILTER's too); a held CC99 / CC98 takes any number of values */
static __attribute__((noinline)) void midi_nrpn(uint32_t ch, midi_channel_t *c, uint32_t lo, uint32_t value)
{
    uint32_t id = c->nrpn_lsb, x;
    track_t *t = midi_track(ch);
    if (!lo)
        c->nrpn_v = (uint8_t)value;
    x = (uint32_t)c->nrpn_v << 7 | (lo ? value : c->nrpn_v);   /* (7 bits alone: 0 .. 127 -> 0 .. 16383, as a CC) */
    if (!c->nrpn_msb) {
        if (id < P_COUNT && motion_param(id))
            midi_val_set(t, track_desc(t, id), id, x, 16383);
    } else if (nrpn_g_ok(id)) {
        int32_t v = GP[id].min + ((int32_t)x * (GP[id].max - GP[id].min) + 8191) / 16383;
        if (id == G_DJF)
            motion_djf_set(t, (int16_t)v);
        else
            song.g[id] = (int16_t)v;
    }
}
static void __attribute__((noinline)) midi_cc_map(track_t *t, uint32_t cc, uint32_t value)
{
    uint32_t i, id = 0xFFFFu, e;
    for (i = 0; i < NELEM(MIDI_CC_MAP); i++)
        if (MIDI_CC_MAP[i][0] == cc)
            id = MIDI_CC_MAP[i][1];
    if (id >= MCC_CUT) {
        if (id == 0xFFFFu)
            return;
        e = MIDI_CC_ENG[eng_idx(t->eng_req)] >> (id == MCC_CUT ? 4 : 0) & 15u;
        if (!e && eng_idx(t->eng_req) != ENGI_LOFI)
            return;
        id = e ? P_E0 + e - 1u : id == MCC_CUT ? P_FCUT : P_FRES;   /* (V10.6h) MATH: the FILTER page's CUT RES */
    }
    midi_cc_set(t, id, value);
}
/* (p1, main 0.9.12's MIDI LEARN from Felucca 1.5, extended) MENU > CONTROL > MIDI MAP: FACTORY the standard map above
 * alone (as ever); CUSTOM ADD the learned CCs too (a learned CC is the user's: over the map's meaning of it); CUSTOM
 * the learned CCs alone (the map's CCs do nothing unless learned). Up to ML_N CCs, each set to one parameter of a
 * track fixed when it was learned, or of the selected track (ML_SEL: whatever track is selected when it comes),
 * whatever channel it comes on (a channel ROUT ignores stays ignored), over USB or the TRS jack alike (one queue: seq.c
 * events_block). Kept with the settings in a favourites row no engine uses (DIGITAL's, engine 1, retired: its byte 0
 * the old stars' migration, settings_persist.c; byte 1 MIDI MAP; bytes 2..31 the CCs, 0 in every older setting =
 * FACTORY, nothing learned): an entry three bytes, the CC | 0x80 (0: empty), the track 0..5 or ML_SEL, the
 * parameter's code: a common parameter its P_* id + 1 (1 .. ML_E0 - 1: their ids never move, new ones go before
 * P_E0), an engine parameter ML_E0 + k for P_E0 + k (P_E0 moves when common ones are added). The CCs with a meaning of
 * their own are never learned (ml_free_cc): CC0 / 32 bank select, CC1 the wheel, CC6 / 38 / 96..101 data entry and
 * RPN, CC11 EXPR, CC64 the pedal, CC120..127 the channel modes. The UI (midi_learn.c) writes the entries with the
 * interrupts off; ml.arm / ml.heard hand it a CC heard while learning. Its state in the pool (not among the globals
 * the audio ISR reaches) */
#define ML_N 10u
#define ML_E0 120u
#define ML_SEL 0x7Fu
enum { MLM_FACTORY, MLM_ADD, MLM_CUSTOM, MLM_N };
typedef char ml_ids_fit[P_E0 < ML_E0 && P_COUNT - P_E0 == 8 && NTRK <= 8 ? 1 : -1];
static uint8_t *const ml_tab;               /* (midi_learn.c: favorites.factory[ENGI_DIGITAL] + 1, [0] MIDI MAP) */
static struct {
    uint8_t on, pick, trk, id;               /* LEARN: 0 off, 1 SEL TRACK, 2 FIXED TRK, 3 CLEAR (at the MENU's close);
                                              * a parameter picked (a knob turned): its track, P_* id */
    volatile uint8_t arm, heard;             /* the UI waits for a CC; the CC that came meanwhile + 1 (0 none) */
} ml __attribute__((section(".pool")));
static uint32_t ml_id(uint32_t code)        /* an entry's code -> P_* id, P_COUNT: none this firmware knows */
{
    return code >= ML_E0 ? P_E0 + code - ML_E0 : code && code <= P_E0 ? code - 1u : P_COUNT;
}
static int ml_free_cc(uint32_t cc)
{
    return !(cc < 2u || cc == 6u || cc == 11u || cc == 32u || cc == 38u || cc == 64u || (cc >= 96u && cc <= 101u) ||
             cc >= 120u);
}
static uint32_t ml_map(void) { return ml_tab ? ml_tab[0] % MLM_N : MLM_FACTORY; }
/* 1: cc is learned and MIDI MAP is CUSTOM (ADD), set here; or taken by MIDI LEARN waiting for one */
static int __attribute__((noinline)) midi_learned(uint32_t cc, uint32_t value)
{
    const uint8_t *e;
    uint32_t i, id, hit = 0;
    if (!ml_tab)
        return 0;
    if (ml.arm) {
        if (!ml.heard)
            ml.heard = (uint8_t)(cc + 1u);
        return ml_free_cc(cc);
    }
    if (ml_map() == MLM_FACTORY)
        return 0;
    for (i = 0, e = ml_tab + 1; i < ML_N; i++, e += 3)
        if (e[0] == (0x80u | cc) && (e[1] < NTRK || e[1] == ML_SEL) && (id = ml_id(e[2])) < P_COUNT) {
            midi_cc_set(e[1] == ML_SEL ? TSEL : &trk[e[1]], id, value);
            hit = 1;
        }
    return hit;
}

static void midi_control(uint32_t ch, uint32_t cc, uint32_t value)
{
    midi_channel_t *c = midi_channel(ch);
    uint32_t i, mask;
    if (midi_learned(cc, value))                   /* (p1) MIDI LEARN's (a learned CC is the user's) */
        return;
    switch (cc) {
    case 1:
        c->wheel = (uint8_t)value;
        break;
    case 120:                                      /* All Sound Off: ignores the pedal */
        mask = midi_targets(ch);
        for (i = 0; i < NTRK; i++)
            if (mask & (1u << i))
                midi_silence_track(i);
        break;
    case 123:                                      /* All Notes Off: normal releases, honours pedal */
        mask = midi_targets(ch);
        for (i = 0; i < 128u; i++)
            if (mn_get(ch, i))
                midi_note_event(ch, i, 0);
        for (i = 0; i < NTRK; i++)
            if ((mask & (1u << i)) && !midi_track_held(i)) {
                trk_all_off(&trk[i]);
                latch_release(&trk[i]);
                trk[i].arp_phys = trk[i].rh_n = 0;
                if (trk[i].arp_note)
                    arp_silence(&trk[i]);
            }
        break;
    case 121:                                      /* Reset All Controllers, keep bend sensitivity */
        c->bend = 0;
        c->wheel = 0;
        c->rpn_msb = c->rpn_lsb = 127;
        midi_expression_channel(ch);
        midi_pedal_up(ch);
        break;
    case 64:
        if (value >= 64u)
            c->pedal = 1;
        else
            midi_pedal_up(ch);
        break;
    case 101: c->rpn_msb = (uint8_t)value; c->nrpn_msb = 127; break;
    case 100: c->rpn_lsb = (uint8_t)value; c->nrpn_msb = 127; break;
    case 99: case 98:                              /* NRPN selection cancels RPN data entry */
        c->rpn_msb = c->rpn_lsb = 127;
        if (cc == 99u)
            c->nrpn_msb = (uint8_t)value;
        else
            c->nrpn_lsb = (uint8_t)value;
        break;
    case 6: case 38:
        if (c->nrpn_msb <= 1u)                     /* (V10.6z3) NRPN 0 / 1: a track's or a global parameter */
            midi_nrpn(ch, c, cc == 38u, value);
        else if (!c->rpn_msb && !c->rpn_lsb) {      /* RPN 0: +/-0..24 semitones, 0..99 cents */
            if (cc == 6u)
                c->semis = (uint8_t)(value > 24u ? 24u : value);
            else
                c->cents = (uint8_t)(value > 99u ? 99u : value);
            midi_expression_channel(ch);
        }
        break;
    default:
        if (ml_map() != MLM_CUSTOM)                /* (p1) MIDI MAP CUSTOM: the learned CCs alone */
            midi_cc_map(midi_track(ch), cc, value);
        break;
    }
}

/* ROUT CH1-6 listens to channels 1..6 only. A switch to it from SEL (events_block, before the queue) lets go
 * of what channels 5..16 hold: their notes released (also pedal-held ones), their pedal, bend, wheel and RPN
 * selection reset, so no note can hang on a channel that is no longer heard. Each part's bend then follows
 * its own channel (1..6), as CH1-6 routes it. */
static void __attribute__((noinline)) midi_route_ch14(void)
{
    uint32_t ch, note;
    for (ch = NPART; ch < 16u; ch++) {
        if (split_cfg[0] && ch == split_cfg[6]) continue;
        midi_channel_t *c = midi_channel(ch);
        for (note = 0; note < 128u; note++)
            if (mn_get(ch, note))
                midi_release(ch, note);
        c->pedal = c->wheel = 0;
        c->bend = 0;
        c->rpn_msb = c->rpn_lsb = 127;
    }
    for (ch = 0; ch < NPART; ch++)
        midi_expression(&trk[ch], midi_channel(ch));
}

/* (JIANT 0.6.72) PROGRAM CHANGE n on the channel a track plays: the n + 1-th sound of the PRESETS list (every engine's
 * presets in their order, then the user presets; the favourites filter aside), loaded by the UI between frames (a
 * sound load is the main loop's: ui.c midi_pc_poll). 0 none, else n + 1 */
static volatile uint8_t midi_pc_req[NTRK];
/* (JIANT 0.6.80) PROGRAM CHANGE on channel 16, the song's own (never a track's, whatever ROUTE): 0..3 the section A..D
 * (in on the next bar, as the song layer's keys), 16..21 the project 1..6 (V10.6p: 6 songs; stopped only, as SAVE >
 * PROJECT). The UI takes it between frames (ui.c midi_pc_poll). 0 none, else n + 1 */
static volatile uint8_t midi_song_req;

/* Keep the occasional controller/panic dispatch outside the hot rendering loop. Channel voice messages only
 * (realtime and clock are handled in events_block, SysEx never reaches here). With ROUT CH1-6 channels
 * 7..16 are ignored entirely: notes, bend, CCs (CC1/11/64, RPN, and the CC120/121/123 panic and reset),
 * channel aftertouch, so they stay free for other instruments. */
static void __attribute__((noinline)) midi_event(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2)
{
    if (st == 0xC0u && ch == 15u) {                    /* (0.6.80) channel 16's PROGRAM CHANGE: a section, a project */
        midi_song_req = (uint8_t)(d1 + 1u);
        return;
    }
    if (ch >= NPART && !song.g[G_ROUTE] && !(split_cfg[0] && ch == split_cfg[6]) && (!midi_channel(ch)->targets || (st == 0x90u && d2))) {
        if (ch == 15u && st == 0xB0u && (d1 == 99u || d1 == 98u || d1 == 6u || d1 == 38u))
            midi_control(ch, d1, d2);              /* (V10.6z3) channel 16's NRPN (the globals: MIDI OUT sends them there) */
        return;
    }
    if (st == 0x90u || st == 0x80u)
        midi_note_event(ch, d1, st == 0x90u ? d2 : 0);
    else if (st == 0xE0u) {
        midi_channel(ch)->bend = (int16_t)((int32_t)(d1 | (d2 << 7)) - 8192);
        midi_expression_channel(ch);
    } else if (st == 0xB0u || st == 0xD0u) {
        uint32_t mask = split_cfg[0] && ch == split_cfg[6] ? midi_targets(ch) : 1u << trk_index(midi_track(ch));
        for (uint32_t i = 0; i < NTRK; i++) if (mask & (1u << i)) mod_midi(&trk[i], st, d1, d2);
        if (st == 0xB0u) midi_control(ch, d1, d2);
    }
    else if (st == 0xC0u)                          /* (0.6.72) PROGRAM CHANGE: the UI loads it (ui.c midi_pc_poll) */
        midi_pc_req[trk_index(midi_track(ch)) % NTRK] = (uint8_t)(d1 + 1u);
}

