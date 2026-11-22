/*
 * aux.h -- External FX (2.3.14-a5, --aux; AUX-SENDS-FEASIBILITY.md): the
 * delay's and reverb's sends as aux sends to an output, a return from In 1/2
 * into the master bus and, if asked, into the internal delay or reverb too
 * (a5), and channel 1's CC 63 (Return Lvl, swept in dB) with its pop-up. The
 * rest is set and forget (the owner): Settings rows only. And Dub Throw (a5):
 * a pad's delay or reverb send held full while a gate input is high, or CC
 * 103 / 106 on the pad's channel is held.
 * Only in the CHR_AUX object (chr13), so chr12 and 2.3.13-mod are untouched.
 *
 * Stock (traced, AUX-SENDS-FEASIBILITY.md):
 *   - a pad adds `out x send` into bus 0xd (the delay) or 0xe (the reverb);
 *     both effects are graph nodes that overwrite their bus IN PLACE, the send
 *     going in and the return coming out, and the master adds both buses into
 *     bus 0 (0x08096cf2, 0x08096d04), skipping a stale one. So the send exists
 *     only as each effect's render starts: the patcher points each class's
 *     render slot (0x080de090, 0x080de1ac) at aux_dly / aux_rev, which take it
 *     there and then call stock's render, which renders the rest of the graph.
 *   - each effect sleeps after 60 s with no send (its tracker, 0x080a5be8 /
 *     0x080a5c30): about 155 instructions a block instead of 17-39k. AUX
 *     forces that sleep, with the send marked stale, so the algorithm never
 *     runs and its parameter events are still taken.
 *   - waking clears nothing, so the lines are zeroed here, 16 KB a block,
 *     while the effect sleeps, and it may only wake once they are.
 * EFFECT costs a load and a compare; the first block of AUX fades the return
 * out (the effect runs on a muted send), and the tap fades in and out over a
 * block, so a mode change never clicks.
 * The record node's render slot (0x080ddfb4) is taken too, only to note where
 * the node is: it keeps which of In 1/2 the preset makes audio (inputiocon,
 * 0xd9), and the return takes only those; and whether it is monitoring the
 * inputs into bus 0 itself (Rec Mon), when the return stands aside (a3).
 */
#ifndef AUX_H
#define AUX_H

/* ---- where it lives: D2 SRAM1, free once zp_boot has unpacked (its
 * probabilities take 0x30000000-0x30001b00, at power-on only); cacheable
 * under region 9, so the taps' buffers are fast. patch_micro.py AUX_RAM. -- */
#define AUX_RAM         0x3001e000u
#define AUX_SET_OFF     0x000u       /* settings: posts (UI) and CCs (audio) */
#define AUX_UI_OFF      0x040u       /* the UI tick's */
#define AUX_AU_OFF      0x080u       /* the audio task's */
#define AUX_BUF_OFF     0x200u       /* 2 x 256 floats: a tap, before it is added */
#define AUX_DUB_OFF     0xa00u       /* Dub Throw: the audio task's, per pad position (a5) */
#define AUX_RAM_BYTES   0xc00u

#define AUX_SMAGIC      0x41555853u  /* "AUXS" */
#define AUX_UMAGIC      0x41555855u  /* "AUXU" */
#define AUX_OMAGIC      0x4155584fu  /* "AUXO": CC 63's pop-up memory */
#define AUX_AMAGIC      0x41555841u  /* "AUXA" */
#define AUX_DMAGIC      0x44554254u  /* "DUBT" */

/* the Settings rows (patch_micro.py AUX_IDS: unregistered in stock, never an
 * immediate in it, and not one of its mod destinations: patch_micro.py
 * MOD_DEST_LIST), in this order. Eight enums, then Return Lvl: a number,
 * stock's kind 7 (dB in thousandths, shown to 0.1 dB and stepped by 0.1 dB,
 * as the recording Gain row 0xba is), kept apart in rlvl. */
enum { AUX_S_DMODE, AUX_S_DOUT, AUX_S_RMODE, AUX_S_ROUT, AUX_S_HP, AUX_S_MONO, AUX_S_RET, AUX_S_RHP, AUX_S_RLVL,
       AUX_NS };                     /* a7: Return HP, Aux HP's corners on the return */
static const uint16_t aux_ids[AUX_NS] = { 0x1b2, 0x1b3, 0x1b6, 0x1b7, 0x1b9, 0x1ba, 0x1bb, 0x1b1, 0x1bd };
static const uint8_t aux_counts[AUX_NS] = { 3, 9, 3, 9, 10, 3, 4, 10, 0 };
static const uint8_t aux_defaults[AUX_NS] = { 0, 0, 0, 1, 0, 0, 0, 0, 0 };   /* Out3/4, Out5/6 */
#define AUX_RLVL_MIN    (-60000)     /* Return Lvl, thousandths of a dB: -60.0 ... */
#define AUX_RLVL_MAX    6000         /* ... +6.0 dB, default 0 dB */
enum { AUX_EFFECT, AUX_AUX, AUX_BOTH };
enum { AUX_MONO_SUM, AUX_MONO_LEFT, AUX_MONO_RIGHT };
enum { AUX_RET_OFF, AUX_RET_MIX, AUX_RET_DELAY, AUX_RET_REVERB };   /* a5: a3's ON is MIX */
/* AUX Out's values: stock's pad Output labels from Out3/4 (patch_micro.py
 * AUX_OUT_LABELS), and the bus each is (enum index = bus index, 0x080b290e) */
