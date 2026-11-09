# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.6.7) the two cheat sheets of the README: docs/cheatsheet/controls.html and sound.html (the JIANT look: black,
# Chakra Petch, the thermal palette, the signature), then tools/render_cheatsheets.mjs makes the PNGs and PDFs:
#   python3 tools/gen_cheatsheets.py && node tools/render_cheatsheets.mjs
# The content follows firmware/src/params.c (pages, labels) and the README's manual; update both together. The V line's
# firmware is the commit web/STABLE names on main (firmware/src/ui_lens.c, params.c); the firmware branch's own tree.
import html, os

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(R, 'docs', 'cheatsheet')
VERSION = 'V10.7.6'   # (the release; a rename is this one line)

HEAT = ['#22d3ee', '#3b82f6', '#8b5cf6', '#d946ef', '#ef4444', '#ff7a00', '#facc15', '#f4f1ea']

CSS = """
@font-face { font-family: Chakra; src: url('../../assets/fonts/ChakraPetch-Medium.ttf'); font-weight: 500; }
@font-face { font-family: Chakra; src: url('../../assets/fonts/ChakraPetch-SemiBold.ttf'); font-weight: 600; }
@font-face { font-family: Chakra; src: url('../../assets/fonts/ChakraPetch-Bold.ttf'); font-weight: 700; }
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { background: #000; }
body { width: 1600px; color: #f4f1ea; font-family: Chakra, sans-serif; font-weight: 500; font-size: 17px;
       line-height: 1.32; padding: 36px 56px 40px; }
header { display: flex; align-items: center; gap: 36px; }
header img { height: 150px; }
header h1 { font-size: 64px; font-weight: 700; letter-spacing: 1px; line-height: 1; }
header h2 { font-size: 26px; font-weight: 500; color: #b8b2c8; margin-top: 8px; }
header .ver { font-size: 18px; color: #7d7690; margin-top: 6px; }
.rule { height: 4px; margin: 22px 0 26px; border-radius: 2px;
        background: linear-gradient(90deg, #22d3ee, #3b82f6, #8b5cf6, #d946ef, #ef4444, #ff7a00, #facc15, #f4f1ea); }
.grid { display: grid; gap: 22px; }
.g2 { grid-template-columns: 1fr 1fr; } .g3 { grid-template-columns: 1fr 1fr 1fr; } .g4 { grid-template-columns: repeat(4, 1fr); }
.card { background: #100c16; border-radius: 12px; padding: 18px 20px 18px 24px; position: relative; }
.card::before { content: ''; position: absolute; left: 0; top: 14px; bottom: 14px; width: 5px; border-radius: 3px;
                background: var(--c); }
.card h3 { font-size: 23px; font-weight: 700; letter-spacing: .5px; color: var(--c); margin-bottom: 10px; }
.card h4 { font-size: 17px; font-weight: 600; color: #f4f1ea; margin: 12px 0 4px; letter-spacing: .4px; }
.sec { font-size: 30px; font-weight: 700; letter-spacing: 1px; margin: 30px 0 14px; display: flex; align-items: center; gap: 14px; }
.sec span { flex: 1; height: 2px; background: #2a2236; }
.k { display: inline-block; background: #2b2633; color: #f4f1ea; border-radius: 6px; padding: 0 7px; margin: 1px 1px;
     font-weight: 600; font-size: .92em; white-space: nowrap; }
.k.a { background: #ff7a00; color: #120a00; } .k.v { background: #b870ff; color: #12081c; }
.dim { color: #8f88a3; }
p + p { margin-top: 6px; }
table { width: 100%; border-collapse: collapse; }
td, th { text-align: left; vertical-align: top; padding: 5px 8px 5px 0; border-bottom: 1px solid #241d2e; }
th { color: #8f88a3; font-weight: 600; font-size: 15px; letter-spacing: .5px; }
tr:last-child td { border-bottom: 0; }
td.n { color: var(--c); font-weight: 700; white-space: nowrap; width: 1%; padding-right: 14px; }
.knobs { display: grid; grid-template-columns: repeat(4, 1fr); gap: 4px; margin-bottom: 4px; }
.col { display: flex; flex-direction: column; gap: 22px; }
.knobs span { background: #1c1624; border-radius: 6px; text-align: center; padding: 2px 0; font-weight: 600; font-size: 15px; }
.knobs span.e { color: #4a4258; }
.pages td.p { white-space: nowrap; width: 1%; padding-right: 12px; font-weight: 600; }
.kb { position: relative; height: 150px; margin: 8px 0 4px; }
.kb .w { position: absolute; bottom: 0; height: 92px; border-radius: 6px; background: #1f1a27; border: 1px solid #2f2839;
         font-size: 12.5px; font-weight: 600; text-align: center; padding: 4px 2px 0; line-height: 1.1; color: #d8d2e4; }
.kb .w b, .kb .b b { display: block; font-size: 11px; color: #6f6882; font-weight: 600; margin-bottom: 2px; }
.kb .b { position: absolute; top: 0; height: 54px; border-radius: 6px; background: #2b2633; font-size: 12px; font-weight: 600;
         text-align: center; padding: 3px 1px 0; line-height: 1.05; color: #f4f1ea; }
.kb .on { background: var(--c); color: #0b0710; border-color: transparent; }
.kb .on b { color: rgba(0, 0, 0, .55); }
footer { margin-top: 30px; color: #6f6882; font-size: 16px; display: flex; justify-content: space-between; }
ul { padding-left: 18px; } li { margin: 2px 0; }
"""

WHITE = ['F3', 'G3', 'A3', 'B3', 'C4', 'D4', 'E4', 'F4', 'G4', 'A4', 'B4', 'C5', 'D5', 'E5', 'F5', 'G5']
BLACK = [('F#3', 0), ('G#3', 1), ('A#3', 2), ('C#4', 4), ('D#4', 5), ('F#4', 7), ('G#4', 8), ('A#4', 9), ('C#5', 11),
         ('D#5', 12), ('F#5', 14)]   # (name, the white key it follows)


def e(s):
    return html.escape(s)


def k(*names, cls=''):
    return ''.join(f'<span class="k {cls}">{e(n)}</span>' for n in names)


def keyboard(whites, blacks, width=692):
    """whites / blacks: dicts note -> label (only those set are lit)"""
    ww = width / 16.0
    out = ['<div class="kb">']
    for i, n in enumerate(WHITE):
        lab = whites.get(n, '')
        out.append(f'<div class="w{" on" if lab else ""}" style="left:{i * ww:.1f}px;width:{ww - 3:.1f}px">'
                   f'<b>{n}</b>{e(lab)}</div>')
    for n, after in BLACK:
        lab = blacks.get(n, '')
        x = (after + 1) * ww - ww * 0.36
        out.append(f'<div class="b{" on" if lab else ""}" style="left:{x:.1f}px;width:{ww * 0.72:.1f}px">'
                   f'<b>{n[:-1]}</b>{e(lab)}</div>')
    out.append('</div>')
    return ''.join(out)


def card(title, color, body, extra=''):
    return f'<div class="card" style="--c:{color}" {extra}><h3>{e(title)}</h3>{body}</div>'


def knobs(*labels):
    return '<div class="knobs">' + ''.join(
        f'<span class="{"e" if l == "-" else ""}">{e(l)}</span>' for l in labels) + '</div>'


