#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Host tests of the Felucca sources (no hardware). Run from the repo root after ./build.sh:
#   tests/run_tests.sh
#
# Regression suite (tests/regress.c, tests/target_budget.py; details at the top of regress.c):
#   golden renders  every engine x preset, the GM map on DRUM, voice modes, FX sends, a 4-track mix: one hash
#                   each in tests/golden.txt. A change of the sound fails with the list of renders.
#   health          clipping, DC, peak level, voices free after the release, silence at the end.
#   CPU             instructions / sample per preset and mix (tests/cpu_baseline.txt, +25 %), ns printed;
#                   target: loop instructions of the render functions in the pi32v2 disassembly
#                   (tests/target_budget.txt, +10 %; exact, static).
#   voices          the budget of 8, steal fades, MONO / LEGATO / UNISON keep their note, the VOICE cap,
#                   no hanging notes on any MIDI / key routing.
# UI renders (tests/ui_render.c): every screen in every palette from the real drawing code: the layout lint (no text
#                   off the screen, cut, hidden, overlapping or spilling out of its cell / card; only free text
#                   ellipsised), GREY gray, MONO neutral, the draw cost, the text audit (build/ui_new/text_audit.tsv);
#                   PNGs of GREY MONO GREEN PAPER NIGHT in build/ui_new (tests/ui_render.py), the findings in build/ui_new/report.txt;
#                   FM6's 32 algorithm charts as drawn (no box overlapping, no route through a box or crossing another);
#                   DIGITAL's screens (its algorithm charts, OP ENV) with FELUCCA_FM4=1 too (build/ui_fm4: lint, GREY, MONO);
#                   every frame of the rolling digits (lint, GREY, MONO), their filmstrips in build/ui_slot;
#                   the alignment: every text / icon / keycap meant to be centred, or on its neighbours' line, by its
#                   ink against its box (cells, chips, buttons, rows, knobs, the roll's strip, the keycaps' pills), every
#                   screen in FLAT and LINE and every palette, over every value it can show; 1 px off or more fails
#                   (build/ui_new/align.txt). MENU > LARGE: every screen again (FLAT, LINE, every palette) and the page / value
#                   sweep with the tall cards, the same lint and alignment; sheet_LARGE_GREY.png, sheet_LARGE_MONO.png.
# UI (tests/ui_test.c): the UI sources against stub display / buttons / knobs: sound loads keep the steps and
#                   the track's ARP / SCL / SLICER, the SEQ > PATTERNS loader and its REPLACE? dialog, the
#                   one-step undo of both (SAVE held), REC on TRACKS / SEQ / ARP, STEP and ARP while recording,
#                   MIDI IN ROUT, saves refused while playing and the OVERWRITE? dialog, MUTE on TRACKS KNOB 1,
#                   the DRUM grid (keys, knobs, LEDs, pages, live recording into it, BEAT from PATTERNS).
#                   (0.9.9) The power-on parts (T1 DRUM-X, T2 VOICE, T3 ANIMAL, T4 LOFI, T5 ANALOG, T6 FM6, dry, each
#                   sounding), the engines' display order (place <-> engine round trips; PROGRAM CHANGE in 0.9.8's), a
#                   project of 0.9.8's tracks unchanged; the other UI tests on 0.9.8's tracks (engines.c TRK_OLD).
# Audio / persistence / editor: the real C paths against simulated DMA and NOR flash: bounded overload
#                   fades, shared-voice limits, deferred settings and retries, failed-save rollback, the autosave
#                   (1.2: when it writes, the power-on restore, damaged copies, a write cut short, wear over a session),
#                   malformed transfers, transport-stop timeouts, MIDI and UART recovery; the MENU settings over the
#                   editor (MENU_DESC / MENU_SET: every item, clamping, unknown ids, saving, USB SERIAL applied later).
# LIVE (tests/live_test.c, p2 from main 0.9.11): GLO > LIVE's modifiers (seq_live.c, ui_live.c): their timing, the ghost,
#                   RETURN, BYPASS, COMMIT + UNDO, recording at the ghost, MANO's keys, RIELES' PISTAS, the GLO chain,
#                   the PLAYHEADS under a slower RATE (the FX layer's 1/2 TEMPO and LOOP: perform_test).
# RATCH (tests/ratchet_test.c): a step's ratchet (x1..x4): its parts in the sequencer (equal, gated, chords and drum
#                   hits whole, one chance roll, swing, no slide or tie out, STOP), FUN8 round trip and older projects x1,
#                   user preset patterns, SEQ > CHANCE KNOB 3 and the roll / grid drawing.
# COMPONENTS (tests/comp_test.c, 0.9.9-seq2d): the step components v2 (ACTION LENGTH CONDITION VALUE): the mock-up's
#                   16-bar cycle, every action over its LENGTH, conditions, MONO (C2 ELSE) / POLY, a v2 project round trip
#                   and a project of before seq2d migrated (CP_*, EVERY), SEQ > COMP / COMP 2's knobs.
# CTL64 (tests/ctl64_timing_test.c, 0.9.9): blocks of 64 samples (core.h CTL): every note of a MICRO-shifted pattern lands
#                   in the rendered audio within 2 samples of where the sequencer puts it running a sample at a time
#                   (voice.c track_render starts and releases a voice at its sample inside the block); POLY, MONO cuts, a kit.
# MOD (tests/mod_test.c): the modulation matrix: slots that do nothing are bit-identical, every source on each
#                   kind of destination, clamping, MIDI CC1 / CC11 / aftertouch routing, the cost of 4 active
#                   slots (at most +5 %), demos in build/mod_demo/.
# PERFORM (tests/perform_test.c): the FX hold layer (src/perform.c): 1/16 starts, stereo buffer effects, the
#                   too-long REPEAT, the SLICER interplay, silent layer keys, idle bit-identical, cost; build/perform_demo/;
# REVERB (tests/reverb_test.c): REVERB TYPE (src/fx.c): ROOM bit for bit as before, SPRING's decay against SIZE,
#                   its chirp (group delay rising with frequency), stability at the corners, level, a model change
#                   without a click, its cost against ROOM (+30 % at most); demos in build/fx_demo/.
# DELAY (tests/delay_test.c, 0.8.4): the delay's line (src/fx.c dly_lo): no leak of the input at the longest TIME with MOD
#                   up (plain, PING-PONG, WIDE), every long division its own length, the line at half / a quarter rate past
#                   what it holds at the full one (the echo's timing and level, the response, aliases and images, the noise
#                   floor against the full rate), no click when the rate changes (TIME swept, the tempo, a load, an external
#                   clock wavering at a boundary), MOD / GRAIN / DIGITAL / ANALOG 120 / PING-PONG / TONE at those rates.
#                   sine (builds without the CC0 samples); demos in build/slice_demo/.
# INPUT (tests/input_test.c): the key / button debounce of hal/fm1_input.h against the TIMER5 scan and bouncing
#                   contacts: a press within 2 scans (<= 2.3 ms), one note per bouncy press, no early or hanging
#                   release, stray samples ignored, fast repeats, the encoders' detents; the LED scan: lit LEDs every
#                   frame, dim ones a short pulse (the second line write) every frame, each only on its own column;
#                   the breath (#119): dark .. ~60 % of lit (DIM LO ~30 %), smooth, no dark run over ~10 ms near its peak;
#                   the power-on sweep (1.1, hal/fm1_led_anim.h): its length, every LED every frame at its level, the
#                   head left to right with its tail, the buttons, the end on the idle glow, the tick as before after.
# CLICK (tests/click_test.c, 1.1): the metronome (src/click.c) and the count-in (src/seq.c) through audio.c: each beat at
#                   the first sample of its block, with a 1/16 track's steps 1 5 9 13 (internal and external clock, DIV 1/8
#                   and SWING too), the accent, the frequencies, length, LEVEL, MASTER, OFF / REC / ON, USB audio bit for bit
#                   without it; COUNT-IN 1 / 2 BARS (step 1 exactly N bars after PLAY, STOP cancels, PLAY again nothing,
#                   never with an external clock), notes in its last eighth onto step 1; build/click_demo/.
# USB audio (tests/uac_test.c): the UAC1 descriptors as a host parses them (with and without CDC; 44100 and
#                   48000 Hz), the ring and packetiser: 44.1 frames per packet, every frame in order (bit for bit),
#                   underrun / overrun, restart; SET_CUR; the 44.1 -> 48 kHz resampler (every frame against a
#                   double-precision one and bit for bit 0.8.8's, response, SNR, THD / aliasing spurs, cost); the stream
#                   at 48 kHz (47..49 frames per packet, fast and slow I2S clocks, switching rates mid-stream).
# USB STEMS (tests/stems_usb_test.c, 0.9.8): the 8- and 10-channel stream at 44.1 kHz against the FM-1's audio ISR and a
#                   host that reads it as the JIANT Stems app does (each packet's buffer wMaxPacketSize; a longer packet is
#                   babble: cut, the endpoint halted 1..3 ms): every packet fits, every rendered frame reaches the app's lanes
#                   once, in order, in its own channel (2 min each, +-500 ppm); 0.9.7's 720-byte endpoint shown dropping
#                   blocks as a field take did; starvation's repeats made up with the frame spread over a packet.
# STEMS (tests/stems_test.c, 0.9.8): fx.c's stems on dense DICE songs: each bit for bit its track's solo render (LOAD FULL,
#                   LITE, ECO with its tail trimming; one core and CORES 2), the six and the FX return summing to the mix,
#                   an empty track exact zeros, a track through the SLICER its own sound as it got through.
# JIANT Stems app (android/stems, while it is in this repo): usbstream.c against a pretend usbfs (JDK headers), the :core
#                   JVM tests with gradle (offline: the export's length, the trim, the WAV recovery, the mix).
# USB audio scheduler (tests/uac_sched_test.c, 0.8.9): minutes of the stream against a simulated FM-1 (the audio ISR's
#                   order, renders as a heavy song's with late halves, starvation bursts, IRQs off) and host (48 kHz,
#                   +-500 ppm): every IN token finds a packet of n +- 1 frames, every rendered frame sent once (exact), no
#                   drift, underruns repeated and made up; one core and CORES 2 (core 1 resampling the half).
# web (web/test_web.mjs): the editor protocol against its mock device, whose tables must equal the
#                   firmware's (tests/descdump.c -> build/host/desc.json; the MENU settings: tests/editor_test.c -> build/host/menu.json),
#                   the package builder, the updater.
# Browser emulator (web/emu, when emcc is there): the firmware in WebAssembly (build/emu) boots, plays keys and MIDI,
#                   draws, lights its LEDs, keeps a save across instances, plays a song bit for bit as the same file
#                   built with cc (web/emu/native_check.c); the cost of 1 s of a heavy song against real time.
# DRUM-X OCTAVES (tests/drumx_oct_test.c, 0.8.5): OCT 0 the kit as before (bit for bit); OCT +1 / +2 / +3 ratchets x2 / x4 /
#                   x8 in time while held (stopped and playing), -1 / -2 / -3 SHORT / GHOST / FLAM (length, level, tone,
#                   the grace hit measured); real-time REC: the step's RATCH / ECHO x8 EVEN / TOUCH, the pass after the
#                   very hits heard; MIDI's bands around 35..81, MIDI REC, a project round trip, the OCT message, COMP.
# FILTER (tests/filter_test.c, 0.9.9-snd1): the part's FILTER (src/voice.c tf_run, track_comb): every TYPE bounded at RES
#                   127 from C1 to C8 (3 engines), COMB / COMB- in tune with the note, no zipper sweeping CUT, RES the
#                   ring's length, MS20 screaming under full scale; demos in build/filter_demo/.
# DRUM-X (tests/drumx_test.c): the DRUM engine (src/drumx_voice.c): every lane sounds, stays under full scale and ends,
#                   MORPH A <-> B (also while a hit rings), PITCH, the hat choke, MRPH on the knob; demos in build/drumx_demo/.
# NOISE (tests/noise_test.c): the engine (src/eng_noise.c): COLR's slope (white, pink, brown), the filter and the
#                   register clock following the key, META periodic at the key, no DC, no clipping at the
#                   corners, a note from silence the same twice, the cost per voice; demos in build/noise_demo/.
# ANIMAL (tests/animal_test.c, 0.8.3): the engine (src/eng_animal.c): CMOS's relaxation oscillator on the key, the owner's
#                   video (chirps at 1.0 .. 1.3 kHz with rising hooks, 2.4 .. 4.5 a second; a trill of 8 .. 10), the cricket's
#                   carrier and chirp rate, TEMP (Dolbear), the Hopf whistle's onset by bifurcation and its pitch, every
#                   preset sounding, no DC, ending, the same twice, no clipping at any mode's corners; demos in build/animal_demo/.
# STRING / ADD (tests/string_add_test.c, 0.9.9-snd5): the Karplus-Strong string (src/eng_string.c) in tune over C1 .. C6 for every
#                   EXCITE (the decimated loops too), DECAY's T60 rising and even over the keys, BRIGHT, a COMB TYPE turned BP;
#                   the additive engine (src/eng_add.c): every MODE's partials at their ratios, BRIGHT's tilt, EVEN, DAMP, no
#                   alias at the top; every preset of both sounding, no DC, the same twice, ending; the knobs' corners; demos.
# VOICE words (tests/voice_test.c, 0.8.5): the VOICE engine singing (src/sing.c): Spanish by rules and Japanese romaji on word
#                   lists (phonemes, syllables), English through its dictionary, every bank word's syllables; the lyric
#                   generator (the same lyric per seed, phrases as long as the pattern's notes, bank words); the vowels' F1 / F2,
#                   s / sh / f bands, a plosive's closure and burst, no click at syllable changes, no DC, no clipping at the
#                   corners; notes, held notes, "-", POS, LOOP with the sequencer; LANG OFF bit for bit; demos in build/voice_demo/.
# DIGITAL -> FM6 (tests/fm4_test.c, built with FELUCCA_FM4=1): the retired four-operator engine against its conversion
#                   (src/fm4_convert.c): routes and carriers per algorithm, the presets' PTCH, and the sound (pitch,
#                   centroid, RMS envelope) of its presets and algorithms; demos in build/fm4_demo/. tests/digital_test.c
#                   (FELUCCA_FM4=1 too): DIGITAL's operator envelopes. Default builds have no DIGITAL (engine 1 reserved).
#                   tests/fm4_div0_test.c (UBSan, #61): the conversion without a divide that can be 0 gives the values the
#                   guarded form gave (every INDEX x FLT ENV, random sounds); a 0.9 project with DIGITAL ORGAN converts.
# FM6 (tests/fm6_test.c): the 6-operator FM engine (src/eng_fm6.c, src/fm6_core.c): the 32 algorithms' carriers, the
#                   operator envelopes (stages, rates, the voice ending), bit-stable notes, a click-free retrigger, no DC /
#                   clipping over the factory patches, the macros' directions, PTCH, pack / unpack and the SysEx
#                   layouts, the 6-voice cap, the cost per voice; demos in build/fm6_demo/.
# FM6 BANK (tests/fm6bank_test.c, JIANT 0.6.11 from Felucca 1.4.1): the voice bank (src/fm6_vbank.c) and its editor
#                   commands against simulated NOR: write / read back, its sectors only, a power cut at any byte or erase
#                   (the old bank or the new one), a wrong CRC, SLOT B1..B32, the SLOT values stored before as before.
# TAPE (tests/tape_test.c, 0.9.9-snd8): the master's TAPE (src/fx.c tape_block, FX > TAPE): off never runs; TAPE's harmonics,
#                   the highs lost, the head bump; WOW's few cents; no click turning them; kept in a project ('T' 'P').
# DJ FILTER (tests/djf_test.c, 0.9.1): the master DJ FILTER (src/fx.c djf_block, GLO > MIXER KNOB 4): centred, a sequenced
#                   mix bit for bit as 0.9.0 rendered it (never run); the low-pass / high-pass responses (pass band, 12 dB an
#                   octave, the peak, the ends, stereo); no click crossing the middle, jumping sides, swept, automated; its
#                   automation recorded, played, stopped, saved apart ('M' 'G') and loaded; an old project loads it centred.
# MICRO (tests/micro_test.c, 0.9.1): a step's MICRO in 64ths (core.h ST_MICRO): every value, a project of before played
#                   sample for sample and saved byte for byte as 0.9.0 did; live recording at REC Q 0 % within 1/64 of a step
#                   of where played (72 / 120 / 174 BPM, 1/8 1/16 1/32 16T, swing) and played back there; REC Q 50 % half way;
#                   the block 'M' 'F' saved and loaded, without it the sixteenth below; SEQ > NOTE's knob in 64ths.
# ROBUST (tests/robust_test.c): damaged or crafted stored data and editor requests: engine numbers past the last
#                   refused, a retained older
#                   RAM project bounded, user preset patterns inside their fields, malformed requests never stop PLAY.
# Sanitizers (ASan + UBSan, when the compiler has them; SANITIZE=0 skips): the stored-data and protocol tests again
#                   (loader, M-UPGRADE entry, editor, projects, backup, ROBUST) and two short fuzz runs with fixed
#                   seeds: tests/fuzz_ed.c (editor SysEx and raw USB-MIDI packets), tests/fuzz_proj.c (mutated project
#                   stores -> import -> restore -> render). Longer runs: build/host/asan/fuzz_ed 300000 7 (iterations, seed), the same for the others.
#                   UBSan leaves out the DSP's intended wraps (signed overflow, shifts); bounds, object-size and
#                   pointer-overflow stay off as in Felucca (once for the user zones' XIP rebase).
# Change baseline entries only for reviewed, intentional differences in sound or cost;
# retain every unaffected golden / CPU / target entry. VERBOSE=1: every render.
set -e
export AC79_SDK="${AC79_SDK:-$HOME/fw-AC79_AIoT_SDK}"
cd "$(dirname "$0")/.."
OUT=build/host
mkdir -p "$OUT"
CC="${CC:-cc} -O1 -Wall -Wno-unused-function"
fail=0
run() { echo "== $1"; shift; "$@" || fail=1; }

