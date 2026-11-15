/*
 * cmp_engine.h -- the compressor Type switch's store, MIDI side, audio engine
 * and saved setting (2.3.10, -DCHR_CMP). Included by character.c after
 * chr_osd.h; the DSP is in cmp_types.h.
 *
 * Threads and who writes what:
 *   audio task, MIDI half (chr_cc, hook N's nmidi; the CC post at 0x08093f14
 *   runs on the audio task -- see Keep CCs in patch_micro.py)
 *                                  -- the store: CC 47 Type, CC 41-46 for the
 *                                     Airwindows types. Same thread as the
 *                                     engine, so the two never race.
 *   UI (hook Q's post wrapper)     -- the store's Type, from settings.tml at
 *                                     boot, ONLY until the first CC 47; after
 *                                     that CC 47 is the authority and the
 *                                     global store follows it. The audio task
 *                                     can preempt it: a torn store fails its
 *                                     sum and the engine keeps its snapshot;
 *                                     a CC 47 landing inside the boot post
 *                                     can lose to it (once, at boot).
 *   UI (cmp_ui_tick, every tick)   -- the UI area: puts a CC 47 change into the
 *                                     global store at once, and writes
 *                                     settings.tml 2 s after the last change,
 *                                     through the same call the Settings
 *                                     screen's own edits use.
 *   audio (cmp_block, hook N's ncomp) -- the engine and the type states.
 * Hook N's ncomp reads two words to decide whether to come here at all:
 * store->run (the store says an Airwindows type) and engine->busy (a type
 * other than Stock is running or a crossfade is under way). Neither set: it
 * branches straight to hook E, exactly as 2.3.9 does.
 */
#ifndef CMP_ENGINE_H
#define CMP_ENGINE_H

#include "cmp_types.h"

/* ---- where everything lives: the gap above Keep CCs' RAM (patch_micro.py
 * carries the same numbers and checks them against the uncached pool). ----- */
#define CMP_RAM_BASE   0x2406a700u
#define CMP_STORE_OFF  0x000u      /* MIDI-owned, 0x40 */
#define CMP_UI_OFF     0x040u      /* UI-owned, 0x40 */
#define CMP_ENGINE_OFF 0x080u      /* audio-owned, 0x100 */
#define CMP_L4_OFF     0x180u      /* Logical4 state, 0x140 */
#define CMP_VM_OFF     0x2c0u      /* VariMu state, 0x40 */
#define CMP_TH_OFF     0x300u      /* Thunder state, 0x40 */
#define CMP_DRY_OFF    0x400u      /* 2 x 256 floats: the bus before either type */
#define CMP_OUT_OFF    0xc00u      /* 2 x 256 floats: the outgoing type's output */
#define CMP_RAM_BYTES  0x1400u     /* to 0x2406bb00 */

#define CMP_MAGIC  0x434d5031u     /* "CMP1": the store */
#define CMP_RUN    0x434d5052u     /* "CMPR": store->run, an Airwindows type is wanted */
#define CMP_EMAGIC 0x434d5045u     /* "CMPE": the engine */
#define CMP_BUSY   0x434d5042u     /* "CMPB": engine->busy */
#define CMP_UMAGIC 0x434d5055u     /* "CMPU": the UI area */

#define CMP_CHANNEL      0u        /* MIDI channel 1, as the stock compressor's CCs */
#define CMP_CC_FIRST     41u
#define CMP_CC_TYPE      47u
#define CMP_CC_NEXT      48u       /* b4: any value > 0 steps to the next type */
#define CMP_AUTO_OFF     0u        /* CC 46, a press steps Off -> Normal -> Hot (b9), every type */
#define CMP_AUTO_NORMAL  1u
#define CMP_AUTO_HOT     2u
#define CMP_FADE_SAMPLES 1440.0f   /* 30 ms */
#define CMP_SMOOTH_TAU   0.040f    /* 40 ms one-pole on every Airwindows control */
#define CMP_SUB          32        /* control-rate piece, as the Character chain */
#define CMP_SAVE_CYCLES  800000000u   /* 2 s at ~400 MHz (1.7 s at 480) */

/* The saved setting: a hidden registered parameter in the global store, like
 * stock's uiflip, so settings.tml carries it. patch_micro.py: CMP_ID. */
#define CMP_PARAM_ID     0x16bu
#define CMP_GLOBAL_STORE 0x2401bc80u  /* SessionMgr 0x24013cb0 + 0x7fd0 */
#define CMP_SAVE_MGR     0x24022578u  /* handler B's r0 for the settings.tml write */
#define CMP_SETTINGS_TML 0x080c5e64u  /* "settings.tml" in the image */
#define CMP_DSP_VSLOT    0x54u        /* the stock DSP's msg override slot ... */
#define CMP_DSP_VDEFAULT 0x080a2065u  /* ... and what it holds when not overridden */

