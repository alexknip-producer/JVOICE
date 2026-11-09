/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT: GEN's FORM MY LOOP ("FROM LOOP" in the code): the loop that plays made a whole song by GEN's rules. Included
 * by gen.c before gen_make (whose gen_parts makes its sections and rows).
 *
 * The music as it is is the loop: its tracks, their steps and sounds stay as they are and are section A, the song's
 * peak (its drop). Read from it (gls_hear): the key (each pitch class weighed by its length, its velocity and the beat
 * it falls on; the bass's, the lowest notes', count again for the tonic; DRUM-X's tonal lanes, little noise, not METAL,
 * a LONG or long ring, and an EXTENDED kit's chromatic run, by their pitch) and a chord a bar (a half bar in a one-bar
 * loop), the mode's triad whose root, third and fifth sound most there, the bass's note the root's hint, a seventh
 * where it sounds. That becomes the piece's harmony (no tone in it: GEN's own key and progression in the genre). Each
 * track a role by what it plays (gls_role). On EMPTY tracks only (no note), by GEN's knobs (BASS CHORDS ARP; AUTO: the
 * genre's): a bass where the loop has none, chords on the progression where it has none, an arp: a counter-melody made
 * of the loop's own melody (gls_counter) or, without one, an arp of the chords. The genre (GENRE; RANDOM and EXPLORE:
 * inferred, gls_genre) gives the sounds and the mix of those parts and the form: SONG's rows (gen_parts) from the
 * loop's material only: the intro and outro its drums thinned, the layers in by the genre's priorities, the build its
 * drums repeated with a roll on its own snare (clap, rim), the high-pass and the chords' filter opening, the break
 * without drums and bass, the fills ARRANGE's FILL of the kit's own sounds, every drop row the loop whole. A seed a
 * song; OCT+ again a new seed, the loop kept (the parts this one added made again). UNDO (GLO + G4) the loop as it
 * was, in LOOP mode. gls_hear, gls_prep, gls_counter live in the Bluetooth segment (BTAPP_FN: GEN runs only once
 * bt_hw_seg() said it is this build's, gen_go; else its message, BLUETOOTH MISSING) */

static uint32_t gls_sig __attribute__((section(".bss.gls")));   /* the music the last FROM LOOP left (its parts: gls.add) */
static uint32_t gls_hash(void)
{
    uint32_t k, h = 2166136261u;
    for (k = 0; k < NTRK; k++) h = (h ^ steps_sig(&trk[k])) * 16777619u;
    return h;
}
/* a track's role by its steps (RL_N: no note) */
static uint32_t gls_role(const track_t *t)
{
    uint32_t i, n = 0, ch = 0, sum = 0, len = dlen(t);
    for (i = 0; i < len; i++) {
        const step_t *s = &t->step[i];
        if (s->time != ST_NOTE || !(s->n || s->hit)) continue;
        n++; ch += s->n > 1u; sum += s->note[0];
    }
    return !n ? RL_N : drum_track(t) ? RL_DRUM : sum < 48u * n ? RL_BASS : ch * 3u > n ? RL_PAD : n * 2u > len ? RL_ARP : RL_LEAD;
}
/* the loop's key and chords (0: nothing tonal); seg: a chord's steps. The modes it tells apart: MAJ MIN DOR MIX (a
 * constant, not a table: the sound's merged constants stay put) */
#define GLS_MODE(i) ((SC_MAJ | SC_MIN << 8 | SC_DOR << 16 | SC_MIX << 24) >> (8u * (i)) & 255u)
BTAPP_FN static uint32_t gls_hear(uint32_t span)   /* (in the Bluetooth segment: GEN runs
                                                         * only once bt_hw_seg() said it is this build's, gen_go) */
{
    uint16_t w[4][12], b[4][12], tot[12];
    uint8_t pcs[14];
    uint32_t k, i, j, l, seg = span <= 16u || span % 16u ? 8u : 16u, ns = span / seg, all = 0, best = 0, bk = 0;
    memset(w, 0, sizeof w); memset(b, 0, sizeof b); memset(tot, 0, sizeof tot);
    if (!ns) ns = 1;
    if (ns > 4u) ns = 4;
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        uint32_t len = dlen(t), kit = drum_track(t), bass = gs.role[k] == RL_BASS;
        if (gs.role[k] >= RL_N || (gls.add >> k) & 1u) continue;
        for (i = 0; i < ns * seg; i++) {
            const step_t *s = &t->step[i % len];
            uint32_t d = 1, v, si = i / seg;
            if (s->time != ST_NOTE) continue;
            while (d < len && t->step[(i + d) % len].time == ST_TIE) d++;
            v = (d + (i % 4u == 0u)) * (s->vel / 32u + 1u);
            for (j = 0; j <= (kit ? NLANE : s->n - 1u); j++) {
                int32_t nt = s->note[j & 3u];
                if (kit) {                              /* (DRUM-X: a lane's hit, or the note of its chromatic run, on a
                                                         * lane of little noise, not METAL; a hit only where it rings:
                                                         * LONG or a long decay, the 808's, a tuned tom's) */
                    uint32_t c = j == NLANE, ln = c ? drum_note_lane(t, (uint32_t)s->note[0]) & 7u : j;
                    const dx_lane_t *x = &dx_kit[ln];
                    nt = s->note[0];
                    if ((c ? !s->n || !drum_ext(t) || nt >= 27 :
                             !((s->hit >> j) & 1u) || (((x->mode >> 5) & 3u) != DXM_LONG && x->a[DXP_DECAY] < 80u)) ||
                        x->a[DXP_NOISE] > 40u || (x->mode & 3u) == DXW_METAL)
                        continue;
                    nt = (392 + x->a[DXP_PITCH] * 12) / 16 + (c ? nt - (drum_chrom(t) >= 0 ? 13 : nt < 13 ? 6 : 19) : 0);
                }
                l = (uint32_t)nt % 12u;
                w[si][l] += (uint16_t)v; tot[l] += (uint16_t)v;
                if (bass || nt < 52) b[si][l] += (uint16_t)v;   /* (the lowest: the root's hint) */
                all += v;
            }
        }
    }
    if (all < 12u) return 0;                            /* (a few hits: the genre's own harmony) */
    for (k = 0; k < 4u * 12u; k++) {                    /* the key: the mode's notes against the others, the tonic heard */
        uint32_t r = k % 12u, m = SCALE_MASK[GLS_MODE(k / 12u)] * 0x1001u >> (12u - r), sc = 1u << 30, lo = 0;   /* (the
                                                         * mode on r: m bit l; unsigned, from 2^30, never under 0) */
        for (l = 0; l < 12u; l++) sc += (m >> l & 1u) ? 2u * tot[l] : -3u * tot[l];
        for (i = 0; i < ns; i++) lo += b[i][r];
        sc += 2u * tot[r] + 3u * lo + 6u * b[0][r] + w[0][r] + tot[(r + 7u) % 12u];   /* (the first bar's bass: the
                                                         * tonic's likeliest) */
        if (sc > best) { best = sc; bk = k; }
    }
    gs.root = (int16_t)(bk % 12u); gs.scale = (int16_t)GLS_MODE(bk / 12u);
    for (j = 0, l = 0; l < 12u; l++) if ((SCALE_MASK[gs.scale] >> l) & 1u) gs.sc[j++] = (uint8_t)l;
    for (j = 0; j < 14u; j++) pcs[j] = (uint8_t)((gs.root + gs.sc[j % 7u]) % 12u);
    gs.pal = 0x7Fu;
    gs.hx = (uint8_t)(HXF(gs.hx) == HF_JAZZ ? HF_JAZZY : HXF(gs.hx));   /* (no colour of GEN's over the loop's chords) */
    for (i = 0; i < ns; i++) {                          /* a chord a segment: the mode's triad heard most */
        uint32_t bd = i ? gs.pdeg[i - 1u] : 0u, bs = 0, d;
        const uint16_t *x = w[i];
        for (d = 0; d < 7u; d++) {
            gch_t c = {0, 0, (uint8_t)d, 0, 0};
            uint32_t sc = 4u * x[pcs[d]] + 3u * (x[pcs[d + 2u]] + x[pcs[d + 4u]]) + x[pcs[d + 6u]] + 5u * b[i][pcs[d]];
            if (sc > bs && gtriad_ok(&c)) { bs = sc; bd = d; }
        }
        gs.pdeg[i] = (uint8_t)bd;
        gs.pq[i] = (uint8_t)(2u * x[pcs[bd + 6u]] > x[pcs[bd]] ? Q_7 : Q_TRI);
    }
    gs.pn = (uint8_t)ns; gs.cst = seg;
    return 1;
}
/* (FROM LOOP) the counter-melody: the loop's melody src again, displaced an eighth or a beat, its first half
 * answered, turned upside down round its first note or kept to the beats, an octave away, in the key */
