/* SPDX-License-Identifier: GPL-3.0-only */
/* REC, PLAY, keys, STOP, PLAY: the take plays back (a YouTube user on V10.7.6: "I pressed record then play and entered
 * a few notes, pressed stop and pressed play to listen and it played nothing; if I then press record it plays notes I
 * did not enter"). A GEN song leaves PLAY in SONG mode (gen.c: live.mode, kept in the song index over a power cycle):
 * PLAY played the song, which does not record (song_play.c chain_start), and REC's light went out; STOP brought the
 * music before PLAY back, PLAY played the song again. Now a track armed plays (and records) the loop; NEW PROJECT
 * (SAVE > TOOLS, system_init) is LOOP again (it left SONG mode with no rows: PLAY did nothing, "ADD A SONG ROW").
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src tests/rec_play_test.c -lm -o build/rec_play_test */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t rp_heard;                          /* audio blocks with a voice of the selected track gated */
static void rp_run(uint32_t ms)                    /* the audio and the main loop together, 16 ms a frame */
{
    uint32_t i, n;
    for (i = 0; i < ms / 16u; i++) {
        for (n = 0; n < 16u * FS / 1000u; n += CTL) {
            events_block(CTL);
            rp_heard += gated_notes(&trk[song.sel]) != 0;
        }
        song_poll();
        frame();
    }
}
static uint32_t rp_notes(uint32_t t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NSTEP; i++) n += trk[t].step[i].time == ST_NOTE;
    return n;
}
static void rp_power_on(void)                      /* the device's power-on parts (ui_test.c ui_power_on_099) */
{
    ui_power_on();
    load_depth++;
    fm6_init();
    power_on_parts();
    load_depth--;
    track_select(1);                               /* (T2 ANALOG: a melodic track) */
}
static void rp_gen(void)                           /* a GEN song (SONG mode, its rows), stopped */
{
    gp.genre = 3; gp.seed = 7; gp.energy = 60; gp.form = 2;
    gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
    for (unsigned k = 0; k < GP_N; ++k) gp.amt[k] = GA_AUTO;
    gen_go(); rp_run(500);
    if (song.playing) { press(B_PLAY); rp_run(500); }
}
/* REC, PLAY, four keys, STOP, PLAY (no key): 1 when the take was recorded into the loop and is heard back */
static int rp_take(void)
{
    uint32_t k, n0 = rp_notes(song.sel), n1;
    int ok;
    press(B_REC); rp_run(300);
    press(B_PLAY); rp_run(600);
    ok = song.playing && !chain.running && (song.rec >> song.sel & 1u);
    for (k = 0; k < 4u; k++) { key_down(white(k)); rp_run(160); key_up(white(k)); rp_run(240); }
    press(B_PLAY); rp_run(300);
    n1 = rp_notes(song.sel);
    press(B_PLAY); rp_run(100); rp_heard = 0; rp_run(2400);
    ok &= song.playing && !chain.running && n1 >= n0 + 3u && rp_notes(song.sel) == n1 && rp_heard > 0u;
    press(B_PLAY); rp_run(300);
    return ok;
}

int main(void)
{
    int bad = 0;
    uint32_t n;
    rp_power_on();
    bad += check("power-on: REC PLAY keys STOP PLAY plays the take back", rp_take());

    rp_power_on(); undo_mark(); system_init();
    track_select(1);
    bad += check("NEW PROJECT: the same", !live.mode && rp_take());

    rp_power_on(); rp_gen(); undo_mark(); system_init();
    track_select(1);
    bad += check("a GEN song, then NEW PROJECT: PLAY the loop again (was SONG with no rows: nothing played)",
                 !live.mode && rp_take());

    rp_power_on(); rp_gen(); track_select(1);
    bad += check("after a GEN song (SONG mode): a track armed, PLAY records the loop (was: the song, not recorded)",
                 live.mode && chain_config.count && rp_take());
    n = rp_notes(1);
    press(B_REC); rp_run(300);
    press(B_PLAY); rp_run(600);
    bad += check("  REC off: PLAY plays the song again", !song.rec && chain.running && song.playing);
    press(B_PLAY); rp_run(600);
    bad += check("  STOP after the song: the take is back (the music before PLAY)", !song.playing && rp_notes(1) == n);

    {   /* the song SAVEd (SAVE SONG AS: its PLAY mode with its rows), a power cycle: flash keeps the sections and the
         * song index's rows (song_index_boot) */
        static project_store_t slots[4];
        chain_config_t cfg = chain_config;
        memcpy(slots, proj_slot, sizeof slots);
        rp_power_on();
        memcpy(proj_slot, slots, sizeof slots);
        chain_config = cfg;
        chain_config.rsv[2] = SONG_MODE_TAG | 1u;
        song_mode_restore();
    }
    bad += check("power-on after a saved GEN song (SONG mode kept): PLAY records the loop", live.mode && rp_take());
    rp_power_on(); rp_gen(); track_select(1); n = rp_notes(1);
    {   /* SAVE > TIMELINE with the song: PLAY loops its row, STOP brings the recorded loop back */
        uint32_t i;
        for (i = 0; i < NPAGES && !SONG_TRK(&PAGES[i]); i++) {}
        ui.home = 0; ui.page = (uint8_t)i; page_entered(); frame();
        press(B_REC); rp_run(300);
        bad += check("  TIMELINE: REC asks for SEQ, nothing armed", msg_is("[SEQ] TO RECORD") && !song.rec);
        press(B_PLAY); rp_run(1200);
        bad += check("  TIMELINE: PLAY loops its row", chain.running && song.playing);
        press(B_PLAY); rp_run(300);
        bad += check("  TIMELINE: PLAY again, the song goes on", chain.running && !chain.hold && msg_is("THE SONG GOES ON"));
        press(B_PLAY); rp_run(600);
        bad += check("  TIMELINE: STOP, the take is back", !song.playing && rp_notes(1) == n);
    }
    return bad != 0;
}
