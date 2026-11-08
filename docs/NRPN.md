# JIANT FM: the NRPN map

<!-- Generated from firmware/src/params.c by tests/app_contract_test.c (`app-contract --write-nrpn`); the test fails
     when this file and the firmware differ. Do not edit by hand. -->

Editor protocol 1.0 (web/EDITOR_PROTOCOL.md, "Stability contract"). Every parameter that automation records
(the knobs of the sound, the mix and the effects) is reachable by **NRPN**; the fixed CCs (CC5 7 10 16..23 71..75
91 93 94 102..105) are unchanged.

- **Track parameters:** CC99 (NRPN MSB) **0**, CC98 (NRPN LSB) the parameter's id below, on the channel the
  track plays (ROUTE as notes: CH1-6, channel n is track n; SEL, every channel the selected track). Channel 16
  is heard for NRPN also with CH1-6: the selected track.
- **Globals:** CC99 **1**, CC98 the global's id, on channel 16 or any channel the FM-1 hears.
- **The value:** CC6 (data entry MSB) then CC38 (LSB): 14 bits, 0..16383, over the parameter's range as DESC
  gives it, `v = min + (x * (max - min) + 8191) / 16383`. CC6 alone is a 7-bit value (as a CC: 0..127 over the
  range, x = msb * 129). A selected NRPN stays selected: more CC6 / CC38 set it again. CC101 / CC100 (an RPN)
  or CC99 127 end it; RPN 0 (the bend range) works as before.
- **AUTOMATION** records a track's NRPN as a knob move (the selected track, armed and playing). Values outside
  the table (an id automation does not record, a global of the transport, routing or loads) are ignored.
- **MIDI OUT ALL** (MENU > CONTROL > MIDI OUT): a knob turned on the panel goes out the same way (CC99, CC98,
  CC6, CC38 with the 14-bit value `x = ((v - min) * 16383 + (max - min) / 2) / (max - min)`): a track's on its
  channel, a global's on channel 16. Sent back to the FM-1 it sets the same value. Only the panel's knobs send
  (not MIDI in, not the editor, not automation playing).

## Track parameters (CC99 0)

The range is the one DESC (scope 0) and TRACK_PARAM give for the track's engine; where an engine reads a slot
its own way the engines are named. P_E0..P_E7 (CC98 115..122) are the engine's EDIT values: see the next table.

