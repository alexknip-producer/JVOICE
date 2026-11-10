/* SPDX-License-Identifier: GPL-3.0-only */
/* Stuck notes (hanging voices, missing MIDI OUT note-offs): a key or MIDI note held while the state that made its
 * notes changes (octave, chord, scale, ARP, LATCH, KEY SPLIT, the selected track, a preset or engine load, SCENE's
 * roles and octave), then let go: every voice it started ends and every note sent on gets its note-off. MIDI IN:
 * note-on velocity 0, running status on TRS, CC123 / CC120 / CC64, an engine load under held notes. PANIC (PLAY held):
 * every voice and every MIDI OUT note ends. The real sources (seq.c, midi_control.c, voice.c, the UI) on the host. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint8_t mo_open[16][128];                     /* the notes sent on by MIDI OUT, not ended yet */
static uint32_t fz_it, fz_a, mo_who[16][128];       /* (a failure's trace: the random step and action that sent it) */
static void mo_drain(void)
{
    while (mo_r != mo_w) {
        uint32_t p = midi_out_q[mo_r % MQ], st = (p >> 8) & 0xF0u, ch = (p >> 8) & 15u, d1 = (p >> 16) & 127u,
                 d2 = (p >> 24) & 127u;
        mo_r++;
        if (st == 0x90u && d2) {
            mo_open[ch][d1] = 1;
            mo_who[ch][d1] = fz_it << 8 | fz_a;
        }
        else if (st == 0x80u || st == 0x90u)
            mo_open[ch][d1] = 0;
        else if (st == 0xB0u && (d1 == 123u || d1 == 120u))
            memset(mo_open[ch], 0, 128);
    }
}
static uint32_t mo_hung(void)
{
    uint32_t c, n, h = 0;
    mo_drain();
    for (c = 0; c < 16u; c++)
        for (n = 0; n < 128u; n++)
            h += mo_open[c][n];
    return h;
}
static void blocks(uint32_t n)                       /* the audio ISR: events, then every part rendered */
{
    static int32_t out[CTL];
    uint32_t i, k;
    for (i = 0; i < n; i++) {
        events_block(CTL);
        for (k = 0; k < NTRK; k++)
            track_render(&trk[k], out, CTL);
        mo_drain();
    }
}
static uint32_t hung(void)                           /* gated voices, notes waiting for an engine, held arp notes */
{
    uint32_t p, i, n = 0;
    for (p = 0; p < NTRK; p++) {
        for (i = 0; i < NVOICE; i++)
            n += trk[p].v[i].active && trk[p].v[i].gate;
        n += trk[p].xp_n + trk[p].nheld + (trk[p].arp_note != 0);
    }
    return n;
}
static void reset(uint32_t mo)
{
    ui_power_on();
    memset(midi_ch, 0, sizeof midi_ch); memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_owners, 0, sizeof midi_owners); memset(&midi_map, 0, sizeof midi_map);
    memset(split_cfg, 0, sizeof split_cfg); memset(&um, 0, sizeof um);
    memset(scn_role, 0, sizeof scn_role); memset(scn_pat, 0, sizeof scn_pat); scn_on = 0; scn_keys = 0;
    memset(scn_sl, 0, sizeof scn_sl); memset(mo_ks, 0, sizeof mo_ks); kb_lt = 0;
    mi_r = mi_w = 0; midi_in_overflow = 0;
    fm1_in.notes = kb_prev = 0; song.sel = 1; song.octave = 0;
    usb.config = 1; mo_lvl = (uint8_t)mo;
    blocks(4);
    mo_r = mo_w; memset(mo_open, 0, sizeof mo_open);
}
static void kdown(uint32_t k) { fm1_in.notes |= 1u << k; blocks(2); }
static void kup(uint32_t k) { fm1_in.notes &= ~(1u << k); blocks(2); }
static void midi(uint32_t st, uint32_t d1, uint32_t d2) { midi_enqueue((st >> 4) | st << 8 | d1 << 16 | d2 << 24, 1); blocks(1); }
static int released(const char *what, uint32_t settle)
{
    uint32_t h;
    blocks(settle);
    h = hung() + mo_hung();
    if (h) {
        uint32_t c, x, i;
        printf("   (hung %u: voices/notes %u, MIDI OUT %u)", h, hung(), mo_hung());
        for (c = 0; c < 16u; c++)
            for (x = 0; x < 128u; x++)
                if (mo_open[c][x])
                    printf(" out ch%u:%u (step %u action %u)", c + 1u, x, mo_who[c][x] >> 8, mo_who[c][x] & 255u);
        for (c = 0; c < NTRK; c++) {
            for (i = 0; i < NVOICE; i++)
                if (trk[c].v[i].active && trk[c].v[i].gate)
                    printf(" T%u:%u", c + 1u, trk[c].v[i].note);
            if (trk[c].xp_n || trk[c].nheld || trk[c].arp_note)
                printf(" T%u xp %u held %u arp %u", c + 1u, trk[c].xp_n, trk[c].nheld, trk[c].arp_note);
        }
        printf("\n");
    }
    return check(what, !h);
}
static int sounded(void) { uint32_t p, i, n = 0; for (p = 0; p < NTRK; p++) for (i = 0; i < NVOICE; i++) n += trk[p].v[i].active && trk[p].v[i].gate; return n != 0; }