$CC -O2 -o "$OUT/core1_stop_test" tests/core1_stop_test.c
run "core 1 halt: delayed bus fault and restored protection" "$OUT/core1_stop_test"

$CC -o "$OUT/storage_test" tests/storage_test.c
run "flash storage (A/B, torn writes)" "$OUT/storage_test"

$CC -o "$OUT/upreset_test" tests/upreset_test.c
run "user presets (UP_PUT parser, bank round trip, versions, PHYS DRUM kept, SAMPLE PERC -> DRUM, grid records, DIGITAL kept)" "$OUT/upreset_test"

$CC -o "$OUT/input_test" tests/input_test.c
run "keys and buttons: fast press, long release, bouncy contacts (one note each), glitches, encoders" "$OUT/input_test"
$CC -Ibuild/gen -Ifirmware/src -o "$OUT/vel_diag_test" tests/vel_diag_test.c
run "velocity diagnostic (V10.6z5): the scan's ADC slots, MASTER kept, drive, presses and captures on synthetic traces" "$OUT/vel_diag_test"
$CC -Ifirmware/src -o "$OUT/boot_lz4_test" tests/boot_lz4_test.c
run "the boot's packed RAM code (V10.6z5): LZ4 blocks back byte for byte, the build's own too" "$OUT/boot_lz4_test"