| CC98 | parameter | range | default | values |
| --- | --- | --- | --- | --- |
| 0 | LEVEL | 0..127 | 104 | |
| 1 | ATK | 0..127 | 10 | |
| 2 | DEC | 0..127 | 70 | |
| 3 | SUS | 0..127 | 90 | |
| 4 | REL | 0..127 | 60 | |
| 5 | FILTER | -64..63 | 0 | |
| 6 | PITCH | -64..63 | 0 | |
| 7 | SHAPE | -64..63 | 0 | |
| 8 | LOOP | 0..1 | 0 | OFF ON |
| 9 | RATE | -14..127 | 60 | |
| 10 | WAVE | 0..5 | 0 | SIN TRI SAW SQR S&H DRIFT |
| 11 | PHASE | -1..127 | 0 | |
| 12 | FADE | 0..127 | 0 | |
| 13 | PITCH | -64..63 | 0 | |
| 14 | FILTER | -64..63 | 0 | |
| 15 | SHAPE | -64..63 | 0 | |
| 16 | AMP | 0..127 | 0 | |
| 33 | DIST | 0..127 | 0 | |
| 34 | CHORUS | 0..127 | 0 | |
| 35 | DLY | 0..127 | 0 | |
| 36 | REV | 0..127 | 0 | |
| 38 | GLIDE | 0..127 | 0 | |
| 39 | PAN | -64..63 | 0 | |
| 44 | DETUNE | 0..127 | 40 | |
| 61 | ATK | 0..127 | 0 | (ANALOG PHASE WHEEL STRING NOISE FM6 ADD ANIMAL EXT) |
| 61 | RTRG | 0..1 | 1 | (MATH) |
| 61 | LANG | 0..9 | 0 | OFF ES EN JA RU DE FR ZH PT GN (VOICE) |
| 61 | KEY MAP | 0..1 | 1 | CLASSIC EXTENDED (DRUM) |
| 62 | CHANCE | 0..127 | 0 | |
| 63 | HUMAN | 0..127 | 127 | |
| 64 | ATK | 0..127 | 0 | |
| 65 | TIME | 0..127 | 127 | |
| 66 | DEC | 0..127 | 0 | |
| 67 | SUS | 0..127 | 0 | |
| 68 | GATE | 0..127 | 127 | |
| 69 | REL | 0..127 | 0 | |
| 70 | REC Q | 0..127 | 127 | |
| 71 | MORPH | 0..127 | 0 | |
| 72 | C.LEN | 0..64 | 0 | OFF 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 ... |
| 73 | DRIFT | 0..127 | 127 | |
| 74 | IMAGE | 0..127 | 0 | (ANALOG PHASE MATH WHEEL STRING DRUM NOISE FM6 ADD ANIMAL EXT) |
| 74 | LYRIC | 0..25 | 0 | TEXT W1 W2 W3 W4 W5 W6 W7 W8 W9 W10 W11 W12 W13 W14 W15 W16 S1 S2 S3 S4 S5 S6 S7 S8 TEXT FIXED (VOICE) |
| 75 | DUST | 0..127 | 127 | |
| 76 | CHORD | 0..11 | 0 | OFF TRIAD 7TH ADD9 SUS2 SUS4 MAJ MIN 7 MAJ7 MIN7 POWER |
| 77 | VOICING | 0..3 | 0 | CLOSE LEAD OPEN +BASS |
| 78 | DEPTH | 0..127 | 127 | |
| 79 | STRUM | 0..127 | 0 | |
| 80 | LEVEL | 0..127 | 127 | |
| 81 | ROTATE | -32..32 | 0 | |
| 82 | PITCH | -24..24 | 0 | |
| 83 | IMAGE | 0..127 | 127 | (ANALOG PHASE MATH VOICE WHEEL STRING NOISE FM6 ADD ANIMAL EXT) |
| 83 | KICK | 0..127 | 127 | (DRUM) |
| 84 | SNARE | 0..127 | 127 | (ANALOG PHASE MATH WHEEL STRING DRUM NOISE FM6 ADD ANIMAL EXT) |
| 84 | CONS | 0..127 | 127 | (VOICE) |
| 85 | CLAP | 0..127 | 127 | |
| 86 | HATCL | 0..127 | 127 | |
| 87 | HATOP | 0..127 | 127 | |
| 88 | TOM | 0..127 | 127 | |
| 89 | RIM | 0..127 | 127 | |
| 90 | BELL | 0..127 | 127 | |
| 92 | TONE | -64..63 | 0 | |
| 95 | SLEW | 0..127 | 0 | |
| 96 | LEVEL | 0..127 | 0 | |
| 97 | LEVEL | 0..127 | 0 | |
| 98 | LEVEL | 0..127 | 0 | |
| 99 | LEVEL | 0..127 | 0 | |
| 100 | LEVEL | 0..127 | 0 | |
| 101 | LEVEL | 0..127 | 0 | |
| 102 | LEVEL | 0..127 | 0 | |
| 103 | LEVEL | 0..127 | 0 | |
| 104 | LEVEL | 0..127 | 0 | |
| 105 | LEVEL | 0..127 | 0 | |
| 106 | LEVEL | 0..127 | 0 | |
| 107 | LEVEL | 0..127 | 0 | |
| 108 | LEVEL | 0..127 | 0 | |
| 109 | LEVEL | 0..127 | 0 | |
| 110 | LEVEL | 0..127 | 0 | |
| 111 | LEVEL | 0..127 | 0 | |
| 113 | CUT | 0..127 | 127 | |
| 114 | RES | 0..127 | 0 | |

## Engine parameters (CC99 0, CC98 115..122: E1..E8)

Each engine's EDIT values as its preset 1 shows them (a mode may rename one: DESC says what the track has now).