/* the keys */
static int keys_test(void)
{
    int bad = 0;
    track_t *t;
    uint32_t mo;
    for (mo = MO_KEYS; mo <= MO_ALL; mo += MO_ALL - MO_KEYS) {
        const char *m = mo == MO_ALL ? " (MIDI OUT ALL)" : " (MIDI OUT KEYS)";
        char b[96];
#define CASE(name, change) do { reset(mo); t = &trk[1]; t->p[P_VOICE] = V_POLY; (void)t; kdown(7); kdown(11); \
        int s_ = sounded(); change; kup(7); kup(11); snprintf(b, sizeof b, "key held, %s, let go%s", name, m); \
        bad += released(b, 40) + check("  (it sounded)", s_); } while (0)
        CASE("OCT+", song.octave = 1);
        CASE("OCT-", song.octave = -2);
        CASE("CHORD on", t->p[P_CHRD] = CD_SEV);
        CASE("SCALE / QUANT SNAP", (t->p[P_QUANT] = QN_SNAP, t->p[P_SCALE] = 5, t->p[P_ROOT] = 3));
        CASE("ARP on", (t->p[P_AMODE] = AM_UP, blocks(30)));
        CASE("LATCH (HOLD) on", (t->p[P_AHOLD] = 1, blocks(2)));
        CASE("KEY SPLIT on", (split_cfg[0] = 1, split_cfg[2] = 4, split_cfg[3] = 5, split_cfg[1] = 9));
        CASE("track selected", song.sel = 4);
        CASE("ROOT / TRANS", (t->p[P_ROOT] = 5, t->p[P_TRANS] = 7));
        CASE("preset load", (apply_preset_to(t, 2), blocks(30)));
        CASE("engine load", (set_engine_of(t, 0), apply_preset_to(t, 1), blocks(30)));
        CASE("MIDI OUT changed", mo_lvl = (uint8_t)(mo == MO_ALL ? MO_OFF : MO_KEYS));
#undef CASE
        reset(mo); t = &trk[1];
        t->p[P_CHRD] = CD_TRI; kdown(7); t->p[P_CHRD] = 0; t->p[P_AMODE] = AM_UP; blocks(40); kup(7);
        t->p[P_AMODE] = 0; snprintf(b, sizeof b, "chord key held, ARP on, let go, ARP off%s", m);
        bad += released(b, 40);
        reset(mo); t = &trk[1];
        t->p[P_AMODE] = AM_UP; kdown(7); kdown(9); blocks(40); t->p[P_AMODE] = 0; blocks(4); kup(7); kup(9);
        snprintf(b, sizeof b, "ARP on, keys held, ARP off, let go%s", m);
        bad += released(b, 40);
        reset(mo); t = &trk[1];
        t->p[P_AHOLD] = 1; kdown(7); kup(7); blocks(4); kdown(9); t->p[P_AHOLD] = 0; blocks(2); kup(9);
        snprintf(b, sizeof b, "LATCH, a chord, HOLD off with a key held, let go%s", m);
        bad += released(b, 40);
    }
    return bad;
}