static const uint8_t aux_out_bus[9] = { 1, 2, 3, 6, 7, 8, 9, 10, 11 };
#define AUX_CC_RET      63u          /* channel 1: Return Lvl, 0.5 dB a step, 115 = 0 dB (0 -57.5, 127 +6.0) */
#define AUX_CC_UNITY    115u
#define AUX_REC_AUDIO1  0x280u       /* the record node: In 1 is audio (0x080a50d6) */
#define AUX_REC_AUDIO2  0x281u       /* In 2 (0x080a5268) */
#define AUX_REC_NOW     0x48u        /* recording now (cleared at 0x080a4e34) */
#define AUX_REC_INPUT   0x58u        /* Rec Input, 0x7c: 3 is Resample, which monitors nothing */
#define AUX_REC_MON     0x59u        /* Rec Mon, 0x7b: 0 On, 1 Auto, 2 Off */
#define AUX_REC_ACTIVE  0x5au        /* Rec Active, 0x7e: the recording screen is shown */
#define AUX_REC_MONBUS  0x68u        /* Mon Output, 0xbd: its bus; 0 is Out1/2 */

/* Dub Throw (a5). The row: 0 Off, 1-8 Del <input>, 9-16 Rev <input>, 17-24
 * D+R <input> (a7: both sends; the values before it keep their numbers). The
 * sends: the Sample class's (Sample, Multi-Sample, Granular) at voice +0x558 /
 * +0x55c, the Clip class's (Clip, Slicer) at +0xb84 / +0xb88. A voice's key,
 * +0x18, is its pad's position, row << 8 | column on page 0 (clip_idx). */
#define AUX_DUB_ID      0x1c1u       /* patch_micro.py DUB_ID: unregistered in stock, no immediate */
#define AUX_DUB_N       25u
#define AUX_CC_DTHROW   103u         /* the pad's channel: >= 64 throws the delay, < 64 lets go */
#define AUX_CC_RTHROW   106u         /* ... the reverb */
#define AUX_SEND_A      0x558u
#define AUX_SEND_B      0xb84u
#define AUX_W_REV       1u           /* which: the reverb send, else the delay's */
#define AUX_W_B         2u           /* which: the Clip class, else the Sample class */
#define AUX_SLICE       0x4000u      /* bytes of an effect's lines zeroed a block */
#define AUX_SDRAM_LO    0xc0000000u  /* the lines' allocator (0x08042dfc) hands out SDRAM */
#define AUX_SDRAM_HI    0xc4000000u
#define AUX_REGION_MAX  0x100000u
#define AUX_DC_R        0.9995f      /* the return's DC blocker: about 3.8 Hz */

/* the firmware's buffer descriptor (0x080a31d4, 0x080a3288, 0x080a3328) */
typedef struct {
    uint32_t cap, frames;            /* frames 0: cap */
    float *L, *R;
    uint8_t stale, stereo, pad[2];
} aux_desc_t;
_Static_assert(sizeof(aux_desc_t) == 20, "the bus array's stride");
#define AUX_BUSES       0x150e4u     /* msg + this + 20 x bus: the bus array (0x080a82b8) */
#define AUX_INPUTS      0x1588cu     /* msg + this + 20 x i: the inputs (0x080a82cc); [0] In 1/2 */
#define AUX_BUS_DLY     0xdu
#define AUX_BUS_REV     0xeu

/* the effect nodes (stock 2.3.4: the delay's init 0x080a740c, the reverb's 0x080a9afc) */
#define AUX_DLY_TRK     0xd0u        /* the idle tracker (0x080a7852: add.w sl, r5, #0xd0) */
#define AUX_DLY_FLUSHED 0x80u        /* the last line's "flushed" byte, also needed to sleep (0x080a7a82) */
#define AUX_REV_TRK     0x94u        /* (0x080a96d8: add.w r7, r4, #0x94) */
#define AUX_REV_STATE   0x58u        /* the kernel's state, 0xb0 bytes (0x080a9bde) */
#define AUX_REV_PRE     0x78u        /* the predelay buffer, 0x10000 frames a side (0x080a5f4c) */
#define TRK_EMPTY       0x14u        /* the tracker: the send was empty */
#define TRK_SEEN        0x15u
#define TRK_LEFT        0x18u        /* frames until it sleeps */
static const uint8_t aux_dly_lines[3] = { 0x30, 0x4c, 0x68 };   /* {buf, ..., +0x14 floats} each */
static const uint8_t aux_rev_lines[12] = { 0x20, 0x2c, 0x38, 0x44, 0x50, 0x64, 0x78, 0x84, 0x94, 0x9c, 0xa4, 0xac };
static const uint16_t aux_rev_sizes[12] = { 0x800, 0x800, 0x800, 0x800, 0x1000, 0x1000,
                                            0x8000, 0x8000, 0x8000, 0x8000, 0x8000, 0x8000 };

/* 2nd-order Butterworth high-pass at 48 kHz (RBJ): b0, b1, b2, a1, a2 for Aux
 * HP's corners (patch_micro.py AUX_HP_HZ; the test checks every response). a1-a3
 * had OFF/80/150/300, so their stored 1-3 now read as 60, 80 and 100 Hz. */
