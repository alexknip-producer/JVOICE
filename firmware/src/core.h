/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FELUCCA core types: tracks, voices, engines, parameters.
 * Four tracks, each a synth part: its own engine, preset, parameters, voices and 64-step
 * pattern. The parts share a measured-load budget of sounding voices (voice.c). Drums are the DRUM
 * engine (General MIDI map) on any part; the SAMPLE engine's PERC set is retired (drum_from_perc).
 * Sections: sizes, parameters, voices and engines, tracks and the song, system. */
#include <stdint.h>

/* ------------------------------------------------------------ sizes --- */
#define NVOICE 8                 /* slots per part; shared admission budget is separate (voice.c) */
#define NPART 6                  /* synth parts: tracks 1..6 (JIANT 0.6: was 4) */
#define NTRK NPART               /* tracks (the formats and the protocol count these): every track is a part */
#define NSTEP 64
#define HALF_FRAMES 128          /* I2S half buffer: 2.9 ms at 44.1 kHz (a key waits 0..1 half, then plays 1 half later).
                                  * Rendered in blocks of CTL samples (felucca_tables.h, tools/gen_tables.py: 64 since 0.9.9
                                  * ctl64, 32 before): the control rate of envelopes, LFOs, the sequencer */
#ifndef FELUCCA_FM4
#define FELUCCA_FM4 0            /* the DIGITAL engine (eng_digital.c, four-operator FM): kept in the tree, not built
                                  * by default; replaced by FM6, its sounds convert (fm4_convert.c) */
#endif
#define NENGINES 16              /* the stores' engine numbers 0..15 (engines.c ENGINES[], append-only; 0.8.3: 14 ANIMAL;
                                  * 0.9.9-snd5: STRING and ADD in the retired GRAIN's 8 and SLICE's 13; V10.54: 15 INPUT) */
#define ENGI_DIGITAL 1u          /* reserved without FELUCCA_FM4: never selectable (eng_ok), its sounds load as FM6 */
#define NENG_SHOWN (NENGINES - !FELUCCA_FM4 - 3)   /* the engines one can pick: PRESETS, the EDIT layer, the editor,
                                                * in the display order of engines.c ENGINE_ORDER (PHYS 9, and in
                                                * JIANT SAMPLE 4, GRAIN 8, SLICE 13: retired) */
/* (JIANT 0.6.71) the second core (hal/fm1_multicore.h; fx.c mix_block hands it some synth tracks). MC_CPU(): the core
 * running this, for the scratch each core has its own of (fx.c part_buf and the mix, mod.c's matrix, fm6_core.c's
 * buses, voice.c's filter flags). The host (tests): no second core; mc_host runs its jobs at once, as core 1 */
#ifndef FELUCCA_DUAL_CORE
#define FELUCCA_DUAL_CORE 0
#endif
#if FELUCCA_DUAL_CORE
#define MC_CPU() fm1_cnum()
#else
static uint8_t mc_host;
#ifndef MC_HOST_STORAGE
#define MC_HOST_STORAGE
#endif
static MC_HOST_STORAGE uint8_t mc_host_cpu; /* the concurrent host test uses thread-local core identity */
#define MC_CPU() ((uint32_t)mc_host_cpu)
#endif
/* (JIANT 0.6.78) the hottest audio code in RAM, for core 0 (felucca.c FELUCCA_HOTRAM): app.ld puts these functions'
 * sections (-ffunction-sections) in .hot_text by name, so the compiler builds them as before (this clang inlines
 * nothing across a section attribute, not even a static inline). Core 1 keeps running the flash copies (fx.c
 * mix_part_m, fx_run): it now and then reads a word of RAM wrong (hal/fm1_multicore.h) */
#ifndef FELUCCA_HOTRAM
#define FELUCCA_HOTRAM 1
#endif
/* (0.9.9 ctl64) the sample within the block (0 .. CTL - 1) the event being handled falls on: seq.c sets it around the
 * note-ons and -offs it fires inside a block (a step's start, a strum's note, HUMAN TIME, a ratchet's part, an ARP step, a
 * gate's end), voice.c keeps it with the voice (voice_t.sub) and renders the note from that sample. 0 between events */
static uint32_t ev_off;                 /* (a test ticking a whole step at once: past a byte) */
/* (ctl64) the sample a voice's render starts at within the part's block (voice.c track_render), this core's: an engine
 * that writes other buffers than out by index (eng_drum.c: the sides, the sends) offsets them by it */
static uint8_t vo_off_c[2];
/* (ctl64) a VOICE note's random draw (eng_formant.c formant_note_on), made when the note is asked for (voice.c
 * voice_begin, on core 0), not when its voice starts in the render (either core) */
static uint32_t vs_rnd;
#define UP_SLOTS 32u             /* user presets (upreset.c) */
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

/* (JIANT 0.9.9-stab) the 32 x 32 -> 64-bit multiplies of the audio path, out of line. On the FM-1 (pi32v2 at 360 MHz) a
 * multiply into a register pair (rN_rM = rA * rB) inlined beside loads, most of all 16-bit ones (a table, a delay line, a
 * frame of USB audio), now and then leaves a register a bit off: an index 0x12 read as 0x00800012, sp a word off, so a
 * return pops a neighbour (felucca-bsp docs/CPU.md, reproduced in minutes on its AAC filterbank: clean for the whole run
 * with the multiplies in helpers of their own, the product used at once, so the multiply completes before any load that
 * follows). The same products bit for bit (tests/golden.txt); a call each (tools/build.py mul64_near_loads counts what
 * is left inline). Up to four arguments: all in registers, no load in the helper */
#define MUL64_FN static __attribute__((noinline))
MUL64_FN int64_t mul64(int32_t a, int32_t b) { return (int64_t)a * b; }
MUL64_FN int64_t mul64_us(uint32_t a, int32_t b) { return (int64_t)a * b; }
MUL64_FN uint64_t umul64(uint32_t a, uint32_t b) { return (uint64_t)a * b; }
MUL64_FN int64_t mac2_64(int32_t a, int32_t b, int32_t c, int32_t d) { return (int64_t)a * b + (int64_t)c * d; }
MUL64_FN int32_t mul64_sh15(int32_t a, int32_t b) { return (int32_t)(((int64_t)a * b) >> 15); }

/* ------------------------------------------------------- parameters --- */
enum {
    F_INT, F_PCT, F_BIPCT, F_TIME, F_LFOHZ, F_CUTOFF, F_DB, F_SEMI, F_ENUM, F_BPM, F_NOTE,
    F_ONOFF, F_OCT, F_STEPS,
    F_PCTINV                     /* (JIANT 0.6.37) a share of 127 shown and turned the other way: 0 is 100 % */
};

typedef struct {
    const char *label;
    uint8_t fmt;
    int16_t min, max, def;
    const char *const *names;   /* F_ENUM */
    const char *unit;           /* F_INT / F_ENUM optional unit */
} param_desc_t;

enum {                          /* per-track parameters */
    P_LEVEL,
    P_ATK, P_DEC, P_SUS, P_REL,
    P_ED_FLT, P_ED_PIT, P_ED_SHP, P_ED_FX,     /* P_ED_FX (once unused): JIANT 0.4's ENV LOOP, P_ELOOP */
    P_LRATE, P_LWAVE, P_LPHASE, P_LFADE,
    P_LD_PIT, P_LD_FLT, P_LD_SHP, P_LD_AMP,
    P_AMODE, P_ARATE, P_AOCT, P_AGATE,
    P_ASWING, P_APROB, P_AHOLD, P_AORDER,
    P_ROOT, P_SCALE, P_QUANT, P_TRANS,
    P_SLEN, P_SDIV, P_SSWING, P_SGATE,
    P_DIST, P_CHOR, P_DLY, P_REV,
    P_VOICE, P_GLIDE, P_PAN, P_MUTE,
    P_GLMODE, P_PRIO, P_ALLOC, P_DETUNE,
    P_SLCR, P_SLPAT, P_SLRATE, P_SLDEPTH,      /* SLICER insert (slicer.c) */
    P_M1SRC, P_M1DST, P_M1AMT,                 /* modulation matrix (mod.c): 4 slots of SRC, DST, AMT; */
    P_M2SRC, P_M2DST, P_M2AMT,                 /* new common parameters go just before P_E0 (user presets */
    P_M3SRC, P_M3DST, P_M3AMT,                 /* and projects map by count) */
    P_M4SRC, P_M4DST, P_M4AMT,
    P_FM1_ATK, P_FM1_DEC, P_FM1_SUS, P_FM1_REL, P_FM1_LEVEL,
    P_FM2_ATK, P_FM2_DEC, P_FM2_SUS, P_FM2_REL, P_FM2_LEVEL,
    P_FM3_ATK, P_FM3_DEC, P_FM3_SUS, P_FM3_REL, P_FM3_LEVEL,
    P_FM4_ATK, P_FM4_DEC, P_FM4_SUS, P_FM4_REL, P_FM4_LEVEL,
    P_SOFS, P_POFS,                            /* (JIANT; the former chord keys) the sequence offset: OFS steps, PIT semitones */
    P_LN0, P_LN1, P_LN2, P_LN3, P_LN4, P_LN5, P_LN6, P_LN7,   /* DRUM lane levels (eng_drum.c, #97): KICK .. BELL */
    P_DTYPE, P_DTONE,                          /* (JIANT, FUNB) DIST's type and tone (fx.c track_dist) */
    P_MSLEN, P_MSDIV, P_MSSLW,                 /* (JIANT, FUNB) the modulation sequence (mod.c MS_STEP): its length, */
    P_MS0, P_MS15 = P_MS0 + 15,                /* rate and slew, its 16 levels */
    P_FTYPE,                                   /* (JIANT 0.5) the filter's type: LP HP BP COMB (eng_analog.c, voice.c) */
    P_FCUT, P_FRES,                            /* .. every other engine's: a filter on the part's sum (voice.c track_filter) */
    P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7,
    P_COUNT
};
#define P_LRTRG P_FM1_ATK              /* (JIANT 0.6.12) LOFI BYTE / FLOAT's RTRG: a note restarts the formula's time (voice.c
                                         * voice_start), in a slot of the retired DIGITAL's (0: OFF, its default) */
