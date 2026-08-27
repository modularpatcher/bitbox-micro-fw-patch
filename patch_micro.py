#!/usr/bin/env python3
"""
Patch bitbox micro 2.3.4: MIDI control of things only the encoders reach.

Two voice classes handle per-pad Control Change, each with its own struct
layout, stack frame and return target. Both are hooked.

  class A  FUN_080b10b8   Sample, Granular
  class B  FUN_080b74ec   Clip, Slicer

A multi-sample pad is a Sample pad with a folder loaded into it, so it is
class A too. Looper is neither, and has no FX sends at all.

Also rewrites the startup splash: line 2 stops crediting 1010music, and the
version is marked as a modified build.

Usage: patch_micro.py <in.bin> <out.bin> [version]
"""
import hashlib, os, struct, subprocess, sys, tempfile

BASE = 0x08040000
STOCK_SHA = "54ecc3e5c09b9aed7ad8e2d9d1c9f8ee63e06a8351c857304c3ff21fcb9c4365"
STOCK_SIZE = 671376
TAPER = 0x080a04c0          # pure dB->linear taper, float in/out
CC_MAX = 16383              # CC values are 14-bit in this firmware
# --- splash screen -------------------------------------------------------
# Three consecutive 16-byte slots in the product string table:
#   0x080c5750  "bitbox micro"   line 1, names the hardware -- left alone
#   0x080c5760  "by 1010music"   line 2, attribution -- must not survive
#   0x080c5770  "2.3.4"          line 3, version
# A pointer table starts at 0x080c5780, so a slot overrun corrupts real data.
# 15 chars + terminator each.
SLOT = 16
ATTRIB_VA, ATTRIB_STOCK = 0x080c5760, "by 1010music"
ATTRIB_NEW = "community fw"
VER_VA, VER_STOCK = 0x080c5770, "2.3.4"

# (label, hook VA, return VA, cc-value stack slot, delay off, reverb off)
# --- CC map ---------------------------------------------------------------
# Grouped by what each control affects, and within each group the preset-owned
# CCs come first and the firmware-owned ones after:
#
#    70-78   pad, preset    the nine per-pad lanes the app writes
#    79-93   pad, firmware  this patch, read-only, costs no modulation slot
#    94-95   pad, firmware  reserved for Mute and Solo if those hunts land
#   96-101   UNUSABLE       MIDI reserves these (data entry, NRPN, RPN)
#  102-107   FX, preset     the six global FX destinations the app writes
#  108-109   FX, firmware   this patch
#  110-111   FX, firmware   this patch, delay Filter and Width
#  120/123   stop           the standard MIDI messages, per channel
#
# LFO Rate is deliberately absent. The manual marks it "Mod Target? Yes", so it
# is reachable from a preset and never needed a hook here. It reads as one
# control on the device but is two parameters underneath -- lforate when Beat
# Sync is off, lforatebeatsync when it is on -- and both take a modsource. The
# Delay time control is built the same way and behaves the same way.
CC_GDENSITY, CC_GSIZE, CC_GWINDOW, CC_GSCATTER, CC_GPANRND = 79, 80, 81, 82, 83
CC_DELAY, CC_REVERB, CC_REVERSE = 84, 85, 86
CC_SUSTAIN, CC_LOOPMODE, CC_LOOPFADE, CC_PLAYTHRU = 87, 88, 89, 90
CC_LFOWAVE, CC_LFOBEATSYNC, CC_LFOTRIG = 91, 92, 93
CC_BEATSYNC, CC_PINGPONG = 108, 109
CC_DLYFILTER, CC_DLYWIDTH = 110, 111
CC_STOP_HARD, CC_STOP_SOFT = 120, 123   # All Sound Off / All Notes Off

# --- granular parameters -------------------------------------------------
# The five Gran-page controls the module will not let you modulate. The manual
# marks them "Mod Target? No" and they have no three-box indicator on the
# device, so the hardware encoder is the only thing that reaches them. Speed is
# absent deliberately: it IS modulatable and MIDI Learn binds it fine.
#
# These are NOT written directly. Each parameter's setter does real work --
# Scatter indexes a lookup table, Grain Size clamps against 0x348, Window
# calls a converter and offsets by 0x100, Density triggers a recompute -- so
# reimplementing any of it would be five chances to be subtly wrong.
#
# Instead the hook lands in the firmware's own code. The per-pad CC handler and
# the set-parameter handler are the SAME function sharing one stack frame, and
# every parameter case reads its value from the same slot the CC arrives in
# ([sp,#0x40]) and exits via the same branch. So an arm only has to rescale the
# value in place and jump to the case. The firmware does the rest, including
# the recompute, exactly as it does for the encoder.
#
# Posting a set-parameter message instead would deadlock: the queue blocks past
# 32 entries and this function is both producer and consumer.
# case entry -> first instruction there, checked before we trust the address
# Every case we jump into, with the first four bytes found there in stock
# 2.3.4. These are branches into the middle of a function, so landing on the
# wrong instruction would corrupt a pad rather than fail cleanly -- the patcher
# checks each site before writing anything.
CASES = {
    #  CC              case VA     guard word   param  what the case expects
    CC_GDENSITY:    (0x080b30a2, 0x7a10eddd),  # 0x160  0..1000, then a recompute
    CC_GSIZE:       (0x080b3108, 0x7a10eddd),  # 0x15c  0..1000, mod 0x348, clamp
    CC_GWINDOW:     (0x080b2ab8, 0x7a10eddd),  # 0x3e   0..1000, converter, clamp
    CC_GSCATTER:    (0x080b30e6, 0x49aa9b10),  # 0x15e  0..1000, table lookup
    CC_GPANRND:     (0x080b314a, 0x7a10eddd),  # 0x15f  0..1000, straight scale
    CC_SUSTAIN:     (0x080b2bee, 0x7a10eddd),  # 0x22   0..1000, /1000 to a float
    CC_LOOPFADE:    (0x080b2fd0, 0x7a10eddd),  # 0xdb   0..1000, /1000 to a float
    CC_LOOPMODE:    (0x080b31a0, 0xf8c49b10),  # 0x70   raw 0/1/2, plain store
    CC_LFOWAVE:     (0x080b2f2e, 0xf5049b10),  # 0x8    raw 0..9, it adds 1
    CC_LFOBEATSYNC: (0x080b2938, 0xf8d49a10),  # 0xc    zero / non-zero
    CC_LFOTRIG:     (0x080b2ed6, 0xf8d49a10),  # 0xd    zero / non-zero
}

