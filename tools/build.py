#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Build Felucca: the app, the update loader and an installable .fwsc package.

  tools/build.py [--release X.Y[-suffix]]

Outputs in build/: felucca.bin (app), loader/ota.bin (update loader),
felucca.fwsc (package). A release build (--release X.Y) writes felucca-X.Y.fwsc and a folder
release-X.Y/ with the package, the app, SHA256SUMS, the sample attribution and the licence files.
See BUILDING.md for the toolchain and the SDK.

The JieLi toolchain is Linux x86-64 only. JIELI_TOOLCHAIN points at it; on
macOS (or with JIELI_DOCKER=1) each tool runs in a linux/amd64 container.
"""
import argparse
import hashlib
import os
import platform
import re
import shutil
import struct
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
FW = SRC / "firmware"
OUT = SRC / "build"
GEN = OUT / "gen"
LDR = OUT / "loader"
sys.path.insert(0, str(SRC / "tools"))
import fm1pkg_make  # noqa: E402
import lz4blk  # noqa: E402
import size_fns  # noqa: E402

APP_XIP = 0x02000120                # app.bin offset 0 in the XIP map; the SPL jumps here
APP_SLOT = fm1pkg_make.APP_SLOT
LOADER_LOAD = 0x01C0A800
POOL_END = 0x01C6D800            # (JIANT 0.6.78) app.ld: POOL's end, RAMTEXT's start (0.6.97, 0.7.0: up 4 KiB each; 0.9.9-beta4: 9 KiB lower;
                                 # 0.9.9-stab: 1 KiB lower, the 320 MHz switch's RAM code)
RAMTEXT_END = 0x01C76000         # .. RAMTEXT's end (the user stack's guard band)
LOADER_NAME = b"usb_hid_ota.bin"    # the file name the SPL looks for
DOCKER_IMAGE = os.environ.get("JIELI_DOCKER_IMAGE", "debian:bookworm-slim")
CFLAGS = ["-Os", "-ffunction-sections", "-fno-builtin", "-Wall", "-Wno-unused-function"]
LINE = re.compile(r"^\s*([0-9a-f]+):\s+((?:[0-9a-f]{2} )+)\s*\t(.*)$")

# SDK files of AC79NN_SDK_V1.2.1_2023-12-13 (the tested version)
SDK_SHA256 = {
    "uboot.boot": "4e3b4c220dc96641cb5a723f41e68ce41d5261ae9434bb33fbd7f2c59976ded4",
    "cfg_tool.bin": "276579954f076886a6a7694f65dc71c034a63a2c204b76749065c0ac7b010d1b",
    "cfg/eq_cfg_hw.bin": "41167491bffed4651750719c973d2758adeb9021a5670d02d6a53c85ed80ea7d",
}

PRODUCT = "FM-1_909"                # package identity; release builds are FM-1_9XY
VERSION = None                      # FELUCCA_VERSION for release builds (default: firmware/src/felucca.c)


def toolchain():
    tc = os.environ.get("JIELI_TOOLCHAIN")
    if not tc or not (Path(tc) / "pi32v2" / "bin" / "clang").exists():
        raise SystemExit("JIELI_TOOLCHAIN must point at the JieLi Linux toolchain "
                         "(the directory with pi32v2/ and common/; see tools/get_toolchain.sh)")
    return Path(tc).resolve()


def use_docker():
    native = platform.system() == "Linux" and platform.machine() in ("x86_64", "AMD64")
    return os.environ.get("JIELI_DOCKER", "0" if native else "1") == "1"


def tc(tool, *args):
    """run a toolchain binary (pi32v2/bin/..., common/bin/...) with cwd SRC; paths relative to SRC"""
    rel = [str(Path(a).resolve().relative_to(SRC)) if isinstance(a, Path) else a for a in args]
    if tool == "cc":                # the toolchain's cc wrapper needs python3; call clang directly
        tool, rel = "pi32v2/bin/clang", ["-target", "pi32v2", *rel]
    if use_docker():
        cmd = ["docker", "run", "--rm", "--platform", "linux/amd64", "-v", f"{SRC}:/work",
               "-v", f"{toolchain()}:/opt/jieli:ro", "-w", "/work", DOCKER_IMAGE, f"/opt/jieli/{tool}", *rel]
    else:
        cmd = [str(toolchain() / tool), *rel]
    r = subprocess.run(cmd, cwd=SRC, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stdout + r.stderr)
        raise SystemExit(f"build: {tool} failed")
    if r.stderr.strip():
        sys.stderr.write(r.stderr)
    return r.stdout


def tc_all(*cmds):
    with ThreadPoolExecutor(len(cmds)) as ex:
        return list(ex.map(lambda c: tc(*c), cmds))


def generate():
    """generated headers (UI fonts, icons, keycaps, palettes, line art shapes, tables, samples)"""
    GEN.mkdir(parents=True, exist_ok=True)
    for old in ("felucca_font.h", "felucca_icons.h"):     # headers of the bitmap font and icon atlas
        (GEN / old).unlink(missing_ok=True)
    tools = SRC / "tools"
    cmds = [[tools / "gen_aa_font.py", GEN / "ui_fonts.h", "--preset", "jiant-lab", "--kern-min", "4"],   # (snd7: pairs under 1/4 px dropped)   # (gen_aa_font.UI_PRESET),
            [tools / "gen_px_font.py", GEN / "ui_font_x.h"],
            [tools / "gen_about.py", GEN / "about_notices.h", FW / "src" / "ui_menu.c"],
            [tools / "gen_aa_icons.py", GEN / "ui_icons.h"],
            [tools / "gen_aa_keycaps.py", GEN / "ui_keycaps.h"],
            [tools / "gen_ui_palettes.py", GEN / "ui_palettes.h"],
            [tools / "gen_ui_shapes.py", GEN / "ui_shapes.h"],
            [tools / "gen_tables.py", GEN / "felucca_tables.h"],
            [tools / "gen_lyrics.py", GEN / "felucca_lyrics.h"],   # (0.8.5) VOICE's words (sing.c)
            [tools / "gen_fm6_patches.py", GEN / "felucca_fm6.h"],
            [tools / "gen_drum_maps.py", FW / "src" / "gen_tables.c", FW / "src" / "song_dice.c", GEN / "gen_drum_maps.h",
             GEN / "dice_bars.h", GEN / "gen_bass.h"]]   # (0.9.7)
    procs = [subprocess.Popen([sys.executable, *map(str, c)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True) for c in cmds]
    failed = []
    for c, p in zip(cmds, procs):
        sys.stdout.write(p.communicate()[0])
        if p.returncode:
            failed.append(c[0].name)
    if failed:
        raise SystemExit(f"build: {', '.join(failed)} failed")


# ---- update loader

def crc16(d, c=0):
    for b in d:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) if c & 0x8000 else c << 1
        c &= 0xFFFF
    return c


def ldr_head(dcrc, a, b, attr, name):
    body = struct.pack("<HIIBBH16s", dcrc, a, b, attr, 0, 0, name)
    return struct.pack("<H", crc16(body)) + body


def ldr_wrap(image):
    """ota.bin = outer JLFS head (32 B) + inner head (32 B, load address) +
    blocks [clen u32][dlen u32][LZ4 block], dlen 4096 except the last"""
    blocks = bytearray()
    for i in range(0, len(image), 4096):
        raw = image[i:i + 4096]
        c = lz4blk.compress(raw)
        if len(c) > 4096 or lz4blk.decompress(c) != raw:
            raise SystemExit(f"loader: block {i // 4096} does not compress below 4096 B")
        blocks += struct.pack("<II", len(c), len(raw)) + c
    inner = ldr_head(crc16(image), len(image), LOADER_LOAD, 0, LOADER_NAME)
    body = inner + bytes(blocks)
    return ldr_head(crc16(body), 0x20, len(body), 0x41, LOADER_NAME) + body


def build_loader():
    LDR.mkdir(parents=True, exist_ok=True)
    src = FW / "loader"
    flags = [*CFLAGS, "-Ifirmware/hal", "-Ifirmware/src"]
    tc_all(("cc", "-c", src / "crt0_ldr.S", "-o", LDR / "crt0_ldr.o"),
           ("cc", *flags, "-c", src / "loader.c", "-o", LDR / "loader.o"))
    elf = LDR / "loader.elf"
    tc("pi32v2/bin/ld", "--gc-sections", "-e", "_start", "-T", src / "loader.ld",
       LDR / "crt0_ldr.o", LDR / "loader.o", "-o", elf)
    _, dis, hdr, syms = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, LDR / "loader.bin"),
                               ("common/bin/objdump", "-d", elf),
                               ("common/bin/objdump", "-h", elf),
                               ("common/bin/objdump", "-t", elf))
    (LDR / "loader.dis").write_text(dis)
    for ln in hdr.splitlines():     # the loader rewrites the flash: nothing may run from XIP
        p = ln.split()
        if len(p) > 4 and p[1].startswith(".") and int(p[3], 16) >= 0x02000000 and int(p[2], 16):
            raise SystemExit(f"loader: section {p[1]} at {p[3]} is outside RAM")
    image = (LDR / "loader.bin").read_bytes()
    if not image or len(image) > 0x14000:
        raise SystemExit("loader: image empty or too big")
    ota = ldr_wrap(image)
    (LDR / "ota.bin").write_bytes(ota)
    bss = [int(ln.split()[0], 16) for ln in syms.splitlines() if ln.rstrip().endswith("_bss_end")]
    print(f"loader: image {len(image)} B at {LOADER_LOAD:#x}" + (f", bss end {bss[0]:#x}" if bss else ""))
    print(f"loader: ota.bin {len(ota)} B")
    return ota


# ---- app

# (JIANT 0.6.78) core 0's hottest audio code, run from RAM (app.ld .hot_text, src/core.h FELUCCA_HOTRAM): the mix, the
# master, the FX layer, the SLICER, and core 0's own copies of a part's mix and the buses (core 1 runs the flash ones)
# (0.9.9-beta4) and the buses' per-block code both cores run (fx.c fx_buses, from fx_run0 on either core): the delay's
# line (dly_lo and its helpers), the reverb models and their dispatch, the chorus's admission, the DIST's shapers; the
# I-cache (32 KiB, shared by both cores and the UI) kept for the engines (Kirill's audit, LOOM-8)
HOT_FNS = ("mix_block", "mix_part_m0", "fx_run0", "perf_begin", "perf_pre", "perf_block", "perf_master", "slicer_bus",
           "dly_lo", "dly_grain", "dly_grain_adv", "dly_wow_adv", "dly_line_clear", "dly_rate", "fx_rest",
           "rev_run", "rev_idle", "rev_room", "rev_room_mod", "rev_spring", "rev_spring_side", "rev_cloud", "rev_shimmer",
           "rev_reso", "cho_admit", "dist_shape", "dist_amp")


def hot_sections(ir):
    """HOT_FNS' definitions in the optimised IR given section ".hot_text.<name>"; (the IR, how many found)"""
    found = 0

    def put(m):
        nonlocal found
        found += 1
        return f'{m.group(1)} section ".hot_text.{m.group(2)}" {{'
    ir = re.sub(r"^(define [^\n]*?@(" + "|".join(HOT_FNS) + r")\([^\n]*?)\s*\{$", put, ir, flags=re.M)
    return ir, found


