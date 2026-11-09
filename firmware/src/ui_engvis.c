/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* (JIANT 0.9.4) Three engines draw their own creature where every other engine draws its being (ui_being.c): EDIT's
 * SOUND (ui_lzedit.c graph_sound_lz, on the left) and HOME's BEING (ui_lzhome.c, large in the middle); DRUM-X's kit
 * on EDIT > KIT and SOUND 1..4 (the whole panel), the colony staying on KIT 2, KIT 3 and X-MOD. Line art only,
 * antialiased (ui_organic.c og_line), in the beings' thermal colours (bg_col: heat_col; GREY and MONO their greys),
 * from small point tables (int8) and the synthesis state the UI already reads; nothing in the audio path. Each moves
 * only with its sound: in silence it rests (its time runs with the level, as the beings').
 *   VOICE   (0.9.9, "Tracto vivo") the vocal tract, close (the mouth and the neck; the crown off the panel): the head's
 *           outline a faint ghost, the tract a glowing tube of two walls from the lips over the palate and the velum
 *           down the pharynx to the glottis, the trachea under it. The tongue (the tube's floor) takes its height from
 *           F1 and its front / back from F2 (the vowel sounding: sing_view's formants with WORDS on, VOWEL / VOWEL2 /
 *           TALK without), the jaw opens down (and forward) with F1, the lips round (o u w sh) or close (p b m), the
 *           lower lip on the teeth (f v), the tongue's tip, blade or body on the place of the consonant (sing.c
 *           PHON[].place), hot where it closes; the velum drops for m n ng and a branch to the nose opens. The voicing
 *           sends glottal pulses up the tube (each a bulge, through the nose with m n ng), shakes the vocal folds (a V
 *           opening and shutting) and sends sound waves out of the mouth (the nose); turbulence at the constriction for
 *           the frication (s sh f x, the bursts) and at the glottis for the aspiration (h, BREATH); a damped nod at each
 *           vowel's onset (WORDS off: a note's), a slow sway and breath with the level.
 *   ANIMAL  a creature between a bird, an insect and a mammal: its outline, legs, wing, antennae / ears / crest are
 *           the same points in the three (VX_AN), mixed by weights that glide (a metamorphosis, ~0.6 s) to the MODE's
 *           animal (CMOS WHSTL a bird, PULSE WING an insect, GLOTT a mammal), leaned by SIZE past its presets' sizes
 *           (smaller: towards the insect; larger: the mammal) and stirred by CHAOS. With the call (each voice's syllable envelope, CMOS's
 *           chirp gate, WHSTL's syrinx amplitude): the beak / jaw opens and the throat (syrinx, larynx) swells, the
 *           wings buzz at the pulse rate (PULSE, WING), the legs stridulate, the ribs breathe (GLOTT).
 *   DRUM-X  the kit as a colony of small beings with eyes and a mouth, standing as a drum kit does: the kick large in
 *           the middle (its core hot), the snare flat on its stand, the toms on little legs, the hi-hat two shells (CH
 *           together, OH the top one lifted, its mouth open), the crash on a long stem and the cowbell a small head (BELL),
 *           the clap two little ones clapping together, the rim (RS) a spark on the snare's edge. A hit squashes its
 *           being (springing back), shuts its eyes and opens its mouth in a shout, as hard as the hit (the lane's own
 *           envelope: its velocity, its accent; it recovers with the sound's DECAY): a GHOST a small squash, a FLAM two
 *           (its hit to come a shadow), a ratchet repeated squashes. On EDIT > KIT (EDIT 1) the sound last struck named
 *           over it; on SOUND 1..4 the sound edited lit and named; HOME's BEING for a DRUM track. KIT 2, KIT 3 and
 *           X-MOD keep the colony of 0.5 (ui_being.c graph_colony: a being per sound, in two rows).
 * (JIANT 0.9.6) Six more, ported from the approved mock-up (the family's): line art in the same thermal colours (the JIANT
 * palette: DRUM-X's ramp, the mock-up's; the others as the beings), each moving only with its sound, the bichos' face
 * (vx_face) and outline (vx_blob) shared with DRUM-X's beings:
 *   ANALOG  CÉLULA: a cell whose membrane (three isotherms) is two cycles of the oscillators (WAVE, MORPH, DETUNE, MIX,
 *           NOISE, INT) through the filter (CUT on the note, RES) and DRIVE, in polar form; spikes where DRIVE clips it, a
 *           ring at CUT glowing and spreading with RES, cilia with NOISE, the two oscillators orbiting at DETUNE's beat
 *   FM6     COLONIA: the six operators as bichos wired as the algorithm (FM6_CELL, fm6_routes: carriers on the output's
 *           floor, modulators above), each as large and hot as its LEVEL times its envelope now (the sounding notes'),
 *           its spikes its RATIO, pulses down the wires, FB a curled tail; on EDIT, on OPERATOR / OPERATOR 2 (the
 *           operator picked in the accent, DETUNE a shimmer) and HOME. OP ENV: the envelope, its bicho riding it
 *   WHEEL   TUBÍCOLAS: nine tube worms, one a drawbar, as tall as its level (the digits under them), their crowns as
 *           many tentacles as the harmonic; the rotor a current bending them (its real phase and speed), PERC the
 *           worm it strikes, CLICK bubbles, DUST motes
 *   NOISE   ENJAMBRE: a swarm of protozoa round a queen with a face: COLOR their walk (white jitter .. brown drift), FREQ
 *           their swirl and the ring they keep, PEAK how tight; DUST's impulses flare them; LFSR / META the register
 *           marching in columns, one a bit
 *   PHASE   CARACOL: a snail whose shell's chambers sit at the bent phase (WAVE / WAVE2 at DCW, crowding where it
 *           rushes), the foot's sole the wave, FB frays the spiral, SUB its outer whorl, the eyestalks pulling in on a
 *           note; the second line a little one following (RING a thread)
 *   LOFI    ORGANISMO DE CIRCUITO: a caterpillar whose two skins are the last 256 samples out (the scope's: nothing more
 *           from the audio), 8 segments the byte's bits (BYTE: the highest next to the head; the legs lift with their bit,
 *           the antennae the two lowest), lit by the wave's level elsewhere (FLOAT, PLS TRI SAW NOIS WRAM)
 * (JIANT 0.9.9-snd5) Two more, the same family's:
 *   STRING  TEJEDORA: a spider on its web, one thread a voice sounding, each thread the string itself (its delay
 *           line, one period of what the loop holds now: the real standing wave, ringing down as it decays); the
 *           spider sits at POS on the top thread and plucks with a front leg at each note (saws while BOW feeds it), a
 *           cocoon under the web as large as SIZE glows with BODY; BRIGHT warms the silk
 *   ADD     (MEDUSA, the jellyfish of its partials, until V10.7.13: gone for GEN's FORM MY LOOP; the being now)
 * Each takes a rectangle (vx_draw: EDIT's left panel, HOME's middle, or a whole panel) and scales to it.
 * State: vxs (in the pool: no global of the RAM region moves). MENU > ANIM OFF: they rest. */
#define VX_POOL __attribute__((section(".pool")))
#define VX_CODE __attribute__((noinline))                 /* (each creature its own function: its size on its own) */
#define VX_SEG BTAPP_FN                                  /* (V10.48) LOFI's, ANIMAL's and STRING's creatures in the Bluetooth
                                                         * segment (the app slot full): without it vx_draw says none (the
                                                         * being instead) */
#define VX_NE 9                                          /* the creatures: VOICE ANIMAL DRUM-X ANALOG FM6 WHEEL NOISE PHASE LOFI
                                                         * ((0.9.9-snd5) STRING ADD: PHASE's and WHEEL's slots, below) */
#define VX_NSW 56                                        /* NOISE's swarm (the mock-up's 84: its cost) */
typedef struct { int32_t cx, cy, s; } vxf_t;            /* a unit (Q4: 16) -> Q4 px: c + u * s / 256 */
static struct {
    uint32_t tm[VX_NE], fr[VX_NE];                       /* each one's time (it runs with its sound), the frame it ran */
    int32_t en[VX_NE];                                   /* its level now (0..256) */
    uint32_t mt[VX_NE], ms[VX_NE];                       /* (0.9.6) the mock-up's time (Q8 frames, 30 a second: with the sound), ms */
    uint32_t nage[VX_NE], nms[VX_NE];                    /* .. the newest note seen (voice_t.age), when it came (fm1_ms) */
    int16_t wv[256];                                     /* ANALOG's / PHASE's two cycles; LOFI's last 256 samples out (Q12) */
    int16_t sw[VX_NSW][5];                               /* NOISE: the swarm (x y vx vy Q8 units, its flare Q8) */
    uint32_t rs, swt;                                    /* .. its random state, the mock-up frames it has run */
    uint32_t rph[2], bub[9], fhold;                      /* WHEEL: the rotors as last seen sounding, CLICK's bubbles; OP ENV's hold */
    int16_t bd[8];                                       /* LOFI: the byte's bits, slewed (0..256) */
    uint32_t crawl, cmt;                                 /* .. its crawl (16-bit turns), the time it was taken at */
    int16_t dal, frel;                                   /* lines drawn this much fainter (256 - alpha); OP ENV: the release from */
    int16_t art[11];                                     /* VOICE: the articulation, slewed (vx_voice) */
    int16_t js, jc, hs, hc, hk;                          /* .. the jaw's turn (sin, cos Q15) about the condyle; the head's
                                                          * about the neck (the nod and the sway), its breath (Q15, - 1) */
    int32_t nod, nodv;                                   /* .. the nod (Q15 rad) and its speed: a damped spring, struck at a
                                                          * vowel's onset (WORDS off: a note's) */
    uint32_t vmt, vph;                                   /* .. the mock-up's frames it has stepped; the sway's / breath's phase */
    uint16_t pw[2][4];                                   /* .. the glottal pulses (how far up the tract, Q16; 0 none), the
                                                          * waves leaving (their radius, Q4 units; 0 none) */
    uint8_t wh[4], pwn[2], pwi[2], pwk[2], vow;          /* .. the waves' heat; the pulses' / waves' nose bits, the next slot
                                                          * of each, the frames since the last; a vowel (1), its onset to come (2) */
    int16_t w[3];                                        /* ANIMAL: bird insect mammal weights (0..256) */
    uint8_t age[8], last;                                /* DRUM-X: each lane's age seen; the lane struck last */
    uint32_t hitm[8], tl[8], kms;                        /* .. its last hit (fm1_ms), its being's time; the ms drawn */
    uint8_t kc;                                          /* .. drawing it (its colours: vx_col) */
    uint8_t init;
} vxs VX_POOL;

/* DRUM-X's beings in the thermal ramp of the mock-up the kit was chosen from (cyan blue violet red orange yellow white:
 * the 0.6's, the JIANT palette); every other palette as the beings (bg_col) */
static const uint8_t VK_HP[8] = {0, 26, 51, 82, 118, 154, 200, 255};
static const uint16_t VK_HC[8] = {0x13D4u, 0x2DFCu, 0x4B5Fu, 0x8B7Fu, 0xFA49u, 0xFC43u, 0xFE87u, 0xFFFFu};
static uint16_t vx_col(int32_t h)
{
    uint32_t k = 0;
    if (h < -1)                                          /* (0.9.6: -2 the theme's: the operator picked, the mock-up's) */
        return T_THEME;
    if (h < 0)                                           /* (a muted sound: dim, as the colony's) */
        return ux.mono || settings.palette == UI_BW_INDEX ? ux_gray(ux_luma(T_DIM) >> 3) : T_DIM;
    h = clamp(h, 0, 255);
    if (!vxs.kc || settings.palette != UI_JIANT_INDEX)
        return bg_col(h);
    while (k < 6u && h > VK_HP[k + 1u]) k++;
    return ux_mix(VK_HC[k], VK_HC[k + 1u], (h - VK_HP[k]) * 100 / (VK_HP[k + 1u] - VK_HP[k]));
}
/* (0.9.7) HOME's beings (ui_lzhome.c, variant A): the same ramp on JIANT, every other palette as the beings (bg_col) */
static uint16_t vk_heat(int32_t h)
{
    uint32_t k = 0;
    h = clamp(h, 0, 255);
    if (settings.palette != UI_JIANT_INDEX)
        return bg_col(h);
    while (k < 6u && h > VK_HP[k + 1u]) k++;
    return ux_mix(VK_HC[k], VK_HC[k + 1u], (h - VK_HP[k]) * 100 / (VK_HP[k + 1u] - VK_HP[k]));
}
static void vx_line(const vxf_t *f, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t h)
{
    int32_t X0 = f->cx + ((x0 * f->s) >> 8), Y0 = f->cy + ((y0 * f->s) >> 8);
    int32_t X1 = f->cx + ((x1 * f->s) >> 8), Y1 = f->cy + ((y1 * f->s) >> 8);
    og_line_a(X0, Y0, X1, Y1, vx_col(h), 256 - vxs.dal);
}
/* a smooth curve through n points (Q4 units, x y pairs): quadratic arcs from midpoint to midpoint, each point their
 * control; closed or open (from the first point to the last). vx_bez: its 1 + 3 e points into o (e: its arcs, returned) */
static uint32_t vx_bez(const int32_t *p, uint32_t n, int closed, int32_t *o)
{
    uint32_t i, k, e = closed ? n : n - 1u;
    for (i = 0; i < e; i++, o += 6) {
        const int32_t *a = &p[2u * (i ? i - 1u : (closed ? n - 1u : 0u))], *b = &p[2u * i], *c = &p[2u * ((i + 1u) % n)];
        int32_t x0 = (a[0] + b[0]) / 2, y0 = (a[1] + b[1]) / 2, x1 = (b[0] + c[0]) / 2, y1 = (b[1] + c[1]) / 2;
        if (!closed && i == 0u) { x0 = b[0]; y0 = b[1]; }
        if (!closed && i + 1u == e) { x1 = c[0]; y1 = c[1]; }
        if (i == 0u) { o[0] = x0; o[1] = y0; }
        for (k = 1; k <= 3u; k++) {
            int32_t t = (int32_t)k * 85, u = 256 - t;   /* (t / 256: thirds) */
            o[2u * k] = k == 3u ? x1 : (u * u * x0 + 2 * u * t * b[0] + t * t * x1) >> 16;
            o[2u * k + 1u] = k == 3u ? y1 : (u * u * y0 + 2 * u * t * b[1] + t * t * y1) >> 16;
        }
    }
    return e;
}
static void vx_curve(const vxf_t *f, const int32_t *p, uint32_t n, int closed, int32_t h)
{
    int32_t o[2 * (1 + 3 * 24)];                         /* (24 points closed at most: seq-v2's STRING draws 24, its
                                                         * silk; main's 16 overran the stack: p2, ui_render under ASan) */
    uint32_t i, e = 3u * vx_bez(p, n, closed, o);
    for (i = 0; i < e; i++)
        vx_line(f, o[2u * i], o[2u * i + 1u], o[2u * i + 2u], o[2u * i + 3u], h);
}
/* an ellipse (Q4 units), n sides, its top at angle a0 .. a1 (16-bit turns) */
static void vx_arc(const vxf_t *f, int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint32_t a0, uint32_t a1, uint32_t n,
                   int32_t h)
{
    uint32_t i;
    int32_t px = 0, py = 0;
    for (i = 0; i <= n; i++) {
        uint32_t a = a0 + (uint32_t)(((uint64_t)(a1 - a0) * i) / n);
        int32_t x = cx + ((rx * og_cos(a)) >> 15), y = cy + ((ry * og_sin(a)) >> 15);
        if (i) vx_line(f, px, py, x, y, h);
        px = x;
        py = y;
    }
}
static uint32_t vx_rnd(uint32_t *r) { *r ^= *r << 13; *r ^= *r >> 17; *r ^= *r << 5; return *r; }
/* the level of track t now (0..256: its part's peak, slewed: a hit at once, a quick fall), k: which drawing; its time
 * runs with it (once a frame) */
static int32_t vx_level(const track_t *t, uint32_t k, int32_t en_home)
{
    uint32_t dt = ui.frame - vxs.fr[k], dm = fm1_ms - vxs.ms[k];
    int32_t v;
    if (!dt)
        return vxs.en[k];
    if (dt > 8u) dt = 8u;
    vxs.fr[k] = ui.frame;
    vxs.ms[k] = fm1_ms;
    if (dm > 100u) dm = 100u;                            /* (a pause: no jump) */
    if (vxs.en[k])                                       /* (0.9.6) the mock-up's time: 30 frames a second while it sounds */
        vxs.mt[k] += dm * 7680u / 1000u;
    if (ANIM_OFF())
        return vxs.en[k] = 0;                            /* (ANIM OFF: at rest) */
    if (en_home >= 0)
        v = en_home;                                     /* (HOME: its own, eco_step's) */
    else {
        v = ((track_t *)t)->peak * 256 / 7000;
        ((track_t *)t)->peak = 0;
        v = v > 256 ? 256 : v;
        v = v > vxs.en[k] ? v : vxs.en[k] + (v - vxs.en[k] - 2) / 3;
    }
    v = v > 4 ? v : 0;                                   /* (HOME's level rests a little above 0: silence) */
    vxs.en[k] = v;
    if (v)
        vxs.tm[k] += dt * (uint32_t)(120 + v * 4);
    return v;
}

/* ------------------------------------------------------------------------------------------------- VOICE --- */
/* (0.9.9, the mock-up's variant C, "Tracto vivo") the head in units (x right = back of the head, y down; the face left),
 * framed close (~80 x 62 about the mouth and the neck: vx_draw): its outline a ghost, the tract a tube */
static const int8_t VH_SKIN[] = {-37, -20, -31, -35, -19, -48, 0, -54, 19, -52, 33, -43, 43, -28, 46, -11, 44, 3, 38, 15,
                                 35, 27, 38, 40, 42, 58};                                            /* brow .. nape */
static const int8_t VH_FACE[] = {-37, -20, -40, -9, -37, -4, -41, 6, -47, 16, -45, 21, -40, 22};      /* brow .. nose */
/* the tract's fixed points (Q4 units): the roof from the teeth over the palate (the velum's two its own, then) and down
 * the pharynx's back wall to the glottis; the floor's from the epiglottis to the glottis; the air's way to the nose,
 * from the glottis up the pharynx, through the velum's port and the nasal cavity to the nostril (the branch: its 5th
 * point on) */
static const int16_t VH_ROOF[] = {-592, 438, -570, 355, -528, 330, -352, 326, -224, 330, 0, 0, 0, 0, 5, 400, 10, 528,
                                  14, 672, 14, 784, -22, 840};   /* (its 6th, 7th: the velum's) */
static const int16_t VH_CHIN[] = {-576, 648, -600, 760, -496, 872, -304, 880, -192, 912};   /* the chin .. throat */
static const int16_t VH_GLOT[] = {-96, 672, -138, 720, -154, 784, -154, 840};
static const int16_t VH_NOSE[] = {-96, 832, -80, 736, -72, 640, -72, 528, -64, 400, -128, 272, -288, 184, -480, 184,
                                  -624, 232, -714, 320};
static const uint8_t VH_G[16] = {255, 250, 232, 206, 173, 139, 106, 77, 54, 35, 22, 13, 8, 4, 2, 0};   /* a pulse's bulge:
                                                          * e^-(d / 1.6)^2 (Q8), d in quarters of a wall's point */
#define VH_N 22                                          /* (the walls' points, evenly along them) */

/* the articulation (vxs.art): 0 tongue x (front -), 1 its height (palate 19 .. low 32), 2 the tip's lift, 3 the jaw
 * (Q8 of its full opening), 4 lips round, 5 lips closed, 6 velum down, 7 lower lip to the teeth, 8 the constriction's
 * x, 9 its y (Q4 units), 10 how closed it is (0 open .. 256 shut); returns the phoneme's class (PC_VOW without WORDS) */
/* each place of articulation (sing.c PL_LAB .. PL_GLO): the tongue's body -x, y (units; 0: as the vowel), the tip's lift
 * (/ 2), the jaw (0: as the vowel), the lips (1 closed, 2 the lower on the teeth), the constriction's -x, y (units) */
static const uint8_t VH_PL[9][7] = {{0, 0, 0, 40, 1, 40, 30}, {0, 0, 0, 50, 2, 38, 28}, {26, 0, 150, 70, 0, 36, 27},
                                    {0, 0, 128, 60, 0, 31, 22}, {24, 21, 85, 60, 0, 27, 21}, {24, 19, 0, 50, 0, 23, 20},
                                    {12, 19, 0, 60, 0, 12, 21}, {9, 21, 0, 0, 0, 8, 24}, {0, 0, 0, 0, 0, 5, 51}};
static void vx_vowel_xy(int32_t f1, int32_t f2, int32_t *x, int32_t *y)
{
    *x = -11 * 16 - clamp(f2 - 800, 0, 1500) * 15 * 16 / 1500;      /* F2 800 .. 2300 Hz: back .. front */
    *y = 20 * 16 + clamp(f1 - 270, 0, 500) * 11 * 16 / 500;         /* F1 270 .. 770: high .. low */
}
static VX_CODE uint32_t vx_voice_target(const track_t *t, int16_t *a, int32_t *voi, int32_t *asp, int32_t *fri)
{
    int32_t f1, f2, x, y, i;
    uint32_t pl = PL_NONE, cls = PC_VOW, rnd = 0;
    if (t->p[P_VLANG]) {
        const sing_t *s = sing_view(t);
        uint32_t c = sing_phon(s);
        f1 = s->f[0];
        f2 = s->f[1];
        cls = PHON[c].cls;
        pl = PHON[c].place & 0x7Fu;
        rnd = PHON[c].place & PL_RND;
        if (cls == PC_ASP || cls == PC_SIL) pl = PL_NONE;
        *voi = s->gv > 0 ? s->gv >> 7 : 0;
        *asp = s->ga > 0 ? s->ga >> 7 : 0;
        *fri = s->gf > 0 ? s->gf >> 6 : 0;
        if (cls == PC_PLO && s->st == 0u) *fri = 0;
        if (cls == PC_SIL) *voi = 0;
    } else {                                             /* VOWEL -> VOWEL2 over TALK (as formant_render) */
        int32_t v0 = t->p[P_E0], v1 = t->p[P_E1], k = 0, j;
        uint32_t g = 0;
        for (j = 0; j < (int32_t)NVOICE; j++)
            if (t->v[j].active && (uint32_t)(t->v[j].s[7] & FMT_TALK_MASK) > g) g = (uint32_t)t->v[j].s[7] & FMT_TALK_MASK;
        if (t->p[P_E2]) { k = (int32_t)(g >> 9); k = (k * (65536 - k)) >> 15; }
        v0 += ((v1 - v0) * k) >> 15;
        {
            int32_t xx = clamp(v0, 0, 127) * 4 * 256 / 127, ii = xx >> 8, fr = xx & 255;
            if (ii >= 4) { ii = 3; fr = 256; }
            f1 = VOWEL_F[ii][0] + (((VOWEL_F[ii + 1][0] - VOWEL_F[ii][0]) * fr) >> 8);
            f2 = VOWEL_F[ii][1] + (((VOWEL_F[ii + 1][1] - VOWEL_F[ii][1]) * fr) >> 8);
            rnd = v0 > 80;
        }
        *voi = 256 - t->p[P_E5] * t->p[P_E5] / 64;
        *asp = t->p[P_E5] * 2;
        *fri = 0;
    }
    vx_vowel_xy(f1, f2, &x, &y);
    a[0] = (int16_t)x;
    a[1] = (int16_t)y;
    a[2] = 0;
    a[3] = (int16_t)clamp((f1 - 250) * 256 / 520, 0, 256);
    a[4] = (int16_t)(rnd ? 256 : 0);
    a[5] = 0;
    a[6] = (int16_t)(cls == PC_NAS ? 256 : 0);
    a[7] = 0;
    a[8] = (int16_t)(-30 * 16);                          /* (the constriction: the teeth by default) */
    a[9] = (int16_t)(27 * 16);
    a[10] = (int16_t)(pl == PL_NONE ? 0 : cls == PC_PLO || cls == PC_NAS || cls == PC_AFF ? 256 : cls == PC_FRI ? 190 : 110);
    if (pl != PL_NONE && pl <= PL_GLO) {                 /* the place: what touches what (VH_PL) */
        const uint8_t *g = VH_PL[pl - 1u];
        if (g[0]) a[0] = (int16_t)(-g[0] * 16);
        if (g[1]) a[1] = (int16_t)(g[1] * 16);
        a[2] = (int16_t)(g[2] * 2);
        if (g[3]) a[3] = g[3];
        a[5] = (int16_t)((g[4] & 1u) << 8);
        a[7] = (int16_t)((g[4] & 2u) << 7);
        a[8] = (int16_t)(-g[5] * 16);
        a[9] = (int16_t)(g[6] * 16);
    }
    for (i = 0; i < 11; i++)
        a[i] = (int16_t)clamp(a[i], -0x7000, 0x7000);
    return cls;
}
/* n points (Q4 units) turned with the head about the neck (8, 60): the nod and the sway, the breath a scale; the neck
 * bending between (y 56 .. 66), below it still */
static void vh_xf(int32_t *p, uint32_t n)
{
    for (; n--; p += 2) {
        int32_t w = clamp((66 * 16 - p[1]) * 8 / 5, 0, 256), s = vxs.hs * w >> 8, k = 32768 + (vxs.hk * w >> 8);
        int32_t c = 32768 - (((32768 - vxs.hc) * w * w) >> 16), dx = p[0] - 8 * 16, dy = p[1] - 60 * 16;
        p[0] = 8 * 16 + ((((dx * c - dy * s) >> 15) * k) >> 15);
        p[1] = 60 * 16 + ((((dx * s + dy * c) >> 15) * k) >> 15);
    }
}
/* the point (x, y) (Q4 units) with the jaw: opened down about the condyle (14, 6), the condyle gliding forward as it
 * opens (the chin drops more than it retreats) */
static void vh_jaw(int32_t *p, int32_t x, int32_t y)
{
    x -= 14 * 16;
    y -= 6 * 16;
    p[0] = 14 * 16 + ((x * vxs.jc + y * vxs.js) >> 15) - ((vxs.js * 416) >> 15);
    p[1] = 6 * 16 + ((y * vxs.jc - x * vxs.js) >> 15) + ((vxs.js * 48) >> 15);
}
static int32_t vh_len(int32_t dx, int32_t dy)            /* ~ the hypotenuse (the larger + 3/8 the smaller) */
{
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    return dx > dy ? dx + dy * 3 / 8 : dy + dx * 3 / 8;
}
/* m points evenly along the polyline P (n points; m 1: the one at s, Q16 of the way) into o */
static void vh_along(const int32_t *P, uint32_t n, int32_t *o, uint32_t m, uint32_t s)
{
    int32_t L[VH_N];
    uint32_t i, k = 1;
    L[0] = 0;
    for (i = 1; i < n; i++)
        L[i] = L[i - 1] + vh_len(P[2 * i] - P[2 * i - 2], P[2 * i + 1] - P[2 * i - 1]);
    for (i = 0; i < m; i++) {
        int32_t t = m > 1u ? L[n - 1u] * (int32_t)i / (int32_t)(m - 1u) : (int32_t)(((uint32_t)L[n - 1u] * s) >> 16), d, u;
        while (k < n - 1u && L[k] < t) k++;
        d = L[k] - L[k - 1u];
        u = d ? (t - L[k - 1u]) * 256 / d : 0;
        o[2 * i] = P[2 * k - 2] + (((P[2 * k] - P[2 * k - 2]) * u) >> 8);
        o[2 * i + 1] = P[2 * k - 1] + (((P[2 * k + 1] - P[2 * k - 1]) * u) >> 8);
    }
}
static void vh_poly(const vxf_t *f, int32_t *p, uint32_t n, int32_t h)   /* (turned with the head, then drawn) */
{
    uint32_t i;
    vh_xf(p, n);
    for (i = 1; i < n; i++)
        vx_line(f, p[2 * i - 2], p[2 * i - 1], p[2 * i], p[2 * i + 1], h);
}
static void vh_curve(const vxf_t *f, int32_t *p, uint32_t n, int32_t h)   /* (turned with the head, then drawn) */
{
    vh_xf(p, n);
    vx_curve(f, p, n, 0, h);
}
static void vh_line(const vxf_t *f, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t h)
{
    int32_t p[4] = {x0, y0, x1, y1};
    vh_poly(f, p, 2, h);
}
static void vh_node(const vxf_t *f, int32_t x, int32_t y, int32_t r, int32_t h)
{
    int32_t p[2] = {x, y};
    vh_xf(p, 1);
    og_node(f->cx + ((p[0] * f->s) >> 8), f->cy + ((p[1] * f->s) >> 8), r, vx_col(h));
}
static uint32_t vx_age(const track_t *t, uint32_t k, int32_t *note);
/* the frames of the mock-up's time (30 a second, with the sound) since it last ran: the nod's spring, the head's sway and
 * breath, the glottal pulses up the tract (every 4th while it is voiced; up the nose with m n ng), the waves leaving
 * the mouth (every 6th, the lips open; the nose's with m n ng) */
static VX_CODE void vh_step(int32_t en, int32_t voi)
{
    uint32_t n = (vxs.mt[0] >> 8) - vxs.vmt, j, k;
    vxs.vmt = vxs.mt[0] >> 8;
    if (!en) {                                           /* (silence: nothing on its way, the nod as it lies) */
        memset(vxs.pw, 0, sizeof vxs.pw);
        vxs.vow &= 1u;
        return;
    }
    for (n = n < 4u ? n : 4u; n; n--) {
        vxs.vph += (uint32_t)(120 + en * 4);
        if (vxs.vow & 2u) {
            vxs.nodv -= (1049 * (en > 150 ? en : 150)) >> 8;   /* (~0.03 rad a frame: a damped nod, ~3 degrees) */
            vxs.vow &= 1u;
        }
        vxs.nodv += (-vxs.nod * 23 - vxs.nodv * 49) / 256;     /* (w 0.3, damping 0.32, a frame) */
        vxs.nod += vxs.nodv;
        for (j = 0; j < 2u; j++) {                       /* (the pulses, the waves: each a ring, its slot 4 ago long gone) */
            uint32_t lim = j ? 26u * 16u : 65536u, d = j ? 18u : 4915u + (uint32_t)(2294 * en >> 8);   /* (1.15 units;
                                                          * 0.075 .. 0.11 of the way, a frame) */
            if (vxs.pwk[j] < 255u) vxs.pwk[j]++;
            if (voi > 30 + 10 * (int32_t)j && vxs.pwk[j] >= 4u + 2u * j && (!j || vxs.art[5] <= 160 || vxs.art[6] > 128)) {
                k = vxs.pwi[j]++ & 3u;
                vxs.pw[j][k] = 1;
                vxs.pwn[j] = (uint8_t)(vxs.art[6] > 128 ? vxs.pwn[j] | 1u << k : vxs.pwn[j] & ~(1u << k));
                vxs.pwk[j] = 0;
                if (j) vxs.wh[k] = (uint8_t)(voi < 255 ? voi : 255);
            }
            for (k = 0; k < 4u; k++)
                if (vxs.pw[j][k])
                    vxs.pw[j][k] = (uint16_t)(vxs.pw[j][k] + d < lim ? vxs.pw[j][k] + d : 0u);
        }
    }
}
static VX_CODE void vx_voice(const track_t *t, int32_t cx, int32_t cy, int32_t s, int32_t en_home)
{
    vxf_t f = {cx, cy, s};
    int16_t tg[11], *a = vxs.art;
    int32_t en = vx_level(t, 0, en_home), voi, asp, fri, i, j, hb, pr, uly, mid, bul[VH_N], ll[2], q[2 * 7], w[2 * VH_N];
    int32_t W[4][2 * VH_N], *R = W[0], *F = W[1];        /* (the walls, roof and floor; their glows) */
    uint32_t cls = vx_voice_target(t, tg, &voi, &asp, &fri), n0 = vxs.nms[0], r, k, nm[2] = {0, 0};
    vx_age(t, 0, 0);                                     /* (a note newer than the last: WORDS off, the nod's onset) */
    if (!vxs.init) {
        vxs.init = 1;
        for (i = 0; i < 11; i++) a[i] = tg[i];
        vxs.nod = vxs.nodv = 0;
        memset(vxs.pw, 0, sizeof vxs.pw);
        vxs.pwk[0] = vxs.pwk[1] = 255;
        vxs.vmt = vxs.mt[0] >> 8;
    }
    if (en || t->p[P_VLANG])                             /* (it moves as it sings; still in silence) */
        for (i = 0; i < 11; i++) a[i] = (int16_t)(a[i] + (tg[i] - a[i]) / 2);
    voi = voi * en >> 8;
    asp = asp * en >> 8;
    fri = fri * en >> 8;
    hb = en * 50 / 256;                                  /* (the sound warms the tract) */
    k = t->p[P_VLANG] ? cls == PC_VOW : 0u;
    if (t->p[P_VLANG] ? k && !(vxs.vow & 1u) : vxs.nms[0] != n0)
        vxs.vow |= 2u;                                   /* (an onset: the nod struck at the next frame) */
    vxs.vow = (uint8_t)((vxs.vow & 2u) | k);
    vh_step(en, voi);
    {   /* the head's turn now: the nod, the sway (~1.3 degrees at full level), the breath (~2 %) */
        int32_t ph = vxs.nod * 7 / 10 + ((og_sin(vxs.vph * 230u >> 8) * (en * 721 >> 8)) >> 15);
        uint32_t tn = (uint32_t)((ph * 10430) >> 15);    /* (Q15 rad: 16-bit turns) */
        vxs.hs = (int16_t)og_sin(tn);
        vxs.hc = (int16_t)og_cos(tn);
        vxs.hk = (int16_t)((og_sin(vxs.vph * 435u >> 8) * (en * 590 >> 8)) >> 15);
        tn = (uint32_t)clamp(a[3], 0, 256) * 2086u / 256u;   /* (the jaw: up to ~11.5 degrees) */
        vxs.js = (int16_t)og_sin(tn);
        vxs.jc = (int16_t)og_cos(tn);
    }
    /* the lips: rounded (o u w sh) forward, closed (p b m) together, the lower one on the teeth (f v); the upper lip
     * lifts a little as the jaw opens */
    pr = a[4] * 3 / 16;
    uly = 458 - ((a[3] * (256 - a[5])) >> 8) * 144 / 2560;
    vh_jaw(ll, -659 - pr + a[7] / 4, 474 - a[7] * 35 / 256);
    ll[1] = uly + (ll[1] - uly) * (256 - a[4] * 115 / 256) / 256;
    ll[1] += (uly + 11 - ll[1]) * a[5] / 256;
    ll[0] += (-659 - pr - ll[0]) * a[5] / 256;
    mid = (uly + ll[1]) / 2;
    /* the ghost: the skin, the brow .. the nose .. the upper lip, the lower lip and the chin (the jaw's) down the throat */
    for (i = 0; i < 26; i++) w[i] = VH_SKIN[i] * 16;
    vh_curve(&f, w, 13, 34);
    for (i = 0; i < 14; i++) w[i] = VH_FACE[i] * 16;
    w[14] = -678 - pr * 7 / 10; w[15] = 400; w[16] = -666 - pr; w[17] = uly;
    vh_curve(&f, w, 9, 34);
    w[0] = ll[0]; w[1] = ll[1];
    vh_jaw(w + 2, -646 - pr * 3 / 5, 552);
    for (i = 0; i < 5; i++)
        vh_jaw(w + 4 + 2 * i, VH_CHIN[2 * i], VH_CHIN[2 * i + 1]);
    w[12] = (w[12] - 192) / 2; w[13] = (w[13] + 912) / 2;   /* (the throat: half the jaw's) */
    w[14] = -192; w[15] = 1056;
    vh_curve(&f, w, 8, 40 + hb / 2);
    /* the tract's walls: the roof (the upper lip, the teeth, the palate, the velum: down with m n ng, the pharynx's
     * back wall) and the floor (the lower lip, the lower teeth, the tongue: its body at F1 F2 (moved 1.4 x), its tip and
     * blade on the consonant's place; the epiglottis), each to the glottis, VH_N points evenly along */
    i = a[6] * 104 / 256;                                /* (the velum's drop, x 1.3) */
    w[0] = -666 - pr; w[1] = uly;
    for (j = 0; j < 24; j++) w[2 + j] = VH_ROOF[j];
    w[12] = -112; w[13] = 333 + i * 3 / 5; w[14] = -48 - i * 4 / 5; w[15] = 371 + i;
    vh_along(w, 13, R, VH_N, 0);
    {
        int32_t bx = -288 + (a[0] + 288) * 7 / 5, by = 400 + (a[1] - 400) * 7 / 5, rx = -112 + (bx + 288) / 5;
        vh_jaw(q, -480, 576);
        vh_jaw(q + 2, -528 + a[2] / 8, 480);
        q[3] -= a[2] / 2;
        q[4] = (q[2] + bx) / 2; q[5] = (q[3] + by) / 2 - a[2] / 8 + 16;
        q[6] = bx; q[7] = by + a[3] * 3 / 16;
        q[8] = (bx + rx) / 2 + 32; q[9] = (by + 544) / 2 + a[3] / 8;
        q[10] = rx; q[11] = 544; q[12] = -96; q[13] = 672;
        vx_bez(q, 7, 0, W[2]);                           /* (the tongue: 19 points, the floor its 3rd .. 18th) */
        w[0] = ll[0]; w[1] = ll[1];
        vh_jaw(w + 2, -576, 464);
        for (i = 0; i < 32; i++) w[4 + i] = W[2][4 + i];
        for (i = 0; i < 8; i++) w[36 + i] = VH_GLOT[i];
        vh_along(w, VH_N, F, VH_N, 0);
    }
    /* each glottal pulse a bulge travelling up the tube, the walls pushed apart by it (with the voicing); the inner
     * glow a unit in from each wall; hotter where the pulse is, and at the constriction as it closes */
    memset(bul, 0, sizeof bul);
    for (k = 0; k < 4u; k++)
        if (vxs.pw[0][k] && !(vxs.pwn[0] >> k & 1u)) {
            int32_t u = ((65536 - vxs.pw[0][k]) * (VH_N - 1)) >> 8;   /* (Q8 points from the lips) */
            for (i = 0; i < VH_N; i++) {
                int32_t e = i * 256 - u;
                e = (e < 0 ? -e : e) >> 4;               /* (sixteenths) */
                if (e < 60)
                    bul[i] += VH_G[e >> 2] + ((VH_G[(e >> 2) + 1] - VH_G[e >> 2]) * (e & 3)) / 4;
            }
        }
    for (i = 0; i < 2 * VH_N; i += 2) {
        int32_t dx = R[i] - F[i], dy = R[i + 1] - F[i + 1], l = vh_len(dx, dy) + 1, g = (bul[i / 2] * voi * 115) >> 19;
        R[i] += dx * g / l; R[i + 1] += dy * g / l;
        F[i] -= dx * g / l; F[i + 1] -= dy * g / l;
        l = vh_len(dx = F[i] - R[i], dy = F[i + 1] - R[i + 1]) + 1;
        l = l > 44 ? 4506 / l : 102;                     /* (1.1 units in, at most 0.4 of the way: Q8) */
        W[2][i] = R[i] + (dx * l >> 8); W[2][i + 1] = R[i + 1] + (dy * l >> 8);
        W[3][i] = F[i] - (dx * l >> 8); W[3][i + 1] = F[i + 1] - (dy * l >> 8);
        for (j = 0; j < 2; j++) {
            dx = W[j][i] - a[8]; dy = W[j][i + 1] - a[9];
            nm[j] |= (uint32_t)(dx * dx + dy * dy < 96 * 96) << (i / 2);
        }
    }
    vh_xf(W[0], 4 * VH_N);
    for (j = 0; j < 4; j++)
        for (i = 1; i < VH_N; i++) {
            int32_t b = (bul[i] + bul[i - 1]) / 2, h = j < 2 ? 150 + 10 * j + hb + ((b < 256 ? b : 256) * 70 * voi >> 16) +
                        (nm[j] >> i & 1u ? a[10] * 60 / 256 : 0) : 70 + hb + (bul[i] * 60 * voi >> 16);
            vx_line(&f, W[j][2 * i - 2], W[j][2 * i - 1], W[j][2 * i], W[j][2 * i + 1], h);
        }
    vh_line(&f, -154, 872, -160, 1056, 110);             /* the trachea */
    vh_line(&f, -26, 872, -32, 1056, 100);
    /* the nose: a branch that opens as the velum drops (m n ng), the pulses through it */
    if (a[6] > 31) {
        int32_t v = a[6];
        for (j = 0; j < 2; j++) {
            for (i = 0; i < 6; i++) {
                W[2][2 * i] = VH_NOSE[8 + 2 * i] + (i ? (j ? 32 : -16) : (j ? -120 : 120)) * v / 1280;
                W[2][2 * i + 1] = VH_NOSE[9 + 2 * i] + (j ? v : -v) / 10;
            }
            vh_poly(&f, W[2], 6, 60 + v * (90 + hb) / 256);
        }
        for (i = 0; i < 20; i++) w[i] = VH_NOSE[i];
        for (k = 0; k < 4u; k++)
            if (vxs.pwn[0] >> k & 1u && vxs.pw[0][k] > 22938u) {   /* (past 0.35 of the way: in the branch) */
                vh_along(w, 10, q, 1, vxs.pw[0][k]);
                vh_node(&f, q[0], q[1], 1, 220);
            }
    }
    {   /* the vocal folds: a V opening and shutting with the voicing (the commissure in front, the arytenoids behind) */
        int32_t sn = og_sin(vxs.mt[0] * 21174u >> 8), o = voi > 8 ? ((voi * (11469 + (((sn < 0 ? -sn : sn) * 21299) >> 15))) >> 8) * 29 >> 15 : 2;
        for (j = 0; j < 2; j++)
            vh_line(&f, -154, 838, -51, j ? 838 + o : 838 - o, 200 - 20 * j + voi / 4);
    }
    /* the constriction: where the tract closes, a hot mark (p t k: shut; s sh f: a slit; l r y w: narrowed) */
    if (a[10] > 60)
        vh_node(&f, a[8], a[9], 1 + a[10] / 128, 150 + a[10] / 3 + hb);
    /* the air: turbulence from the constriction out through the mouth (frication), at the glottis (aspiration) */
    r = ui.frame * 2654435761u + 1u;
    for (i = 0; i < fri / 14 && i < 12; i++) {           /* (a jet: from the slit forward and out between the teeth) */
        int32_t e = (int32_t)(vx_rnd(&r) & 255u), x = a[8] - e * 18 * 16 / 256;
        int32_t y = a[9] + (29 * 16 - a[9]) * clamp(e * 2, 0, 256) / 256 + ((int32_t)((r >> 8) & 63u) - 32) * (16 + e / 8) / 64;
        if (x > -52 * 16)
            vh_line(&f, x, y, x - 2 * 16 - (int32_t)((r >> 16) & 15u), y + (int32_t)((r >> 20) & 15u) - 8, 236 - e / 3);
    }
    for (i = 0; i < asp / 24 && i < 3; i++) {            /* (breath at the folds, rising) */
        int32_t e = (int32_t)(vx_rnd(&r) & 127u), x = -6 * 16 + (int32_t)((r >> 8) & 63u) - 32, y = 52 * 16 - e * 2;
        vh_line(&f, x, y, x + (int32_t)((r >> 16) & 15u) - 8, y - 2 * 16, 190 - e / 2);
    }
    /* the sound leaving: arcs from the mouth (the nose with m n ng), growing and cooling */
    for (k = 0; k < 4u; k++)
        if (vxs.pw[1][k]) {
            int32_t rr = 32 + vxs.pw[1][k] * 11 / 10, nz = vxs.pwn[1] >> k & 1u;
            uint32_t sp = nz ? 4589u : 5736u;           /* (+-0.44 / 0.55 rad) */
            for (i = 0; i <= 5; i++) {
                uint32_t an = 32768u - sp + 2u * sp * (uint32_t)i / 5u;
                w[2 * i] = (nz ? -730 : -682 - pr) + ((rr * og_cos(an)) >> 15);
                w[2 * i + 1] = (nz ? 326 : mid) + ((rr * 21 / 20 * og_sin(an)) >> 15);
            }
            vh_poly(&f, w, 6, (140 + vxs.wh[k] * 3 / 10) * (26 * 16 - vxs.pw[1][k]) / (26 * 16) + 30);
        }
}

/* ------------------------------------------------------------------------------------------------ ANIMAL --- */
/* the three animals, the same points in each (units, facing left, ~100 x 70): the outline (16, from the beak / snout
 * / mouthparts' tip round the head, the back, the tail, the belly, the throat, the chin), four limbs (hip knee foot:
 * a bird's two legs, the others folded into its belly; an insect's front, middle and jumping legs; a mammal's four), the
 * wing (5: a bird's folded wing, an insect's forewings, a mammal's ribcage), two head strokes (3: a crest, antennae,
 * ears), the eye, the mouth's corner, the throat's organ (syrinx, tymbal, larynx) */
enum { VA_OUT = 0, VA_LIMB = 32, VA_WING = 56, VA_HEAD = 66, VA_EYE = 78, VA_MTH = 80, VA_ORG = 82, VA_NP = 84 };
static const int8_t VX_AN[3][VA_NP] = {
    {   /* a bird (a songbird perched) */
        -48, -15, -34, -24, -26, -29, -17, -24, -8, -15, 8, -11, 22, -7, 30, -6, 52, 3, 30, 4, 18, 11, 2, 15, -12, 9, -20, -1,
        -26, -8, -38, -12,
        2, 13, 7, 23, 1, 33, 8, 12, 13, 23, 8, 33, 10, 12, 10, 12, 10, 12, 12, 11, 12, 11, 12, 11,
        -15, -12, 4, -14, 30, -3, 12, 4, -8, 2,
        -27, -28, -26, -35, -19, -38, -23, -27, -18, -32, -13, -33,
        -32, -20, -36, -14, -16, 2},
    {   /* an insect (a cricket) */
        -46, 3, -44, -8, -37, -13, -29, -11, -21, -13, 0, -11, 20, -9, 36, -5, 50, 0, 37, 4, 22, 8, 2, 8, -18, 7, -28, 6,
        -35, 7, -43, 7,
        -24, 6, -33, 16, -40, 25, -14, 7, -10, 19, -18, 28, 2, 6, 24, -12, 36, 26, -8, 7, -2, 18, 4, 27,
        -22, -13, 0, -16, 42, -7, 30, -2, -16, -7,
        -43, -9, -32, -36, 8, -46, -41, -9, -22, -32, 24, -38,
        -38, -5, -42, 4, 18, -2},
    {   /* a mammal (a fox) */
        -52, -6, -38, -17, -29, -22, -20, -17, -10, -15, 8, -16, 22, -15, 30, -15, 52, -19, 35, -2, 22, 6, 2, 8, -14, 6, -22, 2,
        -31, -1, -45, -1,
        -14, 4, -17, 18, -15, 32, -8, 4, -9, 18, -6, 32, 18, 4, 26, 17, 20, 32, 23, 4, 31, 17, 26, 32,
        -14, -9, 0, -11, 12, -7, 6, 5, -10, 4,
        -28, -21, -26, -33, -20, -20, -24, -20, -19, -31, -15, -17,
        -36, -13, -38, -3, -27, 2},
};
static const uint8_t VA_EYE_R[3] = {3, 5, 2};
/* MODE -> the animal on the line insect 0 .. bird 128 .. mammal 256; the middle of its presets' SIZE */
static const uint8_t VA_POS[AN_NMODE] = {128, 0, 128, 0, 255}, VA_MID[AN_NMODE] = {64, 30, 50, 70, 64};   /* (.. its presets' SIZE) */
static VX_SEG void vx_animal(const track_t *t, int32_t cx, int32_t cy, int32_t s, int32_t en_home)
{
    vxf_t f = {cx, cy, s};
    const int16_t *p = t->p;
    uint32_t mode = an_mode(p), i, j;
    int32_t en = vx_level(t, 1, en_home), act = 0, w[3], m, tgt, pt[2 * 42], q[2 * 16], op, flap, hb;
    uint32_t tm = vxs.tm[1];
    for (i = 0; i < NVOICE; i++) {                       /* the call now: each voice's level (its syllable's, CMOS's gate) */
        const voice_t *v = &t->v[i];
        int32_t a;
        if (!v->active) continue;
        a = v->env_out > 0 ? v->env_out >> 7 : 0;
        if (mode == AN_WHSTL) a = a * clamp((int32_t)v->ph[1], 0, 24000) / 16000;
        if (a > act) act = a;
    }
    act = clamp(act, 0, 256) * clamp(en * 2, 0, 256) >> 8;
    tgt = VA_POS[mode];
    if (mode != AN_CMOS) {                               /* SIZE (CMOS: CAP there) past the mode's own animals' sizes */
        int32_t d = p[P_E3] - VA_MID[mode];
        d = d > 12 ? d - 12 : d < -12 ? d + 12 : 0;
        tgt += d * 6 / 5;
        if (mode != AN_GLOTT)
            tgt += (og_sin(tm >> 2) * p[P_E2]) >> 17;   /* (CHAOS stirs it) */
    }
    tgt = clamp(tgt, 0, 256);
    m = vxs.w[0];
    if (!vxs.w[1]) { vxs.w[1] = 1; m = tgt; }           /* (the first look: at once) */
    if (vxs.w[2] != (int16_t)ui.frame) {                 /* (the metamorphosis glides: once a frame) */
        m += (tgt - m) / 5 + (tgt > m) - (tgt < m);
        vxs.w[2] = (int16_t)ui.frame;
    }
    vxs.w[0] = (int16_t)m;
    w[0] = m >= 128 ? (256 - m) * 2 : m * 2;             /* bird */
    w[1] = m < 128 ? (128 - m) * 2 : 0;                  /* insect */
    w[2] = m > 128 ? (m - 128) * 2 : 0;                  /* mammal */
    if (w[0] > 256) w[0] = 256;
    hb = act * 60 / 256;
    for (i = 0; i < VA_NP / 2; i++)                      /* the animal now: the three mixed (Q4 units) */
        for (j = 0; j < 2u; j++)
            pt[2 * i + j] = (VX_AN[0][2 * i + j] * w[0] + VX_AN[1][2 * i + j] * w[1] + VX_AN[2][2 * i + j] * w[2]) >> 4;
    /* the call: the beak / mouth opens (the chin down about the mouth's corner), wings, legs, ribs move */
    op = act * (8 + (mode == AN_GLOTT ? p[P_E1] / 16 : 0)) / 256 * 16;
    pt[2 * 15 + 1] += op * (w[0] + w[2]) / 256 + op / 3;
    pt[2 * 14 + 1] += op / 3;
    flap = 0;
    if (mode == AN_PULSE) flap = act ? (int32_t)((ui.frame & 1u) ? 1 : -1) * (2600 + act * 20) : 0;
    else if (mode == AN_WING) flap = act ? og_sin(ui.frame * 0x5100u) * (act * 26) >> 15 : 0;
    else if (act > 40) flap = og_sin(ui.frame * 0x2400u) * (act / 4) >> 9;
    flap -= (w[1] * 3000) >> 8;                           /* (an insect's wings raised a little, as it sings) */
    {   /* the body: three isotherms, the outer cold */
        static const int16_t SC[3] = {256, 170, 96}, HT[3] = {40, 116, 176};
        int32_t breath = w[2] ? ((og_sin(tm >> 1) + 32768) >> 12) * w[2] / 256 : 0;
        for (j = 0; j < 3u; j++) {
            for (i = 0; i < 16u; i++) {
                q[2 * i] = 2 * 16 + (((pt[2 * i] - 2 * 16) * SC[j]) >> 8);
                q[2 * i + 1] = ((pt[2 * i + 1] * (SC[j] + (j ? breath : breath / 2))) >> 8);
            }
            vx_curve(&f, q, 16, 1, HT[j] + hb + en / 8);
        }
    }
    /* the limbs: hip, knee, foot (a bird's feet with toes; an insect's legs shaking as it stridulates) */
    for (i = 0; i < 4u; i++) {
        int32_t *l = &pt[VA_LIMB + 6 * i], jx = 0;
        if (mode == AN_PULSE && act && i == 2u) jx = (ui.frame & 1u) ? 2 * 16 : -2 * 16;
        q[0] = l[0]; q[1] = l[1]; q[2] = l[2] + jx; q[3] = l[3] - jx / 2; q[4] = l[4]; q[5] = l[5];
        if ((q[0] - q[4]) * (q[0] - q[4]) + (q[1] - q[5]) * (q[1] - q[5]) < 4 * 256)
            continue;                                    /* (folded away) */
        vx_line(&f, q[0], q[1], q[2], q[3], i & 1u ? 70 : 120);
        vx_line(&f, q[2], q[3], q[4], q[5], i & 1u ? 70 : 120);
        if (w[2] > 96)                                   /* (a mammal's paws) */
            vx_line(&f, q[4], q[5], q[4] - 4 * 16 * w[2] / 256, q[5], i & 1u ? 70 : 120);
        if (w[0] > 96 && i < 2u) {                       /* (toes) */
            vx_line(&f, q[4], q[5], q[4] - 6 * 16 * w[0] / 256, q[5] + 16, 110);
            vx_line(&f, q[4], q[5], q[4] + 4 * 16 * w[0] / 256, q[5] + 16, 110);
        }
    }
    /* the wing (turned about its root by the flap; WING mode: a blur of two) / the ribcage (breathing) */
    {
        int32_t k, n = mode == AN_WING && act ? 2 : 1, c0 = og_cos((uint32_t)flap & 0xFFFFu), s0 = og_sin((uint32_t)flap & 0xFFFFu);
        for (k = 0; k < n; k++) {
            int32_t ox = pt[VA_WING], oy = pt[VA_WING + 1], cs = k ? c0 : c0, sn = k ? -s0 : s0;
            for (i = 0; i < 5u; i++) {
                int32_t x = pt[VA_WING + 2 * i] - ox, y = pt[VA_WING + 2 * i + 1] - oy;
                int32_t sc = 256 + (w[2] * ((og_sin(tm >> 1) >> 9) + act / 2)) / 256 / 2;
                q[2 * i] = ox + (((x * cs - y * sn) >> 15) * sc >> 8);
                q[2 * i + 1] = oy + (((x * sn + y * cs) >> 15) * sc >> 8);
            }
            vx_curve(&f, q, 5, 1, (k ? 90 : 150) + hb);
            for (i = 1; i < 4u; i++)                     /* (veins / feathers / ribs) */
                vx_line(&f, q[0] + (q[2 * i] - q[0]) / 5, q[1] + (q[2 * i + 1] - q[1]) / 5,
                        q[2 * i] - (q[2 * i] - q[0]) / 6, q[2 * i + 1] - (q[2 * i + 1] - q[1]) / 6, 104 + hb);
        }
    }
    /* the head's strokes: crest, antennae (trembling with the call), ears */
    for (i = 0; i < 2u; i++) {
        int32_t *h = &pt[VA_HEAD + 6 * i], tr = w[1] && act ? (og_sin(ui.frame * 0x3700u + i * 0x5000u) * act) >> 17 : 0;
        q[0] = h[0]; q[1] = h[1]; q[2] = h[2] + tr; q[3] = h[3]; q[4] = h[4] + tr * 2; q[5] = h[5] - tr;
        vx_curve(&f, q, 3, 0, i ? 96 : 140);
    }
    /* the eye (hot), the mouth's line, the throat's organ swelling with the call; song arcs from the mouth */
    vx_arc(&f, pt[VA_EYE], pt[VA_EYE + 1], (VA_EYE_R[0] * w[0] + VA_EYE_R[1] * w[1] + VA_EYE_R[2] * w[2]) >> 4,
           (VA_EYE_R[0] * w[0] + VA_EYE_R[1] * w[1] + VA_EYE_R[2] * w[2]) >> 4, 0, 65536u, 8, 236);
    og_node(cx + ((pt[VA_EYE] * s) >> 8), cy + ((pt[VA_EYE + 1] * s) >> 8), 1, vx_col(250));
    vx_line(&f, pt[VA_MTH], pt[VA_MTH + 1], pt[0], pt[1], 150);
    vx_line(&f, pt[VA_MTH], pt[VA_MTH + 1], pt[2 * 15], pt[2 * 15 + 1], 150);
    {
        int32_t r = (2 * 16 + act * 3 * 16 / 256) * (w[0] + w[2]) / 256;
        if (r > 16)
            vx_arc(&f, pt[VA_ORG], pt[VA_ORG + 1], r, r * 3 / 4, 0, 65536u, 10, 180 + hb);
        if (w[1] > 40)                                   /* (the abdomen's segments: the tymbal pulsing) */
            for (i = 0; i < 4u; i++) {
                int32_t x = (pt[2 * 6] * (int32_t)(i + 1) + pt[2 * 5] * (int32_t)(4 - i)) / 5, sp = act ? (int32_t)((ui.frame + i) & 1u) * 16 : 0;
                vx_line(&f, x + sp, pt[2 * 5 + 1] + 2 * 16, x - 2 * 16 + sp, pt[2 * 11 + 1] - 2 * 16, 60 + (w[1] * (90 + hb) >> 8));
            }
    }
    if (act > 24)                                        /* the call: arcs out of the mouth, as loud as it is */
        for (i = 0; i < 3u; i++) {
            int32_t ph = (int32_t)((tm >> 4) + i * 85u) & 255, r = (6 + ph / 12) * 16;
            vx_arc(&f, pt[0] - 2 * 16, pt[1] + op / 2, r, r, 0x6000u, 0xA000u, 5, (act * (255 - ph)) >> 8);
        }
}

/* ------------------------------------------------------------------------------------------------ DRUM-X --- */
/* the kit as a colony of small beings (units ~124 x 90: x right, y down, the floor at 40), each drum one with eyes and
 * a mouth: the kick large in the middle, the snare flat on its stand, the toms on little legs, the hi-hat two shells,
 * the crash on a long stem, the cowbell a small head over the kick, the clap two little ones clapping */
enum { VK_KICK, VK_SNARE, VK_CLAP, VK_HATC, VK_HATO, VK_TOM, VK_RIM, VK_BELL };
/* 256 * 2^(-ms / half), Q8 (the squash's fall) */
static int32_t vx_fall(uint32_t ms, uint32_t half)
{
    uint32_t n = ms / half;
    int32_t v;
    if (n > 8u) return 0;
    v = 256 >> n;
    return v - (int32_t)((uint32_t)v * (ms % half) / (2u * half));
}
/* a being's outline (ui_being.c bg_outline's, its lobes 3, the spikes' sharpness 4), squashed: at (x, y), radius r
 * (Q4 units), x stretched by ax / 256, then scaled sx sy / 256 about x and its foot (by: y's place after the squash) */
typedef struct { int32_t x, y, by, ax, sx, sy, sp, len, wob, rip, lob, shp; } vxb_t;   /* (0.9.6: lob lobes, shp 4 or 1) */
static void vx_blob(const vxf_t *f, const vxb_t *b, int32_t r, int outer, uint32_t t, uint32_t nv, int32_t h)
{
    uint32_t i, lb = (uint32_t)b->lob;
    int32_t px = 0, py = 0;
    for (i = 0; i <= nv; i++) {
        uint32_t th = (i % nv) * 65536u / nv;
        int32_t rr = r + ((((r * b->wob) >> 8) * ((9175 * og_sin(lb * th + t) + 3932 * og_sin((lb + 2u) * th - 2u * t)) >> 15)) >> 15), x, y;
        if (outer) {
            int32_t c2 = og_sin(((uint32_t)b->sp * th >> 1) + 0x4000u), p = c2 > 0 ? c2 : 0;
            if (b->shp > 1) {
                p = (p * p) >> 15;
                p = (p * p) >> 15;                       /* (c2^4) */
            }
            rr += (((((r * b->len) >> 8) * p) >> 15) * 26) >> 4;
            rr += ((((r * b->rip) >> 8) * og_sin(th * 18u + 3u * t)) >> 15) / 14;
        }
        x = b->x + (((((rr * og_cos(th)) >> 15) * b->ax) >> 8) * b->sx >> 8);
        y = b->by + ((((rr * og_sin(th)) >> 15) * b->sy) >> 8);
        if (i) vx_line(f, px, py, x, y, h);
        px = x;
        py = y;
    }
}
/* a small dot at (x, y) (Q4 units), radius r (Q4 px: under 1.5 px a 2 x 2 dot) */
static void vx_dot(const vxf_t *f, int32_t x, int32_t y, int32_t r, int32_t h)
{
    uint16_t c = vx_col(h);
    int32_t X = f->cx + ((x * f->s) >> 8), Y = f->cy + ((y * f->s) >> 8);
    if (r >= 32) {
        og_node(X, Y, r >> 4, c);
        return;
    }
    X = (X - 8) >> 4;
    Y = (Y - 8) >> 4;
    for (r = 0; r < 4; r++)
        og_plot(X + (r & 1), Y + (r >> 1), c, 256);
}
/* the bichos' face (DRUM-X's beings, the 0.9.6 creatures): two eyes ex either side of x at ey (shut: short lines,
 * else dots of er Q4 px), the mouth at my: a line mw either side, or open (mo) an ellipse mx wide; their heats */
static void vx_face(const vxf_t *f, int32_t x, int32_t ey, int32_t ex, int32_t er, int shut, int32_t my, int32_t mw,
                    int32_t mo, int32_t mx, int32_t he, int32_t hm, int32_t hl)
{
    int32_t k;
    for (k = -1; k <= 1; k += 2) {
        if (shut)
            vx_line(f, x + k * ex - 22, ey, x + k * ex + 22, ey, he);
        else
            vx_dot(f, x + k * ex, ey, er, he);
    }
    if (mo)
        vx_arc(f, x, my + mo / 2, mx, mo, 0, 65536u, 12, hm);
    else
        vx_line(f, x - mw, my, x + mw, my, hl);
}
/* .. as the mock-up's bface: at (x, y) for a body of radius r (Q4 units), m its shout (0..256), shut its eyes; hot
 * warms the mouth (0..256) */
static void vx_bface(const vxf_t *f, int32_t x, int32_t y, int32_t r, int32_t m, int shut, int32_t hot)
{
    int32_t ey = y - r / 10, mo = clamp(m, 0, 256) * r * 42 / 25600, er = ((r * f->s) >> 8) / 10;
    vx_face(f, x, ey, r * 36 / 100, er, shut, mo > 8 ? ey + r * 36 / 100 : ey + r * 34 / 100, r * 14 / 100,
            mo > 8 ? mo : 0, r * 16 / 100 + mo * 22 / 100, 243, 205 + hot / 5, 154);
}
/* a drum's being at (x, y), rx x ry (Q4 units): e its hit now (0..256: the lane's envelope), age since it (ms), t its
 * time (it runs with the sound), sp its spikes, len their length, legs (Q4 units, 0: none); lit 1 the sound edited, -1
 * muted. Struck it squashes (springing back with the hit's age), shuts its eyes and shouts (the mouth open) */
static void vx_bug(const vxf_t *f, int32_t x, int32_t y, int32_t rx, int32_t ry, int32_t e, uint32_t age, uint32_t t,
                   int32_t sp, int32_t len, int32_t legs, int32_t lit)
{
    vxb_t b;
    int32_t sq = e ? (((e * og_cos(age * 229u)) >> 15) * vx_fall(age, 115u)) >> 8 : 0;   /* (22 rad/s, e^-6t) */
    int32_t k, ey, ex, mo, hb = lit > 0 ? 50 : 0, ho = lit < 0 ? -1 : 41 + e * 77 / 256 + hb;
    int32_t hi = lit < 0 ? -1 : 92 + e * 90 / 256 + hb, he = lit < 0 ? -1 : 243;
    uint32_t nv = (uint32_t)clamp(((rx + ry) * f->s >> 12) * 2, 20, 64);
    if (nv < (uint32_t)sp * 3u) nv = (uint32_t)sp * 3u;
    if (sys_load) nv = nv * (sys_load == LOAD_ECO ? 1u : 2u) / (sys_load == LOAD_ECO ? 2u : 3u);   /* (LITE, ECO) */
    b.x = x;
    b.y = y;
    b.sx = 256 + sq * 90 / 256;
    b.sy = 256 - sq * 102 / 256;
    b.by = y + ry * (256 - b.sy) / 256;
    b.ax = rx * 256 / ry;
    b.sp = sp;
    b.len = len + e * 60 / 256;
    b.wob = 40 + e * 120 / 256;
    b.rip = e * 100 / 256;
    b.lob = 3;
    b.shp = 4;
    if (legs)
        for (k = -1; k <= 1; k += 2) {
            int32_t x1 = x + k * rx * 7 / 10, y1 = y + ry + legs * 6 / 10;
            vx_line(f, x + k * rx / 2, y + ry * 7 / 10, x1, y1, lit < 0 ? -1 : 56 + hb);
            vx_line(f, x1, y1, x + k * rx * 6 / 10, y + ry + legs, lit < 0 ? -1 : 56 + hb);
        }
    vx_blob(f, &b, ry, 1, t, nv, ho);                    /* the membrane (cold), the core */
    vx_blob(f, &b, ry * 66 / 100, 0, t, nv * 2u / 3u, hi);
    ey = b.by - ry * 15 * b.sy / 25600;
    ex = rx * 32 * b.sx / 25600;
    mo = e * ry * 4 / 2560;                              /* the eyes shut on a hard hit; the mouth a shout as loud as it */
    vx_face(f, x, ey, ex, ((ry * 12 / 100 * f->s) >> 12) << 4, e > 90, mo > 6 ? ey + ry * 32 * b.sy / 25600 : ey + ry * 3 / 10,
            ry * 15 / 100, mo > 6 ? mo : 0, ry * 18 / 100 + mo / 5, he, 205, lit < 0 ? -1 : 154);
}
/* the kit at (cx, cy) Q4 px, s: a unit's px (Q8); sel: the sound edited (lit), -1 none */
static void vx_kit(const track_t *t, int32_t cx, int32_t cy, int32_t s, int32_t en_home, int32_t sel)
{
    vxf_t f = {cx, cy, s};
    drum_lane_t *K = drum_kit_of(t);
    int32_t e[8], lt[8], en = vx_level(t, 2, en_home), i, k;
    uint32_t ag[8], now = fm1_ms, dt = now - vxs.kms;
    if (dt > 100u) dt = 100u;                            /* (a pause: no jump) */
    vxs.kms = now;
    vxs.kc = 1;
    for (i = 0; i < 8; i++) {                            /* each lane's hit now: its envelope (velocity, accent, decay) */
        int32_t a = K ? K[i].age : 255;
        lt[i] = dx_lane_muted((uint32_t)i) ? -1 : i == sel;
        e[i] = K && K[i].x.live && lt[i] >= 0 && !ANIM_OFF() ? clamp(K[i].x.ea >> 22, 0, 256) : 0;
        if (K && K[i].dm == 2u) e[i] = e[i] * 2 / 3;   /* (GHOST: a small squash) */
        if (K && a < vxs.age[i] && e[i]) {               /* a new hit (a FLAM's two, a ratchet's each) */
            vxs.hitm[i] = now;
            vxs.last = (uint8_t)i;
        }
        vxs.age[i] = (uint8_t)a;
        ag[i] = now - vxs.hitm[i] < 4000u ? now - vxs.hitm[i] : 4000u;
        if (en || e[i])                                  /* (its time runs with the sound: in silence it rests) */
            vxs.tl[i] += dt * (uint32_t)(60 * 256 + e[i] * 300) * 30u / 256000u;
    }
    vx_line(&f, -60 * 16, 40 * 16, 60 * 16, 40 * 16, 26);   /* the floor */
    /* the crash (BELL) on its stem */
    vx_line(&f, 46 * 16, -24 * 16, 48 * 16, 40 * 16, 41);
    vx_bug(&f, 44 * 16, -30 * 16, 16 * 16, 4 * 16, e[VK_BELL], ag[VK_BELL], vxs.tl[VK_BELL] + 3000u, 9, 40, 0, lt[VK_BELL]);
    /* the hi-hat: two shells on their stand, the top one lifting (OH: its mouth open), together on CH */
    {
        int32_t op = e[VK_HATO] > 5 ? 48 + e[VK_HATO] * 80 / 256 : e[VK_HATC] > 5 ? 0 : 24;
        vx_line(&f, -52 * 16, -2 * 16, -52 * 16, 40 * 16, 41);
        vx_bug(&f, -52 * 16, 0, 12 * 16, 54, e[VK_HATC] * 3 / 5, ag[VK_HATC], vxs.tl[VK_HATC], 12, 10, 0, lt[VK_HATC]);
        vx_bug(&f, -52 * 16, -5 * 16 - op, 12 * 16, 54, e[VK_HATC] > e[VK_HATO] ? e[VK_HATC] : e[VK_HATO],
               ag[VK_HATC] < ag[VK_HATO] ? ag[VK_HATC] : ag[VK_HATO], vxs.tl[VK_HATO] + 900u, 12, 10, 0,
               lt[VK_HATO] < 0 && lt[VK_HATC] < 0 ? -1 : lt[VK_HATO] > 0 || lt[VK_HATC] > 0);
    }
    /* the toms (TOM): two on little legs over the kick, the floor tom on longer ones */
    vx_bug(&f, -15 * 16, -14 * 16, 9 * 16, 7 * 16, e[VK_TOM], ag[VK_TOM], vxs.tl[VK_TOM] + 200u, 5, 20, 6 * 16, lt[VK_TOM]);
    vx_bug(&f, 15 * 16, -14 * 16, 9 * 16, 7 * 16, e[VK_TOM] * 3 / 4, ag[VK_TOM] + 30u, vxs.tl[VK_TOM] + 400u, 5, 20, 6 * 16, lt[VK_TOM]);
    vx_bug(&f, 40 * 16, 18 * 16, 11 * 16, 10 * 16, e[VK_TOM] / 2, ag[VK_TOM] + 60u, vxs.tl[VK_TOM] + 600u, 5, 20, 12 * 16, lt[VK_TOM]);
    /* the kick, large in the middle; the snare flat on its stand */
    vx_bug(&f, 0, 16 * 16, 22 * 16, 20 * 16, e[VK_KICK], ag[VK_KICK], vxs.tl[VK_KICK] + 1000u, 4, 14, 4 * 16, lt[VK_KICK]);
    vx_bug(&f, -34 * 16, 10 * 16, 13 * 16, 6 * 16, e[VK_SNARE], ag[VK_SNARE], vxs.tl[VK_SNARE] + 1400u, 14, 12, 22 * 16, lt[VK_SNARE]);
    /* the rim (RS): a spark on the snare's edge, its rays turning as it rings */
    if (e[VK_RIM] > 5 || lt[VK_RIM] > 0)
        for (k = 0; k < 6; k++) {
            uint32_t a = (uint32_t)k * 10923u + ag[VK_RIM] * 63u;
            int32_t r1 = 3 * 16 + e[VK_RIM] * 4 * 16 / 256;
            vx_line(&f, -47 * 16 + ((32 * og_cos(a)) >> 15), 8 * 16 + ((32 * og_sin(a)) >> 15), -47 * 16 + ((r1 * og_cos(a)) >> 15),
                    8 * 16 + ((r1 * og_sin(a)) >> 15), 154 + e[VK_RIM] * 102 / 256);
        }
    og_node(cx + ((-47 * 16 * s) >> 8), cy + ((8 * 16 * s) >> 8), 1, vx_col(lt[VK_RIM] < 0 ? -1 : 102 + e[VK_RIM] * 154 / 256));
    /* the cowbell (BELL): a small head over the kick */
    vx_bug(&f, 0, -30 * 16, 6 * 16, 5 * 16, e[VK_BELL], ag[VK_BELL], vxs.tl[VK_BELL] + 1800u, 3, 30, 0, lt[VK_BELL]);
    /* the clap: two little ones, apart, clapping together */
    {
        int32_t g = 8 * 16 - e[VK_CLAP] * 6 * 16 / 256;
        vx_bug(&f, -27 * 16 - g, -33 * 16, 4 * 16, 4 * 16, e[VK_CLAP], ag[VK_CLAP], vxs.tl[VK_CLAP] + 2200u, 5, 20, 0, lt[VK_CLAP]);
        vx_bug(&f, -27 * 16 + g, -33 * 16, 4 * 16, 4 * 16, e[VK_CLAP], ag[VK_CLAP], vxs.tl[VK_CLAP] + 2600u, 5, 20, 0, lt[VK_CLAP]);
    }
    /* a FLAM's hit to come: the being's shadow, doubled */
    for (i = 1; i < 8; i++)
        if (K && K[i].fl) {
            static const int8_t PX[8] = {0, -34, -27, -52, -52, -15, -47, 44}, PY[8] = {16, 10, -33, 0, -8, -14, 8, -30};
            vx_arc(&f, PX[i] * 16 + 2 * 16, PY[i] * 16 + 16, 6 * 16, 3 * 16, 0, 65536u, 12, 64);
        }
    vxs.kc = 0;
}

/* ============================================================================ (0.9.6) the mock-up's family === */
/* the mock-up's frame (uFrame): its unit space 124 x 94 fitted in the rectangle A (x y w h px), k / 100 larger, lifted
 * dy (Q4 units) */
static vxf_t vx_uf(const int32_t *A, int32_t k, int32_t dy)
{
    vxf_t f;
    int32_t a = A[2] * 256 / 124, b = A[3] * 256 / 94;
    f.s = (a < b ? a : b) * k / 100;
    f.cx = (A[0] * 2 + A[2]) * 8;
    f.cy = (A[1] * 2 + A[3]) * 8 - ((dy * f.s) >> 8);
    return f;
}
/* the angle (16-bit turns) of t * r radians, t the mock-up's frames (mt, Q8), r = r1000 / 1000 */
static uint32_t vx_ang(uint32_t mt, int32_t r1000)
{
    return mt * (uint32_t)r1000 * 2670u >> 16;          /* (mod 2^32: an angle's 16 bits exact) */
}
static int32_t vx_hash(uint32_t n) { n = (n ^ 61u) ^ (n >> 16); n *= 9u; n ^= n >> 4; n *= 668265261u; return (int32_t)((n ^ (n >> 15)) & 255u); }
/* the squash of a hit (Q8): e (0..256) at age ms (22 rad/s, e^-6t) */
static int32_t vx_sq(int32_t e, uint32_t age)
{
    return (((e * og_cos(age * 229u)) >> 15) * vx_fall(age, 115u)) >> 8;
}
/* ms since track t's newest note (k: which creature saw it; 4000 at most); *note: that note */
static uint32_t vx_age(const track_t *t, uint32_t k, int32_t *note)
{
    uint32_t i, a = 0, n = 60;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].age > a) { a = t->v[i].age; n = t->v[i].note; }
    if (a > vxs.nage[k])
        vxs.nms[k] = fm1_ms;
    vxs.nage[k] = a;
    if (note) *note = (int32_t)n;
    return fm1_ms - vxs.nms[k] < 4000u ? fm1_ms - vxs.nms[k] : 4000u;
}
/* a polyline of n points (Q4 units: x y pairs), closed or not */
static void vx_poly(const vxf_t *f, const int32_t *p, uint32_t n, int closed, int32_t h)
{
    uint32_t i;
    for (i = 1; i < n; i++)
        vx_line(f, p[2u * i - 2u], p[2u * i - 1u], p[2u * i], p[2u * i + 1u], h);
    if (closed)
        vx_line(f, p[2u * n - 2u], p[2u * n - 1u], p[0], p[1], h);
}
/* a line between two points in polar form about c (cx cy, x y scales / 256): angles (16-bit turns), radii (Q4 units) */
static void vx_pl(const vxf_t *f, const int32_t *c, uint32_t a0, int32_t r0, uint32_t a1, int32_t r1, int32_t h)
{
    vx_line(f, c[0] + (((og_cos(a0) * r0) >> 15) * c[2] >> 8), c[1] + (((og_sin(a0) * r0) >> 15) * c[3] >> 8),
            c[0] + (((og_cos(a1) * r1) >> 15) * c[2] >> 8), c[1] + (((og_sin(a1) * r1) >> 15) * c[3] >> 8), h);
}
/* a ring (Q4 units), its sides as many as its size asks */
static void vx_ring(const vxf_t *f, int32_t x, int32_t y, int32_t rx, int32_t ry, int32_t h)
{
    uint32_t n = (uint32_t)clamp(((rx + ry) * f->s) >> 12, 8, 28);
    vx_arc(f, x, y, rx, ry, 0, 65536u, n, h);
}
/* a bicho's body (isoU): its outer outline (the spikes) of radius r, its inner one ri (0: none); g its features (spikes,
 * their length, wobble, ripple, lobes, sharpness, x stretch) and its place (x, by, squashed sx sy about it) */
