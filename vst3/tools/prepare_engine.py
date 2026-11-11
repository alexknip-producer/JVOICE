#!/usr/bin/env python3
"""Adapt the pinned firmware sources without changing hardware firmware.

Mutable firmware globals become thread-local pointers to per-plugin storage.
The oscillator and language algorithms are copied verbatim at build time.
"""
from pathlib import Path
import csv
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for name in ('tables', 'lyrics'):
    subprocess.run([sys.executable, str(root / f'tools/gen_{name}.py'),
                    str(out / f'felucca_{name}.h')], check=True)
core = (root / 'firmware/src/core.h').read_text()
core = core.replace('(r - 32u) * 192u / 95u','(r - 32u) * 320u / 95u') # desktop CONS up to 600%
core = re.sub(r'#define NPART 6\b', '#define NPART 9', core)
core = core.replace('static track_t trk[NTRK];', 'static JV_TLS track_t *trk;')
core = core.replace('static song_t song;', 'static JV_TLS song_t *song_ptr;\n#define song (*song_ptr)')
core = re.sub(r'#define RING_PUBLISH\(\).*', '#define RING_PUBLISH() ((void)0)', core)
sing = (root / 'firmware/src/sing.c').read_text()
vowels = list(csv.DictReader((root / 'vst3/src/RussianVowels.csv').open(encoding='utf-8')))
assert len({v['phoneme'] for v in vowels}) == len(vowels)
for v in vowels:
    assert re.fullmatch(r'PH_[A-Z]+', v['phoneme']) and re.fullmatch(r'[a-z]{1,4}', v['latin'])
    assert all(100 <= int(v[f'F{i}_Hz']) <= 7000 for i in range(1, 4)) and v['rounded'] in ('0', '1')
ext = (root / 'firmware/src/sing_ext.c').read_text()
rules = ''.join(f'{{"{v["latin"]}",LR,{v["phoneme"]},0}}, ' for v in sorted(vowels, key=lambda v: -len(v['latin'])))
assert ext.count('{"y",LR|LG,PH_UH,0},') == 1
ext = ext.replace('{"y",LR|LG,PH_UH,0},', rules + '{"y",LG,PH_UH,0},')
sing = sing.replace('#include "sing_ext.c"', ext)
# Language-specific vowel profiles: leave the firmware's shared phoneme table
# and other languages intact, including vowel transitions through consonants.
profile = 'static const struct { uint8_t code; phon_t target; } JV_RU_VOWELS[] = {\n'
for v in vowels:
    profile += f'    {{{v["phoneme"]}, {{{v["F1_Hz"]}, {v["F2_Hz"]}, {v["F3_Hz"]}, PC_VOW, 255, 0, 0, 0, 0, 16, {"PL_RND" if v["rounded"] == "1" else "PL_NONE"}}}}},\n'
profile += '''};
static const phon_t *jv_phon(uint32_t lang, uint32_t code) {
    if (lang == LG_RU) for (uint32_t j = 0; j < sizeof JV_RU_VOWELS / sizeof *JV_RU_VOWELS; ++j)
        if (JV_RU_VOWELS[j].code == code) return &JV_RU_VOWELS[j].target;
    return &PHON[code];
}
/* Shift relative to the current vowel along A-I-E-A-O-U. Multiplicative
 * interpolation retains the source vowel's individual colour at neutral. */
static void jv_smear(uint32_t lang, int32_t *f) {
    double offset=(jv_control_values[19]-.5)*6;
    if (fabs(offset)<1e-12) return;
    static const uint8_t ring[6]={PH_A,PH_I,PH_E,PH_A,PH_O,PH_U};
    int base=0;double best=1e30;
    for(int k=0;k<6;++k){
        const phon_t *p=jv_phon(lang,ring[k]);double a=(double)f[0]/p->f1-1,b=(double)f[1]/p->f2-1,c=(double)f[2]/p->f3-1;
        double score=a*a+b*b+c*c;if(score<best){best=score;base=k;}
    }
    double position=base+offset;position-=floor(position/6)*6;
    int first=(int)floor(position);double fraction=position-first;
    const phon_t *from=jv_phon(lang,ring[base]),*a=jv_phon(lang,ring[first]),*b=jv_phon(lang,ring[(first+1)%6]);
    int origin[3]={from->f1,from->f2,from->f3},left[3]={a->f1,a->f2,a->f3},right[3]={b->f1,b->f2,b->f3};
    for(int j=0;j<3;++j)f[j]=(int32_t)lround(f[j]*((left[j]+(right[j]-left[j])*fraction)/origin[j]));
}
'''
needle = 'static int ph_vowel(uint32_t c)'
assert sing.count(needle) == 1 and sing.count('P = &PHON[c];') == 2 and sing.count('Q = &PHON[nxt];') == 1
sing = sing.replace(needle, profile + needle).replace('P = &PHON[c];', 'P = jv_phon(lang, c);').replace('Q = &PHON[nxt];', 'Q = jv_phon(lang, nxt);')
needle = '    if (F) {'
assert sing.count(needle) == 1
sing = sing.replace(needle, '    if (k == PC_VOW) jv_smear(lang, tF);\n' + needle)
# 128 Unicode codepoints require at most 512 UTF-8 bytes, plus terminator.
sing = sing.replace('#define LYR_LEN 64u', '#define LYR_LEN 513u')
sing = sing.replace('uint8_t to, tl;', 'uint16_t to, tl;')
sing = sing.replace('s->to = (uint8_t)p;', 's->to = (uint16_t)p;')
sing = sing.replace('s->tl = (uint8_t)(q - p);', 's->tl = (uint16_t)(q - p);')
# Cache's token-end byte is hardware-specific; use the dictionary directly in
# the desktop port, so long transliterations cannot wrap offsets at 256 bytes.
sing = sing.replace('lyc_build(k);', '/* Desktop: resolve words at note boundaries. */')
sing = sing.replace("s[i] >= 32 && s[i] <= 126", "(unsigned char)s[i] >= 32")
fields = []
def replace_array(m):
    ctype, name, dims = m.group(1), m.group(2), m.group(3)
    fields.append((ctype, name, dims))
    # pointer to the remaining dimensions, or pointer to the scalar element
    rest = dims[dims.index(']')+1:]
    decl = f'{ctype} (*{name}){rest}' if rest else f'{ctype} *{name}'
    return f'static JV_TLS {decl};'
