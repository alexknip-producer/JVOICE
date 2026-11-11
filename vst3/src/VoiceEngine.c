/* SPDX-License-Identifier: GPL-3.0-only */
#include "VoiceEngine.h"
#include "engine_generated.inc"
_Static_assert(CTL == JV_BLOCK, "Desktop block size must match pinned preview");
_Static_assert(FS == JV_SAMPLE_RATE, "FM-1 DSP must remain at 44100 Hz");
enum { JV_DECLICK_FRAMES = 132 }; /* 3 ms at the native clock. */

/* Eight independent articulation tracks. Track zero is only the lyric cursor.
 * No shared mutable DSP state survives jv_bind: different plugin instances may
 * render concurrently on different host threads. Rendering uses 44.1 kHz / 64 frames. */
/* Track's otherwise unused MIDI CC slot holds the channel for note pairing. */
void jv_reset(jv_engine *e)
{
    uint32_t seed=e->noise_seed ? e->noise_seed : 0x2545F491;
    memset(e, 0, sizeof *e);e->noise_seed=seed;
    jv_bind(e);
    formant_nz = (int32_t)seed;
    sing_init();
    e->controls[9]=1; /* eight poly voices until plugin parameters are applied */
    e->controls[12]=72.0/127;
    e->controls[14]=60.0/72; /* Master Volume: 0 dB. */
    e->controls[15]=.5; /* External input gain: 0 dB above the +12 dB calibration. */
    e->controls[16]=.5; /* Wheel fully up: +/-25 cents by default, +/-50 maximum. */
    e->controls[17]=49.0/119; /* 5 Hz sine LFO. */
    e->controls[21]=.5; /* Phoneme Glide: native transition times at centre. */
    e->controls[19]=.5; /* Smear centre leaves each vowel unchanged. */
    e->mix_gain=.7f;
    e->external_gain=1;
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
void jv_seed(jv_engine *e,unsigned seed){e->noise_seed=seed ? seed:1;jv_bind(e);formant_nz=(int32_t)e->noise_seed;}
void jv_bend(jv_engine *e,int ch,double v){if(ch>=0 && ch<16 && isfinite(v))e->bend_target[ch]=v<-1 ? -1:v>1 ? 1:v;}
void jv_mod_wheel(jv_engine *e,int ch,double v){if(ch>=0 && ch<16 && isfinite(v))e->mod_target[ch]=v<0 ? 0:v>1 ? 1:v;}
void jv_control(jv_engine *e, int index, double value)
{
    if(index<0 || index>=JV_CONTROL_COUNT || (index==11 || index==22) || !isfinite(value))return;
    jv_bind(e);
    value=value<0 ? 0 : value>1 ? 1 : value;
    e->controls[index]=value;
    if(index==9){int cap=1+(int)(value*7+.5);for(int k=cap+1;k<NTRK;++k){if(trk[k].v[0].active)e->declick_pending=1;trk[k].v[0].active=trk[k].v[0].gate=0;}}
    if(index==18 && value>=.5){
        uint32_t newest=0;
        for(int k=1;k<NTRK;++k)if(trk[k].v[0].active && trk[k].v[0].age>newest)newest=trk[k].v[0].age;
        for(int k=1;k<NTRK;++k)if(trk[k].v[0].active && trk[k].v[0].age!=newest){trk[k].v[0].active=trk[k].v[0].gate=0;e->declick_pending=1;}
    }
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
    if(e->controls[18]>=.5){
        /* Replace the old stack, then remove the output discontinuity over 3 ms. */
        if(jv_active(e) || e->declick_remaining)e->declick_pending=1;
        for(int k=1;k<NTRK;++k)memset(trk[k].v,0,sizeof trk[k].v);
    }

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
        /* Replacing a sounding unison layer (or a stolen poly voice) removes
         * its current waveform. Apply the same short stereo correction as Mono. */
        if(t->v[0].active)e->declick_pending=1;
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
        e->right_voice[slot]=*v;memset(e->right_f5[slot],0,sizeof e->right_f5[slot]);
        e->lateral_hold[slot]=0;
        e->envelope[slot]=0;
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
        for(int k=1;k<NTRK;++k)if(trk[k].v[0].age==age){
            trk[k].v[0].gate=0;
            const sing_t *s=sing_view(&trk[k]);
            if(trk[k].p[P_VLANG]==LG_RU && s->cd<s->np && (s->ph[s->np-1]==PH_L || s->ph[s->np-1]==PH_RU_L))
                e->lateral_hold[k]=(uint16_t)sy_ms(80);
        }
    }
}
void jv_panic(jv_engine *e)
{
    jv_bind(e);
    for (int k = 1; k < NTRK; ++k) memset(trk[k].v, 0, sizeof trk[k].v);
    e->declick_pending=e->declick_remaining=0;
    e->last_output[0]=e->last_output[1]=0;
    memset(e->lateral_hold,0,sizeof e->lateral_hold);
}
int jv_active(jv_engine *e)
{
    jv_bind(e);
    int n = 0;
    for (int k = 1; k < NTRK; ++k) n += !!trk[k].v[0].active;
    return n;
}
unsigned jv_position(jv_engine *e){jv_bind(e);return sing[0].nsy;}
void jv_render_input(jv_engine *e,const float *input_l,const float *input_r,float *left,float *right)
{
    jv_bind(e);
    float mix_l[CTL]={0},mix_r[CTL]={0};
    const float zero[CTL]={0};int external=e->controls[10]>=.5;
    /* Manual master only. Notes, release tails and unison never change gain. */
    float target_gain=.7f*(float)pow(10.0,(e->controls[14]*72-60)/20),previous_gain=e->mix_gain;
    float external_target=(float)pow(10.0,(e->controls[15]*48-24)/20),carrier_gain[CTL];
    for(int i=0;i<CTL;++i)carrier_gain[i]=e->external_gain+(external_target-e->external_gain)*(i+1)/CTL;
    jv_carrier_gain=carrier_gain;
    e->external_gain=external_target;
    for(int ch=0;ch<16;++ch){
        e->bend[ch]+=(e->bend_target[ch]-e->bend[ch])*.2;
        e->mod_wheel[ch]+=(e->mod_target[ch]-e->mod_wheel[ch])*.2;
        if(fabs(e->mod_wheel[ch]-e->mod_target[ch])<1e-9)e->mod_wheel[ch]=e->mod_target[ch];
    }
    /* One continuous sine per instance: the wheel scales its depth, never a knob.
     * Native control clock keeps the speed identical at all host sample rates. */
    double vibrato=sin(e->mod_phase*6.2831853071795864769)*e->controls[16]*.5;
    e->mod_phase+=(.1+11.9*e->controls[17])*CTL/FS;
    e->mod_phase-=floor(e->mod_phase);
    sing_block(&trk[0]); /* silent cursor follows articulation timing between notes */
    for (int k = 1; k < NTRK; ++k) {
        track_t *t = &trk[k];
        voice_t *v = &t->v[0];
        if (!v->active) continue;
        sing_block(t);
        int32_t target = v->gate ? 32700 : 0;
        int32_t amp = e->envelope[k], old_output=v->env_out;
        /* 5 ms onset; adjustable upstream release curve. */
        uint32_t rel=ENV_EXP[(uint32_t)(e->controls[12]*127+.5)];
        int32_t next = v->gate ? amp + (target - amp + 3) / 4 : amp-(int32_t)(((uint32_t)amp*rel+65535u)>>16);
        if(!v->gate && e->lateral_hold[k]){next=amp;--e->lateral_hold[k];}
        if(!v->gate && next<8)next=0;
        e->envelope[k]=next;
        /* Firmware's amp hook keeps timed coda consonants connected to the
         * held vowel while the underlying release envelope continues. */
        int32_t output_amp=formant_amp(t,v,next);
        const sing_t *articulation=sing_view(t);
        if(!v->gate && !articulation->jv_consonant_unit && articulation->cdon &&
           articulation->cd<articulation->np && articulation->i<articulation->np &&
           (articulation->i<=articulation->nu || articulation->tb) && v->s[7]>output_amp)
            output_amp=v->s[7];
        int finished=!v->gate && !next && output_amp<8;
        if(finished)output_amp=0;
        vmod_t m = {0};
        m.inc = pitch_inc(v->pitch16);
        int channel=t->p[P_M1AMT]&15;
        double bend=e->bend[channel]*(2+(int)(e->controls[13]*10+.5));
        double ratio=pow(2.0,(bend+vibrato*e->mod_wheel[channel])/12+e->detune_position[k]*e->controls[7]*40/1200);
        m.fine=(int32_t)((ratio-1)*4096);
        m.inc=(uint32_t)fmin(m.inc*ratio,2147483647.0);
        m.pitch16 = clamp(v->pitch16+(int)floor(bend*16+.5),0,2047);
        m.amp0 = old_output * v->vel / 127;
        m.amp1 = output_amp * v->vel / 127;
        m.envq15 = output_amp;
        m.shape = 64 << 8;
        int32_t mono[CTL]={0},mono_r[CTL]={0};voice_t before=*v;
        int32_t noise_before=formant_nz;
        jv_carrier=external ? (input_l ? input_l:zero):NULL;
        formant_render(t, v, mono, CTL, &m);
        if(external){
            voice_t left_state=*v;int32_t left_f5[2];memcpy(left_f5,fmt_f5[k][0],sizeof left_f5);
            int32_t noise_after=formant_nz;
            *v=before;memcpy(v->ph,e->right_voice[k].ph,sizeof v->ph);memcpy(v->s,e->right_voice[k].s,sizeof v->s);v->vjit=e->right_voice[k].vjit;
            memcpy(fmt_f5[k][0],e->right_f5[k],sizeof e->right_f5[k]);formant_nz=noise_before;
            jv_carrier=input_r ? input_r:zero;formant_render(t,v,mono_r,CTL,&m);
            e->right_voice[k]=*v;memcpy(e->right_f5[k],fmt_f5[k][0],sizeof e->right_f5[k]);
            *v=left_state;memcpy(fmt_f5[k][0],left_f5,sizeof left_f5);formant_nz=noise_after;
        }else{
            memcpy(mono_r,mono,sizeof mono);e->right_voice[k]=*v;memcpy(e->right_f5[k],fmt_f5[k][0],sizeof e->right_f5[k]);
        }
        float pan0=e->pan_current[k],pan1=e->pan_position[k]*(float)e->controls[6];
        for(int i=0;i<CTL;++i) {
            float pan=pan0+(pan1-pan0)*(i+1)/CTL, gain=1.0f/(65536.0f*4096.0f);
            mix_l[i]+=mono[i]*gain*(pan>0 ? 1-pan : 1);mix_r[i]+=mono_r[i]*gain*(pan<0 ? 1+pan : 1);
        }
        e->pan_current[k]=pan1;
        v->env_out = output_amp;
        if(finished)v->active=0; /* Render the final ramp before retiring the voice. */
    }
    for (int i = 0; i < CTL; ++i) {
        float gain=previous_gain+(target_gain-previous_gain)*(i+1)/CTL;
        left[i]=mix_l[i]*gain;
        right[i]=mix_r[i]*gain;
        if(e->declick_pending){
            e->declick_offset[0]=e->last_output[0]-left[i];e->declick_offset[1]=e->last_output[1]-right[i];
            e->declick_remaining=JV_DECLICK_FRAMES;e->declick_pending=0;
        }
        if(e->declick_remaining){
            float phase=(JV_DECLICK_FRAMES-e->declick_remaining)/(float)JV_DECLICK_FRAMES;
            float fade=.5f+.5f*cosf(3.141592653589793f*phase);
            left[i]+=e->declick_offset[0]*fade;right[i]+=e->declick_offset[1]*fade;--e->declick_remaining;
        }
        e->last_output[0]=left[i];e->last_output[1]=right[i];
    }
    e->mix_gain=target_gain;
    jv_carrier=NULL;
    jv_carrier_gain=NULL;
}
void jv_render_stereo(jv_engine *e,float *left,float *right){jv_render_input(e,NULL,NULL,left,right);}

void jv_render(jv_engine *e, float *out) {
    float left[CTL],right[CTL];jv_render_stereo(e,left,right);
    for(int i=0;i<CTL;++i)out[i]=(left[i]+right[i])*.5f;
}