BTAPP_FN static void gls_counter(track_t *t, const track_t *src, uint32_t len)
{
    uint32_t i, sl = dlen(src), v = gr(), sh = v & 1u ? 2u : 4u, half = sl / 2u ? sl / 2u : 1u;
    int32_t piv = -1, oct = (v & 2u) ? 12 : -12;
    for (i = 0; i < sl; i++) if (src->step[i].time == ST_NOTE && src->step[i].n) { piv = src->step[i].note[0]; break; }
    if (piv < 0) return;
    if (piv > 72) oct = -12;
    else if (piv < 55) oct = 12;
    for (i = 0; i < len; i++) {
        uint32_t at = (v & 4u ? i % half : i + sl - sh) % sl;
        const step_t *s = &src->step[at];
        step_t *d = &t->step[i];
        int32_t n;
        if (s->time == ST_TIE && i && t->step[i - 1u].time != ST_REST) { dstep_tie(d); continue; }
        if (s->time != ST_NOTE || !s->n || ((v & 24u) == 8u && i % 4u)) continue;
        n = s->note[0];
        if ((v & 24u) == 16u) n = 2 * piv - n;          /* (inverted) */
        n = clamp(n + oct, 36, 96);
        while (!gin_mode(n)) n--;
        gnote(d, &n, 1, (int32_t)s->vel * 3 / 4 + 10, s->flags & SF_SLIDE);
    }
}
/* the build's roll (FROM LOOP): on the lane the loop's backbeat plays (its snare, clap or rim; else its busiest) */
static uint32_t gls_snare(const track_t *t)
{
    uint32_t i, l, n, best = 1, bn = 0;
    for (l = 1; l < NLANE; l++) {
        for (i = n = 0; i < 16u; i++) n += (t->step[i].hit >> l) & 1u;
        n *= (0x46u >> l) & 1u ? 4u : 1u;
        if (n > bn) { bn = n; best = l; }
    }
    return best;
}
/* the genre (RANDOM, EXPLORE: by the loop's tempo and drums: four kicks a bar or more from 100 BPM HOUSE, or HARD
 * TECHNO past 138; else an arc: KRAUT from 110 BPM, DREAM POP under) */
