/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT (0.6.91) the song generator, second version: HOME + SELECT opens GEN, a screen of three pages (GENRE SEED
 * ENERGY FORM; DRUMS BASS CHORDS ARP; LEAD COMPLEX SWING HUMAN), OCT+ makes the piece, HOME (or OCT-) is done. Included by
 * ui_input.c after song_dice.c (whose helpers it shares: the roles, dhold, euclid, dname_has, dlevel ..).
 *
 * The piece is deterministic: a seed and the knobs make the same piece every time (the same settings and OCT+ again
 * throw a new seed). Each part (drums, bass, chords, arp, lead, sounds, form) draws from its own stream, so turning
 * one part's knob leaves the others as they were. What the tables know (gen_tables.c): the genre's tempo, modes and
 * progressions, its drums as a map of priorities (a hit where the level clears the density: ghosts go first, anchors
 * last), its bass as one of ~50 archetypes written against the kick, its sounds as recipes a role, its kit, HUMAN by
 * how human the genre is, the snare laid back where the genre does. FORM: a loop, parts (A B A C in ARRANGE, as
 * 0.6.81), or a club song (intro, build with a filter opening and a roll, drop, break). UNDO (GLO + G4) brings back
 * what was there. (V10.6m) The club song a plan of rows (gen_parts: a drum intro, grooves, a break, a four-bar build
 * with a roll, a riser and the master's high-pass, the drop's crash, an outro), each row's scene its layers (three
 * melodic at most), the fills at the phrases' ends only (a row's, not every loop's).
 * (0.8.3) Composed, not thrown: the key a mode of seven written to the SCALE page (QUANT SNAP) and the chord track's
 * CHORD page set to the piece's chords, so the keys play along in key; the harmony a plan of chords over the loop
 * (gplan: the genre's progression as degrees of the mode, a mode that would turn a chord diminished taking another;
 * the minor key's dominant raised at its cadence, jazz's turnaround, secondary dominants and mixture where the genre
 * colours, each a chord's alterations off the mode); every part reads the plan: the bass the chord's root at each change
 * and its own third, fifth, seventh, approaches that resolve; the chords voiced voice by voice (gvoice: common tones
 * held, the least motion, close, spread or rootless by the genre, no mud below G3); the arp the voiced chord; the lead
 * composed (gen_lead: a motif developed by a phrase template, chord tones on the beats, steps, a leap turned back, a
 * cadence on a stable degree, the borrowed chords' tones followed). The sections relate: B on the predominant, the pop
 * genres' last section a whole step up (a real modulation, its SCALE page stored with it). Every pitched note is in the
 * key but the rules' chromaticism (tests/gen_test.c measures it).
 * (0.8.6) GENRE's first choice, and where the screen always opens: EXPLORE, no genre, every rule broken (gx_genre).
 * (0.9.9-gen1) A piece from a clean base (gen_clean: NEW PROJECT's state, nothing of the piece before); each role's sound
 * its genre's archetype in the engine's real parameters (gen_tables.c RECIPE, packed), a pattern grammar a genre over
 * the maps and archetypes (GGRAM: the bass's accents, slides and octaves, DRUM-X GLUE, hat rolls, MUTATE). */

#include "gen_tables.c"

/* ------------------------------------------------------------------------------------------- the settings --- */
static struct {
    uint8_t genre;                                      /* 0 RANDOM, 1 + the genre, (0.8.6) GEN_EXPLORE */
    uint16_t seed;                                      /* 0..9999 */
    uint8_t energy, form;                               /* 0..100; 0 LOOP, 1 PARTS, 2 SONG */
    uint8_t amt[GP_N];                                  /* 0 OFF .. 100, GA_AUTO the genre's */
    uint8_t complex, swing, human;                      /* 0..100 (swing: GA_AUTO the genre's) */
    uint8_t page, hot;                                  /* the GEN screen's page, its last knob */
    uint32_t sig;                                       /* the settings of the last piece made (OCT+ again: a new seed) */
    char last[20];                                      /* the last piece: "HOUSE 0421" */
} gp = {GEN_EXPLORE, 1, 50, 1, {GA_AUTO, GA_AUTO, GA_AUTO, GA_AUTO, GA_AUTO}, 50, GA_AUTO, 30, 0, 0, 0, ""};

/* the generator's own random numbers: a stream a part, from the seed */
static uint32_t gq = 1;
static uint32_t gr(void) { gq ^= gq << 13; gq ^= gq >> 17; gq ^= gq << 5; return gq; }
static uint32_t gseed_mix(uint32_t part);
static void gseed(uint32_t part)
{
    gq = ((uint32_t)gp.seed + 1u) * 2654435761u ^ (part + 1u) * 0x9E3779B9u ^ 0x6D2B79F5u ^
         gseed_mix(part);                               /* (0.8.3: a genre's own motifs, figures and drums; V10.6z10: a
                                                         * section's bass, chords and arp their own variation) */
    if (!gq) gq = 1;
    gr(); gr(); gr();
}
static int32_t grr(int32_t lo, int32_t hi) { return hi <= lo ? lo : lo + (int32_t)(gr() % (uint32_t)(hi - lo + 1)); }
static int gch(uint32_t pct) { return gr() % 100u < pct; }
static uint32_t gpickw(const uint8_t *w, uint32_t n)    /* an index by its weight */
{
    uint32_t i, s = 0, r;
    for (i = 0; i < n; i++) s += w[i];
    if (!s) return 0;
    r = gr() % s;
    for (i = 0; i < n; i++) { if (r < w[i]) return i; r -= w[i]; }
    return n - 1u;
}
enum { GS_PICK, GS_DRUMS, GS_BASS, GS_CHORDS, GS_ARP, GS_LEAD, GS_SOUND, GS_FORM, GS_MIX, GS_TEX, GS_PLAN, GS_GRAM };

/* ------------------------------------------------------------------------------------------------ the piece --- */
/* (0.8.3) a chord of the plan: its first step, its steps, its root's degree in the mode, its quality (QSTACK) and how
 * its root, third, fifth and seventh move off the mode (bit pairs 0 2 4 6: 1 a semitone up, 2 down; 0 the mode's) */
typedef struct { uint8_t at, len, deg, q, alt; } gch_t;
#define GCH_N 8u
static struct {                                         /* (V10.6z10: the fields read most first, the arrays last: shorter
                                                         * code, ~70 B) */
    const ggenre_t *g;
    uint32_t gi;
    uint32_t len, cst;                                  /* the loop, a chord's steps */
    uint32_t amt[GP_N], energy, complex, human, swing;
    uint32_t fm6, ba, vm;
    uint8_t role[NTRK], act[RL_N], trk_of[RL_N];
    int16_t root, scale;                                /* the key, as the SCALE page has it (always a mode of seven) */
    uint8_t sc[7];                                      /* (0.8.3) the mode: its degrees' semitones over the tonic */
    uint8_t pal, hx;                                    /* the lead's degrees (bit d: degree d of the mode), the harmony */
    uint8_t pdeg[4], pq[4], pn;                         /* the progression as chosen: degrees, qualities with their colour */
    uint8_t nch;                                        /* the plan's chords (ch) */
    uint8_t mutant;                                     /* (0.6.95) RANDOM: a hybrid of two or three genres */
    uint8_t song;                                       /* (V10.6m) a FORM's piece being made: 1 PARTS / ARC, 2 the plan */
    uint8_t riff;                                       /* (V10.6p) LD_RIFF's riff (BASS_ARCH's BA_RF1 ..; gen_pick) */
    uint8_t sec;                                        /* (V10.6z10) the section being made (gen_parts; A 0) */
    int8_t reg;                                         /* (V10.6z10) its register: the arp's and the lead's windows moved */
    uint32_t drum_fill;                                 /* the fill forced (V10.6m: the fill section's, gen_parts) */
    uint32_t nvo, navo;                                 /* the chord track's and the arp's last voicings' notes (vo, avo) */
    uint64_t kick;                                      /* the kick's steps (bit i; the bass reads them) */
    gch_t ch[GCH_N];                                    /* the plan: the chords over the loop */
    int32_t vo[4];                                      /* the chord track's last voicing */
    int32_t avo[4];                                     /* the arp's */
} gs;
static __attribute__((noinline)) uint32_t gkick(uint32_t i) { return (uint32_t)(gs.kick >> (i & 63u)) & 1u; }
#define GKICK(i) gkick(i)
static __attribute__((noinline)) void gkick_of(const track_t *t, uint32_t len)   /* (gs.kick from a kit's steps) */
{
    uint32_t i;
    for (i = 0; i < len; i++) gs.kick |= (uint64_t)(t->step[i].time == ST_NOTE && (t->step[i].hit & 1u)) << i;
}
static uint32_t gseed_mix(uint32_t part)
{
    return part == GS_PICK ? 0u : (gs.gi + 1u) * 0x85EBCA6Bu ^ (part - GS_BASS < 3u ? gs.sec * 0x2545F491u : 0u);
}
static ggenre_t gmut;                                   /* (0.6.95) the genre as this throw plays it (its neighbours' parts) */
static char gmut_name[20];
#define GGR (gs.gi < NGENRE ? (uint32_t)GGRAM[gs.gi] : 0u)       /* (0.9.9-gen1) this piece's grammar (EXPLORE: none) */

static void gclear(step_t *s) { memset(s, 0, sizeof *s); s->time = ST_REST; }
static __attribute__((noinline)) void gclear_all(track_t *t) { uint32_t i; for (i = 0; i < NSTEP; i++) gclear(&t->step[i]); }

/* (0.8.6) EXPLORE: on (this piece is EXPLORE's), the engine gen_sound takes (-1: the recipe's); (V10.6n) its oddities
 * (GXO_: one or two a piece) and the role that is the odd one (its timbre, its components or its playhead) */
static struct { uint8_t on, odd, orole; int8_t eng; } gx = {0, 0, 0, -1};
enum { GXO_POLY = 1, GXO_SCALE = 2, GXO_TIMBRE = 4, GXO_TEMPO = 8, GXO_GLITCH = 16, GXO_FLOAT = 32, GXO_AN = 64, GXO_VO = 128 };   /* (the
                                                         * GENE fix: the components every piece's, the playheads half
                                                         * the pieces'; the last two bits ANIMAL and VOICE taken: gen_sound) */
/* (gen5) the club's four on the floor (HOUSE DEEP HOUSE TECH HOUSE, V10.6n: MINIMAL TECHNO's map, the first four): the
 * kick in front, the parts the records give it (no arp, no texture), the stabs' figures off the records */
static int gclub(void) { return !gx.on && gs.g->drum <= DM_TECHNO; }

static void gnote(step_t *s, const int32_t *n, uint32_t cnt, int32_t vel, uint32_t flags)
{
    uint32_t j;
    int32_t top = 0;
    s->n = (uint8_t)cnt;
    for (j = 0; j < 4u; j++) {
        s->note[j] = (uint8_t)(j < cnt ? clamp(n[j], 0, 127) : 0);
        if (j < cnt && n[j] > top) top = n[j];
    }
    s->time = ST_NOTE;
    s->flags = (uint8_t)(flags | (s->flags & SF_MFINE));   /* (0.9.1: MICRO's 64ths stay with its sixteenth) */
    if (top > 74) vel -= (top - 74) * 2 < 30 ? (top - 74) * 2 : 30;   /* (high notes measured: softer) */
    s->vel = (uint8_t)clamp(vel, 30, 127);
}

/* ---------------------------------------------------------------------------------- (0.8.3) the harmony --- */
/* the mode's degree d (any integer: 7 the octave) from the tonic at or above lo; its pitch class */
static int32_t gnd(int32_t lo, int32_t d)
{
    int32_t o = d >= 0 ? d / 7 : -((6 - d) / 7);
    return lo + ((gs.root - lo) % 12 + 12) % 12 + 12 * o + gs.sc[d - 7 * o];
}
static uint32_t gpc(int32_t d) { return (uint32_t)(gs.root + gs.sc[((d % 7) + 7) % 7]) % 12u; }
static int gminor(void) { return gs.sc[2] == 3u; }      /* the mode's third minor */
static int gin_mode(int32_t n)
{
    uint32_t pc = (uint32_t)(n - gs.root + 120) % 12u, d;
    for (d = 0; d < 7u; d++) if (gs.sc[d] == pc) return 1;
    return 0;
}
/* a chord's stack step s (0 its root, 2 its third, 4 its fifth ..) as a note from the tonic at or above lo, coloured */
static int32_t galt(const gch_t *c, int32_t s)
{
    uint32_t k = (uint32_t)(((s % 7) + 7) % 7), a;
    if (k & 1u) return 0;                               /* (its 2nd, 4th, 6th: the mode's) */
    a = (c->alt >> k) & 3u;
    return a == 1u ? 1 : a == 2u ? -1 : 0;
}
static int32_t gtone(const gch_t *c, int32_t s, int32_t lo) { return gnd(lo, c->deg + s) + galt(c, s); }
static int32_t giv(const gch_t *c, int32_t s) { return gtone(c, s, 12) - gtone(c, 0, 12); }   /* over its root */
/* its pitch classes as a line takes them: the stack, the root and the fifth, the third (not a suspension's) */
static uint32_t gpcm(const gch_t *c)
{
    uint32_t m = 0, j, q = c->q;
    for (j = 0; j < 4u && QSTACK[q][j] != 127; j++) m |= 1u << ((uint32_t)gtone(c, QSTACK[q][j], 12) % 12u);
    m |= 1u << ((uint32_t)gtone(c, 0, 12) % 12u) | 1u << ((uint32_t)gtone(c, 4, 12) % 12u);
    if (q != Q_SUS2 && q != Q_SUS4) m |= 1u << ((uint32_t)gtone(c, 2, 12) % 12u);
    return m;
}
static uint32_t gci(uint32_t step)                      /* the plan's chord at a step of any track's loop */
{
    uint32_t j = gs.nch ? gs.nch - 1u : 0u, s = gs.len ? step % gs.len : step;
    while (j && gs.ch[j].at > s) j--;
    return j;
}
static const gch_t *gchord(uint32_t step) { return &gs.ch[gci(step)]; }
static const gch_t *gchord_next(uint32_t step) { return &gs.ch[gs.nch ? (gci(step) + 1u) % gs.nch : 0u]; }
static int gchord_starts(uint32_t step) { return gs.len && gchord(step)->at == step % gs.len; }
static uint32_t gchord_pcs(uint32_t step) { return gpcm(gchord(step)); }
static uint32_t gchord_root_pc(uint32_t step) { return (uint32_t)gtone(gchord(step), 0, 12) % 12u; }
/* a chord's colour (F_MAJ F_DOM F_PAR F_FLAT) as moves of its tones off the mode; 0: not a chord this mode can take */
static int gcolour(gch_t *c, uint32_t f)
{
    static const uint8_t AEOL[7] = {0, 2, 3, 5, 7, 8, 10}, ION[7] = {0, 2, 4, 5, 7, 9, 11};
    int32_t have[4], want[4];
    uint32_t k;
    const uint8_t *P = 0;
    for (k = 0; k < 4u; k++) { uint32_t d = c->deg + 2u * k; have[k] = want[k] = gs.sc[d % 7u] + 12 * (int32_t)(d / 7u); }
    if (f == F_MAJ || f == F_DOM) {
        want[1] = have[0] + 4; want[2] = have[0] + 7;
        if (f == F_DOM) want[3] = have[0] + 10;
    } else if (f == F_PAR) P = gminor() ? ION : AEOL;   /* (the parallel mode: major's minor, minor's major) */
    else if (f == F_FLAT && have[0] != AEOL[c->deg]) P = AEOL;   /* (the flat side's: the mode's own when it has it) */
    for (k = 0; P && k < 4u; k++) { uint32_t d = c->deg + 2u * k; want[k] = P[d % 7u] + 12 * (int32_t)(d / 7u); }
    if (P && f == F_FLAT) { want[1] = want[0] + 4; want[2] = want[0] + 7; }
    c->alt = 0;
    for (k = 0; k < 4u; k++) {
        int32_t d = want[k] - have[k];
        if (d > 1 || d < -1) return 0;
        c->alt |= (uint8_t)((d > 0 ? 1u : d < 0 ? 2u : 0u) << (2u * k));
    }
    return 1;
}
static int gtriad_ok(const gch_t *c)                    /* major or minor (diminished: ii and vii in jazz and film, m7b5) */
{
    int32_t t3 = giv(c, 2), t5 = giv(c, 4);
    if (t5 == 7 && (t3 == 3 || t3 == 4)) return 1;
    return t5 == 6 && t3 == 3 && (HXF(gs.hx) == HF_JAZZ || HXF(gs.hx) == HF_CINE) && (c->deg == 1u || c->deg == 6u);
}
/* the quality as this degree of the mode takes it: a natural ninth only (never b9), no 11 over a major third, a 6
 * major, a fourth perfect, a fifth perfect; a coloured chord a triad or a seventh */
static uint32_t gq_fix(const gch_t *c, uint32_t q)
{
    if (c->alt) return q == Q_TRI || q == Q_SUS2 || q == Q_SUS4 || q == Q_POWER || q == Q_ADD9 ? (q == Q_POWER ? q : Q_TRI) : Q_7;
    if ((q == Q_7 || q == Q_9) && giv(c, 2) == 3 && giv(c, 6) == 11 && HXF(gs.hx) != HF_JAZZ && HXF(gs.hx) != HF_CINE)
        q = giv(c, 8) == 14 ? Q_ADD9 : Q_TRI;          /* (a minor-major seventh: the spy's chord, jazz's and film's only) */
    switch (q) {
    case Q_9: case Q_R9: if (giv(c, 8) != 14) q = Q_7; break;
    case Q_ADD9: if (giv(c, 8) != 14) q = Q_TRI; break;
    case Q_11: q = giv(c, 2) == 4 || giv(c, 8) != 14 ? (giv(c, 8) == 14 ? Q_9 : Q_7) : q; break;
    case Q_6: if (giv(c, 5) != 9) q = Q_7; break;
    case Q_SUS4: if (giv(c, 3) != 5) q = giv(c, 1) == 2 ? Q_SUS2 : Q_TRI; break;
    case Q_SUS2: if (giv(c, 1) != 2) q = giv(c, 3) == 5 ? Q_SUS4 : Q_TRI; break;
    case Q_QUART: if (giv(c, 3) != 5 || giv(c, 6) != 10) q = giv(c, 3) == 5 ? Q_SUS4 : Q_TRI; break;
    case Q_POWER: if (giv(c, 4) != 7) q = Q_TRI; break;
    default: break;
    }
    return q;
}
/* a progression the mode and the genre take: every chord major or minor (ii and vii diminished in jazz and film), a
 * secondary dominant only where the genre has them, a borrowed chord only where it borrows */
static int gprog_ok(const gprog_t *p)            /* (V10.6z4: gen_pick's only; V10.6z10: in the app, the segment full) */
{
    uint32_t k;
    for (k = 0; k < p->n; k++) {
        gch_t c = {0, 0, (uint8_t)p->deg[k], 0, 0};
        uint32_t f = QF(p->q[k]);
        if (!gcolour(&c, f) || !gtriad_ok(&c)) return 0;
        if (c.alt && (f == F_DOM ? !(gs.hx & HX_SEC) : f == F_MAJ ? !(gs.hx & HX_RAISE) : !(gs.hx & HX_BOR))) return 0;
        if (HXF(gs.hx) == HF_JAZZ && c.deg == 4u && !c.alt && !gminor() && giv(&c, 6) != 10) return 0;   /* (jazz's V a
                                                         * dominant: lydian's maj7 V is no ii-V) */
    }
    return 1;
}
/* chord j's second half another chord (the plan has room, the chord a bar at least, the new one a chord of the mode) */
static int gsplit(uint32_t j, uint32_t deg, uint32_t q, uint32_t f)
{
    gch_t c;
    uint32_t k, h;
    if (gs.nch >= GCH_N || j >= gs.nch || gs.ch[j].len < 16u) return 0;
    c.deg = (uint8_t)(deg % 7u);
    if (!gcolour(&c, f) || !gtriad_ok(&c)) return 0;
    c.q = (uint8_t)gq_fix(&c, q);
    h = gs.ch[j].len / 2u;
    c.at = (uint8_t)(gs.ch[j].at + h); c.len = (uint8_t)h;
    gs.ch[j].len = (uint8_t)h;
    for (k = gs.nch; k > j + 1u; k--) gs.ch[k] = gs.ch[k - 1u];
    gs.ch[j + 1u] = c;
    gs.nch++;
    return 1;
}
/* the plan: the progression's chords over the loop from its chord rot, each gs.cst steps; then the cadences and the
 * colours the genre takes (their own stream: the same plan every time for the same settings): the minor key's
 * dominant raised before its tonic (the leading tone), jazz's turnaround (ii V in the last chord), a secondary dominant
 * on the second half of the chord before its target, a major key's IV going minor before I */
static void gplan(uint32_t rot)
{
    uint32_t j, n = gs.pn ? gs.pn : 1u, st = gs.cst ? gs.cst : 16u, at, fam = HXF(gs.hx), keep = gq;
    gseed(GS_PLAN);
    while (st > 8u && st * n > gs.len) st /= 2u;        /* (a section's two chords where A had one: half each) */
    gs.nch = 0;
    for (at = 0; at < gs.len && gs.nch < GCH_N; at += st) {
        gch_t *c = &gs.ch[gs.nch];
        uint32_t k = (gs.nch + rot) % n;
        c->at = (uint8_t)at; c->len = (uint8_t)(at + st <= gs.len ? st : gs.len - at);
        c->deg = gs.pdeg[k];
        if (!gcolour(c, QF(gs.pq[k]))) c->alt = 0;
        c->q = (uint8_t)gq_fix(c, QQ(gs.pq[k]));
        gs.nch++;
    }
    if ((gs.hx & HX_RAISE || fam == HF_JAZZ) && gminor() && gs.sc[6] == 10u && gs.nch > 1u)
        for (j = 0; j < gs.nch; j++) {
            gch_t *c = &gs.ch[j];
            if (c->deg != 4u || c->alt || gs.ch[(j + 1u) % gs.nch].deg != 0u) continue;
            if (gcolour(c, F_MAJ)) c->q = (uint8_t)gq_fix(c, c->q == Q_TRI || c->q == Q_POWER ? c->q : Q_7);
            else c->alt = 0;
        }
    if (fam == HF_JAZZ && gs.complex > 30u && gs.nch && gs.ch[gs.nch - 1u].deg == 0u && gs.ch[gs.nch - 1u].len >= 16u) {
        gch_t *c = &gs.ch[gs.nch - 1u], keep = *c;
        c->deg = 1; c->alt = 0;
        if (gtriad_ok(c)) {
            c->q = (uint8_t)gq_fix(c, Q_7);
            if (!gsplit(gs.nch - 1u, 4, Q_7, gminor() ? F_MAJ : 0u)) *c = keep;
        } else *c = keep;
    }
    if ((gs.hx & HX_SEC) && gs.complex > 45u && gch(fam == HF_JAZZ ? 70u : 45u))
        for (j = 0; j + 1u < gs.nch; j++) {
            const gch_t *x = &gs.ch[j + 1u];
            uint32_t d = x->deg;
            if (x->alt || !(d == 1u || d == 2u || d == 3u || d == 5u) || gs.ch[j].deg == (d + 4u) % 7u) continue;
            if (gsplit(j, d + 4u, Q_7, F_DOM)) break;
        }
    if ((gs.hx & HX_BOR) && !gminor() && gs.complex > 45u && gch(40))
        for (j = 0; j + 1u < gs.nch; j++)
            if (gs.ch[j].deg == 3u && !gs.ch[j].alt && gs.ch[j + 1u].deg == 0u && gsplit(j, 3, Q_TRI, F_PAR)) break;
    gq = keep;                                          /* (the caller's stream as it was) */
}

/* ---------------------------------------------------------------------------------------------- the voicing --- */
enum { VO_CLOSE, VO_OPEN, VO_PLANE };
/* (0.8.3) a chord voiced inside [lo, hi]: of its inversions and spreads, the one nearest the last voicing (the least
 * motion, each voice to its nearest old one, the common tones held, the top voice steady); no second below G3, no close
 * third at the very bottom; CLOSE keeps the hand inside a tenth, OPEN (washes, film) spreads it (drop 2: the second
 * voice from the top an octave down; a triad's middle an octave up); PLANE the same shape moved, the root at the bottom
 * (techno, impressionism). Sorted low to high, no unisons */
static uint32_t gvoice(const gch_t *c, uint32_t q, int32_t lo, int32_t hi, int32_t *prev, uint32_t *np, uint32_t mode, int32_t *out)
{
    int32_t pc[4], best[4], cd[4], bcost = 0x7FFFFFFF;
    uint32_t n = 0, j, r, v, k;
    for (j = 0; j < 4u && QSTACK[q][j] != 127; j++) pc[n++] = gtone(c, QSTACK[q][j], 0);
    if (!n) return 0;
    for (r = 0; r < (mode == VO_PLANE ? 1u : n); r++) {
        int32_t base[4], o;
        for (j = 0; j < n; j++) base[j] = j + r < n ? pc[j + r] : pc[j + r - n] + 12;
        for (o = -1; o <= 10; o++) {
            int32_t sh = lo + (((base[0] - lo) % 12) + 12) % 12 + 12 * o - base[0];
            if (base[0] + sh < lo || base[0] + sh >= lo + 24) continue;
            for (v = 0; v < (mode == VO_PLANE ? 1u : 2u); v++) {
                int32_t cost = 0, span;
                for (j = 0; j < n; j++) cd[j] = base[j] + sh;
                if (v) {                                /* (the spread) */
                    if (n == 4u) cd[2] -= 12; else if (n == 3u) cd[1] += 12; else continue;
                    for (j = 1; j < n; j++) for (k = j; k > 0 && cd[k] < cd[k - 1]; k--) { int32_t x = cd[k]; cd[k] = cd[k - 1]; cd[k - 1] = x; }
                }
                if (cd[0] < lo || cd[n - 1u] > hi) continue;
                for (j = 1; j < n; j++) {
                    if (cd[j] <= cd[j - 1]) break;
                    if (cd[j - 1] < 55 && cd[j] - cd[j - 1] <= 2) break;   /* (no seconds below G3) */
                }
                if (j < n) continue;
                if (cd[0] < 52 && n > 1u && cd[1] - cd[0] < 5) cost += 6;   /* (a close third at the bottom: mud) */
                span = cd[n - 1u] - cd[0];
                if (*np && mode == VO_PLANE) cost += cd[0] > prev[0] ? cd[0] - prev[0] : prev[0] - cd[0];
                else if (*np) {                         /* (voice by voice, low to high; each to its nearest
                                                         * old one when their numbers differ; a common tone held) */
                    for (j = 0; j < n; j++) {
                        int32_t d = 127, e, com = 0;
                        for (k = 0; k < *np; k++) { e = cd[j] - prev[k]; if (e < 0) e = -e; if (e < d) d = e; com |= !e; }
                        if (n == *np) { d = cd[j] - prev[j]; if (d < 0) d = -d; }
                        cost += 2 * d - (com ? 3 : 0);
                    }
                } else {                                /* (the first: in the middle of the range) */
                    int32_t m = (cd[0] + cd[n - 1u]) / 2 - (lo + hi) / 2 + 2;
                    cost += m < 0 ? -m : m;
                }
                if (mode == VO_OPEN) cost += span < 12 ? 6 : 0;
                else if (span > 16) cost += 10 + (span - 16) * 2;
                if (cost < bcost) { bcost = cost; for (j = 0; j < n; j++) best[j] = cd[j]; }
            }
        }
    }
    if (bcost == 0x7FFFFFFF)                            /* (nothing fits: the root position from lo) */
        for (j = 0; j < n; j++) best[j] = pc[j] + lo + (((pc[0] - lo) % 12) + 12) % 12 - pc[0];
    for (j = 0; j < n; j++) { out[j] = best[j]; prev[j] = best[j]; }
    *np = n;
    return n;
}
/* the voicing family of the genre's chords */
static uint32_t gvo_mode(void)
{
    uint32_t f = HXF(gs.hx);
    return gs.vm == VM_PLANE ? VO_PLANE : f == HF_COLOR || f == HF_CINE ? VO_OPEN : VO_CLOSE;
}

/* -------------------------------------------------------------------------------------------------- drums --- */
/* lane k's level at step p (gen_drum_maps.h GM_LANE: a nibble a step; the text's '.' 0, '1'..'9' 26 a step, 'X' 255) */
static uint32_t gm_level(uint32_t k, uint32_t p)
{
    uint32_t v = (uint32_t)(GM_LANE[k][(p >> 1) & 7u] >> ((p & 1u) ? 0 : 4)) & 15u;
    return v == 10u ? 255u : v * 26u;
}
static void gdrum_clear_region(track_t *t, uint32_t from, uint32_t to, uint32_t lanes)
{
    uint32_t i;
    for (i = from; i < to; i++) {
        step_t *s = &t->step[i];
        s->hit &= (uint8_t)~lanes; s->acc &= s->hit;
        if (!s->hit) gclear(s);
    }
}
static void gdrum_hit(track_t *t, uint32_t i, uint32_t lane, int32_t vel, int acc)
{
    step_t *s = &t->step[i];
    if (s->time != ST_NOTE || !s->hit) { gclear(s); s->time = ST_NOTE; s->vel = (uint8_t)clamp(vel, 30, 127); }
    else if (!acc) s->vel = (uint8_t)clamp((s->vel + vel) / 2, 30, 127);
    s->hit |= (uint8_t)(1u << lane);
    if (acc) s->acc |= (uint8_t)(1u << lane); else s->acc &= (uint8_t)~(1u << lane);
}
/* (0.8.6) EXPLORE's drums: (V10.6n, the study's 6.2) one lane loose, not hits thrown about: its own chance, its hits a
 * little off the grid (rushed or laid back), a few of its steps the DRUM-X way (x2 x4 x8, SHORT GHOST FLAM; x8 a hat's
 * or the rim's only); the kick and the snare never */
static void gx_drums(track_t *t, uint32_t len)
{
    uint32_t i, l = 3u + gr() % 5u, ch = (uint32_t)grr(55, 85), m;
    int32_t mt = grr(-2, 2) * 4;
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        if (s->time != ST_NOTE || !((s->hit >> l) & 1u)) continue;
        if (gch(10 + gs.complex / 10u)) {
            m = gpickw(GX_LM_W, LM_N);
            step_set_lmode(s, l, m == LM_X8 && l != 3u && l != 6u ? LM_X4 : m);
        }
        if (s->hit != 1u << l) continue;                /* (the lane alone on its step) */
        step_set_chance(s, ch);
        if (!s->acc) ST_MICRO_SET(s, mt);
    }
}
/* (p1, main 0.9.18) a roll on lane l of step s alone (its mode x2 / x4: n 2..3, 4; main's x3 -> x4 put HOUSE's drums
 * past gen5's 75 % of the energy), not the step's RATCH: the step's other sounds (a kick under the hat) and a hit
 * recorded live into it never roll with it */
static void glane_rat(step_t *s, uint32_t l, uint32_t n)
{
    step_set_lmode(s, l, n > 2u ? LM_X4 : LM_X2);
}
/* (V10.6n, the study's 6.3) IDM: the break edited a bar at a time (after Attack's Warp drums: program a bar, then edit
 * each copy), the phrase's fourth bar most: a roll (the snare's or a hat's x2 / x4), a stutter (a step again one to three
 * times), a beat moved a step, half a bar reversed, the kick out for a beat (never the bar's one); the steps without the
 * kick or the snare a chance of their own; the phrase's last beat a roll of thirty-seconds (lv: 1 a little .. 3) */
static __attribute__((noinline)) void gidm_edit(track_t *t, uint32_t len, uint32_t lv)
{
    uint32_t b, i, j, k, nb = len / 16u;
    for (b = 0; b < nb; b++) {
        uint32_t n = gch(lv * 22u + (b % 4u == 3u ? 30u : 0u)) + (lv > 2u && b % 4u == 3u);
        for (j = 0; j < n; j++) {
            uint32_t p = gr() % 16u, at = b * 16u + p, q = b * 16u + (p & ~3u);
            step_t *s = &t->step[at], x;
            switch (gr() % 5u) {
            case 0:                                     /* a roll */
                if (s->time == ST_NOTE && (s->hit & 0x0Au)) glane_rat(s, s->hit & 2u ? 1u : 3u, (uint32_t)grr(2, 4));
                break;
            case 1:                                     /* a stutter */
                for (k = 1; k <= (uint32_t)grr(1, 3) && p + k < 16u; k++) t->step[at + k] = *s;
                break;
            case 2:                                     /* a beat moved a step late */
                x = t->step[q + 3u];
                for (k = 3; k; k--) t->step[q + k] = t->step[q + k - 1u];
                t->step[q] = x;
                break;
            case 3:                                     /* half a bar reversed */
                q = b * 16u + (p & 8u);
                for (k = 0; k < 4u; k++) { x = t->step[q + k]; t->step[q + k] = t->step[q + 7u - k]; t->step[q + 7u - k] = x; }
                break;
            default:                                    /* the kick out for a beat */
                for (k = q % 16u ? 0u : 1u; k < 4u; k++) {
                    s = &t->step[q + k];
                    if (s->time != ST_NOTE || !(s->hit & 1u)) continue;
                    s->hit &= (uint8_t)~1u; s->acc &= s->hit;
                    if (!s->hit) gclear(s);
                }
                break;
            }
        }
    }
    for (i = 0; i < len; i++)
        if (t->step[i].time == ST_NOTE && !(t->step[i].hit & 3u) && gch(30)) step_set_chance(&t->step[i], (uint32_t)grr(40, 85));
    if (nb >= 4u && !gs.song)                           /* (a song's: the fill row's) */
        for (k = 12; k < 16u; k++) {
            gdrum_hit(t, len - 16u + k, 1, 64 + (int32_t)k * 3, 0);
            glane_rat(&t->step[len - 16u + k], 1, 4);
        }
}
/* (V10.6z10) a throw's groove DNA, the first draws of its drums' stream (gen_new_seed reads them too): the map, its
 * lanes from the pools the genre lets go (gen_tables.c GDNA: before, the club's alone), the snare and the clap swapped;
 * the extra kicks of the genre's idiom (GDNA), none to three a throw, in every bar. Out: those kicks' steps */
static const uint8_t GDNA_PL[5][3] = {{3, 50, 8}, {4, 40, 6}, {6, 35, 8}, {7, 18, 6}, {5, 30, 6}};   /* (lane, %, pool's size) */
static const uint8_t *const GDNA_POOL[5] = {HAT_POOL_I, OH_POOL_I, RS_POOL_I, CB_POOL_I, TM_POOL_I};
static uint32_t gdna(uint8_t *map)
{
    uint32_t j, d = GDNA[gs.gi], km = 0, n;
    memcpy(map, DRUM_MAP_I[gs.g->drum], 8);
    for (j = 0; j < 5u; j++) if (((d >> (8u + j)) & 1u) && gch(GDNA_PL[j][1])) map[GDNA_PL[j][0]] = GDNA_POOL[j][gr() % GDNA_PL[j][2]];
    if ((d & GPL_SWAP) && map[1] && map[2] && gch(30)) { uint8_t x = map[1]; map[1] = map[2]; map[2] = x; }
    for (n = d & 255u ? gr() % (2u + gs.complex / 40u) : 0u; n; n--) km |= 1u << ((d >> 4u * (gr() & 1u)) & 15u);   /* (0 .. 1
                                                         * at COMPLEX 0, .. 3 past 80) */
    return km;
}
static void gen_drums(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    uint8_t map[8];                                     /* (0.9.7) lane numbers (gen_drum_maps.h; 0: none) */
    static const int8_t OFF[8] = {40, 20, 20, 0, 0, -20, -20, -20};
    uint32_t a = gs.amt[GP_DRUMS], dens = (uint32_t)clamp(40 + (int32_t)a * 2 + ((int32_t)gs.energy - 50) / 2, 20, 255), i, l, bars = len / 16u ? len / 16u : 1u;
    /* (0.6.93) ENERGY: the density and the velocities with it, the hats sixteenths above 66, the percussion thinned under 34 */
    uint32_t chaos = gs.complex * 40u / 100u, ng = (g->ghosts * a + 50u) / 100u + (gs.complex > 60u), km;
    const uint8_t *cell = HAT_CELL[g->hats];
    gseed(GS_DRUMS);
    gs.kick = 0;
    km = gdna(map);                                     /* (0.6.97) a throw's own top end (gen6: an archetype's as written:
                                                         * V10.6z10 its genre's GDNA); (the GENE fix) the breaks' and
                                                         * drum & bass's back on them */
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        uint32_t b = i / 16u, p = i % 16u, h = 0, acc = 0, hat_only = 1, lv_sum = 0, lv_n = 0, ghost_lv = 0;
        for (l = 0; l < 8u; l++) {
            uint32_t lv = map[l] ? gm_level(map[l], l == 3u && (g->spec & GSP_POLY) ? i % 12u : p) : 0u, lim;   /* (V10.6n:
                                                         * the 3:4 hats) */
            int32_t d;
            if (!lv) continue;
            if (lv < 230u) {                            /* (Grids: a perturbation a bar; V10.6z10: the first bar at half
                                                         * the second's, the phrase's last the most) */
                uint32_t k = chaos * ((0x4021u >> (4u * (b % 4u))) & 15u) / 4u;
                if (k) lv += gr() % (k + 1u);
            }
            d = (int32_t)dens + OFF[l];
            lim = (uint32_t)clamp(d, 0, 255);
            if (lv <= 255u - lim) continue;
            if (l >= 3u && lv < 230u && a < 100u && gr() % 100u >= 55u + a * 45u / 100u) continue;   /* (0.6.95) the hats
                                                         * and the percussion thinned by the amount, a hit at a time */
            h |= 1u << l;
            if (lv >= 230u) acc |= 1u << l;
            else { lv_sum += lv; lv_n++; }
            if (l < 3u || l > 4u) hat_only = 0;
            if (lv < 90u && l < 3u) ghost_lv = lv;
        }
        if (!h) continue;
        s->time = ST_NOTE; s->hit = (uint8_t)h; s->acc = (uint8_t)(acc & h);
        {
            int32_t vel = lv_n ? 56 + (int32_t)(lv_sum / lv_n) * 58 / 255 : 100;
            if (ghost_lv) vel = 36 + (int32_t)ghost_lv * 20 / 90;
            if (hat_only) vel = vel * cell[p % 4u] / 100;
            if (!(acc & h)) vel = vel * (88 + (int32_t)gs.energy / 4) / 100;
            if (gs.human) vel += grr(-(int32_t)gs.human / 12, (int32_t)gs.human / 12);
            s->vel = (uint8_t)clamp(vel, 30, 127);
        }
    }
    if (gs.energy > 66u && map[3] && g->human < 40u) {   /* the hats driven: the empty sixteenth after a closed hat, softly
                                                         * (V10.6m: the machines' only: not jazz's ride, bossa's, dub's) */
        uint8_t ch[NSTEP];
        for (i = 0; i < len; i++) ch[i] = (uint8_t)(t->step[i].time == ST_NOTE && (t->step[i].hit & 8u));
        for (i = 1; i < len; i++) {
            step_t *s = &t->step[i];
            if ((s->time == ST_NOTE && (s->hit & 0x18u)) || !ch[i - 1u] || !gch(55 + gs.energy / 4u)) continue;
            gdrum_hit(t, i, 3, 44 + (int32_t)gs.energy / 5, 0);
        }
    } else if (gs.energy < 34u && g->human < 40u) {    /* the room: half the open hats and the percussion gone (V10.6w:
                                                         * a band's ride kept, jazz's) */
        for (i = 0; i < len; i++) {
            step_t *s = &t->step[i];
            if (s->time != ST_NOTE || !(s->hit & 0xD0u) || !gch(50)) continue;
            s->hit &= (uint8_t)~0xD0u; s->acc &= s->hit;
            if (!s->hit) gclear(s);
        }
    }
    for (i = 0; i < len; i++)                           /* (V10.6z10) the throw's extra kicks (gdna), soft, every bar */
        if (((km >> (i % 16u)) & 1u) && !(t->step[i].time == ST_NOTE && (t->step[i].hit & 1u))) gdrum_hit(t, i, 0, 56, 0);
    for (i = 0, l = gr() % 4u; i < len && ng; i++) {    /* the snare's ghosts: the best positions first, a bar at a time
                                                         * (V10.6z10: from the throw's own place in the ranking) */
        uint32_t b = i / 16u, k, placed = 0;
        if (i % 16u) continue;
        for (k = 0; k < 12u && placed < ng; k++) {
            uint32_t p = GHOST_RANK[(k + l) % 12u], j = b * 16u + p;
            step_t *s = &t->step[j];
            if (j >= len || (s->time == ST_NOTE && (s->hit & 6u)) || !gch(55 + gs.complex / 3u)) continue;
            if (p && t->step[j - 1u].time == ST_NOTE && (t->step[j - 1u].hit & 2u) && (t->step[j - 1u].acc & 2u)) continue;
            gdrum_hit(t, j, 1, grr(34, 50), 0);
            placed++;
        }
    }
    if (gs.complex > 20u) {                             /* a variation on the second and fourth bars: a perc moved (V10.6z10:
                                                         * its kick added on a weak step now the throw's own, gdna) */
        for (i = 16; i < len; i++) {
            uint32_t b = i / 16u, p = i % 16u;
            step_t *s = &t->step[i];
            if ((b & p & 1u) && s->time == ST_NOTE && (s->hit & 0xC0u) && gch(gs.complex / 4u)) {
                s->hit ^= (uint8_t)(s->hit & 0xC0u) ^ (uint8_t)(((s->hit & 0x40u) << 1) | ((s->hit & 0x80u) >> 1));
                s->acc &= s->hit;
            }
        }
    }
    if (GSP_EDIT(g->spec)) gidm_edit(t, len, GSP_EDIT(g->spec));
    {   /* the fill: the phrase's last bar; the genre's kind. (V10.6m) A song's patterns have none: the fill is a row's,
         * the last loop before the section changes (gen_parts: the fill section's drums, gs.drum_fill). (the GENE fix) The
         * linear fill's genres (breaks, drum & bass) keep it in the loop: their turn is the pattern's second bar */
        uint32_t fl = gs.drum_fill ? gs.drum_fill : g->fills, last = bars - 1u, base = last * 16u, k;
        int32_t e = (int32_t)gs.energy;
        if (gs.drum_fill || (!(gs.song && g->fills != FL_LINEAR) && bars >= 2u && fl != FL_NONE && (bars >= 4u || gch(50)))) {
            uint32_t lane = map[1] ? 1u : map[2] ? 2u : 6u;
            switch (fl) {
            case FL_ROLL:                               /* (V10.7.5, after Compas: cumbia's break, a song's fill section:
                                                         * the bar the kick's and the guiro's alone, the roll its fill) */
                k = gs.drum_fill && (g->drum == DM_CUMBIA || g->drum - DM_CUMB2 < 2u);
                gdrum_clear_region(t, k ? base : base + 12u, base + 16u, k ? 0xF6u : 0x10u);
                for (k = 0; k < 4u; k++) {
                    gdrum_hit(t, base + 12u + k, lane, 70 + (int32_t)k * 15, k == 3u);
                    if (e > 55 && k >= 2u) glane_rat(&t->step[base + 12u + k], lane, 2);
                }
                break;
            case FL_TOM: {
                static const uint8_t TM[8] = {1, 0, 1, 1, 0, 1, 0, 1};
                gdrum_clear_region(t, base + 8u, base + 16u, 0x18u);
                for (k = 0; k < 8u; k++) if (TM[k]) gdrum_hit(t, base + 8u + k, 5, 88 + (int32_t)k * 3, k == 0u || k == 5u);
                if (bars >= 4u && !gs.song) gdrum_hit(t, 0, 4, 110, 1);   /* (a crash on the one; a song's: the drop's) */
                break;
            }
            case FL_DROP:
                gdrum_clear_region(t, base + 8u, base + 16u, 0x01u);
                gdrum_hit(t, base + 15u, 4, 100, 1);
                break;
            case FL_HAT:
                for (k = 12; k < 16u; k++) { gdrum_hit(t, base + k, 3, 70 + (int32_t)(k - 12u) * 12, 0); glane_rat(&t->step[base + k], 3u, k < 14u ? 2u : 3u); }
                break;
            case FL_LINEAR: {
                static const uint8_t BD[8] = {1, 0, 1, 0, 1, 0, 0, 1}, SD[8] = {0, 1, 0, 1, 0, 1, 1, 0};
                gdrum_clear_region(t, base + 8u, base + 16u, 0x1Fu);
                for (k = 0; k < 8u; k++) {
                    if (BD[k]) gdrum_hit(t, base + 8u + k, 0, 100, k == 0u);
                    if (SD[k]) gdrum_hit(t, base + 8u + k, 1, 84 + (int32_t)k * 4, k == 6u);
                }
                break;
            }
            default:                                    /* FL_KICK16 (V10.6n: metal's double kick) */
                for (k = 12; k < 16u; k++) gdrum_hit(t, base + k, 0, 96, 1);
                break;
            }
        }
    }
    if (g->drum - DM_REGG < 6u || g->drum - DM_REGG2 < 2u) {   /* (0.6.93) the hats' rolls: a step before the snare and the bar's
                                                         * end, sixteenth or triplet rolls (V10.6n: reggaeton's, dembow's,
                                                         * phonk's; V10.6p: both phonk maps; V10.6w: trap's, drill's;
                                                         * V10.7.2: perreo's, dembow rapido's) */
        for (i = 0; i < len; i++) {
            step_t *s = &t->step[i];
            uint32_t p = i % 16u;
            if (s->time != ST_NOTE || !(s->hit & 8u) || (s->hit & 3u)) continue;
            if ((p == 6u || p == 14u || p == 10u) && gch(35 + gs.complex / 2u)) glane_rat(s, 3u, (uint32_t)grr(2, p == 14u ? 4 : 3));
        }
    }
    gkick_of(t, len);                                   /* the kick's steps */
    for (i = 0; i < len; i++) {                         /* the snare laid back where the genre plays late */
        step_t *s = &t->step[i];
        if (s->time != ST_NOTE) continue;
        if (g->sd_micro && gs.human && (s->hit & 2u) && !(s->hit & 1u)) ST_MICRO_SET(s, g->sd_micro * 4);   /* (16ths) */
        if (g->drum == DM_CUMB2 && (s->hit & 8u) && i % 4u == 3u) ST_MICRO_SET(s, -11);   /* (V10.7.8: RKT's guiro pulled
                                                         * ~28 ms early at 92, its "a" a triplet's) */
        if (s->acc & ~s->hit) s->acc &= s->hit;
    }
    if (gx.on) gx_drums(t, len);
}

/* --------------------------------------------------------------------------------------------------- bass --- */
static int32_t gbass_root(int32_t lo, const gch_t *c)   /* a chord's root as the bass plays it: in [lo, lo + 12) */
{
    int32_t n = gtone(c, 0, lo);
    while (n >= lo + 12) n -= 12;
    while (n < lo) n += 12;
    return n;
}
/* (0.8.3) an archetype's degree over the chord as the plan has it, from its root as played: '3' its third (a
 * suspension's second or fourth), '5' its fifth, '7' its seventh, '2' '6' '9' the mode's steps over the root (a coloured
 * chord: its root, fifth, octave: never a step against its own altered tones) */
static int32_t gbass_tone(const gch_t *c, int32_t root, char ch)
{
    int32_t s;
    switch (ch) {
    case '3': s = c->q == Q_SUS2 ? 1 : c->q == Q_SUS4 ? 3 : 2; break;
    case '5': s = 4; break;
    case '7': s = 6; break;
    case '2': s = c->alt ? 0 : 1; break;
    case '6': s = c->alt ? 4 : 5; break;
    case '9': s = c->alt ? 7 : 8; break;
    default: s = 0; break;
    }
    return root + giv(c, s);
}
/* (0.9.9-gen2) the 303's line as X0X's TB-3PO writes one (after Phazerville's TB_3PO): one 16-step line, each step a
 * note at ~68 % (the beat's first sixteenth always; a rest else), a degree of the mode over the chord (the root most, on
 * the beat more), an octave up a third of the time (down now and then: the 1-octave jumps), an ACCENT on ~30 % of the
 * notes, a SLIDE on ~25 % (never into a rest: the gate held into the next note, which glides and does not retrigger),
 * a TIE now and then where a rest was; every other bar a few of its notes re-rolled (TB-3PO's mutate) */
static __attribute__((noinline)) void gen_acid(track_t *t, uint32_t len, int32_t lo)
{
    static const uint8_t W[7] = {34, 8, 14, 9, 14, 7, 14};   /* the mode's degrees over the chord: 1 2 3 4 5 6 7 */
    uint8_t dg[16], oc[16], fl[16];
    uint32_t i, b, bars = len / 16u ? len / 16u : 1u;
    for (i = 0; i < 16u; i++) {
        uint32_t on = i == 0u || gch(i % 4u == 0u ? 80u : 60u + gs.complex / 10u);
        dg[i] = (uint8_t)(i % 4u == 0u && gch(40) ? 0u : gpickw(W, 7));
        oc[i] = (uint8_t)(gch(30) ? 2u : gch(8) ? 0u : 1u);   /* (up, down, as written) */
        fl[i] = (uint8_t)(on ? 1u | (gch(30) ? 2u : 0u) | (gch(34) ? 4u : 0u) : gch(10) ? 8u : 0u);   /* note accent slide; tie */
        if (i && on && (fl[i - 1u] & 1u) && gch(30)) { dg[i] = dg[i - 1u]; oc[i] = (uint8_t)(oc[i - 1u] == 2u ? 1u : 2u); }   /* (the
                                                         * note before an octave away: the 303's jump) */
    }
    for (i = 0; i < 16u; i++) {                         /* (a slide into a rest does nothing; a tie only after a note) */
        if ((fl[i] & 4u) && !(fl[(i + 1u) % 16u] & 1u)) fl[i] &= (uint8_t)~4u;
        if ((fl[i] & 8u) && !(fl[(i + 15u) % 16u] & 9u)) fl[i] = 0;
    }
    for (b = 0; b < bars; b++) {
        for (i = 0; i < 16u; i++) {
            uint32_t s = b * 16u + i, d = dg[i], o = oc[i];
            const gch_t *c;
            int32_t n[1], r;
            if (s >= len) break;
            if (fl[i] & 8u) { dstep_tie(&t->step[s]); continue; }
            if (!(fl[i] & 1u)) continue;
            if ((b & 1u) && i && gch(15)) { d = gpickw(W, 7); o = gch(30) ? 2u : 1u; }   /* (the answer bar: a few re-rolled) */
            c = gchord(s);
            r = gbass_root(lo, c);
            n[0] = gchord_starts(s) ? r : r + giv(c, (int32_t)d);   /* (the root at a change) */
            if (o == 2u) n[0] += 12;
            else if (!o && n[0] >= lo + 7) n[0] -= 12;
            if (n[0] > lo + 24) n[0] -= 12;
            gnote(&t->step[s], n, 1, 84, ((fl[i] & 2u) ? SF_ACCENT : 0u) | ((fl[i] & 4u) ? SF_SLIDE : 0u));
        }
    }
}
/* (0.9.7) an archetype's note and flag at step i (gen_bass.h: the text's characters) */
#define BA_NOTE(c) (".=R23456789ALNSx"[(c) & 15u])
#define BA_FLAG(c) (".asg"[((c) >> 4) & 3u])
/* (p2, main 0.9.18's gba_vary) SEQ + SELECT's bass: the archetype as this press plays it, one to three of its steps
 * moved (a ghost or a soft root after a note on an off sixteenth, a weak root up the octave or to the fifth, a weak
 * note left out, a note a sixteenth late or early), the same in every bar (a riff, its own); (V10.6z10, the owner:
 * "the same bass every throw") GEN's pieces too, by COMPLEX (none to one under 20), and the leads' riffs */
static void gba_vary(uint8_t *bc, uint32_t alen)
{
    uint32_t n = (uint32_t)grr(gs.complex >= 20u, 2 + (int32_t)gs.complex / 40), j, tries;
    for (j = 0, tries = 0; j < n && tries < 24u; tries++) {
        uint32_t p = 1u + gr() % (alen - 1u), q = (p + 1u) % alen, c = bc[p] & 15u, pc = bc[p - 1u] & 15u, op = gr() % 4u;
        if (!(p % 4u) || (bc[q] & 15u) == 1u) continue;  /* (the beats kept; a note held on kept whole) */
        if (op == 0u && !c && (p & 1u) && pc > 1u) bc[p] = gch(50) ? 15u : 2u | 3u << 4;
        else if (op == 1u && c == 2u) bc[p] = (uint8_t)((bc[p] & 0xF0u) | (gch(60) ? 9u : 6u));
        else if (op == 2u && (c == 2u || c == 6u || c == 9u) && pc != 0u) bc[p] = 0;
        else if (op == 3u && q % 4u && (!c) != !(bc[q] & 15u) && c != 1u) { uint8_t x = bc[p]; bc[p] = bc[q]; bc[q] = x; }
        else continue;
        j++;
    }
}
/* (V10.7.2) CUMBIA, REGGAETON, DEMBOW (their maps, their throws'): their rules kept whole by V10.6z10's variations (the
 * bass's archetype as written: cumbia's tonica-quinta, the tresillo, the dembow's; the progression the genre's) */
static __attribute__((noinline)) int glatin(void)
{
    uint32_t d = gs.g->drum;
    return d == DM_CUMBIA || d - DM_REGG < 2u || d - DM_CUMB2 < 4u;
}
static void gen_bass(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    const barch_t *a = &BASS_ARCH[gs.ba];
    int32_t lo = g->bass_lo;
    uint32_t i, alen = 16u * a->bars, held = 0;
    uint8_t bc[64];
    gseed(GS_BASS);
    if (gs.ba == BA_ACID) { gen_acid(t, len, lo); return; }
    memcpy(bc, &BA_STEP[a->at], alen);
    if (!glatin()) gba_vary(bc, alen);                  /* (p5, the owner: EBM's basses were two motifs whatever the seed:
                                                         * its riff moved a seed, as the dice's; V10.6z10 every genre's;
                                                         * V10.7.2: the latin genres' as written) */
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        char c = BA_NOTE(bc[i % alen]), f = BA_FLAG(bc[i % alen]);
        const gch_t *cc = gchord(i), *nc = gchord_next(i);
        int32_t root, n[1], vel = 96, same = nc == cc || (nc->deg == cc->deg && nc->alt == cc->alt);
        uint32_t fl = 0, chg = gchord_starts(i);
        if (chg && i % 16u && (c == '.' || c == '=') && !(GKICK(i) && g->kick_mode == KM_ANSWER)) c = 'R';   /* (a chord
                                                         * that comes in mid-bar: its root there) */
        if (c == '.') { held = 0; continue; }
        if (c == '=') { if (held) dstep_tie(s); continue; }
        if (GKICK(i) && i % 16u && (g->kick_mode == KM_ANSWER || g->kick_mode == KM_INTERLOCK) && c != 'x' && c != 'r' && !chg) {
            held = 0; continue;                         /* (the kick's step is the kick's) */
        }
        root = gbass_root(lo, cc);
        switch (c) {
        case '7':                                       /* (a seventh alone over a major one with a major seventh: its
                                                         * sixth, the reggae's and the funk's note; in a run 6 7 8 it passes) */
            n[0] = gbass_tone(cc, root, giv(cc, 6) == 11 && giv(cc, 2) == 4 && BA_NOTE(bc[(i + alen - 1u) % alen]) != '6' &&
                                        BA_NOTE(bc[(i + 1u) % alen]) != '8' ? '6' : '7');
            break;
        case '2': case '3': case '5': case '6': case '9': n[0] = gbass_tone(cc, root, c); break;
        case 'L': n[0] = gbass_tone(cc, root, '5') - 12; break;
        case '4': n[0] = root + 5; break;               /* (V10.7.5: cumbia's turn, a perfect fourth over the root; out
                                                         * of the mode, the last loop below makes it the root) */
        case '8': n[0] = root + 12; break;
        case 'A': case 'B': {                           /* (into the next root as it will sound: the loop's first note
                                                         * when the loop closes) */
            int32_t r2 = nc == &gs.ch[0] && t->step[0].time == ST_NOTE ? t->step[0].note[0] : gbass_root(lo, nc);
            n[0] = same ? gbass_tone(cc, root, '5') : r2 + (c == 'A' ? -1 : 1);
            break;
        }
        case 'N': n[0] = gbass_root(lo, nc); break;
        case 'S': {                                     /* a step of the mode towards the next root */
            int32_t r2 = gbass_root(lo, nc);
            if (same) n[0] = gbass_tone(cc, root, '2');
            else n[0] = r2 + (r2 > root ? gnd(0, nc->deg - 1) - gnd(0, nc->deg) : gnd(0, nc->deg + 1) - gnd(0, nc->deg));
            break;
        }
        default: n[0] = root; break;                    /* R r x */
        }
        if (chg && c != 'x') n[0] = c == '8' ? root + 12 : root;   /* (0.8.3: the root at every change) */
        if (n[0] > lo + 26) n[0] -= 12;                /* (two octaves from the root's: the octave jumps live) */
        if (n[0] < lo - 7) n[0] += 12;
        if (c == 'r') vel = 70;
        if (c == 'x') vel = 42;
        if (f == 'g') vel = 50;
        if (f == 'a') { vel = 110; fl |= SF_ACCENT; }
        if (f == 's') fl |= SF_SLIDE;
        if (gs.human) vel += grr(-(int32_t)gs.human / 10, (int32_t)gs.human / 10);
        gnote(s, n, 1, vel, fl);
        held = 1;
    }
    if (g->kick_mode == KM_DOUBLE || g->kick_mode == KM_INTERLOCK) {   /* the kick doubled (DOUBLE: on its steps when the
                                                         * bass rests), interlocked (the step after it) */
        for (i = 0; i < len; i++) {
            uint32_t j = g->kick_mode == KM_DOUBLE ? i : i + 1u;
            int32_t n[1];
            if (!GKICK(i) || j >= len || t->step[j].time != ST_REST || dsounding(t, j) >= 0) continue;
            if (!gch(g->kick_mode == KM_DOUBLE ? 30 + gs.amt[GP_BASS] / 4u : 35)) continue;
            n[0] = gbass_root(lo, gchord(j));
            gnote(&t->step[j], n, 1, g->kick_mode == KM_DOUBLE ? 92 : 72, 0);
        }
    }
    if (len >= 64u && gs.complex > 30u && a->bars < 4u) {   /* the fourth bar's pickup: an approach into the next root */
        uint32_t j = 62;
        const gch_t *cc = gchord(63), *nc = gchord(0);
        int32_t n[1];
        if (t->step[j].time == ST_REST && t->step[j + 1u].time == ST_REST && gch(60)) {
            int32_t r = gbass_root(lo, cc);
            n[0] = nc->deg == cc->deg && nc->alt == cc->alt ? gbass_tone(cc, r, '5') : gbass_root(lo, nc) - 1;
            if (n[0] > lo + 26) n[0] -= 12;
            gnote(&t->step[j], n, 1, 84, 0);
        }
    }
    if (gs.amt[GP_BASS] < 60u) {                        /* a thinner bass: the weak notes thinned (never the note an
                                                         * approach resolves into) */
        int32_t last = -1;
        for (i = 0; i < len; i++) {
            step_t *s = &t->step[i];
            int keep;
            if (s->time != ST_NOTE) continue;
            keep = last >= 0 && !gin_mode(last);
            last = s->note[0];
            if (!keep && i % 4u && !(s->flags & SF_ACCENT) && gin_mode(s->note[0]) && gr() % 100u < (60u - gs.amt[GP_BASS])) {
                gclear(s);
                if (i + 1u < len && t->step[i + 1u].time == ST_TIE) gclear(&t->step[i + 1u]);
            }
        }
    }
    for (i = 0; i < len; i++) {                         /* (an approach left with nothing to go to: its chord's root) */
        step_t *s = &t->step[i];
        const step_t *x;
        uint32_t j;
        if (s->time != ST_NOTE || gin_mode(s->note[0])) continue;
        for (j = 1; j < len && t->step[(i + j) % len].time != ST_NOTE; j++) {}
        x = &t->step[(i + j) % len];
        if (j < len && (x->note[0] - s->note[0] == 1 || s->note[0] - x->note[0] == 1)) continue;
        if ((gpcm(gchord(i)) >> (s->note[0] % 12u)) & 1u) continue;
        s->note[0] = (uint8_t)gbass_root(lo, gchord(i));
    }
}

/* -------------------------------------------------------------------------------------------------- chords --- */
/* the chord at step i voiced for a track (its own last voicing): the pads' rootless where jazz has the bass on the root */
static uint32_t gchord_at(uint32_t i, int32_t lo, int32_t hi, int32_t *n, int32_t *prev, uint32_t *np, uint32_t mode, int rootless)
{
    const gch_t *c = gchord(i);
    uint32_t q = c->q;
    if (rootless && (q == Q_7 || q == Q_9) && !c->alt && giv(c, 8) == 14) q = Q_R9;
    return gvoice(c, q, lo, hi, prev, np, mode, n);
}
static void gen_pads(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    uint32_t i, a = gs.amt[GP_CHORDS], last = 0xFFFFu, cnt = 0, style = g->pads, mode = gvo_mode(), fam = HXF(gs.hx);
    int32_t n[4];
    int rootless = (fam == HF_JAZZ || (fam == HF_JAZZY && gs.vm == VM_RICH)) && gs.act[RL_BASS];
    uint8_t dub[16] = {0}, comp[16] = {0}, brk[16] = {0};
    gseed(GS_CHORDS);
    gs.nvo = 0;
    {   /* the bar figures kept the loop through */
        static const uint8_t AT[6] = {6, 10, 14, 7, 11, 13}, CP[6] = {2, 6, 7, 11, 14, 15}, BR[6] = {0, 3, 6, 10, 11, 14};
        uint32_t m = (uint32_t)grr(1, 2) + (gs.energy > 66u), j;
        dub[2] = 1;
        for (j = 0; j < m; j++) dub[AT[gr() % 6u]] = 1;
        for (j = 0, m = (uint32_t)grr(3, 5); j < m; j++) comp[CP[(gr() + j * 2u) % 6u]] = 1;
        comp[gch(50) ? 6u : 14u] = 1;                   /* (the anticipation of 3 or of the bar) */
        for (j = 0, m = (uint32_t)grr(3, 4); j < m; j++) brk[BR[gr() % 6u]] = 1;
    }
    /* (gen4) the bar's figure drawn once and played every bar, as the records keep theirs (Mr Fingers, Jeff Mills, DAF:
     * a part's bar repeats, 90 % and more): before, a coin each step made every bar another; fig[1] the phrase's last
     * bar (the fourth of four), the figure with one step moved (a push into the next phrase); hold and velocity still
     * by the step */
    uint32_t fig[2] = {0, 0}, stab1 = gch(50), sparse = 40u + a / 3u, keep = 70u + a * 3u / 10u, p, club = style == CS_STAB && gclub(), mem_n = 0;
    int32_t mem_r = 0;
    int8_t mem_iv[4];
    /* (gen5) the club's stabs off the records: the organ / M1 offbeats (Marshall Jefferson, Kerri Chandler), the piano's
     * 3 + 3 + 2 (Big Fun, Inner City), the deep house syncopation (Larry Heard, Chez Damier), Detroit's strings (Strings
     * of Life), techno's one stab a bar or its answer (Jeff Mills, Robert Hood): a figure a piece, the chord memory's
     * shape moved with the chords (VM_PLANE) */
    if (club) {
        fig[0] = STAB_FIG[STAB_SET[g->drum][gr() % 3u]];
        stab1 = 0;
    }
    for (p = 0; p < 16u && !club; p++) {
        int o = 0;
        switch (style) {
        case CS_STAB: o = p % 4u == 2u && gch(40 + a / 2u); break;
        case CS_PULSE: o = p % (gs.energy > 66u ? 2u : 4u) == 0u && !(p == 12u && gch(40)); break;
        case CS_DUB: o = dub[p]; break;
        case CS_SKANK: o = p % 4u == 2u; break;
        case CS_COMP: o = (comp[p] && gch(70 + a / 4u)) || (p == 0u && gch(40)) || (!comp[p] && (p == 6u || p == 10u || p == 13u) && gch(a / 4u)); break;
        case CS_CHARL: o = ((0x0909u >> p) & 1u) || (((0xC080u >> p) & 1u) && gch(30)); break;   /* (V10.6w) JAZZ's
                                                         * Charleston in its eighths: 1 and 2's "and" a bar (0 3, 8 11),
                                                         * now and then the next bar anticipated (7 15) or 4 (14) */
        case CS_POWER: o = p == 0u || (p == 8u && gch(60)); break;
        case CS_BROKEN: o = brk[p]; break;
        case CS_CHUG: o = (int)((CHUG_FIG[stab1 * 2u + (a > 60u)] >> p) & 1u); break;   /* (V10.6n: a riff, the amount's) */
        case CS_HOLD: case CS_SWELL: case CS_DRONE: case CS_SPARSE: break;
        default: o = ((0xDB6Du >> p) & 1u) && gch(70 + a / 4u); break;   /* CS_TRANCE */
        }
        if (o && (p == 0u || style == CS_SKANK || gch(keep))) fig[0] |= 1u << p;
    }
    if (!fig[0]) fig[0] = style == CS_STAB || style == CS_SKANK ? 0x4444u : 1u;
    fig[1] = fig[0];
    if (gch(70)) {
        uint32_t at = 10u + gr() % 6u;                  /* (a step of the bar's second half on or off) */
        fig[1] ^= 1u << at;
        if (!fig[1]) fig[1] = fig[0];
    }
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        const gch_t *c = gchord(i);
        uint32_t pp = i % gs.len - c->at, b = i / 16u, cl = c->len, f = fig[len >= 64u && b % 4u == 3u];
        int on, vel = 80, hold = 0;
        p = i % 16u;
        on = (int)((f >> p) & 1u);
        switch (style) {
        case CS_HOLD: on = pp == 0u; hold = (int)cl - 1; break;
        case CS_SWELL: on = pp == 0u; hold = (int)cl - 1; vel = 72; break;
        case CS_STAB: on |= pp == 0u && stab1; vel = p % 4u == 2u ? 84 : 92; break;
        case CS_PULSE: hold = (int)stab1; vel = p % 4u ? 76 : 86; break;
        case CS_DUB: vel = p == 2u ? 92 : 78; break;
        case CS_SKANK: vel = 84; break;
        case CS_COMP: case CS_CHARL: hold = p == 14u || p == 7u; vel = p == 0u ? 84 : 80; break;
        case CS_POWER: on = on && (p != 8u || (b & 1u)); hold = p == 8u ? 5 : 7; vel = 96; break;   /* (a push every other bar) */
        case CS_DRONE: on = i == 0u; hold = (int)len - 1; vel = 70; break;
        case CS_SPARSE: on = pp == 0u && (i == 0u || (gci(i) & 1u) == 0u || sparse > 66u); hold = (int)cl - 1; vel = 76; break;
        case CS_BROKEN: case CS_CHUG: vel = p % 4u ? 80 : 100; break;
        default: vel = p % 4u ? 78 : 96; break;         /* CS_TRANCE */
        }
        if (!on) continue;
        if (gci(i) != last || !cnt || style == CS_HOLD || style == CS_SWELL || style == CS_SPARSE) {
            cnt = gchord_at(i, style == CS_CHUG ? 46 : 48, style == CS_CHUG ? 70 : 79, n, gs.vo, &gs.nvo, mode, rootless);   /* (a
                                                         * riff's power chords low) */
            if (club && g->drum < DM_TECHHOUSE) {   /* (gen5) the CHORD MEMORY of house and
                                                         * Detroit (an M1, a Juno's): the first chord's shape, the same
                                                         * intervals over every chord's root (the nearest), so the chords
                                                         * move in parallel, their colour kept */
                int32_t r = (int32_t)gchord_root_pc(i), d;
                uint32_t j;
                if (!mem_n) {
                    mem_n = cnt;
                    mem_r = n[0] - (((n[0] - r) % 12) + 12) % 12;   /* (the root under the voicing's lowest note) */
                    for (j = 0; j < cnt; j++) mem_iv[j] = (int8_t)(n[j] - mem_r);
                }
                d = ((r - mem_r) % 12 + 12) % 12;
                d = d > 6 ? d - 12 : d;                 /* (the nearest: up to a tritone either way) */
                cnt = mem_n;
                for (j = 0; j < cnt; j++) n[j] = clamp(mem_r + d + mem_iv[j], 36, 96);
                for (j = 0; j < cnt; j++) gs.vo[j] = n[j];
                gs.nvo = cnt;
            }
            last = gci(i);
        } else {
            uint32_t j;
            for (j = 0; j < gs.nvo; j++) n[j] = gs.vo[j];
            cnt = gs.nvo;
        }
        if (hold > 0 && pp + (uint32_t)hold >= cl && style != CS_DRONE) hold = (int)(cl - pp) - 1;   /* (held to its chord's end) */
        if (gs.human) vel += grr(-(int32_t)gs.human / 12, (int32_t)gs.human / 12);
        gnote(s, n, cnt, vel, 0);
        if (hold > 0) dhold(t, i, (uint32_t)hold, len);
    }
}

