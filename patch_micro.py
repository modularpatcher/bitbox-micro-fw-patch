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
import ast, collections, hashlib, os, struct, subprocess, sys, tempfile

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

# The slot STORES 15 characters plus a terminator. The panel DISPLAYS fewer, and
# we have no figure for that from documentation -- only from an incident.
#
# 2026-09-05: `2.3.7-mod-b10` is thirteen characters and was read back off the
# module as `2.3.7-mod-b1`, which is exactly twelve. Every earlier EQ build was
# twelve or fewer (`2.3.7-mod-b7`) and read correctly. That cost real time: two
# hardware faults were reported against "B1" and had to be traced back to which
# image was actually on the card before either could be investigated.
#
# One observation is not a specification, so this limit is NOT set at the
# observed edge. Ten characters leaves two to spare against the only evidence we
# have, and `2.3.7-b10` fits in nine -- so the scheme from b11 onward drops the
# `-mod` infix, which the `community fw` line above already says.
#
# CONFIRMED END TO END ON HARDWARE, 2026-09-05. b11 is the first build in the
# nine-character scheme and its splash was read back in full -- "all characters,
# nothing cut". String chosen, guard enforced here at build time, legible on the
# panel. But read the evidence for what it is: ONE build at NINE characters
# displaying fully proves nine fits. It does not measure the panel, so 10 is
# still a conservative choice made from a single truncation observation rather
# than a measured limit. Do not raise it on the strength of b11.
#
# This is a guard, not a convention, for the reason plan 01 gave: a rule you have
# to remember is a rule you will forget at 2am with a card in your hand.
VER_DISPLAY_MAX = 10

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
MSG_CHAN_SP    = 0x80         # MIDI channel 0-15, caller-frame offset
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

# --- the compressor (new in 2.3.6) ---------------------------------------
# The module has a compressor across Out 1 and 2. It ships, it works, and the
# only control the UI gives you is On/Off -- everything else is compiled in.
#
# Unlike every other control here, these are DIRECT FIELD WRITES. The
# compressor object has no set-parameter handler at all, so there is no setter
# to hand a value to. That is safe only because each field is consumed raw:
# the gain computer reads threshold and ratio out of the object every block,
# and nothing is derived or cached from them. See COMPRESSOR-DESIGN.md.
#
# The addresses are RAM, so they cannot be checked directly. Each one is
# instead proved by an instruction in the image that computes it, and those
# are guarded below -- an offset is only as trustworthy as its proof.
# The session is NOT at a fixed address. MANAGER is the engine; the session
# hangs off it as a POINTER, and everything the compressor needs is an offset
# from that pointer's target. Reading it at runtime is the only correct way --
# treating MANAGER as the session lands ~150 KB away, in memory belonging to
# something else entirely.
# CC 40 writes the on/off flag on the SESSION, which is allocated once at boot
# and is therefore a safe target. CCs 41-45 do NOT write the engine at all --
# they write the patch's own store, and hook E stamps that onto whichever
# compressor object the audio is actually processing, every block. That is what
# makes the values survive: hardware testing showed that a direct
# write to the compressor object is undone after a variable period.
# The compressor block listens on ONE channel, unlike every other patched
# global. CC 108-111 get away with answering on all sixteen because they sit in
# 102-119, which the MIDI spec genuinely leaves undefined. 40-45 do not: CC 32-63
# are the fine-adjust LSBs of CC 0-31, so CC 40 is formally the LSB of CC 8.
# Almost nothing sends those, which is why the range looked free -- but gear does
# help itself to the numbers, and a patched global consumes a CC on
# every channel. A stray CC 45 at value 0 sets makeup to -36 dB, which is silence
# and reads exactly like a firmware fault. Confirmed on hardware.
COMP_CHANNEL   = 0            # 0 = MIDI channel 1

CC_COMP_ONOFF  = 40
CC_COMP_FIRST  = 41           # 41 thresh, 42 ratio, 43 attack, 44 release, 45 makeup
CC_COMP_COUNT  = 5
COMP_ONOFF_OFF = 0x28cc0      # session -> on/off byte

# The patch's own parameter store. Chosen from a 60 KB run of AXI SRAM with no
# literal references anywhere in the image, and below the two MPU regions at
# 0x24060000/0x24070000 which are configured for DMA. UNVERIFIED -- "no literal
# references" does not prove "not reached by pointer arithmetic". The magic word
# means a clobber degrades to the feature switching itself off rather than
# stamping garbage into the audio path. See COMPRESSOR-DESIGN.md milestone M1.
# Where the patch keeps its five parameters. This has to be memory the module
# does not use, and "no code mentions it" is NOT enough -- a heap hands out
# addresses it computes at runtime. Beta 4 put the store at 0x24054000, which
# is two-thirds of the way UP the heap; it worked until the heap grew into it,
# then allocations ate the tail of the store and the compressor went silent.
#
#   0x2402e128  heap start   \  malloc's own bounds, read out of its
#   0x24068aa8  heap end     /   literal pool at 0x080bcbe4 / 0x080bcbe0
#   0x240692a8  initial SP -- ARM stacks are full-descending, so nothing
#               above this is ever stack (vector table word 0)
#   0x24080000  end of AXI SRAM
#
# So the 93 KB above the stack pointer is above the heap AND above the stack.
# Nothing in the image references it, and a DMA engine cannot target a buffer
# whose address appears nowhere. The guards below pin all three boundaries, so
# a firmware that moves any of them fails the patch instead of corrupting RAM.
HEAP_LO_VA, HEAP_LO   = 0x080bcbe4, 0x2402e128   # malloc's heap start
HEAP_HI_VA, HEAP_HI   = 0x080bcbe0, 0x24068aa8   # malloc's heap end
STACK_VA,   STACK_TOP = 0x08040000, 0x240692a8   # vector table word 0 = initial SP
SRAM_END              = 0x24080000

PATCH_RAM   = 0x2406c000      # above heap end and above the stack top
PATCH_MAGIC = 0x1010c0de      # +0 magic, +4 thresh, +8 ratio, +12 atk, +16 rel, +20 makeup
PATCH_SUM   = 24              # +24 checksum = magic XOR all five values

# What the compressor comes up as. 1010music ship -4 dB / 4:1 / 10 ms / 250 ms
# / -4 dB, which is a headroom trim rather than a musical setting -- it exists
# so the stock module cannot clip itself, not because it sounds like anything.
# Since the patch has to own these five values anyway, it may as well start
# somewhere useful: gentle bus glue. Every one stays adjustable, and the guide
# gives the numbers to put the stock voicing back on a button.
# Three of these five are literally SSL bus-compressor switch positions -- 2:1,
# 30 ms, 0.3 s -- which is the classic mix-glue setting. The threshold is the one
# the SSL does not give you a number for either: on that unit you turn it until
# the meter shows 2-4 dB of reduction. -12 dB puts us there at sensible levels,
# does less rather than more if the module is run cold, and cannot crush at 2:1
# even if it is badly wrong. Makeup stays at 0 dB so switching the compressor on
# can only ever reduce the level, never clip someone.
DEFAULTS = [
    (struct.unpack("<I", struct.pack("<f", -12.0))[0], "threshold -12.0 dB   70%"),
    (struct.unpack("<I", struct.pack("<f",   2.0))[0], "ratio       2:1       5%"),
    (1440,                                             "attack     30 ms     30%"),
    (14400,                                            "release   300 ms     29%"),
    (struct.unpack("<I", struct.pack("<f",   0.0))[0], "makeup      0.0 dB   50%"),
]
DEFAULT_SUM = PATCH_MAGIC
for _w, _n in DEFAULTS:
    DEFAULT_SUM ^= _w

assert PATCH_RAM > HEAP_HI,   "patch store must sit above malloc's heap"
assert PATCH_RAM > STACK_TOP, "patch store must sit above the initial stack pointer"
assert PATCH_RAM + PATCH_SUM + 4 <= SRAM_END, "patch store must fit in AXI SRAM"

# Field offsets on the compressor object, proved by its own two constructors.
COMP_F_THRESH, COMP_F_RATIO   = 0x428, 0x42c
COMP_F_MAKEUP                 = 0x430
COMP_F_ATTACK, COMP_F_RELEASE = 0x448, 0x44c

# (scale, offset, is_integer) per CC, for a 0..CC_MAX input
COMP_RANGES = [
    (-40.0,     0.0,   False),   # 41 threshold, dB
    (  1.0,    20.0,   False),   # 42 ratio
    ( 24.0,  4800.0,   True ),   # 43 attack,  samples (0.5 .. 100 ms @ 48 k)
    (480.0, 48000.0,   True ),   # 44 release, samples (10 ms .. 1 s @ 48 k)
    (-36.0,    36.0,   False),   # 45 makeup, dB
]

# Finding the session for CC 40. Not by arithmetic -- scan the engine's pointer
# slots and PROVE each candidate by reading back the safety limiter's two timing
# constants. Nothing else in memory carries 192 then 36000 at those offsets, and
# nothing on the module ever writes them.
SCAN_LO, SCAN_HI = MANAGER + 0xa000, MANAGER + 0xa400
RAM_LO, RAM_SPAN = 0x24000000, 0x50000     # every deref is range-checked first
PROOF_A_OFF, PROOF_A_VAL = 0xffa0, 192     # safety-stage attack,  samples
PROOF_R_OFF, PROOF_R_VAL = 0xffa4, 36000   # safety-stage release, samples

# Image guards. The RAM offsets above are only as trustworthy as the
# instructions that prove them, and those are all in the image.
COMP_DISP_VA, COMP_DISP = 0x0809704c, 0x3038f64f   # movw r0,#0xfb38
COMP_FLAG_VA, COMP_FLAG = 0x08097030, 0x00028cc0   # the on/off flag's offset
COMP_INIT_VA, COMP_INIT = 0x080b9d88, 0x6085f504   # add.w r0,r4,#0x428
COMP_DFLT_VA, COMP_DFLT = 0x080b9dc0, 0xc0800000   # the -4.0 default
# movw r8,#0xa204 -- proves where the session pointer is kept on the engine
COMP_SESS_VA, COMP_SESS = 0x08094146, 0x2804f24a

# hook E: the one call site where the engine hands us the live compressor
COMP_DSP_CALL, COMP_DSP_CALL_GUARD = 0x08097056, 0xfd55f022
COMP_DSP_FN = 0x080b9b04

# ---------------------------------------------------------------- the EQ
# 2.3.7-mod. Everything below is read out of the stock image -- see
# EQ-RE-FINDINGS.md, which records how each address was proved.
#
# The EQ node is a live chain node that already runs once per audio block,
# already fetches the audio handle and the frame count, and then never asks for
# the buffers. The arm supplies the arithmetic that is missing.

# The panel is fixed and the vocabulary follows it (D-01): four encoders, each
# with a push button. Encoder 1 selects the mode and its push is master bypass;
# encoders 2-4 are the generic performance controls A, B and C, and their
# pushes are buttons A, B and C. Each mode declares what its three knobs mean.
# Nothing reads a control called sweep, shape or second sweep any more -- those
# names existed only while modes were assumed to need differing control counts.
#
# CC 50-58 sit in the 32-63 fine-adjust LSB range, which other gear on the same
# bus helps itself to. That is exactly why they are gated to channel 1, the
# same exception already established for the compressor at CC 40-45 -- and one
# that cost a hardware session to learn (D-04). Not to be revisited.
EQ_CHANNEL   = 0              # channel 1 only -- CC 50-58 are LSBs, see hook D
CC_EQ_FIRST  = 50             # the whole EQ block, and the only two numbers
CC_EQ_LAST   = 59             # hook F's bounds check cares about
CC_EQ_MODE = 50                                     # mode select, encoder 1
CC_EQ_A, CC_EQ_B, CC_EQ_C = 51, 52, 53              # controls A/B/C, encoders 2-4
CC_EQ_BTN_A, CC_EQ_BTN_B, CC_EQ_BTN_C = 54, 55, 56  # their pushes, binary (D-03)
CC_EQ_BYPASS   = 57           # master bypass, encoder 1's push
CC_EQ_SLOPE    = 59           # filter slope 12 <-> 24 dB/oct, its own button.
                              # Applies to EVERY filter mode, which is why it is
                              # a control of its own and not DJ Filter's button B
                              # as it was through b5.
# CC 58 is RETIRED. It was the self test (D-09), which existed to prove OUR
# biquad was transparent -- and b5 deleted that biquad, so it proved nothing the
# stock handler does not already guarantee. It duplicated master bypass audibly
# and it was a stage hazard: the flag lived in the patch store, not the preset,
# so a preset reload could not clear it and only a power cycle could. Hook F
# still accepts CC 58 because it falls inside the 50..59 window, but its slots
# entry points at EQ_S_SCRATCH, so it writes a dead word and does nothing.
# 59-63 are spare and sit outside CC_EQ_LAST, so hook F ignores them entirely.

# The class, from the constructor's own literal pool.
EQ_VTABLE      = 0x080e259c   # in flash; the ctor stores this address into obj+0
EQ_VT_HANDLER  = EQ_VTABLE + 0x0c
EQ_HANDLER     = 0x080bb220   # what that slot holds, and our tail-branch target

# Object layout. Coefficients are (b2, b1, b0, a1, a2) -- NOT (b0, b1, b2, ...).
EQ_CUR_OFF     = 0x2c         # smoothed coefficients -- what the DSP must read
EQ_STATE_OFF   = 0x54         # 4 floats: left z1/z2 at +0x54/+0x58, right at
                              # +0x5c/+0x60. THE STOCK FILTER'S OWN HISTORY --
                              # it loads these before its sample loop and stores
                              # them after. The ctor zeroes them and nothing
                              # else ever does, not even the stock skip-band
                              # path, which only advances its pointers.
EQ_BAND_STRIDE = 0x38
EQ_PARM_OFF    = 0x10c        # per band, stride 0x14
EQ_TYPE_OFF    = EQ_PARM_OFF          # byte
EQ_GAIN_OFF    = EQ_PARM_OFF + 4      # float, in DECIBELS -- 10^(g/20) inside
EQ_FREQ_OFF    = EQ_PARM_OFF + 8      # float, in HZ -- the designer does 2*pi*f/SR
EQ_Q_OFF       = EQ_PARM_OFF + 0xc    # float, ctor default 1.0
EQ_EN_OFF      = EQ_PARM_OFF + 0x10
EQ_PARM_STRIDE = 0x14

# design(r0 = obj, r1 = band), indexed by type: 0 None, 1 L Cut, 2 L Shelf,
# 3 Param, 4 H Shelf, 5 H Cut.
EQ_DESIGNERS = [0, 0x080ba794, 0x080ba8d8, 0x080bab98, 0x080baf5c, 0x080bae18]
EQ_T_LCUT, EQ_T_LSHELF, EQ_T_PARAM, EQ_T_HSHELF, EQ_T_HCUT = 1, 2, 3, 4, 5

# NOT USED, and deliberately kept as a headstone. The whole DSP tail this arm
# used to carry -- buffer fetch, biquad, filter state, pole and finiteness
# tests -- existed because EQ-RE-FINDINGS.md concluded the stock EQ handler
# "never asks for the buffers ... that one call is the entire gap". It was
# wrong. The EQ's buffer fetch is 0x080a3228; the RE searched for the
# COMPRESSOR's 0x080a3288, one transposed digit away, did not find it, and
# invented a redundant DSP for us to write. The stock handler at 0x080bb504 is
# already a complete two-channel transposed-direct-form-II biquad, gated on the
# band's type at +0x10c and enable at +0x11c -- which is to say, gated on
# exactly the two fields `gsetband` writes.
#
# Worse than redundant: our biquad kept its state in the band's own state words
# at +0x54 and +0x5c, which is where the stock filter keeps ITS state. Two
# recursive filters sharing one state array inside the same block destroyed each
# other's history, and the master bus went silent whenever any band was enabled.
# Hardware-confirmed 2026-09-04: deleting our DSP made every mode and every
# control work. Do not reintroduce a per-sample loop here. Set parameters,
# chain to the stock handler, and let 1010music filter.
# EQ_AUD_HANDLE / EQ_AUD_FRAMES / EQ_AUD_BUFS were 0x080a82b8 / 0x080a31e0 /
# 0x080a3288 respectively.

# The EQ's own block in the patch store, above the compressor's.
#
# Hook F bumps `gen` on every accepted CC. Hook G re-applies the whole mode on
# EVERY audio block, unconditionally, exactly as hook E stamps the compressor's
# store onto its live object every block.
#
# It used to apply only when `gen` differed from `agen`, and that economy gave
# away the property that makes hook E robust. `gen` tracks OUR intent; it says
# nothing about the state of the object. A preset load reconstructs the EQ
# object to constructor defaults while the store still says `agen == gen`, so
# the compare took its early exit forever and the object stayed configured by
# nobody -- audio passing, nothing happening, and structurally invisible to a
# null test, because failure exits to pass-through and pass-through is exactly
# what such a test expects.
#
# The cost of applying every block is BOUNDED, not assumed: 2,512 instructions
# and 5,444 cycles in a 1,280,000-cycle block, 0.43%. D-25's identity fill is
# what makes that tractable -- `gapply` designs exactly four bands on every
# call, in every mode, so the worst case IS the typical case and there is no
# tail. The derivation and its counts are in DEV-README.
EQ_STORE  = PATCH_RAM + 0x40
EQ_MAGIC  = 0x1010e00e
# Ten live words, one per accepted CC, all checksummed. Three bookkeeping
# words that are not. And one dead scratch word, which exists so hook F's
# unreachable slots-table entries have somewhere harmless to point.
EQ_S_MODE  = 0x04             # CC 50, already divided down to a slot number
EQ_S_GA    = 0x08             # CC 51, control A, raw 0..16256
EQ_S_GB    = 0x0c             # CC 52, control B
EQ_S_GC    = 0x10             # CC 53, control C
EQ_S_BTN_A = 0x14             # CC 54, button A, 0 or 1
EQ_S_BTN_B = 0x18             # CC 55, button B
EQ_S_BTN_C = 0x1c             # CC 56, button C
EQ_S_BYP   = 0x20             # CC 57, master bypass, 0 or 1
EQ_S_SLOPE = 0x24             # CC 59, filter slope, 0 = 12 dB/oct, 1 = 24.
                              # Reuses the retired self test's slot rather than
                              # renumbering the store: same width, same checksum
                              # arithmetic, one fewer thing to get wrong.
EQ_S_GEN   = 0x28             # bumped by every accepted CC
EQ_S_SUM   = 0x2c             # the checksum, deliberately outside EQ_S_LIVE
EQ_S_AGEN  = 0x30             # the generation hook G last applied. RECORDED,
                              # not a gate: hook G applies every block. Kept
                              # because a later plan may want a cheap "did
                              # anything change" signal for something other
                              # than skipping the apply.
EQ_S_MASK  = 0x34             # which of the four bands the mode built
EQ_S_SCRATCH = 0x38           # dead, and NOT in EQ_S_LIVE -- see hook F's slots
EQ_S_LIVE = (EQ_S_MODE, EQ_S_GA, EQ_S_GB, EQ_S_GC,
             EQ_S_BTN_A, EQ_S_BTN_B, EQ_S_BTN_C,
             EQ_S_BYP, EQ_S_SLOPE, EQ_S_GEN)
EQ_STORE_END = EQ_STORE + 0x3c
assert EQ_STORE >= PATCH_RAM + 0x20, "EQ store must clear the compressor's"
assert EQ_STORE_END <= SRAM_END, "EQ store must fit in AXI SRAM"

# BUG-01, promoted from a bug into a check. A duplicated constants block once
# bound EQ_S_SUM to 0x14 -- the same offset as EQ_S_GA, Control A's slot -- so
# the checksum was written into a word it checksums and could never agree with
# itself. Hook G took its pass-through exit on every audio block and the EQ
# never engaged, on any mode, on any CC, for the whole of 2.3.7-mod-b2. The
# four below refuse that shape of mistake at import, before a build can start.
# They are cheap and they are not noise; leave them in.
assert len(set(EQ_S_LIVE)) == len(EQ_S_LIVE), \
    "EQ_S_LIVE names the same store offset twice, so the checksum folds that " \
    "word in twice and cancels it. Check the nine EQ_S_* offsets above."
assert EQ_S_SUM not in EQ_S_LIVE, \
    f"EQ_S_SUM ({EQ_S_SUM:#x}) is inside EQ_S_LIVE, so the checksum is stored " \
    "in a word it checksums and can never agree with itself -- hook G would " \
    "exit to pass-through on every audio block and the EQ would never engage. " \
    "This is BUG-01. Give EQ_S_SUM an offset of its own, outside EQ_S_LIVE."
assert len({*EQ_S_LIVE, EQ_S_SUM, EQ_S_AGEN, EQ_S_MASK, EQ_S_SCRATCH}) \
        == len(EQ_S_LIVE) + 4, \
    "one of EQ_S_SUM / EQ_S_AGEN / EQ_S_MASK / EQ_S_SCRATCH sits on a live " \
    "word or on another bookkeeping word. Every store offset must be distinct " \
    "-- two names on one address means one of them is silently overwritten at " \
    "runtime, and EQ_S_SCRATCH landing on a live word would turn hook F's dead " \
    "slots-table entries into real writes that re-checksum the store."
assert EQ_STORE_END - EQ_STORE >= \
    max(EQ_S_LIVE + (EQ_S_SUM, EQ_S_AGEN, EQ_S_MASK, EQ_S_SCRATCH)) + 4, \
    "EQ_STORE_END stops short of the highest store offset, so the fits-in-SRAM " \
    "assert above does not cover every word the store actually writes. Raise " \
    "EQ_STORE_END to at least the top offset plus four."

