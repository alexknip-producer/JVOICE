/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z6) The keys' velocity and pressure where a played note goes, on the real sources (ui_test.c's): the key scan's
 * side (vel.c: what it hands the keys' block, vel_kb.h) is set by hand here; tests/vel_test.c runs that side itself.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/vel_seq_test.c -lm
 * 1. VELOCITY OFF (the scan's state zeroed, as at power-on with it OFF): a key plays 100 to the voice, MIDI OUT and the
 *    recording, as before V10.6z6; a MIDI channel aftertouch is left alone;
 * 2. a key's velocity (37) reaches the voice (the matrix's VEL), MIDI OUT's note-on, the recorded step, a chord's every
 *    note, a strum's later notes and the arp's notes (MIDI OUT ALL);
 * 3. a press waiting for its strike (vk_hold) is not seen until let through, then plays its velocity;
 * 4. the pressure: a held key's (80) into its track's AT, slewed (no step over 1/8 of the way + 1 a block), MIDI OUT
 *    ALL's channel aftertouch to 80 and back to 0 at the release; recorded (P_XPR bits 2..4) and played back without the
 *    keys; stopped: the sound's value back;
 * 5. the default routing: with AFTERTOUCH on, INIT SOUND and a new project's sounds AT -> CUT 20 in the matrix's free
 *    slot 4 (not on a DRUM track, not over a slot in use; AFTERTOUCH OFF: none, as before); a preset load adds none;
 * 6. the MENU: CONTROL's rows VELOCITY, VEL MIN, AFTERTOUCH, VEL LEARN (an older setting: NORMAL, 20, MID, ON); RESET
 *    asks for the seeds (vel.c) once picked, and reads RESET;
 * 7. (after V10.7.2) a sound's VEL (EDIT > VOICE 2, core.h P_TVEL): 100 % (127, the default and every older store's)
 *    as played; 0 % every note 100 from the keys, the sequencer, the arp and MIDI in (the matrix's VEL too), a step's
 *    ACCENT 127 still, the recording the key's own; 50 % halfway; a preset sets it (the factory basses 0 %, the others
 *    100 %), a project keeps it, an older one loads at 100 %.
 * 8. (V10.7.10) the keys whose press never waits for its strike's velocity (seq.c kb_drum_keys): exactly those whose
 *    note goes to a DRUM track (the selected one, KEY SPLIT's half either way round, the DRUM grid). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("vel_seq: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void blocks(uint32_t n) { while (n--) events_block(CTL); }
static void ms_run(uint32_t ms)
{
    uint32_t k;
    for (k = 0; k < ms / 16u; k++) {
        frame();
        blocks(16u * (FS / 1000u) / CTL);
    }
}
static uint32_t on_vel[8], n_on, at_last = 999, n_at;   /* MIDI OUT: note-ons' velocities, the last aftertouch */
static void drain(void)
{
    while (mo_r != mo_w) {
        uint32_t pkt = midi_out_q[mo_r++ % MQ], st = (pkt >> 8) & 0xF0u, d1 = (pkt >> 16) & 127u, d2 = pkt >> 24;
        if (st == 0x90u && d2 && n_on < 8u)
            on_vel[n_on++] = d2;
        if (st == 0xD0u)
            at_last = d1, n_at++;
    }
}
static void setup(void)
{
    ui_power_on();
    usb.config = 1;
    mo_r = mo_w = 0;
    mo_lvl = MO_KEYS;
    song.sel = 0;
    memset(&vs, 0, sizeof vs);
    trk[0].p[P_TVEL] = 127;                           /* (track 1 at power-on is a bass, its VEL 0 %: 7.) */
    frame();
    n_on = 0;
    n_at = 0;
    at_last = 999;
}
static uint32_t vvel(const track_t *t)               /* the velocity the newest voice sounds */
{
    uint32_t i, best = 0, age = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].age >= age) { age = t->v[i].age; best = t->v[i].vel; }
    return best;
}
static void gclear_step(step_t *s) { memset(s, 0, sizeof *s); }
static uint32_t page_find(const char *title)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++) if (str_eq(PAGES[i].title, title)) return i;
    return 0;
}
static uint32_t KEY = 12u;                            /* F4: a key with a converter */