$CC -o "$OUT/midi_uart_test" tests/midi_uart_test.c
run "TRS MIDI parser" "$OUT/midi_uart_test"
$CC -o "$OUT/ble_midi_test" tests/ble_midi_test.c
run "BLE-MIDI packets (the stack's codec, firmware/bt/ble/ble_midi.c)" "$OUT/ble_midi_test"
$CC -o "$OUT/ble_aes_test" tests/ble_aes_test.c
run "BLE pairing's crypto (V10.32: AES-128, c1 / s1, the LL's AES-CCM: firmware/bt/ble/ble_aes.c)" "$OUT/ble_aes_test"
$CC -o "$OUT/usb_host_desc_test" tests/usb_host_desc_test.c
run "USB > HOST (V10.41): a keyboard's MIDI IN found, a stick / an output-only synth refused, the event filter; (V10.53) an audio interface's 16-bit 44.1 capture" "$OUT/usb_host_desc_test"
$CC -o "$OUT/bt_midi_test" tests/bt_midi_test.c
run "BLE-MIDI to and from JIANT's MIDI rings (firmware/bt/bt_midi.h, V10.13)" "$OUT/bt_midi_test"

HALF=$(sed -n 's/^#define HALF_FRAMES \([0-9]*\).*/\1/p' firmware/src/core.h)
$CC -O2 -DT_CDC=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_test" tests/uac_test.c -lm
run "USB audio input: descriptors (with CDC), ring and packets, 44.1 -> 48 kHz resampler, 48 kHz stream" "$OUT/uac_test"
$CC -O2 -DT_CDC=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_test_nocdc" tests/uac_test.c -lm
run "USB audio input: descriptors (without CDC), ring and packets, resampler, 48 kHz stream" "$OUT/uac_test_nocdc"
$CC -O2 -DT_C1=0 -DHALF_FRAMES=$HALF -o "$OUT/uac_sched_test" tests/uac_sched_test.c -lm
run "USB audio scheduler: 3 min at +-500 ppm, late renders, starvation, IRQs off (one core)" "$OUT/uac_sched_test"
$CC -O2 -DT_C1=1 -DHALF_FRAMES=$HALF -o "$OUT/uac_sched_test_c1" tests/uac_sched_test.c -lm
run "USB audio scheduler: the same, CORES 2 (core 1 resamples each half at 48 kHz)" "$OUT/uac_sched_test_c1"
$CC -O2 -DHALF_FRAMES=$HALF -o "$OUT/stems_usb_test" tests/stems_usb_test.c -lm
run "USB STEMS / STEMS+IN (0.9.8): minutes into the app's lanes, every frame once in its channel, no babble, slips crossfaded" "$OUT/stems_usb_test"
# USB descriptor layouts (#67): CDC UAC LAYOUT CDC-presented 48K; layout 0 and the console left out = 1.0's bytes
# (+ the 48 kHz rate with 48K = 1, the default; 48K = 0: byte for byte)
for v in 1.1.0.1.1 1.1.1.1.1 1.1.2.1.1 1.1.3.1.1 1.1.0.0.1 1.1.2.0.1 0.1.0.1.1 1.1.0.1.0 1.1.0.0.0 1.1.2.1.0 0.1.0.1.0 \
         1.0.0.1.1 1.0.1.1.1 1.0.0.0.1 0.0.0.1.1; do
    IFS=. read -r t_cdc t_uac t_lay t_on t_48 <<EOF