/* ----------------------------------------------------------------------------------------------------- arp --- */
static void gen_arp(track_t *t, uint32_t len)
{
    const garp_t *ap;
    uint32_t i, k = 0, idx = 0, a = gs.amt[GP_ARP], no = 0, last = 0xFFFFu, cnt = 0, rate, mask;
    int32_t c[4], n[1];
    gseed(GS_ARP);
    ap = &ARP[gs.g->arp[gch(70) ? 0 : 1]];
    rate = ap->rate;
    if (rate == 1u && (a < 35u || (gs.energy < 34u && gch(50)))) rate = 2;   /* (0.6.93) a small amount: eighths */
    gs.navo = 0;
    for (no = 0; no < 8u && ap->ord[no] >= 0; no++) {}
    {   /* (gen4) the holes chosen once, the same every figure (before: a coin each step, every bar another: 30 % of the
         * bars as the one before): the amount is how many of the figure's steps play (a quarter .. all, three at the least), the strong
         * ones kept first (the beats, then the eighths, then the rest), a drawn turn among equals */
        uint32_t j, tier, nb = 0, want, got = 0, rot = gr() % 16u;
        for (j = 0; j < 16u; j++) nb += (ap->mask >> j) & 1u;
        want = (nb * (25u + a * 3u / 4u) + 99u) / 100u;
        if (want < 3u) want = nb < 3u ? nb : 3u;          /* (three at the least: a figure) */
        mask = 0;
        for (tier = 0; tier < 3u && got < want; tier++)
            for (j = 0; j < 16u && got < want; j++) {
                uint32_t q = tier ? (j + rot) % 16u : j;
                if (!((ap->mask >> q) & 1u) || ((mask >> q) & 1u)) continue;
                if (tier == 0u ? q % 4u != 0u : tier == 1u ? q % 2u != 0u : 0) continue;
                mask |= 1u << q;
                got++;
            }
    }
    for (i = 0; i < len; i++) {
        uint32_t j;
        int32_t o;
        if (i % rate) continue;
        idx = i / rate;
        if (!((mask >> (idx % (gs.g->spec & GSP_POLY ? 12u : 16u))) & 1u)) { k++; continue; }   /* (V10.6n: a 3:4 figure) */
        if (gci(i) != last || !cnt) {
            cnt = gchord_at(i, 50, 74, c, gs.avo, &gs.navo, gvo_mode() == VO_PLANE ? VO_PLANE : VO_CLOSE, 0);
            last = gci(i);
            if (gchord_starts(i)) k = 0;                /* (0.8.3: each chord's figure from its start) */
        } else {
            for (j = 0; j < gs.navo; j++) c[j] = gs.avo[j];
            cnt = gs.navo;
        }
        o = ap->ord[k % no];
        j = (uint32_t)(o & 3) % cnt;
        n[0] = c[j] + (o >= 4 ? 12 : 0);
        while (n[0] > 79 + gs.reg) n[0] -= 12;            /* (V10.6z10: a section's register, gen_parts) */
        while (n[0] < 48 + gs.reg) n[0] += 12;
        gnote(&t->step[i], n, 1, idx % 4u == 0u || k % no == 0u ? 92 : 78, 0);
        if (gs.complex > 50u && gch(6)) step_set_ratchet(&t->step[i], 2);
        k++;
    }
    t->p[P_SGATE] = (int16_t)clamp((int32_t)ap->gate + grr(-8, 8), 10, 127);
}

