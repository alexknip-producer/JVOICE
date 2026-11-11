#!/usr/bin/env python3
"""Adapt the pinned firmware sources without changing hardware firmware.

Mutable firmware globals become thread-local pointers to per-plugin storage.
The oscillator and language algorithms are copied verbatim at build time.
"""
from pathlib import Path
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
core = re.sub(r'#define NPART 6\b', '#define NPART 9', core)
core = core.replace('static track_t trk[NTRK];', 'static JV_TLS track_t *trk;')
core = core.replace('static song_t song;', 'static JV_TLS song_t *song_ptr;\n#define song (*song_ptr)')
core = re.sub(r'#define RING_PUBLISH\(\).*', '#define RING_PUBLISH() ((void)0)', core)
sing = (root / 'firmware/src/sing.c').read_text()
sing = sing.replace('#include "sing_ext.c"', (root / 'firmware/src/sing_ext.c').read_text())
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
prefix = '''/* Generated adaptation; upstream copyright and GPL notices follow. */
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stddef.h>
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
#include "felucca_tables.h"
'''
context = '\nstruct jv_engine {\n    track_t tracks[NTRK];\n    song_t song_state;\n'
for ctype, name, dims in fields:
    context += f'    {ctype} state_{name}{dims};\n'
context += '''    uint8_t cache_ready;
    int32_t noise;
    uint32_t age;
    int ids[NTRK];
    double controls[10];
    float detune_position[NTRK], pan_position[NTRK], gain[NTRK], pan_current[NTRK];
};
static void jv_bind(struct jv_engine *e) {
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