static void vx_body(const vxf_t *f, const vxb_t *g, int32_t r, int32_t ri, uint32_t t, int32_t ho, int32_t hi, uint32_t no, uint32_t ni)
{
    if (sys_load) {                                      /* (LITE, ECO) */
        no = no * (sys_load == LOAD_ECO ? 1u : 2u) / (sys_load == LOAD_ECO ? 2u : 3u);
        ni = ni * (sys_load == LOAD_ECO ? 1u : 2u) / (sys_load == LOAD_ECO ? 2u : 3u);
    }
    vx_blob(f, g, r, 1, t, no, ho);
    if (ri) vx_blob(f, g, ri, 0, t, ni, hi);
}
/* text s centred on (x, its capitals' top y) Q4 px */
static uint8_t vx_small;                                 /* (V10.8) LARGE's strip: the creatures small, no labels */
static void vx_text(int32_t x, int32_t y, const char *s, uint16_t c)
{
    if (vx_small) return;
    cv_text((x >> 4) - text_w(&AF_X, s) / 2, (y >> 4) - AF_X_CAP_Y, &AF_X, s, c);
}

/* ------------------------------------------------------------------------------------------- ANALOG --- */
/* 2^(l / 256), Q12 */
static int32_t vx_x2(int32_t l)
{
    int32_t i = l >> 8, f = l & 255, m = 4096 + ((f * (2690 + ((f * 1407) >> 8))) >> 8);
    return i >= 0 ? m << i : i > -31 ? m >> -i : 0;
}
/* a wave of ANALOG's at phase q (16 bit): SAW SQR TRI SIN */
static int32_t vx_anb(uint32_t w, uint32_t q)
{
    q &= 65535u;
    w &= 3u;
    return w == 0u ? 32767 - (int32_t)q : w == 1u ? (q < 32768u ? 32767 : -32767) :
           w == 2u ? (q < 32768u ? (int32_t)q * 2 - 32768 : 98303 - (int32_t)q * 2) : og_sin(q);
}
/* two cycles (64 samples each) of ANALOG's oscillators, noise, the filter (CUT on the note, the envelope opening it a
 * little, RES) and DRIVE (tanh), as the mock-up's anTick: into vxs.wv, normalised (Q12) */