/* Logical4's Threshold is an input drive: -20 dB drives +20 dB in, and the
 * stages' make-down takes it back out. Driven hard, it inverts: the output
 * FALLS as the input rises. With the host build of the original on pink
 * noise at Ratio 4:1, a -18 dBFS RMS mix inverts below about -14 dB and a hot
 * -12 dBFS mix below about -8 dB; so CC 41 stops at -8 dB (CC 0) and keeps
 * the plugin's top, +20 dB (CC 127). COMPRESSOR-DESIGN.md has the tables. */
#define CMP_L4_T_MIN     (-8.0f)
#define CMP_L4_T_MAX     20.0f

typedef struct {
    uint32_t magic;
    uint32_t run;                /* CMP_RUN when type != Stock; written last */
    uint32_t type;               /* 0 Stock, 1 Logical4, 2 VariMu, 3 Thunder */
    uint32_t tgen;               /* +1 per CC 47 change: the UI tick saves it */
    uint8_t v[3][6];             /* 7-bit, [type - 1][cc - 41]; [0][5] Logical4 auto makeup */
    uint8_t cc_seen;             /* a CC 47/48 has arrived since boot */
    uint8_t amode;               /* auto makeup, every type: Off / Normal / Hot (b4) */
    uint32_t gen;                /* +1 per accepted CC */
    uint32_t sum;
} cmp_store_t;
_Static_assert(sizeof(cmp_store_t) <= 0x40, "store");

typedef struct {
    uint32_t magic;
    uint32_t tgen_done;          /* the store's tgen last put in the global store */
    uint32_t dirty;              /* the global store changed; settings.tml not yet written */
    uint32_t t_last;             /* when (DWT) */
    uint32_t saves;              /* settings.tml writes, for diagnosis */
} cmp_ui_t;
_Static_assert(sizeof(cmp_ui_t) <= 0x40, "ui");

typedef struct {
    uint32_t magic;
    uint32_t busy;               /* CMP_BUSY unless Stock is settled */
    uint32_t cur;                /* the type running (fading in while fade < 1) */
    uint32_t old;                /* the type fading out */
    float fade;                  /* cur's weight, 0..1 */
    uint32_t s_type;             /* the last good store snapshot */
    uint8_t s_v[3][6];
    uint8_t s_amode, s_pad;
    float ctl[3][6];             /* each Airwindows type's smoothed parameters, 0..1;
                                    VariMu/Thunder [5]: auto makeup, (dB + 24) / 48 */
    cmp_ramp_t lim[3];           /* each Airwindows type's safety limiter */
    uint32_t faults;             /* non-finite samples caught */
    uint32_t blocks;             /* blocks this engine processed */
} cmp_engine_t;
_Static_assert(sizeof(cmp_engine_t) <= 0x100, "engine");
_Static_assert(__builtin_offsetof(cmp_engine_t, busy) == 4, "hook N's ncomp reads busy at +4");
_Static_assert(__builtin_offsetof(cmp_engine_t, faults) == 0xd4, "tests read faults at +0xd4");
_Static_assert(__builtin_offsetof(cmp_store_t, run) == 4, "ncomp and gate Dg read run at +4");
#ifndef CHR_REF                       /* the double reference build is host-only */
_Static_assert(sizeof(logical4_t) <= 0x140, "Logical4 state");
_Static_assert(sizeof(varimu_t) <= 0x40, "VariMu state");
_Static_assert(sizeof(thunder_t) <= 0x40, "Thunder state");
#endif

#ifdef CHR_HOST
uint32_t cmp_host_ram[CMP_RAM_BYTES / 4];
#define CMP_PTR(off) ((void *)((uint8_t *)cmp_host_ram + (off)))
static void cmp_stock_e(void *obj, void *msg) { (void)obj; (void)msg; }
static void *cmp_x_desc(void *msg, uint32_t bus) { (void)bus; return msg; }
static int32_t cmp_x_frames(void *desc) { (void)desc; return 0; }
static void cmp_x_fetch(void *desc, float **L, float **R) { (void)desc; *L = 0; *R = 0; }
static void cmp_x_setparam(void *store, uint32_t id, int32_t v) { (void)store; (void)id; (void)v; }
static void cmp_x_save(void *mgr, const char *name, void *store) { (void)mgr; (void)name; (void)store; }
#else
#define CMP_PTR(off) ((void *)(uintptr_t)(CMP_RAM_BASE + (off)))
/* Linked by patch_micro.py (c_link externs): hook E, and stock functions
 * called exactly as their stock callers call them. */
