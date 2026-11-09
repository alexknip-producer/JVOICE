/* SPDX-License-Identifier: GPL-3.0-only */
/* The master's EQ and TAPE (params.c eq_v tape_v) through the save flows (Discord: "saving a project resets the
 * master EQ; sometimes it won't remember it on loading"). */
static void eq_set(int a, int b, int c, int d) { eq_v[0] = (int16_t)a; eq_v[1] = (int16_t)b; eq_v[2] = (int16_t)c; eq_v[3] = (int16_t)d; }
static int eq_is(int a, int b, int c, int d) { return eq_v[0] == a && eq_v[1] == b && eq_v[2] == c && eq_v[3] == d; }
static int eq_save_test(void)
{
    int bad = 0, ok;
    /* a song of four sections stored flat, the music on C, then the EQ set (and TAPE) */
    song_fixture();
    eq_set(0, 0, 0, 0); tape_v[0] = tape_v[1] = 0;
    for (uint32_t s = 0; s < 4; s++) section_store(s);
    section_load(2);
    eq_set(5, -3, 0, 12); tape_v[0] = 40; tape_v[1] = 7;
    ok = !song_save_as(song_cur, "EQ SONG");
    bad += check("EQ: SAVE SONG AS over the open project: the live EQ and TAPE kept", ok && eq_is(5, -3, 0, 12) && tape_v[0] == 40 && tape_v[1] == 7);
    ok = !song_open(1) && eq_is(5, -3, 0, 12);          /* SAVE > PROJECT's KNOB 1: the music stays */
    eq_set(0, 0, 0, 0); tape_v[0] = tape_v[1] = 0;
    ok &= !song_select(0);
    bad += check("EQ: the saved project loaded again (section A): its EQ and TAPE", ok && eq_is(5, -3, 0, 12) && tape_v[0] == 40 && tape_v[1] == 7);
    ok = 1;
    for (uint32_t s = 0; s < 4; s++) {
        eq_set(0, 0, 0, 0);
        ok &= !section_load(s) && eq_is(5, -3, 0, 12);
    }
    bad += check("EQ: every section of it holds the master's EQ (a section change keeps it)", ok);
    ok = !song_open(2) && eq_is(5, -3, 0, 12);          /* another (empty) project picked: the music, its EQ, stays */
    ok &= !song_select(0) && eq_is(5, -3, 0, 12);
    bad += check("EQ: another project picked, then this one loaded: the EQ as saved", ok);

    /* a section saved alone (SAVE > PROJECT's slot) and loaded; UNDO; NEW PROJECT */
    eq_set(-2, 4, -6, 8);
    ok = !project_save_as(1, "B") && eq_is(-2, 4, -6, 8);
    eq_set(1, 1, 1, 1);
    undo_mark();
    project_load(1);
    ok &= eq_is(-2, 4, -6, 8);
    ok &= !undo_do() && eq_is(1, 1, 1, 1);              /* UNDO: the EQ before the load .. */
    ok &= !undo_do() && eq_is(-2, 4, -6, 8);            /* .. and again: the loaded one */
    bad += check("EQ: a section saved and loaded; UNDO over the load and back", ok);
    system_init();
    bad += check("EQ: NEW PROJECT (INIT SYSTEM): flat, TAPE out", eq_is(0, 0, 0, 0) && !tape_v[0] && !tape_v[1]);

    /* RESTORE LAST: an EQ / TAPE change alone is music the autosave keeps (autosave_sig) */
    reset();
    ui_prefs |= PREF_RESTORE_ON;
    as_power_on();
    eq_set(0, 0, 0, 0); tape_v[0] = tape_v[1] = 0;
    autosave_boot(1);
    as_edit(0);
    as_run(20000u);
    {
        uint32_t w = as.writes;
        eq_set(-4, 2, 6, -12);
        as_run(80000u);
        ok = as.writes == w + 1u;
        w = as.writes;
        tape_v[1] = 9;
        as_run(80000u);
        ok &= as.writes == w + 1u;
    }
    bad += check("EQ: an EQ change alone autosaved, a TAPE change alone too", ok);
    as_power_on();
    eq_set(0, 0, 0, 0); tape_v[0] = tape_v[1] = 0;
    ok = autosave_boot(1) == 1 && eq_is(-4, 2, 6, -12) && tape_v[1] == 9;
    bad += check("EQ: power-on (RESTORE LAST): the EQ and TAPE as left", ok);
    eq_set(0, 0, 0, 0); tape_v[0] = tape_v[1] = 0;
    ui_prefs &= ~PREF_RESTORE_ON;
    return bad;
}