/* ---------------------------------------------------------------------------------------------------- lead --- */
/* (0.8.3) the lead composed: a line of notes as degrees of the mode (from the lead's tonic), written once polished.
 * The rules every style keeps: a chord tone on the strong steps (the beats), the lead's degrees (the genre's
 * pentatonic where it sings one), inside a tenth or so, a leap past a fourth followed by a step back, never more than
 * an octave, the phrase ending on a stable degree where it cadences, the tones a borrowed or secondary chord moves
 * followed (b7 to the leading tone over the minor key's V). MOTIF and HOOK: a one-bar motif (a rhythm cell and a
 * contour of steps), developed over the phrase by a template (a period: antecedent ending open on the 2nd or 5th,
 * consequent closing on the tonic; a sentence: the motif, its sequence, a continuation climbing to the climax two thirds
 * in, the cadence; A A B A; call and response; the techno hook A A A A') and one operator a varied bar */
#define GL_MAX 48u
typedef struct { uint8_t at, len, vel, fl, lock; int8_t d, chrom; } gln_t;
typedef struct { gln_t n[GL_MAX]; uint32_t k; int32_t tn, dlo, dhi; } gline_t;
static int gl_ct(int32_t d, uint32_t at) { return (int)((gchord_pcs(at) >> gpc(d)) & 1u); }
static int gl_pal(int32_t d) { return (int)((gs.pal >> (uint32_t)(((d % 7) + 7) % 7)) & 1u); }
static int32_t gl_pitch(const gline_t *L, int32_t d) { return gnd(L->tn, d); }
/* the chord tone at step at nearest degree d (the lead's degrees first, dir's side first on a tie), inside the window */
static int32_t gl_near(const gline_t *L, int32_t d, uint32_t at, int dir)
{
    int32_t k, best = d, bs = 99;
    for (k = 0; k <= 4; k++) {
        int32_t sgn, e;
        for (sgn = 0; sgn < 2; sgn++) {
            e = d + ((sgn == 0) == (dir >= 0) ? k : -k);
            if (e < L->dlo || e > L->dhi || !gl_ct(e, at)) continue;
            if (k * 2 + !gl_pal(e) < bs) { bs = k * 2 + !gl_pal(e); best = e; }
        }
    }
    return best;
}
static int32_t gl_fold(const gline_t *L, int32_t d)     /* into the window by octaves */
{
    while (d > L->dhi) d -= 7;
    while (d < L->dlo) d += 7;
    return d;
}
static void gl_add(gline_t *L, uint32_t at, int32_t d, uint32_t vel, uint32_t lock)
{
    gln_t *x;
    if (L->k >= GL_MAX) return;
    if (L->k && L->n[L->k - 1u].at >= at) return;       /* (in time order, one a step) */
    x = &L->n[L->k++];
    x->at = (uint8_t)at; x->d = (int8_t)gl_fold(L, d); x->vel = (uint8_t)vel; x->lock = (uint8_t)lock;
    x->fl = 0; x->len = 0; x->chrom = 0;
}
/* the cadence: the last note of a phrase on the tonic (a full close: else the third, the fifth, the nearest chord tone)
 * or open (a half close: the 2nd or the 5th, else the third), the octave nearest the note before */
static int32_t gl_cad(const gline_t *L, int32_t d, uint32_t at, int full)
{
    static const int8_t FULL[3] = {0, 2, 4}, HALF[3] = {1, 4, 2};
    const int8_t *T = full ? FULL : HALF;
    int32_t k, best = 0x7FFF, bo = 99, ref = L->k ? L->n[L->k - 1u].d : d;   /* (reached from the line's last note:
                                                         * the stable degree nearest it, the tonic first on a near tie) */
    for (k = 0; k < 3; k++) {
        int32_t e;
        for (e = T[k] - 7; e <= T[k] + 14; e += 7) {
            int32_t dd = (e - ref < 0 ? ref - e : e - ref) * 3 + (e - d < 0 ? d - e : e - d) + k * 2;
            if (e >= L->dlo && e <= L->dhi && gl_ct(e, at) && dd < best) { best = dd; bo = e; }
        }
    }
    return bo < 99 ? bo : gl_near(L, d, at, 0);
}
/* the line polished and written: the octave nearest the last note past a 9th; a leap past a fourth followed by a step
 * back (a third where the strong step wants a chord tone); a borrowed or secondary chord's moved tones followed; each
 * note held to the next (a breath before it, more of one at low energy); the slides now and then */
static int gl_clash(int32_t d, uint32_t at)              /* a minor ninth or a tritone over the chord's root (the bass's) */
{
    uint32_t iv = (gpc(d) + 12u - gchord_root_pc(at)) % 12u;
    return (iv == 1u || iv == 6u) && !gl_ct(d, at);
}
static void gl_write(track_t *t, gline_t *L, uint32_t len, uint32_t legato)
{
    uint32_t i;
    for (i = 0; i < L->k; i++) {                        /* no minor ninth, no tritone over the root: a step aside, a
                                                         * chord tone where one is a step away */
        gln_t *x = &L->n[i];
        int32_t dn = x->d - 1, up = x->d + 1;
        int okd, oku;
        if (!gl_clash(x->d, x->at)) continue;
        okd = !gl_clash(dn, x->at) && dn >= L->dlo; oku = !gl_clash(up, x->at) && up <= L->dhi;
        if (okd && (!oku || gl_ct(dn, x->at) || !gl_ct(up, x->at))) x->d = (int8_t)dn;
        else if (oku) x->d = (int8_t)up;
    }
    for (i = 1; i + 1u < L->k; i++) {                   /* the weak notes between two others: passing and neighbour
                                                         * tones (the smoothest of the note, a step up, a step down) */
        gln_t *x = &L->n[i];
        int32_t pv = gl_pitch(L, L->n[i - 1u].d), nv = gl_pitch(L, L->n[i + 1u].d), k, best = x->d, bc = 0x7FFF;
        if (x->at % 8u == 0u || x->lock) continue;
        {   /* (only where the line skips: a step in and a step out are left as they are) */
            int32_t p0 = gl_pitch(L, x->d), a1 = p0 - pv, a2 = nv - p0;
            if (a1 < 0) a1 = -a1;
            if (a2 < 0) a2 = -a2;
            if (a1 <= 2 && a2 <= 2 && (a1 || a2)) continue;
        }
        for (k = -1; k <= 1; k++) {
            int32_t d = x->d + k, p = gl_pitch(L, d), c;
            if (d < L->dlo || d > L->dhi || gl_clash(d, x->at)) continue;
            c = (p > pv ? p - pv : pv - p) + (nv > p ? nv - p : p - nv) + (k ? 2 : 0) + (gl_pal(d) ? 0 : 4) +
                (d == L->n[i - 1u].d ? 5 : 0) + (gl_ct(d, x->at) ? 0 : x->at % 4u ? 1 : 3);   /* (beats 2 and 4: a
                                                         * passing note only where it makes the line move by steps) */
            if (c < bc) { bc = c; best = d; }
        }
        x->d = (int8_t)best;
    }
    for (i = 0; i < L->k; i++) {                        /* no avoid note held a beat or more (the 4th over a major
                                                         * third, b6 over the fifth, b9 over the root) */
        gln_t *x = &L->n[i];
        uint32_t nxt = i + 1u < L->k ? L->n[i + 1u].at : len, dur = x->len ? x->len : nxt - x->at;
        const gch_t *c = gchord(x->at);
        int sus3 = (c->q == Q_SUS2 || c->q == Q_SUS4) && gpc(x->d) == (uint32_t)gtone(c, 2, 12) % 12u;   /* (a suspension's
                                                         * third: against its 4th) */
        if (dur >= 4u && !x->lock && !gl_ct(x->d, x->at) && (sus3 || ((gchord_pcs(x->at) >> ((gpc(x->d) + 11u) % 12u)) & 1u)))
            x->d = (int8_t)gl_near(L, x->d, x->at, i ? (L->n[i - 1u].d > x->d ? 1 : -1) : 0);   /* (a semitone over a chord
                                                         * tone, held: down onto it, or the nearest) */
    }
    for (i = 1; i < L->k; i++) {                        /* never past an octave */
        gln_t *x = &L->n[i], *p = &L->n[i - 1u];
        int32_t iv = gl_pitch(L, x->d) - gl_pitch(L, p->d);
        if (iv > 12 && x->d - 7 >= L->dlo - 2) x->d = (int8_t)(x->d - 7);
        else if (iv < -12 && x->d + 7 <= L->dhi + 2) x->d = (int8_t)(x->d + 7);
    }
    for (i = 1; i + 1u < L->k; i++) {                   /* a leap past a fourth: the next note a step back (a third
                                                         * where the strong step wants a chord tone) */
        gln_t *x = &L->n[i], *p = &L->n[i - 1u], *nx = &L->n[i + 1u];
        int32_t iv = gl_pitch(L, x->d) - gl_pitch(L, p->d), sg = iv > 0 ? 1 : -1, a = iv < 0 ? -iv : iv, nv, b;
        if (a <= 5 || nx->lock) continue;
        nv = gl_pitch(L, nx->d) - gl_pitch(L, x->d);
        b = nv < 0 ? -nv : nv;
        if (b && b <= 2 && (nv > 0) != (iv > 0)) continue;   /* (already back by a step) */
        if ((nx->at % 16u || gl_ct(x->d - sg, nx->at)) && !gl_clash(x->d - sg, nx->at)) nx->d = (int8_t)(x->d - sg);   /* (on
                                                         * beat 3 an appoggiatura too; on the bar's one a chord tone) */
        else if (gl_ct(x->d - 2 * sg, nx->at)) nx->d = (int8_t)(x->d - 2 * sg);
    }
    for (i = 0; i < L->k; i++) {                        /* a coloured chord's moved tones: the line follows them */
        gln_t *x = &L->n[i];
        const gch_t *c = gchord(x->at);
        uint32_t s;
        if (!c->alt) continue;
        for (s = 0; s <= 6u; s += 2u) {
            int32_t al = galt(c, (int32_t)s);
            uint32_t ap = (uint32_t)gtone(c, (int32_t)s, 12) % 12u;
            if (al && (gpc(x->d) + 12u + (uint32_t)al) % 12u == ap && ((gpcm(c) >> ap) & 1u)) { x->chrom = (int8_t)al; break; }
        }
    }
    for (i = 0; i < L->k; i++) {
        gln_t *x = &L->n[i];
        uint32_t nxt = i + 1u < L->k ? L->n[i + 1u].at : len + L->n[0].at, gap, h;
        int32_t n[1];
        if (x->at >= len) break;
        n[0] = gl_pitch(L, x->d) + x->chrom;
        gnote(&t->step[x->at], n, 1, x->vel, (x->fl & 1u) ? SF_SLIDE : 0u);
        gap = nxt - x->at;
        h = x->len ? x->len : gap > legato ? gap - legato : 0u;
        if (h + x->at >= len) h = len - x->at - 1u;
        if (h) dhold(t, x->at, h, len);
    }
}
/* the motif's rhythm cells: figures that hold together (a long note and two short, a tresillo, a pickup into three, a
 * dotted push ..); (V10.6p) bit p the step p (were text: 228 B less): x..x..x...x..... x.x.x...x.x..... x...x.x.x.......
 * ..x...x...x...x. x..x.x..x..x.x.. x.......x.x.x... x...x...x...x... x.x...x.x...x... ..x.x...x.x..... x..x..x..x..x...
 * x.....x...x.x... x...x.x.......x. */
static const uint16_t GL_CELL[12] = {0x0449u, 0x0515u, 0x0151u, 0x4444u, 0x2929u, 0x1501u, 0x1111u, 0x1145u, 0x0514u,
                                     0x1249u, 0x1441u, 0x4051u};
enum { LB_M, LB_V, LB_R, LB_C, LB_REST };               /* a bar of the phrase: the motif, varied, the answer, the
                                                         * continuation, a rest */
/* (V10.6p) a riff as written (gen_tables.c BASS_ARCH's BA_RF1 ..): its codes the key's degrees from the tonic (R .. 9:
 * 0 .. 8, A -1, L -3, N 9, S -2), the tonic C4 .. B4 (the 808 cowbell's place), looped whatever the chords (the hook
 * over them, as the records: the 808 moves under it); an accent louder, a slide */