# (0.9.9-snd3) the flash layout of the sound, pinned: from 0.9.9 to 0.9.10 +6 KB of UI code moved every engine's render
# 6 KB further through the XIP flash's small shared cache (none of the sound changed) and the owner's FM-1 went from fine to
# 94 % load and crackling. tools/audio_layout.txt keeps the order of every code section in .text and the address of what
# an interrupt reaches (tools/size_fns.py isr_reach: the audio ISR, TIMER5, the second core's jobs, the tables they read):
# each run of the sound's code starts at its recorded address ('@', the linker script build/layout.ld places it there),
# every audio function ('a') and constant table ('r') is checked at its address. Code the UI loses leaves a hole before
# the next run: build.py fills it with the UI's constant tables ('n': fonts, icons, ...; moved out of .rodata into
# .rodata.n.<name>), so the image shrinks while the sound stays where it was. FELUCCA_REPIN=1 records the layout anew
# (FELUCCA_REPIN=keep: in the order recorded before, V10.14)
# (after a change to the sound's code, which moves what follows it in its run)
LAYOUT = SRC / "tools" / "audio_layout.txt"
LAYOUT_LD = OUT / "layout.ld"


def map_inputs(text):
    """the linker map's input sections: [(section, address, size, object)]"""
    out, pend = [], None
    for ln in text.splitlines():
        if pend:
            m = re.match(r"^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)$", ln)
            if m:
                out.append((pend, int(m[1], 16), int(m[2], 16), m[3]))
                pend = None
                continue
            pend = None
        m = re.match(r"^ (\.\S+)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S+)$", ln)
        if m:
            out.append((m[1], int(m[2], 16), int(m[3], 16), m[4]))
        else:
            m = re.match(r"^ (\.\S+)$", ln)
            pend = m[1] if m else None
    return out


def sym_table(syms):
    """objdump -t: {name: address} of the symbols in .text"""
    out = {}
    for ln in syms.splitlines():
        p = ln.split()
        if len(p) >= 4 and p[-3] == ".text" and re.fullmatch(r"[0-9a-f]{8}", p[0]):
            out.setdefault(p[-1], int(p[0], 16))
    return out


