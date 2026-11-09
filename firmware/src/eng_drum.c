/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DRUM: JIANT's drum engine, DRUM-X (drumx_voice.c, after Sonic Charge's Microtonic): a kit of 8 lanes, KICK, SNARE,
 * CLAP, HAT CL, HAT OP, TOM, RIM, BELL, each a synthesized sound with two patches A and B and a MORPH between them.
 * The kit is the section's (drumx_voice.c dx_kit), edited on EDIT > SOUND. Felucca's DRUM kits (STD and the model
 * kits 80 10 66 55 77, drum_voice.c) were retired in JIANT 0.1: a sound or project that picked one plays DRUM-X.
 *
 * Keys and notes: the keys play the General MIDI drum map with the kick on the first C (C KICK, C# RIM,
 * D SNARE, D# CLAP, F# HAT CL, A# HAT OP, F G A B C D TOMS, C# CYM, G# BELL; at every OCT: above it ratcheted, below it damped, 0.8.5), and a note
 * plays its GM drum on its lane (35..81: kicks, snares, toms, hats, cymbals and bells on BELL, congas on TOM, claves
 * on RIM, each at its pitch from the lane's; other notes as their octave of 36..47), so GM patterns and MIDI parts
 * play as drums.
 *
 * Parameters (stored as before: KIT's slot is MRPH, SNAP's NOIS, KICK's WARP): MRPH the MORPH A..B of every
 * lane; TUNE (+-12 semitones), TONE (COLOR), DECY (DECAY) and NOIS (NOISE) move every lane from its patch (64: as
 * the kit has it); FM ENV (JIANT 0.6.36, once ACC, then FM) the FM struck with each hit and gone with its pitch envelope: a click, a beater, a metal edge (drumx_voice.c dx_run); FOLD (JIANT 0.5, was WARP) every sound's oscillator through a wavefolder after its envelope: a bright attack folding back to the plain tail (0: as the kit has it); DRV a soft clip on every hit
 * (x1..x4, level kept). Each lane has its LEVEL (P_LN0..P_LN7, EDIT > LANES / LANES 2, #97: square law, 100 % the
 * default) on the voice amplitude after the knee. The knobs move a hit while it rings.
 *
 * Polyphony: a lane per drum and part, mono: a hit reuses its lane's voice even at another pitch, the 8 lanes ring
 * together. A sounding voice plays one lane (s[0]); the lane remembers its voice (owner), and a voice
 * whose lane was struck again by another voice, or has rung out (-84 dB), ends itself (drum_amp). The
 * voices come from the shared budget (voice.c), up to 8 per part. A closed hat chokes the open one.
 *
 * What applies (engine_t.oneshot): the hit is the envelope. VOICE (always POLY), GLIDE, the ADSR, ENV DEST,
 * LFO DEST PIT / FLT / SHP, the global TUNE and the matrix's PITCH CUT SHP do nothing; LEVEL, PAN, the
 * sends, DIST, the SLICER, LFO DEST AMP, velocity, the matrix's AMP and its E1..E8 (the knobs, per
 * block) do. A released key does not end a hit.
 *
 * State: 8 lanes per part (drum_kit: each lane's hit; .bss since JIANT 0.6.53). */
#include "drumx_voice.c"

typedef struct {
    dx_voice_t x;                /* the hit */
    uint8_t owner;               /* the voice playing the lane: index + 1, 0 = none */
    int8_t st;                   /* its semitones from the lane's pitch (the GM map) */
    uint8_t mg;                  /* its group mute's fade: DXG_FADE (sounding) .. 0 (muted, silent) */
    uint8_t age;                 /* blocks since the hit (255: and more): PUNCH's shape */
    int8_t rm, rt;               /* (0.5.1) X-MOD RAND: this hit's MORPH offset, its tune (1/16 semitones) */
    int16_t bc, bd, bs;          /* (0.6.82) a step's BEND on the hit: now, where to, a block's step (1/16 semitones) */
    uint8_t dm, fl, fv;          /* (0.8.5) its TOUCH 0..3 (DXO_*: SHORT GHOST FLAM, 0 as the kit has it); FLAM: blocks
                                  * to the hit itself (0 none), its velocity */
} drum_lane_t;
/* (JIANT 0.5.1) X-MOD (EDIT > X-MOD on a DRUM track): the kit's own modulator, saved with the kit (project.c, in its
 * reserved bytes): RATE (OFF, 4BAR 2BAR 1BAR 1/2 1/4 1/8 1/16 1/32: a tempo-synced LFO), DPTH (how far it moves
 * MORPH, +-64), SHPE (SINE TRI SAW RAMP S&H), RAND (each hit its own MORPH, up to +-64, and tune, up to +-1
 * semitone: no two hits alike). dx_mot_v: the LFO's MORPH offset this block (fx.c dx_mot_block) */
enum { XM_RATE, XM_DPTH, XM_SHPE, XM_RAND };
static uint8_t dx_mot[4];
static int32_t dx_mot_v;
static uint32_t dx_mot_rng = 0x9E3779B9u;

static drum_lane_t drum_kit[NPART][DV_NLANE];   /* (0.6.53: in .bss, was .pool: the pool kept for the FX lines) */

/* The group mutes (JIANT): KICK (the kick), SNARE (snare and clap), HAT (both hats), PERCS (tom, rim, bell), on every
 * DRUM track. A section's (project.c keeps them with its DRUM-X kit, the song stages them); set live on the
 * GLO layer (ui_layer.c). A muted group's new hits do not sound (voice.c trk_note_on); what rings when it is muted
 * fades out (drum_render: DXG_FADE blocks, ~5 ms, no click), then ends */
enum { DXG_KICK = 1, DXG_SNARE = 2, DXG_HAT = 4, DXG_PERC = 8, DXG_ALL = 15 };
static const uint8_t DXG_LANE[DV_NLANE] = {DXG_KICK, DXG_SNARE, DXG_SNARE, DXG_HAT, DXG_HAT, DXG_PERC, DXG_PERC, DXG_PERC};
#define DXG_FADE (256u / CTL)
/* .. and each sound's own mute (EDIT held + black keys 1..8 on a DRUM track, ui_layer.c): bit 8 + lane */
#define DXM_LANE(l) (1u << (8u + (l)))
#define DXM_ALL 0xFF0Fu
static volatile uint16_t dx_mute;                       /* DXG_* groups | DXM_LANE lanes */
static void dx_mute_set(uint32_t m) { dx_mute = (uint16_t)(m & DXM_ALL); }
static int dx_lane_muted(uint32_t l) { return (DXG_LANE[l & 7u] & dx_mute) || (dx_mute & DXM_LANE(l & 7u)); }

static int16_t clip_eff, pnch_eff;   /* G_CLIP / G_PUNCH as the master plays them (mod.c macro_master) */
/* The drum bus (JIANT, MENU-less: FX > MASTER). PUNCH (G_PUNCH 0..100, no detector: each hit's own age), made to
 * fatten (JIANT 0.5; it was Drum Buss's +11 dB / -14 dB, which thinned everything out): the whole hit driven into the
 * voice's knee, its first ~3 ms up to x1.45, its body (~15 .. 115 ms) up to x1.7, back to unity by ~180 ms: the peak
 * held by the knee, the body denser and louder, the tail whole (the master's leveler does not pump on a spike). DUCK (G_DUCK, fx.c): a kick struck on any
 * DRUM track (its group not muted) ducks the other parts; duck_hit tells fx.c's mix_block */
static volatile uint8_t duck_hit;
#define DRUM_ATK_BLK (1536u / CTL)                       /* (JIANT 0.4) ATK+ on a DRUM track: a hit's fade-in, blocks (~35 ms) */
static int32_t punch_gain(uint32_t age, int32_t p)     /* Q15 (32768 = 1, up to x1.7), p 0..100; age in blocks */
{
    int32_t up = 32768 + p * 150, body = 32768 + p * 230;
    age *= CTL / 32u;                                    /* (the shape in blocks of 32) */
    return age < 4u ? up : age < 20u ? up + (body - up) * (int32_t)(age - 4u) / 16
                       : age < 160u ? body : age < 250u ? body + (32768 - body) * (int32_t)(age - 160u) / 90 : 32768;
}

/* General MIDI notes 35..81 -> the lane (DV_*) and semitones from its pitch */
static const int8_t DRUM_GM[47][2] = {
    {DV_KICK, -2}, {DV_KICK, 0}, {DV_RIM, 0}, {DV_SNARE, 0}, {DV_CLAP, 0}, {DV_SNARE, 2},             /* 35 */
    {DV_TOM, -7}, {DV_HATC, 0}, {DV_TOM, -4}, {DV_HATC, -2}, {DV_TOM, 0}, {DV_HATO, 0},               /* 41 */
    {DV_TOM, 3}, {DV_TOM, 5}, {DV_BELL, 0}, {DV_TOM, 8}, {DV_BELL, -3}, {DV_BELL, 2},                 /* 47 */
    {DV_BELL, 5}, {DV_HATC, 5}, {DV_BELL, 4}, {DV_BELL, 0}, {DV_BELL, 1}, {DV_RIM, -12},              /* 53 */
    {DV_BELL, -2}, {DV_TOM, 10}, {DV_TOM, 7}, {DV_TOM, 5}, {DV_TOM, 5}, {DV_TOM, 0},                  /* 59 */
    {DV_TOM, 10}, {DV_TOM, 7}, {DV_BELL, 7}, {DV_BELL, 3}, {DV_HATC, 3}, {DV_HATC, 7},                /* 65 */
    {DV_RIM, 7}, {DV_RIM, 5}, {DV_HATC, -4}, {DV_HATC, -6}, {DV_RIM, 0}, {DV_RIM, -4},                /* 71 */
    {DV_RIM, -7}, {DV_TOM, 12}, {DV_TOM, 8}, {DV_BELL, 12}, {DV_BELL, 12},                            /* 77 */
};
static uint32_t drum_gm_ix(uint32_t note) { return (note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u) - 35u; }

/* (JIANT 0.8.5) the octaves of the kit. OCT 0, where the keys play the GM map (key 7 the kick, 36), is the kit as it
 * always was. The other octaves play the same sounds on the same keys (drum_keys: OCT no longer moves the notes).
 * Above, repeated in time while held (seq.c dxo_tick; p1, as main 0.9.13 / 0.9.17: twice over half as fast, x2 at
 * OCT +1 already too fast to play): OCT +1 the hit every other step (REPEAT 1/2), +2 once a step (REPEAT: a roll at the
 * step rate), +3 x2 a step (RATCHET x2); recorded as the hit on each step entered while held (+1 every other one) and,
 * at +3, its lane's x2 (seq.c rec_lmode). Below, each hit with a
 * TOUCH (drum_lane_t.dm; the step's TOUCH component, core.h CP_TOUCH, when recorded): OCT -1 SHORT (half the decay),
 * -2 GHOST (short, darker, at ~45 % of its level: a ghost note), -3 FLAM (a soft grace hit, then the hit itself
 * DXO_FLAM later, ~20 ms whatever the tempo). MIDI keeps the GM map on 35..81, as ever; the notes around it, which
 * played their octave of 36..47 plain, are those octaves' bands: 23..34 -1, 11..22 -2, 0..10 -3, 82..93 +1, 94..105
 * +2, 106..127 +3, each by its pitch class (84 the kick REPEAT 1/2, 24 the kick SHORT). dx_in_tch: a live hit's TOUCH,
 * track_t.dtch: a step's */
static const uint8_t DXO_RAT[4] = {1, 1, 1, 2};          /* OCT 0 +1 +2 +3: hits a step (p1, main 0.9.13 / 0.9.17: was 1 2 4
                                                          * 8; +1 every other step: seq.c dxo_t.half) */
static const int8_t DXO_DEC[4] = {0, -14, -14, 0};       /* TOUCH 0..3: on DECAY (14: half the decay time, dx_tau) */
static const int8_t DXO_TON[4] = {0, 0, -24, 0};         /* .. on TONE (COLOR: the lane's filter, its drive) and
                                                          * (V10.6q) on NOISE (a ghost's click softer too) */
static const uint8_t DXO_VEL[4] = {128, 128, 58, 58};    /* .. on the velocity, /128 (FLAM: its grace hit's) */
#define DXO_FLAM ((FS * 20u / 1000u + CTL / 2u) / CTL)   /* FLAM: blocks from the grace hit to the hit (~20 ms) */
static uint8_t dx_in_tch, dx_in_rat;                     /* the live note's TOUCH / RATCH (seq.c dxo_on; 0 none) */
static int32_t drum_oct_of(uint32_t note)                /* a MIDI note's octave of the kit */
{
    if (note >= 35u && note <= 81u)
        return 0;
    if (note < 35u)
        return -(int32_t)((35u - note + 11u) / 12u);
    return note >= 106u ? 3 : note >= 94u ? 2 : 1;
}
static uint32_t drum_oct_base(uint32_t note) { return 36u + (note + 120u - 36u) % 12u; }   /* .. its sound (36..47) */
/* the MIDI note of key note at octave o (MIDI OUT): its band's note of the same pitch class (DRUM_GM's 48..55 are
 * their pitch class there) */
static uint32_t drum_oct_note(uint32_t note, int32_t o)
{
    int32_t lo, hi, n;
    if (!o)
        return note;
    lo = o > 0 ? 82 + 12 * (o > 3 ? 2 : o - 1) : o == -1 ? 23 : o == -2 ? 11 : 0;
    hi = o > 2 ? 127 : o == -3 ? 10 : lo + 11;
    n = lo + (((int32_t)drum_oct_base(note) - lo) % 12 + 12) % 12;
    return (uint32_t)(n > hi ? n - 12 >= 0 ? n - 12 : 11 : n);
}
static const char *drum_oct_name(int32_t o)              /* what OCT does on a DRUM track (ui_input.c) */
{
    static const char *const N[7] = {"FLAM", "GHOST", "SHORT", "DRUM-X KIT", "REPEAT 1/2", "REPEAT", "RATCHET x2"};
    return N[clamp(o, -3, 3) + 3];
}

/* ----------------------------------------------------- the grid's lanes --- */
/* A step's lane hits (step_t.hit / acc, the DRUM grid: SEQ > STEP on a DRUM track) play these GM notes, on any
 * engine: DRUM strikes its lanes, a synth plays the pitches. Each is the designed pitch of
 * its lane (st 0 in DRUM_GM) */
static const uint8_t DRUM_LANE_NOTE[NLANE] = {36, 38, 39, 42, 46, 45, 37, 56};

/* the lane a note strikes */
static uint32_t drum_lane(uint32_t note) { return (uint32_t)DRUM_GM[drum_gm_ix(note)][0]; }

/* Extended note namespace is active only on an opted-in DRUM track. GM 35..81
 * remains unchanged. 0..12 are chromatic kick, 13..26 chromatic tom; twelve
 * per-hit effect sounds use 27..34 and 82..85. Other upper notes are live
 * performance controls, expanded into ordinary, recordable GM hits by seq.c. */
#define DXF_REV 8u
#define DXF_DLY 16u
#define DXF_LONG 32u
#define DXF_FB 64u
#define DXF_FOLD 128u
#define DXF_NOISE 256u
static const uint16_t DX_EXT_FX[12] = {
    DV_KICK|DXF_REV|DXF_LONG, DV_SNARE|DXF_DLY|DXF_LONG, DV_CLAP|DXF_REV,
    DV_HATC|DXF_FB|DXF_FOLD, DV_HATO|DXF_REV|DXF_LONG, DV_TOM|DXF_DLY|DXF_LONG|DXF_FB,
    DV_RIM|DXF_DLY|DXF_FB, DV_BELL|DXF_REV|DXF_LONG, DV_KICK|DXF_FB|DXF_LONG,
    DV_SNARE|DXF_REV|DXF_NOISE, DV_TOM|DXF_LONG|DXF_FB|DXF_FOLD, DV_BELL|DXF_DLY|DXF_FB|DXF_LONG
};
/* P_DKEYMODE: 0 CLASSIC, 1 EXTENDED (the default: ui.c set_engine_of), its -1 octave KICK 0..12 / TOM 13..26; 2..9
 * EXTENDED, the -1 octave's 27 keys one lane's chromatic run (lane v - 2: 9 the BELL, the cowbell's melodies), 13 its
 * own pitch. The track's value: a recorded 0..26 note plays back on the same lane at the same pitch */
static uint32_t drum_ext(const track_t *t) { return t->p[P_DKEYMODE] >= 1; }
static int32_t drum_chrom(const track_t *t) { return t->p[P_DKEYMODE] - 2; }   /* (< 0: the KICK / TOM split) */
static const char *drum_lane_name(const track_t *t, uint32_t l);
static const char *drum_oct_label(const track_t *t, int32_t o)
{
    if(drum_ext(t)) {
        static const char *const names[7]={"FLAM","FX KIT","KICK / TOM","DRUM-X KIT","FILLS","RATCH SETS","RATCH SETS"};
        static char b[12];
        if (o == -1 && drum_chrom(t) >= 0) {
            str_cpy(b, drum_lane_name(t, (uint32_t)drum_chrom(t)), 6);
            str_cpy(b + str_len(b), " CHROM", 7);
            return b;
        }
        return names[clamp(o,-3,3)+3];
    }
    return drum_oct_name(o);
}
static uint32_t drum_fx_note(uint32_t k) { k %= 12u; return k < 8u ? 27u+k : 82u+k-8u; }
static uint32_t drum_fx_id(uint32_t note) { return note>=27u&&note<=34u ? note-26u : note>=82u&&note<=85u ? note-73u : 0u; }
static uint32_t drum_note_lane(const track_t *t, uint32_t note)
{
    uint32_t fx = drum_ext(t) ? drum_fx_id(note) : 0u;
    if (drum_ext(t) && note < 27u) return drum_chrom(t) >= 0 ? (uint32_t)drum_chrom(t) & 7u : note < 13u ? DV_KICK : DV_TOM;
    return fx ? DX_EXT_FX[fx-1u]&7u : drum_lane(note);
}

/* a note of a muted group (dx_mute): voice.c trk_note_on drops it */
static int drum_muted(uint32_t note) { return dx_lane_muted(drum_lane(note)); }
static int drum_muted_t(const track_t *t, uint32_t note) { return dx_lane_muted(drum_note_lane(t,note)); }

/* the lane's name (5 characters at most) */
static const char *drum_lane_name(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"};
    (void)t;
    return N[l & (NLANE - 1u)];
}
/* .. in two letters, the drum machine way (the grid's lane column) */
static const char *drum_lane_abbr(const track_t *t, uint32_t l)
{
    static const char *const N[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    (void)t;
    return N[l & (NLANE - 1u)];
}

/* the lanes step s strikes: its hits, and its notes on their lanes (a NOTE step only) */
static __attribute__((noinline)) uint32_t step_lanes_of(const track_t *t, const step_t *s)
{
    uint32_t m = 0, i;
    if (s->time != ST_NOTE)
        return 0;
    for (i = 0; i < s->n && i < 4u; i++)
        m |= 1u << (t&&t->engine==ENGI_DRUM?drum_note_lane(t,s->note[i]):drum_lane(s->note[i]));
    return m | s->hit;
}

/* .. and which of them are accented */
static uint32_t step_lanes(const step_t *s) {return step_lanes_of(0,s);}
static __attribute__((noinline)) uint32_t step_accents_of(const track_t *t,const step_t *s)
{return s->flags&SF_ACCENT?step_lanes_of(t,s):s->acc&step_lanes_of(t,s);}
static uint32_t step_accents(const step_t *s) { return step_accents_of(0,s); }

/* a step's notes that are a lane's note become that lane's hits (the same note, the same velocity: nothing
 * sounds different). Other notes (a low tom 41, a crash 49) stay notes, shown on their lane. A step accent
 * of a step left with hits only becomes the hits' accents. Pattern loads into a DRUM track, projects of
 * before the grid, the grid's edits */
static void step_to_grid(step_t *s)
{
    uint32_t i, k = 0;
    if (s->time != ST_NOTE)
        return;
    for (i = 0; i < s->n && i < 4u; i++) {
        uint32_t l = drum_lane(s->note[i]);
        if (s->note[i] == DRUM_LANE_NOTE[l])
            s->hit |= (uint8_t)(1u << l);
        else
            s->note[k++] = s->note[i];
    }
    for (i = k; i < 4u; i++)
        s->note[i] = 0;
    s->n = (uint8_t)k;
    if (!k && (s->flags & SF_ACCENT)) {
        s->acc |= s->hit;
        s->flags &= (uint8_t)~SF_ACCENT;
    }
}

static drum_lane_t *drum_kit_of(const track_t *t)
{
    return t >= &trk[0] && t < &trk[NPART] ? drum_kit[t - trk] : 0;
}

/* the lane voice v plays, 0 when it plays none (any more) */
static drum_lane_t *drum_lane_of(track_t *t, const voice_t *v)
{
    drum_lane_t *K = drum_kit_of(t), *L;
    uint32_t i = (uint32_t)(v - t->v);
    if (!K || i >= NVOICE)
        return 0;
    L = &K[(uint32_t)v->s[0] & (DV_NLANE - 1u)];
    return L->owner == i + 1u ? L : 0;
}

/* Different GM pitches on a lane still share its one sounding voice. */
static voice_t *drum_reuse(track_t *t, uint32_t note)
{
    drum_lane_t *K = drum_kit_of(t);
    uint32_t lane = drum_note_lane(t,note), owner = K ? K[lane].owner : 0;
    voice_t *v;
    if (!owner || owner > NVOICE)
        return 0;
    v = &t->v[owner - 1u];
    return v->active && (uint32_t)v->s[0] == lane ? v : 0;
}

static __attribute__((noinline)) void drum_note_on(track_t *t, voice_t *v)
{
    drum_lane_t *K = drum_kit_of(t), *L;
    uint32_t i = (uint32_t)(v - t->v), lane;
    if (!K || i >= NVOICE)
        return;
    lane = drum_note_lane(t,v->note);
    L = &K[(uint32_t)v->s[0] & (DV_NLANE - 1u)];
    if (L->owner == i + 1u)                              /* this voice played another lane: it stops there */
        L->owner = 0;
    L = &K[lane];
    L->st = (int8_t)clamp(DRUM_GM[drum_gm_ix(v->note)][1] + t->pfx_rpit / 16 + t->dpit, -60, 60);   /* (pfx.c RANDOM;
                                                          * 0.6.82: the step's components, seq.c) */
    if (drum_ext(t) && v->note < 27u)
        L->st = (int8_t)clamp((int32_t)v->note-(drum_chrom(t)>=0?13:v->note<13u?6:19)+t->pfx_rpit/16+t->dpit,-60,60);
    else if (drum_ext(t) && drum_fx_id(v->note)) L->st = (int8_t)clamp(t->pfx_rpit/16+t->dpit,-60,60);
    L->bc = 0;                                           /* (0.6.82) its BEND, from the step */
    L->bd = t->dbend;
    L->bs = (int16_t)(t->dbend ? ((t->dbend < 0 ? -t->dbend : t->dbend) + t->dbt) / (t->dbt ? t->dbt : 1u) : 0);
    L->owner = (uint8_t)(i + 1u);                        /* (its last voice, if another, ends: drum_amp) */
    L->dm = (uint8_t)((dx_in_tch ? dx_in_tch : t->dtch) & 3u);   /* (0.8.5) TOUCH: a key below the kit's octave, a step */
    L->fl = 0;
    if (L->dm) {
        L->fv = v->vel;
        v->vel = (uint8_t)((v->vel * DXO_VEL[L->dm] + 64u) >> 7 ? (v->vel * DXO_VEL[L->dm] + 64u) >> 7 : 1u);
        if (L->dm == 3u)
            L->fl = (uint8_t)(DXO_FLAM + 1u);            /* (FLAM: the grace hit now, the hit DXO_FLAM blocks on, drum_render) */
    }
    if (drum_ext(t)) L->dm |= (uint8_t)(drum_fx_id(v->note)<<2);
    v->s[0] = (int32_t)lane;
    v->env_out = v->vel * 258;                           /* the hit starts at its level (no ramp from 0) */
    v->env = 1 << 24;                                    /* the ADSR held at full from here, not from drum_amp:
                                                          * a key-off before the first block (zero-length MIDI
                                                          * notes) would end a fresh voice at env 0 in env_tick */
    L->mg = DXG_FADE;
    L->age = 0;
    if (dx_mot[XM_RAND]) {                               /* (0.5.1) X-MOD RAND: this hit's own MORPH and tune */
        int32_t r = dx_mot[XM_RAND];
        dx_mot_rng = dx_mot_rng * 1664525u + 1013904223u;
        L->rm = (int8_t)(((int32_t)(dx_mot_rng >> 24) - 128) * r >> 8);
        L->rt = (int8_t)(((int32_t)((dx_mot_rng >> 16) & 255u) - 128) * r >> 10);
    } else {
        L->rm = L->rt = 0;
    }
    if (lane == DV_KICK)
        duck_hit = 1;
    L->x.sv = v->vel;                                    /* (V10.7.8: GR's stroke, drumx_voice.c) */
    dx_trigger(&L->x);
    if (lane == DV_HATC)                                 /* a closed hat chokes the open one */
        dx_choke(&K[DV_HATO].x);
}

/* the voice's amplitude: the hit, not the ADSR (which still runs, held at full so a release never ends
 * it). A voice that plays no lane any more, or whose drum has rung out, ends here */
static int32_t drum_amp(track_t *t, voice_t *v, int32_t adsr)
{
    const drum_lane_t *L = drum_lane_of(t, v);
    if (!v->active)                                      /* (taken for another part: env_tick ended it) */
        return 0;
    if (v->stage == 4u)                                  /* (0.6.74) given up: env_tick's fade, not held at full */
        return adsr;
    if (!L || (!L->x.live && !L->x.trig && !L->fl)) {   /* (0.8.5: a FLAM's hit to come keeps it) */
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    v->env = 1 << 24;
    return 32767;
}

/* (JIANT 0.6.48) IMAGE (P_DXIMG, a track's) and each sound's SEND (dx_snd, the kit's: saved with it, project.c). The
 * part stays one mono sum through its chain (DRV, FILTER, DIST); each sound's pan comes out as what it takes from one
 * side (dx_sl / dx_sr: L = sum - dx_sl, R = sum - dx_sr, fx.c drum_mix), and its SEND as its own feed of a bus (dx_sb),
 * both from the hit as it leaves the voice: no stage run twice. DRV, DIST and the FILTER shape the sum only, so with
 * them the image narrows (dx_img_k), or the side taken would not match the side shaped. IMAGE 0 and no SEND: nothing
 * of this runs */
#define DXS_BUS(s) ((uint32_t)(s) >> 6)                  /* 0 none, 1 CHORUS, 2 DELAY, 3 REVERB */
#define DXS_AMT(s) ((int32_t)(s) & 63)                   /* 0..63 */
static uint8_t dx_snd[DV_NLANE];
static uint32_t drum_hit_fx(const drum_lane_t *L)
{
    uint32_t i=L->dm>>2;
    return i && i<=NELEM(DX_EXT_FX) ? DX_EXT_FX[i-1u] : 0u;
}
static uint32_t drum_send(const track_t *t, uint32_t l)
{
    drum_lane_t *K=drum_kit_of(t);
    const drum_lane_t *L=K?&K[l&7u]:0;
    uint32_t f=L&&L->owner&&(L->x.live||L->x.trig||L->fl) ? drum_hit_fx(L) : 0;
    return f&DXF_REV ? 255u : f&DXF_DLY ? 191u : dx_snd[l&7u];
}
/* (JIANT 0.7.8) a core's own (mod.c's mod_c the same way): the two cores render two parts at once (fx.c mix_block), and
 * with one set the other core's part cleared dx_xon and the sides mid-render: the image on and off block by block (a
 * grit at the block rate, like a downsampling) and clicks on the side a sound leaned to. A core renders its parts one
 * after another, so one set a core is enough */
typedef struct {
    int32_t sl[CTL], sr[CTL], sb[3][CTL];                /* what each side gives up; each bus's feed */
    int8_t lpan[DV_NLANE];                               /* this block's pans, -64 (left) .. 64, at its end */
    int8_t lpan0[DV_NLANE];                              /* (0.6.81) .. and at its start: each ramped across the block */
    uint8_t xon;                                         /* this part's render: bit 0 the image, bits 1..3 a bus fed */
} dx_core_t;
static dx_core_t dx_core[2];
#define DXC (dx_core[MC_CPU() & 1u])
#define dx_sl (DXC.sl)
#define dx_sr (DXC.sr)
#define dx_sb (DXC.sb)
#define dx_lpan (DXC.lpan)
#define dx_lpan0 (DXC.lpan0)
#define dx_xon (DXC.xon)
static int8_t dx_pcur[NTRK][DV_NLANE];                   /* (0.6.81) each part's pans as they are, slewed toward the
                                                          * knob's: a turn of IMAGE jumped a sound across (a click) */
#define DX_PSLEW (2 * (int32_t)CTL / 32)                 /* (a pan's step a block at most: across in ~45 ms) */
/* IMAGE img 1..127 -> lane l's pan, -64..64: each sound on a sine of its own rate over the knob (1 to 9 turns), so a
 * sweep goes through the kit's placings one after another, every one coming back at the same value; the first 16 open
 * it from the middle; the kick a quarter as far (the low end stays centred) */
static int32_t dx_img_pan(uint32_t l, int32_t img)
{
    static const uint8_t F[DV_NLANE] = {1, 3, 5, 2, 7, 4, 6, 9};
    int32_t w = img >= 16 ? 64 : img * 4, s;
    if (img <= 0)
        return 0;
    s = (osc_sine((uint32_t)img * F[l & 7u] * 33554432u + (l & 7u) * 0x2E000000u) * w) >> 15;
    return l == DV_KICK ? s / 4 : s;
}
/* how wide the image plays, Q8: all of it, less with DRV, DIST and the COMB (they shape the mono sum only) */
static int32_t dx_img_k(const track_t *t)
{
    int32_t k = 256 - clamp(t->p[P_E7], 0, 127) * 160 / 127, d = clamp(t->p[P_DIST], 0, 64);
    if (d)
        k = k * (256 - d * 3) >> 8;
    if (FT_POST(ft_of(t->p)))                          /* (0.6.81: the other filters shape the sides too, voice.c
                                                          * tf_sides: the image stays whole through them) */
        k >>= 2;
    return k;
}
/* a DRUM part's render starts (voice.c track_render): the pans of the block, the buffers that will be fed cleared */
/* (V10.49) IMAGE on a synth part in POLY (the users' ask; DRUM-X's knob): each voice to its own place across the stereo
 * field as the kit's sounds (voice.c voice_block: what the voice adds taken from one side), through the part's filter
 * (tf_sides) and fx.c drum_mix; narrowed by DIST as the kit's; slewed a little a block (no jump when turned). Its
 * width this block, per part: syn_img (0 off) */
static uint8_t syn_img[NTRK], syn_cur[NTRK];
static __attribute__((noinline)) void drum_side_begin(const track_t *t, uint32_t n)
{
    int32_t img = clamp(t->p[P_DXIMG], 0, 127), k;
    if (t->engine % NENGINES != ENGI_DRUM) {             /* (V10.49) a synth part */
        uint32_t ti = (uint32_t)(t - trk) % NTRK, j;
        int32_t want = t->p[P_VOICE] == V_POLY && t->engine != ENGI_INPUT ? 127 - clamp(t->p[P_SIMG], 0, 127) : 0, cur = syn_cur[ti], d = clamp(t->p[P_DIST], 0, 64);
        cur += clamp(want - cur, -4, 4);
        syn_cur[ti] = (uint8_t)cur;
        syn_img[ti] = (uint8_t)(n <= CTL ? (d ? cur * (256 - d * 3) >> 8 : cur) : 0);
        if (syn_img[ti]) {
            dx_xon |= 1u;
            for (j = 0; j < n; j++)
                dx_sl[j] = dx_sr[j] = 0;
        }
        return;
    }
    k = img ? dx_img_k(t) : 0;
    uint32_t l, i, on = 0;
    dx_core_t *dc = &DXC;                                /* (0.7.9: this core's, read once) */
    int8_t *pc = dx_pcur[(uint32_t)(t - trk) % NTRK];
    for (l = 0; l < DV_NLANE; l++) {
        int32_t p = img ? dx_img_pan(l, img) * k >> 8 : 0, p0 = pc[l];
        p = p0 + clamp(p - p0, -DX_PSLEW, DX_PSLEW);
        pc[l] = (int8_t)p;
        dc->lpan0[l] = (int8_t)p0;
        dc->lpan[l] = (int8_t)p;
        if (p || p0)
            on |= 1u;
        uint32_t send=drum_send(t,l);
        if (DXS_BUS(send) && DXS_AMT(send))
            on |= 1u << DXS_BUS(send);
    }
    dc->xon = (uint8_t)on;
    if (n > CTL)
        n = CTL;
    if (on & 1u)
        for (i = 0; i < n; i++)
            dc->sl[i] = dc->sr[i] = 0;
    for (l = 0; l < 3u; l++)
        if ((on >> (l + 1u)) & 1u)
            for (i = 0; i < n; i++)
                dc->sb[l][i] = 0;
}
/* lane l's sides and bus this block (drum_render's loop feeds them as it adds the hit to the part): 0 when none. (0.6.81)
 * g[0..3]: what it gives up of the left at the block's start and end, of the right at its start and end (a pan crossing
 * the middle within a block fades out of one side into the other) */
static int32_t dx_dump[CTL];                             /* (the bus not fed: a sink, so the loop has no branch) */
static __attribute__((noinline)) int drum_side_of(dx_core_t *dc, uint32_t l, uint32_t send, int32_t *g, int32_t **d2, int32_t *a)
{
    int32_t p0 = dc->lpan0[l & 7u], p1 = dc->lpan[l & 7u];
    uint32_t b = DXS_BUS(send);
    *a = b ? DXS_AMT(send) : 0;
    /* A note can start inside the render, after side_begin. Clear a newly
     * requested bus before its first write, on this core only. */
    if(*a && !(dc->xon&(1u<<b))) {
        uint32_t i;
        for(i=0;i<CTL;i++) dc->sb[b-1u][i]=0;
        dc->xon|=(uint8_t)(1u<<b);
    }
    if (!(dc->xon & 1u))
        p0 = p1 = 0;
    g[0] = p0 > 0 ? p0 : 0; g[1] = p1 > 0 ? p1 : 0;
    g[2] = p0 < 0 ? -p0 : 0; g[3] = p1 < 0 ? -p1 : 0;
    *d2 = *a ? dc->sb[b - 1u] : dx_dump;
    return p0 || p1 || *a;
}
/* the voice's output loop with them: the hit into the part, its sides and its bus at once; (0.6.81) each side ramped
 * across the block (IMAGE turned: a pan crossing the middle fades out of one side into the other). (V10.6q: the steady
 * pan's own loop gone, the same sums: a side's gain held is its ramp with no step) */
static __attribute__((noinline)) void drum_side_ramp(int32_t *sl, int32_t *sr, int32_t *y, int32_t *out, uint32_t n,
                                                     const vmod_t *m, const int32_t *g, int32_t *d2, int32_t a)
{
    uint32_t i;
    int32_t gl = g[0] << CTL_LOG2, dgl = g[1] - g[0], gr = g[2] << CTL_LOG2, dgr = g[3] - g[2];   /* (Q6 + CTL_LOG2) */
    for (i = 0; i < n; i++, gl += dgl, gr += dgr) {
        int32_t s = y[i], o = voice_amp(s + (s >> 2), m, i) << 1;
        out[i] += o;
        sl[i] += (o * gl) >> (6 + CTL_LOG2);
        sr[i] += (o * gr) >> (6 + CTL_LOG2);
        d2[i] += (o * a) >> 6;
    }
}

static void drum_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    drum_lane_t *L = drum_lane_of(t, v);
    int32_t y[CTL], pb0 = 4096, pb1 = 4096;
    int32_t lv = clamp(p[P_LN0 + ((uint32_t)v->s[0] & (DV_NLANE - 1u))], 0, 127);
    if (((uint32_t)v->s[0] & (DV_NLANE - 1u)) == DV_BELL)   /* (JIANT 0.6.49) the cowbell 4 dB under the others: it */
        lv = lv * 100 / 127;                                  /* stood out of every kit (its LEVEL at 100 % is this) */
    uint32_t i;
    int32_t lfo_pit, lfo_shp;
    vmod_t ml;                                           /* (only its amplitude ramp is read: voice_amp) */
    if (!L)
        return;
    if (dx_lane_muted((uint32_t)v->s[0]) || L->mg < DXG_FADE) {   /* its group muted:
                                                          * fading (to the end, if unmuted meanwhile), then ended */
        uint32_t g0 = L->mg;
        if (!g0) {
            L->x.live = L->x.trig = 0;                   /* (drum_amp ends the voice) */
            L->fl = 0;
            L->x.hl = 0;                                 /* (0.6.19: the next hit starts its upsampling from 0) */
            return;
        }
        L->mg = (uint8_t)(g0 - 1u);
        ml.amp0 = m->amp0 * (int32_t)g0 / (int32_t)DXG_FADE;
        ml.amp1 = m->amp1 * (int32_t)(g0 - 1u) / (int32_t)DXG_FADE;
        m = &ml;
    }
    if (L->fl == 2u)                                     /* (0.8.5) FLAM: its velocity back a block before the hit
                                                          * (voice.c takes the level before the render) */
        v->vel = L->fv;
    if (L->fl && !--L->fl) {                             /* .. the hit itself */
        L->x.sv = v->vel;
        dx_trigger(&L->x);
        L->age = 0;
        if (((uint32_t)v->s[0] & (DV_NLANE - 1u)) == DV_HATC)
            dx_choke(&drum_kit_of(t)[DV_HATO].x);
    }
    if (lv < 127) {                                      /* the lane's LEVEL (square law) on the block's amplitude
                                                          * ramp, not per sample; 100 %: the ramp as it was */
        int32_t gl = lv * lv * 2 + (lv * lv >> 6);       /* Q15, 127: 32508 (not used), 64: 8256 */
        ml.amp0 = (m->amp0 >> 4) * gl >> 11;
        ml.amp1 = (m->amp1 >> 4) * gl >> 11;
        m = &ml;
    }
    if (n > CTL)
        n = CTL;
    {   /* (0.6.81) the part's LFO on the kit (its hits never had a vmod's pitch or shape): PITCH tunes it, SHAPE moves
         * MORPH (both did nothing on DRUM) */
        int32_t lfo = mulq15(t->lfo_val, t->lfo_fade);
        lfo_pit = (lfo * p[P_LD_PIT] * 3) >> 15;
        lfo_shp = (lfo * p[P_LD_SHP]) >> 15;
    }
    if (L->bc != L->bd) {                                /* (0.6.82) BEND: towards its pitch over the step */
        int32_t d = L->bd - L->bc;
        L->bc = (int16_t)(L->bc + (d > 0 ? (d < L->bs ? d : L->bs) : (-d < L->bs ? d : -L->bs)));
    }
    const dx_lane_t *patch=&dx_kit[(uint32_t)v->s[0] & (DV_NLANE-1u)];
    uint32_t hitfx=drum_hit_fx(L);
    dx_run_fx(patch, &L->x, p[P_E0] + dx_mot_v + L->rm + lfo_shp,
           L->st * 16 + L->rt + L->bc + (p[P_E1] - 64) * 3 + t->pfx_pit + lfo_pit,
           p[P_E3] - 64 + DXO_DEC[L->dm & 3u] + ((hitfx&DXF_LONG)?36:0),
           p[P_E2] - 64 + DXO_TON[L->dm & 3u], p[P_E4] - 64 + DXO_TON[L->dm & 3u] + ((hitfx&DXF_NOISE)?32:0),
           (hitfx&DXF_FOLD)&&p[P_E6]<80 ? 80 : p[P_E6], p[P_E5], y, n, (hitfx&DXF_FB)?6u:0u);
    {                                                    /* PUNCH: the boost before the knee (driven into it, as
                                                          * Drum Buss), the tail's cut on the amplitude ramp */
        int32_t pu = pnch_eff;
        if (pu > 0) {
            int32_t g0 = punch_gain(L->age, pu), g1 = punch_gain(L->age + 1u, pu);
            if (g0 < 32768 || g1 < 32768) {              /* (gains < 1: amp stays <= 32767) */
                int32_t a0 = (m->amp0 * (g0 < 32768 ? g0 : 32768)) >> 15;
                ml.amp1 = (m->amp1 * (g1 < 32768 ? g1 : 32768)) >> 15;
                ml.amp0 = a0;
                m = &ml;
            }
            pb0 = g0 > 32768 ? g0 >> 3 : 4096;           /* Q12 */
            pb1 = g1 > 32768 ? g1 >> 3 : 4096;
        }
        if ((t->pfx & PFX_ATK) && L->age < DRUM_ATK_BLK) {   /* (JIANT 0.4) ATK+: the hit faded in (~35 ms), softer */
            int32_t a0 = m->amp0 * (int32_t)L->age / (int32_t)DRUM_ATK_BLK;
            ml.amp1 = m->amp1 * (int32_t)(L->age + 1u) / (int32_t)DRUM_ATK_BLK;
            ml.amp0 = a0;
            m = &ml;
        }
        if (L->age < 255u)
            L->age++;
    }
    if (pb0 != 4096 || pb1 != 4096)                      /* (0.6.16: DRV and the knee on the kit's sum, drum_bus) */
        for (i = 0; i < n; i++)
            y[i] = (y[i] * (pb0 + (((pb1 - pb0) * (int32_t)i) >> CTL_LOG2))) >> 12;
    {
        int32_t *d2, g[4], a;
        dx_core_t *dc = &DXC;                            /* (0.7.8: this core's, read once) */
        uint32_t vo = vo_off_c[MC_CPU() & 1u];           /* (ctl64) the voice's first sample in the part's block */
        if (drum_side_of(dc, (uint32_t)v->s[0], drum_send(t,(uint32_t)v->s[0]), g, &d2, &a)) {   /* IMAGE, SEND: the same loop */
            drum_side_ramp(dc->sl + vo, dc->sr + vo, y, out, n, m, g, d2 + vo, a);
            return;
        }
    }
    for (i = 0; i < n; i++) {                            /* x1.25 */
        int32_t s = y[i];
        out[i] += voice_amp(s + (s >> 2), m, i) << 1;
    }
}

static uint8_t dx_glue;                                 /* (0.9.9-snd1) GLUE, below */
static int32_t glue_g[NTRK];                            /* .. a DRUM track's gain at the last block's end, Q15 (0: rest) */
static __attribute__((noinline)) void drum_glue(uint32_t ti, int32_t *out, uint32_t n, uint32_t side);
/* (JIANT 0.6.16) the kit's sum, once a block (voice.c track_render): DRV x1..x4 into the soft clip, its level kept,
 * and the knee (were each hit's: eight lanes, two curves a sample each; now the bus's, a glue) */
static __attribute__((noinline)) void drum_bus(const track_t *t, int32_t *out, uint32_t n)
{
    int32_t drv = t->p[P_E7], g = 0, mk = 0, k = 4096, *sl = dx_sl, *sr = dx_sr;
    uint32_t i, side = (dx_xon & 1u) != 0u;
    if (drv > 0) {
        g = 4096 + drv * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
        k = (g * mk) >> 15;                              /* (the curve's slope at 0: a quiet sum's gain) */
    }
    for (i = 0; i < n; i++) {
        int32_t s = out[i] >> 1, y = s;
        if (g)                                           /* (0.6.19: s held to +-131071: s * g fits; several loud hits
                                                         * at once with DRV high wrapped it: a full-scale click) */
            y = (softclip((clamp(s, -131071, 131071) * g) >> 12) * mk) >> 15;
        y = soft_knee(y, 24000);
        out[i] = y << 1;
        if (side && (g || y != s)) {                     /* (0.7.9: the curve flat here, nothing to scale) */                                      /* (0.7.8) IMAGE: what each side gives up through the same
                                                         * gain the curve gave the sum here (it was taken raw from the
                                                         * squashed sum: a loud hit's side over-taken, clicks and grit
                                                         * on the side it leaned to), so L and R are the curve's exactly */
            if ((s > 256 || s < -256) && !(i & 3u))      /* (0.7.9: the gain every 4th sample: a divide's cost) */
                k = clamp((y << 12) / s, 0, 16384);   /* (y within +-41000: y << 12 fits 32 bits) */
            sl[i] = ((sl[i] >> 2) * k) >> 10;
            sr[i] = ((sr[i] >> 2) * k) >> 10;
        }
    }
    if (dx_glue)                                        /* (0.9.9-snd1) GLUE, on */
        drum_glue((uint32_t)(t - trk) % NTRK, out, n, side);
    else
        glue_g[(uint32_t)(t - trk) % NTRK] = 0;
}

/* (0.9.9-snd1) GLUE (KIT 3 KNOB 4, the kit's: 0 OFF .. 15), the owner's "a one-knob glue / limiter on the DRUM-X bus".
 * After the bus's DRV and knee (drum_bus), only when on (OFF: not run, the kit bit for bit as before): the block's peak,
 * a gain toward the curve's (over T: (T + (pk - T) s) / pk; T 24000 .. 10000 (by 2x - x^2: the middle's ~13000), s 1 - x: 2:1 at the middle, a limit at
 * the top; x = GLUE / 15) at once down (from the block's first sample: its peak caught) and ~93 ms back up, the makeup (x1 .. x1.6) and a
 * soft clip whose knee comes down with it (24000 .. 16000: the top a hard limit and a clip). The IMAGE's sides through
 * the gain the sum got, as drum_bus's (every 4th sample) */
static __attribute__((noinline)) void drum_glue(uint32_t ti, int32_t *out, uint32_t n, uint32_t side)
{
    int32_t x = (int32_t)dx_glue * 256 / 15, th = 24000 - (2 * x - ((x * x) >> 8)) * 14000 / 256, pk = 0, g0, g1, tg = 32768, dg, kn, mk;
    int32_t k = 4096, *sl = dx_sl, *sr = dx_sr;
    uint32_t i;
    for (i = 0; i < n; i++) {                           /* the peak (the sum's, as drum_bus left it: x2) */
        int32_t a = out[i] < 0 ? -out[i] : out[i];
        pk = a > pk ? a : pk;
    }
    pk = clamp(pk >> 1, 0, 131071);
    if (pk > th)                                        /* (num <= pk: num << 14 fits) */
        tg = ((th + (((pk - th) * (256 - x)) >> 8)) << 14) / pk << 1;
    g0 = glue_g[ti] ? glue_g[ti] : 32768;
    g1 = tg < g0 ? tg : g0 + ((tg - g0) >> 6);          /* down at once, up over ~64 blocks */
    dg = (g1 - g0) / (int32_t)(n ? n : 1u);             /* (up: over the block) */
    if (g1 < g0) {                                      /* (down: from the block's first sample, its peak caught: a
                                                         * limiter's; a ramp let a hit's onset through) */
        g0 = g1;
        dg = 0;
    }
    mk = 4096 + ((((x * x) >> 8) * x >> 8) * 2458 >> 8);   /* the makeup, Q12: 1 + 0.6 x^3 (the middle's peaks no
                                                         * higher, the top's limit loud) */
    kn = 24000 - x * 8000 / 256;
    for (i = 0; i < n; i++) {
        int32_t s = out[i] >> 1, gm = ((g0 + dg * (int32_t)(i + 1u)) * mk) >> 12;   /* Q15, < 2^16 */
        int32_t y = soft_knee((clamp(s, -65535, 65535) * (gm >> 1)) >> 14, kn);
        out[i] = y << 1;
        if (side) {
            if ((s > 256 || s < -256) && !(i & 3u))
                k = clamp((y << 12) / s, 0, 16384);
            sl[i] = ((sl[i] >> 2) * k) >> 10;
            sr[i] = ((sr[i] >> 2) * k) >> 10;
        }
    }
    glue_g[ti] = g1;
}

/* the GM drum map, the first C (key 7) is the kick (C2, 36), at every OCT (0.8.5: the octaves, above) */
static int32_t drum_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return clamp(29 + (int32_t)k, 0, 127);                /* (0.8.5: every octave; OCT ratchets / damps, seq.c dxo_on) */
}

/* {MRPH, TUNE, TONE, DECY, NOIS, FM, FOLD, DRV}; the kit suggests the BEAT pattern (GM notes). (V10.6i) the kits as they
 * were; a few move the whole kit (the per-block destinations: the knobs, the sends; the LFO FREE, a hit does not restart
 * it): no RAND -> PAN (one pan for the kit: every hit would throw the kick about too) */
static const preset_t DRUM_PRESETS[] = {                /* (0.7.2) one a factory kit: drumx_voice.c DX_KITS, this order */
    {"DRUM-X", DRUM_KIT_E, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 20), PAT(12)},   /* (core.h: SAMPLE PERC's too) */
    {"909 CLUB", {60, 64, 78, 64, 64, 0, 0, 10}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 16), PAT(12)},
    {"808 BOOM", {58, 60, 54, 76, 59, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 24), PAT(12)},
    {"LOFI DUST", {70, 64, 46, 54, 78, 0, 0, 20}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(8, 0, 0, 18), PAT(12),
     MODS(MLFO(LW_DRIFT, -3, 0, 0), P_LPHASE, -1, P_LD_SHP, 8)},                 /* MORPH wanders, a worn tape */
    {"INDUSTRIAL", {72, 66, 99, 70, 75, 20, 0, 40}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(16, 0, 10, 30), PAT(12),
     MODS(MLFO(LW_SH, -10, 0, 0), P_LPHASE, -1, MMOD(1, PS_LFO, PD_E(6), 16))},   /* FOLD at random on the 16ths */
    {"ACOUSTIC", {56, 64, 56, 62, 64, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 34), PAT(12)},
    {"DUB ROOTS", {60, 60, 50, 70, 59, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 18, 36), PAT(12),
     MODS(MMOD(1, PS_RAND, PD_DLY, 20))},                                       /* each hit its own throw into the echo */
    {"TECHNO RAW", {66, 64, 83, 62, 66, 10, 0, 30}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(10, 0, 0, 14), PAT(12),
     MODS(MLFO(LW_TRI, -1, 0, 0), P_LPHASE, -1, MMOD(1, PS_LFO, PD_E(2), 10))},   /* TONE opens and closes over 4 bars */
    {"BREAKS", {62, 66, 74, 62, 71, 0, 0, 10}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(4, 0, 0, 22), PAT(12)},
    {"HIP HOP", {58, 62, 56, 64, 67, 0, 0, 18}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(6, 0, 0, 20), PAT(14)},   /* (V10.34) */
    {"LATIN PERC", {60, 64, 64, 64, 64, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 22), PAT(12)},   /* (V10.7.8) */
};
_Static_assert(sizeof DRUM_PRESETS / sizeof DRUM_PRESETS[0] == sizeof DX_KITS / sizeof DX_KITS[0], "a DRUM preset a kit");
/* (0.7.2) a factory kit into the section's: the sounds, their LAYERs, their SENDs (the project saves them) */
static void dx_kit_load(uint32_t k)
{
    const dx_kit_t *K = &DX_KITS[k % DX_NKIT];
    memcpy(dx_kit, K->lane, sizeof dx_kit);
    memcpy(dx_lay, K->lay, sizeof dx_lay);
    memcpy(dx_snd, K->snd, sizeof dx_snd);
}

static const engine_t ENG_DRUM = {
    .name = "DRUM",
    .page_title = {"KIT", "KIT 2"},                     /* (0.6.54: both the whole kit's, was HIT) */
    .edit = {
        {"MORPH", F_PCT, 0, 127, 64, 0, 0},
        {"TUNE", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 64, 0, 0},
        {"DECAY", F_PCT, 0, 127, 64, 0, 0},
        {"NOISE", F_PCT, 0, 127, 64, 0, 0},
        {"FM ENV", F_PCT, 0, 127, 0, 0, 0},             /* (JIANT: was ACC; 0.6.36: was FM, a fixed FM: the FM struck
                                                         * with the hit, gone with the pitch envelope) */
        {"FOLD", F_PCT, 0, 127, 0, 0, 0},              /* (JIANT 0.5: a wavefolder, was WARP) */
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = DRUM_PRESETS,
    .npresets = NELEM(DRUM_PRESETS),
    .note_on = drum_note_on,
    .render = drum_render,
    .amp = drum_amp,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = DV_NLANE,
    .oneshot = 1,
    .keys = drum_keys,
};


