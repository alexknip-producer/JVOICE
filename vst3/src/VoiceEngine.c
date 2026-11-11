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
    e->controls[9]=1; /* eight poly voices until plugin parameters are applied */
    const preset_t *p = &FORMANT_PRESETS[9]; /* SINGER EN */
    for (int k = 0; k < NTRK; ++k) {
        for (int j = 0; j < 8; ++j) trk[k].p[P_E0+j] = p->e[j];
        trk[k].p[P_VLANG] = LG_EN;
        trk[k].p[P_VOICE] = V_POLY;
        trk[k].p[P_VCONS] = 127; /* upstream's 100% consonant duration */
    }
}
jv_engine *jv_create(void)
{
    jv_engine *e = calloc(1, sizeof *e);
    if (e) jv_reset(e);
    return e;
}
void jv_destroy(jv_engine *e) { free(e); }
void jv_control(jv_engine *e, int index, double value)
{
    if(index<0 || index>=10 || !isfinite(value))return;
    jv_bind(e);
    value=value<0 ? 0 : value>1 ? 1 : value;
    e->controls[index]=value;
    if(index==9){int cap=1+(int)(value*7+.5);for(int k=cap+1;k<NTRK;++k)trk[k].v[0].active=trk[k].v[0].gate=0;}
    if(index>=6)return;
    int param, raw=(int)(value*127+.5);
    switch(index) {
    case 0: param=P_VCONS;raw=(raw+95)&127;break;
    case 1: param=P_E3;raw=(int)floor(value*24-12+.5);break;
    case 2: param=P_E6;break;
    case 3: param=P_E5;break;
    case 4: param=P_E4;break;
    default:param=P_E2;raw=127-raw;break;
    }
    for(int k=0;k<NTRK;++k)trk[k].p[param]=(int16_t)raw;
}
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

    if (!chord || !sing[0].live) {
        trk[0].v[0].active = trk[0].v[0].gate = 1;
        trk[0].v[0].env_out = 32700;
        sing_note(&trk[0]);
        sing_block(&trk[0]);
    }
    const int unison=e->controls[8]>=.5;
    const int cap=1+(int)(e->controls[9]*7+.5), nv=unison ? cap : 1;
    const uint32_t age=++e->age;
    static const float VP[8]={-1,1,-.5f,.5f,-.75f,.75f,-.25f,.25f};
    for(int sub=0;sub<nv;++sub) {
        int slot=1;
        for(int k=1;k<=cap;++k) {
            if(!trk[k].v[0].active){slot=k;break;}
            if(trk[k].v[0].age<trk[slot].v[0].age)slot=k;
        }
        track_t *t = &trk[slot];
        memcpy(t->p, trk[0].p, sizeof t->p);
        memcpy(lyr_txt[slot], lyr_txt[0], LYR_LEN);
        lyr_gen[slot] = lyr_gen[0];
        sing[slot] = sing[0];sing_x[slot] = sing_x[0];sing_io[slot] = sing_io[0];
        memset(fmt_f5[slot], 0, sizeof fmt_f5[slot]);sing_w[slot] = sing_w[0];
        voice_t *v = &t->v[0];memset(v, 0, sizeof *v);
        v->note = (uint8_t)note;v->vel = (uint8_t)vel;v->active = v->gate = 1;
        v->pitch16 = v->pitch_cur = v->gfrom = note * 16;v->age = age;
        if(unison && sub)v->ph[0]=noise32(&formant_nz); /* decorrelate layered oscillators */
        e->ids[slot] = note_id;t->p[P_M1AMT] = (int16_t)channel;
        e->detune_position[slot]=unison ? (nv==1 ? 0 : 2.0f*sub/(nv-1)-1) : VP[slot-1];
        e->pan_position[slot]=unison ? (nv==1 ? 0 : 2.0f*sub/(nv-1)-1) : VP[slot-1];
        e->pan_current[slot]=e->pan_position[slot]*(float)e->controls[6];
        e->gain[slot]=unison ? 1.0f/sqrtf((float)nv) : 1;
    }
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
    if(slot) {
        uint32_t age=trk[slot].v[0].age;
        for(int k=1;k<NTRK;++k)if(trk[k].v[0].age==age)trk[k].v[0].gate=0;
    }
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
unsigned jv_position(jv_engine *e){jv_bind(e);return sing[0].nsy;}
void jv_render_stereo(jv_engine *e, float *left, float *right)
{
    jv_bind(e);
    float mix_l[CTL]={0},mix_r[CTL]={0};
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
        double ratio=pow(2.0,e->detune_position[k]*e->controls[7]*40/1200);
        m.fine=(int32_t)((ratio-1)*4096);
        m.inc=(uint32_t)(m.inc*ratio);
        m.pitch16 = v->pitch16;
        m.amp0 = amp * v->vel / 127;
        m.amp1 = next * v->vel / 127;
        m.envq15 = next;
        m.shape = 64 << 8;
        int32_t mono[CTL]={0};
        formant_render(t, v, mono, CTL, &m);
        float pan0=e->pan_current[k],pan1=e->pan_position[k]*(float)e->controls[6];
        for(int i=0;i<CTL;++i) {
            float pan=pan0+(pan1-pan0)*(i+1)/CTL, x=mono[i]*e->gain[k]/65536.0f;
            mix_l[i]+=x*(pan>0 ? 1-pan : 1);mix_r[i]+=x*(pan<0 ? 1+pan : 1);
        }
        e->pan_current[k]=pan1;
        v->env_out = next;
    }
    for (int i = 0; i < CTL; ++i) {
        left[i]=mix_l[i]/(1+fabsf(mix_l[i]));
        right[i]=mix_r[i]/(1+fabsf(mix_r[i]));
    }
}

void jv_render(jv_engine *e, float *out) {
    float left[CTL],right[CTL];jv_render_stereo(e,left,right);
    for(int i=0;i<CTL;++i)out[i]=(left[i]+right[i])*.5f;
}
