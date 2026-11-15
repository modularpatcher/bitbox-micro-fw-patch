/*
 * sidechain.h -- 2.3.11: SC In, the compressor's sidechain input (CHR_SC builds
 * only). Included by character.c after cmp_engine.h. SIDECHAIN-FEASIBILITY.md
 * has the trace.
 *
 * SC In (CC 49, channel 1, 10 bands): Self, Gate 1-8, MIDI. Self is the
 * compressor as it always was. Gate n ducks the mix while input n's gate is
 * high; MIDI while a note is held on channel 16, softer notes less. Threshold,
 * Ratio, Attack and Release shape the duck as they shape any compression.
 * Kept through power-off: it rides in the compressor's last saved row
 * (chr_save.h), so settings.tml carries it (owner, 2026-11-08).
 *
 * The level is all a gate or note has, so the key is that level, constant
 * through the block: 1 (0 dBFS) for a gate, velocity / 127 for a note.
 *   Stock:   hook S retargets the detector's one RMS call (0x080b9b68) to
 *            skey, which calls sc_key: -1 for Self (skey makes the stock
 *            call, bit for bit), else the level.
 *   VariMu, Thunder: cmp_engine.h asks sc_level and hands it to the type,
 *            which detects it in place of the mix (cmp_types.h).
 *   Logical4: its gain follows each sample's own polarity, so it keeps
 *            detecting itself; the pop-up says N/A.
 *
 * The Gate hook writes gates (each input high, as gbit numbers its router
 * record), mheld and mvel; the rest is ours. All of it runs on the audio task.
 */
#ifndef SIDECHAIN_H
#define SIDECHAIN_H

#define SC_RAM           0x240693c0u   /* patch_micro.py: SC_RAM */
#define SC_MAGIC         0x53434b32u   /* "SCK2": b10's layout; b8's "SCK1" reads as Self */
#define SC_CC_IN         49u
#define SC_OSD_IN        9u            /* cmp_osd_compose's ctrl for CC 49 */
enum { SC_SELF, SC_GATE1, SC_GATE8 = SC_GATE1 + 7, SC_MIDI, SC_NKEYS };

typedef struct {
    uint32_t magic;
    uint8_t in;                        /* +4: SC In */
    uint8_t spare;
    uint8_t gates;                     /* +6: the Gate hook: inputs high */
    uint8_t mheld;                     /* +7: the Gate hook: notes held on channel 16 */
    uint8_t mvel;                      /* +8: the Gate hook: the last note-on's velocity */
    uint8_t cvpick[9];                 /* +9: per router record, the CV row's last pick + 1; 0 none (b20) */
    uint16_t cvshow;                   /* +0x12: router records whose next pick pops up (set by gcap) */
    uint8_t rowstale[8];               /* +0x14: per pad position, CC 112 moved its Clip Tempo (b21) */
    uint8_t ccmoved;                   /* +0x1c: a bit per pad position: CC 112 or 113 moved its Clip Tempo since power-on (b23) */
    uint8_t spare2;
    uint16_t ccmeth;                   /* +0x1e: 2 bits per pad position: the method it moved to */
} sc_ram_t;
_Static_assert(__builtin_offsetof(sc_ram_t, gates) == 6, "patch_micro.py: SC_GATES");
_Static_assert(__builtin_offsetof(sc_ram_t, mheld) == 7, "patch_micro.py: SC_MHELD");
_Static_assert(__builtin_offsetof(sc_ram_t, mvel) == 8, "patch_micro.py: SC_MVEL");
_Static_assert(__builtin_offsetof(sc_ram_t, cvpick) == 9, "patch_micro.py: SC_CVPICK");
_Static_assert(__builtin_offsetof(sc_ram_t, cvshow) == 0x12, "patch_micro.py: SC_CVSHOW");
_Static_assert(__builtin_offsetof(sc_ram_t, rowstale) == 0x14, "patch_micro.py: SC_ROWSTALE");
_Static_assert(__builtin_offsetof(sc_ram_t, ccmoved) == 0x1c, "patch_micro.py: SC_CCMOVED");
_Static_assert(__builtin_offsetof(sc_ram_t, ccmeth) == 0x1e, "patch_micro.py: SC_CCMETH");
_Static_assert(sizeof(sc_ram_t) <= 0x20, "the sidechain has 0x20 bytes of patch RAM");

