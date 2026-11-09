/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* SAVE > PROJECT > SAVE SONG AS. Main loop, stopped only. song_keep and proj_wire are existing work
 * buffers: no new audio memory. The source stays open and untouched. Each destination section has the
 * storage layer's A/B protection; the four sections and index are NOT one atomic transaction. On an
 * interrupted overwrite the source remains available to retry the complete copy. */

/* Normalize a source section into proj_wire, including current song-wide globals (and the master EQ, TAPE). 0 empty, 1 ready,
 * -1 cannot serialize. song_keep holds the unsaved live section for the duration of this operation. */
BTAPP_FN static int song_copy_section(uint32_t section, uint32_t current, const char *name)
{
    project_t *p = &proj_scratch;
    if (song_save_loop) {
        if (section) return 0;
        if (!proj_import(p, &song_keep, sizeof song_keep)) return -1;
    } else if (!proj_import(p, section == current ? &song_keep : &proj_slot[section], sizeof(project_store_t))) {
        if (section || !proj_import(p, &song_keep, sizeof song_keep)) return 0;
    }
    for (uint32_t i = 0; i < G_COUNT; i++)
        p->g[i] = mom_base(MOM_G, i, song.g[i]);
    p->g[G_DJF] = mom_base(MOM_G, G_DJF, motion_djf_value());
    proj_master_put(p);                          /* the master's EQ and TAPE, song-wide too */
    if (song_save_loop) chain_defaults(&p->chain); else p->chain = chain_config;
    p->chain.rsv[1] = 0;                         /* saved sections belong to the player, not GEN */
    p->chain.rsv[2] = SONG_MODE_TAG | (!song_save_loop && live.mode ? 1u : 0u);
    if (section == 0u && name) {
        memset(p->name, 0, sizeof p->name);
        memcpy(p->name, name, str_len(name) < PROJ_NAME_LEN ? str_len(name) : PROJ_NAME_LEN);
    }
    p->sum = proj_sum(p);
    return proj_pack(&proj_wire, p) ? 1 : -1;
}

/* Saved index and one destination's metadata; the live draft keeps its own row data. */
static int song_saved_index(song_index_t *index)
{
#if FELUCCA_FLASH
    if (song_draft) return st_load(OBJ_SONGIDX, index, sizeof *index) != sizeof *index || index->magic != SONG_MAGIC;
#endif
    memcpy(index, &song_idx, sizeof *index);
    index->rows[song_cur] = chain_config;
    return 0;
}
static void song_index_target(const song_index_t *index, uint32_t target)
{
    song_idx.rows[target] = index->rows[target];
    memcpy(song_idx.scene[target], index->scene[target], sizeof index->scene[target]);
    memcpy(song_idx.tsrc[target], index->tsrc[target], sizeof index->tsrc[target]);
    memcpy(song_idx.tvar[target], index->tvar[target], sizeof index->tvar[target]);
}

