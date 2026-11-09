/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SEQ > PATTERNS) and the parts' sounds at power-on. */
#include "dsp.c"
#include "eng_analog.c"
#include "eng_phase.c"
#include "eng_lofi.c"
#include "eng_formant.c"
#include "eng_wheel.c"
#include "eng_drum.c"           /* DRUM: the 8-lane DRUM-X kit (drumx_voice.c) */
#include "eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#include "eng_animal.c"         /* (JIANT 0.8.3) ANIMAL: animal calls (CMOS PULSE WHSTL WING GLOTT) */
#include "eng_string.c"         /* (JIANT 0.9.9-snd5) STRING: Karplus-Strong, in the COMB's lines (engine 8) */
#include "eng_add.c"            /* (JIANT 0.9.9-snd5) ADD: additive, nine partials, in WHEEL's voice state (engine 13) */
#include "eng_input.c"          /* (V10.54) INPUT: the audio input on a track (engine 15) */
#include "fm4_convert.c"        /* DIGITAL's tables, and its sounds -> FM6 */
#if FELUCCA_FM4
#include "eng_digital.c"        /* DIGITAL: four-operator FM (retired; FELUCCA_FM4=1 builds it) */
#endif

/* PHYS (engine 9): retired in TONIC (its 50 KB of voice state went to the song and the drums); SAMPLE (4), GRAIN (8) and
 * SLICE (13): retired in JIANT (core.h eng_gone). The numbers stay reserved, as DIGITAL's: the stores name engines by
 * index and the table is append-only. ENG_GONE below has no presets and is never offered (eng_ok); a sound of theirs
 * that arrives anyway -- a project, a user preset, the editor's PRESET / SET G_ENGSEL, a backup -- plays as ANALOG's
 * first preset (eng_live; project.c proj_phys_gone, ui.c set_engine_of, ui_input.c). A SAMPLE PERC user preset still
 * loads as DRUM first (core.h drum_from_perc) */
static void phys_gone_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void phys_gone_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_GONE = {
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {
        {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 127, 0, 0, 0}, {"-", F_INT, 0, 127, 0, 0, 0},
    },
    .note_on = phys_gone_note_on,
    .render = phys_gone_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
#define ENGI_PHYS_TO 0u          /* the engine a retired engine's sound plays as: ANALOG (its first preset) */

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 */
#if FELUCCA_FM4
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
#else
    &ENG_FM4_GONE,               /* 1: reserved (DIGITAL, retired: its sounds convert to FM6, fm4_convert.c) */
#endif
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
    &ENG_GONE,                   /* 4: reserved (SAMPLE, retired in JIANT) */
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    &ENG_GONE,                   /* 6: reserved (TRIO, folded into ANALOG in JIANT 0.4: core.h analog_from_trio) */
    &ENG_WHEEL,                  /* 7 */
    &ENG_STRING,                 /* 8 (ENGI_STRING, 0.9.9-snd5; GRAIN's, retired in JIANT: its sounds play as STRING's) */
    &ENG_GONE,                   /* 9: reserved (PHYS, retired in TONIC: its sounds play as ANALOG) */
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
    &ENG_NOISE,                  /* 11 */
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
    &ENG_ADD,                    /* 13 (ENGI_ADD, 0.9.9-snd5; SLICE's, retired in JIANT: its sounds play as ADD's) */
    &ENG_ANIMAL,                 /* 14 (ENGI_ANIMAL, JIANT 0.8.3) */
    &ENG_INPUT,                  /* 15 (ENGI_INPUT, V10.54) */
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it comes last). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them. The parts at power-on first (TRK_DEF: T1..T6 are
 * its first six; V10.6d: DRUM-X ANALOG PHASE FM6 VOICE ANIMAL, then MATH WHEEL NOISE ..; 0.9.9: DRUM-X VOICE ANIMAL
 * LOFI ANALOG FM6); 0.9.8's ANALOG FM6 PHASE LOFI VOICE WHEEL NOISE DRUM ANIMAL is what PROGRAM CHANGE keeps
 * (ENGINE_PC_ORDER) */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    ENGI_DRUM,                   /* DRUM (DRUM-X) */
    ENGI_ANALOG,                 /* ANALOG */
    ENGI_PHASE,                  /* PHASE */
    ENGI_FM6,                    /* FM6 */
    ENGI_VOICE,                  /* VOICE */
    ENGI_ANIMAL,                 /* ANIMAL */
    ENGI_LOFI, ENGI_WHEEL,       /* MATH (was LOFI) WHEEL */
    ENGI_NOISE,                  /* NOISE */
    ENGI_STRING, ENGI_ADD,       /* (0.9.9-snd5) STRING ADD */
    ENGI_INPUT,                  /* (V10.54) the audio input */