# Modes. mode = (7-bit value * 10) >> 7, so each of the ten slots is about 12.8
# encoder values wide. The controller sends ten steps over Min 6 / Max 122,
# which centres every step in its window with SIX values of margin either side
# -- the widest margin this design has ever had, because fewer modes means
# wider windows. A full 0..127 sweep would land a step within one value of a
# boundary, which is what made dialling a mode unreliable through b5.
#
# TWO NUMBERINGS, and every confusion in this feature has come from mixing
# them. The enum below is ZERO-based. The dial, and every table in
# EQ-CONTROL-LAYOUT.md, is ONE-based. Both columns are in the table so that
# neither document has to be translated in somebody's head.
#
# A push earns a CC only when it does something the controller cannot. The
# Drop's `Reset Mid` already gives instant neutral on any encoder, so a button
# that merely returns a control to its resting value is redundant. What
# survives is the momentary maximum: held, the value is forced; released, the
# encoder's stored position comes back untouched, with no jump. Every push
# below is that gesture, and the slots that are blank are blank on purpose.
#
# ALL TEN ARE BUILT. The last column is the build each one first shipped in,
# which is more use than a bare "yes" and cannot rot the way a NOT BUILT marker
# does -- four of them were still claiming NOT BUILT three builds after they
# went on the module.
#
# enum dial  mode           A          B          C          pushes: 54 / 55 / 56                      built
# ----------------------------------------------------------------------------------------------------------
#   0    1   DJ Filter      filter     resonance  knee       -            / -              / -               b5
#   1    2   Dual Cut       high cut   low cut    resonance  open HC      / open LC        / -               b5
#   2    3   Band Pass      centre     width      gain       -            / open widest    / -               b5
#   3    4   Mixer EQ       high       mid        low        kill high    / kill mid       / kill low        b5
#   4    5   Tone + Filter  high       low        filter     kill high    / kill low       / filter open     b13
#   5    6   Tilt           tilt       hinge      -          full treble  / -              / -               b12
#   6    7   Notch          frequency  Q          depth      -            / -              / slam depth      b5
#   7    8   Peak           frequency  Q          gain       -            / max Q          / max boost       b12
#   8    9   Multi Notch    sweep      depth      spread     -            / max depth      / widest spread   b14
#   9   10   Formant        vowel      intensity  shift      -            / max intensity  / -               b15
#
# TEN, and it is final (D-26). Three slots went, for three different reasons.
#
#   `Off` went at b6. CC 57 is a working master bypass and did the job better,
#   so a slot spent on it was a dead encoder position.
#
#   Two mid/side modes went at b10, and they are IMPOSSIBLE rather than merely
#   unwritten (D-27). Both need a mid/side matrix, which is per-sample
#   arithmetic on the raw buffers, and D-19 forbids that absolutely. There is
#   no way round it in band parameters either: the EQ object loads ONE
#   coefficient set per band and applies it to BOTH channels -- only the
#   recursive state is per-channel -- so band parameters cannot express a
#   left-right difference at any setting. Do not re-open this, and do not
#   propose a per-sample loop for it.
#
#   The tone-shaping mode recorded as the fallback for the slot those two
#   vacated went with them (D-28). Tilt already covers tone shaping, and ten
#   modes that each earn their place beat eleven with a passenger.
#
#   Neither the two nor the fallback is named here, on purpose. A name in a
#   comment is a name a future session will try to build, and this table is the
#   first place it would look. EQ-CONTROL-LAYOUT.md carries the record of what
#   was dropped and why; that is the one place any of them should appear.
#
# Nothing renumbered when twelve became ten: the two that went sat past every
# other slot, so every built mode kept its value and `gapply`'s five compares
# were untouched. EQ-CONTROL-LAYOUT.md is the authority here (D-31) and it
# supersedes D-08 and MASTER-EQ-DESIGN.md section 5.
(EQ_M_DJ, EQ_M_DUALCUT, EQ_M_BANDPASS, EQ_M_MIXER, EQ_M_TONE,
 EQ_M_TILT, EQ_M_NOTCH, EQ_M_PEAK, EQ_M_MULTINOTCH, EQ_M_FORMANT) = range(10)
# SLOT 8 IS CALLED **PHASER** ON THE PANEL. The internal name stays
# MULTINOTCH -- and so do `gmmnot`, `gmnsp`, `gqmn` and the EQ_MN_ constants --
# for two reasons, neither of them inertia. First, b14 is flashed and has a
# hardware verdict against sha256 f0c1a144..., and the safest way to keep that
# verdict describing a rebuildable image is not to touch the emitting source at
# all. Second, `ph` is already Band Pass's: `gmphone`, `gqph` and
# EQ_PHONE_HALF are the telephone band, and a `gqphs` one character away from
# `gqph` is a collision waiting to be misread. Internal names differing from
# panel names is already this file's normal -- it says "control A" where the
# panel says "encoder 2". D-15 renamed the SLOT from Phaser to Multi Notch on a
# DSP argument, which still holds (see `gmmnot`); the performer renamed it back
# on an audible one, which is a different question and theirs to settle.
EQ_M_SLOTS = 10               # and the divisor: mode = (value * 10) >> 7
EQ_BOOT_MODE = EQ_M_DJ        # slot 0, and the boot mode

EQ_CC_MAX    = 127 << 7       # 16256, what the arm can actually be sent
EQ_CENTRE    = EQ_CC_MAX // 2 # 8128
EQ_DETENT    = 128            # one 7-bit step either side of centre
EQ_HALF_SPAN = EQ_CENTRE - EQ_DETENT

# The values a button substitutes for a control's stored one (D-03). They are
# in the RAW CC DOMAIN on purpose, exactly as if the encoder had been turned
# there: a forced value goes on to take the same gu16 / gftab / gqmap / ggain
# path the encoder's value would have taken, so a mode cannot accidentally give
# its forced case a different curve from its swept case, and the sweep that
# already covers the control range already covers the forced value too.
# Naming them beats repeating the literals, because "why 8128" is the question
# somebody asks in six months and EQ_FORCE_CENTRE answers it in the name.
EQ_FORCE_CENTRE = EQ_CENTRE   # the detent: flat gain, or a real filter bypass
EQ_FORCE_MAX    = EQ_CC_MAX   # the top of a control: filter open, widest
EQ_FORCE_MIN    = 0           # the bottom: fully closed, zero depth, no tilt

# Sweep range. The low end is NOT arbitrary: a two-pole section's stability
# margin is 2(1-cos w)/(1+alpha), which collapses as the corner approaches DC --
# 114 float32 ULP at 40 Hz, 29 at 20 Hz, 7 at 10 Hz. The top is the designer's
# own clamp: it compares 2*pi*f/SR against pi/2, so SR/4 is the ceiling.
EQ_F_LO, EQ_F_HI = 40.0, 12000.0
EQ_TBL_N = 16                 # 17 entries; worst linear-interp error 1.6%

# Band Pass half-width, in gftab table steps. EQ_BP_WIDE is half the table, so
# a centred width control puts both edges at or past the ends of the range and
# gftab clamps them there -- which is what makes centre transparent (Rule 1).
# EQ_BP_NARROW is the tightest the clockwise end reaches; it is deliberately
# not zero, because a zero-width band pass is two coincident cuts and that is
# exactly the silence Dual Cut used to produce.
# DJ Filter's variable knee, in gftab table steps: how far the second section
# is spread from the first at the anticlockwise end of control C. Four steps is
# a little over an octave and a half on this table, which is enough to hear the
# transition soften without the second section wandering out of the audible
# part of the sweep.
EQ_DJ_SPREAD = 4.0

EQ_BP_WIDE   = 8.0
EQ_BP_NARROW = 0.4
EQ_FTABLE = [EQ_F_LO * (EQ_F_HI / EQ_F_LO) ** (i / EQ_TBL_N) for i in range(EQ_TBL_N + 1)]

EQ_Q_LO, EQ_Q_HI       = 0.707, 8.0     # the filter modes' resonance
EQ_QN_LO, EQ_QN_HI     = 1.0, 20.0      # the notch, which wants to get narrower
EQ_Q_FIXED             = 0.707          # cascaded second section, and the shelves
EQ_Q_BELL              = 0.9            # mixer mid, and the telephone band edges

# Mixer EQ gain, asymmetric: full cut to a near-kill, limited boost, flat at
# centre. Two linear segments so the centre detent really is 0 dB.
EQ_G_CUT, EQ_G_BOOST = -24.0, 9.0

# Tilt's gain law is SYMMETRIC where ggain is deliberately not, and the reason
# is what a tilt control is FOR. Its two shelves have to cancel: equal and
# opposite is the whole point, so a clockwise tilt lifts the treble by exactly
# what it drops the bass, and the total loudness stays roughly put instead of
# swelling. Reuse ggain and the two halves stop cancelling -- -24 dB of bass
# against +9 dB of treble is not a tilt, it is a bass cut with a garnish, and
# every position off centre would read as a volume change.
#
# 9.0 dB, not some new number: it is EQ_G_BOOST's own ceiling, so Tilt
# introduces no extreme this patch has not already swept, designed and played.
# The magnitude is Claude's Discretion under D-17 and the sweep covers whatever
# it is set to; what is NOT discretionary is the symmetry.
EQ_TILT_G = 9.0
EQ_MIX_F  = (150.0, 700.0, 4000.0)      # low shelf, mid bell, high shelf
EQ_NOTCH_G   = -24.0

# Multi Notch, called PHASER on the panel. Four notches swept together, sharing
# one depth and one Q, spaced by a fixed number of gftab table steps.
#
# ONE Q FOR ALL FOUR, and fixed because there is no fourth encoder: the panel is
# sweep, depth and spread, and every one of them earns its place. 4.0 is narrow
# enough that four of them read as notches rather than as a general dulling, and
# wide enough that they stay audible while the stack is moving.
EQ_MN_Q = 4.0

# The spread range, in table steps. `gftab`'s table is LOGARITHMIC, so a fixed
# number of steps is a fixed RATIO between neighbours -- which is what keeps the
# spacing geometric and stops it ever being a comb's arithmetic harmonic series.
# One step is (12000/40)^(1/16) = 1.4283.
#
#   spread, steps | neighbour ratio | nearest simple interval | distance
#   --------------|-----------------|-------------------------|----------
#   0.35          | 1.132           | about two semitones     | --
#   1.0           | 1.428           | tritone, 1.414          | +17 cents
#   1.675 (rest)  | 1.817           | between a minor 7th and an octave | dissonant
#   1.944         | 2.000           | THE OCTAVE              | the comb zone
#   3.0           | 2.914           | octave and a fifth, 3:1 | -50 cents
#
# `gqmap` is linear over the whole control range, so the resting spread is the
# midpoint, 1.675 steps -- a neighbour ratio of 1.817, which is genuinely
# dissonant and is the position most likely to read as a moving sweep. The
# octave, the setting most likely to read as a STATIC TUNED COMB, sits about
# 60% clockwise: reachable on purpose, never where the knob rests.
#
# The low end is deliberately NOT zero. Four coincident notches are one deeper
# notch pretending to be four, which is the same category of mistake as Dual
# Cut's zero-width pass band.
#
# No offline gate has an opinion on any of this. R2 and the smoother-path check
# are numerical stability tests with no concept of musical consonance, so where
# this range reads as a sweep and where as a comb is an ears-only judgement.
EQ_MN_SP_LO, EQ_MN_SP_HI = 0.35, 3.0

# Formant, called FORMANT on the panel (slot 9, dial position 10). Three bells
# parked on the first three resonances of a human vocal tract, morphing
# continuously along one path through vowel space.
#
# THESE FIFTEEN NUMBERS ARE THE ONLY CONSTANTS IN THIS FILE THAT COME FROM
# OUTSIDE THIS PROJECT. Every other number here was either read out of the
# stock image or chosen against something the repository can measure. These are
# measurements of human speakers, and NOTHING in the simulator, the R2 sweep,
# the emitted-code gate or the smoother-path check can tell a right one from a
# wrong one. A mistyped formant produces a mode that sweeps convincingly and
# does not sound like speech, which at the bench is indistinguishable from a DSP
# bug. So the provenance is written down at the same length as the numbers.
#
# --- SOURCE -----------------------------------------------------------------
#
# Peterson, G. E. and Barney, H. L. (1952), "Control Methods Used in a Study of
# the Vowels", Journal of the Acoustical Society of America 24(2), 175-184.
#
# The rows below were NOT copied from a secondary table. They were COMPUTED, on
# 2026-09-05, from that study's own measurements: `verified_pb.data` in the CMU
# Artificial Intelligence Repository's `areas/speech/database/pb/` package,
#
#   www.cs.cmu.edu/afs/cs/project/ai-repository/ai/areas/speech/database/pb/pb.tgz
#
# which is 1,520 rows -- 76 speakers x 10 vowels x 2 repetitions -- of speaker
# class, speaker id, vowel, F0, F1, F2 and F3 in Hz. It reached CMU from the
# University of Pennsylvania and is the file documented in Watrous, JASA 89
# (May 1991). Each row below is the MEAN OVER THE 33 MEN, sixty-six utterances
# per vowel, rounded to the nearest 10 Hz.
#
# The method is recorded rather than only the citation, so anybody who doubts a
# number can regenerate the whole table in four lines instead of arguing about
# it: filter the file to class 1, group by the ARPABET column, average columns
# 6, 7 and 8.
#
# --- HOW THE VOWELS ARE NAMED HERE ------------------------------------------
#
# THIS FILE IS PURE ASCII and IPA is not, so every vowel below is named three
# ways that are all unambiguous: the dataset's own ARPABET code, the keyword
# Peterson and Barney recorded it in, and the letter this mode's knob shows.
# ARPABET is the one to trust in an argument -- it is what the data file
# itself says, and it needs no transcription convention.
#
#   knob  ARPABET  keyword
#   I     IY       heed
#   E     EH       head
#   A     AA       hod
#   O     AO       hawed
#   U     UW       who'd
#
# --- WHAT WAS CHECKED AGAINST WHAT ------------------------------------------
#
# The computed means agree with the average-male table that is quoted
# second-hand throughout the literature, within one 10 Hz rounding step on every
# value except one:
#
#   vowel        computed here        commonly quoted      difference
#   IY  heed     270  2290  2940      270  2290  3010      F3, 70 Hz (2.4%)
#   EH  head     530  1850  2480      530  1840  2480      F2, 10 Hz
#   AA  hod      720  1090  2440      730  1090  2440      F1, 10 Hz
#   AO  hawed    570   840  2400      570   840  2410      F3, 10 Hz
#   UW  who'd    310   880  2240      300   870  2240      F1, 10 Hz
#
# The one real gap is /i/'s third formant, and it is the least load-bearing
# number in the table: F3 of the brightest vowel, a 6 dB bell 2.4% off a
# frequency the shift control moves by an octave either way. The computed value
# ships, because it is the one derived from the primary data.
#
# --- THE LABEL DISCREPANCY, AND HOW IT WAS RESOLVED -------------------------
#
# The phase research found a course PDF whose second vowel box was captioned as
# the O vowel but carried 530/1840/2480, and separately proposed 570/840/2410
# for O from memory. Those two claims cannot both be right, and the second was
# flagged as unsourced. A row in that state must not reach firmware.
#
# BOTH ARE NOW SETTLED, AND THE HYPOTHESIS THE PLAN OFFERED IS CONFIRMED. In
# Peterson and Barney's own ten-vowel set, "head" and "hawed" are DIFFERENT
# vowels with different rows -- the dataset's own HEADER lists them separately,
# as `3 EH` and `7 AO`. Computed from the primary data, EH is 530/1850/2480 and
# AO is 570/840/2400. So the course PDF's caption was wrong and its numbers were
# right: they are the EH row. And the row proposed from memory for O turns out
# to be the real AO row, almost to the hertz. Nothing here now rests on that
# memory -- the shipped value was recomputed from the measurements -- but it is
# worth recording that the two sources agreed once they were correctly labelled.
#
# There is a documented mechanism for exactly this confusion, and it is worth
# recording because the next reader will find the same PDF. Praat's manual page
# for "Create formant table (Peterson & Barney 1952)" notes that the IPA
# notation in Watrous (1991) -- the transcription nearly every redistribution of
# this dataset descends from, including the CMU copy used here -- differs from
# Peterson and Barney's own for three vowels, EH, AO and ER. Where the original
# writes an open-mid symbol, Watrous writes a close-mid one. A table that
# inherits one convention and a caption that inherits the other is how an EH row
# ends up wearing an O label.
#
# THIS COPY SIDESTEPS THE WHOLE QUESTION by naming vowels in ARPABET and taking
# the numbers from the measurements rather than from any caption, so neither
# convention can bite it.
#
# --- THE ORDER IS D-29, AND THE DATA AGREES WITH IT -------------------------
#
# I -> E -> A -> O -> U, front to back, which is the vowel chart traversed once
# rather than crossfaded across. That matters here more than anywhere: EVERY
# knob position between two rows is an interpolation, so adjacency decides what
# the in-between positions sound like. Adjacent vowels interpolate into a real
# intermediate vowel; unrelated ones interpolate into a chord of two.
#
# The measurements bear the ordering out. F2 -- the front-back formant -- falls
# right down the column: 2290, 1850, 1090, 840, 880. The one step that does not
# fall is U, forty hertz above O, and that is the data being honest rather than
# a fault: /u/ is distinguished from /O/ mostly by a much lower F1 (310 against
# 570) and by lip rounding, not by F2. The last step of the morph is carried by
# F1, and it should still read as a move further back.
#
# This supersedes the A -> E -> I -> O -> U ordering that survives in older
# proposal tables. Correct it wherever it is still found.
EQ_FMT_TABLE = (
    (270.0, 2290.0, 2940.0),   # I  IY  "heed"
    (530.0, 1850.0, 2480.0),   # E  EH  "head"
    (720.0, 1090.0, 2440.0),   # A  AA  "hod"
    (570.0,  840.0, 2400.0),   # O  AO  "hawed"
    (310.0,  880.0, 2240.0),   # U  UW  "who'd"
)

# Q and gain per formant, F1 first. UNLIKE THE FREQUENCIES, THESE SIX ARE NOT
# MEASUREMENTS -- they are starting points under Claude's Discretion, gated by
# the D-17 sweep for safety and by ears for whether it sounds like a voice.
# Nothing offline in this project can judge either.
#
# The shape of both ladders is the argument, not the exact values:
#
#   F1 is the loudest and the broadest. It sits closest to where the rest of the
#   spectral energy already is, so it reads as the BODY of the vowel; a narrow
#   F1 reads as a whistle sitting on top of the music instead of as a voice
#   inside it.
#
#   F2 and F3 are narrower and quieter, in a declining ladder, which is what
#   natural vocal-tract spectral tilt does. Boosting them equally makes
#   something bright and sibilant rather than something spoken.
#
# The Qs are DELIBERATELY far lower than a real vocal tract's. A measured
# formant has a bandwidth around 40-110 Hz, which is a Q of 10 to 20; the Csound
# manual's formant table (Table III, Piche and Nix) gives exactly that range.
# Those are the numbers for SYNTHESISING a voice out of silence. This is an EQ
# boosting a signal that is already there, and a Q of 15 on a mix is nearly
# inaudible -- the boost falls between the partials. 4 / 7 / 9 is the audible
# end of the same shape.
#
# If it does not read as a vowel on hardware, RAISE THE QS FIRST -- that is the
# axis this compromise is on, and it is one edit and one sweep.
EQ_FMT_Q = (4.0, 7.0, 9.0)
EQ_FMT_G = (12.0, 9.0, 6.0)

# The shift control's two stops: exactly one octave down and one octave up, with
# a true 1.0 in the centre detent. An octave is the right span because it is the
# whole plausible range of vocal-tract lengths and then some -- it moves the
# apparent size of the mouth without leaving the territory where three bells
# still read as a voice.
EQ_FMT_DN, EQ_FMT_UP = 0.5, 2.0

# THE RANGE ASSERT, AND WHY IT IS HERE RATHER THAN IN A TEST. This is the only
# mode that writes frequencies into the object in HERTZ that did not come out of
# `gftab`. `gftab` clamps to the table's ends, so every other mode is protected
# by construction; these fifteen values, multiplied by the shift, go straight to
# 1010music's designer. EQ_F_LO is where a two-pole section's stability margin
# starts collapsing and EQ_F_HI is the designer's own SR/4 ceiling, so an
# out-of-window frequency is not a cosmetic problem.
#
# It runs at import, so it fires on EVERY build rather than when somebody
# remembers to check. Same reason as the four EQ_S_* asserts above.
assert len(EQ_FMT_TABLE) == 5 and all(len(r) == 3 for r in EQ_FMT_TABLE), \
    "EQ_FMT_TABLE must be five vowels of three formants, in D-29's I E A O U " \
    "order -- the row interpolation indexes it as a flat fifteen-float array " \
    "with a twelve-byte stride and cannot see a ragged one."
assert len(EQ_FMT_Q) == 3 and len(EQ_FMT_G) == 3, \
    "EQ_FMT_Q and EQ_FMT_G are per FORMANT, not per vowel -- three each."
assert min(min(r) for r in EQ_FMT_TABLE) * EQ_FMT_DN >= EQ_F_LO, \
    f"a formant shifted fully down reaches " \
    f"{min(min(r) for r in EQ_FMT_TABLE) * EQ_FMT_DN:.1f} Hz, below EQ_F_LO " \
    f"({EQ_F_LO}). This mode writes Hz straight into the object, so gftab's " \
    "clamp is not there to catch it and the designer sees the value directly."
assert max(max(r) for r in EQ_FMT_TABLE) * EQ_FMT_UP <= EQ_F_HI, \
    f"a formant shifted fully up reaches " \
    f"{max(max(r) for r in EQ_FMT_TABLE) * EQ_FMT_UP:.1f} Hz, above EQ_F_HI " \
    f"({EQ_F_HI}), which is the designer's own SR/4 ceiling."