int main(void)
{
    char what[160];
    uint32_t i, k;
    /* ---- 1. OFF */
    setup();
    trk[0].at = 55;                                   /* (as a MIDI channel aftertouch left it) */
    key_down(KEY);
    blocks(4);
    drain();
    ck("1. VELOCITY OFF: a key plays 100 (the voice's VEL, MIDI OUT's note-on)", trk[0].m_vel == 100u && n_on == 1u &&
       on_vel[0] == 100u);
    ck("   nothing pressed harder: a MIDI channel aftertouch on the track left alone", trk[0].at == 55u);
    key_up(KEY);
    blocks(4);
    /* ---- 2. a velocity */
    setup();
    vs.on = 1;
    kb_velx[KEY] = 37u ^ 100u;
    key_down(KEY);
    blocks(4);
    drain();
    ck("2. a key's velocity 37: the voice's VEL, MIDI OUT's note-on", trk[0].m_vel == 37u && n_on == 1u && on_vel[0] == 37u);
    key_up(KEY);
    blocks(4);
    trk[0].p[P_CHRD] = 1;                             /* (a chord: TRIAD, POLY) */
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CSTR] = 20;                            /* (strummed) */
    n_on = 0;
    key_down(KEY);
    blocks(200);
    drain();
    {
        uint32_t ok = n_on >= 3u;
        for (i = 0; i < n_on; i++)
            ok &= on_vel[i] == 37u;
        snprintf(what, sizeof what, "   a strummed chord: its %u notes all 37 (the later ones too)", n_on);
        ck(what, ok);
    }
    key_up(KEY);
    blocks(8);
    trk[0].p[P_CHRD] = 0;
    trk[0].p[P_CSTR] = 0;
    midi_out_mode = MO_ALL;
    frame();
    trk[0].p[P_AMODE] = 1;                            /* (ARP UP) */
    transport_req = 1;
    ms_run(32);
    drain();
    n_on = 0;
    key_down(KEY);
    ms_run(400);
    drain();
    {
        uint32_t ok = n_on >= 2u;
        for (i = 0; i < n_on; i++)
            ok &= on_vel[i] == 37u;
        snprintf(what, sizeof what, "   the arp (MIDI OUT ALL): its %u notes 37", n_on);
        ck(what, ok);
    }
    key_up(KEY);
    transport_req = 2;
    ms_run(32);
    trk[0].p[P_AMODE] = 0;
    midi_out_mode = MO_KEYS;
    frame();
    /* recording */
    setup();
    vs.on = 1;
    kb_velx[KEY] = 37u ^ 100u;
    trk[0].p[P_SLEN] = 16; trk[0].p[P_SDIV] = 2; song.g[G_BPM] = 120;
    song.rec = 1;
    transport_req = 1;
    ms_run(32);
    key_down(KEY);
    ms_run(160);
    key_up(KEY);
    ms_run(64);
    {
        uint32_t v = 0;
        for (i = 0; i < 16u; i++)
            if (trk[0].step[i].vel)
                v = trk[0].step[i].vel;
        ck("   recorded: the step keeps the key's velocity (37)", v == 37u);
    }
    song.rec = 0;
    transport_req = 2;
    ms_run(32);
    /* ---- 3. a press waiting */
    setup();
    vs.on = 1;
    kb_velx[KEY] = 0;
    vk_hold = 1u << KEY;
    key_down(KEY);
    blocks(3);
    drain();
    k = n_on;
    kb_velx[KEY] = 90u ^ 100u;
    vk_hold = 0;
    blocks(2);
    drain();
    ck("3. a press waiting for its strike: unseen until let through, then its velocity (90)", k == 0 && n_on == 1u &&
       on_vel[0] == 90u);
    key_up(KEY);
    blocks(4);
    /* ---- 4. the pressure */
    setup();
    vs.on = 3;
    midi_out_mode = MO_ALL;
    frame();
    key_down(KEY);
    blocks(4);
    vk[KEY].at = 80;
    {
        uint32_t step_ok = 1, prev = trk[0].at;
        for (i = 0; i < 100u; i++) {
            int32_t want = 80 - (int32_t)prev;
            events_block(CTL);
            if ((int32_t)trk[0].at - (int32_t)prev > want / 8 + 1) step_ok = 0;
            prev = trk[0].at;
        }
        drain();
        snprintf(what, sizeof what, "4. a held key's pressure 80: the track's AT %u, slewed; MIDI OUT ALL's channel aftertouch %u",
                 trk[0].at, at_last);
        ck(what, trk[0].at == 80u && step_ok && at_last == 80u && n_at <= 45u);
    }
    vk[KEY].at = 0;
    key_up(KEY);
    blocks(100);
    drain();
    ck("   let go: back to 0, and MIDI OUT's aftertouch 0", trk[0].at == 0 && at_last == 0);
    /* recorded */
    setup();
    vs.on = 3;
    trk[0].p[P_SLEN] = 16; trk[0].p[P_SDIV] = 2; song.g[G_BPM] = 120;
    motion.on = 0;
    song.rec = 1;
    transport_req = 1;
    ms_run(32);
    key_down(KEY);
    ms_run(160);
    vk[KEY].at = 90;
    ms_run(480);
    vk[KEY].at = 0;
    ms_run(48);
    key_up(KEY);
    ms_run(48);
    {
        uint32_t n = 0, mx = 0;
        for (i = 0; i < motion.count; i++) {
            const motion_event_t *e = &motion.event[i];
            if (MOTION_TRK(e) == 0u && MOTION_ID(e) == P_XPR && MOTION_VAL(e) == 127 - (90 >> 4 << 2))
                n++;
        }
        song.rec = 0;
        frame();
        for (i = 0; i < 2u * 16u * 125u / 16u; i++) {
            ms_run(16);
            if (trk[0].at > mx) mx = trk[0].at;
        }
        snprintf(what, sizeof what, "   recorded (P_XPR %d, %u events), played back without the keys: the track's AT to %u",
                 127 - (90 >> 4 << 2), n, mx);
        ck(what, n >= 1u && mx == 5u * 18u);
    }
    transport_req = 2;
    ms_run(64);
    blocks(200);
    ck("   stopped: the sound's value back (P_XPR 127), the pressure 0", trk[0].p[P_XPR] == 127 && trk[0].at == 0);
    /* ---- 5. the default routing */
    setup();
    VEL_SET2 = 2;                                     /* (AFTERTOUCH OFF: its place 0, stored ^ 2) */
    power_on_parts();
    ck("5. AFTERTOUCH OFF: a new project's sounds as before (no routing added)", trk[1].p[P_M1SRC + 9] != MS_AT &&
       trk[2].p[P_M1SRC + 9] != MS_AT);
    VEL_SET2 = 0;                                     /* (AFTERTOUCH MID, the default) */
    load_depth++;                                     /* (as ui_test.c's ui_power_on_099: the power-on parts) */
    fm6_init();
    power_on_parts();
    load_depth--;
    {
        uint32_t ok = 1, drums = 0;
        for (i = 0; i < trk_live; i++) {
            const track_t *t = &trk[i];
            if (drum_track(t)) {
                drums++;
                ok &= t->p[P_M1SRC + 9] != MS_AT;
            } else {
                ok &= t->p[P_M1SRC + 9] == MS_AT && t->p[P_M1SRC + 10] == MD_CUT && t->p[P_M1SRC + 11] == 20;
            }
        }
        ck("   AFTERTOUCH on: a new project's sounds AT -> CUT 20 in slot 4 (a DRUM track none)", ok && drums >= 1u);
        apply_preset_to(&trk[0], 1);
        ck("   a preset loaded: no routing added (its slots its own)", trk[0].p[P_M1SRC + 9] != MS_AT ||
           trk[0].p[P_M1SRC + 11] != 20);
        trk[0].p[P_M1SRC + 9] = MS_LFO; trk[0].p[P_M1SRC + 10] = MD_PAN; trk[0].p[P_M1SRC + 11] = 5;
        vel_at_route(&trk[0]);
        ck("   slot 4 in use: kept", trk[0].p[P_M1SRC + 9] == MS_LFO && trk[0].p[P_M1SRC + 11] == 5);
        init_sound_of(&trk[1], trk[1].eng_req);
        ck("   INIT SOUND: AT -> CUT 20", trk[1].p[P_M1SRC + 9] == MS_AT && trk[1].p[P_M1SRC + 10] == MD_CUT);
    }
    /* ---- 6. the MENU */
    setup();
    VEL_SET = VEL_SET2 = 0;
    ck("6. MENU > CONTROL: VELOCITY, VEL MIN, AFTERTOUCH, VEL LEARN; an older setting NORMAL, 20, MID, ON",
       MI_TAB[MI_VEL] == MTAB_CONTROL && MI_TAB[MI_VLEARN] == MTAB_CONTROL && str_eq(MI_NAME[MI_VEL], "VELOCITY") &&
       str_eq(menu_vname(MI_VEL, menu_get(MI_VEL)), "NORMAL") && str_eq(menu_vname(MI_VELMIN, menu_get(MI_VELMIN)), "20") &&
       str_eq(menu_vname(MI_AT, menu_get(MI_AT)), "MID") && str_eq(menu_vname(MI_VLEARN, menu_get(MI_VLEARN)), "ON") &&
       menu_n(MI_VEL) == 4u && menu_n(MI_VELMIN) == 8u && menu_n(MI_AT) == 4u && menu_n(MI_VLEARN) == 3u);
    menu_put(MI_VEL, 0);
    menu_put(MI_AT, 3);
    menu_put(MI_VELMIN, 0);
    ck("   VELOCITY OFF, AFTERTOUCH HIGH, VEL MIN 1: stored", menu_get(MI_VEL) == 0u && menu_get(MI_AT) == 3u &&
       str_eq(menu_vname(MI_VELMIN, menu_get(MI_VELMIN)), "1") && (VEL_SET & 3u) == 2u);
    vk_cal = 1;
    menu_put(MI_VLEARN, 2);
    k = vk_cal;
    vk_cal = 1;
    menu_put(MI_VLEARN, 2);
    ck("   VEL LEARN RESET: the seeds asked for once picked (vel.c), it reads RESET", k == 2u && vk_cal == 1u &&
       menu_get(MI_VLEARN) == 2u);
    /* ---- 7. a sound's VEL */
    {
        track_t *t = &trk[0];
        uint32_t e, pi, okb = 1, nb = 0, no = 0;
        setup();
        vs.on = 1;
        trk[3].p[P_TVEL] = 0;
        track_defaults(&trk[3]);
        ck("7. VEL: 100 % by default (127; NRPN 80, VOICE 2's KNOB 3)", trk[3].p[P_TVEL] == 127 && track_desc(&trk[3], P_TVEL)->def == 127 &&
           str_eq(track_desc(&trk[3], P_TVEL)->label, "VEL") && track_desc(&trk[3], P_TVEL)->max == 127 && P_TVEL == 80 && PAGES[page_find("VOICE 2")].id[2] == P_TVEL);
        t->p[P_TVEL] = 127;                           /* (track 1 at power-on is a bass: 0 %) */
        kb_velx[KEY] = 37u ^ 100u;
        key_down(KEY); blocks(4);
        ck("   100 %: a key's 37 as played (the voice, the matrix's VEL)", t->m_vel == 37u && vvel(t) == 37u);
        key_up(KEY); blocks(8);
        t->p[P_TVEL] = 0;
        key_down(KEY); blocks(4);
        ck("   0 %: a key's 37 plays 100 (the voice, the matrix's VEL)", t->m_vel == 100u && vvel(t) == 100u);
        key_up(KEY); blocks(8);
        kb_velx[KEY] = 127u ^ 100u;
        key_down(KEY); blocks(4);
        ck("   0 %: a key struck at 127 plays 100 too (only a step's ACCENT keeps 127)", t->m_vel == 100u);
        key_up(KEY); blocks(8);
        t->p[P_TVEL] = 64;
        kb_velx[KEY] = 37u ^ 100u;
        key_down(KEY); blocks(4);
        ck("   50 %: a key's 37 plays 69 (halfway to 100)", t->m_vel == 69u && vvel(t) == 69u);
        key_up(KEY); blocks(8);
        t->p[P_TVEL] = 0;
        /* the sequencer playing a step of 40 and an accented one */
        for (pi = 0; pi < 16u; pi++) gclear_step(&t->step[pi]);
        t->step[0].time = ST_NOTE; t->step[0].n = 1; t->step[0].note[0] = 60; t->step[0].vel = 40;
        t->step[1].time = ST_NOTE; t->step[1].n = 1; t->step[1].note[0] = 62; t->step[1].vel = 40; t->step[1].flags = SF_ACCENT;
        t->p[P_SLEN] = 2; t->p[P_SDIV] = 2; song.g[G_BPM] = 120;
        transport_req = 1;
        {
            uint32_t n100 = 0, n127 = 0, other = 0, j, last = 0;
            for (j = 0; j < 120u; j++) {
                blocks(16u * (FS / 1000u) / CTL / 4u);
                if (t->m_vel && t->m_key != last) { last = t->m_key; n100 += t->m_vel == 100u; n127 += t->m_vel == 127u;
                                                    other += t->m_vel != 100u && t->m_vel != 127u; }
            }
            snprintf(what, sizeof what, "   0 %%: the sequencer's step of 40 plays 100 (%u), its ACCENT still 127 (%u), none else (%u)",
                     n100, n127, other);
            ck(what, n100 >= 1u && n127 >= 1u && !other);
        }
        transport_req = 2; ms_run(64);
        /* MIDI in */
        midi_enqueue(0x9u | 0x90u << 8 | 64u << 16 | 30u << 24, 1); events_block(CTL);
        ck("   0 %: a MIDI note of 30 plays 100", t->m_vel == 100u);
        midi_enqueue(0x8u | 0x80u << 8 | 64u << 16, 1); blocks(8);
        /* the arp: the keys' velocity, even at 127 (not an accent) */
        midi_out_mode = MO_ALL; frame();
        kb_velx[KEY] = 127u ^ 100u;
        t->p[P_AMODE] = 1;
        transport_req = 1; ms_run(32);
        t->m_vel = 0;
        key_down(KEY); ms_run(400);
        ck("   0 %: the arp's notes of a key struck at 127 play 100", t->m_vel == 100u);
        key_up(KEY); transport_req = 2; ms_run(32);
        t->p[P_AMODE] = 0; midi_out_mode = MO_KEYS; frame();
        /* recorded: the key's own velocity */
        setup();
        vs.on = 1;
        t->p[P_TVEL] = 0;
        kb_velx[KEY] = 37u ^ 100u;
        t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; song.g[G_BPM] = 120;
        song.rec = 1; transport_req = 1; ms_run(32);
        key_down(KEY); ms_run(160); key_up(KEY); ms_run(64);
        {
            uint32_t v = 0;
            for (i = 0; i < 16u; i++) if (t->step[i].vel) v = t->step[i].vel;
            ck("   0 %: the recording keeps the key's 37 (VEL up later: the dynamics back)", v == 37u);
        }
        song.rec = 0; transport_req = 2; ms_run(32);
        /* presets: the factory basses 0 %, every other 100 % */
        for (e = 0; e < NENGINES; e++) {
            const engine_t *en = ENGINES[e];
            if (en->name[0] == '-' || eng_gone(e) || !en->npresets) continue;
            set_engine_of(t, e);
            for (pi = 0; pi < en->npresets; pi++) {
                uint32_t bass = PR_TAG(&en->presets[pi]) == PT_BASS;
                t->p[P_TVEL] = 40;
                apply_preset_to(t, pi);
                t->engine = t->eng_req;
                okb &= t->p[P_TVEL] == (bass ? 0 : 127);
                nb += bass; no += !bass;
            }
        }
        snprintf(what, sizeof what, "   a preset sets it: the %u factory basses 0 %%, the other %u presets 100 %%", nb, no);
        ck(what, okb && nb >= 10u && no > nb);
        /* a project keeps it; one of before loads at 100 % */
        setup();
        trk[1].p[P_TVEL] = 40; trk[2].p[P_TVEL] = 127;
        project_save(1);
        trk[1].p[P_TVEL] = 99; trk[2].p[P_TVEL] = 0;
        project_load(1);
        ck("   a project keeps each track's VEL (40, 127)", trk[1].p[P_TVEL] == 40 && trk[2].p[P_TVEL] == 127);
        ck("   saved with the VEL mark (FUNC byte 65, V10.7.6)", proj_slot[1].raw[65] == 'V');
        {   /* (V10.7.6) a FUNC of before the mark (MUTATE's P_MCFG in the slot: 127 - cfg; or V10.7.3..7.5's VEL): 100 % */
            uint32_t h;
            proj_slot[1].raw[65] = 0;
            h = proj_hash(proj_slot[1].raw, PROJ_STORE_SIZE - 4u); memcpy(proj_slot[1].raw + PROJ_STORE_SIZE - 4u, &h, 4);
            trk[1].p[P_TVEL] = 99;
            project_load(1);
            ck("   one saved before the mark (VEL 40 there): VEL 100 % on every track", trk[1].p[P_TVEL] == 127 &&
               trk[2].p[P_TVEL] == 127 && trk[0].p[P_TVEL] == 127);
            trk[1].p[P_TVEL] = 40;
            project_save(1); trk[1].p[P_TVEL] = 99; project_load(1);
            ck("   saved again: its VEL kept from now on", trk[1].p[P_TVEL] == 40);
        }
        {
            int16_t in[69], out[P_COUNT], def[P_COUNT];
            for (i = 0; i < P_COUNT; i++) def[i] = TP[i].def;
            for (i = 0; i < 69u; i++) in[i] = 5;
            params_by_count(out, in, 69u, def);         /* (a store of P_COUNT 69: before the DIGITAL slots) */
            ck("   an older store without the slot: VEL 100 % (its default)", out[P_TVEL] == 127);
        }
    }
    /* ---- 8. (V10.7.10) the keys whose press never waits for its strike (vel.c vel_note_on, vel_kb.h vs.now: seq.c
     * kb_drum_keys): those keyboard_block sends to a DRUM track; checked against where each key's note goes */
    {
        uint32_t dt = NTRK, st = NTRK, ok = 1, c, kk, m, split_ok = 0;
        setup();
        for (i = 0; i < NTRK; i++) {
            if (drum_track(&trk[i]) && dt == NTRK) dt = i;
            if (!drum_track(&trk[i]) && st == NTRK) st = i;
        }
        ok &= dt < NTRK && st < NTRK;
        for (c = 0; ok && c < 4u; c++) {                 /* the selected track a DRUM, a synth; KEY SPLIT both ways */
            memset(split_cfg, 0, sizeof split_cfg);
            song.sel = (uint8_t)(c == 1u ? st : dt);
            if (c >= 2u) {
                split_cfg[0] = 1;
                split_cfg[1] = (uint8_t)(c == 2u ? 8u : 0u);   /* (0: the default cut, 13) */
                split_cfg[2] = (uint8_t)(c == 2u ? dt : st);   /* low */
                split_cfg[3] = (uint8_t)(c == 2u ? st : dt);   /* high */
            }
            m = kb_drum_keys();
            if (c >= 2u)
                split_ok += m != 0u && (m & 0xFFFFFu) != 0xFFFFFu;
            for (kk = 0; kk < 20u; kk++) {
                key_down(kk);
                blocks(1);
                ok &= ((m >> kk) & 1u) == (uint32_t)(drum_track(&trk[kb_trk[kk]]) != 0);
                key_up(kk);
                blocks(1);
            }
        }
        memset(split_cfg, 0, sizeof split_cfg);
        song.sel = (uint8_t)st;
        song.grid = 1;                                   /* (the DRUM grid: its lanes) */
        ok &= kb_drum_keys() == ~0u;
        song.grid = 0;
        ok &= kb_drum_keys() == 0u;
        song.sel = 0;
        ck("8. no wait on a DRUM track's keys: the selected one, KEY SPLIT's half (both ways), the DRUM grid; a synth's wait",
           ok && split_ok == 2u);
    }
    printf("vel_seq: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
