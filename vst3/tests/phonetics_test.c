/* SPDX-License-Identifier: GPL-3.0-only */
#include "../src/VoiceEngine.c"
#include <assert.h>
#include <stdio.h>
static void held_vowel(jv_engine *e,const char *word,int *f) {
    jv_reset(e);jv_text(e,word,LG_RU);jv_note_on(e,0,60,100,0,1);
    float out[JV_BLOCK];for(int k=0;k<200;++k)jv_render(e,out);
    const sing_t *s=sing_view(&trk[1]);assert(s->live && ph_vowel(sing_phon(s)));
    for(int j=0;j<3;++j)f[j]=s->f[j];
}
static void bare_sound(jv_engine *e,int lang,const char *word,int burst) {
    jv_reset(e);jv_control(e,4,1);jv_control(e,3,0);jv_text(e,word,lang);jv_note_on(e,0,60,100,0,1);
    float out[JV_BLOCK];double early=0,late=0;
    for(int k=0;k<500;++k){jv_render(e,out);for(int j=0;j<JV_BLOCK;++j){assert(isfinite(out[j]));if(k<100)early+=out[j]*out[j];if(k>=400)late+=out[j]*out[j];}}
    if(!(early>1e-7 && (burst ? late<early*.02:late>1e-7)))fprintf(stderr,"bare %s lang%d: early%g late%g\n",word,lang,early,late);
    assert(early>1e-7);assert(burst ? late<early*.02:late>1e-7);
}
int main(void) {
    uint8_t codes[SW_MAX],kinds[SW_MAX];
    assert(sy_ext(LG_RU,"y",1,codes,kinds)==1 && codes[0]==PH_UH && kinds[0]!=TK_C);
    assert(sy_ext(LG_RU,"e",1,codes,kinds)==1 && codes[0]==PH_E);
    assert(sy_ext(LG_RU,"eh",2,codes,kinds)==1 && codes[0]==PH_EH);
    assert(sy_ext(LG_RU,"ye",2,codes,kinds)==2 && codes[0]==PH_Y && codes[1]==PH_E);
    assert(sy_ext(LG_RU,"vecher",6,codes,kinds)==5 && codes[1]==PH_E && codes[3]==PH_E);
    assert(sy_ext(LG_GN,"y",1,codes,kinds)==1 && codes[0]==PH_UH);
    for(int lang=LG_ES;lang<LG_N;++lang)if(lang!=LG_RU)
        for(int c=0;c<PH_COUNT;++c)assert(jv_phon(lang,c)==&PHON[c]);
    jv_engine *e=jv_create();assert(e);int i[3],y[3],eh[3],ee[3];
    held_vowel(e,"i",i);held_vowel(e,"y",y);held_vowel(e,"eh",eh);held_vowel(e,"e",ee);
    const phon_t *iy=jv_phon(LG_RU,PH_UH),*ev=jv_phon(LG_RU,PH_E);
    int yTargets[]={iy->f1,iy->f2,iy->f3},eTargets[]={ev->f1,ev->f2,ev->f3};
    for(int j=0;j<3;++j){assert(y[j]<i[j] && ee[j]>eh[j]);assert(abs(y[j]-yTargets[j])<yTargets[j]/20);assert(abs(ee[j]-eTargets[j])<eTargets[j]/20);}
    // Smear changes an already held U into A; returning to centre restores U.
    int u[3];held_vowel(e,"u",u);jv_control(e,19,2.0/3);
    float out[JV_BLOCK];for(int k=0;k<200;++k)jv_render(e,out);
    const sing_t *s=sing_view(&trk[1]);const phon_t *a=jv_phon(LG_RU,PH_A);int aTargets[]={a->f1,a->f2,a->f3};
    for(int j=0;j<3;++j)assert(abs(s->f[j]-aTargets[j])<aTargets[j]/20);
    jv_control(e,19,.5);for(int k=0;k<200;++k)jv_render(e,out);
    for(int j=0;j<3;++j)assert(s->f[j]==u[j]);
    assert(trk[1].v[0].note==60);
    // Slower Phoneme Glide moves less far in the first few blocks.
    held_vowel(e,"u",u);jv_control(e,19,2.0/3);for(int k=0;k<5;++k)jv_render(e,out);int normal=sing_view(&trk[1])->f[0];
    held_vowel(e,"u",u);jv_control(e,21,1);jv_control(e,19,2.0/3);for(int k=0;k<5;++k)jv_render(e,out);
    assert(sing_view(&trk[1])->f[0]<normal && sing_view(&trk[1])->f[0]>=u[0]);
    // A few Russian contrasts, with voicing deliberately preserved.
    uint32_t count=jv_ru_tokens("ugol'",5,codes,kinds);assert(count==4 && codes[3]==PH_RU_L);
    count=jv_ru_tokens("ugol",4,codes,kinds);assert(count==4 && codes[3]==PH_L);
    count=jv_ru_tokens("lya",3,codes,kinds);assert(count==2 && codes[0]==PH_RU_L && codes[1]==PH_A);
    count=jv_ru_tokens("l'ya",4,codes,kinds);assert(count==3 && codes[0]==PH_RU_L && codes[1]==PH_Y);
    count=jv_ru_tokens("s''el",5,codes,kinds);assert(count==4 && codes[0]==PH_S && codes[1]==PH_Y);
    count=jv_ru_tokens("shch",4,codes,kinds);assert(count==1 && codes[0]==PH_RU_SHCH);
    count=jv_ru_tokens("sad",3,codes,kinds);assert(codes[count-1]==PH_D);
    count=jv_ru_tokens("raz",3,codes,kinds);assert(codes[count-1]==PH_Z);
    assert(PHON[PH_RU_L].f2>PHON[PH_L].f2 && PHON[PH_RU_S].ff>PHON[PH_S].ff);
    for(int lang=LG_ES;lang<LG_N;++lang){bare_sound(e,lang,"s",0);bare_sound(e,lang,"g",1);}
    bare_sound(e,LG_RU,"sh",0);bare_sound(e,LG_RU,"m",0);bare_sound(e,LG_RU,"l'",0);
    // Russian R and R' produce ongoing repeated closures rather than one tap.
    for(int soft=0;soft<2;++soft){jv_reset(e);jv_text(e,soft?"r'":"r",LG_RU);jv_note_on(e,0,60,100,0,1);int low=32767,high=0;
        for(int k=0;k<160;++k){jv_render(e,out);int gain=sing_view(&trk[1])->gv;if(gain<low)low=gain;if(gain>high)high=gain;}assert(high>20000 && low<high/2);}
    // Newspeak: exactly four notes for m-a-m-a, in every language mode.
    for(int lang=LG_ES;lang<LG_N;++lang){
        jv_reset(e);uint32_t ns=0;count=sy_word(lang,"mama",4,codes,&ns);assert(count==4 && ns==2);
        jv_control(e,20,1);count=sy_word(lang,"mama",4,codes,&ns);assert(count==4 && ns==4);
        jv_text(e,"mama",lang);for(int note=0;note<4;++note){jv_note_on(e,0,60,100,0,note);assert(jv_position(e)==(unsigned)note+1);assert(sing_view(&trk[note+1])->np==1);}
    }
    jv_destroy(e);puts("PASS: vowel/Smear targets, RU soft consonants and trills, isolated S/G and Newspeak in all nine languages");
}