extern void cmp_stock_e(void *obj, void *msg);                     /* hook E: stamp, stock DSP */
extern void *cmp_x_desc(void *msg, uint32_t bus);                  /* 0x080a82b8 */
extern int32_t cmp_x_frames(void *desc);                           /* 0x080a31e0 */
extern void cmp_x_fetch(void *desc, float **L, float **R);         /* 0x080a3288 */
extern void cmp_x_setparam(void *store, uint32_t id, int32_t v);   /* 0x080894fa SetParam */
extern void cmp_x_save(void *mgr, const char *name, void *store);  /* 0x0808c154 */
#endif

static cmp_store_t *cmp_store(void) { return CMP_PTR(CMP_STORE_OFF); }

/* ---- the store ------------------------------------------------------------- */
static const uint8_t cmp_defaults[3][6] = {
    { 36, 25, 24, 127, 64, 0 },  /* Logical4: +0.0 dB threshold (the plugin's), 1.6:1, 4.6 ms, 100% wet, 0 dB */
    { 64, 0, 64, 127, 127, 0 },  /* VariMu: intensity 50%, speed 50%, 100% wet, output 100% */
    { 64, 0, 0, 0, 127, 0 },     /* Thunder: 50%, output 100% */
};

static uint32_t cmp_sum(const cmp_store_t *s)
{
    const uint8_t *b = &s->v[0][0];
    uint32_t x = s->magic ^ s->run ^ s->type ^ s->tgen ^ s->gen;
    for (int i = 0; i < 20; i += 4) x ^= w_of(b + i);    /* v[18], cc_seen, amode */
    return x;
}

static int cmp_store_ok(const cmp_store_t *s)
{
    if (s->magic != CMP_MAGIC || s->sum != cmp_sum(s) || s->type >= CMP_NTYPES) return 0;
    if (s->amode > CMP_AUTO_HOT) return 0;
    if (s->run != (s->type ? CMP_RUN : 0u)) return 0;
    for (int t = 0; t < 3; t++)
        for (int c = 0; c < 6; c++)
            if (s->v[t][c] > 127) return 0;
    return 1;
}

static void cmp_store_seed(cmp_store_t *s)
{
    s->run = 0;                       /* first: ncomp must not see a stale RUN */
    s->type = CMP_STOCK;
    for (int t = 0; t < 3; t++)
        for (int c = 0; c < 6; c++) s->v[t][c] = cmp_defaults[t][c];
    s->cc_seen = 0; s->amode = CMP_AUTO_OFF;
    s->tgen = 0; s->gen = 0;
    s->magic = CMP_MAGIC;
    s->sum = cmp_sum(s);
}

/* Change the type, keeping run consistent with it. run goes to 0 BEFORE a
 * change to Stock and to RUN only AFTER the type is written, so ncomp never
 * reads RUN with a Stock type (it would only cost a pass through C anyway). */
static void cmp_set_type(cmp_store_t *s, uint32_t t)
{
    if (!t) s->run = 0;
    s->type = t;
    if (t) s->run = CMP_RUN;
}

/* CC 46's three bands, the same in hook Da (Stock): 0-42 Off, 43-85 Normal,
 * 86-127 Hot. A 2.3.9-style on/off button sends 127: Hot. */
static uint32_t cmp_amode_of(uint32_t v7) { return v7 < 43u ? CMP_AUTO_OFF : v7 < 86u ? CMP_AUTO_NORMAL : CMP_AUTO_HOT; }

/* CC 46's mode as hook Da (Stock) reads it: one of its three bands */
static uint32_t cmp_amode_band(uint32_t m) { return m == CMP_AUTO_HOT ? 127u : m == CMP_AUTO_NORMAL ? 64u : 0u; }

/* ---- the MIDI side: channel 1, CC 40-48, every one also an OSD request ----
 * Returns the value the hooks behind hook N see: val14, except CC 46, whose
 * press (b9) steps the shared auto makeup Off -> Normal -> Hot and whose
 * press and release both hand hook Da the current mode as a band, so the
 * Stock type and the Airwindows types can never disagree. */