/* SCENE: the roles' notes, from the key down, end at its key-up whatever changed */
static void scene_setup(void)
{
    static const uint8_t R[NTRK] = {SC_KEEP, SC_CHORD, SC_KEEP, SC_KEEP, SC_LEAD, SC_BASS};
    uint32_t i;
    memcpy(scn_role, R, sizeof scn_role);
    for (i = 0; i < NTRK; i++)
        sc_setup(i);
    scn_on = 1;
    trk[4].p[P_AMODE] = 0;                              /* (LEAD a plain voice: a chord of its own) */
    trk[4].p[P_VOICE] = V_POLY;
    blocks(2);
}
static int scene_test(void)
{
    int bad = 0;
    uint32_t mo;
    for (mo = MO_KEYS; mo <= MO_ALL; mo += MO_ALL - MO_KEYS) {
        const char *m = mo == MO_ALL ? " (MIDI OUT ALL)" : " (MIDI OUT KEYS)";
        char b[96];
#define SCASE(name, change) do { reset(mo); scene_setup(); kdown(7); kdown(14); blocks(20); int s_ = sounded(); change; \
        blocks(20); kup(7); kup(14); snprintf(b, sizeof b, "SCENE: keys held, %s, let go%s", name, m); \
        bad += released(b, 60) + check("  (it sounded)", s_); } while (0)
        SCASE("OCT+", song.octave = 1);
        SCASE("roles diced", sc_dice());
        SCASE("a role to KEEP", (scn_role[1] = SC_KEEP, scn_role[4] = SC_KEEP));
        SCASE("KEEP given a role", (scn_role[2] = SC_CHORD, sc_setup(2)));
        SCASE("CHORD type and scale", (trk[1].p[P_CHRD] = CD_SEV, trk[1].p[P_SCALE] = 9, trk[4].p[P_CHRD] = CD_SUS4));
        SCASE("BASS pattern", (scn_pat[5] = 6, sc_setup(5)));
        SCASE("SCENE off", scn_on = 0);
        SCASE("KEY SPLIT on", (split_cfg[0] = 1, split_cfg[2] = 2, split_cfg[3] = 3));
#undef SCASE
        reset(mo); scene_setup();
        kdown(7); kdown(19); kup(7); song.octave = 1; kdown(8); kup(19); kup(8);   /* (shared notes, overlapping keys) */
        snprintf(b, sizeof b, "SCENE: overlapping keys and OCT+ between them%s", m);
        bad += released(b, 60);
    }
    return bad;
}

/* MIDI IN */
static int midi_test(void)
{
    int bad = 0, ok;
    track_t *t;
    reset(MO_KEYS); t = &trk[1];
    midi(0x91, 60, 100); midi(0x91, 64, 100);
    ok = sounded();
    midi(0x91, 60, 0); midi(0x81, 64, 0);
    bad += released("MIDI IN: note-on velocity 0 and note-off end their notes", 20) + check("  (it sounded)", ok);
    reset(MO_KEYS);
    um_byte(0x91); um_byte(60); um_byte(100); um_byte(64); um_byte(100); blocks(2);
    ok = sounded();
    um_byte(60); um_byte(0); um_byte(0xF8); um_byte(64); um_byte(0); blocks(2);
    bad += released("MIDI IN (TRS): running status, a note-on velocity 0 as note-off, a clock between", 20) + check("  (it sounded)", ok);
    reset(MO_KEYS); t = &trk[1];
    midi(0x91, 60, 100); midi(0x91, 67, 100); blocks(4);
    set_engine_of(t, 0); apply_preset_to(t, 1); blocks(40);
    ok = sounded();
    midi(0x81, 60, 0); midi(0x81, 67, 0);
    bad += released("MIDI IN: notes held over an engine load, let go", 40) + check("  (carried on the new engine)", ok);
    reset(MO_KEYS); t = &trk[1];
    midi(0x91, 60, 100); apply_preset_to(t, 3); blocks(40); midi(0x81, 60, 0);
    bad += released("MIDI IN: a note held over a preset load, let go", 40);
    reset(MO_KEYS); t = &trk[1];
    t->p[P_AMODE] = AM_UP; midi(0x91, 60, 100); midi(0x91, 64, 100); blocks(40);
    set_engine_of(t, 0); apply_preset_to(t, 1); blocks(40);
    midi(0x81, 60, 0); midi(0x81, 64, 0); t->p[P_AMODE] = 0;
    bad += released("MIDI IN: ARP over an engine load, let go", 40);
    reset(MO_KEYS);
    midi(0x91, 60, 100); midi(0xB1, 64, 127); midi(0x81, 60, 0); blocks(4);
    ok = sounded();
    midi(0xB1, 64, 0);
    bad += released("MIDI IN: CC64 holds a released note, its release ends it", 20) + check("  (held by the pedal)", ok);
    reset(MO_KEYS);
    midi(0x91, 60, 100); midi(0x91, 62, 100); midi(0xB1, 123, 0);
    bad += released("MIDI IN: CC123 ends the channel's notes", 20);
    reset(MO_KEYS);
    midi(0x91, 60, 100); midi(0xB1, 64, 127); midi(0x81, 60, 0); midi(0xB1, 120, 0);
    bad += released("MIDI IN: CC120 ends even a pedal-held note", 20);
    reset(MO_KEYS); song.g[G_ROUTE] = 1;                  /* SEL: every channel the selected track */
    midi(0x95, 60, 100); song.sel = 2; midi(0x85, 60, 0);
    bad += released("MIDI IN (SEL): the selected track changed under a held note", 20);
    reset(MO_KEYS);
    split_cfg[0] = 1; split_cfg[2] = 1; split_cfg[3] = 4; split_cfg[6] = 0; split_cfg[7] = 62;
    midi(0x90, 60, 100); midi(0x90, 70, 100); split_cfg[7] = 72; split_cfg[3] = 2; split_cfg[0] = 0; midi(0x80, 60, 0); midi(0x80, 70, 0);
    bad += released("MIDI IN: KEY SPLIT changed under held notes", 20);
    return bad;
}

