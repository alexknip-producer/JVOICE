/* SPDX-License-Identifier: GPL-3.0-only
 * Selected-track generation. Context and a temporary track live on the main
 * stack; no audio buffers, project-format changes or new DSP path. */
typedef struct { uint16_t seed; uint8_t genre,role,complex,energy; } tg_config_t;
/* Keep the new UI settings out of the compiler's merged DSP globals. */
static tg_config_t tg_config[NTRK] __attribute__((section(".data.trackgen"))) = {[0 ... NTRK-1]={1,0,0,50,50}};
static const char *const TG_ROLE[] BTAPP_RO = {"BASS","CHORDS","LEAD","ARP"};
static int trackgen_ok(const track_t *t)
{return eng_ok(t->eng_req)&&t->eng_req!=ENGI_INPUT;}

BTAPP_FN static void trackgen_knob_bt(uint32_t k,int32_t s)
{
    tg_config_t *c=&tg_config[song.sel%NTRK];
    if(!k) c->genre=gen_genre_next(c->genre,s);
    else if(k==1u) c->seed=(uint16_t)((c->seed+10000+clamp(accel(EN_K2,s,9999),-9999,9999))%10000);
    else if(k==2u) {
        uint8_t *p=drum_track(TSEL)?&c->complex:&c->role;
        *p=(uint8_t)clamp((int32_t)*p+s*(drum_track(TSEL)?5:1),0,drum_track(TSEL)?100:3);
    } else c->energy=(uint8_t)clamp((int32_t)c->energy+s*5,0,100);
}
BTAPP_FN static void trackgen_cards_bt(void)
{
    const tg_config_t *c=&tg_config[song.sel%NTRK];char b[12];
    hot_text_column(0,"GENRE",c->genre==GEN_EXPLORE?"EXPLORE":c->genre?GENRE[(c->genre-1u)%NGENRE].name:"RANDOM");
    fmt_int(b,c->seed);hot_text_column(1,"SEED",b);
    fmt_int(b,c->complex);hot_text_column(2,drum_track(TSEL)?"COMPLEX":"ROLE",drum_track(TSEL)?b:TG_ROLE[c->role%4u]);
    fmt_int(b,c->energy);hot_unit_column(3,"ENERGY",b,"%");
}
static void trackgen_knob(uint32_t k,int32_t s) {if(bt_hw_seg()) trackgen_knob_bt(k,s);}
static void trackgen_cards(void) {if(bt_hw_seg()) trackgen_cards_bt();}
static __attribute__((noinline)) void trackgen_make(void)
{
    track_t *t=TSEL;uint32_t k=trk_index(t),i,j,n,len,f;
    const tg_config_t *c=&tg_config[k];
    struct { __typeof__(gp) p; __typeof__(gs) s; ggenre_t g; __typeof__(gx) x; uint32_t q; }
        save={gp,gs,gmut,gx,gq};
    if(!bt_hw_seg()) {seg_missing();return;}   /* (V10.6y: gen.c gen_pick in the segment) */
    track_t temp;
    _Static_assert(sizeof temp+sizeof save<8192,"track generator main-stack budget (15.75 KiB stack)");
    if(!trackgen_ok(t)||chain_busy()) {ui_message("STOP SONG TO EDIT");return;}
    /* These pattern helpers never write gmut_name (only gen_make does). */
    memset(&temp,0,sizeof temp);memcpy(temp.p,t->p,sizeof temp.p);
    temp.eng_req=t->eng_req;temp.engine=t->engine;
    len=(uint32_t)clamp(t->p[P_SLEN],1,NSTEP);
    for(i=0;i<NSTEP;i++) gclear(&temp.step[i]);
    memset(&gs,0,sizeof gs);memset(&gx,0,sizeof gx);
    gp.seed=c->seed;gp.energy=c->energy;gp.complex=c->complex;gp.human=30;gp.swing=GA_AUTO;
    for(i=0;i<GP_N;i++) gp.amt[i]=GA_AUTO;
    gseed(GS_PICK);
    gs.gi=c->genre==GEN_EXPLORE?NGENRE:c->genre?(c->genre-1u)%NGENRE:gr()%NGENRE;
    if(c->genre==GEN_EXPLORE) {gx.on=1;gx_genre(&gmut);}
    else gmut=GENRE[gs.gi];
    gs.g=&gmut;
    if(gmut.lead==LD_NONE) gmut.lead=LD_HOOK;
    if(drum_track(t)&&gmut.drum==DM_NONE) gmut.drum=DM_SPARSE;
    gen_pick();
    gs.root=t->p[P_ROOT];gs.scale=t->p[P_SCALE];
    uint32_t mask=SCALE_MASK[(uint32_t)t->p[P_SCALE]%NELEM(SCALE_MASK)],count=0;
    for(i=0;i<12;i++) count+=(mask>>i)&1u;
    if(count==7u) for(i=0,j=0;i<12;i++) if((mask>>i)&1u) gs.sc[j++]=(uint8_t)i;
    gs.len=len;gs.energy=c->energy;
    uint32_t r=drum_track(t)?RL_DRUM:RL_BASS+c->role%4u;
    gs.role[k]=(uint8_t)r;gs.act[r]=1;
    static const uint8_t amount[4]={GP_BASS,GP_CHORDS,GP_LEAD,GP_ARP};
    gs.amt[r==RL_DRUM?GP_DRUMS:amount[c->role%4u]]=c->energy;
    for(i=0;i<NTRK;i++) if(i!=k&&drum_track(&trk[i]))
        for(j=0;j<(uint32_t)clamp(trk[i].p[P_SLEN],1,NSTEP);j++)
            if(trk[i].step[j].time==ST_NOTE) gs.kick|=(uint64_t)(trk[i].step[j].hit&1u)<<j;
    gplan(0);
    if(r==RL_DRUM&&c->role==4u) {gs.drum_fill=gmut.fills?gmut.fills:FL_ROLL;gs.song=1;}   /* (V10.6r) SCENE GEN's FILL */
    gen_role(&temp,k,len);
    if(r!=RL_DRUM) for(i=0;i<len;i++) {
        step_t *s=&temp.step[i];n=0;
        for(j=0;j<s->n&&j<4u;j++) {
            uint32_t note=(uint32_t)clamp(scale_snap(t,s->note[j]),0,127),a;
            for(a=0;a<n&&s->note[a]!=note;a++);
            if(a==n) s->note[n++]=(uint8_t)note;
        }
        s->n=(uint8_t)n;
    }
    gp=save.p;gs=save.s;gmut=save.g;gx=save.x;gq=save.q;
    undo_mark();
    f=motion_guard();
    memcpy(t->step,temp.step,len*sizeof(step_t));
    shift_valid[0][k]=shift_valid[1][k]=0;
    t->rat_left=t->lr_on=t->ss_n=t->ss_i=0;
    t->arp_from=t->cv_on=t->csp_n=t->jump_to=t->rev=0;t->seq_active=1;
    motion_unguard(f);
    ui_message("TRACK GENERATED");ui.force=1;
}
/* (V10.6m) GEN chosen (or its role): the generator writes the track a line, a bass or a lead of the last GENE's genre
 * (any, after EXPLORE or none), in the track's key, 2 bars at least (gen_track.c, undoable); played by the sequence */