#define P_TCHAN P_FM1_DEC              /* (JIANT 0.6.37) SHIFT > CHANCE: how much of the track's steps it skips, 0 none (100 %,
                                         * every project and preset before it); seq.c trk_chance; DIGITAL's OP1 DEC slot */
/* (V10.6: MUTATE retired; its slots stay the sequencer's, unused) (JIANT 0.6.43) MUTATE (project.c mutate_poll), in DIGITAL's OP1 REL / OP2 slots (0, every project and preset before it:
 * OFF, 0 %, ORBIT): SOUND and PATTERN every OFF / 1 / 2 / 4 / 8 / 16 / 32 bars, AMOUNT, MODE ORBIT / DRIFT */
/* (V10.22) MUTATE's slots now ENV 2's (ENV > ENV 2: ATK DEC SUS REL, a MOD source, mod.c MS_ENV2): read only through
 * a MOD slot with SRC ENV2 (none in any project or preset before it), so the values an old project kept there change nothing */
#define P_E2ATK P_FM1_REL
#define P_E2DEC P_FM2_ATK
#define P_E2SUS P_FM2_DEC
#define P_E2REL P_FM2_REL
#define P_MSND P_FM1_REL
#define P_MPAT P_FM2_ATK
#define P_MAMT P_FM2_DEC
#define P_MMODE P_FM2_REL
/* (JIANT 0.9.9-seq2c) MUTATE with control: P_MMODE bit 0 DIR (ORBIT / DRIFT, as before), bits 1..3 PARAM (the one value
 * WHAT = PARAM moves, MUT_PIDS); P_MCFG, DIGITAL's free OP4 LEVEL slot, holds 127 - cfg (its stored default 127: cfg 0 in
 * every project and preset before it): bits 0..2 WHAT, 3..5 LOOP, 6 SCALE OFF. WHAT 0 with P_MSND / P_MPAT set is the
 * 0.6.43 MUTATE as it was (SOUND / PATTERN every OFF 1 2 4 8 16 32 bars: every old project plays the same) */
/* (after V10.7.2, the owner: "many sounds don't benefit from velocity, basses, certain leads") EDIT > VOICE 2's VEL, the sound's
 * velocity amount, in MUTATE's old P_MCFG (retired with MUTATE in V10.6, read by nothing since): 127 (its stored default:
 * every project, preset and GENE song before it) 100 %, the velocity as played; less: each note's distance from 100
 * scaled by VEL / 127, 0 every note at 100 (VELOCITY OFF's) from every source: the keys, the sequencer, the arp, MIDI;
 * the matrix's VEL reads it too. A step's ACCENT keeps its 127, HUMAN and RATCHET's DECAY act on the scaled value;
 * the recording keeps the key's own (seq.c tvel, STEP_VEL) */
#define P_TVEL P_FM4_LEVEL
#define P_MORPH P_FM3_ATK              /* (JIANT 0.6.45) EDIT > MORPH: ANALOG PHASE LOFI's wave on towards the next ones
                                         * (0: as WAVE has it; every project and preset before it), in DIGITAL's slot */
#define P_CLEN P_FM3_DEC               /* (JIANT 0.6.47) SHIFT > C.LEN: the step components as a lane of their own, steps
                                         * 1..C.LEN's read in turn as the track plays (polymetric over the steps); 0 OFF
                                         * (each step its own: every project and preset before it); seq.c comp_lane */
#define P_DXIMG P_FM3_REL              /* (JIANT 0.6.48) a DRUM track's EDIT > IMAGE: one knob through the kit's pan maps
                                         * (eng_drum.c dx_img_pan; 0: every sound in the middle, as before) */
/* (JIANT 0.6.81) SCL > CHORD (seq.c chord_build), in DIGITAL's OP4 slots (0, every project and preset before it: OFF,
 * CLOSE, no strum) */
#define P_CHRD P_FM4_ATK               /* OFF; in the scale: TRIAD 7TH ADD9 SUS2 SUS4; any key: MAJ MIN 7 MAJ7 MIN7 POWER */
#define P_CVOI P_FM4_DEC               /* CLOSE, LEAD (each chord the inversion nearest the last), OPEN, +BASS */
#define P_CSTR P_FM4_REL               /* STRUM: 0 at once; up to ~63 ms between the notes, low to high */
/* (JIANT 0.6.87) SEQ > HUMAN, in DIGITAL's OP1 SUS / LEVEL and OP2 SUS slots (127, every project and preset before
 * it: shown as 0 %, none): how much each note's VELOCITY, TIME (a little late) and GATE vary at random as the sequencer
 * plays them (seq.c hum_amt); stored the other way round, 127 - the amount */
#define P_HVEL P_FM1_SUS
#define P_HTIM P_FM1_LEVEL
#define P_HGAT P_FM2_SUS
#define P_HRQ P_FM2_LEVEL              /* (0.6.88) SEQ > TIMING's REC Q: how much live recording snaps to the grid, 127 (every
                                         * project before it) 100 %: on it; less: the notes keep that much of where they were */
/* (JIANT 0.7.8) three more in DIGITAL's last slots; SUS and LEVEL slots hold 127 in every project and preset before them,
 * so 127 is each one's "as before": DRIFT and DUST read 127 as 0 % (F_PCTINV, as HUMAN), R.DEPTH 127 as 100 % */
#define P_DRIFT P_FM3_SUS              /* EDIT > MORPH KNOB 2: each voice's pitch (and filter) wandering slowly, its own way */
#define P_WDUST P_FM3_LEVEL            /* WHEEL's EDIT > ROTOR: an old organ's crackle and hiss */
#define P_WRDEP P_FM4_SUS              /* .. the rotor's depth (the Leslie's swirl), 100 % as before */
#define P_ELOOP P_ED_FX                /* (JIANT 0.4) ENV LOOP: OFF / ON (voice.c env_tick) */
/* (JIANT 0.8.5) VOICE's words (sing.c), EDIT > WORDS, in slots of other engines' (0 there in every project and preset
 * before, on a VOICE track: OFF, TEXT): LANG OFF ES EN JA in LOFI's RTRG slot, LYRIC (TEXT, W1..W16, S1..S8) in DRUM's
 * IMAGE slot; a preset sets both (preset_t.xp) */
#define P_SIMG P_LN0                   /* (V10.49) a synth's EDIT > STEREO IMAGE, stored inverted (127 = 0 %: the lane level's
                                         * 127 every synth part holds); DRUM-X's lane 1 level there. (P_DXIMG is VOICE's LYRIC) */
#define P_VLANG P_LRTRG
#define P_DKEYMODE P_LRTRG        /* DRUM-X only: 0 classic, 1 extended performance octaves. */
#define P_VLYR P_DXIMG
/* (V10.6y, alex.knip) VOICE's CONS (EDIT > WORDS KNOB 3): its consonants' length, 50 .. 400 %, in DRUM-X's lane 2 level
 * slot. 127 there (what every synth part holds) is 100 %, so a project or preset of before sings as it did: the knob's
 * place (vcons_rank, params.c param_turn) is the value + 33, mod 128 (127 at 32: 50 .. 98 % left of it, 103 .. 400 %
 * right). vcons_q6: the length, 1/64 (sing.c sy_enter) */
#define P_VCONS P_LN1
/* (V10.6z4) a synth's expression keys as the sequencer recorded them (seq.c kx_block, ui_input.c kx_frame), in DRUM-X's
 * lane 8 level (127 every synth part holds): 127 - (bit 0 OCT- VIBRATO, bit 1 OCT+ BEND), automation only */
#define P_XPR P_LN7
static inline uint32_t vcons_rank(int32_t v) { return ((uint32_t)v + 33u) & 127u; }
static inline uint32_t vcons_q6(int32_t v)
{
    uint32_t r = vcons_rank(v);
    return r < 32u ? 32u + r : 64u + (r - 32u) * 192u / 95u;
}
/* (JIANT 0.6.9) AUTO PAN (fx.c mix_part), in the slots of the old per-track SLICER (one on a bus since 0.6.3, its
 * slots stored, never read but by proj_sl_migrate). Laid so what old stores hold means OFF: DPTH where SLCR was (0, or
 * 1..2 from a project of before 0.6.3: inaudible), RATE where PAT was (1..16: slow), WAVE where RATE was (1: TRI), SYNC
 * where DEPTH was (127: BEAT) */
#define P_APDEP P_SLCR                 /* depth 0..127 (0: off) */
#define P_APRATE P_SLPAT               /* FREE: 0..127 as the LFO's RATE; BEAT: 0..7, 4 BARS .. 1/32 (fx.c AP_SIXT) */
#define P_APWAVE P_SLRATE              /* SIN TRI SAW SQR S&H (N_LWAVE) */
#define P_APSYNC P_SLDEPTH             /* FREE / BEAT (any value but 0: BEAT) */