static const float aux_hp_coef[9][5] = {
    { 0.994461789f, -1.988923578f, 0.994461789f, -1.988892906f, 0.988954250f },   /* 60 Hz */
    { 0.992622543f, -1.985245086f, 0.992622543f, -1.985190658f, 0.985299513f },   /* 80 Hz */
    { 0.990786698f, -1.981573396f, 0.990786698f, -1.981488509f, 0.981658283f },   /* 100 Hz */
    { 0.986211925f, -1.972423849f, 0.986211925f, -1.972233729f, 0.972613969f },   /* 150 Hz */
    { 0.981658268f, -1.963316537f, 0.981658268f, -1.962980089f, 0.963652984f },   /* 200 Hz */
    { 0.972613898f, -1.945227797f, 0.972613898f, -1.944477658f, 0.945977936f },   /* 300 Hz */
    { 0.954774031f, -1.909548061f, 0.954774031f, -1.907501626f, 0.911594497f },   /* 500 Hz */
    { 0.928623778f, -1.857247556f, 0.928623778f, -1.852146485f, 0.862348626f },   /* 800 Hz */
    { 0.894858606f, -1.789717212f, 0.894858606f, -1.778631778f, 0.800802647f },   /* 1.2 kHz */
};

typedef struct {                     /* settings: posts write them, CC 63 too */
    uint32_t smagic;
    uint8_t s[AUX_NS];               /* the enums; s[AUX_S_RLVL] is unused */
    uint8_t ccgen[AUX_NS];           /* bumped by a CC that changed one */
    int32_t rlvl;                    /* Return Lvl, thousandths of a dB */
} aux_set_t;
typedef struct {                     /* the UI tick's */
    uint32_t umagic;
    uint8_t ccdone[AUX_NS];          /* the ccgen it last put in the global store */
    uint32_t omagic, olast;          /* the text CC 63's pop-up last showed */
} aux_ui_t;
typedef struct {                     /* one effect */
    uint8_t sleep;                   /* forced asleep (AUX, or its lines not yet clear) */
    uint8_t clear;                   /* its lines are zero */
    uint8_t pad[2];
    uint32_t pos;                    /* bytes of its lines zeroed so far */
    float tapg;                      /* the tap's gain at the end of the last block */
    float z[2][2];                   /* the tap's HP state, per side */
} aux_fx_t;
typedef struct {                     /* the audio task's */
    uint32_t amagic;
    aux_fx_t fx[2];                  /* 0 the delay (bus 0xd), 1 the reverb (bus 0xe) */
    uint32_t hp;                     /* the HP the states belong to */
    float retg;                      /* the return's gain at the end of the last block */
    int32_t rmdb;                    /* the Return Lvl rgain belongs to */
    float rgain;
    uint8_t *rec;                    /* the record node, as its render last found it */
    float injg[2];                   /* a5: Return into the delay / reverb, the gain at the last block's end */
    float idcx[2][2], idcy[2][2];    /* ... their DC blockers */
    float dcx[2], dcy[2];            /* its DC blocker */
    aux_desc_t tmp;                  /* a tap, as a bus the stock add takes */
    uint32_t blocks, sleeps, clears; /* for the tests: blocks seen, asleep, lines done */
    uint32_t rhp;                    /* a7: the Return HP the states below belong to */
    float rz[3][2][2];               /* ... its state: the mix, into the delay, into the reverb; per side */
} aux_au_t;
_Static_assert(AUX_SET_OFF + sizeof(aux_set_t) <= AUX_UI_OFF, "aux settings");
_Static_assert(AUX_UI_OFF + sizeof(aux_ui_t) <= AUX_AU_OFF, "aux UI area");
_Static_assert(AUX_AU_OFF + sizeof(aux_au_t) <= AUX_BUF_OFF, "aux audio area");
_Static_assert(AUX_BUF_OFF + 2 * 256 * 4 <= AUX_DUB_OFF, "aux buffers");
typedef struct {                     /* Dub Throw: written only on the audio task */
    uint32_t dmagic;
    uint8_t row[16];                 /* per pad position: the Dub Throw row */
    uint8_t cc[16];                  /* ... CC 103 (bit 0) and 106 (bit 1) held */
    float g[2][16][2];               /* per class, position and send: the gain the last block ended on; < 0 none yet */
} aux_dub_t;
_Static_assert(AUX_DUB_OFF + sizeof(aux_dub_t) <= AUX_RAM_BYTES, "aux Dub Throw area");

#define AUX_PTR(off) ((void *)(uintptr_t)(AUX_RAM + (off)))
#define U8(p, o)  (*(volatile uint8_t *)((uint8_t *)(p) + (o)))
#define U16(p, o) (*(volatile uint16_t *)((uint8_t *)(p) + (o)))
#define U32(p, o) (*(volatile uint32_t *)((uint8_t *)(p) + (o)))

extern void aux_x_add(aux_desc_t *dst, const aux_desc_t *src);   /* 0x080a3328: dst += src */
extern void aux_x_addg(aux_desc_t *dst, const aux_desc_t *src, float g);   /* 0x080a33ec: dst += g src */
extern void aux_x_stale(aux_desc_t *d);                          /* 0x080a31d4: frames 0, stale */
extern uint32_t aux_x_dly(void *node, void *msg);   /* 0x080a7e79: the delay's render slot, as stock */
extern uint32_t aux_x_rev(void *node, void *msg);   /* 0x080a9691: the reverb's */
extern uint32_t aux_x_rec(void *node, void *msg);   /* 0x080a4d1d: the record node's */