$v
EOF
    $CC -DT_CDC="$t_cdc" -DT_UAC="$t_uac" -DT_LAYOUT="$t_lay" -DT_ON="$t_on" -DT_48K="$t_48" -o "$OUT/usb_desc_test" \
        tests/usb_desc_test.c
    run "USB descriptors: CDC $t_cdc (presented $t_on), UAC $t_uac (48 kHz $t_48), layout $t_lay" "$OUT/usb_desc_test"
done

[ -f build/felucca.fwsc ] || { echo "run ./build.sh first"; exit 1; }

$CC -DOWN_PKG=1 -o "$OUT/ota_test" tests/ota_test.c
run "M-UPGRADE entry (own loader)" "$OUT/ota_test" build/felucca.fwsc

head -c 200000 build/felucca.bin > "$OUT/old_app.bin"
python3 tools/fm1pkg_make.py "$OUT/old_app.bin" build/loader/ota.bin "$OUT/old.fwsc" >/dev/null
$CC -o "$OUT/ldr_test" tests/ldr_test.c
SLOT=$(python3 -c 'import sys; sys.path.insert(0, "tools"); import fm1pkg_make; print(fm1pkg_make.APP_SLOT)')
run "update loader: other app -> this build" "$OUT/ldr_test" "$OUT/old.fwsc" build/felucca.fwsc "$SLOT"
# (0.9.13) an app that fills the whole grown slot (the region then ends at 0x92FFE), with today's loader
python3 -c 'import os, sys; a = open(sys.argv[1], "rb").read(); n = int(sys.argv[3]); open(sys.argv[2], "wb").write(a + os.urandom(n - len(a)))' \
    build/felucca.bin "$OUT/full_app.bin" "$SLOT"
