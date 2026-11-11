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
    jv_destroy(e);puts("PASS: RU y/e/eh vowel parsing, held articulation targets, preserved glides and other languages");
}
