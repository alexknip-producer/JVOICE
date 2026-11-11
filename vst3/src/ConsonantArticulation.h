/* SPDX-License-Identifier: GPL-3.0-only */
/* This flag describes only an explicitly isolated consonant unit. Normal
 * word syllables follow the original VOICE articulation state machine. */
static int jv_vowelless(const sing_t *s) { return s->jv_consonant_unit; }
static int jv_timed_nucleus(const sing_t *s,uint32_t cls) {
    return jv_vowelless(s) && (cls==PC_PLO || cls==PC_AFF);
}
static uint32_t jv_fric(uint32_t lang,uint32_t c) {
    return lang==LG_RU && c==PH_CH ? PH_RU_SHCH:ph_fric(c);
}
