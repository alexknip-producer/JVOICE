/* SPDX-License-Identifier: GPL-3.0-only
 * Exercise real key ownership, scheduler, rendering, locks and project codec. */
#define DRUM_OCT_NO_MAIN 1
#include "drumx_oct_test.c"

static int extensions(void)
{
    int bad=0,ok=1;uint32_t k,j,p,r,n;
    /* (V10.6r) EXTENDED is DRUM-X's default: power-on, an engine switch, INIT SOUND; a kit's load keeps the map
     * (CLASSIC included); LOFI's RTRG in the same slot is LOFI's alone */
    ui_power_on();
    trk[0].p[P_DKEYMODE]=0;power_sound_of(&trk[0],0);
    ok=trk_eng(&trk[0])==ENGI_DRUM&&trk[0].p[P_DKEYMODE]==1;
    set_engine_of(&trk[3],ENGI_DRUM);ok&=trk[3].p[P_DKEYMODE]==1;
    trk[3].p[P_DKEYMODE]=0;apply_preset_to(&trk[3],2);ok&=trk[3].p[P_DKEYMODE]==0;
    trk[3].p[P_DKEYMODE]=9;apply_preset_to(&trk[3],1);ok&=trk[3].p[P_DKEYMODE]==9;
    init_sound_of(&trk[3],ENGI_DRUM);ok&=trk[3].p[P_DKEYMODE]==1;
    set_engine_of(&trk[2],ENGI_LOFI);ok&=trk[2].p[P_LRTRG]==1;
    trk[2].p[P_LRTRG]=0;apply_preset_to(&trk[2],0);ok&=trk[2].p[P_LRTRG]==1;
    bad+=check("EXT the default (power-on, engine, INIT); a kit load keeps the map; LOFI's RTRG its own",ok);
    drum_setup();
    bad+=check("CLASSIC still selectable (the octave tests run on it)",!drum_ext(dt));
    dt->p[P_DKEYMODE]=1;
    for(k=35;k<=81;k++) ok &= drum_note_lane(dt,k)==drum_lane(k);
    bad+=check("EXT preserves every GM note 35..81",ok);
    ok=1;
    for(k=0;k<27;k++) {
        song.octave=-1;key_dn(k);lanes_b(2);
        uint32_t l=k<13?DV_KICK:DV_TOM;
        ok &= kb_snd[k][0]==k && drum_kit[3][l].st==(int32_t)k-(k<13?6:19) && !drum_kit[3][l].dm;
        key_upk(k);lanes_b(2);
        ok &= !kb_on[k] && !dxo_n[3];
    }
    bad+=check("EXT -1: 13 chromatic kicks and 14 toms, captured note-offs",ok);
    ok=!strcmp(drum_oct_label(dt,-1),"KICK / TOM");
    for(j=0;j<NLANE;j++) {                              /* (V10.6r) 2..9: every lane chromatic, the BELL's melodies */
        dt->p[P_DKEYMODE]=(int16_t)(2+j);
        char want[12];str_cpy(want,drum_lane_name(dt,j),6);str_cpy(want+str_len(want)," CHROM",7);
        ok &= !strcmp(drum_oct_label(dt,-1),want);
        for(k=0;k<27;k++) {
            song.octave=-1;key_dn(k);lanes_b(2);
            ok &= kb_snd[k][0]==k && drum_kit[3][j].st==(int32_t)k-13 && drum_note_lane(dt,k)==j;
            key_upk(k);lanes_b(2);
            ok &= !kb_on[k] && !dxo_n[3];
        }
    }
    bad+=check("EXT -1 chromatic of any lane (KEY MAP 2..9), its label",ok);
    dt->p[P_DKEYMODE]=9;ok=1;
    for(k=0;k<27;k+=5) {                                /* recorded: the same lane, the same pitch */
        step_t st;step_clear(&st);rec_drum(dt,&st,k,100,0);
        ok &= st.n==1&&st.note[0]==k&&step_lanes_of(dt,&st)==1u<<DV_BELL;
        quiet_kit();trk_note_on(dt,st.note[0],100);
        ok &= drum_kit[3][DV_BELL].st==(int32_t)k-13;
    }
    ui.home=0;for(p=0;p<NPAGES;p++) if(str_eq(PAGES[p].title,"KIT 4")) ui.page=(uint8_t)p;
    dx_kit3_knob(1,5);ok &= dt->p[P_DKEYMODE]==9;
    dx_kit3_knob(1,-1);ok &= dt->p[P_DKEYMODE]==8;
    bad+=check("EXT BELL CHROM: recorded steps keep lane and pitch; KIT 4 knob 2 walks the maps",ok);
    dt->p[P_DKEYMODE]=1;
    ok=1;
    for(k=0;k<12;k++) {
        song.octave=-2;key_dn(k);lanes_b(2);
        n=drum_fx_note(k);uint32_t l=DX_EXT_FX[k]&7;
        ok &= kb_snd[k][0]==n && (drum_kit[3][l].dm>>2)==k+1u;
        /* Changing map and octave while held must not lose the release. */
        dt->p[P_DKEYMODE]=0;song.octave=2;key_upk(k);lanes_b(2);
        ok &= !kb_on[k]&&!dxo_n[3];dt->p[P_DKEYMODE]=1;
        trk_note_on(dt,DRUM_LANE_NOTE[l],100);
        ok &= !drum_kit[3][l].dm;
    }
    bad+=check("EXT -2: per-hit FX captured; ordinary hits restore kit, no stuck key",ok);
    /* A hit can start after side_begin: bus contents must not leak from the
     * previously rendered track on the same core. */
    dt->p[P_DXIMG]=0;memset(dx_snd,0,sizeof dx_snd);
    dx_core_t *dc=&DXC;dc->xon=0;
    for(k=0;k<CTL;k++) dc->sb[2][k]=123456;
    trk_note_on(dt,drum_fx_note(0),100);
    int32_t g[4],*bus,amt;
    /* (V10.7.10, the owner) the FX KIT's hits no longer force their sound full into the delay or the reverb: with
     * the kit's SENDs OFF none of the twelve sends anything */
    for(k=0;k<12;k++) { trk_note_on(dt,drum_fx_note(k),100); ok &= drum_send(dt,DX_EXT_FX[k]&7u)==0u; }
    bad+=check("EXT -2 (V10.7.10): the twelve FX hits send nothing of their own (no forced DLY / REV)",ok);
    {   /* (V10.7.10, the owner: "remove the default sends from all kits") every factory kit dry: no SEND on any sound */
        uint32_t q,okk=1;
        for(q=0;q<DX_NKIT;q++) for(k=0;k<8;k++) okk &= DX_KITS[q].snd[k]==0;
        bad+=check("V10.7.10: every factory kit (LATIN PERC too) with all eight SENDs OFF",okk);
    }
    dx_snd[DV_KICK]=(uint8_t)(3u<<6|63u);              /* (the kit's own SEND: REV 63) */
    dc->xon=0;
    for(k=0;k<CTL;k++) dc->sb[2][k]=123456;
    trk_note_on(dt,drum_fx_note(0),100);
    ok=drum_side_of(dc,DV_KICK,drum_send(dt,DV_KICK),g,&bus,&amt)&&amt==63&&bus==dc->sb[2];
    for(k=0;k<CTL;k++) ok &= !bus[k];
    dx_snd[DV_KICK]=0;
    bad+=check("EXT sends: lazy per-core bus clearing for intra-block note-on",ok);
    uint32_t hashes[27];ok=1;
    for(k=0;k<27;k++) {
        dxo_t e={0};e.half=2;e.note=(uint8_t)k;uint32_t h=2166136261u;
        for(p=0;p<16;p++) for(j=0;j<3;j++) {
            n=dxp_note(&e,p,j,&r);h=(h^n^r<<8)*16777619u;
            ok &= r==1||r==2||r==4;
        }
        for(j=0;j<k;j++) ok &= hashes[j]!=h;
        hashes[k]=h;
    }
    bad+=check("EXT: all 27 keys have distinct 16-step fills and recordable rates",ok);
    /* (V10.6r) the fills are drum fills: a snare roll building, toms high to low, flams, the downbeat after */
    {
        dxo_t e={0};e.half=2;uint32_t cnt[4]={0},hits[4]={0},lo=99,okd=1;int32_t last=99;
        e.note=9;                                       /* snare roll, a bar */
        for(p=0;p<16;p++){n=dxp_note(&e,p,0,&r);if(n==38){cnt[p/4]++;hits[p/4]+=r;}}
        ok=hits[0]<=hits[1]&&hits[1]<hits[2]&&hits[2]<hits[3]&&cnt[3]==4;
        e.note=10;                                      /* toms, a bar: high to low */
        for(p=0;p<16;p++){n=dxp_note(&e,p,0,&r);int32_t st=DRUM_GM[drum_gm_ix(n)][1];
            okd &= drum_lane(n)==DV_TOM&&st<=last;last=st;if(st<(int32_t)lo)lo=(uint32_t)(st+50);}
        ok &= okd&&last<0;
        e.note=3;dx_in_tch=0;n=dxp_note(&e,0,0,&r);ok &= n==38&&dx_in_tch==3;dx_in_tch=0;
        e.note=3;n=dxp_note(&e,0,2,&r);ok &= n==36&&dx_in_tch==3;dx_in_tch=0;
        for(k=0;k<27;k++){e.note=(uint8_t)k;e.rec=255;
            ok &= dxp_note(&e,0,0,&r)==(k>=18?49u:46u)&&dxp_note(&e,0,2,&r)==36u&&!dxp_note(&e,0,1,&r);
            e.rec=0;ok &= k>=9||(dxp_note(&e,8,0,&r)==46u);}
        bad+=check("EXT fills: snare roll 8ths>16ths>32nds, tom descent, flams, open hat/crash + kick on the downbeat",ok);
        drum_setup();dt->p[P_DKEYMODE]=1;
        step_t *s0=&dt->step[seq_src(dt,dt->seq_idx)];step_clear(s0);
        e.note=3;e.vel=100;e.rec=0;dxp_fire(dt,&e,0,0,2);
        ok=(s0->hit&1u)&&(s0->hit&2u)&&step_lmode(s0,DV_KICK)==LM_FLAM&&step_lmode(s0,DV_SNARE)==LM_FLAM&&!dx_in_tch;
        step_clear(s0);e.note=9;dxp_fire(dt,&e,14,0,2);
        ok &= (s0->hit&2u)&&step_lmode(s0,DV_SNARE)==LM_X2;
        bad+=check("EXT fills record as hits: flams as FLAM, the roll's 32nds as x2",ok);
    }
    ok=1;
    for(k=0;k<27;k++) {
        drum_setup();dt->p[P_DKEYMODE]=1;song.octave=1;
        key_dn(k);lanes_b(2);
        ok &= dxo_n[3]==1 && dxo[3][0].half==2 && dxo[3][0].ph==255 && kb_snd[k][0]==86u+k;
        uint32_t per=div_samples(dt->p[P_SDIV])/4u;
        for(p=0;p<68;p++) dxo_tick(dt,per);
        ok &= dxo[3][0].ph==0 && dxo[3][0].part==0;
        song.octave=0;key_upk(k);lanes_b(2);
        ok &= !dxo_n[3]&&!kb_on[k];
    }
    bad+=check("EXT fills repeat while held, wrap after one bar, release at changed octave",ok);
    drum_setup();dt->p[P_DKEYMODE]=1;song.octave=1;seq_start();
    lanes_until(1,step_len(1)/8u);uint32_t before=vage;
    key_dn(0);lanes_b(2);
    ok=dxo_n[3]==1&&dxo[3][0].ph==255u&&before==vage;
    lanes_until(2,step_len(2)/8u);
    ok &= dxo_n[3]==1&&dxo[3][0].ph==0;
    key_dn(12);lanes_b(2);ok &= dxo_n[3]==2&&dxo[3][0].note==0&&dxo[3][1].note==12;
    lanes_until(3,step_len(3)/8u);ok &= dxo_n[3]==1&&dxo[3][0].note==12;
    key_upk(0);lanes_b(2);ok &= dxo_n[3]==1;
    key_dn(4);lanes_b(2);key_dn(26);lanes_b(2);
    lanes_until(4,step_len(4)/8u);ok &= dxo_n[3]==1&&dxo[3][0].note==26;
    key_upk(12);key_upk(26);lanes_b(2);
    lanes_until(5,step_len(5)/8u);ok &= !dxo_n[3];seq_stop();
    bad+=check("EXT quantized choke: latest key replaces fill; old release cannot cancel or restart it",ok);
    drum_setup();dt->p[P_DKEYMODE]=1;
    uint32_t quarter=div_samples(dt->p[P_SDIV])/4u;
    dxo_on(dt,86,0,100,DXO_MIDI+86);
    for(k=0;k<5;k++) dxo_tick(dt,quarter); /* active, quarter 1 */
    dxo_on(dt,112,0,100,DXO_MIDI+112);ok=dxo_n[3]==2;
    dxo_tick(dt,quarter);dxo_tick(dt,quarter);
    ok &= dxo_n[3]==2&&dxo[3][0].note==0;
    dxo_tick(dt,quarter);ok &= dxo_n[3]==1&&dxo[3][0].note==26&&dxo[3][0].ph==0;
    dxo_on(dt,87,0,100,DXO_MIDI+87);dxo_off(dt,DXO_MIDI+87);
    for(k=0;k<4;k++) dxo_tick(dt,quarter);
    ok &= dxo_n[3]==1&&dxo[3][0].note==26;
    bad+=check("EXT stopped clock: choke shares phase; releasing queued key cancels only its replacement",ok);
    dxo_t human={0};human.half=2;human.vel=100;human.ph=255;uint32_t variation=0;
    for(k=0;k<64;k++) {dxp_step(&human,k);variation|=1u<<(human.r>>4);}
    bad+=check("EXT humanize varies velocity per step and keeps the timing grid",variation&&(variation&(variation-1u)));
    drum_setup();dt->p[P_DKEYMODE]=1;
    midi_play(dt,0,113,100);
    bad+=check("EXT external keyboard starts ratchet sets",dxo_n[3]==1&&dxo[3][0].half==3);
    dxo_off(dt,DXO_MIDI+113);
    bad+=check("EXT external keyboard releases the performance",!dxo_n[3]);
    drum_setup();dt->p[P_DKEYMODE]=1;
    for(k=0;k<16;k++) {
        dt->step[k]=(step_t){{81},1,ST_NOTE,0,100,1u<<DV_BELL};
        step_set_lmode(&dt->step[k],DV_BELL,LM_X8);
    }
    step_t original[NSTEP];memcpy(original,dt->step,sizeof original);
    seq_start();lanes_until(1,step_len(1)/8u);dxo_on(dt,86,0,100,DXO_MIDI+86);
    lanes_until(2,step_len(2)/8u);before=vage;seq_note_on(dt,81,100);
    ok=dxp_overrides(dt)&&vage==before&&!dxp_overrides(&trk[0]);
    dxo_off(dt,DXO_MIDI+86);seq_stop();
    bad+=check("EXT live fill suppresses original sequencer hits without altering its pattern",ok&&!memcmp(original,dt->step,sizeof original));
    /* Recording stores the same lane/rate recipe, not virtual control notes. */
    drum_setup();dt->p[P_DKEYMODE]=1;
    for(k=0;k<16;k++) {
        dt->step[k]=(step_t){{81},1,ST_NOTE,0,100,1u<<DV_BELL};
        step_set_lmode(&dt->step[k],DV_BELL,LM_X8);
        dt->step[k].comp=dt->step[k].comp2=CO_MAKE(CA_SOUND,CC_ALWAYS);
        dt->step[k].carg=2;dt->step[k].carg2=1;
    }
    song.rec=1u<<3;seq_start();
    lanes_until(1,step_len(1)/8u);dxo_on(dt,114,0,100,DXO_MIDI+114);
    lanes_until(5,step_len(5)/4u);dxo_off(dt,DXO_MIDI+114);seq_stop();
    ok=1;n=0;
    for(k=2;k<5;k++) {
        const step_t *s=&dt->step[k];n+=s->hit!=0;ok &= s->vel>=96u&&s->vel<=103u;
        ok &= !(s->hit&(1u<<DV_BELL))&&!s->n&&!step_lmode(s,DV_BELL)&&
              s->comp==CO_MAKE(CA_SOUND,CC_ALWAYS)&&s->carg==2&&s->comp2==s->comp&&s->carg2==1;
        for(j=0;j<s->n;j++) ok &= s->note[j]<86;
        for(j=0;j<8;j++) ok &= step_lmode(s,j)<=LM_X4;
    }
    bad+=check("EXT recording replaces original notes and ratchets, preserving both components",ok&&n>=2);
    bad+=check("EXT release keeps the recorded fill; steps outside it remain intact",
        !dt->step[5].n&&!(dt->step[5].hit&(1u<<DV_BELL))&&dt->step[1].note[0]==81&&dt->step[6].note[0]==81);
    drum_setup();dt->p[P_DKEYMODE]=1;
    for(k=0;k<16;k++) dt->step[k]=(step_t){{81},1,ST_NOTE,0,100,1u<<DV_BELL};
    song.rec=1u<<3;seq_start();lanes_until(1,step_len(1)/8u);
    dxo_on(dt,86,0,100,DXO_MIDI+86);lanes_until(4,step_len(4)/8u);
    bad+=check("EXT recording replaces original hits with rests where the fill is silent",
        dt->step[3].time==ST_REST&&!dt->step[3].hit&&!dt->step[3].n&&!step_lm_any(&dt->step[3]));
    dxo_off(dt,DXO_MIDI+86);seq_stop();
    /* Lane view and lane editing must agree with the renderer for encoded FX. */
    step_t *s=&dt->step[7];step_clear(s);s->time=ST_NOTE;s->n=1;s->note[0]=drum_fx_note(10);s->vel=100;
    ok=step_lanes_of(dt,s)==(1u<<DV_TOM);
    grid_hit(dt,7,DV_TOM,0);ok &= !step_lanes_of(dt,s);
    bad+=check("EXT grid removes the correct chromatic/effected lane",ok);
    return bad;
}

