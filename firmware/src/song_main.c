/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT (FELUCCA TONIC): after SLOOP 2.4.1's song (isod89/sloop-fm1, GPL-3.0). */
/* The song, the main loop's part (song_chain.c: what it is). Included by project.c. */
static project_store_t song_keep __attribute__((section(".pool")));   /* the music as it was before PLAY */

/* ---- the songs (NSONG, four sections each: storage.c) */
static uint8_t song_idx_dirty;                    /* the index changed (rows, the current song), not in flash yet */
static uint32_t song_idx_t;                       /* .. since (fm1_ms): written once the rows rest a while */

static void song_rows_fix(chain_config_t *c)      /* a song's rows as chain_config takes them */
{
    if (!chain_valid(c))
        chain_defaults(c);
}
/* persist_boot, before the slots are filled: the index from flash (none or broken: song 1, its rows taken from its
 * section A at song_index_boot: Felucca's song, kept in the project) */
static void song_index_load(void)
{
    uint32_t k;
#if FELUCCA_FLASH
    int n = st_load(OBJ_SONGIDX, &song_idx, sizeof song_idx);
    if (song_idx.magic == SONG_MAGIC && song_idx.cur < NSONG_IDX &&
        ((song_idx.version == SONG_IDX_V && n == (int)sizeof song_idx) ||
         (song_idx.version == 3u && n == (int)__builtin_offsetof(song_index_t, tvar)) ||
         (song_idx.version == 2u && n == (int)__builtin_offsetof(song_index_t, tsrc)) ||
         (song_idx.version == 1u && n == (int)__builtin_offsetof(song_index_t, scene)))) {
        if (song_idx.version == 1u)                 /* (before the scenes: none) */
            memset(song_idx.scene, 0, sizeof song_idx.scene);
        if (song_idx.version < 3u)                  /* (add15: before ARRANGE 2, every track its row's section) */
            memset(song_idx.tsrc, 0, sizeof song_idx.tsrc);
        if (song_idx.version < 4u)                  /* (add15-beta4: before VAR, no variations) */
            memset(song_idx.tvar, 0, sizeof song_idx.tvar);
        song_idx.version = SONG_IDX_V;
        for (k = 0; k < NSONG_IDX; k++)
            song_rows_fix(&song_idx.rows[k]);
        if (song_idx.cur >= NSONG)                  /* (V10.6p) an older index on song 7 or 8 (retired): song 6 */
            song_idx.cur = NSONG - 1u;
        song_cur = song_idx.cur;
        return;
    }
#endif
    memset(&song_idx, 0, sizeof song_idx);
    song_idx.magic = SONG_MAGIC;
    song_idx.version = SONG_IDX_V;
    for (k = 0; k < NSONG_IDX; k++)
        chain_defaults(&song_idx.rows[k]);
    song_cur = 0;
    song_idx.rsv[0] = 1;                          /* (no index yet: song_index_boot takes section A's rows) */
}
#if FELUCCA_FLASH
/* (V10.6p) power-on (autosave_boot), once: songs 7 and 8 retired (6 songs), their sections an older JIANT stored still
 * in their flash (FL_SAMPLES, a commit record of one of their objects: OBJ_COUNT..+7): said; checked once (the index's
 * rsv[1], written with it) */
static __attribute__((noinline)) void song_retired(void)   /* (out of line: fm1_main's room is pinned) */
{
    uint32_t i, h[2];
    if (song_idx.rsv[1])
        return;
    song_idx.rsv[1] = 1;
    for (i = 0; i < FL_SAMPLES_SIZE / ST_SECTOR; i++)
        if (!st_read(FL_SAMPLES + i * ST_SECTOR, h, sizeof h) && h[0] == ST_MAGIC && (uint16_t)(h[1] - OBJ_COUNT) < 8u) {
            ui_message("SONGS 7-8 RETIRED");
            str_cpy(ui.msg2, "6 SONGS NOW: SEE MANUAL", sizeof ui.msg2);
            song_idx_touch();
            return;
        }
}
#endif
/* persist_boot, after the slots: the current song's rows */
static void song_index_boot(void)
{
    if (song_idx.rsv[0]) {
        song_idx.rsv[0] = 0;
        if (proj_import(&proj_scratch, &proj_slot[0], sizeof(project_store_t)) && chain_valid(&proj_scratch.chain))
            song_idx.rows[0] = proj_scratch.chain;
    }
    chain_config = song_idx.rows[song_cur];
    song_mode_restore();
}
/* the current song's name: its section A's, else SONG n (b: PROJ_NAME_LEN + 1 bytes) */
static void song_name(char *b)
{
    if (!project_name(0, b) || !b[0]) {
        str_cpy(b, "SONG ", PROJ_NAME_LEN + 1u);
        b[5] = (char)('1' + song_cur);
        b[6] = 0;
    }
}

