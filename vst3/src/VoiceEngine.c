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
    for(int k=0;k<NTRK;++k){e->grain[k].bits=(seed^(0x5a39u*(k+1)))&0x7fffu;if(!e->grain[k].bits)e->grain[k].bits=1;}
    e->controls[9]=7.0/31; /* eight poly voices until plugin parameters are applied */
    e->controls[12]=72.0/127;
    e->controls[13]=1.0/11; /* Pitch bend remains +/-2 semitones by default. */
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
    if(index==9){int cap=1+(int)(value*31+.5);if(e->controls[8]>=.5 && e->controls[29]>=.5 && cap<4)++cap;for(int k=cap+1;k<NTRK;++k){if(trk[k].v[0].active)e->declick_pending=1;trk[k].v[0].active=trk[k].v[0].gate=0;}}
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
/* Snapshot the preceding chord before allocating the next one. Simultaneous
 * note-ons choose distinct nearest prior pitches where possible. */
static double jv_glide_start(jv_engine *e,int channel,int note,int chord)
{
    if(channel<0 || channel>=16)return note;
    if(!chord || !e->glide_epoch)++e->glide_epoch;
    if(e->glide_group[channel]!=e->glide_epoch){
        e->glide_group[channel]=e->glide_epoch;
        e->glide_source_count[channel]=e->glide_source_used[channel]=0;
        uint32_t ages[32];int count=0;
        for(int k=1;k<NTRK && count<32;++k){voice_t *v=&trk[k].v[0];
            if(!v->active || trk[k].p[P_M1AMT]!=channel)continue;
            int duplicate=0;for(int j=0;j<count;++j)if(ages[j]==v->age)duplicate=1;
            if(!duplicate){ages[count]=v->age;e->glide_sources[channel][count++]=e->glide_now[k];}
        }
        if(!count && e->glide_last_valid[channel])e->glide_sources[channel][count++]=e->glide_last[channel];
        e->glide_source_count[channel]=(uint8_t)count;
    }
    int count=e->glide_source_count[channel],best=-1;double distance=1e9;
    for(int pass=0;pass<2 && best<0;++pass)for(int i=0;i<count;++i){
        if(!pass && (e->glide_source_used[channel]&(1u<<i)))continue;
        double d=fabs(e->glide_sources[channel][i]-note);
        if(d<distance){distance=d;best=i;}
    }
    double start=best<0 ? note:e->glide_sources[channel][best];
    if(best>=0)e->glide_source_used[channel]|=(uint8_t)(1u<<best);
    e->glide_last[channel]=note;e->glide_last_valid[channel]=1;
    return start;
}
void jv_note_on(jv_engine *e, int channel, int note, int vel, int chord, int note_id)
{
    jv_bind(e);
    int direction=(int)(e->controls[26]*2+.5);
    int last_valid=channel>=0 && channel<16 && e->glide_last_valid[channel];
    double last_pitch=last_valid ? e->glide_last[channel]:note;
    double start_pitch=jv_glide_start(e,channel,note,chord);
    /* Directional glide follows the last played MIDI note, even if a nearer
     * polyphonic voice is still sounding or its release tail is active. */
    if(direction)start_pitch=last_pitch;
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
    const int requested=1+(int)(e->controls[9]*31+.5);
    const int sub_on=unison && e->controls[29]>=.5;
    const int nv=unison ? requested+(sub_on && requested<4 ? 1:0):1;
    const int sub_count=sub_on ? (requested<4 ? 1:(requested/2<8 ? requested/2:8)):0;
    const int main_count=nv-sub_count;
    const int cap=unison ? nv:requested;
    const uint32_t age=++e->age;
    static const float VP[8]={-1,1,-.5f,.5f,-.75f,.75f,-.25f,.25f};
    for(int sub=0;sub<nv;++sub) {
        const int is_sub=sub>=main_count;
        const int group_count=is_sub ? sub_count:main_count, group_index=is_sub ? sub-main_count:sub;
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
        v->note = (uint8_t)note;v->vel = (uint8_t)(e->controls[28]>=.5 ? vel:100);v->active = v->gate = 1;
        v->pitch16 = v->pitch_cur = v->gfrom = note * 16;v->age = age;
        if(unison && sub)v->ph[0]=noise32(&formant_nz); /* decorrelate layered oscillators */
        e->ids[slot] = note_id;t->p[P_M1AMT] = (int16_t)channel;
        e->key_down[slot]=1;
        e->octave_offset[slot]=is_sub ? -12:0;
        e->detune_position[slot]=unison ? (group_count==1 ? 0 : 2.0f*group_index/(group_count-1)-1) : VP[(slot-1)%8];
        e->mod_offset[slot]=unison ? (double)sub/nv:fmod((slot-1)*.6180339887498949,1.0);
        e->pan_position[slot]=unison ? (group_count==1 ? 0 : 2.0f*group_index/(group_count-1)-1) : VP[(slot-1)%8];
        e->pan_current[slot]=e->pan_position[slot]*(float)e->controls[6];
        e->right_voice[slot]=*v;memset(e->right_f5[slot],0,sizeof e->right_f5[slot]);
        e->lateral_hold[slot]=0;
        e->envelope[slot]=0;
        e->glide_from[slot]=e->glide_now[slot]=start_pitch;
        e->glide_elapsed[slot]=0;
        double seconds=e->controls[24]*e->controls[24]*10.0;
        if((direction==1 && note<=start_pitch) || (direction==2 && note>=start_pitch))seconds=0;
        if(unison && nv>1 && e->controls[25]>0){
            double position=2.0*sub/(nv-1)-1.0;
            seconds=fmin(10.0,seconds*pow(4.0,position*e->controls[25]));
        }
        e->glide_duration[slot]=(uint32_t)(seconds*FS+.5);
        if(!e->glide_duration[slot])e->glide_now[slot]=note;
    }
}
static void jv_release_slot(jv_engine *e,int k)
{
    voice_t *v=&trk[k].v[0];
    if(!v->active || !v->gate)return;
    v->gate=0;
    const sing_t *s=sing_view(&trk[k]);
    if(trk[k].p[P_VLANG]==LG_RU && s->cd<s->np && (s->ph[s->np-1]==PH_L || s->ph[s->np-1]==PH_RU_L))
        e->lateral_hold[k]=(uint16_t)sy_ms(80);
}
void jv_sustain(jv_engine *e,int channel,double value)
{
    if(channel<0 || channel>=16 || !isfinite(value))return;
    jv_bind(e);
    int down=value>=.5;
    if(e->sustain[channel] && !down)
        for(int k=1;k<NTRK;++k)
            if(trk[k].p[P_M1AMT]==channel && !e->key_down[k])jv_release_slot(e,k);
    e->sustain[channel]=(uint8_t)down;
}
void jv_note_off(jv_engine *e, int channel, int note, int note_id)
{
    if(channel<0 || channel>=16)return;
    jv_bind(e);
    int slot = 0;
    for (int k = 1; k < NTRK; ++k) {
        voice_t *v = &trk[k].v[0];
        if (v->active && v->gate && e->key_down[k] && v->note == note && trk[k].p[P_M1AMT] == channel &&
            (note_id < 0 || e->ids[k] == note_id) &&
            (!slot || v->age < trk[slot].v[0].age)) slot = k;
    }
    if(slot) {
        uint32_t age=trk[slot].v[0].age;
        for(int k=1;k<NTRK;++k)if(trk[k].v[0].active && trk[k].v[0].age==age){
            e->key_down[k]=0;
            if(!e->sustain[channel])jv_release_slot(e,k);
        }
    }
}
void jv_panic(jv_engine *e)
{
    jv_bind(e);
    for (int k = 1; k < NTRK; ++k) memset(trk[k].v, 0, sizeof trk[k].v);
    e->declick_pending=e->declick_remaining=0;
    e->last_output[0]=e->last_output[1]=0;
    memset(e->glide_last_valid,0,sizeof e->glide_last_valid);
    memset(e->glide_group,0,sizeof e->glide_group);e->glide_epoch=0;
    memset(e->lateral_hold,0,sizeof e->lateral_hold);
    memset(e->sustain,0,sizeof e->sustain);
    memset(e->key_down,0,sizeof e->key_down);
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
    double mod_phase=e->mod_phase;
    double vibrato=sin(mod_phase*6.2831853071795864769)*e->controls[16]*.5;
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
        double played=v->note;
        if(e->glide_elapsed[k]<e->glide_duration[k]){
            double phase=(double)e->glide_elapsed[k]/e->glide_duration[k];
            played=e->glide_from[k]+(v->note-e->glide_from[k])*phase;
            e->glide_elapsed[k]=e->glide_elapsed[k]+CTL<e->glide_duration[k] ? e->glide_elapsed[k]+CTL:e->glide_duration[k];
        }
        e->glide_now[k]=played;
        played+=e->octave_offset[k];
        int32_t pitch16=clamp((int32_t)floor(played*16),0,2047);
        m.inc = pitch_inc((uint32_t)pitch16);
        int channel=t->p[P_M1AMT]&15;
        double bend=e->bend[channel]*(1+(int)(e->controls[13]*11+.5));
        double voice_vibrato=e->controls[27]>0 ? sin((mod_phase+e->mod_offset[k]*e->controls[27])*6.2831853071795864769)*e->controls[16]*.5:vibrato;
        double ratio=pow(2.0,(bend+voice_vibrato*e->mod_wheel[channel]+played-pitch16/16.0)/12+e->detune_position[k]*e->controls[7]*40/1200);
        m.fine=(int32_t)((ratio-1)*4096);
        m.inc=(uint32_t)fmin(m.inc*ratio,2147483647.0);
        m.pitch16 = clamp((int)floor((played+bend)*16+.5),0,2047);
        m.amp0 = old_output * v->vel / 127;
        m.amp1 = output_amp * v->vel / 127;
        m.envq15 = output_amp;
        m.shape = 64 << 8;
        int32_t mono[CTL]={0},mono_r[CTL]={0};voice_t before=*v;
        int32_t noise_before=formant_nz;
        jv_grain=&e->grain[k];jv_grain_state grain_before=*jv_grain;
        jv_carrier=external ? (input_l ? input_l:zero):NULL;
        formant_render(t, v, mono, CTL, &m);
        if(external){
            voice_t left_state=*v;int32_t left_f5[2];memcpy(left_f5,fmt_f5[k][0],sizeof left_f5);
            int32_t noise_after=formant_nz;jv_grain_state grain_after=*jv_grain;
            *v=before;memcpy(v->ph,e->right_voice[k].ph,sizeof v->ph);memcpy(v->s,e->right_voice[k].s,sizeof v->s);v->vjit=e->right_voice[k].vjit;
            memcpy(fmt_f5[k][0],e->right_f5[k],sizeof e->right_f5[k]);formant_nz=noise_before;
            *jv_grain=grain_before;
            jv_carrier=input_r ? input_r:zero;formant_render(t,v,mono_r,CTL,&m);
            *jv_grain=grain_after;
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
    jv_grain=NULL;
}
void jv_render_stereo(jv_engine *e,float *left,float *right){jv_render_input(e,NULL,NULL,left,right);}

void jv_render(jv_engine *e, float *out) {
    float left[CTL],right[CTL];jv_render_stereo(e,left,right);
    for(int i=0;i<CTL;++i)out[i]=(left[i]+right[i])*.5f;
}