python3 tools/fm1pkg_make.py "$OUT/full_app.bin" build/loader/ota.bin "$OUT/full.fwsc" >/dev/null
run "update loader: an app filling the whole slot" "$OUT/ldr_test" "$OUT/old.fwsc" "$OUT/full.fwsc" "$SLOT"
run "update loader: full slot -> this build" "$OUT/ldr_test" "$OUT/full.fwsc" build/felucca.fwsc "$SLOT"
python3 -c 'import sys; open(sys.argv[2], "wb").write(open(sys.argv[1], "rb").read() + b"\0")' "$OUT/full_app.bin" "$OUT/over_app.bin"
if python3 tools/fm1pkg_make.py "$OUT/over_app.bin" build/loader/ota.bin "$OUT/over.fwsc" >/dev/null 2>&1; then
    echo "FAIL: the packager took an app one byte past the slot"; fail=1
else echo "ok: the packager refuses an app one byte past the slot"; fi

if [ -f build/gen/felucca_tables.h ]; then
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/hostsim" tests/hostsim.c -lm
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/scale_test" tests/scale_test.c -lm
    run "scales: white-key mapping and note lifecycle" "$OUT/scale_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/speaker_test" tests/speaker_test.c -lm
    run "SPEAKER EQ: FLAT / LOWCUT / BASS+ responses, BASS+ harmonics of the bass, the sub cut, no offset after" "$OUT/speaker_test"
    run "DSP render (ANALOG preset 0)" "$OUT/hostsim" 0 0 1 "$OUT/render.wav"
    mkdir -p build/tracks_demo
    run "TRACKS: 4-track pattern, live recording (lengths, swing), voice budget, engine switch, cost" env TRACKS=build/tracks_demo "$OUT/hostsim" 0 0 1 "$OUT/tracks.wav"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/project_test" tests/project_test.c -lm
    run "project formats (FUN1..FUN6 refused; FUN7..FUNC; DIGITAL tracks -> FM6 init, SAMPLE -> ANALOG)" "$OUT/project_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/motion_test" tests/motion_test.c -lm
    run "motion, whole-step chance, FUN7 migration, song restore, ARP repeat and the 1.2 ARP modes" "$OUT/motion_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/djf_test" tests/djf_test.c -lm
    run "DJ FILTER (0.9.1): centred bit for bit 0.9.0, LP / HP response, no click crossing the middle, automation, old projects" "$OUT/djf_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/tape_test" tests/tape_test.c -lm
    run "TAPE (0.9.9-snd8): off never runs, the drive's harmonics, the highs lost, the head bump, WOW's few cents, no click, kept in a project" "$OUT/tape_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/eq_test" tests/eq_test.c -lm
    run "EQ (p9): the master's four bands, flat bit for bit, each band moving its tones, kept in a project" "$OUT/eq_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/micro_test" tests/micro_test.c -lm
    run "MICRO in 64ths (0.9.1): recorded within 1/64 of a step at any tempo / division, played there, REC Q 50 %, old projects, 'M' 'F'" "$OUT/micro_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ctl64_timing_test" tests/ctl64_timing_test.c -lm
    run "ctl64 (0.9.9): the rendered notes at the sequencer's sample (MICRO-shifted steps, POLY, MONO cuts, kit hits) in blocks of CTL" "$OUT/ctl64_timing_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/gen_test" tests/gen_test.c -lm
    run "GEN (0.6.91): the song generator, every genre: deterministic, one scale, the kick anchored, the bass's roots, chord tones, forms, the screen; (0.9.9-gen1) the clean base, the archetypes, the CPU budget; (gen2) the 303 line and its legato slides, the leads' vibrato, the drums' decays; (gen3) the rules by role, the drums' share, the loudness" "$OUT/gen_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/loop_song_test" tests/loop_song_test.c -lm
    run "GEN FROM LOOP: the loop's key and chords heard (C major, A minor, a DRUM-X 808 line, drums alone), the loop untouched as A, every section its material, new parts on empty tracks in key, a counter-melody of its melody, played end to end, kept, UNDO, seeds" "$OUT/loop_song_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/stems_test" tests/stems_test.c -lm
    run "STEMS (0.9.8): DICE songs, each stem = its track's solo render (FULL LITE ECO, CORES 1 / 2), the sum = the mix, the SLICER" "$OUT/stems_test"
    $CC -O1 -w -o "$OUT/cpu_safe_test" tests/cpu_safe_test.c
    run "CPU safety (p1, main 0.9.12 / 0.9.13): crash code, the crash record (row 9), a crash logged (p2b: 2 CORES kept), MENU > SYSTEM > CPU CORES / CPU CLOCK" "$OUT/cpu_safe_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/ratchet_test" tests/ratchet_test.c -lm
    run "RATCH: x1..x4 in a step (notes, chords, drum hits), gates, chance, swing, projects, user presets, CHANCE page" "$OUT/ratchet_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/live_test" tests/live_test.c -lm
    run "LIVE (p2, main 0.9.11, GLO > LIVE MANO / RIELES): RATE on the beat, CORTE / LOOP / RATCH / THIN / REV / PING on the 1/16, the ghost, RETURN, BYPASS, COMMIT + UNDO, REC at the ghost, PISTAS ALL..T6, MANO's keys only there, the chain, the PLAYHEADS" "$OUT/live_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/comp_test" tests/comp_test.c -lm
    run "COMPONENTS v2 (seq2d): the 16-bar cycle, RATCHET STUTTER MUTE ARP SEQ JUMP over their LENGTH, MONO / POLY, migration, COMP pages; (seq2f) the global lane" "$OUT/comp_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumx_oct_test" tests/drumx_oct_test.c -lm
    run "DRUM-X OCTAVES (0.8.5): ratchets above the kit, SHORT GHOST FLAM below, live, recorded and played back as heard, MIDI bands" "$OUT/drumx_oct_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/midi_control_test" tests/midi_control_test.c -lm
    run "USB/TRS clock, bend, sustain, ownership and panic recovery" "$OUT/midi_control_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/stuck_test" tests/stuck_test.c -lm
    run "stuck notes: keys, SCENE, LATCH, ARP, MIDI IN and MIDI OUT notes end whatever changed while held; PANIC (PLAY held)" "$OUT/stuck_test"
    $CC -O1 -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/digital_test" tests/digital_test.c -lm
    run "DIGITAL (retired, built here with FELUCCA_FM4=1): operator envelopes/levels" "$OUT/digital_test"
    $CC -O2 -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/fm4_test" tests/fm4_test.c -lm
    mkdir -p build/fm4_demo
    run "DIGITAL -> FM6: the conversion against DIGITAL (FELUCCA_FM4=1): pitch, centroid, RMS envelope; demos" \
        "$OUT/fm4_test" build/fm4_demo
    $CC -O2 -w -fsanitize=integer-divide-by-zero -fno-sanitize-recover=all -Ibuild/gen -Ifirmware/src \
        -o "$OUT/fm4_div0_test" tests/fm4_div0_test.c -lm
    run "DIGITAL -> FM6 without a divide by zero (#61): the same values as before, a 0.9 ORGAN project (UBSan)" \
        "$OUT/fm4_div0_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/theme_test" tests/theme_test.c -lm
    run "themes: contrast, text blending and font metrics" "$OUT/theme_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/text_ref_test" tests/text_ref_test.c -lm
    run "text: pens, kerning and pixels equal the reference renderer" "$OUT/text_ref_test"
    if python3 -c "import PIL" 2>/dev/null; then
        run "text spacing: glyph gaps against the font's own (16x supersampled), a phase step (S M L <= 0.5 px)" \
            python3 tests/text_spacing_test.py build/gen/ui_fonts.h
    fi
    $CC -O1 -w -Ibuild/gen -o "$OUT/settings_test" tests/settings_test.c
    run "settings: PER1..PER4 migration, palette ids and preference preservation" "$OUT/settings_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test" tests/ui_test.c -lm
    run "UI: sounds keep steps, undo, recording, MIDI overflow, pending saves, panel recovery, drum grid, song chain, GREY gray, MONO neutral; (gen3) the COMP cards whole, every value" "$OUT/ui_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/rec_play_test" tests/rec_play_test.c -lm
    run "REC PLAY keys STOP PLAY: the take plays back (power-on, NEW PROJECT, after a GEN song in SONG mode, TIMELINE)" "$OUT/rec_play_test"
    FV=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)   # (the splash, ABOUT: the real version)
    $CC -O1 -w "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render" tests/ui_render.c -lm
    mkdir -p build/ui_new/ppm build/ui_slot
    run "UI renders: layout lint (every screen and palette, every page, engine and column value), GREY gray, MONO neutral, alignment by ink (1 px fails), draw cost" \
        "$OUT/ui_render" build/ui_new build/ui_slot
    if python3 -c "import PIL" 2>/dev/null; then python3 tests/ui_render.py build/ui_new build/ui_slot; fi
    $CC -w -DFELUCCA_FM4=1 -Ibuild/gen -Ifirmware/src -o "$OUT/ui_test_fm4" tests/ui_test.c -lm
    run "UI built with FELUCCA_FM4=1 (DIGITAL, kept in the tree): its OP pages, EDIT cycle, algorithm charts" "$OUT/ui_test_fm4"
    $CC -O1 -w -DFELUCCA_FM4=1 "-DFELUCCA_VERSION=\"$FV\"" -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/ui_render_fm4" tests/ui_render.c -lm
    mkdir -p build/ui_fm4/ppm build/ui_fm4_slot
    run "UI renders with FELUCCA_FM4=1: DIGITAL's screens (EDIT, OP ENV, the 8 algorithm charts), lint, GREY gray, MONO neutral" \
        "$OUT/ui_render_fm4" build/ui_fm4 build/ui_fm4_slot
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/audio_test" tests/audio_test.c -lm
    run "audio: overload protection, bounded fades and DMA diagnostics" "$OUT/audio_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/click_test" tests/click_test.c -lm
    run "metronome and count-in: beats sample for sample (internal and external clock), accent, level, MASTER, not in USB, count-in timing, notes onto step 1" "$OUT/click_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fit_test" tests/fit_test.c -lm
    run "FIT: exact loop spans, live controls, projects and arrangement" "$OUT/fit_test"
    $CC -O1 -w -Ibuild/gen -Ifirmware/src -o "$OUT/probability_component_test" tests/probability_component_test.c -lm
    run "step probability with SEQ and STUTTER components" "$OUT/probability_component_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/persistence_test" tests/persistence_test.c -lm
    run "persistence: deferred settings, retry and failed-save rollback" "$OUT/persistence_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/startup_test" tests/startup_test.c -lm
    run "startup: saved arrangement survives initialization and idle saving (by @workeffortwaste, #134)" "$OUT/startup_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/backup_test" tests/backup_test.c -lm
    run "full backup: CRC before writes, stale runtime, USB reset / timeout, malformed objects, older projects" "$OUT/backup_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/editor_test" tests/editor_test.c -lm
    run "editor: real C protocol, malformed transfers, queue recovery, MENU settings (writes build/host/menu.json)" \
        env MENU_JSON="$OUT/menu.json" "$OUT/editor_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/app_contract_test" tests/app_contract_test.c -lm
    run "app contract: the Stems app's requests vs golden replies, 1.0 pushes, engine ids, NRPN (docs/NRPN.md)" \
        "$OUT/app_contract_test"
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/robust_test" tests/robust_test.c -lm
    run "robustness: engine numbers, retained old projects, preset patterns, malformed requests" \
        "$OUT/robust_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/mod_test" tests/mod_test.c -lm
    mkdir -p build/mod_demo
    run "modulation matrix: off = bit-identical, the math, MIDI CC1 / CC11 / aftertouch, cost, demos" "$OUT/mod_test" build/mod_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/slicer_test" tests/slicer_test.c -lm
    mkdir -p build/slicer_demo
    run "SLICER: no clicks, timing, sync with the sequencer, STUT, cost, demos" "$OUT/slicer_test" build/slicer_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/swing_test" tests/swing_test.c -lm
    run "SWING: track + global at most 100, sequencer and SLICER step lengths, the SWG display" "$OUT/swing_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/perform_test" tests/perform_test.c -lm
    mkdir -p build/perform_demo build/fx_demo
    run "FX layer effects: on the 1/16, stereo, too-long REPEAT, SLICER, silent keys, idle bit-identical, clicks, cost, demos" "$OUT/perform_test" build/perform_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/reverb_test" tests/reverb_test.c -lm
    run "REVERB TYPE: ROOM bit-identical, SPRING decay / chirp / stability / level, model change, cost, demos" "$OUT/reverb_test" build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/cloud_hot_test" tests/cloud_hot_test.c -lm
    run "CLOUD driven hot (p1, main 0.9.14 / 0.9.16): the loop never runs away (any SIZE / RATE), its line never clamps, no grain across the write head, no trip, SIZE max from a cleared line sounds" "$OUT/cloud_hot_test" build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/delay_test" tests/delay_test.c -lm
    run "DELAY: no leak at the far end, the long times at half / quarter rate (timing, response, aliases, noise), no click on a rate change, MOD / GRAIN / FEEDBK there, demos" "$OUT/delay_test" build/fx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/voice_fairness_test" tests/voice_fairness_test.c -lm
    run "four single-note tracks: own release before cross-track stealing (budget 8)" "$OUT/voice_fairness_test" 8
    run "four single-note tracks: own release before cross-track stealing (budget 12)" "$OUT/voice_fairness_test" 12
    run "four single-note tracks at 70..90 % (cpu5): no tail cut under the cap, a spike no trip, the guard keeps the leads, the prediction" "$OUT/voice_fairness_test" load
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/chorus_sleep_test" tests/chorus_sleep_test.c -lm
    run "chorus: exact zero-ring shortcut, tails and restart, unchanged state" python3 tests/chorus_sleep_test.py "$OUT/chorus_sleep_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/empty_fx_test" tests/empty_fx_test.c -lm
    run "empty FX: exact zero-state shortcuts, wake and model changes" python3 tests/empty_fx_test.py "$OUT/empty_fx_test"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fx_rest_test" tests/fx_rest_test.c -lm
    $CC -O2 -w -DFX_REST_OFF -Ibuild/gen -Ifirmware/src -o "$OUT/fx_rest_ref" tests/fx_rest_test.c -lm
    run "FX at rest (cpu5): the gate shut, the buses not run and silenced, the same output before, zeros through, the wake" \
        python3 tests/fx_rest_test.py "$OUT/fx_rest_test" "$OUT/fx_rest_ref"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/regress" tests/regress.c -lm
    run "regression: golden renders, health, voices, CPU budget" "$OUT/regress" tests/golden.txt tests/cpu_baseline.txt
    $CC -O2 -w -pthread -Ibuild/gen -Ifirmware/src -o "$OUT/mc_thread_test" tests/mc_thread_test.c -lm
    run "mixer: serial, deferred, real thread, delays and lost job" python3 tests/mc_thread_test.py "$OUT/mc_thread_test"
    # (FM4_CONVERT 1: the DIGITAL -> FM6 conversion the editor mirrors, kept in the tree; the firmware retired it in V10.6s)
    $CC -w -DFM4_CONVERT=1 -Ibuild/gen -Ifirmware/src -o "$OUT/descdump" tests/descdump.c -lm
    echo "== parameter and engine tables as JSON (for the editor mock test)"
    "$OUT/descdump" > "$OUT/desc.json" || fail=1
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/filter_test" tests/filter_test.c -lm
    run "FILTER: every TYPE bounded at RES 127 across the keyboard, COMB / COMB- in tune, no zipper, RES the ring, MS20, demos" \
        "$OUT/filter_test" build/filter_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/drumx_test" tests/drumx_test.c -lm
    mkdir -p build/drumx_demo
    run "DRUM-X: every lane sounds and ends, MORPH, PITCH, NOIS, hat choke, group mutes, demos" "$OUT/drumx_test" build/drumx_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/noise_test" tests/noise_test.c -lm
    mkdir -p build/noise_demo
    run "NOISE: colour slopes, key-tracked filter and clock, META period, DC, clipping, retrigger, cost, demos" "$OUT/noise_test" build/noise_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/animal_test" tests/animal_test.c -lm
    mkdir -p build/animal_demo
    run "ANIMAL: CMOS on the key and the owner's video, cricket, TEMP, Hopf onset, presets, DC, clipping, retrigger, demos" "$OUT/animal_test" build/animal_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/string_add_test" tests/string_add_test.c -lm
    mkdir -p build/string_add_demo
    run "STRING ADD (0.9.9-snd5): in tune C1 .. C6, DECAY's T60, BRIGHT, the COMB TYPE, partials, tilt, EVEN, DAMP, no alias, presets, corners, demos" "$OUT/string_add_test" build/string_add_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/voice_test" tests/voice_test.c -lm
    mkdir -p build/voice_demo
    run "VOICE words: ES / JA / EN reading, the generator, formants, fricatives, plosives, clicks, DC, the lyric, POS, LOOP, demos" "$OUT/voice_test" build/voice_demo
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_env_compare" tests/fm6_env_compare.c -lm
    run "FM6 compact envelope: original state machine and all carrier masks" "$OUT/fm6_env_compare"
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6_test" tests/fm6_test.c -lm
    mkdir -p build/fm6_demo
    run "FM6: algorithms, envelopes, retrigger, DC, clipping, macros, patch formats, voices, cost, demos" "$OUT/fm6_test" build/fm6_demo
    $CC -w -Ibuild/gen -Ifirmware/src -o "$OUT/fm6bank_test" tests/fm6bank_test.c -lm
    run "FM6 voice bank: write / read, power cuts, CRC, SLOT B1..B32, stored SLOT values as before" "$OUT/fm6bank_test"