/* (add15-beta4) ARRANGE 2's VAR: track k's steps (the first len) as row `row` plays them. The same row, the same
 * variation: VAR 1..4 draw from a seed of the song, the row and the track, never rng() */
static void row_vary(step_t *st, uint32_t len, int kit, uint32_t v, uint32_t seed)
{
    static step_t tmp[NSTEP] __attribute__((section(".pool")));   /* (the main loop's, scratch) */
    uint32_t i, h, n = len > NSTEP ? NSTEP : (len ? len : 1u), half = n / 2u ? n / 2u : 1u;
    int32_t last = -1;
    if (v == RV_NONE || v == RV_MUTE)
        return;
    memcpy(tmp, st, n * sizeof *st);
    for (i = 0; i < n; i++) {
        step_t s = tmp[i];
        h = (seed ^ (i + 1u) * 2654435761u) * 2246822519u;
        h ^= h >> 15;
        switch (v) {
        case RV_THIN:                                   /* the off-beats out (the 16ths between the 8ths) */
            if (i & 1u) { s.time = ST_REST; s.n = 0; s.hit = s.acc = 0; }
            break;
        case RV_FILL:                                   /* the last beat: every step plays (a roll into the next row) */
            if (i + 4u >= n) {
                if (kit) { s.hit |= (uint8_t)((tmp[n - 1u].hit | 2u) & ~1u); s.time = ST_NOTE; }   /* (lane 1 and the last
                                                         * step's, never the kick) */
                else if (last >= 0 && s.time != ST_NOTE) { memset(&s, 0, sizeof s); s.n = 1; s.time = ST_NOTE; s.vel = 100; s.note[0] = (uint8_t)last; }
            }
            break;
        case RV_HALF: s = tmp[i % half]; break;         /* the first half twice */
        case RV_REV: s = tmp[n - 1u - i]; if (s.time == ST_TIE) s.time = ST_NOTE; break;
        case RV_SHIFT: s = tmp[(i + n - (2u % n)) % n]; break;   /* an 8th late, round in LEN */
        case RV_OCTU: case RV_OCTD:
            if (!kit) for (uint32_t j = 0; j < s.n && j < 4u; j++) s.note[j] = (uint8_t)clamp(s.note[j] + (v == RV_OCTU ? 12 : -12), 0, 127);
            break;
        default: {                                      /* VAR 1..4: hits / notes moved, dropped, added */
            uint32_t amt = 40u + 30u * (v - RV_MUT1);   /* (of 256) */
            if (kit) {
                for (uint32_t l = 1; l < NLANE; l++)
                    if (((h >> (l * 3u)) & 255u) < amt / 2u) s.hit ^= (uint8_t)(1u << l);
                s.acc &= s.hit;
                s.time = s.hit || s.n ? ST_NOTE : ST_REST;
            } else if (s.time == ST_NOTE && s.n) {
                if ((i & 3u) && (h & 255u) < amt / 2u) { s.time = ST_REST; s.n = 0; }
                else if (((h >> 8) & 255u) < amt / 2u)
                    for (uint32_t j = 0; j < s.n && j < 4u; j++) s.note[j] = (uint8_t)clamp(s.note[j] + ((h >> 16) & 1u ? 12 : 7), 0, 127);
            } else if (s.time == ST_REST && (i & 1u) && last >= 0 && (h & 255u) < amt / 3u) {
                memset(&s, 0, sizeof s); s.n = 1; s.time = ST_NOTE; s.vel = 90; s.note[0] = (uint8_t)last;
            }
        } }
        if (s.time == ST_NOTE && s.n) last = s.note[0];
        st[i] = s;
    }
}