static void gen_riff(track_t *t, uint32_t len)
{
    const barch_t *a = &BASS_ARCH[gs.riff];
    uint32_t i, alen = 16u * a->bars;
    int32_t tn = 60 + ((gs.root - 60) % 12 + 12) % 12;
    uint8_t bc[32];                                     /* (V10.6z10) the riff a throw's own (gba_vary: its codes are
                                                         * the bass's: R up the octave or to the fifth, a ghost, a step moved) */
    memcpy(bc, &BA_STEP[a->at], alen);
    gq ^= gs.sec * 0x2545F491u;                         /* (a section's own) */
    gba_vary(bc, alen);
    for (i = 0; i < len; i++) {
        uint32_t c = bc[i % alen], f = c >> 4;
        int32_t n[1];
        c &= 15u;
        if (c < 2u) { if (c) dstep_tie(&t->step[i]); continue; }
        n[0] = gnd(tn, c <= 10u ? (int32_t)c - 2 : (int8_t)"\xff\xfd\x09\xfe\x00"[c - 11u]);
        gnote(&t->step[i], n, 1, f == 1u ? 114 : 94, f == 2u ? SF_SLIDE : 0u);
    }
}
static void gen_lead(track_t *t, uint32_t len)
{
    const ggenre_t *g = gs.g;
    uint32_t i, j, a = gs.amt[GP_LEAD], bars = (len + 15u) / 16u, style = g->lead, fam = HXF(gs.hx), legato = gs.energy < 34u ? 4u : 2u;
    gline_t L;
    gseed(GS_LEAD);
    if (style == LD_NONE) return;
    if (style == LD_RIFF) { gen_riff(t, len); return; }
    L.k = 0;
    L.tn = 55 + ((gs.root - 55) % 12 + 12) % 12;        /* the lead's tonic (degree 0): G3 .. F#4 */
    {   /* its window, the same in every key: Bb3 .. E5 (a motif, a hook), lower for an ostinato and long notes, higher
         * for Part's line */
        int32_t lo = (style == LD_OSTINATO ? 55 : style == LD_SPARSE ? 56 : style == LD_TINTI ? 60 : 58) + gs.reg, hi = lo + 17;
        if (style == LD_OSTINATO) hi = 72 + gs.reg;
        if (style > LD_NONE) hi = lo + 26;               /* (gen6: an archetype's octave leaps whole) */
        for (L.dlo = -7; gnd(L.tn, L.dlo) < lo; L.dlo++) {}
        for (L.dhi = L.dlo; gnd(L.tn, L.dhi + 1) <= hi; L.dhi++) {}
    }
    if (style == LD_OSTINATO) {                         /* a cell of two to four notes (repeats welcome) on an euclid, the
                                                         * chord's tones on the beats, its last note lifted on the third bar */
        int8_t cl[4];
        uint32_t k = (uint32_t)grr(3, 4) + (gs.energy > 66u), rot = (uint32_t)grr(0, 3) + gs.sec, nc = (uint32_t)grr(2, 4), q = 0, bre = 0, keep = 0xFFFFu;
        int32_t d = gl_near(&L, grr(0, 4), 0, 1);
        for (j = 0; j < nc; j++) {
            static const uint8_t W[5] = {25, 45, 20, 0, 10};   /* (a repeat, a step, a third, -, a fourth) */
            uint32_t m = gpickw(W, 5);
            if (j + 1u == nc && !m && cl[0] == d && (j < 2u || cl[1] == d)) m = 1;   /* (two notes at least) */
            cl[j] = (int8_t)d;
            d += (gch(50) ? 1 : -1) * (int32_t)m;
            if (d < L.dlo + 1 || d > L.dhi - 2) d -= 2 * (d > L.dhi - 2 ? (int32_t)m : -(int32_t)m);   /* (inside, off the
                                                         * window's edges: room for the chords' moves) */
        }
        for (j = 0; j < 16u; j++) if (j % 4u && gr() % 100u >= 50u + a / 2u) keep &= ~(1u << j);   /* (the same holes a bar) */
        for (i = 0; i < len; i++) {
            uint32_t b = i / 16u;
            int32_t dd, sh = (int32_t)gchord(i)->deg - (int32_t)gchord(0)->deg;
            if (i % 16u == 0u) bre = b % 4u == 3u && gch(50);   /* (the fourth bar: a breath, its second half) */
            if (!euclid(i % 16u, k, 16, rot)) continue;
            if (bre && i % 16u >= 8u) continue;
            if (!((keep >> (i % 16u)) & 1u)) continue;
            sh = (sh + 10) % 7 - 3;                     /* (-3 .. 3: the nearest way) */
            dd = cl[q % nc] + sh + (b % 4u == 2u && q % nc == nc - 1u ? 1 : 0);
            if (i % 4u == 0u || !gl_pal(dd)) dd = gl_near(&L, dd, i, 0);
            gl_add(&L, i, dd, q % nc == 0u ? 98 : 82, 0);
            if (gch(12) && L.k) L.n[L.k - 1u].fl = 1;
            q++;
        }
        gl_write(t, &L, len, 1);
        return;
    }
    if (style > LD_NONE) {                              /* (gen6) an archetype's motif (gen_tables.c LD_CELLS) */
        const gcell_t *c = &LD_CELLS[style == LD_EBM && gch(50) ? 2u : style - LD_EBM];   /* (V10.6n: EBM's two shouts) */
        int32_t o = -14, lo = c->d[0] < c->d[1] ? c->d[0] : c->d[1];
        uint32_t q = 0;
        while (lo + o < L.dlo) o += 7;                   /* (the motif from the window's floor: its octave whole) */
        for (i = 0; i < len; i += c->every, q++) {
            int32_t sh = (int32_t)gchord(i)->deg - (int32_t)gchord(0)->deg, dd;
            sh = (sh + 10) % 7 - 3;                     /* (-3 .. 3: the nearest way) */
            dd = c->d[q % 4u] + o + sh + ((i / 16u) % 4u == 3u && q % 4u == 3u);   /* (as written: the fourth too) */
            gl_add(&L, i, dd, q % 4u ? 88u : 100u, 1);   /* (locked: the voice leading leaves it) */
            if (L.k) L.n[L.k - 1u].len = c->hold;
        }
        gl_write(t, &L, len, 1);
        return;
    }
    if (style == LD_MECH) {                             /* a machine's figure on an euclid, moved with the chord */
        static const int8_t MT[4][8] = {{0, 1, 2, 1, 0, 2, 4, 2}, {4, 4, 2, 1, 2, 2, 0, -1}, {0, 0, 4, 3, 2, 1, 2, 0}, {7, 6, 4, 4, 2, 4, 5, 4}};
        const int8_t *mt = MT[gr() % 4u];
        uint32_t k = (uint32_t)grr(4, 6) + (gs.energy > 66u), rot = (uint32_t)grr(0, 3), idx, q;
        int32_t oo = 0, o, best = 99;
        for (o = -7; o <= 7; o += 7) {                  /* (the figure's octave: the most of it inside the window) */
            int32_t c = 0;
            for (q = 0; q < 8u; q++) c += mt[q] + o < L.dlo || mt[q] + o > L.dhi;
            if (c < best) { best = c; oo = o; }
        }
        for (i = 0; i < len; i++) {
            int32_t dd, sh;
            if (!euclid(i % 16u, k, 16, rot)) continue;
            for (q = 0, idx = 0; q < i % 16u; q++) idx += euclid(q, k, 16, rot);
            sh = (int32_t)gchord(i - i % 16u)->deg - (int32_t)gchord(0)->deg;
            sh = (sh + 10) % 7 - 3;                     /* (-3 .. 3: the nearest way) */
            dd = mt[idx % 8u] + sh + oo;
            if (i % 4u == 0u || !gl_pal(dd)) dd = gl_near(&L, dd, i, 0);
            gl_add(&L, i, dd, 92, 0);
        }
        gl_write(t, &L, len, 1);
        return;
    }
    if (style == LD_SPARSE || style == LD_TINTI) {      /* long notes (SPARSE: a guide-tone line, each the chord tone
                                                         * nearest the last, now and then a short figure stepping into it;
                                                         * TINTI: Part's line, stepwise, a chord tone on each long note) */
        int32_t d = gl_near(&L, style == LD_TINTI ? grr(2, 5) : 2, 0, 1), dir = gch(50) ? -1 : 1;
        uint32_t fig = style == LD_SPARSE && gch(55), off = style == LD_SPARSE && gch(40) ? 4u : 0u, nlong = 0;
        for (i = 0; i < len; i += 8u) {
            uint32_t b = i / 16u, at = i + off, hold, last = i + 8u >= len;
            if (at >= len) break;
            if (style == LD_SPARSE && ((i % 16u && gr() % 100u >= a) || (b % 2u == 1u && gch(40))) && !last) continue;
            if (style == LD_SPARSE && nlong) {
                int32_t e, k2;                          /* (moving on: the next chord tone that way, a step or a
                                                         * third; the same note only now and then) */
                if (d + 2 * dir < L.dlo || d + 2 * dir > L.dhi) dir = -dir;
                for (k2 = 1, e = d; k2 <= 3; k2++) if (gl_ct(d + dir * k2, at)) { e = d + dir * k2; break; }
                if (e == d || (gl_ct(d, at) && gch(15))) e = gl_near(&L, d, at, dir);
                d = e;
                if (gch(35)) dir = -dir;
            } else if (style == LD_SPARSE) {
                d = gl_near(&L, d, at, 1);              /* (the first: a chord tone near the third) */
            } else if (i % 16u == 0u) {                 /* (TINTI: the bar's long note a chord tone a step or a third
                                                         * on; its second a step on, passing) */
                int32_t e = d + dir;
                if (e < L.dlo + 1 || e > L.dhi - 1) { dir = -dir; e = d + dir; }
                d = gl_ct(e, at) ? e : gl_ct(e + dir, at) ? e + dir : gl_near(&L, e, at, dir);
            } else {
                at = i + 2u;                            /* (off the beat: a passing note) */
                d += dir;
                if (d < L.dlo || d > L.dhi) { dir = -dir; d += 2 * dir; }
            }
            if (last) d = gl_cad(&L, d, at, 1);         /* (the loop closes on the tonic) */
            hold = style == LD_TINTI ? 6u : 12u - (gs.energy > 66u ? 4u : 0u) - off;
            if (fig && gch(60) && nlong) {              /* the figure: one or two short notes stepping into the long one */
                uint32_t q, nf = (uint32_t)grr(1, 2);
                int32_t s = gch(50) ? 1 : -1;
                for (q = 0; q < nf && at + q * 2u + 2u < len; q++) gl_add(&L, at + q * 2u, d + s * (int32_t)(nf - q), 76, 0);
                at += nf * 2u;
                hold = hold > nf * 2u ? hold - nf * 2u : 2u;
            }
            if (at >= len) break;
            gl_add(&L, at, d, 84, last);
            if (L.k) L.n[L.k - 1u].len = (uint8_t)hold;
            nlong++;
        }
        gl_write(t, &L, len, 2);
        return;
    }
    {   /* MOTIF, HOOK */
        static const uint8_t TPL[6][4] = {{LB_M, LB_V, LB_M, LB_V},    /* a period: open, closed */
                                          {LB_M, LB_M, LB_C, LB_V},    /* a sentence */
                                          {LB_M, LB_M, LB_R, LB_V},    /* A A B A' */
                                          {LB_M, LB_M, LB_M, LB_V},    /* the hook: A A A A' */
                                          {LB_M, LB_R, LB_M, LB_R},    /* call and response */
                                          {LB_M, LB_REST, LB_M, LB_V}};/* the dub's: space */
        static const int8_t ARCH[6][4] = {{0, 0, 1, 0}, {0, 0, 2, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 0}};
        uint8_t at[8], used[16] = {0};
        int8_t mo[8];
        uint32_t k = 3u + gs.complex / 40u + (a > 60u), nc = 0, tpl, op = gr() % 5u, ct = gr() % 4u;   /* (op: the varied
                                                         * bar's: 0 TRUNC, 1 RETRO, 2 END moved, 3 ORN, 4 AUG) */
        int32_t anchor, d = 0, pv = 0, base0;
        uint32_t cell = GL_CELL[gr() % 12u];
        if (a < 35u && k > 3u) k--;
        if (k > 6u) k = 6u;
        tpl = style == LD_HOOK ? (gch(65) ? 4u : 2u) :
              fam == HF_LOOP ? (gch(60) ? 3u : 5u) :
              fam == HF_POP ? (gr() % 3u == 0u ? 1u : gch(50) ? 0u : 4u) :
              fam == HF_JAZZ || fam == HF_JAZZY ? (gch(40) ? 1u : gch(50) ? 2u : 0u) : (gch(50) ? 0u : 1u);
        if (gs.sec) { op = (op + gs.sec) % 5u; tpl = (tpl + gs.sec) % 6u; }   /* (V10.6z10) a section's development of the
                                                         * same motif: another of the varied bar's operators (TRUNC RETRO
                                                         * END ORN AUG), another phrase (the answer inverted, call and
                                                         * response, space) */
        for (i = 0; i < 16u; i++) if ((cell >> i) & 1u) { used[i] = 1; nc++; }
        while (nc > k) {                                /* (thinned: a weak onset dropped, never the first) */
            uint32_t p = 1u + gr() % 15u;
            if (used[p] && (p % 4u || gch(30))) { used[p] = 0; nc--; }
        }
        k = nc;
        for (j = 0, i = 0; i < 16u; i++) if (used[i]) at[j++] = (uint8_t)i;
        for (j = 0; j < k; j++) {                       /* the contour: steps mostly, a leap turned back by a step, the
                                                         * shape (an arch, falling, rising, a wave) leaning each move */
            if (j) {
                static const uint8_t W[4] = {8, 64, 22, 6};   /* (a repeat, a step, a third, a fourth or fifth) */
                int32_t m = (int32_t)gpickw(W, 4), dir;
                if (!m && !pv) m = 1;                   /* (never two repeats running) */
                if (m == 3) m = gch(60) ? 3 : 4;
                dir = ct == 0u ? (j <= k / 2u ? 1 : -1) : ct == 1u ? (gch(75) ? -1 : 1) : ct == 2u ? (gch(75) ? 1 : -1) : (j & 1u ? 1 : -1);
                if (pv >= 3 || pv <= -3) { m = 1; dir = pv > 0 ? -1 : 1; }   /* (the leap filled) */
                if (d + dir * m > 4 || d + dir * m < -3) dir = -dir;
                pv = dir * m;
                d += pv;
            }
            mo[j] = (int8_t)d;
        }
        {   /* the anchor: the first note a chord tone of the first chord, the motif centred in the window */
            int32_t lo = 0, hi = 0;
            for (j = 0; j < k; j++) { if (mo[j] < lo) lo = mo[j]; if (mo[j] > hi) hi = mo[j]; }
            anchor = gl_near(&L, (L.dlo + L.dhi) / 2 - (lo + hi) / 2 - 1, at[0], 1);
            if (anchor + hi > L.dhi) anchor -= 7;
            if (anchor + lo < L.dlo) anchor += 7;
            base0 = (int32_t)gchord(0)->deg;
        }
        for (i = 0; i < bars; i++) {
            uint32_t bt = bars == 1u ? LB_M : bars == 2u ? (i ? (tpl == 4u ? LB_R : LB_V) : LB_M) : TPL[tpl][i % 4u];
            uint32_t b0 = i * 16u, last = i + 1u == bars, cad = last ? 2u : (bars >= 4u && i % 4u == 1u && (tpl == 0u || tpl == 4u)) ? 1u : 0u;
            int32_t sh = (int32_t)gchord(b0)->deg - base0, arch = bars >= 4u ? ARCH[tpl][i % 4u] : 0, inv = bt == LB_R;
            uint32_t kk = k, vop = bt == LB_V ? op : 99u;
            const gch_t *bc0 = gchord(b0);
            int lit = gch(25);
            sh = (sh + 10) % 7 - 3;                     /* (-3 .. 3: the nearest way) */
            if (bt == LB_REST) continue;
            if (i && (bc0->deg == (uint32_t)base0 ? bt != LB_C : (!lit || bt == LB_C))) {   /* (0.8.3) the motif moved to this bar's
                                                         * chord the voice-led way: its first note the chord tone a step
                                                         * (or a common tone) from the line's last, a step higher into the
                                                         * climax; on the first chord again: as it was */
                int32_t first = anchor + mo[0] + arch, pd = L.k ? L.n[L.k - 1u].d : first, c = (L.dlo + L.dhi) / 2;
                int32_t want = arch > 0 ? 2 : pd > c ? -1 : 1;
                sh = gl_near(&L, pd + want, b0, want) - first;
            } else if (i && bc0->deg == (uint32_t)base0) sh = 0;
            if (bt == LB_C && !sh) sh = 1;              /* (the continuation: a sequence up) */
            if (vop == 0u && kk > 2u) kk--;             /* TRUNC: its last note left out */
            if (vop == 4u) {                            /* (AUG: the notes of its first half, two at least) */
                uint32_t q;
                for (q = 0; q < kk && at[q] < 8u; q++) {}
                if (q >= 2u) kk = q; else vop = 99u;
            }
            {   /* the bar's octave: inside the window, its first note nearest the line's last (never a note folded alone) */
                int32_t o, bo = 0, bc = 0x7FFF, pd = L.k ? L.n[L.k - 1u].d : anchor;
                for (o = -7; o <= 7; o += 7) {
                    int32_t cst = 0, e;
                    for (j = 0; j < kk; j++) {
                        e = anchor + (inv ? -mo[j] + 2 * mo[0] : mo[j]) + sh + arch + o;
                        cst += (e < L.dlo || e > L.dhi) * 5;
                    }
                    e = anchor + mo[0] + sh + arch + o - pd;
                    cst += 2 * (e < 0 ? -e : e);
                    if (cst < bc) { bc = cst; bo = o; }
                }
                sh += bo;
            }
            for (j = 0; j < kk; j++) {
                uint32_t pos = b0 + (vop == 4u ? at[j] * 2u : at[j]);   /* AUG: its first half's rhythm doubled */
                int32_t mj = vop == 1u ? mo[kk - 1u - j] - mo[kk - 1u] + mo[0] : mo[j];   /* RETRO: its notes backwards
                                                         * from its first (the rhythm kept) */
                int32_t dd = anchor + (inv ? -mj + 2 * mo[0] : mj) + sh + arch;
                if (vop == 4u && at[j] >= 8u) break;
                int strong = at[j] % 8u == 0u || j == 0u, dir = j ? (mo[j] > mo[j - 1] ? 1 : -1) : 0;   /* (beats 1 and 3:
                                                         * a chord tone; 2 and 4 one too, or a passing note, below) */
                if (bt == LB_C && at[j] >= 8u) continue;
                if (pos >= len) break;
                if (vop == 2u && j + 1u == kk) dd += gch(50) ? 1 : -1;   /* END moved */
                if (j && (mo[j] - mo[j - 1] >= 3 || mo[j - 1] - mo[j] >= 3)) dir = -dir;   /* (after a leap: the chord tone
                                                         * on its near side, the leap never widened) */
                if (strong && !gl_ct(dd, pos)) dd = gl_near(&L, dd, pos, inv ? -dir : dir);
                else if (!gl_pal(dd)) dd = gl_near(&L, dd, pos, dir);
                if (cad && j + 1u == kk) dd = gl_cad(&L, dd, pos, cad == 2u);
                if (vop == 3u && j + 1u == kk && at[j] >= 2u && (!L.k || L.n[L.k - 1u].at + 1u < pos - 1u))
                    gl_add(&L, pos - 1u, dd + (gch(50) ? 1 : -1), 70, 0);   /* ORN: a neighbour into the last note */
                gl_add(&L, pos, dd, (strong ? 96u : 82u) + (arch > 0 ? 6u : 0u), cad && j + 1u == kk);
                if (gch(8) && L.k) L.n[L.k - 1u].fl = 1;
                if (cad == 2u && j + 1u == kk && L.k) {  /* (the close held, a breath after it) */
                    uint32_t end = b0 + 14u < len ? b0 + 14u : len - 1u;
                    L.n[L.k - 1u].len = (uint8_t)(end > pos ? end - pos : 0u);
                }
            }
            if (bt == LB_C) {                           /* the continuation: the motif's first half twice, the second a
                                                         * step higher (the climax) */
                for (j = 0; j < kk; j++) {
                    uint32_t pos = b0 + 8u + at[j];
                    int32_t dd = anchor + mo[j] + sh + arch + 1;
                    if (at[j] >= 8u) break;
                    if (pos >= len) break;
                    if ((at[j] % 4u == 0u || j == 0u) && !gl_ct(dd, pos)) dd = gl_near(&L, dd, pos, 1);
                    gl_add(&L, pos, dd, at[j] % 4u ? 84u : 98u, 0);
                }
            }
        }
        gl_write(t, &L, len, legato);
    }
}

/* ------------------------------------------------------------------------------------------------- texture --- */
static void gen_texture(track_t *t, uint32_t len)
{
    uint32_t i, ri = gs.g->tex;
    int32_t n[1], lo = ri == SR_DRONE || ri == SR_SUB || ri == SR_NOISE || ri == SR_TEXTURE ? 43 : 55;   /* (an organ, a
                                                         * choir, strings: above the bass) */
    gseed(GS_TEX);
    for (i = 0; i < len; i += 16u) {
        const gch_t *c = gchord(i);
        if (i && gch(45)) continue;
        n[0] = gtone(c, gch(30) ? 4 : 0, lo);
        gnote(&t->step[i], n, 1, 72, 0);
        dhold(t, i, gs.g->form == FM_ARC ? 63 : 15, len);
    }
}

/* -------------------------------------------------------------------------------------------------- sounds --- */
static int gnever(const char *s)
{
    uint32_t i;
    for (i = 0; i < NELEM(GEN_NEVER); i++) if (dname_has(s, GEN_NEVER[i])) return 1;
    return 0;
}
/* (0.9.9-gen1) a value from a packed range (gen_tables.c GR, GR4) */
static int16_t grg(uint32_t b, uint32_t u) { return (int16_t)grr((int32_t)(b >> 4) * (int32_t)u, (int32_t)(b & 15u) * (int32_t)u + (int32_t)u - 1); }

#define GVIB_RLO 88                                     /* (0.9.9-gen2) a vibrato's RATE: 5.1 .. 6.3 Hz (params.c F_LFOHZ) */
#define GVIB_RHI 92
static int16_t gnear(int32_t c, int32_t w) { return (int16_t)(c ? clamp(grr(c - w, c + w), 0, 127) : 0); }
/* (0.9.9-gen1) a role's sound as its archetype has it (gen_tables.c RECIPE): the archetype's engine and preset (a bass
 * its first two options only: the archetype's engine), the engine's knobs a little their own, then the archetype's
 * targets in the engine's parameters (the wave, the filter, its envelope, the amp envelope, glide, drive, chorus) */
static __attribute__((noinline)) void gen_sound(track_t *t, uint32_t role, uint32_t ri)
{
    const grecipe_t *r = &RECIPE[ri];
    static const uint8_t EW[2][3] = {{55, 30, 15}, {70, 30, 0}};
    uint32_t o = gpickw(EW[role == RL_BASS], 3), e, i, n, ft = r->ft & 7u, pre;
    const engine_t *en;
    if (r->o[o][0] == 255u) o = 0;
    if (r->o[o][0] == ENGI_FM6 && gs.fm6 >= 2u) o = o == 2u ? 0u : 2u;   /* (FM6 twice at most: the third option, or the
                                                         * first where the third is the FM6: never FM6 again; V10.6z10:
                                                         * the two to whichever roles a throw makes first, gen_make) */
    e = r->o[o][0]; pre = r->o[o][1];
    if (gx.eng >= 0 && (uint32_t)gx.eng != e) { e = (uint32_t)gx.eng; pre = 255; }   /* (0.8.6) EXPLORE: any engine */
    if (gx.on) {                                        /* (EXPLORE: ANIMAL and VOICE on one track each at most) */
        uint32_t b = e == ENGI_ANIMAL ? GXO_AN : e == EG_VO ? GXO_VO : 0u;
        if (gx.odd & b) { e = EG_PH; pre = 255; }
        gx.odd |= (uint8_t)b;
    }
    gs.fm6 += e == ENGI_FM6;
    set_engine_of(t, e);
    en = trk_engp(t);
    n = en->npresets;
    if (!n) return;
    if (pre != 255u && gch(35) && !(role == RL_BASS && gs.g->lead == LD_EBM) && ri != SR_COWBELL)   /* (V10.6z10) a third of
                                                         * the time another of the engine's presets of the same type (EBM's
                                                         * bass: its two oscillators always, the owner's; phonk's and baile
                                                         * funk's 808 COWBELL always) */
        for (i = 0; i < 6u; i++) {
            uint32_t k = gr() % n;
            if (PR_TAG(&en->presets[k]) == PR_TAG(&en->presets[pre % n]) && !gnever(en->presets[k].name)) { pre = k; break; }
        }
    for (i = 0; pre == 255u && i < 8u; i++) {           /* (EXPLORE: a preset of the engine, not the odd ones) */
        uint32_t k = gr() % n;
        if (!gnever(en->presets[k].name) && (role == RL_BASS || PR_TAG(&en->presets[k]) != PT_BASS))
            pre = k;                                    /* (p1, main 0.9.12: a bass's sound only on the bass, by its type) */
    }
    apply_preset_to(t, pre % n);
    if (role == RL_BASS)                                /* (VEL) the bass 75 %: evener, as the owner wants a */
        t->p[P_TVEL] = 96;                              /* bass, its ghosts ('x' 'g') still softer, ACCENTs 127 */
    for (i = 0; i < 8u; i++) {                          /* (a sixteenth of each range: still that sound, a bit its own) */
        const param_desc_t *d = track_desc(t, P_E0 + i);
        int32_t span = (d->max - d->min) / 16;
        if (!d->label || d->label[0] == '-' || d->max <= d->min || d->fmt == F_ENUM || d->names || !span) continue;   /* (the
                                                         * GENE mix pass: a list's never: FM6's ALG and SLOT moved by it took
                                                         * another patch, an empty bank slot, the bass 36 dB gone) */
        if (d->fmt == F_SEMI) {                         /* (V10.38) an interval: consonant (0, 4th, 5th, octave) */
            t->p[P_E0 + i] = (int16_t)(gch(25) ? semi_pick(gr(), d->min, d->max) : clamp(semi_snap(t->p[P_E0 + i]), d->min, d->max));
            continue;
        }
        t->p[P_E0 + i] = (int16_t)clamp(t->p[P_E0 + i] + grr(-span, span), d->min, d->max);
    }
    if (e == EG_A && !(r->wv & GNOWV)) {                /* ANALOG: the archetype's wave, one oscillator */
        t->p[P_E0] = (int16_t)(gch(33) ? (r->wv >> 3) & 7u : r->wv & 7u);
        if (r->wv & 0x40u) t->p[P_E2] = 0;
        if (ri == SR_COWBELL) { t->p[P_E1] = 79; t->p[P_E7] = 6; }   /* (V10.6p) the 808's two squares, x 1.48 */
    }
    t->p[P_ATK] = grg(r->atk, 8); t->p[P_DEC] = grg(r->dec, 8); t->p[P_SUS] = grg(r->sus, 8); t->p[P_REL] = grg(r->rel, 8);
    {   /* (gen3) the envelope by the role, whatever the archetype (gen_tables.c ENV_RULE): a lead never at full sustain, a
         * pluck or a stab short, a held pad slow in and out */
        const uint8_t *er = ENV_RULE[role == RL_LEAD ? ((r->sus & 15u) < 7u) : role == RL_ARP ? 1 : role == RL_BASS ? 4 : role != RL_PAD ? 5 :
                                     GSTAB(gs.g->pads) ? 2 : (0x103u >> gs.g->pads) & 1u ? 3 : 5];   /* (held: HOLD SWELL DRONE) */
        for (i = 0; i < 4u; i++)
            t->p[P_ATK + i] = (int16_t)clamp(t->p[P_ATK + i], (er[i] >> 4) * 8, (er[i] & 15u) * 8 + 7);
    }
    if (e == EG_FM) t->p[P_E2] = (int16_t)clamp(t->p[P_E2] - grr(4, 20), -64, 63);   /* (warmer) */
    if (ft != 7u) {
        int32_t cut = grg(r->cut, 8), res = grg(r->res, 8);
        if (role == RL_BASS && e != EG_A) cut += 20;    /* (a bass: only its fizz taken) */
        if (gs.g->human >= 40u && role != RL_BASS) cut += 10;   /* (a band: air) */
        if (gs.energy > 66u) cut += 6;
        t->p[P_FTYPE] = (int16_t)ft;
        if (e == EG_A) { t->p[P_E4] = (int16_t)clamp(cut, 0, 127); t->p[P_E5] = (int16_t)clamp(res, 0, ri == SR_ACID ? 120 : 112); }
        else { t->p[P_FCUT] = (int16_t)clamp(cut, 0, 127); t->p[P_FRES] = (int16_t)res; }
        t->p[P_ED_FLT] = grg(r->envf, 4);
        if (role != RL_PAD && role != RL_TEX && t->p[P_ED_FLT] < 6)   /* (gen3: the filter's envelope on a bass, a lead, an arp) */
            t->p[P_ED_FLT] = (int16_t)(6 + (gs.complex >> 4));
    }
    if (r->lfo) {                                       /* the LFO on the filter: a slow drift, or on the beat */
        uint32_t lr = r->lfo & 15u;
        t->p[P_LWAVE] = (int16_t)(lr >= 12u || gch(70) ? 0 : 1);
        t->p[P_LRATE] = (int16_t)(lr == 12u ? -5 : lr > 12u ? 5 - (int32_t)lr : grr((int32_t)lr * 2 + 1, (int32_t)lr * 2 + 4));   /* (synced:
                                                         * 1/4 1/8 1/8T 1/16, N_LSYNC's -5 -8 -9 -10) */
        t->p[P_LD_FLT] = (int16_t)grr((int32_t)(r->lfo >> 4) * 3, (int32_t)(r->lfo >> 4) * 4);
    }
    if (role == RL_LEAD && gch(55)) {                   /* (0.6.97) a vibrato that comes in late; (0.9.9-gen2) a singer's,
                                                         * not a siren's: ~19 cents (PITCH 1), 5.1 .. 6.3 Hz, faded in over
                                                         * 160 .. 450 ms (the attack clean), the filter's LFO off under it */
        t->p[P_LWAVE] = 0; t->p[P_LRATE] = (int16_t)grr(GVIB_RLO, GVIB_RHI); t->p[P_LFADE] = (int16_t)grr(70, 84);
        t->p[P_LD_PIT] = 1; t->p[P_LD_FLT] = 0;
    } else if (role == RL_LEAD) {                       /* (gen3) else the filter moving: slow (0.1 .. 0.5 Hz), shallow */
        t->p[P_LWAVE] = 0; t->p[P_LD_PIT] = 0;
        if (t->p[P_LD_FLT] < 3 || t->p[P_LD_FLT] > 16 || t->p[P_LRATE] >= 52) { t->p[P_LRATE] = (int16_t)grr(12, 44); t->p[P_LD_FLT] = (int16_t)grr(3, 8); }   /* (V10.6p:
                                                         * a preset's own fast or deep one too: VOICE's CHANT, the flutes) */
    } else if (role == RL_PAD && GSTAB(gs.g->pads) && gch(GG_MUT(GGR) ? 70 : 40)) {   /* (gen3) a stab's filter swept over
                                                         * 2 or 4 bars (Basic Channel, the hypnotic stabs) */
        t->p[P_LWAVE] = 0; t->p[P_LRATE] = (int16_t)(gch(50) ? -1 : -2); t->p[P_LD_FLT] = (int16_t)grr(8, 16);
    } else if (role == RL_PAD && gch(35)) {             /* (a pad breathing: a slow tremolo or a slow pan) */
        if (gch(50)) { t->p[P_LWAVE] = 0; t->p[P_LRATE] = (int16_t)grr(6, 18); t->p[P_LD_AMP] = (int16_t)grr(12, 30); }
        else { t->p[P_APDEP] = (int16_t)grr(25, 55); t->p[P_APSYNC] = 1; t->p[P_APRATE] = (int16_t)grr(1, 3); }
    } else if (role == RL_ARP && gch(40)) {             /* (an arp moving across the stereo on the beat) */
        t->p[P_APDEP] = (int16_t)grr(30, 70); t->p[P_APSYNC] = 1; t->p[P_APRATE] = (int16_t)grr(3, 5);
    }
    if (role == RL_ARP) t->p[P_LD_PIT] = 0;             /* (V10.6z10: another preset's pad drift never on an arp) */
    t->p[P_DIST] = (int16_t)(role == RL_BASS || gch(70) ? gnear((int32_t)(r->dc >> 4) * 4, 12) : 0);   /* (V10.6z10: 12 and
                                                         * 14 either way, were 8 and 10) */
    t->p[P_DTYPE] = (int16_t)(gch(70) ? 0 : 2);
    t->p[P_CHOR] = gnear((int32_t)(r->dc & 15u) * 4, 14);
    t->p[P_GLIDE] = gnear((int32_t)(r->ft >> 3) * 4, 6);
    t->p[P_VOICE] = (int16_t)r->voice;
    if (ri == SR_ACID && e == EG_A) {                   /* (0.9.9-gen2) the 303: no noise, a fixed ~55 ms slide (TIME:
                                                         * any interval as long, as the 303's RC) */
        t->p[P_E3] = 0;
        t->p[P_GLMODE] = 1; t->p[P_GLIDE] = (int16_t)grr(24, 26);
    }
    t->p[P_DETUNE] = (int16_t)(ri == SR_REESE || ri == SR_WALL || ri == SR_HOOVER ? grr(30, 60) : gnear(t->p[P_DETUNE], 6));   /* (V10.6z10:
                                                         * the preset's own a little moved) */
    if (ri == SR_GTR || ri == SR_DRONE) {               /* (a tremolo: AUTO PAN on the beat) */
        t->p[P_APDEP] = (int16_t)grr(30, 60); t->p[P_APSYNC] = 1; t->p[P_APRATE] = (int16_t)grr(4, 6); t->p[P_APWAVE] = (int16_t)(ri == SR_GTR ? 3 : 0);
    }
    /* (the GENE mix pass) the sound's own loudness evened (gen_tables.c RTRIM: an option's trim, 3 dB steps), carried
     * to gen_mix on LEVEL (112: 0 dB); an engine EXPLORE chose instead: none. (V10.7.2) V10.6z10's other preset of the
     * option's engine and type its option's trim too (measured: within ~2 dB of it; the chant's others 20 dB under);
     * the options z10 made two presets (SUB BASS's and the 808's SQR BASS, the upright's FAT BASS, the chant's TALK
     * BOX) measured anew */
    t->p[P_LEVEL] = (int16_t)(112 + (e == r->o[o][0] && !gx.on ? 6 * ((int16_t)(RTRIM[ri] << (12u - 4u * o)) >> 12) : 0));
}
/* (0.9.9-gen2) the owner: the drums "eat the rest of the sounds": every lane's DECAY shorter, twice a nibble a lane
 * (KICK SNARE CLAP HATC HATO TOM RIM BELL: -12 0 -8 -6 -12 -14 -4 -6; the snare shortened below), a constant (not a
 * table: the pinned sound's .rodata stays where it is) */
