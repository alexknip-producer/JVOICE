/* SPDX-License-Identifier: GPL-3.0-only */
#include "VoiceEngine.h"
#include "engine_generated.inc"
_Static_assert(CTL == JV_BLOCK, "Desktop block size must match pinned preview");

/* Eight independent articulation tracks. Track zero is only the lyric cursor.
 * No shared mutable DSP state survives jv_bind: different plugin instances may
 * render concurrently on different host threads. Rendering uses 44.1 kHz / 64 frames. */
/* Track's otherwise unused MIDI CC slot holds the channel for note pairing. */
void jv_reset(jv_engine *e)
{
    memset(e, 0, sizeof *e);
    jv_bind(e);
    formant_nz = 0x2545F491;
    sing_init();
    const preset_t *p = &FORMANT_PRESETS[9]; /* SINGER EN */
    for (int k = 0; k < NTRK; ++k) {
        for (int j = 0; j < 8; ++j) trk[k].p[P_E0+j] = p->e[j];
        trk[k].p[P_VLANG] = LG_EN;
        trk[k].p[P_VOICE] = V_POLY;
    }
}
jv_engine *jv_create(void)
{
    jv_engine *e = calloc(1, sizeof *e);
    if (e) jv_reset(e);
    return e;
}
void jv_destroy(jv_engine *e) { free(e); }
void jv_text(jv_engine *e, const char *s, int lang)
{
    jv_bind(e);
    trk[0].p[P_VLANG] = clamp(lang, LG_ES, LG_N - 1);
    lyr_set(0, s);
    memset(&sing[0], 0, sizeof sing[0]);
    memset(&sing_x[0], 0, sizeof sing_x[0]);
    memset(&sing_w[0], 0, sizeof sing_w[0]);
    memset(&sing_io[0], 0, sizeof sing_io[0]);
}
void jv_note_on(jv_engine *e, int channel, int note, int vel, int chord, int note_id)
{
    jv_bind(e);
    int slot = 1;
    for (int k = 1; k < NTRK; ++k) {
        if (!trk[k].v[0].active) { slot = k; break; }
        if (trk[k].v[0].age < trk[slot].v[0].age) slot = k;
    }
    if (!chord || !sing[0].live) {
        trk[0].v[0].active = trk[0].v[0].gate = 1;
        trk[0].v[0].env_out = 32700;
        sing_note(&trk[0]);
        sing_block(&trk[0]);
    }
    track_t *t = &trk[slot];
    memcpy(t->p, trk[0].p, sizeof t->p);
    memcpy(lyr_txt[slot], lyr_txt[0], LYR_LEN);
    lyr_gen[slot] = lyr_gen[0];
    sing[slot] = sing[0];
    sing_x[slot] = sing_x[0];
    sing_io[slot] = sing_io[0];
    memset(fmt_f5[slot], 0, sizeof fmt_f5[slot]);
    sing_w[slot] = sing_w[0];
    voice_t *v = &t->v[0];
    memset(v, 0, sizeof *v);
    v->note = (uint8_t)note;
    v->vel = (uint8_t)vel;
    v->active = v->gate = 1;
    v->pitch16 = v->pitch_cur = v->gfrom = note * 16;
    v->age = ++e->age;
    e->ids[slot] = note_id;
    t->p[P_M1AMT] = (int16_t)channel;
}
void jv_note_off(jv_engine *e, int channel, int note, int note_id)
{
    jv_bind(e);
    int slot = 0;
    for (int k = 1; k < NTRK; ++k) {
        voice_t *v = &trk[k].v[0];
        if (v->active && v->gate && v->note == note && trk[k].p[P_M1AMT] == channel &&
            (note_id < 0 || e->ids[k] == note_id) &&
            (!slot || v->age < trk[slot].v[0].age)) slot = k;
    }
    if (slot) trk[slot].v[0].gate = 0;
}
void jv_panic(jv_engine *e)
{
    jv_bind(e);
    for (int k = 1; k < NTRK; ++k) memset(trk[k].v, 0, sizeof trk[k].v);
}
int jv_active(jv_engine *e)
{
    jv_bind(e);
    int n = 0;
    for (int k = 1; k < NTRK; ++k) n += !!trk[k].v[0].active;
    return n;
}
void jv_render(jv_engine *e, float *out)
{
    jv_bind(e);
    int32_t mix[CTL] = {0};
    sing_block(&trk[0]); /* silent cursor follows articulation timing between notes */
    for (int k = 1; k < NTRK; ++k) {
        track_t *t = &trk[k];
        voice_t *v = &t->v[0];
        if (!v->active) continue;
        sing_block(t);
        int32_t target = v->gate ? 32700 : 0;
        int32_t amp = v->env_out;
        /* 5 ms onset; ~200 ms release. Articulation remains upstream's. */
        int32_t next = v->gate ? amp + (target - amp + 3) / 4 : amp * 956 / 1000;
        if (!v->gate && next < 8) { v->active = 0; continue; }
        vmod_t m = {0};
        m.inc = pitch_inc(v->pitch16);
        m.pitch16 = v->pitch16;
        m.amp0 = amp * v->vel / 127;
        m.amp1 = next * v->vel / 127;
        m.envq15 = next;
        m.shape = 64 << 8;
        formant_render(t, v, mix, CTL, &m);
        v->env_out = next;
    }
    for (int i = 0; i < CTL; ++i) {
        float x = mix[i] / 65536.0f;
        out[i] = x / (1.0f + fabsf(x));
    }
}
