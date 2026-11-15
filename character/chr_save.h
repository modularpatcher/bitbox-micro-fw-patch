/*
 * chr_save.h -- 2.3.10-b24: the master bus's settings kept across power-off
 * (CHR_CMP builds only). Included by character.c after the stores.
 *
 * The Character chain and the compressor sit on the master bus, so their
 * settings belong to the module, not to a preset: hidden registered integers
 * in the global store, the way the compressor Type is kept (cmp_engine.h), so
 * settings.tml carries them. Each packs four values seven bits apiece:
 *   rows 0-2  the Character chain's Amount, Tone and Type, slots 1-4;
 *   rows 3-7  the compressor types' controls (b25): Logical4's six, VariMu's
 *             six, Thunder's six, then auto makeup (Off / Normal / Hot), then
 *             (CHR_SC, 2.3.12) SC In in row 7's spare fourth place.
 * Stock's own compressor controls are not here: they are the preset's.
 * Hook T registers the rows, adds them to the global store (the stores' own
 * seeds), and offers boot's apply-all posts of 0x18d-0x19f to chr_post, which
 * takes the rows and hands every other id back. The UI tick puts each change
 * back into the global store and asks the compressor's saver for a
 * settings.tml write, two seconds after the last change (cmp_save_soon).
 */
#ifndef CHR_SAVE_H
#define CHR_SAVE_H

#define CHS_ROWS         8u
#define CHS_RAM          0x2406c0b0u  /* patch RAM: chs_ram_t (patch_micro.py: CHRSAVE_RAM) */
#define CHS_MAGIC        0x43485332u  /* "CHS2" */
/* patch_micro.py: CHRSAVE_IDS. Unregistered in stock and never an immediate in it. */
static const uint16_t chs_id[CHS_ROWS] = { 0x18du, 0x18eu, 0x18fu, 0x191u, 0x192u, 0x193u, 0x195u, 0x196u };

typedef struct {
    uint32_t magic;
    uint32_t w[CHS_ROWS];            /* the rows as the global store has them */
} chs_ram_t;                         /* 36 bytes */

#ifdef CHR_SC
static uint8_t sc_saved_in(void);          /* sidechain.h: SC In, kept through power-off (owner, 2026-11-08) */
static void sc_restore_in(uint32_t k);
#define CHS_SPARE(n) ((n) == 19u ? sc_saved_in() : 0u)
#else
#define CHS_SPARE(n) 0u
#endif

static uint32_t chs_pack(const uint8_t *b)
{
    return (uint32_t)b[0] | (uint32_t)b[1] << 7 | (uint32_t)b[2] << 14 | (uint32_t)b[3] << 21;
}

static uint8_t *chs_crow(chr_store_t *st, uint32_t k)
{
    return k == 0u ? st->amount : k == 1u ? st->tone : st->type;
}

/* the compressor's 19 values in row order: v[0..2][0..5], then auto makeup */
static uint8_t *chs_cval(cmp_store_t *s, uint32_t n)
{
    return n < 18u ? &s->v[n / 6u][n % 6u] : &s->amode;
}

static uint32_t chs_cpack(cmp_store_t *s, uint32_t r)
{
    uint8_t b[4];
    for (uint32_t i = 0; i < 4u; i++) {
        uint32_t n = 4u * r + i;
        b[i] = n < 19u ? *chs_cval(s, n) : CHS_SPARE(n);
    }
    return chs_pack(b);
}

/* the rows now; a store never used since power-on is its seed. 0: a CC was
 * halfway through a store, try again next tick. */
static int chs_rows(uint32_t *w)
{
    chr_store_t *st = CHR_PTR(CHR_STORE_OFF), cs;
    cmp_store_t *s = cmp_store(), ms;
    if (st->magic != CHR_MAGIC) { store_seed(&cs); st = &cs; }
    if (s->magic != CMP_MAGIC) { cmp_store_seed(&ms); s = &ms; }
    if (!store_ok(st) || !cmp_store_ok(s)) return 0;
    uint32_t g0 = st->sum, g1 = s->sum;
    for (uint32_t k = 0; k < 3u; k++) w[k] = chs_pack(chs_crow(st, k));
    for (uint32_t r = 0; r < 5u; r++) w[3u + r] = chs_cpack(s, r);
    return st->sum == g0 && s->sum == g1 && st->sum == store_sum(st) && s->sum == cmp_sum(s);
}