# How many discrete values each enum control has, for even CC banding.
ENUM_STEPS = {CC_LOOPMODE: 3, CC_LFOWAVE: 10}

# Stopping a pad. Class A case 0x32 and class B's shared 0x32/0x46 handler both
# stop whatever the pad is playing WITHOUT testing the message key -- and we are
# already inside the target pad's handler when a CC arrives, so branching there
# stops exactly that pad. No key to look up, no value to convert.
STOP_A, STOP_A_GUARD = 0x080b209c, 0x2b006ee3
STOP_B, STOP_B_GUARD = 0x080b77ee, 0x3260f894

# --- reaching class B's own parameter setter ----------------------------
# Class A can branch into a case because its CC handler and its setter are one
# function. Class B's setter is separate (FUN_080b6ce8) and virtually
# dispatched, so there is no case to jump into and no static caller to copy.
#
# Calling it is possible because its argument surface is tiny -- across the
# whole switch it reads param_2+0x18, param_3+0xc (the parameter id) and
# param_3+0x10 (the value), and nothing else. param_4 is touched by two cases
# we do not use.
#
# The two pointers are both live at the hook. FUN_080b74ec opens with
# "mov r4, r0" and "ldr.w sb, [r1]", so r4 is the voice and r9 is *param_2 --
# the same object class A calls iVar14 and reads its timestamp from. r9 is
# written once, in that prologue, and not again in the 748 instructions before
# the hook, so it still holds it when a CC arrives.
#
# This is how the three LFO controls are reached. Writing their fields by hand
# was the alternative and a bad one: each case ends by calling a resync
# (FUN_080ba214) that takes that timestamp, and without it the value lands
# somewhere the LFO never reads.
CLASSB_SETPARAM, CLASSB_SETPARAM_GUARD = 0x080b6ce8, 0x41f0e92d
CLASSB_MSG = 0x18            # scratch message frame; 8-aligned, id at +0xc
P_LFOWAVE, P_LFOBEATSYNC, P_LFOTRIG = 0x8, 0xc, 0xd

# A Control Change arrives widened to 14-bit as (7-bit value << 7) -- the LSB
# is discarded, so only 128 values are reachable however the controller sends
# it. Shift back to 0..127, then scale to the 0..1000 the parameters use.
# 8063/1024 == 7.87402, and 127*8063 >> 10 == 1000 exactly, so both endpoints
# land clean; scaling the raw 14-bit by 1000/16383 would top out at 992.
CC7_TO_1000_MUL, CC7_TO_1000_SHIFT = 8063, 10

# --- global delay toggles ------------------------------------------------
# The delay FX object has no Control Change handler at all; MIDI reaches FX
# parameters through the modulation matrix, which computes base +/- amount and
# so cannot drive a boolean. Instead we post the same set-parameter message the
# FX screen knobs use, from the MIDI dispatcher (the control thread, which is
# where engine commands are meant to be posted from).
MIDI_POST_CALL = 0x08093f14   # 'bl FUN_080950c0' that posts the CC event
MIDI_POST_FN   = 0x080950c0   # the function it calls
MSG_CC_SP      = 0x84         # CC number,   caller-frame offset
MSG_VAL_SP     = 0x88         # 14-bit value, caller-frame offset
SETPARAM_FN    = 0x080937f4   # cmd39: (manager, padKey, paramId, value, 0, 0)
MANAGER        = 0x24022978
DELAY_KEY      = 0x30000      # delay cell = layer 3, col 0, row 0 (reverb 0x30001)
P_BEATSYNC, P_PINGPONG = 0x17, 0x18
# The delay's own filter switch and bandwidth. Ghidra's decompile of the delay
# object silently drops the stretch of the switch these live in, which is why
# they looked unreachable for a while -- but the comparisons are plainly there
# in the disassembly at DLYFX_GUARD_VA, and they are registered alongside
# cutoff/res/delay/feedback so they belong to this object. Filter is 0/1;
# Width is 0..1000 like every other continuous FX parameter.
P_DLYFILTER, P_DLYWIDTH = 0xab, 0xac
DLYFX_GUARD_VA, DLYFX_GUARD = 0x080a7ca8, 0xf0002bab   # 'cmp r3,#0xab'
BOOL_THRESHOLD = 8192          # half of 16383: >= is "on"