else
    echo "== skip hostsim (run ./build.sh once)"
fi

# ASan + UBSan: the stored-data and protocol paths, and short fuzz runs (fixed seeds: the same inputs every run)
SAN="-O1 -g -fsanitize=address,undefined -fno-sanitize=signed-integer-overflow,shift,bounds,object-size,pointer-overflow"
SAN="$SAN -fno-sanitize-recover=undefined -w"
san_ok() {
    printf 'int main(void){return 0;}\n' > "$OUT/san_probe.c" &&
        ${CC%% *} $SAN -o "$OUT/san_probe" "$OUT/san_probe.c" 2>/dev/null && "$OUT/san_probe"
}
if [ "${SANITIZE:-1}" = 0 ]; then
    echo "== skip the sanitizer runs (SANITIZE=0)"
elif [ ! -f build/gen/felucca_tables.h ]; then
    echo "== skip the sanitizer runs (run ./build.sh once)"
elif ! san_ok; then
    echo "== skip the sanitizer runs (the compiler has no ASan / UBSan)"
else
    mkdir -p "$OUT/asan"
    A="$OUT/asan"
    SCC="${CC%% *} $SAN -Ibuild/gen -Ifirmware/src"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}"
    $SCC -o "$A/ldr_test" tests/ldr_test.c
    run "ASan/UBSan: update loader (other app -> this build)" "$A/ldr_test" "$OUT/old.fwsc" build/felucca.fwsc
    $SCC -DOWN_PKG=1 -o "$A/ota_test" tests/ota_test.c
    run "ASan/UBSan: M-UPGRADE entry (own loader)" "$A/ota_test" build/felucca.fwsc
    for t in editor_test project_test backup_test robust_test fm6bank_test; do
        $SCC -o "$A/$t" tests/$t.c -lm
        run "ASan/UBSan: $t" "$A/$t"
    done
    $SCC -o "$A/fuzz_ed" tests/fuzz_ed.c -lm
    run "ASan/UBSan fuzz: editor SysEx and raw USB-MIDI packets (20000, seed 7)" "$A/fuzz_ed" 20000 7
    $SCC -o "$A/fuzz_proj" tests/fuzz_proj.c -lm
    run "ASan/UBSan fuzz: project stores -> import -> restore -> render (5000, seed 13)" "$A/fuzz_proj" 5000 13
