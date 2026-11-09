/* SPDX-License-Identifier: GPL-3.0-only */
/* GEN FORM FROM LOOP (gen_loop.c): the loop made a song. The loop's key and chords heard (C major I V vi IV, A minor
 * i VI III VII, a DRUM-X 808 line alone, drums without a tone: the genre's own), the loop untouched as section A, every
 * section of the loop's material only, the new parts on empty tracks in the heard key, the song played from its first
 * row to its last and round, kept through a round trip of its sections and rows, UNDO, a seed a song, OCT+ again
 * another song from the same loop, no empty track: only arranged. Host UI model (tests/ui_test.c) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void lt_note(track_t *t, uint32_t i, const uint8_t *n, uint32_t cnt, uint32_t ties)
{
    step_t *s = &t->step[i];
    uint32_t j;
    memset(s, 0, sizeof *s);
    s->time = ST_NOTE; s->vel = 100; s->n = (uint8_t)cnt;
    for (j = 0; j < cnt; j++) s->note[j] = n[j];
    for (j = 1; j <= ties && i + j < NSTEP; j++) { memset(&t->step[i + j], 0, sizeof *s); t->step[i + j].time = ST_TIE; }
}
static void lt_hit(track_t *t, uint32_t i, uint32_t lanes)
{
    step_t *s = &t->step[i];
    if (s->time != ST_NOTE) { memset(s, 0, sizeof *s); s->time = ST_NOTE; s->vel = 100; }
    s->hit |= (uint8_t)lanes;
}
static void lt_clear(void)
{
    uint32_t k;
    ui_power_on();
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        load_begin(t, LOAD_PAT); track_defaults_steps(t); t->p[P_SLEN] = 64; t->p[P_MUTE] = 0; load_end(t);
    }
    set_engine_of(&trk[0], ENGI_DRUM);
    set_engine_of(&trk[3], 0);                          /* (T4 a synth: ANALOG; ui_test's power-on has DRUM-X there) */
    memcpy(dx_kit, DX_KIT_DEF, sizeof dx_kit);
    song.g[G_BPM] = 124;
}
static void lt_drums(uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++) {
        if (i % 4u == 0u) lt_hit(&trk[0], i, 1u << DV_KICK);
        if (i % 8u == 4u) lt_hit(&trk[0], i, 1u << DV_SNARE);
        if (i % 4u == 2u) lt_hit(&trk[0], i, 1u << DV_HATC);
    }
    trk[0].p[P_SLEN] = (int16_t)len;
}
/* four bars, a chord a bar on T3 (held), its root on T2's beats, a melody on T4 */
static void lt_prog(const uint8_t ch[4][3], const uint8_t *mel)
{
    uint32_t b, i;
    for (b = 0; b < 4u; b++) {
        lt_note(&trk[2], b * 16u, ch[b], 3, 15);
        for (i = 0; i < 4u; i++) { uint8_t r = (uint8_t)(ch[b][0] - 24u); lt_note(&trk[1], b * 16u + i * 4u, &r, 1, 1); }
    }
    for (i = 0; i < 16u; i++) if (mel[i]) lt_note(&trk[3], i * 4u, &mel[i], 1, 1);
}
static void lt_go(unsigned genre, unsigned seed)
{
    gp.genre = (uint8_t)genre; gp.seed = (uint16_t)seed; gp.energy = 60; gp.form = 3;
    gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
    for (unsigned k = 0; k < GP_N; ++k) gp.amt[k] = GA_AUTO;
    gen_go();
}
static uint32_t lt_fh(uint32_t h, const void *p, size_t n) { const uint8_t *b = p; while (n--) h = (h ^ *b++) * 16777619u; return h; }
static uint32_t lt_song(void)                           /* the song made: its sections and rows */
{
    uint32_t h = lt_fh(2166136261u, proj_slot, sizeof proj_slot);
    h = lt_fh(h, &chain_config, sizeof chain_config);
    h = lt_fh(h, song_idx.scene[song_cur], sizeof song_idx.scene[0]);
    h = lt_fh(h, song_idx.tsrc[song_cur], sizeof song_idx.tsrc[0]);
    return lt_fh(h, song_idx.tvar[song_cur], sizeof song_idx.tvar[0]);
}
static proj_trk_t lt_loop[NTRK];
static uint8_t lt_used;                                 /* the loop's tracks */
static void lt_keep(void)
{
    uint32_t k;
    project_capture(&proj_scratch);
    memcpy(lt_loop, proj_scratch.t, sizeof lt_loop);
    lt_used = 0;
    for (k = 0; k < NTRK; k++) if (gls_role(&trk[k]) <= RL_TEX) lt_used |= (uint8_t)(1u << k);   /* (RL_N: ui_layer.c's macro here) */
}
/* the loop's tracks as they were (steps, sound, mix) in what proj_scratch holds */
static int lt_same(void)
{
    uint32_t k, ok = 1;
    for (k = 0; k < NTRK; k++)
        if ((lt_used >> k) & 1u && memcmp(&proj_scratch.t[k], &lt_loop[k], sizeof lt_loop[k])) {
            ok = 0;
            if (getenv("LT_V")) {
                uint32_t i;
                printf("   T%u differs: steps %d eng %u/%u", k + 1, memcmp(proj_scratch.t[k].step, lt_loop[k].step, sizeof lt_loop[k].step) != 0,
                       proj_scratch.t[k].engine, lt_loop[k].engine);
                for (i = 0; i < P_COUNT; i++) if (proj_scratch.t[k].p[i] != lt_loop[k].p[i]) printf(" p%u %d/%d", i, proj_scratch.t[k].p[i], lt_loop[k].p[i]);
                printf("\n");
            }
        }
    return (int)ok;
}
/* every note of a loop track in section s is one the loop's track plays; the drums' lanes the loop's */
static int lt_derived(uint32_t s)
{
    uint32_t k, i, j, ii, ok = 1;
    if (!proj_import(&proj_scratch, &proj_slot[s], sizeof(project_store_t))) return 0;
    for (k = 0; k < NTRK; k++) {
        const proj_trk_t *t = &proj_scratch.t[k];
        uint32_t lanes = 0;
        if (!((lt_used >> k) & 1u)) continue;
        for (ii = 0; ii < NSTEP; ii++) lanes |= lt_loop[k].step[ii].time == ST_NOTE ? lt_loop[k].step[ii].hit : 0u;
        for (i = 0; i < NSTEP; i++) {
            const step_t *st = &t->step[i];
            if (st->time != ST_NOTE) continue;
            ok &= !(st->hit & ~lanes);
            for (j = 0; j < st->n; j++) {
                uint32_t found = 0;
                for (ii = 0; ii < NSTEP && !found; ii++)
                    found = lt_loop[k].step[ii].time == ST_NOTE && memchr(lt_loop[k].step[ii].note, st->note[j], lt_loop[k].step[ii].n) != 0;
                ok &= found;
            }
        }
    }
    return (int)ok;
}
/* the parts FROM LOOP added: their notes in the heard key (pct: of them at least) */
static int lt_in_key(uint32_t pct, uint32_t *nadd)
{
    uint32_t k, i, j, in = 0, all = 0;
    *nadd = 0;
    for (k = 0; k < NTRK; k++) {
        if (!((gls.add >> k) & 1u)) continue;
        ++*nadd;
        for (i = 0; i < dlen(&trk[k]); i++)
            for (j = 0; j < trk[k].step[i].n && trk[k].step[i].time == ST_NOTE; j++) {
                all++; in += gin_mode(trk[k].step[i].note[j]);
                if (getenv("LT_V") && !gin_mode(trk[k].step[i].note[j])) printf("   T%u role %u step %u note %u\n", k + 1, gs.role[k], i, trk[k].step[i].note[j]);
            }
    }
    if (getenv("LT_V")) for (i = 0; i < gs.nch; i++) printf("   chord at %u len %u deg %u q %u alt %u\n", gs.ch[i].at, gs.ch[i].len, gs.ch[i].deg, gs.ch[i].q, gs.ch[i].alt);
    if (getenv("LT_V")) printf("  in key %u of %u (root %d scale %d)\n", in, all, gs.root, gs.scale);
    return all && in * 100u >= all * pct;
}

