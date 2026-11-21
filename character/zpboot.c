/*
 * zpboot.c -- 2.3.13-mod (2.3.12-zp1 in testing): the Character object,
 * unpacked into D2 SRAM at power-on (see patch_micro.py, apply_zp).
 *
 * Some modules' bootloaders write only the first 768 KB of MICRO.BIN, and
 * 2.3.12-mod is 804 KB. 2.3.13-mod keeps the Character object (effects,
 * compressor, clips, OSD, themes: 136 KB) LZMA-compressed in flash and runs it from D2
 * SRAM (0x30000000-0x30047fff), which stock never touches (traced, and the
 * 2.3.12-d2p probe watched it through ten minutes of use). Nothing here ever
 * writes flash.
 *
 * zp_boot stands in for main's call of the MPU set-up (0x08041190), before
 * main switches the caches on: stock's set-up runs first (the patcher has made
 * region 9, D2 SRAM, executable), then the I-cache goes on for the decoder's
 * own speed (main invalidates and enables it again straight after us), then
 * the object is decoded with the D-cache still off, so it is in RAM, not a
 * cache, when main's invalidations run. A stream that does not decode, or
 * output whose checksum is not the patcher's, stops here (`b .`): the patch's
 * hooks would otherwise run whatever was in D2.
 *
 * The decoder is raw LZMA1 with lc 0, lp 1, pb 1 (the patcher compresses with
 * exactly these), written from the format's public-domain reference
 * specification (LzmaSpec.cpp). The output buffer is the whole window.
 */
#include <stdint.h>
#include <stddef.h>

#define ZP_MAGIC     0x3150505au     /* "ZPP1" */
#define ZP_LC        0
#define ZP_LP        1
#define ZP_PB        1
#define ZP_STATES    12
#define ZP_POS_MAX   (1 << ZP_PB)

#define SCB_CCR      0xE000ED14u
#define SCB_ICIALLU  0xE000EF50u
#define REG(a)       (*(volatile uint32_t *)(uintptr_t)(a))

/* Filled in by the patcher: where the stream is, where it goes, its checksum,
 * and the scratch the probabilities live in while it runs. */
typedef struct {
    uint32_t magic, src, src_len, dst, dst_len, sum, probs;
} zp_desc_t;

const volatile zp_desc_t zp_desc = { ZP_MAGIC, 0u, 0u, 0u, 0u, 0u, 0u };

typedef struct {
    uint16_t choice, choice2;
    uint16_t low[ZP_POS_MAX][8], mid[ZP_POS_MAX][8], high[256];
} zp_len_t;

typedef struct {
    uint16_t lit[0x300 << (ZP_LC + ZP_LP)];
    uint16_t is_match[ZP_STATES][ZP_POS_MAX], is_rep0_long[ZP_STATES][ZP_POS_MAX];
    uint16_t is_rep[ZP_STATES], is_rep_g0[ZP_STATES], is_rep_g1[ZP_STATES], is_rep_g2[ZP_STATES];
    uint16_t slot[4][64], pos[115], align[16];
    zp_len_t len, rep_len;
} zp_probs_t;

typedef struct {
    const uint8_t *in, *end;
    uint32_t range, code;
    int bad;
} zp_rc_t;

static uint32_t zp_byte(zp_rc_t *r)
{
    if (r->in >= r->end) { r->bad = 1; return 0u; }
    return *r->in++;
}

static void zp_norm(zp_rc_t *r)
{
    if (r->range < (1u << 24)) {
        r->range <<= 8;
        r->code = (r->code << 8) | zp_byte(r);
    }
}

static uint32_t zp_bit(zp_rc_t *r, uint16_t *p)
{
    uint32_t v = *p, bound = (r->range >> 11) * v, b;
    if (r->code < bound) {
        v += ((1u << 11) - v) >> 5;
        r->range = bound;
        b = 0u;
    } else {
        v -= v >> 5;
        r->code -= bound;
        r->range -= bound;
        b = 1u;
    }
    *p = (uint16_t)v;
    zp_norm(r);
    return b;
}

static uint32_t zp_direct(zp_rc_t *r, uint32_t n)
{
    uint32_t res = 0u;
    do {
        r->range >>= 1;
        r->code -= r->range;
        uint32_t t = 0u - (r->code >> 31);
        r->code += r->range & t;
        zp_norm(r);
        res = (res << 1) + t + 1u;
    } while (--n);
    return res;
}

static uint32_t zp_tree(zp_rc_t *r, uint16_t *p, uint32_t bits)
{
    uint32_t m = 1u;
    for (uint32_t i = 0; i < bits; i++) m = (m << 1) + zp_bit(r, &p[m]);
    return m - (1u << bits);
}

static uint32_t zp_tree_rev(zp_rc_t *r, uint16_t *p, uint32_t bits)
{
    uint32_t m = 1u, s = 0u;
    for (uint32_t i = 0; i < bits; i++) {
        uint32_t b = zp_bit(r, &p[m]);
        m = (m << 1) + b;
        s |= b << i;
    }
    return s;
}

static uint32_t zp_len(zp_rc_t *r, zp_len_t *l, uint32_t ps)
{
    if (!zp_bit(r, &l->choice)) return zp_tree(r, l->low[ps], 3u);
    if (!zp_bit(r, &l->choice2)) return 8u + zp_tree(r, l->mid[ps], 3u);
    return 16u + zp_tree(r, l->high, 8u);
}