def layout_write(lines):
    LAYOUT.write_text("# tools/build.py (FELUCCA_REPIN=1): the order of the code in .text and where the sound sits\n"
                      "# offset in the image, @ a run of the sound's code pinned there, a its code, r its tables, c its merged\n"
                      "# constants (all checked), - the UI's code (order only), n the UI's tables (moved to fill the holes)\n"
                      + "\n".join(lines) + "\n")


def layout_read():
    rows = []
    for ln in LAYOUT.read_text().splitlines():
        if ln and not ln.startswith("#"):
            off, flag, name = ln.split()
            rows.append((int(off, 16), flag, name))
    return rows


def layout_record(mapt, syms, reach):
    """tools/audio_layout.txt from a link in the compiler's order"""
    ins = map_inputs(mapt)
    text = [(a, n, o) for n, a, sz, o in ins if n.startswith(".text.") and n != ".text.crt0" and APP_XIP <= a < 0x02100000
            and not o.endswith("bt_seg.o")]     # (V10.12: the Bluetooth segment is not in the image)
    rodata = [(a, sz) for n, a, sz, o in ins if (n == ".rodata" or n.startswith(".rodata.cst")) and o.endswith("felucca.o")]
    lines, prev = [], False
    for a, n, o in text:
        audio = n[6:] in reach or not o.endswith("felucca.o")     # (the ISR entries of hal/*.S: the sound's too)
        lines.append(f"{a - APP_XIP:06x} {'@' if audio and not prev else 'a' if audio else '-'} {n[6:]}")
        prev = audio
    for name, a in sorted(sym_table(syms).items(), key=lambda kv: kv[1]):
        if any(lo <= a < lo + sz for lo, sz in rodata) and not name.startswith("."):
            cst = not (rodata[0][0] <= a < rodata[0][0] + rodata[0][1])
            if name in reach or not cst:            # (the UI's small merged constants stay merged)
                lines.append(f"{a - APP_XIP:06x} {'c' if cst else 'r' if name in reach else 'n'} {name}")
    layout_write(lines)
    print(f"layout: recorded {len(text)} code sections, {sum(1 for x in lines if ' @ ' in x)} runs of the sound pinned")


# (0.9.9-gen1) main-loop code placed after the sound (never in the holes before its pinned runs): the song generator's
# functions (firmware/src/gen.c), so it can change size without moving the sound and without the UI's room before it.
# Only UI code ('-' rows): they leave the recorded order and land with the code not in the layout, past the tables
LATE_SRC = ("gen.c", "favorites.c", "gen_track.c")   # movable main-loop generators


def late_fns():
    # REC arming is UI-only; keep its undo checkpoint out of the pinned DSP gaps.
    names = {"rec_tap", "ly_keyname", "ag", "text_w", "gls_prep",
             # Only the overflowing UI gaps move: keep all audio addresses pinned without wasting large holes.
             "fm1_irq_on", "large_kind", "draw_confirm", "vx_analog", "toast_clear", "draw_text_box", "as_mix",
             "ed_bk_r32", "set_engine", "step_lanes", "song_copy_section", "palette_from_stored",
             "proj_steps", "splash_sig", "up_name_ok", "song_idx_touch", "song_end_row"}
    for f in LATE_SRC:
        names |= set(re.findall(r"^static [^;=()]*?\b(\w+)\((?![^)]*\)\s*;)", (FW / "src" / f).read_text(), flags=re.M))
    return names


def late_tables():
    """the generator's own constant tables (gen.c, gen_tables.c): out of .rodata like the UI's recorded ones ('n'), so a
    new one never moves the sound's merged constants after .rodata"""
    names = set()
    for f in ("gen.c", "gen_tables.c", "eng_string.c", "eng_add.c", "ui_engvis.c"):   # (0.9.9-snd5: the two engines' own
        # tables and descriptors, the creatures' new ones: out of .rodata too, the sound's tables where they were)
        txt = (FW / "src" / f).read_text()
        names |= set(re.findall(r"^static const [^=;(]*?\b(\w+)\s*\[", txt, flags=re.M))
        names |= set(re.findall(r"^static const engine_t (\w+) =", txt, flags=re.M))
    return names


def layout_ld(rows, fills=None):
    """build/layout.ld: the code sections in the recorded order, each run of the sound at its address; fills: {run: [n
    sections]} put in the hole before it"""
    out, k, late = [], 0, late_fns()
    for off, flag, name in rows:
        if flag == "@":
            out += [f"*(.text.{f[1:]})" if f.startswith("@") else f"*(.rodata.n.{f})"
                    for f in (fills or {}).get(k, [])]
            out += [f"_lay_gap{k} = .;", f". = {off:#x};"]
            k += 1
        if flag in "@a" or (flag == "-" and name not in late):
            out.append(f"*(.text.{name})")
    LAYOUT_LD.write_text("\n".join(out) + "\n")


