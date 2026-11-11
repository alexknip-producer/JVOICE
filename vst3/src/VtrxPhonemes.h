/* SPDX-License-Identifier: GPL-3.0-only
 * SC-02 symbols mapped to the closest existing JVoice phonemes. */
typedef struct { const char *name; uint8_t phon; } jv_vtrx_token;
static const jv_vtrx_token JV_VTRX_TOKENS[]={
    {"pa",PH_SIL},
    {"e",PH_I},{"e1",PH_EH},{"e2",PH_IH},{"y",PH_IH},{"y1",PH_Y},
    {"ay",PH_I},{"ie",PH_IH},{"i",PH_IH},{"a",PH_E},{"ai",PH_EH},
    {"eh",PH_EH},{"eh1",PH_EH},{"ae",PH_AE},{"ae1",PH_AE},
    {"ah",PH_AA},{"ah1",PH_AA},{"aw",PH_AO},{"o",PH_O},{"ou",PH_O},
    {"oo",PH_UH},{"iu",PH_UJ},{"iu1",PH_UH},{"u",PH_U},{"u1",PH_U},
    {"uh",PH_AX},{"uh1",PH_AH},{"uh2",PH_AX},{"uh3",PH_AX},{"er",PH_ER},
    {"r1",PH_RA},{"r2",PH_RT},{"l",PH_L},{"l1",PH_L},{"lf",PH_L},
    {"w",PH_W},{"b",PH_B},{"d",PH_D},{"kv",PH_G},{"p",PH_P},{"t",PH_T},{"k",PH_K},
    {"hf",PH_H},{"z",PH_Z},{"s",PH_S},{"j",PH_ZH},{"sch",PH_SH},{"v",PH_V},
    {"f",PH_F},{"thv",PH_DH},{"th",PH_TH},{"m",PH_M},{"n",PH_N},{"ng",PH_NG},
    {":a",PH_EH},{":oh",PH_ER},{":u",PH_UJ},{":uh",PH_UJ},{"lb",PH_L},
    /* JVoice extensions for sounds outside the SC-02 English inventory. */
    {"rr",PH_RR},{"ch",PH_CH},{"ts",PH_TS},{"x",PH_X},{"zh",PH_ZH},{"sh",PH_SH},
    {"yi",PH_UH},{"ly",PH_RU_L},{"ry",PH_RU_R},{"ny",PH_RU_N},{"sy",PH_RU_S},
    {"zy",PH_RU_Z},{"ty",PH_RU_T},{"dy",PH_RU_D},{"my",PH_RU_M},{"by",PH_RU_B},
    {"py",PH_RU_P},{"vy",PH_RU_V},{"fy",PH_RU_F},{"gy",PH_RU_G},{"ky",PH_RU_K},
    {"xy",PH_RU_X},{"shch",PH_RU_SHCH}
};
static uint32_t jv_vtrx_word(uint32_t lang,const char *w,uint32_t n,uint8_t *out,uint32_t *ns) {
    uint32_t pos=0,count=0;*ns=0;
    while(pos<n){
        uint32_t start=pos;while(pos<n && w[pos]!='-' && w[pos]!='_')++pos;
        uint32_t len=pos-start;if(!len || count>=10)return 0;
        int phon=-1;
        if(len==1 && w[start]=='r')phon=lang==LG_RU ? PH_RR:lang==LG_DE ? PH_RT:PH_RA;
        for(uint32_t j=0;phon<0 && j<NELEM(JV_VTRX_TOKENS);++j)
            if(strlen(JV_VTRX_TOKENS[j].name)==len && !memcmp(w+start,JV_VTRX_TOKENS[j].name,len))phon=JV_VTRX_TOKENS[j].phon;
        if(phon<0)return 0;
        out[count]=(uint8_t)(phon | (!count ? SYB:0));++count;
        if(pos<n && ++pos==n)return 0;
    }
    if(count)*ns=1;
    return count;
}