/* (V10.6r) DRUM's GEN: the kit's track GENE's drums (gen_drums, the genre's maps), a key a variation: by its place mod 4
 * BASE, DENSE, BREAK (sparse, edited), FILL (the genre's fill in the second bar); a set of four every 4 keys up (the
 * seed: GENE's + the key's set). The track's own TRACK GEN settings are kept */
static __attribute__((noinline)) void sc_gen(uint32_t i)
{
    static const uint8_t EN[4] = {50, 85, 25, 70}, CX[4] = {20, 45, 70, 50};
    uint32_t r = scn_role[i % NTRK], sel = song.sel, v = sc_dk % 4u;
    track_t *t = &trk[i % NTRK];
    tg_config_t *c = &tg_config[i % NTRK], keep = *c;
    if (r == SC_DRUM ? scn_pat[i % NTRK] % (SC_DGEN + 1u) != SC_DGEN || !drum_track(t)
                     : (r != SC_BASS && r != SC_LEAD) || scn_pat[i % NTRK] % SC_NPAT != SC_GEN || drum_track(t)) return;
    c->genre = gp.genre == GEN_EXPLORE ? 0u : gp.genre;
    c->seed = (uint16_t)(r == SC_DRUM ? (gp.seed + sc_dk / 4u * 997u) % 10000u : rng() % 10000u);
    c->role = (uint8_t)(r == SC_LEAD ? 2u : r == SC_DRUM && v == 3u ? 4u : 0u);   /* (a kit's: 4 the fill forced, trackgen_make) */
    if (r == SC_DRUM) { c->energy = EN[v]; c->complex = CX[v]; }
    if (t->p[P_SLEN] < 32) t->p[P_SLEN] = 32;
    song.sel = (uint8_t)(i % NTRK);
    trackgen_make();
    song.sel = (uint8_t)sel;
    if (r == SC_DRUM) *c = keep;
}