/* The distance minus one, as the reference keeps rep0..rep3. */
static uint32_t zp_dist(zp_rc_t *r, zp_probs_t *p, uint32_t len)
{
    uint32_t slot = zp_tree(r, p->slot[len < 3u ? len : 3u], 6u);
    if (slot < 4u) return slot;
    uint32_t nd = (slot >> 1) - 1u, d = (2u | (slot & 1u)) << nd;
    if (slot < 14u) return d + zp_tree_rev(r, p->pos + d - slot, nd);
    d += zp_direct(r, nd - 4u) << 4;
    return d + zp_tree_rev(r, p->align, 4u);
}

/* Returns 0 when exactly dst_len bytes came out of a well-formed stream. */
static int zp_decode(const uint8_t *src, uint32_t src_len, uint8_t *out, uint32_t n, zp_probs_t *p)
{
    uint16_t *all = (uint16_t *)p;
    for (uint32_t i = 0; i < sizeof(*p) / 2u; i++) all[i] = 1024u;
    zp_rc_t r = { src, src + src_len, 0xffffffffu, 0u, 0 };
    if (zp_byte(&r)) return 1;
    for (int i = 0; i < 4; i++) r.code = (r.code << 8) | zp_byte(&r);
    uint32_t state = 0u, rep0 = 0u, rep1 = 0u, rep2 = 0u, rep3 = 0u, at = 0u;
    while (at < n) {
        if (r.bad) return 2;
        uint32_t ps = at & (ZP_POS_MAX - 1u);
        if (!zp_bit(&r, &p->is_match[state][ps])) {
            uint16_t *lp = p->lit + 0x300u * ((at & ((1u << ZP_LP) - 1u)) << ZP_LC);
            uint32_t s = 1u;
            if (state >= 7u) {
                if (rep0 >= at) return 3;
                uint32_t mb = out[at - rep0 - 1u];
                do {
                    uint32_t mbit = (mb >> 7) & 1u;
                    mb <<= 1;
                    uint32_t b = zp_bit(&r, &lp[((1u + mbit) << 8) + s]);
                    s = (s << 1) | b;
                    if (mbit != b) break;
                } while (s < 0x100u);
            }
            while (s < 0x100u) s = (s << 1) | zp_bit(&r, &lp[s]);
            out[at++] = (uint8_t)s;
            state = state < 4u ? 0u : state < 10u ? state - 3u : state - 6u;
            continue;
        }
        uint32_t len;
        if (zp_bit(&r, &p->is_rep[state])) {
            if (!at) return 4;
            if (!zp_bit(&r, &p->is_rep_g0[state])) {
                if (!zp_bit(&r, &p->is_rep0_long[state][ps])) {   /* one byte at rep0 */
                    state = state < 7u ? 9u : 11u;
                    out[at] = out[at - rep0 - 1u];
                    at++;
                    continue;
                }
            } else {
                uint32_t d;
                if (!zp_bit(&r, &p->is_rep_g1[state])) d = rep1;
                else {
                    if (!zp_bit(&r, &p->is_rep_g2[state])) d = rep2;
                    else { d = rep3; rep3 = rep2; }
                    rep2 = rep1;
                }
                rep1 = rep0;
                rep0 = d;
            }
            len = zp_len(&r, &p->rep_len, ps);
            state = state < 7u ? 8u : 11u;
        } else {
            rep3 = rep2; rep2 = rep1; rep1 = rep0;
            len = zp_len(&r, &p->len, ps);
            state = state < 7u ? 7u : 10u;
            rep0 = zp_dist(&r, p, len);
            if (rep0 == 0xffffffffu) return at == n ? 0 : 5;   /* the end marker */
            if (rep0 >= at) return 6;
        }
        len += 2u;
        if (len > n - at) return 7;
        for (uint32_t i = 0; i < len; i++, at++) out[at] = out[at - rep0 - 1u];
    }
    return r.bad ? 2 : 0;
}

/* A word at a time, rotated: the patcher computes the same over its output. */
static uint32_t zp_sum(const uint32_t *w, uint32_t n)
{
    uint32_t h = 0x811c9dc5u;
    for (uint32_t i = 0; i < n / 4u; i++) h = ((h << 5) | (h >> 27)) ^ w[i];
    return h;
}

extern void zp_x_mpu(void);           /* 0x08041098: stock's MPU set-up */

void zp_boot(void)
{
    zp_x_mpu();
    REG(SCB_ICIALLU) = 0u;
    __asm volatile ("dsb sy\n isb sy" ::: "memory");
    REG(SCB_CCR) |= 1u << 17;          /* IC: main does this again in a moment */
    __asm volatile ("dsb sy\n isb sy" ::: "memory");
    const volatile zp_desc_t *z = &zp_desc;
    uint8_t *out = (uint8_t *)(uintptr_t)z->dst;
    if (z->magic != ZP_MAGIC
        || zp_decode((const uint8_t *)(uintptr_t)z->src, z->src_len, out, z->dst_len,
                     (zp_probs_t *)(uintptr_t)z->probs)
        || zp_sum((const uint32_t *)(uintptr_t)z->dst, z->dst_len) != z->sum)
        for (;;) { }
}