static VX_CODE void vx_an_wave(const track_t *t, int32_t en, uint32_t mt, int32_t note)
{
    const int16_t *p = t->p;
    uint32_t w = (uint32_t)p[P_E0] & 7u, i, rep, r = 4096, rs = (mt >> 8) * 2654435761u + 1u;
    int32_t det = p[P_E1], mix = (p[P_E2] * 516 >> 8), nz = (p[P_E3] * 361 >> 8), mo = clamp(t->mo_cur / 127, 0, 256), k;
    int32_t pos = (int32_t)w * 256 + mo * 3, fr = pos & 255, i0 = pos >> 8, g, a1, a2, a3, ic1 = 0, ic2 = 0, mx = 1, dg;
    int32_t r5 = 256 + det * 2022 / 127 * (256 - mo / 2) / 256 + mo * 3, d3 = det * 218 * (256 + 4 * mo) >> 8;
    uint32_t dr = mt * (uint32_t)det * 2642u >> 16, inc2, wd;
    for (k = p[P_E7]; k > 0; k--) r = (r * 4340u) >> 12;   /* INT: osc 2 semitones away, DETUNE cents */
    for (k = p[P_E7]; k < 0; k++) r = (r * 3866u) >> 12;
    r += r * (uint32_t)det / 1200u;
    inc2 = 1024u * r >> 12;
    wd = (uint32_t)(32768 - (mo * 27525 >> 8) - ((og_sin(vx_ang(mt, 50)) * 3932) >> 15));   /* PWM's width */
    g = vx_x2(-992 - 1536 + ((p[P_E4] * 4851) >> 8) - (note - 69) * 256 / 12 + (en * 82 >> 8));   /* CUT / the note / 64 */
    g = clamp(g, 4, 1843);
    g = (g * 12868) >> 12;                               /* (tan (pi fc)) */
    g = clamp(g + (((((g * g) >> 12) * g) >> 12) / 3), 1, 12288);
    k = 8192 - (p[P_E5] * 16017 >> 8);
    a1 = (4096 * 4096) / (4096 + ((g * (g + k)) >> 12));
    a2 = (g * a1) >> 12;
    a3 = (g * a2) >> 12;
    dg = 256 + (p[P_E6] * 3612 >> 8);
    for (rep = 0; rep < 2u; rep++)                       /* (a cycle for the filter to settle, then the two) */
        for (i = rep ? 0u : 64u; i < 128u; i++) {
            uint32_t P = i * 1024u, q = P & 65535u, q2 = (i * inc2 + dr) & 65535u;
            int32_t s, v1, v2, v3, u;
            if (w < 4u) {
                int32_t x1 = (vx_anb((uint32_t)i0, q) * (256 - fr) + vx_anb((uint32_t)i0 + 1u, q) * fr) >> 8;
                int32_t x2 = (vx_anb((uint32_t)i0, q2) * (256 - fr) + vx_anb((uint32_t)i0 + 1u, q2) * fr) >> 8;
                s = (x1 * (256 - mix / 2) + x2 * (mix / 2)) >> 8;
            } else if (w == 4u)
                s = ((q < wd ? 32767 : -32767) * (256 - mix / 2) + (q2 < wd ? 32767 : -32767) * (mix / 2)) >> 8;
            else if (w == 5u)
                s = (vx_anb(0, q) * (256 - mix) + vx_anb(0, (q * (uint32_t)r5) >> 8) * mix) >> 8;
            else if (w == 6u)
                s = (vx_anb(0, q) * (256 - mix) + ((vx_anb(0, q) * og_sin((P * (uint32_t)r5) >> 8)) >> 15) * mix) >> 8;
            else
                s = (vx_anb(0, P) + vx_anb(0, P + (((P >> 4) * (uint32_t)d3) >> 12) + dr) +
                     vx_anb(0, P - (((P >> 4) * (uint32_t)d3) >> 12) - dr * 13u / 10u)) / 3;
            rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
            s = (s + ((((int32_t)(rs >> 16) - 32768) * nz) >> 7)) >> 3;   /* (NOISE) Q12 */
            v3 = s - ic2;
            v1 = (a1 * ic1 + a2 * v3) >> 12;
            v2 = ic2 + ((a2 * ic1 + a3 * v3) >> 12);
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            if (!rep) continue;
            u = clamp(((v2 * dg) >> 8) >> 4, -768, 768);   /* DRIVE: tanh, Q8 */
            u = u * (1769472 + u * u) / (1769472 + 9 * u * u);
            vxs.wv[i] = (int16_t)(u << 4);
            mx = u << 4 > mx ? u << 4 : -(u << 4) > mx ? -(u << 4) : mx;
        }
    if (mx > 205)
        for (i = 0; i < 128u; i++) vxs.wv[i] = (int16_t)(vxs.wv[i] * 4096 / mx);
}
/* CÉLULA */
static VX_CODE void vx_analog(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    int32_t en = vx_level(t, 3, en_home), note, j, i, cy = 32;
    uint32_t mt = vxs.mt[3], age = vx_age(t, 3, &note), a0 = vx_ang(mt, 4), a1 = vx_ang(mt, 100);
    vxf_t f = vx_uf(A, 112, 32);
    int32_t dr = (p[P_E6] * 516 >> 8), cut = (p[P_E4] * 516 >> 8), res = (p[P_E5] * 516 >> 8), hb = (dr * 56 >> 8), nz = (p[P_E3] * 516 >> 8);
    int32_t sq = vx_sq(102 + (en * 154 >> 8), age), R = 432 + (en * 52 >> 8), am = 72 + (en * 64 >> 8);
    int32_t sx = 256 + (sq * 56 >> 8), sy = 256 - (sq * 56 >> 8), rc, beat, det = p[P_E1], c[4] = {0, cy, sx, sy};
    vxs.kc = 1;
    vx_an_wave(t, en, mt, note);
    for (j = 0; j < 3; j++) {                            /* the membrane: three isotherms, the outer cold */
        static const int16_t SC[3] = {256, 184, 118};
        uint32_t pa = 0, n = 96u - (uint32_t)j * 24u;     /* (the inner ones in fewer lines) */
        int32_t pr = 0;
        for (i = 0; i <= (int32_t)n; i++) {
            uint32_t th = (uint32_t)i % n * 65536u / n + a0;
            int32_t r = (R * SC[j] >> 8) + ((vxs.wv[(uint32_t)i % n * 256u / n & 127u] * am) >> 12) * SC[j] / 256 * (j ? 205 : 256) / 256 +
                        ((og_sin(3u * th + a1) * 10) >> 15);
            if (i) vx_pl(&f, c, pa, pr, th, r, 36 + j * 54 + (en * 61 >> 8) + hb);
            pa = th; pr = r;
        }
    }
    for (i = 0; dr > 26 && i < 96; i += 2) {             /* DRIVE: spikes where the wave clips */
        int32_t s = vxs.wv[i * 8 / 3 & 127], r = R + ((s * am) >> 12);
        if (s > 3686 || s < -3686)
            vx_pl(&f, c, (uint32_t)i * 65536u / 96u + a0, r, (uint32_t)i * 65536u / 96u + a0, r + 32 + dr / 4, 179 + (dr * 77 >> 8));
    }
    rc = R * (56 + (cut * 141 >> 8)) / 256;               /* RES: a ring at CUT, glowing and spreading */
    if (res > 10) {
        vx_ring(&f, 0, cy, rc * sx >> 8, rc * sy >> 8, 102 + (res * 154 >> 8));
        if (res > 154) vx_ring(&f, 0, cy, (rc + 6) * sx >> 8, (rc + 6) * sy >> 8, 102 + (res * 154 >> 8));
        for (i = 0; i < 2; i++) {
            int32_t ph = (int32_t)((mt * (uint32_t)(256 + res) / 12800u + (uint32_t)i * 128u) & 255u);
            vxs.dal = (int16_t)(256 - (256 - ph) * res / 256);
            vx_ring(&f, 0, cy, (rc + ph / 2) * sx >> 8, (rc + ph / 2) * sy >> 8, 154 + (res * 102 >> 8));
        }
        vxs.dal = 0;
    }
    for (i = 0; nz > 8 && i < 48; i++)                   /* NOISE: cilia */
        vx_pl(&f, c, (uint32_t)i * 65536u / 48u, R + am + 16, (uint32_t)i * 65536u / 48u + 1043u,
              R + am + 32 + nz * vx_hash((uint32_t)i + (mt >> 8)) / 1024, 64 + (nz * 77 >> 8));
    beat = (int32_t)vx_ang(mt, 10 + (det * 202 >> 8));    /* the two oscillators, orbiting at the beat */
    for (i = 0; i < 2; i++) {
        uint32_t a = i ? 32768u - (uint32_t)beat : (uint32_t)beat;
        int32_t sep = 32 + (det * 258 >> 8), r = i ? 24 + (p[P_E2] * 71 >> 8) : 42;
        vx_ring(&f, (og_cos(a) * sep) >> 15, cy + 144 + ((og_sin(a) * sep * 2 / 5) >> 15), r, r, 200 + en / 5);
    }
    vx_bface(&f, 0, cy - 80, 240, (en * 294 >> 8), age < 90u, dr);
    vxs.kc = 0;
}

