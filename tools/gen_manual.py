#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(JIANT 0.9.12) The interactive manual's generated half (web/manual): the navigation chains, read from the firmware
through tools/gen_nav_map.py's own data model (parse(), its rules / layers / combos in English and Spanish, COND), and
the firmware's real screens (tests/ui_render.c's renders) that the manual shows.

  python3 tools/gen_manual.py [RENDERS]   web/manual/nav.js, and web/manual/shots/*.png copied from RENDERS
                                          (default build/ui_new/AIR_JIANT: run tests/run_tests.sh, or ui_render alone)
  JIANT_FW=TREE python3 tools/gen_manual.py   (V10) the chains and the renders of the firmware in TREE (the V line lives
                                          on codex/add15-private: JIANT_FW=<its checkout>, RENDERS from that tree)
  python3 tools/gen_manual.py --check     exit 1 if nav.js differs from what the firmware's chains give, or a screen
                                          the manual names (nav.js or web/manual/data.js) is missing in shots/

The screens are the AIR skin (the default of new settings) in the JIANT palette. Only the ones the manual names are
copied (the page each nav pill opens, PAGE_SHOT below, and every shot:'name' in data.js); stale ones are removed.
"""
import json
import os
import re
import shutil
import sys
from pathlib import Path

R = Path(__file__).resolve().parents[1]
FW = Path(os.environ.get('JIANT_FW', R)).resolve()       # (V10) the firmware tree the map is read from
sys.path.insert(0, str(FW / 'tools'))
import gen_nav_map as nav  # noqa: E402

OUT = R / 'web/manual'
SHOTS = OUT / 'shots'

# the screen each page of the map opens (tests/ui_render.c's scene names; the breadcrumb of each checked by hand)
PAGE_SHOT = {
    'HOME': {'ECO': 'home', 'MACROS': 'home_macros', 'BEING': 'home_being', 'ROUTES': 'home_routes',
             'GEN 1': 'gen_explore', 'GEN 2': 'gen_parts', 'GEN 3': 'gen_lead',
             'GENE 1': 'gen_explore', 'GENE 2': 'gen_parts', 'GENE 3': 'gen_lead'},   # (V10.6n: GEN is GENE)
    'CE_SYN': {'EDIT 1': 'edit_analog', 'EDIT 2': 'v6_analog_sqr', 'FILTER': 'ed_filter', 'MORPH': 'morph',
               'VOICE': 'voice', 'VOICE 2': 'ed_voice2', 'WORDS': 'words', 'FOCUS': 'ed_focus',
               'RETRIG': 'pg_retrig', 'ROTOR': 'pg_rotor'},
    'CE_FM6': {'EDIT 1': 'edit_fm6', 'EDIT 2': 'fm6_alg_22', 'FILTER': 'ed_filter', 'VOICE': 'voice',
               'VOICE 2': 'ed_voice2', 'OPERATOR': 'ed_operator', 'OP ENV': 'v6_fm6_env', 'OPERATOR 2': 'v6_fm6_op2'},
    'CE_DRM': {'KIT': 'drumx', 'SOUND': 'dx_sound', 'SOUND 2': 'dx_sound2', 'SOUND 3': 'vx_kit_ghost',
               'SOUND 4': 'dx_send', 'FILTER': 'ed_filter', 'STEREO': 'dx_image', 'VOICE': 'voice',
               'KIT 2': 'pg_kit2', 'KIT 3': 'pg_kit3', 'KIT 4': 'pg_kit4', 'LEVELS': 'pg_levels', 'LEVELS 2': 'pg_levels2',
               'X-MOD': 'pg_xmod'},
    'SEQ': {'PATTERN': 'lente_pattern', 'STRETCH': 'lente_stretch', 'PLAYHEADS': 'fugue', 'TRACK': 'shift',
            'EUCLID': 'euclid', 'LENGTH': 'pg_length', 'MODES': 'euclid_modes', 'STEP': 'lente_bar', 'NOTE': 'lente_step_note',
            'COMP': 'lente_step_comp', 'COMP 2': 'lente_step_comp2', 'AUTOMATION': 'motion', 'AUTO LIST': 'auto_list'},
    'FX': {'SENDS': 'fx_track_sends', 'DIST': 'fx_track_dist', 'AUTO PAN': 'fx_track_pan', 'BUSES': 'fx_buses',
           'DLY': 'fx_effect_delay', 'DLY 2': 'fx_effect_delay2', 'REVERB': 'fx_effect_reverb', 'REVERB 2': 'reverb2',
           'CHORUS': 'chorus', 'SLICER': 'fx_effect_slicer', 'SL BUS 2': 'slbus', 'SL PITCH': 'slpitch',
           'MASTER': 'fx_effect_master', 'PUNCH': 'toast_punch',
           'DLY 3': 'pg_dly3', 'SL BUS': 'pg_slbus', 'TAPE': 'pg_tape', 'EQ': 'pg_eq'},
    'GLO': {'MANO': 'live_mano', 'RIELES': 'live_rieles', 'MIXER': 'mixer', 'SENDS': 'glo_sends',
            'GLOBAL': 'global', 'SYSTEM': 'system',
            'SYSTEM 2': 'pg_system2', 'COMPS': 'pg_comps', 'COMPS 2': 'pg_comps2'},
    'ENV': {'TRACKS': 'env_tracks', 'TRACKS DEST': 'env_tracks_dest', 'ENV': 'env', 'ENV DEST': 'env_dest',
            'STAGE': 'env_stage'},
    'LFO': {'MOD': 'mod', 'LFO': 'lfo', 'LFO 2': 'lfo2', 'LFO DEST': 'lfo_dest', 'MSEQ': 'mseq', 'MSEQ 2': 'mseq2'},
    'SCL': {'SCL': 'scl', 'CHORD': 'key_chord'},
    'ARP': {'ARP': 'key_arp', 'ARP 2': 'key_arp2'},
    'SAVE': {'USER': 'save_sound', 'PRESETS': 'save_presets', 'SECTIONS': 'save_project', 'ARRANGE': 'save_arrange',
             'TIMELINE': 'save_timeline', 'PROJECTS': 'save_library',
             'TOOLS': 'save_tools'},
}
CHAIN_LABEL = {'CE_SYN': ('SINTES', 'SYNTHS'), 'CE_FM6': ('FM6', 'FM6'), 'CE_DRM': ('DRUM-X', 'DRUM-X')}


def build():
    sec, keys = nav.parse()
    chains = []
    for k in keys:
        shots = PAGE_SHOT.get(k['ce'], PAGE_SHOT.get(k['sec'], {}))
        lab = CHAIN_LABEL.get(k['ce'])
        chains.append(dict(
            sec=k['sec'], ce=k['ce'], label=dict(es=lab[0], en=lab[1]) if lab else None,
            groups=[g['nm'] for g in k['groups']],
            pages=[dict(name=p['name'], g=p['g'], on=p['i'] == k['default'],
                        cond=(k['sec'], p['name']) in nav.COND, shot=shots.get(p['name'])) for p in k['pages']]))
    lang = {}
    for code in ('es', 'en'):
        L = nav.L[code]
        lang[code] = dict(rules=[dict(k=a, t=b, d=c) for a, b, c in L['rules']],
                          layers=[dict(b=b, d=d.replace('**', '')) for b, d in L['layers']],
                          combos=[dict(k=k, d=d) for k, d in L['combos']],
                          hints={s: dict(hold=h[0], sel=h[1]) for s, h in L['hints'].items()},
                          notes=L['notes'])
    data = dict(version=nav.VERSION, sections=sec, chains=chains, lang=lang)
    return ('// SPDX-License-Identifier: GPL-3.0-only\n'
            '// GENERATED by tools/gen_manual.py from firmware/src/ui_lens.c (via tools/gen_nav_map.py): do not edit\n'
            'window.JNAV = ' + json.dumps(data, ensure_ascii=False, separators=(',', ':')) + ';\n')


def wanted(navjs):
    names = set(re.findall(r'"shot":"([\w-]+)"', navjs))
    data = (OUT / 'data.js').read_text(encoding='utf-8')
    names |= set(re.findall(r"\bshot:\s*'([\w-]+)'", data))
    for m in re.findall(r"\bshots:\s*\[([^\]]*)\]", data):
        names |= set(re.findall(r"'([\w-]+)'", m))
    return names


def main(argv):
    navjs = build()
    if '--check' in argv:
        bad = []
        have = (OUT / 'nav.js').read_text(encoding='utf-8')
        hv = re.search(r'"version":"([^"]+)"', have)
        if hv and hv.group(1) != nav.VERSION:          # (V10) made from another tree (JIANT_FW): its screens still checked
            print(f'gen_manual: nav.js is {hv.group(1)}\'s map, this tree is {nav.VERSION}: chains not compared (JIANT_FW)')
            navjs = have
        elif have != navjs:
            bad.append('web/manual/nav.js differs from the firmware chains: run tools/gen_manual.py')
        bad += [f'web/manual/shots/{n}.png missing' for n in sorted(wanted(navjs)) if not (SHOTS / f'{n}.png').exists()]
        for b in bad:
            print('gen_manual:', b)
        sys.exit(1 if bad else 0)
    src = Path(argv[0]) if argv else R / 'build/ui_new/AIR_JIANT'
    (OUT / 'nav.js').write_text(navjs, encoding='utf-8')
    names = wanted(navjs)
    SHOTS.mkdir(exist_ok=True)
    for f in SHOTS.glob('*.png'):
        if f.stem not in names:
            f.unlink()
    missing = []
    for n in sorted(names):
        s = src / f'{n}.png'
        if s.exists():
            shutil.copy(s, SHOTS / s.name)
        else:
            missing.append(n)
    print(f'gen_manual: nav.js ({len(navjs)} B), {len(names) - len(missing)} screens from {src}')
    if missing:
        sys.exit('gen_manual: no render for ' + ' '.join(missing))


if __name__ == '__main__':
    main(sys.argv[1:])