| engine | CC98 | parameter | range | default | values |
| --- | --- | --- | --- | --- | --- |
| ANALOG 0 | 115 | WAVE | 0..8 | 0 | SAW SQR TRI SIN PWM SYNC RING SAW3 PLS3 |
| ANALOG 0 | 116 | DETUNE | 0..127 | 10 | |
| ANALOG 0 | 117 | MIX | 0..127 | 64 | |
| ANALOG 0 | 118 | NOISE | 0..127 | 0 | |
| ANALOG 0 | 119 | CUT | 0..127 | 90 | |
| ANALOG 0 | 120 | RES | 0..127 | 30 | |
| ANALOG 0 | 121 | DRIVE | 0..127 | 0 | |
| ANALOG 0 | 122 | INT | -24..24 | 0 | |
| PHASE 2 | 115 | WAVE | 0..7 | 0 | SAW SQR PLS DSIN SPLS RSAW RTRI RTRP |
| PHASE 2 | 116 | WAVE2 | 0..8 | 0 | - SAW SQR PLS DSIN SPLS RSAW RTRI RTRP |
| PHASE 2 | 117 | DCW | 0..127 | 60 | |
| PHASE 2 | 118 | ENV | 0..127 | 64 | |
| PHASE 2 | 119 | DETUNE | 0..127 | 0 | |
| PHASE 2 | 120 | LINE | 0..1 | 0 | MIX RING |
| PHASE 2 | 121 | SUB | 0..127 | 0 | |
| PHASE 2 | 122 | FB | 0..127 | 0 | |
| MATH 3 | 115 | CHIP | 0..3 | 2 | 4BIT 4B/2 8BIT 1BIT |
| MATH 3 | 116 | WAVE | 5..6 | 5 | BYTE FLOAT |
| MATH 3 | 117 | ALGO | 0..191 | 64 | |
| MATH 3 | 118 | VAR | 0..127 | 0 | |
| MATH 3 | 119 | MOTION | 0..255 | 0 | |
| MATH 3 | 120 | GRIT | 0..127 | 0 | |
| MATH 3 | 121 | BEND | 0..127 | 0 | |
| MATH 3 | 122 | LOOP | 0..127 | 0 | |
| VOICE 5 | 115 | VOWEL | 0..127 | 0 | |
| VOICE 5 | 116 | VOWEL2 | 0..127 | 64 | |
| VOICE 5 | 117 | TALK | 0..127 | 0 | |
| VOICE 5 | 118 | SHIFT | -12..12 | 0 | |
| VOICE 5 | 119 | BUZZ | 0..127 | 64 | |
| VOICE 5 | 120 | BREATH | 0..127 | 10 | |
| VOICE 5 | 121 | Q | 0..127 | 64 | |
| VOICE 5 | 122 | RAND | 0..127 | 0 | |
| WHEEL 7 | 115 | REG | 0..15 | 4 | FLUTE MELLO HOLLW SMOOT 3BAR BLUES GOSPL ROCK TOPS CLARI REED STRNG CHAPL BRITE BASS FULL |
| WHEEL 7 | 116 | SUB | -8..8 | 0 | |
| WHEEL 7 | 117 | BODY | -8..8 | 0 | |
| WHEEL 7 | 118 | TOP | -8..8 | 0 | |
| WHEEL 7 | 119 | PERC | 0..6 | 0 | OFF 2ND 3RD 2SOFT 3SOFT 2SLOW 3SLOW |
| WHEEL 7 | 120 | CLICK | 0..127 | 40 | |
| WHEEL 7 | 121 | DRIVE | 0..127 | 0 | |
| WHEEL 7 | 122 | ROTOR | 0..2 | 1 | OFF SLOW FAST |
| STRING 8 | 115 | EXCITE | 0..5 | 1 | NOISE PLUCK PICK HAMMR STICK FDBK |
| STRING 8 | 116 | DECAY | 0..127 | 72 | |
| STRING 8 | 117 | BRIGHT | 0..127 | 60 | |
| STRING 8 | 118 | POS | 0..127 | 28 | |
| STRING 8 | 119 | STRETCH | 0..127 | 0 | |
| STRING 8 | 120 | BODY | 0..127 | 40 | |
| STRING 8 | 121 | SIZE | 0..127 | 40 | |
| STRING 8 | 122 | BOW | 0..127 | 0 | |
| DRUM 10 | 115 | MORPH | 0..127 | 64 | |
| DRUM 10 | 116 | TUNE | 0..127 | 64 | |
| DRUM 10 | 117 | TONE | 0..127 | 64 | |
| DRUM 10 | 118 | DECAY | 0..127 | 64 | |
| DRUM 10 | 119 | NOISE | 0..127 | 64 | |
| DRUM 10 | 120 | FM ENV | 0..127 | 0 | |
| DRUM 10 | 121 | FOLD | 0..127 | 0 | |
| DRUM 10 | 122 | DRIVE | 0..127 | 0 | |
| NOISE 11 | 115 | MODE | 0..3 | 0 | ANLG DUST LFSR META |
| NOISE 11 | 116 | COLOR | 0..127 | 64 | |
| NOISE 11 | 117 | FREQ | 0..127 | 80 | |
| NOISE 11 | 118 | PEAK | 0..127 | 30 | |
| NOISE 11 | 119 | TRK | 0..127 | 64 | |
| NOISE 11 | 120 | DENSE | 0..127 | 0 | |
| NOISE 11 | 121 | DRIFT | 0..127 | 0 | |
| NOISE 11 | 122 | CRUSH | 0..127 | 0 | |
| FM6 12 | 115 | ALG | 0..32 | 0 | |
| FM6 12 | 116 | FB | -7..7 | 0 | |
| FM6 12 | 117 | MOD | -64..63 | 0 | |
| FM6 12 | 118 | RATIO | -16..16 | 0 | |
| FM6 12 | 119 | MEG | -64..63 | 0 | |
| FM6 12 | 120 | VMOD | -7..7 | 0 | |
| FM6 12 | 121 | DETUNE | 0..127 | 0 | |
| FM6 12 | 122 | SLOT | 0..40 | 0 | |
| ADD 13 | 115 | MODE | 0..9 | 0 | HARM ODD WIDE BELL BAR DRUM PRIME FIFTH SUB PLATE |
| ADD 13 | 116 | MORPH | 0..127 | 0 | |
| ADD 13 | 117 | BRIGHT | 0..127 | 70 | |
| ADD 13 | 118 | PARTIAL | 0..127 | 127 | |
| ADD 13 | 119 | SHIFT | -64..63 | 0 | |
| ADD 13 | 120 | COMB | 0..127 | 0 | |
| ADD 13 | 121 | DECAY | 0..127 | 127 | |
| ADD 13 | 122 | MOTION | -64..63 | 0 | |
| ANIMAL 14 | 115 | MODE | 0..4 | 0 | CMOS PULSE WHSTL WING GLOTT |
| ANIMAL 14 | 116 | TOUCH | 0..127 | 100 | |
| ANIMAL 14 | 117 | COUPLE | 0..127 | 0 | |
| ANIMAL 14 | 118 | CAP | 0..127 | 64 | |
| ANIMAL 14 | 119 | HYST | -64..63 | 0 | |
| ANIMAL 14 | 120 | RATE | 0..127 | 80 | |
| ANIMAL 14 | 121 | GROUP | 0..15 | 0 | |
| ANIMAL 14 | 122 | FM | 0..40 | 22 | |
| EXT 15 | 115 | SOURCE | 0..4 | 0 | IN 1 IN 2 1+2 STEREO SWARM |
| EXT 15 | 116 | GAIN | 0..127 | 64 | |
| EXT 15 | 117 | GATE | 0..127 | 0 | |
| EXT 15 | 118 | PLAY | 0..1 | 0 | THRU KEYS |
| EXT 15 | 119 | WIDTH | 0..127 | 127 | |
| EXT 15 | 120 | RING | 0..127 | 0 | |
| EXT 15 | 121 | BITS | 0..127 | 0 | |
| EXT 15 | 122 | RATE | 0..127 | 0 | |