int main(void)
{
    static const uint8_t CMAJ[4][3] = {{60, 64, 67}, {55, 59, 62}, {57, 60, 64}, {53, 57, 60}};   /* I V vi IV */
    static const uint8_t AMIN[4][3] = {{57, 60, 64}, {53, 57, 60}, {60, 64, 67}, {55, 59, 62}};   /* i VI III VII */
    static const uint8_t MEL_C[16] = {72, 0, 74, 76, 74, 0, 71, 0, 72, 76, 0, 79, 77, 0, 76, 0};
    static const uint8_t MEL_A[16] = {69, 0, 72, 0, 76, 72, 0, 0, 72, 0, 74, 76, 74, 0, 71, 0};
    int bad = 0, ok;
    uint32_t h1, h2, n, k, r, transitions, last, bar;
    char msg[32];

    /* ---- the key and the chords heard */
    lt_clear(); lt_drums(16); lt_prog(CMAJ, MEL_C); lt_keep();
    lt_go(4, 21);                                       /* (HOUSE) */
    ok = gls.heard && gs.root == 0 && gs.scale == SC_MAJ && gs.pn == 4 && gs.pdeg[0] == 0 && gs.pdeg[1] == 4 &&
         gs.pdeg[2] == 5 && gs.pdeg[3] == 3;
    printf("  heard: root %d scale %d chords %u: %u %u %u %u\n", gs.root, gs.scale, gs.pn, gs.pdeg[0], gs.pdeg[1], gs.pdeg[2], gs.pdeg[3]);
    bad += check("FROM LOOP hears C major's I V vi IV (the key, a chord a bar)", ok);
    lt_clear(); lt_drums(16); lt_prog(AMIN, MEL_A); lt_keep();
    lt_go(4, 21);
    ok = gls.heard && gs.root == 9 && gs.scale == SC_MIN && gs.pn == 4 && gs.pdeg[0] == 0 && gs.pdeg[1] == 5 &&
         gs.pdeg[2] == 2 && gs.pdeg[3] == 6;
    printf("  heard: root %d scale %d chords %u: %u %u %u %u\n", gs.root, gs.scale, gs.pn, gs.pdeg[0], gs.pdeg[1], gs.pdeg[2], gs.pdeg[3]);
    bad += check("FROM LOOP hears A minor's i VI III VII", ok);
    lt_clear(); lt_drums(16);
    {   /* an 808 line on DRUM-X's KICK, its chromatic run (EXTENDED: notes 0..12 the kick a semitone each) */
        static const uint8_t L[8] = {9, 9, 12, 16, 9, 7, 12, 9};   /* (A A C E A G C A as the run's pitch classes) */
        int32_t base = (392 + dx_kit[DV_KICK].a[DXP_PITCH] * 12) / 16;
        for (k = 0; k < 16u; k++) {                     /* (the drums' kick lane off: the line alone, on its notes) */
            trk[0].step[k].hit &= (uint8_t)~(1u << DV_KICK);
            if (!trk[0].step[k].hit) trk[0].step[k].time = ST_REST;
        }
        trk[0].p[P_DKEYMODE] = 1;
        dx_kit[DV_KICK].mode = (uint8_t)((dx_kit[DV_KICK].mode & 0x9Fu) | DXM_LONG << 5);
        for (k = 0; k < 8u; k++) {
            uint8_t nt = (uint8_t)(((L[k] - base + 6) % 12 + 12) % 12);
            step_t *s = &trk[0].step[k * 2u];
            if (s->time != ST_NOTE) { memset(s, 0, sizeof *s); s->time = ST_NOTE; s->vel = 110; }
            s->note[0] = nt; s->n = 1;
        }
    }
    lt_keep();
    lt_go(4, 21);
    printf("  heard: root %d scale %d\n", gs.root, gs.scale);
    bad += check("FROM LOOP hears a DRUM-X 808 line alone (its chromatic kick: A minor)", gls.heard && gs.root == 9);
    lt_clear(); lt_drums(16); lt_keep();
    lt_go(4, 21);
    bad += check("drums without a tone: the genre's own key and progression (nothing heard)", !gls.heard && gs.pn >= 1u);

    /* ---- the song from a loop: untouched, derived, played, kept */
    for (unsigned g = 0; g < 3; g++) {
        static const unsigned GEN[3] = {4, 0, GEN_EXPLORE};   /* HOUSE, RANDOM (inferred), EXPLORE (inferred) */
        lt_clear(); lt_drums(16); lt_prog(g == 1 ? AMIN : CMAJ, g == 1 ? MEL_A : MEL_C);
        load_begin(&trk[1], LOAD_PAT); track_defaults_steps(&trk[1]); load_end(&trk[1]);   /* (no bass: one is added) */
        if (g != 2u) { load_begin(&trk[2], LOAD_PAT); track_defaults_steps(&trk[2]); load_end(&trk[2]); }   /* (nor chords) */
        lt_keep();
        lt_go(GEN[g], 7 + g);
        printf("  %s: %u rows, parts added %02x (form %u)\n", gp.last, chain_config.count, gls.add, gs.g->form);
        ok = live.mode == 1 && chain_config.count >= 6 && song_draft && project_used(0) && project_used(1) && project_used(2) &&
             project_used(3);
        bad += check("FROM LOOP makes a SONG: rows and its four sections (PLAY plays it, SAVE saves it)", ok);
        ok = proj_import(&proj_scratch, &proj_slot[0], sizeof(project_store_t)) && lt_same();
        project_capture(&proj_scratch);
        ok &= lt_same();
        bad += check("the loop's tracks untouched (steps, sound, mix) in section A and in the music after", ok);
        ok = 1;
        for (k = 0; k < 4u; k++) ok &= lt_derived(k);
        bad += check("every section's notes and drum lanes the loop's own (B C D derived from A)", ok);
        ok = lt_in_key(g == 1 ? 90u : 95u, &n) && n >= 1u && !(gls.add & lt_used);
        bad += check("its new parts only on empty tracks, in the heard key", ok);
        if (g == 0) bad += check("RANDOM / EXPLORE aside: the genre the knob chose", gs.gi == 3u);
        else bad += check("RANDOM / EXPLORE: a club genre for a four-on-the-floor loop at 124 BPM", gs.g->form != FM_ARC);
        /* played from the first row to the last and round again */
        press(B_PLAY); events_block(CTL); song_poll(); frame();
        ok = chain.running && chain.row == 0;
        transitions = 0; last = chain.row;
        for (bar = 1; bar < 600u && !(transitions && chain.row == 0); bar++) {
            events_block(chain_bar()); song_poll();
            if (chain.row != last) {
                last = chain.row; transitions++;
                ok &= live.cur == chain.config.row[last].slot;
            }
        }
        bad += check("PLAY plays it from its first row to its last and round again", ok && transitions == chain.config.count);
        seq_stop(); song_poll(); frame();
        project_capture(&proj_scratch);
        bad += check("STOP: the music the loop again (and its added parts)", lt_same() && live.cur == 0);
        {   /* kept: every section packs as SAVE writes it, every row's tracks from a section that is there; then the
             * sections and rows alone (as a load gives them) play the same song */
            static project_store_t keep[4];
            static song_index_t ki;
            chain_config_t kc = chain_config;
            uint32_t mask = 0, hs[CHAIN_ROWS], i;
            for (k = 0; k < 4u; k++) if (song_copy_section(k, 0, "LOOP SONG") > 0) mask |= 1u << k;
            ok = mask == 15u;
            for (r = 0; r < chain_config.count; r++)
                for (k = 0; k < NTRK; k++) ok &= (mask >> row_src(r, k, chain_config.row[r].slot)) & 1u;
            for (r = 0; r < chain_config.count; r++) {
                ok &= !song_stage(chain_config.row[r].slot, r);
                hs[r] = lt_fh(2166136261u, sec_stage.t, sizeof sec_stage.t);
            }
            memcpy(keep, proj_slot, sizeof keep); ki = song_idx;
            lt_clear();
            memcpy(proj_slot, keep, sizeof keep); song_idx = ki; chain_config = kc; live.mode = 1;
            for (r = 0, i = 1; r < chain_config.count; r++) {
                i &= !song_stage(chain_config.row[r].slot, r);
                i &= hs[r] == lt_fh(2166136261u, sec_stage.t, sizeof sec_stage.t);
            }
            bad += check("kept: its sections pack as SAVE writes them; read back, every row plays the same", ok && i);
        }
    }

    /* ---- the counter-melody: the loop's melody again (an arp's genre: DREAM POP), in the key, never more notes */
    lt_clear(); song.g[G_BPM] = 84; lt_drums(16); lt_prog(CMAJ, MEL_C);
    load_begin(&trk[1], LOAD_PAT); track_defaults_steps(&trk[1]); load_end(&trk[1]);
    load_begin(&trk[2], LOAD_PAT); track_defaults_steps(&trk[2]); load_end(&trk[2]);
    lt_keep();
    lt_go(23, 3);
    {
        uint32_t ar = gs.trk_of[RL_ARP], src = 0, cn = 0, in = 0, i;
        for (i = 0; i < NSTEP; i++) src += trk[3].step[i].time == ST_NOTE;
        ok = ar < NTRK && ((gls.add >> ar) & 1u);
        for (i = 0; ok && i < dlen(&trk[ar]); i++)
            if (trk[ar].step[i].time == ST_NOTE) { cn++; in += gin_mode(trk[ar].step[i].note[0]); }
        printf("  %s: counter-melody on T%u: %u notes (the melody %u), %u in key\n", gp.last, ar + 1, cn, src, in);
        bad += check("a counter-melody of the loop's melody on an empty track: in the key, no more notes than it", ok && cn && in == cn && cn <= src);
        project_capture(&proj_scratch);
        bad += check("  the loop untouched", lt_same());
    }

    /* ---- a seed a song; OCT+ again another from the same loop; UNDO */
    lt_clear(); lt_drums(16); lt_prog(CMAJ, MEL_C);
    load_begin(&trk[1], LOAD_PAT); track_defaults_steps(&trk[1]); load_end(&trk[1]);
    lt_keep();
    lt_go(4, 33); h1 = lt_song();
    lt_clear(); lt_drums(16); lt_prog(CMAJ, MEL_C);
    load_begin(&trk[1], LOAD_PAT); track_defaults_steps(&trk[1]); load_end(&trk[1]);
    lt_go(4, 33); h2 = lt_song();
    bad += check("a seed its song: the same loop and settings, the same song", h1 == h2);
    gp.sig = gen_sig();
    gen_press();                                        /* (OCT+ again: a new seed) */
    project_capture(&proj_scratch);
    ok = gp.seed != 33 && lt_song() != h1 && lt_same() && gls.add && !(gls.add & lt_used);
    bad += check("OCT+ again: another song, the loop kept, its added parts made again (not taken for the loop's)", ok);
    undo_ui();
    project_capture(&proj_scratch);
    ok = lt_same() && !live.mode && !chain_config.count && !gls.u;
    for (k = 0; k < NTRK; k++) if (!((lt_used >> k) & 1u)) ok &= gls_role(&trk[k]) > RL_TEX;
    bad += check("UNDO (GLO + G4): the loop as it was, no song, the added parts gone", ok);

    /* ---- no empty track: arranged only; an empty loop: nothing */
    lt_clear(); lt_drums(16);
    for (k = 1; k < NTRK; k++) { uint8_t nn = (uint8_t)(72 + k); lt_note(&trk[k], 0, &nn, 1, 0); }   /* (no bass, no chords) */
    lt_keep();
    lt_go(4, 5);
    str_cpy(msg, ui.msg2, sizeof msg);
    ok = !gls.add && chain_config.count >= 6 && strstr(msg, "NO EMPTY TRACK") != 0;
    project_capture(&proj_scratch);
    ok &= lt_same();
    bad += check("no empty track: the song arranged from the loop alone, and said so", ok);
    lt_clear();
    lt_go(4, 5);
    bad += check("an empty loop: nothing made, said so", !chain_config.count && !song_draft && strstr(ui.msg, "EMPTY") != 0);
    /* ---- GENE 1's FORM card: MY LOOP whole on it */
    lt_clear(); gp.form = 3; gp.page = 0;
    nav_put(SEC_HOME, "GENE 1"); gp.hot = 3;
    ut_card_bad = ut_card_txt = 0; ui.force = 1; frame();
    {
        char v[16];
        gen_value(0, 3, v);
        bad += check("GENE 1's FORM shows MY LOOP whole on its card", !strcmp(v, "MY LOOP") && ut_card_txt && !ut_card_bad);
    }
    screen_save("build/song-check", "generator-from-loop");
    printf(bad ? "loop song: %d FAILED\n" : "loop song: all ok\n", bad);
    return bad != 0;
}