enum {                          /* global parameters */
    G_BPM, G_SWING, G_CLOCK, G_TUNE,
    G_DTIME, G_DFDBK, G_DCOLOR, G_DMIX,
    G_RSIZE, G_RDAMP, G_CRATE, G_CDEPTH,
    G_MIDI, G_DREL, G_ROUTE, G_INFO,   /* G_DREL (JIANT): DUCK's release; was G_SYNC, a placeholder ("--", 0..0) */   /* G_ROUTE: MIDI IN, 0 CH1-6 (channels 1..6 -> parts 1..6, 7..16 ignored), 1 SEL (seq.c) */
    G_SLOT, G_DJF, G_LOAD, G_SAVE,  /* G_DJF (0.9.1): GLO > MIXER KNOB 4, the master DJ FILTER (fx.c djf_block), -64 .. 63:
                                 * 0 out of the chain, below a low-pass sweeping down, above a high-pass sweeping up. Was
                                 * G_NAME, a placeholder ("--", 0..0, never set): every project before holds 0 there */
    G_ENGSEL, G_CLIP,           /* no page: the editor switches the engine with a SET of G_ENGSEL. G_CLIP (JIANT):
                                 * the master clipper; was G_ENGGO, unused (0..0) */
    G_CLRSEQ, G_INITSND,
    G_RTYPE,                    /* REVERB TYPE: 0 ROOM, 1 SPRING (fx.c). Was G_DRCH, the GM drum part's MIDI
                                 * channel (inert since 1.0, never read); projects of formats before FUN7 load it
                                 * as ROOM (project.c proj_rtype_room) */
    G_PUNCH, G_DUCK,            /* (JIANT) the drum bus's PUNCH and the kick's DUCK of the synths (fx.c, eng_drum.c).
                                 * Were G_DRLVL / G_DRREV, inert since 1.0 (0..0): the GM drum part's level and
                                 * reverb send, read only by the import of a project of before 1.0
                                 * (proj_drums_to_part, which clears them after) */
    G_NSTORE,                   /* the 27 a project's header holds (FUN7 on: their ids kept) */
    G_DHPF = G_NSTORE,          /* (JIANT, FUNB: after the punch-in lane, project.c) the delay's feedback low cut (0.4:
                                 * in DLY's TONE; stored, unused); 0.6.9: the CHORUS's STEREO (G_CSTER), */
    G_WIDTH,                    /* the buses' stereo width (the delay's right echo later, the chorus's taps apart), */
    G_RMOD, G_RRATE,            /* the reverb's modulation (ROOM: its combs' lengths; SPRING: its wobble) depth, rate, */
    G_RPRE,                     /* its pre-delay */
    G_RFILT, G_RWIDE,           /* (JIANT 0.3) the reverb's tone (darker / brighter) and width, */
    G_DPIT, G_DSPRY,            /* the delay's grains: their pitch (0: a plain delay) and spray */
    G_STRN,                     /* (JIANT 0.4) every sequence moved this many steps of its scale (seq.c seq_step) */
    G_SLMODE, G_SLPAT, G_SLRATE, G_SLDEP,   /* (JIANT 0.6.3) the SLICER, one on a bus (slicer.c): OFF / GATE / STUT, its */
    G_SLT1, G_SLT6 = G_SLT1 + 5, G_SLFX,    /* pattern, step, depth; which tracks go in (and FX: the delay, reverb and */
    G_SLPLEN, G_SLPQ,                       /* chorus returns); the repeats' pitch sequence: its length, CHR / SCL, */
    G_SLP0, G_SLP15 = G_SLP0 + 15,          /* its 16 steps (semitones) */
    G_DMOD,                     /* (0.6.86) the delay's tape drift (WOW and FLUTTER) depth (project.c: its own block, 'DM') */
    G_DTYPE, G_DPING,           /* (0.6.90) the delay's TYPE (ANALOG / DIGITAL) and PING-PONG (project.c: block 'DT') */
    G_PFXLEN, G_PFXRES,         /* (0.6.99) the punch-in lane's length (1..64 steps) and resolution (1/8 1/16 1/32)
                                 * (pfx.c; project.c: block 'PX') */
    G_DDRV,                     /* (0.9.9-snd2) the delay's DRIVE, 0..100: a soft saturation inside its loop (fx.c dly_drive;
                                 * project.c: block 'DD', after GLUE's) */
    G_DRTIM,                    /* (V10.49) the delay's right TIME as a ratio of the left's: 0 =L (as before), x1/2 .. x2
                                 * (fx.c dly_rate; project.c: block 'DR', the last) */
    G_COUNT
};
#define G_SL_N (G_SLP15 + 1 - G_SLMODE)     /* the SLICER's globals (stored together: project.c, song_chain.c) */
#define G_CSTER G_DHPF                 /* (JIANT 0.6.9) CHORUS STEREO -64..63: 0 as before (G_DHPF's 0), - narrower .. mono */
#define G_SYNC G_DREL                   /* (the old names, for the formats' importers and their tests) */
#define G_ENGGO G_CLIP
#define G_DRLVL G_PUNCH
#define G_DRREV G_DUCK

/* stored parameters of an older layout -> today's P_* order. A store keeps np = the P_COUNT it was
 * written with; common parameters are only ever added just before P_E0, so the first np - 8 are
 * P_LEVEL.. in order and the last 8 are P_E0..P_E7; the parameters a store does not have take def[]
 * (user presets, projects of formats 1 and 2) */
static void params_by_count(int16_t *out, const int16_t *in, uint32_t np, const int16_t *def)
{
    uint32_t i, nc = np - 8u;
    for (i = 0; i < P_E0; i++)
        out[i] = i < nc ? in[i] : def[i];
    for (i = 0; i < 8u; i++)
        out[P_E0 + i] = in[nc + i];
}

/* engine indices the stores name (engines.c ENGINES[]: append-only) */
#define ENGI_ANALOG 0u           /* (JIANT 0.5: named for its FILTER page) */
#define ENGI_PHYS 9u
#define ENGI_DRUM 10u
#define ENGI_GRAIN 8u
#define ENGI_SLICE 13u
#define ENGI_SAMPLE 4u
#define ENGI_ANIMAL 14u          /* (JIANT 0.8.3) eng_animal.c */
#define ENGI_STRING 8u           /* (JIANT 0.9.9-snd5) eng_string.c: Karplus-Strong (GRAIN's number, retired in JIANT) */
#define ENGI_INPUT 15u           /* (V10.54) eng_input.c: the audio input */
#define ENGI_ADD 13u             /* (JIANT 0.9.9-snd5) eng_add.c: additive (SLICE's number, retired in JIANT) */
#define ENGI_PHASE 2u            /* (0.9.9: named for the power-on parts and the display order, engines.c) */
#define ENGI_LOFI 3u
#define ENGI_VOICE 5u            /* eng_formant.c ENG_FORMANT */
#define ENGI_WHEEL 7u
#define ENGI_NOISE 11u
#define ENGI_TRIO 6u             /* (JIANT 0.4) folded into ANALOG (analog_from_trio) */
/* (JIANT) the retired engines: PHYS, and the sample engines SAMPLE GRAIN SLICE (their 123 KB of samples and GRAIN's
 * 27 KB of RAM went to DRUM-X and the effects). Their numbers stay reserved (engines.c ENG_GONE), never offered; a sound
 * of theirs that arrives -- a project, a user preset, the editor -- plays as ANALOG's first preset (SAMPLE's old PERC
 * set as DRUM first: drum_from_perc) */
static inline int eng_gone(uint32_t e)
{
    return e == ENGI_PHYS || e == ENGI_SAMPLE || e == ENGI_TRIO;   /* (0.9.9-snd5: GRAIN's 8 and SLICE's 13 are STRING and ADD) */   /* (0.9.9-snd5: GRAIN's 8 and SLICE's 13 are STRING and ADD) */
}
/* (JIANT 0.4) a sound stored before 0.4 -> today's E values, in place: LOFI (3) {.., SWP, VIB, ARP, TONE} -> {.., CUT
 * (its TONE), VIB, BEND 0, MASK 0}; ANALOG (0) KTR -> INT 0 (the key tracking fixed at its old default). Projects
 * (project.c: the kit's marker below 3, or an older format) and user presets (upreset.c: no UP_V04_MARK) */
static void sound_v04(uint32_t engine, int16_t *e)
{
    if (engine == 3u) {
        e[4] = e[7];
        e[6] = e[7] = 0;
    } else if (engine == 0u) {
        e[7] = 0;
    }
}
/* (JIANT 0.4) TRIO (engine 6) folded into ANALOG: its sound's E values -> ANALOG's, in place, the nearest WAVE (its
 * three saws SAW3, its sync sets SYNC, its ring sets RING, pulses PWM, triangles TRI, noise as NOIS), INT2 as the
 * SYNC / RING ratio (DTN: 16 x (2^(st/12) - 1)), DTN, CUT and RES kept; 1 = it was TRIO (its engine is ANALOG now):
 * projects, user presets, the editor's PRESET and SET (project.c, upreset.c, ui.c). {WAVE INT2 INT3 DTN MODE CUT RES
 * PW} -> {WAVE DTN MIX NOIS CUT RES DRV KTR} */
static int analog_from_trio(uint32_t engine, int16_t *e)
{
    static const uint8_t W[16] = {7, 4, 4, 7, 2, 0, 1, 4, 0, 5, 5, 5, 6, 6, 6, 6};
    static const uint8_t R[25] = {0, 1, 2, 3, 4, 5, 6, 8, 9, 11, 12, 14, 16, 18, 20, 22, 25, 27, 30, 34, 37, 41, 45, 49,
                                  54};   /* (16 x (2^(st/12) - 1), st 0..24) */
    uint32_t w = (uint32_t)e[0] & 15u;
    int32_t st = e[1] < 0 ? -e[1] : e[1];
    if (engine != ENGI_TRIO)
        return 0;
    e[0] = W[w];
    e[1] = W[w] >= 5u ? (int16_t)R[st > 24 ? 24 : st] : (int16_t)(e[3] > 0 ? e[3] : 6);
    e[2] = (int16_t)(w == 8u ? 0 : 64);
    e[3] = (int16_t)(w == 7u ? 60 : w == 8u ? 100 : 0);
    e[4] = e[5];                                        /* CUT */
    e[5] = e[6];                                        /* RES */
    e[6] = 0;
    e[7] = 64;
    return 1;
}
/* (V10.6d) LOFI is MATH, BYTE and FLOAT alone: a sound of a chip wave (WAVE 0..4: PLS TRI SAW NOIS WRAM) -> ANALOG's
 * nearest, its E values in place: one oscillator (MIX 0), PLS SQR, TRI TRI, SAW SAW, NOIS a saw under full NOISE, WRAM
 * SIN; CUT open, no RES DRIVE INT (the FILTER page's CUT RES, the envelope, the mix, the sends stay). A BYTE / FLOAT with
 * CHIP STEP (4) -> 4BIT, the same sound. 1 = it was a chip sound (its engine is ANALOG now): projects (project.c
 * proj_phys_gone), user presets (upreset.c). Idempotent */
static const char U_REG64[] = "";   /* (V10.6d) a desc's unit: a register stored 0..127, shown as semitones from 64
                                     * (64 - v: smaller is higher), "+0" the key played (params.c param_format) */