static uint32_t cmp_cc(uint32_t cc, uint32_t val14)
{
    uint32_t v = val14 >> 7;
    if (v > 127) v = 127;
    if (cc == CMP_CC_NEXT && v == 0) return val14;   /* a push's release */
    cmp_store_t *s = cmp_store();
    if (!cmp_store_ok(s)) cmp_store_seed(s);
    if (cc == CMP_CC_TYPE || cc == CMP_CC_NEXT) {
        uint32_t t = cc == CMP_CC_TYPE ? v >> 5 : (s->type + 1u) & 3u;
        s->cc_seen = 1;
        if (t != s->type) { cmp_set_type(s, t); s->tgen++; }
        s->gen++;
        s->sum = cmp_sum(s);
    } else if (cc == 46) {                        /* shared by every type; Da sets Stock's */
        uint32_t band = cmp_amode_band(s->amode);
        if (v == 0) return band << 7;             /* a release: Da keeps the mode */
        s->amode = (uint8_t)((s->amode + 1u) % 3u);
        s->gen++;
        s->sum = cmp_sum(s);
        v = cmp_amode_band(s->amode);
        val14 = v << 7;
    } else if (cc >= CMP_CC_FIRST && cc < 46 && s->type != CMP_STOCK) {
        s->v[s->type - 1][cc - CMP_CC_FIRST] = (uint8_t)v;
        s->gen++;
        s->sum = cmp_sum(s);
    }
    chr_osd_note(OSD_COMP, 0, cc - 40u, v, s->type);
    return val14;
}

/* ---- the saved setting --------------------------------------------------------
 * Hook Q's wrapper on the Settings posts (UI thread) calls this for our id
 * instead of posting it to the engine. Out of range: Stock, in the global
 * store too, as Keep CCs does. */
void cmp_post(int32_t v)
{
    if (v < 0 || v >= (int32_t)CMP_NTYPES) {
        cmp_x_setparam((void *)(uintptr_t)CMP_GLOBAL_STORE, CMP_PARAM_ID, 0);
        v = 0;
    }
    cmp_store_t *s = cmp_store();
    if (!cmp_store_ok(s)) cmp_store_seed(s);
    if (s->cc_seen || s->type == (uint32_t)v) return;   /* CC 47 is the authority now */
    cmp_set_type(s, (uint32_t)v);
    s->gen++;
    s->sum = cmp_sum(s);
}

/* UI side, every tick (from chr_osd_tick). */
static void cmp_ui_tick(uint32_t now)
{
    cmp_ui_t *u = CMP_PTR(CMP_UI_OFF);
    cmp_store_t *s = cmp_store();
    int ok = cmp_store_ok(s);
    if (u->magic != CMP_UMAGIC) {
        u->tgen_done = ok ? s->tgen : 0u;
        u->dirty = 0; u->t_last = now; u->saves = 0;
        u->magic = CMP_UMAGIC;
    }
    if (ok && s->tgen != u->tgen_done) {
        uint32_t t = s->type, g = s->tgen;
        if (s->sum == cmp_sum(s)) {     /* still the snapshot just taken */
            cmp_x_setparam((void *)(uintptr_t)CMP_GLOBAL_STORE, CMP_PARAM_ID, (int32_t)t);
            u->tgen_done = g;
            u->dirty = 1;
            u->t_last = now;
        }
    }
    if (u->dirty && now - u->t_last > CMP_SAVE_CYCLES) {
        u->dirty = 0;
        u->saves++;
        cmp_x_save((void *)(uintptr_t)CMP_SAVE_MGR, (const char *)(uintptr_t)CMP_SETTINGS_TML,
                   (void *)(uintptr_t)CMP_GLOBAL_STORE);
    }
}

/* b24: another saved setting changed (the Character chain's, chr_save.h):
 * settings.tml written with the rest, 2 s after the last change */
static void cmp_save_soon(uint32_t now)
{
    cmp_ui_t *u = CMP_PTR(CMP_UI_OFF);
    if (u->magic != CMP_UMAGIC) return;
    u->dirty = 1;
    u->t_last = now;
}

/* ---- the audio side ------------------------------------------------------------ */
static void cmp_engine_reset(cmp_engine_t *e)
{
    memset(e, 0, sizeof(*e));
    e->cur = e->old = CMP_STOCK;
    e->fade = 1.0f;
    e->s_type = CMP_STOCK;
    for (int t = 0; t < 3; t++) {
        for (int c = 0; c < 6; c++) e->s_v[t][c] = cmp_defaults[t][c];
        ramp_reset(&e->lim[t], CMP_LIM_ATTACK, CMP_LIM_RELEASE);
    }
    e->magic = CMP_EMAGIC;
}

static int cmp_engine_ok(const cmp_engine_t *e)
{
    if (e->magic != CMP_EMAGIC || e->cur >= CMP_NTYPES || e->old >= CMP_NTYPES || e->s_type >= CMP_NTYPES)
        return 0;
    if (!(e->fade >= 0.0f && e->fade <= 1.0f)) return 0;
    for (int t = 0; t < 3; t++) {
        for (int c = 0; c < 6; c++)
            if (!(e->ctl[t][c] >= 0.0f && e->ctl[t][c] <= 1.0f)) return 0;
        if (e->lim[t].state > 2 || !(e->lim[t].gain >= 0.0f && e->lim[t].gain <= 1.0f)) return 0;
    }
    return 1;
}