static uint32_t gls_genre(void)
{
    uint32_t bpm = (uint32_t)song.g[G_BPM], i, n = 0, dr = gs.trk_of[RL_DRUM] % NTRK;
    if (gp.genre && gp.genre <= NGENRE) return gp.genre - 1u;
    for (i = 0; i < 64u; i++) n += gkick(i);
    return bpm >= 100u && n * 4u >= dlen(&trk[dr]) ? (bpm > 138u ? 1u : 3u) : bpm >= 110u ? 16u : 22u;
}
static uint32_t gls_any(void)                           /* the music has a note */
{
    uint32_t k, n = 0;
    for (k = 0; k < NTRK; k++) n |= gls_role(&trk[k]) < RL_N;
    return n;
}
/* gen_go's first step: the UNDO point (FROM LOOP again on its own song: still the project before the first); 1: an
 * empty loop, nothing to make */
static __attribute__((noinline)) uint32_t gls_begin(void)
{
    if (gp.form == 3u && !gls_any()) { ui_message("THE LOOP IS EMPTY"); return 1; }
    if (gp.form != 3u || !gls.u || gls_sig != gls_hash()) undo_mark();
    return 0;
}
/* the song from the loop (gen_make, FORM FROM LOOP: its sections and rows then gen_parts'); gls.room 0: a part could
 * not be added for want of an empty track */