/* section s (a project slot) into sec_stage for row `row` (SEC_LIVE: a live jump); 0 = staged. (add15) A song's row may
 * take tracks from other sections (ARRANGE 2: row_src): each of those is decoded in turn, its tracks (sound, steps, FM6
 * patch, automation; DRUM's kit with the DRUM track) taken from it; the row's own section last, with the rest.
 * (V10.6s) clip_mix (CLIPS): the row's own is the music as it is (song_keep, 4), the tracks asked for (clip_go) from
 * their parts, a track asked to stop muted; s the section the stage is said to be (SONG REC's row: its tracks' tsrc) */
static int song_stage(uint32_t s, uint32_t row)
{
    project_t *p = &proj_scratch;
    uint32_t own = clip_mix ? 4u : s & 3u, k, i, u, n = 0, on = 0, dx_from = own, use = 0, srcs[NTRK], src = 0;
    sec_stage.ready = 0;
    for (k = 0; k < NTRK; k++) {
        uint32_t c = (uint32_t)(clip_go[k] ? clip_go[k] : clip_now[k]) - 1u;
        srcs[k] = clip_mix ? ((uint32_t)clip_go[k] - 1u < 4u ? clip_go[k] - 1u : 4u) :
                  row < CHAIN_ROWS && !chain.loop ? row_src(row, k, s) : s & 3u;
        use |= 1u << srcs[k];
        if (clip_mix && c < 4u && c != (s & 3u))
            src |= (c + 1u) << (3u * k);
    }
    for (u = 0; u < 5u; u++) {
        uint32_t sec = u < 4u ? u : own;
        if (u < 4u && (sec == own || !((use >> sec) & 1u)))
            continue;                                 /* (the row's own goes last: its globals) */
        if (!proj_import(p, sec < 4u ? &proj_slot[sec] : &song_keep, sizeof(project_store_t)))
            return 1;
        for (k = 0; k < NTRK; k++) {
            sec_trk_t *d = &sec_stage.t[k];
            const proj_trk_t *t = &p->t[k];
            uint32_t e = eng_live(t->engine);
            if (srcs[k] != sec)
                continue;
            sec_stage_fit[k] = fit_valid(p->fit[k]);
            d->engine = (uint8_t)e;
            d->preset = (uint8_t)(ENGINES[e]->npresets ? (t->preset >= PROJ_DEF_KEEP ? 0u : t->preset) % ENGINES[e]->npresets
                                                       : 0u);
            for (i = 0; i < P_COUNT; i++)
                d->p[i] = (int16_t)param_fit(param_desc_of(e, i), t->p[i]);
            memcpy(d->step, t->step, sizeof d->step);
            proj_steps(d->step);
            if (row < CHAIN_ROWS && !chain.loop) {      /* (add15-beta4) ARRANGE 2's VAR */
                uint32_t v = row_var(row, k);
                row_vary(d->step, (uint32_t)d->p[P_SLEN], e == ENGI_DRUM, v,
                         (song_cur + 1u) * 40503u ^ (row + 1u) * 104729u ^ (k + 1u) * 1299709u ^ v * 15485863u);
                if (v == RV_MUTE) d->p[P_MUTE] = 1;
            }
            if (clip_mix && clip_go[k] == CLIP_STOP)    /* (V10.6s) CLIPS' OCT-: the track stops */
                d->p[P_MUTE] = 1;
            fm6_unpack(p->fm6[k], d->fm6);
            fm6_sanitize(d->fm6);
            on |= p->motion.on & (1u << k);
            if (e == ENGI_DRUM && sec != own)
                dx_from = sec;
        }
        for (i = 0; i < p->motion.count && i < MOTION_MAX && n < MOTION_MAX; i++)
            if (srcs[MOTION_TRK(&p->motion.event[i]) % NTRK] == sec)
                sec_stage.motion.event[n++] = p->motion.event[i];
        if (sec == dx_from) {
            memcpy(sec_stage.dx, p->dx, sizeof sec_stage.dx);
            sec_stage.dx_mute = (uint16_t)(p->dx_mute & DXM_ALL);
        }
    }
    memset(sec_stage.motion.event + n, 0, (MOTION_MAX - n) * sizeof sec_stage.motion.event[0]);
    sec_stage.motion.count = (uint8_t)n;
    sec_stage.motion.on = (uint8_t)(on | (p->motion.on & ~((1u << NTRK) - 1u)));
    memcpy(sec_stage.motion.rsv, p->motion.rsv, sizeof sec_stage.motion.rsv);
    memcpy(sec_stage.pfx_lane, p->pfx_lane, sizeof sec_stage.pfx_lane);
    memcpy(sec_stage.slg, &p->g[G_SLMODE], sizeof sec_stage.slg);   /* (0.6.3) the section's SLICER */
    sec_stage.pfx_ltgt = p->pfx_ltgt > 2u ? 0u : p->pfx_ltgt;
    sec_stage.row = (uint8_t)row;
    sec_stage.section = (uint8_t)(s & 3u);
    sec_stage.src = src;
    RING_PUBLISH();
    sec_stage.ready = 1;
    return 0;
}