static int analog_from_chip(uint32_t engine, int16_t *e)
{
    static const uint8_t W[5] = {1, 2, 0, 0, 3};
    uint32_t w = (uint32_t)e[1];
    if (engine != ENGI_LOFI)
        return 0;
    if (w == 5u || w == 6u) {
        if (e[0] < 0 || e[0] > 3)
            e[0] = 0;
        return 0;
    }
    w = w < 5u ? w : 0u;
    e[0] = (int16_t)W[w];
    e[1] = e[2] = e[5] = e[6] = e[7] = 0;
    e[3] = (int16_t)(w == 3u ? 127 : 0);
    e[4] = 110;
    return 1;
}
/* SAMPLE SET 4 was PERC, the GM-mapped drum kit (tools/gen_samples.py); retired after 1.0.2: its index stays
 * (SET / GRAIN SRC 4 is an alias of PIANO; 5..7, once USR1..3, clamp to it). A sound that selected it is the DRUM engine with
 * its default kit (eng_drum.c DRUM_PRESETS[0]: the same GM key map, so its patterns still play as drums): its E
 * values become the kit's, the rest of the sound (mix, sends, matrix, ..) stays. 1 = it was one (its engine is
 * ENGI_DRUM now); user presets (upreset.c up_migrate), factory preset 4 and
 * favourites (ui.c, settings_persist.c). Idempotent */
#define SMP_SET_PERC 4u
#define DRUM_KIT_E {64, 64, 70, 64, 64, 0, 0, 0}   /* {MRPH, TUNE, TONE, DECY, NOIS, FM, WARP, DRV} (eng_drum.c) */
static int drum_from_perc(uint32_t engine, int16_t *e)
{
    static const int16_t KIT[8] = DRUM_KIT_E;
    uint32_t i;
    if (engine != ENGI_SAMPLE || e[0] != (int16_t)SMP_SET_PERC)
        return 0;
    for (i = 0; i < 8u; i++)
        e[i] = KIT[i];
    return 1;
}

/* ------------------------------------------------- voices, engines --- */
enum { V_POLY, V_MONO, V_LEGATO, V_UNISON };   /* P_VOICE */
typedef struct {
    uint8_t note, vel, gate, active;
    uint8_t stage;               /* env: 0 off, 1 attack, 2 decay/sustain, 3 release, 4 given up (fading) */
    uint8_t kfade;               /* (0.6.74) stage 4: the fade's blocks so far (voice.c env_tick) */
    uint16_t fin_d;              /* (0.6.81) a chord glide's new note fading in (seq.c chord_glide); 0.6.82: as its glide
                                  * goes, the distance it set out from (1/16 st; 0 none) */
    int32_t env;                 /* Q24 */
    int32_t env_out;             /* last control-rate amplitude, Q15 */
    int32_t pitch16, pitch_cur;  /* 1/16 semitone, with glide */
    int32_t gstep;               /* glide TIME mode: 1/16 st per control tick, Q8 (0.6.82: slow glides smooth), 0 = RATE */
    int32_t fine;                /* unison detune: phase increment * (1 + fine / 4096) */
    uint32_t ph[3];
    int32_t s[8];                /* engine state (filters, envs) */
    uint32_t age;
    uint8_t mvel;                /* mod.c: the note-on velocity (before UNISON scaling) and RAND of the note */
    uint8_t gacc;                /* (0.6.82) the glide's fraction of a 1/16 st carried (gstep Q8) */
    int8_t henv;                 /* (0.6.92) HUMAN: this note's DEC and REL moved (steps of the knobs; voice.c env_tick) */
    int8_t vjit;                 /* (0.9.8) VOICE words: this glottal period's jitter / shimmer (eng_formant.c; padding) */
    int16_t mrnd;
    int16_t gfrom;               /* (0.9.8) where the glide set out from (1/16 st; VOICE sings it as an S-curve: voice.c
                                  * glide_sung; in the struct's padding, its size as before) */
    /* (0.9.9 ctl64) a start or a release inside the block, kept for its sample (voice.c voice_begin / voice_off_at;
     * track_render renders the note sounding up to it, then starts or releases the voice there): sub = the sample + 1
     * (0: none; CTL + 1: the next block's first), pn_*: the start as asked (voice_start's arguments and what it reads
     * of the track at the time) */
    uint8_t sub, pn_flags, pn_note, pn_vel;
    uint8_t pn_mvel, pn_glide, pn_tch, env2_st;   /* (V10.22) env2_st: ENV 2's stage (0 off, 1 attack, 2 decay /
                                  * sustain, 3 release; was padding) */
    int8_t pn_henv, pn_dpit;
    int16_t pn_mrnd, pn_dbend;
    uint16_t pn_dbt, pn_fin;
    int16_t pn_bend;             /* PN_BEND: the step's BEND (1/16 st) the started voice glides by (seq.c seq_step) */
    int32_t pn_fine, pn_pcur, pn_gstep;   /* PN_GLIDE: where the glide sets out from, its step (seq.c chord_glide) */
    uint32_t pn_phr, pn_age;
    uint32_t pn_rnd;             /* the note's random draw (VOICE's RAND vowel: core.h vs_rnd), made at the event */
    int32_t env2;                /* (V10.22) ENV 2, Q24 (voice.c env2_tick; a MOD source, mod.c MS_ENV2) */
} voice_t;
#define PN_START 1u              /* pn_flags: a start (else a release) */
#define PN_OLD 2u                /* .. of a voice sounding: its note rendered up to the sample */
#define PN_BEND 4u               /* .. then its BEND set (pn_bend, pn_gstep) */
#define PN_GLIDE 8u              /* .. then its glide in (pn_pcur, pn_gstep, pn_fin) */

typedef struct {                 /* per-voice control-rate modulation, computed in voice.c */
    uint32_t inc;                /* phase increment of the base pitch */
    int32_t pitch16;
    int32_t amp0, amp1;          /* Q15 ramp over the block */
    int32_t cutoff;              /* 0..127 << 8 */
    int32_t shape;               /* 0..127 << 8 */
    int32_t envq15;              /* env value (for engines that use it as a mod source) */
    int32_t env2q15;             /* (V10.22) ENV 2 (mod.c MS_ENV2) */
    int32_t fine;                /* the residual below 1/16 semitone in inc: unison detune, TUNE, bend (1/4096) */
} vmod_t;

typedef struct {
    const char *name;
    int8_t e[8];                 /* P_E0..P_E7 (signed: an interval below the note; every value fits) */
    uint8_t env[4];              /* ATK DEC SUS REL */
    int8_t fenv;                 /* ENV -> FILTER amount (-64..63) */
    uint8_t mono;                /* bit 0: 1 = MONO (bass / lead), 0 = POLY; (p1, main 0.9.12) bits 1..4 its sound type
                                  * (PT_*, TAG) */
    /* the rest of the patch; each value is stored + 1, 0 = the default */
    uint8_t fx[4];               /* DIST, CHORUS, DELAY, REVERB sends */
    uint8_t pat;                 /* suggested pattern (PATTERNS[pat - 1], 0 = none): only a hint. A sound load
                                  * never touches the steps; SEQ > PATTERNS loads a pattern (ui.c pat_load). The
                                  * arp is the track's too: a preset does not set it */
    uint8_t xp;                  /* (JIANT 0.8.5) VOICE: LANG | LYRIC << 2 (P_VLANG, P_VLYR; 0 = OFF, its vowels) */
    const int8_t *mod;           /* (V10.6i) the sound's movement: pairs of a common parameter (< P_E0) and its value, -1
                                  * ends (MODS: the LFO and its DEST, ENV 2, the MOD slots, the FILTER page, GLIDE ..),
                                  * set after the rest of the preset (ui.c apply_preset_to); 0 none. In flash */
} preset_t;
#define MODS(...) .mod = (const int8_t[]){__VA_ARGS__, -1}
/* the MOD slots' SRC and DEST numbers a preset names (mod.c MS_* / MD_*, checked there; the engines come before it) */
enum { PS_LFO = 1, PS_ENV, PS_VEL, PS_KEY, PS_RAND, PS_MODW, PS_AT, PS_NOISE = 14, PS_ENV2 = 16 };
enum { PD_PITCH = 1, PD_CUT, PD_SHP, PD_AMP, PD_PAN, PD_DIST, PD_CHO, PD_DLY, PD_REV, PD_LRATE, PD_VIB, PD_E1,
       PD_FCUT = 30, PD_LEVEL };
#define PD_E(k) (PD_E1 + (k))    /* EDIT k (P_E0 + k: 0 the first, as the engine's edit[]) */
/* a MOD slot k (1..4): SRC DEST AMT; the LFO: WAVE RATE and its depths to PITCH FILTER; ENV 2 */
#define MMOD(k, s, d, a) P_M##k##SRC, (s), P_M##k##DST, (d), P_M##k##AMT, (a)
#define MLFO(w, r, pit, flt) P_LWAVE, (w), P_LRATE, (r), P_LD_PIT, (pit), P_LD_FLT, (flt)
#define MENV2(a, d, s, r) P_E2ATK, (a), P_E2DEC, (d), P_E2SUS, (s), P_E2REL, (r)
enum { LW_SIN, LW_TRI, LW_SAW, LW_SQR, LW_SH, LW_DRIFT };
#define FX(d, c, dl, r) .fx = {(d) + 1, (c) + 1, (dl) + 1, (r) + 1}
#define PAT(n) .pat = (n)
#define WORDS(lang, lyr) .xp = (uint8_t)((lang) | (lyr) << 2)
/* (V10.36) the other engines' xp: BREATHE, the pad drifting by itself: the LFO DRIFT on FILTER (flt 1..63), a little on
 * PITCH too (pit 1), over 16 bars (slow 1) else 4 (ui.c preset_load) */
#define BREATHE(flt, pit, slow) .xp = (uint8_t)((flt) | (pit) << 6 | (slow) << 7)
/* (p1, main 0.9.12, after Skiff by illuminus5) every factory preset's sound type, in the free bits of preset_t.mono (no
 * byte more; presets live in flash, nothing stored changes): PRESETS' LIST browses all engines' presets by it (ui.c
 * preset_in), the song generator's EXPLORE keeps a bass's sounds off the other roles by it (gen.c gen_sound) */
enum { PT_NONE, PT_BASS, PT_LEAD, PT_PAD, PT_KEYS, PT_PLUCK, PT_BELL, PT_VOX, PT_DRUM, PT_FX, PT_N };
#define TAG(t) (PT_##t << 1)
#define PR_MONO(pr) ((pr)->mono & 1u)
#define PR_TAG(pr) ((uint32_t)(pr)->mono >> 1)