def layout_fills(rows, symt, ins, aligns, reach=()):
    """the holes before the pinned runs filled with the UI's tables, largest first (best fit)"""
    pins = [off for off, flag, _ in rows if flag == "@"]
    holes = [(k, symt[f"_lay_gap{k}"], APP_XIP + off) for k, off in enumerate(pins) if f"_lay_gap{k}" in symt]
    items = sorted(((sz, n[10:]) for n, a, sz, o in ins if n.startswith(".rodata.n.")), reverse=True)
    recorded = {name for _, flag, name in rows if flag in "@a-"}
    cold = size_fns.size_names() - set(reach)
    # Main-loop functions omitted from the recorded order can use stranded
    # gaps too. Code read by an ISR, hot RAM and fixed audio tables never enter
    # this candidate set. A four-byte boundary satisfies pi32v2 text alignment.
    late_code = late_fns()
    for section, address, size, obj in ins:
        name = section[6:] if section.startswith(".text.") else ""
        if name in cold and (name not in recorded or name in late_code) and obj.endswith("felucca.o"):
            key = "@" + name
            items.append((size, key)); aligns[key] = 4
    items.sort(reverse=True)
    fills, used, lost = {}, set(), 0
    for k, lo, hi in holes:
        pos = lo
        for sz, name in items:
            if name in used:
                continue
            al = aligns.get(name, 4)
            at = -(-pos // al) * al
            if at + sz <= hi:
                fills.setdefault(k, []).append(name)
                used.add(name)
                pos = at + sz
        lost += hi - pos
    return fills, sum(hi - lo for _, lo, hi in holes), lost


def layout_room(rows, objs):
    """Report all grown pinned gaps before linking, without changing any addresses or guards."""
    headers = tc("common/bin/objdump", "-h", *objs)
    sections = {name: int(size, 16) for name, size in re.findall(
        r"^\s*\d+\s+(\.text\.\S+)\s+([0-9a-fA-F]+)\s+", headers, re.M)}
    pos, ui, late = 0, [], late_fns()
    for off, flag, name in rows:
        if flag == "@":
            if pos > off:
                print(f"layout gap: {name} exceeds pinned address by {pos - off} B; preceding UI: {', '.join(ui)}", flush=True)
            pos, ui = off, []
        if flag in "@a" or (flag == "-" and name not in late):
            pos = (pos + 1) & ~1
            pos += sections.get(".text." + name, 0)
            if flag == "-" and ".text." + name in sections:
                ui.append(name)


def layout_check(rows, mapt, symt):
    sect = {n[6:]: a for n, a, sz, o in map_inputs(mapt) if n.startswith(".text.")}
    moved = []
    for off, flag, name in rows:
        at = sect.get(name) if flag in "@a" else symt.get(name) if flag in "rc" else None
        if at is not None and at != APP_XIP + off:
            moved.append(f"{name} {APP_XIP + off:#x} -> {at:#x}")
    return moved


def build_app():
    flags = [*CFLAGS, "-Ifirmware/hal", "-Ifirmware/src", "-Ibuild/gen"]
    for flag in ("FELUCCA_FLASH", "FELUCCA_OTA", "FELUCCA_OTA_DRYRUN", "FELUCCA_OTA_RAMONLY", "FELUCCA_CDC", "FELUCCA_CONSOLE_VERBOSE",
                 "FELUCCA_UART", "FELUCCA_UAC", "FELUCCA_UAC_TONE", "FELUCCA_UAC_48K", "FELUCCA_ICONS", "FELUCCA_SLICE", "FELUCCA_FM4",
                 "FELUCCA_CDC_DEFAULT", "FELUCCA_BT"):
        v = os.environ.get(flag)    # unset: the default in firmware/src/felucca.c
        if v in ("0", "1"):
            flags.append(f"-D{flag}={v}")
    v = os.environ.get("FELUCCA_USB_LAYOUT")   # firmware/src/usb.c: how the USB composite presents itself (#67), 0..3
    if v in ("0", "1", "2", "3"):
        flags.append(f"-DFELUCCA_USB_LAYOUT={v}")
    flags.append(f'-DFELUCCA_ID="{PRODUCT}"')
    if VERSION:
        flags.append(f'-DFELUCCA_VERSION="{VERSION}"')
    # felucca.c goes to LLVM IR without the optimizer, the main-loop functions (UI, stores, editor) are
    # marked minsize (tools/size_fns.py), then the IR is compiled at -Os. FELUCCA_SIZE=0: -Os everywhere
    size = os.environ.get("FELUCCA_SIZE") != "0"
    hot = size and os.environ.get("FELUCCA_HOTRAM") != "0"   # (0.6.78) core 0's hottest audio code in RAM (HOT_FNS)
    flags.append(f"-DFELUCCA_HOTRAM={int(hot)}")
    cmain = (("cc", *flags, "-S", "-emit-llvm", "-Xclang", "-disable-llvm-optzns", "-c",
              FW / "src" / "felucca.c", "-o", OUT / "felucca.ll") if size else
             ("cc", *flags, "-c", FW / "src" / "felucca.c", "-o", OUT / "felucca.o"))
    tc_all(("cc", "-c", FW / "crt0.S", "-o", OUT / "crt0.o"),
           ("cc", "-c", FW / "hal" / "fm1_vec.S", "-o", OUT / "fm1_vec.o"),
           ("cc", "-c", FW / "hal" / "fm1_isr.S", "-o", OUT / "fm1_isr.o"),
           # (V10.12) the Bluetooth segment, a unit of its own at -Os (app.ld BTSEG: flash 0xEA000, outside the slot)
           ("cc", *CFLAGS, "-fdata-sections", "-Wno-undefined-internal", "-Ifirmware/hal", "-Ifirmware/bt/ble", "-c",
            FW / "bt" / "bt_seg.c", "-o", OUT / "bt_seg.o"),
           cmain)
    if size:
        subprocess.run([sys.executable, SRC / "tools" / "size_fns.py", OUT / "felucca.ll", OUT / "felucca_size.ll"],
                       check=True)
        cf = [f for f in flags if not f.startswith(("-I", "-D", "-W"))]
        if hot:   # (JIANT 0.6.78) optimised first, then HOT_FNS put in .hot_text, then only the code generator: a
            # section attribute before the optimiser would stop it inlining anything into them (this clang inlines
            # nothing across sections), so they come out as they were, only in RAM (app.ld)
            tc("cc", *cf, "-S", "-emit-llvm", "-c", OUT / "felucca_size.ll", "-o", OUT / "felucca_opt.ll")
            ir, n = hot_sections((OUT / "felucca_opt.ll").read_text())
            if n != len(HOT_FNS):
                raise SystemExit(f"hot RAM code: {n} of the {len(HOT_FNS)} functions found in the IR ({HOT_FNS})")
            hot_ir = ir
        else:
            tc("cc", *cf, "-c", OUT / "felucca_size.ll", "-o", OUT / "felucca.o")
    elf = OUT / "felucca.elf"
    objs = (OUT / "crt0.o", OUT / "fm1_vec.o", OUT / "fm1_isr.o", OUT / "felucca.o", OUT / "bt_seg.o")

    def link(probe=False):
        """probe (0.9.9-gen1): the holes measured with the code moved past the sound (LATE_SRC) before the UI's tables
        fill them, in an XIP region 1 MiB larger (the image would not fit with the holes still empty)"""
        ld = FW / "app.ld"
        if probe:
            ld = OUT / "app_probe.ld"
            ld.write_text(re.sub(r"(XIP\s+\(rx\)\s*:\s*ORIGIN = 0x[0-9A-Fa-f]+, LENGTH = )(0x[0-9A-Fa-f]+)",
                                 lambda m: m[1] + hex(int(m[2], 16) + 0x100000), (FW / "app.ld").read_text()))
        tc("pi32v2/bin/ld", "-T", ld, *objs, "-o", elf, "-Map", OUT / "felucca.map")
        return (OUT / "felucca.map").read_text(), tc("common/bin/objdump", "-t", elf)

    def compile_hot(rows):
        """the hot IR, the UI's tables of the layout ('n') in sections of their own; {table: alignment}"""
        tables, aligns = {name for _, flag, name in rows if flag == "n"}, {}
        late = late_tables() - tables

        def ir_bytes(ty):
            """[N x iK] / [N x [M x iK]]: its size in bytes (None: another type)"""
            m = re.fullmatch(r"\[(\d+) x (.+)\]", ty.strip())
            if not m:
                k = re.fullmatch(r"i(8|16|32|64)", ty.strip())
                return int(k[1]) // 8 if k else None
            inner = ir_bytes(m[2])
            return int(m[1]) * inner if inner else None

        def const(m):
            if ' section "' in m.group(0):
                return m.group(0)
            if m.group(2) not in tables:
                # (0.9.9-gen1) a generator table the layout has not recorded: out of .rodata too, unless the
                # compiler merges it with the small constants (4 8 16 32 B: the sound's are checked after them)
                ty = re.search(r" constant (\[[^\]]*(?:\][^\]]*)*?\]|i\d+) ", m.group(1) + " ")
                # Long UI-only strings also fill otherwise stranded gaps. The
                # ISR/global closure and every recorded audio table stay put;
                # explicit sections (including BTAPP) were excluded above.
                ui_string = (m[2].startswith(".str") and m[2] not in reach and ty and
                             re.fullmatch(r"\[\d+ x i8\]", ty[1]) and ir_bytes(ty[1]) >= 24)
                ui_string |= m[2].startswith("AF_") and m[2].endswith(("_PACK", "_HC")) and m[2] not in reach
                if not ui_string and (m.group(2) not in late or (ty and ir_bytes(ty[1]) in (4, 8, 16, 32))):   # (snd5: a struct's too)
                    return m.group(0)
            aligns[m.group(2)] = int(m.group(4))
            return f'{m.group(1)}, section ".rodata.n.{m.group(2)}"{m.group(3)}'
        ir = re.sub(r'^(@"?([\w.]+)"? = [^\n]*? constant [^\n]*?)(, align (\d+))$', const, hot_ir, flags=re.M)
        (OUT / "felucca_hot.ll").write_text(ir)
        tc("cc", *cf, "-Xclang", "-disable-llvm-optzns", "-c", OUT / "felucca_hot.ll", "-o", OUT / "felucca.o")
        return aligns
    if not hot:
        LAYOUT_LD.write_text("*(.text*)\n")            # (FELUCCA_SIZE=0 / FELUCCA_HOTRAM=0: the compiler's order)
        link()
    else:
        reach = size_fns.isr_reach((OUT / "felucca_size.ll").read_text(), True)
        repin = os.environ.get("FELUCCA_REPIN")
        if repin in ("1", "keep") or not LAYOUT.exists():
            # (V10.14) FELUCCA_REPIN=keep: the code in the order recorded before (new functions after it), only the
            # addresses recorded anew: the sound moves by what grew before it, not by the compiler's new order (=1: the
            # compiler's order, which a change elsewhere can reshuffle by tens of KB)
            keep = [n for _, f, n in layout_read() if f in "@a-"] if repin == "keep" and LAYOUT.exists() else []
            if keep and (FW / "src" / "seq_fit.c").exists():
                # FIT's new out-of-line clock is called from RAM audio code: keep it near seq_step,
                # not among new functions after the tables, beyond pi32v2's short-call reach.
                clocks = ["fit_valid", "fit_value_span", "fit_span", "fit_step_samples", "step_samples"]
                keep = [name for name in keep if name not in clocks]
                at = keep.index("seq_step") if "seq_step" in keep else 0
                keep[at:at] = clocks
            compile_hot([])
            LAYOUT_LD.write_text("".join(f"*(.text.{n})\n" for n in keep) + "*(.text*)\n")
            # This preliminary link has not packed UI tables into the audio
            # layout's holes yet. Use the same enlarged probe as below; the
            # final link and package budgets still enforce the real app slot.
            mapt, syms = link(probe=True)
            layout_record(mapt, syms, reach)
            rows = layout_read()          # (the merged constants' place once the UI's tables have left .rodata)
            compile_hot(rows)
            layout_ld(rows)
            symt = sym_table(link(probe=True)[1])   # (p1) the UI's tables not yet in the holes: in the larger region
            # (V10.14) the sound's tables ('r') too, as they sit once the UI's have left .rodata: the compiler's order
            # may put a UI table ('n') before one of them (V10.14: HALO_X / HALO_Y before FM1_KEYMAP), which then moved
            layout_write([f"{(symt[n] - APP_XIP) if f in 'cr' and n in symt else o:06x} {f} {n}" for o, f, n in rows])
        rows = layout_read()
        aligns = compile_hot(rows)
        layout_room(rows, objs)
        layout_ld(rows)
        mapt, syms = link(probe=True)
        fills, hole, lost = layout_fills(rows, sym_table(syms), map_inputs(mapt), aligns, reach)
        print(f"layout packing: {hole} B gaps, {lost} B padding, {sum(map(len, fills.values()))} UI tables/strings packed", flush=True)
        print("UI string candidates:", sum(1 for n in aligns if n.startswith(".str")), flush=True)
        layout_ld(rows, fills)
        mapt, syms = link()
        moved = layout_check(rows, mapt, sym_table(syms))
        new = [n for n in reach if f".text.{n}" in {s[0] for s in map_inputs(mapt)}
               and n not in {name for _, _, name in rows}]
        print(f"layout: {sum(1 for r in rows if r[1] == '@')} runs of the sound pinned, "
              f"{sum(1 for r in rows if r[1] in '@a')} functions and {sum(1 for r in rows if r[1] in 'rc')} tables recorded; "
              f"holes {hole} B, {sum(map(len, fills.values()))} UI tables moved into them, {lost} B left as padding")
        if moved:
            raise SystemExit("layout: the sound's code or tables moved (FELUCCA_REPIN=1 records the layout anew, only "
                             "after a change to the sound itself): " + "; ".join(moved[:6]) + (" ..." if len(moved) > 6 else ""))
        if new:
            print(f"  note  sound functions not in the layout (placed after the tables; FELUCCA_REPIN=1): {sorted(new)[:6]}")
    for sect in ("text.bin", "data.bin", "ramtext.bin", "c1text.bin", "hottext.bin", "bt.bin"):
        (OUT / sect).unlink(missing_ok=True)
    *_, syms, dis, rt = tc_all(("common/bin/objcopy", "-O", "binary", "-j", ".bt", elf, OUT / "bt.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".text", elf, OUT / "text.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".data", elf, OUT / "data.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".ram_text", elf, OUT / "ramtext.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".c1_text", elf, OUT / "c1text.bin"),
                               ("common/bin/objcopy", "-O", "binary", "-j", ".hot_text", elf, OUT / "hottext.bin"),
                               ("common/bin/objdump", "-t", elf),
                               ("common/bin/objdump", "-d", elf),
                               ("common/bin/objdump", "-d", "-j", ".ram_text", elf))
    (OUT / "felucca.dis").write_text(dis)

    def symv(name):
        return int(re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M).group(1), 16)
    img = bytearray((OUT / "text.bin").read_bytes())
    # .ram_text, .c1_text (0.6.71: the second core's loop), .hot_text (0.6.78: core 0's hot audio code) and .data follow .text at their load addresses; crt0 copies
    # them by words
    # (V10.6z5) packed: app.ld links their load images outside the flash (PACKED); here one LZ4 block each
    # (tools/lz4blk.py compress_small), one after the other from _etext on (the end of .text), in this order; main.c
    # fm1_cstart unpacks them into their sections (firmware/src/boot_lz4.h). Each block is unpacked here as boot_lz4
    # does it and must give its section back byte for byte
    load = symv("_etext")
    if load % 4 or load - APP_XIP != len(img):
        raise SystemExit(f"_etext {load:#x}: not word aligned or not the end of .text")
    stream, raw_total = bytearray(), 0
    for sect, lo, hi in (("ramtext.bin", "_rt_start", "_rt_end"), ("c1text.bin", "_c1t_start", "_c1t_end"),
                         ("hottext.bin", "_hot_start", "_hot_end"), ("data.bin", "_data_start", "_data_end")):
        blob = (OUT / sect).read_bytes() if (OUT / sect).exists() else b""
        if len(blob) != symv(hi) - symv(lo):
            raise SystemExit(f"{sect}: {len(blob)} B, its section {lo}..{hi} {symv(hi) - symv(lo)} B")
        at = len(stream)
        stream += lz4blk.compress_small(blob) if blob else b""
        back, end = lz4blk.boot_unpack(bytes(stream), at, len(blob))
        if back != blob or end != len(stream):
            raise SystemExit(f"{sect}: its LZ4 block does not unpack to it (boot_lz4)")
        raw_total += len(blob)
    (OUT / "boot_lz4.bin").write_bytes(stream)   # (tests/boot_lz4_test.c unpacks it with boot_lz4.h)
    img += stream
    print(f"boot: RAM code and data {raw_total} B packed to {len(stream)} B (LZ4, unpacked by fm1_cstart)")
    img += b"\xff" * (-len(img) % 4)
    bt_segment(img, symv)
    (OUT / "felucca.bin").write_bytes(img)
    return bytes(img), syms, dis, rt


