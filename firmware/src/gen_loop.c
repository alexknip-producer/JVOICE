/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT: GEN's FORM MY LOOP ("FROM LOOP" in the code): the loop that plays made a whole song by GEN's rules. Included
 * by gen.c before gen_make (whose gen_parts makes its sections and rows).
 *
 * The music as it is is the loop: its tracks, their steps and sounds stay as they are and are section A, the song's
 * peak (its drop). Read from it (gls_hear): the key (each pitch class weighed by its length, its velocity and the beat
 * it falls on; the bass's, the lowest notes', count again for the tonic; DRUM-X's tonal lanes, little noise, not METAL,
 * a LONG or long ring, and an EXTENDED kit's chromatic run, by their pitch), one of six modes (PHRY MIN HARM DOR MIX
 * MAJ; where the loop leaves two alike, the genre's, the minor, the darker where the loop is distorted or the genre
 * heavy). (MY LOOP 2, the owner: "every seed the same major chords, the aggression gone, the genre does nothing") Its
 * chords made again each seed (gls_reharm): a chord a segment (a bar; a half bar in a one-bar loop) holding the loop's
 * notes there, the heard progression a quarter of the seeds, else drawn by fit with the genre's own chords and a dark
 * loop's minor ones likelier (EXPLORE: wider). No tone in it: GEN's own key and progression in the genre. Each track a
 * role by what it plays (gls_role). On EMPTY tracks only (no note), by GEN's knobs (BASS CHORDS ARP; AUTO: the
 * genre's): a bass where the loop has none, chords on the progression where it has none, an arp: a counter-melody made
 * of the loop's own melody (gls_counter) or, without one, an arp of the chords. The genre (GENRE; RANDOM: inferred,
 * gls_genre; EXPLORE: a seed's, near the loop's tempo) gives the sounds and the mix of those parts, and its DNA to every
 * section but the drop (gen_parts): the verse (B) its drums (its map, on the loop's kit), its bass, chords, arp and
 * lead on the loop's sounds over its own progression (gls.v); the build (C) the genre's parts on the drop's chords, the
 * drums the verse's or the loop's (half the seeds) repeated with a roll on its snare (clap, rim), the high-pass and the
 * chords' filter opening; the break (D) without drums and bass, on the genre's or the drop's chords from the
 * predominant. The rows SONG's (the club's or the arc's, a seed's), the verse in the groove and in a seed's intros and
 * outros, a seed's lengths and energies; the intro and outro drums thinned, the layers in by the genre's priorities,
 * the fills ARRANGE's FILL, every drop row the loop whole. A seed a song; OCT+ again a new seed, the loop kept (the
 * parts this one added made again). UNDO (GLO + G4) the loop as it was, in LOOP mode. gls_hear, gls_role, gls_any,
 * gls_begin, gls_counter live in the Bluetooth segment (BTAPP_FN: GEN runs only once bt_hw_seg() said it is this
 * build's, gen_go; else its message, BLUETOOTH MISSING) */

static uint32_t gls_sig __attribute__((section(".bss.gls")));   /* the music the last FROM LOOP left (its parts: gls.add) */
static uint32_t gls_hash(void)
{
    uint32_t k, h = 2166136261u ^ trk_live;
    for (k = 0; k < trk_live && k < NTRK; k++) h = (h ^ steps_sig(&trk[k])) * 16777619u;
    return h;
}
/* a track's role by its steps (RL_N: no note) */
BTAPP_FN static uint32_t gls_role(const track_t *t)   /* (the segment's: gls_begin, gls_prep) */
{
    uint32_t i, n = 0, ch = 0, sum = 0, len = dlen(t);
    for (i = 0; i < len; i++) {
        const step_t *s = &t->step[i];
        if (s->time != ST_NOTE || !(s->n || s->hit)) continue;
        n++; ch += s->n > 1u; sum += s->note[0];
    }
    return !n ? RL_N : drum_track(t) ? RL_DRUM : sum < 48u * n ? RL_BASS : ch * 3u > n ? RL_PAD : n * 2u > len ? RL_ARP : RL_LEAD;
}
/* the loop's key (0: nothing tonal; else its segments, a chord's steps each: gs.cst): w, b each segment's pitch classes
 * heard and its lowest's (gls_reharm's). The modes it tells apart: PHRY MIN HARM DOR MIX MAJ (a constant, not a table:
 * the sound's merged constants stay put), the darkest first; where the loop leaves two of them alike (no third, no
 * sixth, no second: a power riff, an 808 on its root), the genre's own modes, then the minor one, the darker ones
 * first where the loop is distorted or its genre heavy (gls.dark) */
#define GLS_MODE(i) ((SC_PHRY | SC_MIN << 4 | SC_HARM << 8 | SC_DOR << 12 | SC_MIX << 16 | SC_MAJ << 20) >> (4u * (i)) & 15u)
BTAPP_FN static uint32_t gls_hear(uint32_t span, uint16_t (*w)[12], uint16_t (*b)[12])   /* (in the Bluetooth segment:
                                                         * GEN runs only once bt_hw_seg() said it is this build's, gen_go) */
{
    uint16_t tot[12];
    uint32_t k, i, j, l, seg = span <= 16u || span % 16u ? 8u : 16u, ns = span / seg, all = 0, best = 0, bk = 0;
    memset(w, 0, 4u * sizeof *w); memset(b, 0, 4u * sizeof *b); memset(tot, 0, sizeof tot);
    if (!ns) ns = 1;
    if (ns > 4u) ns = 4;
    for (k = 0; k < trk_live && k < NTRK; k++) {
        const track_t *t = &trk[k];
        uint32_t len = dlen(t), kit = drum_track(t), bass = gs.role[k] == RL_BASS;
        if (gs.role[k] >= RL_N || (gls.add >> k) & 1u) continue;
        for (i = 0; i < ns * seg; i++) {
            const step_t *s = &t->step[i % len];
            uint32_t d = 1, v, si = i / seg;
            if (s->time != ST_NOTE) continue;
            while (d < len && t->step[(i + d) % len].time == ST_TIE) d++;
            v = (d + (i % 4u == 0u)) * (s->vel / 32u + 1u);
            for (j = 0; j < (kit ? NLANE + 1u : s->n); j++) {
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
    for (k = 0; k < 6u * 12u; k++) {                    /* the key: the mode's notes against the others, the tonic heard */
        uint32_t r = k % 12u, mi = k / 12u, md = GLS_MODE(mi), m = SCALE_MASK[md] * 0x1001u >> (12u - r), sc = 1u << 24, lo = 0;   /* (the
                                                         * mode on r: m bit l; unsigned, from 2^24, never under 0) */
        for (l = 0; l < 12u; l++) sc += (m >> l & 1u) ? 2u * tot[l] : -3u * tot[l];
        for (i = 0; i < ns; i++) lo += b[i][r];
        sc += 2u * tot[r] + 3u * lo + 6u * b[0][r] + w[0][r] + tot[(r + 7u) % 12u];   /* (the first bar's bass: the
                                                         * tonic's likeliest) */
        for (j = 0, l = 0; j < 4u; j++) l |= gs.g->scale[j] == md;
        sc = sc * 8u + 2u * l + (mi == 1u) + (gls.dark && mi < 3u ? 4u : 0u);   /* (alike: the genre's, minor, dark) */
        if (sc > best) { best = sc; bk = k; }
    }
    gs.root = (int16_t)(bk % 12u); gs.scale = (int16_t)GLS_MODE(bk / 12u);
    for (j = 0, l = 0; l < 12u; l++) if ((SCALE_MASK[gs.scale] >> l) & 1u) gs.sc[j++] = (uint8_t)l;
    gs.pal = 0x7Fu;
    gs.hx = (uint8_t)(HXF(gs.hx) == HF_JAZZ ? HF_JAZZY : HXF(gs.hx));   /* (no colour of GEN's over the loop's chords) */
    gs.cst = seg;
    return ns;
}
/* (MY LOOP) the drop's chords, a seed's own: a chord a segment that holds the loop's notes there (its root, third and
 * fifth what sounds most, the bass's note one of them, no note of the loop a semitone off them louder than half its
 * tones but the tonic's own, the mode's b2 and b6; a seventh where it sounds; the genre's power chords where it plays
 * them): a quarter of the seeds the heard one (each segment's likeliest), else each segment's drawn by how well it fits
 * (EXPLORE: more often, and chords fitting less well), the genre's own chord there (gen_pick's progression) and a dark
 * loop's minor chords likelier. The verse's (gen_parts' B): the genre's own where the loop's mode takes it (gls.v) */
static __attribute__((noinline)) void gls_reharm(const uint16_t (*w)[12], const uint16_t (*b)[12], uint32_t ns)
{
    uint32_t i, d, l, x = gp.genre == GEN_EXPLORE, way = gr() % 100u, s7[7], best, bd, sum;
    uint8_t pc[14];
    for (d = 0; d < 14u; d++) pc[d] = (uint8_t)gpc((int32_t)d);
    for (i = 0; i < ns; i++) {
        for (d = best = bd = 0; d < 7u; d++) {
            gch_t c = {0, 0, (uint8_t)d, 0, 0};
            uint32_t r = pc[d], m = 1u << r | 1u << pc[d + 2u] | 1u << pc[d + 4u], sv = pc[d + 6u];
            uint32_t nb = (m * 2u | m / 2u | m << 11 | m >> 11) & ~(m | 1u << sv), neg = 0, pos = 0;
            int32_t v;
            for (l = 0; l < 12u; l++) {
                uint32_t y = w[i][l] + b[i][l];
                if (m >> l & 1u) pos += y;
                else if (nb >> l & 1u && d) neg += y;   /* (the tonic's: the mode's own tensions) */
            }
            v = (int32_t)(3u * pos + 2u * b[i][r] + w[i][sv]) - 3 * (int32_t)neg;   /* (the bass the root, the seventh
                                                         * where it sounds; a semitone off a chord tone against) */
            s7[d] = gtriad_ok(&c) && 2u * neg <= pos && v > 0 ?
                    (uint32_t)v << ((gls.dark && giv(&c, 2) == 3) + (gls.v.n && d == (uint32_t)gls.v.deg[i % gls.v.n])) : 0u;
            if (s7[d] > best) { best = s7[d]; bd = d; }
        }
        for (d = sum = 0; d < 7u; d++) sum += s7[d] = s7[d] * (x ? 4u : 3u) >= best ? s7[d] : 0u;
        if (way >= (x ? 10u : 25u) && sum)              /* (drawn by its fit) */
            for (l = gr() % sum, bd = 0; l >= s7[bd]; bd++) l -= s7[bd];
        gs.pdeg[i] = (uint8_t)bd;
        gs.pq[i] = (uint8_t)(2u * w[i][pc[bd + 6u]] > w[i][pc[bd]] ? Q_7 : gs.vm == VM_POWER ? Q_POWER : Q_TRI);
    }
    gs.pn = (uint8_t)ns;
    d = gs.hx; gs.hx = gls.hx;
    if (!gprog_ok(&gls.v)) gls.v.n = 0;                 /* (the verse's: with the genre's colours) */
    gs.hx = (uint8_t)d;
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
    if (gp.genre == GEN_EXPLORE)                        /* (EXPLORE: a seed's, any whose tempo is near the loop's) */
        for (i = 0; i < 32u; i++) { n = gr() % NGENRE; if (bpm + 24u >= GENRE[n].bpm_lo && bpm <= GENRE[n].bpm_hi + 24u) return n; }
    for (i = n = 0; i < 64u; i++) n += gkick(i);
    return bpm >= 100u && n * 4u >= dlen(&trk[dr]) ? (bpm > 138u ? 1u : 3u) : bpm >= 110u ? 16u : 22u;
}
BTAPP_FN static uint32_t gls_any(void)                  /* the music has a note */
{
    uint32_t k, n = 0;
    for (k = 0; k < trk_live && k < NTRK; k++) n |= gls_role(&trk[k]) < RL_N;
    return n;
}
/* gen_go's first step: the UNDO point (FROM LOOP again on its own song: still the project before the first); 1: an
 * empty loop, nothing to make */
BTAPP_FN static uint32_t gls_begin(void)   /* (the segment's: gen_go asked bt_hw_seg() first) */
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
    uint16_t w[4][12], b[4][12];
    uint32_t k, r, j, span = 1, room = 1, ld = NTRK, ml = NTRK, fm6 = 0, dk = 0;
    if (gls_sig != gls_hash()) gls.add = 0;             /* (OCT+ again: the parts it added made again) */
    for (k = 0; k < trk_live && k < NTRK; k++)
        if ((gls.add >> k) & 1u) { load_begin(&trk[k], LOAD_PAT); gclear_all(&trk[k]); motion_clear(&trk[k]); load_end(&trk[k]); }
    memset(&gs, 0, sizeof gs);
    memset(gs.role, RL_TEX, sizeof gs.role);   /* inactive tracks have no musical role */
    gx.on = 0; gx.odd = 0; gx.eng = -1;
    gls.on = 1; gls.mute = 0;
    for (k = 0; k < RL_N; k++) gs.trk_of[k] = NTRK;
    for (k = 0; k < trk_live && k < NTRK; k++) {                        /* the roles the loop's tracks play */
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
        dk |= t->p[P_DIST] >= 30;                       /* (a distorted part: the darker mode where two are alike) */
        if (dlen(t) > span) span = dlen(t);
    }
    if (gs.act[RL_DRUM]) gkick_of(&trk[gs.trk_of[RL_DRUM]], dlen(&trk[gs.trk_of[RL_DRUM]]));
    gseed(GS_PICK);
    gs.gi = gls_genre();
    gmut = GENRE[gs.gi]; gs.g = &gmut;
    gen_pick();                                         /* (the genre's choices: amounts, voicing, bass, its harmony) */
    gs.len = span;
    gls.hx = gs.hx; gls.v.n = gs.pn;                   /* (the genre's progression: the verse's) */
    memcpy(gls.v.deg, gs.pdeg, 8);
    gls.dark = (uint8_t)(dk | ((gmut.spec & GSP_HEAVY) != 0));
    if ((gls.heard = (uint8_t)(r = gls_hear(span, w, b))) != 0) {
        gs.hx &= 7u;
        if (gs.vm == VM_PLANE) gs.vm = VM_PLAIN;        /* (a shape moved would leave the key the loop's chords keep) */
        gls_reharm(w, b, r);
    } else gls.v.n = 0;
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