#ifdef CHR_HOST
static uint8_t sc_host_ram[0x20];
#define SC_PTR ((sc_ram_t *)sc_host_ram)
#else
#define SC_PTR ((sc_ram_t *)(uintptr_t)SC_RAM)
#endif

/* Gate n's router record, as GATE_IN_INPUTS in patch_micro.py (b16: the rows
 * the other way round, as the bench found) */
static const uint8_t sc_inputs[8] = { 0, 5, 1, 6, 2, 7, 3, 8 };

static sc_ram_t *sc_ram(void)
{
    sc_ram_t *s = SC_PTR;
    if (s->magic != SC_MAGIC || s->in >= SC_NKEYS) {
        s->in = SC_SELF; s->spare = 0;
        s->gates = 0; s->mheld = 0; s->mvel = 0;
        for (uint32_t i = 0; i < 9u; i++) s->cvpick[i] = 0;
        s->cvshow = 0;
        for (uint32_t i = 0; i < 8u; i++) s->rowstale[i] = 0;
        s->ccmoved = 0; s->spare2 = 0; s->ccmeth = 0;
        s->magic = SC_MAGIC;
    }
    return s;
}

/* chr_save.h: SC In out to settings.tml, and back in at boot's apply-all */
static uint8_t sc_saved_in(void)
{
    return sc_ram()->in;
}

static void sc_restore_in(uint32_t k)
{
    sc_ram()->in = (uint8_t)(k < SC_NKEYS ? k : SC_SELF);
}

/* ---- the MIDI side: CC 49 on channel 1, an OSD request ---- */
static uint32_t sc_cc(uint32_t cc, uint32_t val14)
{
    (void)cc;
    uint32_t v = val14 >> 7;
    if (v > 127) v = 127;
    sc_ram_t *s = sc_ram();
    s->in = (uint8_t)(v * SC_NKEYS / 128u);
    const cmp_store_t *cs = cmp_store();
    uint32_t type = cmp_store_ok(cs) ? cs->type : CMP_STOCK;
    chr_osd_note(OSD_COMP, 0, SC_OSD_IN, s->in, type);
    return val14;
}

/* the key's level this block, 0..1; 0 if SC In is Self (the type detects itself) */
static int sc_level(float *level)
{
    sc_ram_t *s = sc_ram();
    uint32_t k = s->in;
    if (k >= SC_GATE1 && k <= SC_GATE8) {
        uint32_t rec = sc_inputs[k - SC_GATE1];
        *level = (s->gates >> (rec > 4u ? rec - 1u : rec)) & 1u ? 1.0f : 0.0f;
        return 1;
    }
    if (k == SC_MIDI) {
        *level = !s->mheld ? 0.0f : s->mvel >= 127u ? 1.0f : (float)s->mvel * (1.0f / 127.0f);
        return 1;
    }
    return 0;
}

/* ---- hook S, the Stock type's detector, once a block ---- */
float sc_key(void *msg, int32_t n)
{
    (void)msg; (void)n;
    float level;
    return sc_level(&level) ? level : -1.0f;   /* -1: the stock call */
}