# (V10.12) the Bluetooth segment (firmware/bt/bt_seg.c, app.ld BTSEG): what the update loader writes at 0xEA000
BT_BASE, BT_END, BT_HEAD, BT_MAGIC, BT_WANT = 0xEA000, 0xFC000, 32, 0x3154424A, 0x544E5742


def bt_segment(img, symv):
    """build/bt.bin = the header (magic, payload length, CRC-32) + the payload as linked; the same length and CRC-32
    patched into the app's bt_want (src/bt_hw.c), so the app calls only the segment of its own build"""
    import zlib
    seg = bytearray((OUT / "bt.bin").read_bytes())
    if len(seg) <= BT_HEAD or struct.unpack_from("<I", seg)[0] != BT_MAGIC:
        raise SystemExit("bt: the segment's header (bt_seg.c bt_head) is not first in it")
    if symv("_bt_start") != fm1pkg_make.FM1_XIP(BT_BASE) or len(seg) > BT_END - BT_BASE:
        raise SystemExit(f"bt: the segment at {symv('_bt_start'):#x}, {len(seg)} B: not at flash {BT_BASE:#x} or too big")
    hdr = tc("common/bin/objdump", "-h", OUT / "bt_seg.o")
    for ln in hdr.splitlines():                 # its RAM is zeroed (pool / .bss): an initialised object would cost the
        p = ln.split()                          # app's flash for its load image
        if len(p) > 3 and p[1].startswith((".data", ".sdata")) and int(p[2], 16):
            raise SystemExit(f"bt: initialised data in the segment ({p[1]}): make it const or zero")
    payload = bytes(seg[BT_HEAD:])
    crc = zlib.crc32(payload) & 0xFFFFFFFF
    struct.pack_into("<II", seg, 4, len(payload), crc)
    at = symv("bt_want") - APP_XIP
    if struct.unpack_from("<I", img, at)[0] != BT_WANT:
        raise SystemExit("bt_want: no BWNT marker in the image")
    struct.pack_into("<II", img, at + 4, len(payload), crc)
    fm1pkg_make.bt_check(bytes(seg))
    (OUT / "bt.bin").write_bytes(seg)
    print(f"bt: segment {len(seg)} B at {BT_BASE:#x} ({-(-len(seg) // 4096)} of {(BT_END - BT_BASE) // 4096} sectors), "
          f"crc {crc:08x} -> build/bt.bin")