static aux_set_t *aux_set(void)
{
    aux_set_t *t = AUX_PTR(AUX_SET_OFF);
    if (t->smagic != AUX_SMAGIC) {   /* power-on: D2 holds noise */
        for (int i = 0; i < AUX_NS; i++) { t->s[i] = aux_defaults[i]; t->ccgen[i] = 0; }
        t->rlvl = 0;
        t->smagic = AUX_SMAGIC;
    }
    return t;
}

/* a setting, as the audio task reads it: the default until a post or CC has
 * made the area ours, and never out of range */
static uint32_t aux_get(uint32_t i)
{
    const aux_set_t *t = AUX_PTR(AUX_SET_OFF);
    uint32_t v = t->smagic == AUX_SMAGIC ? t->s[i] : aux_defaults[i];
    return v < aux_counts[i] ? v : aux_defaults[i];
}

static int32_t aux_rlvl(void)
{
    const aux_set_t *t = AUX_PTR(AUX_SET_OFF);
    int32_t v = t->smagic == AUX_SMAGIC ? t->rlvl : 0;
    return v < AUX_RLVL_MIN ? AUX_RLVL_MIN : v > AUX_RLVL_MAX ? AUX_RLVL_MAX : v;
}

static aux_au_t *aux_au(void)
{
    aux_au_t *a = AUX_PTR(AUX_AU_OFF);
    if (a->amagic != AUX_AMAGIC) {
        uint8_t *p = (uint8_t *)a;
        for (uint32_t i = 0; i < sizeof(*a); i++) p[i] = 0;
        a->amagic = AUX_AMAGIC;
    }
    return a;
}

/* ---- the Settings rows: boot's apply-all and a live edit (hook X) -------- */
void aux_post(uint32_t id, uint32_t value)
{
    aux_set_t *t = aux_set();
    if (id == aux_ids[AUX_S_RLVL]) {
        int32_t v = (int32_t)value;
        t->rlvl = v < AUX_RLVL_MIN ? AUX_RLVL_MIN : v > AUX_RLVL_MAX ? AUX_RLVL_MAX : v;
        return;
    }
    for (int i = 0; i < AUX_NS; i++)
        if (aux_ids[i] == id) { t->s[i] = (uint8_t)(value < aux_counts[i] ? value : aux_defaults[i]); return; }
}

/* ---- CC 63: channel 1, from chr_cc (the MIDI dispatch, audio task). It
 * sweeps Return Lvl only, 0.5 dB a step with 0 dB at 115; Return, the row,
 * says whether there is a return at all, so a fader at 0 is quiet, not off. */
static int32_t aux_cc_mdb(uint32_t v) { return ((int32_t)v - (int32_t)AUX_CC_UNITY) * 500; }

static void aux_cc(uint32_t val14)
{
    aux_set_t *t = aux_set();
    uint32_t v = val14 >> 7;
    if (v > 127) v = 127;
    int32_t m = aux_cc_mdb(v);
    if (t->rlvl != m) { t->rlvl = m; t->ccgen[AUX_S_RLVL]++; }
    chr_osd_note(OSD_AUX, 0, AUX_CC_RET, v, 0);
}

/* ---- the UI tick: CC 63's change into the global store, so the rows show
 * it, and settings.tml 2 s later with the rest (cmp_save_soon) ----------- */
static void aux_ui_tick(uint32_t now)
{
    const aux_set_t *t = AUX_PTR(AUX_SET_OFF);
    aux_ui_t *u = AUX_PTR(AUX_UI_OFF);
    if (t->smagic != AUX_SMAGIC) return;      /* no post or CC yet */
    if (u->umagic != AUX_UMAGIC) {
        for (int i = 0; i < AUX_NS; i++) u->ccdone[i] = t->ccgen[i];
        u->umagic = AUX_UMAGIC;
    }
    for (int i = 0; i < AUX_NS; i++) {
        uint8_t g = t->ccgen[i];
        if (g == u->ccdone[i]) continue;
        cmp_x_setparam((void *)(uintptr_t)CMP_GLOBAL_STORE, aux_ids[i],
                       i == AUX_S_RLVL ? t->rlvl : (int32_t)t->s[i]);
        u->ccdone[i] = g;
        cmp_save_soon(now);
    }
}

/* ---- the pop-ups: CC 63's level, and a throw (a5) --------------------- */
static void aux_osd_compose(uint32_t ctrl, uint32_t value, osd_text_t *tx)
{
    tx->n = 2;
    if (ctrl == AUX_CC_DTHROW || ctrl == AUX_CC_RTHROW) {
        osd_put(tx->t[0], 0, "DUB THROW");
        osd_put(tx->t[1], 0, ctrl == AUX_CC_DTHROW ? "Delay" : "Reverb");
        return;
    }
    osd_put(tx->t[0], 0, "RETURN");
    osd_db(tx->t[1], osd_put(tx->t[1], 0, "Level  "), (float)aux_cc_mdb(value & 127u) * 0.001f);
}

/* CC 84/85's pop-up names the effect's mode (a4); the pad screen's own knob
 * labels stay stock's. "Del", never "Dly" (the owner). */
static const char *aux_send_name(uint32_t cc)
{
    static const char *const names[2][3] = {
        { "Delay Send", "Del Aux Send", "Del+Aux Send" },
        { "Reverb Send", "Rev Aux Send", "Rev+Aux Send" },
    };
    uint32_t k = cc == 85u;
    return names[k][aux_get(k ? AUX_S_RMODE : AUX_S_DMODE)];
}

/* osd_offer's rule (a), with its own memory here, not one of chr_osd2's keys:
 * CC 63 shows only when the text differs from what it last showed. A throw is
 * noted only on a press (a held button's repeats are not), so each one shows. */
