/*
 * character.c -- the Character chain (hook N): four chained Airwindows
 * slots on the master bus, after the delay/reverb returns and before the
 * master EQ and compressor.
 *
 *   Destruction -> Drive -> Colour -> Saturation
 *
 * Each slot has a Type (one of its bank's four effects), an Amount (0 = the
 * slot is bypassed and costs nothing) and a Tone (each effect's most useful
 * second control). Driven by MIDI CC on channel 1; see CHR_CC_* below and
 * CHARACTER-DESIGN.md.
 *
 * Built by patch_micro.py --character: compiled with clang for Cortex-M7,
 * reduced and frozen in FROZEN_C, linked by the patcher itself. No libc.
 *
 * ---------------------------------------------------------------------------
 * The effects are ports of Airwindows plugins by Chris Johnson, used under
 * the MIT licence:
 *
 *   MIT License
 *
 *   Copyright (c) 2018 Chris Johnson
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a
 *   copy of this software and associated documentation files (the
 *   "Software"), to deal in the Software without restriction, including
 *   without limitation the rights to use, copy, modify, merge, publish,
 *   distribute, sublicense, and/or sell copies of the Software, and to
 *   permit persons to whom the Software is furnished to do so, subject to
 *   the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included
 *   in all copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 *   OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 *   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 *   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 *   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Every source file of every ported plugin carries "Airwindows uses the MIT
 * license" in its header; each was checked before porting.
 * ---------------------------------------------------------------------------
 */
#include <stdint.h>
#include <stddef.h>
#include "chr_port.h"
#include "chr_destruction.h"
#include "chr_drive.h"
#include "chr_colour.h"
#include "chr_saturation.h"

/* ---- where everything lives ----------------------------------------------
 * Patch RAM: the free gap between the stock main stack's top (0x240692a8)
 * and the floor of the stock UNCACHED POOL (0x2406f4e0). Device memory per the
 * MPU (regions 4/5): aligned access only. patch_micro.py carries the same
 * numbers, refuses to build if any of them reaches the pool, and a test
 * checks they agree.
 *
 * ch12: up to ch11 the slots were 0x3000 apart (0x2406cc00-0x24078c00) and the
 * OSD's UI-side area followed at 0x24078c00. Slots 1-3 and that area sat in
 * the 50 KB buffer the stock firmware reads preset.xml into and streams
 * samples through (pool allocator 0x08046ba4, buffer 0x2406f8c0-0x2407bcc0):
 * live slots corrupted preset loads. Each slot now has its real size, and
 * everything ends at 0x2406ef00. See CHARACTER-DESIGN.md, "Memory map". */
#define CHR_RAM_BASE   0x2406c200u
#define CHR_STORE_OFF  0x000u      /* MIDI-owned control store, 32 bytes */
#define CHR_ENGINE_OFF 0x040u      /* audio-owned engine state            */
#define CHR_DRY_OFF    0x200u      /* 2 x 256 floats for crossfades       */
/* Slot states, as offsets into the work area (which starts with the dry
 * buffers), and their reserved sizes. Packed smallest first; slot 2 (Colour:
 * ToTape8 is 8172 bytes) last. */
#define CHR_SLOT0_OFF   0x0800u    /* 0x2406cc00 */
#define CHR_SLOT1_OFF   0x0900u    /* 0x2406cd00 */
#define CHR_SLOT3_OFF   0x0a00u    /* 0x2406ce00 */
#define CHR_SLOT2_OFF   0x0b00u    /* 0x2406cf00 */
#define CHR_SLOT0_BYTES 0x0100u
#define CHR_SLOT1_BYTES 0x0100u
#define CHR_SLOT3_BYTES 0x0100u
#define CHR_SLOT2_BYTES 0x2000u
#define CHR_WORK_BYTES  (CHR_SLOT2_OFF + CHR_SLOT2_BYTES)    /* 0x2b00: to 0x2406ef00 */
#define CHR_OSD2_ADDR   0x24069400u /* the OSD's UI-side area, 0xe00 bytes */
_Static_assert(CHR_SLOT0_OFF >= 0x800u && CHR_SLOT1_OFF >= CHR_SLOT0_OFF + CHR_SLOT0_BYTES
               && CHR_SLOT3_OFF >= CHR_SLOT1_OFF + CHR_SLOT1_BYTES
               && CHR_SLOT2_OFF >= CHR_SLOT3_OFF + CHR_SLOT3_BYTES, "slots overlap");