/* PANIC: PLAY held 0.7 s, whatever hangs */
static int panic_test(void)
{
    int bad = 0, ok;
    uint32_t k;
    reset(MO_ALL);
    trk[1].p[P_AHOLD] = 1; kdown(7); kup(7);               /* (a latched chord) */
    midi(0x92, 60, 100);                                  /* (a MIDI note left without its note-off) */
    trk[0].p[P_SLEN] = 4; trk[0].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 100};
    transport_req = 1; blocks(8);                          /* (the sequencer playing, MIDI OUT ALL sending) */
    ok = hung() != 0;
    fm1_in.buttons |= 1u << panel.btn[B_PLAY];
    host_pressed |= 1u << panel.btn[B_PLAY];
    for (k = 0; k < 50u; k++) { frame(); blocks(2); }
    fm1_in.buttons &= ~(1u << panel.btn[B_PLAY]);
    frame();
    mo_lvl = MO_ALL;
    bad += released("PANIC (PLAY held): every voice, latch, MIDI note and MIDI OUT note ends, the transport stops", 20);
    bad += check("  (something hung before) and the transport stopped", ok && !song.playing);
    midi(0x82, 60, 0); midi(0x92, 62, 100);
    bad += check("  MIDI IN plays again after it", sounded());
    return bad;
}

/* the sequencer, the arp and MIDI OUT ALL while its transposition, scale, octave and sound change: STOP ends it all */
static int seq_test(void)
{
    int bad = 0;
    uint32_t i;
    reset(MO_ALL);
    for (i = 0; i < 4u; i++) {
        trk[i].p[P_SLEN] = 4; trk[i].p[P_SGATE] = 127;
        trk[i].step[0] = (step_t){{60, 64, 67}, 3, ST_NOTE, 0, 100};
        trk[i].step[2] = (step_t){{62}, 1, ST_NOTE, SF_SLIDE, 100};
    }
    trk[2].p[P_AMODE] = AM_UP; kdown(7); song.sel = 2; kdown(9);
    transport_req = 1; blocks(50);
    song.g[G_STRN] = 2; trk[1].p[P_QUANT] = QN_SEQ; trk[1].p[P_SCALE] = 2; song.octave = 1; blocks(50);
    apply_preset_to(&trk[1], 2); set_engine_of(&trk[0], 2); apply_preset_to(&trk[0], 0); blocks(50);
    trk[2].p[P_AMODE] = AM_DN; blocks(50);
    transport_req = 2; kup(7); kup(9); trk[2].p[P_AMODE] = 0;
    bad += released("sequencer, ARP, MIDI OUT ALL under ALL TR, QUANT SEQ, OCT, preset and engine loads: STOP ends it all", 60);
    return bad;
}