struct track;
typedef struct {                 /* an engine (engines.c ENGINES[]; the eng_*.c files) */
    const char *name;            /* "ANALOG" (PRESETS, the editor) */
    const char *page_title[2];   /* EDIT 1 and EDIT 2 */
    param_desc_t edit[8];        /* P_E0..P_E7 */
    const preset_t *presets;
    uint8_t npresets;
    uint8_t knob[4];             /* HOME: the four parameters on KNOB 1..4 */
    uint8_t poly;                /* voice cap for POLY and UNISON, 0 = NVOICE */
    uint8_t sampled;             /* 1 = plays recorded material (a position, not a phase): voice.c keeps
                                  * no phases over a retrigger, spreads none for UNISON, renders at SUS 0 */
    uint8_t keep;                /* bit k: s[k] is kept when a sounding voice is retriggered (filters) */
    uint8_t oneshot;             /* 1 = hits (DRUM): always POLY, no glide; amp ends its voices (eng_drum.c) */
    void (*note_on)(struct track *t, voice_t *v);
    void (*render)(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m);
    /* optional (0 = none): the voice amplitude instead of the ADSR curve, once per control tick;
     * gets the ADSR value (Q15, env_tick already ran: it still gates the voice), returns Q15 */
    int32_t (*amp)(struct track *t, voice_t *v, int32_t adsr);
    /* optional: a mode-dependent descriptor of EDIT k (the same range and default as edit[k],
     * another label / value names), 0 = edit[k] */
    const param_desc_t *(*desc)(const struct track *t, uint32_t k);
    /* optional: once per block and part, before its voices (also with no voice sounding) */
    void (*block)(struct track *t);
    /* optional: the note of key k (0 = the lowest) when the engine maps the keys itself, else -1
     * (then the scale keyboard of seq.c kb_map) */
    int32_t (*keys)(const struct track *t, uint32_t k);
    /* 1 = the engine's own envelopes are the voice's amplitude (FM6): voice.c renders it at 1.0 (no ADSR, no
     * velocity; LFO -> AMP, the matrix's AMP and the fades still apply; ENV is 0) and the voice ends when
     * done() says so (once per control tick, before the render), not at the end of the ADSR's release */
    uint8_t ownenv;
    int (*done)(struct track *t, voice_t *v);
    /* (0.9.9 ctl64) 1 = renders whole blocks only (FM6: its operators' block): a note inside a block starts at the
     * nearer block edge (within CTL / 2), as every engine did with blocks of 32; 0 = a note starts at its sample
     * (voice.c track_render renders the voice from there) */
    uint8_t whole;
} engine_t;

/* ------------------------------------------------- tracks, the song --- */
enum { ST_NOTE, ST_TIE, ST_REST };
#define SF_ACCENT 1u
#define SF_SLIDE 2u
#define SF_RATCH_SH 3u                   /* RATCH: the hits of a NOTE step - 1 (0..3: x1..x4, seq.c seq_ratchet); */
#define SF_RATCH (3u << SF_RATCH_SH)     /* bits 3..4, so a user preset's pattern (flag 4 = tie) carries it too */
#define SF_MFINE_SH 5u                   /* (0.9.1) MICRO's fine part: the 64ths under its sixteenth (ST_MICRO), bits 5..6 */
#define SF_MFINE (3u << SF_MFINE_SH)
#define NLANE 8                  /* drum lanes of a step (the DRUM engine's: eng_drum.c DRUM_LANE_NOTE) */
typedef struct {                 /* acid-style step: up to 4 notes (POLY), time, accent, slide; drum hits */
    uint8_t note[4];
    uint8_t n;                   /* notes in use, 0 = empty */
    uint8_t time;                /* ST_NOTE / ST_TIE / ST_REST */
    uint8_t flags;               /* SF_ACCENT | SF_SLIDE | SF_RATCH */
    uint8_t vel;
    uint8_t hit;                 /* bit l: lane l hits (its GM note, on any engine): the DRUM grid */
    uint8_t acc;                 /* bit l: that hit is accented (velocity 127) */
    uint8_t probability;         /* 0 = legacy 100%; 1..100 = percent, 101 = silent */
    uint8_t comp, carg;          /* (JIANT 0.6.39) the step's component (CP_*, 0 none) and its arguments (seq.c) */
    uint8_t comp2, carg2;        /* (0.6.82) a second component (SEQ > COMP 2), the same way */
    uint8_t cwhen;               /* (0.6.82) WHEN each plays: bits 0-1 comp's, 2-3 comp2's (CW_*) */
    uint8_t ctime;               /* (0.6.82) bits 0-2 DIV: RATCH's and ECHO's parts a step/2 .. /8 apart (0: the step
                                  * shared out, as before); bits 3-6 LEN: a part's gate n/16 (0: the track's GATE) */
    uint8_t lm[3];               /* (0.8.5) each drum lane's own way of playing on this step (a DRUM track: its hit and
                                  * its notes on the lane): 3 bits a lane, lane l at bits 3l..3l+2 of the 24, LM_* */
    uint8_t clen[2];             /* (0.9.9-seq2d) components v2: C1's / C2's LENGTH - 1 (0..31; CL_LEGACY: a CP_* still
                                  * to migrate, seq.c comp_migrate) */
    uint8_t cnm[2];              /* .. their N:M: N - 1 | (M - 1) << 4 */
} step_t;
/* (0.9.9-seq2h) the main loop's scratch: buffers each used only inside one call of its own, never across a frame nor
 * from an interrupt, and none of those calls reaching another's (one storage, 4.5 KiB of .bss back):
 *   tl_src    ui_tools.c: the steps as they were, taken at the start of a SEQ TOOLS action and read by that action
 *   gen_keep  gen.c gen_parts: the drums of the section before, while DICE makes the sections
 *   row_tmp   song_main.c row_vary: a row's track's steps as stored, while its variation is written (was its own 1.5 KiB
 *             of the pool)
 *   ota       ota.c ota_stage: the package's header in step 1, then its blocks (the main loop is inside the update)
 *   snap      ui_graph.c scope_at: the HOME / EDIT scope's 512 samples, copied from the audio's ring for one draw */
#define MSCR 1
static union {
    step_t tl_src[NSTEP];
    step_t gen_keep[NSTEP];
    step_t row_tmp[NSTEP];                              /* (song_main.c row_vary: the steps as they were) */
    union { uint8_t hdr[0x400]; struct { uint8_t b[512], mine[512]; } blk; } ota;   /* (hdr: step 1 only) */
    int16_t snap[512];
} mscr;
/* (0.8.5) a lane's mode (eng_drum.c, the DRUM-X octaves): its hit ratcheted x2 / x4 / x8 over the step, or struck SHORT,
 * GHOST, FLAM; that lane only. 0: as ever (every step before 0.8.5) */
enum { LM_NONE, LM_X2, LM_X4, LM_X8, LM_SHORT, LM_GHOST, LM_FLAM, LM_N };
/* (p1, main 0.9.16 / 0.9.18) LM_ONCE (the code left, 7): a hit recorded live into a step that repeats (RATCH, a RATCHET
 * component, the global lane's), on a lane the step did not play, or a note added live to such a POLY step (its slot:
 * the note's index): struck once, as it was heard, not in the step's repeats (seq.c rec_drum, rec_note, seq_step). Read
 * as none by step_lmode (and by every firmware before: there it rolls with the step, as it did) */
#define LM_ONCE 7u
static uint32_t step_lm_raw(const step_t *s, uint32_t l)
{
    return (((uint32_t)s->lm[0] | (uint32_t)s->lm[1] << 8 | (uint32_t)s->lm[2] << 16) >> (3u * (l & 7u))) & 7u;
}
static uint32_t step_lmode(const step_t *s, uint32_t l)
{
    uint32_t m = step_lm_raw(s, l);
    return m < LM_N ? m : LM_NONE;
}
static void step_set_lmode(step_t *s, uint32_t l, uint32_t m)
{
    uint32_t v = (uint32_t)s->lm[0] | (uint32_t)s->lm[1] << 8 | (uint32_t)s->lm[2] << 16, sh = 3u * (l & 7u);
    v = (v & ~(7u << sh)) | ((m & 7u) << sh);
    s->lm[0] = (uint8_t)v; s->lm[1] = (uint8_t)(v >> 8); s->lm[2] = (uint8_t)(v >> 16);
}
static int step_lm_any(const step_t *s) { return s->lm[0] | s->lm[1] | s->lm[2]; }
static uint32_t lm_hits(uint32_t m) { return m == LM_X2 ? 2u : m == LM_X4 ? 4u : m == LM_X8 ? 8u : 1u; }   /* a step */
static uint32_t lm_touch(uint32_t m) { return m >= LM_SHORT && m <= LM_FLAM ? m - LM_SHORT + 1u : 0u; }   /* TOUCH 1..3 */
/* (0.6.92) EVERY: on which loops of the pattern the step's components play (both of them; seq.c every_on), in cwhen's
 * bits 0-3: ALWAYS, 1:2 (the odd loops), 2:2, SKIP (kept, never), 1:3 2:3 3:3, 1:4 2:4 3:4 4:4, 1:8, FIRST, NOT FIRST.
 * (0.6.82 WHEN: bits 0-1 the first component's ALWAYS / 1:2 / 2:2 / SKIP, the same codes; bits 2-3 the second's, now
 * read as the step's: a project that set WHEN B alone reads another EVERY) */
enum { EV_ALWAYS, EV_1_2, EV_2_2, EV_SKIP, EV_1_3, EV_2_3, EV_3_3, EV_1_4, EV_2_4, EV_3_4, EV_4_4, EV_1_8, EV_FIRST,
       EV_NOT1, EV_N };
static const uint8_t EV_AB[EV_N][2] = {{0, 0}, {1, 2}, {2, 2}, {0, 3}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4},
                                       {4, 4}, {1, 8}, {0, 1}, {0, 2}};   /* a of b loops; {0,0} always, {0,3} never,
                                                                          * {0,1} the first, {0,2} not the first */