/* Hook T, at boot's apply-all, for ids 0x18d-0x19f: a saved row into its
 * store (1), or not one of ours (0: the post goes on to the engine). A Type
 * this build has no effect for is the slot's first; a value out of range,
 * its default. */
uint32_t chr_post(uint32_t id, int32_t v)
{
    uint32_t k = 0;
    while (k < CHS_ROWS && chs_id[k] != id) k++;
    if (k == CHS_ROWS) return 0u;
    uint32_t u = (uint32_t)v;
    if (k < 3u) {
        chr_store_t *st = CHR_PTR(CHR_STORE_OFF);
        if (!store_ok(st)) store_seed(st);
        uint8_t *f = chs_crow(st, k);
        for (uint32_t i = 0; i < 4u; i++) {
            uint32_t x = (u >> (7u * i)) & 127u;
            if (k == 2u && (x > 3u || !(chr_present[i] & (1u << x)))) x = 0u;
            f[i] = (uint8_t)x;
        }
        st->gen++;
        st->sum = store_sum(st);
    } else {
        cmp_store_t *s = cmp_store();
        if (!cmp_store_ok(s)) cmp_store_seed(s);
        for (uint32_t i = 0; i < 4u; i++) {
            uint32_t n = 4u * (k - 3u) + i, x = (u >> (7u * i)) & 127u;
            if (n >= 19u) {
#ifdef CHR_SC
                if (n == 19u) sc_restore_in(x);
#endif
                break;
            }
            if (n == 18u && x > CMP_AUTO_HOT) x = CMP_AUTO_OFF;
            *chs_cval(s, n) = (uint8_t)x;
        }
        s->gen++;
        s->sum = cmp_sum(s);
    }
    chs_ram_t *r = (chs_ram_t *)(uintptr_t)CHS_RAM;
    if (r->magic != CHS_MAGIC) {
        uint32_t w[CHS_ROWS];
        if (!chs_rows(w)) return 1u;
        for (uint32_t j = 0; j < CHS_ROWS; j++) r->w[j] = w[j];
        r->magic = CHS_MAGIC;
    }
    uint32_t w[CHS_ROWS];
    if (chs_rows(w)) r->w[k] = w[k];      /* what settings.tml holds: no write back */
    return 1u;
}

static void cmp_save_soon(uint32_t now);  /* cmp_engine.h */

/* UI side, every tick: a change by CC into the global store, saved soon */
static void chs_ui_tick(uint32_t now)
{
    chs_ram_t *r = (chs_ram_t *)(uintptr_t)CHS_RAM;
    if (r->magic != CHS_MAGIC) {          /* power-on: the global store's defaults, until boot posts */
        chr_store_t cs;
        cmp_store_t ms;
        store_seed(&cs);
        cmp_store_seed(&ms);
        for (uint32_t k = 0; k < 3u; k++) r->w[k] = chs_pack(chs_crow(&cs, k));
        for (uint32_t j = 0; j < 5u; j++) r->w[3u + j] = chs_cpack(&ms, j);
        r->magic = CHS_MAGIC;
    }
    uint32_t w[CHS_ROWS];
    if (!chs_rows(w)) return;             /* none yet, or a CC halfway through */
    int changed = 0;
    for (uint32_t k = 0; k < CHS_ROWS; k++)
        if (w[k] != r->w[k]) {
            cmp_x_setparam((void *)(uintptr_t)CMP_GLOBAL_STORE, chs_id[k], (int32_t)w[k]);
            r->w[k] = w[k];
            changed = 1;
        }
    if (changed) cmp_save_soon(now);
}

#endif