static float cmp_x7(uint32_t v) { return (float)v * (1.0f / 127.0f); }

/* A Logical4 Makeup in dB from CC 45: CC 64 is exactly 0 dB, CC 1 and 127
 * are -20 and +20 dB (the plugin's own range). */
static float cmp_l4_makeup_db(uint32_t v)
{
    return chr_clampf(((float)v - 64.0f) * (20.0f / 63.0f), -20.0f, 20.0f);
}

/* Auto makeup, every Airwindows type: the loudness each original takes off
 * (or adds to) a typical mix, given back -- measured, not modelled
 * (cmp_auto.h, K-weighted). Logical4's depends on Speed because its third
 * stage's gain does. */
#include "cmp_auto.h"

static float cmp_db2lin(float db) { return chr_expf(db * 0.11512925464970229f); }   /* ln 10 / 20 */

static float cmp_l4_lerp(const float *row, float f)   /* row[0..8], f in [0, 8] */
{
    int i = (int)f;
    if (i > 7) i = 7;
    return row[i] + (f - (float)i) * (row[i + 1] - row[i]);
}

static float cmp_l4_auto(float A, float B, float C, uint32_t mode)
{
    const float (*tab)[8][9] = mode == CMP_AUTO_HOT ? cmp_l4_auto_hot_db : cmp_l4_auto_db;
    float T = A * 40.0f - 20.0f;
    float ft = chr_clampf((T - CMP_L4_T_MIN) * (1.0f / 4.0f), 0.0f, 7.0f);
    float fb = chr_clampf(B * 8.0f, 0.0f, 8.0f);
    float fc = chr_clampf(C * 4.0f, 0.0f, 4.0f);
    int it = (int)ft, ic = (int)fc;
    if (it > 6) it = 6;
    if (ic > 3) ic = 3;
    float at = ft - (float)it, ac = fc - (float)ic;
    float v[2];
    for (int k = 0; k < 2; k++) {
        float a = cmp_l4_lerp(tab[ic + k][it], fb);
        float b = cmp_l4_lerp(tab[ic + k][it + 1], fb);
        v[k] = a + at * (b - a);
    }
    return v[0] + ac * (v[1] - v[0]);
}

static float cmp_vm_auto(float A, float B, uint32_t mode)   /* Intensity, Speed */
{
    const float (*tab)[9] = mode == CMP_AUTO_HOT ? cmp_vm_auto_hot_db : cmp_vm_auto_db;
    float fb = chr_clampf(B * 4.0f, 0.0f, 4.0f);
    int ib = (int)fb;
    if (ib > 3) ib = 3;
    float fa = chr_clampf(A * 8.0f, 0.0f, 8.0f);
    float a = cmp_l4_lerp(tab[ib], fa), b = cmp_l4_lerp(tab[ib + 1], fa);
    return a + (fb - (float)ib) * (b - a);
}

static float cmp_th_auto(float A, uint32_t mode)      /* Thunder */
{
    return cmp_l4_lerp(mode == CMP_AUTO_HOT ? cmp_th_auto_hot_db : cmp_th_auto_db,
                       chr_clampf(A * 8.0f, 0.0f, 8.0f));
}

/* Auto makeup in dB, as a 0..1 control (so it glides with the others). */
static float cmp_auto_ctl(float db) { return chr_clampf((db + 24.0f) * (1.0f / 48.0f), 0.0f, 1.0f); }
static float cmp_auto_db(float c) { return c * 48.0f - 24.0f; }

/* The plugin parameters each type's CCs ask for, 0..1; `mode` the shared
 * auto makeup (Off / Normal / Hot). */
