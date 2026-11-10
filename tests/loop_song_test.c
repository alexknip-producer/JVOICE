/* SPDX-License-Identifier: GPL-3.0-only */
/* GEN FORM FROM LOOP (gen_loop.c): the loop made a song. The loop's key heard (C major, A minor, a DRUM-X 808 line
 * alone, drums without a tone: the genre's own; an aggressive phrygian loop, a power riff without a third, a dark
 * techno loop: phrygian, minor, never major), its chords made again each seed (every chord of the drop holding the
 * loop's notes there; the heard progression one seed's; many progressions over sixteen seeds, a dark loop's mostly
 * minor), the loop untouched as section A, the verse and the break the genre's (its drums on the loop's kit: other
 * drums for another genre), the new parts on empty tracks in the heard key, the rows a seed's, the song played from
 * its first row to its last and round, kept through a round trip of its sections and rows, UNDO, a seed a song, OCT+
 * again another song from the same loop, no empty track: only arranged. Host UI model (tests/ui_test.c) */
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
/* every note of a loop track in section s is one the loop's track plays; the drums' lanes the loop's (the drop's: A) */
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

/* (MY LOOP 2) the loops of the owner's report: 0 an aggressive E phrygian (a distorted 808 E F E F, a distorted lead
 * on E F G C B), 1 a power riff without a third (E B D, distorted), 2 dark techno (A one-note bass, a m7 stab) */
static void lt_dark(int which)
{
    static const uint8_t B0[8] = {28, 29, 28, 29, 28, 31, 29, 28}, L0[16] = {64, 65, 64, 0, 65, 64, 0, 67, 65, 64, 0, 0, 72, 71, 65, 64};
    static const uint8_t B1[8] = {28, 28, 26, 28, 28, 35, 33, 28}, L1[16] = {64, 0, 64, 71, 0, 74, 71, 0, 64, 0, 76, 74, 0, 71, 69, 71};
    uint32_t i, b;
    uint8_t n;
    lt_clear();
    for (i = 0; i < 64u; i++) {
        if (i % 4u == 0u) lt_hit(&trk[0], i, 1u << DV_KICK);
        if (i % 8u == 4u) lt_hit(&trk[0], i, 1u << (which == 2 ? DV_CLAP : DV_SNARE));
        if (i % 2u) lt_hit(&trk[0], i, 1u << DV_HATC);
    }
    set_engine_of(&trk[1], 0);
    if (which == 2) {
        uint8_t c[3] = {57, 60, 67};
        song.g[G_BPM] = 132;
        for (i = 0; i < 64u; i++) if (i % 4u) { n = i % 16u == 14u ? 36 : 33; lt_note(&trk[1], i, &n, 1, 0); }
        set_engine_of(&trk[2], 0);
        for (b = 0; b < 4u; b++) lt_note(&trk[2], b * 16u + 6u, c, 3, 1);
        return;
    }
    song.g[G_BPM] = 145;
    trk[0].p[P_DIST] = 70; trk[1].p[P_DIST] = 110; trk[3].p[P_DIST] = 100;
    for (i = 0; i < 8u; i++) lt_note(&trk[1], i * 8u, which ? &B1[i] : &B0[i], 1, 3);
    for (b = 0; b < 4u; b++) for (i = 0; i < 16u; i++) if ((which ? L1 : L0)[i]) lt_note(&trk[3], b * 16u + i, &(which ? L1 : L0)[i], 1, 0);
}
/* the drop's plan against the loop: every segment's chord holds the loop's notes there (its tones heard, a note a
 * semitone off them softer than half of them, the tonic's chord its mode's tensions); its chords major (power chords
 * not), all; a hash of its degrees and qualities */