/* ---------------------------------------------------------------------------------------------- FM6 --- */
#ifndef VX_FM6_HOOK
#define VX_FM6_HOOK(k, x, y, r) ((void)0)               /* ui_render.c (host tests): each bicho as drawn (its lint) */
#endif
static const uint8_t FM6_CELL[32][6];                    /* (ui_graph.c) */
static void fm6_routes(uint32_t a, uint8_t *m);
static uint32_t fm6_fb_op(uint32_t a);
static uint32_t fm6_car_ops(uint32_t a);
static uint32_t fm6_alg_of(const track_t *t);
static void fop_freq(const uint8_t *op, char *s);
typedef struct {
    const uint8_t *p;                                    /* the patch as it plays (the macros on it) */
    int32_t env[6], amp[6], rat[6], rad[6], x[6], y[6];  /* per operator 1..6: its envelope now, level x envelope (0..256),
                                                          * ratio (Q8), radius, place (Q4 units) */
    uint32_t alg, car, fbo, st[6];                       /* the algorithm, its carriers, the feedback operator; each stage */
    uint8_t m[6];
} vxfm_t;
/* the operators of track t now: the sounding notes' envelopes (the loudest), each one's level, ratio, size, place */
static void vx_fm_ops(const track_t *t, vxfm_t *o)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK, k, v, hi = 0, top = 0;
    o->p = fm6_eff[tr].ok ? fm6_eff[tr].p : fm6_patch[tr];
    o->alg = fm6_alg_of(t);
    o->car = fm6_car_ops(o->alg);
    o->fbo = fm6_fb_op(o->alg);
    fm6_routes(o->alg, o->m);
    for (k = 0; k < 6u; k++) {
        const uint8_t *op = o->p + (5u - k) * FP_OP;
        int32_t e = 0, ol = op[FP_OL] % 100u, a = ol * 256 / 99, c = op[FP_FC] & 31;
        o->st[k] = 4;
        for (v = 0; v < FM6_POLY; v++) {                 /* (msfa's level: scaleout units 0..127, the stage's L 0..99) */
            const fm6_env_t *n = &fm6_note[tr][v].env[5u - k];
            int32_t s = (((n->level >> 16) - n->outlevel + 4256) / 32 - 28) * 256 / 99;
            if (!fm6_note[tr][v].live || !t->v[v].active) continue;
            if (s > e || o->st[k] == 4u) { o->st[k] = n->ix | (n->down ? 0u : 8u); }
            e = s > e ? s : e;
        }
        o->env[k] = clamp(e, 0, 256);
        o->amp[k] = ol ? (a * ((a * 154 >> 8) + 102) / 256) * o->env[k] / 256 : 0;   /* ((ol / 99) ^ 1.6) x env */
        o->rat[k] = op[FP_MODE] ? 256 : (c ? c * 256 : 128) * (100 + op[FP_FF] % 100u) / 100;
        o->rad[k] = 120 * (141 + 141 * ol / 99) / 256 * (200 + (o->env[k] * 82 >> 8)) / 256;
        hi = (FM6_CELL[o->alg][k] & 7u) > hi ? FM6_CELL[o->alg][k] & 7u : hi;
        top = (uint32_t)(FM6_CELL[o->alg][k] >> 4) > top ? (uint32_t)FM6_CELL[o->alg][k] >> 4 : top;
    }
    for (k = 0; k < 6u; k++) {                           /* (fmPos: columns 26 apart, the carriers on the floor 38) */
        o->x[k] = ((int32_t)(FM6_CELL[o->alg][k] & 7u) * 2 - (int32_t)hi) * 13 * 16;
        o->y[k] = 28 * 16 - (int32_t)(FM6_CELL[o->alg][k] >> 4) * (top >= 3u ? 324 : 352);
    }
}
/* operator k's bicho at (x, y): spikes its ratio, size and heat its level now, breathing with its envelope; sel: the
 * operator picked (the accent); sh: DETUNE's shimmer (Q4 units) */