static const uint8_t EV_ORD[EV_N] = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 3};   /* the knob's order, SKIP last */
#define ST_EVERY(s) ((uint32_t)(s)->cwhen & 15u)
#define ST_EVERY_SET(s, e) ((s)->cwhen = (uint8_t)(((s)->cwhen & 0xF0u) | ((uint32_t)(e) & 15u)))
/* (0.6.88) TIMING: the step's own place off the grid (0 on it); recorded off the grid (REC Q) or set on SEQ > NOTE's
 * MICRO. seq.c: late by a queue, early by ending the step before it that much sooner. (0.9.1) -32 .. +31 64ths of a
 * step (~2 ms a 64th of a 1/16 at 120 BPM; was -8 .. +7 sixteenths): the sixteenth below it in cwhen's bits 4-7 as
 * always (stored with it, project.c; what firmware before 0.9.1 plays), the 64ths above that (0..3) in flags' bits 5-6
 * (SF_MFINE: FUNC's block 'M' 'F'). A step of before: its sixteenths x 4, exactly */
#define MICRO_MIN (-32)
#define MICRO_MAX 31
static inline int32_t st_micro(const step_t *s)
{
    return ((int32_t)(int8_t)s->cwhen >> 4) * 4 + (int32_t)((s->flags & SF_MFINE) >> SF_MFINE_SH);
}
static inline void st_micro_set(step_t *s, int32_t m)   /* m: MICRO_MIN .. MICRO_MAX */
{
    s->cwhen = (uint8_t)((s->cwhen & 15u) | (((uint32_t)m >> 2) & 15u) << 4);
    s->flags = (uint8_t)((s->flags & ~SF_MFINE) | ((uint32_t)m & 3u) << SF_MFINE_SH);
}
#define ST_MICRO(s) st_micro(s)
#define ST_MICRO_SET(s, m) st_micro_set((s), (m))
#define CT_DIV(s) ((uint32_t)(s)->ctime & 7u)
#define CT_LEN(s) (((uint32_t)(s)->ctime >> 3) & 15u)
#define CT_NDIV 6u
static const uint8_t CT_DIVS[CT_NDIV] = {0, 2, 3, 4, 6, 8};
/* (JIANT 0.6.39) step components, OP-Z style (SEQ > COMP): one a step, on a NOTE step of a melodic track.
 * ARP NOTES / SEQ / SCALE: the step plays an arpeggio, RATE notes in its length, over the TIEs after it; its notes
 * (NOTES: the step's own, SEQ: every note of the track's pattern, both over RANGE octaves; SCALE: the scale up from
 * the step's lowest note, RANGE: 3 5 8 15 degrees) in the ORDER. TRIG: the step plays on some loops of the pattern
 * only (1:2 the first of every two, .., FIRST, NOT FIRST). carg: ARP ORDER bits 0-1, RATE 2-4, RANGE 5-6; TRIG the
 * condition. */
/* (0.6.42) RAND: the notes up / down the scale at random (carg 1..7 degrees); OCT: an octave (carg: +1, -1, +1 every
 * other loop, -1 every other loop, + / - by turns, at random); TONAL: the rest of the loop moved (carg 0..14: -7..+7
 * degrees); BEND: the notes glide over the step (carg 0..24: -12..+12 semitones); ECHO: repeats fading (carg 0..2:
 * x2..x4; 0.6.82: .. x8); PULSE: its notes again on the empty steps after it (carg 0..3: every 1..4); JUMP: then step carg + 1;
 * REV: back from here to step 1 (ping-pong); FILL: only while FX is held; KILL: what the track sounds cut first */
enum { CP_NONE, CP_ARPN, CP_ARPQ, CP_ARPS, CP_TRIG, CP_RAND, CP_OCT, CP_TONAL, CP_BEND, CP_ECHO, CP_PULSE, CP_JUMP,
       CP_REV, CP_FILL, CP_KILL, CP_WALK, CP_CLIMB, CP_TOUCH, CP_N };
/* (0.8.5) TOUCH: on a DRUM track how the step's hits are struck (carg 0..2: SHORT, half the decay; GHOST, short, darker
 * and softer; FLAM, a soft grace hit ~20 ms before the hit; eng_drum.c DXO_*): what a hit played in the octaves below
 * the kit's records (OCT -1 .. -3, MIDI below 35: seq.c dxo_on). Other engines: nothing. ECHO's carg bit 7, EVEN
 * (0.8.5): its repeats all at the step's level, a ratchet past RATCH's x4 (OCT +3 on a kit records x8 so) */
#define CP_ECHO_EVEN 0x80u
/* (0.6.98) WALK and CLIMB (after the OP-Z's and OP-XY's evolving components): the step's notes move one degree of the
 * scale (WALK) or one octave (CLIMB) every RATE loops of the pattern, over a RANGE of moves, then round again: UP,
 * DOWN, UP-DN (there and back) or RND (a random place a move, the same every time round). carg: bits 0-3 RATE
 * (CP_WRATES: 1..10, 12, 16 loops a move), bits 4-5 MODE, bits 6-7 RANGE (WALK: 3 5 7 12 degrees; CLIMB: 1 2 3 4
 * octaves). Both stack with each other and with the rest (TONAL, OCT, RAND, an ARP): four notes, two of these, and
 * the pattern comes back to itself after RATE x RANGE loops, different every time round */