/* the bars section s's loop takes: its longest pattern (LEN steps of its DIV) in bars, rounded up, 1..CHAIN_BARS */
static uint32_t section_bars(uint32_t s)
{
    project_t *p = &proj_scratch;
    uint32_t k, bars = 1, bar = 16u * div_samples(2);
    if (!bar || !proj_import(p, &proj_slot[s & 3u], sizeof(project_store_t)))
        return 1;
    for (k = 0; k < NTRK; k++) {
        uint32_t len = (uint32_t)clamp(p->t[k].p[P_SLEN], 1, NSTEP);
        uint32_t span = p->fit[k] & 128u ? fit_value_span(p->fit[k]) : len;
        uint32_t b = (span * div_samples((uint32_t)p->t[k].p[P_SDIV]) + bar - 1u) / bar;
        bars = b > bars ? b : bars;
    }
    return bars > CHAIN_BARS ? CHAIN_BARS : bars;
}
/* (V10.6o) PARTS / TIMELINE: section s as proj_scratch holds it (just decoded: graph_project_used): its tracks that
 * play (part_play) */
static uint32_t part_scan(uint32_t s)
{
    const project_t *p = &proj_scratch;
    uint32_t k, i, n, m = 0;
    for (k = 0; k < NTRK; k++) {
        n = (uint32_t)clamp(p->t[k].p[P_SLEN], 1, NSTEP);
        for (i = 0; i < n && p->t[k].step[i].time != ST_NOTE; i++) {}
        if (i < n && !p->t[k].p[P_MUTE])
            m |= 1u << k;
    }
    return m;
}

/* Main loop only: PLAY a song (V10.6o: from row r0). 0 = armed, 1 = no rows, 2 = busy, 3 + s = section s not saved */
static uint32_t chain_prepare_at(uint32_t r0)
{
    uint32_t i;
    if (transport_busy())
        return 2;
    if (!chain_valid(&chain_config) || r0 >= chain_config.count)
        return 1;
    for (i = 0; i < chain_config.count; i++)
        if (!proj_import(&proj_scratch, &proj_slot[chain_config.row[i].slot], sizeof(project_store_t)))
            return 3u + chain_config.row[i].slot;
    project_capture(&proj_scratch);                   /* the music as it is: back when the song ends */
    chain.kept = proj_pack(&song_keep, &proj_scratch) ? (uint8_t)(1u | ((uint32_t)(live.cur + 1) << 1)) : 0u;
    chain.config = chain_config;
    chain.loop = 0;
    chain.pass = 0;
    chain.ended = 0;
    if (song_stage(chain.config.row[r0].slot, r0))
        return 3u + chain.config.row[r0].slot;
    RING_PUBLISH();
    chain.armed = 1;
    transport_req = 1;
    return 0;
}

/* section s as the music now, the song's rows kept (a section is a project: its own rows are not the song's) */
static int section_load(uint32_t s)
{
    chain_config_t keep = chain_config;
    uint8_t row = ui.song_row;
    int rc;
    proj_lfo_keep = 1;                                  /* (0.9.18: the song plays on, MODE FREE's LFOs not faded again) */
    rc = !proj_import(&proj_scratch, &proj_slot[s & 3u], sizeof(project_store_t)) || project_restore_runtime(&proj_scratch);
    proj_lfo_keep = 0;
    if (rc)
        return 1;
    chain_config = keep;
    ui.song_row = row;
    proj_cur = (uint8_t)(s & 3u);
    live.cur = (int8_t)(s & 3u);
    memset(clip_now, (int)(s & 3u) + 1, sizeof clip_now);   /* (V10.6s) CLIPS: every track that part's */
    memset(clip_go, 0, sizeof clip_go);
    sec_stage.src = 0;                                  /* (SONG REC's first row: the whole section) */
    return 0;
}