#define DX_SHORT(l) (2 * (int32_t)((0x32763406u >> (4u * (l))) & 15u))
static __attribute__((noinline)) void gen_kit(track_t *t)
{
    const gkit_t *k = &KIT[gs.g->kit];
    uint32_t l, i;
    for (l = 0; l < 8u; l++) {
        const dx_lane_t *D = &DX_KIT_DEF[l];
        dx_lane_t *L = &dx_kit[l];
        L->mode = D->mode;
        for (i = 0; i < DXP_N; i++) {
            int32_t j = grr(-6, 6) * (int32_t)gs.complex / 100;
            int32_t o = k->d[l][i] + j;
            if (i == DXP_DECAY) o -= DX_SHORT(l);       /* (0.9.9-gen2) shorter: room for the bass and the lead */
            if (i == DXP_PMOD || i == DXP_COLOR) o = (o > 0 ? o / 2 : o) - (i == DXP_PMOD ? (D->a[i] + D->b[i]) / 8 : 0);
            L->a[i] = (uint8_t)clamp(D->a[i] + o, 0, 127);   /* (.. and less of the pitch sweep and the FM / drive:
                                                         * the kits' own offsets halved, the factory sweep an eighth less) */
            L->b[i] = (uint8_t)clamp(D->b[i] + o, 0, 127);
        }
    }
    dx_glue = (uint8_t)(GG_GLUE(GGR) * 4u);                /* (0.9.9-gen1) DRUM-X GLUE where the genre bus-compresses */
    {   /* (0.6.94) the snare a snare (it came out tonal): mostly noise, shorter, less of the pitch sweep, the noise's
         * band a little brighter, and its own FEEDBK on top of the LAYER */
        dx_lane_t *S = &dx_kit[DV_SNARE];
        int32_t nz = grr(26, 40), dc = grr(10, 18), pm = grr(8, 14), cl = grr(4, 10);
        S->a[DXP_NOISE] = (uint8_t)clamp(S->a[DXP_NOISE] + nz, 0, 127); S->b[DXP_NOISE] = (uint8_t)clamp(S->b[DXP_NOISE] + nz, 0, 127);
        S->a[DXP_DECAY] = (uint8_t)clamp(S->a[DXP_DECAY] - dc, 0, 127); S->b[DXP_DECAY] = (uint8_t)clamp(S->b[DXP_DECAY] - dc, 0, 127);
        S->a[DXP_PMOD] = (uint8_t)clamp(S->a[DXP_PMOD] - pm, 0, 127); S->b[DXP_PMOD] = (uint8_t)clamp(S->b[DXP_PMOD] - pm, 0, 127);
        S->a[DXP_COLOR] = (uint8_t)clamp(S->a[DXP_COLOR] + cl, 0, 127); S->b[DXP_COLOR] = (uint8_t)clamp(S->b[DXP_COLOR] + cl, 0, 127);
        dx_lay[DV_SNARE] = (uint8_t)(DX_LAY_L(DX_LAY_DEF[DV_SNARE]) | ((uint32_t)grr(2, 4) << 4));   /* (gen2: FEEDBK 2..4, was 3..6) */
    }
    {   /* (0.6.97) the snare and the clap into the reverb (the kit's space), the rim into the delay where the genre is
         * wet (dub, dub techno ..): the kit's own SENDs (eng_drum.c dx_snd) */
        int32_t wet = (int32_t)gs.g->wet, rv = clamp((12 + grr(0, 16)) * (40 + wet) / 100, 6, 63);   /* (gen2: drier) */
        dx_snd[DV_SNARE] = (uint8_t)((3u << 6) | (uint32_t)rv);
        dx_snd[DV_CLAP] = (uint8_t)((3u << 6) | (uint32_t)clamp(rv + grr(-6, 10), 6, 63));
        dx_snd[DV_RIM] = (uint8_t)(wet >= 60 ? (2u << 6) | (uint32_t)grr(20, 45) : 0u);
        dx_snd[DV_HATO] = (uint8_t)(wet >= 70 && gch(50) ? (3u << 6) | (uint32_t)grr(8, 20) : 0u);
    }
    for (l = DV_HATC; l <= DV_HATO; l++) {               /* (0.6.94) the hats: a band-pass high up, more noise in them */
        dx_lane_t *H = &dx_kit[l];
        int32_t nz = grr(18, 28), cl = grr(10, 18);
        H->mode = (uint8_t)((H->mode & ~0x0Cu) | (DXF_BP << 2));
        H->a[DXP_NOISE] = (uint8_t)clamp(H->a[DXP_NOISE] + nz, 0, 127); H->b[DXP_NOISE] = (uint8_t)clamp(H->b[DXP_NOISE] + nz, 0, 127);
        H->a[DXP_COLOR] = (uint8_t)clamp(H->a[DXP_COLOR] + cl, 0, 127); H->b[DXP_COLOR] = (uint8_t)clamp(H->b[DXP_COLOR] + cl, 0, 127);
    }
    {   /* (V10.7.8, the owner: "a guiro", "percs") the latin kits' percussion, the factory LATIN PERC's: CUMBIA's
         * llamador, GUIRO (the filter GR, its stroke a throw's own), maracas, conga and clave; REGGAETON's conga and
         * clave; BAILE FUNK's "tcha" the llamador's high tom shot on the clap's lane (its "tum" the kit's tom, both
         * driven: GSP_HEAVY) */
        uint32_t d = gs.g->drum, lo = d == DM_CUMBIA || d - DM_CUMB2 < 2u || d - DM_BAILE < 2u ? DV_CLAP : d == DM_REGG || d == DM_REGG2 ? DV_TOM : DV_BELL;
        uint32_t hi = d - DM_BAILE < 2u ? DV_HATC : DV_BELL;
        memcpy(&dx_kit[lo], &DX_KITS[DXK_LATIN].lane[lo], (hi - lo) * sizeof dx_kit[0]);   /* (none: DV_BELL) */
        memset(&dx_lay[lo], 0, hi - lo);
        if (hi - lo == 5u) dx_kit[DV_HATC].a[DXP_PMOD] = (uint8_t)grr(96, 116);   /* (CUMBIA: the guiro's SCRAPE) */
    }
    if (gx.on)                                          /* (0.8.6) EXPLORE: the sounds far from the kit's, each its own */
        for (l = 0; l < 8u; l++) {
            dx_lane_t *L = &dx_kit[l];
            for (i = 0; i < DXP_N; i++) {
                int32_t j = gch(40) ? grr(-20, 20) : 0;
                L->a[i] = (uint8_t)clamp(L->a[i] + j, 0, 127);
                L->b[i] = (uint8_t)clamp(L->b[i] + j + grr(-6, 6), 0, 127);
            }
        }
    t->p[P_E0] = (int16_t)grr(40, 88);                  /* MORPH */
    t->p[P_E2] = (int16_t)clamp(70 + k->tone, 0, 127);
    t->p[P_E3] = (int16_t)clamp(64 + k->decy, 0, 127);
    t->p[P_E4] = (int16_t)clamp(64 + k->nois, 0, 127);
    t->p[P_E7] = (int16_t)clamp(k->drv * 2 / 3 + (gs.g->spec & GSP_HEAVY ? grr(24, 40) : 0), 0, 127);   /* (gen2: the kit's
                                                         * DRIVE two thirds; V10.6p: a heavy genre's driven harder) */
    t->p[P_FTYPE] = 0;
    t->p[P_FCUT] = (int16_t)grr(gs.g->human >= 40u ? 122 : 114, 127);   /* (V10.6w, the owner: the drums' brightness
                                                         * open: 8.4 .. 16 kHz, a band's 12.5 .. 16; was 5 .. 11) */
    t->p[P_FRES] = (int16_t)grr(0, 14);
    t->p[P_LN3] = (int16_t)(gs.g->hats == HC_MACHINE ? 112 : 118);   /* (the hats a little under) */
    t->p[P_LN4] = 118;
    t->p[P_LN6] = t->p[P_LN7] = 110;
    if (gs.g->drum == DM_JAZZ) t->p[P_LN0] = 72;        /* (V10.6w) JAZZ's kick feathered, under the ride */
    if (gclub()) {                                      /* (gen5) the kick in front: the rest of the kit under it */
        t->p[P_LN0] = 127;
        t->p[P_LN1] = 104;
        t->p[P_LN2] = 110;
        t->p[P_LN3] = 98;
        t->p[P_LN4] = 106;
        t->p[P_LN6] = t->p[P_LN7] = 100;
    }
}
/* the mix and the humanity of a role */
static __attribute__((noinline)) void gen_mix(track_t *t, uint32_t r)
{
    const ggenre_t *g = gs.g;
    int32_t wet = (int32_t)g->wet + (gs.energy < 34 ? 20 : gs.energy > 66 ? -15 : 0), hum = (int32_t)(gs.human);
    static const uint8_t HV[RL_N] = {20, 25, 45, 60, 35, 40}, HT[RL_N] = {0, 0, 30, 25, 10, 60}, HG[RL_N] = {0, 10, 10, 40, 30, 0};
    wet = clamp(wet, 0, 100);
    /* (gen3) the parts round the drums given their room back (gen2 left the drums 60 .. 93 % of the parts' energy in
     * house, techno, italo, synth pop, darkwave, electro ..): the bass, the chords, the arp and the lead 8 .. 12 dB up,
     * the drums 5 dB, then the genre's own lift (GG_LIFT: -3 .. +7.5 dB on the four, by what its kit and its kick weigh).
     * The whole louder into the LEVELER (fx.c lev_block, 2:1 toward its target, x0.35 .. x2): gen2's mix sat ~3 dB under
     * gen1's; now its gain ~0.5 .. 0.8 (inside its range), the mix as loud as gen1's (gen_test: the drums' share, the gain) */
    /* (p3, the owner: "the leads eat the drums and everything else; the drums always in front, the rest accompanies")
     * the drums up, the bass under them, the chords and the arp back, the lead ~6 dB under gen3's; the genre's lift at
     * half and on the bass and the chords only */
    t->p[P_LEVEL] = (int16_t)clamp((r == RL_DRUM ? 110 : r == RL_BASS ? 114 : r == RL_LEAD ? (g->lead == LD_RIFF ? 108 : 92) : r == RL_ARP ? 96 :
                                    r == RL_TEX ? (g->tex == SR_NOISE ? 56 : 66) : 100) +
                                   (r == RL_BASS || r == RL_PAD ? GG_LIFT(GGR) / 2 : 0) +   /* (the genre's own: GGRAM) */
                                   (gs.gi < NGENRE ? (r == RL_TEX ? 8 : 3) * ((int8_t)(GMIX[gs.gi][r >> 1] << (r & 1u ? 0 : 4)) >> 4) : 0) +
                                   (r == RL_DRUM ? 0 : t->p[P_LEVEL] - 112), 1, 127);   /* (gen_sound's trim) */
                                                        /* (the GENE mix pass) a role's level as the genre's mix wants it
                                                         * (gen_tables.c GMIX, 1.5 dB steps, the texture's 4 dB;
                                                         * measured on the parts' stems) */
    t->p[P_PAN] = (int16_t)(r == RL_ARP ? grr(-40, -16) : r == RL_TEX ? grr(16, 40) : r == RL_PAD ? grr(-8, 8) : r == RL_LEAD ? grr(4, 22) : 0);
                                                        /* (the GENE mix pass: the arp and the lead apart, either side;
                                                         * V10.6n's mixes 23 .. 33 dB of side under the middle, a mono's) */
    t->p[P_REV] = (int16_t)clamp((r == RL_PAD || r == RL_TEX ? grr(45, 95) : r == RL_LEAD ? grr(30, 70) : r == RL_ARP ? grr(20, 60) :
                                  r == RL_DRUM ? grr(4, 20) : grr(0, 6)) * (40 + wet) / 100, 0, 127);
    t->p[P_DLY] = (int16_t)clamp((r == RL_LEAD || r == RL_ARP ? grr(25, 70) : r == RL_PAD && (g->pads == CS_DUB || g->pads == CS_SKANK) ? grr(55, 95) :
                                  r == RL_PAD ? grr(0, 30) : r == RL_DRUM ? grr(0, 10) : 0) * (40 + wet) / 100, 0, 127);
    if (r == RL_LEAD && t->p[P_REV] < 25 && t->p[P_DLY] < 20) t->p[P_DLY] = (int16_t)grr(25, 55);
    t->p[P_HVEL] = (int16_t)(127 - clamp((HV[r] + HT[r] + HG[r]) * hum / 150, 0, 127));   /* (0.6.92: one amount) */
    t->p[P_HTIM] = t->p[P_HGAT] = 127;
    if (g->lead == LD_EBM) {   /* (p7b, the owner's references measured: the bass's and the pads' brightness swings
         * 30 .. 38 % over 4 bars in EBM; its bass a little driven, held) */
        if ((r == RL_BASS || r == RL_PAD) && t->p[P_LD_FLT] < 3 && !t->p[P_LD_AMP]) {
            t->p[P_LWAVE] = 0; t->p[P_LRATE] = -2; t->p[P_LD_FLT] = (int16_t)grr(10, 18);
        }
    }
    if (r == RL_BASS && (g->spec & GSP_HEAVY) && t->p[P_DIST] < 30) t->p[P_DIST] = (int16_t)grr(30, 48);   /* (V10.6p: a
                                                         * heavy genre's bass driven; the level above: a riff up front) */
    if ((g->spec & GSP_WOW) && (r == RL_PAD || r == RL_TEX)) {   /* (V10.6n) the tape's wobble: the pitch moved ~19 cents
                                                         * at 0.3 .. 1 Hz, the filter still; detuned */
        t->p[P_LWAVE] = 0; t->p[P_LRATE] = (int16_t)grr(30, 60); t->p[P_LD_PIT] = 1; t->p[P_LD_FLT] = 0; t->p[P_LFADE] = 0;
        t->p[P_DETUNE] = (int16_t)grr(10, 25);
    }
    if (r == RL_PAD || r == RL_ARP || r == RL_LEAD)     /* (p6b: the owner's references all dark, their centroid 350 ..
                                                         * 660 Hz: the melodic parts' cutoff 15 % down) */
        t->p[P_FCUT] = (int16_t)(t->p[P_FCUT] * 85 / 100);
    if (r == RL_DRUM && (g->spec & GSP_HEAVY))          /* (p4b, the owner: EBM's drums industrial, distorted, simple;
                                                         * V10.6p: phonk's and baile funk's too) */
        t->p[P_DIST] = (int16_t)grr(40, 60);
}

/* ---------------------------------------------------------------------------------------- the form: parts --- */
static void gen_role(track_t *t, uint32_t k, uint32_t len)   /* a track's part as its role plays it */
{
    uint32_t r = gs.role[k];
    if (!gs.act[r] || (r == RL_TEX && k != gs.trk_of[RL_TEX])) return;
    switch (r) {
    case RL_DRUM: gen_drums(t, len); break;
    case RL_BASS: gen_bass(t, len); break;
    case RL_PAD: gen_pads(t, len); break;
    case RL_ARP: gen_arp(t, len); break;
    case RL_LEAD: gen_lead(t, len); break;
    default: gen_texture(t, len); break;
    }
}
/* (V10.6n) FUGA's voices: the subject (the lead) answered on the arp's track a bar later, its bass imitating it two bars
 * later two octaves under; each note moved by the chord it lands on as the chord's root moves, down rather than up (a
 * chord tone stays one: the tonal answer, i's notes over V a fourth under, V's over i a fifth under), that chord's
 * alterations taken (the raised leading tone over V), a note off the mode taken as the degree under it */
static void gfugue(void)
{
    static const int8_t OF[2] = {0, -14};
    uint32_t ld = gs.trk_of[RL_LEAD], v, i, j, len;
    if (!(gs.g->spec & GSP_FUGUE) || ld >= NTRK || !gs.act[RL_LEAD]) return;
    len = dlen(&trk[ld]);
    for (v = 0; v < 2u; v++) {
        uint32_t k = gs.trk_of[v ? RL_BASS : RL_ARP], d = 16u << v;
        track_t *t;
        if (k >= NTRK || !gs.act[gs.role[k]]) continue;
        t = &trk[k];
        load_begin(t, LOAD_PAT);
        gclear_all(t);
        for (i = 0; i < len; i++) {
            const step_t *s = &trk[ld].step[i];
            step_t *o = &t->step[(i + d) % len];
            const gch_t *c = gchord(i + d);
            int32_t x = s->note[0] - gs.root % 12, oc, dg, mv = ((int32_t)c->deg - (int32_t)gchord(i)->deg + 11) % 7 - 4;
            if (s->time == ST_TIE) { dstep_tie(o); continue; }
            if (s->time != ST_NOTE || !s->n) continue;
            oc = x >= 0 ? x / 12 : -((11 - x) / 12);
            for (j = 6; j && gs.sc[j] > (uint32_t)(x - 12 * oc); j--) {}
            dg = 7 * oc + (int32_t)j + mv + OF[v];
            *o = *s;
            o->n = 1;
            o->note[0] = (uint8_t)clamp(gnd(0, dg) + galt(c, dg - c->deg), 24, 108);
        }
        t->p[P_SLEN] = (int16_t)len;
        load_end(t);
        pat_sig[k] = steps_sig(t);
    }
}
static void gen_post(void);
/* (0.8.3) a section's harmony against A's: the loop started on its predominant (IV or VI, the same chords turned: B,
 * the break; D, the club's break and the arc's apex), a one-chord loop going to its subdominant and back; the chords
 * of the section's plan as rot says */
static uint32_t gsec_rot(void)
{
    uint32_t k;
    if (gs.pn == 1u) {
        gch_t c = {0, 0, (uint8_t)(gs.sc[3] == 5u ? 3u : 1u), 0, 0};   /* (IV or iv; lydian: II) */
        if (!gtriad_ok(&c)) return 0;
        gs.pdeg[1] = gs.pdeg[0]; gs.pq[1] = gs.pq[0];
        gs.pdeg[0] = c.deg; gs.pq[0] = QQ(gs.pq[1]);
        gs.pn = 2;
        return 0;
    }
    for (k = 1; k < gs.pn; k++) if ((gs.pdeg[k] == 3u || gs.pdeg[k] == 5u) && !QF(gs.pq[k])) return k;
    return 1;
}
/* the pitched parts again over the section's plan (each from its own stream: the same motif, the same figures, on
 * these chords), the polish after; lift: the whole of it moved up that many semitones, the SCALE page's ROOT with it
 * (a real modulation, stored in the section: the song plays it in its key) */
static void gen_pitched(uint32_t rot, int32_t lift)
{
    uint32_t k, i, j, dr = gs.trk_of[RL_DRUM];
    gs.kick = 0;                                        /* (the bass reads this section's kick) */
    if (dr < NTRK && gs.act[RL_DRUM]) gkick_of(&trk[dr], dlen(&trk[dr]));
    gplan(rot);
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        if (k == dr) continue;
        load_begin(t, LOAD_PAT);
        gclear_all(t);
        motion_clear(t);
        gen_role(t, k, dlen(t));
        load_end(t);
    }
    gfugue();
    gseed(GS_FORM);
    gen_post();
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        t->p[P_ROOT] = (int16_t)((gs.root + lift + 12) % 12);
        if (k == dr || !lift) continue;
        for (i = 0; i < NSTEP; i++)
            for (j = 0; j < t->step[i].n && t->step[i].time == ST_NOTE; j++) t->step[i].note[j] = (uint8_t)clamp(t->step[i].note[j] + lift, 0, 127);
        pat_sig[k] = steps_sig(t);
    }
}
#define gen_keep (mscr.gen_keep)                        /* (seq2h: core.h mscr) (the drums of the section before; 0.8.3: the chords are made
                                                         * again, not kept) */
static void section_store(uint32_t s);                  /* (song_main.c) */
static int section_load(uint32_t s);
static uint8_t gform_kept;                              /* (gen6c) FORM asked, sections of the player's in A..D */
/* the form FORM and the genre make: LOOP, PARTS (A B A C), the club plan (FM_CLUB: gen_parts' rows), ARC */
static uint32_t gform(void)
{
    uint32_t f = gs.g->form;
    return gp.form == 0u ? FM_LOOP : gp.form == 2u ? (f == FM_ARC ? FM_ARC : FM_CLUB) : f == FM_LOOP ? FM_PARTS : f;
}
/* (V10.6m) an automation record (a new one: the last), a ramp from the one before (motion.c: MOTION_RAMP) */
static void gmot(track_t *t, uint32_t step, uint32_t id, int32_t v, int ramp)
{
    if (!motion_set_event(t, step, id, (int16_t)clamp(v, 0, 127)) && ramp) motion.event[motion.count - 1u].value ^= (int16_t)(1 << 13);
}
/* (V10.6m) the song as rows (after SIREN's plan, docs: the owner's "nothing happens in a section, fills every loop, no
 * builds"): a row's kind, GT_*: the section it plays, the drums' groups out (DXG_*), the bass in or out, the melodic
 * layers it lets in by priority (a role a nibble, the first first), three at most; GT_SEC + s: section s as it stands
 * (PARTS, ARC: its own mutes). Every row a scene (song_chain.c: the mutes, the macros). The fills a row's: the last loop
 * of a section before another, its drums from the fill section (tsrc: the pattern with the genre's fill) or, where
 * the section's drums are its own (the denser ones), ARRANGE's FILL (tvar). The drop's first loop: the crash's (B) */
/* (V10.6n) FUGA's kinds: the subject alone (the lead), the answer with it (the arp's track), the bass's entry */
enum { GT_INTROK, GT_INTRO, GT_GROOVE, GT_BREAK, GT_BUILD, GT_DROP, GT_GROOVE2, GT_OUTRO, GT_V1, GT_V2, GT_V3, GT_SEC };
#define GTF(cap, bass, nodr, grp) (uint8_t)((cap) | (bass) << 2 | (nodr) << 3 | (grp) << 4)
/* (the fix of V10.6m's bare intros: four bars of kick and hats, then drums alone, read as nothing happening) the
 * opening row the drums with their snare (the percussion out) and the chords or the texture; the next the bass and two
 * layers; the grooves two; the outro one */
static const uint16_t GT_PRIO[GT_SEC] = {0x452, 0x5342, 0x342, 0x4532, 0x542, 0x5423, 0x432, 0x2, 0x3, 0x43, 0x43};
static const uint8_t GT_FL[GT_SEC] = {GTF(1, 0, 0, DXG_PERC), GTF(2, 1, 0, 0), GTF(2, 1, 0, 0), GTF(3, 0, 1, 0),
                                      GTF(3, 0, 0, 0), GTF(3, 1, 0, 0), GTF(2, 1, 0, 0), GTF(1, 1, 0, 0), GTF(1, 0, 1, 0),
                                      GTF(2, 0, 1, 0), GTF(2, 1, 1, 0)};
#define GPL(t, bars) (uint8_t)((t) | ((bars) / 4) << 4)   /* a plan's row: its kind, its bars (SONG's; PARTS: half) */
/* the plans, one after the other (GPLAN: each one's first row and its rows): the club's; PARTS' A B A C; (V10.6n) the
 * arc's layers entering one by one (the drums, the bass, then the melodic ones by priority), its climax at ~70 %, the
 * break, the way out; FUGA's voices one by one, the episode, the coda */
static const uint8_t PLAN_ROWS[] = {
    GPL(GT_INTROK, 8), GPL(GT_INTRO, 8), GPL(GT_GROOVE, 16), GPL(GT_BREAK, 8), GPL(GT_BUILD, 4), GPL(GT_DROP, 16),
    GPL(GT_GROOVE2, 8), GPL(GT_OUTRO, 8), GPL(GT_INTROK, 8),
    GPL(GT_SEC, 16), GPL(GT_SEC + 1, 16), GPL(GT_SEC, 16), GPL(GT_SEC + 2, 16),
    GPL(GT_INTRO, 8), GPL(GT_OUTRO, 8), GPL(GT_GROOVE, 16), GPL(GT_GROOVE2, 16), GPL(GT_DROP, 16), GPL(GT_BREAK, 8),
    GPL(GT_DROP, 16), GPL(GT_OUTRO, 8),
    GPL(GT_V1, 8), GPL(GT_V2, 8), GPL(GT_V3, 8), GPL(GT_DROP, 16), GPL(GT_BREAK, 8), GPL(GT_DROP, 16), GPL(GT_OUTRO, 4),
};
static const uint8_t PLAN_OF[5] = {0, 9, 13, 21, 28};   /* the club's, PARTS', the arc's, FUGA's first rows (and the end) */
/* a row: section s, its bars, the tracks muted (m: bit k), DRUM's groups out, where the tracks play from, their variation */
static uint32_t grow(uint32_t r, uint32_t s, uint32_t bars, uint32_t m, uint32_t gm, uint32_t src, uint32_t var)
{
    scene_t *sc = &song_idx.scene[song_cur][r % CHAIN_ROWS];
    if (!bars || r >= CHAIN_ROWS || !((dice_secs >> s) & 1u)) return r;
    chain_config.row[r].slot = (uint8_t)s;
    chain_config.row[r].bars = (uint8_t)(bars > CHAIN_BARS ? CHAIN_BARS : bars);
    sc->on = (uint8_t)(1u | ((m >> 3) & 6u));
    sc->mute = (uint8_t)((m & 15u) | gm << 4);
    memcpy(sc->mac, macro_v, sizeof sc->mac);
    song_idx.tsrc[song_cur][r] = src;
    song_idx.tvar[song_cur][r] = var;
    return r + 1u;
}
static __attribute__((noinline)) void gmute(uint32_t k) { if (k < NTRK) trk[k].p[P_MUTE] = 1; }
static void gen_parts(void)
{
    const ggenre_t *g = gs.g;
    uint32_t form = gform(), plan = form == FM_CLUB, fx = plan ? 3u : 1u, half = gp.form != 2u;
    uint32_t dr = gs.trk_of[RL_DRUM], bs = gs.trk_of[RL_BASS], pd = gs.trk_of[RL_PAD], ld = gs.trk_of[RL_LEAD], ar = gs.trk_of[RL_ARP];
    uint32_t tx = gs.trk_of[RL_TEX], k, j, rows = 0, i, fam = HXF(gs.hx), lb, np;
    uint32_t base = 1u | (form == FM_ARC ? 4u : 0u), dense = form == FM_PARTS ? 4u : form == FM_ARC ? 8u : 0u;   /* (the sections
                                                         * whose drums are A's; the denser ones) */
    uint8_t pdeg[4], pq[4], pn = gs.pn, secm[4] = {0};
    uint32_t en;
    const uint8_t *pl;
    if (form == FM_LOOP) { section_store(0); dice_secs |= 1u; return; }
    gseed(GS_FORM);
    live.cur = -1;
    section_store(0);                                   /* A: the whole piece */
    if (live.cur != 0) return;
    dice_secs |= 1u;
    memcpy(pdeg, gs.pdeg, sizeof pdeg); memcpy(pq, gs.pq, sizeof pq);
    if (dr < NTRK) memcpy(gen_keep, trk[dr].step, sizeof gen_keep);
    lb = dr < NTRK ? (dlen(&trk[dr]) + 15u) / 16u : 1u;   /* (a loop of the drums: a fill row's bars) */
    for (j = 1; j < (form == FM_PARTS ? 3u : 4u); j++) {
        uint32_t rot = 0;
        int32_t lift = 0;
        k = plan ? (0x2310u >> (4u * j)) & 15u : j;     /* (the plan: B D C, the build last: its loops four bars) */
        /* back to the whole piece, then this section's cut */
        for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = 0;
        memcpy(gs.pdeg, pdeg, sizeof pdeg); memcpy(gs.pq, pq, sizeof pq); gs.pn = pn;
        if (dr < NTRK) {
            track_t *t = &trk[dr];
            uint32_t a0 = gs.amt[GP_DRUMS], c0 = gs.complex;
            load_begin(t, LOAD_PAT); memcpy(t->step, gen_keep, sizeof gen_keep); motion_clear(t);
            if (gs.act[RL_DRUM] && (k == fx || ((dense >> k) & 1u))) {   /* the fill section: A's drums and the genre's fill
                                                         * (a breath where it has none); the apex, PARTS' C denser */
                gs.drum_fill = k == fx ? (g->fills ? g->fills : FL_DROP) : 0u;
                if (k != fx) {
                    gs.amt[GP_DRUMS] = a0 + (form == FM_ARC ? 30u : 25u) > 100u ? 100u : a0 + (form == FM_ARC ? 30u : 25u);
                    if (form == FM_PARTS) gs.complex = c0 + 20u > 100u ? 100u : c0 + 20u;
                }
                gclear_all(t);
                gen_drums(t, dlen(t));
                gs.drum_fill = 0; gs.amt[GP_DRUMS] = a0; gs.complex = c0;
            }
            if (plan && k == 1u && gs.act[RL_DRUM]) gdrum_hit(t, 0, 4, 120, 1);   /* B: the drop's first loop, a crash on its one */
            if (plan && k == 2u) {                      /* C BUILD, four bars: A's drums, the snare rolling faster (quarters,
                                                         * eighths, sixteenths, then rolled), louder and louder, the kick
                                                         * out for the last half bar */
                uint32_t l0 = dlen(t);
                for (i = l0; i < NSTEP; i++) t->step[i] = t->step[i % l0];
                t->p[P_SLEN] = NSTEP;
                if (gs.act[RL_DRUM]) {
                    gdrum_clear_region(t, 0, NSTEP, 0x06u);
                    gdrum_clear_region(t, 56u, NSTEP, 0x01u);
                    for (i = 0; i < NSTEP; i++) {
                        uint32_t b = i >> 4;
                        if (i & (b ? b == 1u : 3u)) continue;
                        gdrum_hit(t, i, 1, 46 + (int32_t)i * 81 / 64, b >= 2u && !(i & 3u));
                        if (b == 3u) glane_rat(&t->step[i], 1, i >= 56u ? 4u : 2u);
                    }
                }
            }
            load_end(t);
            if (plan && k == 2u)                        /* .. the master's high-pass opening over its last two bars */
                for (i = 0; i < 8u; i++) gmot(t, 32u + 4u * i, MOT_DJF, (int32_t)((i + 1u) * (i + 1u) * 3u / 4u), 0);
        }
        /* (0.8.3) the section's harmony: the break and the apex on the predominant (not the modal loops: theirs keep
         * the loop); PARTS' last section a whole step up where the genre lifts (pop's last chorus) */
        if (fam != HF_LOOP && ((form == FM_PARTS && k == 1u) || (form != FM_PARTS && k == 3u))) rot = gsec_rot();
        if (form == FM_PARTS && k == 2u && (gs.hx & HX_LIFT)) {
            static const int8_t XL[7] = {1, 3, 5, 6, -1, -3, -5};   /* (0.8.6) EXPLORE: anywhere, not a whole step */
            lift = gx.on ? XL[gr() % 7u] : 2;
        }
        if (plan && k == 2u && pd < NTRK) trk[pd].p[P_SLEN] = NSTEP;   /* (the build's chords over its four bars) */
        gs.sec = (uint8_t)k;                            /* (V10.6z10) the section's own variations (gseed_mix, gen_lead) */
        en = gs.energy;
        if (pn == 1u) {                                 /* (V10.6z10) one chord: the section's register and density its
                                                         * own (C up a fifth and busier, B and D thinner, D down a fourth:
                                                         * the arp's rate, the lead's notes, the chords' pulse by ENERGY) */
            gs.reg = (int8_t)(k == 2u ? 7 : k == 3u ? -5 : 0);
            gs.energy = k == 2u ? (en + 100u) / 2u : en * 2u / 3u;
        }
        if (!plan || k != 1u) gen_pitched(rot, lift);   /* (B: A's but the crash) */
        gs.energy = en; gs.reg = 0;
        if (plan) {
            if (k == 2u) {                              /* C BUILD: the chords' filter opening over it, the delay thrown on
                                                         * its last step; a stutter into the drop (1/8 on the last beat, 1/16
                                                         * on its last two sixteenths); a riser on the texture's track */
                gmute(ld);
                memset(pfx_lane, 0, sizeof pfx_lane);
                pfx_ltgt = 0;
                for (i = 60; i < 64u; i++) pfx_lane_put(i, i < 62u ? 6u : 7u);
                if (pd < NTRK && gs.act[RL_PAD]) {
                    track_t *t = &trk[pd];
                    uint32_t id = t->eng_req == EG_A ? P_E4 : P_FCUT;
                    int32_t c = clamp(t->p[id], 0, 127);
                    gmot(t, 0, id, c - 34, 0); gmot(t, 63, id, c + 10, 1);
                    gmot(t, 0, P_DLY, t->p[P_DLY], 0); gmot(t, 63, P_DLY, t->p[P_DLY] + 50, 0);
                }
                if (tx < trk_live) {                    /* (the riser: a note held the four bars, its filter and level up
                                                         * as the square of the time) */
                    track_t *t = &trk[tx];
                    uint32_t id = t->eng_req == EG_A ? P_E4 : P_FCUT;
                    int32_t n[1], lv = t->p[P_LEVEL];
                    load_begin(t, LOAD_PAT);
                    gclear_all(t);
                    n[0] = gnd(55, 0);
                    gnote(&t->step[0], n, 1, 100, 0);
                    dhold(t, 0, NSTEP - 1u, NSTEP);
                    t->p[P_SLEN] = NSTEP; t->p[P_SUS] = 120; t->p[P_ATK] = 0;
                    if (id == P_FCUT) t->p[P_FTYPE] = 0;
                    load_end(t);
                    for (i = 0; i < 5u; i++) {
                        uint32_t at = i == 4u ? 63u : i * 16u, q = i * i;
                        gmot(t, at, id, 16 + (int32_t)q * 6, i != 0u);
                        gmot(t, at, P_LEVEL, lv * (20 + 5 * (int32_t)q) / 100, i != 0u);
                    }
                }
            } else if (k == 3u) {                       /* D BREAK: no drums, no bass; the chords wetter; (0.6.97) the
                                                         * synths an octave up on the break's last bar */
                memset(pfx_lane, 0, sizeof pfx_lane);
                pfx_ltgt = 1;
                for (i = 48; i < 64u; i++) pfx_lane_put(i, 2u);
                gmute(dr);
                gmute(bs);
                if (pd < NTRK) gmot(&trk[pd], 0, P_REV, trk[pd].p[P_REV] + 30, 0);
            }
        } else if (form == FM_ARC) {
            if (k == 1u) {                              /* B: thin (the chords and the bass) */
                gmute(dr);
                gmute(ld);
                gmute(ar);
            } else if (k == 2u) {                       /* C: the drums in, the lead out */
                gmute(ld);
            }                                           /* D: the apex, the drums denser (above) */
        } else if (k == 1u) {                           /* FM_PARTS: B a break (drums and bass out), C a drop (denser
                                                         * drums, above) */
            gmute(dr);
            if (bs < NTRK && (ld < NTRK || ar < NTRK || pd < NTRK)) trk[bs].p[P_MUTE] = 1;
        }
        for (i = 0; i < NTRK; i++) secm[k] |= (uint8_t)((trk[i].p[P_MUTE] != 0) << i);
        section_store(k);
        if (live.cur == (int8_t)k) dice_secs |= (uint8_t)(1u << k);
    }
    memcpy(gs.pdeg, pdeg, sizeof pdeg); memcpy(gs.pq, pq, sizeof pq); gs.pn = pn; gs.sec = 0;
    gplan(0);                                           /* (A's plan: the music live is A) */
    section_load(0);                                    /* (the whole piece back, as A holds it) */
    live.cur = 0;
    if (!(dice_secs & 2u)) return;
    {
        uint8_t ds = dice_secs;
        chain_defaults(&chain_config);
        dice_secs = ds;                                 /* (gen6c: the marks live in the rows) */
    }
    np = plan ? 0u : form == FM_ARC ? (g->spec & GSP_FUGUE ? 3u : 2u) : 1u;
    pl = &PLAN_ROWS[PLAN_OF[np]];
    np = PLAN_OF[np + 1u] - PLAN_OF[np];
    gseed(GS_FORM);
    for (j = 0; j < np; j++) {
        uint32_t t = pl[j] & 15u, nt = pl[(j + 1u) % np] & 15u, bars = (pl[j] >> 4) * 4u >> half, s, m = 0, gm = 0, fill, f;
        if (bars < 4u) bars = 4u;
        if (t >= GT_SEC) { s = t - GT_SEC; m = secm[s]; }
        else {                                          /* the kind's layers: the drums, the bass, then by priority */
            uint32_t p = GT_PRIO[t], cap, allow, r;
            f = GT_FL[t];
            cap = (f & 3u) + (!gs.act[RL_DRUM] && !(f & 3u));   /* (no drums: an intro of a layer) */
            allow = (f & 8u ? 0u : 1u << RL_DRUM) | (f & 4u ? 1u << RL_BASS : 0u);
            for (; p && cap; p >>= 4) {
                r = p & 15u;
                if (gs.trk_of[r] < NTRK && (gs.act[r] || r == RL_TEX)) { allow |= 1u << r; cap--; }
            }
            for (k = 0; k < NTRK; k++) if (!((allow >> gs.role[k]) & 1u)) m |= 1u << k;
            gm = f >> 4;
            s = t == GT_BREAK ? 3u : t == GT_BUILD ? 2u : 0u;
            if (t == GT_DROP && gch(50)) bars *= 2u;    /* (a drop of 16 or 32) */
        }
        fill = dr < NTRK && gs.act[RL_DRUM] && nt != t && !gm && !((m >> dr) & 1u) && (((base | dense) >> s) & 1u);
        if (t == GT_DROP && plan && dr < NTRK && gs.act[RL_DRUM] && bars > lb) { rows = grow(rows, 1, lb, m, gm, 0, 0); bars -= lb; }   /* (the
                                                         * crash's: the club's B) */
        f = fill ? (bars > lb ? bars - lb : 0u) : bars;
        rows = grow(rows, s, f, m, gm, 0, 0);
        if (fill)
            rows = grow(rows, s, bars - f, m, gm, ((base >> s) & 1u) && ((dice_secs >> fx) & 1u) ? (fx + 1u) << (3u * dr) : 0u,
                        ((base >> s) & 1u) && ((dice_secs >> fx) & 1u) ? 0u : (uint32_t)RV_FILL << (4u * dr));
    }
    chain_config.count = (uint8_t)rows;
    SONG_END(&chain_config) = SONG_FOREVER;
    ui.song_row = 0;
    live.mode = 1;
}