sing = re.sub(r'static (char|uint8_t|uint16_t|sing_t|sing_x_t|sing_io_t|lyc_t|sw_t) (\w+)((?:\[[^\]]+\])+)(?: __attribute__\(\(section\("[^"]+"\)\)\))?;', replace_array, sing)
sing = re.sub(r'static uint8_t sy_quick __attribute__\(\(section\("[^"]+"\)\)\);', 'static JV_TLS uint8_t sy_quick;', sing)
sing = sing.replace('static int16_t sy_cx;', 'static JV_TLS int16_t sy_cx;')
sing = sing.replace('static uint8_t lyb_ok __attribute__((section(".pool")));',
                    'static JV_TLS uint8_t *lyb_ok_ptr;\n#define lyb_ok (*lyb_ok_ptr)')
formant = (root / 'firmware/src/eng_formant.c').read_text().replace('#include "sing.c"', sing)
formant = re.sub(r'static (int32_t) (fmt_f5)((?:\[[^\]]+\])+)(?: __attribute__\(\(section\("[^"]+"\)\)\))?;', replace_array, formant)
formant = formant.replace('static int32_t formant_nz = 0x2545F491;',
    'static JV_TLS int32_t *formant_noise;\n#define formant_nz (*formant_noise)\n')
# Replace only the periodic excitation; aspiration and frication remain upstream's.
assert formant.count('x = (lp * gv) >> 15;') == 2
formant = formant.replace('x = (lp * gv) >> 15;', 'x = jv_carrier ? jv_excitation(jv_carrier[i], gv, jv_carrier_gain[i]) : (lp * gv) >> 15;')
prefix = '''/* Generated adaptation; upstream copyright and GPL notices follow. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#ifdef _MSC_VER
#define __builtin_offsetof offsetof
#endif
static void str_cpy(char *d, const char *s, unsigned n) {
    if (!n) return;
    unsigned i = 0;
    while (s[i] && i + 1 < n) { d[i] = s[i]; ++i; }
    d[i] = 0;
}
#ifdef _MSC_VER
#define JV_TLS __declspec(thread)
#define __attribute__(x)
#else
#define JV_TLS _Thread_local
#endif
static JV_TLS const float *jv_carrier;
static JV_TLS const float *jv_carrier_gain;
static JV_TLS const double *jv_control_values;
/* +12 dB on external tonal excitation only. Keep headroom in the conversion:
 * a normal full-scale input must not clip at the old int16 carrier boundary. */
static inline int32_t jv_excitation(float sample, int32_t voicing, float gain) {
    if (!isfinite(sample)) sample = 0;
    sample = fmaxf(-4.0f, fminf(4.0f, sample));
    return (int32_t)(((int64_t)(sample * (12000.0f * 3.981071706f) * gain) * voicing) >> 15);
}
#include "felucca_tables.h"
'''
context = '\nstruct jv_engine {\n    track_t tracks[NTRK];\n    song_t song_state;\n'
for ctype, name, dims in fields:
    context += f'    {ctype} state_{name}{dims};\n'
context += '''    uint8_t cache_ready;
    int32_t noise;
    uint32_t age;
    int ids[NTRK];
    double controls[JV_CONTROL_COUNT];
    double bend[16], bend_target[16];
    double mod_wheel[16], mod_target[16], mod_phase;
    uint32_t noise_seed;
    voice_t right_voice[NTRK];
    int32_t right_f5[NTRK][2];
    float mix_gain;
    float external_gain;
    float detune_position[NTRK], pan_position[NTRK], pan_current[NTRK];
};
static void jv_bind(struct jv_engine *e) {
    jv_control_values = e->controls;
    trk = e->tracks;
    song_ptr = &e->song_state;
    lyb_ok_ptr = &e->cache_ready;
    formant_noise = &e->noise;
'''
for _, name, _ in fields:
    context += f'    {name} = e->state_{name};\n'
context += '}\n'
(out / 'engine_generated.inc').write_text(prefix + core + '\n' +
    (root / 'firmware/src/dsp.c').read_text() + '\n' + formant + context)