#ifdef CHR_HOST
uint32_t chr_host_ram[(CHR_DRY_OFF + CHR_WORK_BYTES) / 4];
uint32_t chr_host_osd2[0xe00 / 4];
#define CHR_PTR(off) ((void *)((uint8_t *)chr_host_ram + (off)))
#define CHR_WORK(off) CHR_PTR(CHR_DRY_OFF + (off))
#define CHR_OSD2_PTR() ((void *)chr_host_osd2)
#else
#define CHR_PTR(off) ((void *)(uintptr_t)(CHR_RAM_BASE + (off)))
/* The WORK AREA -- the dry copies and the four slot states -- is found
 * through this word rather than a constant (volatile: never folded); the
 * patcher checks it holds CHR_RAM_BASE + CHR_DRY_OFF. ch12 removed the
 * untested --fast-ram variant that repointed it at D2 SRAM. */
const volatile uint32_t chr_work_base = CHR_RAM_BASE + CHR_DRY_OFF;
#define CHR_WORK(off) ((void *)(uintptr_t)(chr_work_base + (off)))
#define CHR_OSD2_PTR() ((void *)(uintptr_t)CHR_OSD2_ADDR)
#endif
/* Features the patcher switches on after linking (volatile: never folded).
 * Bit 0: hook Dr's CC 46 auto makeup is in this image (--v239), so the OSD
 * may show CC 46 and "Makeup  Auto". 0 in every other build. */
const volatile uint32_t chr_features = 0;
static const uint16_t chr_slot_off[4] = { CHR_SLOT0_OFF, CHR_SLOT1_OFF, CHR_SLOT2_OFF, CHR_SLOT3_OFF };
#define CHR_WORK_SLOT(s) CHR_WORK(chr_slot_off[(s) & 3u])

/* ---- MIDI map: channel 1, CC 14-29 ---------------------------------------- */
#define CHR_CHANNEL      0          /* 0-based: MIDI channel 1 */
#define CHR_CC_AMOUNT    14         /* 14-17 Amount, per slot in chain order */
#define CHR_CC_TONE      18         /* 18-21 Tone */
#define CHR_CC_TYPE      22         /* 22-25 Type, absolute, 4 bands of 32  */
#define CHR_CC_NEXT      26         /* 26-29 next Type, on any value > 0    */
#define CHR_CC_LAST      29

#define CHR_BANKS 4
#define CHR_TYPES 4
enum { BANK_DESTRUCTION, BANK_DRIVE, BANK_COLOUR, BANK_SATURATION };

#define CHR_MAGIC     0x43485231u   /* "CHR1" */
#define CHR_EMAGIC    0x43484531u   /* "CHE1" */
#define CHR_TONE_DEF  64

typedef struct {
    uint32_t magic;
    uint8_t amount[4];   /* 0-127, per slot */
    uint8_t tone[4];     /* 0-127 */
    uint8_t type[4];     /* 0-3   */
    uint32_t gen;        /* bumped by every accepted CC */
    uint32_t sum;        /* magic ^ amount ^ tone ^ type ^ gen, as words */
} chr_store_t;

typedef struct {
    uint32_t magic;
    uint32_t bad;                 /* consecutive blocks with a bad store sum */
    uint32_t snap[3];             /* last good amount/tone/type words */
    uint32_t type[4];             /* the type each slot's state belongs to */
    float fade[4];                /* 0 = bypassed, 1 = fully in */
    float amt[4], tone[4];        /* smoothed controls, 0..1 */
    uint32_t faults[4];           /* NaN/inf resets, for diagnosis */
    uint32_t blocks;              /* blocks processed with any slot live */
    uint32_t faults_in;           /* non-finite samples caught at the chain's exit */
} chr_engine_t;

/* Which type positions hold an effect. A reserved empty position is a
 * pass-through; "next Type" skips it. */
static const uint8_t chr_present[CHR_BANKS] = {
    CHR_PRESENT_DESTRUCTION, CHR_PRESENT_DRIVE, CHR_PRESENT_COLOUR, 0x0f,
};

#ifndef CHR_HOST
/* clang may lower struct copies to these; device memory forbids unaligned
 * access, so both work in words when they can and in bytes otherwise. */