/* random play and changes, then everything let go: nothing left */
static uint32_t fz_seed = 12345u;
static uint32_t fz(uint32_t n) { fz_seed = fz_seed * 1103515245u + 12345u; return (fz_seed >> 8) % n; }
static int fuzz_test(void)
{
    int bad = 0;
    uint32_t round, it, i;
    static const uint8_t KEYS[8] = {3, 7, 9, 12, 14, 16, 19, 23};
    for (round = 0; round < (getenv("FZ_ROUNDS") ? strtoul(getenv("FZ_ROUNDS"), 0, 0) : 6u); round++) {
        uint8_t mn[4][16];
        reset(round & 1u ? MO_ALL : MO_KEYS);
        memset(mn, 0, sizeof mn);
        song.g[G_ROUTE] = (int16_t)(round >> 2);
        for (it = 0; it < 1500u; it++) {
            uint32_t a = fz(27), k = KEYS[fz(8)], t = 1u + fz(2) * 3u;   /* (tracks 2 and 5: not the kit) */
            fz_it = it; fz_a = a;
            switch (a) {
            case 0: case 1: case 2: fm1_in.notes ^= 1u << k; break;
            case 3: case 4: {
                uint32_t c = fz(4), n = fz(16), on = mn[c][n] ^= 1u, st = on || fz(2) ? 0x90u : 0x80u;
                midi(st | c, 52u + n, on ? 90u : 0u);
                break;
            }
            case 5: song.octave = (int8_t)((int32_t)fz(5) - 2); break;
            case 6: trk[t].p[P_CHRD] = (int16_t)fz(5); break;
            case 7: trk[t].p[P_QUANT] = (int16_t)fz(4); trk[t].p[P_SCALE] = (int16_t)fz(8); break;
            case 8: trk[t].p[P_AMODE] = (int16_t)(fz(3) ? 0 : 1 + fz(5)); break;
            case 9: trk[t].p[P_AHOLD] = (int16_t)fz(2); break;
            case 10: split_cfg[0] = (uint8_t)fz(2); split_cfg[1] = (uint8_t)(8 + fz(10)); split_cfg[2] = (uint8_t)fz(3);
                     split_cfg[3] = (uint8_t)(4 + fz(2)); split_cfg[4] = (uint8_t)fz(2); break;
            case 11: song.sel = (uint8_t)(fz(2) ? 1 : 4); break;
            case 12: if (!scn_on) scene_setup(); else scn_on = 0; break;
            case 13: if (scn_on) sc_dice(); break;
            case 14: apply_preset_to(&trk[t], fz(4)); break;
            case 15: set_engine_of(&trk[t], fz(2) ? 0 : 2); apply_preset_to(&trk[t], fz(3)); break;
            case 16: transport_req = song.playing ? 2 : 1; break;
            case 17: midi(0xB0u | fz(4), 64, fz(2) ? 127u : 0u); break;
            case 18: trk[t].p[P_VOICE] = (int16_t)fz(4); break;
            case 19: mo_lvl = (uint8_t)fz(4); break;
            case 20: song.g[G_STRN] = (int16_t)((int32_t)fz(5) - 2); break;
            case 21: trk[t].p[P_CSTR] = (int16_t)(fz(2) ? 0 : 40); break;
            case 22: midi(0xB0u | fz(4), fz(2) ? 123u : 120u, 0); break;
            case 23: if (!fz(8)) panic_all = 1; break;
            case 24: trk[t].p[P_AMODE] = AM_TRNS; break;
            case 25: trk[t].p[P_MUTE] = (int16_t)!trk[t].p[P_MUTE]; break;
            default: if (!fz(4)) { set_engine_of(&trk[t], ENGI_DRUM); apply_preset_to(&trk[t], 0); } break;
            }
            blocks(1 + fz(6));
        }
        fm1_in.notes = 0; blocks(2);                       /* everything let go */
        for (i = 0; i < 64u; i++)
            if ((mn[i >> 4][i & 15u]))
                midi(0x80u | (i >> 4), 52u + (i & 15u), 0);
        for (i = 0; i < 4u; i++)
            midi(0xB0u | i, 64, 0);
        for (i = 0; i < NTRK; i++)
            trk[i].p[P_AHOLD] = trk[i].p[P_AMODE] = trk[i].p[P_MUTE] = 0;
        transport_req = 2;
        mo_lvl = MO_ALL;
        {
            char b[80];
            snprintf(b, sizeof b, "random play and changes (round %u): all let go, nothing hangs", round);
            bad += released(b, 80);
        }
    }
    return bad;
}

int main(void)
{
    int bad = keys_test() + scene_test() + midi_test() + panic_test() + seq_test() + fuzz_test();
    printf("%s\n", bad ? "STUCK NOTES TEST FAILED" : "stuck notes tests passed");
    return bad != 0;
}