# (0.9.9-stab) felucca-bsp docs/CPU.md: on the AC79 a 32 x 32 -> 64-bit multiply inlined beside loads (most of all 16-bit
# ones) now and then leaves a register a bit off; src/core.h's helpers (mul64 ...) hold the audio path's. These keep theirs:
# once a block or a frame, 64 x 64-bit products or the UI (the SDK's own 64-bit printf digits are the same case: not hot)
MUL64_SKIP = {"djf_coef", "draw_graph", "graph_adsr", "vx_arc", "fm1_main", "fugue_tick", "midi_clock_pulse", "seq_step"}


def mul64_near_loads(dis, window=4):
    """(0.9.9-stab, after fm1-base's tools/build.py, MPL-2.0) functions with a load in flight beside a 32x32->64-bit multiply
    (rN_rM = rA * rB): one up to two instructions before it, or one after it before its product is used (using it waits
    for the multiply). On the FM-1 such code now and then leaves a register a bit off, most of all beside 16-bit loads
    (felucca-bsp docs/CPU.md); a multiply in a helper of its own (noinline, src/core.h mul64) is clear of it.
    [(function, sites, the nearest load's distance in instructions, 1 = a 16-bit load among them)]"""
    found, fn, body = [], "", []

    def scan():
        sites, near, half = 0, 99, 0
        for i, ins in enumerate(body):
            m = re.match(r"(r\d+)_(r\d+) [+-]?= r\d+ \* r\d+", ins)
            if not m:
                continue
            dst, hit = {m.group(1), m.group(2)}, 0
            for d, b in enumerate(reversed(body[max(0, i - 2):i]), 1):
                if re.search(r"\bcall\b", b):
                    break
                if re.search(r"= [hbd]?\[", b):
                    hit, near = 1, min(near, d)
                    half |= b.split("=", 1)[1].lstrip().startswith("h[")
            for d, b in enumerate(body[i + 1:i + 1 + window], 1):
                if any(re.search(r"\b" + r + r"\b", b) for r in dst):
                    break                       # the product used: the multiply has finished
                if re.search(r"= [hbd]?\[", b):
                    hit, near = 1, min(near, d)
                    half |= b.split("=", 1)[1].lstrip().startswith("h[")
            sites += hit
        return sites, near, half

    for ln in dis.splitlines() + ["end:"]:
        mm = LINE.match(ln)
        if not mm:
            m2 = re.match(r"^([A-Za-z_$][\w.$]*):$", ln.strip())
            if m2:                              # (a label starting with '.': a jump table's piece, still the function)
                if fn and not fn.startswith("__") and fn.split(".")[0] not in MUL64_SKIP:
                    n, near, half = scan()
                    if n:
                        found.append((fn, n, near, half))
                fn, body = m2.group(1), []
            continue
        body.append(mm.group(3))
    return found