fi

run "regression: target cost of the render loops (pi32v2 disassembly)" python3 tests/target_budget.py \
    build/felucca.dis tests/target_budget.txt

# (JIANT 0.6.83) the audio path's CPU where proc_pid_rusage is missing (Linux): callgrind's exact counts, section by
# section (each engine's heaviest preset, every REVERB TYPE, a full mix, the sequencer at its heaviest), against
# tests/cpu_baseline_vg.txt (x86-64 gcc; another compiler: BUDGET_UPDATE=1 once, then it watches from there)
if command -v valgrind >/dev/null 2>&1 && [ "$(uname -s)" = Linux ] && [ "$(uname -m)" = x86_64 ]; then
    $CC -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o "$OUT/cpu_vg" tests/cpu_vg.c -lm
    run "CPU under valgrind: engines, reverbs, full mix, the sequencer's heaviest (exact counts, +8 %)" \
        python3 tests/cpu_vg.py "$OUT/cpu_vg" tests/cpu_baseline_vg.txt
else
    echo "== skip CPU under valgrind (no valgrind, or not Linux x86-64)"
fi

run "installer CLI (fm1_install.py) against a simulated FM-1" python3 tests/install_test.py
run "navigation map (docs/navigation-map*.svg) matches the firmware's chains" python3 tools/gen_nav_map.py --check

