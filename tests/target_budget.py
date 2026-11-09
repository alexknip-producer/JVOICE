#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Target-side cost estimate of the hot DSP code, from the pi32v2 objdump listing tools/build.py writes
(build/felucca.dis). For each engine's render function and the audio ISR: the instructions
inside loops (every address range closed by a backward branch: the per-sample loops and what they
contain), and the hardware divides and calls in them. cost = the loop instructions, each weighted 4 per
level of nesting (an inner loop runs several times per sample) and 1 + 8 for a divide (many cycles). A static count, not a cycle count: it changes only when the compiled code changes,
so it is exact run to run, and it catches a render loop that grew (more work per sample, a new divide,
something not inlined any more). Compared with tests/target_budget.txt at +10 %. The host
CPU check (regress.c) is the primary one; this one sees the target compiler's code.
  tests/target_budget.py [DIS [BUDGET]]      BUDGET_UPDATE=1 rewrites BUDGET"""
import os
import re
import sys

FUNCS = ["analog_render", "digital_render", "digital_render_legacy", "digital_render_custom", "phase_render", "lofi_render", "formant_render",
         "wheel_render", "wheel_block", "analog_ext", "analog_morph", "lofi_byte_render",   # (JIANT 0.4: SYNC RING SAW3, BYTE)
         "drum_render", "noise_render", "fm6_render", "fm6_op_run", "fm6_op_fb",
         "dx_run_fx", "dx_layer",                           # DRUM-X (drumx_voice.c): a hit, osc + noise (V10.6q: dx_run_fx, was dx_run); (0.6.49) LAYER
         "drum_side_ramp", "drum_mix", "drum_bus",           # (JIANT 0.6.48) DRUM-X IMAGE and SENDs (drum_side_ramp, was drum_side_add); (0.7.9) the kit's curve
         "drum_glue",                                       # (0.9.9-snd1) DRUM-X GLUE, while it is on
         "slicer_bus",                                      # (JIANT 0.6.3) the SLICER, one on a bus
         "fm1_alnk0_irq", "fm1_timer5_irq",               # the audio ISR; TIMER5: the key / LED scan (hal/fm1_input.h)
         "ep4_tx", "ep4_fill",                              # (p1, main 0.9.12) TIMER5's USB audio refill, nested in the
                                                            # render (usb.c: the poll, and the packet once a ms)
         "uac_rs_run",                                      # the USB audio input at 48 kHz: the 44.1 -> 48 resampler (usb.c;
                                                            # 0.8.9: in RAM, on core 1 with CORES 2; was uac_tap48)
         "click_render",                                    # the metronome's click (click.c), while it sounds
         "mod_begin", "mod_voice", "mod_end",                 # the modulation matrix (mod.c), called when active
         "perf_begin", "perf_mute", "perf_pre", "perf_block", "perf_master", "perf_buf", "perf_glt",   # the FX layer (perform.c), when busy (snd2: its buffer effects out of perf_block, GLITCH)
         "rev_room", "rev_spring", "dist_amp", "dx_hands_step", "drw_dust",                # the reverb bus (fx.c): REVERB TYPE ROOM / SPRING
         "dly_lo",                                          # (0.8.4) the delay at half / a quarter rate (fx.c)
         "animal_render",                                   # (JIANT 0.8.3) ANIMAL (eng_animal.c): its five sources (one loop a voice)
         "string_render", "add_render",                     # (0.9.9-snd5) STRING (its loop at 1 / 2^d), ADD (nine partials)
         "formant_word_render", "formant_block",           # (JIANT 0.8.5) VOICE singing words (eng_formant.c, sing.c)
         "djf_block",                                      # (0.9.1) the master DJ FILTER (fx.c), while it is off the middle
         "eq_block",                                       # (p9) the master EQ (fx.c), while a band is off 0
         "track_comb", "tf_run", "tf_ms20"]                 # (0.9.9-snd1) the FILTER after the voices: COMB / COMB-, the SVF, MS20
# built only with FELUCCA_FM4=1 (DIGITAL, src/eng_digital.c; not in the default build, so not in BUDGET): absent,
# they are skipped; present, checked against these (their budget lines until the engine was retired in 1.0)
OPTIONAL = {"digital_render": 12, "digital_render_legacy": 333, "digital_render_custom": 558,
            "uac_rs_run": 838}          # (absent from a FELUCCA_UAC_48K=0 or FELUCCA_UAC=0 build; 0.8.9: four taps a pass, so
                                        # the static count is up, ~185 instructions an output frame run against ~230;
                                        # 0.9.9-stab: its products two taps a call, uac_mac2: 558 -> 838)
TOL = 0.10                      # exact (no noise): small edits pass, a grown render loop does not
DIV_W = 8                       # a divide weighs 1 + 8 instructions
NEST = 4                        # an instruction in a loop inside a loop weighs 4, two deep 16, ...
MAXD = 4

dis = sys.argv[1] if len(sys.argv) > 1 else "build/felucca.dis"
budget = sys.argv[2] if len(sys.argv) > 2 else "tests/target_budget.txt"
LABEL = re.compile(r"^([A-Za-z_][A-Za-z_0-9.]*):$")
INSN = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*(.*)$")
TARGET = re.compile(r"goto -?\d+ <[^>]*: ([0-9a-f]+) >")


def functions(path):
    """{name: [(addr, text)]}: a function runs to the next label not starting with '.' (the compiler's
    jump-table labels are inside functions); jump-table data ('< n : 0x.. >') is left out"""
    out, cur = {}, None
    with open(path) as f:
        for line in f:
            m = LABEL.match(line.strip())
            if m:
                name = m.group(1)
                if not name.startswith("."):
                    cur = out.setdefault(name, []) if name in FUNCS else None
                continue
            m = INSN.match(line)
            if cur is not None and m and not m.group(3).lstrip().startswith("<"):
                cur.append((int(m.group(1), 16), m.group(3).strip()))
    return out


def cost(insns):
    """loops: one span per loop head (the farthest backward branch to it); an instruction inside d
    spans weighs NEST ** (d - 1) (an inner loop runs several times per pass of the outer one)"""
    lo = insns[0][0]
    heads = {}
    for a, t in insns:
        m = TARGET.search(t)
        if m and lo <= int(m.group(1), 16) < a:
            h = int(m.group(1), 16)
            heads[h] = max(heads.get(h, a), a)
    n = divs = calls = 0
    w = 0
    for a, t in insns:
        d = min(sum(1 for h, e in heads.items() if h <= a <= e), MAXD)
        if not d:
            continue
        k = NEST ** (d - 1)
        n += 1
        if re.search(r"= r\d+ / r\d+", t):
            divs += 1
            k *= 1 + DIV_W
        calls += t.startswith("call")
        w += k
    return {"insns": len(insns), "loop": n, "div": divs, "call": calls, "cost": w}


def main():
    if not os.path.exists(dis):
        print(f"target: skip ({dis} missing: run ./build.sh)")
        return 0
    fns = functions(dis)
    res = {n: cost(fns[n]) for n in FUNCS if fns.get(n)}
    missing = [n for n in FUNCS if n not in res and n not in OPTIONAL]
    base = {}
    if os.path.exists(budget):
        for line in open(budget):
            p = line.split()
            if len(p) >= 2 and not line.startswith("#"):
                base[p[0]] = int(p[1])
    if os.environ.get("BUDGET_UPDATE"):
        with open(budget, "w") as f:
            f.write("# FELUCCA target cost budget (tests/target_budget.py): instructions in the loops of each\n"
                    f"# function in build/felucca.dis, x{NEST} per nesting level, divides x{1 + DIV_W}. The check allows "
                    f"+{TOL * 100:.0f} %.\n# Rewritten by BUDGET_UPDATE=1.\n")
            for n, r in res.items():
                if n not in OPTIONAL:
                    f.write(f"{n} {r['cost']}\n")
        print(f"target: budget {budget} rewritten ({len(res)} functions)")
    fail = 0
    for n in missing:
        print(f"target: FAIL {n} not found in {dis} (renamed? inlined? update FUNCS)")
        fail += 1
    for n, r in res.items():
        b = base.get(n, OPTIONAL.get(n))
        state = "no budget (BUDGET_UPDATE=1 adds it)" if b is None else "ok"
        if b is not None and not os.environ.get("BUDGET_UPDATE"):
            if r["cost"] > b * (1 + TOL):
                state = f"OVER BUDGET (+{(r['cost'] / b - 1) * 100:.0f} %, limit +{TOL * 100:.0f} %)"
                fail += 1
            elif r["cost"] < b * (1 - TOL):
                state = f"note: {(r['cost'] / b - 1) * 100:.0f} % (BUDGET_UPDATE=1 to keep it)"
        print(f"target: {n:15s} {r['insns']:5d} instructions, {r['loop']:4d} in loops, {r['div']} divides, "
              f"{r['call']:2d} calls there: cost {r['cost']:5d} (budget {b if b is not None else '-'}) {state}")
    return 1 if fail else 0


if __name__ == "__main__":
    sys.exit(main())