/* (V10.6o) what plays stops being a song or a quick chain (the music it brought plays on, the song's REC arming back);
 * a live jump asked for forgotten */
static void chain_cut(void)
{
    fm1_irq_off();                                    /* (the ISR may be applying one) */
    if (chain.running && !chain.loop) {
        song.rec = chain.rec;
        scene_pfx = 0;
    }
    chain.running = 0;
    chain.armed_bar = 0;
    chain.hold = 0;
    live.req = -1;
    fm1_irq_on();
}
/* (V10.6o) the song from row r (PARTS' OCT-, TIMELINE's PLAY): playing, on the next bar; stopped, now. hold: that row
 * round and round until PLAY lets it go (tl_play). 0 done */
static int chain_goto(uint32_t r, uint32_t hold)
{
    if (!song.playing) {
        uint32_t rc = transport_busy() ? 2u : chain_prepare_at(r);
        if (rc) {
            ui_message(rc == 1u ? "ADD A SONG ROW" : rc == 2u ? "STOP FIRST" : "SECTION EMPTY");
            return 1;
        }
    } else {
        if (r >= chain_config.count || !chain_valid(&chain_config)) { ui_message("ADD A SONG ROW"); return 1; }
        chain_cut();
        chain.config = chain_config;
        chain.loop = 0;
        chain.pass = 0;
        chain.ended = 0;
        chain.kept = 0;
        chain.scn = 0;
        if (song_stage(chain.config.row[r].slot, r)) { ui_message("SECTION EMPTY"); return 1; }
        RING_PUBLISH();
        chain.armed_bar = 1;                          /* (chain_tick: on the next bar) */
    }
    chain.hold = (uint8_t)hold;
    return 0;
}
/* the song layer (ui_layer.c), main loop. Section s: in on the next bar (stopped: now). 0 done */
static int section_jump(uint32_t s)
{
    char b[8] = "A";
    s &= 3u;
    b[0] = (char)('A' + s);
    if (chain.running && !chain.loop) {
        ui_message("STOP THE SONG FIRST");
        return 1;
    }
    if (!project_used(s)) {
        ui_say(b, " IS EMPTY");
        return 1;
    }
    if (!song.playing && !transport_busy()) {
        if (section_load(s))
            return 1;
        ui_say("SECTION ", b);
        return 0;
    }
    chain_cut();                                      /* (a single section ends a quick chain) */
    if (song_stage(s, SEC_LIVE))
        return 1;
    live.req = (int8_t)s;
    ui_say(b, " NEXT BAR");
    return 0;
}
/* (V10.6s) SAVE > CLIPS: track k's cell of part s in on the next bar (stopped: now, and PLAY): the other tracks play on
 * as they are (the music now, song_keep, the stage's own: their sounds, steps, motion; the kit unless the DRUM track's
 * cell goes in), what plays stops being a song. s 4: the track stops (muted on the bar); k NTRK: every track, the part
 * (a live jump: section_jump). Cells asked for in one bar go in together. 0 done */
static int clip_launch(uint32_t k, uint32_t s)
{
    uint32_t sec = live.cur >= 0 ? (uint32_t)live.cur : s & 3u, i;
    if (s < 4u && !project_used(s)) {
        ui_message("PART EMPTY");
        return 1;
    }
    chain_cut();
    live.mode = 0;
    if (k >= NTRK) {
        if (section_jump(s))
            return 1;
        if (song.playing)
            memset(clip_go, (int)s + 1, sizeof clip_go);
    } else {
        project_capture(&proj_scratch);
        if (!proj_pack(&song_keep, &proj_scratch)) {
            ui_message("SECTION FULL");
            return 1;
        }
        clip_go[k] = (uint8_t)(s + 1u);
        clip_mix = 1;
        i = (uint32_t)song_stage(sec, SEC_LIVE);
        clip_mix = 0;
        if (i) {
            clip_go[k] = 0;
            return 1;
        }
        if (song.playing) {
            live.req = (int8_t)sec;
            ui_message("NEXT BAR");
        } else {
            fm1_irq_off();                              /* (stopped: in now, as the ISR would) */
            sec_apply();
            fm1_irq_on();
        }
    }
    if (!song.playing && !transport_busy())
        transport_req = 1;
    return 0;
}
/* (song_poll) what went in: each track's part (CLIPS' cells asked for, else the section) */
static void clip_seen(void)
{
    uint32_t k, any = 0;
    for (k = 0; k < NTRK; k++)
        any |= clip_go[k];
    for (k = 0; k < NTRK; k++) {
        if (clip_go[k] || !any)
            clip_now[k] = clip_go[k] ? clip_go[k] : (uint8_t)(((uint32_t)live.cur & 3u) + 1u);
        clip_go[k] = 0;
    }
}