/* ---- the pad page's CV Action row: a voltage picks ----
 * One choice per semitone of 1 V/octave CV, low to high from 0 V (owner,
 * 2026-11-08): C is the first choice, C# the second, and so on, rounded to the
 * nearest semitone as stock rounds inputs 5-8. The value read is the one
 * stock's own 1V/Octave reads (the mod matrix passes it when a slot's
 * 1V/Octave is on, 0x08095258): a float per input, volts - 2, at the SESSION +
 * 0xd63c + 8 * router record, the session being the object MANAGER + 0xa204
 * points to (b18: b12-b17 read MANAGER + 0xd63c itself, which nothing writes).
 * Inputs 1-4 are 5 * the block's last sample - 2, every block (0x080b9518);
 * inputs 5-8 come from the ADC scan, already rounded to semitones (0x08046750,
 * stored at 0x080953d4). Neither looks at the input's mode.
 * Flip In n (b20, owner): input n turns the pad's Live Flip mode like CC 117's
 * knob, all the time, however the pad is played (clip_cv_flip, from clip_read
 * for each playing Clip pad). Hold FX In n: input n picks which Hold FX a gate
 * holds, at the gate (cv_pick); it beats the Hold FX row; 0 V is Off, which
 * holds nothing (b24, owner), C# Roll, up to G Scratch. Neither pops up in
 * play: only the first pick after the row is set (gcap marks cvshow). */
#define CV_SESSION       (0x24022978u + 0xa204u)   /* MANAGER's session pointer, as rec.h's REC_ENGINE */
#define CV_TONES         0xd63cu                   /* session + this + 8 * record */
#define CV_TOP           120u          /* 10 V: the highest pick */
#define CV_DEAD          0.25f         /* semitones past a boundary before a held pick moves on */
#define CV_ROWS          0x240693a8u   /* patch_micro.py GATE3_RAM: u8 per router record, the row (GC_CV) */
#define CV_GTIN          0x2406f080u   /* patch_micro.py GATE_IN_RAM: its magic vouches for the rows */
#define CV_GTIN_MAGIC    0x4e495447u   /* "GTIN" */
#define CV_FLIP1         9u            /* the row: 0 Off, 1-8 Hold FX In n, 9-16 Flip In n */
#define CV_OFF           0xffu         /* the pop-up's value: the pad's Clip Tempo is Stock */
#define CV_HOLDK         0x10u         /* the pop-up's type: | this for a Hold FX pick */
#define SC_OSD_CV        10u           /* cmp_osd_compose's ctrl for the CV row's pick */

/* Pad positions. clip_idx numbers a pad from its address (pad + 0x18, row << 8
 * | column) as column * 4 + row; the pad stores, the UI's messages and the
 * router's records go row first. b20-b21 read clip_idx as row * 4 + column,
 * which only pad 1 survives (bench, b21: CC 112 never reached the row, Flip In
 * read another pad's CV row). A position is row * 4 + column, rows 0-1. */
#define CLIP_POS_N       8u

/* clip_idx -> position, or -1 off the Micro's two rows */
static int clip_pos(int idx)
{
    if (idx < 0 || (idx & 3) > 1) return -1;
    return (idx & 3) * 4 + (idx >> 2);
}

/* position -> clip_idx */
static uint32_t clip_pos_idx(uint32_t p)
{
    return (p & 3u) * 4u + (p >> 2);
}

/* input n's CV in semitones above 0 V, unrounded; 0 below it, for a NaN or
 * with no session */
static float cv_semis(uint32_t n)
{
    uint32_t rec = sc_inputs[(n - 1u) & 7u];
    uint32_t ses = *(volatile const uint32_t *)(uintptr_t)CV_SESSION;
    if (!ses) return 0.0f;
    float t = *(volatile const float *)(uintptr_t)(ses + CV_TONES + 8u * rec);
    float semis = (t + 2.0f) * 12.0f;
    if (!(semis > 0.0f)) return 0.0f;  /* also a NaN */
    return semis >= (float)CV_TOP ? (float)CV_TOP : semis;
}

/* the nearest semitone */
static uint32_t cv_semitone(uint32_t n)
{
    return (uint32_t)(cv_semis(n) + 0.5f);
}

/* this pick pops up if the row was set since the last one; once */
static int cv_show(sc_ram_t *r, uint32_t rec)
{
    if (rec > 8u || !(r->cvshow & (1u << rec))) return 0;
    r->cvshow = (uint16_t)(r->cvshow & ~(1u << rec));
    return 1;
}