static __attribute__((noinline)) void gls_prep(void)
{
    static const uint8_t ROLE[3] = {RL_BASS, RL_PAD, RL_ARP}, AMT[3] = {GP_BASS, GP_CHORDS, GP_ARP};
    uint32_t k, r, j, span = 1, room = 1, ld = NTRK, ml = NTRK, fm6 = 0;
    if (gls_sig != gls_hash()) gls.add = 0;             /* (OCT+ again: the parts it added made again) */
    for (k = 0; k < NTRK; k++)
        if ((gls.add >> k) & 1u) { load_begin(&trk[k], LOAD_PAT); gclear_all(&trk[k]); motion_clear(&trk[k]); load_end(&trk[k]); }
    memset(&gs, 0, sizeof gs);
    gx.on = 0; gx.odd = 0; gx.eng = -1;
    gls.on = 1; gls.mute = 0;
    for (k = 0; k < RL_N; k++) gs.trk_of[k] = NTRK;
    for (k = 0; k < NTRK; k++) {                        /* the roles the loop's tracks play */
        track_t *t = &trk[k];
        r = gls_role(t);
        gs.role[k] = (uint8_t)(r < RL_N ? r : RL_TEX);
        if (r >= RL_N) continue;
        gs.act[r] = 1;
        if (gs.trk_of[r] == NTRK) gs.trk_of[r] = (uint8_t)k;
        if ((r == RL_LEAD || r == RL_ARP) && ld == NTRK) ld = k;
        if (r != RL_DRUM && ml == NTRK) ml = k;
        gls.mute |= (uint8_t)((t->p[P_MUTE] != 0) << k);
        fm6 += t->eng_req == ENGI_FM6;
        if (dlen(t) > span) span = dlen(t);
    }
    if (gs.act[RL_DRUM]) gkick_of(&trk[gs.trk_of[RL_DRUM]], dlen(&trk[gs.trk_of[RL_DRUM]]));
    gs.gi = gls_genre();
    gmut = GENRE[gs.gi]; gs.g = &gmut;
    gseed(GS_PICK);
    gen_pick();                                         /* (the genre's choices: amounts, voicing, bass, its harmony) */
    gs.len = span;
    if ((gls.heard = (uint8_t)gls_hear(span)) != 0) {
        gs.hx &= 7u;
        if (gs.vm == VM_PLANE) gs.vm = VM_PLAIN;        /* (a shape moved would leave the key the loop's chords keep) */
    }
    gplan(0);
    gs.fm6 = fm6;
    gseed(GS_SOUND);
    for (j = 0; j < 3u; j++) {                          /* the parts the loop lacks, on its empty tracks */
        track_t *t;
        r = ROLE[j];
        if (gs.act[r] || !gs.amt[AMT[j]]) continue;
        for (k = 0; k < trk_live && k < NTRK && (gs.role[k] != RL_TEX || drum_track(&trk[k])); k++) {}
        if (k >= trk_live || k >= NTRK) { room = 0; break; }
        t = &trk[k];
        gs.role[k] = (uint8_t)r; gs.act[r] = 1; gs.trk_of[r] = (uint8_t)k;
        gls.add |= (uint8_t)(1u << k);
        gen_sound(t, r, gmut.snd[AMT[j]]);
        gen_mix(t, r);
        load_begin(t, LOAD_PAT);
        gclear_all(t);
        motion_clear(t);
        if (r == RL_ARP && ld < NTRK) gls_counter(t, &trk[ld], span);
        else gen_role(t, k, span);
        t->p[P_SLEN] = (int16_t)span;
        t->p[P_SGATE] = (int16_t)(r == RL_BASS ? clamp(gmut.bass_gate, 1, 127) : r == RL_PAD ? 90 : 60);
        t->p[P_ROOT] = gs.root; t->p[P_SCALE] = gs.scale; t->p[P_QUANT] = QN_SNAP;
        if (ml < NTRK)   /* (the loop's SCALE page kept: every melodic track's,
                                                         * the keys' link, ui_input.c scale_link) */
            memcpy(&t->p[P_ROOT], &trk[ml].p[P_ROOT], 3 * sizeof t->p[0]);
        dlevel(t, span);
        comp_migrate(t->step, span);
        load_end(t);
        pat_sig[k] = steps_sig(t);
    }
    gs.trk_of[RL_TEX] = NTRK;                           /* (no riser over a track of the player's) */
    gs.song = 2;
    gls.room = (uint8_t)room;
}