# (label, hook VA, return VA, cc-value stack slot, [(cc, kind, byte offset)])
# kind 'send' -> taper(value/16383) as a float;  'bool' -> 1 byte, 0 or 1
HOOKS = [
    ("A sample/multi", 0x080b2316, 0x080b1110, 0x40, [
        # Written straight into the pad -- the setter does nothing we need.
        (CC_DELAY,   "send", 0x558),
        (CC_REVERB,  "send", 0x55c),
        (CC_REVERSE, "bool", 0x10c),   # class B has no reverse field at all

        # Everything below branches into the firmware's own case for that
        # parameter, so the clamps, lookups, conversions and recomputes are
        # the module's rather than ours. Harmless on pad types that do not use
        # a given field -- the field exists, nothing reads it.
        (CC_GDENSITY,    "jump",  CASES[CC_GDENSITY][0]),
        (CC_GSIZE,       "jump",  CASES[CC_GSIZE][0]),
        (CC_GWINDOW,     "jump",  CASES[CC_GWINDOW][0]),
        (CC_GSCATTER,    "jump",  CASES[CC_GSCATTER][0]),
        (CC_GPANRND,     "jump",  CASES[CC_GPANRND][0]),
        (CC_SUSTAIN,     "jump",  CASES[CC_SUSTAIN][0]),
        (CC_LOOPFADE,    "jump",  CASES[CC_LOOPFADE][0]),
        (CC_LOOPMODE,    "enum",  CASES[CC_LOOPMODE][0]),
        (CC_LFOWAVE,     "enum",  CASES[CC_LFOWAVE][0]),
        (CC_LFOBEATSYNC, "jbool", CASES[CC_LFOBEATSYNC][0]),
        (CC_LFOTRIG,     "jbool", CASES[CC_LFOTRIG][0]),

        # Both standard stop messages reach the same code. 120 is meant to cut
        # dead and 123 to let notes release; the module has one stop, so they
        # behave identically here rather than one of them doing nothing.
        (CC_STOP_HARD, "stop", STOP_A),
        (CC_STOP_SOFT, "stop", STOP_A),
    ]),
    ("B clip/slicer",  0x080b7cd6, 0x080b7554, 0x38, [
        (CC_DELAY,   "send", 0xb84),
        (CC_REVERB,  "send", 0xb88),

        # Sustain exists on this class too -- the earlier build hooked only
        # class A, so CC87 did nothing on Clip and Slicer pads. Its own case
        # (0x22) is two float stores and nothing else: no lookup, no clamp,
        # no recompute, so there is nothing to gain by reaching the case and
        # this class keeps its parameter switch in a different frame anyway.
        #
        (CC_SUSTAIN, "pair", (0x508, 0x918)),

        # The LFO three go through class B's own setter rather than having
        # their fields written here, because each ends in a resync that needs
        # the context object. See CLASSB_SETPARAM above.
        (CC_LFOWAVE,     "call", (P_LFOWAVE, ENUM_STEPS[CC_LFOWAVE])),
        (CC_LFOBEATSYNC, "call", (P_LFOBEATSYNC, None)),
        (CC_LFOTRIG,     "call", (P_LFOTRIG, None)),
        # PlayThru is a Slicer control and lives only in this class. Its own
        # setter is a plain boolean store, so there is nothing worth jumping
        # into -- class B keeps its parameter switch in a separate function
        # with a different frame anyway.
        (CC_PLAYTHRU, "bool", 0x255),
        (CC_STOP_HARD, "stop", STOP_B),
        (CC_STOP_SOFT, "stop", STOP_B),
    ]),
]


USAGE = """
  Community enhancements for BitBox Micro
  ---------------------------------------

  Adds MIDI control of the five unmodulatable granular controls, the per-pad
  delay and reverb sends, per-pad reverse, and the global delay beat-sync and
  ping-pong switches.

  You need your own copy of the stock 2.3.4 firmware, from
  https://1010music.com/downloads -- none of it is included here. It arrives
  as MICRO234.zip; unzip it and you get MICRO.BIN.

    mkdir patched
    python3 patch_micro.py MICRO.BIN patched/MICRO.BIN

  The output must be called MICRO.BIN for the module to find it, which is why
  it goes in its own folder -- writing over your only stock copy would leave
  you no way back.

    -v    show the addresses and opcodes as it works

  Not made by, endorsed by, or supported by 1010music. Read the README
  before you flash anything.
"""


# --- frozen hook code ----------------------------------------------------
# Assembler output for the three hooks, with branch placeholders left zeroed
# so the bytes are origin-independent. Embedding them is what lets this script
# run with no toolchain: the assembly below is still the source of truth, and
# --reassemble rebuilds it and fails loudly if these constants have drifted.
# Regenerate by deleting an entry and running with --reassemble.