#if FELUCCA_FM4
    ENGI_DIGITAL,                /* DIGITAL (retired, FELUCCA_FM4 builds only): after the rest */
#endif
};
/* (0.9.9) PROGRAM CHANGE n loads the n + 1-th sound of the PRESETS list in the order of 0.9.8 (ANIMAL last: 0.8.3's
 * numbers kept), so a MIDI rig's program numbers name the sounds they named before the display order changed; the
 * PRESETS list itself follows ENGINE_ORDER (ui.c preset_all_at_in) */
static const uint8_t ENGINE_PC_ORDER[NENG_SHOWN] = {
    ENGI_ANALOG, ENGI_FM6,
#if FELUCCA_FM4
    ENGI_DIGITAL,
#endif
    ENGI_PHASE, ENGI_LOFI, ENGI_VOICE, ENGI_WHEEL, ENGI_NOISE, ENGI_DRUM, ENGI_ANIMAL,
    ENGI_STRING, ENGI_ADD,       /* (0.9.9-snd5: after ANIMAL, every number before kept) */
    ENGI_INPUT,                  /* (V10.54) */
};

/* the engines one can pick (engine 1 only with FELUCCA_FM4), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e) { return e < NENGINES && (FELUCCA_FM4 || e != ENGI_DIGITAL) && !eng_gone(e); }
/* the engine a stored engine number plays on: DIGITAL (without FELUCCA_FM4) FM6, a retired one ANALOG, a bad number ANALOG */
static uint32_t eng_live(uint32_t e)
{
    return e >= NENGINES ? 0u : eng_gone(e) ? ENGI_PHYS_TO : !eng_ok(e) ? ENGI_FM6 : e;
}
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    e = eng_live(e);                             /* (DIGITAL: FM6, PHYS: ANALOG) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* factory sequence patterns: SEQ > PATTERNS loads one into the selected track (ui.c pat_load); a preset
 * only suggests one with PAT(n), loading a sound never touches the steps. Absolute notes, loaded as they
 * are (DRUM plays them as GM drums, SLICE as slices; SCL TRANS and OCT transpose): 0 = rest;
 * flags 1 = accent, 2 = slide, 4 = tie (holds the previous note). Names: at most 8 characters */