/* the music now into section s: RAM at once (playing too), flash once stopped (song_poll) */
static void section_store(uint32_t s)
{
    char b[8] = "A";
    s &= 3u;
    dice_secs &= (uint8_t)~(1u << s);                 /* (0.6.81) a STORE: the section yours (DICE ALL marks its own after) */
    b[0] = (char)('A' + s);
    project_capture(&proj_scratch);
    if (!proj_pack(&proj_wire, &proj_scratch)) {        /* (0.6: packed aside first, the section kept if it does not fit) */
        ui_message(proj_full ? "SECTION FULL" : "SAVE FORMAT ERROR");
        return;
    }
    proj_wire_gen++;
    memcpy(&proj_slot[s], &proj_wire, sizeof proj_slot[s]);
    live.cur = (int8_t)s;
    live.dirty |= (uint8_t)(1u << s);
    ui_say("STORED ", b);
}
/* RECALL: the section playing back as it was stored */
static void section_recall(void)
{
    if (live.cur < 0) {
        ui_message("NO SECTION YET");
        return;
    }
    if (!section_jump((uint32_t)live.cur))
        str_cpy(ui.msg2, "RECALL", sizeof ui.msg2);
}
/* a quick chain of n sections (n >= 2), each its longest pattern's bars, round and round */
static void quick_chain(const uint8_t *s, uint32_t n)
{
    uint32_t i;
    if (chain.running && !chain.loop) {
        ui_message("STOP THE SONG FIRST");
        return;
    }
    for (i = 0; i < n; i++)
        if (!project_used(s[i])) {
            char b[8] = "A";
            b[0] = (char)('A' + (s[i] & 3u));
            ui_say(b, " IS EMPTY");
            return;
        }
    chain_cut();
    chain_defaults(&chain.config);
    chain.config.count = (uint8_t)n;
    for (i = 0; i < n; i++) {
        chain.config.row[i].slot = (uint8_t)(s[i] & 3u);
        chain.config.row[i].bars = (uint8_t)section_bars(s[i]);
    }
    chain.loop = 1;
    chain.kept = 0;
    chain.ended = 0;
    if (song_stage(chain.config.row[0].slot, 0))
        return;
    RING_PUBLISH();
    if (song.playing)
        chain.armed_bar = 1;                          /* on the next bar */
    else {
        chain.armed = 1;                              /* from the top, now */
        transport_req = 1;
    }
    ui_message("QUICK CHAIN");
}
/* SONG REC on / off */
static void song_rec_toggle(void)
{
    if (chain.running && !chain.loop) {
        ui_message("STOP THE SONG FIRST");
        return;
    }
    fm1_irq_off();
    if (live.srec)
        srec_stop();
    else
        live.srec = 1;
    fm1_irq_on();
    if (live.srec)
        ui_message(song.playing ? "SONG REC NEXT BAR" : "SONG REC: PLAY");
}

/* the current song's stored sections into flash: 0 done (or no flash) */
static int song_flush(void)
{
    uint32_t k;
    if (song_draft) return 0;
#if FELUCCA_FLASH
    if (!flash_ok)
        return 0;
    for (k = 0; k < 4u; k++)
        if (((live.dirty >> k) & 1u) && st_save(proj_obj(k), &proj_slot[k], sizeof proj_slot[k]))
            return 1;
#else
    (void)k;
#endif
    live.dirty = 0;
    return 0;
}
/* (JIANT) the index changed outside the rows (a scene): into flash as the rows go (song_poll) */
static void song_idx_touch(void)
{
    song_idx_dirty = 1;
    song_idx_t = fm1_ms;
}
/* the index into flash (the songs' rows, the current song): 0 done (or no flash) */
static int song_index_save(void)
{
    if (song_draft) return 0;
    song_idx.rows[song_cur] = chain_config;
    song_idx.cur = song_cur;
#if FELUCCA_FLASH
    if (flash_ok && st_save(OBJ_SONGIDX, &song_idx, sizeof song_idx))
        return 1;
#endif
    song_idx_dirty = 0;
    return 0;
}