FROZEN = {
    "hookA": (
        "542b00f02f80552b00f04080562b00f051804f2b00f05880502b00f05f80512b"
        "00f06680522b00f06d80532b00f07480572b00f07b80592b00f08280582b00f0"
        "89805b2b00f08f805c2b00f095805d2b00f09b80782b00f0a1807b2b00f0a080"
        "00000000dded107ab8eee70a43f6ff7300ee903af8eee00a80ee200a00000000"
        "04f2585383ed000a00000000dded107ab8eee70a43f6ff7300ee903af8eee00a"
        "80ee200a0000000004f25c5383ed000a00000000109bb3f5005fb4bf00230123"
        "84f80c3100000000109bdb1141f67f7203fb02f39b0a109300000000109bdb11"
        "41f67f7203fb02f39b0a109300000000109bdb1141f67f7203fb02f39b0a1093"
        "00000000109bdb1141f67f7203fb02f39b0a109300000000109bdb1141f67f72"
        "03fb02f39b0a109300000000109bdb1141f67f7203fb02f39b0a109300000000"
        "109bdb1141f67f7203fb02f39b0a109300000000109b40f2030203fb02f39b0b"
        "109300000000109b40f20a0203fb02f39b0b109300000000109bb3f5005fb4bf"
        "00230123109300000000109bb3f5005fb4bf0023012310930000000000000000"
        "00000000"
        ,
        {'$t': 394, 'aA0': 100, 'aA1': 140, 'aA2': 180, 'aA3': 200, 'aA4': 220, 'aA5': 240, 'aA6': 260, 'aA7': 280, 'aA8': 300, 'aA9': 320, 'aA10': 340, 'aA11': 358, 'aA12': 376, 'aA13': 394, 'aA14': 412, 'aA15': 416, '$d': 408, '_sA': 0, 'fA': 96, 'aA0_bl': 124, 'aA0_ret': 136, 'aA1_bl': 164, 'aA1_ret': 176, 'aA2_ret': 196, 'aA3_j': 216, 'aA4_j': 236, 'aA5_j': 256, 'aA6_j': 276, 'aA7_j': 296, 'aA8_j': 316, 'aA9_j': 336, 'aA10_j': 354, 'aA11_j': 372, 'aA12_j': 390, 'aA13_j': 408, 'aA14_j': 412, 'aA15_j': 416},
    ),
    "hookB": (
        "542b00f01a80552b00f02b80572b00f03c805b2b00f04f805c2b00f060805d2b"
        "00f071805a2b00f08280782b00f089807b2b00f0888000000000dded0e7ab8ee"
        "e70a43f6ff7300ee903af8eee00a80ee200a0000000004f6843383ed000a0000"
        "0000dded0e7ab8eee70a43f6ff7300ee903af8eee00a80ee200a0000000004f6"
        "883383ed000a00000000dded0e7ab8eee70a43f6ff7300ee903af8eee00a80ee"
        "200a04f2085383ed000a04f6181383ed000a000000000e9b40f20a0203fb02f3"
        "9b0b86b040f2080203920493204649466a4600230000000006b0000000000e9b"
        "b3f5005fb4bf0023012386b040f20c0203920493204649466a46002300000000"
        "06b0000000000e9bb3f5005fb4bf0023012386b040f20d020392049320464946"
        "6a4600230000000006b0000000000e9bb3f5005fb4bf0023012384f855320000"
        "00000000000000000000"
        ,
        {'$t': 302, 'aB0': 58, 'aB1': 98, 'aB2': 138, 'aB3': 182, 'aB4': 222, 'aB5': 262, 'aB6': 302, 'aB7': 322, 'aB8': 326, '$d': 318, '_sB': 0, 'fB': 54, 'aB0_bl': 82, 'aB0_ret': 94, 'aB1_bl': 122, 'aB1_ret': 134, 'aB2_ret': 178, 'aB3_bl': 212, 'aB3_ret': 218, 'aB4_bl': 252, 'aB4_ret': 258, 'aB5_bl': 292, 'aB5_ret': 298, 'aB6_ret': 318, 'aB7_j': 322, 'aB8_j': 326},
    ),
    "hookC": (
        "30b5249c259d000000006c2c06d06d2c06d06e2c06d06f2c0bd030bd172202e0"
        "182200e0ab22b5f5005fb4bf0025012506e0ac22ed1141f67f7305fb03f5ad0a"
        "2b46002482b00094019442f67810c2f2024040f20001c0f203010000000002b0"
        "30bd"
        ,
        {'$t': 94, '$d': 90, 'cbs': 28, 'cpp': 32, 'cfl': 36, 'cwd': 50, 'cbool': 38, 'csend': 64, '_sC': 0, 'cpost': 6, 'csetp': 90},
    ),
}

REASSEMBLE = False
VERBOSE = False

# What the patch gives you, in the order it gets applied. Kept here so the
# summary the user reads is generated from the same constants that do the work
# and cannot drift into a comfortable lie.
FEATURES = [
    ("Granular density, per pad", f"CC {CC_GDENSITY}"),
    ("Granular grain size, per pad", f"CC {CC_GSIZE}"),
    ("Granular window, per pad", f"CC {CC_GWINDOW}"),
    ("Granular scatter, per pad", f"CC {CC_GSCATTER}"),
    ("Granular pan random, per pad", f"CC {CC_GPANRND}"),
    ("Delay send, per pad", f"CC {CC_DELAY}"),
    ("Reverb send, per pad", f"CC {CC_REVERB}"),
    ("Reverse, per pad", f"CC {CC_REVERSE}"),
    ("Envelope sustain, per pad", f"CC {CC_SUSTAIN}"),
    ("Loop mode, per pad", f"CC {CC_LOOPMODE}"),
    ("Loop crossfade, per pad", f"CC {CC_LOOPFADE}"),
    ("Play through, slicer pads", f"CC {CC_PLAYTHRU}"),
    ("LFO wave, per pad", f"CC {CC_LFOWAVE}"),
    ("LFO beat-sync, per pad", f"CC {CC_LFOBEATSYNC}"),
    ("LFO retrigger, per pad", f"CC {CC_LFOTRIG}"),
    ("Delay beat-sync, global", f"CC {CC_BEATSYNC}"),
    ("Delay ping-pong, global", f"CC {CC_PINGPONG}"),
    ("Delay filter, global", f"CC {CC_DLYFILTER}"),
    ("Delay filter width, global", f"CC {CC_DLYWIDTH}"),
    ("Stop this pad", f"CC {CC_STOP_HARD} / {CC_STOP_SOFT}"),
]


def vprint(msg):
    """Addresses, opcodes and offsets. Only when -v asks for them."""
    if VERBOSE:
        print(f"       {msg}")


def refuse(headline, *detail):
    """Stop, and explain it in a way somebody can act on."""
    sys.stdout.flush()          # else the buffered header lands after this
    out = [f"\n  X  {headline}\n"]
    out += [f"     {line}".rstrip() for line in detail]
    out.append("\n     Nothing has been written. Your firmware file is untouched.\n")
    sys.stderr.write("\n".join(out) + "\n")
    raise SystemExit(1)


def asm(src):
    with tempfile.TemporaryDirectory() as td:
        s, obj = os.path.join(td, "p.s"), os.path.join(td, "p.o")
        open(s, "w").write(src)
        r = subprocess.run(["xcrun", "clang", "-target", "thumbv7em-none-eabi",
                            "-mcpu=cortex-m7", "-mfloat-abi=hard", "-c", "-o", obj, s],
                           capture_output=True, text=True)
        if r.returncode:
            sys.stderr.write(src + "\n" + r.stderr)
            raise SystemExit("assembly failed")
        return parse_elf32(open(obj, "rb").read())