/* The Gate hook, at a gate's rise, for Hold FX In n on router record rec:
 * which of `count` choices input n's voltage picks, 0 .. count - 1, one per
 * semitone; the rest the last. */
uint32_t cv_pick(uint32_t n, uint32_t count, uint32_t rec)
{
    if (n < 1u || n > 8u || count == 0u) return 0u;
    uint32_t s = cv_semitone(n);
    uint32_t pick = s < count ? s : count - 1u;
    if (cv_show(sc_ram(), rec)) chr_osd_note(OSD_COMP, 0, SC_OSD_CV, pick, n | CV_HOLDK);
    return pick;
}

/* Flip In n: from clip_read, for each playing Clip pad, every block. The pick
 * holds until the voltage is CV_DEAD past its band, so a voltage on a boundary
 * does not flick between two modes. With the pad's Clip Tempo on Stock nothing
 * is set; the pop-up then says so. */
/* the input (1-8) whose Flip In turns pad idx's mode, else 0. CC 117 leaves
 * such a pad's mode alone (b24, owner): CV would set it back the next block. */
static uint32_t clip_cv_input(int idx)
{
    if (*(volatile const uint32_t *)(uintptr_t)CV_GTIN != CV_GTIN_MAGIC) return 0u;
    int p = clip_pos(idx);
    if (p < 0) return 0u;
    uint32_t v = *(volatile const uint8_t *)(uintptr_t)(CV_ROWS + (uint32_t)(p >> 2) * 5u + (uint32_t)(p & 3));
    return v >= CV_FLIP1 && v < CV_FLIP1 + 8u ? v - CV_FLIP1 + 1u : 0u;
}

/* b25 (owner): Gate Action, Hold FX and CV Action show on the two Clip pages
 * only, and any other pad plays on its gate whatever its Gate Action says. The
 * Gate hook cannot see a pad's mode; each pad's Clip voice can (clip_is_clip),
 * so it keeps bit p of the Gate Action block's notclip byte (patch_micro.py
 * GD_NOTCLIP) up to date whenever it hears anything: every setting above 0xd5,
 * which every preset load posts (grow), every CC (clip_jm_cc), every block it
 * plays (clip_read). gdoes reads it. Written only on a change: patch RAM is
 * uncached. */
#define CK_NOTCLIP       (0x2406c3c0u + 0x1du)     /* patch_micro.py: GATE2_RAM + GD_NOTCLIP */
void clip_kind(const uint8_t *pad)
{
    if (*(volatile const uint32_t *)(uintptr_t)CV_GTIN != CV_GTIN_MAGIC) return;
    int p = clip_pos(clip_idx(pad));
    if (p < 0) return;
    volatile uint8_t *b = (volatile uint8_t *)(uintptr_t)CK_NOTCLIP;
    uint32_t was = *b, now = clip_is_clip(pad) ? was & ~(1u << p) : was | 1u << p;
    if (now != was) *b = (uint8_t)now;
}

static void clip_cv_flip(clip_ram_t *c, const uint8_t *pad)
{
    if (!clip_is_clip(pad)) return;
    int idx = clip_idx(pad);
    uint32_t n = clip_cv_input(idx);
    if (!n) return;
    int p = clip_pos(idx);
    uint32_t rec = (uint32_t)(p >> 2) * 5u + (uint32_t)(p & 3);
    sc_ram_t *r = sc_ram();
    float f = cv_semis(n);
    uint32_t last = r->cvpick[rec], pick;
    if (last && f > (float)last - 1.5f - CV_DEAD && f < (float)last - 0.5f + CV_DEAD) pick = last - 1u;
    else pick = (uint32_t)(f + 0.5f);
    r->cvpick[rec] = (uint8_t)(pick + 1u);
    uint32_t mode = pick < CLIP_JMODES ? pick : CLIP_JMODES - 1u;
    int on = clip_ok(c) && (c->mode[idx] & CLIP_METHOD) != CLIP_OFF;
    if (on && c->jmode[idx] != mode) {
        c->jmode[idx] = (uint8_t)mode;
        c->sum = clip_sum(c);
    }
    if (cv_show(r, rec)) chr_osd_note(OSD_COMP, 0, SC_OSD_CV, on ? mode : CV_OFF, n);
}