SOFT_DOUBLE = ("__adddf3", "__subdf3", "__muldf3", "__divdf3", "__floatsidf", "__floatunsidf", "__fixdfsi",
               "__fixunsdfsi", "__extendsfdf2", "__truncdfsf2", "__eqdf2", "__nedf2", "__gtdf2", "__gedf2", "__ltdf2", "__ledf2")


def check(img, syms, dis, rt):
    errors, notes = [], []
    mul64 = mul64_near_loads(dis)       # (0.9.9-stab) the CPU fault of felucca-bsp docs/CPU.md: a note, the code may be fine
    if mul64:
        notes.append("64-bit multiplies inlined beside loads (felucca-bsp docs/CPU.md: may corrupt a register; move them "
                     "into src/core.h's mul64 helpers): " +
                     ", ".join(f"{f} ({n} site{'s' if n > 1 else ''}, a load {d} insn{'s' if d > 1 else ''} away{', 16-bit' if h else ''})"
                               for f, n, d, h in mul64[:8]) + (" ..." if len(mul64) > 8 else ""))
    else:
        notes.append("no 64-bit multiply inlined beside a load (src/core.h mul64 helpers; felucca-bsp docs/CPU.md)")
    dbl = [n for n in SOFT_DOUBLE if re.search(r"\s" + n + r"$", syms, re.M)]
    if dbl:                             # (0.9.9-stab) the pi32v2 has no double: a soft one is a slow mistake (fm1-base's check)
        errors.append(f"soft-double routines linked (a double crept in): {dbl}")
    else:
        notes.append("no soft-double routine linked")
    m = re.search(r"^([0-9a-f]+) .*\s_start$", syms, re.M)
    if not m or int(m.group(1), 16) != APP_XIP:
        errors.append(f"_start is not at {APP_XIP:#x}")
    if img[:4] != bytes.fromhex("04818000"):
        errors.append(f"image starts with {img[:4].hex()}, not the entry stub")
    if not re.search(r"^[0-9a-f]+ .*\s\.ram_text\s.*\sfm1_rf_burst_run$", syms, re.M):
        errors.append("bt: the radio's crystal ramp (hal/fm1_ble_rf.h fm1_rf_burst_run) is not in .ram_text: a flash "
                      "fetch in it hangs the FM-1 (Melodee's finding)")
    rt_calls = [ln for ln in rt.splitlines() if re.search(r"\bcall\b", ln)]
    if rt_calls:                    # RAM code runs with the flash off: no calls into XIP
        errors.append(f".ram_text contains calls: {rt_calls[:3]}")
    else:
        notes.append(f".ram_text: {len([ln for ln in rt.splitlines() if LINE.match(ln)])} insns, no calls")
    for ln in dis.splitlines():     # nothing may call or load an address in the chip ROM
        mm = LINE.match(ln)
        if not mm:
            continue
        for v in re.findall(r"= (-?\d+) <|call -?\d+ <[^:>]*: ([0-9a-f]+) >", mm.group(3)):
            val = (int(v[0]) & 0xFFFFFFFF) if v[0] else int(v[1], 16) & 0xFFFFFFFF
            if 0xFFC00000 <= val < 0xFFD00000:
                errors.append(f"reference to ROM address {val:#010x}")
    if len(img) > APP_SLOT:
        errors.append(f"image {len(img)} B exceeds the app slot")
    ld = re.search(r"XIP\s*\(rx\)\s*:\s*ORIGIN = (0x[0-9A-Fa-f]+), LENGTH = (0x[0-9A-Fa-f]+)", (FW / "app.ld").read_text())
    if not ld or int(ld[1], 16) != APP_XIP or int(ld[2], 16) != APP_SLOT:
        errors.append(f"app.ld XIP must be ORIGIN {APP_XIP:#x}, LENGTH {APP_SLOT:#x} (fm1pkg_make.APP_SLOT)")

    def sym(name):
        mm = re.search(r"^([0-9a-f]+) .*\s" + name + r"$", syms, re.M)
        return int(mm.group(1), 16) if mm else 0
    bss = sym("_bss_end") - 0x01C08000
    pool = sym("_pool_end") - sym("_pool_start")
    pool_top = max(sym("_pool_end"), sym("_c1_stack_hi"))   # (the second core's stacks after the pool)
    notes.append(f"image {len(img)} B of the {APP_SLOT} B slot ({APP_SLOT - len(img)} B free); RAM .data+.bss {bss} B of 98304; pool {pool} B, spare {POOL_END - pool_top} B")
    notes.append(f"LOWBUF {sym('_low_end') - sym('_low_start')} B of 32768 B (linker-enforced)")
    if bss > 96 * 1024:
        errors.append("RAM region overflow")
    if POOL_END - pool_top < 8192:                # keep >= 8 KiB of the pool spare
        errors.append(f"pool headroom {POOL_END - pool_top} B < 8192 B")
    # (JIANT 0.6.78) the RAM code at the pool's top: a call reaches +-4 MiB (app.ld), so from the lowest RAM code
    # address the XIP image must end within 4 MiB; and the room left for RAM code
    # (0.7.5) what must be in reach is what RAM code calls, not the whole image: the calls in .ram_text (none),
    # .c1_text and .hot_text, from the disassembly (`call <rel> <name : addr>`); the sound code they call comes early
    # in the image (the TU's order: engines, voice, fx before the UI), so the UI can grow past the old whole-image
    # bound (the image's end in reach of the lowest RAM code: 12 KiB short by 0.7.5, after RAMTEXT moved up twice)
    targets, sect = [], None
    for ln in dis.splitlines():
        mm = re.match(r"Disassembly of section (\S+):", ln)
        if mm:
            sect = mm.group(1)
            continue
        if sect in (".ram_text", ".c1_text", ".hot_text"):
            mm = re.search(r"\bcall\b[^<]*<([\w.]+)\s*:\s*([0-9a-f]+)\s*>", ln)
            if mm and int(mm.group(2), 16) >= APP_XIP:
                targets.append((int(mm.group(2), 16), mm.group(1)))
    far = max(targets) if targets else (APP_XIP, "-")
    reach = sym("_rt_start") + 0x400000 - far[0]
    ramcode = sym("_hot_end") - sym("_rt_start")
    notes.append(f"RAM code {ramcode} B of {RAMTEXT_END - sym('_rt_start')}; {len(targets)} calls into the image, the farthest "
                 f"{far[1]} with {reach} B of reach to spare (the image's end: {sym('_rt_start') + 0x400000 - sym('_etext')} B)")
    if not targets and "call" in dis:
        errors.append("no call from RAM code found in the disassembly: the reach check cannot read it")
    if reach < 12288:                              # (12 KiB to spare beyond the farthest callee)
        errors.append(f"RAM code's farthest call ({far[1]}) has only {reach} B of reach to spare (< 12 KiB): "
                      "move RAMTEXT up (app.ld) or that code earlier in the image")
    if RAMTEXT_END - sym("_hot_end") < 2048:
        errors.append(f"RAM code headroom {RAMTEXT_END - sym('_hot_end')} B < 2048 B")
    return errors, notes