static int aux_osd_offer(uint32_t ctrl, osd_text_t *tx, osd_text_t *out, int show)
{
    aux_ui_t *u = AUX_PTR(AUX_UI_OFF);
    if (tx->n != 2 || !show) return 0;
    if (ctrl != AUX_CC_RET) { *out = *tx; return 1; }
    uint32_t h = osd_hash2(tx->t[0], tx->t[1]);
    if (u->omagic == AUX_OMAGIC && u->olast == h) return 0;
    u->olast = h;
    u->omagic = AUX_OMAGIC;
    *out = *tx;
    return 1;
}

/* ---- the audio side ------------------------------------------------------ */
static int aux_frames(const aux_desc_t *b)
{
    int n = (int)(b->frames ? b->frames : b->cap);
    return n > 0 && n <= 256 ? n : 0;
}

/* one sample through the biquad (transposed direct form II); z1, z2 its state */
static inline float aux_bq(const float *c, float *z1, float *z2, float in)
{
    float y = c[0] * in + *z1;
    *z1 = c[1] * in - c[3] * y + *z2;
    *z2 = c[2] * in - c[4] * y;
    return y;
}

static void aux_bq_rest(float *z)    /* no denormals once the input stops */
{
    if (z[0] < 1e-20f && z[0] > -1e-20f) z[0] = 0.0f;
    if (z[1] < 1e-20f && z[1] > -1e-20f) z[1] = 0.0f;
}

static void aux_hp(const float *c, float *z, float *x, int n)
{
    float z1 = z[0], z2 = z[1];
    for (int i = 0; i < n; i++) x[i] = aux_bq(c, &z1, &z2, x[i]);
    z[0] = z1; z[1] = z2;
    aux_bq_rest(z);
}

/* The send, folded and filtered, added into its output at a gain going from
 * g0 to g1 across the block. */
static void aux_tap(aux_au_t *a, uint32_t k, uint8_t *msg, const aux_desc_t *send, int n, float g0, float g1)
{
    if (send->stale || !send->L) return;
    uint32_t o = aux_get(k ? AUX_S_ROUT : AUX_S_DOUT), hp = aux_get(AUX_S_HP);
    uint32_t bus = aux_out_bus[o];
    int mono = bus >= 4u;
    aux_fx_t *f = &a->fx[k];
    float *bL = AUX_PTR(AUX_BUF_OFF), *bR = bL + 256;
    const float *L = send->L, *R = send->stereo && send->R ? send->R : send->L;
    float g = g0, dg = (g1 - g0) / (float)n;
    if (mono) {
        uint32_t m = aux_get(AUX_S_MONO);
        for (int i = 0; i < n; i++) {
            g += dg;
            float x = m == AUX_MONO_LEFT ? L[i] : m == AUX_MONO_RIGHT ? R[i] : 0.5f * (L[i] + R[i]);
            bL[i] = g * x;
        }
    } else {
        for (int i = 0; i < n; i++) { g += dg; bL[i] = g * L[i]; bR[i] = g * R[i]; }
    }
    if (hp) {
        aux_hp(aux_hp_coef[hp - 1u], f->z[0], bL, n);
        if (!mono) aux_hp(aux_hp_coef[hp - 1u], f->z[1], bR, n);
    }
    aux_desc_t *t = &a->tmp;
    t->cap = t->frames = (uint32_t)n;
    t->L = bL; t->R = bR;
    t->stale = 0; t->stereo = mono ? 0u : 1u;
    aux_x_add((aux_desc_t *)(msg + AUX_BUSES + 20u * bus), t);
}

/* Zero the next AUX_SLICE bytes of the effect's lines; 1 once all are. A
 * pointer outside SDRAM, or a size past 1 MB, is skipped: never a wild write. */
static int aux_clear(uint32_t rev, uint8_t *node, uint32_t *pos)
{
    uint32_t left = AUX_SLICE, at = 0, done = *pos;
    uint32_t nreg = rev ? 14u : 3u;
    for (uint32_t r = 0; r < nreg; r++) {
        uint32_t p, bytes;
        if (!rev) {
            p = U32(node, aux_dly_lines[r]);
            bytes = U32(node, aux_dly_lines[r] + 0x14u) * 4u;
        } else if (r < 12u) {
            uint32_t st = U32(node, AUX_REV_STATE);
            if (st < AUX_SDRAM_LO || st >= AUX_SDRAM_HI) continue;
            p = U32(st, aux_rev_lines[r]);
            bytes = aux_rev_sizes[r];
        } else {
            p = U32(node, AUX_REV_PRE + (r == 12u ? 8u : 12u));
            bytes = U32(node, AUX_REV_PRE) * 4u;
        }
        if (p < AUX_SDRAM_LO || (p & 3u) || bytes > AUX_REGION_MAX || p + bytes > AUX_SDRAM_HI) continue;
        if (done < at + bytes) {
            uint32_t from = done - at, m = bytes - from;
            if (m > left) m = left;
            volatile uint32_t *w = (volatile uint32_t *)(uintptr_t)(p + from);
            for (uint32_t i = 0; i < m / 4u; i++) w[i] = 0u;
            done += m;
            left -= m;
            if (!left) { *pos = done; return 0; }
        }
        at += bytes;
    }
    *pos = done;
    return 1;
}