def page(title, sub, body, name):
    doc = f"""<!doctype html><html><head><meta charset="utf-8"><title>JIANT FM · {e(title)}</title>
<style>{CSS}</style></head><body>
<header><img src="signature.png" alt="JIANT FM signature"><div>
<h1>JIANT FM · {e(title)}</h1><h2>{e(sub)}</h2>
<div class="ver">firmware {VERSION} · M-VAVE FM-1 · juanjiant-bit.github.io/JIANT</div></div></header>
<div class="rule"></div>
{body}
<footer><span>GPL-3.0 · fork of Felucca (Hügelton Instruments) · song mode from SLOOP</span>
<span>JIANT FM {VERSION}</span></footer>
</body></html>"""
    with open(os.path.join(OUT, name), 'w') as f:
        f.write(doc)


def sec(t):
    return f'<div class="sec">{e(t)}<span></span></div>'


# ------------------------------------------------------------------------------------------------ 1. CONTROLS ---
def controls():
    b = []
    b.append(sec('NAVIGATION · SIX RULES'))
    rules = [
        ('1', 'SELECT WALKS THE PAGES', 'Every page of the section in one chain, a page a detent, stopping at both ends. '
         'No levels, no zoom.', 'FX: SENDS → DIST → AUTO PAN → BUSES → DLY → DLY 2 → … → PUNCH'),
        ('2', 'THE BUTTON: IN, OR ON', 'From another section: the page you were last on there. Already inside: the '
         'first page of the next group (round).', 'FX › DLY 2: FX → REVERB · EDIT › FILTER: EDIT → FOCUS'),
        ('3', 'PRESETS LOADS PRESETS', 'Always: the track\'s sounds; SEQ the patterns (PHRASES, OCT+ loads); GEN the '
         'genre. Never a page, a track or a module.', 'FX › DLY: PRESETS ▶ the next sound'),
        ('4', 'KNOB 1 PICKS', 'The step, the bar, the slot, the stage, the operator, the kit\'s sound, the row. The track: '
         'always ALGORITHM.', 'SEQ › STEP: K1 = all 64 steps · SEQ › BAR: K1 = the bar'),
        ('5', 'HOLD = THE LAYER', 'Held + SELECT: HOME GEN · EDIT SEQ LFO FX their dice · GLO the tempo · ENV SCL ARP '
         'SAVE their chain.', 'SEQ + G5 CLR STEP · SAVE + C5 NAME'),
        ('6', 'THE SCREEN SAYS WHERE', 'Tabs on top: the section\'s groups, the one shown lit, a dot a page. Footer: '
         'SECTION · GROUP · page and n/N.', 'FX · DELAY · DLY 2   6/16'),
    ]
    b.append('<div class="grid g3">' + ''.join(
        card(f'{n} · {t}', HEAT[int(n) + 1], f'<p>{e(d)}</p><p class="dim">{e(x)}</p>') for n, t, d, x in rules) + '</div>')
    b.append(sec('THE PANEL'))
    rows = [
        ('MASTER', 'Master volume.'),
        ('SELECT', '<b>Walks the section\'s pages</b>, a page a detent, stopping at the ends (the MENU: every row). Held with a button: '
                   'HOME <b>GEN</b>; EDIT · SEQ · LFO · FX their <b>dice</b> (OK keeps, CANCEL back); GLO the tempo; ENV · SCL · ARP · SAVE their chain.'),
        ('PRESETS', '<b>Always presets</b>: the sounds (factory and user) on every page; SEQ: the patterns (PHRASES); GEN: the genre. '
                    'Nothing in the MENU or with a button held, except <b>EDIT</b> (V10.7): the first sound of the next / previous engine.'),
        ('ALGORITHM', 'Selects the track from any page: T1–T4, or T1–T6 with TRACKS 6. MENU: the tabs. A layer held: MAP · KNOBS · STATE.'),
        ('KNOB 1–4', 'The four columns on screen; KNOB 1 picks (step, bar, slot, row). Fast sweeps the whole range, slow fine-tunes. With a layer held: '
                     'the layer\'s.'),
        ('OCT− · OCT+', 'The keyboard\'s octave (both together: back to 0); on DRUM the kit stays put: +1/+2/+3 ratchet x2/x4/x8, −1 SHORT, −2 GHOST, −3 FLAM, each sound its own (grid KNOB 3 HIT). On action pages and dialogs: back / confirm.'),
        ('PLAY', 'Start / stop. GLO + PLAY: restart from the top.'),
        ('REC', 'Arms live recording on the selected track (notes and knob moves, overdub).'),
        ('HOME', 'Tap: HOME (on HOME the next group: ECO · BEING · GENE). <b>Hold: the MENU.</b>'),
        ('KEYS', '27 keys play the selected track in its scale; with a layer held they become that layer\'s.'),
    ]
    t = '<table>' + ''.join(f'<tr><td class="n">{e(n)}</td><td>{d}</td></tr>' for n, d in rows) + '</table>'
    pb = [
        ('HOME', 'ECO: ECO · MACROS  |  BEING: BEING · ROUTES  |  GENE: GENE 1 · GENE 2 · GENE 3', 'MENU'),
        ('EDIT', 'SOUND: EDIT 1 · EDIT 2  |  TONE: FILTER · MORPH · RETRIG · ROTOR · WORDS · STEREO · SWARM · SWARM 2  |  FOCUS  |  VOICE: VOICE · VOICE 2 (V10.50: last). EXT: EDIT 1 / 2 are EXT and SHAPE. FM6: SOUND  |  TONE: FILTER · STEREO  |  OPERATOR: OPERATOR · OP ENV · OPERATOR 2  |  VOICE. DRUM: KIT: KIT · KIT 2 · KIT 3 · KIT 4  |  SOUND: SOUND 1–4  |  LEVELS 1–2  |  TONE: X-MOD · FILTER · STEREO · VOICE', 'INIT · RECALL · sound mutes'),
        ('SEQ', 'STEP  |  LENGTH: LENGTH · RHYTHM · TRACK  |  EUCLID: EUCLID · MODES · MUTATE  |  PLAYHEADS  |  STRETCH  |  COMP: COMP · COMP 2  |  AUTO: AUTOMATION · AUTO LIST · LYRIC STEP  |  GENERATE', 'SEQ TOOLS'),
        ('FX', 'TRACK: SENDS · DIST · AUTO PAN  |  BUSES  |  DELAY: DLY 1–3  |  REVERB 1–2  |  CHORUS  |  SLICER: SLICER · SL BUS 1–2 · SL PITCH  |  PUNCH  |  MASTER · TAPE · EQ',
         'PUNCH-IN FX'),
        ('GLO', 'LIVE: MANO · RIELES  |  MIX: MIXER · SENDS  |  COMPS: COMPS · COMPS 2  |  GLOBAL  |  SYSTEM: SYSTEM · SYSTEM 2 (at power-on GLO opens MIXER)', 'MUTE · SOLO · LEVEL · TAP'),
        ('ENV', 'TRACKS: TRACKS · TRACKS DEST  |  ENV: ENV · ENV DEST · ENV 2  |  STAGE', '—'),
        ('LFO', 'MOD  |  LFO: LFO · LFO 2 · LFO DEST  |  MSEQ: MSEQ · MSEQ 2', 'MACROS M1–M4 (stay latched)'),
        ('SCL', 'KEY  |  CHORD  |  SCENE', 'ROOT by key'),
        ('ARP', 'ARP: ARP · ARP 2', '—'),
        ('SAVE', 'SONG: TIMELINE · PARTS · CLIPS · SECTIONS  |  PROJECT: PROJECTS · ARRANGE · TOOLS · SAVE SONG AS  |  SOUND: USER · PRESETS (from outside, USER / SECTIONS come armed)', 'SONG layer · C5 NAME'),
        ('REC', '(transport)', 'CLEAR · CLICK · held alone: UNDO'),
    ]
    t2 = ('<table class="pages"><tr><th>BUTTON</th><th>GROUPS (A TAP: THE NEXT) · PAGES (SELECT)</th><th>HOLD</th></tr>' +
          ''.join(f'<tr><td class="n">{e(n)}</td><td>{e(p)}</td><td class="dim">{e(h)}</td></tr>' for n, p, h in pb) +
          '</table>')
    b.append('<div class="grid g2">' + card('KNOBS AND TRANSPORT', HEAT[0], t) +
             card('SECTIONS', HEAT[2], t2 + '<p style="margin-top:10px">' + k('BUTTON', cls='v') +
                  ' from elsewhere: the page last shown; inside: the next group. ' + k('SELECT', cls='v') +
                  ' walks every page of the section.</p>') + '</div>')

    b.append(sec('LAYERS · HOLD A BUTTON'))
    fx_w = {'F3': 'REP 1/8', 'G3': 'REP 1/16', 'A3': 'REP 1/32', 'B3': 'LPF', 'C4': 'HPF', 'D4': 'OCT−', 'E4': 'OCT+',
            'F4': '½ TEMPO', 'G4': 'DEC−', 'A4': 'DEC+', 'B4': 'STUT 1/8', 'C5': 'STUT 1/16', 'D5': 'ATK+', 'E5': 'ARP',
            'F5': 'RAND', 'G5': 'CLR AUTO'}
    fx_b = {'F#3': 'M1', 'G#3': 'M2', 'A#3': 'M3', 'C#4': 'M4', 'D#4': 'M5', 'F#4': 'M6', 'G#4': 'LOOP 3', 'A#4': 'TGT',
            'C#5': 'LOOP 5', 'D#5': 'LOOP 7', 'F#5': 'GLITCH'}
    glo_w = {'F3': 'SOLO 1', 'G3': 'SOLO 2', 'A3': 'SOLO 3', 'B3': 'SOLO 4', 'C4': 'SOLO 5', 'D4': 'SOLO 6',
             'E4': 'ALL', 'F4': 'TAP', 'G4': 'UNDO'}
    glo_b = {'F#3': 'M1', 'G#3': 'M2', 'A#3': 'M3', 'C#4': 'M4', 'D#4': 'M5', 'F#4': 'M6', 'G#4': 'KICK',
             'A#4': 'SNR', 'C#5': 'HAT', 'D#5': 'PERC'}
    sv_w = {'F3': 'PLAY A', 'G3': 'PLAY B', 'A3': 'PLAY C', 'B3': 'PLAY D', 'C4': 'STORE A', 'D4': 'STORE B',
            'E4': 'STORE C', 'F4': 'STORE D', 'C5': 'NAME', 'D5': 'LOOP/ SONG', 'E5': 'SONG REC', 'G5': 'SONG'}
    ed_w = {'F3': 'INIT', 'G3': 'RECALL'}
    ed_b = {'F#3': 'BD', 'G#3': 'SD', 'A#3': 'CP', 'C#4': 'CH', 'D#4': 'OH', 'F#4': 'TM', 'G#4': 'RS', 'A#4': 'CB'}
    b.append('<div class="grid g2">')
    b.append(card('FX · PUNCH-IN EFFECTS', HEAT[5], keyboard(fx_w, fx_b) +
                  '<p>A key plays its effect while held (MENU FX LATCH ON: toggles); hold FX and let go untouched: <b>LOCKED</b> '
                  'open, FX again unlocks; OCT- all off. In on the next 1/16. Black 1–6 mute T1–T6, A#4 picks which tracks the MIDI ones '
                  'touch (all · synths · drums). G#4 C#5 D#5 loop the last 3 / 5 / 7 steps; F#5 <b>GLITCH</b> skips on the 1/16 grid, its cycle '
                  'locked to the bar (par9). With REC armed and playing they record into the section\'s lane; G5 '
                  'clears it.</p><h4>KNOBS</h4>' + knobs('FILTER', 'CRUSH', 'THROW', 'DEPTH')))
    b.append(card('GLO · MIX', HEAT[1], keyboard(glo_w, glo_b) +
                  '<p>Black 1–6 mute T1–T6 (latched), 7–10 the DRUM-X groups. White: solo while held, E4 unmutes all, '
                  'F4 tap tempo, G4 <b>UNDO</b> the last big change (again: redo; V10.7: also REC held alone and let go). SELECT: the tempo. PLAY: restart.</p><h4>KNOBS</h4>' +
                  knobs('LEVEL T1', 'LEVEL T2', 'LEVEL T3', 'LEVEL T4') +
                  '<p class="dim" style="margin-top:4px">T5 or T6 selected: the knobs are T3–T6.</p>'))
    b.append(card('SAVE · SONG', HEAT[6], keyboard(sv_w, {}) +
                  '<p>PLAY A–D on the next bar (several in one hold: a quick chain), STORE the section into A–D. '
                  'OCT+ stores the playing section, OCT− recalls it. C5 renames (USER, SECTIONS, PROJECTS). On the SONG page KNOB 4 stores a scene.</p>'
                  '<h4>KNOBS</h4>' + knobs('SONG 1–8', '-', '-', '-')))
    b.append(card('EDIT · SOUND', HEAT[3], keyboard(ed_w, ed_b) +
                  '<p>INIT: the factory sound (DRUM: the kit too). RECALL: the sound stored in the section. On a DRUM '
                  'track black 1–8 mute each DRUM-X sound.</p><h4>KNOBS</h4>' + knobs('EDIT 1', 'EDIT 2', 'EDIT 3', 'EDIT 4')))
    b.append('</div>')
    small = [
        ('LFO · MACROS', HEAT[2], 'Knobs M1–M4; they stay latched when let go (LFO again: off). F3 rolls new routes, '
                                  'G3 clears them (only tracks that sound). LFO + SELECT: the track\'s LFO diced.', ('M1', 'M2', 'M3', 'M4')),
        ('SCL · SCALE', HEAT[0], 'Any key sets the ROOT. ROOT and SCALE are every melodic track\'s (and RESO\'s).',
         ('ROOT', 'SCL', 'OFS', 'PIT')),
        ('SEQ · TOOLS', HEAT[4], 'On the SEQ pages: F3 CLEAR · G3 REVERSE · A3 B3 SHIFT · F4 CLR AUTO (the automation) · G5 CLR STEP (AUTO LIST: the record). DRUM: G4 CLR · A4 REV · B4 FILL the lane (black 1–8 pick it).',
         ('LEN', 'DIV', 'SWING', 'GATE')),
        ('REC · RECORD', '#ef4444', 'F3 CLEAR the track (at once; GLO + G4 undoes it), G3 the metronome CLICK.', ('CLICK', '-', '-', '-')),
    ]
    b.append('<div class="grid g4" style="margin-top:22px">' + ''.join(
        card(t, c, f'<p>{e(d)}</p><h4>KNOBS</h4>' + knobs(*kn)) for t, c, d, kn in small) + '</div>')

    b.append(sec('COMBOS'))
    combos = [
        (k('HOME', cls='v') + ' + ' + k('SELECT', cls='v'), 'GENE, the song generator (HOME\'s last pages).'),
        (k('EDIT · SEQ · LFO · FX', cls='v') + ' + ' + k('SELECT', cls='v'), 'Their dice: sound · sequence · LFO · effects (KEEP? OK / CANCEL).'),
        (k('ENV · SCL · ARP · SAVE', cls='v') + ' + ' + k('SELECT', cls='v'), 'Into that section and along its chain.'),
        (k('SEQ') + ' + ' + k('G5'), 'CLR STEP: the cursor step and its locks (AUTO LIST: deletes the record).'),
        (k('SAVE') + ' + ' + k('C5'), 'NAME: rename the slot, the section, the project.'),
        (k('REC') + ' + ' + k('FX') + k('EDIT') + k('ENV') + k('LFO') + k('SCL') + k('ARP') + k('GLO'),
         'Clear that part\'s automation on the selected track (FX: the punch-in lane too).'),
        (k('SEQ') + ' + ' + k('REC'), 'CLEAR ALL SEQUENCES? (OCT+ confirms).'),
        (k('SAVE') + ' + ' + k('OCT+') + ' / ' + k('OCT−'), 'Store / recall the playing section.'),
        (k('GLO') + ' + ' + k('PLAY'), 'Restart from the top.'),
        (k('GLO') + ' + ' + k('SELECT'), 'Tempo (from any page).'),
        (k('STEP HELD', cls='a') + ' + ' + k('KNOB'), 'A parameter lock on that step (roll, grid).'),
        (k('HOME', cls='a') + ' held', 'MENU, four tabs DISPLAY · CONTROL · AUDIO · SYSTEM (ALGORITHM the tab, PRESETS the row, OCT−/+ the value; '
         'each knob its card): skins (AIR · COLORS · BOTTOM · HUD), SCOPE, palettes, knob accel, FX latch, hold time, USB level; '
         'SYSTEM › USB: AUDIO · STEREO · STEMS · IN+OUT · STEMS+IN · <b>HOST</b> (a USB-MIDI keyboard or a USB audio interface, '
         'on a powered hub; never saved); SYSTEM › BLUETOOTH: OFF · ON (MIDI, app JIANT FM-1) · AIR (PROX and SWARM from the devices around), '
         'OCT+ the PROX page (FAR / NEAR from 2.5 s, HOLD STILL). CONTROL › <b>VELOCITY</b> · VEL MIN · <b>AFTERTOUCH</b> · VEL LEARN '
         '(V10.6z7, F3..C5); AUDIO › <b>OCT BEND</b> (a note held + OCT− vibrato, OCT+ bend).'),
    ]
    t3 = '<table>' + ''.join(f'<tr><td style="white-space:nowrap;width:1%;padding-right:18px">{a}</td><td>{d}</td></tr>'
                             for a, d in combos) + '</table>'
    b.append(card('BUTTONS TOGETHER', HEAT[7], t3))

    b.append(sec('EVERY PAGE · KNOB 1 – 4'))
    fams = [
        ('ENV', HEAT[0], [('ENV', 'ATK DEC SUS REL'), ('ENV DEST', 'FILTER PITCH SHAPE LOOP'), ('ENV 2', 'ATK DEC SUS REL'),
                          ('ENV 2 →', 'a second ADSR: MOD SRC ENV2, any DEST (V10.22)'), ('STAGE', 'STAGE VALUE LOOP -')]),
        ('LFO', HEAT[1], [('LFO', 'RATE WAVE MODE FADE'), ('LFO 2', 'PHASE - - -'), ('LFO DEST', 'PITCH FILTER SHAPE AMP'), ('MOD', 'SLOT SRC DEST AMT'),
                          ('PROX (BT)', 'SRC PROX: the phone\'s distance (AIR: a hand, no phone) · MENU › SYSTEM › BLUETOOTH, OCT+: OCT− FAR, OCT+ NEAR; K1 MIN, K2 MAX, K3 SPD (V10.61)'),
                          ('FOLLOW', 'SRC FOLLOW: the audio input\'s envelope (EXT, V10.54), any track, any DEST'),
                          ('MSEQ', 'STEP LEVEL LEN SLEW'), ('MSEQ 2', 'DIV - - -')]),
        ('FX', HEAT[5], [('FX', 'DIST CHORUS DLY REV'), ('DIST', 'DIST TYPE TONE -'), ('AUTO PAN', 'DEPTH RATE SYNC WAVE'), ('SLICER', 'MODE PAT RATE DEPTH'),
                         ('SL BUS', 'T1 T2 T3 T4'), ('SL BUS 2', 'T5 T6 FX -'), ('SL PITCH', 'STEP PITCH LEN QUANT'),
                         ('DLY', 'TIME FEEDBK TONE MIX'), ('DLY 2', 'WIDE PITCH SPRAY MOD'), ('DLY 3', ['TYPE', 'PING', 'DRIVE', 'R TIME']),
                         ('REVERB', 'TYPE SIZE DAMP PRE/AIR'), ('REVERB 2', 'MOD RATE FILT WIDE'),
                         ('CHORUS', 'RATE DEPTH STEREO -'), ('PUNCH', 'LEN RES REC CLEAR'), ('MASTER', 'CLIP PUNCH DUCK REL'), ('TAPE', 'TAPE WOW - -'),
                         ('EQ', ['LOW', 'LO MID', 'HI MID', 'HIGH'])]),
        ('EDIT', HEAT[3], [('EDIT 1', 'E1 E2 E3 E4'), ('EDIT 2', 'E5 E6 E7 E8'), ('EXT', 'SOURCE GAIN GATE PLAY'), ('SHAPE', 'WIDTH RING BITS RATE'), ('SWARM', 'SIDE RATE MOVE TONE'), ('SWARM 2', 'SIDE NOISE MOD NOW'),
                           ('MORPH', 'MORPH DRIFT - -'), ('ROTOR', 'DEPTH DUST - -'),
                           ('WORDS', 'LANG LYRIC CONS (OCT+ TYPE)'), ('RETRIG', 'RTRG - - -'), ('OPERATOR', 'OP RATIO FINE LEVEL'),
                           ('OP ENV', 'OP STAGE RATE LEVEL'), ('OPERATOR 2', 'OP MODE DETUNE VEL'), ('FILTER', 'TYPE CUT RES DRIVE'),
                           ('VOICE', 'VOICE GLIDE MODE PRIO'), ('VOICE 2', 'ALLOC DETUNE - -'), ('FOCUS', 'PARAM VALUE SRC AMT'), ('KIT 3', 'BP FEEDBK HANDS -'), ('KIT 4', '- KEYMAP - -'),
                           ('X-MOD', 'RATE DEPTH SHAPE RAND'), ('LEVELS', 'KICK SNARE CLAP HATCL'),
                           ('LEVELS 2', 'HATOP TOM RIM BELL'), ('SOUND', 'SOUND SIDE PITCH PMOD'),
                           ('SOUND 2', 'SOUND DECAY NOISE COLOR'), ('SOUND 3', 'SOUND MODE PM.TYPE DRIVE'),
                           ('SOUND 4', 'SOUND LAYER FEEDBK SEND'), ('STEREO', 'IMAGE - - -'), ('STEREO →', 'DRUM-X and (V10.49) every synth in POLY: the voices spread L–R')]),
        ('SCL · ARP', HEAT[2], [('SCL', 'ROOT SCALE QUANT ALL TR'), ('CHORD', 'CHORD VOICING STRUM -'), ('SCENE', 'ON TRACK ROLE CHORD'), ('ARP', 'MODE RATE OCT GATE'),
                                ('ARP 2', 'SWING PROB HOLD ORDER')]),
        ('SEQ', HEAT[4], [('LENGTH', 'LEN DIV SWING GATE'), ('STRETCH', 'x/÷ REPEAT NUDGE ROTATE'), ('NUDGE', 'OCT− blue: the whole sequence by steps · OCT+ red: the cursor step in 64ths (MICRO)'), ('PLAYHEADS', 'HEAD RATE DIR INTVL'),
                          ('TRACK', 'PITCH CHANCE HUMAN REC Q'), ('EUCLID', 'HITS STEPS ROTATE VARY'), ('EUCLID · DRUM', 'HITS STEPS ROTATE LANE'), ('EUCLID · DRUM ALL', 'DENSITY STEPS ROTATE LANE'),
                          ('MODES', 'MOTION HARMONY OCTAVE SLIDE'), ('MODES · DRUM', 'ACCENT GHOST RATCHET VARY'), ('MUTATE', 'EVERY AMOUNT MODE WHAT'),
                          ('STEP', 'STEP NOTE TIME VEL'), ('STEP · DRUM', 'STEP LANE HIT ACC'), 
                          ('AUTOMATION', 'PLAY EVENT LOCK CLEAR'), ('AUTO LIST', 'ROW STEP PARAM VALUE'),
                          ('COMP', 'STEP TYPE VAL EVERY'), ('COMP 2', 'STEP TYPE VAL EVERY')]),
        ('GLO · SAVE', HEAT[6], [('MANO', 'RATE LOOP THIN DJ FILT (the keys: live modifiers)'), ('RIELES', 'PISTAS RATE CORTE DENS'),
                                 ('MIXER', 'LEVEL PAN REV BPM'), ('GLOBAL', 'BPM SWING CLOCK TUNE'), ('COMPS', 'ACTION LENGTH COND VALUE'), ('COMPS 2', 'the global lane: its TRACKS, MONO / POLY'),
                                 ('SYSTEM', 'TRACKS LOAD ROUTE CPU'), ('SYSTEM 2', ['USB', 'TRS IN', 'BT', '-']), ('PRESETS', 'No. ENG FAV LIST · OCT+ keep, OCT- back'),
                                 ('USER', 'SLOT LOAD ERASE SAVE'), ('PROJECTS', 'PROJECT SECT LOAD SAVE'), ('SECTIONS', 'SECT ERASE LOAD SAVE'), ('ARRANGE', 'ROW SECT BARS SCENE (OCT+ adds a row, OCT− deletes the one picked)'), ('TIMELINE', 'ROW LANE · SEC: SECT BARS · T1–T6: FROM VAR'),
                                 ('TOOLS', 'K1 SCOPE (TRACK SOUND SONG SYSTEM) · K2 the action · hold OCT+ a second')]),
    ]

    def ptable(rows):
        out = '<table class="pages">'
        for p, ks in rows:
            parts = ks if isinstance(ks, list) else ks.split(' ')   # (a list: a label with a space, R TIME)
            if len(parts) == 4:
                cells = knobs(*parts)
            else:
                cells = f'<span class="dim">{e(ks if isinstance(ks, str) else " ".join(ks))}</span>'
            out += f'<tr><td class="p">{e(p)}</td><td>{cells}</td></tr>'
        return out + '</table>'
    cards = {n: card(n, c, ptable(r)) for n, c, r in fams}
    b.append('<div class="grid g3"><div class="col">' + cards['ENV'] + cards['LFO'] + cards['SCL · ARP'] + '</div>' +
             cards['FX'] + cards['EDIT'] + '</div>')
    b.append('<div class="grid g2" style="margin-top:22px">' + cards['SEQ'] + cards['GLO · SAVE'] + '</div>')
    page('CONTROLS', 'every button, knob, layer, combo and page', ''.join(b), 'controls.html')