# Every hardware register access lives in hal/. In src/ and loader/ (comments stripped):
#  - no volatile pointer cast, except of a C object's address (`*(volatile T *)&x`: a read-once
#    of a RAM flag shared with an ISR, e.g. usb.c ota_wire_send);
#  - no literal in a register or reserved window (core SFRs 0x10000-0x13FFF, SFC 0x40000-0x43FFF,
#    GPIO/IOMAP 0x50000-0x51FFF, CPU 0x1EE0000-0x1EEFFFF, RAM top 0x01C7F000- (boot info,
#    mailbox, vectors), XIP 0x02000000-0x020FFFFF);
#  - no inline asm, except the empty compiler barrier RING_PUBLISH() (emits no instruction);
#  - no HAL register macro (a hal/ #define that is, or expands to, a volatile access).
# Linker symbols (_bss_start[], _rt_load[], ...) are plain C objects and pass.
MMIO_LIT = re.compile(r"\b0x0*(1[0-3][0-9a-f]{3}|4[0-3][0-9a-f]{3}|5[01][0-9a-f]{3}|1ee[0-9a-f]{4}|"
                      r"1c7f[0-9a-f]{3}|20[0-9a-f]{5})u?l?\b", re.I)
MMIO_CAST = re.compile(r"\(\s*(?:const\s+)?volatile\b[^()]*\*\s*\)(?!\s*&)")
MMIO_ASM = re.compile(r"\b(?:__asm__|asm)\b(?!\s+volatile\s*\(\s*\"\"\s*:::\s*\"memory\"\s*\))")


def mmio_check():
    def strip(s):
        s = re.sub(r"/\*.*?\*/", lambda m: "\n" * m.group(0).count("\n"), s, flags=re.S)
        return re.sub(r"//[^\n]*", "", s)
    defs = {}
    for h in sorted((FW / "hal").glob("*.h")):
        for m in re.finditer(r"^#define\s+(\w+)(?:\([^)]*\))?\s+(.*)$", strip(h.read_text()), re.M):
            defs[m.group(1)] = m.group(2)
    regs = {n for n, b in defs.items() if re.search(r"\bvolatile\b", b)}
    while True:                                   # macros built on register macros (FM1_WR_LIMIT_H -> FM1_X2)
        more = {n for n, b in defs.items() if n not in regs and set(re.findall(r"\w+", b)) & regs}
        if not more:
            break
        regs |= more
    errors = []
    for f in sorted([*(FW / "src").glob("*.[ch]"), *(FW / "loader").glob("*.c")]):
        for no, ln in enumerate(strip(f.read_text()).splitlines(), 1):
            where = f"{f.relative_to(FW)}:{no}"
            for rx, what in ((MMIO_LIT, "register/window address"), (MMIO_CAST, "volatile pointer cast"),
                             (MMIO_ASM, "inline asm")):
                m = rx.search(ln)
                if m:
                    errors.append(f"{where}: {what} outside hal/ ({m.group(0).strip()}); add a hal/ helper")
            used = set(re.findall(r"\b[A-Z_][A-Z0-9_]*\b", ln)) & regs
            if used:
                errors.append(f"{where}: HAL register macro {sorted(used)[0]} outside hal/; use a hal/ helper")
    return errors


def main():
    global PRODUCT, VERSION
    ap = argparse.ArgumentParser()
    ap.add_argument("--release", metavar="X.Y", help="release build: identity FM-1_9XY, version string vX.Y")
    ap.add_argument("--sdk", type=Path, help="JieLi AC79 SDK checkout (default: $AC79_SDK)")
    a = ap.parse_args()
    name = "felucca.fwsc"
    if a.release:                   # one digit each: the identity has room for two (X.Y.Z and X.Y.Z.W keep X.Y's)
        m = re.fullmatch(r"(\d)\.(\d)(?:\.\d){0,2}(-[A-Za-z0-9]+)?", a.release)
        if not m:
            raise SystemExit(f"--release {a.release}: use X.Y, X.Y.Z, X.Y.Z.W or X.Y-suffix, one digit each")
        PRODUCT = "FM-1_9" + m[1] + m[2]
        VERSION = "v" + a.release.lower()      # e.g. v1.0, v1.1-rc1
        name = f"felucca-{a.release}.fwsc"
    fm1pkg_make.SDK = a.sdk
    for rel, sha in SDK_SHA256.items():          # fail early without the SDK
        if hashlib.sha256(fm1pkg_make.sdk_file(rel)).hexdigest() != sha:
            print(f"warning: SDK {rel} differs from AC79NN_SDK_V1.2.1; the package will not match the reference")
    OUT.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(2) as ex:
        gen, ldr = ex.submit(generate), ex.submit(build_loader)
        gen.result()
        ota = ldr.result()
    img, syms, dis, rt = build_app()
    errors, notes = check(img, syms, dis, rt)
    hal_err = mmio_check()
    errors += hal_err
    if not hal_err:
        notes.append("register access: hal/ only (src/, loader/ clean)")
    for n in notes:
        print("  ok   ", n)
    for e in errors:
        print("  FAIL ", e)
    if errors:
        raise SystemExit("build: checks failed")
    pkg = fm1pkg_make.ufw(fm1pkg_make.flash_image(img, fm1pkg_make.KEY), ota, PRODUCT, (OUT / "bt.bin").read_bytes())
    (OUT / name).write_bytes(pkg)
    print(f"app      {OUT / 'felucca.bin'}  {len(img)} B")
    print(f"loader   {LDR / 'ota.bin'}  {len(ota)} B")
    print(f"package  {OUT / name}  {len(pkg)} B, identity {PRODUCT}")
    if a.release:                   # what a release carries: the package, the app and every licence they need
        rel = OUT / f"release-{a.release}"
        shutil.rmtree(rel, ignore_errors=True)
        (rel / "LICENSES").mkdir(parents=True)
        app = f"felucca-{a.release}-app.bin"
        (rel / name).write_bytes(pkg)
        (rel / app).write_bytes(img)
        (rel / "SHA256SUMS").write_text("".join(f"{hashlib.sha256(b).hexdigest()}  {n}\n"
                                                for n, b in ((name, pkg), (app, bytes(img)))))
        for f in sorted((SRC / "LICENSES").glob("*.txt")):
            shutil.copy(f, rel / "LICENSES" / f.name)
        for doc in ("LICENSE", "LICENSING.md"):
            shutil.copy(SRC / doc, rel / doc)
        print(f"release  {rel}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())