static VX_CODE void vx_fm_bicho(const vxf_t *f, const vxfm_t *o, uint32_t k, int32_t x, int32_t y, int32_t r, uint32_t age, uint32_t mt,
                        int sel, int32_t sh)
{
    int32_t a = o->amp[k], car = (int32_t)(o->car >> k & 1u), sp = clamp((o->rat[k] + 128) >> 8, 1, 9), d;
    int32_t sq = vx_sq(car ? a : a * 3 / 5, age), sx = 256 + (sq * 77 >> 8), sy = 256 - (sq * 82 >> 8), by = y + r * (256 - sy) / 256;
    uint32_t tt = (mt * (uint32_t)(60 + (a * 260 >> 8)) >> 8) + k * 3000u;
    int32_t oc = sel ? -2 : 41 + (a * 102 >> 8);
    vxb_t g = {x, y, by, 256, sx, sy, sp, 12 + (a * 50 >> 8), 40 + (a * 110 >> 8), (a * 80 >> 8), 3, 4};
    if (!(o->p[(5u - k) * FP_OP + FP_OL] % 100u)) {      /* (silent: a cold outline) */
        g.by = y; g.sx = g.sy = 256;
        vx_body(f, &g, r, 0, tt, 20, 0, 40, 0);
        return;
    }
    r = r * (256 + ((og_sin(vx_ang(mt, 220 * (o->rat[k] < 12 * 256 ? o->rat[k] : 12 * 256) / 256)) * 15 >> 15) * a >> 8)) / 256;
    if (sh) {                                            /* (DETUNE: a shimmer, two faint copies) */
        vxs.dal = 102;
        for (d = -1; d <= 1; d += 2) {
            g.x = x + d * sh;
            vx_body(f, &g, r, 0, tt, 51 + (a * 77 >> 8), 0, 48, 0);
        }
        vxs.dal = 0;
        g.x = x;
    }
    vx_body(f, &g, r, (r * 159 >> 8), tt, oc, sel ? -2 : 87 + (a * 128 >> 8), 48, 20);
    vx_bface(f, x, by, r, car ? a * 8 / 5 : a * 3 / 5, age < 90u, 0);
}
/* the operator's number by its bicho */
static void vx_fm_num(const vxf_t *f, const vxfm_t *o, uint32_t k, int32_t x, int32_t y, int sel)
{
    char n[2] = {(char)('1' + k), 0};
    vx_text(f->cx + ((x * f->s) >> 8), f->cy + ((y * f->s) >> 8) - 64, n,
            sel ? T_THEME : o->p[(5u - k) * FP_OP + FP_OL] % 100u ? T_MID : T_DIM);
}
/* COLONIA: the operators wired as the algorithm, the carriers standing on the output; sel the operator picked (or 6),
 * op2: OPERATOR 2 (its DETUNE a shimmer) */
static VX_CODE void vx_fm6(const track_t *t, const int32_t *A, int32_t en_home, uint32_t sel, int op2)
{
    vxfm_t o;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t en = vx_level(t, 4, en_home), x0 = 9999, x1 = -9999, y0 = 9999, s, l = 9999, r = -9999, k, j, fb;
    uint32_t mt = vxs.mt[4], age = vx_age(t, 4, 0);
    vxs.kc = 1;
    vx_fm_ops(t, &o);
    for (k = 0; k < 6; k++) {
        x0 = o.x[k] < x0 ? o.x[k] : x0; x1 = o.x[k] > x1 ? o.x[k] : x1; y0 = o.y[k] < y0 ? o.y[k] : y0;
        if (o.car >> k & 1u) { l = o.x[k] < l ? o.x[k] : l; r = o.x[k] > r ? o.x[k] : r; }
    }
    x0 -= 16 * 16; x1 += 18 * 16; y0 -= 13 * 16;         /* (fitTo: the colony fills the frame, at most 1.75 x) */
    s = f.s * 124 * 16 / (x1 - x0);
    j = f.s * 94 * 16 / (47 * 16 - y0);
    s = s < j ? s : j;
    s = s < f.s * 7 / 4 ? s : f.s * 7 / 4;
    f.cx -= ((x0 + x1) / 2 * s) >> 8;
    f.cy -= ((y0 + 47 * 16) / 2 * s) >> 8;
    f.s = s;
    l = (l < -160 ? l : -160) - 128;                    /* the output: the floor, an arrow out */
    r = (r > 160 ? r : 160) + 192;
    vx_line(&f, l, 38 * 16, r, 38 * 16, 36 + en / 2);
    vx_line(&f, r - 48, 36 * 16, r, 38 * 16, 77 + en / 2);
    vx_line(&f, r - 48, 40 * 16, r, 38 * 16, 77 + en / 2);
    for (k = 0; k < 6; k++)                              /* the wires: down, across, down; pulses as fast as the ratio */
        for (j = 0; j < 6; j++)
            if (o.m[k] >> j & 1u) {
                int32_t a = o.amp[j], ya = o.y[j] + o.rad[j], yb = o.y[k] - o.rad[k], my = (ya + yb) / 2, q[8];
                q[0] = o.x[j]; q[1] = ya; q[2] = o.x[j]; q[3] = my; q[4] = o.x[k]; q[5] = my; q[6] = o.x[k]; q[7] = yb;
                vx_curve(&f, q, 4, 0, 31 + (a * 154 >> 8));
                if (a > 8) {                             /* (two pulses along it, as fast as the modulator's ratio) */
                    int32_t n, dx = o.x[k] > o.x[j] ? o.x[k] - o.x[j] : o.x[j] - o.x[k], len = (my - ya) + dx + (yb - my);
                    int32_t rr = f.s * 7 >> 8, mr = o.rat[j] < 14 * 256 ? o.rat[j] : 14 * 256;
                    for (n = 0; n < 2; n++) {
                        int32_t d = (int32_t)(((mt * (uint32_t)(256 + mr / 4) * 786u >> 16) + (uint32_t)n * 128u) & 255u) * len / 256;
                        if (d < my - ya) vx_dot(&f, o.x[j], ya + d, rr, 154 + (a * 102 >> 8));
                        else if ((d -= my - ya) < dx) vx_dot(&f, o.x[j] + (o.x[k] > o.x[j] ? d : -d), my, rr, 154 + (a * 102 >> 8));
                        else vx_dot(&f, o.x[k], my + d - dx, rr, 154 + (a * 102 >> 8));
                    }
                }
            }
    for (k = 0; k < 6; k++)                              /* the carriers' legs, on the floor */
        if (o.car >> k & 1u)
            for (j = -1; j <= 1; j += 2)
                vx_line(&f, o.x[k] + (j * o.rad[k] * 115 >> 8), o.y[k] + (o.rad[k] * 179 >> 8), o.x[k] + (j * o.rad[k] * 154 >> 8), 38 * 16,
                        51 + (o.amp[k] * 77 >> 8));
    fb = clamp(o.p[FP_FB] + t->p[P_E1], 0, 7);           /* FB: a curled tail */
    {
        int32_t X = o.x[o.fbo], Y = o.y[o.fbo], rd = o.rad[o.fbo], q[10];
        q[0] = X + (rd * 205 >> 8); q[1] = Y - (rd * 102 >> 8);
        q[2] = X + rd + 48 + fb * 48 / 7; q[3] = Y - rd - 32 - fb * 32 / 7;
        q[4] = X + rd + 96 + fb * 64 / 7; q[5] = Y + ((og_sin(vx_ang(mt, 200)) * fb * 32 / 7) >> 15);
        q[6] = X + rd + 32; q[7] = Y + (rd * 179 >> 8);
        q[8] = X + (rd * 179 >> 8); q[9] = Y + rd / 4;
        vx_curve(&f, q, 5, 0, fb ? 64 + fb * 154 / 7 : 20);
        if (fb) vx_dot(&f, q[4], q[5], f.s >> 5, 154 + fb * 102 / 7);
    }
    for (k = 0; k < 6; k++) {
        const uint8_t *op = o.p + (5u - (uint32_t)k) * FP_OP;
        int32_t sh = op2 && (uint32_t)k == sel ? (op[FP_DET] > 7u ? op[FP_DET] - 7 : 7 - op[FP_DET]) * 8 : 0;
        vx_fm_bicho(&f, &o, (uint32_t)k, o.x[k], o.y[k], o.rad[k], age, mt, (uint32_t)k == sel, sh);
        if (A[2] >= 80)                                  /* (V10.8: small, LARGE's strip, the numbers left out) */
            vx_fm_num(&f, &o, (uint32_t)k, o.x[k] - o.rad[k] - 64, o.y[k] - o.rad[k] + 16, (uint32_t)k == sel);
        VX_FM6_HOOK(k, f.cx + ((o.x[k] * f.s) >> 8), f.cy + ((o.y[k] * f.s) >> 8), (145 * f.s) >> 8);   /* (the largest it gets) */
    }
    vxs.kc = 0;
}
/* OPERATOR / OPERATOR 2: the colony on the whole panel, the operator picked lit; "ALG 05" over it on the left, its
 * frequency on the right ("OP3 x1.50") */
