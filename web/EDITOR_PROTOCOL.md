# Felucca editor protocol (SysEx over USB-MIDI)

The firmware side is `firmware/src/editor.c`. Commands 1-10, track selection, dumps, step reads
and parameter writes (27, 29, 30, 31) were checked on hardware. `tests/editor_test.c` also exercises the real C
handler, malformed transfers and stop timeouts against simulated flash. User-preset
flash writes and live pushes (16-26, 32) have host coverage; that does not establish
their hardware behavior. The step chance byte, motion (64) and the full backup (65-67) have host
coverage only (`tests/editor_test.c`, `tests/backup_test.c`, `web/test_backup.mjs`).

**v3 (tracks):** the device has NTRK tracks (six since JIANT 0.6, four before), each a synth part (since 1.0; before it, track 4
was a GM drum track: see "Track 4 since 1.0" below). One
of them is *selected* (ALGORITHM on the device, or `TRACK`). Every v1 / v2 command acts on the
selected track (its parameters, engine, preset, steps, the user presets it stores or loads); `TRACK`,
`TRACK_MIX`, `TRACK_DUMP` and `TRACK_STEP` reach any track. Command numbers 1-26 are unchanged.

**v4 (any track's parameters):** `TRACK_PARAM` gets or sets a parameter of any track without changing
the selection, and the `TRACK_CHANGED` push follows level, pan and mute of the tracks that are not
selected. The v1-v3 commands are byte for byte as before; v4 is asked for with bit 1 of `WATCH`.

**v5 (the drum grid):** a step also holds lane hits and their accents (see "v5: the drum grid" below).
`STEP_GET` / `STEP_SET` / `TRACK_STEP` replies get 3 more bytes, `STEP_SET` / `TRACK_STEP` take them
optionally, and `UP_GET` / `UP_PUT` carry a user preset's 16-step grid after its pattern. Every
earlier byte is where it was; an editor that reads the first bytes of a reply keeps working.

**v7 (motion, chance, backup, 89 parameters):** a step gets a chance byte after its hits. `MOTION` (64) reads
and edits a track's recorded knob moves. `BACKUP_LIST` / `BACKUP_GET` / `BACKUP_PUT` (65-67) read and
restore the whole device. P_COUNT is 89 and P_E0 is 81: twenty FM operator parameters (ids 61..80) went in
before the engine parameters, which moved from 61..68 to 81..88. Always take P_E0 from `INFO`. INFO ends with
tagged capability blocks for these (see `INFO`).

**MENU settings (1.0.4):** `MENU_DESC` (72) and `MENU_SET` (73) read and set the device's MENU settings, described
by the device (see "MENU settings" at the end); INFO advertises them with `4E 01 count`. 1.0.5: a `MENU_DESC` reply
ends with the item's tab on the device (index and name), after the bytes 1.0.4 sent.

**Ratchet (1.0.5, INFO `52 01 04`):** a step gets a ratchet byte (its hits, 1..4) after its chance: one more byte at
the end of the step replies, and an optional last byte of a step write (see "Ratchet" below). Nothing else changed.

**DRUM lane modes (JIANT 0.8.5, INFO `4F 01 07`):** a step gets 8 more bytes after its ratchet, each DRUM lane's mode
0..6 (see "DRUM lane modes" below). Nothing else changed.

**DJ FILTER (JIANT 0.9.1):** global 17 (was `NAME`, `--`, 0..0, never set) is the master DJ FILTER, `DJ FILT`, -64..63
(0 out of the chain, below a low-pass, above a high-pass; GLO > MIXER KNOB 4 on the device). G_COUNT is unchanged; a
`SET` of it while the selected track records is recorded as the knob is. Its automation records are `MOTION` records
of id 127 (past every track parameter; automation only: op 5, a lock, is refused with 1), listed by the query as any
other. Firmware before 0.9.1 shows global 17 as `NAME`.

**MICRO in 64ths (JIANT 0.9.1):** a step's place off the grid is -32..+31 64ths of a step (was -8..+7 sixteenths).
The step bytes of `STEP_GET` / `STEP_SET` / `TRACK_STEP` do not carry it (as before): a write keeps the step's MICRO,
and the flags byte stays accent | slide. Backups and projects carry it (FUNC block `M` `F`).

**Parameter locks (1.1, INFO `4C 01 01`):** a motion record can be a lock: its value sounds on its step only and
goes back after it. `MOTION` (64) gets ops 5..7 (set a lock, clear a step's locks, the query with the kinds); the
query and ops 1..4 reply byte for byte as before (see "Parameter locks" below). A lock takes no new bytes in a
project (bit 7 of a motion record's id); 1.1 projects are FUN9 for the DRUM lane levels below.

**Chord keys (91 parameters, 1.0):** two track parameters, `CHRD` (81) and `VOIC` (82), went in before the
engine parameters, which moved from 81..88 to 83..90: P_COUNT 91, P_E0 83. No command changed; an editor that
takes P_COUNT and P_E0 from `INFO` keeps working (see the table below: in JIANT they are SHIFT's OFS / PIT).

**DRUM lane levels (99 parameters, 1.1):** eight track parameters, `KICK` .. `BELL` (83..90, the DRUM engine's lane
levels), went in before the engine parameters, which moved from 83..90 to 91..98: P_COUNT 99, P_E0 91. No command
changed; projects were FUN9 (3648 bytes, see "Projects (FUN9)"; JIANT writes FUNA). See "The DRUM lane levels" below.

**JIANT 0.3: SAMPLE (4), GRAIN (8) and SLICE (13) retired** (JIANT 0.9.9-snd5: 8 and 13 hold STRING and ADD again, named so). Their numbers stay, named `-` in `INFO` and `G_ENGSEL`, with no
presets (as PHYS, 9); a sound of theirs that arrives (a project, a user preset, `PRESET`, a `SET` of `G_ENGSEL`, a backup) plays as
ANALOG's first preset, a SAMPLE PERC sound (SET 4) as DRUM's kit. The factory samples left the firmware.

**JIANT FUNB (120 parameters, 32 globals):** 21 track parameters went in before the engine parameters, which moved from
91..98 to 112..119: P_COUNT 120, P_E0 112; JIANT 0.5 adds the FILTER (112 TYPE, 113 CUT, 114 RES): P_COUNT 123, P_E0 115. Five globals were added after the 27 a project's header holds: G_COUNT 32.
No command changed; an editor that takes P_COUNT, G_COUNT and P_E0 from `INFO` keeps working.

| id | label | values |
| --- | --- | --- |
| 91 | TYPE | DIST's type, enum: 0 SOFT (the drive as before), 1 HARD, 2 FOLD (wavefolder), 3 CRUSH (bits and a held sample), 4 RECT (full-wave rectifier, an octave up) |
| 92 | TONE | DIST's tone, −64..63 (fmt BIPCT): darker / brighter |
| 93 | LEN | the modulation sequence's length, 1..16 steps |
| 94 | DIV | its step rate, the `N_DIV` enum (default 1/16) |
| 95 | SLEW | its glide between steps, 0..127 (0: steps) |
| 96..111 | LVL | its 16 levels, 0..127; the matrix reads the step playing as the source `STEP` (SRC 13) |
| G 27 | HPF | the delay's feedback low cut, 0..127 (off .. ~1.2 kHz; COLR is its high cut) |
| G 28 | WIDE | the buses' width, 0..127: the delay's right echo up to 1/32 later, the chorus's second tap opposite |
| G 29 | MOD | the reverb's modulation depth, 0..127 (ROOM: its combs' lengths drift; SPRING: its wobble deeper) |
| G 30 | RATE | its rate (fmt LFOHZ, default 30) |
| G 31 | PRE | the reverb's pre-delay, 0..100 ms |
| G 32 | FILT | (0.3) the reverb's tone, −64..63 (fmt BIPCT): below 0 a low-pass into it (darker), above a low cut (thinner) |
| G 33 | WIDE | (0.3) the reverb's width, 0..127: ROOM's combs 1, 3 against 2, 4, SPRING's taps, as a side |
| G 34 | PITCH | (0.3) the GRAIN delay, −12..12 semitones (fmt SEMI): each repeat through two grains this far up / down |
| G 35 | SPRY | (0.3) its spray, 0..127: each grain from a random place up to ~190 ms back (PITCH and SPRY 0: the plain delay) |

| G 36 | STRN | (0.4) every sequence (not a kit) this many steps up / down its scale, −14..14; ROOT and SCALE are one for every
melodic track: set on one, the others follow |

Track parameter 8 (once ENV DEST FX, read by nothing) is **LOOP** since 0.4: OFF / ON (enum); ON, a held note's envelope
starts its attack again at the sustain.

**JIANT 0.4 engines:** TRIO (6) is folded into ANALOG: `INFO` names it `-`, a TRIO sound that arrives loads as ANALOG's
nearest (core.h `analog_from_trio`). ANALOG's WAVE gains SYNC (5), RING (6), SAW3 (7); LOFI's WAVE gains BYTE (5:
bytebeat, DUTY the formula, CRSH its variable); PHASE's E8 (once `-`, 0..0) is FB, 0..127.

**JIANT 0.4 delay:** G 4 TIME's enum is the ten divisions, then 48 free times (values 10..57: 5 ms .. 1.48 s, the
names say so); G 6 (once COLR) is TONE, 0..127: below 64 a low-pass, 64 open, above a low cut; G 27 (HPF) is stored and
no longer read. **DRUM-X kit mode byte:** bits 5..6 the pitch modulation (0 DECAY, 1 LONG, 2 NOISE, 3 SINE), bit 7
DRIVE (firmware before 0.4 refuses a kit with them set).

**JIANT 0.4 E values:** LOFI {CHIP WAVE DUTY CRSH CUT VIB BEND MASK} (CUT BEND MASK were SWP ARP TONE); ANALOG's E8
is INT, osc 2's interval −24..24 (was KTR). A project (kit marker below 3, or an older format) or a user preset (no 0.4
mark) stored before loads LOFI's TONE as CUT, BEND MASK 0, and ANALOG's KTR as INT 0. VOICE 2 (ALLOC DETUNE PAN MUTE)
is no page any more: PAN and MUTE on MIXER, the others at their stored values.

**JIANT 0.6.3: 66 globals**: G 37..65 the SLICER bus (37 SLCR, 38 PAT, 39 RATE, 40 DEPTH, 41..46 T1..T6 and 47 FX ON/OFF, 48 LEN, 49 QNT, 50..65 the pitch steps, -24..24); the track parameters P_SLCR..P_SLDEPTH stay, unused. **JIANT 0.4: 37 globals** (G 36 STRN, stored behind a 3). **JIANT 0.3: 36 globals.** G_COUNT 36 (G 32..35 above). DRUM's E6 (id P_E0 + 5) is FM, 0..127 (once ACC): its sounds through
harmonic FM, eight ratio bands; a project or user preset saved before loads its DRUM tracks with FM 0.

Projects are FUNB (`46 55 4E 42`, 3840 bytes: FUNA's layout, 96 bytes longer, so everything after the data moved up by
96; the new globals as biased bytes after the punch-in lane's tracks and the macros, behind a 1 for the first five or,
since 0.3, a 2 for all nine). FUNA (3744, 99
parameters) and every older format load mapped by count, the new values at their defaults.

## Stability contract (1.0)

From V10.6z3 on (the first firmware after V10.6z2) the editor protocol is a **versioned contract** for the programs
built on it (the web editor and installer, the JIANT Stems app, anyone's). `INFO` says the version with the tag
`43 02 major minor` (1.0: `43 02 01 00`), placed right before the last tag (`59`, LYRIC, which the web editor reads
as the last three bytes). No tag: firmware before the contract (V10.6z2 and older), which speaks everything below
except what is marked 1.0.

**The rule: only additive changes.** Each addition raises the minor version (1.1, 1.2, ..) and is listed in the
history below; the major stays 1 (a 2 would be a break, and none is planned). An old client keeps working against a
newer firmware, byte for byte; a new client tells the additions apart by the version tag (or by the capability
tag of the addition, as before).

**Frozen** (never renumbered, re-laid out, re-meant or removed):

- **Framing**: `F0 7D 46 4C cmd args F7`, one reply per request with the same `cmd` (no reply: an unknown command or
  a wrong fixed length), pushes never replies; the encodings v14, u32, pack7, strings.
- **Command numbers** 1..10, 16..42, 64..77, 80 and (1.1) 81 as listed in this file; 11..15 stay reserved (no reply).
- **Reply layouts**: every byte stays where it is. New bytes only at the end of a reply, behind a request byte that
  asks for them (as `STEP_GET`'s flag 1, `WATCH`'s bits), or as a new `INFO` tag before `59`.
- **INFO's prefix**: version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, NENGINES engine names, NTRK,
  CHAIN_ROWS, then the tags (each tag's number and meaning). The JIANT Stems app reads the string, NENGINES, the fifth
  byte after it (P_E0) and the names, and ignores the rest.
- **The engine byte**: 0 ANALOG, 1 reserved, 2 PHASE, 3 LOFI (named `MATH` by INFO since V10.6d), 4 reserved,
  5 VOICE, 6 reserved, 7 WHEEL, 8 STRING, 9 reserved, 10 DRUM, 11 NOISE, 12 FM6, 13 ADD, 14 ANIMAL, 15 INPUT (named
  `EXT`). New engines take the next numbers. The *names* are display text and may change: a client shows the names
  `INFO` sends, not a table of its own.
- **Parameter ids**: the track parameters 0..P_E0−1 keep their id and the slot keeps its meaning for every engine
  that reads it (a range may widen, a label change: read `DESC`); E1..E8 are always the last eight, P_E0..P_COUNT−1
  (read P_E0 and P_COUNT from `INFO`: new common parameters go just before P_E0, which then grows). Globals keep
  their ids; new ones are appended (G_COUNT grows). Enum values keep their numbers (new ones appended).
- **Pushes**: `CHANGED` 23, `RELOAD` 24, `STEP_CHANGED` 26, `TRACK_CHANGED` 32 and the 1.0 pushes 40..42, each sent
  only for the `WATCH` bit that asks for it: a client that sends `WATCH 3` gets exactly what it got before 1.0.
- **MENU ids** (`MENU_SET` takes the id: append-only), the backup protocol (65..67: object ids, u32 / pack7,
  rc codes; the *count* of objects is the `BACKUP_LIST`'s: 35 since V10.6p), the `PUNCH` bit numbers, the NRPN map
  (`docs/NRPN.md`: CC99 0 / 1, CC98 the id, the 14-bit scaling) and the fixed CCs (MANUAL, "MIDI and USB").

**May change** (read it from the device): the values of P_COUNT, P_E0, G_COUNT, the menu count, the backup object
count; labels, units, ranges (`DESC`), the engines' display names and order; new commands, tags, pushes, engines,
parameters, enum values, menu items, at the end of what there is.

**Enforced** by `tests/app_contract_test.c` (CI, `.github/workflows/voice-user-check.yml`): the requests the JIANT
Stems app sends (its `Fm1Link` and `Fm1Archive` bytes, cut into USB-MIDI packets as it cuts them) against golden
descriptions of the replies, byte for byte; the engine table; the pushes' opt-in; NRPN both ways; `docs/NRPN.md`
regenerated from `params.c` and compared.

| version | firmware | changes |
| --- | --- | --- |
| 0.x (no tag) | Felucca 1.0 .. JIANT V10.6z2 | v1 .. v7 and the tagged additions described in this file |
| 1.0 | V10.6z3 | the contract; `INFO` tag `43 02 01 00`; `WATCH` bits 2..4 and the pushes `TRANSPORT` (40), `PUNCH` (41), `MUTATE` (42); NRPN for every automatable parameter and the globals (`docs/NRPN.md`), sent for knob moves with MIDI OUT ALL |
| 1.1 | V10.6z5 | `VEL` (81): the velocity diagnostic (below); `INFO` tag `43 02 01 01` |
| 1.2 | V10.6z7 | `VEL` ops 4 and 5: the keys' velocity, calibration and pressure (below); MENU ids 27..30 (VELOCITY, VEL MIN, AFTERTOUCH, VEL LEARN); `INFO` tag `43 02 01 02` |

## 1.0: performance pushes (40-42)

Asked for with `WATCH` bits 2..4 (`WATCH 31`: everything; 1.0 firmware answers 31, older 3). Each bit asked for anew
sends the state as it is first (`TRANSPORT`, `PUNCH`; `MUTATE` counts from then on). They need no MIDI OUT setting.

- **TRANSPORT** (40): `playing, beat, bar lo, bar hi`. Sent on PLAY and STOP and at each beat while playing (the
  beat of the bar 0..3; the bar counts from PLAY, 14 bits). With MIDI OUT CLOCK the realtime messages say the same;
  this needs neither the setting nor a clock reader.
- **PUNCH** (41): `m0 m1 m2 m3 tracks`: the punch-in / FX-layer effects running (held, latched, a scene's, the
  punch-in lane's step) as `m0 | m1 << 7 | m2 << 14 | m3 << 21`; 0 while a menu or dialog turns them off. Bits
  (frozen): 0 REPEAT 1/8, 1 REPEAT 1/16, 2 REPEAT 1/32, 3 LPF, 4 HPF, 5 OCT−, 6 OCT+, 7 1/2 TEMPO, 8 DEC−, 9 DEC+,
  10 STUTTER 1/8, 11 STUTTER 1/16, 12 ATK+, 13 ARP, 14 RANDOM, 15..20 the mutes of tracks 1..6 (black keys), 21
  GLITCH, 22 LOOP 3, 23 LOOP 5, 24 LOOP 7. `tracks`: the MIDI effects' tracks, 0 ALL, 1 SYN, 2 DRM.
- **MUTATE** (42): `track, count`: a EUCLID MUTATE of that track (EVERY's, or OCT+ on the MUTATE page); count is
  the track's MUTATEs since power-on, 7 bits, so a missed frame shows as a jump.

## 1.1: the velocity diagnostic (81)

`VEL` turns the key sensing of the velocity diagnostic on and off and reads what it measured
(`firmware/src/vel_diag.c`, `docs/VELOCITY_DIAG.es.md`; the page `web/vel.html` uses it). It never changes the sound
or how the keys play; it is off at power-on. Request: `op [drive]`; a firmware before 1.1 does not answer.

| op | request | what it does |
| --- | --- | --- |
| 0 | `0` | status only |
| 1 | `1 drive` | on; `drive` 0..4 (0: no driven samples; 2 the default, Velo's DRIVE 2); again while on: the drive only |
| 2 | `2` | off (the scan lets go at its next tick) |
| 3 | `3` | the next press or capture measured, taken (read until `kind` 0) |

Reply: `op, on (0/1), drive, lost v14, timeouts v14, master v14` (presses or captures nobody read in time; ADC
conversions not done by the next tick; the MASTER pot as the scan reads it, −1 none yet), then for op 3 `kind`:

- 0: nothing to read;
- 1, a press (sent when the key is let go): `key, dt v14, pmin v14, dmin v14`: the key 0..19 (0 F3 .. 19 C5: the
  rows PA7 / PA8, the top row has no converter), touch to seated in 1/10 ms (−1: a graze, never seated), the lowest
  reading with the row pulled up and the lowest with it driven high during the press (0..1023, 1023 open, lower
  pressed harder; −1: none);
- 2, a capture (the first key touched while none runs): `key, count (56), count × v14`: 8 pulled-up readings before
  the touch, then 48 from it (~2.2 ms apart), each `reading | 1024` when it was a driven one.

From 1.2 (V10.6z7) the key sensing also runs whenever MENU > CONTROL > VELOCITY or AFTERTOUCH is not OFF (the
defaults: NORMAL, MID), the diagnostic or not: `on` in the reply says whether the *diagnostic* is on (its presses
and captures queued), `drive` is the drive the scan uses (2 unless the diagnostic asked for another; the velocity's
calibration belongs to drive 2 and learns only there). Op 2 turns the diagnostic off; the sensing stays on for the
velocity.

## 1.2: the keys' velocity (81, ops 4 and 5)

| op | request | what it does |
| --- | --- | --- |
| 4 | `4` | the keys' velocity, calibration and pressure now (a live view: poll it, ~20 a second is fine) |
| 5 | `5` | the calibration back to its seeds (as MENU > CONTROL > VEL LEARN RESET), saved; then as op 4 |

Reply: the common part (`op, on, drive, lost v14, timeouts v14, master v14`), then (only on a firmware whose
Bluetooth segment is installed: the code is there; otherwise the reply ends here):

- `curve` (VELOCITY: 0 OFF, 1 SOFT, 2 NORMAL, 3 HARD), `min` (VEL MIN, 1..80), `at` (AFTERTOUCH: 0 OFF, 1 LOW,
  2 MID, 3 HIGH), `learn` (0/1), `mean` (the mean of the recent velocities, what the top row C#5..G5 plays: it has no
  converter), `strikes` (a count of the strikes measured, 7 bits: a new one when it changes), `last` (the key of the
  last strike, 0..19);
- then 20 × (`vel, reading v14, hard v14, normal v14, soft v14, pressure`), keys 0..19 (F3..C5): the key's last
  velocity (1..127, 0 none yet), the driven reading it came from (lower = harder), the key's calibration (its HARD,
  NORMAL and SOFT references, 0..1023: the velocity maps the reading through them, 112 / 76 / 36 before VELOCITY's
  curve and VEL MIN), and its pressure now (0..127, 0 when not held).

The velocity is continuous: piecewise linear through the three references, half the slope past either end, clamped
1..127; SOFT `v + v (127 - v) / 254`, HARD `v - v (127 - v) / 254`; VEL MIN scales 1..127 onto MIN..127.

## NRPN (1.0)

Every parameter automation records, and the globals but the transport, routing, loads and engine switch, are set by
NRPN: CC99 0 (a track's, on its channel) or 1 (a global), CC98 the id, CC6 / CC38 the value (14 bits over the
range `DESC` gives; CC6 alone 7 bits). With MIDI OUT ALL the panel's knob moves go out the same way. The table,
generated from `firmware/src/params.c`: `docs/NRPN.md` (and `web/NRPN.md`).

## Framing

A request is `F0 7D 46 4C <cmd> <args...> F7`:

- `7D` is the non-commercial SysEx ID.
- `46 4C` is "FL".

Every valid request gets exactly one reply, with the same header and the same `<cmd>`.
Unknown commands and invalid fixed argument lengths get no reply; argument errors
follow the command-specific rules below.
Fixed-size commands require exactly the lengths in the tables; step writes accept the
8-byte legacy step, the 11-byte grid step, the 12-byte step with its chance (a chance above 100 gets
no reply) or the 13-byte step with its ratchet (a ratchet outside 1..4 gets no reply). Every data byte is 7 bit.
MIDI realtime bytes may occur inside SysEx; another status aborts the partial frame. While the editor
watches (v2, `WATCH`), the device also sends push frames (cmds 23, 24, 26) at any time.

| Item | Encoding |
| --- | --- |
| value (v14) | 2 bytes, LSB first, holding value + 8192, so the range is -8192..8191. `[lo, hi]`: value = (lo \| hi << 7) − 8192 |
| u32 (v7) | 5 bytes, 7 bits each, LSB first; the last byte is 0..15. Used by the backup commands |
| string | ASCII bytes, ended by a 0 byte |
| scope | 0 = parameter of the selected track (`P_*`, 0..P_COUNT−1); 1 = global parameter (`G_*`, 0..G_COUNT−1) |
| track | 0..NTRK-1: tracks 1..6 (synth parts; JIANT 0.6: six, INFO's NTRK; before, four) |
| engine byte | 0..NENGINES−1 (firmware before 1.0: NENGINES = its drum track, no engine). The numbers are fixed, new engines are appended: 0 ANALOG, 1 reserved (DIGITAL before 1.0: see below), 2 PHASE, 3 LOFI, 4 SAMPLE, 5 VOICE, 6 TRIO, 7 WHEEL, 8 GRAIN, 9 reserved (PHYS until FELUCCA TONIC: its sounds load as ANALOG's first preset), 10 DRUM, 11 NOISE, 12 FM6, 13 SLICE (retired in JIANT), 14 ANIMAL (JIANT 0.8.3; NENGINES 15, before it 14: an engine byte of 14 from a 0.8.3 store on older firmware is refused as any number past its last; a user-slot favourite (`FAV_GET` / `FAV_SET`, engine = NENGINES) is engine 15 from 0.8.3 on, read NENGINES from `INFO`); JIANT 0.9.9-snd5 puts STRING in GRAIN's 8 and ADD in SLICE's 13 (both retired; NENGINES stays 15: a GRAIN or SLICE sound of an old store plays as STRING / ADD there). V10.54 appends 15 INPUT (NENGINES 16: the user-slot favourite is engine 16 from then on, read NENGINES from `INFO`): SOURCE (0 IN 1, 1 IN 2, 2 1+2, 3 STEREO), GAIN (64 = 1.0), GATE, PLAY (0 THRU, 1 KEYS), WIDTH, RING, BITS, RATE. The device and the editor list them in another order (JIANT 0.9.9: DRUM VOICE ANIMAL LOFI ANALOG FM6 PHASE WHEEL NOISE, snd5: then STRING ADD, DIGITAL last in FELUCCA_FM4 builds: `ENGINE_ORDER`; 0.8.3 .. 0.9.8: ANALOG FM6 PHASE LOFI VOICE WHEEL NOISE DRUM ANIMAL, the order PROGRAM CHANGE still numbers the sounds in); the numbers stay. ANIMAL's EDIT values: MODE (0 CMOS, 1 PULSE, 2 WHSTL, 3 WING, 4 GLOTT), PRESS, CHAOS, SIZE, SWEEP (−64..63), RATE (the LFO's Hz scale), GROUP (0..15: HOLD TRILL 1+1 1+3 2+2 2+6 3+3 3+9 4+4 4+12 6+6 8+8 8+24 12+12 16+16 32+32), TEMP (0..40 °C); the device relabels some per MODE (`DESC` sends these, the engine's own). STRING's: EXCITE (0 NOISE, 1 PLUCK, 2 PICK, 3 HAMMR, 4 STICK), DECAY, BRIGHT, POS, STRETCH, BODY, SIZE, BOW (0..127). ADD's: MODE (0 HARM, 1 ODD, 2 WIDE, 3 BELL, 4 BAR, 5 DRUM), BRIGHT, EVEN (−64..63), STRETCH (−64..63), PEAK, FORMNT, DECAY (127: held), DAMP |

The engine parameters are `P_E0..P_E7`: P_COUNT−8 .. P_COUNT−1 (91..98), and `INFO` gives `P_E0`.
Their meaning, range and names depend on the current engine, so re-read `DESC` for them
after an engine change.

The modulation matrix (the MOD page, `firmware/src/mod.c`) is twelve track parameters at ids 49..60 (since
P_COUNT 69): slot k (1..4) is `SRCk`, `DSTk`, `AMTk` at ids 49 + 3 (k − 1) .. 51 + 3 (k − 1).

| id | label | values |
| --- | --- | --- |
| 49, 52, 55, 58 | SRC1..SRC4 | enum: 0 OFF, 1 LFO, 2 ENV, 3 VEL, 4 KEY, 5 RAND, 6 MODW (CC1), 7 AT (channel aftertouch), 8 EXPR (CC11), 9..12 M1..M4 (JIANT: the project's macros, 0..1; LFO held turns them) |
| 50, 53, 56, 59 | DST1..DST4 | enum (22 names): 0 OFF, 1 PITCH, 2 CUT, 3 SHP, 4 AMP (per voice); 5 PAN, 6 DIST, 7 CHO, 8 DLY, 9 REV, 10 RATE (LFO rate), 11 VIB (LFO pitch depth), 12..19 E1..E8 = `P_E0..P_E7` (per block); 20 CLIP, 21 PNCH (JIANT: the master's G_CLIP / G_PUNCH, the slots of every track adding) |
| 51, 54, 57, 60 | AMT1..AMT4 | −64..63 (fmt BIPCT) |

`DESC` names E1..E8 as such; the device shows the engine's label of that parameter instead (`DESC` of
`P_E0 + n`), and so does the editor. A slot with SRC, DST or AMT at 0 does nothing; every default is 0
(a factory preset sets all twelve to 0). The modulation never changes the stored values: `GET`,
`DUMP` and the pushes report what was set.

**DIGITAL (engine 1) was replaced by FM6 (Dexed-based)** in 1.0. The number stays reserved: `INFO` names it
"-" (a build with `FELUCCA_FM4=1` has DIGITAL back, named "DIGITAL"), `NAMES 1` lists no presets, its EDIT
descriptors are "-" with DIGITAL's ranges, and no track ever has it. Whatever brings a DIGITAL sound plays it as
FM6 with a patch converted from its values (`firmware/src/fm4_convert.c`; the patch is the track's own, `FM6_GET`
reads it, a project keeps it): `SET` of `G_ENGSEL` = 1 (DIGITAL's first preset), `PRESET` 1 k (its preset k),
`UP_LOAD` of a slot stored with engine 1 (`UP_PUT` still takes engine 1: the record keeps the DIGITAL values, every
load converts them), projects with DIGITAL tracks (their motion on E1..E8 and ids 61..80 is dropped). The track
reports engine 12 (FM6) and the FM6 preset that covers the sound. The editor converts DIGITAL patches of library
files the same way (it equals the firmware's, `test_web.mjs`).

The four FM operator envelopes (the DIGITAL engine's OP1..OP4 ENV and OP LEVEL pages, `eng_digital.c`) are
twenty track parameters at ids 61..80, five per operator: operator k (1..4) has `ATK`, `DEC`, `SUS`, `REL`,
`LVL` at ids 61 + 5 (k − 1) .. 65 + 5 (k − 1). Since DIGITAL was retired they are inert: no page and no editor
layout shows them, only the conversion of a DIGITAL sound reads them (its FM6 tracks hold the defaults). The ids
and labels stay (library files key values by label).

| id | label | fmt, range, default |
| --- | --- | --- |
| 61, 66, 71, 76 | ATK | TIME, 0..127, 0 |
| 62, 67, 72, 77 | DEC | TIME, 0..127, 0 |
| 63, 68, 73, 78 | SUS | PCT, 0..127, 127 |
| 64, 69, 74, 79 | REL | TIME, 0..127, 0 |
| 65, 70, 75, 80 | LVL | PCT, 0..127, 127 |

The defaults change nothing: the sound is the same as before the parameters existed. The master `ENV`
(ids 1..4) shapes the whole voice; these shape each operator on top of it. Other engines ignore them, and the
device shows the pages for DIGITAL only. Descriptors are the same for every engine.

The chord keys (SCL > CHORD on the device, `firmware/src/chord.c`) are two track parameters at ids 81, 82:

| id | label | values |
| --- | --- | --- |
| 81 | OFS | -32..32 steps (JIANT; the chord keys' CHRD before): the sequence plays OFS steps later, wrapping inside LEN |
| 82 | PIT | -24..24 semitones (JIANT; VOIC before): the sequence's notes move up / down (a kit's lanes do not) |

**JIANT:** the chord keys are gone; their two ids are SEQ > SHIFT, the sequence offset. Live recording writes where
it is heard. They are the track's, like ARP and SCL: a sound load keeps them. Unlike the chord keys, motion records
them (they are automatable). A project written before SHIFT (no mark in its DRUM-X block) loads them at 0.

The DRUM lane levels (EDIT > LANES / LANES 2 on the device, `firmware/src/eng_drum.c`) are eight track parameters at
ids 83..90, one per lane: 83 KICK, 84 SNARE, 85 CLAP, 86 HATCL, 87 HATOP, 88 TOM, 89 RIM, 90 BELL. Percent (`F_PCT`),
0..127, default 127 = 100 % (the kit as designed), square law (64: about −12 dB). Only the DRUM engine reads them; the
editor shows them on a DRUM track only (the device's pages). They are the sound's (not the track's): a factory preset
sets them back to 127, user presets and projects store them (older ones load at 127), motion records them.

Other enums that grew: `P_SDIV` (DIV, id 30) has 10 names (1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1/1, 2BAR,
4BAR; the first six keep their numbers), `P_AMODE` (id 17) 15 (OFF, UP, DN, UPDN, RND, ORD, REPEAT, and since 1.2 DNUP, UP+8, CONV, DIVG, PINKY, THUMB, WALK,
CHORD: values 7..14, appended; an editor that knows only 7 names shows the value by `DESC`'s name) and the
global `G_CLOCK` (id 2, label "CLK") 3 (INT, USB, TRS). `G_MIDI` (id 12) is an enum of USB / TRS that nothing reads.

**Retired enum values (aliases).** A value that no longer exists keeps its number, so stored sounds stay valid:
`DESC` names it like the value it now plays (an alias), the device never holds it (a `SET` of it lands on that value
and the reply says so) and its knob steps over it. Of a repeated name, the original is the first value when that has
the name, else the last one with it; an editor should list only the originals and show an alias by its name when
it ever sees one. Today: SAMPLE SET and GRAIN SRC 1 and 4 (once TRANH, PERC) = 0 PIANO (5..7, once USR1..USR3, clamp
to 4); SLICE SRC 1..3 (once USR1..USR3) = 0 BREAK, named "BREAK". (Felucca's DRUM KIT, engine 10 E1, and its
aliases are gone in JIANT: E1 is MRPH, see "DRUM is DRUM-X".) An editor of before 1.0.5 (first of a name = original) offers 66 10 77
at 1..3 and hides 6 5 8: what it sets still plays the right kit, and the device answers with 6 5 8.

## Commands

| cmd | Request args | Reply args |
| --- | --- | --- |
| 1 INFO | — | version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, then NENGINES engine-name strings, then (v3) NTRK (6 since JIANT 0.6, 4 before), then (v6) CHAIN_ROWS (16), then the tagged blocks `55 01 uiCaps`, `4D 01 64 01`, `42 01 3`, `46 01 nfactory nbank`, `53 01 3`, (1.0.3) `50 01 3` and (1.0.4) `4E 01 count` (MENU settings; 12, 1.1: 15, 1.2: 17, 1.1.5: 18) and (1.0.5) `52 01 4` (ratchet) and (1.1) `4C 01 1` (parameter locks) and (JIANT 0.6.11) `56 01 32` (FM6's voice bank) and (JIANT 0.6.62) `4A 01 1` (JIANT) and (JIANT 0.8.5) `4F 01 7` (DRUM lane modes) (below), (JIANT 0.8.5) `59 01 63` (LYRIC, always the last tag) and, just before it, (1.0) `43 02 major minor`: the protocol's version (see "Stability contract (1.0)"); older firmware ends earlier |
| 2 GET | scope, id | scope, id, v14 |
| 3 SET | scope, id, v14 | scope, id, v14 (the value after clamping). Setting global `G_ENGSEL` (id from DESC label "ENG") changes the engine: its defaults, then its first preset (as on the device) |
| 4 DUMP | — | engine, preset, then P_COUNT × v14 (the selected track), then G_COUNT × v14 (globals) |
| 5 DESC | scope, id | scope, id, fmt, min v14, max v14, def v14, label string, unit string, then for an enum (fmt 8) one name string per value (at most 32 since 0.7.0; 24 before it, 16 before the matrix) |
| 6 STEP_GET | index 0..NSTEP−1 | index, n (0..4 notes), note0..note3, time (0 NOTE, 1 TIE, 2 REST), flags (1 accent, 2 slide), vel, then (v5) hits (3 bytes, below), then (v7) chance 0..100, then (`52 01`) ratchet 1..4, then (`4F 01`) 8 lane modes 0..6 |
| 7 STEP_SET | index, n, note0..3, time, flags, vel [, hits (3 bytes, v5) [, chance 0..100 (v7) [, ratchet 1..4 [, 8 lane modes 0..6 (`4F 01`)]]]] | same as STEP_GET (after the write). Without the hits the step keeps its own; without the chance, the ratchet or the lane modes it keeps its own. The chance can only follow the hits, the ratchet the chance, the lane modes the ratchet |
| 8 PRESET | engine, preset | engine, preset (applies the preset's sound to the selected track and sends; the steps and the track's own parameters stay, see "Sound loads and undo"). For another track, select it with `TRACK` first |
| 9 PROJECT | op (0 load, 1 save, 2 query), slot 0..3 | op, slot, used (1/0). Save writes flash: allow ~2 s; it stops the transport first (see "Saves while playing") |
| 10 NAMES | engine | engine, count, count preset-name strings, then the two edit-page titles |
| 11..15 | — | retired (Felucca's user sample upload SMP_BEGIN / SMP_WRITE / SMP_END / SMP_ERASE / SMP_INFO; JIANT has no user sample slots): reserved, no reply |
| 16 UP_LIST | start, count (1..16) | start, count, total slots, then per slot: used (0/1), engine, name string ("" if unused) |
| 17 UP_GET | slot | slot, used, engine, name, P_COUNT × v14, 16 × (note, flags), then (v5) kind (0 a note pattern, 1 a drum grid) and for kind 1 16 × hi |
| 18 UP_PUT | slot, engine, name, P_COUNT × v14, 16 × (note, flags) [, kind 0 or 1, 16 × hi (v5)] | slot, rc (0 ok, 1 args, 2 flash). Writes flash: allow 1 s |
| 19 UP_STORE | slot, name | slot, rc. Stores the current sound: engine, parameters, the first 16 sequencer steps as the pattern (TIE steps → flag 4) |
| 20 UP_LOAD | slot | slot, rc (0 ok, 1 empty/invalid). Applies its sound (not its pattern; the steps stay) |
| 21 UP_ERASE | slot | slot, rc |
| 22 WATCH | on (0/1; v4: 3 = also `TRACK_CHANGED`; 1.0: bit 2 `TRANSPORT`, bit 3 `PUNCH`, bit 4 `MUTATE`) | the bits it took: on (bit 0), bit 1 (v4 firmware), bits 2..4 (1.0 firmware; older answers 3 or 1). While on, the device pushes cmds 23, 24, 26 (and 32 with bit 1, 40 / 41 / 42 with bits 2 / 3 / 4) |
| 23 CHANGED (push) | — | scope, id, v14 |
| 24 RELOAD (push) | — | engine, preset, then (v3) the selected track |
| 25 PING | — | 0 |
| 26 STEP_CHANGED (push) | — | index, then (v3) the selected track |

| cmd (v3) | Request args | Reply args |
| --- | --- | --- |
| 27 TRACK | — (query), or track (select it) | selected track, NTRK, then per track: engine byte, preset, level v14, mute (0/1), armed (0/1, live recording) |
| 28 TRACK_MIX | track (get), or track, level v14 (0..127), mute (set) | track, level v14, mute: the track's `P_LEVEL` and `P_MUTE` |
| 29 TRACK_DUMP | track | track, engine byte, preset, P_COUNT × v14 (that track's parameters; no globals) |
| 30 TRACK_STEP | track, index (get), or track, index, n, note0..3, time, flags, vel [, hits (v5) [, chance (v7) [, ratchet [, 8 lane modes]]]] (set) | track, index, n, note0..3, time, flags, vel, then (v5) hits, then (v7) chance, then (`52 01`) ratchet, then (`4F 01`) 8 lane modes |

| cmd (v4) | Request args | Reply args |
| --- | --- | --- |
| 31 TRACK_PARAM | track, id (get), or track, id, v14 (set); id = `P_*` (0..P_COUNT−1) | track, id, v14 (the value after clamping, as `SET`). The selection does not change; no push about the editor's own write |
| 32 TRACK_CHANGED (push) | — | track, id, v14: `P_LEVEL`, `P_PAN` or `P_MUTE` of a track that is not selected changed on the device (only while `WATCH` was sent with bit 1) |

| cmd (1.0) | Request args | Reply args |
| --- | --- | --- |
| 40 TRANSPORT (push) | — | playing (0/1), beat of the bar (0..3), bar lo, bar hi (the bars since PLAY, 14 bits): on PLAY / STOP and each beat while playing (only while `WATCH` was sent with bit 2) |
| 41 PUNCH (push) | — | the effects running, 4 × 7 bits LSB first (the bit numbers in "1.0: performance pushes"), then their tracks (0 ALL, 1 SYN, 2 DRM): on every change (only with `WATCH` bit 3) |
| 42 MUTATE (push) | — | track, its MUTATE count (7 bits, wrapping): a EUCLID MUTATE of that track (only with `WATCH` bit 4) |

| cmd (v7) | Request args | Reply args |
| --- | --- | --- |
| 64 MOTION | track (query); track, 1, on 0/1 (play on / off); track, 2 (clear); track, 3, step, id, v14 (set an event); track, 4, step, id (delete an event or a lock); (1.1) track, 5, step, id, v14 (set a lock); track, 6, step (clear the step's locks; step 127: every step's); track, 7 (query with the kinds) | track, rc, on (0/1), count (this track's events and locks), max (64), then count × (step, id, v14); after ops 5..7 (1.1) then count × kind (0 automation, 1 lock), in the same order |
| 65 BACKUP_LIST | — | 1, rc, count (35 since V10.6p, 6 songs; 43 with 8 songs; 10 before JIANT 0.3), then per object: id, size u32, crc u32 |
| 66 BACKUP_GET | id, offset u32, count lo, count hi (≤ 256) | id, rc, offset u32, count lo, count hi, pack7 data |
| 67 BACKUP_PUT | op 0 begin: 0, id, size u32, crc u32; op 1 data: 1, id, offset u32, pack7 data; op 2 commit: 2, id; op 3 abort: 3, id | op, id, rc |

**pack7:** groups of up to 7 bytes, each preceded by one byte holding their top bits
(bit j = bit 7 of byte j). A mask must have at least one following data byte;
unused mask bits in the last group must be zero. Oversized or incomplete transfers
are rejected; a `BACKUP_PUT` data piece decodes at most 256 bytes.

`fmt` values (`firmware/src/core.h`):

| Value | Name | Value | Name | Value | Name |
| --- | --- | --- | --- | --- | --- |
| 0 | INT | 5 | CUTOFF | 10 | NOTE |
| 1 | PCT | 6 | DB | 11 | ONOFF |
| 2 | BIPCT | 7 | SEMI | 12 | OCT |
| 3 | TIME | 8 | ENUM | 13 | STEPS |
| 4 | LFOHZ | 9 | BPM | | |

The editor should show the value with the unit; formatting it exactly like the device does
is not required.

## v2: user presets

A user preset = engine (0..NENGINES−1), name (1..12 chars, ASCII 32..126; the device shows it upper
case), all P_COUNT instrument parameters (v14 each, the same order as `DUMP`), and a 16-step pattern:
16 × (note 0..127 (0 = rest), flags: 1 accent, 2 slide, 4 tie, 8 | 16 the ratchet − 1 (firmware with `52 01`;
before, 0: x1, and such firmware drops those bits when it loads the pattern)). Loading one applies the engine and
the parameters of the sound, as a factory preset (the track's own parameters, the steps and LEN stay; see
"Sound loads and undo"). The stored pattern is kept and returned by `UP_GET`; on the device SEQ > PHRASES
lists it as "U07" and loads it, with the stored LEN (at most 16), DIV, SWING and GATE. The slots are
numbered 0..31 (the device shows U01..U32).

- `UP_LIST`: count is cut at 16 and at the last slot (start ≥ 32: count 0, no entries).
- `UP_GET` of an empty slot has the same shape with used 0, engine 0, name "" and all values 0.
  Values come back in the current parameter order, inside their ranges.
- `UP_PUT`: rc 1 for a slot ≥ 32, an engine ≥ NENGINES, a name that is empty, longer than 12 or has
  bytes outside 32..126, or an incomplete or oversized frame. Values are clamped to their ranges for that
  engine. A note with flag 4 is stored as a tie (note 0); flags on a rest are dropped.
- **DRUM** (engine 10) was PHYS's MODEL 4 (DRUM) before 1.0. A record of PHYS with E1 (MODEL) = 4,
  stored then or sent by `UP_PUT` from an older editor, is the DRUM engine: the device rewrites it (engine
  10; E1..E8 {MODEL, TUNE, TONE, DECY, SNAP, ACC, KICK, PERC} become {MRPH 64, TUNE, TONE, DECY, NOIS 64, FM 0
  (ACC until 0.3), 0, DRV 0}), and `UP_GET` / `UP_LIST` give it so.
  Projects do the same. PHYS's MODEL is 0..3 (MODAL STRNG MEMB SYMP) now.
- **SAMPLE SET 4** was PERC, the General MIDI drum kit, until 1.0.2. A record of SAMPLE (engine 4) with E1 (SET)
  = 4, stored then or sent by `UP_PUT`, is the DRUM engine with its default kit: the device rewrites it (engine 10,
  E1..E8 = the DRUM-X preset's {64, 64, 70, 64, 64, 100, 0, 0}, the other values and the pattern as they were). Projects
  and `PRESET` 4 / 4 do the same (a project's track keeps its steps). SET 4 itself stays (`DESC` names it "PIANO":
  an alias, as SET 1; a `SET` of 4 lands on 0); GRAIN's SRC 4 plays PIANO. JIANT has no user sample slots: SET and
  GRAIN SRC end at 4, and a stored 5..7 (once USR1..USR3) clamps to 4 (PIANO).
- **SLICE SRC 1..3** were USR1..USR3; JIANT has no user samples, so they are aliases of 0 (BREAK, named "BREAK";
  a `SET` of 1..3 lands on 0), and 4 stays PIANO. SLICE DIV is 0..4 (4, 8, 16, 32, AUTO): MAN (5, Felucca's slices
  set by hand on the EDIT > SLICES page for the user slots) is gone with that page; a stored 5 plays AUTO.
- **DRUM is DRUM-X (JIANT 0.1).** Felucca's kits (KIT STD 66 10 77 80 10 66 55 77, and 1..3 once HAND CYM H+CYM)
  are retired. E1..E8 of DRUM are {MRPH (KIT's slot: the morph A..B of every sound), TUNE, TONE, DECY, NOIS (SNAP's
  slot: moves every sound's noise, 64 = as the kit has it), FM (ACC until JIANT 0.3; a DRUM record stored before loads
  with FM 0), FOLD (KICK's slot; was WARP), DRV}. A stored KIT
  value plays as that MRPH; the kit's sounds are the section's DRUM-X kit (see "Projects (FUNA)").
- `UP_STORE`: name "" stores with the automatic name the device uses (engine name + slot number,
  "ANALOG 07"). rc 1 for a bad slot or name.
- rc 2 = the flash write failed or there is no flash. A failed flash write keeps the previous
  record in RAM and flash, including its name. Without flash, a valid change is RAM-only until power-off.
  (`UP_PUT` / `UP_STORE` / `UP_ERASE` stop the transport first, see "Saves while playing"; a device
  that cannot stop it in 100 ms answers rc 2 and writes nothing.)
- Frames stay below 640 bytes (`UP_PUT` is 5 + 1 + 1 + 13 + 2 × P_COUNT + 32 + 1).

**On the device:** SAVE > USER page: KNOB 1 picks the slot, KNOB 2 LOAD, KNOB 3 ERASE, KNOB 4 SAVE;
OCT+ executes the selected action, OCT- goes back. SAVE over a used slot asks
"OVERWRITE U07?" with OCT+ / OCT-. SAVE uses the automatic name. The PRESETS knob and the SAVE > PRESETS browser continue past the factory presets into the used user
presets.

**Flash** (`firmware/src/upreset.c`): two storage objects (`OBJ_UPRESET0/1`, A/B sector pairs at
0xDC000..0xDFFFF), 16 records of 192 bytes each, behind a bank header (magic "UPB1", record size,
slot count; a mismatch reads as an empty bank). A record keeps its layout version (versions 1..5 are read; another: empty)
and the P_COUNT it was stored with; another count is mapped by count (last 8 values = P_E0..P_E7, the
first ones = P_LEVEL.. in order, missing ones = defaults). Versions 4 (a note pattern) and 5 (a drum grid)
store each value as one byte, value + 64, so P_COUNT can reach 127; versions 1..3 stored 16-bit values.
The wire format is unchanged: `UP_GET` / `UP_PUT` still carry v14 values, and `UP_PUT` now answers rc 1
for a value outside −64..127. P_COUNT was 53 (P_E0 45) until the SLICER
parameters (SLCR, PAT, RATE, DEPTH: ids 45..48) went in just before P_E0: P_COUNT 57, P_E0 49; then
the twelve matrix parameters (ids 49..60): P_COUNT 69, P_E0 61. An editor takes both from `INFO`;
records stored with 53 load with the SLICER off and every matrix slot off, with 57 with every slot off. The
twenty FM operator parameters (ids 61..80) went in before P_E0 after that: P_COUNT 89, P_E0 81. Records stored with 69 load with
those at their defaults, which change nothing. Then the chord keys (ids 81, 82): P_COUNT 91, P_E0 83; records
stored with 89 (or 69) load with CHRD OFF and VOIC CLOSE, their engine values at 83..90. Then the DRUM lane levels
(ids 83..90): P_COUNT 99, P_E0 91; records stored with 91 (1.0.x) or fewer load with every lane at 127 (100 %), their
engine values at 91..98.

## v5: the drum grid

A DRUM track (engine 10) is edited on the device as a grid of 8 lanes × the steps (SEQ > STEP). The
grid lives in the steps themselves, so every engine has it:

- **Hits.** Each step has a lane mask `hit` and an accent mask `acc` (bit l = lane l, `acc` only on
  lanes that hit). The lanes and the General MIDI note each plays: 0 KICK 36, 1 SNARE 38, 2 CLAP 39,
  3 HAT CL 42, 4 HAT OP 46, 5 TOM 45, 6 RIM 37, 7 BELL 56 (other GM notes play on their lane at their pitch:
  congas on TOM, claves on RIM, cymbals on BELL). A NOTE step plays its notes and then its hits, on any engine (DRUM strikes its
  lanes, a synth plays the pitches); an accented hit at velocity 127, the others at
  the step's velocity (0 = 96). A TIE or REST step plays no hits.
- **On the wire** (after `vel`): `hit & 127`, `acc & 127`, then `(hit >> 7) | (acc >> 7) << 1`. A
  `STEP_SET` / `TRACK_STEP` of 8 step bytes (an editor of before v5) leaves the step's hits as they are;
  send `0, 0, 0` to clear them.
- **Notes on lanes.** A step's notes keep playing as before; the device shows each note on the lane
  it strikes (GM 35..81, the others folded into their octave of 36..47: a low tom 41 on TOM, a crash 49 on
  BELL). When the device edits a lane of a step, notes on that lane become the lane's hit. Patterns
  loaded on the device into a DRUM track (SEQ > PHRASES, e.g. 12 BEAT), and DRUM tracks of projects saved
  before the grid, get their lanes' own notes as hits (the same notes and velocities play).
- **Sound loads never convert steps** (as before): switching a track to or from DRUM keeps its notes
  and hits.
- **User presets.** A user preset stored from a DRUM track whose first 16 steps strike a lane holds a
  16-step grid instead of a note pattern (record version 3; version 5 with byte values). `UP_GET` gives its low 7 bits in the 16
  (note, flags) pairs (hits, accents), then kind 1 and 16 bytes of bit 7s (bit 0 hits, bit 1 accents);
  a note pattern ends with kind 0. `UP_PUT` with kind 1 and the 16 bytes stores a grid (accents only on
  hits); without an extension, a note pattern as before. If an extension is sent, it must be
  exactly 17 bytes: kind 0 or 1 followed by 16 bytes (kind 0 ignores those bytes).
  Unknown kinds, truncated extensions and extra bytes are rejected without changing the slot.
  Firmware before v5 shows a grid record as empty.
- **Projects** in v5 used format 5 ("FUN5"): 10-byte steps and 40 reserved bytes per track.
  FUN6 retained those fields and added the song chain (v6 below); FUN7 is current (v7 below). Older formats
  are converted on load.

## v2: live sync

- `WATCH 1` starts the pushes. Watching ends by itself 3 s after the last request of any kind (send
  `PING` about every 1 s), on a USB reset, and when the host goes away; `WATCH 0` ends it at once.
- **CHANGED** (scope, id, v14): a parameter changed on the device (knob, menu, sequencer edit of a
  `P_*`), not by the editor's own `SET`. Coalesced: each (scope, id) at most every 20 ms, with the
  latest value.
- **RELOAD** (engine, preset): the engine, a preset, a user preset or a project was loaded; re-read
  `DESC` of the engine parameters, `DUMP` and the steps. It is also sent after loads the editor asked
  for with `PROJECT` load and `UP_LOAD`. After the editor's own `PRESET` and `SET` of G_ENGSEL it is not
  (firmware with INFO `53 01`, bit 1): the device takes the load as known (the engine, the preset and the
  parameters the load changed), since the editor re-reads `DUMP` after the reply; a device change still
  pending at that moment (a load or another track selected on the device, a knob turned) is pushed as before.
  Older firmware sends `RELOAD` after those too; an editor that skips its own echoes should do so only when
  bit 1 is clear.
- **WATCH while watching** (firmware with INFO `53 01`, bit 0): `WATCH 1` (or 3) while already watching on
  the same USB connection keeps what has not been pushed yet, so a change made just before is still pushed.
  Older firmware takes everything as known again (re-read `DUMP` after a re-`WATCH` there). `WATCH 0`, the 3 s
  timeout and a USB reset end watching; the next `WATCH` starts from the values as they are.
- **STEP_CHANGED** (index): a sequencer step changed on the device (record, clear, step edit,
  pattern load); not after the editor's own `STEP_SET`.
- Push frames have the normal header. Accept them at any time, also while waiting for a reply:
  match replies by cmd (23, 24 and 26 are never replies). The device sends at most a few per
  ~5 ms pass, and only when its USB send queue has room, so a push never delays a reply.

## v3: tracks

- **Track 4 since 1.0** is a synth part like tracks 1..3: `DUMP` / `RELOAD` / `TRACK` / `TRACK_DUMP`
  give its real engine byte (power-on until 0.9.8: DRUM, preset DRUM-X, the General MIDI map; since 0.9.9 the power-on
  has DRUM on track 1 and LOFI BYTEBEAT on track 4), and `PRESET`, `SET` of
  `G_ENGSEL`, `UP_LOAD` and `UP_STORE` work on it; its level is its `P_LEVEL`. The commands are byte for
  byte as before; only the meaning changed. Firmware before 1.0 had a GM drum track there: engine byte
  NENGINES, no presets (`UP_LOAD` / `UP_STORE` rc 1), its level the global `G_DRLVL`. Drums are now the
  DRUM engine on any part: the first C key is the kick (C2 = 36), notes are GM numbers.
- The globals `G_DRCH`, `G_DRLVL`, `G_DRREV` (ids 24..26: the old drum track's MIDI channel, level and
  reverb send) keep their ids, and `G_COUNT` stays 27. `G_DRLVL` and `G_DRREV` are inert: `DESC` gives
  label "-", range 0..0, and no page shows them. Id 24 (`G_DRCH`, never read since 1.0) is `G_RTYPE`
  since 1.0: the reverb's model, `DESC` label "TYPE", enum ROOM (0) / SPRING (1), on the REVERB
  page (FX); projects of formats before FUN7 load it as ROOM. MIDI channel 10 is no longer special (channels 1..4 play
  tracks 1..4, channels 5..16 are ignored since 1.0.3; with ROUT SEL every channel the selected track).
- Selecting a track with `TRACK` does not push `RELOAD` (the editor re-reads `DUMP`, the steps and the
  engine `DESC` itself); selecting one on the device does (`RELOAD` with the new track).
- Pushes are about the selected track only: `CHANGED` (scope 0) and `STEP_CHANGED` refer to it, and
  changes to other tracks (live recording from MIDI into another track, `TRACK_*` writes) push nothing.
- Level and mute are also `P_LEVEL` / `P_MUTE` of the selected track (`SET`); `TRACK_MIX` reaches the
  others. Presets and user presets change a part's sound but keep its `P_LEVEL`, `P_PAN`, `P_MUTE`.
- Projects (`PROJECT`) save and load all four tracks, the selection, the song chain and the motion (FUN7;
  FUN5 added the drum grid, FUN6 the chain). Formats 6..1 are converted; a format 1 project loads into track 1. A project saved before
  1.0 loads its drum track as track 4 with DRUM's kit (SAMPLE PERC until 1.0.2), its steps kept.
- Older firmware (no NTRK in `INFO`): one instrument; skip the track UI.

## v4: any track's parameters

- **Finding out:** send `WATCH 3`. v4 firmware answers 3; v3 (0.8) firmware answers 1, does not know
  cmds 31 / 32 (no reply) and never pushes `TRACK_CHANGED`. `WATCH 1` behaves exactly as in v2 / v3
  (reply 1, no `TRACK_CHANGED`). Match the `WATCH` reply by bit 0.
- `TRACK_PARAM` clamps like `SET` scope 0: to the range of that parameter; the engine parameters
  `P_E0..P_E7` to the ranges of that track's engine. A parameter
  with a fixed range (min = max) keeps its value. A track ≥ NTRK or an id ≥ P_COUNT gets no reply.
  For the selected track it is the same as `SET` scope 0.
- `TRACK_CHANGED` is never about the selected track (its changes stay `CHANGED` scope 0). Coalesced like
  `CHANGED` (each track and id at most every 20 ms, latest value), and not sent for the editor's own
  `TRACK_PARAM` / `TRACK_MIX` writes. After a selection change (`RELOAD`, or the editor's `TRACK`) the
  device takes the current values as known.

## Notes for the editor

- **One request at a time.** Wait for the reply, about 10–50 ms, before sending the next, from one queue
  for every request, keep-alive `PING` included. The device holds one incoming SysEx frame until the main
  loop has answered it; a frame that arrives before then is dropped whole (no reply). A frame is never
  taken in part: USB delivers every packet (acknowledged, retried), and a dropped frame leaves nothing.
  Any frame size up to the largest request (a 256-byte `BACKUP_PUT` piece: 306 bytes on the
  wire, 640 bytes of buffer) is safe at full speed; pacing between bytes is not needed. With a second
  request in flight (pipelined, or a keep-alive from another thread) the second is lost: no reply, and a
  following `BACKUP_PUT` piece is then refused (rc 1, the offset). Measured on an FM-1 with
  1.0 over USB: 256-byte pieces one at a time, the next sent from the reply callback, 560 of 560 taken
  (about 10 ms each); with the next piece sent before the reply, every second piece was dropped.
- **Following the device.** With v2 firmware, `WATCH` and `PING` (above). Older firmware pushes
  nothing (no reply to `PING`): poll `DUMP` about every 300–500 ms while the page is visible.
- **Port.** The device's MIDI port is named "JIANT" (USB 1209:0001; JIANT 0.9.12, iProduct at string index 4;
  JIANT 0.7.0 .. 0.9.11 "JIANT FM", Felucca "Felucca", both at index 2; Windows may still show the name it cached for
  index 2). Match the port by name loosely (`/felucca|jiant/i`: Windows numbers ports "2- JIANT",
  "MIDIIN2 (JIANT)") and tell the firmware by `INFO` / the identity reply, never by the name. Updates use the same
  port with other SysEx (the `F0 22 24 35 …` keys, `00 59 …` frames); never send those
  from the editor. Since 1.0 the same USB device also has an audio input (the same name,
  44.1 kHz stereo; bcdDevice 3.11; since 1.1 44.1 or 48 kHz, the host's choice, bcdDevice 3.21
  (3.20 with MENU > USB SERIAL OFF)); the MIDI port and this protocol are unchanged, and both
  work while the computer records.
- **Global ids.** `G_ROUTE` (id 14, label "ROUT", GLO > SYSTEM) was a placeholder ("--", range 0..0);
  since 1.0 it is the MIDI IN routing: 0 "CH1-6" (JIANT 0.6; "CH1-4" before: channels 1..6 → tracks 1..6; what projects stored
  before. Until 1.0.2 every other channel played the selected track; since 1.0.3 channels 5..16 are
  ignored: notes, bend, CCs, aftertouch, panic and reset), 1 "SEL" (every channel → the selected track).
  Neither setting affects this SysEx protocol or MIDI clock. The
  id, `G_COUNT` (27) and the project format are unchanged; it is saved and loaded with the project like
  the other globals. The editor shows it under GLOBAL > SYSTEM.
- **Sound loads and undo.** A load that changes a track's sound (`PRESET`, `SET` of `G_ENGSEL`, `UP_LOAD`,
  and the same on the device) changes the sound only: the engine and the parameters of the sound. It
  never changes the steps, nor the track's own parameters: `P_LEVEL`, `P_PAN`, `P_MUTE` (0, 39, 40), the
  ARP pages, SCL and LEN / DIV / SWING / GATE (17..32), the SLICER (45..48). It does drop the track's recorded
  motion (the device's SAVE held brings it back). (Before 1.0 a preset also
  set the arp and replaced the steps with its pattern; a factory preset turned the SLICER off.) The byte
  layout of every command is unchanged. Patterns are loaded on the device (SEQ > PHRASES) or written by
  the editor with `STEP_SET` / `TRACK_STEP`.
  The device keeps one copy of the track from before the last load (any track); SAVE held 0.7 s on the
  device swaps back what the load changed (the sound; after a SEQ > PHRASES load, the steps and LEN / DIV
  / SWING / GATE). Loads in a row on one track with nothing changed between them keep the copy from before
  the first; `SET` (scope 0) within 1.5 s of a load on the selected track counts as part of that load, so
  an audition (`G_ENGSEL`, then the patch's values by `SET`, the track's own parameters skipped as
  `UP_LOAD` skips them) and the next one still undo to the state before the first. `PROJECT` load takes no
  copy.
- **Saves while playing.** A flash erase silences the audio and stalls the sequencer for a moment. The
  device refuses its own saves while the transport plays ("STOP TO SAVE"); `PROJECT` save, `UP_PUT`,
  `UP_STORE`, `UP_ERASE`, `BACKUP_LIST` and `BACKUP_PUT` from the editor stop the transport
  first (`BACKUP_GET` does not: it answers rc 3 while the transport runs). If it does not stop within 100 ms, no flash operation starts: user-preset commands
  return rc 2, backup commands rc 3. PROJECT retains its existing reply shape
  (op, slot, used); a failed PROJECT save gets no reply, allowing the editor to report
  a timeout instead of confirming the previous used slot. The previous slot is kept.
  A pending PLAY or SONG start also prevents a device save.
- **Safety.** `PROJECT` save, `UP_PUT` / `UP_STORE` / `UP_ERASE`, and the tagged preference writes below write flash, and only in
  Felucca's own storage; never the app or the update area.


## v6: song chain

INFO appends CHAIN_ROWS (16) after NTRK. Earlier INFO bytes and command numbers
stay where they were. No trailing byte means no SONG command: hide its controls.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 33 SONG | op 0 query; op 1 set, count 0..16, count × (section 0..3, bars 1..64); op 2 start; op 3 stop | op, rc, count, running 0/1, row 0..15, bars to go, count × (section, bars) |

A set must have exactly `2 + count × 2` argument bytes. rc 0 = accepted,
1 = invalid rows (or an empty chain on start), 2 = busy, 3..6 = source slot 0..3
is empty. Rejected sets keep every previous row. Set edits RAM; PROJECT save
persists it, PROJECT load recalls it. A stop/start is acknowledged before the
next audio block; query reports the actual running state. `row` and `remaining`
are meaningful only while running. Rows always start from 0; the last row stops.

**JIANT (FELUCCA TONIC):** a row is a section (the saved project slot 0..3 = A..D) and the bars it plays (1..64, a
4/4 bar of 1/16 steps at the tempo); before, its second byte was the repeats of track 1's loop (1..16). A section is
the whole project: the four tracks' sounds (engine, parameters, FM6 patch), steps and motion, swapped in on the bar,
every track from its step 0; the song's tempo and globals stay. The music as it was before the start comes back when
the song ends or stops. Sources are shared project slots; overwriting a slot changes its uses on the next start.

STEP_GET / TRACK_STEP and WATCH report the playing source. During chain
playback, STEP_SET / TRACK_STEP writes, SET / TRACK_PARAM writes to
LEN / DIV / SWING / GATE and MOTION edits (rc 3) are ignored, returning the current value. Other
sound and mix parameters remain editable. Recording and panel pattern edits
also require STOP. Older editors keep working; older firmware gets no SONG
requests from the new editor.

Projects now write FUN7 (below), the same 3388 bytes in the same A/B sectors. FUN6 has the same size;
FUN5 (3352 bytes) and FUN1–FUN4 convert with an empty chain.
A chain row also plays the motion of its source project (events for an FM operator parameter are skipped
when the track's engine is not the saved one).

## v7: chance, motion, MIDI clock

- **Chance.** Each step has a chance, 0..100 %, as the last byte of the step reply (and the optional last byte
  of a step write). 100 is the default and what every older pattern holds; 0 means the step never plays. The
  device rolls once each time a step comes round, for all of it: its notes, its drum hits and a TIE. A failed
  roll plays nothing and releases what rang before. Chance is saved in projects (FUN7) and in song rows; a
  user preset does not store it. Changing it on the device pushes `STEP_CHANGED`.
- **Motion** is knob moves recorded per step: 64 events shared by the four tracks. An event is (track, step
  0..63, parameter id, value). While a track plays its motion, the step sets the value at the step and it
  holds until another step changes it; the loop restarts from the sound's own value, and stopping puts the
  sound's own values back. Parameters that can be recorded (`motion_param`): ids 0..16 (LEVEL, ENV, LFO),
  33..36 (DIST, CHO, DLY, REV), 38 (GLIDE), 39 (PAN), 44 (DETUNE), 61..80 (the FM operator parameters) and
  83..90 (the DRUM lane levels), 91..98 (the engine parameters; not the chord keys 81, 82). The device records them while the track is armed, playing and selected, from its knobs
  and from `SET` / `TRACK_PARAM` alike.
  - `MOTION` with the track alone is the query. `on` 0 keeps the data and stops playing it; 1 plays it. Clear
    drops the track's events and turns it off; on the device it is undone by SAVE held.
  - Set: `value` must be inside the parameter's range for that track's engine (and −64..127); a
    new event when the 64 are used gets rc 2 ("MOTION FULL" on the device), an event that already exists is
    updated. A set turns the track's motion on. A delete of an event that does not exist succeeds.
  - rc: 0 ok, 1 invalid (step ≥ 64, an id that cannot be recorded, a value outside its range), 2 full, 3 a song is playing.
    A track ≥ NTRK, `on` above 1 or any other length gets no reply.
  - Events come back in the order they are stored. The device does not push motion changes: poll `MOTION`
    (the web editor does, while the sequencer tab shows).
  - `DUMP`, `TRACK_DUMP`, a user preset store and a project save give the sound's own (base) values;
    `GET`, `SET` and the pushes give what is sounding now, which differs while a motion event is in effect.
  - A pattern load and a pattern clear on the device drop the track's motion; the device's UNDO brings it back.
    `MOTION` clear does the same through the same undo copy. A sound load (a preset, `PRESET`, `G_ENGSEL`, `UP_LOAD`,
    INIT SOUND) keeps it since 1.1.5 when the engine stays the same; when the engine changes the records on the
    engine's own ids (E1..E8 `P_E0..P_E7`, DIGITAL's 61..80, the DRUM lane levels) go and the rest (both kinds)
    stays, PLAY as it was (until 1.1 a sound load dropped the whole motion). Poll `MOTION` after a load.
  - The device's own lock edits (a step held + a knob, SEQ > AUTO LIST, 1.1.5) go through its UNDO; `MOTION` ops
    3..7 do not (the protocol is unchanged in 1.1.5).
- **Projects (FUN7).** 3388 bytes, little endian, in the same A/B sectors as before: `46 55 4E 37` ("FUN7"),
  size u32 (3388), the 27 globals as i16 (bytes 8..61), sel, parts, phys (62..64), P_COUNT as stored (byte 66),
  then from byte 68 for each of the 4 tracks: P_COUNT bytes (value + 64), engine, preset, 64 steps of 9 bytes
  (4 notes; n | time << 3 | flags << 5; vel; hit; acc; chance byte where 0 = 100 %, 1..100, 101 = never; firmware
  with `52 01`: the ratchet − 1 in bit 7 of vel (bit 0) and of the chance byte (bit 1), see "Ratchet");
  then the song chain, then the motion (260 bytes: count, on mask, 2 reserved, 64 × (track << 6 | step, id,
  i16 value); 1.1: bit 7 of the id byte marks a parameter lock, see "Parameter locks"); the reserved tail is zero up to byte 3371; bytes 3372..3383 are the project's name (since
  1.0: ASCII 32..126, upper case, 0-padded; all zero = no name, as firmware before wrote them; a byte
  outside 32..126 reads as no name, it never refuses the project); the last 4 bytes are an FNV-1a hash of all
  before (the name included). The device names projects itself (SAVE > PROJECT, NAME); the editor's PROJECT
  save keeps the current name, and backups carry it as part of the 3388 bytes. FUN6..FUN1 load with no name
  and are bounded to today's ranges. A count smaller than P_COUNT is mapped as for user presets, and so are the
  motion events' ids: a FUN7 of 89 parameters has its engine parameters' events at 81..88, which load as 83..90
  (the ids below its P_E0 stay). 68 + 4 × (91 + 2 + 576) + chain + motion = 3040 bytes: 332 to spare.
- **Projects (FUN8, with FM6).** FUN7 laid out the same way, `46 55 4E 38` ("FUN8"), size 3584: the data up to
  byte 3040 as above (16 bytes to spare), the four tracks' FM6 patches as packed 128-byte records (see "FM6
  patches") at bytes 3056..3567, the name at 3568..3579, the hash last. FUN7 (3388) and older load with the
  init patch on every track. Backups, `PROJECT` and the editor's project files carry the 3584 bytes.
- **Projects (FUN9, since 1.1).** FUN8 laid out the same way, `46 55 4E 39` ("FUN9"), size 3648: P_COUNT 99 made
  the data 68 + 4 × (99 + 2 + 576) + chain + motion = 3072 bytes, which no longer fit before FUN8's patches. The
  FM6 patches at 3120..3631 (48 bytes to spare before them), the name at 3632..3643, the hash last. FUN8 (3584,
  91 parameters) loads mapped by count (the lane levels 127, the engine values and their motion events and locks
  from 83..90 to 91..98, a lock's bit 7 kept); FUN7 and older as before. Firmware 1.0.x refuses a FUN9 (its size, magic and P_COUNT). Backups,
  `PROJECT` and the editor's project files carry the 3648 bytes; a backup `PUT` takes 3648, 3584 or 3388.
- **Projects (FUNA, JIANT 0.1).** FUN9 with 96 more bytes, `46 55 4E 41` ("FUNA"), size 3744: the section's DRUM-X
  kit (8 sounds × 11 bytes: the mode, then PITCH PMOD DECAY NOISE COLOR of patch A and of patch B, each 0..127;
  the mode: bits 0..1 the wave, 2..3 the noise filter 0..2, bit 4 SNAP) at 3120..3207, then the group mutes at 3208 (bit 0 KICK, 1 SNARE + CLAP, 2 HATS, 3 TOM RIM BELL; the rest 0) and 7
  reserved bytes (0), the FM6
  patches at 3216..3727, the name at 3728..3739, the hash last. FUN9 and older load with the factory kit. Backups,
  `PROJECT` and the editor's project files carry the 3744 bytes; a backup `PUT` also takes 3648, 3584 or 3388.
- **MIDI clock** has no SysEx. `G_CLOCK` selects the source: 0 INT, 1 USB, 2 TRS. With 1 or 2 the sequencer
  steps on that port's Clock pulses (the other port's are ignored), Start (0xFA) restarts from step 0,
  Continue (0xFB) resumes and Stop (0xFC) stops; BPM follows the incoming tempo (40..240), and 500 ms
  without a pulse stops the transport. Changing `G_CLOCK` stops it.
- Pitch bend, sustain (CC64), RPN 0 (bend range, ±0..24 semitones), CC120 / 121 / 123 are MIDI only and
  have no parameters, protocol or saved state. The standard CC map (1.1: CC5 7 10 71..75 91 93 94, README "MIDI
  in") sets parameters as a knob does: the editor sees those changes as `CHANGED` / `TRACK_CHANGED` pushes.

## Ratchet

INFO advertises `52 01 4` after the MENU settings tag (`4E 01 count`): a step plays up to 4 times. Firmware without the tag has no
ratchet byte (read every step as 1).

- A NOTE step's ratchet 2..4 plays all of it (its notes and its drum hits) that many times, in equal parts of the
  step as swung, each part retriggered and held for GATE of the part. The chance is rolled once for the step (a
  failed roll: no part). A slide into it glides into the first part; it never slides or ties into the next step. 1
  (the default, what every older pattern holds) is the step as before. TIE and REST steps ignore it.
- `STEP_GET` / `STEP_SET` / `TRACK_STEP`: the byte after the chance, 1..4. The flags byte stays 1 accent, 2 slide in
  both directions; a write without the ratchet byte (an older editor) keeps the step's ratchet.
- On the device: SEQ > CHANCE, KNOB 3 (RATCH x1..x4) of the cursor step; the piano roll and the DRUM grid draw a
  ratcheted step in its parts. Changing it pushes `STEP_CHANGED`.
- Saved in projects (FUN7 / FUN8 layout unchanged: bit 7 of the velocity byte is the ratchet − 1's bit 0, bit 7 of
  the chance byte its bit 1; 0 in every older project, which load x1), so also in backups and song rows. Firmware
  before the ratchet refuses a project that holds one. A user preset's note pattern keeps it in its flags (8 | 16);
  a drum grid record has no room for it and loads x1.

## DRUM lane modes

INFO advertises `4F 01 7` at the end of its tags (JIANT 0.8.5): each step holds a mode for each of the 8 DRUM lanes
(BD SD CP CH OH TM RS CB), 0 none, 1 x2, 2 x4, 3 x8 (that lane's hit and its notes ratcheted over the step), 4 SHORT,
5 GHOST, 6 FLAM (how it is struck). That lane only: the rest of the step plays as it is. Firmware without the tag has
none (read every lane as 0).

- `STEP_GET` / `STEP_SET` / `TRACK_STEP`: 8 bytes after the ratchet, lane 0 first. A write without them (an older
  editor) keeps the step's; a write with them must carry the chance and the ratchet before. An editor that does not
  know them reads the 13 bytes it knows and ignores the rest.
- On the device: what the keys record at OCT ≠ 0 on a DRUM track (and MIDI notes outside 35..81); SEQ > STEP's grid,
  KNOB 3 HIT of the selected lane: --, ON, x2, x4, x8, SHORT, GHOST, FLAM.
- Saved in projects (FUNC: bit 6 of a step's extension byte, then 3 bytes, 3 bits a lane; every older project has
  none), so in backups and song rows too. Firmware before 0.8.5 refuses a project that holds one (an extension bit it
  does not know). User presets' patterns have no room for them.

## Parameter locks

INFO advertises `4C 01 1` after the ratchet tag (`52 01 4`). Firmware without it has no locks.

- A **lock** is a motion record (the same 64 shared by the four tracks, one per track, step and id, of either kind)
  whose value sounds on its step only. When the step plays (its chance passed; a step that does not play applies
  none of its locks) the value is set at the step's start, before its notes, and lasts the whole step (its ratchet
  parts too); the next step puts back what sounds without it: the latest automation event of that id at a step up
  to that one in this pass, else the sound's own value. An **automation event** (what the device records with REC
  armed) sets its value at its step and holds it, as before. The ids that can be locked are the ones motion can
  record (`motion_param`, see above); AUTOMATION's PLAY OFF bypasses locks too, its CLEAR clears them.
- `MOTION` 5 sets a lock (track, 5, step, id, v14): as op 3, the value inside the parameter's range for the track's
  engine, rc 2 when the 64 are used. A lock and an automation event on the same step and id are one record: op 3
  over a lock makes it an automation event, op 5 over an automation event makes it a lock. Op 4 deletes either
  kind. Op 6 (track, 6, step) removes that step's locks (step 127: every step's), its automation events stay.
  Op 7 (track, 7) is the query. Ops 5..7 reply as the query and then one kind byte per record, in the same order
  (0 automation, 1 lock); the query and ops 1..4 reply as before 1.1, where a lock reads as an automation event.
- In projects (FUN9; the motion block is laid out as in FUN7 / FUN8) a lock is a motion record with bit 7 of its id
  byte set (ids are below 128: P_COUNT is 99). Every project before 1.1 has none; a FUN8 / FUN7 of a 1.1
  development build may hold some, and on load their ids move to today's positions (83..90 to 91..98 for a FUN8)
  with bit 7 kept. Firmware before 1.1 refuses a project that holds a lock (an id out of its range), as it does a
  newer format; so do backups and song rows.
- On the device: on SEQ > STEP hold a step and turn KNOB 1..4. The knobs lock the four parameters of the sound
  page shown last before (HOME's four, ENV, LFO, FX, EDIT 1 / 2, ..; the cards show them while a step is held). On
  the DRUM grid a step is its white key (several held: each gets the lock; a key pressed on a hit takes it away
  only when let go without a knob turned); on the piano roll the step being entered (its note keys held). [EDIT]
  tapped with a step held clears its locks; EDIT alone clears the step and its locks. The roll and the grid mark
  the steps that hold a lock; AUTOMATION shows the count (LOCK).

## v7: full backup (65-67)

INFO advertises `42 01 caps`: bit 0 = `BACKUP_LIST` / `BACKUP_GET` (read), bit 1 = `BACKUP_PUT` (restore);
this firmware sends 3. Requests name objects, never flash addresses.

| id | object | size |
| --- | --- | --- |
| 0 | runtime: the music being played now, as a FUNA project (Felucca 1.1: FUN9, 3648; 1.0.x: FUN8, 3584; before FM6: FUN7, 3388) | 3744 |
| 1 | settings (palette, speaker, HOLD time, favorites, panel calibration, ...) | the settings record's size |
| 2..5 | PROJECT slots 1..4 (FUNA; Felucca: FUN9 3648, FUN8 3584); JIANT: sections A..D of the current song | 3744, or 0 if empty |
| 6, 7 | user preset banks (slots 1..16, 17..32) | the bank's size, or 0 if empty |
| 8 | the FM6 patch bank of 1.0..1.0.2 (B1..B27). Since 1.0.3 always listed empty (see below) | 3472, or 0 if empty |
| 9 | the user presets' FM6 patches (1.0.3; `up_fm6.c`: per slot a tag and the packed patch) | 3728, or 0 if none |

Reading: `BACKUP_LIST` (no arguments) stops the transport, then takes a snapshot of the runtime object and
answers `1, rc, count` (JIANT: 10; Felucca with the user sample slots 32..34: 13 since 1.0.3, 12 with FM6 up to 1.0.2, 11 before) and, per object in the order above, `id, size u32, crc u32` (CRC-32, zlib). The other objects are read as
they are in RAM or flash. Then `BACKUP_GET` reads an object in pieces: `id, offset u32, count lo, count hi`
(count 1..256, LSB first 7 bit pair) answers `id, rc, offset u32, count lo, count hi` and the data as pack7. Check each object's CRC
against the list; if it differs the device changed, so start again.

Restoring: `BACKUP_PUT` takes ids 0..9 (firmware before 1.0.3: 0..8, id 9 answers rc 1 at begin and nothing is written).

**JIANT 0.3: every song.** Id 10 is the song index (`song_index_t`, version 2: every song's rows and scenes, 1328
bytes; add15, version 3: + each row's track sources for ARRANGE 2, a `uint32_t` per row, 3 bits a track (0 the row's
section, 1..4 A..D), 1840 bytes; add15-beta4, version 4: + each row's track variations (VAR), a `uint32_t` per row, 4 bits a track (0 none,
1 MUTE, 2 THIN, 3 FILL, 4 HALF, 5 REV, 6 SHIFT, 7 OCT+, 8 OCT-, 9..12 VAR 1..4), 2352 bytes; version 2 (1328) and 3
(1840) indexes are still taken, the missing parts empty); a restore keeps the song playing. Ids 40..71 are every song's sections, 40 + 4 × song + section (0 = A): the
current song's come from RAM (as ids 2..5), the others are read from flash as they are asked for, into the storage
driver's own buffer (no RAM of their own). A restore writes each into its song's flash object (an older project format
becomes today's), the current song's into RAM too. Firmware before answers rc 1 at begin for 10 and 40..71, and the
web editor skips them (as id 9).
**V10.6p: 6 songs.** Ids 40..63 (35 objects listed); songs 7 and 8 (ids 64..71) are gone, their flash reserved for a
drum sample bank. The song index (id 10) keeps its layout (room for 8 songs: the same 2352 bytes; rows 7, 8 unused).
An archive from 8-song firmware (43 objects) still reads: its songs 1..6 restore, 64..71 get rc 1 at begin (nothing
written) and the web editor says which songs were not restored (`restoreBackup`'s `notRestored`). A 6-song archive
restores onto 8-song firmware as it is (songs 7, 8 left as they were).
Ids 32..34 (Felucca's user sample slots) are retired with the slots: the web editor skips them when it reads an older
archive and restores the rest. Begin: `0, id, size u32, crc u32`: size is 3744 (FUNA), 3648 (FUN9), 3584 (FUN8) or 3388 (FUN7, FUN6) for id 0,
the settings record's size for id 1, 3744, 3648, 3584, 3388 or 0 (empty the slot) for ids 2..5, the bank's size or 0 for 6 and 7,
3472 or 0 for 8 (the FM6 bank: its magic, layout and every byte below 128 are checked), 3728 or 0 for 9 (its magic,
version and slot count checked; 0 clears the patches). FUN8, FUN7 and FUN6 become FUN9.
An archive without id 8 (written before FM6) still restores; the web editor reads all three kinds (8, 9, 10 objects;
an older archive that also holds the sample slots, 11, 12 or 13 objects, reads with those skipped).
Since 1.0.3 an id 8 with data (an archive of 1.0..1.0.2) is not stored: like the first boot after the update, every FM6
user preset whose stored SLOT is a B slot (8..34) and has no patch yet gets that bank slot's patch, and id 9 is written.
(V10.6s) That move is retired: an id 8 of 3472 bytes is taken (rc 0) and ignored, whatever it holds; a FUN6
project (3388 bytes, retired with FUN1..FUN5) is refused at commit (rc 2), the slot as it was.
So restore ids 6 and 7 before 8 (the web editor does); an empty id 8 is taken and ignored (rc 0). Old archives
therefore restore on both old and new firmware; a 1.0.3 archive on older firmware loses only id 9 (the web editor
skips it when the device answers rc 1). Data: `1, id, offset u32, pack7` with the next offset (they must follow each other)
and at most 256 decoded bytes (256-byte pieces are fine at full speed, one request at a time: see "Notes for
the editor"; a piece that gets no reply was not taken: abort and begin the object again). Commit: `2, id`: the device checks the length and the CRC, validates the
content, and then writes. Abort: `3, id`. Id 0 replaces the music now playing (RAM only, no flash);
ids 1..9 are written to flash (settings and presets are applied too). Nothing is written before the commit.
The web editor sends ids 2..9 first, then 1, then 0 last. A failed restore can leave
earlier objects restored; the file is still the source.

| rc | Meaning |
| --- | --- |
| 0 | ok |
| 1 | invalid: arguments, id, size, offset, count, or an id `BACKUP_PUT` does not take |
| 2 | validation failed: CRC, length or content at commit (and a runtime object that cannot be packed) |
| 3 | stop playback first (`LIST` and `PUT` stop it themselves, and answer 3 if it does not stop in 100 ms; `GET` answers 3 while it plays) |
| 4 | flash write failed |
| 5 | stale: no `LIST` yet, the USB bus was reset, 15 s with no `PUT` request, or a project save / load reused the staging RAM (for `GET`, of id 0 only). Start again with `LIST` / `BACKUP_PUT` begin |

`BACKUP_LIST` answers `1, rc, 0` when rc is not 0. `GET` and `PUT` answer id 127 when they got no arguments.
A `LIST` replaces the snapshot, and a `PUT` begin ends it: a `GET` after a begin gets rc 5.

## FM6 patches (68-71)

The FM6 engine (12) plays a 6-operator patch per track; its eight EDIT parameters are macros on top of it
(ALG 0 = the patch's algorithm, 1..32 another; FB, MLVL, MRAT, MEG, VMOD offsets; DTUN; SLOT). The patch itself
only travels through these commands. INFO advertises `46 01 nfactory nbank` after the backup tag (1.0.3:
`46 01 08 00`; 1.0..1.0.2: `46 01 08 1B`); firmware without it has no FM6 and does not answer 68..71.
After it, `53 01 caps` (live sync, v2 above): bit 0 = `WATCH` while watching keeps what is not pushed yet,
bit 1 = no `RELOAD` after the editor's own `PRESET` / `SET` of G_ENGSEL. This firmware sends 3.
Then (1.0.3) `50 01 caps`, FM6 v2: bit 0 = no patch bank (SLOT is F1..F8 = 0..7 and 8 = OWN; the bank target
answers rc 3), bit 1 = user presets carry their patch (target 3; backup id 9). This firmware sends 3. Firmware
without the tag has the bank and SLOT 0..34 (F1..F8, B1..B27): an editor should not offer the bank either way.

SLOT (P_E0 + 7), since 1.0.3: 0..7 load that factory patch into the track; 8 (OWN) is the track's own patch (what
a project, a user preset, a converted DIGITAL sound or an FM6_PUT to the track put there). Every such load sets SLOT
to F n when the patch is that factory patch unchanged, else OWN, and nothing reloads over it. Setting SLOT from OWN
to F n keeps the own patch aside; setting it back to OWN brings it back. A stored value 9..34 (a B slot of 1.0.2) is
clamped to 8.

A patch is the 128-byte packed record of the generic 6-operator voice (the 32-voice bank's record; every byte is
7-bit, so it travels as it is, no pack7). Operators come sixth first: per operator 17 bytes (R1..R4, L1..L4,
break point, left / right depth, curves `LC | RC << 2`, `RS | DET << 3`, `AMS | KVS << 2`, output level,
`MODE | FC << 1`, fine), then pitch EG rates and levels (102..109), algorithm 0..31 (110), `FB | OKS << 3`,
LFO speed, delay, PMD, AMD, `SYNC | WAVE << 1 | PMS << 4`, transpose (24 = none), the name (10 ASCII bytes).
The device stores every value clamped into its range.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 68 FM6_GET | target, index | target, index, rc, then (rc 0) the 128 bytes |
| 69 FM6_PUT | target, index, the 128 bytes | target, index, rc |
| 70 FM6_LIST | — | nfactory, nbank, then per slot (factory first): used (0/1), name string ("" if empty); 1.0.3: nbank 0, the factory patches only |
| 71 FM6_ERASE | bank index | index, rc (1.0.3: 3, no bank) |

target:
- 0 a track's own patch (index 0..3: what it plays and what its project saves). A PUT is heard at once; SLOT becomes
  OWN (F n if it is that factory patch unchanged), and the device does not reload a factory patch over it.
- 1 the patch bank. 1.0..1.0.2: index 0..26 = B1..B27 in flash (A 0x9F000, B 0xFE000; backup id 8). Since 1.0.3:
  GET, PUT and ERASE answer rc 3 ("no bank") and change nothing; LIST reports nbank 0.
- 2 a factory patch (0..7, GET only).
- 3 a user preset's patch (1.0.3; index 0..31 = U01..U32). It is kept beside the record and counts only for an FM6
  record as it was when the patch was stored (a rename keeps it; an `UP_PUT` that changes the record, `UP_ERASE` or another store
  drops it). `UP_STORE` / SAVE of an FM6 track stores the track's patch with it; `UP_LOAD` plays it. GET: rc 2 when
  the slot has none (not an FM6 sound, or stored without one: it then loads SLOT's factory patch, or the init voice
  for OWN). PUT (after an `UP_PUT` of the record): rc 1 when the slot is not a used FM6 record; it writes flash (stops
  the transport, allow 1 s).

rc: 0 ok, 1 arguments (an unknown target, an index out of range, a record that is not 128 bytes), 2 empty (GET) or a
flash error / transport that did not stop (PUT), 3 no bank (1.0.3, target 1 and ERASE).

The web editor (6-OP FM tab) imports the generic SysEx files of the format: a single voice `F0 43 0n 00 01 1B`, the
155-byte unpacked voice, checksum, `F7` (163 bytes), and 32 voices `F0 43 0n 09 20 00`, 32 x 128 packed, checksum,
`F7` (4104 bytes); the checksum is the two's complement of the data's sum, 7 bits. Raw 155 / 4096-byte files are
read too. Pick a voice, edit it, send it to a track (target 0); to keep it, SAVE it as a user preset (or save the
project) on the device. It exports single voices. Its librarian reads and writes a user preset's patch (target 3)
with the record when the firmware has FM6 v2 bit 1, and keeps it in library files as `fm6` (128 numbers).

## Tagged device preferences v1

INFO appends `0x55, 1, uiCaps` after the existing `NTRK, CHAIN_ROWS` bytes.
Only this complete tagged extension enables commands 34–38. An untagged byte
from an experimental firmware is not a SONG or preference capability.
Command 33 remains SONG. Older editors can ignore the additional INFO bytes.

`uiCaps`: bit 0 palette, bit 1 font weight (retired: firmware since the 1.0 UI has one
weight and no longer sets it), bit 2 reserved for a MIDI monitor, bit 3 favorites. This build
advertises 9 (palette and favorites).

| cmd | Request args | Reply args |
| --- | --- | --- |
| 34 UI_STATE | none | caps, palette, font, monitor, filter, favoriteSig u28, bankSig u28 |
| 35 UI_SET | id, value | rc, id, value, UI_STATE payload |
| 36 UI_PALETTES | none | count, count × name string |
| 37 FAV_GET | engine, start v14, count 1..32 | rc; on success: engine, start v14, count, count × on/off |
| 38 FAV_SET | engine, preset v14, on/off | rc; when applied: engine, preset v14, on/off |

UI_SET ids: 0 palette (0..count-1, the order of UI_PALETTES: GREY GREEN AMBER ICE VIOLET ROSE PAPER
HI-CON NIGHT MONO; 1.0.2: GREY is the MONO of 1.0.1, MONO a new black and white palette), 1 font (retired: rc 2), 2 reserved, 3 preset filter (0 all, 1 favorites). Unsupported state
fields are 127.
Both u28 signatures are four least-significant-first 7-bit bytes; compare them
to refresh changed favorites and user slots. Factory references use stable
engine/preset ids; engine NENGINES denotes a user slot. An empty user slot
cannot be marked. Factory DRUM sounds use the same favorite path.

rc 0 = applied and saved; 1 = invalid arguments; 2 = unsupported feature;
3 = applied in RAM but not saved (no flash or a failed write);
4 = applied and queued to save after STOP. A failed settings write is retried
by the same persistence path as the panel. These changes never stop playback.
Unchanged writes do not erase flash. Replies echo preference ids/values and
favorite ranges so the editor rejects replies to a different request.

## MENU settings (72, 73; 1.0.4)

The device describes the settings of its MENU (HOME held) to the editor, which builds its settings from what it
is told: no list of settings is fixed in the editor. INFO advertises `4E 01 count` after the FM6 v2 tag
(`50 01 caps`): `count` items, MENU_DESC index 0..count−1. Firmware without the tag answers neither command.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 72 MENU_DESC | index (0..count−1) | index, id, kind, value v14, min v14, max v14, name string, then for kind 0 (max−min+1) value-name strings, for kind 1 a unit string; then (1.0.5) tab, tab-name string. An index past the list: index, 127 (no more bytes) |
| 73 MENU_SET | id, value v14 | rc, id, value v14: the device's value after the write (clamped to min..max) |

- **index** is the position in the device's list: up to 1.0.5 the menu's order; from 1.1 a new setting is appended
  (index = id), whatever tab it shows in, so an index keeps the setting it had and an older editor lists the ones it
  knew where they were. Shown by tab (below), each tab's items in index order, they appear as on the device. **id**
  names the setting and never changes its meaning: a new setting takes a new id (append-only), wherever it shows in
  the menu. Ids are 0..126; 127 means "no item". Remember settings by id, not by index.
- **kind**: 0 an enum (the value is an index into the names, min..max), 1 a number (min..max, its unit after the
  name; no setting uses it yet). An editor shows a kind it does not know read-only, or not at all.
- Values are v14 like the rest of the protocol, so a setting may later go past 127; today every value is 0..9.
- **rc** as `UI_SET`: 0 applied and saved, 3 applied in RAM only (no flash or a failed write; retried by the
  settings path), 4 applied and saved after STOP; 1 an unknown id (the request's id and value are echoed,
  nothing changes). A value out of range is not refused: it is clamped and the reply says which value the device
  took. A wrong argument length gets no reply.
- A MENU_SET is applied exactly as the menu's KNOB 1 / OCT+ applies it (`src/menu_items.c` `menu_put`, shared by
  both), and saved through the same settings record (`settings_save`; unchanged values write nothing). If the
  device shows the MENU page, it redraws with the new value. There is no push: an editor that wants to follow
  changes made on the device reads MENU_DESC again (for instance when its settings page opens).
- CALIBRATION and ABOUT have no value and are not offered. COLOR (id 0) is the same setting as `UI_SET` id 0.
- **tab** (1.0.5): the MENU shows its settings in tabs (ALGORITHM steps between them on the device). After the
  names (kind 0) or the unit (kind 1) the reply carries the item's tab: its index (0.., the device's tab order,
  left to right) and its name (≤ 12 characters). An editor that wants the device's grouping shows the items by tab,
  in tab-index order, each tab's items in index order; the tab names come from the device, as the item names do.
  Firmware 1.0.4 ends the reply after the names: no tab (show the items as one list). An editor written for 1.0.4
  reads exactly the names and ignores what follows, so the tab changes nothing for it. Tab indexes are not ids: an
  item may move to another tab in a later firmware, and tabs may be added (appended); remember settings by id.
  A tab may hold items with no value that are not offered (this firmware: SYSTEM also holds CALIBRATION and ABOUT).
  For a kind it does not know, an editor cannot find the tab (it does not know that kind's bytes).

This firmware (count 18; tabs 0 DISPLAY, 1 CONTROL, 2 AUDIO, 3 SYSTEM; 1.0.5 had the first 12, 1.1 the first 15, 1.2
before SCALE LEDS the first 16, before SCREEN OFF the first 17):

| index | id | name | kind | values (min 0) | default | tab |
| --- | --- | --- | --- | --- | --- | --- |
| 0 | 0 | COLOR | 0 | 0..9: GREY GREEN AMBER ICE VIOLET ROSE PAPER HI-CON NIGHT MONO (the order of `UI_PALETTES`) | GREY | 0 DISPLAY |
| 1 | 1 | STYLE | 0 | 0 FLAT, 1 LINE | FLAT | 0 DISPLAY |
| 2 | 2 | LARGE | 0 | 0 OFF, 1 ON | OFF | 0 DISPLAY |
| 3 | 3 | ANIM | 0 | 0 ON, 1 OFF | ON | 0 DISPLAY |
| 4 | 4 | LEDS | 0 | 0 OFF, 1 DIM LO, 2 DIM HI, 3 INV (darkest first, as the menu steps them) | DIM HI | 0 DISPLAY |
| 5 | 5 | HOLD | 0 | 0 "0.3 s", 1 "0.4 s", 2 "0.5 s", 3 "0.6 s" | 0.4 s | 1 CONTROL |
| 6 | 6 | KNOB ACCEL | 0 | 0 OFF, 1 ON | OFF | 1 CONTROL |
| 7 | 7 | FX LATCH | 0 | 0 OFF, 1 ON | OFF | 1 CONTROL |
| 8 | 8 | BPM LOCK | 0 | 0 OFF, 1 ON | OFF | 1 CONTROL |
| 9 | 9 | SPEAKER EQ | 0 | 0 FLAT, 1 LOWCUT, 2 BASS+ | FLAT | 2 AUDIO |
| 10 | 10 | USB LEVEL | 0 | 0 MASTER, 1 FIXED | MASTER | 2 AUDIO |
| 11 | 11 | USB SERIAL | 0 | 0 ON, 1 OFF | ON | 3 SYSTEM |
| 12 | 12 | CLICK | 0 | 0 OFF, 1 REC, 2 ON (1.1) | OFF | 2 AUDIO |
| 13 | 13 | CLICK LEVEL | 0 | 0 LOW, 1 MID, 2 HIGH (1.1) | MID | 2 AUDIO |
| 14 | 14 | COUNT-IN | 0 | 0 OFF, 1 "1 BAR", 2 "2 BARS" (1.1) | OFF | 2 AUDIO |
| 15 | 15 | RESTORE LAST | 0 | 0 ON, 1 OFF (1.2) | ON | 3 SYSTEM |
| 16 | 16 | SCALE LEDS | 0 | 0 OFF, 1 ON (1.2) | OFF | 1 CONTROL |
| 17 | 17 | SCREEN OFF | 0 | 0 NEVER, 1 "5 MIN", 2 "15 MIN", 3 "30 MIN", 4 "60 MIN" (1.1.5) | NEVER (1.1.5.1; 1.1.5: 30 MIN) | 0 DISPLAY |

JIANT appends (index = id; count 23 since 0.9.7): 18 TRACKS (4 / 6, AUDIO), 19 LOAD (ECO / LITE / FULL, SYSTEM),
20 MIDI OUT (KEYS / CLOCK / ALL / OFF, CONTROL), 21 SKIN (DISPLAY; 0.9.7: the values in the menu's order, 0 AIR,
1 COLORS, 2 BOTTOM, 3 HUD, AIR the default of new settings; 0.8.9 had 0 COLORS, 1 BOTTOM, 2 HUD) and (0.9.7) 22 SCOPE
(0 OFF, 1 ON; OFF the default: an engine's EDIT pages the creature on the whole panel, DISPLAY).
V10.6z4 appends index 26, id 26 **OCT BEND** (AUDIO; count 27): 0 OFF, 1 "1 ST", 2 "2 ST" (the default), 3 "3 ST",
4 "4 ST", 5 "5 ST", 6 "7 ST", 7 "12 ST": OCT+ held while a key's note sounds bends it up this far, OCT− a vibrato (the
expression keys; OFF: OCT is the octave then too). A synth track records them as automation of id 90 (`P_LN7`, a
DRUM track's lane 8 level, 127 on every synth part): 127 − (bit 0 vibrato, bit 1 bend), 124..127; an editor shows or
leaves such records on a synth track as it likes (their value is not a level there). With MIDI OUT ALL they go out as
CC1 and pitch bend (full = OCT BEND's range). GATE's `DESC` default is 127 (100 %) since V10.6z4 (64 before); stores
still leave out 64, so a project or user preset saved at 50 % loads 50 %.
V10.6z6 (protocol 1.2) appends index 27..30, the keys' velocity and pressure (CONTROL; count 31): 27 **VELOCITY**
(0 OFF, 1 SOFT, 2 NORMAL, 3 HARD; NORMAL the default: OFF plays every key 100 as before), 28 **VEL MIN** (0 "1",
1 "10", 2 "20", 3 "30", 4 "40", 5 "50", 6 "64", 7 "80"; 20 the default: the velocity's floor), 29 **AFTERTOUCH**
(0 OFF, 1 LOW, 2 MID, 3 HIGH; MID the default), 30 **VEL LEARN** (0 ON, 1 OFF, 2 RESET: setting RESET puts the
calibration's seeds back, then it learns as ON). A synth track's recording of the keys' pressure joins the
expression keys' in id 90: 127 − (gates | pressure 0..7 << 2), 96..127 (pressure p plays as p × 18).

The device's AUDIO tab shows SPEAKER EQ, USB LEVEL, CLICK, CLICK LEVEL, COUNT-IN (index 9, 10, 12, 13, 14).
CLICK: the metronome while the transport runs: OFF, REC (while a track is armed), ON (always); CLICK LEVEL its
level; COUNT-IN: PLAY from stop with a track armed counts in that many bars first (internal clock only). The click
goes to the headphones / speaker, never into USB audio or a recording (README: Metronome and count-in). Applied at
once, as on the device.

RESTORE LAST (id 15, 1.2; the SYSTEM tab shows USB SERIAL and RESTORE LAST, index 11 and 15): ON (the default)
keeps the music as it is in an autosave while the device is stopped and idle, and brings it back at power-on; OFF
writes no autosave and starts with the power-on sounds (README: Restore the last session).

SCALE LEDS (id 16, 1.2, Discussion #127; the CONTROL tab shows HOLD, KNOB ACCEL, FX LATCH, BPM LOCK and SCALE LEDS,
index 5..8 and 16): ON shows the selected track's scale on the key LEDs outside the layers (README: Key LEDs).
Applied at once.

SCREEN OFF (id 17, 1.1.5; the DISPLAY tab shows COLOR, STYLE, LARGE, ANIM, LEDS and SCREEN OFF, index 0..4 and 17):
after that long without input on the panel (a button, a key, a knob; not MIDI, not the editor) the screen and its
backlight go off while the sound, the sequencer, MIDI and USB go on; the next touch wakes it and does nothing else.
NEVER keeps it on. A change counts from then (a MENU_SET to 5 MIN does not turn it off at once). Applied at once.

Example (bytes in hex): `F0 7D 46 4C 48 04 F7` asks for index 4; the reply
`F0 7D 46 4C 48 04 04 00 02 40 00 40 03 40 4C 45 44 53 00 4F 46 46 00 44 49 4D 20 4C 4F 00 44 49 4D 20 48 49 00 49 4E 56 00 00 44 49 53 50 4C 41 59 00 F7`
is index 4, id 4, kind 0, value 2 (`02 40`), min 0 (`00 40`), max 3 (`03 40`), "LEDS", then "OFF" "DIM LO"
"DIM HI" "INV", then tab 0 "DISPLAY" (1.0.4 firmware: the same reply without `00 44 49 53 50 4C 41 59 00`). `F0 7D 46 4C 49 04 03 40 F7` sets LEDS to INV and answers `F0 7D 46 4C 49 00 04 03 40 F7` (rc 0).
`F0 7D 46 4C 49 00 32 40 F7` (COLOR 50) answers `F0 7D 46 4C 49 00 00 09 40 F7` (clamped to 9, MONO).
`F0 7D 46 4C 49 0E 01 40 F7` sets COUNT-IN to 1 BAR and answers `F0 7D 46 4C 49 00 0E 01 40 F7` (1.1).
`F0 7D 46 4C 48 12 F7` answers `F0 7D 46 4C 48 12 7F F7` (no index 18; before SCREEN OFF: none past 16, `11`; 1.2 before SCALE LEDS: none past 15, `10`; 1.1 firmware: none past 14, `0F`; 1.0.5: none past 11, `0C`).

**USB SERIAL (id 11).** The menu applies it when it closes; a MENU_SET applies it about 200 ms after its reply
(`usb_serial_apply`), so the reply leaves first (if the device shows the MENU at that moment: when it closes).
A change (ON <-> OFF; setting the value it already has does nothing) re-enumerates the whole device: it drops off
the bus (its D+ pull-up off) and comes back with the other descriptors, so USB-MIDI, USB audio and the serial
console all go away and come back. On the device the gap is about 25 ms plus one main-loop pass (`usb_retry`,
`usb_start`), or up to 1 s when it re-enumerated in the second before; the host then debounces the attach
(>= 100 ms), resets and enumerates the device. Measured on macOS (CoreMIDI): OFF -> ON, the reply at ~0.04 s, the
device gone at ~0.3 s, the MIDI port gone at ~0.57 s and back at ~1.15 s; ON -> OFF, the serial port goes
at ~0.6 s while the MIDI port was not seen to disappear at all (so do not wait for a disconnect event: after
a real change, wait ~1.5 s and open again). What the editor sends in that window is lost, `WATCH` ends with the
bus reset (send `INFO` and `WATCH` again after reconnecting), and a backup in progress answers rc 5 (stale).

## FM6 voice bank (74-77; JIANT 0.6.11, from Felucca 1.4.1)

32 voices in flash, written whole from the editor (a 32-voice SysEx file: its 128-byte packed records, the format
`FM6_PUT` takes), picked on the device with SLOT B1..B32 (9..40). Loading one copies it into the track's patch as F1..F8
do: the patch is the track's from then on, saved with projects and user presets (`FM6_GET` target 0 reads it), so a
project made with a bank voice sounds the same after the bank was replaced. INFO advertises `56 01 32` (32 voices)
after `4C 01 1` (Felucca: after its `57 01 4`); firmware without the tag answers none of 74..77 and has SLOT 0..8.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 74 FM6B_BEGIN | — | rc. Starts a transfer (stops the transport; erases a sector: allow 1 s) |
| 75 FM6B_WRITE | index 0..31, the 128 bytes | index, rc. Each index at most once per transfer; voices not written are empty in the new bank |
| 76 FM6B_END | name (10 ASCII bytes, space-padded), CRC-32 (zlib's) of the written records, 128 bytes each in index order (5 × 7 bits, LSB first: bits 0..6, 7..13, .., 28..31) | rc, voices (how many the bank the device holds now has). rc 0: the new bank replaced the old one (allow 1 s) |
| 77 FM6B_LIST | — | 32, bank (1 there is one, 0 none), its name string, then per voice: used (0/1), name string ("" if empty) |

rc: 0 ok, 1 arguments (an index past 31 or written twice, a record that is not 128 bytes, an END that is not 15
bytes), 2 the CRC did not match (nothing changed: the old bank stays; start again with `FM6B_BEGIN`), 3 playback did
not stop, 4 flash (no flash, an erase or a write failed), 5 no transfer (no `FM6B_BEGIN`, an END already answered, or
the device's autosave took the transfer's flash between two requests, after 10 s without one: start again).

- An END with no WRITE (the CRC of nothing: 0) writes an empty bank: every B slot is empty.
- The old bank stays until END answers rc 0; a power cut at any point leaves the old bank or the new one, never a mix.
- A track on a bank slot when a new bank arrives keeps the voice it played, as its own patch: SLOT becomes OWN.
- The editor's import (6-OP FM tab): a 32-voice file's voices, each packed as for a single voice (every value into
  its range); an empty slot of a shorter file is not written. Flash: the bank is at 0xE7000; a transfer is staged in
  the autosave's older sector first (0xE5000 / 0xE6000), which the next autosave writes over anyway.

## VOICE words and LYRIC (39; JIANT 0.8.5)

The VOICE engine (5) sings syllables, words and lyrics (`firmware/src/sing.c`). Its two settings are track parameters
in other engines' slots, read on a VOICE track only (`DESC` gives DIGITAL's / DRUM's descriptors there; the device
names them per engine):

| id | label | values |
| --- | --- | --- |
| 61 | LANG | (`P_VLANG`, LOFI's RTRG slot) 0 OFF (VOICE as before, its vowels: bit for bit), 1 ES, 2 EN, 3 JA |
| 74 | LYRIC | (`P_VLYR`, DRUM's IMAGE slot) 0 TEXT (the track's lyric, below; empty: W1), 1..16 W1..W16 (words the dice puts together from the language's bank, a seed each), 17..24 S1..S8 (scat) |

With LANG on, the device relabels VOICE's EDIT values (`DESC` sends the engine's own, VOWEL ..): E1 POS (0 AUTO: each
note the next syllable; 1..127: that syllable of the lyric at the next note, as it changes, so a lock or automation picks
it), E2 THEME (0..127 in eight: LOVE NIGHT SEA SKY ANY ROAD DREAM FIRE; 64 ANY), E3 ARTIC (stored the other way: 0 =
100 %, crisp consonants; 127 = 0 %, slurred), E4 SHIFT, E5 BUZZ, E6 BREATH, E7 Q as before, E8 LOOP (0..127 in five:
FREE, 1, 2, 4, 8 pattern loops: the lyric from its start every so many loops of the pattern). A factory preset sets
LANG and LYRIC (the word presets CANTOR ES .. WHISPER JA; every other preset 0, 0); user presets and projects store
them with the sound.

INFO advertises `59 01 63` (the last tag): LYRIC, a track's lyric of at most 63 bytes. Firmware without the tag answers
nothing to 39.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 39 LYRIC | track (get), or track, ASCII bytes, 0 (set) | track, rc (0 ok, 1 longer than 63: cut), the lyric string |

The lyric: printable ASCII (a byte outside 32..126 becomes a space), case ignored; words are letters (an apostrophe and
"_" join: "mi_amor" sings as one word, two syllables), a "-" alone holds the vowel one more note (a melisma), anything
else separates. Spanish is read by rules (the enye written "ny"), Japanese as romaji (Hepburn or kunrei), English
through the device's dictionary (~2100 words; a word it lacks by rough letter rules). The editor turns accented letters
into plain ones (stress is not marked in singing) and ñ into "ny" before sending. A set takes effect at once (RAM: no
flash write, also while playing): the track's lyric starts over at its next note. Saved with the project (FUNC's
`L` `Y` block per track that has one; a project without it loads with none) and in the backup's runtime object (id 0).