## Globals (CC99 1)

| CC98 | parameter | range | default | values |
| --- | --- | --- | --- | --- |
| 0 | BPM | 40..240 | 120 | |
| 1 | SWING | 0..100 | 0 | |
| 3 | TUNE | -50..50 | 0 | |
| 4 | TIME | 0..66 | 1 | 1/4 1/8 1/16 1/32 8T 16T 1/2 1/1 2BAR 4BAR 5ms 6ms 7ms 8ms 9ms 10ms 11ms 12ms 13ms 15ms 17ms 19ms 21ms 24ms 27ms 31ms 35ms 39ms 44ms 50ms 56ms 64ms 72ms 81ms 91ms 103ms 116ms 131ms 148ms 167ms ... |
| 5 | FEEDBK | 0..120 | 60 | |
| 6 | TONE | 0..127 | 64 | |
| 7 | MIX | 0..127 | 90 | |
| 8 | SIZE | 0..127 | 90 | |
| 9 | DAMP | 0..127 | 60 | |
| 10 | RATE | 0..127 | 40 | |
| 11 | DEPTH | 0..127 | 60 | |
| 13 | REL | 0..100 | 30 | |
| 17 | DJ FILT | -64..63 | 0 | |
| 21 | CLIP | 0..100 | 0 | |
| 24 | TYPE | 0..4 | 0 | ROOM SPRING SHIMR RESO CLOUD |
| 25 | PUNCH | 0..100 | 0 | |
| 26 | DUCK | 0..100 | 0 | |
| 27 | STEREO | -64..63 | 0 | |
| 28 | WIDE | 0..127 | 0 | |
| 29 | MOD | 0..127 | 0 | |
| 30 | RATE | 0..127 | 30 | |
| 31 | PRE | -24..100 | 0 | |
| 32 | FILT | -64..63 | 0 | |
| 33 | WIDE | 0..127 | 0 | |
| 34 | PITCH | -12..12 | 0 | |
| 35 | SPRAY | 0..127 | 0 | |
| 36 | ALL TR | -14..14 | 0 | |
| 37 | MODE | 0..2 | 0 | OFF GATE STUT |
| 38 | PAT | 1..16 | 1 | |
| 39 | RATE | 0..5 | 1 | 1/8 1/16 1/32 8T 16T 32T |
| 40 | DEPTH | 0..127 | 127 | |
| 41 | T1 | 0..1 | 1 | OFF ON |
| 42 | T2 | 0..1 | 1 | OFF ON |
| 43 | T3 | 0..1 | 1 | OFF ON |
| 44 | T4 | 0..1 | 1 | OFF ON |
| 45 | T5 | 0..1 | 1 | OFF ON |
| 46 | T6 | 0..1 | 1 | OFF ON |
| 47 | FX | 0..1 | 1 | OFF ON |
| 48 | LEN | 1..16 | 16 | |
| 49 | QUANT | 0..1 | 1 | CHR SCL |
| 50 | PITCH | -24..24 | 0 | |
| 51 | PITCH | -24..24 | 0 | |
| 52 | PITCH | -24..24 | 0 | |
| 53 | PITCH | -24..24 | 0 | |
| 54 | PITCH | -24..24 | 0 | |
| 55 | PITCH | -24..24 | 0 | |
| 56 | PITCH | -24..24 | 0 | |
| 57 | PITCH | -24..24 | 0 | |
| 58 | PITCH | -24..24 | 0 | |
| 59 | PITCH | -24..24 | 0 | |
| 60 | PITCH | -24..24 | 0 | |
| 61 | PITCH | -24..24 | 0 | |
| 62 | PITCH | -24..24 | 0 | |
| 63 | PITCH | -24..24 | 0 | |
| 64 | PITCH | -24..24 | 0 | |
| 65 | PITCH | -24..24 | 0 | |
| 66 | MOD | 0..127 | 0 | |
| 67 | TYPE | 0..2 | 0 | ANALOG DIGITAL REVERS |
| 68 | PING | 0..1 | 0 | OFF ON |
| 69 | LEN | 1..64 | 64 | |
| 70 | RES | 0..2 | 1 | 1/8 1/16 1/32 |
| 71 | DRIVE | 0..100 | 0 | |
| 72 | R TIME | 0..6 | 0 | =L x1/2 x2/3 x3/4 x4/3 x3/2 x2 |