/* Commit the old project before borrowing its four RAM buffers. All later
 * generations are drafts until an explicit SAVE; no new audio storage. */
static int song_draft_begin(void)
{
    if (!bt_hw_seg()) { ui_message("UPDATE PACKAGE"); return 1; }
    if (transport_busy()) { ui_message("STOP TO GENERATE"); return 1; }
#if FELUCCA_FLASH
    if (!flash_ok) { ui_message("NO FLASH"); return 1; }
#endif
    if (!song_draft) {
        if (song_flush() || song_index_save()) { ui_message("SAVE ERROR"); return 1; }
        song_save_target = song_cur;
    }
    song_draft = 1;
    dice_secs = 0;
    snd_held = snd_seen = 0;                          /* (V10.6z8: a throw, the sections' sounds) */
    live.dirty = 0; live.cur = live.req = -1; sec_stage.ready = 0;
    proj_cur = PROJ_NO_SLOT; proj_name[0] = 0;
    memset(proj_slot, 0, sizeof proj_slot);
    proj_wire_gen++;
    return 0;
}

#include "song_save.c"

/* song n (0..NSONG - 1) in, stopped only: the current one kept first, then n's sections and rows; load_a: its
 * section A becomes the music (an empty song: the music stays, a new song to store sections into); else (JIANT 0.6.12,
 * SAVE > PROJECT) the music stays as it is, to be stored into one of n's sections or replaced by one. 0 done */
static int song_switch(uint32_t n, int load_a)
{
    char nm[PROJ_NAME_LEN + 1u];
    uint32_t k;
    if (n >= NSONG || (n == song_cur && !song_draft))
        return 1;
    if (transport_busy()) {
        ui_message(load_a ? "STOP TO CHANGE SONG" : "STOP TO CHANGE PROJECT");
        return 1;
    }
    if (song_draft) {
        song_draft = 0; live.dirty = 0;
        song_index_load();
    } else {
        if (song_flush()) { ui_message("SAVE ERROR"); return 1; }
        song_idx.rows[song_cur] = chain_config;
    }
    /* The original index is restored before opening another saved project. */
    song_cur = (uint8_t)n;
    chain_config = song_idx.rows[n];
    if (song_index_save()) {                          /* (failed: written again by song_poll) */
        song_idx_dirty = 1;
        song_idx_t = fm1_ms;
    }
    for (k = 0; k < 4u; k++) {
        memset(proj_slot[k].raw, 0, 4);
#if FELUCCA_FLASH
        if (flash_ok)
            proj_fetch(k);
#endif
    }
    ui.song_row = 0;
    live.cur = -1;
    live.req = -1;
    if (load_a && project_used(0))
        (void)section_load(0);
    else
        proj_cur = PROJ_NO_SLOT;
    ui.force = 1;
    if (load_a) {
        song_mode_restore();
        song_name(nm);
        ui_say(nm, "");
    } else {                                          /* PROJECT n, then its section A's name */
        char b[12] = "PROJECT  ";
        b[8] = (char)('1' + n);
        if (!project_name(0, nm) || !nm[0]) b[9] = 0;
        ui_say(b, b[9] ? nm : "");
    }
    return 0;
}
static int song_select(uint32_t n) { return song_switch(n, 1); }   /* (the song layer's KNOB 1) */
static int song_open(uint32_t n) { return song_switch(n, 0); }     /* (SAVE > PROJECT's KNOB 1) */

/* Main loop, each pass: the next section staged; the music back after the song; SONG REC's rows; stored sections
 * into flash once stopped */