static void aux_force_sleep(uint32_t rev, uint8_t *node)
{
    uint8_t *trk = node + (rev ? AUX_REV_TRK : AUX_DLY_TRK);
    U8(trk, TRK_EMPTY) = 1u;
    U8(trk, TRK_SEEN) = 1u;
    U32(trk, TRK_LEFT) = 0u;
    if (!rev) U8(node, AUX_DLY_FLUSHED) = 1u;
}

static uint32_t aux_orig(uint32_t rev, uint8_t *node, uint8_t *msg)
{
    return rev ? aux_x_rev(node, msg) : aux_x_dly(node, msg);
}

static float aux_ret_target(aux_au_t *a);

/* In 1/2 (only those the preset makes audio: both in stereo, one on both
 * sides), through a DC blocker (x, y: its state per side) and, with c, Return
 * HP (a7; z: its state per side), added into bus b at a gain going from g0 to
 * g1 across the block; an empty bus starts from silence. The mix (aux_ret) and
 * the effects (aux_inject) each keep their own blocker and HP state, so each
 * ramps and rests on its own and all hear the same filtered return. */
static void aux_in_add(aux_desc_t *b, const aux_desc_t *in, uint32_t au1, uint32_t au2,
                       float *x, float *y, float (*z)[2], const float *c, float g0, float g1)
{
    int n = aux_frames(in);
    if (in->stale || !n || !in->L || !b->L || !b->R) return;
    if (b->stale) {
        for (int i = 0; i < n; i++) b->L[i] = b->R[i] = 0.0f;
        b->frames = (uint32_t)n; b->stale = 0; b->stereo = 1;
    } else {
        if (aux_frames(b) != n) return;
        if (!b->stereo) { for (int i = 0; i < n; i++) b->R[i] = b->L[i]; b->stereo = 1; }
    }
    const float *i1 = in->L, *i2 = in->stereo && in->R ? in->R : in->L;
    const float *iL = au1 ? i1 : i2, *iR = au2 ? i2 : i1;
    float xl = x[0], yl = y[0], xr = x[1], yr = y[1];
    float g = g0, dg = (g1 - g0) / (float)n;
    if (c) {
        float zl1 = z[0][0], zl2 = z[0][1], zr1 = z[1][0], zr2 = z[1][1];
        for (int i = 0; i < n; i++) {
            g += dg;
            float l = iL[i], r = iR[i];
            yl = l - xl + AUX_DC_R * yl; xl = l;
            yr = r - xr + AUX_DC_R * yr; xr = r;
            b->L[i] += g * aux_bq(c, &zl1, &zl2, yl);
            b->R[i] += g * aux_bq(c, &zr1, &zr2, yr);
        }
        z[0][0] = zl1; z[0][1] = zl2; z[1][0] = zr1; z[1][1] = zr2;
        aux_bq_rest(z[0]);
        aux_bq_rest(z[1]);
    } else {
        for (int i = 0; i < n; i++) {
            g += dg;
            float l = iL[i], r = iR[i];
            yl = l - xl + AUX_DC_R * yl; xl = l;
            yr = r - xr + AUX_DC_R * yr; xr = r;
            b->L[i] += g * yl;
            b->R[i] += g * yr;
        }
    }
    if (yl < 1e-20f && yl > -1e-20f) yl = 0.0f;
    if (yr < 1e-20f && yr > -1e-20f) yr = 0.0f;
    x[0] = xl; y[0] = yl; x[1] = xr; y[1] = yr;
}

/* Return HP's coefficients, or 0 when OFF (a7); a new corner starts every
 * return path's state from rest, as Aux HP's do */
static const float *aux_rhp(aux_au_t *a)
{
    uint32_t hp = aux_get(AUX_S_RHP);
    if (a->rhp != hp) {
        a->rhp = hp;
        for (int p = 0; p < 3; p++) for (int s = 0; s < 2; s++) a->rz[p][s][0] = a->rz[p][s][1] = 0.0f;
    }
    return hp ? aux_hp_coef[hp - 1u] : 0;
}

/* a5: Return DELAY (REVERB) also feeds the internal delay (reverb), at Return
 * Lvl, into its send bus just before its render: after the tap has taken the
 * send, so the return never reaches an aux output (no loop through the
 * external effect), and only while the effect runs (EFFECT, BOTH; in AUX it
 * is asleep and DELAY/REVERB act as MIX). Stock's Rec Mon monitor goes to bus
 * 0, never into the effects, so the guard leaves this part alone: the effect
 * keeps hearing the return while stock monitors the mix. In 1/2 are converted
 * before the block routine (0x08093d44, then 0x08093e48), so they are this
 * block's here. */
static void aux_inject(aux_au_t *a, uint32_t k, uint8_t *msg, aux_desc_t *bus)
{
    const uint8_t *rec = a->rec;
    uint32_t au1 = rec ? rec[AUX_REC_AUDIO1] : 0u, au2 = rec ? rec[AUX_REC_AUDIO2] : 0u;
    float g1 = (au1 || au2) && aux_get(AUX_S_RET) == AUX_RET_DELAY + k ? aux_ret_target(a) : 0.0f;
    float g0 = a->injg[k];
    if (g0 == 0.0f && g1 == 0.0f) return;
    a->injg[k] = g1;
    if (!(au1 || au2)) return;
    aux_in_add(bus, (const aux_desc_t *)(msg + AUX_INPUTS), au1, au2, a->idcx[k], a->idcy[k],
               a->rz[1 + k], aux_rhp(a), g0, g1);
}