static int lt_fits(uint32_t *nmaj, uint32_t *nall, uint32_t *hash)
{
    uint32_t i, k, l, ok = 1, seg = gs.cst ? gs.cst : 16u, h = 2166136261u;
    for (i = 0; i < gs.pn; i++) {
        uint32_t w[12] = {0}, m = 0, nb, pos = 0, neg = 0, sv;
        const gch_t *c = gchord(i * seg);
        for (k = 0; k < NTRK; k++) {
            const track_t *t = &trk[k];
            uint32_t len = dlen(t), j, st;
            if (!((lt_used >> k) & 1u) || drum_track(t)) continue;
            for (st = i * seg; st < (i + 1u) * seg; st++) {
                const step_t *s = &t->step[st % len];
                uint32_t d = 1;
                if (s->time != ST_NOTE) continue;
                while (d < len && t->step[(st + d) % len].time == ST_TIE) d++;
                for (j = 0; j < s->n; j++) w[s->note[j] % 12u] += d;
            }
        }
        for (k = 0; k < 3u; k++) m |= 1u << (uint32_t)gtone(c, 2 * (int32_t)k, 12) % 12u;
        sv = 1u << (uint32_t)gtone(c, 6, 12) % 12u;
        nb = (m * 2u | m / 2u | m << 11 | m >> 11) & 0xFFFu & ~(m | sv);
        for (l = 0; l < 12u; l++) { if (m >> l & 1u) pos += w[l]; else if (nb >> l & 1u) neg += w[l]; }
        ok &= pos > 0 && (c->deg == 0 || 2u * neg <= pos);
        if (getenv("LT_V") && !(pos > 0 && (c->deg == 0 || 2u * neg <= pos))) printf("   segment %u deg %u: tones %u, a semitone off %u\n", i, c->deg, pos, neg);
    }
    for (i = 0; i < gs.nch; i++) {
        h = (h ^ (gs.ch[i].deg * 16u + gs.ch[i].q)) * 16777619u;
        ++*nall; *nmaj += gs.ch[i].q != Q_POWER && giv(&gs.ch[i], 2) == 4;
    }
    *hash = h;
    return (int)ok;
}
static int lt_distinct(const uint32_t *h, int n) { int i, j, d = 0; for (i = 0; i < n; i++) { for (j = 0; j < i && h[j] != h[i]; j++) {} d += j == i; } return d; }
/* section s's drum track steps (a hash) */
static uint32_t lt_drums_of(uint32_t s)
{
    if (!proj_import(&proj_scratch, &proj_slot[s], sizeof(project_store_t))) return 0;
    return lt_fh(2166136261u, proj_scratch.t[0].step, sizeof proj_scratch.t[0].step);
}
static uint32_t lt_rows(void)
{
    uint32_t h = 2166136261u, r;
    for (r = 0; r < chain_config.count; r++) h = (h ^ (chain_config.row[r].slot * 64u + chain_config.row[r].bars)) * 16777619u;
    return h;
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

    /* Retained DRUM-X lane hits on a melodic engine have no pitches to hear. */
    lt_clear(); lt_drums(16); set_engine_of(&trk[0], 0);
    lt_go(4, 21);
    bad += check("DRUM-X -> ANALOG retained hits: MY LOOP returns without inventing pitches",
                 !gls.heard && chain_config.count >= 6);

    /* Hidden tracks must not make an empty four-track loop audible. */
    for (k = 4; k < NTRK; k++) {
        uint8_t nn = 28;
        lt_clear(); set_engine_of(&trk[k], 0); lt_note(&trk[k], 0, &nn, 1, 0);
        ui_rec_prefs &= ~(PREF_SIX >> 8); rp_apply();
        lt_go(4, 5);
        bad += check("TRACKS 4 ignores a note only on hidden T5/T6",
                     trk_live == 4 && !gls_any() && !chain_config.count && !song_draft && strstr(ui.msg, "EMPTY"));
    }
    {
        uint32_t audible, sig;
        uint8_t nn = 28;
        lt_clear(); lt_drums(16);
        ui_rec_prefs &= ~(PREF_SIX >> 8); rp_apply();
        lt_go(4, 21); audible = lt_fh(lt_rows(), gs.pdeg, sizeof gs.pdeg) ^ gs.len ^ (gs.root << 8) ^ (gs.scale << 16);
        lt_clear(); lt_drums(16);
        for (k = 4; k < NTRK; k++) {
            set_engine_of(&trk[k], 0); lt_note(&trk[k], 0, &nn, 1, 63);
            trk[k].p[P_DIST] = 127;
        }
        ui_rec_prefs &= ~(PREF_SIX >> 8); rp_apply();
        sig = gls_hash(); trk[4].step[0].note[0]++;
        bad += check("MY LOOP fingerprint ignores hidden notes", sig == gls_hash());
        lt_go(4, 21);
        bad += check("hidden bass, length and distortion do not alter audible harmony or arrangement",
                     audible == (lt_fh(lt_rows(), gs.pdeg, sizeof gs.pdeg) ^ gs.len ^ (gs.root << 8) ^ (gs.scale << 16)) && !(gls.add & 0x30u));
        sig = gls_hash(); ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
        bad += check("changing active-track count invalidates the MY LOOP fingerprint", sig != gls_hash());
    }

    /* ---- the key and the chords heard */
    for (r = 0; r < 2u; r++) {   /* (a seed a progression: the heard one some seed's, every one holding the loop's notes) */
        static const uint8_t HEARD[2][4] = {{0, 4, 5, 3}, {0, 5, 2, 6}};
        uint32_t seen = 0, fit = 1, key = 1, hs[16], nmj = 0, nal = 0;
        for (k = 0; k < 16u; k++) {
            lt_clear(); lt_drums(16); lt_prog(r ? AMIN : CMAJ, r ? MEL_A : MEL_C); lt_keep();
            lt_go(4, 21 + k * 13u);                     /* (HOUSE) */
            key &= gls.heard && gs.root == (r ? 9 : 0) && gs.scale == (r ? SC_MIN : SC_MAJ) && gs.pn == 4;
            seen |= !memcmp(gs.pdeg, HEARD[r], 4);
            fit &= lt_fits(&nmj, &nal, &hs[k]);
        }
        printf("  heard: root %d scale %d; %d progressions of 16 seeds\n", gs.root, gs.scale, lt_distinct(hs, 16));
        bad += check(r ? "FROM LOOP hears A minor; its i VI III VII one seed's, every drop's chords holding the loop's notes" :
                         "FROM LOOP hears C major (a chord a bar); its I V vi IV one seed's, every drop's chords holding the loop's notes",
                     key && seen && fit && lt_distinct(hs, 16) >= 4);
    }
    {   /* (MY LOOP 2, the owner: "always the same major chords, the aggression gone") dark loops stay dark */
        static const char *const W[3] = {"an aggressive E phrygian loop (its 808 and lead distorted, the b2): E PHRYGIAN",
                                         "a power riff without a third (distorted): E MINOR, not major",
                                         "a dark techno loop (one note, a m7 stab): A MINOR"};
        static const int16_t ROOT[3] = {4, 4, 9}, SC[3] = {SC_PHRY, SC_MIN, SC_MIN};
        static const unsigned GN[4] = {9, 3, 2, GEN_EXPLORE};   /* REGGAETON HARD BASS HARD TECHNO EXPLORE */
        for (r = 0; r < 3u; r++) {
            uint32_t key = 1, fit = 1, nmj = 0, nal = 0, hs[16], hr[16], g;
            for (g = 0; g < 4u; g++)
                for (k = 0; k < 4u; k++) {
                    uint32_t i = g * 4u + k;
                    lt_dark((int)r); lt_keep();
                    lt_go(GN[g], 5 + i * 31u);
                    key &= gls.heard && gs.root == ROOT[r] && gs.scale == SC[r];
                    fit &= lt_fits(&nmj, &nal, &hs[i]);
                    hr[i] = lt_rows() ^ lt_song();
                }
            printf("  %s: root %d scale %d, %d drop progressions and %d songs of 16, %u%% of the drops' chords major\n",
                   W[r], gs.root, gs.scale, lt_distinct(hs, 16), lt_distinct(hr, 16), nal ? nmj * 100u / nal : 0u);
            bad += check(W[r], key);
            bad += check("  every drop chord holds the loop's notes; many progressions, every song another; mostly not major",
                         fit && lt_distinct(hs, 16) >= (r == 2u ? 3 : 5) && lt_distinct(hr, 16) == 16 && nmj * 100u <= nal * 35u);
        }
    }
    {   /* (MY LOOP 2: "the genre does nothing") the genre's DNA in the verse: its drums on the loop's kit, another for
         * another genre; its progression; EXPLORE a genre a seed (near the loop's tempo) */
        uint32_t dA, dR, dH, gi[8], nv = 0;
        lt_dark(0); lt_keep(); lt_go(9, 77); dA = lt_drums_of(0); dR = lt_drums_of(1); nv += gls.v.n != 0;
        lt_dark(0); lt_go(3, 77); dH = lt_drums_of(1); nv += gls.v.n != 0;
        printf("  verse drums: loop %08x REGGAETON %08x HARD BASS %08x\n", dA, dR, dH);
        bad += check("the verse's drums the genre's (not the loop's; REGGAETON's not HARD BASS's), its progression the genre's",
                     dA && dR && dH && dR != dA && dH != dA && dR != dH && nv == 2u);
        for (k = 0; k < 8u; k++) { lt_dark(0); lt_go(GEN_EXPLORE, 100 + k); gi[k] = gs.gi; }
        ok = 1;
        for (k = 0; k < 8u; k++) ok &= song.g[G_BPM] + 24 >= GENRE[gi[k]].bpm_lo && song.g[G_BPM] <= GENRE[gi[k]].bpm_hi + 24;
        printf("  EXPLORE: %d genres of 8 seeds\n", lt_distinct(gi, 8));
        bad += check("EXPLORE: a genre a seed, near the loop's tempo", ok && lt_distinct(gi, 8) >= 4);
    }
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
        bad += check("the drop's notes and drum lanes the loop's own (A as it was)", lt_derived(0));
        ok = lt_in_key(g == 1 ? 90u : 95u, &n) && n >= 1u && !(gls.add & lt_used);
        bad += check("its new parts only on empty tracks, in the heard key", ok);
        if (g == 0) bad += check("RANDOM / EXPLORE aside: the genre the knob chose", gs.gi == 3u);
        else if (g == 1) bad += check("RANDOM: a club genre for a four-on-the-floor loop at 124 BPM", gs.g->form != FM_ARC);
        else bad += check("EXPLORE: a genre near the loop's tempo", song.g[G_BPM] + 24 >= gs.g->bpm_lo && song.g[G_BPM] <= gs.g->bpm_hi + 24);
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