static void cmp_targets(uint32_t t, const uint8_t *v, uint32_t mode, float *p)
{
    switch (t) {
    case CMP_LOGICAL4: {
        float T = CMP_L4_T_MIN + cmp_x7(v[0]) * (CMP_L4_T_MAX - CMP_L4_T_MIN);
        p[0] = (T + 20.0f) * (1.0f / 40.0f);       /* A Threshold */
        p[1] = cmp_x7(v[1]);                       /* B Ratio */
        p[2] = cmp_x7(v[2]);                       /* C Speed */
        p[4] = cmp_x7(v[3]);                       /* E Dry/Wet */
        float mk = mode ? cmp_l4_auto(p[0], p[1], p[2], mode) : cmp_l4_makeup_db(v[4]);
        p[3] = chr_clampf((mk + 20.0f) * (1.0f / 40.0f), 0.0f, 1.0f);   /* D Makeup */
        p[5] = 0.0f;
        break;
    }
    case CMP_VARIMU:
        p[0] = cmp_x7(v[0]);                       /* A Intensity (CC 41) */
        p[1] = cmp_x7(v[2]);                       /* B Speed (CC 43) */
        p[2] = cmp_x7(v[4]);                       /* C Output (CC 45) */
        p[3] = cmp_x7(v[3]);                       /* D Dry/Wet (CC 44) */
        p[4] = mode ? 1.0f : 0.0f;                 /* auto makeup on (CC 46), glides */
        p[5] = cmp_auto_ctl(mode ? cmp_vm_auto(p[0], p[1], mode) : 0.0f);
        break;
    default:                                       /* Thunder */
        p[0] = cmp_x7(v[0]);                       /* A Thunder (CC 41) */
        p[1] = cmp_x7(v[4]);                       /* B Output (CC 45) */
        p[2] = mode ? 1.0f : 0.0f;                 /* auto makeup on (CC 46), glides */
        p[3] = p[4] = 0.0f;
        p[5] = cmp_auto_ctl(mode ? cmp_th_auto(p[0], mode) : 0.0f);
        break;
    }
}

static void *cmp_state(uint32_t t)
{
    return CMP_PTR(t == CMP_LOGICAL4 ? CMP_L4_OFF : t == CMP_VARIMU ? CMP_VM_OFF : CMP_TH_OFF);
}

static void cmp_wake(cmp_engine_t *e, uint32_t t)
{
    void *st = cmp_state(t);
    switch (t) {
    case CMP_LOGICAL4: l4_init(st); break;
    case CMP_VARIMU: vm_init(st); break;
    default: th_init(st); break;
    }
    cmp_targets(t, e->s_v[t - 1], e->s_amode, e->ctl[t - 1]);   /* no glide in */
    ramp_reset(&e->lim[t - 1], CMP_LIM_ATTACK, CMP_LIM_RELEASE);
}

/* Non-finite samples become 0; returns 1 if there were any. */
static int cmp_guard(float *b, int n)
{
    int bad = 0;
    for (int i = 0; i < n; i++)
        if (!chr_finite(b[i])) { b[i] = 0.0f; bad = 1; }
    return bad;
}

#ifdef CHR_SC
static int sc_level(float *level);               /* sidechain.h */
#endif

static void cmp_run_aw(cmp_engine_t *e, uint32_t t, float *L, float *R, int n)
{
    float tgt[6];
    float *ctl = e->ctl[t - 1];
#ifdef CHR_SC
    float sc_lev;                                /* SC In: a gate's or a note's level, this block */
    const float *sc = sc_level(&sc_lev) ? &sc_lev : 0;
#endif
    cmp_targets(t, e->s_v[t - 1], e->s_amode, tgt);
    float k = 1.0f - chr_expf(-(float)CMP_SUB / (CHR_SR * CMP_SMOOTH_TAU));
    void *st = cmp_state(t);
    for (int off = 0; off < n; off += CMP_SUB) {
        int m = n - off < CMP_SUB ? n - off : CMP_SUB;
        for (int j = 0; j < 6; j++) ctl[j] += k * (tgt[j] - ctl[j]);
        switch (t) {
        case CMP_LOGICAL4: {
            l4_coef_t c;
            l4_coef(&c, ctl[0], ctl[1], ctl[2], ctl[3], ctl[4]);
            m_logical4(st, &c, L + off, R + off, m);
            break;
        }
        case CMP_VARIMU: {
            vm_coef_t c;
            vm_coef(&c, ctl[0], ctl[1], ctl[2], ctl[3]);
            if (ctl[4] > 0.0f) {                   /* auto makeup: Output's place, may boost */
                float man = c.output < 1.0f ? c.output : 1.0f;
                c.output = man + ctl[4] * (cmp_db2lin(cmp_auto_db(ctl[5])) - man);
                c.force = 1;
            }
            m_varimu(st, &c, L + off, R + off, m CMP_SC_ARG);
            break;
        }
        default: {
            th_coef_t c;
            float out = ctl[1];
            if (ctl[2] > 0.0f) out += ctl[2] * (cmp_db2lin(cmp_auto_db(ctl[5])) - out);
            th_coef(&c, ctl[0], out);
            m_thunder(st, &c, L + off, R + off, m CMP_SC_ARG);
            break;
        }
        }
    }
    int bad = cmp_guard(L, n);
    bad |= cmp_guard(R, n);
    if (t == CMP_LOGICAL4 && !l4_state_finite(st)) {   /* b5: see l4_state_finite */
        for (int i = 0; i < n; i++) { L[i] = 0.0f; R[i] = 0.0f; }
        bad = 1;
    }
    if (bad) { e->faults++; cmp_wake(e, t); }     /* restart the type from clean */
    cmp_limit(&e->lim[t - 1], L, R, n);
}