static uint32_t aux_fx(uint32_t rev, uint8_t *node, uint8_t *msg)
{
    uint32_t id = U16(node, 0x1e);
    if (id != AUX_BUS_DLY && id != AUX_BUS_REV) return aux_orig(rev, node, msg);
    uint32_t k = id - AUX_BUS_DLY;
    uint32_t mode = aux_get(k ? AUX_S_RMODE : AUX_S_DMODE);
    aux_au_t *a = aux_au();
    aux_fx_t *f = &a->fx[k];
    float g1 = mode == AUX_EFFECT ? 0.0f : 1.0f;
    aux_desc_t *bus = (aux_desc_t *)(msg + AUX_BUSES + 20u * id);
    if (mode == AUX_EFFECT && !f->sleep && f->tapg == 0.0f) {
        aux_inject(a, k, msg, bus);
        return aux_orig(rev, node, msg);
    }
    int n = aux_frames(bus);
    a->blocks++;
    if (n && (f->tapg != 0.0f || g1 != 0.0f)) {
        if (a->hp != aux_get(AUX_S_HP)) {     /* a new corner: start its states from rest */
            a->hp = aux_get(AUX_S_HP);
            for (int j = 0; j < 2; j++) for (int s = 0; s < 2; s++) a->fx[j].z[s][0] = a->fx[j].z[s][1] = 0.0f;
        }
        aux_tap(a, k, msg, bus, n, f->tapg, g1);
    }
    f->tapg = g1;
    if (!f->sleep) {
        if (mode != AUX_AUX) {                /* the tap has the send: now the return may join it */
            aux_inject(a, k, msg, bus);
            return aux_orig(rev, node, msg);
        }
        a->injg[k] = 0.0f;                    /* asleep from here: nothing to inject into */
        aux_x_stale(bus);                     /* the first block of AUX: the effect hears nothing ... */
        uint32_t r = aux_orig(rev, node, msg);
        if (!bus->stale && n && bus->L) {     /* ... and what it still returns fades out */
            float *L = bus->L, *R = bus->stereo ? bus->R : 0;
            float g = 1.0f, dg = 1.0f / (float)n;
            for (int i = 0; i < n; i++) { g -= dg; L[i] *= g; if (R) R[i] *= g; }
        }
        f->sleep = 1; f->clear = 0; f->pos = 0;
        return r;
    }
    if (!f->clear && aux_clear(rev, node, &f->pos)) { f->clear = 1; a->clears++; }
    if (mode != AUX_AUX && f->clear) {        /* clean: let it wake on the next send */
        f->sleep = 0;
        aux_inject(a, k, msg, bus);
        return aux_orig(rev, node, msg);
    }
    a->sleeps++;
    aux_force_sleep(rev, node);
    aux_x_stale(bus);
    uint32_t r = aux_orig(rev, node, msg);
    aux_x_stale(bus);                         /* nothing reaches the master */
    return r;
}

uint32_t aux_dly(void *node, void *msg) { return aux_fx(0, node, msg); }
uint32_t aux_rev(void *node, void *msg) { return aux_fx(1, node, msg); }

/* the record node renders before the master, every block: note where it is */
uint32_t aux_rec(void *node, void *msg)
{
    aux_au()->rec = node;
    return aux_x_rec(node, msg);
}

/* ---- the return: In 1/2 into the master bus, before the Character chain
 * (chr_block's head: after the dry copy and both returns, before Character,
 * EQ and compressor), through a DC blocker, at Return Lvl. Only the inputs
 * the preset makes audio: both, in stereo; one, on both sides; none, or no
 * record node seen yet, silence. The gain moves linearly across each block,
 * so CC 63's half-dB steps never zipper. -------------------------------- */
static float aux_ret_target(aux_au_t *a)
{
    if (!aux_get(AUX_S_RET)) return 0.0f;
    int32_t m = aux_rlvl();
    if (m != a->rmdb || a->rgain == 0.0f) {    /* 10^(dB/20), only when it moves */
        a->rmdb = m;
        a->rgain = chr_expf((float)m * 0.000115129255f);
    }
    return a->rgain;
}

/* Stock is monitoring In 1/2 into bus 0 itself (the record node, 0x080a5128-
 * 0x080a5192): Rec Mon On, or Auto while recording or with the recording
 * screen shown; not Resample; Mon Output Out1/2. Any other Mon Output bus
 * leaves the return alone. */
static int aux_rec_monitoring(const uint8_t *rec)
{
    uint32_t mon = rec[AUX_REC_MON];
    int on = mon == 0u || (mon == 1u && (rec[AUX_REC_ACTIVE] || rec[AUX_REC_NOW]));
    return on && rec[AUX_REC_INPUT] != 3u && *(const volatile uint32_t *)(rec + AUX_REC_MONBUS) == 0u;
}

static void aux_ret(void *h0)
{
    aux_desc_t *b0 = h0;
    aux_au_t *a = aux_au();
    const uint8_t *rec = a->rec;
    uint32_t au1 = rec ? rec[AUX_REC_AUDIO1] : 0u, au2 = rec ? rec[AUX_REC_AUDIO2] : 0u;
    /* stock monitoring them: stand aside, ramping out over this block, and back in
     * over the first block after, as any change of level */
    float g1 = (au1 || au2) && !aux_rec_monitoring(rec) ? aux_ret_target(a) : 0.0f;
    float g0 = a->retg;
    if (g0 == 0.0f && g1 == 0.0f) return;
    a->retg = g1;
    if (!(au1 || au2)) return;                /* not audio now: no ramp through CV */
    aux_in_add(b0, (const aux_desc_t *)((uint8_t *)h0 + (AUX_INPUTS - AUX_BUSES)), au1, au2,
               a->dcx, a->dcy, a->rz[0], aux_rhp(a), g0, g1);
}