static VX_CODE void vx_fm6_page(const track_t *t, uint32_t sel, int op2)
{
    static const int32_t A[4] = {4, 14, 232, 124};
    const uint8_t *pt = fm6_patch[(t - trk) % NTRK];
    uint32_t alg = fm6_alg_of(t);
    uint32_t hot = ui.hot_t ? (uint32_t)cur_page()->id[ui.hot_col & 3u] : 0u;
    char b[16] = {'A', 'L', 'G', ' ', (char)('0' + (alg + 1u) / 10u), (char)('0' + (alg + 1u) % 10u), 0};
    char fq[16] = {'O', 'P', (char)('1' + sel), ' ', 0};
    cv_oy = 0;
    vx_fm6(t, A, -1, sel, op2);
    if (!t->p[P_E0])
        str_cpy(b + 6, " PAT", 5);
    cv_text(10, 4, &AF_S, b, hot == P_E0 || hot == P_E7 ? T_ACCENT : T_MID);
    fop_freq(pt + (5u - sel) * FP_OP, fq + 4);
    cv_text_r(228, 4, &AF_S, fq, T_TEXT, T_SURF);
}
/* OP ENV: the picked operator's envelope as the patch has it (from L4 to L1 at R1, L2 at R2, L3 at R3, held while the key
 * is down: a dashed line where it is let go, back to L4 at R4; a stage as wide as its rate is slow), the stage KNOB 3 / 4
 * edit in the accent, its bicho riding the curve where the operator is now; "OP3" top left, the stage's R and L right */
static VX_CODE void vx_fm6env(const track_t *t)
{
    static const char *const STG[4] = {"ATK", "DEC", "SUS", "REL"};
    static const int32_t A[4] = {4, 12, 232, 126};
    const uint8_t *op = fop_bytes((uint32_t)(t - trk));
    vxfm_t o;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t x[6], y[6], k, i, en = vx_level(t, 4, -1), px, py, e, a, r, u, sg = fop_stg & 3u;
    uint32_t mt = vxs.mt[4], age = vx_age(t, 4, 0), ko = fop_op % 6u, st;
    char b[24] = {'O', 'P', (char)('1' + ko), 0};
    (void)en;
    cv_oy = 0;
    vxs.kc = 1;
    vx_fm_ops(t, &o);
#define VFY(l) (22 * 16 - (int32_t)(l) * 42 * 16 / 99)
#define VFW(r) ((4 + (99 - (int32_t)(r)) * (99 - (int32_t)(r)) * 44 / 9801) * 112 * 16 / 216)
    x[0] = -56 * 16;
    y[0] = VFY(op[FP_L1 + 3]);
    for (k = 0; k < 3; k++) {
        x[k + 1] = x[k] + VFW(op[FP_R1 + k]);
        y[k + 1] = VFY(op[FP_L1 + k]);
    }
    x[5] = 56 * 16;
    x[4] = x[5] - VFW(op[FP_R1 + 3]);
    y[4] = y[3];
    y[5] = y[0];
    vx_line(&f, -60 * 16, 26 * 16, 60 * 16, 26 * 16, 26);
    for (i = -20 * 16; i < 22 * 16; i += 48)             /* the key let go */
        vx_line(&f, x[4], i, x[4], i + 22, 31);
    for (k = 0; k < 5; k++)                              /* (3: the hold, no stage of its own) */
        for (i = 0; i < 10; i++) {
            int32_t on = k != 3 && (k < 3 ? k : 3) == sg, dx = x[k + 1] - x[k], dy = y[k + 1] - y[k];
            int32_t ya = y[k] + dy * i / 10, d;
            for (d = 0; d <= on * 12; d += 12)
                vx_line(&f, x[k] + dx * i / 10, ya + d, x[k] + dx * (i + 1) / 10, y[k] + dy * (i + 1) / 10 + d,
                        on ? -2 : 38 + ((22 * 16 - ya) * 154 / (42 * 16)));
        }
    k = sg < 3 ? sg + 1 : 5;
    vx_ring(&f, x[k], y[k], 32, 32, -2);
    vx_ring(&f, x[k], y[k], 40, 40, -2);
    e = o.env[ko];                                       /* where the operator is now: on its stage, as far as its level */
    st = o.st[ko];                                       /* came (held: along the hold, 1.5 s across) */
    py = VFY(e * 99 / 256);
    k = (st & 7u) >= 4u ? 5 : st == 3u ? 3 : st < 3u ? (int32_t)st : 4;
    if (k == 3) {
        if (!vxs.fhold) vxs.fhold = fm1_ms | 1u;
        u = clamp((int32_t)(fm1_ms - vxs.fhold) * 256 / 1500, 0, 256);
    } else {
        int32_t l0 = k == 4 ? vxs.frel * 99 / 256 : op[FP_L1 + ((k + 3) & 3)], l1 = op[FP_L1 + (k < 4 ? k : 3)];
        vxs.fhold = 0;
        u = l1 != l0 ? clamp((e * 99 / 256 - l0) * 256 / (l1 - l0), 0, 256) : 256;
    }
    if (k == 5) { k = 4; u = 256; }
    if (k != 4) vxs.frel = (int16_t)e;                   /* (the level its release sets out from) */
    px = x[k] + (x[k + 1] - x[k]) * u / 256;
    a = o.amp[ko];
    r = 64 + (96 * e >> 8);
    {
        int32_t sp = clamp((o.rat[ko] + 128) >> 8, 1, 9);
        uint32_t tt = mt * 90u >> 8;
        vxb_t g = {px, 0, py - r, 256, 256, 256, sp, 12 + (a * 50 >> 8), 40 + (a * 110 >> 8), (a * 80 >> 8), 3, 4};
        vx_body(&f, &g, r, (r * 159 >> 8), tt, -2, 90 + a / 2, 48, 28);
        vx_bface(&f, px, py - r, r, a * 7 / 5, age < 90u, 0);
    }
    k = sg < 3 ? (x[sg] + x[sg + 1]) / 2 : (x[4] + x[5]) / 2;
    k = clamp(k, -49 * 16, 49 * 16);                     /* (the stage's name under it, inside the curve's ends) */
    vx_text(f.cx + ((k * f.s) >> 8),
            f.cy + ((31 * 16 * f.s) >> 8) - 112, STG[sg], T_THEME);
#undef VFY
#undef VFW
    vxs.kc = 0;
    cv_text(10, 4, &AF_S, b, T_MID);
    str_cpy(b, STG[sg], sizeof b);
    str_cpy(b + str_len(b), "  R ", 5);
    fmt_int(b + str_len(b), op[FP_R1 + sg]);
    str_cpy(b + str_len(b), "  L ", 5);
    fmt_int(b + str_len(b), op[FP_L1 + sg]);
    cv_text_r(228, 4, &AF_S, b, T_TEXT, T_SURF);
}

/* -------------------------------------------------------------------------------------------- WHEEL --- */
static const uint8_t VW_RAT[9] = {8, 24, 16, 32, 48, 64, 80, 96, 128};   /* the bars' harmonics, x16 */
/* TUBÍCOLAS */
static VX_CODE void vx_wheel(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    const drw_trk_t *T = &drw_t[drw_part(t)];
    vxf_t f = vx_uf(A, 100, 0);
    uint32_t n0 = vxs.nms[5], mt = vxs.mt[5], i, j;
    int32_t en = vx_level(t, 5, en_home), dr = (p[P_E6] * 516 >> 8), hb = (dr * 56 >> 8), perc = 0, pk, dep, click = (p[P_E5] * 516 >> 8);
    uint32_t age = vx_age(t, 5, 0), pc = (uint32_t)p[P_E4] % 7u;
    vxs.kc = 1;
    if (en) { vxs.rph[0] = T->rph[0]; vxs.rph[1] = T->rph[1]; }   /* (the rotors as they turn while it sounds) */
    dep = (clamp(p[P_WRDEP], 0, 127) + 1) * (T->rwet >> 7) >> 7;  /* (0..256: DEPTH, the rotor on) */
    pk = pc ? 3 + (int32_t)((pc - 1u) & 1u) : -1;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && drw_v[drw_part(t)][i].perc > perc) perc = drw_v[drw_part(t)][i].perc;
    perc >>= 7;                                          /* (PERC's envelope, 0..256) */
    if (vxs.nms[5] != n0)                                /* a note: CLICK's bubbles out of the worms' mouths */
        for (i = 0; i < 9u; i++)
            if (drw_level(p, i) && vx_hash(i + vxs.nage[5] * 9u) < click) vxs.bub[i] = fm1_ms | 1u;
    vx_line(&f, -62 * 16, 30 * 16, 62 * 16, 30 * 16, 31 + (en * 77 >> 8));
    for (i = 0; i < 9u; i++) {
        int32_t l = drw_level(p, i), X = (-48 + (int32_t)i * 12) * 16, pp = (int32_t)i == pk ? perc : 0;
        int32_t h = 48 + l * 102 + (pp * 160 >> 8) + (en * l * 10 >> 8), th = 64 + l * 35, c0 = (en * 51 >> 8) + hb + (pp * 102 >> 8);
        int32_t bend = ((og_sin((vxs.rph[1] >> 16) - i * 5734u) * dep) >> 15) * h / 60 * 6 / 256;
        int32_t am = 128 + ((og_sin((vxs.rph[0] >> 16) - i * 8344u) * dep) >> 16), hx = 0, hy = 0, k, m;
        char d[2] = {(char)('0' + l), 0};
        th = th < h ? th : h;
        for (k = -1; k <= 1; k += 2)                     /* the tube, its mouth */
            vx_line(&f, X + k * 42, 30 * 16, X + k * 38, 30 * 16 - th, 46 + c0);
        vx_ring(&f, X, 30 * 16 - th, 42, 14, 64 + c0);
        vx_text(f.cx + ((X * f.s) >> 8), f.cy + ((30 * 16 * f.s) >> 8) + 64, d, l ? T_MID : T_DIM);
        if (!l) continue;
        for (k = 0; k <= 10; k++) {                      /* the worm out of it, waving at its harmonic */
            int32_t yy = 30 * 16 - th - k * (h - th) / 10, xx = X + bend * k * k / 100 +
                         ((((og_sin((uint32_t)(k * VW_RAT[i] * 65536 / 320) - vx_ang(mt, 200 + (int32_t)i * 16)) * 10) >> 15) * (77 + en)) >> 8);
            for (m = -26; k && m <= 26; m += 52)
                vx_line(&f, hx + m, hy, xx + m, yy, 51 + l * 77 / 8 * am / 256 + c0);
            hx = xx; hy = yy;
        }
        hx = X + bend;
        hy = 30 * 16 - h - 42;
        m = (int32_t)(VW_RAT[i] + 4) / 8;                /* its crown: as many tentacles as its harmonic */
        m = clamp(m, 2, 8);
        for (j = 0; j < (uint32_t)m; j++) {
            uint32_t a = 65536u - (uint32_t)(4915 + 22938 * (int32_t)j / (m - 1));
            int32_t wl = 38 + ((og_sin(vx_ang(mt, 200) + (j + i) * 10430u) * 13 >> 15) * en >> 8);
            vx_line(&f, hx + ((og_cos(a) * 45) >> 15), hy + ((og_sin(a) * 45) >> 15), hx + ((og_cos(a) * (45 + wl)) >> 15),
                    hy + ((og_sin(a) * (45 + wl)) >> 15), 77 + l * 102 / 8 + c0);
        }
        vx_ring(&f, hx, hy, 51, 48, 82 + l * 102 / 8 + c0);
        m = en * l / 8 * ((int32_t)i == pk ? 256 + perc : 256) / 256;   /* its face: shouting with its bar */
        vx_face(&f, hx, hy - 6, 18, 16, age < 90u && l > 3, m > 64 ? hy + 21 - (m * 7 >> 8) : hy + 19, 10, m > 64 ? (m * 14 >> 8) : 0,
                10 + (m * 8 >> 8), 243, 218, 154);
    }
    for (i = 0; i < 9u; i++)                             /* CLICK's bubbles, rising */
        if (vxs.bub[i] && fm1_ms - vxs.bub[i] < 1200u) {
            int32_t a = (int32_t)(fm1_ms - vxs.bub[i]) * 256 / 1000, l = drw_level(p, i);
            vxs.dal = (int16_t)clamp(a, 0, 256);
            vx_ring(&f, (-48 + (int32_t)i * 12) * 16 + ((og_sin((uint32_t)a * 367u) * 24) >> 15),
                    30 * 16 - (3 + l * 7) * 16 - 96 - (a * 30 * 16 >> 8), 16 + a / 16, 16 + a / 16, 128);
            vxs.dal = 0;
        }
    if (T->dust)                                         /* DUST: motes drifting */
        for (i = 0; i < (uint32_t)((T->dust * 40 >> 8)); i++)
            vx_dot(&f, (vx_hash(i * 9u + 1u) * 124 / 256 - 62) * 16,
                   (int32_t)(((uint32_t)vx_hash(i * 3u + 2u) * 6u + (mt >> 4) * (uint32_t)(77 + vx_hash(i) / 2) / 256u) % 1504u) - 752, 16, 90);
    vxs.kc = 0;
}

/* -------------------------------------------------------------------------------------------- NOISE --- */
/* the swarm's step (the mock-up's, a frame of 30 a second): COLOR their walk, FREQ their swirl and its ring, PEAK
 * how tight, DUST's impulses flare them (DENSE) */
static VX_CODE void vx_nz_step(const track_t *t, int32_t en)
{
    const int16_t *p = t->p;
    int32_t col = (p[P_E1] * 516 >> 8), peak = (p[P_E3] * 516 >> 8), rp = (12 * 256 + p[P_E2] * 26 * 256 / 127), dens = (p[P_E5] * 516 >> 8);
    int32_t sp = (4 + (p[P_E2] * 60 >> 8)) * (256 + en) / 256, jit = (256 - col) * 358 / 256 * (77 + en) / 256 + (col * 90 >> 8);
    int32_t pull = 3 + (peak * 15 >> 8), damp = 230 + (col * 20 >> 8), i;
    uint32_t r = vxs.rs ? vxs.rs : 2463534242u;
    for (i = 0; i < VX_NSW; i++) {
        int16_t *q = vxs.sw[i];
        int32_t x = q[0], y = q[1], ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, rr = (ax > ay ? ax + ay * 3 / 8 : ay + ax * 3 / 8) + 1;
        int32_t vx = q[2], vy = q[3], k = (rr - rp) * pull / 256;
        vx = (vx * damp >> 8) + ((int32_t)(vx_rnd(&r) & 255u) - 128) * jit / 256 - y * sp / 1000 - x * k / rr;
        vy = (vy * damp >> 8) + (((int32_t)(vx_rnd(&r) & 255u) - 128) * jit / 256) * 4 / 5 + x * sp / 1000 - y * k / rr;
        if ((p[P_E0] & 3) == NZ_DUST && (int32_t)(vx_rnd(&r) & 65535u) < dens * dens / 50 * (51 + en) / 256) {
            q[4] = 256;                                  /* (an impulse: it flares, it jumps) */
            vx += ((int32_t)(vx_rnd(&r) & 255u) - 128) * 6;
            vy += ((int32_t)(vx_rnd(&r) & 255u) - 128) * 6;
        }
        vx -= x / 250;
        vy -= y / 166;
        q[4] = (int16_t)((q[4] * 217 >> 8));
        x += vx / 2;
        y += vy / 2;
        if (x > 62 * 256 || x < -62 * 256 || y > 46 * 256 || y < -46 * 256) { x = (x * 77 >> 8); y = (y * 77 >> 8); vx = vy = 0; }
        q[0] = (int16_t)clamp(x, -32000, 32000); q[1] = (int16_t)clamp(y, -32000, 32000);
        q[2] = (int16_t)clamp(vx, -32000, 32000); q[3] = (int16_t)clamp(vy, -32000, 32000);
    }
    vxs.rs = r;
}
/* ENJAMBRE */
static VX_CODE void vx_noise(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    vxf_t f = vx_uf(A, 100, 0);
    int32_t en = vx_level(t, 6, en_home), md = p[P_E0] & 3, peak = (p[P_E3] * 516 >> 8), i, j, n;
    uint32_t mt = vxs.mt[6], age = vx_age(t, 6, 0), reg = 0;
    int32_t sq = vx_sq(77 + (en * 154 >> 8), age), sx = 256 + (sq * 77 >> 8), sy = 256 - (sq * 77 >> 8);
    vxs.kc = 1;
    if (!vxs.sw[0][0] && !vxs.sw[0][1])                  /* (scattered, the first time) */
        for (i = 0; i < VX_NSW; i++) {
            vxs.sw[i][0] = (int16_t)((vx_hash((uint32_t)i * 13u + 1u) - 128) * 80);
            vxs.sw[i][1] = (int16_t)((vx_hash((uint32_t)i * 29u + 7u) - 128) * 60);
        }
    for (n = 0; vxs.swt < mt >> 8 && n < 6; n++, vxs.swt++)   /* (its frames as its time ran: still in silence) */
        vx_nz_step(t, en);
    vxs.swt = mt >> 8;
    if (md >= NZ_LFSR) {                                 /* LFSR / META: the register marching in columns, a column a bit */
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active) { reg = t->v[i].ph[1]; break; }
        for (i = 0; i < 16; i++) {
            int32_t on = md == NZ_LFSR ? (int32_t)(reg >> i & 1u) : vx_hash(((uint32_t)i + (mt >> 9)) % 16u * 7u + (uint32_t)(p[P_E5] >> 4)) > 128;
            int32_t X = -56 * 16 + i * 118;
            for (j = 0; j < 5; j++) {
                int32_t Y = (-36 + j * 6 + (on ? 0 : 36)) * 16;
                if (on) vx_line(&f, X - 16, Y, X + 22, Y, 128 + (en * 102 >> 8));
                else vx_dot(&f, X, Y, 16, 46);
            }
        }
    }
    for (i = md >= NZ_LFSR ? VX_NSW / 2 : 0; i < VX_NSW; i++) {   /* the protozoa: a stroke along their way, a dot */
        const int16_t *q = vxs.sw[i];
        int32_t vx = q[2], vy = q[3], av = vx < 0 ? -vx : vx, bv = vy < 0 ? -vy : vy, v = (av > bv ? av + bv * 3 / 8 : bv + av * 3 / 8) + 1;
        int32_t s = v < 3 * 256 ? v : 3 * 256, L = 16 + (s * 230 >> 12), x = q[0] / 16, y = q[1] / 16;
        int32_t c = q[4] > 51 ? 218 + (q[4] * 38 >> 8) : clamp(31 + (s * 31 >> 8) + (en * 46 >> 8), 0, 179);
        vx_line(&f, x - vx * L / v, y - vy * L / v, x + vx * L * 2 / 5 / v, y + vy * L * 2 / 5 / v, c);
        if (i % 3 == 0) vx_dot(&f, x, y, 16, c);
    }
    if (peak > 51) {                                     /* PEAK: the ring they keep */
        int32_t rp = (12 * 16 + p[P_E2] * 26 * 16 / 127);
        vxs.dal = (int16_t)(256 - peak / 2);
        vx_ring(&f, 0, 0, rp, rp, 77 + (peak * 102 >> 8));
        vxs.dal = 0;
    }
    og_node(f.cx, f.cy, (9 * f.s) >> 8, T_SURF);         /* the queen */
    {
        vxb_t g = {0, 0, 0, 256, sx, sy, 6, 14 + (en * 50 >> 8), 50 + (en * 100 >> 8), (en * 80 >> 8), 3, 4};
        vx_body(&f, &g, 144, 96, mt * 90u >> 8, 51 + (en * 90 >> 8), 108 + (en * 90 >> 8), 56, 32);
    }
    vx_bface(&f, 0, 0, 144, (en * 307 >> 8), age < 90u, 0);
    vxs.kc = 0;
}