#define CP_WRATE(a) ((uint32_t)(a) & 15u)
#define CP_WMODE(a) (((uint32_t)(a) >> 4) & 3u)
#define CP_WRNG(a) (((uint32_t)(a) >> 6) & 3u)
#define CP_WARG(r, m, g) ((uint8_t)(((r) & 15u) | ((m) & 3u) << 4 | ((g) & 3u) << 6))
#define CP_NWRATE 12u
static const uint8_t CP_WRATES[CP_NWRATE] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 16};
static const uint8_t CP_WDEGS[4] = {3, 5, 7, 12};
/* the range of carg each component's KNOB 3 turns (ARP: KNOB 3 / 4 their own; WALK CLIMB: theirs) */
static const uint8_t CP_AMAX[CP_N] = {0, 0, 0, 0, 11, 7, 5, 14, 24, 6, 3, 63, 0, 0, 0, 0, 0, 2};
static const uint8_t CP_ADEF[CP_N] = {0, 0, 0, 0, 0, 2, 0, 9, 7, 1, 0, 0, 0, 0, 0, 0x40, 0x00, 0};
static const uint8_t CP_AMIN[CP_N] = {0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static int cp_iswalk(uint32_t c) { return c == CP_WALK || c == CP_CLIMB; }
#define CP_ORD(a) ((uint32_t)(a) & 3u)            /* UP DOWN UP-DN RND */
/* (0.6.62) JUMP / REV: (V10.6v) gone, a legacy one migrates to none */
#define CP_RATE(a) (((uint32_t)(a) >> 2) & 7u)    /* 0..4: x2 x3 x4 x6 x8 */
#define CP_RNG(a) (((uint32_t)(a) >> 5) & 3u)     /* 0..3: 1..4 octaves (SCALE: 3 5 8 15 degrees) */
#define CP_ARG(o, r, g) ((uint8_t)(((o) & 3u) | ((r) & 7u) << 2 | ((g) & 3u) << 5))
#define CP_NRATE 6u                                 /* (0.6.92: x1, a note a step over the TIEs, as index 5) */
#define CP_NTRIG 12u
static const uint8_t CP_RATES[CP_NRATE] = {2, 3, 4, 6, 8, 1};
static const uint8_t CP_RATE_ORD[CP_NRATE] = {5, 0, 1, 2, 3, 4};   /* the knob's order: x1 x2 x3 x4 x6 x8 */
static const uint8_t CP_DEGS[4] = {3, 5, 8, 15};
static const uint8_t CP_TRIGS[CP_NTRIG][2] = {{1, 2}, {2, 2}, {1, 3}, {2, 3}, {3, 3}, {1, 4}, {2, 4}, {3, 4}, {4, 4},
                                              {1, 8}, {0, 1}, {0, 2}};   /* (a of b; {0, 1} FIRST, {0, 2} NOT FIRST) */
static int cp_isarp(uint32_t c) { return c >= CP_ARPN && c <= CP_ARPS; }
/* (0.9.9-seq2d) STEP COMPONENTS v2 (the approved mock-up's chapter 4): the CP_* above are the legacy codes, read only by
 * the migration (seq.c comp_migrate: projects before seq2d, GEN and DICE write them and migrate). A component now is
 * ACTION + LENGTH + CONDITION + VALUE (SEQ > COMP / COMP 2 KNOB 1..4): comp / comp2 hold ACTION (bits 0-3) and
 * CONDITION (4-7), carg / carg2 the VALUE, clen[] LENGTH - 1 (the steps it stays on, from its own on, as the track
 * plays them), cnm[] N:M. Two a step: C1 first, then C2; C2's condition ELSE is MONO (it acts only when C1 did not),
 * any other POLY (both stack: C1 on the step, C2 over the result). seq.c comp_run */
enum { CA_NONE, CA_ARP, CA_SEQ, CA_OCTU, CA_OCTD, CA_RATCH, CA_STUT, CA_MUTE, CA_TRANS, CA_RAND, CA_LOCK, CA_RAMP,
       CA_SOUND, CA_BEND, CA_SHIFT, CA_CHORD, CA_N };   /* (V10.6v: RAMP took JUMP's code, 11) */
enum { CC_ALWAYS, CC_P25, CC_P50, CC_P75, CC_FIRST, CC_NFIRST, CC_FILL, CC_NFILL, CC_PRE, CC_NPRE, CC_NEI, CC_NNEI,
       CC_ELSE, CC_NM, CC_NNM, CC_N };
#define CO_ACT(c) ((uint32_t)(c) & 15u)
#define CO_COND(c) ((uint32_t)(c) >> 4)
#define CO_MAKE(a, c) ((uint8_t)(((a) & 15u) | ((c) & 15u) << 4))
#define CL_LEGACY 0xFFu
#define CL_MAX 32u                                  /* LENGTH 1..32 */
#define CNM_N(v) (((uint32_t)(v) & 15u) + 1u)
#define CNM_M(v) (((uint32_t)(v) >> 4) + 1u)
#define CNM_MAKE(n, m) ((uint8_t)((((n) - 1u) & 15u) | (((m) - 1u) & 15u) << 4))
/* VALUE: ARP CP_ARG's ORDER RATE RANGE, bit 7 SCALE (else the step's NOTES); SEQ bits 0-5 the note it starts from, bit 6
 * BOUNCE; OCT+ / OCT- 0..2: 1..3 octaves; RATCHET bits 0-2 x2..x8, bits 3-4 DECAY (0 flat, 1 down, 2 up); STUTTER 0..3: again
 * every 1..4 steps; MUTE 0 MUTE, 1 CUT; TRANSPOSE bits 0-3 the move -7..+7 (7: 0), bits 4-7 RANGE - 1 (RANGE 1: FIX, the
 * move whenever the condition holds; 2..16: it adds up each pass the condition holds, round RANGE places, acting every
 * pass); RANDOM 0..6: 1..7 degrees; RAMP CV_RAMP_*; SOUND 0..2 SHORT GHOST FLAM (a kit); BEND 0..24 -12..+12 */
#define CV_ARP_SCALE 0x80u
#define CV_SEQ_BOUNCE 0x40u
#define CV_RAT_HITS(v) (2u + ((uint32_t)(v) & 7u) % 7u)
#define CV_RAT_DEC(v) ((((uint32_t)(v) >> 3) & 3u) % 3u)
#define CV_TR_MOVE(v) ((int32_t)((uint32_t)(v) & 15u) % 15 - 7)
#define CV_TR_RANGE(v) (((uint32_t)(v) >> 4) + 1u)
#define CV_TR_MAKE(mv, rng) ((uint8_t)(((uint32_t)((mv) + 7) & 15u) | (((rng) - 1u) & 15u) << 4))
/* (V10.6v) RAMP (seq.c ramp_run): the step's parameter LOCK glides there from FROM (KNOB: the knob's value; OPP: the
 * other end of the parameter's range) along CURVE over its LENGTH (clen + 1 of the track's steps, or bars with BARS: 1..32
 * steps, 1..16 bars), once a launch: the step again while it runs does not restart it (shorter than the pattern: it
 * ends first, each pass); then (and at a stop) the knob's value back. No lock on the step: nothing. VALUE bits 0-3 KNOB 4's
 * place: CURVE (LIN EXP LOG S STEPS ZIGZAG) from KNOB, then the six from OPP; bit 4 BARS; anything else (a JUMP of
 * before) is no RAMP (comp_migrate) */
#define CV_RAMP_NC 6u
#define CV_RAMP_BARS 16u
#define CV_RAMP_OPP(v) (((uint32_t)(v) & 15u) >= CV_RAMP_NC)
#define CV_RAMP_CURVE(v) (((uint32_t)(v) & 15u) % CV_RAMP_NC)
#define CV_RAMP_OK(v) ((uint32_t)(v) < 32u && ((uint32_t)(v) & 15u) < 2u * CV_RAMP_NC)   /* (BARS: 1..16, clen % 16) */
static const uint8_t CA_VMAX[CA_N] = {0, 191, 127, 2, 2, 20, 3, 1, 239, 6, 0, 11, 2, 24, 126, 10};   /* KNOB 4's list, 0.. */
static const uint8_t CA_VDEF[CA_N] = {0, 0x08, 0, 0, 0, 0, 0, 0, 0x38, 1, 0, 0, 0, 14, 63, 0};    /* (stored VALUE) */
static int comp_any(const step_t *s) { return CO_ACT(s->comp) || CO_ACT(s->comp2); }
typedef struct {                 /* a step as formats 1..4 (projects to FUN4) stored it: no hits */
    uint8_t note[4];
    uint8_t n, time, flags, vel;
} step8_t;

/* Probability keeps zero-initialized and legacy patterns at 100%. */
static uint32_t step_chance(const step_t *s) { return !s->probability ? 100u : s->probability <= 100u ? s->probability : 0u; }
static void step_set_chance(step_t *s, uint32_t chance) { s->probability = (uint8_t)(chance >= 100u ? 0u : chance ? chance : 101u); }
/* RATCH: a NOTE step plays its notes and hits this many times (1..4), in equal parts of the step; every older
 * pattern holds 0 there, x1 */
static uint32_t step_ratchet(const step_t *s) { return ((s->flags & SF_RATCH) >> SF_RATCH_SH) + 1u; }
static void step_set_ratchet(step_t *s, uint32_t hits)
{
    s->flags = (uint8_t)((s->flags & ~SF_RATCH) | ((hits < 1u ? 0u : hits > 4u ? 3u : hits - 1u) << SF_RATCH_SH));
}
#define MOTION_MAX 64u
/* Four tracks x64 steps fit one byte (JIANT 0.6, six tracks: the track's bit 2 is value's bit 12 flipped against its
 * sign, MOTION_TRK; values stay within -64..127, so an old record reads as track 0..3). Values retain their signed parameter range.
 * param: the P_* id (< 128: P_COUNT is at most 127, project.c), bit 7 (MOTION_LOCK, 1.1) a parameter lock: the value
 * sounds on that step only and goes back after it (motion.c motion_step); without it an automation event, the
 * value holds until another one changes it. One record per (track, step, id), of either kind */
#define MOTION_LOCK 0x80u
#define MOTION_ID(e) ((uint32_t)(e)->param & 0x7Fu)
#define MOTION_HI(e) ((uint32_t)(((e)->value >> 12) ^ ((e)->value >> 15)) & 1u)
#define MOTION_TRK(e) ((uint32_t)(e)->place >> 6 | MOTION_HI(e) << 2)
#define MOTION_STEP(e) ((uint32_t)(e)->place & 63u)
#define MOTION_PLACE(e) (MOTION_TRK(e) << 6 | MOTION_STEP(e))   /* track << 6 | step */
/* (V10.5) bit 13 against the sign: a RAMP, the value glides there step by step from the record before (of its track and
 * id; none: from step 0 and the sound's value); an old record has it clear, a hold */
#define MOTION_RAMP(e) ((uint32_t)(((e)->value >> 13) ^ ((e)->value >> 15)) & 1u)
#define MOTION_VAL(e) ((int16_t)((e)->value ^ (int32_t)(MOTION_HI(e) << 12) ^ (int32_t)(MOTION_RAMP(e) << 13)))
/* (0.9.1) MOT_DJF: a record whose id is past every P_* is the master DJ FILTER's (G_DJF), automation only (no lock):
 * recorded on the selected track (GLO > MIXER KNOB 4), it sets song.g[G_DJF] at its step and the value holds as any
 * event's; a stop or the loop's restart puts back the knob's (motion.c). Stored in FUNC's block 'M' 'G' (project.c) */
#define MOT_DJF 127u
_Static_assert(P_COUNT < MOT_DJF, "MOT_DJF past every track parameter");
typedef struct { uint8_t place, param; int16_t value; } motion_event_t;
typedef struct { uint8_t count, on, rsv[2]; motion_event_t event[MOTION_MAX]; } motion_store_t;
static void motion_at(motion_event_t *e, uint32_t place, int16_t v)   /* place: track << 6 | step; v: the value */
{
    e->place = (uint8_t)(place & 255u);
    e->value = (int16_t)(v ^ (int32_t)(((place >> 8) & 1u) << 12));
}
_Static_assert(sizeof(motion_store_t) == 260u, "motion disk layout");
#define CHAIN_ROWS 16u
typedef struct { uint8_t slot, bars; } chain_row_t;   /* a song row: section A..D (a project slot), 1..CHAIN_BARS bars */
#define CHAIN_BARS 64u
typedef struct {
    uint8_t count, rsv[3];
    chain_row_t row[CHAIN_ROWS];
} chain_config_t;

#define CSP_N 4                  /* (0.9.9-seq2d) spans of components a track at once (a fifth: the oldest goes) */
typedef struct { uint8_t act, val, left, len; int8_t mv; uint8_t base; } cspan_t;
typedef struct track {
    int16_t p[P_COUNT];
    uint8_t engine, preset;      /* engine: what the audio ISR renders */
    uint8_t eng_req;             /* engine the UI asked for (the ISR switches at a block start) */
    uint8_t user;                /* user preset slot + 1 the sound came from (UI), 0 = none */
    int16_t rh_p;                /* (V10.6z4, in what was padding) a recorded held note: where it was played, samples from
                                  * its step's place (seq.c rec_end: its length) */
    voice_t v[NVOICE];
    /* LFO */
    uint32_t lfo_ph;
    int32_t lfo_val;             /* Q15 */
    int32_t lfo_fade;            /* Q15 ramp after note-on */
    uint32_t lfo_rnd;
    /* keyboard / arp input: held notes in press order */
    uint8_t held[16];
    uint8_t nheld;
    uint8_t arp_phys;            /* keys physically held for the arp */
    uint8_t latched;             /* HOLD: keep notes after release */
    /* arp runtime */
    uint32_t arp_pos;            /* q8 samples into the current arp step */
    uint32_t arp_idx;
    uint8_t arp_note;            /* sounding arp note, 0 = none */
    uint8_t arp_walk;            /* WALK: the place in the note list it stands on */
    uint8_t arp_ch[3];           /* CHORD: the other notes sounding with arp_note, 0 = none */
    uint32_t arp_off;            /* q8 sample time of its note-off */
    /* sequencer */
    step_t step[NSTEP];
    uint32_t seq_pos;            /* q8 samples into the current step */
    uint16_t seq_idx;
    uint8_t seq_notes[4 + NLANE];   /* sounding seq notes (the step's notes, then its hits) */
    uint8_t seq_n;
    uint8_t seq_hold;            /* last step slides: keep the notes until the next step */
    uint8_t slide_glide;         /* next legato note glides (slide) */
    uint8_t rat_left;            /* RATCH: repeats of the playing step still to come (seq.c seq_ratchet) */
    uint8_t arp_from;            /* (0.6.39) an ARP component playing: its step + 1 (0: none), over the TIEs after it */
    uint8_t arp_k;               /* .. the arpeggio's next note (its position in the ORDER) */
    uint16_t loops;              /* (0.6.39) the pattern's loops since PLAY (TRIG components) */
    /* (0.9.9-seq2d) components v2 (seq.c comp_run): the spans playing (the oldest first), what they do to the step
     * playing (cx_*), the last condition's result (PRE; the next track's NEI) and the count of TRANSPOSE's passes */
    cspan_t csp[CSP_N];
    uint8_t csp_n, c_pre, c_acc;
    int8_t cx_deg, cx_oct;       /* .. TRANSPOSE's degrees, OCT's semitones on the step playing */
    uint8_t cx_rnd, cx_rat, cx_dec, cx_tch, cx_bend, cx_arp, cx_lock;   /* RANDOM's degrees, RATCHET's hits and DECAY,
                                  * SOUND (1..3), BEND (1 + 0..24), an ARP from this step, a LOCK held over it */
    uint8_t jump_to, rev;        /* .. JUMP: the next step + 1 (0: the next one); REV: backwards to step 1 */
    uint16_t mo_cur, mo_prev;    /* (0.6.50) MORPH as the engines play it: Q8, slewed a block at a time (voice.c
                                  * track_render) and ramped across the block from mo_prev (no step: no click) */
    uint8_t cpos, cv_on;         /* (0.6.47) C.LEN: the component lane's next step; the step playing is cvs */
    step_t cvs, cva, cvp;        /* .. the step playing with the lane's component (seq2d: and its components applied);
                                  * the ARP's (CV_IDX); STUTTER's step (seq2d, was PULSE's) */
    uint32_t seq_off;
    uint8_t seq_active;          /* any step programmed */
    uint8_t rskip_idx;           /* live recording put notes into the step about to play: */
    uint8_t rskip_n, rskip[NLANE];   /* do not trigger them again there (they sound already) */
    /* live recording of held notes (seq.c rec_hold): the steps they are held into become TIEs */
    uint8_t rh_n, rh_note[4];    /* recorded notes still held, 0 = none */
    uint8_t rh_start;            /* the step they were recorded into */
    uint8_t rh_ties;             /* TIE steps written after it */
    uint8_t rh_last;             /* the last of them; rh_bak: what it held (an early release puts it back) */
    step_t rh_bak;
    /* mono */
    uint8_t mono_stack[8];
    uint8_t nmono;
    uint8_t mono_note;           /* note the MONO / LEGATO / UNISON voice(s) play, 0 = none */
    uint8_t rr;                  /* POLY ROTATE: next voice to try */
    /* mix runtime */
    int32_t peak;
    int32_t dist_hp, dist_lp1, dist_lp2;   /* DIST insert state (fx.c) */
    int32_t dist_dc, dist_hold;  /* (JIANT) RECT's DC; CRUSH's held sample */
    uint8_t dist_hc;             /* .. CRUSH's hold count */
    int32_t dist_cab_l, dist_cab_b;   /* (JIANT 0.7.8) AMP's cabinet: its state-variable filter (low, band) */
    int32_t ms_v;                /* (JIANT) the modulation sequence's level now (mod.c MS_STEP, Q15, slewed) */
    uint8_t tail;                /* blocks to mix after the last voice (the DIST tail) */
    int16_t armp, aholdp;        /* P_AMODE / P_AHOLD as last seen by the ISR */
    /* engine switch (voice.c engine_block): the old engine's voices fade out, then it switches */
    uint8_t xf_on, xf;           /* fading; blocks of the fade still to render */
    uint8_t xf_len, xf_dip;      /* (0.6.41) the fade's length; a dip (an engine's list value changed: the voices stay) */
    uint8_t xin, xin_len;        /* (0.6.41) fading in after it: blocks still to come, of xin_len */
    int16_t pe_old[8];           /* P_E0..P_E7 of the sounding engine: the fade renders with these */
    uint8_t xp_n, xp_note[4], xp_vel[4];   /* note-ons during the fade, played on the new engine */
    /* modulation matrix (mod.c): MIDI performance controllers of the track's channel, 0..127 */
    uint8_t mw, at;              /* CC1 mod wheel, channel aftertouch */
    uint8_t ex_off;              /* 127 - CC11 expression (0: full, the MIDI default) */
    uint8_t m_vel, m_key, m_vi;  /* the latest note-on: velocity, note, voice index (per-block destinations) */
    int16_t m_rnd;               /* .. its RAND */
    int32_t m_env;               /* the amp envelope of voice m_vi, last block (Q15) */
    int32_t m_env2;              /* (V10.22) .. its ENV 2 (Q15) */
    /* the punch-in MIDI effects (pfx.c), the ISR's: what acts on this track this block, never saved */
    int8_t trn;                  /* ARP TRNS (seq.c): the semitones the keys set (from C4), on the steps' notes */
    uint8_t pfx;                 /* PFX_* */
    int16_t pfx_pit;             /* OCT- / OCT+: 1/16 semitones on every voice */
    int16_t pfx_rpit;            /* RANDOM: the next note's offset (1/16 semitones), drawn by trk_note_on */
    int8_t dpit;                 /* (0.6.82) the sequencer's next drum hits: their tune (semitones; a step's OCT, RAND,
                                  * TONAL, ARP SCALE on a kit) */
    int16_t dbend;               /* .. BEND: where the hit glides to over dbt blocks (1/16 semitones; eng_drum.c) */
    uint16_t dbt;
    uint8_t ss_n, ss_i;          /* (0.6.84) STRUM on the sequencer's chords: the step's notes still to come in, in */
    uint8_t ss_nn[4 + NLANE], ss_vv[4 + NLANE];  /* order low to high (seq.c seq_step, seq_tick), ss_due samples to the next */
    uint8_t dtch, ss_tch;        /* (0.8.5) a step's TOUCH (1..3, 0 none) on the sequencer's next drum hits; .. on the
                                  * queued ones (HUMAN TIME: seq_tick) */
    uint8_t lr_on;               /* (0.8.5) the playing step has a lane ratcheting (seq.c seq_lane_rat) */
    uint32_t ss_due, ss_gap;     /* (.. and the gap between them; 0.6.88: 32 bits, a long TIMING) */
    uint16_t ext;                /* (0.6.88) TIMING: samples the playing step began before its place (it is early) */
    int8_t mic;                  /* .. the playing step's own TIMING (ST_MICRO), late: seq_step queues its notes */
    int8_t hum_env;              /* (0.6.92) HUMAN: the next notes' DEC / REL move (seq_step sets it, voice_start takes it) */
    uint8_t seq_fit;             /* FIT: bit 7 ON; SPAN 1..64 (0 = 16), in an unused byte: track_t stays the same size */
    uint8_t pfx_rs;
    uint16_t pfx_idx0;
    uint32_t pfx_pos0, pfx_el;
    uint8_t pfx_ln[4], pfx_lv[4], pfx_lnn;   /* the last notes struck together (a step's), STUTTER / ARP repeat them */
    uint32_t pfx_lblk;           /* .. the block they were struck in */
    uint8_t pfx_sn[4], pfx_snn;  /* the notes a repeat holds now */
    uint8_t pfx_si;              /* ARP: the next of them */
    uint32_t pfx_sc;             /* samples since the last repeat */
    uint8_t pfx_lpn, pfx_lpc;    /* (0.9.9-snd2) LOOP 3 / 5 (pfx.c): the steps it repeats (p2: lpc unused); */
    uint16_t pfx_lps;            /* .. the first of them */
} track_t;
enum { PFX_RND = 1, PFX_HALF = 2, PFX_REP = 4, PFX_DEC = 8, PFX_ATK = 16, PFX_LOOP = 32 };   /* (snd2) LOOP 3 / 5 / 7 */   /* (track_t.pfx) */

typedef struct {
    int16_t g[G_COUNT];
    uint8_t playing, seq_mode;
    uint8_t grid;                /* 1: the keys are the DRUM grid (ui.c grid_on): only the lane keys play (seq.c);
                                  * 2: NAME (ui_name.c): no key plays */
    uint8_t rec;                 /* live recording armed: bit per track */
    uint8_t sel;                 /* selected track 0..NTRK-1: keys, pages, editor */
    int8_t octave;
    uint32_t tick;               /* sub-blocks since play */
    uint32_t cpu_q8;             /* audio ISR load, 1/256 */
    uint32_t master_q12;
    int32_t batt_raw;            /* smoothed ADC ch3 (battery divider), 0 = not read yet */
} song_t;

static uint8_t trk_live = 4;
static track_t trk[NTRK];        /* the instrument: four parts */
static song_t song;
#define TSEL (&trk[song.sel])    /* the selected track */

/* SWING of a track's step clock: the track's own plus the global one, at most 100 (#31). The sequencer
 * (seq.c step_samples) and the SLICER (slicer.c sl_enter) both time steps with it. At 100 the even
 * steps are 1.4 x the straight length and the odd ones 0.6 x. */
#define SWING_MAX 100
static inline int32_t track_swing(const track_t *t)
{
    int32_t sw = t->p[P_SSWING] + song.g[G_SWING];
    return sw < 0 ? 0 : sw > SWING_MAX ? SWING_MAX : sw;
}
/* the length of step idx of a straight length `base`, swung: even steps longer, odd ones shorter */
static inline uint32_t swing_step_len(const track_t *t, uint32_t base, uint32_t idx)
{
    int32_t sw = track_swing(t) * (int32_t)base / 250;
    return base + (uint32_t)((idx & 1u) ? -sw : sw);
}

/* ----------------------------------------------------------- system --- */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")   /* slot store before the index update */
static volatile uint32_t fm1_ms;  /* milliseconds since boot (TIMER4-based, TIMER5 ISR in main.c) */
/* boot-loop guard (main.c): two boots in a row that die in the first 30 s -> UBOOT */
#define BOOTGUARD_MAGIC 0x42475244u
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));

/* (V10.60) the Bluetooth segment's section macros and its check, here, ahead of the sound: cold code of
 * its files (fm4_convert.c) goes there too (bt.c has the story) */
#if FELUCCA_BT
#define BTAPP_RO __attribute__((section(".btapp.rodata"), used))
#define BTAPP_FN __attribute__((section(".btapp.text"), used, noinline))
static int bt_hw_seg(void);                             /* bt_hw.c */
#else
#define BTAPP_RO
#define BTAPP_FN
static int bt_hw_seg(void) { return 1; }                /* (the host: everything in the one image) */
#endif

