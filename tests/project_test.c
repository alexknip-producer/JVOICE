/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Host test of the project formats (firmware/src/project.c, -DPROJ_HOST part). (V10.6s) The formats of Felucca's
 * betas to 1.0.2, FUN1 .. FUN6 (built here byte for byte as the firmware stored them: tests/legacy_proj.h), are
 * retired: each, valid as it was, is refused and leaves the slot as it was; nothing reads past its end. The
 * project as the RAM slot holds it round-trips (its chain checked), FUN7 .. FUNC load (below). A DIGITAL track
 * (retired 1.0) loads as FM6 with the init patch; a SAMPLE track (its old PERC set too) as ANALOG's first.
 * Run by tests/run_tests.sh (needs build/gen from one firmware build). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1                             /* (trk_old_engine: engines.c, through hostsim.c) */
#define PROJ_FUNB_WRITER 1                      /* (JIANT 0.6) proj_pack_vb: the older formats are made of a FUNB */
#include "../firmware/src/project.c"
#include "legacy_proj.h"

static int check(const char *what, int ok)
{
    printf("%-60s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* the value parameter k (old id) of track t had in the old project */
static int16_t oldv(uint32_t t, uint32_t k) { return (int16_t)(t * 100u + k * 3u + 1u); }

static const uint8_t OLD_ENG[PROJ_LT] = {7, 0, 5, 0};   /* WHEEL, ANALOG, VOICE; the drum track: 0 (no engine) */
static void fill_v2_track(proj_trk_v2_t *d, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V2; k++)
        d->p[k] = oldv(t, k);
    d->engine = OLD_ENG[t];
    d->preset = (uint8_t)(t + 5u);
    for (k = 0; k < NSTEP; k++) {
        step8_t *s = &d->step[k];
        s->note[0] = (uint8_t)(36u + (k + t) % 40u);
        s->n = (uint8_t)(k % 3u);
        s->time = (uint8_t)(k % 3u);
        s->flags = (uint8_t)(k & 3u);
        s->vel = (uint8_t)(64u + t);
    }
}

/* today's project q as format 4 stored it (8-byte steps; the hits dropped) */
static void to_v4(project_v4_t *v, const project_t *q)
{
    uint32_t t, k;
    memset(v, 0, sizeof *v);
    v->magic = PROJ_MAGIC_V4;
    v->size = sizeof *v;
    memcpy(v->g, q->g, sizeof v->g);
    v->sel = q->sel;
    v->parts = q->parts;
    v->phys = q->phys;
    for (t = 0; t < PROJ_LT; t++) {
        for (k = 0; k < 61u; k++) v->t[t].p[k] = q->t[t].p[k];
        for (k = 0; k < 8u; k++) v->t[t].p[61u + k] = q->t[t].p[P_E0 + k];
        v->t[t].engine = q->t[t].engine;
        v->t[t].preset = q->t[t].preset;
        for (k = 0; k < NSTEP; k++) {
            memcpy(v->t[t].step[k].note, q->t[t].step[k].note, 4);
            v->t[t].step[k].n = q->t[t].step[k].n;
            v->t[t].step[k].time = q->t[t].step[k].time;
            v->t[t].step[k].flags = q->t[t].step[k].flags;
            v->t[t].step[k].vel = q->t[t].step[k].vel;
        }
    }
    v->sum = proj_hash(v, sizeof *v - 4u);
}

/* a format 3 track (57 parameters: the SLICER at 45..48, P_E0 49) */
static int16_t oldv3(uint32_t t, uint32_t k) { return (int16_t)(t * 200u + k * 5u + 3u); }
static void fill_v3_track(proj_trk_v3_t *d, uint32_t t)
{
    uint32_t k;
    for (k = 0; k < PROJ_NP_V3; k++)
        d->p[k] = oldv3(t, k);
    d->engine = (uint8_t)(t == 3u ? 11u : t == 2u ? 7u : t * 3u);   /* (0 3 7 11: not a retired one, PHYS 9, TRIO 6) */
    d->preset = (uint8_t)(t + 1u);
    for (k = 0; k < NSTEP; k++) {
        step8_t *s = &d->step[k];
        s->note[0] = (uint8_t)(40u + (k * 7u + t) % 40u);
        s->n = (uint8_t)((k + t) % 3u);
        s->time = (uint8_t)(k % 3u);
        s->flags = (uint8_t)((k + 1u) & 3u);
        s->vel = (uint8_t)(70u + t);
    }
}
/* (JIANT, FUNB) an older record of the project packed in st: np parameters a track (the first np - 8, then E0..E7;
 * the motion's engine ids moved down with them), the layout of that size (FUNA: the punch-in block and the kit; FUN8 /
 * FUN9: the FM6 patches; the name at the end), its magic and hash. old_motion: where its motion went */
static uint32_t old_motion;
static void old_image(uint8_t *o, const project_store_t *st, uint32_t np, uint32_t size, uint32_t magic)
{
    const uint8_t *b = st->raw;
    uint32_t pos = 68u, src = 68u, t, j, sum, k;
    uint32_t name = size - 4u - PROJ_NAME_LEN, fm6 = name - PROJ_LT * FM6_PACKED;
    memset(o, 0, size);
    memcpy(o, b, 68);
    memcpy(o, &magic, 4); memcpy(o + 4, &size, 4);
    o[66] = (uint8_t)np;
    for (t = 0; t < PROJ_LT; t++) {
        for (j = 0; j < np; j++) o[pos++] = b[src + (j < np - 8u ? j : P_E0 + j - (np - 8u))];
        src += P_COUNT;
        memcpy(o + pos, b + src, 2u + NSTEP * 9u);
        pos += 2u + NSTEP * 9u; src += 2u + NSTEP * 9u;
    }
    memcpy(o + pos, b + src, sizeof(chain_config_t) + sizeof(motion_store_t));
    old_motion = pos + sizeof(chain_config_t) + 4u;
    for (k = 0; k < MOTION_MAX; k++) {
        uint8_t *p = o + old_motion + k * 4u + 1u;
        if ((*p & 0x7Fu) >= P_E0)
            *p = (uint8_t)((*p & MOTION_LOCK) | ((*p & 0x7Fu) - (P_COUNT - np)));
    }
    if (size == PROJ_STORE_VA) {
        memcpy(o + fm6 - 96u - 48u, b + PROJ_PFX_OFF, 37u);   /* (the lane, its tracks, the macros: no new globals) */
        memcpy(o + fm6 - 96u, b + PROJ_DX_OFF, 96u);
    }
    if (size >= PROJ_STORE_V8)
        memcpy(o + fm6, b + PROJ_FM6_OFF, PROJ_LT * FM6_PACKED);
    memcpy(o + name, b + PROJ_NAME_OFF, PROJ_NAME_LEN);
    sum = proj_hash(o, size - 4u);
    memcpy(o + size - 4u, &sum, 4);
}

int main(void)
{
    static project_v4_t v4;
    static project_v3_t v3;
    static project_v2_t v2;
    static project_v1_t v1;
    static project_t q, q2;
    static project_store_t slot_old;
    static union {
        project_t v5;
        project_v4_t v4;
        project_v3_t v3;
        project_v2_t v2;
        project_v1_t v1;
    } buf;
    uint32_t i, t;
    int bad = 0, ok;

    bad += check("layout: SLICER after DETUNE, then the matrix just before P_E0",
                 P_SLCR == P_DETUNE + 1 && P_SLDEPTH + 1 == P_M1SRC && P_M4AMT + 1 == P_FM1_ATK && P_FM4_LEVEL + 1 == P_SOFS && P_POFS + 1 == P_LN0 &&
                 P_LN7 + 1 == P_DTYPE && P_MS15 + 1 == P_FTYPE && P_FRES + 1 == P_E0 && P_E0 == 115 && P_COUNT == PROJ_NP_V3 + 66u &&
                 PROJ_NP_V3 == PROJ_NP_V2 + 4u);
    bad += check("FUNB fits one flash object (its whole payload), the retained cache in NOINIT",
                 sizeof(project_store_t) == 4096u - 256u && 0xC8u + 4u * sizeof(project_store_t) <= 0x3D50u);

    /* a project as the RAM slot holds it: four sounds and steps (the old formats below are made of it) */
    memset(&q, 0, sizeof q);
    q.magic = PROJ_MAGIC; q.size = sizeof q; q.parts = NPART; q.phys = PROJ_PHYS; q.sel = 2;
    chain_defaults(&q.chain);
    for (i = 0; i < G_COUNT; i++)
        q.g[i] = GP[i].def;
    for (t = 0; t < NTRK; t++) {
        for (i = 0; i < P_COUNT; i++)
            q.t[t].p[i] = param_desc_of(OLD_ENG[t % PROJ_LT], i)->def;
        q.t[t].engine = OLD_ENG[t % PROJ_LT];
        for (i = 0; i < NSTEP; i++)
            q.t[t].step[i] = (step_t){{(uint8_t)(40u + (i + t) % 30u), 0, 0, 0}, 1, i % 3u ? ST_NOTE : ST_REST, 0, 90, 0, 0};
        memcpy(q.fm6[t], FM6_INIT, FM6_PACKED);
    }
    q.sum = proj_sum(&q);

    /* (V10.6s) FUN1 .. FUN6, valid as stored: refused, the slot untouched */
    memset(&v2, 0, sizeof v2);
    v2.magic = PROJ_MAGIC_V2;
    v2.size = sizeof v2;
    for (i = 0; i < PROJ_NG_V2; i++)
        v2.g[i] = (int16_t)(500 + i);
    v2.sel = 2;
    for (t = 0; t < PROJ_LT; t++)
        fill_v2_track(&v2.t[t], t);
    v2.sum = proj_hash(&v2, sizeof v2 - 4u);
    memset(&v3, 0, sizeof v3);
    v3.magic = PROJ_MAGIC_V3;
    v3.size = sizeof v3;
    v3.parts = NPART;
    for (t = 0; t < PROJ_LT; t++)
        fill_v3_track(&v3.t[t], t);
    v3.sum = proj_hash(&v3, sizeof v3 - 4u);
    to_v4(&v4, &q);
    memset(&v1, 0, sizeof v1);
    v1.magic = PROJ_MAGIC_V1;
    v1.size = sizeof v1;
    fill_v2_track(&v1.t, 0);
    v1.sum = proj_hash(&v1, sizeof v1 - 4u);
    bad += check("FUN1 .. FUN4 images as stored (688, 2552, 2584, 2680 bytes)", sizeof v1 == 688u && sizeof v2 == 2552u &&
                 sizeof v3 == 2584u && sizeof v4 == 2680u);
    {
        static project_v6_t v6;
        static uint8_t big[sizeof(project_store_t) + 64u];
        const void *img[6] = {&v1, &v2, &v3, &v4, &v6, &v6};
        const uint32_t len[6] = {sizeof v1, sizeof v2, sizeof v3, sizeof v4, sizeof(project_v5_t), sizeof v6};
        uint32_t k;
        memset(&v6, 0, sizeof v6);
        v6.magic = PROJ_MAGIC_V6; v6.size = sizeof v6; v6.parts = NPART; v6.phys = PROJ_PHYS;
        for (k = 0; k < PROJ_LT; k++) {
            for (i = 0; i < 61u; i++) v6.t[k].p[i] = q.t[k].p[i];
            for (i = 0; i < 8u; i++) v6.t[k].p[61u + i] = q.t[k].p[P_E0 + i];
            v6.t[k].engine = q.t[k].engine;
        }
        chain_defaults(&v6.chain);
        v6.sum = proj_hash(&v6, sizeof v6 - 4u);
        for (k = 0; k < 6u; k++) {
            const char *nm[6] = {"FUN1", "FUN2", "FUN3", "FUN4", "FUN5", "FUN6"};
            char what[80];
            if (k == 4u) {                              /* FUN5: FUN6 without the chain */
                project_v5_t *v5 = (project_v5_t *)big;
                memset(big, 0, sizeof big);
                memcpy(v5, &v6, sizeof *v5 - 4u);
                v5->magic = PROJ_MAGIC_V5; v5->size = sizeof *v5;
                v5->sum = proj_hash(v5, sizeof *v5 - 4u);
                img[4] = v5;
            }
            memset(&q2, 0x5A, sizeof q2);
            ok = !proj_import(&q2, img[k], (int)len[k]);
            for (i = 0; i < sizeof q2; i++)
                ok &= ((const uint8_t *)&q2)[i] == 0x5Au;
            snprintf(what, sizeof what, "%s (retired): a valid image refused, the slot untouched", nm[k]);
            bad += check(what, ok);
        }
        memset(&slot_old, 0xA5, sizeof slot_old);       /* in a retained (longer) slot: its size field says FUN6's */
        memcpy(slot_old.raw, &v6, sizeof v6);
        bad += check("FUN6 in a retained slot: refused", !proj_import(&q2, &slot_old, sizeof slot_old));
    }

    /* the RAM slot's own: as stored (a matrix slot set; another engine: WHEEL, 7; a DRUM track with notes on its
     * lanes stays so) */
    q.t[1].engine = 7;
    q.t[2].p[P_M2SRC] = 6;
    q.t[2].p[P_M2DST] = 11;
    q.t[2].p[P_M2AMT] = -17;
    q.t[0].engine = ENGI_DRUM;
    q.t[0].step[0] = (step_t){{36, 42, 0, 0}, 2, ST_NOTE, 0, 96, 1u << DV_SNARE, 1u << DV_SNARE};
    q.sum = proj_sum(&q);
    memcpy(&buf, &q, sizeof q);
    bad += check("RAM slot -> RAM slot: as stored (a matrix slot, engine 7, a DRUM step with notes and hits)",
                 proj_import(&q2, &buf, (int)sizeof q) && !memcmp(&q, &q2, sizeof q) && q2.t[1].engine == 7 &&
                 q2.t[2].p[P_M2AMT] == -17 && q2.t[0].step[0].n == 2u && q2.t[0].step[0].hit == 1u << DV_SNARE);
    q.t[0].engine = 3;
    q.t[0].step[0] = q.t[0].step[1];
    q.chain.count = 2;
    q.chain.row[0] = (chain_row_t){0, 4};
    q.chain.row[1] = (chain_row_t){3, 16};
    q.sum = proj_sum(&q);
    bad += check("RAM slot: chain round trip", proj_import(&q2, &q, sizeof q) && !memcmp(&q.chain, &q2.chain, sizeof q.chain));
    q.chain.row[0].bars = 0;
    q.sum = proj_sum(&q);
    bad += check("RAM slot: invalid row refused even with a correct checksum", !proj_import(&q2, &q, sizeof q));
    chain_defaults(&q.chain);
    q.sum = proj_sum(&q);
    memcpy(&buf, &q, sizeof q);
    buf.v5.magic = PROJ_MAGIC_V3;
    bad += check("RAM slot size with a FUN3 magic: refused", !proj_import(&q2, &buf, (int)sizeof q));

    {   /* (V10.7.13+, room for GEN's FORM MY LOOP) JIANT 0.1 .. 0.5's formats (FUN7 .. FUNB) are no longer read: each
         * refused (an empty slot), known for one of them (proj_fetch: OLD PROJECT: NOT READ); FUNC is not */
        static project_t a, c;
        static project_store_t st, sc;
        static uint8_t v9[3648], va[3744], v8[3584], v7[PROJ_STORE_V7];
        memset(&a, 0, sizeof a);
        a.magic = PROJ_MAGIC; a.size = sizeof a; a.parts = NPART; a.phys = PROJ_PHYS;
        chain_defaults(&a.chain);
        a.t[1].p[P_LEVEL] = 99;
        memcpy(a.name, "OLD SONG", 8);
        a.sum = proj_sum(&a);
        ok = proj_pack_vb(&st, &a) && ((uint32_t *)st.raw)[0] == PROJ_MAGIC_VB && !proj_import(&c, &st, sizeof st) &&
             proj_old(&st, sizeof st);
        old_image(va, &st, 99u, 3744u, PROJ_MAGIC_VA); ok &= !proj_import(&c, va, sizeof va) && proj_old(va, sizeof va);
        old_image(v9, &st, 99u, 3648u, PROJ_MAGIC_V9); ok &= !proj_import(&c, v9, sizeof v9) && proj_old(v9, sizeof v9);
        old_image(v8, &st, 91u, 3584u, PROJ_MAGIC_V8); ok &= !proj_import(&c, v8, sizeof v8) && proj_old(v8, sizeof v8);
        old_image(v7, &st, 99u, PROJ_STORE_V7, PROJ_MAGIC_V7); ok &= !proj_import(&c, v7, sizeof v7) && proj_old(v7, sizeof v7);
        ok &= proj_pack(&sc, &a) && proj_import(&c, &sc, sizeof sc) && !proj_old(&sc, sizeof sc) && c.t[1].p[P_LEVEL] == 99;
        bad += check("V10.7.13+: FUN7 FUN8 FUN9 FUNA FUNB (JIANT 0.1 .. 0.5) refused and known as old; FUNC loads", ok);
    }

#if !FELUCCA_FM4
    {   /* DIGITAL tracks (engine 1, retired: src/fm4_convert.c) in FUN8 / FUN7 projects load as FM6 with the init
         * patch as the track's own (V10.6s: no conversion), the values as they are; their motion on the EDIT or OP ENV
         * values goes, the rest stays */
        project_t a, c, d;
        static project_store_t st;
        uint8_t v[FP_SIZE + 1u], pk[FM6_PACKED];
        int16_t p[P_COUNT];
        uint32_t i;
        memset(&a, 0, sizeof a);
        a.magic = PROJ_MAGIC; a.size = sizeof a; a.parts = NPART; a.phys = PROJ_PHYS;
        chain_defaults(&a.chain);
        for (t = 0; t < PROJ_LT; t++) {
            for (i = 0; i < P_COUNT; i++)
                a.t[t].p[i] = param_desc_of(t == 1u ? ENGI_DIGITAL : 0u, i)->def;
            a.t[t].engine = t == 1u ? ENGI_DIGITAL : 0u;
            a.t[t].preset = t == 1u ? 5u : 0u;
            memcpy(a.fm6[t], FM6_INIT, FM6_PACKED);
        }
        fm4_preset_values(a.t[1].p, 5);                 /* (V10.6s: retired, nothing) an OP ENV edit on op 2 */
        a.t[1].p[P_FM2_ATK] = 30;
        a.t[1].p[P_FM2_LEVEL] = 100;
        a.t[1].p[P_LEVEL] = 90;
        memcpy(p, a.t[1].p, sizeof p);
        bad += check("DIGITAL conversion retired: the init patch, preset 0, the values as they are",
                     fm4_convert(p, v) == 0u && !memcmp(p, a.t[1].p, sizeof p));
        fm6_pack(v, pk);
        bad += check("  (the init patch)", !memcmp(pk, FM6_INIT, FM6_PACKED));
        a.motion.count = 5;
        a.motion.event[0] = (motion_event_t){1u << 6 | 3u, P_E4, 20};          /* track 2: INDEX */
        a.motion.event[1] = (motion_event_t){1u << 6 | 4u, P_FM1_LEVEL, 50};   /* track 2: OP LEVEL */
        a.motion.event[2] = (motion_event_t){1u << 6 | 5u, P_CHOR, 60};        /* track 2: a send: stays */
        a.motion.event[3] = (motion_event_t){0u << 6 | 3u, P_E4, 70};          /* track 1 (ANALOG): stays */
        a.motion.event[4] = (motion_event_t){1u << 6 | 9u, P_E7, 0};           /* track 2: E7 */
        a.sum = proj_sum(&a);
#define FM4_OK(c) ((c).t[1].engine == ENGI_FM6 && (c).t[1].preset == 0u && !memcmp((c).fm6[1], pk, FM6_PACKED) && \
                   !memcmp((c).t[1].p, p, sizeof p) && (c).t[0].engine == 0u && !memcmp((c).fm6[0], FM6_INIT, FM6_PACKED))
        ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
        bad += check("FUNC with a DIGITAL track: FM6, the init patch its own, its values kept", ok && FM4_OK(c));
        bad += check("  its motion on INDEX / OP LEVEL / E7 goes; a send's and the other track's stay",
                     c.motion.count == 2u && c.motion.event[0].param == P_CHOR && c.motion.event[1].place == 3u &&
                     c.motion.event[1].param == P_E4 && motion_valid(&c.motion) && proj_ok(&c));
        bad += check("  imported again: as it is (FM6 now)", proj_import(&d, &c, sizeof c) && !memcmp(&d, &c, sizeof d));
#undef FM4_OK
    }
#endif
    {   /* (JIANT) GRAIN (8) and SLICE (13) tracks, retired: ANALOG's first preset, their engine motion gone, the rest kept */
        static project_t g, h;
        static project_store_t gs;
        memset(&g, 0, sizeof g);
        g.magic = PROJ_MAGIC; g.size = sizeof g; g.parts = NPART; g.phys = PROJ_PHYS;
        chain_defaults(&g.chain);
        for (t = 0; t < PROJ_LT; t++) {
            for (i = 0; i < P_COUNT; i++) g.t[t].p[i] = param_desc_of(0, i)->def;
            memcpy(g.fm6[t], FM6_INIT, FM6_PACKED);
            memcpy(g.dx, DX_KIT_DEF, sizeof g.dx);
        }
        g.t[1].engine = ENGI_SAMPLE; g.t[2].engine = ENGI_PHYS; g.t[2].p[P_LEVEL] = 66;   /* (0.9.9-snd5: GRAIN's and SLICE's numbers
                                                         * are STRING and ADD now: two other retired ones) */
        g.motion.count = 2; g.motion.on = 4;
        g.motion.event[0] = (motion_event_t){2u << 6 | 1u, P_E2, 30};
        g.motion.event[1] = (motion_event_t){2u << 6 | 2u, P_REV, 40};
        g.sum = proj_sum(&g);
        ok = proj_pack(&gs, &g) && proj_import(&h, &gs, sizeof gs) && h.t[1].engine == ENGI_PHYS_TO &&
             h.t[2].engine == ENGI_PHYS_TO && h.t[2].p[P_LEVEL] == 66 && h.motion.count == 2u && proj_ok(&h) &&
             h.t[2].p[P_E0] == ENGINES[ENGI_PHYS_TO]->presets[0].e[0] && !eng_ok(ENGI_PHYS) && eng_ok(ENGI_STRING) && eng_ok(ENGI_ADD) &&
             !eng_ok(ENGI_SAMPLE) && eng_live(ENGI_PHYS) == ENGI_PHYS_TO;
        bad += check("JIANT: SAMPLE and PHYS tracks (retired) load as ANALOG's first preset (V10.7.13+: its motion as it was), LEVEL and sends kept", ok);
    }
    {   /* (JIANT 0.6) FUNC: six tracks in FUNB's 3840 bytes, only the steps that are not empty stored */
        static project_t f, g, h;
        static project_store_t fs, gs;
        uint32_t k, i, n;
        memset(&f, 0, sizeof f);
        f.magic = PROJ_MAGIC; f.size = sizeof f; f.parts = NPART; f.phys = PROJ_PHYS;
        chain_defaults(&f.chain);
        proj_new_tracks(&f, 0u);
        for (k = 0; k < NTRK; k++) memcpy(f.fm6[k], FM6_INIT, FM6_PACKED);
        for (k = 0; k < G_COUNT; k++) f.g[k] = GP[k].def;
        memcpy(f.dx, DX_KIT_DEF, sizeof f.dx);
        for (k = 0; k < NTRK; k++)                      /* a typical song: 16 steps a track, T5 a chord on all 64 */
            for (i = 0; i < (k == 4u ? NSTEP : 16u); i++) {
                step_t *s = &f.t[k].step[i];
                s->time = ST_NOTE; s->n = (uint8_t)(k == 4u ? 3u : 1u); s->vel = (uint8_t)(60u + k);
                s->note[0] = (uint8_t)(40u + i); s->note[1] = 64; s->note[2] = 67;
                if (i == 3u) step_set_ratchet(s, 3u), step_set_chance(s, 40u);
            }
        f.t[5].p[P_LEVEL] = 77; f.t[4].engine = ENGI_FM6; memcpy(f.fm6[4], FM6_FACTORY[3], FM6_PACKED);
        motion_at(&f.motion.event[0], 5u << 6 | 9u, 33); f.motion.event[0].param = P_LEVEL;
        motion_at(&f.motion.event[1], 4u << 6 | 2u, -20); f.motion.event[1].param = P_PAN | MOTION_LOCK;
        f.motion.count = 2; f.motion.on = 0x30;
        memcpy(f.name, "SIX", 3);
        f.sum = proj_sum(&f);
        ok = proj_pack(&fs, &f) && ((uint32_t *)fs.raw)[0] == PROJ_MAGIC && fs.raw[67] == NTRK && proj_import(&g, &fs, sizeof fs);
        ok &= !memcmp(g.t, f.t, sizeof f.t) && !memcmp(g.fm6, f.fm6, sizeof f.fm6) && !memcmp(&g.motion, &f.motion, sizeof f.motion) &&
              !memcmp(g.name, "SIX", 3) && !memcmp(g.g, f.g, sizeof f.g) && !memcmp(g.dx, f.dx, sizeof f.dx);
        ok &= MOTION_TRK(&g.motion.event[0]) == 5u && MOTION_STEP(&g.motion.event[0]) == 9u && MOTION_VAL(&g.motion.event[0]) == 33 &&
              MOTION_TRK(&g.motion.event[1]) == 4u && MOTION_VAL(&g.motion.event[1]) == -20;
        bad += check("FUNC: six tracks round trip (steps, ratchet, chance, patches, T5 / T6 motion, name, globals)", ok);
        {   /* (JIANT 0.8.5) VOICE's lyrics: 'L' 'Y' blocks for the tracks that have one; none before */
            static project_t l1, l2;
            static project_store_t ls;
            static const char *const LY[NTRK] = {"la noche es azul", "", "", "you are my sunshine - oh",
                                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJK", "sakura"};
            uint32_t hv, nl = 0;
            int oky = 1;
            ok = proj_import(&l2, &fs, sizeof fs);          /* the FUNC above: written without lyrics */
            for (k = 0; k < NTRK; k++)
                ok &= !l2.lyr[k][0];
            bad += check("0.8.5: a project without lyrics (FUNC of before) loads with none", ok);
            l1 = f;
            for (k = 0; k < NTRK; k++) {
                memset(l1.lyr[k], 0, LYR_LEN);
                memcpy(l1.lyr[k], LY[k], strlen(LY[k]));
                nl += LY[k][0] != 0;
            }
            l1.t[0].engine = 5; l1.t[0].p[P_VLANG] = LG_ES; l1.t[0].p[P_VLYR] = 0; l1.t[0].p[P_E0] = 3;
            l1.sum = proj_sum(&l1);
            ok = proj_pack(&ls, &l1) && proj_import(&l2, &ls, sizeof ls);
            for (k = 0; k < NTRK; k++)
                ok &= !memcmp(l2.lyr[k], l1.lyr[k], LYR_LEN);
            ok &= !memcmp(l2.t, l1.t, sizeof l1.t) && l2.t[0].p[P_VLANG] == LG_ES && l2.t[0].p[P_E0] == 3;
            bad += check("0.8.5: lyrics round trip (four tracks, 63 characters, \"-\"), LANG and POS with the sound", ok);
            l2 = l1;
            memset(l2.lyr, 0, sizeof l2.lyr);
            ok = proj_packed_size(&l1) == proj_packed_size(&l2) + 4u * nl + (uint32_t)strlen(LY[0]) + (uint32_t)strlen(LY[3]) +
                                          (uint32_t)strlen(LY[4]) + (uint32_t)strlen(LY[5]);
            bad += check("0.8.5: the lyrics' blocks counted in the section's room (4 + length a track)", ok);
            {   /* a lyric byte outside 32..126 (hash right): refused; a firmware of before stops at the block */
                static project_store_t bs;
                uint32_t p;
                bs = ls;
                for (p = 68u; p + 4u < PROJC_PFX_OFF && !(bs.raw[p] == 'L' && bs.raw[p + 1u] == 'Y' && bs.raw[p + 2u] == 0u); p++)
                    ;
                oky = p + 4u < PROJC_PFX_OFF && bs.raw[p + 3u] == strlen(LY[0]) && !memcmp(bs.raw + p + 4u, LY[0], strlen(LY[0]));
                bs.raw[p + 5u] = 7;
                hv = proj_hash(bs.raw, PROJ_STORE_SIZE - 4u);
                memcpy(bs.raw + PROJ_STORE_SIZE - 4u, &hv, 4);
                oky &= !proj_import(&l2, &bs, sizeof bs);
                bad += check("0.8.5: the block in place (track, length, text); a control byte in it: refused", oky);
            }
            for (k = 0; k < NTRK; k++)                  /* lyrics on a full section: not packed, proj_full */
                memset(l1.lyr[k], 'A', LYR_LEN - 1u);
            for (k = 0; k < NTRK; k++)
                for (i = 0; i < NSTEP; i++) {
                    l1.t[k].step[i].time = ST_NOTE; l1.t[k].step[i].n = 2; l1.t[k].step[i].note[0] = 60; l1.t[k].step[i].vel = 90;
                }
            l1.sum = proj_sum(&l1);
            ok = proj_packed_size(&l1) <= PROJC_PFX_OFF ? proj_pack(&ls, &l1) && proj_import(&l2, &ls, sizeof ls) :
                                                       !proj_pack(&ls, &l1) && proj_full;
            bad += check("0.8.5: long lyrics on a full section: packed whole, or SECTION FULL (never cut)", ok);
        }
        {   /* (JIANT 0.6.3) the SLICER's bus: its globals round trip; a project of before it: from the tracks' SLICERs */
            static project_t s1, s2;
            s1 = f;
            s1.g[G_SLMODE] = 2; s1.g[G_SLPAT] = 9; s1.g[G_SLRATE] = 4; s1.g[G_SLDEP] = 77;
            for (k = 0; k <= NTRK; k++) s1.g[G_SLT1 + k] = (int16_t)(k & 1u);
            s1.g[G_SLPLEN] = 5; s1.g[G_SLPQ] = 0;
            for (k = 0; k < 16u; k++) s1.g[G_SLP0 + k] = (int16_t)((int)k * 3 - 24);
            s1.sum = proj_sum(&s1);
            ok = proj_pack(&fs, &s1) && proj_import(&s2, &fs, sizeof fs) &&
                 !memcmp(&s2.g[G_SLMODE], &s1.g[G_SLMODE], G_SL_N * sizeof s1.g[0]);
            bad += check("FUNC: the SLICER bus (mode, pattern, rate, depth, T1..T6 FX, pitch LEN QNT, 16 steps) round trips", ok);
        }
        {   /* damaged on purpose with the hash made right: refused or loaded, never read past the room (ASan) */
            static project_store_t z;
            uint32_t r = 12345u, it, okz = 1, loaded = 0;
            for (it = 0; it < 20000u; it++) {
                uint32_t hz, nfl = 1u + (r >> 29), q2i;
                z = fs;
                for (q2i = 0; q2i < nfl; q2i++) {
                    r = r * 1664525u + 1013904223u;
                    z.raw[68u + (r >> 8) % (PROJC_PFX_OFF - 68u)] = (uint8_t)(r >> 24);
                }
                hz = proj_hash(z.raw, PROJ_STORE_SIZE - 4u); memcpy(z.raw + PROJ_STORE_SIZE - 4u, &hz, 4);
                if (proj_import(&h, &z, sizeof z)) {
                    loaded++;
                    okz &= proj_ok(&h);
                }
            }
            bad += check("FUNC: 20000 damaged sections (hash right): refused or loaded valid, nothing read past", okz && loaded < 20000u);
        }
        fs.raw[100] ^= 1u;
        bad += check("FUNC: a byte changed: the hash refuses it", !proj_import(&g, &fs, sizeof fs));
        for (k = 0, n = 1; k < NTRK; k++)               /* every step of every track a chord: too much for one slot */
            for (i = 0; i < NSTEP; i++) {
                f.t[k].step[i].time = ST_NOTE; f.t[k].step[i].n = 4; f.t[k].step[i].hit = 3; f.t[k].step[i].acc = 1;
                step_set_chance(&f.t[k].step[i], 50u); step_set_ratchet(&f.t[k].step[i], 4u);
            }
        f.sum = proj_sum(&f);
        ok = !proj_pack(&fs, &f) && proj_full;
        f.t[0].step[0].n = 9;                           /* (a value out of range: not for room) */
        ok &= !proj_pack(&fs, &f) && !proj_full;
        bad += check("FUNC: six full tracks do not fit: not packed, proj_full (SECTION FULL); a bad value: not proj_full", ok && n);
    }
    printf("%s\n", bad ? "PROJECT FORMAT TEST FAILED" : "project format test passed");
    return bad != 0;
}