EQ_NULL_FREQ = 1000.0
EQ_PHONE_HALF = 6.0           # telephone half-width, in table steps (~0.5 oct each)

# Guards -- every address above is only as good as the instruction that proves it.
EQ_VT_GUARD_VA, EQ_VT_GUARD = EQ_VT_HANDLER, EQ_HANDLER | 1
EQ_CTOR_VA,   EQ_CTOR_GUARD = 0x080ba5da, 0x46056005   # str r5,[r0] -- vtable store
EQ_DSGN_VA,   EQ_DSGN_GUARD = 0x080bae18, 0x0381eb01   # H Cut entry
EQ_COEF_VA,   EQ_COEF_GUARD = 0x080baef8, 0x6a12edc1   # vstr s13,[r1,#0x48] -- b0 slot
EQ_KONST_VA,  EQ_KONST      = 0x080baf40, 0x40c90fdb   # 2*pi -- proves freq is in Hz
EQ_GAINK_VA,  EQ_GAINK      = 0x080badcc, 0x3d4ccccd   # 0.05 -- proves gain is in dB
EQ_HAND_VA,   EQ_HAND_GUARD = 0x080bb23a, 0x46288bc1   # ldrh r1,[r0,#0x1e] / mov r0,r5

# --- no module-scope constant may be bound twice -------------------------
# The other half of BUG-01, and the half that let it happen at all. A whole
# constants block had been duplicated verbatim; Python binds top to bottom, so
# the later copy's stale values won, silently, and nothing said a word. This
# reads the file's own source and refuses any module-scope name assigned more
# than once, naming every binding's line.
#
# It lives here rather than with vprint and refuse further down because it has
# to RUN here -- after the constants are bound and before the first structure
# built out of them -- and Python will not call a function it has not read yet.
# It is stdlib only, and it skips itself without complaint if the source cannot
# be read: this script ships to users and must never fail because it could not
# find itself on disk.
#
# Only a genuinely intended re-binding belongs in REBIND_OK, and each needs its
# reason written beside it. Empty is the correct state. Adding a name is a
# decision; widening the check until it says nothing is how BUG-01 comes back.
REBIND_OK = frozenset()


def _refuse_shadowed_constants(path):
    """Stop if any module-scope name in `path` is assigned more than once.

    Walks tree.body only, so a local inside a function that happens to share a
    constant's name is not a finding. Tuple unpacking counts -- the EQ store
    offsets are written that way, and that is exactly where BUG-01 lived.
    """
    try:
        with open(path, encoding="utf-8") as f:
            tree = ast.parse(f.read(), path)
    except (OSError, TypeError, ValueError, SyntaxError):
        return                                  # cannot read itself: say nothing

    def names(node):
        if isinstance(node, ast.Name):
            yield node.id, node.lineno
        elif isinstance(node, (ast.Tuple, ast.List)):
            for elt in node.elts:
                yield from names(elt)

    bound = collections.defaultdict(list)
    for stmt in tree.body:                      # module scope only
        if isinstance(stmt, ast.Assign):
            for target in stmt.targets:
                for name, line in names(target):
                    bound[name].append(line)
        elif isinstance(stmt, ast.AnnAssign) and stmt.value is not None:
            for name, line in names(stmt.target):
                bound[name].append(line)

    dup = {n: ls for n, ls in bound.items() if len(ls) > 1 and n not in REBIND_OK}
    assert not dup, (
        "these module-scope constants are each bound more than once, and the "
        "LAST binding is the one that wins, silently: "
        + "; ".join(f"{n} at lines " + ", ".join(str(l) for l in ls)
                    for n, ls in sorted(dup.items()))
        + ". That is exactly BUG-01's shape -- a duplicated block whose stale "
          "values overrode the live ones. Delete the duplicate, or, if the "
          "re-binding is genuinely intended, add the name to REBIND_OK above "
          "with the reason.")


_refuse_shadowed_constants(globals().get("__file__"))

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


# Windows has no "python3" -- there the command is plain "python", and printing
# the wrong one sends a first-timer chasing a command that does not exist on
# their machine, at the exact moment they are already unsure.
PY = "python" if os.name == "nt" else "python3"


USAGE = f"""
  Community enhancements for Bitbox Micro
  ---------------------------------------

  Adds MIDI control of the master bus compressor -- threshold, ratio, attack,
  release and makeup gain, none of which the module exposes to anyone -- plus
  the five unmodulatable granular controls, the per-pad delay and reverb sends,
  reverse, envelope sustain, loop mode and crossfade, play through, the three
  LFO controls, the global delay switches and filter, and per-pad stop.

  You need your own copy of the stock 2.3.4 firmware, from
  https://1010music.com/downloads -- none of it is included here. It arrives
  as MICRO234.zip; unzip it and you get MICRO.BIN.

    mkdir patched
    {PY} patch_micro.py MICRO.BIN patched/MICRO.BIN

  The output must be called MICRO.BIN for the module to find it, which is why
  it goes in its own folder -- writing over your only stock copy would leave
  you no way back.

  A third argument sets the version string on the splash screen. Keep it to
  {VER_DISPLAY_MAX} characters or fewer -- the slot stores 15 but the panel shows
  fewer than that, and a truncated version means you cannot tell which image
  is on the card.

    -v                 show the addresses and opcodes as it works
    --legacy-version   allow a version string too long for the panel, for
                       rebuilding a pre-b11 image to check its fingerprint

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
    "hookD": ("03b4ddf888c0bcf1000f40f07f80ddf88cc0bcf1280f04d0acf12903042b2cd974e04cf67810c2f202404cf67851c2f202410268a2f11053b3f5a02f0cd24ff6a073d358b3f1c00f06d14ff6a473d35848f6a04c634503d004308842e9d355e0ddf890c0bcf5005fb4bf0023012348f6c041c0f20201535448e04cf20002c2f2064211684cf2de0cc1f2100c61450ed0c2f800c01fa002f1040102f1180c90ed000a81ed000a043004316145f7d3ddf890c000ee10cab8ee400a03eb43001aa101eb8001d1ed000a91ed011a886820ee200a30ee010a002801d0bceec00a02eb830080ed010ad2f800c053688cea030c93688cea030cd3688cea030c13698cea030c53698cea030cc2f818c003bc0000000000bf000040c100000040a005000040380000000000008002203b000020c2000000006002983a0000803f000000005542953e0000c04101000000e7a239400000f043010000004002903b000010c200000000",
        {'$t': 0, 'dout': 268, 'donoff': 34, 'dparam': 122, 'dscan': 50, 'dnextc': 88, 'dfound': 96, 'dseeded': 174, 'dstock': 276, 'dseed': 158, 'dscale': 296, 'dstore': 222, '$d': 270, '_sD': 0, 'dnext': 270}),
    "hookE": ("4cf20002c2f2064213684cf2de0cc1f2100c634519d0c2f800c040f20003ccf24013536040f20003c4f20003936040f2a053d36043f64003136140f2000353614ff63e53c9f25013936153688cea030c93688cea030cd3688cea030c13698cea030c53698cea030c936963450ed15368c0f828349368c0f82c34d368c0f848341369c0f84c345369c0f8303400000000",
        {'$t': 0, 'everify': 74, 'eout': 140, '$d': 140, '_sE': 0, 'enext': 140}),
    "hookF": (
        "03b4ddf888c0bcf1000f40f09580ddf88cc0acf13203092b00f28e804cf24002"
        "c2f2064211684ef20e00c1f21000814226d0106040f20001516041f6c0719160"
        "41f6c071d16041f6c071116140f20001516140f20001916140f20001d16140f2"
        "0001116240f20001516240f2010191620021116351634ff6cf71c1f21001d162"
        "2499002b05d10a2001fb00f1890b51602fe0072b06d1b1f5005fb4bf00210121"
        "116226e0092b06d1b1f5005fb4bf0021012151621de0042b06d1b1f5005fb4bf"
        "00210121516114e0052b06d1b1f5005fb4bf0021012191610be0062b06d1b1f5"
        "005fb4bf00210121d16102e014a0c05c1150916a013191621068536880ea0300"
        "936880ea0300d36880ea0300136980ea0300536980ea0300936980ea0300d369"
        "80ea0300136a80ea0300536a80ea0300936a80ea0300d06203bc0000000000bf"
        "38080c10383838383838",
        {'$t': 0, 'fout': 312, 'fseeded': 128, 'fnotmode': 146, 'fbump': 242, 'fnotbyp': 164, 'fnotslope': 182, 'fnotba': 200, 'fnotbb': 218, 'fnotbc': 236, 'fplain': 236, 'fslots': 320, '$d': 314, '_sF': 0, 'fnext': 314}),
    "hookG": (
        "2de9f0412ded068b04460d464cf24006c2f2064633684ef20e00c1f210008342"
        "27d03246106040f20001516041f6c071916041f6c071d16041f6c071116140f2"
        "0001516140f20001916140f20001d16140f20001116240f20001516240f20101"
        "91620021116351634ff6cf71c1f21001d1623068736880ea0300b36880ea0300"
        "f36880ea0300336980ea0300736980ea0300b36980ea0300f36980ea0300336a"
        "80ea0300736a80ea0300b36a80ea0300f36a984240f00480b06a306300f0fcf8"
        "20462946bdec068bbde8f0410000000000b500eb800204eb820282f80c1182ed"
        "441a82ed450ac2ed460a0123c2f81c310ff6dc1353f8213001462046984700bd"
        "002000ee900af8ee600ab4eee00af1ee10fa01d5b0ee600af3ee000ab4eee00a"
        "f1ee10fa01ddb0ee600afceec00a10ee900a0f2888bf0f2000ee900af8ee600a"
        "70ee600a0ff6942101eb800191ed001ad1ed011a71eec11aa1eea01ab0ee410a"
        "704741f6c071401a48bf002000ee100ab8ee400a0ff6f011d1ed000a20ee200a"
        "f7ee000ab4eee00af1ee10fa01ddb0ee600a704700ee100ab8ee400a0ff6c011"
        "d1ed000a20ee200a704791ed000ad1ed010a01ee100ab8ee411a0ff6a012d2ed"
        "001a21ee211aa1ee200a704741f6c071421a1346002bb8bf5b42802b18d9803b"
        "00ee103ab8ee400a0ff66c11d1ed000a20ee200a002a04dc0ff65011d1ed000a"
        "03e00ff64c11d1ed000a20ee200a7047002300ee103ab8ee400a704741f6c071"
        "421a1346002bb8bf5b42802b15d9803b00ee103ab8ee400a0ff61c11d1ed000a"
        "20ee200a0ff60c11d1ed000a20ee200a002a01dcb1ee400a7047002300ee103a"
        "b8ee400a704741f6c071421a1346002bb8bf5b42802b1cd9803b00ee103ab8ee"
        "400a0ff6d401d1ed000a20ee200a002a04dc0ff6ec01d1ed000a03e00ff6e401"
        "d1ed000ab7ee001aa0ee201ab0ee410a7047b7ee000a704700b5002000eb8001"
        "04eb8101002281f80c21c1f81c2101300428f3d300227263336a002b40f0dd83"
        "7368002b00f01d80012b00f08981022b00f0fd81032b00f09380042b00f0d280"
        "052b00f0bf82062b00f08f82072b00f0e982082b00f01783092b00f05f8300f0"
        "bcbbb26841f6c073d21a10460028b8bf4042802840f2b183803800ee100ab8ee"
        "400a0ff61401d1ed000a20ee200af7ee000ab4eee00af1ee10fa01ddb0ee600a"
        "002a05dc0527f7ee000a30eec00a00e00127f3ee000a20ee200ab0ee409afff7"
        "bffeb0ee408af0680ff29471fff70dfff0ee408a00203946b0ee480af0ee680a"
        "002301ee103afff793fe01227263736a002b01d1306903e043f68070c0f20000"
        "fff7e8fef7ee000ab4eee00af1ee10fac0f26383f3ee000a30eec00a0ff28472"
        "d2ed000a20ee200a052f02d039ee400a01e039ee000afff783feb0ee408a0120"
        "3946b0ee480a0ff23c72d2ed000a002301ee103afff75cfe0322726300f03dbb"
        "00f004f80722726300f037bb00b500270ff2b86000eb870090ed008a1c20a0eb"
        "87003058002811d11020a0eb87003058fff7bcfeb0ee409a0ff29c6000eb8700"
        "d0ed008a0ff28060c15d0ee00ff2986000eb8700d0ed008a0ff2986000eb8700"
        "90ed009a0ff27c60c15d3846b0ee480af0ee680ab0ee491afff71afe0137032f"
        "c6d300bd0ff2446090ed008ab369002b0cd1f068fff78afeb0ee409a0ff23860"
        "d0ed008a0ff2206001780ae00ff23860d0ed008a0ff23c6090ed009a0ff22460"
        "01780020b0ee480af0ee680ab0ee491afff7eefd0ff2f45090ed028a7369002b"
        "0cd1b068fff762feb0ee409a0ff2e850d0ed028a0ff2d05081780ae00ff2e850"
        "d0ed028a0ff2ec5090ed029a0ff2d45081780120b0ee480af0ee680ab0ee491a"
        "fff7c6fd03227263f369002b01d1326903e041f6c072c0f2000241f6c073d21a"
        "10460028b8bf4042802840f29682803800ee100ab8ee400a0ff2dc51d1ed000a"
        "20ee200af7ee000ab4eee00af1ee10fa01ddb0ee600a002a05dc0527f7ee000a"
        "30eec00a00e00127f3ee000a20ee200afff7a6fdb0ee408a0ff28852d2ed008a"
        "02203946b0ee480af0ee680a002301ee103afff77dfd07227263736a002b00f0"
        "5c8203203946b0ee480af0ee680a002301ee103afff76cfd0f22726300f04dba"
        "30690ff21c51fff7d0fdf0ee408a7369002b01d1b06803e041f6c070c0f20000"
        "fff79ffdf3ee000a20ee200af3ee000a30eec00afff764fdb0ee408af0ee409a"
        "00200521b0ee480af0ee680a002301ee103afff73dfdb369002b01d1f06803e0"
        "41f6c070c0f20000fff77bfdf3ee000a20ee200afff744fdb0ee409ab0ee408a"
        "01200121b0ee480af0ee680a002301ee103afff71dfd03227263736a002b00f0"
        "fc81b0ee698a0ff29c42d2ed008a02200521b0ee480af0ee680a002301ee103a"
        "fff706fdb0ee498a0ff27842d2ed008a03200121b0ee480af0ee680a002301ee"
        "103afff7f5fc0f22726300f0d6b9b068fff750fdb0ee409ab369002b01d1f068"
        "03e041f6c070c0f20000fff72afd0ff26041d1ed000a20ee200a0ff25041d1ed"
        "000a70eec09a0ff22042d2ed008a39ee690afff7e5fcb0ee408a00200121b0ee"
        "480af0ee680a002301ee103afff7c0fc0ff2f432d2ed008a39ee290afff7d0fc"
        "b0ee408a01200521b0ee480af0ee680a002301ee103afff7abfc032272633069"
        "41f6c073c01a0028b8bf404280281ad9b0ee490afff7b4fcb0ee408a0ff2a832"
        "d2ed008a3069fff711fdb0ee401a02200321b0ee480af0ee680afff789fc0722"
        "726300f06ab9736a002b00f066810ff27432d2ed008a39ee690afff791fcb0ee"
        "408a02200121b0ee480af0ee680a002301ee103afff76cfc0ff24832d2ed008a"
        "39ee290afff77cfcb0ee408a03200521b0ee480af0ee680a002301ee103afff7"
        "57fc0f22726300f038b9b068fff7b2fcfff766fcb0ee408af0680ff2ec21fff7"
        "b4fcf0ee408af369002b01d1306903e043f68070c0f20000fff783fc0ff21431"
        "d1ed000a20ee209a00200321b0ee480af0ee680ab0ee491afff72afc01227263"
        "00f00bb97369002b01d1b06803e043f68070c0f20000fff7c1fcb0ee409af068"
        "fff778fcfff72cfcb0ee408a0ff29422d2ed008a00200221b0ee480af0ee680a"
        "b1ee491afff704fc01200421b0ee480af0ee680ab0ee491afff7fafb03227263"
        "00f0dbb8b068fff755fcfff709fcb0ee408ab369002b01d1f06803e043f68070"
        "c0f200000ff21821fff74ffcf0ee408af369002b01d1306903e043f68070c0f2"
        "0000fff753fcb0ee409a00200321b0ee480af0ee680ab0ee491afff7c9fb0122"
        "726300f0aab8b068fff724fcf0ee409af369002b01d1306903e043f68070c0f2"
        "00000ff2cc11fff720fcb0ee40aab369002b01d1f06803e043f68070c0f20000"
        "fff7effb0ff2ec11d1ed000a20ee209a0ff2a411d1ed008a0f227263002700ee"
        "107ab8ee400af7ee080a30ee600a20ee0a0a30ee290afff7a3fbb0ee408a3846"
        "0321b0ee480af0ee680ab0ee491afff77ffb0137042fe2d300f05fb83069fff7"
        "42fcf0ee408ab369002b01d1f06803e043f68070c0f20000fff7b3fbb0ee409a"
        "b06800ee100ab8ee400a0ff27011d1ed000a20ee200af1ee000ab4eee00af1ee"
        "10fa01ddb0ee600afceec00a10ee900a032888bf032000ee900af8ee600a30ee"
        "608a00eb40000ff2581808eb800807227263002708eb870393ed000ad3ed030a"
        "70eec00aa0ee880a20ee280a0ff2181303eb8703d3ed000a0ff2181303eb8703"
        "93ed001a21ee091a38460321fff720fb0137032fded300f000b8002707eb8701"
        "04eb8101d1f81c1100290cd1384603210ff2c40292ed000af7ee000a002301ee"
        "103afff705fb0137042fe7d300bd00bf0000000095a70b08d9a80b0899ab0b08"
        "5daf0b0819ae0b08020304000000164300002f4400007a45f4fd343f6666663f"
        "f4fd343f01030500f4fd343f6666663ff4fd343f000000000000c0c100000000"
        "f4fd343f4260e9400000803f000098413333b33e9a992940000080400000003f"
        "0000c040f4fd343f6666663f0000c0c100001041000010416f12033904028138"
        "0402813a0000803e04020139000000413333f3400000c0c100007a4404028139"
        "000000bf0000803f000080400000e0400000104100004041000010410000c040"
        "0000874300200f4500c03745008004440040e74400001b450000344400408844"
        "0080184500800e44000052440000164500009b4300005c4400000c4500002042"
        "418764422934a342b11ae942bf78264396c56d4314cea9436888f24380342d44"
        "a16377445aacb044c057fc44fb353445a1b28045bed1b7455646034600803b46"
        ,
        {'$t': 208, 'gvalid': 114, 'gout': 192, 'gapply': 696, '$d': 2768, 'gsetband': 208, 'gdtab': 2768, 'gftab': 256, 'gft1': 280, 'gft2': 298, 'gtable': 3036, 'ghalf': 354, 'ghspan': 2920, 'ghdone': 402, 'gu16': 404, 'gidxs': 2912, 'gqmap': 426, 'gccinv': 2908, 'ggain': 460, 'ggflat': 528, 'ghsinv': 2904, 'ggboost': 514, 'ggcut': 2892, 'ggend': 522, 'ggboo': 2896, 'gtiltd': 540, 'gtdflat': 602, 'gtiltg': 2900, 'gtdend': 600, 'gfshift': 614, 'gfsflat': 690, 'gfsup': 668, 'gfmtdn': 2944, 'gfsend': 676, 'gfmtup': 2948, 'gclr': 700, 'gmout': 2714, 'gmdj': 802, 'gmtwin': 1536, 'gmphone': 1774, 'gmmix': 1056, 'gmtone': 1188, 'gmtilt': 2180, 'gmnotch': 2090, 'gmpeak': 2276, 'gmmnot': 2374, 'gmfmt': 2524, 'gmdjc': 864, 'gmdjhp': 880, 'gmdjf': 882, 'gqres': 2848, 'gctl0f': 952, 'gctl0j': 960, 'gdjspr': 2916, 'gmdjs1': 1010, 'gmdjs2': 1014, 'gqfix': 2884, 'gmix3': 1068, 'gmix3l': 1072, 'gmixf': 2796, 'gmix3k': 1132, 'gmixq': 2808, 'gmixt': 2792, 'gmix3s': 1162, 'gmixkq': 2824, 'gmixkg': 2836, 'gmixk': 2820, 'gmtnk0': 1228, 'gmtns0': 1250, 'gmtnk1': 1308, 'gmtns1': 1330, 'gctl1f': 1362, 'gctl1j': 1370, 'gmtnc': 1430, 'gmtnhp': 1446, 'gmtnf': 1448, 'gctl2f': 1560, 'gctl2j': 1568, 'gctl3f': 1632, 'gctl3j': 1640, 'gctl4f': 1794, 'gctl4j': 1802, 'gbpnar': 2928, 'gbpwid': 2924, 'gqbell': 2888, 'gmbpsl': 1990, 'gqnot': 2856, 'gctl5f': 2128, 'gctl5j': 2136, 'gnotchg': 2932, 'gctl6f': 2190, 'gctl6j': 2198, 'gctl7f': 2300, 'gctl7j': 2308, 'gctl8f': 2330, 'gctl8j': 2338, 'gctl9f': 2394, 'gctl9j': 2402, 'gmnsp': 2864, 'gctl10f': 2424, 'gctl10j': 2432, 'gqmn': 2872, 'gmmnl': 2462, 'gctl11f': 2544, 'gctl11j': 2552, 'gfmtis': 2940, 'gmfmc': 2600, 'gfmtf': 2976, 'gmfmtl': 2644, 'gfmtq': 2952, 'gfmtg': 2964, 'gmfill': 2716, 'gmfnext': 2758, 'gnullf': 2936, 'gqph': 2876, '_sG': 0, 'gnext': 204}),
}

REASSEMBLE = False
VERBOSE = False
LEGACY_VERSION = False    # --legacy-version: lift the splash DISPLAY limit only,
                          # for rebuilding a pre-b11 image to check its hash

# What the patch gives you, in the order it gets applied. Kept here so the
# summary the user reads is generated from the same constants that do the work
# and cannot drift into a comfortable lie.
FEATURES = [
    ("Compressor on/off, ch 1", f"CC {CC_COMP_ONOFF}"),
    ("Compressor threshold, ch 1", f"CC {CC_COMP_FIRST}"),
    ("Compressor ratio, ch 1", f"CC {CC_COMP_FIRST + 1}"),
    ("Compressor attack, ch 1", f"CC {CC_COMP_FIRST + 2}"),
    ("Compressor release, ch 1", f"CC {CC_COMP_FIRST + 3}"),
    ("Compressor makeup, ch 1", f"CC {CC_COMP_FIRST + 4}"),
    ("EQ mode select, ch 1", f"CC {CC_EQ_MODE}"),
    ("EQ control A, ch 1", f"CC {CC_EQ_A}"),
    ("EQ control B, ch 1", f"CC {CC_EQ_B}"),
    ("EQ control C, ch 1", f"CC {CC_EQ_C}"),
    ("EQ button A, ch 1", f"CC {CC_EQ_BTN_A}"),
    ("EQ button B, ch 1", f"CC {CC_EQ_BTN_B}"),
    ("EQ button C, ch 1", f"CC {CC_EQ_BTN_C}"),
    ("EQ master bypass, ch 1", f"CC {CC_EQ_BYPASS}"),
    ("EQ filter slope, ch 1", f"CC {CC_EQ_SLOPE}"),
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


def build_comp_hook(org, hookc):
    """CC 40-45, chained in front of hook C.

    CC 40 writes the session's on/off flag directly -- the session is allocated
    once at boot, so that target is stable, and hardware testing confirms it.
    It is found by proof rather than arithmetic: scan the engine's pointer slots
    and validate each candidate by reading back the safety limiter's two timing
    constants, which nothing on the module ever writes.

    CC 41-45 do not touch the engine at all. They write the patch's own store,
    which hook E applies to the live object every block.
    """
    CCMAX = CC_MAX
    dstock = "\n".join(f"    .word  {w:#010x}   /* {n} */" for w, n in DEFAULTS)
    rng = "".join(
        f"    .float {(hi - lo) / CCMAX!r}\n    .float {lo!r}\n    .word  {1 if i else 0}\n"
        for lo, hi, i in COMP_RANGES)
    src = f"""
    .syntax unified
    .thumb
    .section .text
    .global _sD
    .thumb_func
