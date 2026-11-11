/* SPDX-License-Identifier: GPL-3.0-only
 * Included by the generated engine after the pinned language readers. */
static uint32_t jv_ru_tokens(const char *w,uint32_t n,uint8_t *tc,uint8_t *tk) {
    static const uint8_t letters[26]={PH_A,PH_B,PH_K,PH_D,PH_E,PH_F,PH_G,PH_H,PH_I,PH_Y,
        PH_K,PH_L,PH_M,PH_N,PH_O,PH_P,PH_K,PH_RR,PH_S,PH_T,PH_U,PH_V,PH_W,PH_X,PH_UH,PH_Z};
    uint32_t m=0;int separated=0;
    for(uint32_t i=0;i<n && m+2<=SW_MAX;){
        uint32_t c=0,used=1;int iotated=0,soften=0;
        if(w[i]=='\''){
            if(i+1<n && w[i+1]=='\'')i+=2;
            else {if(m)tc[m-1]=(uint8_t)jv_ru_soft(tc[m-1]);++i;}
            separated=1;continue;
        }
        if(i+4<=n && !memcmp(w+i,"shch",4)){c=PH_RU_SHCH;used=4;}
        else if(i+2<=n && !memcmp(w+i,"sh",2)){c=PH_SH;used=2;}
        else if(i+2<=n && !memcmp(w+i,"zh",2)){c=PH_ZH;used=2;}
        else if(i+2<=n && !memcmp(w+i,"kh",2)){c=PH_X;used=2;}
        else if(i+2<=n && !memcmp(w+i,"ch",2)){c=PH_CH;used=2;}
        else if(i+2<=n && !memcmp(w+i,"ts",2)){c=PH_TS;used=2;}
        else if(i+2<=n && !memcmp(w+i,"eh",2)){c=PH_EH;used=2;}
        else if(w[i]=='y' && i+1<n && strchr("aeou",w[i+1])){
            c=w[i+1]=='a'?PH_A:w[i+1]=='o'?PH_O:w[i+1]=='u'?PH_U:PH_E;used=2;iotated=soften=1;
        }else if(w[i]>='a' && w[i]<='z'){
            c=letters[w[i]-'a'];iotated=w[i]=='e';soften=iotated || w[i]=='i';
        }
        i+=used;if(!c)continue;
        if(ph_vowel(c) && soften){
            int previousConsonant=m && !ph_vowel(tc[m-1]) && tc[m-1]!=PH_Y;
            if(previousConsonant && !separated){
                tc[m-1]=(uint8_t)jv_ru_soft(tc[m-1]);
                if(c==PH_A && iotated && jv_phon(LG_RU,tc[m-1])->place==PL_PAL)c=PH_AE;
            }
            else if(iotated && (!m || separated || tc[m-1]!=PH_Y)){tc[m]=PH_Y;tk[m++]=TK_C;}
        }
        tc[m]=(uint8_t)c;tk[m++]=ph_vowel(c)?TK_S:TK_C;separated=0;
    }
    return m;
}

/* Consonant-only input is an articulation token, before dictionary letter names
 * or a reader's automatic vowel. Full words still use their original readers. */
static uint32_t jv_bare_tokens(uint32_t lang,const char *w,uint32_t n,uint8_t *tc,uint8_t *tk) {
    uint32_t letters=0;
    for(uint32_t i=0;i<n;++i){if(strchr("aeiouy",w[i]))return 0;if(w[i]>='a'&&w[i]<='z')++letters;}
    if(!letters)return 0;
    uint32_t code=0;
    if(n==2 && !memcmp(w,"sh",2))code=PH_SH;
    if(n==2 && !memcmp(w,"zh",2))code=PH_ZH;
    if(n==2 && !memcmp(w,"kh",2))code=PH_X;
    if(n==2 && !memcmp(w,"ng",2))code=PH_NG;
    if(n==2 && !memcmp(w,"ts",2))code=PH_TS;
    if(lang==LG_EN && n==2 && !memcmp(w,"th",2))code=PH_TH;
    if(lang==LG_EN && n==2 && !memcmp(w,"dh",2))code=PH_DH;
    if(code){tc[0]=(uint8_t)code;tk[0]=TK_C;return 1;}
    uint32_t m=0;
    if(lang==LG_JA){
        static const uint8_t consonants[26]={0,PH_B,PH_K,PH_D,0,PH_F,PH_G,PH_H,0,PH_J,PH_K,PH_RT,PH_M,PH_NM,0,PH_P,PH_K,PH_RT,PH_S,PH_T,0,PH_V,PH_W,PH_X,0,PH_Z};
        if(n==2 && !memcmp(w,"ch",2)){tc[0]=PH_CH;tk[0]=TK_C;return 1;}
        for(uint32_t i=0;i<n && m<SW_MAX;++i)if(w[i]>='a'&&w[i]<='z' && consonants[w[i]-'a'])tc[m++]=consonants[w[i]-'a'];
    }else if(lang==LG_EN)m=sy_en_rules(w,n,tc,tk);
    else if(lang>=LG_RU)m=sy_ext(lang,w,n,tc,tk);
    else m=sy_es_tok(w,n,tc,tk);
    if(!m && n==1 && w[0]=='h'){tc[0]=PH_H;m=1;}
    for(uint32_t i=0;i<m;++i)tk[i]=TK_C;
    return m;
}
