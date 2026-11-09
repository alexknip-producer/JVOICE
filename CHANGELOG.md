# JIANT FM changelog

Custom firmware for the M-VAVE FM-1 by Juan Jiant. Newest version first.

**Release status**

| Version | Date | Status |
| --- | --- | --- |
| **V10.7.6** | 2026-11-09 | **Public** |
| V10.6d … V10.7.5 (except V10.6z2) | 2026-11-06 … 11-09 | Beta |
| **V10.6z2** | 2026-11-07 | **Public** |
| **V10.6c** | 2026-11-03 | **Public** (the last public release before this cycle) |

If you are coming from **V10.6c**, read everything down to V10.6c. If you are coming from **V10.6z2**, read down to V10.6z3.
The sections marked **Compatibility notes** are the ones to check before you install.

> **A note on version numbers.** Before V10.6c the versions counted up as V10.1, V10.2 … V10.61. V10.6c started a new
> scheme: V10.6c, V10.6d … V10.6z, V10.6z2 … V10.6z10, then V10.7, V10.7.1 … V10.7.5. The **V10.6** and **V10.7** of
> 31 October 2026 are much older builds with the same names; they are covered under "Older versions" at the end.

---

## Known issues in V10.7.6

- **Velocity on the top row.** The keys C#5..G5 have no pressure sensor, so they play at the average of your recent
  playing and have no aftertouch. Velocity and aftertouch work on F3..C5.
- **AIR is noisy.** As a modulation source and for SWARM, AIR (the Bluetooth devices heard around you) jumps
  more than PROX does. Use **PROX** (your own phone, calibrated FAR / NEAR) when you can.
- **VEL on old projects.** The new VEL setting (EDIT > VOICE 2, KNOB 3) is saved in the place where MUTATE kept its
  settings. A project that still has old MUTATE settings in it may open with **VEL under 100 %** on some tracks. Check
  VEL on each track after you load an old project, and set it back to 100 % if a track feels flatter than before.
- **Installing from Android can stall.** Install from Chrome or Edge on a computer when you can. A phone with
  an OTG cable may work, but a computer is safer.
- **GLO > SYSTEM 2 only shows values.** You can't edit them there. The page tells you where to change them
  (MENU > BLUETOOTH).

---

## V10.7.6 — 2026-11-09 — public

**Fixed**
- SEQ > STRETCH > NUDGE (OCT+ twice, the step): a held note moves with its ties; the neighbour keeps its length; at
  step 1 and at LEN the step stays (ties can't wrap). Locks and automation move with it; UNDO puts it back.
- SEQ > COMP: the MOVE card reads whole ("-63 ST").
- Web editor: the delay times include the dotted ones (1/4D 1/8D 1/16D) in the right order.
- The automatic tests run to the end again (they had stopped early since 8 Nov), so regressions show up again.

---

## V10.7.5 — 2026-11-09 — beta

**New**
- GENE **CUMBIA** rebuilt from the owner's reference and his own loops: syncopated basses (root on 1, the fifth
  anticipating beat 3, a closing 4th or 7th), kick on 1 and 3, llamador and conga accents, the bell on 2 (and 4).