void *memcpy(void *d, const void *s, size_t n)
{
    if ((((uintptr_t)d | (uintptr_t)s | n) & 3u) == 0) {
        volatile uint32_t *dw = d; const volatile uint32_t *sw = s;
        for (size_t i = 0; i < n / 4; i++) dw[i] = sw[i];
    } else {
        volatile uint8_t *db = d; const volatile uint8_t *sb = s;
        for (size_t i = 0; i < n; i++) db[i] = sb[i];
    }
    return d;
}

void *memset(void *d, int v, size_t n)
{
    if ((((uintptr_t)d | n) & 3u) == 0) {
        uint32_t w = (uint8_t)v * 0x01010101u;
        volatile uint32_t *dw = d;
        for (size_t i = 0; i < n / 4; i++) dw[i] = w;
    } else {
        volatile uint8_t *db = d;
        for (size_t i = 0; i < n; i++) db[i] = (uint8_t)v;
    }
    return d;
}
#ifdef CHR_CMP
/* The compressor types copy their state structs to the stack and back, which
 * clang lowers to this EABI entry point (word-aligned, length a multiple of 4). */
void __aeabi_memcpy4(void *d, const void *s, size_t n) { memcpy(d, s, n); }
#endif
#else
#include <string.h>
#endif

static uint32_t w_of(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint32_t store_sum(const chr_store_t *st)
{
    return st->magic ^ w_of(st->amount) ^ w_of(st->tone) ^ w_of(st->type) ^ st->gen;
}

static void store_seed(chr_store_t *st)
{
    for (int i = 0; i < 4; i++) { st->amount[i] = 0; st->tone[i] = CHR_TONE_DEF; st->type[i] = 0; }
    st->gen = 0;
    st->magic = CHR_MAGIC;
    st->sum = store_sum(st);
}

static int store_ok(const chr_store_t *st)
{
    if (st->magic != CHR_MAGIC || st->sum != store_sum(st)) return 0;
    for (int i = 0; i < 4; i++)
        if (st->amount[i] > 127 || st->tone[i] > 127 || st->type[i] > 3) return 0;
    return 1;
}

/* ---- the MIDI side --------------------------------------------------------
 * Called from the MIDI dispatcher for every CC (hook N's shim saves and
 * restores everything around it). chan is 0-15, cc 0-127, val14 is the
 * firmware's widened value (7-bit << 7). */
#include "chr_osd.h"
#ifdef CHR_CMP
#include "cmp_engine.h"                           /* 2.3.10: the compressor Type switch */
#include "clip_tempo.h"                           /* 2.3.10: Clip Tempo (Vinyl) */
#include "theme.h"                                /* 2.3.10-b20: the screen themes */
#include "chr_save.h"                             /* 2.3.10-b24: the chain's settings, saved */
#ifdef CHR_SC
#include "sidechain.h"                            /* 2.3.11: SC In, the compressor's sidechain */
#include "rec.h"                                  /* 2.3.12-b15: recording from the gate inputs */
#endif
#ifdef CHR_AUX
#include "aux.h"                                  /* 2.3.14-a1: AUX sends, the return */
#endif
#endif

/* 2.3.10 (b9): chr_cc returns the value the hooks behind it see -- hook N's
 * nmidi writes it back into the dispatcher's frame. Only CC 46 changes: a
 * press steps Off -> Normal -> Hot, and hook Da (Stock) is handed that mode
 * as one of its three bands. */
#ifdef CHR_CMP
#define CHR_CC_RET uint32_t
#define CHR_CC_DONE return val14
#else
#define CHR_CC_RET void
#define CHR_CC_DONE return
#endif

CHR_CC_RET chr_cc(uint32_t chan, uint32_t cc, uint32_t val14)
{
    chr_osd_any_cc(chan, cc, val14);              /* pad and delay switches: OSD only */
    if (chan != CHR_CHANNEL) CHR_CC_DONE;
#ifdef CHR_SC
    if (cc == SC_CC_IN) return sc_cc(cc, val14);  /* SC In, the compressor's sidechain input */
#endif
#ifdef CHR_AUX
    if (cc == AUX_CC_RET) { aux_cc(val14); CHR_CC_DONE; }   /* Return Lvl, the only AUX CC */
#endif
#ifdef CHR_CMP
    if (cc >= 40 && cc <= 48) return cmp_cc(cc, val14);   /* Type, next, the types' controls, OSD */
#else
    if (cc >= 40 && cc <= 46) {                   /* compressor: OSD only */
        if (cc == 46 && !(chr_features & 1u)) return;   /* no auto makeup in this build */
        chr_osd_comp_cc(cc, val14);
        return;
    }
#endif
    if (cc >= 50 && cc <= 59) { chr_osd_eq_cc(cc, val14); CHR_CC_DONE; }   /* the EQ's CCs: OSD only */
    if (cc < CHR_CC_AMOUNT || cc > CHR_CC_LAST) CHR_CC_DONE;
    chr_store_t *st = CHR_PTR(CHR_STORE_OFF);
    if (!store_ok(st)) store_seed(st);
    uint32_t v = val14 >> 7;
    if (v > 127) v = 127;
    uint32_t k = cc - CHR_CC_AMOUNT;
    uint32_t slot = k & 3;
    switch (k >> 2) {
    case 0: st->amount[slot] = (uint8_t)v; break;
    case 1: st->tone[slot] = (uint8_t)v; break;
    case 2: st->type[slot] = (uint8_t)(v >> 5); break;
    default:
        if (v == 0) CHR_CC_DONE;                   /* a button release: nothing */
        {
            uint32_t t = st->type[slot];
            for (int i = 0; i < CHR_TYPES; i++) {      /* skip empty positions */
                t = (t + 1) & 3;
                if (chr_present[slot] & (1u << t)) break;
            }
            st->type[slot] = (uint8_t)t;
        }
        break;
    }
    st->gen++;
    st->sum = store_sum(st);
    switch (k >> 2) {                              /* tell the OSD what moved */
    case 0: chr_osd_note(OSD_CHR, slot, OSD_C_AMOUNT, st->amount[slot], st->type[slot]); break;
    case 1: chr_osd_note(OSD_CHR, slot, OSD_C_TONE, st->tone[slot], st->type[slot]); break;
    default: chr_osd_note(OSD_CHR, slot, OSD_C_TYPE, st->amount[slot], st->type[slot]); break;
    }
    CHR_CC_DONE;
}

/* ---- slot state and dispatch ---------------------------------------------- */
typedef union { CHR_STATES_DESTRUCTION } bank0_u;
typedef union { CHR_STATES_DRIVE } bank1_u;
typedef union { CHR_STATES_COLOUR } bank2_u;
typedef union { purestdrive_t a; spiral2_t b; density3_t c; mojo_t d; } bank3_u;
#ifndef CHR_REF                     /* the double reference build is host-only */
_Static_assert(sizeof(bank0_u) <= CHR_SLOT0_BYTES, "Destruction state too big for slot 0");
_Static_assert(sizeof(bank1_u) <= CHR_SLOT1_BYTES, "Drive state too big for slot 1");
_Static_assert(sizeof(bank2_u) <= CHR_SLOT2_BYTES, "Colour state too big for slot 2");
_Static_assert(sizeof(bank3_u) <= CHR_SLOT3_BYTES, "Saturation state too big for slot 3");
#endif

static const uint32_t chr_state_bytes[CHR_BANKS] = {
    sizeof(bank0_u), sizeof(bank1_u), sizeof(bank2_u), sizeof(bank3_u),
};

void chr_mode_init(uint32_t bank, uint32_t type, void *state)
{
    if (bank >= CHR_BANKS) return;
    memset(state, 0, (chr_state_bytes[bank] + 3u) & ~3u);
    switch (bank) {
    case BANK_DESTRUCTION: CHR_INIT_DESTRUCTION(type, state); break;
    case BANK_DRIVE:       CHR_INIT_DRIVE(type, state); break;
    case BANK_COLOUR:      CHR_INIT_COLOUR(type, state); break;
    default: break;
    }
}

/* Run one effect in place. Returns 0 if the position is empty (the caller
 * then treats the slot as pass-through). */
int chr_mode_run(uint32_t bank, uint32_t type, void *state, float amt, float tone,
                 float *L, float *R, int n)
{
    switch (bank) {
    case BANK_DESTRUCTION: CHR_RUN_DESTRUCTION(type, state, amt, tone, L, R, n); break;
    case BANK_DRIVE:       CHR_RUN_DRIVE(type, state, amt, tone, L, R, n); break;
    case BANK_COLOUR:      CHR_RUN_COLOUR(type, state, amt, tone, L, R, n); break;
    case BANK_SATURATION:
        switch (type) {
        case 0: m_purestdrive(state, amt, tone, L, R, n); return 1;
        case 1: m_spiral2(state, amt, tone, L, R, n); return 1;
        case 2: m_density3(state, amt, tone, L, R, n); return 1;
        case 3: m_mojo(state, amt, tone, L, R, n); return 1;
        }
        break;
    }
    return 0;
}

/* ---- the audio side -------------------------------------------------------
 * The firmware's audio buffer descriptor (see the trace in CHARACTER-DESIGN):
 *   +0 capacity in frames, +4 frames this block (0 = use capacity),
 *   +8 float *left, +0xc float *right, +0x10 stale flag (contents not valid,
 *   to be cleared before use), +0x11 right valid (0 = mono, L only). */
typedef struct {
    int32_t cap;
    int32_t frames;
    float *L;
    float *R;
    uint8_t stale;
    uint8_t stereo;
} chr_buf_t;

#define CHR_FADE_SAMPLES 480.0f     /* 10 ms crossfade in and out */
#define CHR_SMOOTH_TAU   0.040f     /* 40 ms one-pole on Amount and Tone */
#define CHR_SUB          32         /* control-rate piece: 4 per 128-sample block */
#define CHR_LIMIT        8.0f       /* +18 dBFS: anything past it is clamped */

static void engine_reset(chr_engine_t *e)
{
    memset(e, 0, sizeof(*e));
    e->magic = CHR_EMAGIC;
}

static int engine_ok(const chr_engine_t *e)
{
    if (e->magic != CHR_EMAGIC) return 0;
    for (int s = 0; s < 4; s++) {
        if (e->type[s] > 3) return 0;
        if (!(e->fade[s] >= 0.0f && e->fade[s] <= 1.0f)) return 0;   /* NaN fails */
        if (!(e->amt[s] >= 0.0f && e->amt[s] <= 1.0f)) return 0;
        if (!(e->tone[s] >= 0.0f && e->tone[s] <= 1.0f)) return 0;
    }
    return 1;
}

static void slot_reset(chr_engine_t *e, int s, uint32_t type)
{
    e->type[s] = type;
    chr_mode_init((uint32_t)s, type, CHR_WORK_SLOT(s));
}

/* Clamp to +-CHR_LIMIT; returns 1 if a NaN or infinity was found. */
#ifdef CHR_FASTF
typedef uint32_t __attribute__((may_alias)) chr_u32a;
#endif
static int guard(float *b, int n)
{
    int bad = 0;
    for (int i = 0; i < n; i++) {
#ifdef CHR_FASTF
        /* The test below on the sample's bits, with no float compare: the
         * magnitude bits order like |x|, and inf and NaN sit above every
         * finite value, so this is !(|x| <= 8) for every input (a denormal,
         * which the FPU compares as 0, has small magnitude bits too). */
        if ((((const chr_u32a *)b)[i] & 0x7fffffffu) > 0x41000000u) {    /* 8.0f */
            float x = b[i];
#else
        float x = b[i];
        if (!(chr_fabsf(x) <= CHR_LIMIT)) {
#endif
            if (!chr_finite(x)) { bad = 1; x = 0.0f; }
            else x = x > 0.0f ? CHR_LIMIT : -CHR_LIMIT;
            b[i] = x;
        }
    }
    return bad;
}

void chr_block(chr_buf_t *h)
{
#ifdef CHR_AUX
    aux_ret(h);                                  /* In 1/2 joins the bus first, so the chain hears it */
#endif
    if (h->stale) return;                        /* nothing valid to colour */
    int n = h->frames ? h->frames : h->cap;
    if (n <= 0 || n > 256) return;
    float *L = h->L;
    float *R = h->stereo ? h->R : 0;
    if (!L) return;

    chr_store_t *st = CHR_PTR(CHR_STORE_OFF);
    chr_engine_t *e = CHR_PTR(CHR_ENGINE_OFF);
    if (!engine_ok(e)) engine_reset(e);
    if (st->magic != CHR_MAGIC) store_seed(st);  /* first block after boot */
    if (store_ok(st)) {
        e->snap[0] = w_of(st->amount); e->snap[1] = w_of(st->tone); e->snap[2] = w_of(st->type);
        e->bad = 0;
    } else if (++e->bad > 375) {                 /* a second of bad sums: start over */
        store_seed(st);
        e->snap[0] = 0; e->snap[1] = 0x40404040u; e->snap[2] = 0;
        e->bad = 0;
    }

    float k = 1.0f - chr_expf(-(float)CHR_SUB / (CHR_SR * CHR_SMOOTH_TAU));   /* per piece */
    float step = (float)n / CHR_FADE_SAMPLES;
    int live = 0;

    for (int s = 0; s < 4; s++) {
        uint32_t want_type = (e->snap[2] >> (8 * s)) & 3u;
        float amt_t = (float)((e->snap[0] >> (8 * s)) & 0x7fu) * (1.0f / 127.0f);
        float tone_t = (float)((e->snap[1] >> (8 * s)) & 0x7fu) * (1.0f / 127.0f);
        int on = amt_t > 0.0f && (chr_present[s] & (1u << want_type));
        float f0 = e->fade[s];
        float target;
        if (e->type[s] != want_type) {
            if (f0 <= 0.0f) { slot_reset(e, s, want_type); target = on ? 1.0f : 0.0f; }
            else target = 0.0f;                      /* fade out, then switch */
        } else {
            target = on ? 1.0f : 0.0f;
        }
        float f1 = f0 < target ? chr_minf(target, f0 + step) : chr_maxf(target, f0 - step);
        if (f0 <= 0.0f && f1 <= 0.0f) { e->fade[s] = 0.0f; continue; }   /* bypassed: free */
        if (f0 <= 0.0f) {                            /* waking up: clean state, no glide */
            slot_reset(e, s, e->type[s]);
            e->amt[s] = amt_t; e->tone[s] = tone_t;
        }
        live = 1;
        void *state = CHR_WORK_SLOT(s);
        int fading = !(f0 >= 1.0f && f1 >= 1.0f);
        float *dL = CHR_WORK(0), *dR = dL + 256;
        if (fading) {
            for (int i = 0; i < n; i++) dL[i] = L[i];
            if (R) for (int i = 0; i < n; i++) dR[i] = R[i];
        }
        /* The effects compute their coefficients once per call, so a control
         * that moves between calls steps. Calling in CHR_SUB-sample pieces
         * with the one-pole advanced per piece makes those steps a quarter of
         * the size (1.6% of the remaining distance each, 375 x 4 a second).
         * While a slot fades OUT (Amount to 0, or a Type change pending) its
         * controls are frozen: the crossfade alone carries the change, so the
         * effect does not slide its gain underneath the fade. */
        int ran = 1;
        int glide = target > 0.0f;
        for (int off = 0; off < n && ran; off += CHR_SUB) {
            int m = n - off < CHR_SUB ? n - off : CHR_SUB;
            if (glide) {
                e->amt[s] += k * (amt_t - e->amt[s]);
                e->tone[s] += k * (tone_t - e->tone[s]);
            }
            ran = chr_mode_run((uint32_t)s, e->type[s], state, e->amt[s], e->tone[s],
                               L + off, R ? R + off : 0, m);
        }
        int bad = guard(L, n);
        if (R) bad |= guard(R, n);
        if (bad || !ran) {
            if (bad) e->faults[s]++;
            if (fading) {                            /* dry is to hand: use it */
                for (int i = 0; i < n; i++) L[i] = dL[i];
                if (R) for (int i = 0; i < n; i++) R[i] = dR[i];
            }
            e->fade[s] = 0.0f;                       /* and restart from clean */
            slot_reset(e, s, e->type[s]);
            continue;
        }
        if (fading) {
            float w = f0, dw = (f1 - f0) / (float)n;
            for (int i = 0; i < n; i++) {
                w += dw;
                L[i] = dL[i] + w * (L[i] - dL[i]);
                if (R) R[i] = dR[i] + w * (R[i] - dR[i]);
            }
        }
        e->fade[s] = f1;
    }
    if (live) e->blocks++;
    /* The chain's exit (ch11): whatever arrived -- even with every slot
     * bypassed -- never hand a non-finite or runaway sample to the master EQ
     * and the compressor, whose recursive states would hold a NaN until the
     * next preset load. Non-finite becomes 0; beyond +-CHR_LIMIT (+18 dBFS,
     * far past the DAC's own clip) is clamped. A finite in-range bus is
     * untouched, bit for bit. */
    if (guard(L, n)) e->faults_in++;
    if (R && guard(R, n)) e->faults_in++;
}

#ifdef CHR_HOST
void *chr_host_ptr(uint32_t off) { return CHR_PTR(off); }
#endif