static void song_poll(void)
{
    uint32_t k;
    if (chain.applied) {
        chain.applied = 0;
        clip_seen();                                  /* (V10.6s) CLIPS: what sounds now */
        for (k = 0; k < NTRK; k++)
            fm6_adopt(k);                             /* (an FM6 track's SLOT: F n or OWN) */
        sync_reload = 1;
        ui.force = 1;
    }
    if (chain.running && !sec_stage.ready) {
        uint32_t next = chain.row + !chain.hold;      /* (V10.6o: held, the row again) */
        if (next >= chain.config.count && (chain.loop || SONG_END(&chain.config) == SONG_FOREVER ||
                                           (SONG_END(&chain.config) >= 2u && chain.pass + 1u < SONG_END(&chain.config))))
            next = 0;                                 /* (0.6.22: the song's END: round again) */
        if (next < chain.config.count)
            (void)song_stage(chain.config.row[next].slot, next);
    }
    if (live.rec_done) {
        uint32_t n = live.rec_done;
        live.rec_done = 0;
        if (n == 0xFFu) {
            ui_message("NO SONG");
        } else {
            char b[8];
            chain_defaults(&chain_config);
            chain_config.count = (uint8_t)n;
            memset(song_idx.scene[song_cur], 0, sizeof song_idx.scene[song_cur]);
            memset(song_idx.tsrc[song_cur], 0, sizeof song_idx.tsrc[song_cur]);   /* (SONG REC: whole sections; */
            memset(song_idx.tvar[song_cur], 0, sizeof song_idx.tvar[song_cur]);   /* V10.6s: CLIPS' tracks FROM) */
            for (k = 0; k < n; k++) {
                chain_config.row[k] = live.rec[k];
                song_idx.tsrc[song_cur][k] = rec_src[k];
                song_idx.scene[song_cur][k] = live.rec_scene[k];   /* (each row's scene, as it began) */
            }
            song_idx_touch();
            ui.song_row = 0;
            fmt_int(b, (int32_t)n);
            ui_say("SONG ROWS ", b);
        }
    }
    if (chain.ended && !song.playing) {
        uint8_t cur = proj_cur;
        char name[PROJ_NAME_LEN + 1u];
        uint32_t held = 0;
        chain.ended = 0;
        str_cpy(name, proj_name, sizeof name);
        for (k = 0; k <= NTRK; k++)                   /* (V10.6z8) the sounds the player changed (the last section's */
            held |= snd_own(k) << k;                  /* changes too: motion_end put the sounds' own values back) */
        if (held) {                                   /* (V10.7.6) the music as the song left it, aside */
            project_capture(&proj_scratch);
            proj_wire_gen++;
        }
        if (chain.kept && (!held || proj_pack(&proj_wire, &proj_scratch)) &&
            proj_import(&proj_scratch, &song_keep, sizeof song_keep) && !project_restore_runtime(&proj_scratch)) {
            proj_cur = cur;                           /* (the music before the song: its slot and name) */
            live.cur = (int8_t)((int32_t)(chain.kept >> 1) - 1);
            str_cpy(proj_name, name, PROJ_NAME_LEN + 1u);
            ui_message("SONG END");
            if (held && proj_import(&proj_scratch, &proj_wire, sizeof proj_wire)) {   /* (V10.7.6) then the held */
                if (!(held >> NTRK & 1u))             /* sounds over it (engine, preset, sound, FM6 patch: sound_put; */
                    memcpy(proj_scratch.dx, dx_kit, sizeof proj_scratch.dx);   /* the kit when held): nothing else lost */
                for (k = 0; k < NTRK; k++)
                    if (held >> k & 1u)
                        sound_put(&proj_scratch, k);
                memcpy(dx_kit, proj_scratch.dx, sizeof dx_kit);
                snd_held = (uint8_t)held;             /* (PLAY again: still the player's) */
            }
        }
    }
    if (song_draft) return;
    chain_config.rsv[2] = SONG_MODE_TAG | (live.mode ? 1u : 0u);
    if (memcmp(&chain_config, &song_idx.rows[song_cur], sizeof chain_config)) {   /* the rows or PLAY mode changed */
        song_idx.rows[song_cur] = chain_config;
        song_idx_dirty = 1;
        song_idx_t = fm1_ms;
    }
#if FELUCCA_FLASH
    if (song_idx_dirty && flash_ok && !transport_busy() && (uint32_t)(fm1_ms - song_idx_t) >= 2000u &&
        song_index_save())
        song_idx_t = fm1_ms;                          /* (failed: tried again later) */
    if (live.dirty && flash_ok && !transport_busy())
        for (k = 0; k < 4u; k++)
            if (((live.dirty >> k) & 1u) && !st_save(proj_obj(k), &proj_slot[k], sizeof proj_slot[k]))
                live.dirty &= (uint8_t)~(1u << k);    /* (a failed write stays dirty: tried again) */
#else
    live.dirty = 0;
    song_idx_dirty = 0;
#endif
}