/* ---- b21, the Clip Tempo row (id 0x1ae), after Sync on the Clip pads' page ----
 * The same setting as CC 112, saved with the pad. An edit on the page, and a
 * preset load posting the pad's saved value, reach the Clip voice's own setter
 * on the audio task, as CC 112 does: the Gate hook's grow calls clip_row. CC
 * 112 (or 113, from Stock) marks the pad, and the UI tick puts the new value in
 * the pad's store, which the page shows and preset.xml saves: clip_row_tick.
 * Keep CCs ON (b23, owner): as for the other controls, what the controller
 * sent carries over and nothing else. CC 112 or 113 records the method it
 * moved a pad to (ccmoved, ccmeth, until power-off); before a load's Finalize
 * posts the new preset's stores, clip_row_keep writes those pads' recorded
 * methods into them. Every other pad, and everything with Keep CCs OFF, takes
 * the preset's own: a screen edit belongs to the preset, and a preset saved
 * before the row loads Stock. (b21-b22 carried every pad's current value, so a
 * screen edit followed the owner into every preset.) */
#define CLIP_ROW_ID      0x1aeu        /* patch_micro.py: CLIP_ROW_ID */
#define CLIP_ROW_SM      0x24013cb0u   /* patch_micro.py: SESSION_MGR */
#define CLIP_ROW_STORES  0x1840u       /* its pad stores, + row * 0xf0 + column * 0x30 (0x08076d54) */
#define CLIP_ROW_UIQ     0x2cu         /* its UI message queue (0x0805b18c's first argument) */
#define CLIP_ROW_MSG     0x66u         /* the message: pad (row << 4 | column) changed, read its store again */

#ifdef CHR_HOST
static void clip_x_uipost(void *q, const void *msg) { (void)q; (void)msg; }
#else
extern void clip_x_uipost(void *q, const void *msg);   /* 0x0805b18c: post(queue, &message) */
#endif

/* position p's store */
static void *clip_row_store(uint32_t p)
{
    return (void *)(uintptr_t)(CLIP_ROW_SM + CLIP_ROW_STORES + (p >> 2) * 0xf0u + (p & 3u) * 0x30u);
}

/* clip_cc: CC 112 or 113 moved pad idx's Clip Tempo to `method` (audio task):
 * its row to update, and the value Keep CCs carries; by position */
static void clip_row_mark(int idx, uint32_t method)
{
    int p = clip_pos(idx);
    if (p < 0) return;
    sc_ram_t *s = sc_ram();
    s->rowstale[p] = 1u;
    s->ccmoved = (uint8_t)(s->ccmoved | 1u << p);
    s->ccmeth = (uint16_t)((s->ccmeth & ~(3u << 2 * p)) | (method & CLIP_METHOD) << 2 * p);
}

/* The Clip voice's setter, id 0x1ae (audio task): the method only, as CC 112
 * steps it; Manual stays as it was, unless Stock. No pop-up: the row shows it. */
void clip_row(uint8_t *pad, uint32_t clk, uint32_t val)
{
    clip_ram_t *c = clip_ram();
    if (!clip_ok(c)) clip_reset(c);
    if (clip_clk_ok(clk)) c->clk = clk;
    int idx = clip_idx(pad);
    if (idx < 0 || val > CLIP_KEYLOCK) return;
    uint32_t was = c->mode[idx], wm = was & CLIP_METHOD;
    if (val == wm) return;
    uint32_t now = val == CLIP_OFF || wm == CLIP_OFF ? val : (val | (was & CLIP_MANUAL));
    if (!clip_is_clip(pad)) {                    /* nothing of a Clip voice to move: kept for the day it is one */
        c->mode[idx] = (uint8_t)now;
        c->sum = clip_sum(c);
        return;
    }
    clip_method_go(pad, c, idx, was, now);
}