- The güiro plays on the "e" in sonidera throws (as in the owner's loops) and on the beat in villera and digital throws.
- A generated cumbia song has a break in its fill bar.

## V10.7.4 — 2026-11-09 — beta

**Changed**
- SEQ > STRETCH, NUDGE now has a third mode. Press OCT+ again to move the selected step itself one step left or right.
  So: OCT- moves the whole sequence, OCT+ moves the step's MICRO timing, and OCT+ again moves the step.

## V10.7.3 — 2026-11-09 — beta

**New**
- **VEL**: how much velocity affects each sound (EDIT > VOICE 2, KNOB 3, 0..100 %). At 0 % every note plays at a
  fixed full level. Accents still work.

**Changed**
- Factory basses are set to VEL 0 % (steady level). GENE's basses are set to 75 %.

**Compatibility notes**
- Old projects open with VEL at 100 %, so they sound as before. One exception: see "VEL on old projects" in Known issues.

## V10.7.2 — 2026-11-08 — beta

**Changed**
- GENE combines V10.6z10's variety (below) with V10.7's mix and the rules for the Latin genres.
- **CUMBIA** throws are sonidera, villera or digital. **REGGAETON** throws are classic, romántico or perreo. **DEMBOW**
  throws are classic or rápido. Each style follows its own rules: güiro, root–fifth bass, the dembow riddim, tresillo
  808, chops.

## V10.6z10 — 2026-11-08 — beta (built in parallel, merged into V10.7.2)

**Changed**
- GENE: throwing the same genre twice no longer gives you the same song.
  - The bass style changes on every throw, and so do the riffs. JAZZ gets a third walking-bass style.
  - Sounds vary more. A third of the time GENE picks another preset of the same type, never the same option twice, and
    never an INIT preset. Drive, chorus and detune spread a little wider. Phonk and baile funk always keep the
    808 COWBELL.
  - Tempo moves within a third of the genre's range around the ENERGY setting.
  - More drum variety: each genre's own extra kicks, euclidean percussion, ghost notes on the snare, a slightly varied
    first bar, and two more IDM breaks (FUNKY, THINK).
  - Chord progressions come from a family of harmonies, and the genre's own family a third of the time.
  - Later sections develop the first one (bass, chords, arp and the lead's motif vary) instead of repeating it.
  - OCT+ never brings back one of the genre's last eight songs. The same seed still gives the same song.

## V10.7.1 — 2026-11-08 — beta

**Fixed**
- Velocity and aftertouch are back to V10.6z9's behaviour. V10.7's steadier velocity made hard strikes silent and
  aftertouch weak. GENE's mix and the new manuals from V10.7 stay.

## V10.7 — 2026-11-08 — beta (prepared as a release, replaced by V10.7.1)

**New**
- GENE songs mixed like a record: part levels for each genre, level trims for each sound, stereo width, steady sounds.
- DnB gets the Amen break and a four-chord loop. The club genres get arp hooks.
- New manuals (English and Spanish), README and cheat sheets.

**Changed**
- Steadier velocity. The top row follows your dynamics. *(Undone in V10.7.1.)*
- GENE replaces SOFT PAD and BRASS, and no longer randomly changes list settings.

## V10.6z9 — 2026-11-08 — beta

**Fixed**
- DRUM-X has its punch back. The kicks have their body from before V10.6q again: NOISE is off by default, and raising it
  adds a short knock. The hats are bright again (LOFI DUST, HIP HOP, 808, ACOUSTIC, DUB ROOTS, GENE's soft kits, the JAZZ
  ride). Snares, claps and cowbells are unchanged.

## V10.6z8 — 2026-11-08 — beta

**Changed**
- If you change a sound while a song plays, the song keeps your change. The track holds it until a new GENE throw, a
  load, a jump or EDIT > RECALL. Sections still change the steps, mix and mutes.
- PROX calibration (FAR / NEAR) now takes the middle reading of 2.5 seconds. Hold the phone still while it measures.
- AIR is smoothed so it jumps less.
- GLO > SYSTEM 2 tells you where its values are edited.

## V10.6z7 — 2026-11-08 — beta

**New**
- **Key velocity.** The keys F3..C5 play as hard as you strike them and calibrate themselves as you play. MENU >
  CONTROL > VELOCITY sets the curve (SOFT / NORMAL / HARD, or OFF: every note at 100 as before), and VEL MIN sets the
  lowest level. The top row plays your recent average.
- **Aftertouch.** Press harder on a held key. It works as a modulation source (AT) and goes out over MIDI. New sounds
  start with aftertouch opening the filter.
- MENU > CONTROL > AFTERTOUCH and VEL LEARN.
- A live velocity view in the web editor.

## V10.6z6 — 2026-11-08 — beta

**New**
- SWARM plays a melody from your proximity (PROX first, then AIR), on the grid and in the track's scale. With the
  chromatic scale it plays blues with blue notes.
- SWARM gets a FAR / NEAR morph like DRUM-X: RATE, MOVE, TONE, NOISE and MOD for each side (EDIT > SWARM / SWARM 2).
- Each Bluetooth device nearby adds modulation. CHAOS sets the maximum.

## V10.6z5 — 2026-11-08 — beta

**New**
- A key-velocity test mode, off by default (the first step towards V10.6z7), with a live view in the web editor.

**Changed**
- More free space in the firmware for later features. Nothing sounds different.

## V10.6z4 — 2026-11-07 — beta

**New**
- Hold **OCT-** over a sounding note for vibrato, which fades in like a mod wheel. Hold **OCT+** to bend the note up.
  MENU > AUDIO > OCT BEND sets the range (OFF, 1, 2, 3, 4, 5, 7 or 12 semitones; 2 by default). Both are recorded and
  sent out with MIDI OUT ALL.

**Changed**
- REC keeps how you played: note lengths, legato as SLIDE, legato notes that came late put on time, velocity without
  false accents, and the first note on step 1.
- New tracks start with GATE at 100 %.

## V10.6z3 — 2026-11-07 — beta

**New**
- A stable connection for companion apps (version 1.0): apps can ask which version the device runs and follow the
  transport, punch-in FX and mutes.
- Every automatable track setting and the global settings can be controlled with high-resolution MIDI (NRPN), and they
  are recorded. With MIDI OUT ALL, turning a knob sends NRPN.

## V10.6z2 — 2026-11-07 — **PUBLIC**

**Fixed**
- Some units crashed with Bluetooth on. The second processor core now starts only on the normal power setup, and not
  after a SAFE START.

## V10.6z — 2026-11-07 — beta

**Fixed**
- **Newer FM-1s** (sold from late 2026, logo on the front) use a different flash chip. On those units saving, GENE and
  firmware updates did not work. They work now.

**Compatibility notes**
- If you have one of the newer units, install V10.6z or later. Older versions can't save on it.

## V10.6y — 2026-11-07 — beta

**Fixed**
- VOICE no longer distorts on open vowels such as "AH". Its level now sits beside ANALOG's.

**New**
- WORDS, KNOB 3 **CONS**: consonant length from 50 to 400 %. POS moves to EDIT 1 when LANG is on.

## V10.6x — 2026-11-07 — beta

**Changed**
- GENE JAZZ uses any tempo, from a ballad to bebop depending on the throw's energy. The swing gets straighter as the
  tempo rises. The screen shows half the felt tempo.

## V10.6w — 2026-11-07 — beta

**New**
- GENE **HIP HOP**: each throw is trap, drill, pluggnb, boom bap or lofi.
- GENE **JAZZ**: ride pattern, hi-hat on 2 and 4, walking bass, ii–V–I sevenths, Charleston piano comping, swing.
- GENE **TECHNO ROCK**: throws rock or 80s synth.

**Changed**
- Drums brighter, and the 808 less dark.
- The piano roll, the bar thumbnails and the LIVE rails are coloured by note, in the thermal colours.

## V10.6v — 2026-11-07 — beta

**New**
- **RAMP** step component: from a step, a setting glides to a target over 1–32 steps or 1–16 bars. You choose the curve
  (LIN, EXP, LOG, S, STEPS, ZIGZAG). It glides back to the knob at the end and on STOP.
- SWARM: OCTAVE's upper half walks the scale. Each new Bluetooth device changes the timbre and gets its own.
  New **CHAOS** knob (replaces SELF).

**Removed**
- The JUMP step component (RAMP takes its place).

## V10.6u — 2026-11-07 — beta

**Changed**
- GENE genres based on the owner's reference loops:
  - CUMBIA at 76–92 BPM, with the güiro on the "e" and a brighter kit.
  - EBM gets a half-time throw (148–156) with nonstop 16th-note bass.
  - BAILE FUNK gets its claps, the tamborzão and the mandelão (128–136).
  - PHONK gets a Brazilian phonk throw (130–156).
  - HOUSE, DEEP HOUSE and TECH HOUSE are faster.

## V10.6t — 2026-11-07 — beta

**Fixed**
- GENE songs are full again: intros with more parts, the DnB and breakbeat turnaround and hats, and EXPLORE adventurous again.
- DRUM-X kicks have more body.
- Over-loud presets levelled (INIT SAW/PWM, ORGAN, TINE EP, WHISTLE, OCARINA and others).

## V10.6s — 2026-11-07 — beta

**New**
- **SAVE > SONG > CLIPS**: a grid of 6 tracks × 4 parts. KNOB 1 picks the track and KNOB 2 the part. OCT+ launches that
  cell on the next bar, and OCT- stops the track. F3..B3 launch a whole part. REC writes each launch as a song row.

**Compatibility notes**
- **Old formats retired.** Projects from the earliest JIANT builds (before format 1.0.3) are refused and left untouched.
  Old PHYS MODEL, PERC and GM drum sounds are no longer converted. A DIGITAL sound loads as FM6 INIT.

## V10.6r — 2026-11-07 — beta

**New**
- DRUM-X **KEY MAP EXTENDED** is now the default. OCT +1 plays real fills (snare rolls, tom runs, flams, 3-3-2, a crash on
  the next downbeat). OCT -1 plays any sound chromatically (KEY MAP 2..9, BELL for cowbell melodies).
- SCL > SCENE **DRUM GEN**: GENE writes the kit's part for the genre. Each key plays BASE, DENSE, BREAK or FILL.

## V10.6q — 2026-11-07 — beta

**Changed**
- DRUM-X sound quality pass (no new features): cleaner noise, less aliasing, better 808 clap, smoother knobs.
- A classic analog-style cowbell.

## V10.6p — 2026-11-07 — beta

**Changed**
- GENE **PHONK**, **BAILE FUNK** and **EBM** rewritten to sound like the real genres: a new **808 COWBELL** preset and
  riffs for phonk, the tamborzão for baile funk, and 16th-note analog basses with heavy drums for EBM.

**Compatibility notes**
- **6 songs instead of 8.** Songs 7 and 8 are reserved for a future drum sample bank. A project saved on song 7 or 8
  opens on song 6, and you get a one-time notice if they had music.
- **Back up before you install.** The installer now asks for a backup first. The backup reads 8 or 6 songs. A restore
  tells you which songs stayed in the file.

## V10.6o — 2026-11-07 — beta

**New**
- **SAVE > SONG > TIMELINE**: your song as coloured blocks. Open a row track by track (MUTE, FROM another part, its
  variation). PLAY loops the row, and PLAY again continues the song from there. OCT+ copies the row, OCT- deletes it.
- **SAVE > SONG > PARTS**: the four parts as cards. Pick one to loop it (from the next bar). KNOB 4 switches LOOP / SONG.

**Fixed**
- A long STRUM played back too fast after recording.

**Removed**
- DRUM-X ACOUST.

## V10.6n — 2026-11-07 — beta

**Changed**
- GENE's genre list curated by the owner (25 genres plus EXPLORE and RANDOM). New: HARD BASS, TRIBAL, CUMBIA, REGGAETON,
  DEMBOW, IDM GLITCH / AMBIENT / WEIRD, FUGA, ELECTRO METAL, TECHNO ROCK, DREAM POP, PHONK, BAILE FUNK.
- EXPLORE makes rarer music that is still musical.
- SCL > SCENE's dice picks a key (mostly minor modes).

**Removed**
- The genres not in the new list.

## V10.6m — 2026-11-06 — beta

**Changed**
- GENE genres keep their own drums. EBM has many more patterns and basses. Fills only at the end of a phrase.
- The club genres are arranged the way a DJ plays them: intro, groove, break, a 4-bar build (snare roll, riser,
  high-pass, crash), the drop.

**New**
- SCL > SCENE, BASS / LEAD pattern **GEN**: GENE writes the track a line in the key, and each chord moves it.

## V10.6l — 2026-11-06 — beta

**New**
- A new "jiant fm" logo at power-on, written in the owner's own handwriting order.

**Fixed**
- GENE's first press after power-on was always the same song. Now it throws a new one.

**Changed**
- EXPLORE moves (the filter breathes) instead of throwing steps around.

## V10.6k — 2026-11-06 — beta

**Changed**
- More free space for GENE. Nothing sounds different.
- SCL > SCENE's BASS / LEAD pattern and DRUM style are saved with the project.

**Compatibility notes**
- Older projects open with the first pattern and style.

## V10.6j — 2026-11-06 — beta

**New**
- Every note has its own colour (C coldest … B hottest) on KEY, CHORD, ARP and SCENE.
- SCL > SCENE: BASS and LEAD play a pattern over the chord (KNOB 4), and the DRUM role writes a beat for each key (KNOB 4 STYLE).

## V10.6i — 2026-11-06 — beta

**Changed**
- Every engine's factory presets reworked so they move (LFOs, ENV 2, modulation, filter). The browser lists normal sounds
  first, then experimental ones, then INIT.
- ANIMAL trimmed to 29 presets.

**Compatibility notes**
- Preset numbers are kept, so projects keep their sounds and PROGRAM CHANGE still works.
- Loading a sound no longer keeps MUTATE's old modulation slots.

## V10.6h — 2026-11-06 — beta

**New**
- FX > DLY 3 TYPE **REVERSE**: each repeat plays the last moments backwards.

**Fixed**
- PROGRAM CHANGE numbers are the same as before MATH existed.
- Old LOFI PULSE LD tracks load as an ANALOG organ.
- Converted chip sounds keep their filter.
- CC74 / CC71 move the FILTER page on engines without their own brightness control.
- SWARM no longer hears its own voice as a device.

## V10.6g / V10.6f — 2026-11-06 — beta

**Changed**
- Space saved inside the firmware. Every word, sound and drawing is the same as before.

## V10.6e — 2026-11-06 — beta

**Changed**
- **LOFI is now MATH** (formulas only: BYTE, FLOAT). MATH sounds start still and in tune.
- A new power-on band: DRUM-X ACOUSTIC, ANALOG FAT BASS, PHASE GLASS PAD, FM6 TINE EP, VOICE CHOIR AAH, ANIMAL WHISTLE.
- ANIMAL's tonal voices play the note you press.

**Compatibility notes**
- An old LOFI chip sound (from a project or a user preset) loads as the nearest ANALOG sound.

## V10.6d — 2026-11-06 — beta (includes the V10.6c development builds below)

**New**
- **SWARM**: when nothing is plugged into the INPUT engine and BLUETOOTH AIR is scanning, every device heard around you
  becomes a voice (up to eight). SOURCE picks SWARM or the audio input, and both can play at once on two tracks.
- INPUT is renamed **EXT**.
- GEN is renamed **GENE**, and its page shows a turning double helix.
- From the V10.6c development builds (4–5 November):
  - **SAVE SONG AS**: copy a whole song (A–D and the arrangement) to another project.
  - **FIT / SPAN** (SEQ > LENGTH > RHYTHM): fit any number of steps into a loop length, for example 18 steps against 16.
  - **KEY SPLIT** (GLO > SYSTEM > KEY SPLIT): two tracks on the keyboard and on external MIDI.
  - Generated songs live in a draft until you save them as SONG or LOOP. **ERASE** clears a project or a section.
  - New modulation sources (track envelopes / followers) and clearer modulation destinations.
  - DRUM-X **EXTENDED** key map (one fill per key, chromatic kick and tom, hit FX).
  - VOICE in more languages.
  - USER presets keep VOICE lyrics.

**Removed**
- The developer serial console. The USB port now runs as MENU > USB STEREO.

## V10.6c — 2026-11-03 — **PUBLIC**

**Fixed**
- USB audio input (USB > HOST with an audio interface, or USB IN+OUT) works properly. Two errors in how incoming audio
  was resampled and received were fixed and tested on hardware.

**Changed** (from V10.61, included here)
- The PROX page: KNOB 1 **MIN** and KNOB 2 **MAX** narrow the field, so the phone doesn't have to go far away. KNOB 3
  **SPEED** (1–5) sets how fast PROX responds.

---

## Older versions (summary)

- **V10.47 – V10.61 (2–3 Nov 2026)**: the **INPUT** engine (USB audio in on a track) and the FOLLOW source. USB HOST
  takes audio interfaces (including 48 kHz). Synth STEREO IMAGE with its own page. DLY R TIME. The roll follows the
  playhead. Slow-tempo REPEAT. Master EQ / TAPE no longer reset on their own. A SAFE START for units that stayed dark
  after installing. GEN sounds fixed after the preset changes.
- **V10.20 – V10.46 (1–2 Nov)**: USB HOST for MIDI keyboards. Bluetooth MIDI pairing that works on Windows and Mac.
  Steady MIDI clock out. BLUETOOTH AIR. LFO DRIFT. ENV 2, LP24 filter, ANALOG PLS3 / SAW3 intervals. New DRUM-X kit HIP
  HOP. More distinct factory presets. Pictures on many pages that show what each page does. PROX smoothed.
- **V10.1 – V10.19 (30–31 Oct)**: Bluetooth arrives: BLE-MIDI in and out, and **PROX** (your phone's distance as a
  modulation source). SEQ > EUCLID and MODES replace the first MUTATE. Section ERASE. TIMELINE look and SAVE menu order.
- **V10 (30 Oct)**: new base. TIMELINE, VAR, the ADD engine. M-VAVE's official V15 / V16 firmware can be installed both ways.
- **0.9.x (17–26 Oct)**: STRING and ADD engines, the master 4-band EQ, TAPE, a redesigned song generator, VOICE, clean
  USB stems (and the JIANT Stems app).
- **0.7 – 0.8 (15–16 Oct)**: USB multitrack stems, sequencer and REC improvements, cheat sheets.
- **0.6.x (10–15 Oct)**: six tracks, SLICER, MUTATE, step components, MIDI OUT and PROGRAM CHANGE, dual-core audio,
  INIT SYSTEM (power-on at zero), editable FM6.
- **0.3 – 0.5 (9 Oct)**: DRUM-X FM, punch-in FX, reverb and grain delay, HOME macros, the SELECT dice, the master
  leveler, a filter on every engine.
- **Before 0.3 (2–8 Oct)**: Felucca betas (0.4 – 1.1.5), the web installer and the web editor.
