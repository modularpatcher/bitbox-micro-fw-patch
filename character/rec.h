/*
 * rec.h -- 2.3.12-b15: recording from the gate inputs (CHR_SC builds only).
 * Included by character.c after sidechain.h.
 *
 * Two rows pick a job for an input jack, each "Off", "Record 1-8" or
 * "Replace 1-8", the number being the jack:
 *   Rec In (per pad, on its own page): that jack records into this pad.
 *   Rec Selected (Settings page 3): that jack records into the selected pad.
 * Record posts the UI event a MIDI Rec note posts (0x2b, pad k): stock
 * starts a take into an empty pad and does nothing with a full one. Replace
 * posts clear (0x2c) first; the old WAV stays on the card, as stock's clear
 * leaves it. While a take runs, any jack with a job posts 0x2b for the
 * selected pad, which stock selected when it started the take: that stops
 * it. Stock drops 0x2b for any other pad then, and 0x2c altogether.
 *
 * Pads sharing a jack take turns in screen order, one take each. Per jack, a
 * turn (where the next search starts) and a pending pad (the last take's).
 * The turn moves past the pending pad only once it has content, looked at
 * the jack's next rise with no take running: a take stopped before its
 * quantised start leaves the pad empty, and the same pad goes again. A
 * Record pad is eligible only while empty, a Replace pad always. If the
 * jack is Rec Selected's too, Rec Selected wins and the pads wait.
 *
 * The Gate hook calls rec_gate at each gate's rise, on the engine thread,
 * before it plays any pad: a jack does every job it is given, recording
 * first. gcap keeps Rec In per pad record; the Settings posts keep Rec
 * Selected. The rest of the block is ours.
 */
#ifndef REC_H
#define REC_H

#define REC_RAM          0x240693e0u   /* patch_micro.py: REC_RAM, cleared by ginit under the Gate magic */
#define REC_EVT_RECORD   0x2bu         /* record pad k (0x080aba98's event) */
#define REC_EVT_CLEAR    0x2cu         /* clear pad k */
#define REC_UI_LIST      0x15288u      /* the UI list, at the router's context + this */
#define REC_ENGINE       (0x24022978u + 0xa204u)   /* MANAGER's engine pointer */
#define REC_TAKE         0x143u        /* engine + this: a take is running */
#define REC_UI           0x24013cb0u   /* SessionMgr */
#define REC_SELECTED     (REC_UI + 0x14u)          /* u16 page << 8 | row << 4 | col */
#define REC_PADCFG       (REC_UI + 0x1840u)        /* + row * 0xf0 + col * 0x30: +0x18 the file name */
enum { REC_OFF, REC_RECORD, REC_REPLACE };

typedef struct {
    uint8_t in[9];                     /* +0x00: Rec In per pad record: 0 Off, 1-8 Record n, 9-16 Replace n */
    uint8_t sel;                       /* +0x09: Rec Selected, as Rec In */
    uint8_t spare[2];
    uint8_t turn[8];                   /* +0x0c: per jack, the pad (screen order) the next search starts at */
    uint8_t pending[8];                /* +0x14: per jack, the pad its last take went to, + 1; 0 none */
    uint32_t count;                    /* +0x1c: posts so far: each event's +4 */
} rec_ram_t;
_Static_assert(__builtin_offsetof(rec_ram_t, sel) == 9, "patch_micro.py: RC_SEL");
_Static_assert(__builtin_offsetof(rec_ram_t, turn) == 0x0c, "patch_micro.py: RC_TURN");
_Static_assert(__builtin_offsetof(rec_ram_t, pending) == 0x14, "patch_micro.py: RC_PEND");
_Static_assert(__builtin_offsetof(rec_ram_t, count) == 0x1c, "patch_micro.py: RC_COUNT");
_Static_assert(sizeof(rec_ram_t) == 0x20, "recording has 0x20 bytes of patch RAM");

#ifdef CHR_HOST
static uint8_t rec_host_ram[0x20];
#define REC_PTR ((rec_ram_t *)rec_host_ram)
static void rec_x_append(void *list, const void *ev) { (void)list; (void)ev; }
#else
#define REC_PTR ((rec_ram_t *)(uintptr_t)REC_RAM)
extern void rec_x_append(void *list, const void *ev);   /* 0x080a396c: append(list, &event) */
#endif