/* The Gate hook's mfinal, before every preset load's Finalize (UI thread):
 * the marks are the last preset's; Keep CCs ON writes what CC 112 or 113 last
 * sent each pad it moved into the new stores, so Finalize posts it back. */
void clip_row_keep(void)
{
    sc_ram_t *s = SC_PTR;
    if (s->magic != SC_MAGIC) return;             /* nothing marked, nothing moved */
    for (uint32_t p = 0; p < CLIP_POS_N; p++) s->rowstale[p] = 0;
    if (*(volatile const uint32_t *)(uintptr_t)CLIP_KEEP_RAM != CLIP_KEEP_ON) return;
    for (uint32_t p = 0; p < CLIP_POS_N; p++)
        if (s->ccmoved & 1u << p)
            cmp_x_setparam(clip_row_store(p), CLIP_ROW_ID, (int32_t)(s->ccmeth >> 2 * p & 3u));
}

/* UI side, every tick (chr_osd_tick): a pad CC 112 moved, into its store, and
 * its page told. A byte a pad, so the audio task's mark is never lost. */
static void clip_row_tick(void)
{
    sc_ram_t *s = SC_PTR;
    clip_ram_t *c = clip_ram();
    if (s->magic != SC_MAGIC || !clip_ok(c)) return;
    for (uint32_t p = 0; p < CLIP_POS_N; p++) {
        if (!s->rowstale[p]) continue;
        s->rowstale[p] = 0;
        cmp_x_setparam(clip_row_store(p), CLIP_ROW_ID, (int32_t)(c->mode[clip_pos_idx(p)] & CLIP_METHOD));
        uint32_t msg[6] = { CLIP_ROW_MSG, 0u, (p >> 2) << 4 | (p & 3u), 0u, 0u, 0u };
        clip_x_uipost((void *)(uintptr_t)(CLIP_ROW_SM + CLIP_ROW_UIQ), msg);
    }
}

/* ---- the pop-up (UI side): "Flip In 3" over "Rotate"; "Hold FX In 3" over
 * "Freeze"; "Clip Tempo off" when the pad's Clip Tempo is Stock ---- */
static void cv_osd_compose(uint32_t value, uint32_t type, osd_text_t *tx)
{
    static const char *const hold[8] = { "Off", "Roll", "Pitch Roll", "Freeze", "Pull Up", "Power Down", "Brake", "Scratch" };
    uint32_t n = type & 0xfu;
    int i = osd_put(tx->t[0], 0, type & CV_HOLDK ? "Hold FX In " : "Flip In ");
    char d[2] = { (char)('0' + (n >= 1u && n <= 8u ? n : 8u)), 0 };
    osd_put(tx->t[0], i, d);
    tx->n = 2;
    if (value == CV_OFF) osd_put(tx->t[1], 0, "Clip Tempo off");
    else if (type & CV_HOLDK) osd_put(tx->t[1], 0, hold[value < 8u ? value : 7u]);   /* b24: Off first */
    else osd_put(tx->t[1], 0, clip_jm_names[value < CLIP_JMODES ? value : CLIP_JMODES - 1u]);
}

static const char *const sc_in_names[SC_NKEYS] = {
    "Self", "Gate 1", "Gate 2", "Gate 3", "Gate 4", "Gate 5", "Gate 6", "Gate 7", "Gate 8", "MIDI 16",
};

static void sc_osd_compose(uint32_t ctrl, uint32_t value, uint32_t type, osd_text_t *tx)
{
    (void)ctrl;
    osd_put(tx->t[0], 0, "COMPRESSOR");
    tx->n = 2;
    int i = osd_put(tx->t[1], 0, "SC In  ");
    osd_put(tx->t[1], i, type == CMP_LOGICAL4 ? "N/A" : sc_in_names[value < SC_NKEYS ? value : 0]);
}

#endif