static __attribute__((noinline)) int song_save_as(uint32_t target, const char *name)
{
#if FELUCCA_FLASH
    uint32_t section, current, mask = 0;
    song_index_t *index = (song_index_t *)proj_wire_u.raw;
    _Static_assert(sizeof(song_index_t) <= sizeof proj_wire_u.raw, "song index fits existing staging RAM");
    if (transport_busy()) { ui_message("STOP TO SAVE"); return 1; }
    if (!bt_hw_seg()) { seg_missing(); return 1; }
    if (target >= NSONG) { ui_message("CHOOSE ANOTHER PROJECT"); return 1; }
    if (!flash_ok) { ui_message("NO FLASH: NOT SAVED"); return 1; }
    current = live.cur >= 0 && live.cur < 4 ? (uint32_t)live.cur : proj_cur < 4u ? proj_cur : 0u;
    project_capture(&proj_scratch);
    if (!proj_pack(&song_keep, &proj_scratch)) {
        ui_message(proj_full ? "SECTION FULL" : "SAVE FORMAT ERROR");
        return 2;
    }
    /* Validate every section and every row's per-track sources BEFORE touching the destination. */
    for (section = 0; section < 4u; section++) {
        int rc = song_copy_section(section, current, name);
        if (rc < 0) { ui_message("SECTION FULL"); return 2; }
        if (rc) mask |= 1u << section;
    }
    if (!song_save_loop && !chain_valid(&chain_config)) { ui_message("INVALID ARRANGEMENT"); return 2; }
    for (uint32_t row = 0; !song_save_loop && row < chain_config.count; row++) {
        if (!(mask & (1u << chain_config.row[row].slot))) { ui_message("MISSING SECTION"); return 2; }
        for (uint32_t track = 0; track < NTRK; track++)
            if (!(mask & (1u << row_src(row, track, chain_config.row[row].slot)))) {
                ui_message("MISSING SECTION"); return 2;
            }
    }
    if (song_saved_index(index)) { ui_message("SAVE INDEX ERROR"); return 2; }
    proj_wire_gen++;                             /* invalidate editor staging and the library, even on failure */
    for (section = 0; section < 4u; section++) {
        uint32_t obj = target ? OBJ_SONG1 + (target - 1u) * 4u + section : OBJ_PROJECT0 + section;
        int rc = song_copy_section(section, current, name);
        if (rc < 0) { ui_message("SAVE FORMAT ERROR"); return 2; }
        /* A zero-length committed object hides both older copies of an absent section. */
        if (st_save(obj, &proj_wire, rc ? sizeof proj_wire : 0u)) {
            ui_message("SAVE ERROR: RETRY"); return 2;
        }
    }
    if (song_saved_index(index)) { ui_message("SAVE INDEX ERROR"); return 2; }
    if (song_save_loop) {
        chain_defaults(&index->rows[target]);
        memset(index->scene[target], 0, sizeof index->scene[target]);
        memset(index->tsrc[target], 0, sizeof index->tsrc[target]);
        memset(index->tvar[target], 0, sizeof index->tvar[target]);
    } else {
        index->rows[target] = chain_config;
        memcpy(index->scene[target], song_idx.scene[song_cur], sizeof index->scene[target]);
        memcpy(index->tsrc[target], song_idx.tsrc[song_cur], sizeof index->tsrc[target]);
        memcpy(index->tvar[target], song_idx.tvar[song_cur], sizeof index->tvar[target]);
    }
    index->rows[target].rsv[1] = 0;
    index->rows[target].rsv[2] = SONG_MODE_TAG | (!song_save_loop && live.mode ? 1u : 0u);
    if (st_save(OBJ_SONGIDX, index, sizeof *index)) { ui_message("SAVE ERROR: RETRY"); return 2; }
    /* RESTORE LAST must not mask an explicit overwrite of the open project
     * with a stale pre-generation session at the next boot. song_keep is the
     * valid, already-packed live music; writing it does not clobber index. */
    if (target == song_cur && st_save(OBJ_AUTOSAVE, &song_keep, sizeof song_keep)) {
        ui_message("SAVE ERROR: RETRY"); return 2;
    }
    /* Keep the live draft metadata while updating the destination's library entry. */
    if (!song_draft || target != song_cur) song_index_target(index, target);
    if (!song_draft && target == song_cur) {
        for (section = 0; section < 4u; section++) {
            int rc = song_copy_section(section, current, name);
            if (rc > 0) memcpy(&proj_slot[section], &proj_wire, sizeof proj_wire);
            else memset(&proj_slot[section], 0, sizeof proj_slot[section]);
        }
        chain_config = song_idx.rows[target];
        live.mode = !song_save_loop && live.mode;
        if (song_save_loop) live.cur = proj_cur = 0;
        live.dirty = 0;
    }
    song_idx_dirty = 0;
    { char b[4]; fmt_int(b, (int32_t)target + 1); ui_say(song_save_loop ? "SAVED LOOP " : "SAVED SONG ", b); }
    ui.force = 1;
    return 0;
#else
    (void)target; (void)name;
    ui_message("NO FLASH: NOT SAVED");
    return 1;
#endif
}