static int lyrics(void)
{
    int bad=0,ok=1;uint32_t l,i,ns,n;
    static const struct {uint8_t lang,code;const char *word;} cases[]={
        {LG_RU,PH_ZH,"zha"},{LG_DE,PH_SH,"scha"},{LG_DE,PH_TS,"za"},
        {LG_FR,PH_SH,"chat"},{LG_ZH,PH_SH,"xian"},{LG_PT,PH_NY,"nha"},{LG_GN,PH_NY,"nya"}};
    for(i=0;i<NELEM(cases);i++) {
        uint8_t ph[SW_MAX];n=sy_word(cases[i].lang,cases[i].word,strlen(cases[i].word),ph,&ns);
        ok &= n&&ns&&(ph[0]&63u)==cases[i].code;
    }
    bad+=check("VOICE: six language readers apply distinct phonetic rules",ok);
    ok=1;
    for(l=LG_RU;l<LG_N;l++) {
        for(i=0;i<64;i++) {
            struct {uint8_t pre,ph[SW_MAX],post;} buf={.pre=0xA5,.post=0x5A};
            char text[64];for(uint32_t j=0;j<63;j++) text[j]="shcha~ng'yue"[(i+j)%11];text[63]=0;
            n=sy_word(l,text,63,buf.ph,&ns);
            ok &= n<=SW_MAX&&buf.pre==0xA5&&buf.post==0x5A;
            for(uint32_t j=0;j<n;j++) ok &= (buf.ph[j]&63u)<PH_COUNT;
        }
    }
    bad+=check("VOICE language readers bound phonemes on long input",ok);
    ui_power_on();track_t *t=&trk[0];set_engine_of(t,ENGI_VOICE);t->engine=t->eng_req;song.sel=0;
    lyr_set(0,"hola luna.");t->p[P_VLANG]=LG_ES;t->p[P_VLYR]=LYR_FIXED;t->p[P_E0]=2;
    sing[0].pend=1;sing_block(t);sing_t first=sing[0];
    sing[0].pend=1;sing_block(t);
    ok=sing[0].wsy==first.wsy&&sing[0].nsy==first.nsy&&!strcmp(sing[0].w,first.w);
    bad+=check("VOICE TEXT FIXED: repeated POS really repeats the selected syllable",ok);
    t->p[P_E0]=0;sing[0].pend=1;sing_block(t);
    bad+=check("VOICE TEXT FIXED: POS 0 advances automatically",sing[0].nsy!=first.nsy);
    /* Same word and syllable selected from the real text, including punctuation. */
    lyric_word=31;lyric_syl=1;char label[20];uint32_t pos=lyric_pick(label);
    bad+=check("LYRIC STEP clamps selection to the last word past punctuation",pos==3&&!strcmp(label,"luna"));
    lyric_word=0;lyric_syl=2;pos=lyric_pick(label);
    bad+=check("LYRIC STEP maps a word/syllable to absolute POS",pos==2&&!strcmp(label,"hola"));
    ui.home=0;for(i=0;i<NELEM(PAGES);i++) if(PAGES[i].graph==GR_LYRSTEP)ui.page=(uint8_t)i;
    ui.cursor=4;motion_clear(t);act_do();
    ok=motion.count==1&&MOTION_STEP(&motion.event[0])==4&&MOTION_ID(&motion.event[0])==P_E0&&
       (motion.event[0].param&MOTION_LOCK)&&t->p[P_VLYR]==LYR_FIXED;
    bad+=check("LYRIC STEP places a real POS lock without using either component",ok);
    /* Saved project preserves the new choices and locks in the current format. */
    t->p[P_VLANG]=LG_GN;trk[3].p[P_DKEYMODE]=1;
    static project_t before,after;static project_store_t blob;
    project_capture(&before);
    ok=proj_pack(&blob,&before)&&proj_import(&after,&blob,sizeof blob)&&
       after.t[0].p[P_VLANG]==LG_GN&&after.t[0].p[P_VLYR]==LYR_FIXED&&after.t[3].p[P_DKEYMODE]==1;
    bad+=check("Project round trip preserves languages, fixed lyrics and EXT key map",ok);
    return bad;
}
/* (V10.6r) SCENE: the kit's DRUM role GEN, GENE's drums a key a variation */
static int scene_gen(void)
{
    int bad=0,ok;int16_t *vp;uint32_t k,i,h[4];
    ui_power_on();song.playing=0;
    set_engine_of(&trk[0],ENGI_DRUM);trk[0].engine=trk[0].eng_req;
    sc_trk=0;scn_role[0]=SC_DRUM;
    const param_desc_t *d=sc_desc(3,&vp);
    ok=d->max==SC_DGEN&&!strcmp(d->names[SC_DGEN],"GEN");
    tg_config[0].energy=33;tg_config[0].complex=7;
    trk[0].p[P_SLEN]=16;sc_put(3,SC_DGEN);
    d=sc_desc(3,&vp);ok&=scn_pat[0]==SC_DGEN&&*vp==SC_DGEN;
    bad+=check("SCENE DRUM: GEN shown and chosen",ok);
    ok=1;
    for(k=0;k<4;k++) {
        for(i=0;i<NSTEP;i++) gclear(&trk[0].step[i]);
        scn_dreq=(uint8_t)(k+1u);sc_beat();
        uint32_t n=0,x=2166136261u;
        for(i=0;i<32;i++){const step_t *s=&trk[0].step[i];n+=s->time==ST_NOTE&&s->hit;x=(x^s->hit^i<<8)*16777619u;}
        ok &= n>=4&&trk[0].p[P_SLEN]>=32;h[k]=x;
    }
    ok &= h[0]!=h[1]&&h[0]!=h[2]&&h[0]!=h[3]&&h[1]!=h[2];
    ok &= tg_config[0].energy==33&&tg_config[0].complex==7&&tg_config[0].role!=4u;
    bad+=check("SCENE DRUM GEN: each key a GENE beat (base, dense, break, fill); TRACK GEN's settings kept",ok);
    sc_put(3,1);ok=scn_pat[0]==1;
    scn_dreq=3;sc_beat();
    bad+=check("SCENE DRUM: the fixed styles still play",ok&&trk[0].step[0].hit);
    scn_role[0]=SC_KEEP;
    return bad;
}
int main(void)
{
    int bad=extensions()+lyrics()+scene_gen();
    printf("drum/voice extensions: %s\n",bad?"FAILED":"all ok");return bad!=0;
}