/* The stock compressor's two gain ramps (its compressor at +0x434, its limiter
 * at +0x454) back to idle at unity, so a Stock that fades back in starts
 * fresh rather than from whatever gain it held when it was left. */
static void cmp_stock_wake(uint8_t *obj)
{
    for (int k = 0; k < 2; k++) {
        uint8_t *r = obj + (k ? 0x454 : 0x434);
        *(volatile float *)(r + 0x00) = 1.0f;     /* gain */
        *(volatile int32_t *)(r + 0x0c) = 0;      /* samples left */
        *(volatile uint8_t *)(r + 0x1c) = 0;      /* state: idle */
    }
}

/* One type over the bus, in place. */
static void cmp_run(cmp_engine_t *e, uint32_t t, void *obj, void *msg, float *L, float *R, int n)
{
    if (t == CMP_STOCK) cmp_stock_e(obj, msg);     /* hook E: it fetches the same bus */
    else cmp_run_aw(e, t, L, R, n);
}

/* Hook N's ncomp, in place of `bl hook E` at 0x08097056, whenever the store
 * wants an Airwindows type or the engine is busy. r0 = the compressor object,
 * r1 = the audio-args object, as the stock DSP receives them. */
void cmp_block(uint8_t *obj, void *msg)
{
    cmp_engine_t *e = CMP_PTR(CMP_ENGINE_OFF);
    cmp_store_t *s = cmp_store();
    if (!cmp_engine_ok(e)) cmp_engine_reset(e);
    if (cmp_store_ok(s)) {                         /* else the last good snapshot */
        e->s_type = s->type;
        e->s_amode = s->amode;
        for (int t = 0; t < 3; t++)
            for (int c = 0; c < 6; c++) e->s_v[t][c] = s->v[t][c];
    }
    uint32_t want = e->s_type;

    if (e->fade >= 1.0f && want != e->cur) {       /* start a crossfade */
        e->old = e->cur;
        e->cur = want;
        e->fade = 0.0f;
        if (want == CMP_STOCK) cmp_stock_wake(obj);
        else cmp_wake(e, want);
    }
    e->busy = CMP_BUSY;
    if (e->fade >= 1.0f && e->cur == CMP_STOCK) {  /* settled on Stock: as 2.3.9 */
        cmp_stock_e(obj, msg);
        e->busy = 0;
        return;
    }

    /* Stock's own msg override (its vtable slot 0x54), as its prologue does. */
    void *const *vt = *(void *const *const *)obj;
    if ((uintptr_t)vt[CMP_DSP_VSLOT / 4] != CMP_DSP_VDEFAULT)
        msg = ((void *(*)(void *))vt[CMP_DSP_VSLOT / 4])(obj);
    void *desc = cmp_x_desc(msg, *(volatile uint16_t *)(obj + 0x1e));
    int n = cmp_x_frames(desc);
    float *L = 0, *R = 0;
    cmp_x_fetch(desc, &L, &R);                     /* clears a stale bus; mono -> both */
    if (n <= 0 || n > 256 || !L || !R) {
        if (e->fade >= 1.0f) e->busy = e->cur ? CMP_BUSY : 0u;
        return;
    }
    e->blocks++;
    if (e->fade >= 1.0f) {                         /* settled on an Airwindows type */
        cmp_run_aw(e, e->cur, L, R, n);
        return;
    }
    float *dL = CMP_PTR(CMP_DRY_OFF), *dR = dL + 256;
    float *oL = CMP_PTR(CMP_OUT_OFF), *oR = oL + 256;
    for (int i = 0; i < n; i++) { dL[i] = L[i]; dR[i] = R[i]; }
    cmp_run(e, e->old, obj, msg, L, R, n);
    for (int i = 0; i < n; i++) { oL[i] = L[i]; oR[i] = R[i]; L[i] = dL[i]; R[i] = dR[i]; }
    cmp_run(e, e->cur, obj, msg, L, R, n);
    float w0 = e->fade, w1 = chr_minf(1.0f, w0 + (float)n / CMP_FADE_SAMPLES);
    float w = w0, dw = (w1 - w0) / (float)n;
    for (int i = 0; i < n; i++) {
        w += dw;
        L[i] = oL[i] + w * (L[i] - oL[i]);
        R[i] = oR[i] + w * (R[i] - oR[i]);
    }
    e->fade = w1;
    if (w1 >= 1.0f && e->cur == CMP_STOCK) e->busy = 0;
}