/* Drop rows which would reference an erased part, moving scene/source/variation together. */
BTAPP_FN static void song_remove_parts(song_index_t *index, uint32_t target, uint32_t mask)
{
    chain_config_t *c = &index->rows[target];
    uint32_t r, w = 0, k;
    for (r = 0; r < c->count; r++) {
        uint32_t used = 1u << c->row[r].slot;
        for (k = 0; k < NTRK; k++) {
            uint32_t src = index->tsrc[target][r] >> (3u * k) & 7u;
            if (src && src <= 4u) used |= 1u << (src - 1u);
        }
        if (used & mask) continue;
        c->row[w] = c->row[r];
        index->scene[target][w] = index->scene[target][r];
        index->tsrc[target][w] = index->tsrc[target][r];
        index->tvar[target][w++] = index->tvar[target][r];
    }
    c->count = (uint8_t)w; c->rsv[1] &= (uint8_t)~mask;
    if (!w) c->rsv[2] = SONG_MODE_TAG;
    for (; w < CHAIN_ROWS; w++) {
        c->row[w] = (chain_row_t){0,1};
        memset(&index->scene[target][w], 0, sizeof(scene_t));
        index->tsrc[target][w] = index->tvar[target][w] = 0;
    }
}
/* parts=0 erases a saved project; a part in a draft is RAM-only. Tombstone
 * commits preserve each object's previous copy on a failed write. Multi-part
 * erasure is not atomic across a power failure; a failed current-project erase
 * keeps its RAM and queues completed parts for restoration. */
static int song_erase_parts(uint32_t target, uint32_t parts)
{
    song_index_t *index = (song_index_t *)proj_wire_u.raw;
    uint32_t mask = parts ? parts : 15u, done = 0, k;
    int draft = parts && song_draft && target == song_cur;
    if (!bt_hw_seg()) { seg_missing(); return 1; }
    if (target >= NSONG || transport_busy()) { ui_message("STOP TO ERASE"); return 1; }
    if (draft) { memcpy(index, &song_idx, sizeof *index); index->rows[target] = chain_config; }
    else if (song_saved_index(index)) { ui_message("SAVE INDEX ERROR"); return 1; }
    song_remove_parts(index, target, mask);
#if FELUCCA_FLASH
    if (!draft && !flash_ok) { ui_message("NO FLASH: NOT ERASED"); return 1; }
    if (!draft && flash_ok) {
        for (k = 0; k < 4u; k++) if ((mask >> k) & 1u) {
            uint32_t obj = target ? OBJ_SONG1 + (target - 1u) * 4u + k : OBJ_PROJECT0 + k;
            if (st_save(obj, 0, 0)) goto failed;
            done |= 1u << k;
        }
        if (!parts && target == song_cur && st_save(OBJ_AUTOSAVE, 0, 0)) goto failed;
        if (st_save(OBJ_SONGIDX, index, sizeof *index)) goto failed;
    }
#endif
    if (!song_draft || target != song_cur || draft) song_index_target(index, target);
    if (target == song_cur && (!song_draft || draft)) {
        chain_config = song_idx.rows[target];
        for (k = 0; k < 4u; k++) if ((mask >> k) & 1u) memset(&proj_slot[k], 0, sizeof proj_slot[k]);
        live.dirty &= (uint8_t)~mask;
        if (live.cur >= 0 && ((mask >> live.cur) & 1u)) live.cur = -1;
        if (!chain_config.count) live.mode = 0;
        if (!parts) { system_init(); proj_cur = PROJ_NO_SLOT; live.cur = -1; }
        ui.song_row = 0;
    }
    proj_wire_gen++; ui.force = 1;
    ui_message(parts ? "PART ERASED" : "PROJECT ERASED");
    return 0;
#if FELUCCA_FLASH
failed:
    if (target == song_cur && !song_draft) live.dirty |= (uint8_t)done;
    proj_wire_gen++;
    ui_message("ERASE ERROR: RETRY"); return 2;
#endif
}