/* -------------------------------------------------------------------------------------------- PHASE --- */
/* the bent phase of PHASE's wave w at dcw (0..65535), as eng_phase.c's pd_wave bends it (the mock-up's pdPhase) */
static uint32_t vx_pd(uint32_t w, uint32_t dcw, uint32_t ph)
{
    uint32_t x, pk;
    switch (w) {
    case 0: x = 32768u + ((dcw * 32767u) >> 16); return ph < x ? ph * 32768u / x : 32768u + (ph - x) * 32767u / (65535u - x);
    case 1:
        x = 32768u - ((dcw * 31120u) >> 16);
        return ph < x ? ph * 32768u / x : ph < 32768u ? 32768u : ph < 32768u + x ? 32768u + (ph - 32768u) * 32767u / x : 65535u;
    case 2: x = 65535u - ((dcw * 63487u) >> 16); return ph < x ? ph * 65535u / x : 65535u;
    case 3: x = 65535u - ((dcw * 49151u) >> 16); return ph < x ? ph * 65535u / x : (ph - x) * 65535u / (65535u - x);
    case 4:
        x = 65535u - ((dcw * 32767u) >> 16);
        pk = (x * (32768u + ((dcw * 29491u) >> 16))) >> 16;
        pk = pk ? pk : 1u;
        return ph >= x ? 65535u : ph < pk ? ph * 32768u / pk : 32768u + (ph - pk) * 32767u / (x - pk);
    default: return (ph + ((ph * ((dcw * 44000u) >> 16)) >> 16) * 7u) & 65535u;
    }
}
/* .. its sample (Q15): -cos of the bent phase; RSAW RTRI RTRP windowed */
static int32_t vx_pdw(uint32_t w, uint32_t dcw, uint32_t ph)
{
    int32_t c = -og_cos(vx_pd(w, dcw, ph)), win;
    if (w < 5u) return c;
    win = w == 5u ? 65535 - (int32_t)ph : w == 6u ? (ph < 32768u ? (int32_t)ph * 2 : (65535 - (int32_t)ph) * 2) :
          (ph < 16384u ? (int32_t)ph * 4 : ph > 49152u ? (65535 - (int32_t)ph) * 4 : 65535);
    return (c * (win > 65535 ? 65535 : win)) >> 16;
}
/* the shell's spiral: its radius at u (Q16 turns from the centre), Q4 units: 2.5 e^(0.19 th) */
static int32_t vx_rs(int32_t u) { return 40 * vx_x2(u * 441 >> 16) >> 12; }
/* the spiral from u0 to u1 (n lines) about c, its radius k / 100 of the shell's, plus dr, frayed by fr; its heat h0 rising
 * by h1 along it */
static void vx_whorl(const vxf_t *f, const int32_t *c, int32_t u0, int32_t u1, int32_t n, uint32_t da, int32_t k, int32_t dr,
                     int32_t fr, uint32_t seed, int32_t h0, int32_t h1)
{
    int32_t i, pr = 0;
    uint32_t pa = 0;
    for (i = 0; i <= n; i++) {
        int32_t u = u0 + (u1 - u0) / n * i, r = vx_rs(u) * k / 100 + dr + (vx_hash((uint32_t)i + seed) - 128) * fr / 256 * 26 / 256 * i / n;
        uint32_t a = (uint32_t)u + 16384u + da;
        if (i) vx_pl(f, c, pa, pr, a, r, h0 + h1 * i / n);
        pa = a; pr = r;
    }
}
/* CARACOL */
static VX_SEG void vx_phase(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    vxf_t f = vx_uf(A, 110, 48);
    int32_t en = vx_level(t, 7, en_home), i, k, mx = 1, prev = 0, px = 0, py = 0, q[2 * 13];
    uint32_t mt = vxs.mt[7], age = vx_age(t, 7, 0), w1 = (uint32_t)p[P_E0] & 7u, w2 = p[P_E1] ? (uint32_t)(p[P_E1] - 1) & 7u : w1;
    int32_t fb = (p[P_E7] * 516 >> 8), sub = (p[P_E6] * 516 >> 8), det = p[P_E4], ring = p[P_E5], line2 = det || ring;
    int32_t dcw = clamp((p[P_E2] * 516 >> 8) + en * p[P_E3] * 179 / 127 / 256 - (p[P_E3] * 77 >> 8), 0, 255) * 256;
    int32_t sq = vx_sq(102 + (en * 154 >> 8), age), cy = -96 - sq / 4, alt = p[P_E1] && (mt >> 8) / 20u % 2u, sh = age < 100u;
    int32_t c[4] = {0, cy, 256, 235};
    uint32_t dr = mt * (uint32_t)det * 6606u >> 16;
    vxs.kc = 1;
    for (i = 0; i < 128; i++) {                          /* two cycles out: WAVE then WAVE2, FB, the second line, SUB */
        uint32_t w = i >= 64 ? w2 : w1, ph = ((uint32_t)(i & 63) * 1024u + (uint32_t)((prev * fb >> 9)) + 65536u) & 65535u;
        int32_t s = vx_pdw(w, (uint32_t)dcw, ph);
        prev = s;
        if (line2) {
            int32_t s2 = vx_pdw(w, (uint32_t)dcw, ((uint32_t)i * 1024u * (3000u + (uint32_t)det) / 3000u + dr) & 65535u);
            s = ring ? (s * s2) >> 15 : (s + s2) / 2;
        }
        s += (((og_sin((uint32_t)i * 512u) * sub) >> 8) * 154) >> 8;
        vxs.wv[i] = (int16_t)clamp(s >> 3, -32000, 32000);
        mx = s >> 3 > mx ? s >> 3 : -(s >> 3) > mx ? -(s >> 3) : mx;
    }
    if (mx > 205)
        for (i = 0; i < 128; i++) vxs.wv[i] = (int16_t)(vxs.wv[i] * 4096 / mx);
    /* the foot: its back a smooth line, its sole the wave */
    q[0] = -42 * 16; q[1] = 14 * 16; q[2] = -30 * 16; q[3] = 18 * 16; q[4] = -10 * 16; q[5] = 21 * 16; q[6] = 16 * 16; q[7] = 21 * 16;
    q[8] = 38 * 16; q[9] = 30 * 16;
    vx_curve(&f, q, 5, 0, 61 + en / 4);
    for (i = 0; i <= 84; i++) {
        int32_t s = vxs.wv[i * 127 / 84], x = (-44 + i) * 16, y = 31 * 16 + ((s * (26 + (en * 38 >> 8))) >> 12);
        if (i) vx_line(&f, px, py, x, y, 64 + (s < 0 ? -s : s) * 102 / 4096 + en / 4);
        px = x; py = y;
    }
    vx_line(&f, -60 * 16, 36 * 16, 60 * 16, 36 * 16, 26);
    {   /* the head: the eyestalks (they pull in on a note), the tentacles, the mouth shouting with the level */
        int32_t hx = -44 * 16, hy = 18 * 16, st = 144 * (sh ? 115 : 256) / 256 * (256 + en / 5) / 256, mo = (en * 42 >> 8);
        for (k = 0; k < 2; k++) {
            uint32_t a = 65536u - 20316u - (uint32_t)k * 4588u + (uint32_t)((og_sin(vx_ang(mt, 50) + (uint32_t)k * 10430u) * 834) >> 15);
            int32_t ex = hx + 48 + ((og_cos(a) * st) >> 15), ey = hy - 48 + ((og_sin(a) * st) >> 15);
            q[0] = hx + 32 + k * 32; q[1] = hy - 48; q[2] = hx + 48 + ((og_cos(a) * st) >> 16); q[3] = hy - 48 + ((og_sin(a) * st) >> 16);
            q[4] = ex; q[5] = ey;
            vx_curve(&f, q, 3, 0, 77 + en / 4);
            vx_ring(&f, ex, ey, 26, 26, 154);
            if (sh) vx_line(&f, ex - 14, ey, ex + 14, ey, 243);
            else vx_dot(&f, ex, ey, 16, 243);
        }
        for (k = -1; k <= 1; k += 2)
            vx_line(&f, hx - 16, hy + 64, hx - 64, hy + 96 + k * 16, 77);
        q[0] = hx + 96; q[1] = hy - 64; q[2] = hx; q[3] = hy - 64; q[4] = hx - 48; q[5] = hy + 32; q[6] = hx; q[7] = hy + 128;
        q[8] = hx + 96; q[9] = hy + 160;
        vx_curve(&f, q, 5, 0, 77 + (en * 77 >> 8));
        if (mo > 10) vx_arc(&f, hx + 16, hy + 80, 13 + (mo * 77 >> 8), (mo * 141 >> 8), 0, 65536u, 10, 218);
        else vx_line(&f, hx, hy + 80, hx + 32, hy + 80, 154);
    }
    vx_whorl(&f, c, 0, 140902, 120, 0, 100, 0, fb, mt >> 8, 64 + en / 4 + fb / 5, 77);   /* the shell: the spiral (FB frays it) */
    for (i = 0; i < 24; i++) {                           /* .. its chambers at the bent phase */
        uint32_t b = vx_pd(alt ? w2 : w1, (uint32_t)dcw, (uint32_t)i * 2731u), u = 65536u + ((b * 75366u) >> 16);
        int32_t s = vxs.wv[i * 63 / 24 + (alt ? 64 : 0)];
        if (b <= 62915u)
            vx_pl(&f, c, u + 16384u, vx_rs((int32_t)u - 65536), u + 16384u, vx_rs((int32_t)u), 51 + (s < 0 ? -s : s) * 128 / 4096 + en / 4);
    }
    if (sub > 13) {                                      /* SUB: the outer whorl breathing slowly */
        vxs.dal = (int16_t)(256 - sub * (128 + (og_sin(vx_ang(mt, 60)) >> 8)) / 256);
        vx_whorl(&f, c, 140902 - 39322, 140902, 39, 0, 100, 38, 0, 0, 141, 0);
        vxs.dal = 0;
    }
    if (line2) {                                         /* the second line: a little one following; RING a thread */
        static const int32_t C2[4] = {50 * 16, 22 * 16, 256, 256};
        q[0] = 41 * 16; q[1] = 30 * 16; q[2] = 46 * 16; q[3] = 27 * 16; q[4] = 54 * 16; q[5] = 27 * 16; q[6] = 59 * 16; q[7] = 30 * 16;
        vx_curve(&f, q, 4, 0, 64 + en / 5);
        vx_whorl(&f, C2, 0, 140902, 49, vx_ang(mt, (det * 202 >> 8)), 38, 0, 0, 0, 77 + (en * 77 >> 8), 0);
        if (ring) {
            for (i = 0; i <= 12; i++) {
                q[2 * i] = 18 * 16 + (28 * 16) * i / 12;
                q[2 * i + 1] = cy + 96 + (20 * 16 - cy - 96) * i / 12 + ((og_sin((uint32_t)i * 7823u + vx_ang(mt, 400)) * en * 32 / 256) >> 15);
            }
            vx_poly(&f, q, 13, 0, 128 + (en * 102 >> 8));
        }
    }
    vxs.kc = 0;
}

/* --------------------------------------------------------------------------------------------- LOFI --- */
/* ORGANISMO DE CIRCUITO */
static VX_SEG void vx_lofi(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    int32_t a0 = A[2] * 256 / 124, b0 = (A[3] - 4) * 256 / 94, fl = p[P_E1] != RW_BYTE, new_fr = vxs.fr[8] != ui.frame;
    int32_t en = vx_level(t, 8, en_home), i, j, pk = 6000, sx[9], sy[9], bc[8], px = 0, py = 0, qx = 0, qy = 0, q[2 * 22], g = fl ? 38 : 27;
    uint32_t mt = vxs.mt[8], age = vx_age(t, 8, 0), w0 = scope_w - 256u, byte = 0, cr;
    int32_t sq = vx_sq(vx_fall(age, 154u), age);
    vxf_t f;
    f.s = (a0 < b0 ? a0 : b0) * 112 / 100;
    f.cx = (A[0] * 2 + A[2]) * 8 - ((32 * f.s) >> 8);
    f.cy = (A[1] * 2 + A[3]) * 8;
    vxs.kc = 1;
    for (i = 0; i < 256; i++) {                          /* the last 256 samples out (the scope's): their peak */
        int32_t v = scope_buf[(w0 + (uint32_t)i) & (SCOPE_N - 1u)];
        v = v < 0 ? -v : v;
        pk = v > pk ? v : pk;
    }
    for (i = 0, j = (4096 << 15) / pk; i < 256; i++)
        vxs.wv[i] = (int16_t)((scope_buf[(w0 + (uint32_t)i) & (SCOPE_N - 1u)] * j) >> 15);
#define VH(i) vxs.wv[i]
    byte = (uint32_t)(VH(255) / 32 + 128) & 255u;       /* the byte now: BYTE's held sample, else the output's level */
    for (i = 0, j = 0; i < NVOICE; i++)
        if (!fl && t->v[i].active && t->v[i].age >= (uint32_t)j) { j = (int32_t)t->v[i].age; byte = (uint32_t)((t->v[i].s[0] + 32640) >> 8) & 255u; }
    for (i = 0; i < 8; i++) {                            /* (its bits slewed, once a frame) */
        if (new_fr)
            vxs.bd[i] = (int16_t)(vxs.bd[i] + (((byte >> i & 1u) ? en : 0) - vxs.bd[i]) * 141 / 256);
        bc[i] = clamp((vxs.bd[i] - 46) * 410 / 256, 0, 256);
    }
    vxs.crawl += ((mt - vxs.cmt) * (uint32_t)(307 + 4 * en) >> 8) * 10430u / 7680u;   /* the crawl */
    vxs.cmt = mt;
    cr = vxs.crawl;
    for (i = 0; i <= 8; i++) {                           /* the spine: the head, then 8 segments back */
        int32_t amp = (26 + (en * 54 >> 8)) * (77 + i * 179 / 8) / 256;
        sx[i] = i ? (27 * 16 - (i - 1) * 149) : 38 * 16;
        sy[i] = 6 * 16 + ((og_sin(cr * 22u / 10u - (uint32_t)i * 8344u) * amp) >> 15) - (i ? 0 : 64);
    }
#define VR(i) ((i) ? 147 - (i) * 8 : 176)
    vx_line(&f, -54 * 16, 33 * 16, 54 * 16, 33 * 16, 26);
    for (i = 1; i <= 8; i++) {                           /* legs: lifted where the bit is high */
        int32_t r = VR(i), b = bc[8 - i], sw = (og_sin(cr * 4u + (uint32_t)i * 13560u) * 22) >> 15, k;
        for (k = -1; k <= 1; k += 2) {
            q[0] = sx[i] + k * 29; q[1] = sy[i] + (r * 179 >> 8);
            q[2] = sx[i] + k * 45 + sw; q[3] = sy[i] + r + 38 - (b * 19 >> 8);
            q[4] = sx[i] + k * 35 + (sw * 333 >> 8); q[5] = sy[i] + r + 96 - (b * 48 >> 8);
            q[5] = q[5] < 33 * 16 ? q[5] : 33 * 16;
            vx_poly(&f, q, 3, 0, 51 + b / 5);
        }
    }
    for (j = 0; j <= 80; j++) {                          /* the two skins: the samples themselves along the body */
        int32_t u = 256 + j * 7 * 256 / 80, i0 = u >> 8 < 7 ? u >> 8 : 7, fu = u - i0 * 256;
        int32_t x = sx[i0] + (sx[i0 + 1] - sx[i0]) * fu / 256, y = sy[i0] + (sy[i0 + 1] - sy[i0]) * fu / 256;
        int32_t dx = sx[i0 + 1] - sx[i0], dy = sy[i0 + 1] - sy[i0], adx = dx < 0 ? -dx : dx, ady = dy < 0 ? -dy : dy;
        int32_t L = (adx > ady ? adx + ady * 3 / 8 : ady + adx * 3 / 8) + 1, j5 = j * 16 / 5;
        int32_t r = (VR(i0) + (VR(i0 + 1) - VR(i0)) * fu / 256) * (256 - ((((j5 * j5 >> 8) * (j5 * j5 >> 8) >> 8) * j5 >> 8) * 141 >> 8)) / 256 + 26;   /* (thinner to the tail) */
        int32_t s1 = VH(j * 127 / 80), s2 = VH(128 + j * 127 / 80), d1 = r + ((s1 * g) >> 12), d2 = r + ((s2 * g) >> 12);
        int32_t tx = x + dy * d1 / L, ty = y - dx * d1 / L, bx = x - dy * d2 / L, by = y + dx * d2 / L;
        if (j) {
            vx_line(&f, px, py, tx, ty, 46 + (s1 < 0 ? -s1 : s1) * 130 / 4096 + (en * 40 >> 8));
            vx_line(&f, qx, qy, bx, by, 46 + (s2 < 0 ? -s2 : s2) * 130 / 4096 + (en * 40 >> 8));
        }
        px = tx; py = ty; qx = bx; qy = by;
    }
    q[0] = px; q[1] = py; q[2] = sx[8] - 96; q[3] = sy[8]; q[4] = qx; q[5] = qy;
    vx_curve(&f, q, 3, 0, 60);
    for (i = 8; i >= 1; i--) {                           /* the segments: rings lit by their bit (or the wave's level) */
        int32_t r = VR(i), v = VH(i * 30 + 6), m;
        v = fl ? clamp((v < 0 ? -v : v) * 115 / 100, 0, 4096) * 218 / 4096 + 13 : bc[8 - i];
        for (m = 0; m < 18; m++) {
            uint32_t a = (uint32_t)m * 65536u / 18u;
            int32_t w = 4096 + VH((i * 29 + m * 9) & 255) * 9 / 100;
            q[2 * m] = sx[i] + ((((og_cos(a) * r * 66 / 100) >> 15) * w) >> 12);
            q[2 * m + 1] = sy[i] + ((((og_sin(a) * r * 8 / 10) >> 15) * w) >> 12);
        }
        vx_poly(&f, q, 18, 1, 44 + (v * 200 >> 8));
        vx_ring(&f, sx[i], sy[i], (r * 77 >> 8), (r * 97 >> 8), 28 + (v * 150 >> 8));
        if (v > 115) vx_dot(&f, sx[i], sy[i], (v - 51) * f.s / 4267, 184 + (v * 72 >> 8));
    }
    for (i = -1; i <= 1; i += 2) {                       /* the antennae: tipped by the two lowest bits */
        int32_t b = bc[i < 0 ? 0 : 1], tx = sx[0] + 48 + i * 96 + (b * 32 >> 8);
        int32_t ty = sy[0] - 320 - (b * 64 >> 8) + ((og_sin(cr * 3u + (uint32_t)(i * 10430)) * 24) >> 15);
        q[0] = sx[0] + i * 48; q[1] = sy[0] - 144; q[2] = sx[0] + i * 80; q[3] = sy[0] - 240; q[4] = tx; q[5] = ty;
        vx_curve(&f, q, 3, 0, 77 + b / 5);
        vx_dot(&f, tx, ty, ((256 + (b * 307 >> 8)) * f.s * 6 / 10) >> 12, 128 + b / 2);
    }
    {   /* the head: a bicho shouting with the level, squashed by the note */
        uint32_t tt = mt * (uint32_t)(60 + (en * 260 >> 8)) >> 8;
        int32_t hx = sx[0], hy = sy[0], ssx = 256 + (sq * 77 >> 8), ssy = 256 - (sq * 77 >> 8);
        og_node(f.cx + ((hx * f.s) >> 8), f.cy + ((hy * f.s) >> 8), (10 * f.s) >> 8, T_SURF);
        vxb_t g = {hx, 0, hy, 288, ssx, ssy, 4, 8 + (en * 30 >> 8), 40 + (en * 100 >> 8), (en * 80 >> 8), 4, 1};
        vx_body(&f, &g, 168, 112, tt, 51 + (en * 77 >> 8), 102 + (en * 77 >> 8), 72, 40);
        vx_bface(&f, hx + 16, hy, 168, (en * 333 >> 8), age < 177u, 0);
    }
#undef VH
#undef VR
    vxs.kc = 0;
}