/* ------------------------------------------------------------------------------------------- (0.8.6) EXPLORE --- */
/* EXPLORE (GENRE's first choice, the dice's default): no genre's template. Its genre is thrown, field by field, from
 * everything the tables hold: any tempo from 56 to 200 BPM, any drum map (the procedural IDM's too) and kit with its
 * sounds pulled far from the kit's, any three bass archetypes against any kick rule, any chord play and voicing, any
 * arp, any lead, the recipes a role may take beyond its usual ones (a plucked bass, a bell lead), any space; its key a
 * mode of seven of the SCALE page (Phrygian, Lydian, harmonic and melodic minor, Locrian likeliest), the lead's degrees
 * a pentatonic, a hexatonic or a gapped set over it; its harmony any family with any of the colours (secondary
 * dominants, mixture, the raised dominant, chromatic approaches). Then what no genre does: a progression of one to four
 * chords on any degrees in any order (non-functional: a chord a triad the mode takes, any quality, now and then a
 * borrowed, secondary or flat-side chord: the out-of-key moves), its chords 8 to 48 steps long and the loop 8 to 64
 * (odd metres: 3/4, 5/4, 7/8 ..); each track its own loop and DIV (1/8, 1/32, 8T, 16T: polyrhythm), any engine on any
 * role (ANIMAL, VOICE singing words, FM6 twice at most as the genres, ANIMAL and VOICE twice at most), DRUM-X's octaves
 * per sound on the steps (x2 x4 x8, SHORT GHOST FLAM), hits off the grid, step components scattered (RAND OCT BEND
 * ECHO PULSE WALK CLIMB ARP TONAL JUMP REV, some every other loop), PLAYHEADS, MUTATE, the delay's pitch and spray,
 * any reverb. What it keeps: every note in the key the SCALE page shows but the colours' (the CHORD page as the chords
 * are), two melodic voices at least, the genres' levels, the voice and engine budgets. A seed is a piece (determinism
 * as every genre's); the genres' pieces are untouched by it (their streams, their tables) */
/* its fields thrown: each grr(lo, hi), 0 one time in z % (a part left out) */
typedef struct { uint8_t off, lo, hi, z; } gxf_t;
#define GXF(f, lo, hi, z) {(uint8_t)__builtin_offsetof(ggenre_t, f), (uint8_t)(lo), (uint8_t)(hi), z}
static const gxf_t GX_FIELD[] = {   /* (p3, the owner: "90 % of EXPLORE's pieces far too busy, drums without sense":
                                     * the ghosts, the fills, the swing and the amounts relaxed; the arp and the lead
                                     * often left out, the chords sometimes: an idea outside the archetypes, still music) */
    GXF(swing, 8, 34, 50), GXF(kit, 0, KIT_N - 1, 0), GXF(ghosts, 0, 1, 50),
    GXF(hats, 0, HC_MACHINE, 0), GXF(fills, 0, FL_DROP, 40), GXF(bass[0], 0, BA_N - 1, 0), GXF(bass[1], 0, BA_N - 1, 0),
    GXF(bass[2], 0, BA_N - 1, 0), GXF(bass_lo, 29, 40, 0), GXF(kick_mode, 0, KM_INTERLOCK, 0), GXF(bass_gate, 40, 115, 0),
    GXF(cbars, 1, 2, 0), GXF(pads, 0, CS_TRANCE, 0), GXF(voicing, VM_PAD, VM_SUS, 0), GXF(arp[0], 0, AP_N - 1, 0),
    GXF(arp[1], 0, AP_N - 1, 0), GXF(lead, 0, LD_NONE - 1, 0),   /* (never NONE: LEAD OFF is the knob's) */
    GXF(amt[GP_DRUMS], 35, 70, 12), GXF(amt[GP_BASS], 35, 75, 0), GXF(amt[GP_CHORDS], 25, 70, 25),
    GXF(amt[GP_ARP], 15, 60, 45), GXF(amt[GP_LEAD], 20, 55, 40), GXF(rtype, 0, 4, 0), GXF(dtime, 0, 7, 0),
    GXF(wet, 10, 95, 0), GXF(dtype, 0, 3, 0), GXF(human, 0, 45, 0),   /* (the form the arc's: gx_genre) */
};
/* (V10.6n) the drums by the tempo (the study's 6.2): slow (half time, broken), the middle (four on the floor, breaks,
 * electro), fast (drum & bass, the Amen, hard) */
static const uint8_t GX_DFAM[3][6] = {{DM_BOC, DM_CUMBIA, DM_REGG, DM_AMEN, DM_DREAM, DM_EBM},
                                      {DM_HOUSE, DM_TECHNO, DM_BREAKS, DM_ELECTRO, DM_TRIBAL, DM_BAILE},
                                      {DM_DNB, DM_AMEN, DM_HARD, DM_HBASS, DM_METAL, DM_PHONK}};
static const uint8_t *const GX_POOL[5] = {0, GX_BASS, GX_PADS, GX_ARPS, GX_LEADS};   /* (a role's recipes, GP_ order) */
static const uint8_t GX_NPOOL[5] = {0, NELEM(GX_BASS), NELEM(GX_PADS), NELEM(GX_ARPS), NELEM(GX_LEADS)};
static void gx_genre(ggenre_t *m)
{   /* (V10.6n, the study's 6.2: "rare but musical") one or two oddities a piece (a 3:4 layer over a cycle of 48, an
     * exotic mode, an odd timbre on one role, an extreme tempo, the drums edited, a floating harmony, one track's
     * components or playhead), everything else coherent: the tempo's drums, a common mode, the recipes' engines */
    uint32_t odd = 1u << (gr() % 6u), lo, k;
    if (gch(45)) odd |= 1u << (gr() % 6u);
    gx.odd = (uint8_t)odd;
    lo = (uint32_t)(odd & GXO_TEMPO ? (gch(50) ? grr(60, 76) : grr(168, 180)) : grr(76, 154));   /* (76 .. 160 BPM; was
                                                         * 92 .. 134: every piece the same pulse) */
    memset(m, 0, sizeof *m);
    m->name = "EXPLORE";
    m->bpm_lo = (uint8_t)lo;
    m->bpm_hi = (uint8_t)(lo + (uint32_t)grr(0, 6));
    for (k = 0; k < NELEM(GX_FIELD); k++) {
        const gxf_t *f = &GX_FIELD[k];
        ((uint8_t *)m)[f->off] = (uint8_t)(gch(f->z) ? 0 : grr(f->lo, f->hi));
    }
    m->drum = GX_DFAM[lo < 100u ? 0 : lo < 136u ? 1 : 2][gr() % 6u];
    m->form = FM_ARC;                                   /* (the layers entering one by one, never the club's bare intro) */
    if (m->cbars == 2u && gch(40)) m->cbars = 4;        /* (a chord a bar, two, four) */
    for (k = 0; k < 4u; k++) m->scale[k] = odd & GXO_SCALE ? GX_SCALE[gpickw(GX_SCALE_W, NELEM(GX_SCALE))] : GX_SCALE[gr() % 4u];
    if (odd & GXO_FLOAT) {                              /* (fourths, or a pedal under the chords) */
        if (gch(50)) m->voicing = VM_QUART;
        else m->bass[0] = m->bass[1] = m->bass[2] = BA_PEDAL;
    }
    m->spec = (uint8_t)((odd & GXO_GLITCH ? grr(2, 3) : 0) | (odd & GXO_POLY ? GSP_POLY : 0u));
    m->drum_pct = 100;
    m->sd_micro = (int8_t)grr(-4, 4);
    m->snd[GP_DRUMS] = m->kit;
    for (k = 1; k < GP_N; k++) m->snd[k] = GX_POOL[k][gr() % GX_NPOOL[k]];
    m->tex = GX_TEX[gr() % NELEM(GX_TEX)];
    if (m->snd[GP_LEAD] == SR_VOXLEAD && m->tex == SR_CHOIR) m->tex = SR_DRONE;   /* (one VOICE at most) */
    m->hx = (uint8_t)((gr() % (HF_JAZZ + 1u)) | (gch(50) ? HX_RAISE : 0u) | (gch(30) ? HX_SEC : 0u) | (gch(30) ? HX_BOR : 0u) |
                      (gch(35) ? HX_LIFT : 0u) | (gch(50) ? HX_APPR : 0u));
}
/* the progression: (V10.6n) one to three chords, each sharing a tone with the one before (a third or a fourth away;
 * a step now and then: the bass moving by step), each a major or minor triad of the mode, now and then coloured (the
 * raised, a secondary dominant, the parallel mode's, the flat side's), any quality but the rootless. (V10.6z10) A
 * genre's grammar too (fam: its harmony's family, HF_N EXPLORE's): two or four chords from its tonic, the root moving
 * the family's way (the modal loops by a step and a third, the sevenths' and film's by thirds and fourths, pop's by
 * fourths, jazz's up a fourth into its ii V, or down a third), each the family's quality, the colours its harmony takes (gen_pick checks
 * it as a genre's own: gprog_ok) */
#define HF_N 6u
BTAPP_FN static void gx_prog(uint32_t fam)    /* (V10.6y: gen_pick's, in the segment with it) */
{
    static const uint8_t QW[Q_N] = {20, 14, 8, 8, 8, 8, 6, 6, 6, 4, 6, 0}, FL[5] = {0, F_MAJ, F_DOM, F_PAR, F_FLAT}, FW[5] = {76, 6, 6, 6, 6};
    static const uint8_t MV[6] = {2, 5, 3, 4, 1, 6}, MW[HF_N + 1u][6] = {{10, 25, 10, 5, 25, 25}, {30, 30, 15, 15, 5, 5},
        {10, 20, 35, 25, 5, 5}, {30, 30, 15, 10, 10, 5}, {25, 25, 25, 20, 0, 5}, {0, 30, 60, 0, 0, 10}, {30, 30, 15, 15, 5, 5}};
    uint32_t n = (uint32_t)(fam < HF_N ? 2u << (gr() & 1u) : grr(1, 3)), k, tries, d;   /* (a genre's two or four:
                                                         * its chords on the bars; the root's move: thirds, fourths, a
                                                         * step: HF_LOOP JAZZY POP COLOR CINE JAZZ, EXPLORE) */
    if ((gx.odd & GXO_POLY) && n == 2u) n = 3;          /* (the cycle of 48: a chord, or three) */
    gs.pn = 0;
    for (k = 0; k < n; k++)
        for (tries = 0; tries < 12u; tries++) {
            gch_t c = {0, 0, (uint8_t)(k ? (gs.pdeg[k - 1u] + MV[gpickw(MW[fam], 6)]) % 7u : fam < HF_N ? 0u : gr() % 7u), 0, 0};
            uint32_t f = FL[gpickw(FW, fam < HF_N ? 1u : 5u)];   /* (a genre's: none, its plan's rules colour it) */
            if (f == F_DOM ? !(gs.hx & HX_SEC) : f == F_MAJ ? !(gs.hx & HX_RAISE) : f && !(gs.hx & HX_BOR)) f = 0;   /* (the
                                                         * colours its harmony took: gx_genre) */
            if (f && (!gcolour(&c, f) || !gtriad_ok(&c))) f = 0;
            if (!f) { c.alt = 0; if (!gtriad_ok(&c)) continue; }
            gs.pdeg[gs.pn] = c.deg;
            gs.pq[gs.pn] = (uint8_t)((fam < HF_N ? (0x2Au >> fam) & 1u : gpickw(QW, Q_N)) | f);   /* (a genre's: Q_TRI, or Q_7
                                                         * where its family plays sevenths: JAZZY COLOR JAZZ) */
            gs.pn++;
            break;
        }
    for (d = 0; !gs.pn && d < 7u; d++) {                /* (none came: the mode's first plain triad) */
        gch_t c = {0, 0, (uint8_t)d, 0, 0};
        if (gtriad_ok(&c)) { gs.pdeg[0] = (uint8_t)d; gs.pq[0] = Q_TRI; gs.pn = 1; }
    }
    if (!gs.pn) { gs.pdeg[0] = 0; gs.pq[0] = Q_TRI; gs.pn = 1; }
    if (gs.pn == 2u && (gx.odd & GXO_POLY)) gs.pn = 1;
}
/* (V10.6n) the odd timbre: the odd role's engine far from its recipe's; (the fix: V10.6n's EXPLORE half ANALOG) any
 * other role's too, 40 % of the time (the bass 20 %): ANIMAL, WHEEL, MATH, ADD, FM6, PHASE, a VOICE on the lead or the
 * texture (gen_sound: ANIMAL and VOICE once a piece) */
static int8_t gx_engine(uint32_t r)
{
    static const uint8_t E[7] = {ENGI_ANIMAL, EG_WH, EG_LO, ENGI_ADD, EG_FM, EG_PH, EG_VO};
    uint32_t e;
    if (((gx.odd & GXO_TIMBRE) == 0u || r != gx.orole) && !gch(r == RL_BASS ? 20 : 40)) return -1;
    e = E[gr() % (r == RL_LEAD || r == RL_TEX ? 7u : 6u)];
    return (int8_t)(e == EG_FM && gs.fm6 >= 2u ? EG_WH : e);
}
/* the sound past its recipe: a VOICE singing words (any language, a dice's phrase or scat), a distortion's type, a
 * wide glide, a vibrato, detune */
static __attribute__((noinline)) void gx_sound(track_t *t, uint32_t r)
{
    if (t->eng_req == EG_VO && r == RL_LEAD && gch(70)) { t->p[P_VLANG] = (int16_t)grr(1, LG_N - 1); t->p[P_VLYR] = (int16_t)grr(1, LYR_N - 1); }   /* (V10.6n:
                                                         * words a lead's only) */
    if (t->p[P_DIST] && gch(50)) t->p[P_DTYPE] = (int16_t)grr(0, 5);
    if ((r == RL_LEAD || r == RL_BASS) && gch(15)) t->p[P_GLIDE] = (int16_t)grr(15, 40);   /* (V10.8: was 30 %, 20..70) */
    if (r != RL_BASS && gch(12)) {                     /* (V10.8: was 25 %) (0.9.9-gen2) a vibrato on a line (shallow, ~5.5 Hz, faded in),
                                                         * a slow drift on the rest (a detune moving), never a siren */
        uint32_t line = r == RL_LEAD || r == RL_ARP;
        t->p[P_LWAVE] = (int16_t)(gr() % 2u); t->p[P_LD_PIT] = 1;
        t->p[P_LRATE] = (int16_t)(line ? grr(GVIB_RLO, GVIB_RHI) : grr(30, 50));
        if (line) { t->p[P_LFADE] = (int16_t)grr(70, 84); t->p[P_LD_FLT] = 0; }
    } else if (!t->p[P_LD_PIT] && gch(r == RL_PAD || r == RL_TEX ? 85 : r == RL_BASS ? 45 : 65)) {
        /* (V10.6l, the owner: EXPLORE's interest in movement, not in steps thrown about) the filter breathing: a slow
         * LFO (a bar to four, or free and slow; S&H on an arp now and then) faded in, the filter half closed under it */
        static const int8_t SY[5] = {-1, -2, -3, -4, -6};   /* (4 BAR .. 1/2: N_LSYNC) */
        t->p[P_LWAVE] = (int16_t)(r == RL_ARP && gch(30) ? LW_SH : gch(25) ? LW_DRIFT : gch(50) ? LW_TRI : LW_SIN);
        t->p[P_LRATE] = (int16_t)(gch(65) ? SY[gr() % 5u] : grr(18, 46));
        t->p[P_LD_FLT] = (int16_t)((gch(80) ? 1 : -1) * grr(r == RL_BASS ? 6 : 10, r == RL_BASS ? 14 : 26));
        t->p[P_LFADE] = (int16_t)grr(40, 95);
        if (t->eng_req != EG_A && t->p[P_FCUT] > 100) {
            t->p[P_FCUT] = (int16_t)grr(r == RL_BASS ? 62 : 70, 96);
            t->p[P_FRES] = (int16_t)grr(8, 40);
        }
    }
    if (r != RL_BASS && !t->p[P_M4SRC] && gch(30)) {   /* (V10.6l) a note's own place: RAND on its pan (an arp, a lead) or
                                                         * the ENV 2 opening the filter a little on each note (the rest; not
                                                         * where the preset's matrix already reads ENV 2) */
        if (r == RL_ARP || r == RL_LEAD) { t->p[P_M4SRC] = PS_RAND; t->p[P_M4DST] = PD_PAN; t->p[P_M4AMT] = (int16_t)grr(14, 34); }
        else if (t->p[P_M1SRC] != PS_ENV2 && t->p[P_M2SRC] != PS_ENV2 && t->p[P_M3SRC] != PS_ENV2) {
            t->p[P_E2ATK] = (int16_t)grr(20, 70); t->p[P_E2DEC] = (int16_t)grr(50, 100); t->p[P_E2SUS] = 0;
            t->p[P_M4SRC] = PS_ENV2; t->p[P_M4DST] = PD_CUT; t->p[P_M4AMT] = (int16_t)grr(10, 24);
        }
    }
    if (gch(15)) t->p[P_DETUNE] = (int16_t)grr(8, 30);   /* (V10.8: was 25 %, 10..50) */
    if (t->p[P_LD_PIT] && (r == RL_LEAD || r == RL_ARP   /* (V10.6l) a preset's pitch LFO on a line only as a vibrato
                                                         * (fast, faded in, the filter still): else the filter's alone;
                                                         * (an exotic engine's preset) on the rest a slow drift at most */
        ? t->p[P_LD_PIT] > 1 || t->p[P_LD_PIT] < 0 || t->p[P_LRATE] < 88 || !t->p[P_LFADE] || t->p[P_LD_FLT] > 4 || t->p[P_LD_FLT] < -4
        : t->p[P_LD_PIT] != 1 || (r != RL_BASS && t->p[P_LRATE] > 60)))
        t->p[P_LD_PIT] = 0;
}
/* the step components on the odd role's track (V10.6n: one track; a bass's never an arpeggio, a jump or a climb) */
static __attribute__((noinline)) void gx_comps(void)
{
    static const uint8_t EV[7] = {EV_1_2, EV_2_2, EV_1_3, EV_1_4, EV_3_4, EV_FIRST, EV_NOT1};
    uint32_t k = gs.trk_of[gx.orole % RL_N];
    if (k < NTRK && gs.act[gx.orole % RL_N]) {
        uint32_t r = gs.role[k], len = dlen(&trk[k]), n, j, tries;
        n = (uint32_t)grr(1, 2 + (int32_t)gs.complex / 50);   /* (V10.6l: was 75 % of the tracks, 1 to 5 each) */
        for (j = 0, tries = 0; j < n && tries < 24u; tries++) {
            step_t *s = &trk[k].step[gr() % len];
            uint32_t c = GX_COMP[gpickw(GX_COMP_W, NELEM(GX_COMP))], a;
            if (s->time != ST_NOTE || !s->n || s->comp) continue;
            if (r == RL_BASS && (cp_isarp(c) || c == CP_CLIMB)) c = CP_OCT;
            if (c == CP_ARPN && s->n < 2u) c = CP_ARPQ;
            switch (c) {
            case CP_RAND: a = (uint32_t)grr(1, 4); break;
            case CP_OCT: a = gr() % 6u; break;
            case CP_BEND: a = (uint32_t)(12 + (gch(50) ? 1 : -1) * grr(1, 12)); break;
            case CP_ECHO: a = gr() % 5u | (gch(25) ? CP_ECHO_EVEN : 0u); break;
            case CP_PULSE: a = gr() % 4u; break;
            case CP_WALK: a = CP_WARG(gr() % CP_NWRATE, gr() % 4u, gr() % 4u); break;
            case CP_CLIMB: a = CP_WARG(gr() % CP_NWRATE, gr() % 4u, gr() % 2u); break;
            case CP_ARPQ: case CP_ARPN: a = CP_ARG(gr() % 4u, gr() % 5u, gr() % 2u); break;
            case CP_TONAL: a = (uint32_t)(7 + grr(-4, 4)); break;
            default: a = gr() % 4u; break;
            }
            comp_legacy(s, 0, c, a);                    /* (seq2d: migrated in gen_post) */
            if (gch(35)) ST_EVERY_SET(s, EV[gr() % 7u]);
            j++;
        }
    }
}
/* PLAYHEADS: (V10.6n) a head on the odd role's track (or its components: gx_comps) reading
 * it at its own rate (1/4 .. x2), way and consonant interval in the scale; (V10.6n) cumbia's second voice: a head a
 * third under the lead, with it */
static void gx_heads(void)
{
    static const int8_t IV[6] = {-7, -4, -2, 2, 4, 7};
    uint32_t ti = gs.trk_of[gx.orole % RL_N];
    if (ti >= NTRK || !gs.act[gx.orole % RL_N]) return;
    fg_set[ti][0].rate = (uint8_t)(1u + gr() % 7u);
    fg_set[ti][0].dir = (uint8_t)(gr() % 3u);
    fg_set[ti][0].ivl = IV[gr() % 6u];
}

/* --------------------------------------------------------------------------------------------- the piece --- */
static void gen_post(void)
{
    uint32_t dr = gs.trk_of[RL_DRUM], bs = gs.trk_of[RL_BASS], i, k;
    if (dr < NTRK && bs < NTRK && gs.act[RL_DRUM] && gs.act[RL_BASS] && gs.g->kick_mode == KM_SUSTAIN) {
        track_t *b = &trk[bs];                          /* the bass under the kick: softer there */
        for (i = 0; i < dlen(b); i++)
            if (GKICK(i) && dstarts(b, i) && i % 16u) b->step[i].vel = (uint8_t)clamp((int32_t)b->step[i].vel - 18, 40, 127);
    }
    {   /* (0.9.9-gen1) the genre's grammar, from a stream of its own (the parts' knobs still each their own part):
         * a closed hat's roll on the beat's last sixteenth every other loop (an ECHO, EVEN: a ratchet); on the bass,
         * accents off the kick, slides into the next note, the offbeat's octave (the mode's notes: no approach moved) */
        uint32_t q0 = gq, gg = GGR, len;
        gseed(GS_GRAM);
        if (dr < NTRK && gs.act[RL_DRUM] && GG_RAT(gg))
            for (i = 3; i < dlen(&trk[dr]); i += 4u) {
                step_t *s = &trk[dr].step[i];
                if (s->time == ST_NOTE && s->hit == 1u << DV_HATC && !s->comp && gch(30)) {
                    comp_legacy(s, 0, CP_ECHO, CP_ECHO_EVEN | (gr() % 2u));
                    ST_EVERY_SET(s, EV_2_2);
                }
            }
        if (bs < NTRK && gs.act[RL_BASS] && gs.ba != BA_ACID)
            for (i = 0, len = dlen(&trk[bs]); i < len; i++) {
                step_t *s = &trk[bs].step[i];
                if (s->time != ST_NOTE) continue;
                if (!GKICK(i) && gch(GG_ACC(gg) * 14u)) s->flags |= SF_ACCENT;
                if (i + 1u < len && trk[bs].step[i + 1u].time == ST_NOTE && gch(GG_SLD(gg) * 12u)) s->flags |= SF_SLIDE;
                if (i % 4u == 2u && s->n == 1u && s->note[0] <= 52u && gin_mode(s->note[0]) && gch(GG_OCT(gg) * 12u)) s->note[0] += 12;
            }
        gq = q0;
    }
    /* (0.8.3: the lead's clashes with the bass are avoided as it is written, against the chord's root: so turning
     * BASS leaves the lead as it was) */
    for (k = 0; k < NTRK; k++) {                        /* the breath: each fourth bar's last two steps softer; (gen3) the
                                                         * lead's and the arp's accents: the bar's one up, the offbeats down */
        track_t *t = &trk[k];
        uint32_t len = dlen(t), r = gs.role[k];
        if (k == dr) continue;
        for (i = 0; i < len; i++) {
            step_t *s = &t->step[i];
            if (s->time != ST_NOTE) continue;
            if ((r == RL_LEAD || r == RL_ARP) && !gx.on)
                s->vel = (uint8_t)clamp((int32_t)s->vel + (i % 16u == 0u ? 14 : i % 4u == 0u ? 4 : i % 2u ? -10 : -4), 30, 127);
            if (len >= 64u && (i / 16u) % 4u == 3u && i % 16u >= 14u)
                s->vel = (uint8_t)clamp((int32_t)s->vel * 60 / 100, 30, 127);
        }
    }
    if (gs.complex > 30u && !gx.on) {                   /* step components (0.6.97: more of them, some every other loop):
                                                         * the lead echoed, an octave up or bent, the arp jumping its
                                                         * octave, ratcheted or pulsed, a chord arpeggiated or echoed;
                                                         * V10.6n: not EXPLORE's, its odd track's only) */
        for (k = 0; k < NTRK; k++) {
            uint32_t r = gs.role[k], tries, want = 1u + (gs.complex > 60u) + (gs.complex > 85u), got = 0;
            if (!gs.act[r] || r == RL_DRUM || r == RL_TEX || r == RL_BASS || !gch(40 + gs.complex / 3u)) continue;
            for (tries = 0; tries < 12u && got < want; tries++) {
                /* (gen4) at a phrase's end: the second half of the fourth bar of four (or the loop's last bar), where
                 * the records turn a figure; before, any step: a bar unlike its neighbours anywhere */
                uint32_t nb = dlen(&trk[k]) / 16u, bar = nb >= 4u ? (gr() % (nb / 4u)) * 4u + 3u : nb ? nb - 1u : 0u;
                uint32_t at = bar * 16u + 8u + gr() % 8u;
                if (at >= dlen(&trk[k])) continue;
                step_t *s = &trk[k].step[at];
                if (s->time != ST_NOTE || !s->n || s->comp) continue;
                if (r == RL_PAD) {
                    if (s->n < 2u) continue;
                    if (gch(60)) comp_legacy(s, 0, CP_ARPN, CP_ARG(gr() % 3u, gch(50) ? 0u : 2u, 0u));
                    else comp_legacy(s, 0, CP_ECHO, gr() % 2u);
                } else if (r == RL_LEAD) {              /* (0.8.3: never a random walk or an octave thrown into the
                                                         * line: its notes are composed) */
                    uint32_t c = gr() % 3u;
                    if (s->time != ST_NOTE || !gin_mode(s->note[0])) continue;
                    uint32_t cc = c == 0u ? CP_ECHO : c == 1u ? CP_BEND : CP_PULSE;
                    comp_legacy(s, 0, cc, cc == CP_ECHO ? gr() % 2u : cc == CP_BEND ? (gch(50) ? 2u : 10u) : 1u);
                } else {                                /* (the arp: its chord's tones; 0.8.3: no RAND, no WALK; gen3: the
                                                         * SEQ walker, its steps from this one on) */
                    uint32_t c = gr() % 4u;
                    uint32_t cc = c == 0u ? CP_OCT : c == 1u ? CP_ECHO : c == 2u ? CP_CLIMB : CP_ARPQ;
                    comp_legacy(s, 0, cc, cc == CP_OCT ? 4u : cc == CP_CLIMB ? CP_WARG(2u + gr() % 6u, gch(50) ? 2u : 0u, gr() % 2u) :
                                cc == CP_ARPQ ? CP_ARG(0u, 0u, 0u) : 0u);
                    if (c == 1u && gs.energy > 60u && gch(50)) { s->comp = 0; s->clen[0] = 0; step_set_ratchet(s, 2); }
                }
                if (s->comp && gch(55)) {               /* (every other loop; gen3: or once in four, the third of four:
                                                         * 1:4 3:4; no table: the sound's merged constants stay put) */
                    uint32_t q = gr() % 4u;
                    ST_EVERY_SET(s, q < 2u ? EV_1_2 + q : EV_1_4 + 2u * (q - 2u));
                }
                got++;
            }
        }
    }
    if (gx.on) gx_comps();                              /* (0.8.6) EXPLORE: wilder ones (V10.6n: the odd track's; every
                                                         * piece's) */
    for (k = 0; k < NTRK; k++)                          /* (seq2d) the CP_* written above as components v2 */
        comp_migrate(trk[k].step, dlen(&trk[k]));
}
/* (0.8.3) the piece's key on the SCALE page, every track (the keys, CHORD, ALL TR and the RESO reverb with it), QUANT
 * SNAP unless one is chosen (the keys play in the key); the CHORD page on the chord track as its chords are (TRIAD, 7TH,
 * ADD9, SUS2, SUS4: the plan's commonest), VOICING LEAD, a strum where a guitar plays them; off on the other tracks */