def parse_elf32(e):
    shoff, = struct.unpack_from("<I", e, 0x20)
    shent, shnum, shstrndx = struct.unpack_from("<HHH", e, 0x2E)
    sh = lambda i: dict(zip(
        ("name", "type", "flags", "addr", "off", "size", "link", "info", "align", "entsz"),
        struct.unpack_from("<10I", e, shoff + i * shent)))
    secs = [sh(i) for i in range(shnum)]
    shstr = secs[shstrndx]
    nm = lambda o: e[shstr["off"] + o:e.index(b"\0", shstr["off"] + o)].decode()
    ti = next(i for i, s in enumerate(secs) if nm(s["name"]) == ".text")
    code = e[secs[ti]["off"]:secs[ti]["off"] + secs[ti]["size"]]
    syms = {}
    for s in secs:
        if s["type"] != 2:
            continue
        st = secs[s["link"]]
        for i in range(s["size"] // s["entsz"]):
            o = s["off"] + i * s["entsz"]
            n, val, _, _, _, shndx = struct.unpack_from("<IIIBBH", e, o)
            if shndx == ti and n:
                syms[e[st["off"] + n:e.index(b"\0", st["off"] + n)].decode()] = val & ~1
    return code, syms


def enc_b_bl(src, dst, is_bl):
    off = dst - (src + 4)
    assert -(1 << 24) <= off < (1 << 24), f"out of range {off}"
    imm = off >> 1
    S, I1, I2 = (imm >> 23) & 1, (imm >> 22) & 1, (imm >> 21) & 1
    hw1 = 0xF000 | (S << 10) | ((imm >> 11) & 0x3FF)
    hw2 = ((0xD000 if is_bl else 0x9000) | ((((~I1) & 1) ^ S) << 13)
           | ((((~I2) & 1) ^ S) << 11) | (imm & 0x7FF))
    return struct.pack("<HH", hw1, hw2)


def enc_bne_w(src, dst):
    off = dst - (src + 4)
    assert -(1 << 20) <= off < (1 << 20), f"out of range {off}"
    imm = off >> 1
    hw1 = 0xF000 | (((imm >> 19) & 1) << 10) | (1 << 6) | ((imm >> 11) & 0x3F)
    hw2 = 0x8000 | (((imm >> 17) & 1) << 13) | (((imm >> 18) & 1) << 11) | (imm & 0x7FF)
    return struct.pack("<HH", hw1, hw2)


def assemble(key, src):
    """Machine code for one hook: frozen bytes by default, clang on request.

    The frozen blobs are this file's own assembler output with every branch
    placeholder still zeroed -- callers patch those per origin, so the bytes
    do not depend on where the block lands and can safely be a constant.
    That is what lets the patcher run with no toolchain installed.

    Pass --reassemble (needs a thumbv7em assembler) to rebuild from the source
    below and prove the constants still match it.
    """
    if REASSEMBLE:
        code, syms = asm(src)
        if key not in FROZEN:                      # generating a new blob
            print(f'    "{key}": ("{code.hex()}",\n'
                  f'        {syms!r}),')
            return code, syms
        if (code.hex(), syms) != FROZEN[key]:
            sys.exit(f"FROZEN[{key!r}] is STALE -- the assembler now produces\n"
                     f'    "{key}": ("{code.hex()}",\n        {syms!r}),')
        print(f"  reassembled {key:8} matches frozen blob ({len(code)} bytes)")
        return code, syms

    if key not in FROZEN:
        sys.exit(f"no frozen blob for {key!r}; rerun with --reassemble")
    blob, syms = FROZEN[key]
    return bytes.fromhex(blob), syms


def build(org, ret, vslot, arms, tag):
    def send_arm(sym, off):
        return f"""
    vldr    s15, [sp, #{vslot:#x}]
    vcvt.f32.s32 s0, s15
    movw    r3, #{CC_MAX}
    vmov    s1, r3
    vcvt.f32.s32 s1, s1
    vdiv.f32 s0, s0, s1
    .global {sym}_bl
{sym}_bl:
    .short 0,0
    addw    r3, r4, #{off:#x}
    vstr    s0, [r3]
    .global {sym}_ret
{sym}_ret:
    .short 0,0
"""
    def jump_arm(sym, _off):
        """Rescale the value in place, then fall into the firmware's own case.

        r2/r3 are free here: every target case loads what it needs before
        reading either, and r0/r1/s16-s21 are untouched.
        """
        return f"""
    ldr     r3, [sp, #{vslot:#x}]
    asrs    r3, r3, #7                 /* 14-bit widened -> 0..127   */
    movw    r2, #{CC7_TO_1000_MUL}
    mul     r3, r3, r2
    lsrs    r3, r3, #{CC7_TO_1000_SHIFT}  /* -> 0..1000, exact at both ends */
    str     r3, [sp, #{vslot:#x}]
    .global {sym}_j
{sym}_j:
    .short 0,0
"""
    def enum_arm(sym, target):
        """Even CC bands onto N discrete values, then into the firmware's case.

        band = (raw * N) >> 14. The value arrives as (7-bit << 7), so this is
        (cc7 * N) >> 7 with one multiply instead of two shifts, and it lands
        exactly on 0 and N-1 at the extremes for every N we use.
        """
        n = ENUM_STEPS[target_cc[sym]]
        return f"""
    ldr     r3, [sp, #{vslot:#x}]
    movw    r2, #{n}
    mul     r3, r3, r2
    lsrs    r3, r3, #14                /* -> 0..{n - 1}, even bands */
    str     r3, [sp, #{vslot:#x}]
    .global {sym}_j
{sym}_j:
    .short 0,0
"""
    def jbool_arm(sym, _target):
        """Zero or one into the value slot, then into the firmware's case.

        These cases test the slot against zero themselves, so the arm only has
        to make it definitely zero or definitely not.
        """
        return f"""
    ldr     r3, [sp, #{vslot:#x}]
    cmp.w   r3, #{BOOL_THRESHOLD}
    ite     lt
    movlt   r3, #0
    movge   r3, #1
    str     r3, [sp, #{vslot:#x}]
    .global {sym}_j
{sym}_j:
    .short 0,0
"""
    def stop_arm(sym, _target):
        """Branch straight into the pad's own stop code.

        Neither class tests the message key here, and we are already inside the
        target pad's handler, so this stops exactly that pad. Nothing to
        convert and nothing to set up.
        """
        return f"""
    .global {sym}_j
{sym}_j:
    .short 0,0
"""
    def bool_arm(sym, off):
        return f"""
    ldr     r3, [sp, #{vslot:#x}]
    cmp.w   r3, #{BOOL_THRESHOLD}
    ite     lt
    movlt   r3, #0
    movge   r3, #1
    strb.w  r3, [r4, #{off:#x}]
    .global {sym}_ret
{sym}_ret:
    .short 0,0
"""
    def pair_arm(sym, offs):
        """One float into BOTH copies the Clip/Slicer voice keeps.

        Class B stores every parameter twice, in two engine structures 0x410
        bytes apart, and its own setter writes both every time. Writing one
        would leave the two halves disagreeing.

        No taper: the parameter is stored as value/1000.0 and a CC spans
        0..1000 of it, so the float wanted is just cc/16383 -- which is what
        the send arm computes before its taper call.
        """
        lo, hi = offs
        return f"""
    vldr    s15, [sp, #{vslot:#x}]
    vcvt.f32.s32 s0, s15
    movw    r3, #{CC_MAX}
    vmov    s1, r3
    vcvt.f32.s32 s1, s1
    vdiv.f32 s0, s0, s1
    addw    r3, r4, #{lo:#x}
    vstr    s0, [r3]
    addw    r3, r4, #{hi:#x}
    vstr    s0, [r3]
    .global {sym}_ret
{sym}_ret:
    .short 0,0
"""
    def call_arm(sym, spec):
        """Hand the value to class B's own setter instead of writing fields.

        spec is (parameter id, number of enum steps or None for a boolean).
        The value is read BEFORE sp moves, then a small message frame is built
        with the id at +0xc and the value at +0x10 -- the only two fields the
        setter reads. r4 is the voice and r9 the context object it wants, both
        still live from the prologue. sp is restored before the arm returns,
        because the code it returns to addresses its frame off sp.
        """
        pid, steps = spec
        if steps is None:
            conv = f"""
    cmp.w   r3, #{BOOL_THRESHOLD}
    ite     lt
    movlt   r3, #0
    movge   r3, #1"""
        else:
            conv = f"""
    movw    r2, #{steps}
    mul     r3, r3, r2
    lsrs    r3, r3, #14                /* -> 0..{steps - 1}, even bands */"""
        return f"""
    ldr     r3, [sp, #{vslot:#x}]{conv}
    sub     sp, #{CLASSB_MSG:#x}
    movw    r2, #{pid}
    str     r2, [sp, #0xc]
    str     r3, [sp, #0x10]
    mov     r0, r4
    mov     r1, r9
    mov     r2, sp
    movs    r3, #0
    .global {sym}_bl
{sym}_bl:
    .short 0,0
    add     sp, #{CLASSB_MSG:#x}
    .global {sym}_ret
{sym}_ret:
    .short 0,0
"""
    dispatch, bodies, fixups = [], [], [(f"f{tag}", ret, False)]
    target_cc = {}
    for n, (cc, kind, off) in enumerate(arms):
        sym = f"a{tag}{n}"
        dispatch.append(f"    cmp   r3, #{cc}\n    beq.w {sym}")
        if kind == "send":
            bodies.append(f"{sym}:{send_arm(sym, off)}")
            fixups += [(f"{sym}_bl", TAPER, True), (f"{sym}_ret", ret, False)]
        elif kind == "pair":
            bodies.append(f"{sym}:{pair_arm(sym, off)}")
            fixups += [(f"{sym}_ret", ret, False)]
        elif kind == "call":
            bodies.append(f"{sym}:{call_arm(sym, off)}")
            fixups += [(f"{sym}_bl", CLASSB_SETPARAM, True),
                       (f"{sym}_ret", ret, False)]
        elif kind in ("jump", "enum", "jbool", "stop"):
            # off is a case address inside this same function, not a field
            # offset -- the arm branches there instead of returning.
            gen = {"jump": jump_arm, "enum": enum_arm,
                   "jbool": jbool_arm, "stop": stop_arm}[kind]
            target_cc[sym] = cc
            bodies.append(f"{sym}:{gen(sym, off)}")
            fixups += [(f"{sym}_j", off, False)]
        else:
            bodies.append(f"{sym}:{bool_arm(sym, off)}")
            fixups += [(f"{sym}_ret", ret, False)]

    src = f"""
    .syntax unified
    .thumb
    .section .text
    .global _s{tag}
    .thumb_func
_s{tag}:
{chr(10).join(dispatch)}
    .global f{tag}
f{tag}:
    .short 0,0
{"".join(bodies)}
"""
    code, syms = assemble(f"hook{tag}", src)
    code = bytearray(code)
    for sym, dst, isbl in fixups:
        o = syms[sym]
        code[o:o + 4] = enc_b_bl(org + o, dst, isbl)
    return bytes(code)


def build_midi_hook(org):
    """Replacement for the CC-event post: do the original post, then translate
    our two CC numbers into set-parameter messages aimed at the delay object."""
    src = f"""
    .syntax unified
    .thumb
    .section .text
    .global _sC
    .thumb_func
_sC:
    push    {{r4, r5, lr}}
    ldr     r4, [sp, #{MSG_CC_SP + 12:#x}]      /* CC number  */
    ldr     r5, [sp, #{MSG_VAL_SP + 12:#x}]     /* 14-bit value */
    .global cpost
cpost:
    .short 0,0                                  /* bl FUN_080950c0 (original) */
    cmp     r4, #{CC_BEATSYNC}
    beq     cbs
    cmp     r4, #{CC_PINGPONG}
    beq     cpp
    cmp     r4, #{CC_DLYFILTER}
    beq     cfl
    cmp     r4, #{CC_DLYWIDTH}
    beq     cwd
    pop     {{r4, r5, pc}}
cbs:
    movs    r2, #{P_BEATSYNC}
    b       cbool
cpp:
    movs    r2, #{P_PINGPONG}
    b       cbool
cfl:
    movs    r2, #{P_DLYFILTER}
cbool:
    cmp.w   r5, #{BOOL_THRESHOLD}
    ite     lt
    movlt   r5, #0
    movge   r5, #1
    b       csend
cwd:
    /* Width is continuous, so it wants 0..1000 rather than a flag. */
    movs    r2, #{P_DLYWIDTH}
    asrs    r5, r5, #7
    movw    r3, #{CC7_TO_1000_MUL}
    mul     r5, r5, r3
    lsrs    r5, r5, #{CC7_TO_1000_SHIFT}
csend:
    mov     r3, r5
    movs    r4, #0
    sub     sp, #8
    str     r4, [sp, #0]
    str     r4, [sp, #4]
    movw    r0, #{MANAGER & 0xffff}
    movt    r0, #{MANAGER >> 16}
    movw    r1, #{DELAY_KEY & 0xffff}
    movt    r1, #{DELAY_KEY >> 16}
    .global csetp
csetp:
    .short 0,0                                  /* bl cmd39_SetParam */
    add     sp, #8
    pop     {{r4, r5, pc}}
"""
    code, syms = assemble("hookC", src)
    code = bytearray(code)
    code[syms["cpost"]:syms["cpost"] + 4] = enc_b_bl(org + syms["cpost"], MIDI_POST_FN, True)
    code[syms["csetp"]:syms["csetp"] + 4] = enc_b_bl(org + syms["csetp"], SETPARAM_FN, True)
    return bytes(code)


def write_slot(d, va, expect, new):
    """Overwrite one 16-byte string slot, guarding on its current contents.

    Same fail-safe discipline as the code hooks: refuse rather than clobber
    something unrecognised. Null-pads the whole slot so no tail of the old
    string can survive past the new terminator.
    """
    o = va - BASE
    cur = bytes(d[o:o + SLOT]).split(b"\0")[0].decode()
    if cur != expect:
        refuse(f"The splash text at {va:#x} is not what it should be.",
               f"Found {cur!r}, expected {expect!r}.",
               "",
               "This firmware is not the build this patcher was made for.")
    if len(new) >= SLOT:
        refuse(f"{new!r} is too long for the splash screen.",
               f"It is {len(new)} characters and the limit is {SLOT - 1}.",
               "",
               "There is a pointer table straight after this text, so a longer",
               "string would overwrite something the module needs.")
    d[o:o + SLOT] = new.encode().ljust(SLOT, b"\0")
    vprint(f"splash {va:#x} {cur!r} -> {new!r}")


def main():
    global REASSEMBLE, VERBOSE
    flags = sys.argv[1:]
    REASSEMBLE = "--reassemble" in flags
    VERBOSE = "-v" in flags or "--verbose" in flags
    argv = [a for a in flags if not a.startswith("-")]

    if len(argv) < 2:
        sys.exit(USAGE)

    inp, outp = argv[0], argv[1]
    ver = argv[2] if len(argv) > 2 else "2.3.5-mod"

    try:
        sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    except Exception:                      # ancient Python, or an odd console
        pass

    # 1010music ship MICRO234.zip containing a plain MICRO.BIN, and the module
    # requires the patched image be called MICRO.BIN too -- so the obvious
    # command overwrites the only stock copy, which is the one thing you cannot
    # get back except by downloading again.
    if os.path.realpath(inp) == os.path.realpath(outp):
        refuse("The input and the output are the same file.",
               f"{inp}",
               "",
               "That would overwrite your stock firmware, and it is the only",
               "way back to a working module if anything goes wrong.",
               "",
               "Write the patched image somewhere else and copy it to the card:",
               "",
               "    mkdir patched",
               f"    python3 patch_micro.py {inp} patched/MICRO.BIN")

    print("\n  Checking your firmware...")
    try:
        d = bytearray(open(inp, "rb").read())
    except OSError as e:
        refuse(f"Could not open {inp}", str(e), "",
               "Check the path. If the name has spaces in it, put it in quotes.")

    sha = hashlib.sha256(d).hexdigest()
    if len(d) != STOCK_SIZE or sha != STOCK_SHA:
        refuse("That is not the stock 2.3.4 firmware.",
               f"expected  {STOCK_SIZE:,} bytes   {STOCK_SHA[:24]}...",
               f"got       {len(d):,} bytes   {sha[:24]}...",
               "",
               "Usually this means one of:",
               "  - it is a different firmware version",
               "  - it has already been patched",
               "  - the download did not finish properly",
               "",
               "Fresh copy: https://1010music.com/downloads")
    print(f"     ok   Genuine bitbox micro 2.3.4, nothing done to it yet")
    vprint(f"sha256 {sha}")

    for label, hook, ret, vslot, arms in HOOKS:
        cur = bytes(d[hook - BASE:hook - BASE + 4])
        want = enc_bne_w(hook, ret)
        if cur != want:
            refuse(f"The code at {hook:#x} is not what it should be.",
                   f"found {cur.hex()}, expected {want.hex()}",
                   "",
                   "The file is the right size and checksum but the machine code",
                   "is wrong, which should be impossible. Do not try to force it.")
        vprint(f"hook {label:16} {hook:#x} = bne.w {ret:#x} ({cur.hex()})")

    # The granular arms branch into the middle of a function rather than to an
    # entry point, so check each landing site still holds the instruction it
    # held in 2.3.4 before trusting the address.
    for cc, (va, expect) in sorted(CASES.items()):
        got, = struct.unpack_from("<I", d, va - BASE)
        if got != expect:
            refuse(f"The granular code at {va:#x} is not what it should be.",
                   f"found {got:#010x}, expected {expect:#010x}  (CC{cc})",
                   "",
                   "This is where the patch jumps to let the firmware apply the",
                   "value itself. Landing on the wrong instruction would corrupt",
                   "the pad, so nothing has been written.")
        vprint(f"CC{cc} case {va:#x} = {got:#010x}")
    for nm, va, expect in (("stop A", STOP_A, STOP_A_GUARD),
                           ("stop B", STOP_B, STOP_B_GUARD),
                           # Called, not jumped into, but the same reasoning:
                           # the wrong address here is a call into the middle
                           # of something with four arguments behind it.
                           ("class B set-parameter",
                            CLASSB_SETPARAM, CLASSB_SETPARAM_GUARD),
                           # Proof the delay really does handle Filter and
                           # Width, and at these ids -- the decompiler missed
                           # this stretch, so the disassembly is the evidence.
                           ("delay filter/width",
                            DLYFX_GUARD_VA, DLYFX_GUARD)):
        got, = struct.unpack_from("<I", d, va - BASE)
        if got != expect:
            refuse(f"The {nm} code at {va:#x} is not what it should be.",
                   f"found {got:#010x}, expected {expect:#010x}")
        vprint(f"{nm} {va:#x} = {got:#010x}")

    print("     ok   The code is where the patch expects to find it")

    print("\n  Embellishing your bitbox micro with community requests...")

    for label, hook, ret, vslot, arms in HOOKS:
        org = BASE + len(d)
        assert org % 4 == 0
        code = build(org, ret, vslot, arms, label[0])
        if len(code) % 4:                      # keep the next block 4-byte aligned
            code += b"\x00" * (4 - len(code) % 4)
        d += code
        d[hook - BASE:hook - BASE + 4] = enc_bne_w(hook, org)
        def _o(v):
            if isinstance(v, tuple):
                return "/".join(_o(x) for x in v)
            return "-" if v is None else f"{v:#x}"
        detail = ", ".join(f"CC{c}->{k}+{_o(o)}" for c, k, o in arms)
        vprint(f"{label:16} {len(code):3} bytes at {org:#010x}   {detail}")

    # --- third hook: the MIDI dispatcher's CC-event post ------------------
    off = MIDI_POST_CALL - BASE
    cur = bytes(d[off:off + 4])
    want = enc_b_bl(MIDI_POST_CALL, MIDI_POST_FN, True)
    if cur != want:
        refuse(f"The MIDI code at {MIDI_POST_CALL:#x} is not what it should be.",
               f"found {cur.hex()}, expected {want.hex()}",
               "",
               "Do not try to force it.")
    vprint(f"hook C midi post {MIDI_POST_CALL:#x} = bl {MIDI_POST_FN:#x} ({cur.hex()})")
    org = BASE + len(d)
    assert org % 4 == 0
    code = build_midi_hook(org)
    if len(code) % 4:
        code += b"\x00" * (4 - len(code) % 4)
    d += code
    d[off:off + 4] = enc_b_bl(MIDI_POST_CALL, org, True)
    vprint(f"{'C delay FX':16} {len(code):3} bytes at {org:#010x}   "
           f"CC{CC_BEATSYNC}->beatsync(0x17), CC{CC_PINGPONG}->pingpong(0x18), "
           f"CC{CC_DLYFILTER}->filtenable(0xab), CC{CC_DLYWIDTH}->filtquality(0xac) "
           f"@ key {DELAY_KEY:#x}")

    for what, cc in FEATURES:
        print(f"     ok   {what:<28} {cc}")

    # Splash: drop 1010music's attribution so the modifications are not
    # credited to them, and mark the version as a modified build.
    write_slot(d, ATTRIB_VA, ATTRIB_STOCK, ATTRIB_NEW)
    write_slot(d, VER_VA, VER_STOCK, ver)

    print(f"     ok   Splash screen, so nobody blames 1010music for my work")

    try:
        open(outp, "wb").write(bytes(d))
    except OSError as e:
        refuse(f"Could not write {outp}", str(e))

    out_sha = hashlib.sha256(bytes(d)).hexdigest()
    print(f"\n  Done. {outp} is ready -- {len(d):,} bytes.\n")
    print(f"     Fingerprint  {out_sha}")
    print( "     If that matches the one in the README, you have built exactly")
    print( "     the same firmware as everybody else, down to the byte.")

    print("\n  Getting it onto the module:\n")
    print( "     1.  Copy the file to the root of a microSD card, named")
    print(f"         MICRO.BIN -- capitals, and not inside a folder")
    print( "     2.  Power the module off and put the card in")
    print( "     3.  Hold the white right-arrow button, then power on")
    print( "     4.  Let go when it says Erasing. Takes about 15 seconds.")
    print(f"\n     You will know it worked when the splash reads")
    print(f"     'bitbox micro / {ATTRIB_NEW} / {ver}'.")

    print("\n  Enjoy. If anything misbehaves, put the stock firmware back")
    print("  before you go and tell 1010music about it -- this bit is on us.\n")


if __name__ == "__main__":
    main()
