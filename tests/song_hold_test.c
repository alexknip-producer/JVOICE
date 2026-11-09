/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (V10.6z8) a GENE song keeps the sounds the player changes while it plays (song_chain.c snd_held, song_play.c
 * sec_apply): a YouTube user's "I change the instruments and at the end of the phrase they go back". A preset, an
 * engine, a sound knob and the DRUM-X kit changed mid-song stay over every section after; the sections still bring
 * their steps, pattern and mutes; a song nobody touches holds nothing (no false hold); EDIT > RECALL lets a track go
 * (the sections' sound again), STOP brings the music before PLAY back with the changed sounds over it (V10.7.6), a
 * new throw lets everything go.
 *   cc -O1 -g -w -Wno-implicit-function-declaration -fsanitize=address,bounds -Ibuild/gen -Ifirmware/src \
 *      tests/song_hold_test.c -lm -o build/song-check/song-hold */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void make_song(unsigned genre, unsigned seed)
{
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    gp.genre = genre; gp.seed = seed; gp.energy = 60; gp.form = 2;
    gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
    for (unsigned k = 0; k < GP_N; ++k) gp.amt[k] = GA_AUTO;
    gen_go();
}
/* the section track t plays in the current row, decoded into proj_scratch */
static const proj_trk_t *row_trk(uint32_t t)
{
    uint32_t r = chain.row;
    if (!proj_import(&proj_scratch, &proj_slot[row_src(r, t, chain.config.row[r].slot)], sizeof(project_store_t)))
        return 0;
    return &proj_scratch.t[t];
}
static uint32_t sec_preset(const proj_trk_t *p)
{
    uint32_t e = eng_live(p->engine);
    return ENGINES[e]->npresets ? (p->preset >= PROJ_DEF_KEEP ? 0u : p->preset) % ENGINES[e]->npresets : 0u;
}
/* bars until the row changes (n times); 0: it never did */
static int next_row(void)
{
    uint32_t last = chain.row, bar;
    for (bar = 0; bar < 80u; bar++) {
        events_block(chain_bar());
        song_poll();
        if (chain.row != last)
            return 1;
    }
    return 0;
}

int main(void)
{
    int bad = 0, ok, rows, differs;
    uint32_t k, a = NTRK, b = NTRK, d = NTRK, pre_a, eng_b, dec_b, slen_ok, steps_ok;
    uint32_t pre_eng[NTRK], pre_pre[NTRK];
    step_t pre_step[NTRK];
    dx_lane_t kit0;

    /* 1. a song nobody touches: every section in, nothing held (every genre) */
    ok = 1;
    for (k = 0; k <= NGENRE + 1u; k++) {
        make_song(k > NGENRE ? GEN_EXPLORE : k, 31 + k);
        press(B_PLAY); events_block(CTL); song_poll(); frame();
        ok &= chain.running && snd_held == 0u;
        for (rows = 0; rows < 10 && next_row(); rows++)
            ok &= snd_held == 0u;
        ok &= rows >= 3;
        if (!ok) { printf("song hold: genre %u held %02x after %d rows\n", k, snd_held, rows); break; }
        seq_stop(); song_poll(); frame();
    }
    bad += check("V10.6z8 SONG HOLD: untouched GENE songs (every genre) hold no sound over their sections", ok);
    make_song(1, 31);
    press(B_PLAY); events_block(CTL); song_poll(); frame();
    next_row();
    seq_stop(); song_poll(); frame();
    bad += check("V10.6z8 SONG HOLD: an untouched song's STOP brings the music before PLAY back, as before",
                 live.cur == 0 && proj_cur == 0);

    /* 2. the player changes sounds while it plays (an unsaved edit before PLAY: step 64 of every track) */
    make_song(1, 31);
    for (k = 0; k < NTRK; k++) {
        step_t *st = &trk[k].step[NSTEP - 1u];
        st->time = ST_NOTE; st->n = 1; st->note[0] = (uint8_t)(50u + k); st->vel = 77;
        pre_step[k] = *st; pre_eng[k] = trk[k].eng_req; pre_pre[k] = trk[k].preset;
    }
    press(B_PLAY); events_block(CTL); song_poll(); frame();
    for (k = 0; k < NTRK; k++) {                        /* a pitched track with presets, another for a knob, the DRUM */
        uint32_t e = trk[k].eng_req;
        if (e == ENGI_DRUM) { if (d == NTRK) d = k; continue; }
        if (a == NTRK && ENGINES[e]->npresets > 2u) a = k;
        else if (b == NTRK && e != ENGI_INPUT) b = k;
    }
    ok = a < NTRK && b < NTRK && d < NTRK;
    pre_a = (trk[a].preset + 1u) % ENGINES[trk[a].eng_req]->npresets;
    apply_preset_to(&trk[a], pre_a);                    /* a preset (BROWSE's KNOB 1) */
    pre_a = trk[a].preset;
    eng_b = trk[b].eng_req == ENGI_ANALOG ? ENGI_PHASE : ENGI_ANALOG;
    set_engine_of(&trk[b], eng_b);                      /* another engine */
    trk[b].p[P_DEC] = (int16_t)(trk[b].p[P_DEC] > 60 ? 20 : 100);   /* and a sound knob (DEC) */
    dec_b = (uint32_t)trk[b].p[P_DEC];
    dx_kit[0].a[DXP_DECAY] = (uint8_t)(dx_kit[0].a[DXP_DECAY] > 64u ? 10u : 120u);   /* the DRUM-X kit (EDIT > SOUND) */
    kit0 = dx_kit[0];
    slen_ok = steps_ok = 1;
    differs = 0;
    for (rows = 0; rows < 8 && next_row(); rows++) {
        const proj_trk_t *pa = row_trk(a);
        ok &= pa && trk[a].preset == pre_a && trk[b].eng_req == eng_b && trk[b].p[P_DEC] == (int16_t)dec_b &&
              !memcmp(&dx_kit[0], &kit0, sizeof kit0) && (snd_held & (1u << a | 1u << b | 1u << NTRK)) ==
                                                              (1u << a | 1u << b | 1u << NTRK);
        differs |= pa && sec_preset(pa) != pre_a;     /* (the section would have put its own back) */
        slen_ok &= pa && trk[a].p[P_SLEN] == pa->p[P_SLEN];
        steps_ok &= pa && (row_var(chain.row, a) || !memcmp(trk[a].step, pa->step, sizeof trk[a].step));
        for (k = 0; k < NTRK; k++)                      /* the others: the section's sound, as before */
            if (k != a && k != b) {
                const proj_trk_t *pk = row_trk(k);
                ok &= pk && trk[k].eng_req == eng_live(pk->engine) && trk[k].preset == sec_preset(pk);
            }
    }
    bad += check("V10.6z8 SONG HOLD: a preset, an engine, a knob and the DRUM-X kit changed mid-song stay over every section",
                 ok && rows >= 3 && differs);
    bad += check("V10.6z8 SONG HOLD: the held tracks still take each section's steps and pattern (LEN)", slen_ok && steps_ok);

    /* 3. EDIT > RECALL while it plays: that track the sections' again */
    ok = project_recall_sound((uint32_t)project_recall_slot(), a) == 0 && !(snd_held & 1u << a) && (snd_held & 1u << b);
    ok &= next_row();
    {
        const proj_trk_t *pa = row_trk(a);
        ok &= pa && trk[a].preset == sec_preset(pa) && !(snd_held & 1u << a) && trk[b].eng_req == eng_b;
    }
    bad += check("V10.6z8 SONG HOLD: EDIT > RECALL in a song lets the track go: the next section's sound, the others held", ok);

    /* 4. (V10.7.6) STOP: the music before PLAY back (its unsaved edits too), the held sounds over it; PLAY again:
     * still held. A knob changed in the last section (not seen by a section change yet) is held too */
    trk[b].p[P_ATK] = (int16_t)(trk[b].p[P_ATK] > 60 ? 7 : 99);
    {
        int16_t atk_b = trk[b].p[P_ATK];
        seq_stop(); song_poll(); frame();
        ok = trk[b].eng_req == eng_b && trk[b].p[P_DEC] == (int16_t)dec_b && trk[b].p[P_ATK] == atk_b &&
             !memcmp(&dx_kit[0], &kit0, sizeof kit0) && (snd_held & (1u << b | 1u << NTRK)) == (1u << b | 1u << NTRK) &&
             !(snd_held & 1u << a);
        bad += check("V10.7.6 SONG HOLD: STOP keeps the held sounds (engine, knobs, the last section's change, the kit)", ok);
        ok = live.cur == 0 && proj_cur == 0;
        for (k = 0; k < NTRK; k++) {
            ok &= !memcmp(&trk[k].step[NSTEP - 1u], &pre_step[k], sizeof pre_step[k]);
            if (!(snd_held >> k & 1u))
                ok &= trk[k].eng_req == eng_live(pre_eng[k]) && trk[k].preset == pre_pre[k];
        }
        bad += check("V10.7.6 SONG HOLD: .. over the music before PLAY (its unsaved edits, the other tracks' sounds)", ok);
        press(B_PLAY); events_block(CTL); song_poll(); frame();
        ok = chain.running && next_row() && trk[b].eng_req == eng_b && trk[b].p[P_DEC] == (int16_t)dec_b &&
             trk[b].p[P_ATK] == atk_b && !memcmp(&dx_kit[0], &kit0, sizeof kit0);
        bad += check("V10.6z8 SONG HOLD: PLAY again keeps holding them", ok);
    }

    /* 5. a new throw: nothing held, the sections' sounds */
    seq_stop(); song_poll();
    make_song(1, 32);
    ok = snd_held == 0u;
    press(B_PLAY); events_block(CTL); song_poll(); frame();
    for (rows = 0; rows < 4 && next_row(); rows++) {
        const proj_trk_t *pb = row_trk(b);
        ok &= snd_held == 0u && pb && trk[b].eng_req == eng_live(pb->engine);
    }
    bad += check("V10.6z8 SONG HOLD: a new GENE throw lets every sound go (the sections' again)", ok && rows >= 2);
    seq_stop(); song_poll();

    /* 6. a live jump (not a song): the sections as stored */
    snd_held = 1u;
    chain.running = 0;
    if (!song_stage(0, SEC_LIVE)) { fm1_irq_off(); sec_apply(); fm1_irq_on(); }
    bad += check("V10.6z8 SONG HOLD: a live jump or CLIPS (no song) takes the parts as stored", snd_held == 0u);
    printf("song hold: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