#define T_ 4
static const struct {
    const char *name;
    uint8_t note[16], flags[16];
} PATTERNS[] = {
    {"ACID", {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50},          /* 1 */
     {1, 0, 2, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 2, 0, 1}},
    {"OFFBEAT", {0, 36, 0, 36, 0, 36, 0, 48, 0, 36, 0, 36, 0, 39, 0, 43},          /* 2 bass */
     {0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},
    {"MELODY", {60, 0, 67, 0, 72, 67, 0, 64, 62, 0, 69, 0, 74, 69, 0, 67},         /* 3 pluck */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    {"LEAD", {72, 0, 0, 74, 0, 0, 76, 0, 79, 0, 76, 0, 74, 0, 0, 0},               /* 4 */
     {1, T_, 0, 2, T_, 0, 0, 0, 1, 0, 2, 0, 0, T_, T_, 0}},
    {"PAD", {60, 0, 0, 0, 0, 0, 0, 0, 57, 0, 0, 0, 55, 0, 0, 0},                   /* 5 long notes */
     {0, T_, T_, T_, T_, T_, T_, 0, 0, T_, T_, 0, 0, T_, T_, 0}},
    {"KEYS", {0, 0, 60, 0, 0, 63, 0, 0, 0, 0, 60, 0, 0, 65, 0, 63},                /* 6 offbeat stabs */
     {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}},
    {"BELL", {72, 0, 0, 79, 0, 0, 84, 0, 0, 0, 76, 0, 0, 0, 0, 0},                 /* 7 sparse */
     {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"SUB", {36, 0, 0, 0, 0, 0, 0, 36, 0, 0, 34, 0, 0, 0, 0, 0},                   /* 8 low, held */
     {1, T_, T_, T_, 0, 0, 0, 0, 0, 0, 0, T_, T_, T_, 0, 0}},
    /* SLICE (eng_slice.c): note = C4 + slice */
    {"CHOP", {60, 61, 62, 67, 64, 65, 60, 69, 68, 70, 62, 67, 72, 72, 74, 64},     /* 9 16 slices re-ordered */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"STUTTER", {60, 60, 61, 61, 62, 0, 63, 63, 64, 65, 65, 0, 66, 66, 66, 67},    /* 10 8 slices, repeats */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"SLICES", {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75},   /* 11 in order */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* DRUM (General MIDI: 36 kick, 38 snare, 42 closed / 46 open hi-hat) */
    {"BEAT", {36, 42, 42, 42, 38, 42, 36, 42, 36, 42, 42, 36, 38, 42, 46, 42},     /* 12 */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* 16ths up a C minor arpeggio, twice: the line the ARP presets (RAVE, ARP 8BIT, ARP LEAD) suggest
     * now that the arpeggiator is the track's and a preset no longer switches it on */
    {"ARP", {48, 51, 55, 60, 63, 67, 72, 75, 48, 51, 55, 60, 63, 67, 72, 75},       /* 13 */
     {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}},
    /* (V10.34) boom bap (HIP HOP's): the kick on 1, the "and" of 2 and 3's second 16th, the snare on 2 and 4 */
    {"BOOM BAP", {36, 0, 42, 0, 38, 0, 42, 36, 0, 36, 42, 0, 38, 0, 42, 46},     /* 14 */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0}},
};
#undef T_
#define NPATTERNS (sizeof PATTERNS / sizeof PATTERNS[0])

/* (0.9.9) the parts at power-on, INIT SYSTEM and NEW PROJECT (engine, preset, PATTERNS[n - 1] in the sequencer,
 * 0 = empty: all are), in the display order's first six. (V10.6d, the user: a synth that does not startle a
 * conservative player) a band in tune: T1 DRUM-X's soft ACOUSTIC kit, T2 ANALOG's bass, T3 PHASE's pad, T4 FM6's
 * electric piano, T5 VOICE's vowels (no words), T6 ANIMAL's WHISTLE (C3 is C3). Each its preset's sound, dry (ui.c
 * power_sound_of: no DIST CHOR DLY REV send, the effects are the player's to add); was DRUM-X SINGER EN CMOS BIRD
 * BYTEBEAT FAT BASS TINE EP */
#define TRK_PR_DRUM 5u           /* ACOUSTIC (eng_drum.c DRUM_PRESETS) */
#define TRK_PR_ANALOG 8u         /* PLUCK (eng_analog.c); (V10.7.11, the owner) was 14 FAT BASS */
#define TRK_PR_PHASE 11u         /* INIT SAW (eng_phase.c); was 9 GLASS PAD */
#define TRK_PR_FM6 2u            /* FM BASS (eng_fm6.c); was 0 TINE EP */
#define TRK_PR_VOICE 5u          /* CHANT (eng_formant.c); was 0 CHOIR AAH */
#define TRK_PR_ANIMAL 24u        /* WHISTLE (eng_animal.c) */
static const uint8_t TRK_DEF[NPART][3] = {{ENGI_DRUM, TRK_PR_DRUM, 0}, {ENGI_ANALOG, TRK_PR_ANALOG, 0},
                                          {ENGI_PHASE, TRK_PR_PHASE, 0}, {ENGI_FM6, TRK_PR_FM6, 0}, {ENGI_VOICE, TRK_PR_VOICE, 0},
                                          {ENGI_ANIMAL, TRK_PR_ANIMAL, 0}};
/* (V10.7.11, the owner's choice) ACOUSTIC, ANALOG PLUCK, PHASE INIT SAW, FM BASS, CHANT, WHISTLE (was FAT BASS,
 * GLASS PAD, TINE EP, CHOIR AAH) */
/* the parts at power-on through 0.9.8 (the same columns): what an old project's tracks it had no room for load as
 * (project.c proj_new_tracks, PROJ_DEF_SOUND: format 1's tracks 2..4, a 4-track format's T5 / T6): never change it.
 * ANALOG ACID, FM6 PAD (was DIGITAL PAD), LOFI PULSE LD (3: BYTE first), DRUM KIT; (JIANT 0.6) PHASE's first, WHEEL's first */
static const uint8_t TRK_OLD[NPART][3] = {{0, 4, 0}, {ENGI_FM6, 4, 0}, {0, 24, 0}, {ENGI_DRUM, 0, 0}, {2, 0, 0}, {7, 0, 0}};
/* (V10.6h) LOFI PULSE LD (a chip sound, retired with MATH) as ANALOG ORGAN SQR: its square, one oscillator */
static uint32_t trk_old_engine(uint32_t i) { return TRK_OLD[i % NPART][0]; }
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
