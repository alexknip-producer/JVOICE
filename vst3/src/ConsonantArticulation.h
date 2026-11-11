/* SPDX-License-Identifier: GPL-3.0-only */
static int jv_vowelless(const sing_t *s) {
    for(uint32_t i=0;i<s->np;++i)if(ph_vowel(s->ph[i]))return 0;
    return s->np!=0;
}
static int jv_timed_nucleus(const sing_t *s,uint32_t cls) {
    return jv_vowelless(s) && (cls==PC_PLO || cls==PC_AFF);
}
static uint32_t jv_fric(uint32_t lang,uint32_t c) {
    return lang==LG_RU && c==PH_CH ? PH_RU_SHCH:ph_fric(c);
}