/* (0.9.9-snd5) STRING and ADD keep their time, level and newest note in vxs's slots of PHASE (7) and WHEEL (5): one creature
 * is drawn at a time (EDIT's, HOME's), so a slot shared costs a glance at most when the track changes, and no byte of the
 * pool (at its floor) */
#define VX_K_STRING 7u
/* ---------------------------------------------------------------------------------------- (snd5) STRING --- */
/* TEJEDORA II: a radial web, a ray a voice (its line, one period, along it); the spider stands on the newest ray at POS
 * and plucks it with the exciter EXCITE draws; STRETCH warps the spiral, BODY and SIZE the cocoon hanging at the side */
static VX_SEG void vx_string(const track_t *t, const int32_t *A, int32_t en_home)
{
    const int16_t *p = t->p;
    vxf_t f = vx_uf(A, 95, 0);
    int32_t en = vx_level(t, VX_K_STRING, en_home), silk = 30 + p[P_E2] / 2, st = p[P_E4], k, j, r, q[2 * 24], ux = 0, uy = 0;
    int32_t cx = -6 * 16, cy = -6 * 16, R = 44, bow = 0, nw = -1;
    uint32_t age = vx_age(t, VX_K_STRING, 0), mt = vxs.mt[VX_K_STRING], i, a, th = 0, na = 0, seen = 0;
    vxs.kc = 1;
    for (r = 10; r < R; r += 8) {                        /* the spiral, warped by STRETCH */
        for (k = 0; k < 24; k++) {
            int32_t rr = r * 16 + ((og_sin((uint32_t)k * 8192u + (uint32_t)r * 10430u) * st >> 15) * r * 16 * 31 >> 8) / 127;
            a = (uint32_t)k * 65536u / 24u;
            q[2 * k] = cx + ((og_cos(a) * rr) >> 15);
            q[2 * k + 1] = cy + ((og_sin(a) * rr * 4 / 5) >> 15);
        }
        vx_curve(&f, q, 24, 1, silk - 10);
    }
    for (i = 0; i < NVOICE; i++)                         /* (the newest voice: the spider's ray) */
        if (t->v[i].active && t->v[i].age >= na) { na = t->v[i].age; nw = (int32_t)i; }
    for (i = 0; i < NVOICE + 6u; i++) {                  /* the rays: voice i on ray i % 6, a ray no voice holds still */
        const voice_t *v = &t->v[i % NVOICE];
        uint32_t ray = i < NVOICE ? i % 6u : i - NVOICE, live = i < NVOICE && v->active, n = 2u, w = 0, d;
        const int16_t *b = live ? str_line(t, v) : 0;
        int32_t pk = 0, rx, ry;
        if (i >= NVOICE && (seen >> ray & 1u)) continue;
        if (i < NVOICE && !live) continue;
        seen |= 1u << ray;
        a = 49152u + ray * 10923u + 3129u;
        rx = og_cos(a);
        ry = og_sin(a) * 4 / 5;
        if (live) {
            n = str_period(pitch_inc((uint32_t)clamp(v->pitch_cur, 0, 2047)), &d) >> 8;
            n = n < 2u ? 2u : n;
            w = v->ph[1] % ST_SL;
            if ((int32_t)i == nw) { th = a; ux = rx; uy = ry; if (v->gate && p[P_E7]) bow = 1; }
        }
        for (j = 0; j <= 20; j++) {
            int32_t s = live && j && j < 20 ? b[(w + ST_SL - n + (uint32_t)j * n / 21u) % ST_SL] : 0, dd = s * 7 >> 11;
            pk = s > pk ? s : -s > pk ? -s : pk;
            q[2 * j] = cx + ((rx * R * j / 20 * 16) >> 15) - ((ry * dd) >> 15);
            q[2 * j + 1] = cy + ((ry * R * j / 20 * 16) >> 15) + ((rx * dd) >> 15);
        }
        vx_curve(&f, q, 21, 0, silk + ((pk >> 8) * 110 >> 7));
    }
    if (nw < 0) { th = 49152u + 3129u; ux = og_cos(th); uy = og_sin(th) * 4 / 5; }   /* (at rest: on the top ray) */
    {   /* the spider at POS along its ray: two rings, eight legs, the front one plucking (sawing while BOW feeds it) */
        int32_t pu = 26 + p[P_E3] * 205 / 127, sx = cx + ((ux * R * pu >> 8) * 16 >> 15), sy = cy + ((uy * R * pu >> 8) * 16 >> 15);
        int32_t h = 150 + (en * 60 >> 8), pl = age < 200u ? (int32_t)(200u - age) * 256 / 200 : 0, bx, by, nx, ny, o, e;
        vx_ring(&f, sx - (ux * 4 * 16 >> 15), sy - (uy * 4 * 16 >> 15), 4 * 16, 3 * 16, h);
        vx_ring(&f, sx + (ux * 2 * 16 >> 15), sy + (uy * 2 * 16 >> 15), 40, 40, h + 20);
        for (k = 0; k < 8; k++) {
            int32_t sd = k < 4 ? -1 : 1, j4 = k & 3, fx, fy, kx, ky;
            a = th + (uint32_t)(sd * (5215 + j4 * 3650));
            fx = sx + og_cos(a) * (8 + j4) * 16 / 32768;
            fy = sy + og_sin(a) * (7 + j4) * 16 / 32768;
            if (k == 4) {
                fx = sx + ((ux * 9 * 16 - uy * (pl * 4 * 16 >> 8)) >> 15);
                fy = sy + ((uy * 9 * 16 + ux * (pl * 4 * 16 >> 8)) >> 15);
                if (bow) fx += (og_sin(vx_ang(mt, 12000)) * 3 * 16) >> 15;
            }
            kx = (sx + fx) / 2 - ((uy * 3 * 16 * sd) >> 15);
            ky = (sy + fy) / 2 + ((ux * 3 * 16 * sd) >> 15) - 32;
            vx_line(&f, sx, sy, kx, ky, h - 20);
            vx_line(&f, kx, ky, fx, fy, h - 35);
        }
        /* the exciter past the foot: NOISE dust, PLUCK a finger, PICK a plectrum, HAMMR a hammer, STICK a mallet */
        nx = -uy; ny = ux;
        o = (3 * 256 + 5 * (256 - (age < 250u ? 256 - (int32_t)age * 256 / 250 : 0))) * 16 >> 8;
        bx = sx + ((ux * (10 * 16 - o)) >> 15);
        by = sy + ((uy * (10 * 16 - o)) >> 15);
#define XP(m, c) (bx - ((ux * (m) * 16 - nx * (c) * 16) >> 15)), (by - ((uy * (m) * 16 - ny * (c) * 16) >> 15))
        e = clamp(p[P_E0], 0, SX_N - 1);
        if (e == SX_NOISE)
            for (k = 0; k < 9; k++)
                vx_dot(&f, bx + (vx_hash((uint32_t)k + (mt >> 11)) - 128) * 9 * 16 / 256,
                       by + (vx_hash((uint32_t)k * 7u + (mt >> 11) + 99u) - 128) * 9 * 16 / 256, 0, 190);
        else if (e <= SX_PICK) {
            int32_t c[8] = {XP(8, 2), XP(3, 3), XP(0, 0), XP(-2, 2)}, c2[6] = {XP(0, 0), XP(5, 3), XP(5, -3)};
            if (e == SX_PLUCK) vx_curve(&f, c, 4, 0, 190); else vx_poly(&f, c2, 3, 1, 190);
        } else {
            int32_t c[4] = {XP(9 + (e == SX_MALLET), 0), XP(2, 0)};
            vx_line(&f, c[0], c[1], c[2], c[3], 160);
            vx_ring(&f, c[2] + ((ux * 16) >> 15), c[3] + ((uy * 16) >> 15), e == SX_HAMMER ? 3 * 16 : 2 * 16, e == SX_HAMMER ? 48 : 32, 190);
        }
#undef XP
        sx += ux * 2 * 16 >> 15;
        sy += (uy * 2 * 16 >> 15) - 8;
        vx_face(&f, sx, sy, 31, 12, age < 120u, sy + 37, 12, pl > 64 ? 20 : 0, 16, 220, 220, 200);
    }
    {   /* the cocoon: SIZE its size, BODY its glow */
        int32_t rx = (4 * 127 + p[P_E6] * 7) * 16 / 127, ry = (6 * 127 + p[P_E6] * 9) * 16 / 127, gl = p[P_E5] * (77 + en) / 127;
        vx_line(&f, cx + 34 * 16, cy + 2 * 16, cx + 34 * 16, 46 * 16 - ry * 2, 40);
        vx_ring(&f, cx + 34 * 16, 46 * 16 - ry, rx, ry, 60 + (gl * 170 >> 8));
        vx_ring(&f, cx + 34 * 16, 46 * 16 - ry, rx * 55 / 100, ry * 6 / 10, 40 + (gl * 140 >> 8));
    }
    vxs.kc = 0;
}

/* (snd5) ADD's creature: gone in V10.7.13+ (room for GEN's FORM MY LOOP): ADD draws the being (ui_being.c) */

/* (V10.54) INPUT: the audio interface's two inputs as meters (lit: the ones SOURCE takes), GATE's level across them,
 * between them FOLLOW (the matrix's source) and the gate's gain; the source over them */
/* (SWARM) the swarm as it plays: each device heard a body at its side (its channel: L, both, R) and its note's height,
 * its size its level, lit on each advertisement (its rhythm), turning on a small orbit at its own period (the
 * clocks' polyrhythm seen), a line from the loudest to each other (the relation that detunes them); SWARM n above */
static void vx_swarm(const int32_t *A)
{
    uint32_t v, now = fm1_ticks(), n = 0, ref = SW_N, rlv = 0;
    int32_t x0 = A[0] + 8, w = A[2] - 16, y0 = A[1] + 16, h = A[3] - 26, px[SW_N], py[SW_N], hl[SW_N];
    char b[10];
    if (w < 16 || h < 16)
        return;
    for (v = 0; v < 3u; v++)                             /* (the floor: L, both, R) */
        cv_rect(x0 + (int32_t)(1u + 2u * v) * w / 6 - 3, y0 + h + 4, 7, 1, T_MID);
    for (v = 0; v < SW_N; v++) {
        const struct sw_dev *d = &sw[v];
        uint32_t per = d->per | 1u, ph;
        px[v] = -1;
        if (!d->id || d->amp < 8)
            continue;
        n++;
        ph = (uint32_t)(((uint64_t)((now - d->last) % per) << 32) / per);   /* (its own clock's phase) */
        px[v] = x0 + w / 8 + d->pan * (w * 3 / 4) / 32767 + (osc_sine(ph) * 6 >> 15);   /* (its place: IMAGE's) */
        py[v] = y0 + h - 4 - clamp((int32_t)d->nt - 24, 0, 108) * (h - 8) / 108 + (osc_sine(ph + 0x40000000u) * 4 >> 15);
        hl[v] = clamp(((int32_t)d->lv - sw_lmin) * 255 / (int32_t)(((uint32_t)(sw_lmax - sw_lmin) & 127u) | 1u), 0, 255);
        if (d->lv >= rlv)
            rlv = d->lv, ref = v;
    }
    for (v = 0; v < SW_N && ref < SW_N; v++)              /* the relations: the loudest to each other */
        if (px[v] >= 0 && v != ref)
            cv_line(px[ref], py[ref], px[v], py[v], ux_mix(T_MID, vk_heat(hl[v]), 35));
    for (v = 0; v < SW_N; v++) {
        const struct sw_dev *d = &sw[v];
        int32_t z;
        int hit = d->acc > d->amp / 2;
        if (px[v] < 0)
            continue;
        z = clamp(3 + d->amp * 6 / 14000, 3, 9);
        cv_rect(px[v] - z / 2, py[v] - z / 2, z, z, hit ? T_TEXT : vk_heat(hl[v]));
        if (hit)                                         /* (an advertisement now: its ring) */
            cv_frame(px[v] - z / 2 - 3, py[v] - z / 2 - 3, z + 6, z + 6, vk_heat(hl[v]));
    }
    heat_bar(x0, y0 + h + 7, sm.mq * w >> 7, w, 2, T_THEME);   /* (V10.6z6) FAR .. NEAR: the morph, under the floor
                                                         * (V10.7.7: cold to hot) */
    if (!vx_small) {
        str_cpy(b, "SWARM ", sizeof b);
        fmt_int(b + 6, (int32_t)n);
        cv_text(A[0] + A[2] / 2 - 3 * (int32_t)str_len(b), A[1] + 2 - AF_X_CAP_Y, &AF_X, b, T_THEME);
    }
}
static VX_SEG void vx_input(const track_t *t, const int32_t *A, int32_t en_home)
{
    if (t->p[P_E0] == AIN_SW) {                         /* (SWARM: the devices; the other SOURCEs: the meters below) */
        vx_swarm(A);
        return;
    }
    uint32_t src = (uint32_t)clamp(t->p[P_E0], 0, AIN_N - 1), k;
    int32_t bw = A[2] / 7, bh = A[3] - 30, by = A[1] + 16, cx = A[0] + A[2] / 2, thr = clamp(t->p[P_E2], 0, 127);
    int32_t lv = thr * thr * 2, gg = ain_gg[(uint32_t)(t - trk) % NTRK];
    (void)en_home;
    if (bw < 4 || bh < 16)
        return;
    if (!vx_small)
        cv_text(cx - 3 * (int32_t)str_len(N_AIN_SRC[src]), A[1] + 2 - AF_X_CAP_Y, &AF_X, N_AIN_SRC[src], T_THEME);
    for (k = 0; k < 2u; k++) {
        int32_t x = k ? cx + bw : cx - 2 * bw, v = clamp(ain_pk[k] * 2, 0, 32767), hh = v * bh / 32767;
        int on = src >= AIN_SUM || src == k;
        cv_frame(x, by, bw, bh, T_MID);
        cv_rect(x + 1, by + bh - hh, bw - 2, hh, on ? T_THEME : T_MID);
        if (thr) {
            int32_t gy = by + bh - clamp(lv * 2, 0, 32767) * bh / 32767;
            cv_rect(x - 2, gy, bw + 4, 1, T_TEXT);
        }
        if (!vx_small)
            cv_text(x + bw / 2 - 3, by + bh + 4 - AF_X_CAP_Y, &AF_X, k ? "2" : "1", on ? T_TEXT : T_MID);
    }
    {   /* FOLLOW: a thin column, the gate's gain a dot over it */
        int32_t fh = mod_follow * bh / 32767;
        cv_frame(cx - 2, by, 5, bh, T_MID);              /* (its frame, the input silent too) */
        cv_rect(cx - 1, by + bh - fh, 3, fh, T_TEXT);
        if (thr)
            cv_rect(cx - 2, by - 6, 5, 3, gg > 16384 ? T_THEME : T_MID);
    }
}

/* ------------------------------------------------------------------------------------------------ the glue --- */
/* the engine's own creature in the rectangle x y w h (px: EDIT's left panel 8 14 108 112, HOME's middle 146 x 112, or a
 * whole panel: each fits itself to it), en_home: HOME's level for the track (-1: its own); 0 when the engine has none
 * (its being then) */
static int32_t vx_fit(int32_t w, int32_t h, int32_t uw, int32_t uh)   /* a unit's px (Q8): uw x uh units in w x h px */
{
    return w * 256 / uw < h * 256 / uh ? w * 256 / uw : h * 256 / uh;
}
typedef struct { const engine_t *e; void (*f)(const track_t *, const int32_t *, int32_t); } vxe_t;
static const vxe_t VX_F[] = {{&ENG_ANALOG, vx_analog}, {&ENG_WHEEL, vx_wheel}, {&ENG_NOISE, vx_noise}, {&ENG_PHASE, vx_phase},
                             {&ENG_LOFI, vx_lofi}, {&ENG_STRING, vx_string}};   /* (0.9.9-snd5; V10.7.13+: ADD's gone, room for
                                                         * FORM MY LOOP: it draws the being) */
static int vx_draw(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, int32_t en_home)
{
    const engine_t *e = trk_engp(t);
    int32_t A[4] = {x, y, w, h}, cx = x * 16 + w * 8, cy = y * 16 + h * 8, s;
    if (!bt_hw_seg() && (e == &ENG_LOFI || e == &ENG_ANIMAL || e == &ENG_STRING || e == &ENG_PHASE))
        return 0;                                        /* (V10.48: those creatures live in the segment; V10.6g PHASE's too) */
    if (e == &ENG_FORMANT) {                             /* (the tract ~80 x 62 units about (-20, 37): the crown off) */
        s = vx_fit(w, h, 80, 62);
        vx_voice(t, cx + 5 * s / 4, cy - 37 * s / 16, s, en_home);
    } else if (trk_eng(t) == ENGI_DRUM) {    /* (the kit ~128 x 92) */
        s = vx_fit(w, h, 128, 92);
        vx_kit(t, cx - (s >> 4), cy + (48 * s >> 8), s, en_home, -1);
    } else if (e == &ENG_INPUT) {                       /* (V10.54) the inputs' meters */
        if (!bt_hw_seg())
            return 0;
        vx_input(t, A, en_home);
    } else if (e == &ENG_ANIMAL)                        /* (the animal ~103 x 78) */
        vx_animal(t, cx, cy, vx_fit(w, h, 103, 78), en_home);
    else if (trk_eng(t) == ENGI_FM6)
        vx_fm6(t, A, en_home, 6u, 0);
    else {                                               /* (0.9.9-snd5: the creatures drawn the same way, a table) */
        uint32_t i;
        for (i = 0; i < NELEM(VX_F) && VX_F[i].e != e; i++) {}
        if (i == NELEM(VX_F))
            return 0;
        VX_F[i].f(t, A, en_home);
    }
    return 1;
}
/* EDIT > KIT (lane -1) and SOUND 1..4 (a DRUM track): the kit large on the panel, KIT over it on the left; on the right
 * the sound last struck (KIT) or the sound edited, lit (SOUND: lane) */
static void vx_kit_page(const track_t *t, int32_t lane)
{
    char b[16];
    uint32_t l;
    vx_kit(t, 120 * 16, 73 * 16, 368, -1, lane);
    l = lane < 0 ? vxs.last & 7u : (uint32_t)lane & 7u;
    str_cpy(b, drum_lane_abbr(t, l), sizeof b);
    str_cpy(b + str_len(b), " \xB7 ", sizeof b - str_len(b));
    str_cpy(b + str_len(b), drum_lane_name(t, l), sizeof b - str_len(b));
    cv_text(8, 6 - AF_X_CAP_Y, &AF_X, "KIT", T_THEME);
    cv_text_r(232, 6 - AF_X_CAP_Y, &AF_X, b, lane < 0 ? T_TEXT : T_THEME, T_SURF);
}