if [ -d android/stems ]; then                         # (the app, while it lives here)
    JH="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v javac || command -v java || echo /x/x/x)")")")}"
    if [ -f "$JH/include/jni.h" ]; then
        cc -O1 -I"$JH/include" -I"$JH/include/linux" -o "$OUT/usbstream_test" android/stems/app/src/test/cpp/usbstream_test.c -lm
        run "JIANT Stems: usbstream.c (every frame once, a cut frame left out whole, the mic's drift with no sample slipped)" "$OUT/usbstream_test"
    else
        echo "== skip JIANT Stems usbstream.c (no JDK headers)"
    fi
    if command -v gradle >/dev/null 2>&1; then
        run "JIANT Stems :core (JVM): WAV, layout, splitter, latency, mix, exports, the take's length and trim" \
            gradle -q -p android/stems -PcoreOnly --offline :core:test
    else
        echo "== skip JIANT Stems :core (no gradle)"
    fi
fi
if command -v node >/dev/null 2>&1; then
    run "web pages: editor protocol, package builder, update protocol" node web/test_web.mjs
    run "web backup: capture, validation before writes, restore order" node web/test_backup.mjs
else
    echo "== skip web tests (no node)"
fi

if ! command -v emcc >/dev/null 2>&1; then
    echo "== skip the browser emulator (no emcc: Emscripten builds web/emu)"
elif ! command -v node >/dev/null 2>&1 || [ ! -f build/gen/felucca_tables.h ]; then
    echo "== skip the browser emulator (needs node and build/gen)"
else
    run "browser emulator: the firmware to WebAssembly (web/emu/build.sh -> build/emu)" sh web/emu/build.sh
    cc -O2 -ffp-contract=off -w -Ibuild/gen -Ifirmware/src -o "$OUT/emu_native" web/emu/native_check.c -lm
    run "browser emulator: the same song from the native build" "$OUT/emu_native" "$OUT/emu_native.f32"
    run "browser emulator: boot, keys, MIDI, screen, LEDs, PLAY, a save kept across instances, = native, cost" \
        node web/emu/emu_test.mjs build/emu/felucca.wasm "$OUT/emu_native.f32"
        run "stems from the browser build (web/emu/stems.js): a backup in, each part alone, the song once round, WAV + ZIP" node web/emu/stems_test.mjs build/emu/felucca.wasm
fi

[ $fail -eq 0 ] && echo "ALL HOST TESTS PASSED" || { echo "HOST TESTS FAILED"; exit 1; }