/* a row's value: its job for jack n (1-8), or REC_OFF */
static uint32_t rec_job(uint32_t v, uint32_t n)
{
    if (v >= 1u && v <= 8u) return v == n ? REC_RECORD : REC_OFF;
    if (v >= 9u && v <= 16u) return v - 8u == n ? REC_REPLACE : REC_OFF;
    return REC_OFF;
}

static int rec_taking(void)
{
    uint32_t eng = *(volatile const uint32_t *)(uintptr_t)REC_ENGINE;
    return eng && *(volatile const uint8_t *)(uintptr_t)(eng + REC_TAKE) != 0u;
}

/* the selected pad, 0-7 (row * 4 + column), or -1 if the selection is not a pad */
static int rec_selected(void)
{
    uint32_t s = *(volatile const uint16_t *)(uintptr_t)REC_SELECTED;
    uint32_t row = (s >> 4) & 0xfu, col = s & 0xfu;
    if ((s >> 8) || row > 1u || col > 3u) return -1;
    return (int)(row * 4u + col);
}

static int rec_empty(uint32_t k)
{
    const char *p = *(const char *const volatile *)(uintptr_t)(REC_PADCFG + (k >> 2) * 0xf0u + (k & 3u) * 0x30u + 0x18u);
    return !p || !*p;
}

static void rec_post(void *ctx, uint32_t evt, uint32_t k)
{
    rec_ram_t *r = REC_PTR;
    uint32_t ev[6];                    /* 24 bytes, zeroed: event, +4 differs post to post, +8 pad */
    ev[0] = evt;
    ev[1] = ++r->count;                /* the forwarder drops one identical to one still unread */
    ev[2] = k;
    ev[3] = 0u; ev[4] = 0u; ev[5] = 0u;
    rec_x_append((uint8_t *)ctx + REC_UI_LIST, ev);
}

/* The Gate hook, at a gate's rise: input is the router's input record, ctx
 * the gate's context. Returns the events posted. */
uint32_t rec_gate(uint32_t input, void *ctx)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < 8u; i++)
        if (sc_inputs[i] == input) n = i + 1u;
    if (!n) return 0;
    rec_ram_t *r = REC_PTR;
    uint32_t sel = rec_job(r->sel, n);
    uint32_t group = 0, replace = 0;   /* bit k: pad k records from jack n; replaces */
    for (uint32_t k = 0; k < 8u; k++) {
        uint32_t job = rec_job(r->in[k < 4u ? k : k + 1u], n);
        if (job != REC_OFF) group |= 1u << k;
        if (job == REC_REPLACE) replace |= 1u << k;
    }
    if (sel == REC_OFF && !group) return 0;
    if (rec_taking()) {                /* stop it: stock selected the pad it records */
        int k = rec_selected();
        if (k < 0) return 0;
        rec_post(ctx, REC_EVT_RECORD, (uint32_t)k);
        return 1;
    }
    if (sel != REC_OFF) {              /* Rec Selected wins */
        int k = rec_selected();
        if (k < 0) return 0;
        if (sel == REC_REPLACE) rec_post(ctx, REC_EVT_CLEAR, (uint32_t)k);
        rec_post(ctx, REC_EVT_RECORD, (uint32_t)k);
        return sel == REC_REPLACE ? 2u : 1u;
    }
    uint32_t j = n - 1u;
    uint32_t p = r->pending[j];
    r->pending[j] = 0;
    if (p >= 1u && p <= 8u && !rec_empty(p - 1u))
        r->turn[j] = (uint8_t)(p & 7u);    /* past it: it has its take */
    uint32_t t = r->turn[j] & 7u;
    for (uint32_t i = 0; i < 8u; i++) {
        uint32_t k = (t + i) & 7u;
        if (!(group >> k & 1u)) continue;
        uint32_t rep = replace >> k & 1u;
        if (!rep && !rec_empty(k)) continue;
        r->pending[j] = (uint8_t)(k + 1u);
        if (rep) rec_post(ctx, REC_EVT_CLEAR, k);
        rec_post(ctx, REC_EVT_RECORD, k);
        return rep ? 2u : 1u;
    }
    return 0;
}

#endif