static uint32_t gchord_page(void)
{
    static const uint8_t CD[5] = {CD_TRI, CD_SEV, CD_ADD9, CD_SUS2, CD_SUS4};
    uint8_t n[5] = {0};
    uint32_t k, best = 0;
    for (k = 0; k < gs.nch; k++) {
        uint32_t q = gs.ch[k].q;
        n[q == Q_7 || q == Q_9 || q == Q_11 || q == Q_6 || q == Q_R9 ? 1 : q == Q_ADD9 ? 2 : q == Q_SUS2 ? 3 :
          q == Q_SUS4 || q == Q_QUART || q == Q_7NO5 ? 4 : 0]++;
    }
    for (k = 1; k < 5u; k++) if (n[k] > n[best]) best = k;
    return CD[best];
}
static void gen_scale_page(void)
{
    uint32_t k, pd = gs.trk_of[RL_PAD];
    int16_t qn = QN_OFF;
    for (k = 0; k < NTRK; k++) if (!drum_track(&trk[k])) { qn = trk[k].p[P_QUANT]; break; }
    if (qn == QN_OFF) qn = QN_SNAP;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        t->p[P_ROOT] = gs.root; t->p[P_SCALE] = gs.scale; t->p[P_QUANT] = qn;
        t->p[P_CHRD] = 0; t->p[P_CVOI] = CV_CLOSE; t->p[P_CSTR] = 0;
        if (k == pd && gs.act[RL_PAD]) {
            uint32_t ri = gs.g->snd[GP_CHORDS];
            t->p[P_CHRD] = (int16_t)gchord_page();
            t->p[P_CVOI] = CV_LEAD;
            t->p[P_CSTR] = (int16_t)(ri == SR_GTR ? 24 : ri == SR_EP && HXF(gs.hx) == HF_JAZZ ? 6 : 0);
        }
    }
    sl_root = gs.root; sl_scale = gs.scale; sl_quant = qn;
}
/* (0.9.9-gen1) the clean base: a piece starts from NEW PROJECT's state (project.c system_init: the song's globals but
 * the clock, tune, MIDI and routing; every track's parameters, sound, steps and automation; the punch-in lane, the global
 * components lane, DRUM-X's kit, GLUE, octaves and sends, FUGUE's playheads, MUTATE's anchor, PLAY's SONG mode) and the
 * generator's own state cleared; kept: what the player locked (the tempo under BPM LOCK), the project's name, the
 * sections of theirs (gen_parts). Nothing of the piece before stays in this one */
static void system_init_named(void);                   /* (project.c: system_init, the project's name kept) */
static void gen_clean(void)
{
    int16_t bpm = song.g[G_BPM];
    system_init_named();
    if (ui_prefs & PREF_BPM_LOCK) song.g[G_BPM] = bpm;
    memset(&gs, 0, sizeof gs);
    live.mode = 0;
}
static int8_t dice_force_style = -1, dice_force_energy = -1;   /* (a film's script, the renders: this genre, this energy) */
/* (p2, main 0.9.18) the piece's choices after its genre (gen_make's, from the GS_PICK stream: the same calls in the same
 * order): the key, the energy and the amounts, the progression, the loop, the plan, the bass's archetype. SEQ + SELECT's
 * too (dice_seq: a part where no piece of the generator plays). (V10.6y) In the Bluetooth segment (BTAPP_FN: the app
 * slot full), called only once bt_hw_seg() said it is this build's: gen_go, dice_seq and trackgen_make ask first */
BTAPP_FN static void gen_pick(void)
{
    const ggenre_t *g = gs.g;
    uint32_t k, pr, steps;
    const uint8_t *u = g->drum == DM_TRAP ? GSUB_HH : g->drum == DM_TROCK ? GSUB_TR : g->drum == DM_CUMBIA ? GSUB_CU :
                       g->drum == DM_REGG ? GSUB_RG : g->drum == DM_DEMBOW ? GSUB_DB : 0;   /* (V10.7.2: the latin
                                                         * genres' throws too) */
    if (u && !gs.mutant) {                              /* (V10.6w) HIP HOP's, TECHNO ROCK's throws: one style of theirs,
                                                         * its fields over the row (gen_tables.c GSUB; g is gmut, every
                                                         * caller's) */
        for (k = gr() % *u++; k; k--) u += 1u + 2u * *u;
        for (k = *u++; k; k--, u += 2) ((uint8_t *)&gmut)[u[0]] = u[1];
    }
    gs.root = (int16_t)(g->lead == LD_EBM ? grr(2, 9) : grr(0, 11));   /* (gen6: DAF's dark E F F# G) */
    {   /* (0.8.3) the key: always a mode of seven, so the CHORD page, the chords, the arp and the lead stand on it; a
         * genre's pentatonic or blues is its lead's palette over the mode that holds it (dorian where the genre is
         * jazzy, aeolian else; major for the major pentatonic) */
        uint32_t req = g->scale[gr() % 4u], m, d, c, cnt = 0;
        gs.hx = g->hx; gs.pal = 0x7Fu;
        if (req == SC_MPEN || req == SC_BLUE) { gs.pal = 0x5Du; req = HXF(g->hx) == HF_JAZZY ? SC_DOR : SC_MIN; }
        else if (req == SC_PEN) { gs.pal = 0x37u; req = SC_MAJ; }
        if (req < sizeof SCALE_MASK / sizeof SCALE_MASK[0]) for (c = 0; c < 12u; c++) cnt += (SCALE_MASK[req] >> c) & 1u;
        if (cnt != 7u || (req == 10u && !gx.on)) req = SC_MIN;   /* (LOC, the symmetric scales: never a key of the
                                                         * generator; 0.8.6: LOC EXPLORE's) */
        gs.scale = (int16_t)req;
        m = SCALE_MASK[req];
        for (d = 0, c = 0; c < 12u && d < 7u; c++) if ((m >> c) & 1u) gs.sc[d++] = (uint8_t)c;
        if (gx.on && !(gx.odd & GXO_SCALE)) gs.pal = GX_PAL[gr() % NELEM(GX_PAL)];   /* (V10.6n: a gapped line over a
                                                         * common mode, never over an exotic one) */
    }
    gs.energy = dice_force_energy >= 0 ? (uint32_t)dice_force_energy * 50u : gp.energy;
    gs.complex = gp.complex;
    if (gs.mutant) gs.complex = gs.complex + 20u > 100u ? 100u : gs.complex + 20u;   /* (a mutant: more going on) */
    if (gx.on) gs.complex = (uint32_t)clamp((int32_t)gs.complex + grr(10, 40), 0, 100);   /* (EXPLORE: more still) */
    gs.human = (uint32_t)gp.human * (g->human + 30u) / 100u;
    gs.swing = gp.swing == GA_AUTO ? g->swing : gp.swing;
    for (k = 0; k < GP_N; k++) {
        uint32_t a = gp.amt[k] == GA_AUTO ? g->amt[k] : gp.amt[k];
        if (gp.amt[k] == GA_AUTO && a) a = (uint32_t)clamp((int32_t)a + ((int32_t)gs.energy - 50) / 2, 10, 100);
        gs.amt[k] = a;
    }
    pr = gr() % 3u;
    gs.vm = g->voicing;
    if (gs.complex > 70u && gs.vm == VM_PLAIN && gch(40)) gs.vm = VM_RICH;
    if (gx.on) gx_prog(HF_N);                           /* (0.8.6) EXPLORE: no genre's progression */
    else {   /* (0.8.3) the progression: the genre's one in three, its next one where this mode would make a chord of it
         * diminished or borrow where the genre does not (gprog_ok), else the mode's own two-chord loop. (V10.6z10) Its own
         * a third of the time (a one-chord genre's: half), else one its family's grammar writes (gx_prog) */
        const gprog_t *p = 0;
        gprog_t pg;
        uint32_t t;
        for (t = 0; t < 3u && !p; t++) if (gprog_ok(&PROG[g->prog[(pr + t) % 3u]])) p = &PROG[g->prog[(pr + t) % 3u]];
        if (!p) {
            p = &PROG[gminor() ? (gs.sc[1] == 1u ? PR_PHRY : gs.sc[6] == 11u ? PR_I_IV : PR_I_VII) : gs.sc[3] == 6u ? PR_I_II : PR_I_IV];
            if (!gprog_ok(p)) p = &PROG[PR_ONE];
        }
        if (!glatin() && !gch(p->n == 1u ? 50u : 35u)) {   /* (V10.7.2: the latin genres' progressions their own) */
            gx_prog(HXF(g->hx));
            pg.n = gs.pn; memcpy(pg.deg, gs.pdeg, 4); memcpy(pg.q, gs.pq, 4);
            if (pg.n != 3u && gprog_ok(&pg)) p = &pg;      /* (a chord the mode would not take left out: never three,
                                                         * off the bars) */
        }
        gs.pn = p->n;
        for (k = 0; k < gs.pn; k++) {                   /* the qualities as the voicing mode wants them (a coloured chord
                                                         * keeps its own) */
            uint32_t qq = QQ(p->q[k]), f = QF(p->q[k]);
            switch (f ? (gs.vm == VM_POWER ? VM_POWER : VM_PAD) : gs.vm) {
            case VM_PLAIN: qq = Q_TRI; break;
            case VM_RICH: if (qq == Q_TRI) qq = gch(50) ? Q_7 : gch(50) ? Q_ADD9 : Q_9; break;
            case VM_SUS: qq = gch(50) ? Q_SUS4 : gch(50) ? Q_SUS2 : Q_7NO5; break;
            case VM_QUART: qq = gch(70) ? Q_QUART : Q_SUS4; break;
            case VM_POWER: qq = Q_POWER; break;
            default: break;                             /* PAD, PLANE: as written */
            }
            gs.pdeg[k] = (uint8_t)p->deg[k];
            gs.pq[k] = (uint8_t)(qq | f);
        }
    }
    gs.cst = 16u * g->cbars;
    if (gx.on || (g->spec & GSP_POLY))                  /* (V10.6n) EXPLORE: the chords over a cycle of 64 (48: three of
                                                         * them, or the 3:4 layer's), the period every track divides */
        gs.cst = (g->spec & GSP_POLY || gs.pn == 3u ? 48u : 64u) / gs.pn;
    if (gs.cst * gs.pn > NSTEP) gs.cst = NSTEP / gs.pn;
    steps = gs.cst * gs.pn;
    if (!gx.on && !(g->spec & GSP_POLY)) {
        if (steps < 16u) steps = 16u;
        if ((gs.complex > 50u && steps < 64u && gch(50)) || (g->spec & GSP_FUGUE)) steps = 64;   /* (four bars: fills,
                                                         * variations, a fourth-bar pickup; the fugue's entries) */
    }
    gs.len = steps;
    gplan(0);
    gs.ba = g->bass[gr() % 3u];                         /* (V10.6z10: its three alike, the first no more favoured) */
    if (GSP_EDIT(g->spec) && !gx.on && gch(50)) gmut.drum = IDM_ALT[GSP_EDIT(g->spec) - 1u][gr() % 3u];   /* (V10.6z10) IDM's
                                                         * breaks: half the throws another (gen_tables.c IDM_ALT) */
    if (g->lead == LD_EBM) {                            /* (p7c, the owner: EBM always the same sequence on the same
                                                         * synth; V10.6p: "heavier and faster") eight riffs in sixteenths */
        static const uint8_t EBA[8] __attribute__((section(".rodata.ebm"))) =
            {BA_NITZER3, BA_NITZER1, BA_PSY, BA_DAF, BA_NITZER2, BA_GALLOP, BA_EBMOFF, BA_EBM2B};   /* (Nitzer Ebb's, the
                                                         * techno-body's rumble, DAF's, Front 242's gallop, the offbeat
                                                         * octaves, a two-bar turn) */
        gs.ba = EBA[gr() % 8u];                         /* (its bass the row's: V10.6p, always the MS20's two oscillators) */
        gmut.drum = (uint8_t)(DM_EBM + gr() % 4u);      /* (V10.6m) its drums one of four, a throw's own */
        if (gmut.drum == DM_EBM3) { gmut.bpm_lo = 148; gmut.bpm_hi = 156; }   /* (the owner's slow EBM: 77 felt, the
                                                         * half-time map at 154, the sequence in sixteenths over it) */
        gmut.kit = gmut.snd[GP_DRUMS] = (uint8_t)(gch(75) ? KIT_INDUS : KIT_909);   /* (V10.6n) and its kit: the
                                                         * industrial one, DAF's dry 909 */
    }
    if (g->lead == LD_RIFF) {                           /* (V10.6p) PHONK, BAILE FUNK: a throw's map with its bass (the
                                                         * 808 on its kick) and its tempo; its riff, the cowbell's (baile
                                                         * funk's: Bach's flute two times in five) */
        uint32_t m = gr() & 1u, ph = g->drum == DM_PHONK, fl = !ph && gch(40);
        if (!gs.mutant) {
            gmut.drum = (uint8_t)(g->drum + m);
            gs.ba = g->bass[gch(20) ? 2u : m];
            if (ph ? m : gch(25)) { gmut.bpm_lo = ph ? 125 : 145; gmut.bpm_hi = ph ? 135 : 152; }   /* (V10.6u: Brazilian
                                                         * phonk's, the funk's claps; funk 150) */
        }
        gs.riff = (uint8_t)(fl ? BA_FL1 : BA_RF1 + gr() % 4u);
        if (fl) gmut.snd[GP_LEAD] = SR_FLUTE;
    }
}
/* (V10.6z10, the owner: "the same song again two presses later") OCT+'s seed (gen_new_seed: thrown) never one whose
 * core is one of the genre's last eight throws' (up to eight more tries): the core as gen_pick and gdna draw it (the
 * progression, the bass's archetype, the map, the drums' lanes and extra kicks), before anything is made. A seed is
 * still its song: only which seed OCT+ throws changes */
static struct { uint16_t h[8], s[4]; uint8_t at, thrown, sat; } gsr __attribute__((section(".bss.dice")));   /* (the genre in
                                                         * h; V10.7.2: s the last four seeds thrown) */
static __attribute__((noinline)) void gen_make(void)
{
    const ggenre_t *g;
    uint32_t k, dr = NTRK, nlive = trk_live < NTRK ? trk_live : NTRK, nmel, drums, next, wet;
    gen_clean();                                        /* (0.9.9-gen1) from NEW PROJECT's state, nothing of before */
    /* the choices from the seed: the genre (RANDOM), the root, the scale, the progression, the bass */
    gseed(GS_PICK);
    gx.on = (uint8_t)(dice_force_style < 0 && gp.genre == GEN_EXPLORE);
    gx.odd = 0; gx.eng = -1;
    if (gx.on) {                                        /* (0.8.6) EXPLORE: a genre of its own a seed */
        gs.gi = NGENRE;
        gx_genre(&gmut);
        g = gs.g = &gmut;
    } else {
        gs.gi = dice_force_style >= 0 ? (uint32_t)dice_force_style % NGENRE : gp.genre ? (gp.genre - 1u) % NGENRE : gr() % NGENRE;
        g = gs.g = &GENRE[gs.gi];
        gmut = *g;
        if (!gp.genre && dice_force_style < 0 && gch(15)) {   /* (0.6.95) RANDOM: a mutant, the drums of one genre, the bass
                                                             * of another, the rest from either; a name of its own: "EBM+JAZZ"
                                                             * (V10.6m: 15 %, was 45: a collage, not a genre) */
            const ggenre_t *b = &GENRE[gr() % NGENRE], *c = &GENRE[gr() % NGENRE];
            uint32_t k2, n2 = 0;
            gmut.drum = b->drum; gmut.kit = b->kit; gmut.ghosts = b->ghosts; gmut.hats = b->hats; gmut.fills = b->fills;
            gmut.sd_micro = b->sd_micro; gmut.snd[GP_DRUMS] = b->snd[GP_DRUMS];
            gmut.drum_pct = (uint8_t)(b->drum_pct > g->drum_pct ? b->drum_pct : g->drum_pct);
            for (k2 = 0; k2 < 3u; k2++) gmut.bass[k2] = c->bass[k2];
            gmut.bass_lo = c->bass_lo; gmut.kick_mode = c->kick_mode; gmut.bass_gate = c->bass_gate; gmut.snd[GP_BASS] = c->snd[GP_BASS];
            if (gch(50)) {
                for (k2 = 0; k2 < 3u; k2++) gmut.prog[k2] = b->prog[k2];
                gmut.cbars = b->cbars; gmut.pads = b->pads; gmut.voicing = b->voicing; gmut.snd[GP_CHORDS] = b->snd[GP_CHORDS];
                gmut.hx = b->hx;                            /* (0.8.3: the harmony with its progressions) */
            }
            if (gch(50)) { gmut.arp[0] = c->arp[0]; gmut.arp[1] = c->arp[1]; gmut.snd[GP_ARP] = c->snd[GP_ARP]; }
            if (gch(50)) { gmut.lead = b->lead; gmut.snd[GP_LEAD] = b->snd[GP_LEAD]; }
            if (gch(50)) { gmut.rtype = b->rtype; gmut.dtime = b->dtime; gmut.wet = b->wet; gmut.dtype = b->dtype; }
            gmut.scale[1] = b->scale[0]; gmut.scale[3] = c->scale[0];
            gmut.tex = c->tex;
            gmut.bpm_lo = (uint8_t)(((uint32_t)g->bpm_lo + b->bpm_lo) / 2u);
            gmut.bpm_hi = (uint8_t)(((uint32_t)g->bpm_hi + b->bpm_hi) / 2u);
            if (gmut.bpm_hi < gmut.bpm_lo + 4u) gmut.bpm_hi = (uint8_t)(gmut.bpm_lo + 4u);
            gmut.swing = (uint8_t)(((uint32_t)g->swing + b->swing) / 2u);
            for (k2 = 0; g->name[k2] && g->name[k2] != ' ' && k2 < 7u; k2++) gmut_name[n2++] = g->name[k2];   /* (the first words) */
            gmut_name[n2++] = '+';
            for (k2 = 0; b->name[k2] && b->name[k2] != ' ' && k2 < 6u; k2++) gmut_name[n2++] = b->name[k2];
            gmut_name[n2] = 0;
            gmut.name = gmut_name;
            gs.mutant = 1;
        }
        /* (V10.6n) a genre's drums and chords its own (the neighbours' maps and plays gone: a throw varies inside its
         * genre's lists, the study's D7) */
        g = gs.g = &gmut;
    }
    for (k = 0;; k++) {                                 /* (V10.6z10) a throw's core (gsr) */
        uint32_t q, j, h;
        uint8_t map[8];
        gen_pick();
        /* (the GENE mix pass, the owner: "more results different from each other", "drum and bass sounded better") a
         * DRUM & BASS throw on the Amen two times in five (jungle's break, its own map beside the two-step's; V10.7.2:
         * part of the throw's core) */
        if (!gx.on && g->drum == DM_DNB && gch(40)) gmut.drum = DM_AMEN;
        if (!gsr.thrown || gp.genre - 1u >= NGENRE || gs.mutant) break;
        q = gq;
        gseed(GS_DRUMS);
        h = gdna(map) ^ (gs.ba + gp.genre * 64u + gmut.drum * 4096u) * 2654435761u;
        for (j = 0; j < gs.pn; j++) h = h * 7u + gs.pdeg[j];
        gq = q;
        for (j = 0; j < 8u && gsr.h[j] != (uint16_t)h; j++) {}
        if (j == 8u || k == 8u) { gsr.h[gsr.at++ & 7u] = (uint16_t)h; break; }
        gp.seed = (uint16_t)(rng() % 10000u);           /* (its core one of the last eight: another seed) */
        gseed(GS_PICK);
        gmut = GENRE[gs.gi];
    }
    gsr.thrown = 0;
    /* (the GENE mix pass) the club's three parts (MINIMAL TECHNO, HOUSE, DEEP HOUSE, TECH HOUSE: no arp, no lead) a hook
     * on the arp's track in 45 % of the throws (the genre's arp: a pluck's blip sequence, a house piano's broken chords),
     * the rest as the genre has it */
    if (!gx.on && !g->amt[GP_ARP] && !g->amt[GP_LEAD] && g->drum < DM_CUMBIA && gp.amt[GP_ARP] == GA_AUTO && gch(45))
        gs.amt[GP_ARP] = (uint8_t)(25u + gs.energy / 4u);
    gs.fm6 = 0;
    gs.song = (uint8_t)(gform() == FM_LOOP ? 0u : gform() == FM_CLUB ? 2u : 1u);   /* (V10.6m: no fill in a song's patterns) */
    /* the orchestration: the drums (unless the genre or the knob leaves them out), then the melodic roles with an
     * amount, the most important first; the drums on the DRUM track (none: the fourth, or the last live) */
    {
        static const uint8_t PRI[GP_N] = {RL_DRUM, RL_BASS, RL_PAD, RL_ARP, RL_LEAD};
        uint8_t pri[5];
        uint32_t np = 0;
        drums = gs.amt[GP_DRUMS] && g->drum != DM_NONE && (gp.amt[GP_DRUMS] != GA_AUTO || gch(g->drum_pct));
        for (k = 0; k < RL_N; k++) gs.act[k] = 0;
        gs.act[RL_DRUM] = (uint8_t)drums;
        for (k = 1; k < GP_N; k++) if (gs.amt[k]) { pri[np++] = PRI[k]; gs.act[PRI[k]] = 1; }
        nmel = np;
        if (nmel < 2u) {                                /* (never fewer than two voices: the bass, the chords) */
            if (!gs.act[RL_BASS] && gp.amt[GP_BASS] == GA_AUTO) { gs.act[RL_BASS] = 1; gs.amt[GP_BASS] = 60; pri[np++] = RL_BASS; }
            if (np < 2u && !gs.act[RL_PAD] && gp.amt[GP_CHORDS] == GA_AUTO) {   /* (V10.8: a part set OFF stays off) */ gs.act[RL_PAD] = 1; gs.amt[GP_CHORDS] = 60; pri[np++] = RL_PAD; }
        }
        for (k = 0; k < GP_N - 1u; k++) if (!gs.act[PRI[k + 1u]]) pri[np++] = PRI[k + 1u];   /* (the silent ones after) */
        for (k = 0; k < NTRK && dr == NTRK; k++) if (drum_track(&trk[k])) dr = k;
        if (dr == NTRK) { dr = nlive > 3u ? 3u : nlive - 1u; set_engine_of(&trk[dr], ENGI_DRUM); }
        for (k = 0; k < RL_N; k++) gs.trk_of[k] = NTRK;
        next = 0;
        for (k = 0; k < NTRK; k++) {
            if (k == dr) { gs.role[k] = RL_DRUM; gs.trk_of[RL_DRUM] = (uint8_t)k; continue; }
            gs.role[k] = next < 4u ? pri[next] : RL_TEX;
            if (gs.trk_of[gs.role[k]] == NTRK) gs.trk_of[gs.role[k]] = (uint8_t)k;
            next++;
        }
        gs.act[RL_TEX] = (uint8_t)(nlive > 5u && gs.trk_of[RL_TEX] < nlive && gs.amt[GP_CHORDS] > 0u && !gclub());   /* (gen5:
                                                         * no hiss under the club's kick) */
        if (gx.on) {                                    /* (V10.6n) the odd role: the lead, the arp or the texture */
            static const uint8_t OR[3] = {RL_LEAD, RL_ARP, RL_TEX};
            uint32_t r = OR[gr() % 3u];
            gx.orole = (uint8_t)(gs.act[r] ? r : RL_PAD);
        }
    }
    gen_scale_page();
    wet = (uint32_t)clamp((int32_t)g->wet + (gs.energy < 34 ? 20 : gs.energy > 66 ? -15 : 0), 0, 100);
    /* the sounds (their own stream), then the parts in the pipeline's order: drums, bass (reads the kick), chords,
     * arp (reads the voicing), lead (reads the bass) */
    gseed(GS_SOUND);
    for (next = gr() % NTRK, k = 0; k < NTRK; k++) {   /* (V10.6z10) from a throw's own track on: FM6's two to whichever
                                                         * roles come first (was the bass's and the chords' nearly always) */
        track_t *t = &trk[(k + next) % NTRK];
        uint32_t r = gs.role[(k + next) % NTRK];
        if (r == RL_DRUM) gen_kit(t);
        else {
            if (gx.on) gx.eng = gx_engine(r);           /* (0.8.6) EXPLORE: any engine */
            gen_sound(t, r, r == RL_BASS ? g->snd[GP_BASS] : r == RL_PAD ? g->snd[GP_CHORDS] : r == RL_LEAD ? g->snd[GP_LEAD] :
                            r == RL_ARP ? g->snd[GP_ARP] : g->tex);
            gx.eng = -1;
            if (gx.on) gx_sound(t, r);
        }
    }
    gen_scale_page();
    gseed(GS_MIX);
    for (k = 0; k < NTRK; k++) gen_mix(&trk[k], gs.role[k]);
    for (k = 0; k < NTRK && gx.on; k++)                 /* (0.8.6) EXPLORE: wider (V10.6n: no track's own swing: the pulse
                                                         * one) */
        if (gs.role[k] != RL_DRUM && gs.role[k] != RL_BASS) trk[k].p[P_PAN] = (int16_t)grr(-45, 45);
    {
        static const uint8_t ORDER[5] = {RL_DRUM, RL_BASS, RL_PAD, RL_ARP, RL_LEAD};
        uint32_t o;
        for (o = 0; o < 6u; o++) {
            uint32_t r = o < 5u ? ORDER[o] : RL_TEX;
            for (k = 0; k < NTRK; k++) {
                track_t *t = &trk[k];
                uint32_t len = r == RL_DRUM && (gs.song == 2u || GSP_EDIT(g->spec)) && !gx.on && !(g->spec & GSP_POLY) ? NSTEP : gs.len;   /* (V10.6m:
                                                         * the plan's drums four bars, each its own, Grids' way; V10.6n:
                                                         * IDM's, each bar edited) */
                if (gs.role[k] != r) continue;
                if (gx.on) {                            /* (0.8.6) EXPLORE: (V10.6n) the drums and the bass the cycle's
                                                         * (the anchor); a track over one chord half of it or a quarter,
                                                         * at 1/16 or (where its period still divides the cycle) 1/8 */
                    if (r > RL_BASS && gs.pn == 1u && !gch(GX_KEEP[r])) len >>= 1u + (len > 32u && gch(40));
                    t->p[P_SDIV] = (int16_t)(r > RL_BASS && len * 2u <= gs.len && gch(20) ? 1 : 2);
                    if (r >= RL_LEAD && gch(50)) { len = GX_LEN[gr() % NELEM(GX_LEN)]; t->p[P_SDIV] = 2; }   /* (the
                                                         * lead, the arp, the texture off the bar half the time: a
                                                         * polymeter over the drums' and the bass's cycle) */
                } else {
                    if (r == RL_TEX && gs.complex > 60u && gch(40)) len = (uint32_t)grr(5, 7) * 2u + (gch(50) ? 1u : 0u);
                    if (gs.mutant && (r == RL_ARP || r == RL_LEAD) && gch(50)) {   /* (0.6.95) a mutant: a part in
                                                         * another metre */
                        static const uint8_t ODD[4] = {12, 20, 24, 28};
                        len = ODD[gr() % 4u];
                    }
                }
                load_begin(t, LOAD_PAT);           /* (0.9.9-gen1: no step, no automation: the clean base's) */
                gclear_all(t);
                gen_role(t, k, len);
                t->p[P_SLEN] = (int16_t)len;
                if (r == RL_BASS) t->p[P_SGATE] = (int16_t)clamp(g->bass_gate, 1, 127);
                else if (r == RL_PAD) t->p[P_SGATE] = (int16_t)(g->pads == CS_STAB || g->pads == CS_DUB || g->pads == CS_SKANK || g->pads == CS_BROKEN || g->pads == CS_CHUG ? grr(25, 45) :
                                                              g->pads == CS_TRANCE ? 35 : g->pads == CS_COMP || g->pads == CS_CHARL ? 60 : 120);
                else if (r == RL_LEAD) t->p[P_SGATE] = (int16_t)(g->lead == LD_MECH ? 50 : 85);
                else if (r != RL_ARP) t->p[P_SGATE] = 120;
                if (r != RL_DRUM) dlevel(t, len);
                load_end(t);
                pat_sig[k] = steps_sig(t);
            }
        }
    }
    gfugue();
    gseed(GS_FORM);
    gen_post();
    if (dr < NTRK && gs.act[RL_DRUM] && gch(45 + gs.complex / 4u)) {   /* (0.6.97) MORPH moving over the loop: the kit's
                                                         * character drifting a little and back (the drum track's
                                                         * automation) */
        track_t *t = &trk[dr];
        uint32_t len = dlen(t), j, n = len >= 32u ? 4u : 2u;
        int32_t base = t->p[P_E0], sw = grr(8, 18) * (gch(50) ? 1 : -1);   /* (gen2: was 14 .. 34) */
        for (j = 0; j < n; j++)
            motion_set_event(t, j * len / n, P_E0, (int16_t)clamp(base + (j == 0u ? 0 : j == n / 2u ? sw : sw / 2), 0, 127));
    }
    for (k = 0; k < NTRK; k++) { pat_sig[k] = steps_sig(&trk[k]); pat_last[k] = 0; }   /* (V10.28: a song's, not a pattern's) */
    /* the tempo (the genre's range, the energy in it; always moved), the swing, the space, the master */
    if (!(ui_prefs & PREF_BPM_LOCK)) {                  /* the genre's range, the energy's place in it (from the seed:
                                                         * the same piece, the same tempo; V10.6z10: a third of the range
                                                         * either way, 3 at least; JAZZ's 3: its energy is ballad .. bebop) */
        int32_t lo = g->bpm_lo, hi = g->bpm_hi, w = g->drum == DM_JAZZ || hi - lo < 9 ? 3 : (hi - lo) / 3;
        int32_t v = lo + (hi - lo) * (int32_t)gs.energy / 100 + grr(-w, w);
        song.g[G_BPM] = (int16_t)clamp(v, lo, hi);
    }
    song.g[G_SWING] = (int16_t)clamp((int32_t)gs.swing - (g->drum == DM_JAZZ ? (song.g[G_BPM] - 40) / 12 : 0), 0, 100);
                                                        /* (V10.6x) JAZZ's swing straighter as it speeds up: a ballad's
                                                         * triplets, bebop's near-even eighths */
    song.g[G_DTIME] = g->dtime;
    song.g[G_DFDBK] = (int16_t)clamp(wet >= 70u ? grr(58, 80) : grr(30, 55) + (int32_t)wet / 3, 0, 95);   /* (0.6.97: the wet
                                                         * genres' long tails: dub, dub techno, shoegaze, ambient ..) */
    song.g[G_DMIX] = (int16_t)grr(70, 110);
    song.g[G_DCOLOR] = (int16_t)(g->dtype & 1u ? grr(50, 70) : grr(30, 52));
    song.g[G_DTYPE] = (int16_t)(g->dtype & 1u);
    song.g[G_DPING] = (int16_t)((g->dtype >> 1) & 1u);
    song.g[G_DMOD] = (int16_t)(wet >= 70u ? grr(20, 50) : g->human >= 40u && gch(50) ? grr(10, 36) : 0);   /* (0.6.97: the
                                                         * tape's wobble on the long tails) */
    /* (the grain delay's PITCH and SPRAY, ALL TR: 0, the clean base's) */
    song.g[G_RTYPE] = g->rtype;
    song.g[G_RSIZE] = (int16_t)clamp(grr(40, 80) + (int32_t)wet / 2, 0, 127);
    song.g[G_RRATE] = (int16_t)grr(10, 60);
    song.g[G_RWIDE] = (int16_t)grr(40, 100);
    song.g[G_RDAMP] = (int16_t)grr(55, 100);
    song.g[G_RFILT] = (int16_t)grr(-40, -8);
    song.g[G_RMOD] = (int16_t)grr(0, 60);
    song.g[G_RPRE] = (int16_t)(g->rtype == 3 ? grr(0, 4) : g->rtype == 2 || g->rtype == 4 ? grr(-12, 0) : grr(0, 30));
    song.g[G_CRATE] = (int16_t)grr(10, 60);
    song.g[G_CDEPTH] = (int16_t)grr(20, 80);
    song.g[G_DUCK] = (int16_t)(gs.act[RL_DRUM] ? (g->spec & GSP_PUMP ? grr(60, 80) : g->kick_mode == KM_SUSTAIN ? grr(30, 50) : grr(14, 30)) : 0);   /* (V10.6n:
                                                         * hard bass pumped) */
    song.g[G_DREL] = (int16_t)(g->drum == DM_TECHNO || g->drum == DM_HARD || g->drum == DM_HBASS ? grr(15, 30) : grr(25, 45));
    song.g[G_CLIP] = (int16_t)(gs.act[RL_DRUM] ? (g->spec & GSP_HEAVY ? grr(60, 85) : grr(35, 65)) : grr(10, 30));   /* (gen3: a
                                                         * little more: the peaks rounded; V10.6p: a heavy genre's driven) */
    song.g[G_PUNCH] = (int16_t)(gs.act[RL_DRUM] ? grr(45, 85) : 0);
    if (gx.on) {                                        /* (0.8.6) EXPLORE: the grain delay's pitch and spray, a ping-pong,
                                                         * the feedback short of self-oscillation */
        song.g[G_DPIT] = (int16_t)(gch(10) ? (gch(50) ? 12 : -12) : 0);   /* (V10.8: seldom, octaves only, spray
                                                         * short: a pitched delay's cascade read as discord) */
        song.g[G_DSPRY] = (int16_t)(gch(15) ? grr(10, 40) : 0);
        song.g[G_DPING] = (int16_t)(gr() % 2u);
        song.g[G_DFDBK] = (int16_t)clamp(song.g[G_DFDBK], 0, 85);
    }
    song.g[G_CSTER] = (int16_t)grr(20, 50);             /* (the GENE mix pass) the chorus wide, the delay's right echo */
    song.g[G_WIDTH] = (int16_t)grr(30, 80);             /* later: the space round the parts in stereo */
    rng_state = gq | 1u;
    macro_dice(3);
    if (gx.on && gch(50)) gx_heads();                  /* (0.8.6) EXPLORE: PLAYHEADS, half the pieces (before the sections: each
                                                         * holds them); (V10.6) MUTATE gone */
    if ((g->spec & GSP_THIRDS) && gs.trk_of[RL_LEAD] < NTRK && gs.act[RL_LEAD]) {   /* (V10.6n) cumbia's second voice */
        fg_set_t *f = &fg_set[gs.trk_of[RL_LEAD]][0];
        f->rate = 5; f->dir = FG_FWD; f->ivl = -2;      /* (x1, with it, a third under) */
    }
    gen_parts();
    gs.song = 0;
}
/* the piece made from the settings (UNDO brings back what was there); the message */
static void gen_go(void)
{
    uint32_t k;
    char b[8];
    uint32_t play = song.playing || chain_busy() || seq_counting();
    if (!bt_hw_seg()) { ui_message("UPDATE PACKAGE"); return; }   /* (V10.6y: gen_pick in the segment) */
    if (play) {
        fm1_irq_off(); seq_stop(); transport_req = 0; fm1_irq_on();
        song_poll();
    }
    gform_kept = 0;
    undo_mark();
    if (song_draft_begin()) {
        if (play) { if (live.mode) chain_play_ui(); else transport_req = 1; }
        return;
    }
    gen_make();
    str_cpy(gp.last, gs.g->name, sizeof gp.last);
    k = str_len(gp.last);
    b[0] = ' '; b[1] = (char)('0' + gp.seed / 1000u % 10u); b[2] = (char)('0' + gp.seed / 100u % 10u);
    b[3] = (char)('0' + gp.seed / 10u % 10u); b[4] = (char)('0' + gp.seed % 10u); b[5] = 0;
    str_cpy(gp.last + k, b, sizeof gp.last - k);
    ui_say("GENE: ", gp.last);
    str_cpy(ui.msg2, live.mode ? "PLAY: SONG / SAVE: ALL PARTS" : "UNDO: GLO + G4", sizeof ui.msg2);
    if (play) { if (live.mode) chain_play_ui(); else transport_req = 1; }
    ui.force = 1;
}
static uint32_t gen_sig(void)                           /* the settings that make the piece */
{
    uint32_t h = 2166136261u, k;
    h = (h ^ gp.genre) * 16777619u; h = (h ^ gp.seed) * 16777619u; h = (h ^ gp.energy) * 16777619u; h = (h ^ gp.form) * 16777619u;
    for (k = 0; k < GP_N; k++) h = (h ^ gp.amt[k]) * 16777619u;
    h = (h ^ gp.complex) * 16777619u; h = (h ^ gp.swing) * 16777619u; h = (h ^ gp.human) * 16777619u;
    return h ^ (trk_live << 28);
}
static __attribute__((noinline)) void gen_new_seed(void)
{
    uint32_t j;
    rng_state ^= fm1_ticks() * 2654435761u;
    if (!rng_state) rng_state = 0x1234567u;
    do {                                                /* (V10.7.2) never one of the last four seeds thrown (RANDOM's
                                                         * dice too: a seed is a genre's and a song's) */
        gp.seed = (uint16_t)(rng() % 10000u);
        for (j = 0; j < 4u && gsr.s[j] != gp.seed; j++) {}
    } while (j < 4u);
    gsr.s[gsr.sat++ & 3u] = gp.seed;
    gsr.thrown = 1;                                     /* (V10.6z10: gen_make keeps it off the genre's last eight) */
}
/* OCT+ on the GEN screen: the piece; the same settings again, or the first press since power-on (V10.6l: it was always
 * EXPLORE 0001, the same odd piece after every power-on), a new seed first */