/* ---- Dub Throw (a5) ------------------------------------------------------ */
static aux_dub_t *aux_dub(void)
{
    aux_dub_t *t = AUX_PTR(AUX_DUB_OFF);
    if (t->dmagic != AUX_DMAGIC) {            /* power-on: D2 holds noise */
        for (int i = 0; i < 16; i++) { t->row[i] = 0u; t->cc[i] = 0u; }
        for (int c = 0; c < 2; c++) for (int i = 0; i < 16; i++) t->g[c][i][0] = t->g[c][i][1] = -1.0f;
        t->dmagic = AUX_DMAGIC;
    }
    return t;
}

/* a voice's Dub Throw row, from its own setter (hook X's xdubA, xdubB): a load
 * posts every pad's, and a preset saved before a5 has none, so it stays Off */
void aux_dub_post(uint8_t *voice, uint32_t value)
{
    int idx = clip_idx(voice);
    if (idx < 0) return;
    aux_dub()->row[idx] = (uint8_t)(value < AUX_DUB_N ? value : 0u);
}

/* CC 103 / 106, from clip_cc: every pad's hidden Clip voice takes its
 * channel's CCs, whatever the pad's type. A press pops up; a held button's
 * repeats, and a release, do not. */
static void aux_throw_cc(uint8_t *pad, uint32_t cc, uint32_t v)
{
    int idx = clip_idx(pad);
    if (idx < 0) return;
    aux_dub_t *t = aux_dub();
    uint8_t bit = cc == AUX_CC_DTHROW ? 1u : 2u;
    if (v >= 64u) {
        if (!(t->cc[idx] & bit)) chr_osd_note(OSD_AUX, 0, cc, 0, 0);
        t->cc[idx] |= bit;
    } else {
        t->cc[idx] &= (uint8_t)~bit;
    }
}

/* input (1-8) is high: the Gate hook's own bits, as SC In reads them */
static int aux_gate(uint32_t input)
{
    uint32_t rec = sc_inputs[input - 1u];
    return (sc_ram()->gates >> (rec > 4u ? rec - 1u : rec)) & 1u;
}

static float aux_send_target(const uint8_t *voice, uint32_t which, int idx)
{
    float knob = *(const volatile float *)(voice + ((which & AUX_W_B) ? AUX_SEND_B : AUX_SEND_A)
                                           + 4u * (which & AUX_W_REV));
    if (idx < 0) return knob;
    aux_dub_t *t = aux_dub();
    uint32_t rev = which & AUX_W_REV, r = t->row[idx];
    uint32_t thrown = (t->cc[idx] >> rev) & 1u;
    if (r) {                                  /* 1-8 Del, 9-16 Rev, 17-24 D+R (a7): input (r - 1) % 8 + 1 */
        uint32_t grp = (r - 1u) >> 3;
        if (grp == 2u || grp == rev) thrown |= (uint32_t)aux_gate(((r - 1u) & 7u) + 1u);
    }
    return thrown ? 1.0f : knob;
}

/* The voice's tests, "any send?" and "this send?" (hook X's xsdec): this
 * block's send, or, while a release still ramps down to 0, where it is. */
float aux_send_dec(uint8_t *voice, uint32_t which)
{
    int idx = clip_idx(voice);
    float t = aux_send_target(voice, which, idx);
    if (t != 0.0f || idx < 0) return t;
    float g = aux_dub()->g[(which >> 1) & 1u][idx][which & AUX_W_REV];
    return g > 0.0f ? g : 0.0f;
}

/* The voice's send add (xsadd), in stock's dst += g src's place: that very call
 * while the gain holds still, else a ramp across the block from the last
 * block's gain, so neither a throw nor its release clicks. */
void aux_send_add(aux_desc_t *dst, const aux_desc_t *src, uint8_t *voice, uint32_t which)
{
    int idx = clip_idx(voice);
    float g1 = aux_send_target(voice, which, idx), g0 = g1;
    if (idx >= 0) {
        float *gp = &aux_dub()->g[(which >> 1) & 1u][idx][which & AUX_W_REV];
        if (*gp >= 0.0f) g0 = *gp;
        *gp = g1;
    }
    if (g0 == g1) { aux_x_addg(dst, src, g1); return; }
    int n = aux_frames(src);
    if (src->stale || !n || !src->L || !dst->L) return;
    const float *sL = src->L, *sR = src->stereo && src->R ? src->R : src->L;
    if (dst->stale) {
        for (int i = 0; i < n; i++) { dst->L[i] = 0.0f; if (dst->R) dst->R[i] = 0.0f; }
        dst->frames = (uint32_t)n; dst->stale = 0; dst->stereo = src->stereo && dst->R ? 1u : 0u;
    } else {
        if (aux_frames(dst) != n) return;
        if (src->stereo && !dst->stereo && dst->R) {
            for (int i = 0; i < n; i++) dst->R[i] = dst->L[i];
            dst->stereo = 1;
        }
    }
    float g = g0, dg = (g1 - g0) / (float)n;
    for (int i = 0; i < n; i++) {
        g += dg;
        dst->L[i] += g * sL[i];
        if (dst->stereo) dst->R[i] += g * sR[i];
    }
}

#endif
