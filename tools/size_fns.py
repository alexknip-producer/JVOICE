#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build the main-loop code for size (build.py): the functions defined in SIZE_FILES get LLVM's
minsize (about -Oz) while everything else (the sound, render and ISR code, the flash and OTA code,
the main loop) keeps -Os. The JieLi clang 4 has no '#pragma clang attribute' and its minsize
attribute is an error on variables, so build.py compiles felucca.c to LLVM IR without the
optimizer, this script adds 'minsize' to those definitions, and the IR is then compiled at -Os
(without the edit, that round trip is byte-identical to compiling felucca.c directly).
  size_fns.py IN.ll OUT.ll"""
import re
import sys
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
SRC = _ROOT / "firmware" / "src" if (_ROOT / "firmware" / "src").is_dir() else _ROOT / "src"   # either layout
# the UI, the stores, the editor and the console: main loop only, never called by sound code; and (1.1) every
# other file without DSP: USB, MIDI, the sequencer, the drawing, the LCD, OTA, ... Of those, what an interrupt
# reaches (ISR_ROOTS and everything they call or name, from the IR) keeps -Os (isr_reach)
SIZE_FILES = ["ui.c", "favorites.c", "menu_items.c", "icons.c", "ui_graph.c", "ui_draw.c", "ui_menu.c", "ui_input.c",
              "storage.c", "upreset.c", "project.c", "settings_persist.c",
              "editor.c", "editor_preferences.c", "editor_backup.c", "editor_menu.c", "console.c",
              "usb.c", "midi_uart.c", "midi_control.c", "midi_clock.c", "seq.c", "song_chain.c", "motion.c",
              "panel.c", "lcd.c", "gfx.c", "ota.c", "ota_hw.c", "up_fm6.c", "params.c", "editor_fm6.c",
              "ui_layer.c", "ui_name.c", "ui_events.c", "song_main.c", "main.c", "libc.c", "storage_hw.c", "engines.c", "fm4_convert.c",
              # (0.9.0) the files the ones above include, missed until now (-16.7 KB): GEN, DICE, the lenses, the line
              # art, the beings, the tech graphs, the tools, the FM6 bank and VOICE's words (its sound side, reached
              # from the audio ISR through ENG[]'s hooks, keeps -Os: isr_reach follows globals)
              "gen.c", "gen_tables.c", "song_dice.c", "ui_lens.c", "ui_organic.c", "ui_fm6op.c", "ui_being.c",
              "ui_engvis.c", "ui_tech.c", "ui_lzsave.c", "ui_lzfx.c", "ui_lzhome.c", "ui_lzedit.c", "ui_lzpage.c", "ui_dx.c",
              "ui_tools.c", "fm6_vbank.c", "sing.c",
              # (0.9.7) the main-loop functions of the sound files (what the interrupts reach stays -Os, as above): the
              # MASTER pot's boot and poll, FM6's patch store, the kit's names and grids, MOD's names, the PUNCH lane
              "pfx.c", "master.c", "audio.c", "mod.c", "fx.c", "drumx_voice.c", "eng_drum.c", "eng_fm6.c",
              # (V10.29) the files added since, and the other engines' main-loop side (as 0.9.7's: what the
              # interrupts reach keeps -Os): EUCLID, SCENE, LIVE, MIDI LEARN, the song player, PERFORM, PLAYHEADS
              "ui_euclid.c", "ui_scene.c", "ui_live.c", "seq_live.c", "midi_learn.c", "song_play.c", "perform.c",
              "fugue.c", "slicer.c", "voice.c", "fm6_core.c", "eng_add.c", "eng_analog.c", "eng_animal.c",
              "eng_digital.c", "eng_formant.c", "eng_lofi.c", "eng_noise.c", "eng_phase.c", "eng_string.c",
              "eng_wheel.c", "bt.c", "click.c",
              # Whole-song SAVE is main-loop storage work, added after this list.
              # As above, anything reached from an ISR remains at -Os.
              "song_save.c", "eng_input.c", "usb_host.c", "gen_track.c",
              # (V10.6z6) the velocity diagnostic and the keys' velocity: their main-loop side (the scan's, -Os)
              "vel_diag.c", "vel.c"]
ISR_ROOTS = ("fm1_alnk0_irq", "fm1_timer5_irq")      # hal/fm1_isr.S: the audio ISR, the key / LED scan (+ USB poll)
DEF = re.compile(r"^(?:static|void|int|uint\w*|int\w*|const)\b[^;=(]*?\b([A-Za-z_]\w*)\s*\(", re.M)
IR_DEF = re.compile(r"^(define [^\n]*?@\"?([\w.]+)\"?\([^\n]*\)(?: unnamed_addr| local_unnamed_addr)?)( #\d+[^\n]*\{)$",
                    re.M)


def definitions(text):
    """names of the functions text defines (a signature at column 0 whose ')' is followed by '{')"""
    names = []
    text = re.sub(r"/\*.*?\*/|//[^\n]*", " ", text, flags=re.S)        # comments (none hold code)
    text = re.sub(r"\b__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\)", " ", text)   # (0.9.0) before the name too
    for m in DEF.finditer(text):
        i, depth = m.end() - 1, 0
        while i < len(text):
            depth += {"(": 1, ")": -1}.get(text[i], 0)
            i += 1
            if depth == 0:
                break
        rest = re.sub(r"^(__attribute__\s*\(\(.*?\)\)\s*)+", "", text[i:i + 200].lstrip())
        if rest.startswith("{"):
            names.append(m.group(1))
    return names


def size_names():
    names = set()
    for f in SIZE_FILES:
        names.update(definitions((SRC / f).read_text()))
    return names


IR_BODY = re.compile(r"^define [^\n]*?@\"?([\w.]+)\"?\((.*?)^\}", re.M | re.S)
IR_REF = re.compile(r"@\"?([\w.]+)\"?")
IR_GLOBAL = re.compile(r"^@\"?([\w.]+)\"? = [^\n]*", re.M)


def isr_reach(ir, with_globals=False):
    """the functions an interrupt can run: ISR_ROOTS and, transitively, every function their bodies call or
    name (a pointer taken there counts as called), from the unoptimised IR (before inlining: a superset).
    (0.9.0) A global they name counts as its initialiser: a function in a table they read (an engine's hooks:
    ENG[].block -> formant_block -> sing_block) is reached too. with_globals: the globals reached as well (the
    constant tables the sound reads: tools/build.py's flash layout)"""
    body = {m.group(1): m.group(2) for m in IR_BODY.finditer(ir)}
    fns = set(body)
    for m in IR_GLOBAL.finditer(ir):
        body.setdefault(m.group(1), m.group(0))
    seen, todo = set(), [r for r in ISR_ROOTS if r in body]
    while todo:
        f = todo.pop()
        if f in seen:
            continue
        seen.add(f)
        todo += [g for g in IR_REF.findall(body[f]) if g in body and g not in seen]
    return seen if with_globals else {f for f in seen if f in fns}


def main(src, dst):
    ir = Path(src).read_text()
    isr = isr_reach(ir)
    # Sparse MIDI control dispatch runs per received message, never per rendered sample.
    # Keep synthesis, clocks, DMA, note release and ownership queries at -Os.
    input_size = {"midi_event", "midi_control", "midi_cc_map", "midi_cc_set", "midi_learned", "midi_route_ch14",
                  # (V10.6z4) live recording: once a note, a release or a step a held note reaches (seq.c), never a sample
                  "rec_note_at", "rec_end", "rec_drum", "rec_hold", "rec_len", "rec_repeats", "cin_keep", "dxo_rec_hold",
                  # .. and what runs once a transport start / stop, a section or song row, a chord or scene key, a
                  # punch-in press, an automation record (the UI's calls, the ISR's few)
                  "chord_build_mode", "scene_key", "scn_notes", "seq_start", "seq_stop", "motion_capture", "motion_put",
                  "motion_djf_set", "sec_apply", "chain_apply", "midi_nrpn",
                  "perf_press", "perf_switch", "cin_record", "cin_block", "latch_release", "scene_capture",
                  "srec_finish", "srec_add",
                  # .. and a track's step (its components, its automation): once a step, a pass
                  "motion_step", "motion_unlock", "motion_restore", "seq_step",
                  "arp_pick", "dxp_fire", "pfx_fire", "fg_play", "midi_clock_pulse", "rev_state_clear", "mo_knobs",
                  "mo_flush_of"}
    cold_ui = {"mod_dst_ok", "mod_dst_offer_bt", "mod_dst_name_bt", "gen_value", "gen_cards_bt", "gen_panel_bt",
               "song_remove_parts", "song_copy_section",
               "proj_unpack_bt", "about_load", "bt_prox_panel", "bt_prox_input",
               "fm4_abs", "fm4_lg", "fm4_ms", "fm4_samples", "fm4_rate", "fm4_fall", "fm4_level", "fm4_outlevel", "fm4_convert_bt",
               "crash_log_put", "cpu_status", "cpu_put"}   # (V10.6z4: a crash's record, the CPU menu)
    # (V10.6z5) more code an interrupt reaches but runs once an event, never once a sample: a note's start and end
    # (the engines' note_on hooks, the voice's allocation, the words VOICE sings: once a word), MIDI's and USB's
    # configuration and messages, the arpeggiator's and the keys' chords, the descriptors (UI text the ENG[] tables
    # name), a crash's report, the USB host's descriptor parse. Chosen from a callgrind profile of tests/cpu_vg.c
    # (built -O1 -fno-inline: every function's own instructions): together they are < 0.1 % of the audio path's
    # instructions there; none is in HOT_FNS or called from RAM code (build.py's RAM code size is unchanged)
    event_size = {
                  "add_note_on", "ain_note_on", "analog_desc", "analog_from_chip", "analog_from_trio",
                  "analog_note_on", "animal_desc", "animal_init", "animal_note_on", "arp_chord", "arp_chord_off",
                  "arp_forget", "arp_off", "arp_owner", "arp_silence", "bt_hw_seg", "chord_auto", "chord_pick",
                  "cin_begin", "cp_order", "crash_code", "delay_zero", "drum_lane", "drum_note_lane", "drum_note_on",
                  "drum_oct_note", "drum_oct_of", "dx_mute_set", "dxo_hit", "dxo_off", "dxo_on", "dxo_part",
                  "dxp_choke", "e0_send", "eng_gone", "fg_release", "fg_reset", "fm1_fault_c", "fm6_done",
                  "fm6_note_on", "formant_desc", "formant_note_on", "glide_from_trk", "glide_set", "glide_sung",
                  "hex8", "input_desc", "input_off", "input_on", "kb_lat", "kb_map", "key_next", "key_place",
                  "live_next", "live_swap", "lm_hits", "lm_once", "load_admit", "load_cost_q8", "lofi_desc",
                  "lofi_note_on", "lv_neutral", "ly_at", "ly_pick", "ly_word", "mc_lost", "midi_channel",
                  "midi_clock_transport", "midi_expression", "midi_expression_channel", "midi_forget_track",
                  "midi_local_held", "midi_mapped", "midi_note_event", "midi_note_held", "midi_out_event_at",
                  "midi_pedal_up", "midi_release", "midi_targets", "midi_track", "midi_val_set", "ml_free_cc",
                  "ml_map", "mn_get", "mo_key", "mo_plays", "mo_send", "mod_midi", "mono_pick", "mono_play",
                  "mono_remove", "motion_begin", "motion_desc", "motion_enabled", "motion_set_event", "name_of",
                  "noise_desc", "noise_note_on", "nrpn_g_ok", "param_desc_of", "param_fit", "params_by_count",
                  "perf_buf_done", "perf_buf_start", "perf_glt_seg", "perf_glt_step", "perf_hush_end", "perf_key",
                  "perf_len", "perf_mute", "perf_unhush", "pfx_hush", "pfx_lane_at", "pfx_lane_put", "pfx_rec_ok",
                  "phase_note_on", "rec_note", "rec_on", "rec_release", "rev_line_clear", "room_zero", "scale_mask",
                  "scale_snap", "scale_steps", "seq_note_off", "seq_note_on", "seq_release", "seq_unit",
                  "shed_voice", "shift_step_pass", "sl_enter", "sound_v04", "sp_chain", "srec_stop", "st_micro_set",
                  "step_lm_any", "step_lmode", "step_rolls", "step_set_chance", "step_set_lmode", "step_set_ratchet",
                  "str_cpy", "string_desc", "string_note_on", "sy_en_expand", "sy_enter", "sy_es_tok", "sy_gen_word",
                  "sy_norm", "sy_norm_lang", "sy_phrase_len", "sy_take_entry", "sy_tok", "sy_word", "sy_word_next",
                  "track_desc", "trk_all_off", "trk_chance", "trk_fade_out", "trk_kit", "trk_note_off", "trk_nvoice",
                  "trk_trans", "uac_ep4_reset", "uac_rate_set", "uac_stream", "uh_parse_midi", "uh_parse_uac",
                  "uin_ep_config", "voice_begin", "voice_cap", "voice_off_at", "voice_room", "voice_start_rec",
                  "wheel_note_on",
                  # (V10.6z6) SWARM's block (its voices' values, not their samples: ain_swarm), its melody's step, its
                  # FAR / NEAR values
                  "sw_prep", "sw_step", "sw_sv",
                  # (V10.6z6) the keys' velocity: once a strike, a release, a driven sample of a held key (every
                  # 4.4 ms), a press (V10.7.10: and its wait's bound, a test a tick); the keys' pressure once a block
                  "vel_map", "vel_driven", "vel_lift", "vel_take", "vel_tick", "vel_note_on",
                  # .. and to make room for it, more of the same kind: a voice stolen, a SHIFT pass prepared (once a
                  # pass) and its conditions, a lyric's POS, the PLAYHEADS back in phase, a chord glide's note, USB's
                  # control transfers, a step's parts
                  "voice_victim", "shift_prepare_pass", "shift_cond", "sy_seek", "live_heads", "cg_near", "e0_chunk",
                  "step_parts",
                  # (V10.6z8) a section's change: the sounds the player changed
                  "snd_sig", "snd_own"}
    names, hit = ((size_names() | cold_ui) - isr) | input_size | event_size, set()
    def mark(m):
        if m.group(2) not in names:
            return m.group(0)
        hit.add(m.group(2))
        return m.group(1) + " minsize" + m.group(3)
    Path(dst).write_text(IR_DEF.sub(mark, ir))
    print(f"size: {len(hit)} main-loop functions built for size ({len(names) - len(hit)} not in this build; "
          f"{len(size_names() & isr)} reached from an interrupt kept -Os)")
    return 0


if __name__ == "__main__":
    sys.exit(main(*sys.argv[1:3]))