static void gen_press(void)
{
    if (!gp.sig || gp.sig == gen_sig()) gen_new_seed();
    gen_go();
    gsr.thrown = 0;                                     /* (a throw gen_go did not make: never a later piece's) */
    gp.sig = gen_sig();
}
/* (the old name: tests, the films, the renders) a new throw: any genre (RANDOM, as it always was: the screen's dice is
 * EXPLORE), a new seed (the screen's settings otherwise) */
static void dice_all(void)
{
    uint8_t g = gp.genre;
    gp.genre = 0;
    gen_new_seed();
    gen_go();
    gp.sig = gen_sig();
    gp.genre = g;
}
static uint32_t dice_print(void)                        /* a fingerprint of the piece (the tests) */
{
    uint32_t k, h = 2166136261u ^ (uint32_t)song.g[G_BPM];
    for (k = 0; k < NTRK; k++) h = (h ^ steps_sig(&trk[k]) ^ trk[k].eng_req) * 16777619u;
    return h;
}

/* (0.6.67; p2 as main 0.9.18: the generator's) SEQ + SELECT: the selected track's sequence diced as GEN writes its part,
 * a new stream each press (the random numbers, the ticks in them). Where a piece of the generator plays in the track's
 * key, on its harmony (its plan, its genre, the track's role in it): the part fits the others. Else a piece's choices
 * made up for it (the last genre, or any; the track's own key where it is a mode of seven): the drums on a DRUM track,
 * else the role its steps sound like (low: the bass, chords: the chords, else a lead or an arp). Until p2 song_dice.c's
 * own parts: a random genre's progression against the song's (its lead's strong steps on the chords' tones 64 % of the
 * time, GEN's 85 %). Never what it played before the press (90 % of its steps the same, transposed or not) nor one of
 * the track's last four: another stream, and from the second try the role's figure from the whole generator (a lead's
 * way, an arp's order, the chords' play, the drums' family; the bass's archetype moved, gba_vary). The sound, the mix
 * and the automation stay. Its message */
static uint32_t dsq_last[NTRK][4] __attribute__((section(".bss.dice")));   /* (each track's last diced shapes; their own
                                                         * section: out of the merged statics, whose places are recorded) */
/* (p2b) the last 8 presses' shapes (a byte a step, over the root: transposed the same), whichever track: a new one is
 * never 90 % of one of them on its track (played live, the dice give something new each press: an arp on the piece's
 * own chords came out near one of its last 8 half of the time) */
static struct { uint8_t v[8][NSTEP], trk[8], len[8], at; } dsq_h __attribute__((section(".bss.dice")));
static __attribute__((noinline)) uint32_t dsq_near(uint32_t k, const int16_t *now, uint32_t len)
{
    uint32_t j, i, same;
    for (j = 0; j < 8u; j++) {
        if (dsq_h.len[j] != len || dsq_h.trk[j] != k) continue;
        for (i = 0, same = 0; i < len; i++) same += dsq_h.v[j][i] == (uint8_t)now[i];
        if (same * 10u >= len * 9u) return 1;
    }
    return 0;
}
static __attribute__((noinline)) void dsq_keep(uint32_t k, const int16_t *now, uint32_t len)
{
    uint32_t j = dsq_h.at++ & 7u, i;
    for (i = 0; i < len; i++) dsq_h.v[j][i] = (uint8_t)now[i];
    dsq_h.trk[j] = (uint8_t)k; dsq_h.len[j] = (uint8_t)len;
}
static uint32_t dsq_shape(const track_t *t, int16_t *v, uint32_t len)   /* the steps as notes over the root (or hits) */
{
    uint32_t i, h = 2166136261u;
    for (i = 0; i < len; i++) {
        const step_t *s = &t->step[i];
        v[i] = (int16_t)(drum_track(t) ? (s->time == ST_NOTE ? s->hit : 0) :
                         s->time == ST_NOTE && s->n ? 200 + s->note[0] - t->p[P_ROOT] : s->time == ST_TIE ? 1 : 0);
        h = (h ^ (uint16_t)v[i]) * 16777619u;
    }
    return h ^ len;
}
static const char *dice_seq(track_t *t)
{
    static const char *const RN[RL_N] = {"SEQ DICED: DRUMS", "SEQ DICED: BASS", "SEQ DICED: PAD", "SEQ DICED: LEAD",
                                         "SEQ DICED: ARP", "SEQ DICED: TEXTURE"};
    uint32_t k = trk_index(t) % NTRK, r, i, len, lo = 0, cnt = 0, chord = 0, here, plen = dlen(t), h = 0;
    uint16_t s0 = gp.seed;
    int16_t was[NSTEP], now[NSTEP];
    uint8_t lead0, arp0, arp1, pads0, drum0;
    if (!bt_hw_seg()) return "UPDATE PACKAGE";         /* (V10.6y: gen_pick in the segment) */
    rng_state ^= fm1_ticks() * 2654435761u;
    if (!rng_state) rng_state = 0x1234567u;
    for (i = 0; i < NSTEP; i++)                         /* (what it plays now: its register, its chords) */
        if (t->step[i].time == ST_NOTE && t->step[i].n) { lo += t->step[i].note[0]; cnt++; chord += t->step[i].n > 1u; }
    here = gs.g && gs.root == t->p[P_ROOT] && gs.scale == t->p[P_SCALE];   /* (a piece of the generator plays here) */
    r = drum_track(t) ? RL_DRUM : here && gs.role[k] != RL_DRUM && gs.role[k] != RL_TEX ? gs.role[k] :
        cnt && lo / cnt < 48u ? RL_BASS : chord * 3u > cnt && cnt ? RL_PAD : rng() % 2u ? RL_LEAD : RL_ARP;
    gp.seed = (uint16_t)(rng() % 10000u);
    if (!here) {                                        /* (no piece here: its choices made up) */
        uint32_t m = SCALE_MASK[(uint32_t)t->p[P_SCALE] % (sizeof SCALE_MASK / sizeof SCALE_MASK[0])], c, d;
        gs.gi = gs.g && gs.gi < NGENRE ? gs.gi : rng() % NGENRE;
        gmut = GENRE[gs.gi]; gs.g = &gmut; gs.mutant = 0; gx.on = 0;
        gseed(GS_PICK);
        gen_pick();
        for (d = 0, c = 0; c < 12u; c++) d += (m >> c) & 1u;
        gs.root = t->p[P_ROOT];
        if (d == 7u && t->p[P_SCALE] != 10) {           /* (its own mode, LOC aside) */
            gs.scale = t->p[P_SCALE];
            for (d = 0, c = 0; c < 12u && d < 7u; c++) if ((m >> c) & 1u) gs.sc[d++] = (uint8_t)c;
            gplan(0);
        }
        gs.kick = 0;
        for (c = 0; c < NTRK; c++)                      /* (the kick a bass answers: the DRUM track's) */
            if (drum_track(&trk[c]) && &trk[c] != t) gkick_of(&trk[c], dlen(&trk[c]));
    }
    gs.ba = gs.g->bass[rng() % 3u];
    if (gmut.lead == LD_NONE) gmut.lead = LD_HOOK;      /* (asked for: a lead) */
    if (r == RL_DRUM && (gmut.drum >= DM_NONE || gmut.drum == DM_SPARSE))   /* (V10.7) a genre without drums
                                                         * (NEO CLASSIC) or AMBIENT's three hits: the kit's dice seemed
                                                         * to do nothing; a map that plays (before drum0:
                                                         * the retries' other maps are its neighbours) */
        gmut.drum = (uint8_t)(rng() % DM_SPARSE);
    lead0 = gmut.lead; arp0 = gmut.arp[0]; arp1 = gmut.arp[1]; pads0 = gmut.pads; drum0 = gmut.drum;
    for (i = 0; i < GP_N; i++) if (!gs.amt[i]) gs.amt[i] = 60;
    gs.role[k] = (uint8_t)r; gs.act[r] = 1; gs.trk_of[r] = (uint8_t)k;
    len = (uint32_t)t->p[P_SLEN];
    if (len < 8u || len > NSTEP) len = gs.len < 16u ? 16u : gs.len;
    dsq_shape(t, was, plen);
    load_begin(t, LOAD_PAT);
    latch_drop_req |= (uint8_t)(1u << k);
    for (cnt = 0; cnt < 12u; cnt++) {                   /* (eleven more tries at most) */
        uint32_t j, same = 0, hits = 0;
        for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
        gen_role(t, k, len);
        h = dsq_shape(t, now, len);
        for (i = 0; i < len && plen == len; i++) same += now[i] == was[i];
        for (i = 0; i < len; i++) hits += t->step[i].time == ST_NOTE && t->step[i].hit;
        for (j = 0; j < 4u && dsq_last[k][j] != h; j++) {}
        if (r == RL_DRUM && hits < len / 8u + 2u && cnt < 11u) {   /* (V10.7) a kit's dice with a beat in it: AMBIENT's
                                                         * energy left three hits; another map */
            gmut.drum = (uint8_t)(rng() % DM_SPARSE);
            continue;
        }
        if (j == 4u && same * 10u < len * 9u && (cnt == 11u || !dsq_near(k, now, len))) break;
        gp.seed = (uint16_t)(rng() % 10000u);
        gs.ba = gs.g->bass[rng() % 3u];
        if (cnt) {                                      /* (the figure from the whole generator) */
            gmut.lead = (uint8_t)(rng() % LD_NONE);
            gmut.arp[0] = gmut.arp[1] = (uint8_t)(rng() % AP_N);
            gmut.pads = (uint8_t)(rng() % (CS_TRANCE + 1u));
            if ((drum0 | 1u) < DM_SPARSE) gmut.drum = (uint8_t)(drum0 ^ (cnt & 1u));   /* (V10.6n: the map, its neighbour) */
        }
    }
    gmut.lead = lead0; gmut.arp[0] = arp0; gmut.arp[1] = arp1; gmut.pads = pads0; gmut.drum = drum0;
    for (i = 3; i; i--) dsq_last[k][i] = dsq_last[k][i - 1u];   /* (no memmove on the target) */
    dsq_last[k][0] = h;
    dsq_keep(k, now, len);
    t->p[P_SLEN] = (int16_t)len;
    if (r != RL_DRUM) dlevel(t, len);
    load_end(t);
    gp.seed = s0;
    return RN[r];
}

/* ------------------------------------------------------------------------------------------- the GEN screen --- */
static const char *const GEN_LBL[3][4] BTAPP_RO = {{"GENRE", "SEED", "ENERGY", "FORM"}, {"DRUMS", "BASS", "CHORDS", "ARP"},
                                          {"LEAD", "COMPLEX", "SWING", "HUMAN"}};
static const char *const GEN_FORM[3] BTAPP_RO = {"LOOP", "PARTS", "SONG"};
static uint8_t gen_fresh;                               /* (0.6.96) the screen's first frame: the body cleared once */
static void gen_open(void)
{
    gp.hot = 0xFF;
    gen_fresh = 1;
    /* (V10.8) the GENRE kept from the visit before (was: EXPLORE each time GEN opened; testers: "it resets after
     * switching pages"); power-on and NEW PROJECT: EXPLORE */
}
static void gen_amt_turn(uint8_t *v, int32_t s)         /* OFF, 10 .. 100, AUTO (above 100) */
{
    int32_t x = *v == GA_AUTO ? 110 : (int32_t)*v;
    x = clamp(x + s * 10, 0, 110);
    *v = (uint8_t)(x > 100 ? GA_AUTO : x);
}
static uint8_t gen_genre_next(uint32_t g, int32_t s)   /* (V10.6n) EXPLORE, RANDOM, the genres in their order */
{
    uint32_t pos = g == GEN_EXPLORE || g > NGENRE ? 0u : g + 1u;
    pos = (pos + (s > 0 ? 1u : GEN_NPOS - 1u)) % GEN_NPOS;
    return (uint8_t)(pos ? pos - 1u : GEN_EXPLORE);
}
static void gen_knob(uint32_t k, int32_t s)
{
    uint32_t p = gp.page;
    gp.hot = (uint8_t)k;
    if (p == 0u) {
        if (k == 0u) {                                  /* (0.8.6) EXPLORE first, then RANDOM, then the genres */
            gp.genre=gen_genre_next(gp.genre,s);
        }
        else if (k == 1u) gp.seed = (uint16_t)((gp.seed + 10000u + (uint32_t)clamp(accel(EN_K2, s, 9999), -9999, 9999)) % 10000u);
        else if (k == 2u) gp.energy = (uint8_t)clamp((int32_t)gp.energy + s * 5, 0, 100);
        else gp.form = (uint8_t)clamp((int32_t)gp.form + (s > 0 ? 1 : -1), 0, 2);
    } else if (p == 1u) {
        gen_amt_turn(&gp.amt[k], s);
    } else {
        if (k == 0u) gen_amt_turn(&gp.amt[GP_LEAD], s);
        else if (k == 1u) gp.complex = (uint8_t)clamp((int32_t)gp.complex + s * 5, 0, 100);
        else if (k == 2u) { int32_t x = gp.swing == GA_AUTO ? 105 : (int32_t)gp.swing; x = clamp(x + s * 5, 0, 105); gp.swing = (uint8_t)(x > 100 ? GA_AUTO : x); }
        else gp.human = (uint8_t)clamp((int32_t)gp.human + s * 5, 0, 100);
    }
    ui.force = 1;
}
static uint32_t gen_page(void) { return gp.page % 3u; }
static void gen_page_put(uint32_t p)                     /* (0.9.2) GEN 1..3: HOME's pages (ui_lens.c nav_go) */
{
    if (gp.page != p % 3u)
        gp.hot = 0xFF;
    gp.page = (uint8_t)(p % 3u);
    ui.force = 1;
}
BTAPP_FN static void gen_value(uint32_t p, uint32_t k, char *b)
{
    uint32_t v;
    b[0] = 0;
    if (p == 0u) {
        if (k == 0u) { str_cpy(b, gp.genre == GEN_EXPLORE ? "EXPLORE" : gp.genre ? GENRE[(gp.genre - 1u) % NGENRE].name : "RANDOM", 16); return; }
        if (k == 1u) { b[0] = (char)('0' + gp.seed / 1000u % 10u); b[1] = (char)('0' + gp.seed / 100u % 10u); b[2] = (char)('0' + gp.seed / 10u % 10u); b[3] = (char)('0' + gp.seed % 10u); b[4] = 0; return; }
        if (k == 3u) { str_cpy(b, GEN_FORM[gp.form % 3u], 16); return; }
        v = gp.energy;
    } else if (p == 1u) v = gp.amt[k];
    else if (k == 0u) v = gp.amt[GP_LEAD];
    else if (k == 1u) v = gp.complex;
    else if (k == 2u) v = gp.swing;
    else v = gp.human;
    if (v == GA_AUTO) { str_cpy(b, "AUTO", 16); return; }
    if (!v && (p == 1u || (p == 2u && k == 0u))) { str_cpy(b, "OFF", 16); return; }
    b[0] = (char)('0' + v / 100u); b[1] = (char)('0' + v / 10u % 10u); b[2] = (char)('0' + v % 10u); b[3] = '%'; b[4] = 0;
    if (b[0] == '0') { b[0] = b[1]; b[1] = b[2]; b[2] = b[3]; b[3] = 0; if (b[0] == '0' && v < 10u) { b[0] = b[1]; b[1] = b[2]; b[2] = 0; } }
}
/* (0.8.5) GEN in HOME's lens (ui_lens.c, the mock-up's GEN level): its four settings on the cards, the panel the five
 * parts as rows (the ones the page's knobs weigh lit), each a preview of its density (its amount) and heat (ENERGY)
 * from the seed, the FORM's sections as chips, the last piece and OCT+ MAKE */
BTAPP_FN static void gen_cards_bt(void)
{
    uint32_t k;
    char v[16];
    for (k = 0; k < 4u; k++) {
        uint32_t p = gp.page % 3u, a;
        int32_t r;
        gen_value(p, k, v);
        a = p == 1u ? gp.amt[k] : p == 2u && k == 0u ? gp.amt[GP_LEAD] : p == 2u && k == 2u ? gp.swing : 0u;
        r = p == 0u ? (k == 0u ? (int32_t)(gp.genre == GEN_EXPLORE ? 0u : gp.genre + 1u) * 1000 / (int32_t)(GEN_NPOS - 1u) :   /* (0.8.7:
                                                         * KNOB 1's order, EXPLORE at the left) */ k == 1u ? gp.seed / 10 : k == 2u ? gp.energy * 10 :
                       gp.form * 500) :
            (p == 1u || (p == 2u && (k == 0u || k == 2u))) ? (a == GA_AUTO ? 1000 : (int32_t)a * 10) :
            (int32_t)(k == 1u ? gp.complex : gp.human) * 10;
        draw_column(k, GEN_LBL[p][k], v, "", VAL(k), r, ICON_NONE);
    }
}
static void gen_cards(void)
{
    if (bt_hw_seg()) gen_cards_bt();
}
static uint32_t gen_draw_sig(void)
{
    return gen_sig() * 31u + gp.page * 7u + str_hash(gp.hot * 977u, gp.last) + ux.gen + anim_fr / 2u * 2246822519u +
        chain_config.count * 7919u + live.mode * 13u + song.playing * 29u +
        (chain.running ? (chain.row + 1u) * 104729u + chain.remaining * 1299709u : 0u);
}
BTAPP_FN static void gen_panel_bt(void)
{
    uint32_t k;
    char v[24];
    if (chain_config.count) {
        /* Reuse the actual arrangement, not FORM's illustrative BCADCA chips.
         * Progress refreshes per bar; no new high-rate animation or DSP work. */
        graph_proj(2);                                   /* (V10.6i) the lanes leave the helix a band of its own */
        cv_rect(0, 124, 240, 16, T_SURF);
        if (chain.running) {
            v[0] = (char)('A' + (uint32_t)live.cur % 4u); v[1] = ' ';
            fmt_int(v + 2, (int32_t)chain.row + 1);
            k = str_len(v); v[k++] = '/'; fmt_int(v + k, chain.config.count);
            str_cpy(v + str_len(v), " LEFT ", 7);
            fmt_int(v + str_len(v), chain.remaining);
        } else str_cpy(v, song.playing ? "PLAYING LOOP" : live.mode ? "SONG READY" : "LOOP MODE", sizeof v);
        cv_text(4, 128, &AF_X, v, T_TEXT);
        cv_text_r(236, 128, &AF_X, song_draft ? "DRAFT / SAVE" : "SAVE: SONG", T_ACCENT, T_SURF);
        return;
    }
    graph_proj(0);
    cv_rect(0, 124, 240, 16, T_SURF);
    cv_text(4, 128, &AF_X, song_draft ? "DRAFT LOOP" : "SAVED PARTS", T_TEXT);
    cv_text_r(236, 128, &AF_X, "SAVE: LOOP/SONG", T_ACCENT, T_SURF);
}


/* (V10.6d) GENE: a double helix turning under the parts, its rungs the SEED's own bases (two bits a rung, four
 * colours: each seed its DNA); under ARRANGE's first row when a song is arranged, none past it */
static void gen_helix(void)
{
    uint32_t n = chain_config.count, x, s = gp.seed * 2654435761u + gp.genre * 40503u, t = 0, i;
    int32_t cy = n ? 116 : 94, a = n ? 5 : 16, x0 = n ? 24 : 6;   /* (V10.6i) arranged: a band of its own under the lanes */
    int heat = !ux.mono && settings.palette != UI_BW_INDEX;   /* (V10.6q, the owner) the DNA in the thermal palette */
    uint16_t f = T_THEME, b = T_ACCENT;
    if (n > 16u)
        return;
    for (i = 0; i < n; i++) t += chain_config.row[i].bars;
    for (x = (uint32_t)x0; x < 234u; x += 2u) {
        uint32_t ang = (x * 6u + anim_fr * 4u) << 23;
        int32_t d = osc_sine(ang) * a >> 15, z = osc_sine(ang + 0x40000000u);
        if (heat) {                                      /* along the strand violet > red > orange > yellow and back,
                                                          * drifting; the strand behind dimmer */
            uint32_t p = (x * 2u + anim_fr * 3u) % 440u;
            f = heat_col(24 + (int32_t)(p < 220u ? p : 440u - p));
            b = heat_col(24 + (int32_t)(p < 220u ? 219u - p : p - 220u));
        }
        if (x % 10u == 0u) {                             /* a rung: a base pair, the seed's; arranged, the section's above it */
            int32_t h = d < 0 ? -d : d;
            uint16_t c = ux_mix(T_SURF, heat_col(40 + 64 * (int32_t)(s >> (x / 10u % 16u * 2u) & 3u)), 88);
            if (n && t) {
                uint32_t acc = 0, k;
                for (k = 0; k < n && 24u + (acc + chain_config.row[k].bars) * 212u / t <= x; k++)
                    acc += chain_config.row[k].bars;
                c = sec_col(chain_config.row[k < n ? k : n - 1u].slot & 3u);
            }
            cv_rect((int32_t)x, cy - h, 1, 2 * h + 1, c);
        }
        cv_rect((int32_t)x, cy + d, 2, 2, z > 0 ? f : heat ? ux_mix(T_SURF, f, 45) : T_DIM);
        cv_rect((int32_t)x, cy - d, 2, 2, z > 0 ? (heat ? ux_mix(T_SURF, b, 45) : T_DIM) : b);
    }
}
static void gen_panel(void)
{
    if (bt_hw_seg()) gen_panel_bt(); else graph_proj(chain_config.count ? 2 : 0);
    gen_helix();
}
#include "gen_track.c"