# ------------------------------------------------------------------------------------------- 2. SOUND & FX ---
def sound():
    b = []
    b.append(sec('ENGINES · EDIT 1 / EDIT 2'))
    eng = [
        ('ANALOG', HEAT[1], 'WAVE DETUNE MIX NOISE', 'CUT RES DRIVE INT',
         'SAW SQR TRI SIN PWM <b>SYNC RING SAW3 PLS3</b>; INT: osc 2 ±24 st; on SAW3 / PLS3 the NOISE knob is <b>INT 3</b>, osc 3\'s '
         'interval (V10.39). Its filter is per voice. <b>Its picture</b> (0.9.6): '
         'a cell whose membrane is the wave, spikes where DRIVE clips, a ring at CUT.'),
        ('FM6', HEAT[3], 'ALG FB MOD RATIO', 'MEG VMOD DETUNE SLOT',
         'Dexed\'s 6 operators, 32 algorithms drawn, .syx patches (SLOT), MOD / RATIO move the modulators. '
         '<b>OPERATOR · OP ENV · OPERATOR 2</b>: each operator on the device (ratio / fixed, fine, level, its envelope, '
         'mode, detune, velocity). <b>SLOT B1–B32</b>: a 32-voice .syx sent from the web editor. <b>Its picture</b> (0.9.6): '
         'the operators as bichos wired as the algorithm, OP ENV\'s riding its envelope.'),
        ('PHASE', HEAT[2], 'WAVE WAVE2 DCW ENV', 'DETUNE LINE SUB FB',
         'Phase distortion (CZ); <b>FB</b> feeds the output back into the phase: hard edge to growl. <b>Its picture</b> '
         '(0.9.6): a snail, its shell\'s chambers at the bent phase, its sole the wave.'),
        ('MATH', HEAT[0], 'CHIP WAVE ALGO VAR', 'MOTN GRIT BEND LOOP',
         '(V10.6e: LOFI\'s formulas alone; its chip waves load as ANALOG) <b>BYTE</b>: ALGO VAR · MOTN GRIT BEND LOOP. <b>FLOAT</b> (floatbeat): '
         'ALGO VAR · MOTN FBK BEND LOOP; CHIP sets FULL / 4BIT / 1BIT. Every sound starts still (MOTION 0), RTRG on: in tune. '
         '48 formulas each in BYTE and FLOAT (ALGO 0–191); MOTN past half is <b>SYNC</b>: the formula\'s slow bits follow the bar.'),
        ('STRING', HEAT[3], 'EXCITE DECAY BRIGHT POS', 'STRETCH BODY SIZE BOW',
         'Karplus-Strong string (snd5): EXCITE NOISE PLUCK PICK HAMMR STICK; POS where it is struck, STRETCH a stiff, '
         'metallic string, BODY / SIZE its box, <b>BOW</b> a bow\'s noise into the loop. RELEASE is its damper: the string '
         'rings after the step and dies out when let go. <b>FDBK</b>: guitar feedback (MOVE where POS was; EDIT 2 GAIN DIST SPEAKR '
         'BLOOM). NYLON, STEEL GTR, HARP, KOTO, PIANO, MARIMBA, BOWED…'),
        ('ADD', HEAT[1], 'MODE MORPH BRIGHT PARTIAL', 'SHIFT COMB DECAY MOTION',
         'Additive (snd5): partials by MODE HARM ODD WIDE BELL BAR DRUM <b>PRIME FIFTH SUB PLATE</b> (par6); MORPH between '
         'spectra, PARTIAL how many, SHIFT stretches them, COMB carves notches, DECAY the highs die first, MOTION sweeps '
         'the spectrum. Presets PRIME ORG, QUINT, DRONE, GONG, TOTEM…'),
        ('VOICE', HEAT[6], 'VOWEL VOWEL2 TALK SHIFT', 'BUZZ BREATH Q RAND',
         'Formant voice (after klattsch): vowels, talk, breath. <b>WORDS</b> (0.8.5): LANG ES / EN / JA sings syllables, '
         'a note each (POS THEME ARTIC SHIFT · BUZZ BREATH Q LOOP); LYRIC TEXT (OCT+ on WORDS types it, KNOB 3 dice; '
         'the web editor sends it) or W1–16 words, S1–8 scat; "-" holds the vowel. <b>Its picture</b> (0.9.4): a CT head '
         'whose tongue, jaw, lips and velum sing the phoneme.'),
        ('WHEEL', HEAT[5], 'REG SUB BODY TOP', 'PERC CLICK DRIVE ROTOR', 'Drawbar organ: registrations, percussion, rotary. '
         '<b>Its picture</b> (0.9.6): nine tube worms, one a drawbar, bent by the rotor.'),
        ('NOISE', HEAT[4], 'MODE COLOR FREQ PEAK', 'TRK DENSE DRIFT CRUSH', 'Coloured and metallic noise, tracked or free. '
         '<b>Its picture</b> (0.9.6): a swarm walking by COLOR, swirling by FREQ.'),
        ('DRUM-X', '#ff7a00', 'MORPH TUNE TONE DECAY', 'NOISE FM ENV FOLD DRIVE',
         '8 sounds BD SD CP CH OH TM RS CB, sides A / B: <b>MORPH</b> morphs the kit. The kit first: <b>KIT 3</b> BP FEEDBK, <b>X-MOD</b> RATE DEPTH SHAPE '
         'RAND. Then a sound (KNOB 1 the sound): SIDE PITCH PMOD · DECAY NOISE COLOR · MODE PM.TYPE DRIVE · <b>LAYER</b> (no note) FEEDBK '
         '<b>SEND</b> (CHO / DLY / REV). Last LEVELS and <b>STEREO</b> IMAGE (a knob through the kit\'s stereo placings). '
         '<b>KIT 4 KEY MAP</b> (V10.6r): EXTENDED (default) +1 FILLS, −1 KICK / TOM or one sound chromatic, −2 FX KIT, −3 FLAM; or CLASSIC. Cowbell: classic CMOS (V10.6q). '
         'KIT, SOUND 1–4 and HOME › BEING draw the kit as little beings that squash and shout on each hit (0.9.4).'),
        ('EXT', HEAT[0], 'SOURCE GAIN GATE PLAY', 'WIDTH RING BITS RATE',
         '(V10.54; EXT since V10.6d) SOURCE <b>SWARM</b>: a melody from the Bluetooth around (PROX first, then AIR; FAR / NEAR morph, CHAOS). Else not a synth: the USB audio input on a track of its own (an interface on USB HOST behind a powered hub, or '
         'the computer with USB IN+OUT). SOURCE IN 1 · IN 2 · 1+2 · STEREO (two tracks can take IN 1 and IN 2); PLAY <b>THRU</b> '
         '(always) or <b>KEYS</b> (only while a note plays, at its envelope: the steps gate it); RING a sine at the note (KEYS); '
         'BITS, RATE crush it. FILTER, DIST, LEVEL, PAN, sends as any track; the page shows both inputs\' meters. '
         '<b>MOD SRC FOLLOW</b>: its envelope. Presets IN 1, MIC GATE, SPACE IN, TRANCE GATE, RING IN, CRUSH IN…'),
        ('ANIMAL', '#4ade80', 'MODE PRESS CHAOS SIZE', 'SWEEP RATE GROUP TEMP',
         'Animal calls (0.8.3), MODE the source: <b>CMOS</b> the 40106 / 4093 bird circuit (TOUCH COUPLE CAP · HYST RATE '
         'GROUP FM) · <b>PULSE</b> crickets, cicadas, frogs (CLICK Q · DETUNE) · <b>WHSTL</b> birds, a Hopf whistle · '
         '<b>WING</b> mosquito, bee, fly (BUZZ DRIFT · DOPPLR) · <b>GLOTT</b> wolf, bark, roar, purr (MOUTH ROUGH · AIR). '
         'GROUP: syllables + gaps; TEMP °C speeds a cricket up (Dolbear). A note is a call. <b>Its picture</b> (0.9.4): '
         'bird ↔ insect ↔ mammal by MODE, leaned by SIZE.'),
    ]
    ORDER = ['DRUM-X', 'ANALOG', 'PHASE', 'FM6', 'VOICE', 'ANIMAL', 'MATH', 'WHEEL', 'NOISE', 'STRING', 'ADD', 'EXT']   # (V10.6e: the band first; EXT last)
    eng.sort(key=lambda r: ORDER.index(r[0]))                     # (src/engines.c ENGINE_ORDER: PRESETS, KNOB 2 ENG)
    t = '<table><tr><th>ENGINE</th><th>EDIT 1</th><th>EDIT 2</th><th>WHAT TO KNOW</th></tr>'
    for n, c, e1, e2, d in eng:
        t += (f'<tr><td class="n" style="--c:{c}">{e(n)}</td><td style="width:22%">{knobs(*e1.split())}</td>'
              f'<td style="width:22%">{knobs(*e2.split())}</td><td>{d}</td></tr>')
    t += '</table>'
    b.append(card('TWELVE ENGINES: ELEVEN SYNTHS AND EXT', HEAT[7], t))
    b.append('<div class="grid g3" style="margin-top:22px">')
    b.append(card('FILTER · EVERY ENGINE', HEAT[3], knobs('TYPE', 'CUT', 'RES', 'DRV') +
                  '<p style="margin-top:8px"><b>LP · HP · BP · COMB · COMB- · MS20 · LP24</b> (LP24: four poles, V10.22). COMB rings tuned to the note (COMB-: hollow, odd harmonics): CUT moves it '
                  '±32 st, RES how much it rings. ANALOG and MATH: per voice; the others: on the track, following '
                  'LFO / ENV DEST FLT (DRV = the track\'s DIST).</p>'))
    b.append(card('VOICE · PLAYING', HEAT[2], knobs('VCE', 'GLD', 'GLMOD', 'PRIO') +
                  '<p style="margin-top:8px">POLY · MONO · LEG · UNI; glide and its mode; LAST / LOW / HIGH priority. '
                  '<b>8 voices</b> shared by the six tracks (MONO and LEGATO keep their note). <b>EDIT › STEREO IMAGE</b> (V10.49): '
                  'in POLY each voice its own place across the stereo field, drawn on the page (V10.52).</p>'))
    b.append(card('DICE', HEAT[5], '<p><b>EDIT + SELECT</b>: a random factory preset of the engine, every '
                  'parameter moved up to a quarter of its range. DRUM-X: both sides of every sound around the factory '
                  'kit. Heard at once: OCT+ OK keeps it, OCT- CANCEL brings the sound back. SEQ / LFO / FX + SELECT: the sequence, the LFO, the effects, the same way. HOME + SELECT (or HOME twice on HOME): <b>GEN</b>, the song generator, HOME\'s last three pages: GENRE (<b>EXPLORE</b>, where it opens: no genre, every rule broken; RANDOM; 43 genres; PRESETS too) · SEED · ENERGY · FORM; DRUMS · BASS · CHORDS · ARP · LEAD amounts; COMPLEX · SWING · HUMAN (SELECT: the next page). OCT+ makes the piece (again: a new seed), OCT- back to ECO. GLO + G4 undoes.</p>'))
    b.append('</div>')

    b.append(sec('MODULATION'))
    b.append('<div class="grid g3">')
    b.append(card('ENV · LFO', HEAT[0],
                  '<h4>ENV</h4>' + knobs('ATK', 'DEC', 'SUS', 'REL') + '<h4>ENV DEST</h4>' + knobs('FLT', 'PIT', 'SHP', 'LOOP') +
                  '<p style="margin-top:6px">LOOP: held, the envelope cycles from the sustain: an ADSR-shaped LFO. '
                  '<b>ENV 2</b> (V10.22): a second ADSR, MOD SRC ENV2 on any destination.</p>'
                  '<h4>LFO</h4>' + knobs('RATE', 'WAVE', 'MODE', 'FADE') + '<p class="dim">SIN TRI SAW SQR S&amp;H '
                  '<b style="color:#f4f1ea">DRIFT</b> (V10.36: a smooth random wander; at 4BAR–16BAR pads breathe) · '
                  'RATE up to 16BAR · MODE TRIG / FREE · LFO 2: PHASE</p>'
                  '<h4>LFO DEST</h4>' + knobs('PIT', 'FLT', 'SHP', 'AMP')))
    b.append(card('MOD MATRIX · 4 SLOTS A TRACK', HEAT[2],
                  knobs('SLOT', 'SRC', 'DST', 'AMT') +
                  '<h4>SOURCES</h4><p>' + k('LFO', 'ENV', 'VEL', 'KEY', 'RAND', 'MODW', 'AT', 'EXPR', 'M1', 'M2', 'M3', 'M4',
                                             'STEP', 'NOISE', 'PROX', 'ENV2', 'FOLLOW') + '</p>'
                  '<h4>DESTINATIONS</h4><p>' + k('PITCH', 'CUT', 'SHP', 'AMP', 'PAN', 'DIST', 'CHO', 'DLY', 'REV', 'RATE',
                                                  'VIB', 'E1…E8', 'CLIP', 'PNCH', 'D.TIM', 'D.FB', 'D.TON', 'D.MIX', 'R.SIZ', 'R.DMP', 'C.RAT', 'C.DEP') + '</p>'
                  '<p style="margin-top:6px">E1…E8: the engine\'s own EDIT knobs. CLIP / PNCH: the master. D. R. C.: the DELAY, REVERB and CHORUS buses (a STEP source on D.TIM sequences the delay time). '
                  '<b>PROX</b>: how close the Bluetooth phone is (or AIR: a hand); MENU › SYSTEM › BLUETOOTH, OCT+: FAR / NEAR, MIN MAX SPD. '
                  '<b>FOLLOW</b>: the audio input\'s envelope (an input opens a filter, ducks a pad).</p>'))
    b.append(card('MSEQ · MACROS', HEAT[1],
                  '<h4>MSEQ · A CV SEQUENCER PER TRACK</h4>' + knobs('STEP', 'LVL', 'LEN', 'SLEW') +
                  '<p style="margin-top:6px">16 levels, DIV on MSEQ 2, in time with the transport. Use it from MOD, '
                  'SRC STEP.</p><h4>MACROS M1–M4</h4><p>Hold LFO: the knobs are M1–M4, latched until LFO again. Each '
                  'session starts with 2 random routes a track; F3 rolls new ones, G3 clears. By hand: MOD, '
                  'SRC M1…M4. Saved with the project.</p>'))
    b.append('</div>')
    b.append('<div class="grid g3" style="margin-top:22px">')
    b.append(card('SEQUENCER', HEAT[4],
                  '<p>64 steps a track, up to 4 notes a step, piano roll and drum grid. One chain (V10.6): STEP (the roll / grid, K1 all 64 steps) · LENGTH · TRACK · EUCLID · MODES · MUTATE · PLAYHEADS · STRETCH · COMP · COMP 2 · AUTOMATION · AUTO LIST; SELECT walks, SEQ jumps to the next group, PRESETS loads a pattern. <b>Hold a step + turn a knob</b>: '
                  'a parameter lock. STEP K4 <b>VEL</b> (V10.8): 1–127, then ACC, SLD, A+S. Step recording: hold a chord + KNOB 1 its length; + ARP: arpeggiated (K1 rate, K2 order, K3 oct). COMP / COMP 2: two components a step, each ACTION · LENGTH · CONDITION · VALUE: ARP, SEQ, OCT+, OCT−, RATCH, STUT, MUTE, TRANS, RAND, LOCK, <b>RAMP</b> (V10.6v, JUMP\'s place: the step\'s lock glides there over 1–32 steps or 1–16 bars, LIN EXP LOG S STEPS ZIGZAG), SOUND (a kit\'s SHORT / GHOST / FLAM), BEND, SHIFT, CHORD; CONDITION ALWAYS, 25 / 50 / 75 %, 1ST, FILL, PRE, NEI (and their !), N:M, ELSE (C2: MONO). REC keeps lengths, legato (SLIDE) and velocity; GATE 100 % for new tracks (V10.6z4). TRACK: PITCH, CHANCE thins the whole track, HUMAN one amount (velocity, timing, gate, decay / release), REC Q. STRETCH / REPEAT act as the knob turns; ROTATE reads the sequence from another step; NUDGE in red (OCT+) moves the cursor step off the grid in 64ths (REC Q 0 % records where you played, to the 64th). <b>EUCLID</b> (V10.6; V10.16 VARY in four turns A A\' A\'\' B, and <b>MUTATE</b>: EVERY 1–16 bars, AMOUNT, ORBIT / DRIFT / PULSE, GROOVE / RHYTHM / FEEL / ALL, OCT+ now, OCT− the anchor): HITS, STEPS 2–64, ROTATE write the track\'s pattern live; K4 LANE (DRUM-X: each sound its own; V10.7 <b>ALL</b>, the default: K1 DENSITY 1–8 writes a whole kit, a lane picked writes only that lane) or VARY; MODES adds ACCENT GHOST RATCHET VARY (DRUM-X) or MOTION HARMONY OCTAVE SLIDE; notes in the track\'s SCL, else the key the other tracks fit (FROM Tn), else NEW KEY; OCT+ a new variation, OCT− the one before. REC + knobs while playing: '
                  'automation (64 records a section, MOTION page plays / clears them, AUTO LIST edits them). EUCLID starts at 0 and '
                  'transforms the track\'s own sequence, giving it back as it was at rest (V10.28–V10.31). STRETCH › NUDGE: OCT− the '
                  'whole sequence by steps, OCT+ the cursor step in 64ths (V10.25). Longer than 16 steps, the roll follows the playhead (V10.47).</p>'
                  '<h4>LENGTH · TRACK</h4>' + knobs('LEN', 'DIV', 'SWG', 'GATE') + knobs('PIT', 'CHN', 'HUM', 'RECQ')))
    b.append(card('SCALE · ARP', HEAT[0],
                  '<p><b>ROOT and SCALE</b> are every melodic track\'s: CHR MAJ MIN DOR MIX PEN MPEN HARM PHRY LYD LOC MEL '
                  'BLUES WHOLE DIMHW DIMWH. QNT snaps keys or the sequence, <b>ALL TR</b> (SCL KNOB 4) moves '
                  'every sequence along the scale. <b>CHORD</b>: a key plays a chord (TRIAD 7TH ADD9 SUS2 SUS4 in key, '
                  'MAJ MIN 7 MAJ7 MIN7 POWER), VOICING LEAD leads each to the next, STRUM as a harp. <b>SCENE</b> (V10.24): each track a '
                  'role (KEEP CHORD ARP BASS LEAD DRUM; BASS / LEAD a PATTERN, DRUM a STYLE or GEN) and one key plays them all; OCT+ dices the roles, REC records each.</p><h4>ARP MODES</h4><p class="dim">OFF UP DN UPDN RND ORD REPEAT DNUP '
                  'UP+8 CONV DIVG PINKY THUMB WALK CHORD <b style="color:#f4f1ea">TRNS</b> (the keys transpose the '
                  'sequence)</p>'))
    b.append(card('SONGS', HEAT[6],
                  '<p><b>6 projects × 4 sections (A–D)</b> (V10.6p; 8 before), a song of up to 16 rows by bars (SAVE › PROJECT › ARRANGE, OCT+ adds a row, OCT− deletes one, PLAY plays; END: STOP, x2–x8, LOOP), SLOOP style. '
                  '<b>TIMELINE</b> (V10.6o, SAVE\'s first): the song as blocks, the row picked opened by track: K1 row K2 part K3 bars K4 the track\'s <b>VAR</b> (MUTE THIN FILL HALF REV SHIFT OCT± VAR 1–4), F3–D4 mutes, F4–B4 <b>FROM</b> A–D, PLAY loops the row. <b>PARTS</b>: the four parts, OCT+ loops one. <b>CLIPS</b> (V10.6s): 6 tracks × 4 parts, OCT+ on the next bar, REC writes the rows. A sound changed while the song plays stays (V10.6z8; EDIT › RECALL). '
                  'Each row can hold a <b>scene</b>: mutes, DRUM-X group mutes, macros and MIDI punch-ins. SAVE held '
                  'plays sections live on the next bar, stores them, records the song (SONG REC). Each section keeps its '
                  'sounds, steps, motion, kit and its SLICER.</p>'))
    b.append('</div>')

    b.append(sec('EFFECTS'))
    b.append('<div class="grid g3">')
    b.append(card('PER TRACK', HEAT[5], '<h4>FX · SENDS</h4>' + knobs('DST', 'CHO', 'DLY', 'REV') +
                  '<h4>DIST</h4>' + knobs('DST', 'TYPE', 'TONE', '-') +
                  '<p style="margin-top:6px">SOFT · HARD · FOLD · CRUSH · RECT.</p>'
                  '<h4>AUTO PAN</h4>' + knobs('DEPTH', 'RATE', 'SYNC', 'WAVE') +
                  '<p style="margin-top:6px">The track\'s pan swung L–R around its PAN. SYNC <b>BEAT</b>: RATE 4 BARS … 1/32, '
                  'in time with the bar; <b>FREE</b>: in Hz. SIN · TRI · SAW · SQR · S&amp;H, never a click.</p>'))
    b.append(card('SLICER · ONE ON A BUS', HEAT[3], knobs('SLCR', 'PAT', 'RATE', 'DEPTH') +
                  '<p style="margin-top:6px">OFF · GATE · STUT, 16 patterns, 1/8 … 32T. <b>SL BUS</b>: T1–T6 and '
                  '<b>FX</b> (the delay / reverb / chorus returns) ON / OFF: everything on = the whole mix chopped after '
                  'the effects. <b>SL PITCH</b>: the repeats\' 16-step pitch (±24 st, tape style, QNT SCL to the scale).</p>'))
    b.append(card('DELAY · CHORUS', HEAT[1], '<h4>DLY</h4>' + knobs('TIME', 'FDBK', 'TONE', 'MIX') +
                  '<h4>DLY 2</h4>' + knobs('WIDE', 'PITCH', 'SPRY', 'MOD') + '<h4>DLY 3</h4>' + knobs('TYPE', 'PING', 'DRIVE', 'R TIME') +
                  '<p style="margin-top:6px">Synced (4BAR … 1/32; V10.8 dotted 1/4D 1/8D 1/16D) or free (5 ms – 2.95 s; past 0.74 s darker); PITCH / SPRY: a grain delay. '
                  'TYPE ANALOG / DIGITAL, PING-PONG, DRIVE the loop. <b>R TIME</b> (V10.49): the right echo\'s time as a ratio of TIME '
                  '(=L, x1/2, x2/3, x3/4, x4/3, x3/2, x2: 1/8 with x3/2 is 3/16); not with PING or the grains.</p>'
                  '<h4>CHORUS</h4>' + knobs('RATE', 'DEPTH', 'STER', '-') +
                  '<p style="margin-top:6px">STER: 0 as always, down to mono, up to twice as wide.</p>'))
    b.append('</div>')
    rv = [
        ('ROOM', 'Classic room', 'decay', 'darkens', 'chorus on the tail', 'its speed', 'stereo'),
        ('SPRING', 'Amp spring tank', 'length · decay', 'darkens', 'wobble', 'wobble speed', 'stereo'),
        ('SHIMR', 'Up an octave on every pass; <b>PRE → AIR</b> (±24 st: down darker and lower, up airier and higher)', 'decay', 'darkens', 'how much shimmer', '—', '—'),
        ('RESO', '4 strings tuned to the scale; <b>PRE → PITCH</b> (0–14 steps up), <b>DAMP → FOLW</b> (OFF / T1–T6: the '
                 'chord follows that track\'s notes)', 'how long they ring', 'FOLW', 'past half: the chord opens', '—', '—'),
        ('CLOUD', 'Granular wash; SIZE max <b>freezes</b> it; <b>PRE → AIR</b> (±24 st: the cloud down / up, darker / airier)', 'decay · freeze', 'darkens', 'grains pitched + chorus',
         'grain density', 'grain spread'),
    ]
    t = ('<table><tr><th>TYPE</th><th>WHAT IT IS</th><th>SIZE</th><th>DAMP</th><th>MOD</th><th>RATE</th><th>WIDE</th></tr>' +
         ''.join(f'<tr><td class="n">{e(a)}</td><td>{b_}</td><td>{e(c)}</td><td>{e(d)}</td><td>{e(f)}</td><td>{e(g)}</td>'
                 f'<td>{e(h)}</td></tr>' for a, b_, c, d, f, g, h in rv) + '</table>')
    b.append('<div class="grid g2" style="margin-top:22px;grid-template-columns:2fr 1fr">')
    b.append(card('REVERB · FIVE MODELS', HEAT[2], knobs('TYPE', 'SIZE', 'DAMP', 'PRE') + knobs('MOD', 'RATE', 'FILT', 'WIDE') +
                  '<div style="margin-top:10px">' + t + '</div>'))
    b.append(card('MASTER', HEAT[6], knobs('CLIP', 'PNCH', 'DUCK', 'REL') +
                  '<p style="margin-top:8px"><b>CLIP</b> saturates without getting louder. <b>PNCH</b> drives the drums: '
                  'denser body. <b>DUCK</b>: the kick pulls the rest down (REL its release). An always-on '
                  '<b>leveler</b> (slow 2:1, −9…+6 dB) before the limiter keeps patches at a steady volume.</p>'
                  '<h4>TAPE</h4>' + knobs('TAPE', 'WOW', '-', '-') +
                  '<p style="margin-top:6px">After MASTER: TAPE saturates, darkens the highs and lifts the lows; WOW the '
                  'wow and flutter (par9: much stronger at 100).</p>'
                  '<h4>EQ · 4 BANDS (par9)</h4>' + knobs('LOW', 'LO MID', 'HI MID', 'HIGH') +
                  '<p style="margin-top:6px">−12…+12 dB, wide and gentle (~200 Hz, 1 kHz, 4 kHz); at 0 it costs nothing. The page draws the curve '
                  '(SCOPE ON: the master live behind it); EQ and TAPE change only when a project loads.</p>'
                  '<h4>PUNCH-IN FX (FX HELD)</h4><p>REPEAT 1/8 · 1/16 · 1/32 (1/8 from 41 BPM, 1/16 from 21: V10.59), LPF, HPF, LOOP 3 / 5 / 7, GLITCH on the grid; '
                  'on the notes: OCT−/+, ½ TEMPO, DEC−/+, STUTTER, ATK+, ARP, RANDOM. Knobs: FILTER CRUSH THROW DEPTH. '
                  'Recordable per section.</p>'))
    b.append('</div>')
    page('SOUND · FX · MOD', 'engines, effects, modulation, sequencer and songs', ''.join(b), 'sound.html')


if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    controls()
    sound()
    print('docs/cheatsheet/controls.html, sound.html')