_sD:
    /* r0 AND r1 ARE THE STOCK CALL'S ARGUMENTS. DO NOT CLOBBER THEM.
       This hook is reached by a chain that BRANCHES rather than calls, so it
       shares the register state of the original `bl FUN_080950c0` at
       MIDI_POST_CALL, 0x08093f14. The two instructions immediately before that
       call -- `ldr r0,[r7]` and `mov r1,r6` -- are setting up arguments, and
       0x080950c0 is not a function at all, it is a this-adjusting thunk: it
       adds 0x127dc to the incoming r0 and tail-branches to the queue-append at
       0x080a396c. r2, r3 and r12 are scratch here; r0 and r1 must arrive
       intact.

       Hook D used to fall out of `dout` with r0 holding a patch-store address
       around 0x2406c000, on BOTH its CC 40 path and its CC 41-45 path.
       0x2406c000 + 0x127dc = 0x2407e7dc, which IS mapped -- AXI SRAM -- so it
       corrupted quietly instead of faulting: up to 64 records of 24 bytes once
       per boot, with the queue's own `cmp r7, #0x3f` bound stopping it there.
       Hook F's equivalent formed 0x101227aa, which is NOT mapped, and hard
       faulted on every EQ CC until it was bracketed. The same defect and
       different luck, and the luck is the whole reason this one shipped in two
       firmware versions while hook F's was found in an afternoon. Fixed under
       D-32 in 2.3.7-b16.

       The pop sits at `dout`, BEFORE `dnext`. Hook C is downstream of that
       tail branch and reads the caller frame at sp+0x90 and sp+0x94 counting
       only its own `push {{r4, r5, lr}}`, so a pop placed after the branch
       would leave SP eight bytes low and break the delay FX CCs -- a different
       fault, and not one a compressor test would catch. Every caller-frame
       read below is therefore written `+ 8`, and THERE ARE FOUR OF THEM, NOT
       THREE: MSG_VAL_SP is read twice, once at `dfound` for the CC 40
       threshold compare and once at `dseeded` for the CC 41-45 conversion. */
    push    {{r0, r1}}
    ldr     r12, [sp, #{MSG_CHAN_SP + 8:#x}]   /* +8: our own push */
    cmp     r12, #{COMP_CHANNEL}
    bne.w   dout
    ldr     r12, [sp, #{MSG_CC_SP + 8:#x}]
    cmp     r12, #{CC_COMP_ONOFF}
    beq     donoff
    sub.w   r3, r12, #{CC_COMP_FIRST}
    cmp     r3, #{CC_COMP_COUNT - 1}
    bls     dparam
    b       dout
donoff:
    movw    r0, #{SCAN_LO & 0xffff}
    movt    r0, #{SCAN_LO >> 16}
    movw    r1, #{SCAN_HI & 0xffff}
    movt    r1, #{SCAN_HI >> 16}
dscan:
    ldr     r2, [r0]
    sub.w   r3, r2, #{RAM_LO:#x}
    cmp.w   r3, #{RAM_SPAN:#x}
    bhs     dnextc
    movw    r3, #{PROOF_A_OFF:#x}
    ldr     r3, [r2, r3]
    cmp.w   r3, #{PROOF_A_VAL}
    bne     dnextc
    movw    r3, #{PROOF_R_OFF:#x}
    ldr     r3, [r2, r3]
    movw    r12, #{PROOF_R_VAL}
    cmp     r3, r12
    beq     dfound
dnextc:
    adds    r0, #4
    cmp     r0, r1
    blo     dscan
    b       dout
dfound:
    ldr     r12, [sp, #{MSG_VAL_SP + 8:#x}]
    cmp.w   r12, #{BOOL_THRESHOLD}
    ite     lt
    movlt   r3, #0
    movge   r3, #1
    movw    r1, #{COMP_ONOFF_OFF & 0xffff}
    movt    r1, #{COMP_ONOFF_OFF >> 16}
    strb    r3, [r2, r1]
    b       dout
dparam:
    movw    r2, #{PATCH_RAM & 0xffff}
    movt    r2, #{PATCH_RAM >> 16}
    ldr     r1, [r2]
    movw    r12, #{PATCH_MAGIC & 0xffff}
    movt    r12, #{PATCH_MAGIC >> 16}
    cmp     r1, r12
    beq     dseeded
    str     r12, [r2]                  /* first touch: seed from stock */
    adr     r0, dstock
    add     r1, r2, #4
    add.w   r12, r2, #{4 + 4 * CC_COMP_COUNT}
dseed:
    vldr    s0, [r0]
    vstr    s0, [r1]
    adds    r0, #4
    adds    r1, #4
    cmp     r1, r12
    blo     dseed
dseeded:
    ldr     r12, [sp, #{MSG_VAL_SP + 8:#x}]
    vmov    s0, r12
    vcvt.f32.u32 s0, s0
    add.w   r0, r3, r3, lsl #1
    adr     r1, dscale
    add.w   r1, r1, r0, lsl #2
    vldr    s1, [r1]
    vldr    s2, [r1, #4]
    ldr     r0, [r1, #8]
    vmul.f32 s0, s0, s1
    vadd.f32 s0, s0, s2
    cmp     r0, #0
    beq     dstore
    vcvt.u32.f32 s0, s0
dstore:
    add.w   r0, r2, r3, lsl #2
    vstr    s0, [r0, #4]
    ldr     r12, [r2]                  /* checksum = magic XOR all five */
    ldr     r3, [r2, #4]
    eor.w   r12, r12, r3
    ldr     r3, [r2, #8]
    eor.w   r12, r12, r3
    ldr     r3, [r2, #12]
    eor.w   r12, r12, r3
    ldr     r3, [r2, #16]
    eor.w   r12, r12, r3
    ldr     r3, [r2, #20]
    eor.w   r12, r12, r3
    str     r12, [r2, #{PATCH_SUM}]
dout:
    pop     {{r0, r1}}                  /* before dnext -- see the note at _sD */
    .global dnext
dnext:
    .short 0,0
    .align 2
dstock:
{dstock}
dscale:
{rng}"""
    code, syms = assemble("hookD", src)
    code = bytearray(code)
    code[syms["dnext"]:syms["dnext"] + 4] = enc_b_bl(org + syms["dnext"], hookc, False)
    return bytes(code)


def build_comp_dsp_hook(org):
    """Hook E -- the one that makes the values stick.

    The engine calls the compressor's processing function once per audio block
    with the live object in r0. Stamping our store onto it there means the patch
    owns those five parameters continuously: whatever the engine does to the
    object, the next block puts them back. A magic word gates the whole thing,
    so a clobbered store switches the feature off rather than writing rubbish
    into the audio path.

    r0 and r1 belong to the real DSP and are untouched; r2, r3 and ip are
    caller-saved and free.
    """
    # First audio block after power-on: lay down the defaults so the module
    # comes up as a working compressor rather than 1010music's headroom trim.
    # Costs nothing after that -- the magic word means this runs exactly once.
    _lines = ["    str     r12, [r2]                  /* claim the store */"]
    for _i, (_w, _n) in enumerate(DEFAULTS):
        _lines.append(f"    movw    r3, #{_w & 0xffff:#06x}")
        if _w >> 16:
            _lines.append(f"    movt    r3, #{_w >> 16:#06x}")
        _lines.append(f"    str     r3, [r2, #{4 + _i * 4}]              /* {_n} */")
    _lines.append(f"    movw    r3, #{DEFAULT_SUM & 0xffff:#06x}")
    _lines.append(f"    movt    r3, #{DEFAULT_SUM >> 16:#06x}")
    _lines.append(f"    str     r3, [r2, #{PATCH_SUM}]             /* checksum */")
    eseed = "\n".join(_lines)

    src = f"""
    .syntax unified
    .thumb
    .section .text
    .global _sE
    .thumb_func
_sE:
    movw    r2, #{PATCH_RAM & 0xffff}
    movt    r2, #{PATCH_RAM >> 16}
    ldr     r3, [r2]
    movw    r12, #{PATCH_MAGIC & 0xffff}
    movt    r12, #{PATCH_MAGIC >> 16}
    cmp     r3, r12
    beq     everify
{eseed}
everify:
    ldr     r3, [r2, #4]               /* verify the whole store, not just */
    eor.w   r12, r12, r3               /* the magic -- a half-eaten store  */
    ldr     r3, [r2, #8]               /* must switch the feature OFF, not */
    eor.w   r12, r12, r3               /* stamp rubbish into the audio.    */
    ldr     r3, [r2, #12]
    eor.w   r12, r12, r3
    ldr     r3, [r2, #16]
    eor.w   r12, r12, r3
    ldr     r3, [r2, #20]
    eor.w   r12, r12, r3
    ldr     r3, [r2, #{PATCH_SUM}]
    cmp     r3, r12
    bne     eout
    ldr     r3, [r2, #4]
    str.w   r3, [r0, #{COMP_F_THRESH:#x}]
    ldr     r3, [r2, #8]
    str.w   r3, [r0, #{COMP_F_RATIO:#x}]
    ldr     r3, [r2, #12]
    str.w   r3, [r0, #{COMP_F_ATTACK:#x}]
    ldr     r3, [r2, #16]
    str.w   r3, [r0, #{COMP_F_RELEASE:#x}]
    ldr     r3, [r2, #20]
    str.w   r3, [r0, #{COMP_F_MAKEUP:#x}]
eout:
    .global enext
enext:
    .short 0,0
"""
    code, syms = assemble("hookE", src)
    code = bytearray(code)
    code[syms["enext"]:syms["enext"] + 4] = enc_b_bl(org + syms["enext"], COMP_DSP_FN, False)
    return bytes(code)


def _eq_seed(reg, scratch):
    """Lay the store down with the module's boot state: DJ Filter, knobs centred.

    Both hooks can be first to run -- hook G on the first audio block, hook F on
    the first CC -- so both must be able to seed, and they must agree exactly or
    the checksum will reject whichever went second. `gen` starts at 1 and `agen`
    at 0, so hook G applies the mode on its very first block.

    `scratch` IS DESTROYED, and naming it is not a style preference. This block
    is eleven stores long and every one of them needs a register to store FROM;
    it used to take that register (`r3`) silently. Hook G can afford that -- it
    reloads everything from `r6` afterwards -- but hook F is holding the CC
    index in `r3` at exactly this point, and the block left it holding the
    checksum constant `0x1010ffce` instead. Every compare at `fseeded` then
    missed and `fplain` did `ldrb r0, [r0, r3]`, reading `fslots + 0x1010ffce`
    -- about `0x181f44d6`, reserved space on this part. Bus fault, hard fault,
    watchdog. That was the reboot on the first EQ CC after boot, and only the
    first, because the seed runs once and the module never got far enough to
    have a second.

    So the caller passes a register it can prove is dead here, and the assert
    below refuses the one mistake that is easy to make twice.

    All three controls are seeded at EQ_CENTRE, and that is D-16's whole
    mechanism. The old seed laid the character control down at 0, so Notch
    booted at its widest Q and Band Pass at its narrowest width -- both of the
    things D-16 says must not happen, measured by the simulator's --boot-state
    before this changed. Centring costs no new constant. Every button and the
    self test boot released, because a module that powers on with a control
    already overridden is a module that looks broken.
    """
    boot = {EQ_S_MODE: EQ_BOOT_MODE,
            EQ_S_GA: EQ_CENTRE, EQ_S_GB: EQ_CENTRE, EQ_S_GC: EQ_CENTRE,
            EQ_S_BTN_A: 0, EQ_S_BTN_B: 0, EQ_S_BTN_C: 0,
            EQ_S_BYP: 0, EQ_S_SLOPE: 0, EQ_S_GEN: 1}
    chk = EQ_MAGIC
    for o in EQ_S_LIVE:
        chk ^= boot[o]
    assert scratch not in (reg, "r2"), (
        f"the seed's scratch register ({scratch}) is also carrying the magic "
        f"({reg}) or the store base (r2), so the block would eat its own inputs")
    out = [f"    str     {reg}, [r2]                    /* claim the store */"]
    for off in sorted(boot):
        v = boot[off]
        out.append(f"    movw    {scratch}, #{v & 0xffff:#06x}")
        if v >> 16:
            out.append(f"    movt    {scratch}, #{v >> 16:#06x}")
        out.append(f"    str     {scratch}, [r2, #{off}]")
    out.append(f"    movs    {scratch}, #0")
    out.append(f"    str     {scratch}, [r2, #{EQ_S_AGEN}]     /* != gen, so we apply */")
    out.append(f"    str     {scratch}, [r2, #{EQ_S_MASK}]")
    out.append(f"    movw    {scratch}, #{chk & 0xffff:#06x}")
    out.append(f"    movt    {scratch}, #{chk >> 16:#06x}")
    out.append(f"    str     {scratch}, [r2, #{EQ_S_SUM}]")
    return "\n".join(out)


def _eq_checksum(dst, base, scratch):
    """magic XOR the ten live words, into `dst`."""
    out = [f"    ldr     {dst}, [{base}]"]
    for off in EQ_S_LIVE:
        out.append(f"    ldr     {scratch}, [{base}, #{off}]")
        out.append(f"    eor.w   {dst}, {dst}, {scratch}")
    return "\n".join(out)


def _f(sreg, label, areg):
    """Load a float constant. adr.w reaches +/-4095; a vldr literal only +/-1020,
    and this hook is far longer than that."""
    return f"    adr.w   {areg}, {label}\n    vldr    {sreg}, [{areg}]"


def build_eq_midi_hook(org, hookd):
    """Hook F -- CC 50-58, chained in FRONT of hook D.

    Chained rather than folded into hook D so hooks A-E stay byte-identical and
    the compressor remains provably untouched by this feature.

    It never touches the EQ object (R3). Every parameter write and every call
    into 1010music's filter designers happens on the audio thread in hook G,
    because that is the thread their smoother and their designers already run
    on. All this does is record the knob and bump a generation counter.
    """
    # The slots table, indexed by CC - CC_EQ_FIRST. Only the three continuous
    # controls ever reach `fplain`; mode, the three buttons, master bypass and
    # the self test are all special-cased above it, so their entries here are
    # unreachable. They point at EQ_S_SCRATCH -- a dead word outside EQ_S_LIVE,
    # so a write there changes nothing and is not checksummed.
    #
    # An entry of 0 would point at the store's MAGIC word, and a future edit
    # that dropped one of those special cases would then silently re-seed the
    # store on every CC: the EQ would forget its settings at apparently random
    # moments, with nothing on screen to say why. That is a smaller version of
    # the mistake this whole phase exists to fix, and EQ_S_SCRATCH costs four
    # bytes of RAM to make it harmless.
    slots = [EQ_S_SCRATCH] * (CC_EQ_LAST - CC_EQ_FIRST + 1)
    for cc, off in ((CC_EQ_A, EQ_S_GA), (CC_EQ_B, EQ_S_GB), (CC_EQ_C, EQ_S_GC)):
        slots[cc - CC_EQ_FIRST] = off
    tbl = "".join(f"    .byte {v}\n" for v in slots)

    def fbin(cc, off, nxt, what):
        """One binary CC: at or above half scale is engaged (D-03).

        Same three-instruction idiom for all six, and every one of them leaves
        by `b fbump` -- the shared tail that bumps `gen`. Since hook G applies
        the store on every block, skipping `fbump` no longer makes a control
        appear dead; but `gen` is the store's own record of how many CCs it has
        accepted, and a write that did not bump it would make that record lie.
        Every accepted CC leaves by the same tail, with no exceptions.
        """
        return f"""    cmp     r3, #{cc - CC_EQ_FIRST}
    bne     {nxt}
    cmp.w   r1, #{BOOL_THRESHOLD}      /* {what} */
    ite     lt
    movlt   r1, #0
    movge   r1, #1
    str     r1, [r2, #{off}]
    b       fbump
{nxt}:
"""

    src = f"""
    .syntax unified
    .thumb
    .section .text
    .global _sF
    .thumb_func
_sF:
    /* r0 AND r1 ARE THE STOCK CALL'S ARGUMENTS. DO NOT CLOBBER THEM.
       This hook replaces `bl FUN_080950c0` at MIDI_POST_CALL, and the two
       instructions immediately before that call are `ldr r0,[r7]` and
       `mov r1,r6` -- they are setting up arguments. FUN_080950c0 is not a
       function, it is a this-adjusting thunk:
           mov r3,r0 / ldr r0,=0x000127d8 / mov.w r2,#0x400 / add r0,r3
           b.w 0x080a83a4   ->  r2==0x400 -> add r0,#4 -> b.w 0x080a396c
       so the object pointer it finally dereferences is `incoming r0 + 0x127dc`.
       r2 and r3 are dead on entry (the thunk overwrites both), r12 is scratch
       by AAPCS -- but r0 and r1 must arrive intact.

       Hook F used to fall out of `fbump` with r0 holding the freshly computed
       EQ CHECKSUM, about 0x1010ffce. 0x1010ffce + 0x127dc = 0x101227aa, which
       is not mapped on this part: bus fault, hard fault, watchdog reboot. That
       is the reboot on every EQ CC, and it is why moving the store (dbgZ) did
       not help -- the fatal value is the checksum itself, not where it lives.

       BOTH HOOKS AT THIS SITE NOW BRACKET. Hook D committed the identical
       violation and shipped with it from 2.3.6: its leftover r0 was a store
       address around 0x2406c000, so 0x2406c000 + 0x127dc = 0x2407e7dc landed
       in mapped AXI SRAM and merely corrupted rather than faulting. Same
       defect, different luck -- and the luck is why hook F's was found in an
       afternoon and hook D's took two firmware versions. Fixed under D-32 in
       2.3.7-b16, with this same bracket. ANY future hook chained at
       MIDI_POST_CALL must do the same. See DEV-README's chain-ABI note before
       adding one. */
    push    {{r0, r1}}
    ldr     r12, [sp, #{MSG_CHAN_SP + 8:#x}]   /* +8: our own push */
    cmp     r12, #{EQ_CHANNEL}
    bne.w   fout
    ldr     r12, [sp, #{MSG_CC_SP + 8:#x}]
    sub.w   r3, r12, #{CC_EQ_FIRST}
    cmp     r3, #{CC_EQ_LAST - CC_EQ_FIRST}
    bhi.w   fout
    movw    r2, #{EQ_STORE & 0xffff}
    movt    r2, #{EQ_STORE >> 16}
    ldr     r1, [r2]
    movw    r0, #{EQ_MAGIC & 0xffff}
    movt    r0, #{EQ_MAGIC >> 16}
    cmp     r1, r0
    beq     fseeded
{_eq_seed("r0", "r1")}
fseeded:
    ldr     r1, [sp, #{MSG_VAL_SP + 8:#x}]
    cmp     r3, #{CC_EQ_MODE - CC_EQ_FIRST}
    bne     fnotmode
    /* mode = (7-bit value * 10) >> 7, which is (raw * 10) >> 14 in the raw
       domain. Ten slots of ~12.8 CC values each, and the controller sends ten
       steps over Min 6 / Max 122, so every step sits six CC values clear of a
       window boundary -- the widest margin this design has had, because fewer
       modes means wider windows. r0 held the magic for the compare above and
       is dead here. */
    movs    r0, #{EQ_M_SLOTS}
    mul     r1, r1, r0
    lsrs    r1, r1, #14
    str     r1, [r2, #{EQ_S_MODE}]
    b       fbump
fnotmode:
{fbin(CC_EQ_BYPASS, EQ_S_BYP, "fnotbyp", "master bypass, encoder 1's push")}\
{fbin(CC_EQ_SLOPE, EQ_S_SLOPE, "fnotslope", "filter slope, 12 or 24 dB/oct")}\
{fbin(CC_EQ_BTN_A, EQ_S_BTN_A, "fnotba", "button A")}\
{fbin(CC_EQ_BTN_B, EQ_S_BTN_B, "fnotbb", "button B")}\
{fbin(CC_EQ_BTN_C, EQ_S_BTN_C, "fnotbc", "button C")}\
fplain:
    adr     r0, fslots
    ldrb    r0, [r0, r3]
    str     r1, [r2, r0]
fbump:
    ldr     r1, [r2, #{EQ_S_GEN}]
    adds    r1, #1
    str     r1, [r2, #{EQ_S_GEN}]
{_eq_checksum("r0", "r2", "r3")}
    str     r0, [r2, #{EQ_S_SUM}]
fout:
    pop     {{r0, r1}}                  /* hand the thunk its arguments back */
    .global fnext
fnext:
    .short 0,0
    .align 2
fslots:
{tbl}"""
    code, syms = assemble("hookF", src)
    code = bytearray(code)
    code[syms["fnext"]:syms["fnext"] + 4] = enc_b_bl(org + syms["fnext"], hookd, False)
    return bytes(code)


def build_eq_dsp_hook(org):
    """Hook G -- installed in the EQ class's vtable slot +0x0c.

    The node already runs once per block, already asks the message for its audio
    handle and frame count, and then never asks for the buffers. This supplies
    the arithmetic that is missing, then tail-branches into the stock handler so
    the parameter path, the coefficient smoother and the chain forward all keep
    working untouched.

    It runs BEFORE the stock handler because the stock handler is what forwards
    the block on -- so it filters with the coefficients the smoother produced last
    block. 128 frames, about 2.7 ms. Every exit is pass-through, which for this
    node is exactly stock behaviour.
    """
    zero = "    movs    r3, #0\n    vmov    s2, r3"

    # Formant's vowel table, emitted ROW MAJOR with one vowel per line, so the
    # twelve-byte stride `gmfmt` indexes with is visible in the source that
    # emits it rather than only in the routine that walks it. The names are the
    # dataset's own ARPABET codes and Peterson and Barney's keywords -- see
    # EQ_FMT_TABLE for the citation and for why the file avoids IPA.
    fmt_rows = ("I  IY  heed", "E  EH  head", "A  AA  hod",
                "O  AO  hawed", "U  UW  who'd")
    fmt_tbl = "\n".join(
        "    .float " + ", ".join(f"{v!r}" for v in row)
        + " " * max(1, 34 - len("    .float " + ", ".join(f"{v!r}" for v in row)))
        + f"/* {name} */"
        for row, name in zip(EQ_FMT_TABLE, fmt_rows))

    def setband(band, type_, freq="s16", q="s17", gain=None):
        g = zero.replace("s2", "s2") if gain is None else gain
        return (f"    movs    r0, #{band}\n    movs    r1, #{type_}\n"
                f"    vmov.f32 s0, {freq}\n    vmov.f32 s1, {q}\n{g}\n    bl      gsetband\n")

    # ---- D-03, the button semantic, written here because this is where a
    # future session will come looking for it rather than re-deriving it from
    # a mode routine ---------------------------------------------------------
    #
    #   FORCED, NOT TOGGLED.   Engaged means that control is held at one fixed
    #     value. There is no second press, no third state and no mode cycle.
    #   RELEASED HANDS BACK.   Released means the encoder takes over again, on
    #     the very next audio block, because hook F bumped `gen`.
    #   THE STORED VALUE IS NEVER OVERWRITTEN.   `ctl()` reads the control word
    #     and the button word and writes neither, so releasing returns the sound
    #     to exactly where the knob is. That is what stops a knob-jump.
    #   THE THRESHOLD IS HALF SCALE.   Hook F converts the CC to 0 or 1 against
    #     BOOL_THRESHOLD, which is what makes a momentary footswitch and a
    #     latching toggle behave identically. Nothing here re-decides that.
    #
    # The split is structural, not conventional: storage is hook F on the MIDI
    # thread, interpretation is here on the audio thread, where 1010music's
    # designers and their coefficient smoother already run (R2/R3). `ctl` is
    # defined inside build_eq_dsp_hook and so cannot be called from
    # build_eq_midi_hook even by accident.
    _ctl_n = [0]

    def ctl(slot, btn=None, forced=None, dest="r0", scratch="r3"):
        """Emit: load control `slot` into `dest`, or the literal `forced` if
        `btn` is engaged.

        slot   - a store offset, EQ_S_GA / EQ_S_GB / EQ_S_GC
        btn    - a store offset, EQ_S_BTN_A / EQ_S_BTN_B / EQ_S_BTN_C, or None
                 for no button
        forced - the raw CC-domain value substituted when the button is engaged,
                 normally one of EQ_FORCE_CENTRE / EQ_FORCE_MAX / EQ_FORCE_MIN

        With `btn=None` this emits the plain `ldr` and nothing else, byte for
        byte what a mode with no button on that control emits today -- so
        adopting it across an existing routine is provably a no-op.

        THERE ARE EXACTLY TWO KINDS OF BUTTON, and a mode uses one or the other
        for a given band, never both and never a third shape:

        1. The FORCED-VALUE kind, which is this function. The band is still
           designed; one of its inputs is pinned. DJ Filter's button A (force
           control A to centre, which lands in the detent and is a real
           bypass), Dual Cut's A and B (force a cut fully open), Band Pass's B
           (force the width widest), Notch's depth slam, and the flat/neutral
           and maximum-effect buttons the four unbuilt modes are specified to
           get.

        2. The KILL kind, which does NOT use `ctl` at all. It still calls
           `gsetband` for that band and it still sets that band's mask bit --
           what changes is the band's TYPE, swapped to a kill shape out of
           `gmixk` / `gmixkq` / `gmixkg`. Kill low becomes a high-pass at
           150 Hz, kill high a low-pass at 4 kHz, and kill mid a wide -24 dB
           scoop at 700 Hz. Mixer EQ's three kill buttons are this kind, and
           Tone + Filter's two will be when it is built.

           DO NOT "kill" a band by disabling it. b6 did exactly that, reasoning
           that a disabled band passes nothing. That is true of the BAND and
           false of the AUDIO: a disabled band applies no filter, so the signal
           passes straight through untouched. Worse, Rule 1 already puts every
           band at 0 dB at centre, which is where the knobs rest, so the kill
           was removing something that was already transparent and did nothing
           audible at all. The user found it in the first minute of play.
           Attenuating instead would not have worked either -- `ggain`'s floor
           is -24 dB and a shelf at -24 dB still passes everything past its
           corner.

           D-25 is what makes the type swap the only available lever, and it is
           worth stating plainly: NO BAND IS EVER SWITCHED OFF. A band a mode
           does not want is designed as a flat Param, H(z) = 1 exactly. Only
           the target ever changes, so 1010music's 50 ms smoother ramps every
           transition in both directions, which is what removed the b7
           reconfiguration ring and the b8 click. So the only lever any button
           has is WHAT gets designed, never WHETHER.

        DJ Filter's button B is a third shape only in appearance -- it *adds*
        the cascaded second section rather than swapping a type -- and it is
        the one already in the file, at the bottom of `gmdj`.
        """
        if btn is None:
            return f"    ldr     {dest}, [r6, #{slot}]\n"
        if forced is None:
            raise ValueError("ctl() with a button needs a forced value -- a "
                             "button that forces nothing is not a button")
        if not 0 <= forced <= EQ_CC_MAX:
            raise ValueError(f"ctl() forced value {forced} is outside the raw "
                             f"CC domain 0..{EQ_CC_MAX}; a value the encoder "
                             f"cannot reach is a coefficient set no sweep has "
                             f"covered")
        if dest == scratch:
            raise ValueError(f"ctl() would load the button flag into {dest} "
                             f"and then overwrite it with the control value")
        n = _ctl_n[0]
        _ctl_n[0] += 1
        # A unique suffix per call, so one routine can call this more than once
        # without a label collision. `setband` needs no such thing because it
        # emits no labels; this is the one structural difference between them.
        return f"""    ldr     {scratch}, [r6, #{btn}]    /* D-03: read the button, write nothing */
    cmp     {scratch}, #0
    bne     gctl{n}f                   /* engaged -> the forced value */
    ldr     {dest}, [r6, #{slot}]      /* released -> the encoder's own value,
                                          which no button ever overwrote */
    b       gctl{n}j
gctl{n}f:
    movw    {dest}, #{forced & 0xffff}
    movt    {dest}, #{forced >> 16}
gctl{n}j:
"""

    src = f"""
    .syntax unified
    .thumb
    .section .text
    .global _sG
    .thumb_func
_sG:
    /* r8 is pushed only to keep SP 8-byte aligned across the `blx` into
       1010music's designer: five core registers plus lr is 24 bytes, which is
       what AAPCS wants, where the natural {{r4,r5,r6,r7,lr}} would leave SP at
       4. `vpush {{d8,d9,d10}}` is NOT optional -- the mode routines park
       frequency, Q and gain in s16..s21 across `bl gsetband`, and s16-s31 are
       callee-saved, so this hook owes its own caller the same.

       THREE PAIRS, NOT TWO, from Multi Notch on. That mode designs four bands
       from shared controls, so a base index, a spread, a depth and a Q all have
       to survive three further `bl gsetband` calls on top of the frequency the
       loop recomputes each turn -- six floats live, and four saved registers do
       not hold six. Widening the save set is always safe: s16-s31 are
       callee-saved by AAPCS, so saving more of them can only ever be more
       correct, and three register pairs is 24 bytes, which keeps SP 8-byte
       aligned exactly as two pairs did.

       THE `vpush` AND THE `vpop` MUST CHANGE TOGETHER. A mismatched pair does
       not fail here; it corrupts the caller's FP register file on the way out,
       which is the same class of fault as the `s2` clobber b10 shipped and is
       far harder to see. */
    push.w  {{r4, r5, r6, r7, r8, lr}}
    vpush   {{d8, d9, d10}}
    mov     r4, r0                     /* the EQ object, handed to us */
    mov     r5, r1                     /* the block message */
    movw    r6, #{EQ_STORE & 0xffff}
    movt    r6, #{EQ_STORE >> 16}
    ldr     r3, [r6]
    movw    r0, #{EQ_MAGIC & 0xffff}
    movt    r0, #{EQ_MAGIC >> 16}
    cmp     r3, r0
    beq     gvalid
    mov     r2, r6
{_eq_seed("r0", "r1")}
gvalid:
{_eq_checksum("r0", "r6", "r3")}
    ldr     r3, [r6, #{EQ_S_SUM}]
    cmp     r0, r3
    bne.w   gout                       /* damaged store -> stock behaviour.
                                          THIS EXIT STAYS HERE, and it is not
                                          the bypass exit that moved into
                                          `gapply`. A failed checksum means we
                                          do not know what the store says, so
                                          writing the four bands from it is the
                                          wrong response -- leaving the object
                                          alone is. Bypass is the opposite
                                          case: the store is intact and it is
                                          telling us exactly what to build. */
    /* MASTER BYPASS IS NOT TESTED HERE. It used to be, and that is why it never
       bypassed anything: `gout` contains no store, so the four bands kept
       whatever the last `gapply` left them and the stock handler carried on
       filtering. The 50 ms smoother could not help, because a smoother ramps
       toward a TARGET and the target never changed. Bypass FROZE the EQ. The
       test now lives inside `gapply`, after `gclr` and before the mode
       dispatch, and takes the identity fill. See the note there.

       APPLY EVERY BLOCK, UNCONDITIONALLY -- the same shape as hook E, which
       stamps the compressor's store onto its live object every block and is
       the most robust thing in this patch.

       What used to be here was `cmp gen, agen / beq gout`: rebuild only when
       the generation moved. It looked like a free economy and it was not. The
       counter tracks OUR INTENT, not the OBJECT'S STATE. A preset load
       reconstructs the EQ object to constructor defaults and does not touch
       our patch RAM, so the store still said `agen == gen` and hook G took the
       early exit forever. The object stayed configured by nobody: audio
       passing, nothing happening, and a null test cannot see it, because every
       failure exits to pass-through and pass-through is what the test expects.

       `agen` is still written, so the store's shape and its checksum
       arithmetic do not change and nothing downstream has to learn a new
       layout. It simply stops being a gate.

       The cost was BOUNDED from the disassembly before this was changed, not
       asserted afterwards: 2,512 instructions / 5,444 cycles against a
       1,280,000-cycle block, 0.43%. D-25 is what makes that a real bound --
       the identity fill means `gapply` designs exactly four bands on every
       call in every mode, so the worst case is the typical case. Counts and
       method are in DEV-README. */
    ldr     r0, [r6, #{EQ_S_GEN}]
    str     r0, [r6, #{EQ_S_AGEN}]
    bl      gapply

gout:
    mov     r0, r4
    mov     r1, r5
    vpop    {{d8, d9, d10}}           /* the matching half of the prologue's
                                          save -- changed with it, never alone */
    pop.w   {{r4, r5, r6, r7, r8, lr}}
    .global gnext
gnext:
    .short 0,0                         /* b -> the stock handler */

/* ---- helpers ----------------------------------------------------------- */
gsetband:                              /* r0=band r1=type s0=freq s1=Q s2=gain dB */
    push    {{lr}}
    add.w   r2, r0, r0, lsl #2
    add.w   r2, r4, r2, lsl #2         /* obj + band*0x14 */
    strb.w  r1, [r2, #{EQ_TYPE_OFF:#x}]
    vstr    s2, [r2, #{EQ_GAIN_OFF:#x}]
    vstr    s0, [r2, #{EQ_FREQ_OFF:#x}]
    vstr    s1, [r2, #{EQ_Q_OFF:#x}]
    movs    r3, #1
    str.w   r3, [r2, #{EQ_EN_OFF:#x}]
    adr.w   r3, gdtab
    ldr.w   r3, [r3, r1, lsl #2]
    mov     r1, r0
    mov     r0, r4
    blx     r3                         /* 1010music's own filter designer */
    pop     {{pc}}

gftab:                                 /* s0 = index 0..16 -> s0 = Hz */
    movs    r0, #0
    vmov    s1, r0
    vcvt.f32.u32 s1, s1
    vcmpe.f32 s0, s1
    vmrs    APSR_nzcv, fpscr
    bpl     gft1
    vmov.f32 s0, s1
gft1:
    vmov.f32 s1, #16.0
    vcmpe.f32 s0, s1
    vmrs    APSR_nzcv, fpscr
    ble     gft2
    vmov.f32 s0, s1
gft2:
    vcvt.u32.f32 s1, s0
    vmov    r0, s1
    cmp     r0, #{EQ_TBL_N - 1}
    it      hi
    movhi   r0, #{EQ_TBL_N - 1}
    vmov    s1, r0
    vcvt.f32.u32 s1, s1
    vsub.f32 s1, s0, s1                /* frac */
    adr.w   r1, gtable
    add.w   r1, r1, r0, lsl #2
    vldr    s2, [r1]
    vldr    s3, [r1, #4]
    vsub.f32 s3, s3, s2
    vfma.f32 s2, s3, s1
    vmov.f32 s0, s2
    bx      lr

ghalf:                                 /* r0 = raw CC -> s0 = 0.0 .. 1.0, and
                                          0.0 for everything at or below the
                                          centre. This is Rule 1's mechanism
                                          for the unipolar controls: centre is
                                          neutral, the anticlockwise half is a
                                          plateau there, and the clockwise half
                                          carries the whole sweep. Monotonic,
                                          so Rule 2 still holds, and `Reset Mid`
                                          lands exactly on neutral. */
    movw    r1, #{EQ_CENTRE}
    subs    r0, r0, r1
    it      mi
    movmi   r0, #0
    vmov    s0, r0
    vcvt.f32.u32 s0, s0
    adr.w   r1, ghspan
    vldr    s1, [r1]
    vmul.f32 s0, s0, s1
    vmov.f32 s1, #1.0
    vcmpe.f32 s0, s1
    vmrs    APSR_nzcv, fpscr
    ble     ghdone
    vmov.f32 s0, s1
ghdone:
    bx      lr

gu16:                                  /* r0 = raw CC -> s0 = index 0..16 */
    vmov    s0, r0
    vcvt.f32.u32 s0, s0
{_f("s1", "gidxs", "r1")}
    vmul.f32 s0, s0, s1
    bx      lr

gqmap:                                 /* r0 = raw CC, r1 = &{{lo, span}} -> s0 */
    vldr    s0, [r1]
    vldr    s1, [r1, #4]
    vmov    s2, r0
    vcvt.f32.u32 s2, s2
{_f("s3", "gccinv", "r2")}
    vmul.f32 s2, s2, s3
    vfma.f32 s0, s2, s1
    bx      lr

ggain:                                 /* r0 = raw CC -> s0 = dB, flat at centre */
    movw    r1, #{EQ_CENTRE}
    subs    r2, r0, r1
    mov     r3, r2
    cmp     r3, #0
    it      lt
    rsblt   r3, r3, #0
    cmp     r3, #{EQ_DETENT}
    bls     ggflat
    subs    r3, r3, #{EQ_DETENT}
    vmov    s0, r3
    vcvt.f32.u32 s0, s0
{_f("s1", "ghsinv", "r1")}
    vmul.f32 s0, s0, s1
    cmp     r2, #0
    bgt     ggboost
{_f("s1", "ggcut", "r1")}
    b       ggend
ggboost:
{_f("s1", "ggboo", "r1")}
ggend:
    vmul.f32 s0, s0, s1
    bx      lr
ggflat:
    movs    r3, #0
    vmov    s0, r3
    vcvt.f32.u32 s0, s0
    bx      lr

gtiltd:                                /* r0 = raw CC -> s0 = dB, SYMMETRIC,
                                          flat at centre. ggain's shape with
                                          one difference: one constant instead
                                          of two, and a negate instead of a
                                          second table entry.

                                          Inside EQ_DETENT this returns exactly
                                          0.0, which is Rule 1's mechanism and
                                          the ONLY reason both of Tilt's shelves
                                          are flat with the knob centred.

                                          NO CLAMP, for ggain's exact reason:
                                          EQ_HALF_SPAN is precisely the travel
                                          available either side of the detent,
                                          so the widest input lands on exactly
                                          1.0 and the law reaches its stops
                                          without ever passing them. */
    movw    r1, #{EQ_CENTRE}
    subs    r2, r0, r1                 /* the SIGNED offset -- kept in r2,
                                          untouched by the _f loads below,
                                          which scratch r1 */
    mov     r3, r2
    cmp     r3, #0
    it      lt
    rsblt   r3, r3, #0                 /* and its magnitude, in r3 */
    cmp     r3, #{EQ_DETENT}
    bls     gtdflat
    subs    r3, r3, #{EQ_DETENT}
    vmov    s0, r3
    vcvt.f32.u32 s0, s0
{_f("s1", "ghsinv", "r1")}
    vmul.f32 s0, s0, s1                /* 0 .. 1 across the travel */
{_f("s1", "gtiltg", "r1")}
    vmul.f32 s0, s0, s1                /* 0 .. EQ_TILT_G decibels */
    cmp     r2, #0
    bgt     gtdend
    vneg.f32 s0, s0                    /* anticlockwise: equal and opposite */
gtdend:
    bx      lr
gtdflat:
    movs    r3, #0
    vmov    s0, r3
    vcvt.f32.u32 s0, s0
    bx      lr

gfshift:                               /* r0 = raw CC -> s0 = a FREQUENCY
                                          MULTIPLIER. EQ_FMT_DN at the
                                          anticlockwise stop, EQ_FMT_UP at the
                                          clockwise one, and EXACTLY 1.0 across
                                          the centre detent.

                                          `ggain`'s two-segment shape, for
                                          `ggain`'s reasons, with one difference
                                          that matters: the detent branch
                                          returns a LITERAL 1.0 rather than
                                          something that computes to nearly one.
                                          A shift of 0.999 would move every
                                          formant off the table by a hair with
                                          the knob centred, which is small
                                          enough to be inaudible and exactly the
                                          kind of thing that is later blamed on
                                          the table.

                                          TWO CONSTANTS, ONE `vfma`. The
                                          multiplier is 1.0 + span * u on both
                                          sides, with span -0.5 below centre and
                                          +1.0 above, so the stops land on 0.5
                                          and 2.0 -- one octave each way. That
                                          is `ggcut` / `ggboo`'s structure, so
                                          the two laws read against each other.

                                          NO CLAMP, for `ggain`'s exact reason:
                                          EQ_HALF_SPAN is precisely the travel
                                          available either side of the detent,
                                          so the widest input lands on exactly
                                          1.0 and the law reaches its stops
                                          without ever passing them.

                                          RULE 2: clockwise raises the formants,
                                          which is the direction a listener
                                          expects a voice to move. */
    movw    r1, #{EQ_CENTRE}
    subs    r2, r0, r1                 /* the SIGNED offset -- kept in r2,
                                          untouched by the _f loads below,
                                          which scratch r1 */
    mov     r3, r2
    cmp     r3, #0
    it      lt
    rsblt   r3, r3, #0                 /* and its magnitude, in r3 */
    cmp     r3, #{EQ_DETENT}
    bls     gfsflat
    subs    r3, r3, #{EQ_DETENT}
    vmov    s0, r3
    vcvt.f32.u32 s0, s0
{_f("s1", "ghsinv", "r1")}
    vmul.f32 s0, s0, s1                /* 0 .. 1 across the travel */
    cmp     r2, #0
    bgt     gfsup
{_f("s1", "gfmtdn", "r1")}
    b       gfsend
gfsup:
{_f("s1", "gfmtup", "r1")}
gfsend:
    vmov.f32 s2, #1.0
    vfma.f32 s2, s0, s1
    vmov.f32 s0, s2
    bx      lr
gfsflat:
    vmov.f32 s0, #1.0
    bx      lr

/* ---- apply: rebuild the whole mode ------------------------------------- */
gapply:
    push    {{lr}}
    movs    r0, #0
gclr:
    add.w   r1, r0, r0, lsl #2
    add.w   r1, r4, r1, lsl #2
    movs    r2, #0
    strb.w  r2, [r1, #{EQ_TYPE_OFF:#x}]
    str.w   r2, [r1, #{EQ_EN_OFF:#x}]
    adds    r0, #1
    cmp     r0, #4
    blo     gclr
    movs    r2, #0
    str     r2, [r6, #{EQ_S_MASK}]
    /* MASTER BYPASS, CC 57 -- tested HERE and nowhere else.
       `gclr` has just run, so all four bands are type 0 with enable 0 and the
       mask is 0. `gmout` therefore designs all four as an explicit identity
       Param at 0 dB, which 1010music's designer resolves to H(z) = 1 exactly:
       a bit-exact wire, max |b - a| = 0.000e+00 against the dry signal. It is
       the same path every unbuilt slot took on b10, which swept all ten
       detents with no pops and no clicks, so it is already hardware-proven
       silent.

       And because it is a TARGET change rather than a hard cut, the stock
       smoother at obj+0x168 ramps it in over 50 ms and back out again. No
       click going in, no ring coming out.

       THE TRAP, NAMED SO NOBODY TAKES IT LATER: do not "simplify" this to
       writing enable = 0 to all four bands. The stock handler then skips them
       and audio genuinely passes through, so it looks correct -- and that is
       exactly what makes it dangerous. A band that stops being enabled stops
       contributing INSTANTLY, which is b8's click, and resumes with stale
       coefficients and stale filter history, which is b7's ring. Both were
       rejected on hardware. For a KILL that shape was wrong because it passed
       audio; for a BYPASS passing audio is the whole point, which is precisely
       why the fault would ship unnoticed. The identity fill has neither. */
    ldr     r3, [r6, #{EQ_S_BYP}]
    cmp     r3, #0
    bne.w   gmout
    ldr     r3, [r6, #{EQ_S_MODE}]
    /* Ten slots, DJ Filter at 0 -- which is dial position 1, because the enum
       is zero-based and the dial is one-based. ALL TEN ARE NOW BUILT and get a
       compare each, so for the first time in this feature there is no unbuilt
       slot behind the chain. The trailing `b.w gmout` is what a value past the
       end would take, and it is the identity fill rather than a bare
       `pop {{pc}}` -- see the note on it below. There is no `Off` slot any more
       -- CC 57 is the master bypass and it does the job properly. Going from
       twelve slots to ten moved none of these values, so these compares are
       byte-identical across that change.

       The compares are in ENUM ORDER and new ones go in their numbered place
       rather than on the end: the chain is read against EQ-CONTROL-LAYOUT's
       dial far more often than it is executed, and a chain that runs in the
       dial's own order is one a reader can check by eye. */
    cmp     r3, #{EQ_M_DJ}
    beq.w   gmdj
    cmp     r3, #{EQ_M_DUALCUT}
    beq.w   gmtwin
    cmp     r3, #{EQ_M_BANDPASS}
    beq.w   gmphone
    cmp     r3, #{EQ_M_MIXER}
    beq.w   gmmix
    cmp     r3, #{EQ_M_TONE}
    beq.w   gmtone
    cmp     r3, #{EQ_M_TILT}
    beq.w   gmtilt
    cmp     r3, #{EQ_M_NOTCH}
    beq.w   gmnotch
    cmp     r3, #{EQ_M_PEAK}
    beq.w   gmpeak
    cmp     r3, #{EQ_M_MULTINOTCH}
    beq.w   gmmnot
    cmp     r3, #{EQ_M_FORMANT}
    beq.w   gmfmt
    b.w     gmout                      /* no slot left unbuilt: this is now
                                          unreachable for any value hook F can
                                          store, because the mode word is
                                          (raw * 10) >> 14 and cannot exceed 9.
                                          It stays because a store corrupted
                                          past its own checksum should land on
                                          the identity fill rather than on
                                          whatever follows. */

gmdj:                                  /* DJ Filter (slot 1). A sweeps the corner
                                          either side of a centre detent, B is
                                          resonance.
                                          D-18, RESOLVED and not to be
                                          re-opened: control C AND button C are
                                          unassigned in this mode. Drive was the
                                          only candidate for C and D-05 rules it
                                          out, and a knob that plainly does
                                          nothing is more honest than one
                                          duplicating its neighbour. So this
                                          routine reads neither EQ_S_GC nor
                                          EQ_S_BTN_C, CC 53 and CC 56 are inert
                                          here, and that absence is a decision.
                                          Button A forces control A to the
                                          centre value. The very next thing this
                                          routine does is subtract EQ_CENTRE and
                                          take `bls.w gmout` inside EQ_DETENT --
                                          so centre lands in the detent, and the
                                          detent is a REAL bypass: no band
                                          designed, mask left at 0 by gclr.
                                          That chain is deliberate. If the
                                          detent ever changes width, come back
                                          and check button A still bypasses. */
{ctl(EQ_S_GA, dest="r2")}\
    movw    r3, #{EQ_CENTRE}
    subs    r2, r2, r3
    mov     r0, r2
    cmp     r0, #0
    it      lt
    rsblt   r0, r0, #0
    cmp     r0, #{EQ_DETENT}
    bls.w   gmout                      /* in the detent: a real bypass */
    subs    r0, r0, #{EQ_DETENT}
    vmov    s0, r0
    vcvt.f32.u32 s0, s0
{_f("s1", "ghsinv", "r1")}
    vmul.f32 s0, s0, s1
    vmov.f32 s1, #1.0
    vcmpe.f32 s0, s1
    vmrs    APSR_nzcv, fpscr
    ble     gmdjc
    vmov.f32 s0, s1
gmdjc:
    cmp     r2, #0
    bgt     gmdjhp
    movs    r7, #{EQ_T_HCUT}           /* left: a low-pass, table backwards */
    vmov.f32 s1, #1.0
    vsub.f32 s0, s1, s0
    b       gmdjf
gmdjhp:
    movs    r7, #{EQ_T_LCUT}           /* right: a high-pass */
gmdjf:
    vmov.f32 s1, #16.0
    vmul.f32 s0, s0, s1
    vmov.f32 s18, s0                   /* keep the corner IN TABLE STEPS -- the
                                          second section is placed relative to
                                          it, and doing that in Hz would need a
                                          divide the table already avoids. */
    bl      gftab
    vmov.f32 s16, s0
    ldr     r0, [r6, #{EQ_S_GB}]
    adr.w   r1, gqres
    bl      gqmap
    vmov.f32 s17, s0
    movs    r0, #0
    mov     r1, r7
    vmov.f32 s0, s16
    vmov.f32 s1, s17
{zero}
    bl      gsetband
    movs    r2, #1
    str     r2, [r6, #{EQ_S_MASK}]
    /* CONTROL C IS THE SLOPE, and this supersedes D-18. D-18 locked C
       unassigned because drive was the only candidate and D-05 banned it;
       slope is pure biquad work, so the objection does not reach it.

       BE HONEST ABOUT WHAT THIS CONTROL DOES. Once two sections exist the
       ASYMPTOTIC slope is 24 dB/oct and stays there -- cascaded biquads
       quantise at 12 dB a stage and no amount of sweeping changes that. What
       varies continuously is the KNEE: how far up the band the second section
       sits, so how early the transition starts and how abruptly it bites.
       Converged on the first section is the hard 24 dB corner; spread away
       from it the response leans on one section near the corner and reads as
       roughly 12 dB there, tightening to 24 further out. That is a real and
       useful sweep and it is not a continuous slope, so do not label it one.

       Fully anticlockwise there is no second section at all, which IS a true
       12 dB/oct -- the b5 behaviour, still reachable, deliberately kept.

       CC 59 rides on top through the ordinary `ctl` forced-value path: held,
       control C reads as maximum and the knee snaps hard 24. Released, the
       encoder's own position returns untouched with no jump. That needs no
       special case in hook F -- see the note in the return for why routing
       CC 53 into EQ_S_SLOPE was considered and rejected. */
{ctl(EQ_S_GC, EQ_S_SLOPE, EQ_FORCE_MAX, dest="r0")}
    bl      gu16
    vmov.f32 s1, #1.0
    vcmpe.f32 s0, s1
    vmrs    APSR_nzcv, fpscr
    blt.w   gmout                      /* bottom of C: one section, a true 12 */
    vmov.f32 s1, #16.0
    vsub.f32 s0, s1, s0                /* clockwise converges, so 16 - index */
{_f("s1", "gdjspr", "r2")}
    vmul.f32 s0, s0, s1                /* the offset, in table steps */
    cmp     r7, #{EQ_T_HCUT}
    beq     gmdjs1
    vsub.f32 s0, s18, s0               /* high-pass: spread downward */
    b       gmdjs2
gmdjs1:
    vadd.f32 s0, s18, s0               /* low-pass: spread upward */
gmdjs2:
    bl      gftab
    vmov.f32 s16, s0
    movs    r0, #1                     /* second section, non-resonant */
    mov     r1, r7
    vmov.f32 s0, s16
{_f("s1", "gqfix", "r2")}
{zero}
    bl      gsetband
    movs    r2, #3
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout

gmmix:
    bl      gmix3
    movs    r2, #7
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout

gmix3:                                 /* bands 0-2: low shelf, mid bell, high shelf.
                                          RULE 3: control A is HIGH, C is LOW.
                                          The band table stays low-to-high --
                                          only the control each band reads is
                                          reversed, so gmixf/gmixq/gmixt need no
                                          reordering and stay legible.

                                          THE PUSHES ARE DJ KILLS, and b6 had
                                          them backwards. b6 killed a band by
                                          leaving it DISABLED, reasoning that a
                                          disabled band passes nothing. That is
                                          true of the BAND and false of the
                                          AUDIO: a disabled band applies no
                                          filter, so the signal passes straight
                                          through. Worse, Rule 1 puts every band
                                          at 0 dB at centre, so at rest the kill
                                          was removing a filter that was already
                                          transparent and did nothing audible at
                                          all. The user found exactly that --
                                          the kills only worked once an encoder
                                          had been turned away from centre.

                                          A kill must REMOVE that part of the
                                          spectrum, dramatically, FROM CENTRE,
                                          which is where the knobs rest. So a
                                          held kill swaps the band's TYPE rather
                                          than nudging its gain: low becomes a
                                          high-pass at 150 Hz, high becomes a
                                          low-pass at 4 kHz, mid becomes a wide
                                          -24 dB scoop at 700 Hz. Attenuating
                                          instead would not do it -- ggain's
                                          floor is -24 dB and a shelf at -24 dB
                                          still passes everything past its
                                          corner.

                                          The band is always designed either
                                          way, so the mask is always 7 and the
                                          stored encoder value is never written.
                                          Release returns the sound to exactly
                                          where the knob is: D-03, unchanged. */
    push    {{lr}}
    movs    r7, #0
gmix3l:
    adr.w   r0, gmixf
    add.w   r0, r0, r7, lsl #2
    vldr    s16, [r0]                  /* the corner, the same either way */
    movs    r0, #{EQ_S_BTN_C}
    sub.w   r0, r0, r7, lsl #2         /* band 0 -> push C, 1 -> B, 2 -> A */
    ldr     r0, [r6, r0]
    cmp     r0, #0
    bne     gmix3k
    movs    r0, #{EQ_S_GC}
    sub.w   r0, r0, r7, lsl #2         /* band 0 -> control C, 2 -> control A */
    ldr     r0, [r6, r0]
    bl      ggain
    vmov.f32 s18, s0
    adr.w   r0, gmixq
    add.w   r0, r0, r7, lsl #2
    vldr    s17, [r0]
    adr.w   r0, gmixt
    ldrb    r1, [r0, r7]
    b       gmix3s
gmix3k:                                /* held: the kill shape for this band */
    adr.w   r0, gmixkq
    add.w   r0, r0, r7, lsl #2
    vldr    s17, [r0]
    adr.w   r0, gmixkg
    add.w   r0, r0, r7, lsl #2
    vldr    s18, [r0]
    adr.w   r0, gmixk
    ldrb    r1, [r0, r7]
gmix3s:
    mov     r0, r7
    vmov.f32 s0, s16
    vmov.f32 s1, s17
    vmov.f32 s2, s18
    bl      gsetband
    adds    r7, #1
    cmp     r7, #3
    blo     gmix3l
    pop     {{pc}}

gmtone:                                /* Tone + Filter (slot 4, dial position
                                          5). A is the HIGH shelf, B is the LOW
                                          shelf, C is a bipolar filter with a
                                          centre detent.

                                          D-14 reshaped `Full desk` into this,
                                          and it is the only built mode the
                                          three-encoder budget genuinely broke.
                                          Losing the mid band buys tone AND a
                                          filter on one panel with no mode
                                          change, which is worth more live than
                                          three gains and an unreachable filter.

                                          RULE 3: control A is HIGH. That is the
                                          same correction b6 made to Mixer EQ
                                          after the user found it reading upside
                                          down inside the first minute of play,
                                          and it is not negotiable by preference.

                                          THE TWO PUSHES ARE KILLS, and they are
                                          literally two of Mixer EQ's three --
                                          the same gmixk / gmixkq / gmixkg
                                          shapes at the same corners, which is
                                          why this routine sits directly under
                                          `gmix3` rather than in dial order.

                                          A KILL SWAPS THE BAND'S TYPE and still
                                          calls `gsetband`; it never disables a
                                          band and never omits a mask bit. That
                                          is D-25, and it is what makes the
                                          gesture dramatic FROM CENTRE, which is
                                          where the knobs rest under Rule 1. b6
                                          killed by leaving the band DISABLED:
                                          a disabled band applies no filter, so
                                          the audio passed straight through and
                                          the kill removed something that was
                                          already transparent. Attenuating is no
                                          better -- `ggain`'s floor is -24 dB
                                          and a shelf at -24 dB still passes
                                          everything past its corner.

                                          RULE 1: both shelves take `ggain`,
                                          exactly 0.0 dB in the detent, and the
                                          filter is not engaged at all inside
                                          its own. Centred knobs give two flat
                                          shelves and two undesigned bands --
                                          the EQ doing nothing. */
    /* BAND 0 -- the LOW shelf, on control B, killed by push B. The corner is
       gmixf index 0, 150 Hz, and it is the same corner either way. */
    adr.w   r0, gmixf
    vldr    s16, [r0]
    ldr     r3, [r6, #{EQ_S_BTN_B}]    /* D-03: read the button, write nothing */
    cmp     r3, #0
    bne     gmtnk0
    ldr     r0, [r6, #{EQ_S_GB}]
    bl      ggain
    vmov.f32 s18, s0                   /* the gain goes to the CALLEE-saved s18,
                                          which is `gmix3`'s own shape and what
                                          hook G's `vpush {{d8, d9, d10}}`
                                          exists for.
                                          Nothing between here and `bl gsetband`
                                          calls anything either way. */
    adr.w   r0, gmixq
    vldr    s17, [r0]
    adr.w   r0, gmixt
    ldrb    r1, [r0]                   /* EQ_T_LSHELF */
    b       gmtns0
gmtnk0:                                /* held: kill low -> a high-pass at 150 Hz */
    adr.w   r0, gmixkq
    vldr    s17, [r0]
    adr.w   r0, gmixkg
    vldr    s18, [r0]
    adr.w   r0, gmixk
    ldrb    r1, [r0]                   /* EQ_T_LCUT */
gmtns0:
    movs    r0, #0
    vmov.f32 s0, s16
    vmov.f32 s1, s17
    vmov.f32 s2, s18
    bl      gsetband
    /* BAND 1 -- the HIGH shelf, on control A, killed by push A. Index 2 of the
       same five tables: 4 kHz, and the kill there is a low-pass. */
    adr.w   r0, gmixf
    vldr    s16, [r0, #8]
    ldr     r3, [r6, #{EQ_S_BTN_A}]
    cmp     r3, #0
    bne     gmtnk1
    ldr     r0, [r6, #{EQ_S_GA}]
    bl      ggain
    vmov.f32 s18, s0
    adr.w   r0, gmixq
    vldr    s17, [r0, #8]
    adr.w   r0, gmixt
    ldrb    r1, [r0, #2]               /* EQ_T_HSHELF */
    b       gmtns1
gmtnk1:                                /* held: kill high -> a low-pass at 4 kHz */
    adr.w   r0, gmixkq
    vldr    s17, [r0, #8]
    adr.w   r0, gmixkg
    vldr    s18, [r0, #8]
    adr.w   r0, gmixk
    ldrb    r1, [r0, #2]               /* EQ_T_HCUT */
gmtns1:
    movs    r0, #1
    vmov.f32 s0, s16
    vmov.f32 s1, s17
    vmov.f32 s2, s18
    bl      gsetband
    movs    r2, #3
    str     r2, [r6, #{EQ_S_MASK}]     /* WRITTEN HERE, so the mode is complete
                                          and correct even if the filter turns
                                          out to be sitting in its detent */
    /* BAND 2 -- the filter. THIS BIPOLAR LAW IS A SECOND COPY OF `gmdj`'s
       control A, written out line for line, and the two have to be changed
       together. It is deliberately not shared. The two detent branches do
       different things: `gmdj`'s dead band is a whole-mode bypass that leaves
       the mask at 0, while this one still has two shelves running and a mask of
       3. D-30 also puts `gmdj` out of bounds for reworking here -- the five
       modes that are already playing are not touched by this plan.

       C's push forces the RAW CENTRE, which is Rule 4's OPEN gesture: held, the
       filter stops acting and lets everything through; released, the encoder's
       stored position comes back untouched, so nothing jumps.

       THE FILTER IS DELIBERATELY NON-RESONANT. There is no fourth encoder for
       resonance in this mode and tone wins the two it has. DJ Filter is where
       resonance lives, and putting it here would cost a shelf. Not forgotten --
       decided. */
{ctl(EQ_S_GC, EQ_S_BTN_C, EQ_FORCE_CENTRE, dest="r2")}
    movw    r3, #{EQ_CENTRE}
    subs    r2, r2, r3
    mov     r0, r2
    cmp     r0, #0
    it      lt
    rsblt   r0, r0, #0
    cmp     r0, #{EQ_DETENT}
    bls.w   gmout                      /* inside the detent the filter is not
                                          engaged at all, and bands 2 and 3 take
                                          `gmout`'s identity fill (D-25) rather
                                          than being switched off. That is what
                                          makes crossing the detent a 50 ms ramp
                                          instead of a click. The mask stays 3. */
    subs    r0, r0, #{EQ_DETENT}
    vmov    s0, r0
    vcvt.f32.u32 s0, s0
{_f("s1", "ghsinv", "r1")}
    vmul.f32 s0, s0, s1
    vmov.f32 s1, #1.0
    vcmpe.f32 s0, s1
    vmrs    APSR_nzcv, fpscr
    ble     gmtnc
    vmov.f32 s0, s1
gmtnc:
    cmp     r2, #0
    bgt     gmtnhp
    movs    r7, #{EQ_T_HCUT}           /* left: a low-pass, index taken backwards
                                          -- anticlockwise closes to a thump */
    vmov.f32 s1, #1.0
    vsub.f32 s0, s1, s0
    b       gmtnf
gmtnhp:
    movs    r7, #{EQ_T_LCUT}           /* right: a high-pass -- clockwise thins
                                          out to hats */
gmtnf:
    vmov.f32 s1, #16.0
    vmul.f32 s0, s0, s1
    bl      gftab
    vmov.f32 s16, s0                   /* the corner in Hz, CALLEE-saved, so the
                                          second section converges on it without
                                          designing it a second time */
{_f("s17", "gqfix", "r2")}
    movs    r0, #2
    mov     r1, r7
    vmov.f32 s0, s16
    vmov.f32 s1, s17
{zero}
    bl      gsetband
    movs    r2, #7
    str     r2, [r6, #{EQ_S_MASK}]
    /* BAND 3 -- CC 59's cascade: a CONVERGED second section at the same corner,
       exactly as `gmtwin` does it. BE HONEST ABOUT IT. Once two sections exist
       the asymptotic slope is 24 dB/oct and stays there, and converged sections
       are the hard corner. There is NO variable knee in this mode -- that is DJ
       Filter's encoder 4, it costs a whole encoder, and all three are spent. */
    ldr     r3, [r6, #{EQ_S_SLOPE}]
    cmp     r3, #0
    beq.w   gmout
    movs    r0, #3
    mov     r1, r7
    vmov.f32 s0, s16
    vmov.f32 s1, s17
{zero}
    bl      gsetband
    movs    r2, #15
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout

gmtwin:                                /* Dual Cut (slot 1).
                                          RULE 3: A is the HIGH cut, B is the
                                          LOW cut -- high sits at the top. C is
                                          the resonance both sections share.
                                          RULE 1: centre opens both cuts
                                          OUTWARD, high cut to 12 kHz and low
                                          cut to 40 Hz, so centred knobs pass
                                          everything. Through b5 both cuts
                                          landed on 692.8 Hz at centre and
                                          cancelled to silence.
                                          The law is `ghalf`: the anticlockwise
                                          half is a plateau at fully open, and
                                          the clockwise half closes the cut.
                                          Monotonic, and `Reset Mid` is a real
                                          bypass for that section. */
{ctl(EQ_S_GC, dest="r0")}
    adr.w   r1, gqres
    bl      gqmap
    vmov.f32 s17, s0                   /* shared resonance */
{ctl(EQ_S_GA, EQ_S_BTN_A, EQ_FORCE_CENTRE, dest="r0")}
    bl      ghalf
    vmov.f32 s1, #16.0
    vmul.f32 s0, s0, s1
    vmov.f32 s1, #16.0
    vsub.f32 s0, s1, s0                /* open = 16 (12 kHz), closed = 0 */
    bl      gftab
    vmov.f32 s16, s0
    vmov.f32 s19, s0                   /* keep it for the 24 dB/oct cascade */
{setband(0, EQ_T_HCUT)}
{ctl(EQ_S_GB, EQ_S_BTN_B, EQ_FORCE_CENTRE, dest="r0")}
    bl      ghalf
    vmov.f32 s1, #16.0
    vmul.f32 s0, s0, s1                /* open = 0 (40 Hz), closed = 16 */
    bl      gftab
    vmov.f32 s18, s0                   /* keep it for the cascade too */
    vmov.f32 s16, s0
{setband(1, EQ_T_LCUT)}
    movs    r2, #3
    str     r2, [r6, #{EQ_S_MASK}]
    ldr     r3, [r6, #{EQ_S_SLOPE}]
    cmp     r3, #0
    beq.w   gmout
    vmov.f32 s16, s19                  /* 24 dB/oct: a second, non-resonant */
{_f("s17", "gqfix", "r2")}
{setband(2, EQ_T_HCUT)}
    vmov.f32 s16, s18
{_f("s17", "gqfix", "r2")}
{setband(3, EQ_T_LCUT)}
    movs    r2, #15
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout

gmphone:                               /* Band Pass (slot 2). A drags the centre,
                                          B sets the width.
                                          RULE 1: centre is MAXIMUM width -- a
                                          half-width of 8 table steps either
                                          side, which gftab clamps to the ends
                                          of the range, so centred knobs pass
                                          everything. Through b5 centre gave a
                                          202-2451 Hz band. Clockwise narrows.
                                          C and its push are unassigned. */
{ctl(EQ_S_GA, dest="r0")}
    bl      gu16
    vmov.f32 s18, s0                   /* centre, in table steps */
{ctl(EQ_S_GB, EQ_S_BTN_B, EQ_FORCE_CENTRE, dest="r0")}
    bl      ghalf
{_f("s1", "gbpnar", "r1")}
    vmul.f32 s0, s0, s1                /* narrowing, 0 .. (8 - 0.4) */
{_f("s1", "gbpwid", "r1")}
    vsub.f32 s19, s1, s0               /* half-width, 8.0 down to 0.4 */
{_f("s17", "gqbell", "r2")}
    vsub.f32 s0, s18, s19
    bl      gftab
    vmov.f32 s16, s0
{setband(0, EQ_T_LCUT)}
{_f("s17", "gqbell", "r2")}
    vadd.f32 s0, s18, s19
    bl      gftab
    vmov.f32 s16, s0
{setband(1, EQ_T_HCUT)}
    movs    r2, #3
    str     r2, [r6, #{EQ_S_MASK}]
    /* CONTROL C IS GAIN, which turns Band Pass into isolate-and-boost: cut
       everything outside the band, then lift what is left. Distinct from the
       unbuilt Peak mode, which boosts WITHOUT cutting its surroundings.

       THE BAND BUDGET IS FOUR AND THIS MODE NOW WANTS FIVE. Two cuts, one
       gain bell, and a symmetric 24 dB/oct cascade needs two more. So the two
       compete, and gain wins when it is asked for:

         gain flat (C in the detent)  -> bands 2 and 3 are free, and CC 59
                                         cascades both edges exactly as in b6
         gain dialled in              -> band 2 is the bell and CC 59 is
                                         ignored in this mode

       Gain flat is the resting state, so CC 59 keeps working everywhere the
       performer has not deliberately asked for a boost. The alternative --
       dropping the cascade outright -- would have cost a capability that
       already works on hardware, for a case that only arises once you turn
       control C. Rule 1 is safe either way: the detent is flat, so at centre
       no bell is designed at all. */
    ldr     r0, [r6, #{EQ_S_GC}]
    movw    r3, #{EQ_CENTRE}
    subs    r0, r0, r3
    cmp     r0, #0
    it      lt
    rsblt   r0, r0, #0
    cmp     r0, #{EQ_DETENT}
    bls     gmbpsl                     /* flat: the cascade may have the bands */
    /* ORDER MATTERS HERE, AND IT IS THE WHOLE OF THE b10 FAULT.

       s0-s15 are caller-saved: a callee may destroy any of them and owes the
       caller nothing. s16-s31 are callee-saved, which is why hook G's prologue
       `vpush {{d8, d9, d10}}` and why every other mode routine parks its
       frequency, its Q and its gain in s16..s21 across a call. This routine improvised,
       and that is the defect -- not the maths, not the mode, not the remap.

       What b7 (`61efbede`) shipped, and b10 was simply the first image to put
       on hardware: the gain was computed FIRST and parked in s2, then `gftab`
       was called. `gftab` ends `vldr s2, [r1]`, so s2 came back holding the
       CENTRE FREQUENCY IN HERTZ. `gsetband`'s ABI is s2 = gain in dB, so the
       bell was designed at 40 to 12000 dB where at most +9 was asked for. The
       `vmov.f32 s2, s2` that used to sit two instructions above `bl gsetband`
       was a NO-OP that read as intent, which is how it survived review.

       The fix is a reorder, not a relocation: compute the frequency first and
       keep it in s16, then compute the gain LAST, so nothing whatsoever runs
       between `bl ggain` and `bl gsetband` and there is no lifetime to get
       wrong. This is now the same shape as every other mode routine. The five
       modes still to be built use this same compute-a-gain-then-call-gsetband
       pattern; copy THIS one. */
    vmov.f32 s0, s18                   /* the bell sits on the centre frequency */
    bl      gftab
    vmov.f32 s16, s0                   /* the bell's centre -- CALLEE-saved */
{_f("s17", "gqbell", "r2")}
    ldr     r0, [r6, #{EQ_S_GC}]
    bl      ggain                      /* gain LAST: nothing between here and
                                          gsetband, so nothing can destroy it */
    vmov.f32 s2, s0
    movs    r0, #2
    movs    r1, #{EQ_T_PARAM}
    vmov.f32 s0, s16
    vmov.f32 s1, s17
    bl      gsetband
    movs    r2, #7
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout
gmbpsl:
    ldr     r3, [r6, #{EQ_S_SLOPE}]
    cmp     r3, #0
    beq.w   gmout
{_f("s17", "gqfix", "r2")}
    vsub.f32 s0, s18, s19              /* 24 dB/oct: cascade both edges */
    bl      gftab
    vmov.f32 s16, s0
{setband(2, EQ_T_LCUT)}
{_f("s17", "gqfix", "r2")}
    vadd.f32 s0, s18, s19
    bl      gftab
    vmov.f32 s16, s0
{setband(3, EQ_T_HCUT)}
    movs    r2, #15
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout

gmnotch:                               /* Notch (slot 6). A is the frequency, B is
                                          Q, C is DEPTH.
                                          RULE 1: centre is ZERO depth, so the
                                          band is designed but transparent and
                                          centred knobs pass everything.
                                          Through b5 depth was a -24 dB
                                          constant and centre gave a deep notch
                                          at 692.8 Hz. Giving C the depth is
                                          also what finally gives that encoder
                                          real work in this mode.
                                          C's push forces maximum depth -- the
                                          slam-the-notch gesture, and the one
                                          button here that does something
                                          `Reset Mid` cannot. */
{ctl(EQ_S_GA, dest="r0")}
    bl      gu16
    bl      gftab
    vmov.f32 s16, s0
{ctl(EQ_S_GB, dest="r0")}
    adr.w   r1, gqnot
    bl      gqmap
    vmov.f32 s17, s0
{ctl(EQ_S_GC, EQ_S_BTN_C, EQ_FORCE_MAX, dest="r0")}
    bl      ghalf
{_f("s1", "gnotchg", "r1")}
    vmul.f32 s18, s0, s1               /* 0 dB at centre, -24 dB at full CW */
{setband(0, EQ_T_PARAM, gain="    vmov.f32 s2, s18")}
    movs    r2, #1
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout

gmtilt:                                /* Tilt (slot 5, dial position 6). A is
                                          the tilt, B is the hinge, C and its
                                          push are unassigned.

                                          TWO SHELVES ON ONE FREQUENCY, GEARED
                                          TOGETHER. There is no tilt designer
                                          type and there does not need to be:
                                          a low shelf and a high shelf sharing
                                          a corner, given equal and opposite
                                          gain, IS a tilt. Clockwise lifts the
                                          treble and drops the bass by the same
                                          number of decibels, so the see-saw
                                          pivots rather than the whole level
                                          moving -- which is exactly what a
                                          performer expects of a tone tilt and
                                          what an asymmetric law would break.
                                          That is why this calls `gtiltd` and
                                          not `ggain`.

                                          RULE 1: `gtiltd` returns exactly 0.0
                                          inside the detent, so both shelves are
                                          designed and both are flat. The hinge
                                          needs NO centre handling at all -- a
                                          shelf at 0 dB is transparent whatever
                                          its corner is, so only the control
                                          that maps to gain is Rule 1's problem.

                                          Bands 2 and 3 are not touched, and
                                          `gmout` gives them the identity fill
                                          (D-25). This routine reads neither
                                          EQ_S_GC, EQ_S_BTN_B nor EQ_S_BTN_C:
                                          CCs 53, 55 and 56 are inert here, and
                                          that absence is a decision the
                                          simulator's unread-control check turns
                                          into an executable claim. */
{ctl(EQ_S_GA, EQ_S_BTN_A, EQ_FORCE_MAX, dest="r0")}
    bl      gtiltd
    vmov.f32 s18, s0                   /* the tilt, in dB. s18 is CALLEE-saved,
                                          so it survives the two calls between
                                          here and the second `gsetband`. Both
                                          shelves need it, so it HAS to outlive
                                          a call -- parking it in s2 the way b10
                                          did would put the hinge frequency in
                                          the gain slot. Copy gmphone's order,
                                          never b10's. */
{ctl(EQ_S_GB, dest="r0")}
    bl      gu16
    bl      gftab
    vmov.f32 s16, s0                   /* the hinge -- BOTH shelves share it,
                                          which is what makes the pivot a pivot
                                          rather than two unrelated shelves */
{_f("s17", "gqfix", "r2")}
{setband(0, EQ_T_LSHELF, gain="    vneg.f32 s2, s18")}
{setband(1, EQ_T_HSHELF, gain="    vmov.f32 s2, s18")}
    movs    r2, #3
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout

gmpeak:                                /* Peak (slot 7, dial position 8). A is
                                          the frequency, B is the Q, C is the
                                          gain. One band; three live encoders.

                                          PEAK BOOSTS WITHOUT CUTTING ANYTHING
                                          AROUND IT, and that is the whole
                                          distinction from Band Pass. Band Pass
                                          cuts everything outside its window
                                          first and then lifts what is left;
                                          this touches nothing but the bell.
                                          They sound nothing alike and they are
                                          reached from different dial positions
                                          on purpose. Do not "unify" them.

                                          RULE 1: `ggain` is exactly 0.0 dB in
                                          the centre detent, so the band is
                                          designed and transparent. Frequency
                                          and Q are free at centre for the same
                                          reason Tilt's hinge is.

                                          Button A is unread -- CC 54 is inert
                                          here, deliberately. B forces maximum Q
                                          and C maximum boost: both are Rule 4
                                          hold-to-slam, both forced in the RAW
                                          CC domain so they take the same curve
                                          the encoder would, and neither writes
                                          the stored value, so release hands
                                          back with no jump. */
{ctl(EQ_S_GA, dest="r0")}
    bl      gu16
    bl      gftab
    vmov.f32 s16, s0
{ctl(EQ_S_GB, EQ_S_BTN_B, EQ_FORCE_MAX, dest="r0")}
    adr.w   r1, gqres
    bl      gqmap
    vmov.f32 s17, s0
{ctl(EQ_S_GC, EQ_S_BTN_C, EQ_FORCE_MAX, dest="r0")}
    bl      ggain                      /* the gain LAST, and no call whatsoever
                                          between here and `gsetband` -- this is
                                          gmphone's corrected order and it is
                                          the shape every remaining mode copies */
    vmov.f32 s18, s0
{setband(0, EQ_T_PARAM, gain="    vmov.f32 s2, s18")}
    movs    r2, #1
    str     r2, [r6, #{EQ_S_MASK}]
    b.w     gmout

gmmnot:                                /* PHASER on the panel (slot 8, dial
                                          position 9). A sweeps the whole stack,
                                          B is the depth, C is the spread.

                                          THE SHAPE IS A PHASER'S; THE
                                          MECHANISM IS FOUR `gsetband` CALLS.
                                          D-15 renamed the slot away from
                                          `Phaser` on a DSP argument that still
                                          holds: a real phaser is a cascade of
                                          all-pass sections with feedback, which
                                          is new per-sample DSP and D-19 bans it
                                          outright. NONE OF THAT IS REOPENED
                                          HERE. There is still no all-pass, no
                                          feedback and no per-sample code --
                                          four Param bands, four calls, no
                                          filter state of our own.

                                          What changed at b14 is the LABEL, and
                                          the performer's argument for it is
                                          about sound rather than topology: a
                                          small number of
                                          non-harmonically-related notches
                                          sweeping together as one body IS the
                                          audible signature of a phaser, and
                                          that is what came off the module. It
                                          is emphatically NOT a comb -- a comb's
                                          notches are harmonically locked and
                                          sound metallic and tuned, and these
                                          are neither. It also sounds nothing
                                          like the single Notch at dial 7, which
                                          is what earns it a slot.

                                          THE SPACING IS A RATIO, NOT AN
                                          INTERVAL COUNT. `gftab`'s table is
                                          logarithmic, so a fixed number of
                                          table steps is a fixed RATIO between
                                          neighbours, and geometric spacing can
                                          never be a comb's arithmetic harmonic
                                          series. What is NOT automatic is
                                          landing away from a musical ratio;
                                          EQ_MN_SP_LO / EQ_MN_SP_HI carry that
                                          arithmetic and the reason for it, and
                                          no offline gate has an opinion on it.

                                          RULE 1 RESTS ENTIRELY ON THE DEPTH.
                                          A Param band at 0 dB is H(z) = 1
                                          whatever its frequency and its Q are,
                                          so `ghalf` returning exactly 0.0 at
                                          and below centre makes all four bands
                                          transparent wherever sweep and spread
                                          happen to sit. Those two carry no
                                          Rule 1 obligation at all.

                                          ALL FOUR BANDS ARE DESIGNED ON EVERY
                                          REBUILD and the mask is always 15, so
                                          `gmout` finds nothing to fill. That is
                                          D-25 satisfied rather than dodged: no
                                          band is ever switched off, and a notch
                                          is made inaudible by its depth
                                          reaching 0 dB, never by being skipped.

                                          BUTTON A IS UNREAD -- CC 54 is inert
                                          here, deliberately, and that absence is
                                          a decision the simulator's
                                          unread-control check turns into an
                                          executable claim. */
{ctl(EQ_S_GA, dest="r0")}
    bl      gu16
    vmov.f32 s19, s0                   /* the base index, in table steps. s18,
                                          s19 and s20 are all CALLEE-saved,
                                          because every one of them has to
                                          survive FOUR `bl gsetband` calls and
                                          s0-s15 would not survive one. This is
                                          `gmtilt`'s case, not `gmphone`'s:
                                          there is no reordering that removes
                                          the hazard, so the values go where
                                          `vpush {{d8, d9, d10}}` protects them. */
{ctl(EQ_S_GC, EQ_S_BTN_C, EQ_FORCE_MAX, dest="r0")}
    adr.w   r1, gmnsp
    bl      gqmap                      /* the spread, in table steps: linear
                                          over the whole control range, because
                                          it has no Rule 1 obligation and so
                                          does not want `ghalf`'s plateau */
    vmov.f32 s20, s0
{ctl(EQ_S_GB, EQ_S_BTN_B, EQ_FORCE_MAX, dest="r0")}
    bl      ghalf
{_f("s1", "gnotchg", "r1")}
    vmul.f32 s18, s0, s1               /* 0 dB at centre, -24 dB at full CW --
                                          `gmnotch`'s depth law, unchanged */
{_f("s17", "gqmn", "r1")}              /* one fixed Q, shared by all four */
    movs    r2, #15
    str     r2, [r6, #{EQ_S_MASK}]     /* WRITTEN BEFORE THE LOOP: all four bands
                                          are designed unconditionally, so 15 is
                                          correct from the first instruction and
                                          no early exit added later can leave the
                                          mode half-described */
    movs    r7, #0
gmmnl:
    vmov    s0, r7
    vcvt.f32.u32 s0, s0
    vmov.f32 s1, #1.5
    vsub.f32 s0, s0, s1                /* -1.5, -0.5, +0.5, +1.5 ... */
    vmul.f32 s0, s0, s20               /* ... spreads from the centre, so the */
    vadd.f32 s0, s0, s19               /* stack stays symmetric about the sweep */
    bl      gftab                      /* -> Hz, and it CLAMPS the table's ends,
                                          so a stack driven off either end lands
                                          on 40 Hz or 12 kHz rather than wrapping */
    vmov.f32 s16, s0
    mov     r0, r7
    movs    r1, #{EQ_T_PARAM}
    vmov.f32 s0, s16
    vmov.f32 s1, s17
    vmov.f32 s2, s18
    bl      gsetband                   /* unconditional, all four times: there is
                                          no branch anywhere in this routine that
                                          reaches EQ_S_MASK while skipping a band */
    adds    r7, #1
    cmp     r7, #4
    blo     gmmnl
    b.w     gmout

gmfmt:                                 /* Formant (slot 9, dial position 10).
                                          A is the vowel, B the intensity, C an
                                          octave shift on all three formants at
                                          once.

                                          THREE BELLS PARKED ON THE FIRST THREE
                                          RESONANCES OF A VOCAL TRACT, morphing
                                          continuously I -> E -> A -> O -> U.
                                          The frequencies are Peterson and
                                          Barney's own measurements -- see
                                          EQ_FMT_TABLE for the citation, for how
                                          the rows were computed, and for the
                                          label discrepancy that had to be
                                          settled before a single instruction of
                                          this routine was written. They are the
                                          only constants in this patch that came
                                          from outside it.

                                          THE ORDER IS D-29 AND IT IS THE WHOLE
                                          DESIGN. Front to back, so every step
                                          is between ADJACENT vowels and every
                                          intermediate knob position interpolates
                                          into a real vowel rather than into a
                                          chord of two unrelated ones. A
                                          different ordering would still sweep;
                                          it would just stop sounding like
                                          speech in between.

                                          THE INTERPOLATION IS CONTROL RATE AND
                                          MUST STAY THAT WAY. It runs here,
                                          inside `gapply`, once per accepted
                                          change of the generation counter --
                                          which is where DJ Filter's knee
                                          spread, Band Pass's half-width and
                                          Dual Cut's bipolar corner already do
                                          their sums. D-19 forbids the arm
                                          computing anything per sample or
                                          reading a sample buffer, and nothing
                                          here does either: the result of every
                                          sum is handed to an ordinary
                                          `gsetband`, and 1010music's designer
                                          and its 50 ms smoother do the rest. If
                                          a future session wants the morph
                                          "smoother" and reaches for a frame
                                          loop, THE SMOOTHER IS ALREADY THERE,
                                          on the other side of `gsetband`, and
                                          a frame loop is what caused every
                                          failure of 2026-09-04.

                                          RULE 1 RESTS ENTIRELY ON THE
                                          INTENSITY, exactly as Phaser's rests
                                          on its depth. A Param at 0 dB is
                                          H(z) = 1 whatever its frequency and Q
                                          are, so `ghalf` returning exactly 0.0
                                          at and below centre makes all three
                                          bells transparent wherever the vowel
                                          and the shift happen to sit. Those two
                                          carry no Rule 1 obligation at all,
                                          which is what lets the vowel rest on a
                                          real vowel instead of on a boundary.

                                          BAND 3 IS NOT SKIPPED. The mask is 3
                                          bands' worth, 7, and band 3 falls
                                          through to `gmout`'s identity fill.
                                          That is D-25: no band is ever switched
                                          off, a band a mode does not want is a
                                          flat Param, and the stock smoother
                                          ramps every transition in and out.

                                          BUTTONS A AND C ARE UNREAD. CC 54 and
                                          CC 56 are inert in this mode,
                                          deliberately: the vowel has no
                                          "correct" position to slam to and the
                                          shift's neutral is already the detent,
                                          which `Reset Mid` reaches without
                                          spending a CC (Rule 4). That absence
                                          is a decision, and the simulator's
                                          unread-control check turns it into an
                                          executable claim rather than this
                                          comment. */
{ctl(EQ_S_GC, dest="r0")}
    bl      gfshift
    vmov.f32 s17, s0                   /* the shift multiplier */
{ctl(EQ_S_GB, EQ_S_BTN_B, EQ_FORCE_MAX, dest="r0")}
    bl      ghalf
    vmov.f32 s18, s0                   /* the intensity: 0.0 at and below
                                          centre, 1.0 forced by button B */
{ctl(EQ_S_GA, dest="r0")}
    /* THE VOWEL POSITION. `gftab`'s clamp-split-lerp, on a five-row table
       instead of a seventeen-entry one, and inline rather than a helper because
       everything it produces has to outlive three `bl gsetband` calls anyway. */
    vmov    s0, r0
    vcvt.f32.u32 s0, s0
{_f("s1", "gfmtis", "r1")}
    vmul.f32 s0, s0, s1                /* 0.0 .. 4.0 across the knob */
    vmov.f32 s1, #4.0
    vcmpe.f32 s0, s1
    vmrs    APSR_nzcv, fpscr
    ble     gmfmc
    vmov.f32 s0, s1                    /* THE TOP CLAMP IS NOT DECORATIVE. A
                                          14-bit CC can carry 16383 where this
                                          arm expects at most 16256, which is
                                          4.03 rows into a five-row table -- a
                                          read off the end of it, straight into
                                          whatever the pool put next. `gftab`
                                          clamps for the same reason. The bottom
                                          needs no clamp: an unsigned convert of
                                          a store word cannot go below zero. */
gmfmc:
    vcvt.u32.f32 s1, s0
    vmov    r0, s1
    cmp     r0, #{len(EQ_FMT_TABLE) - 2}
    it      hi
    movhi   r0, #{len(EQ_FMT_TABLE) - 2}
    vmov    s1, r0
    vcvt.f32.u32 s1, s1
    vsub.f32 s16, s0, s1               /* the row in r0, clamped so that row + 1
                                          is still inside the table -- `gftab`'s
                                          own guard on its top entry -- and the
                                          fraction in s16.

                                          s16, s17, s18 AND r8 ARE ALL
                                          CALLEE-SAVED, because every one of
                                          them has to survive THREE `bl
                                          gsetband` calls. This is `gmtilt`'s
                                          and `gmmnot`'s case rather than
                                          `gmphone`'s: each value is consumed
                                          once per band, so there is no
                                          reordering that removes the hazard,
                                          and the prologue's
                                          `vpush {{d8, d9, d10}}` is what
                                          protects them.

                                          `gmphone`'s lesson still applies to
                                          the one value it fits: the per-band
                                          GAIN is loaded last, with no call
                                          whatsoever between it and `gsetband`,
                                          so it never needs protecting at all.
                                          Choose per value, not by rote. */
    add.w   r0, r0, r0, lsl #1         /* row * 3 floats ... */
    adr.w   r8, gfmtf
    add.w   r8, r8, r0, lsl #2         /* ... which is the twelve-byte stride */
    movs    r2, #7
    str     r2, [r6, #{EQ_S_MASK}]     /* WRITTEN BEFORE THE LOOP: three bands
                                          are designed unconditionally, so 7 is
                                          correct from the first instruction and
                                          no early exit added later can leave the
                                          mode half-described */
    movs    r7, #0
gmfmtl:
    add.w   r3, r8, r7, lsl #2
    vldr    s0, [r3]                   /* this vowel's Fn ... */
    vldr    s1, [r3, #12]              /* ... and the next vowel's, one row on */
    vsub.f32 s1, s1, s0
    vfma.f32 s0, s1, s16               /* between the two, by the fraction */
    vmul.f32 s0, s0, s17               /* and shifted -- all three formants move
                                          together, which is what makes it read
                                          as a bigger or smaller mouth rather
                                          than as a different vowel */
    adr.w   r3, gfmtq
    add.w   r3, r3, r7, lsl #2
    vldr    s1, [r3]                   /* this formant's Q: fixed per formant,
                                          not per vowel */
    adr.w   r3, gfmtg
    add.w   r3, r3, r7, lsl #2
    vldr    s2, [r3]
    vmul.f32 s2, s2, s18               /* the gain LAST, and nothing between
                                          here and `gsetband`. All three reach
                                          0 dB together at centre, and rise as a
                                          declining ladder -- F1 loudest,
                                          because F1 is the body of the vowel */
    mov     r0, r7
    movs    r1, #{EQ_T_PARAM}
    bl      gsetband                   /* unconditional, all three times: there
                                          is no branch in this routine that
                                          reaches EQ_S_MASK while skipping a
                                          band */
    adds    r7, #1
    cmp     r7, #3
    blo     gmfmtl
    b.w     gmout

gmout:
    /* EVERY BAND THE MODE DID NOT BUILD IS DESIGNED AS AN EXPLICIT IDENTITY,
       not switched off. This is what makes a mode change silent, and it is
       worth understanding before anyone "simplifies" it back.

       A band that stops being enabled stops contributing INSTANTLY: the output
       jumps from whatever that band was doing to dry, and a jump is a click.
       A band that starts being enabled is worse -- it resumes with whatever
       coefficients and filter history it had when it last ran, which is the
       ring b7 shipped.

       Neither is fixable by clearing state. Clearing state removes the ring and
       keeps the click, which is what b8 did and what the user rejected.

       So no band is ever switched off. Unused bands are designed as a Param at
       0 dB, which 1010music's own designer resolves to b0 = 1, b1 = a1,
       b2 = a2 -- H(z) = 1 exactly, a bit-exact wire once its state settles, and
       self-correcting because z1 accumulates a1*(x - y) which is zero when
       y = x. The band keeps running, so its history stays continuous and there
       is nothing stale to resume from.

       Then the ONLY thing that ever changes is the target coefficient set, and
       the stock smoother already ramps current toward target with a 50 ms time
       constant -- (blockSize/sampleRate)/0.05 per block, set by the ctor at
       obj+0x168. Every transition, in both directions, becomes that ramp.

       Measured, not assumed: across every pair of designs these five modes can
       produce, the interpolation path stays inside R2 AND never becomes more
       resonant than its own endpoints -- 0 excursions in 2970 ordered pairs,
       and 0 again for every design paired with this identity filler. So the
       ramp cannot ring on the way through.

       This also means the arm never writes a raw coefficient. It sets
       parameters and lets 1010music design and smooth, which keeps us out of
       the b0-versus-b2 slot-ordering question entirely. */
    movs    r7, #0
gmfill:
    add.w   r1, r7, r7, lsl #2
    add.w   r1, r4, r1, lsl #2
    ldr.w   r1, [r1, #{EQ_EN_OFF:#x}]
    cmp     r1, #0
    bne     gmfnext                    /* the mode built this one */
    mov     r0, r7
    movs    r1, #{EQ_T_PARAM}
{_f("s0", "gnullf", "r2")}
    vmov.f32 s1, #1.0
    movs    r3, #0
    vmov    s2, r3
    bl      gsetband
gmfnext:
    adds    r7, #1
    cmp     r7, #4
    blo     gmfill
    pop     {{pc}}

    .align 2
gdtab:
    .word 0
    .word {EQ_DESIGNERS[1] | 1:#010x}
    .word {EQ_DESIGNERS[2] | 1:#010x}
    .word {EQ_DESIGNERS[3] | 1:#010x}
    .word {EQ_DESIGNERS[4] | 1:#010x}
    .word {EQ_DESIGNERS[5] | 1:#010x}
gmixt:
    .byte {EQ_T_LSHELF}
    .byte {EQ_T_PARAM}
    .byte {EQ_T_HSHELF}
    .byte 0
    .align 2
gmixf:
{chr(10).join(f"    .float {v!r}" for v in EQ_MIX_F)}
gmixq:
    .float {EQ_Q_FIXED!r}
    .float {EQ_Q_BELL!r}
    .float {EQ_Q_FIXED!r}
gmixk:                                 /* the kill shape, per band */
    .byte {EQ_T_LCUT}                  /* kill low  -> high-pass at 150 Hz */
    .byte {EQ_T_PARAM}                 /* kill mid  -> a wide scoop at 700 Hz */
    .byte {EQ_T_HCUT}                  /* kill high -> low-pass at 4 kHz */
    .byte 0
    .align 2
gmixkq:
    .float {EQ_Q_FIXED!r}
    .float {EQ_Q_BELL!r}
    .float {EQ_Q_FIXED!r}
gmixkg:
    .float 0.0
    .float {EQ_NOTCH_G!r}
    .float 0.0
gqres:   .float {EQ_Q_LO!r}
         .float {EQ_Q_HI - EQ_Q_LO!r}
gqnot:   .float {EQ_QN_LO!r}
         .float {EQ_QN_HI - EQ_QN_LO!r}
gmnsp:   .float {EQ_MN_SP_LO!r}        /* Multi Notch's spread, in gftab table
                                          steps, in gqmap's {{lo, span}} shape.
                                          A fixed step count is a fixed RATIO --
                                          that is the whole trick, and it is why
                                          the stack can never be a harmonic
                                          series. See EQ_MN_SP_LO for where the
                                          octave falls on the knob. */
         .float {EQ_MN_SP_HI - EQ_MN_SP_LO!r}
gqmn:    .float {EQ_MN_Q!r}            /* one Q for all four notches: there is no
                                          fourth encoder, and there should not be */
gqph:    .float 0.5
         .float {EQ_PHONE_HALF!r}
gqfix:   .float {EQ_Q_FIXED!r}
gqbell:  .float {EQ_Q_BELL!r}
ggcut:   .float {EQ_G_CUT!r}
ggboo:   .float {EQ_G_BOOST!r}
gtiltg:  .float {EQ_TILT_G!r}          /* ONE constant, used both ways round --
                                          which is the symmetry, in the pool */
ghsinv:  .float {1.0 / EQ_HALF_SPAN!r}
gccinv:  .float {1.0 / EQ_CC_MAX!r}
gidxs:   .float {float(EQ_TBL_N) / EQ_CC_MAX!r}
gdjspr:  .float {EQ_DJ_SPREAD / float(EQ_TBL_N)!r}
ghspan:  .float {1.0 / (EQ_CC_MAX - EQ_CENTRE)!r}
gbpwid:  .float {EQ_BP_WIDE!r}
gbpnar:  .float {EQ_BP_WIDE - EQ_BP_NARROW!r}
gnotchg: .float {EQ_NOTCH_G!r}
gnullf:  .float {EQ_NULL_FREQ!r}
gfmtis:  .float {float(len(EQ_FMT_TABLE) - 1) / EQ_CC_MAX!r}
                                       /* the vowel knob's scale: raw CC to a
                                          row index 0 .. 4, gidxs's shape with
                                          a different table length */

/* The shift law's two spans, in `ggain`'s own two-constant shape: the
   multiplier is 1.0 + span * u either side of the detent, so ONE `vfma` serves
   both directions and the detent is a true unity rather than something that
   rounds to one. An octave down is a span of -0.5, an octave up is +1.0.

   BOTH LABELS ARE LOAD-BEARING AND THIS COMMENT IS DELIBERATELY ABOVE THEM
   RATHER THAN TRAILING THE FIRST. A block comment opened on a `.float` line and
   closed several lines later swallows every label in between, and that is not a
   hypothetical: it ate `gfmtup` on the first attempt at this pool. The only
   symptom was a symbol quietly missing from the table. */
gfmtdn:  .float {EQ_FMT_DN - 1.0!r}
gfmtup:  .float {EQ_FMT_UP - 1.0!r}
gfmtq:                                 /* per FORMANT, not per vowel: F1 broad,
                                          F2 and F3 progressively narrower */
{chr(10).join(f"    .float {v!r}" for v in EQ_FMT_Q)}
gfmtg:                                 /* the declining gain ladder, in dB at
                                          full intensity. F1 loudest, because it
                                          is the body of the vowel */
{chr(10).join(f"    .float {v!r}" for v in EQ_FMT_G)}
gfmtf:                                 /* FIVE VOWELS x THREE FORMANTS, ROW
                                          MAJOR. A vowel is a twelve-byte
                                          stride, so the NEXT vowel's Fn sits at
                                          [row_ptr, #12] and the interpolation
                                          needs no second pointer.

                                          Frequencies in HERTZ, written straight
                                          into the object -- this is the one
                                          mode whose frequencies do not come out
                                          of `gftab`, so `gftab`'s clamp is not
                                          protecting them. EQ_FMT_TABLE's
                                          import-time assert is what does, at
                                          both extremes of the shift. */
{fmt_tbl}
gtable:
{chr(10).join(f"    .float {v!r}" for v in EQ_FTABLE)}
"""
    code, syms = assemble("hookG", src)
    code = bytearray(code)
    code[syms["gnext"]:syms["gnext"] + 4] = enc_b_bl(org + syms["gnext"], EQ_HANDLER, False)
    return bytes(code)


def write_slot(d, va, expect, new, display_max=None):
    """Overwrite one 16-byte string slot, guarding on its current contents.

    Same fail-safe discipline as the code hooks: refuse rather than clobber
    something unrecognised. Null-pads the whole slot so no tail of the old
    string can survive past the new terminator.

    `display_max` is a SECOND, tighter limit for slots whose contents have to
    stay legible on the module's own screen rather than merely fit in flash.
    Storage and legibility are different questions and this function now asks
    both. See VER_DISPLAY_MAX.
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
    if display_max is not None and len(new) > display_max and not LEGACY_VERSION:
        refuse(f"{new!r} will not be legible on the module's splash screen.",
               f"It is {len(new)} characters and the display limit is "
               f"{display_max}.",
               "",
               "It would FIT in the slot -- storage allows 15 -- but the panel",
               "shows fewer, so it would be silently truncated and you would",
               "not know which image is on the card. `2.3.7-mod-b10` came back",
               "off the module as `2.3.7-mod-b1` and two hardware faults were",
               "very nearly recorded against the wrong build.",
               "",
               f"Use a shorter form, e.g. {new.replace('-mod', '')!r}.",
               "",
               "If you are deliberately REBUILDING A HISTORICAL IMAGE to check",
               "its fingerprint, pass --legacy-version to lift this check. The",
               "15-character storage limit still applies.")
    d[o:o + SLOT] = new.encode().ljust(SLOT, b"\0")
    vprint(f"splash {va:#x} {cur!r} -> {new!r}")


def main():
    global REASSEMBLE, VERBOSE, LEGACY_VERSION
    flags = sys.argv[1:]
    REASSEMBLE = "--reassemble" in flags
    VERBOSE = "-v" in flags or "--verbose" in flags
    LEGACY_VERSION = "--legacy-version" in flags
    argv = [a for a in flags if not a.startswith("-")]

    if len(argv) < 2:
        sys.exit(USAGE)

    inp, outp = argv[0], argv[1]
    ver = argv[2] if len(argv) > 2 else "2.3.7-mod"

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
               f"    {PY} patch_micro.py {inp} patched/MICRO.BIN")

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
                            DLYFX_GUARD_VA, DLYFX_GUARD),
                           # The compressor's fields are in RAM, so these four
                           # are the only way to check them: each is the
                           # instruction or literal that computes an address
                           # the patch is about to write to.
                           ("compressor dispatch", COMP_DISP_VA, COMP_DISP),
                           ("compressor on/off offset", COMP_FLAG_VA, COMP_FLAG),
                           ("compressor field offset", COMP_INIT_VA, COMP_INIT),
                           ("compressor default", COMP_DFLT_VA, COMP_DFLT),
                           ("session pointer", COMP_SESS_VA, COMP_SESS),
                           ("compressor DSP call", COMP_DSP_CALL, COMP_DSP_CALL_GUARD),
                           ("EQ vtable handler slot", EQ_VT_GUARD_VA, EQ_VT_GUARD),
                           ("EQ vtable store", EQ_CTOR_VA, EQ_CTOR_GUARD),
                           ("EQ designer entry", EQ_DSGN_VA, EQ_DSGN_GUARD),
                           ("EQ coefficient slot", EQ_COEF_VA, EQ_COEF_GUARD),
                           ("EQ frequency is in Hz", EQ_KONST_VA, EQ_KONST),
                           ("EQ handler audio fetch", EQ_HAND_VA, EQ_HAND_GUARD),
                           ("EQ gain is in dB", EQ_GAINK_VA, EQ_GAINK),
                           ("heap start", HEAP_LO_VA, HEAP_LO),
                           ("heap end", HEAP_HI_VA, HEAP_HI),
                           ("initial stack pointer", STACK_VA, STACK_TOP)):
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
    hookc_org = org
    vprint(f"{'C delay FX':16} {len(code):3} bytes at {org:#010x}   "
           f"CC{CC_BEATSYNC}->beatsync(0x17), CC{CC_PINGPONG}->pingpong(0x18), "
           f"CC{CC_DLYFILTER}->filtenable(0xab), CC{CC_DLYWIDTH}->filtquality(0xac) "
           f"@ key {DELAY_KEY:#x}")

    # --- fourth block: the compressor, chained in FRONT of hook C ---------
    org = BASE + len(d)
    assert org % 4 == 0
    code = build_comp_hook(org, hookc_org)
    if len(code) % 4:
        code += b"\x00" * (4 - len(code) % 4)
    d += code
    d[off:off + 4] = enc_b_bl(MIDI_POST_CALL, org, True)
    # Capture hook D's origin HERE, next to the block that built it, exactly as
    # hook C does above. The fifth block reassigns `org` before hook F is built,
    # so reading `org` down there yields hook E -- the compressor's audio-thread
    # DSP hook -- and hook F's `fnext` then branches the MIDI thread straight
    # into it. It faults on the first CC of any number, on any channel, because
    # every path through hook F converges on `fout`. That shipped as b1.
    hookd_org = org
    vprint(f"{'D compressor CC':16} {len(code):3} bytes at {org:#010x}   "
           f"CC{CC_COMP_ONOFF}->on/off +{COMP_ONOFF_OFF:#x} (session by proof), "
           f"CC{CC_COMP_FIRST}-{CC_COMP_FIRST + CC_COMP_COUNT - 1}->store @{PATCH_RAM:#x}, "
           f"then -> hook C {hookc_org:#010x}")

    # --- fifth block: stamp the store onto the live compressor each block ---
    off_e = COMP_DSP_CALL - BASE
    cur = bytes(d[off_e:off_e + 4])
    want = enc_b_bl(COMP_DSP_CALL, COMP_DSP_FN, True)
    if cur != want:
        refuse(f"The compressor call at {COMP_DSP_CALL:#x} is not what it should be.",
               f"found {cur.hex()}, expected {want.hex()}")
    org = BASE + len(d)
    assert org % 4 == 0
    code = build_comp_dsp_hook(org)
    if len(code) % 4:
        code += b"\x00" * (4 - len(code) % 4)
    d += code
    d[off_e:off_e + 4] = enc_b_bl(COMP_DSP_CALL, org, True)
    vprint(f"{'E compressor DSP':16} {len(code):3} bytes at {org:#010x}   "
           f"store {PATCH_RAM:#x} -> live object, then -> DSP {COMP_DSP_FN:#010x}")

    # --- sixth block: the EQ's CC arm, chained in FRONT of hook D ---------
    # `hookd_org` was captured beside the fourth block. Do NOT read `org` here:
    # it holds hook E by this point. The guard below refuses the whole class --
    # a chain target that is not the hook the comment names is a silent,
    # cross-thread wild branch, and the only symptom is a reboot on any MIDI.
    eq_chain_targets = {"hook D": hookd_org, "hook C": hookc_org}
    assert hookd_org != org, (
        f"hook F would chain to hook E ({org:#010x}), not hook D -- this is the "
        f"b1 defect: the MIDI thread branching into the compressor's audio-thread "
        f"DSP hook, faulting on the first CC of any number")
    assert len(set(eq_chain_targets.values())) == len(eq_chain_targets), \
        "two chain targets resolved to the same address -- one of them is wrong"
    org = BASE + len(d)
    assert org % 4 == 0
    code = build_eq_midi_hook(org, hookd_org)
    if len(code) % 4:
        code += b"\x00" * (4 - len(code) % 4)
    d += code
    d[off:off + 4] = enc_b_bl(MIDI_POST_CALL, org, True)
    vprint(f"{'F EQ CC':16} {len(code):3} bytes at {org:#010x}   "
           f"CC{CC_EQ_MODE}->mode, "
           f"CC{CC_EQ_A}/{CC_EQ_B}/{CC_EQ_C}->control A/B/C, "
           f"CC{CC_EQ_BTN_A}/{CC_EQ_BTN_B}/{CC_EQ_BTN_C}->button A/B/C, "
           f"CC{CC_EQ_BYPASS}->master bypass, CC{CC_EQ_SLOPE}->slope, "
           f"store @{EQ_STORE:#x}, then -> hook D {hookd_org:#010x}")

    # --- seventh block: the EQ arm itself, installed in the class vtable ---
    off_g = EQ_VT_HANDLER - BASE
    cur = struct.unpack("<I", bytes(d[off_g:off_g + 4]))[0]
    if cur != EQ_VT_GUARD:
        refuse(f"The EQ vtable slot at {EQ_VT_HANDLER:#x} is not what it should be.",
               f"found {cur:#010x}, expected {EQ_VT_GUARD:#010x}")
    org = BASE + len(d)
    assert org % 4 == 0
    code = build_eq_dsp_hook(org)
    if len(code) % 4:
        code += b"\x00" * (4 - len(code) % 4)
    d += code
    d[off_g:off_g + 4] = struct.pack("<I", org | 1)
    vprint(f"{'G EQ DSP':16} {len(code):3} bytes at {org:#010x}   "
           f"vtable {EQ_VT_HANDLER:#010x} -> arm, then -> stock handler {EQ_HANDLER:#010x}")

    for what, cc in FEATURES:
        print(f"     ok   {what:<28} {cc}")

    # Splash: drop 1010music's attribution so the modifications are not
    # credited to them, and mark the version as a modified build.
    write_slot(d, ATTRIB_VA, ATTRIB_STOCK, ATTRIB_NEW)
    write_slot(d, VER_VA, VER_STOCK, ver, display_max=VER_DISPLAY_MAX)

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