/* ---- the pop-up ------------------------------------------------------------------
 * Stock's pop-ups are 2.3.9's, word for word (hook D's store). CC 47 names
 * the type; an Airwindows type's CC 41-46 show its own names and units, the
 * value as the CC set it, and "N/A" where the type has no such control. */
static const char *const cmp_type_names[CMP_NTYPES] = { "Stock", "Logical4", "VariMu", "Thunder" };
static const char *const cmp_ctl_names[3][6] = {
    { "Threshold", "Ratio", "Speed", "Dry/Wet", "Makeup", "Auto Makeup" },
    { "Intensity", "Ratio", "Speed", "Dry/Wet", "Output", "Auto Makeup" },
    { "Thunder", "Ratio", "Speed", "Dry/Wet", "Output", "Auto Makeup" },
};
static const uint8_t cmp_has[3] = { 0x3f, 0x3d, 0x31 };   /* bit k: CC 41 + k exists */
static const char *const cmp_amode_names[3] = { "Off", "Normal", "Hot" };

#ifdef CHR_SC
static void sc_osd_compose(uint32_t ctrl, uint32_t value, uint32_t type, osd_text_t *tx);   /* sidechain.h */
static void cv_osd_compose(uint32_t value, uint32_t n, osd_text_t *tx);                    /* sidechain.h */
#endif
static void cmp_osd_compose(uint32_t ctrl, uint32_t value, uint32_t type, osd_text_t *tx)
{
#ifdef CHR_SC
    if (ctrl == 9u) { sc_osd_compose(ctrl, value, type, tx); return; }   /* CC 49, SC In */
    if (ctrl == 10u) { cv_osd_compose(value, type, tx); return; }        /* the CV row's probe */
#endif
    if (ctrl == 7u || ctrl == 8u) {                     /* CC 47, CC 48 */
        osd_put(tx->t[0], 0, "COMPRESSOR");
        int i = osd_put(tx->t[1], 0, "Type  ");
        osd_name(tx->t[1], i, cmp_type_names[type & 3u], 8);
        tx->n = 2;
        return;
    }
    const cmp_store_t *st = cmp_store();
    uint32_t amode = cmp_store_ok(st) ? st->amode : CMP_AUTO_OFF;
    if (ctrl == 6u) {                                   /* CC 46: every type shares it */
        osd_put(tx->t[0], 0, "COMPRESSOR");
        int i = osd_put(tx->t[1], 0, "Auto Makeup  ");
        osd_name(tx->t[1], i, cmp_amode_names[cmp_amode_of(value)], 6);
        tx->n = 2;
        return;
    }
    if (ctrl == 5u && type == CMP_STOCK && amode) {     /* Stock's CC 45 while auto rules */
        osd_put(tx->t[0], 0, "COMPRESSOR");
        osd_put(tx->t[1], 0, "Makeup  Auto");
        tx->n = 2;
        return;
    }
    if (ctrl == 0u || type == CMP_STOCK || type >= CMP_NTYPES) {
        osd_compose_comp(ctrl, value, tx);              /* CC 40, and every Stock control */
        return;
    }
    uint32_t k = (ctrl - 1u) % 6u;                      /* CC 41 + k */
    char *d = tx->t[1];
    int i = osd_put(tx->t[0], 0, "COMPRESSOR  ");
    osd_put(tx->t[0], i, cmp_type_names[type]);
    tx->n = 2;
    i = osd_put(d, 0, cmp_ctl_names[type - 1][k]);
    i = osd_put(d, i, "  ");
    if (!(cmp_has[type - 1] & (1u << k))) { osd_put(d, i, "N/A"); return; }
    if (k == 4 && amode) { osd_put(d, i, "Auto"); return; }   /* Makeup / Output while auto rules */
    if (type != CMP_LOGICAL4 || k == 3) {              /* percent: VariMu, Thunder, Dry/Wet */
        osd_pct(d, i, osd_v7pct(value));
        return;
    }
    float x = cmp_x7(value);
    switch (k) {
    case 0: osd_db(d, i, CMP_L4_T_MIN + x * (CMP_L4_T_MAX - CMP_L4_T_MIN)); return;
    case 1: i = osd_fix(d, i, osd_round((x * x * 15.0f + 1.0f) * 10.0f), 1, 4, 0); osd_put(d, i, ":1"); return;
    case 2: i = osd_fix(d, i, osd_round((x * x * 99.0f + 1.0f) * 10.0f), 1, 5, 0); osd_put(d, i, " ms"); return;
    default: osd_db(d, i, cmp_l4_makeup_db(value)); return;   /* Makeup */
    }
}

#endif
