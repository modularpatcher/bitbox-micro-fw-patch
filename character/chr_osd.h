/*
 * chr_osd.h -- the OSD pop-up (opt-in build flag --osd; see
 * CHARACTER-DESIGN.md section 11). Included by character.c.
 *
 * Writers, each to its own words of patch RAM:
 *   chr_osd_note()  -- MIDI side, from chr_cc: record what moved and its
 *                      value under an odd-while-writing sequence. Character,
 *                      EQ, compressor and delay share one request slot; pad
 *                      switches have their own.
 *   hook O's asm    -- the Settings row (`off`), the clock object's address
 *                      (recorded by its constructor at boot, `clk`), and a
 *                      pad voice ACCEPTING a pad CC (`acc_*`, written by two
 *                      head insertions in front of hooks A and B, on the
 *                      audio thread: four stores, nothing else).
 *   chr_osd_tick()  -- UI side, every UI tick (hook O at the UI present):
 *                      takes each new request EXACTLY ONCE (by its sequence
 *                      number), composes its two lines -- WHERE on top, WHAT
 *                      and its value underneath, in real units -- and shows it
 *                      only if that text differs from what was last shown for
 *                      the same parameter. Watches the clock for an external
 *                      clock letting go, taking over, or settling on a new
 *                      tempo. The current pop-up's text is frozen when it is
 *                      accepted and retired 1.5 s later, once.
 *
 * chr9: the pop-up used to be RE-COMPOSED from the last request every tick
 * and shown while `now - t0` was under 1.5 s. DWT CYCCNT wraps every 2^32
 * cycles (~10.7 s at 400 MHz), so every expired request and clock event came
 * back for 1.5 s once per wrap, with live values: the compressor pop-up
 * "flashing again and again", a Synced pop-up showing the master's new tempo,
 * and CLOCK OFF "a second time". Requests are now consumed once.
 */
#ifndef CHR_OSD_H
#define CHR_OSD_H

#define CHR_OSD_OFF       0x100u      /* offsets from CHR_RAM_BASE */
/* the UI-side area (last-shown table, clock, mask): CHR_OSD2_ADDR, character.c */
#define CHR_OSD2_BYTES    0xe00u
#define CHR_OSD_OFFMAGIC  0x4f53444fu /* "OSDO": the Settings row says Off */
#define CHR_OSD_RMAGIC    0x4f534452u /* "OSDR": a request has been written */
#define CHR_OSD_UMAGIC    0x4f534457u /* "OSDW": the UI-side state is ours (ch11 layout) */
#define CHR_OSD_CMAGIC    0x4f534443u /* "OSDC": the clock watcher has a baseline */
#define CHR_OSD_TMAGIC    0x4f534455u /* "OSDU": the last-shown table and mask are ours (ch11) */
#define CHR_OSD_CYCLES    600000000u  /* 1.5 s at ~400 MHz (1.25 s at 480) */
#define CHR_OSD_PAD_WAIT  200000000u  /* 0.5 s for a pad voice to take a pad CC */
#define CHR_OSD_STEADY    200000000u  /* 0.5 s: a tempo must hold this long */
#define CHR_OSD_SYNC_MAX  800000000u  /* 2 s: Synced shows even if never steady */
#define CHR_DWT_CYCCNT    0xE0001004u
#define CHR_DCCMVAC       0xE000EF68u
#define CHR_FONT          0x080bf664u /* firmware 6x8 glyphs, ASCII 0x20-0x7f */
#define CHR_LFO_LABELS    0x080c478cu /* firmware's LFO Wave labels, raw 0-9 (patcher guards) */
#define CHR_DISPLAY_TOP_A 0x120u      /* top layer's two buffer descriptors */
#define CHR_DISPLAY_TOP_B 0x12cu
#define CHR_OSD_BG        0x8842u     /* ARGB1555, opaque near-black (K's bar bg) */
#define CHR_OSD_FG        0xffffu     /* opaque white */
#define CHR_SCREEN_W      320
#define CHR_SCREEN_H      240
/* THE BOX IS ONE SIZE FOR EVERY POP-UP: two lines of OSD_COLS characters at
 * 2x (12 x 16 per character), so moving from one pop-up to the next never
 * makes it jump. OSD_COLS is the longest line any pop-up can show (the test
 * enumerates them): "Loop Mode  Bidirectional". Each line is centred in it,
 * and every value has a fixed width, so turning a knob never moves the text. */
#define OSD_COLS 24
#define OSD_PADX 6
#define OSD_PADY 4
#define OSD_BOX_W (12 * OSD_COLS - 2 + 2 * OSD_PADX)     /* 298 */
#define OSD_BOX_H (16 * 2 - 2 + 2 * OSD_PADY)            /* 38 */
#define OSD_X0 ((CHR_SCREEN_W - OSD_BOX_W) / 2)          /* 11 */
#define OSD_Y0 ((CHR_SCREEN_H - OSD_BOX_H) / 2)          /* 101 */
#define CHR_DMA2D         0x52001000u /* the blitter the stock UI fills the top layer with */
#define CHR_DISPLAY_DMA2D 0xa8u       /* display object: its DMA2D handle; +0 Instance */
_Static_assert(OSD_BOX_W <= CHR_SCREEN_W, "the box must fit the screen");

/* Patch RAM the other hooks own, read here and never written. */
#define CHR_COMP_STORE    0x2406c000u /* hook D/E: magic, thresh, ratio, atk, rel, makeup, sum */
#define CHR_COMP_MAGIC    0x1010c0deu
#define CHR_AUTO_ADDR     0x2406a620u /* hook Dr (rc1): auto makeup, {magic, ~magic} = ON */
#define CHR_AUTO_MAGIC    0x4f545541u
#define CHR_CLK_MODE      0x2406c088u /* hook J: CLK_MAGIC = HOLD */
#define CHR_CLK_HELD      0x2406c08cu /* hook J: CLK_MAGIC once an external tempo was used */
#define CHR_CLK_MAGIC     0x484f4c44u
#define CHR_CLK_SOURCE    0x2c60u     /* clock object: 0 internal, 1 CV, 2 MIDI */
#define CHR_CLK_LIVE      0x18u       /* clock object: the tempo in use (float) */
#define CHR_CLK_CONF      0x13cu      /* clock object: the preset's tempo (float) */

/* One MIDI-side request: written by the MIDI thread under an odd-while-
 * writing sequence, read by the UI tick. */
typedef struct {
    uint32_t magic;
    uint32_t seq;
    uint32_t t0;
    uint8_t kind, slot, ctrl, value;
    uint8_t type, pad0, pad1, pad2;   /* Character: type; pad CC: acc_seq at the CC */
} osd_req_t;

typedef struct {
    uint32_t off;                     /* +0x00 the Settings row, from hook O's wrapper:
                                         OFFMAGIC = Off, anything else (default) = On */
    osd_req_t rq;                     /* +0x04 Character, EQ, compressor, delay */
    uint32_t umagic;                  /* +0x18 UI side */
    uint32_t shown;                   /* +0x1c hash of the text on screen */
    uint32_t cmagic;                  /* +0x20 clock watcher: baseline taken */
    uint32_t drawn;                   /* +0x24 */
    uint16_t bx, by, bw, bh;          /* +0x28 */
    uint32_t clk_src;                 /* +0x30 the source byte at the last tick */
    uint32_t clk_was;                 /* +0x34 the clock object at the last tick */
    uint32_t active;                  /* +0x38 a pop-up is current */
    uint32_t act_t0;                  /* +0x3c when it was accepted (UI tick time) */
    uint32_t clk;                     /* +0x40 hook O asm: the clock object, from its ctor */
    uint32_t acc_seq;                 /* +0x44 hook O asm: +1 per pad CC a voice accepted */
    uint32_t acc_cc;                  /* +0x48 hook O asm: cc | voice class << 8 (1 A, 2 B) */
    uint32_t acc_val;                 /* +0x4c hook O asm: the 14-bit value the voice got */
    uint32_t rq_done;                 /* +0x50 the rq sequence last taken */
    uint32_t prq_done;                /* +0x54 the prq sequence last taken */
    char text[2][28];                 /* +0x58 the current pop-up's lines (tests read them) */
    osd_req_t prq;                    /* +0x90 pad CCs: their own slot */
    uint32_t pend, pend_since, pend_snap;   /* +0xa4 a pad CC waiting for a voice */
    uint8_t pend_chan, pend_cc, pend_p0, pend_p1;
} chr_osd_t;
_Static_assert(__builtin_offsetof(chr_osd_t, clk) == 0x40, "hook O writes clk at +0x40");
_Static_assert(__builtin_offsetof(chr_osd_t, acc_seq) == 0x44, "hook O writes acc at +0x44");
_Static_assert(__builtin_offsetof(chr_osd_t, text) == 0x58, "tests read text at +0x58");
_Static_assert(sizeof(chr_osd_t) <= 0x100, "the OSD has 0x100 bytes before the dry buffers");

#ifdef CHR_CMP
/* b24: the roles in theme.h's table of the patch's own colours (THO_RAM) */
enum { THO_BOX_BG, THO_BOX_FG, THO_BAR_OK, THO_BAR_WARN, THO_BAR_HOT, THO_BAR_PEAK, THO_BAR_BG,
       THO_STRIP_INT, THO_STRIP_CV, THO_STRIP_MIDI, THO_STRIP_OFF, THO_STRIP_FALL, THO_STRIP_PARK, THO_N };
uint16_t thm_osd_col(uint32_t r, uint16_t dflt);
#endif

/* UI-side only, past the Character work area (patch RAM, nothing else there). */
#ifdef CHR_CMP
#define OSD_KEYS 290                  /* 32 globals + 16 x 15 pad CCs + CC 47 + 16 Clip Tempo
                                         pads (2.3.10); 290, not 289, keeps col 8-aligned */
#else
#define OSD_KEYS 272                  /* 32 globals + 16 channels x 15 pad CCs (79-93) */
#endif
typedef struct {
    uint32_t tmagic;                  /* the table below is ours */
    uint32_t tempo_shown;             /* the whole BPM last shown (0: none) */
    uint32_t cand, cand_since;        /* the whole BPM now, and since when */
    uint32_t sync_pend, sync_since;   /* an external clock took over; not yet shown */
    uint32_t pad[2];
    uint32_t last[OSD_KEYS];          /* per parameter: hash of the text last shown */
    uint64_t col[OSD_BOX_W];          /* the box, one column per word: bit r = row r is text */
} chr_osd2_t;
_Static_assert(__builtin_offsetof(chr_osd2_t, col) % 8 == 0, "aligned for the device memory");
_Static_assert(sizeof(chr_osd2_t) <= CHR_OSD2_BYTES, "OSD2 size");

enum { OSD_CHR = 1, OSD_EQ = 2, OSD_COMP = 3, OSD_PAD = 4, OSD_DLY = 5, OSD_CLIP = 6 };
#ifdef CHR_SC
#define CLIP_SHOW_JCV    11u          /* clip_tempo.h's pop-up: CC 117 while input n's CV owns the mode, this
                                         + n - 1 (b24; the value, a byte, is CC 117's) */
#endif
enum { OSD_C_TYPE, OSD_C_AMOUNT, OSD_C_TONE };
enum { OSD_E_MODE, OSD_E_A, OSD_E_B, OSD_E_C, OSD_E_PA, OSD_E_PB, OSD_E_PC, OSD_E_BYP, OSD_E_SLOPE };

/* On-screen names. Slot 1 is "drive" internally but shows OVERDRIVE (chr6);
 * RingModulator shows as "Ring Mod" (chr9). */
static const char *const chr_slot_names[4] = { "DESTRUCTION", "OVERDRIVE", "COLOUR", "SATURATION" };
static const char *const chr_type_names[4][4] = {
    { "Pockey2", "DeRez3", "Ring Mod", "Fracture2" },
    { "Tube2", "Drive", "Mackity", "Huge" },
    { "Channel9", "IronOxide5", "ToTape8", "Coils2" },
    { "PurestDrive", "Spiral2", "Density3", "Mojo" },
};
/* Channel9's Tone is its console type, five steps (the port's own switch). */
static const char *const chr_console_names[5] = { "Neve", "API", "SSL", "Teac", "Mackie" };
/* EQ-FILTER.md's names: ten modes, each mode's encoders 2-4 (A-C), and what
 * each push does there (0: that push does nothing in that mode). "Mixer EQ"
 * shows as "Mixer" so "EQ  Mixer" never repeats a word (chr7). */
static const char *const chr_eq_names[10] = {
    "DJ Filter", "Dual Cut", "Band Pass", "Mixer", "Tone + Filter",
    "Tilt", "Notch", "Peak", "Phaser", "Formant",
};
static const char *const chr_eq_labels[10][3] = {
    { "Filter", "Resonance", "Knee" },
    { "High Cut", "Low Cut", "Resonance" },
    { "Centre Freq", "Width", "Gain" },
    { "High", "Mid", "Low" },
    { "High", "Low", "Filter" },
    { "Tilt", "Hinge Freq", "Control C" },
    { "Frequency", "Q", "Depth" },
    { "Frequency", "Q", "Gain" },
    { "Sweep", "Depth", "Spread" },
    { "Vowel", "Intensity", "Shift" },
};
static const char *const chr_eq_pushes[10][3] = {
    { 0, 0, 0 },
    { "Open High Cut", "Open Low Cut", 0 },
    { 0, "Open Width", 0 },
    { "Kill High", "Kill Mid", "Kill Low" },
    { "Kill High", "Kill Low", "Open Filter" },
    { "Slam Tilt", 0, 0 },
    { 0, 0, "Slam Depth" },
    { 0, "Slam Q", "Slam Gain" },
    { 0, "Slam Depth", "Slam Spread" },
    { 0, "Slam Intensity", 0 },
};
/* the modes CC 59 (filter slope) acts on: DJ Filter, Dual Cut, Band Pass, Tone + Filter */
#define CHR_EQ_SLOPE_MODES ((1u << 0) | (1u << 1) | (1u << 2) | (1u << 4))
static const char *const chr_vowels[5] = { "I", "E", "A", "O", "U" };   /* D-29 order */
static const char *const chr_comp_names[5] = { "Threshold", "Ratio", "Attack", "Release", "Makeup" };
static const char *const chr_loop_names[3] = { "None", "Forward", "Bidirectional" };

/* The highpass corners, in tenths of a hertz, per Tone CC value 0-127.
 * Drive (ch11: the full Highpass control, B = tone): MEASURED on the ported
 * code (scratch measure2.c), because its two alternating states mirror the
 * response about 12 kHz and, from about Tone 75, the whole passband drops too
 * (-3.0 dB at best at CC 100, -16.5 dB at CC 127, as the plugin does): the
 * corner is the lowest frequency within 3 dB of the response's own maximum,
 * which matches the 24 kHz one-pole's -3 dB point wherever that exists.
 * Density3: the exact closed form for its one-pole (`lp += a (x - lp); y =
 * x - lp`, a = t^3 / (48000/44100) at 48 kHz), checked on the host. 0 = no
 * filter at all (Tone 0). 0xffffffff = Density3's gain never comes back up to
 * -3 dB below Nyquist. tests/test_firmware_osd.py regenerates Density3's;
 * tests/test_character_dsp.py re-measures Drive's at spot values. */
static const uint32_t osd_drive_hz10[128] = {
    0u, 0u, 0u, 0u, 0u, 0u, 9u, 9u,
    10u, 10u, 10u, 26u, 27u, 35u, 44u, 59u,
    75u, 78u, 100u, 124u, 141u, 159u, 177u, 208u,
    240u, 273u, 306u, 340u, 374u, 422u, 467u, 516u,
    566u, 621u, 674u, 739u, 806u, 880u, 948u, 1030u,
    1113u, 1196u, 1294u, 1388u, 1488u, 1593u, 1703u, 1819u,
    1942u, 2067u, 2200u, 2342u, 2490u, 2640u, 2791u, 2956u,
    3135u, 3303u, 3495u, 3684u, 3891u, 4091u, 4307u, 4532u,
    4764u, 5010u, 5258u, 5518u, 5784u, 6064u, 6348u, 6654u,
    6962u, 7279u, 7610u, 7956u, 8314u, 8678u, 9055u, 9450u,
    9850u, 10273u, 10704u, 11160u, 11623u, 12098u, 12594u, 13103u,
    13632u, 14180u, 14745u, 15325u, 15930u, 16545u, 17192u, 17855u,
    18535u, 19240u, 19972u, 20719u, 21497u, 22300u, 23131u, 23977u,
    24850u, 25764u, 26692u, 27661u, 28650u, 29679u, 30733u, 31819u,
    32938u, 34090u, 35271u, 36490u, 37742u, 39027u, 40347u, 41706u,
    43101u, 44520u, 45982u, 47481u, 49010u, 50576u, 52178u, 53806u,
};

static const uint32_t osd_density3_hz10[128] = {
    0u, 0u, 0u, 1u, 2u, 4u, 7u, 12u,
    18u, 25u, 34u, 46u, 59u, 75u, 94u, 116u,
    141u, 169u, 201u, 236u, 276u, 319u, 367u, 420u,
    478u, 541u, 609u, 683u, 763u, 850u, 942u, 1042u,
    1148u, 1262u, 1383u, 1513u, 1651u, 1797u, 1952u, 2117u,
    2292u, 2477u, 2672u, 2879u, 3097u, 3327u, 3570u, 3826u,
    4096u, 4380u, 4679u, 4994u, 5326u, 5674u, 6041u, 6427u,
    6833u, 7260u, 7709u, 8181u, 8678u, 9201u, 9752u, 10332u,
    10943u, 11587u, 12266u, 12982u, 13738u, 14537u, 15382u, 16275u,
    17222u, 18226u, 19291u, 20424u, 21629u, 22914u, 24286u, 25754u,
    27328u, 29019u, 30840u, 32807u, 34939u, 37257u, 39788u, 42563u,
    45623u, 49016u, 52804u, 57070u, 61920u, 67502u, 74021u, 81782u,
    91259u, 103250u, 119261u, 142783u, 187340u, 0xffffffffu, 0xffffffffu, 0xffffffffu,
    0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
    0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
    0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu,
};

static const float osd_eq_ftab[17] = {
    40.000000f, 57.132084f, 81.601877f, 116.552133f, 166.471658f, 237.771821f, 339.609994f, 485.065671f, 692.820323f, 989.556731f, 1413.385969f, 2018.742164f, 2883.373697f, 4118.328741f, 5882.217640f, 8401.583879f, 12000.000000f,
};

#define CHR_EQ_STORE  0x2406c040u     /* hook F's store: magic, mode slot, ... */
#define CHR_EQ_MAGIC  0x1010e00eu

static chr_osd_t *osd_ram(void) { return CHR_PTR(CHR_OSD_OFF); }
static chr_osd2_t *osd_ram2(void) { return CHR_OSD2_PTR(); }

static uint32_t osd_now(void)
{
#ifdef CHR_HOST
    return 0;
#else
    return *(volatile uint32_t *)CHR_DWT_CYCCNT;
#endif
}

/* ---- the MIDI side --------------------------------------------------------- */
static void osd_note(volatile osd_req_t *r, uint32_t kind, uint32_t slot, uint32_t ctrl, uint32_t value, uint32_t type)
{
    uint32_t s = r->seq | 1u;         /* odd: writing */
    r->seq = s;
    r->kind = (uint8_t)kind; r->slot = (uint8_t)slot; r->ctrl = (uint8_t)ctrl;
    r->value = (uint8_t)value; r->type = (uint8_t)type;
    r->t0 = osd_now();
    r->magic = CHR_OSD_RMAGIC;
    r->seq = s + 1u;                  /* even: done */
}

static void chr_osd_note(uint32_t kind, uint32_t slot, uint32_t ctrl, uint32_t value, uint32_t type)
{
    osd_note(&osd_ram()->rq, kind, slot, ctrl, value, type);
}

/* EQ CCs (channel 1, CC 50-59), seen before hook F runs. Controls carry the
 * 7-bit value; pushes, bypass and slope a 0/1. */
static void chr_osd_eq_cc(uint32_t cc, uint32_t val14)
{
    uint32_t v = val14 >> 7;
    if (v > 127) v = 127;
    const volatile uint32_t *eq = (const volatile uint32_t *)(uintptr_t)CHR_EQ_STORE;
    uint32_t mode = 0;                /* DJ Filter: the EQ's boot mode */
    if (eq[0] == CHR_EQ_MAGIC && eq[1] < 10) mode = eq[1];
    switch (cc) {
    case 50: chr_osd_note(OSD_EQ, ((val14 & 0x3fffu) * 10u) >> 14, OSD_E_MODE, v, 0); return;
    case 51: case 52: case 53: chr_osd_note(OSD_EQ, mode, OSD_E_A + (cc - 51), v, 0); return;
    case 54: case 55: case 56: chr_osd_note(OSD_EQ, mode, OSD_E_PA + (cc - 54), v >= 64, 0); return;
    case 57: chr_osd_note(OSD_EQ, mode, OSD_E_BYP, v >= 64, 0); return;
    case 59: chr_osd_note(OSD_EQ, mode, OSD_E_SLOPE, v >= 64, 0); return;
    default: return;                  /* 58 is retired */
    }
}

#ifndef CHR_CMP                       /* 2.3.10: cmp_cc records these, with the type */
/* The compressor (channel 1, CC 40-45). The value shown is read back from
 * hook D's store when the pop-up is composed, so this records which one. */
static void chr_osd_comp_cc(uint32_t cc, uint32_t val14)
{
    uint32_t v = val14 >> 7;
    if (v > 127) v = 127;
    chr_osd_note(OSD_COMP, 0, cc - 40u, v, 0);
}
#endif

/* Any channel: every per-pad CC the patch adds (79-93) and the delay's four
 * (108-111). A pad CC pops up only once a voice that supports it took it. */
static void chr_osd_any_cc(uint32_t chan, uint32_t cc, uint32_t val14)
{
    uint32_t v = val14 >> 7;
    if (v > 127) v = 127;
    if (cc >= 79 && cc <= 93) {               /* every per-pad CC the patch adds */
        osd_note(&osd_ram()->prq, OSD_PAD, chan & 15u, cc, v, *(volatile uint32_t *)&osd_ram()->acc_seq & 0xffu);
        return;
    }
    switch (cc) {
    case 108: case 109: case 110:
        chr_osd_note(OSD_DLY, 0, cc, v >= 64, 0);
        return;
    case 111:                                 /* Filter Width, continuous */
        chr_osd_note(OSD_DLY, 0, cc, v, 0);
        return;
    default:
        return;
    }
}

/* ---- composing the text ---------------------------------------------------- */
typedef struct { char t[2][28]; int n; } osd_text_t;
/* The string helpers stay out of line: inlined, they multiplied the object by
 * several KB for no speed that matters on the UI tick. */

__attribute__((noinline)) static int osd_put(char *d, int i, const char *s)
{
    while (*s && i < 27) d[i++] = *s++;
    d[i] = 0;
    return i;
}

/* A number, right-aligned in `width` characters: x is the value times
 * 10^dec. "-18.3", " +6.0", "  0.0", " 96". Always the same width for a given
 * control, so the text never moves while a knob turns. */
__attribute__((noinline)) static int osd_fix(char *d, int i, int32_t x, int dec, int width, int plus)
{
    char tmp[12];
    int n = 0;
    uint32_t u = x < 0 ? (uint32_t)-x : (uint32_t)x;
    for (int j = 0; j < dec; j++) { tmp[n++] = (char)('0' + u % 10u); u /= 10u; }
    if (dec) tmp[n++] = '.';
    do { tmp[n++] = (char)('0' + u % 10u); u /= 10u; } while (u && n < 10);
    if (x < 0) tmp[n++] = '-';
    else if (plus && x > 0) tmp[n++] = '+';
    for (int p = n; p < width && i < 27; p++) d[i++] = ' ';
    while (n && i < 27) d[i++] = tmp[--n];
    d[i] = 0;
    return i;
}

/* A name, left-aligned and padded to `width`, so a list's longest name sets
 * the line's width and stepping through it never moves the text. */
__attribute__((noinline)) static int osd_name(char *d, int i, const char *s, int width)
{
    int n = 0;
    while (*s && i < 27) { d[i++] = *s++; n++; }
    while (n < width && i < 27) { d[i++] = ' '; n++; }
    d[i] = 0;
    return i;
}

static int32_t osd_round(float v) { return (int32_t)(v < 0.0f ? v - 0.5f : v + 0.5f); }

static int osd_onoff(char *d, int i, uint32_t v) { return osd_name(d, i, v ? "On" : "Off", 3); }

static int osd_pct(char *d, int i, int32_t pct) { i = osd_fix(d, i, pct, 0, 3, 0); return osd_put(d, i, "%"); }

/* tenths of a hertz -> always 8 characters: " 12.2 Hz", "  850 Hz", " 1.2 kHz", "12.0 kHz" */
__attribute__((noinline)) static int osd_hz10(char *d, int i, uint32_t t10)
{
    if (t10 < 1000u) { i = osd_fix(d, i, (int32_t)t10, 1, 5, 0); return osd_put(d, i, " Hz"); }
    if (t10 < 9995u) { i = osd_fix(d, i, (int32_t)((t10 + 5u) / 10u), 0, 5, 0); return osd_put(d, i, " Hz"); }
    i = osd_fix(d, i, (int32_t)((t10 + 500u) / 1000u), 1, 4, 0);
    return osd_put(d, i, " kHz");
}

static int osd_hz(char *d, int i, float f) { return osd_hz10(d, i, (uint32_t)osd_round(f * 10.0f)); }

static int osd_db(char *d, int i, float db) { i = osd_fix(d, i, osd_round(db * 10.0f), 1, 5, 1); return osd_put(d, i, " dB"); }

/* ---- Character ------------------------------------------------------------ */
/* Amount, as the engine scales it: value / 127. */
static int32_t osd_v7pct(uint32_t v7) { return (int32_t)((v7 * 200u + 127u) / 254u); }

/* Tone, in the effect's own unit where it has one (the port's own mapping,
 * t = value / 127 exactly as the engine computes it); otherwise %. */
static int osd_tone(char *d, int i, uint32_t slot, uint32_t type, uint32_t v7)
{
    float t = (float)v7 * (1.0f / 127.0f);
    switch (((slot & 3u) << 2) | (type & 3u)) {
    case 1: {                                 /* DeRez3: Rez B = 1 - 0.75 t, bits = 15 B + 1 */
        i = osd_put(d, i, "Tone ");
        i = osd_fix(d, i, osd_round((15.0f * (1.0f - 0.75f * t) + 1.0f) * 10.0f), 1, 4, 0);
        return osd_put(d, i, " bits");
    }
    case 2:                                   /* Ring Mod: carrier pow(t, 5) / overallscale rad/sample */
        i = osd_put(d, i, "Tone ");
        if (v7 == 0) return osd_name(d, i, "Off", 8);
        return osd_hz(d, i, t * t * t * t * t * (44100.0f / 6.28318531f));
    case 5:                                   /* Drive: its highpass, the whole control */
        i = osd_put(d, i, "Tone ");
        if (v7 == 0) return osd_name(d, i, "Off", 8);
        return osd_hz10(d, i, osd_drive_hz10[v7 & 127u]);
    case 8:                                   /* Channel9: Console Type, five steps */
        i = osd_put(d, i, "Tone  ");
        return osd_name(d, i, chr_console_names[(int32_t)(t * 4.999f)], 6);
    case 9: {                                 /* IronOxide5: tape speed, as the plugin displays it */
        float B2 = t * t;
        float ips = B2 * B2 * 148.5f + 1.5f;
        i = osd_put(d, i, "Tone ");
        i = osd_fix(d, i, osd_round(ips * 10.0f), 1, 5, 0);
        return osd_put(d, i, " ips");
    }
    case 12:                                  /* PurestDrive: drive into the sine, dB */
        i = osd_put(d, i, "Tone ");
        return osd_db(d, i, t < 0.5f ? (1.0f - 2.0f * t) * -6.0f : (2.0f * t - 1.0f) * 12.0f);
    case 14: {                                /* Density3: its highpass */
        uint32_t h = osd_density3_hz10[v7 & 127u];
        i = osd_put(d, i, "Tone ");
        if (v7 == 0) return osd_name(d, i, "Off", 8);
        if (h == 0xffffffffu) return osd_name(d, i, " >24 kHz", 8);
        return osd_hz10(d, i, h);
    }
    default:
        i = osd_put(d, i, "Tone ");
        return osd_pct(d, i, osd_v7pct(v7));
    }
}

/* ---- EQ: hook F/G's own control laws (eq_simulator.py gu16 / gftab / ghalf /
 * ggain / gtiltd / gfshift / detent), on the raw value hook F stores. ------ */
#define EQ_CCMAX   16256.0f
#define EQ_CENTRE  8128
#define EQ_DETENT  128
#define EQ_HALF    8000.0f

static float eq_gftab(float x)
{
    if (x < 0.0f) x = 0.0f;
    if (x > 16.0f) x = 16.0f;
    int k = (int)x;
    if (k > 15) k = 15;
    float fr = x - (float)k;
    return osd_eq_ftab[k] + (osd_eq_ftab[k + 1] - osd_eq_ftab[k]) * fr;
}
static float eq_gu16(uint32_t raw) { return (float)raw * (16.0f / EQ_CCMAX); }
static float eq_ghalf(uint32_t raw)
{
    if (raw <= EQ_CENTRE) return 0.0f;
    float m = (float)(raw - EQ_CENTRE) * (1.0f / (EQ_CCMAX - (float)EQ_CENTRE));
    return m > 1.0f ? 1.0f : m;
}
static int eq_detent(uint32_t raw, float *mag)
{
    int32_t dd = (int32_t)raw - EQ_CENTRE;
    int32_t m = dd < 0 ? -dd : dd;
    if (m <= EQ_DETENT) { *mag = 0.0f; return 0; }
    *mag = (float)(m - EQ_DETENT) * (1.0f / EQ_HALF);
    return dd > 0 ? 1 : -1;
}
static float eq_ggain(uint32_t raw)
{
    float m; int s = eq_detent(raw, &m);
    return s == 0 ? 0.0f : m * (s > 0 ? 9.0f : -24.0f);
}
static float eq_gtiltd(uint32_t raw)
{
    float m; int s = eq_detent(raw, &m);
    return s == 0 ? 0.0f : (s > 0 ? m * 9.0f : -(m * 9.0f));
}

/* DJ Filter's and Tone + Filter's bipolar filter: "Off" in the detent, else a
 * low-pass (anticlockwise) or high-pass (clockwise) corner. 11 characters. */
static int eq_bifilter(char *d, int i, uint32_t raw)
{
    float m; int s = eq_detent(raw, &m);
    if (m > 1.0f) m = 1.0f;
    if (s == 0) return osd_name(d, i, "Off", 11);
    i = osd_put(d, i, s > 0 ? "HP " : "LP ");
    return osd_hz(d, i, eq_gftab(s > 0 ? m * 16.0f : (1.0f - m) * 16.0f));
}

static int osd_eq_value(char *d, int i, uint32_t mode, uint32_t c, uint32_t v7)
{
    uint32_t raw = v7 << 7;
    int32_t pct = osd_v7pct(v7);              /* gqmap's laws are linear over the CC */
    switch (mode * 3u + c) {
    case 0: case 14: return eq_bifilter(d, i, raw);                   /* DJ, Tone + Filter */
    case 1: case 2: case 5: case 26: return osd_pct(d, i, pct);       /* resonance, knee, spread */
    case 3: return osd_hz(d, i, eq_gftab(16.0f - eq_ghalf(raw) * 16.0f));   /* high cut */
    case 4: return osd_hz(d, i, eq_gftab(eq_ghalf(raw) * 16.0f));           /* low cut */
    case 6: case 16: case 18: case 21: case 24:                       /* centre, hinge, freq, sweep */
        return osd_hz(d, i, eq_gftab(eq_gu16(raw)));
    case 7: {                                 /* band pass width: 2 x half-width, in octaves */
        float hw = 8.0f - eq_ghalf(raw) * 7.6f;
        i = osd_fix(d, i, osd_round(2.0f * hw * 0.514304f * 10.0f), 1, 3, 0);
        return osd_put(d, i, " oct");
    }
    case 8: case 9: case 10: case 11: case 12: case 13: case 23:      /* gains */
        return osd_db(d, i, eq_ggain(raw));
    case 15: return osd_db(d, i, eq_gtiltd(raw));                     /* tilt: treble +, bass - */
    case 17: return osd_name(d, i, "N/A", 3);                         /* Tilt has no control C */
    case 19: {                                /* notch Q, 1 .. 20 */
        i = osd_fix(d, i, osd_round((1.0f + 19.0f * (float)raw * (1.0f / EQ_CCMAX)) * 10.0f), 1, 4, 0);
        return i;
    }
    case 22: {                                /* peak Q, 0.707 .. 8 */
        i = osd_fix(d, i, osd_round((0.707f + 7.293f * (float)raw * (1.0f / EQ_CCMAX)) * 10.0f), 1, 4, 0);
        return i;
    }
    case 20: case 25: return osd_db(d, i, eq_ghalf(raw) * -24.0f);    /* notch, phaser depth */
    case 27: {                                /* formant vowel: I E A O U, continuous */
        float x = (float)raw * (4.0f / EQ_CCMAX);
        if (x > 4.0f) x = 4.0f;
        int k = (int)(x + 0.5f), lo = (int)x;
        float dk = x - (float)k;
        if (dk < 0.0f) dk = -dk;
        if (dk <= 0.25f || lo >= 4) return osd_name(d, i, chr_vowels[k > 4 ? 4 : k], 3);
        i = osd_put(d, i, chr_vowels[lo]);
        i = osd_put(d, i, "-");
        return osd_put(d, i, chr_vowels[lo + 1]);
    }
    case 28: return osd_pct(d, i, osd_round(eq_ghalf(raw) * 100.0f));        /* intensity */
    default: {                                /* formant shift, in octaves */
        float m; int s = eq_detent(raw, &m);
        float mult = s == 0 ? 1.0f : (s > 0 ? 1.0f + m : 1.0f - 0.5f * m);
        i = osd_fix(d, i, osd_round(chr_logf(mult) * (1.0f / 0.693147181f) * 10.0f), 1, 4, 1);
        return osd_put(d, i, " oct");
    }
    }
}

static void osd_compose_chr_eq(uint32_t kind, uint32_t slot, uint32_t ctrl, uint32_t value, uint32_t type, osd_text_t *tx)
{
    int i;
    tx->n = 2;
    if (kind == OSD_CHR) {
        i = osd_put(tx->t[0], 0, chr_slot_names[slot & 3]);
        i = osd_put(tx->t[0], i, "  ");
        osd_put(tx->t[0], i, chr_type_names[slot & 3][type & 3]);
        if (ctrl == OSD_C_TONE) { osd_tone(tx->t[1], 0, slot, type, value); return; }
        i = osd_put(tx->t[1], 0, "Amount ");
        osd_pct(tx->t[1], i, osd_v7pct(value));
        return;
    }
    uint32_t mode = slot < 10 ? slot : 0;
    if (ctrl == OSD_E_MODE) {                 /* "EQ Mode" / "Phaser" */
        osd_put(tx->t[0], 0, "EQ Mode");
        osd_put(tx->t[1], 0, chr_eq_names[mode]);
        return;
    }
    i = osd_put(tx->t[0], 0, "EQ  ");
    osd_put(tx->t[0], i, chr_eq_names[mode]);
    char *d = tx->t[1];
    switch (ctrl) {
    case OSD_E_A: case OSD_E_B: case OSD_E_C:
        i = osd_put(d, 0, chr_eq_labels[mode][ctrl - OSD_E_A]);
        i = osd_put(d, i, ctrl == OSD_E_C && mode == 5 ? "  " : " ");
        osd_eq_value(d, i, mode, ctrl - OSD_E_A, value);
        return;
    case OSD_E_PA: case OSD_E_PB: case OSD_E_PC: {
        const char *p = chr_eq_pushes[mode][ctrl - OSD_E_PA];
        if (!p) {
            char b[] = "Button A";
            b[7] = (char)('A' + (ctrl - OSD_E_PA));
            i = osd_put(d, 0, b);
            i = osd_put(d, i, "  ");
            osd_name(d, i, "N/A", 3);
            return;
        }
        i = osd_put(d, 0, p);
        i = osd_put(d, i, "  ");
        osd_onoff(d, i, value);
        return;
    }
    case OSD_E_BYP:
        i = osd_put(d, 0, "Bypass  ");
        osd_onoff(d, i, value);
        return;
    default:
        i = osd_put(d, 0, "Slope  ");
        if (!(CHR_EQ_SLOPE_MODES & (1u << mode))) { osd_name(d, i, "N/A", 5); return; }
        osd_put(d, i, value ? "24 dB" : "12 dB");
        return;
    }
}

/* ---- the compressor: hook D's store, exactly what hook E stamps ---------- */
static const float osd_comp_lo[5] = { -40.0f, 1.0f, 24.0f, 480.0f, -36.0f };
static const float osd_comp_hi[5] = { 0.0f, 20.0f, 4800.0f, 48000.0f, 36.0f };

static float osd_comp_value(uint32_t k, uint32_t v7)
{
    const volatile uint32_t *s = (const volatile uint32_t *)(uintptr_t)CHR_COMP_STORE;
    uint32_t w[7];
    for (int j = 0; j < 7; j++) w[j] = s[j];
    if (w[0] == CHR_COMP_MAGIC && w[6] == (w[0] ^ w[1] ^ w[2] ^ w[3] ^ w[4] ^ w[5])) {
        if (k == 2 || k == 3) return (float)w[k + 1];        /* samples, integer */
        union { uint32_t u; float f; } c = { w[k + 1] };
        return c.f;
    }
    float x = (float)(v7 << 7) * ((osd_comp_hi[k] - osd_comp_lo[k]) / 16383.0f) + osd_comp_lo[k];
    return (k == 2 || k == 3) ? (float)(uint32_t)x : x;
}

static int osd_auto_on(void)
{
    const volatile uint32_t *a = (const volatile uint32_t *)(uintptr_t)CHR_AUTO_ADDR;
    return (chr_features & 1u) && a[0] == CHR_AUTO_MAGIC && a[1] == ~CHR_AUTO_MAGIC;
}

static void osd_compose_comp(uint32_t ctrl, uint32_t value, osd_text_t *tx)
{
    char *d = tx->t[1];
    int i;
    osd_put(tx->t[0], 0, "COMPRESSOR");
    tx->n = 2;
    if (ctrl == 0) { osd_name(d, 0, value >= 64 ? "On" : "Off", 3); return; }
    if (ctrl == 6) { i = osd_put(d, 0, "Auto Makeup  "); osd_name(d, i, value >= 64 ? "On" : "Off", 3); return; }
    uint32_t k = (ctrl - 1u) & 7u;
    if (k > 4) k = 4;
    if (k == 4 && osd_auto_on()) { osd_put(d, 0, "Makeup  Auto"); return; }   /* CC 45 ignored */
    float v = osd_comp_value(k, value);
    i = osd_put(d, 0, chr_comp_names[k]);
    i = osd_put(d, i, " ");
    switch (k) {
    case 0: osd_db(d, i, v); return;                                  /* threshold */
    case 1: i = osd_fix(d, i, osd_round(v * 10.0f), 1, 4, 0); osd_put(d, i, ":1"); return;
    case 2: i = osd_fix(d, i, osd_round(v * (10.0f / 48.0f)), 1, 5, 0); osd_put(d, i, " ms"); return;  /* rc1: to 100.0 */
    case 3: {                                                         /* release: ms; s from 1000 */
        int32_t m = osd_round(v * (1.0f / 48.0f));
        if (m >= 1000) { i = osd_fix(d, i, osd_round(v * (1.0f / 4800.0f)), 1, 4, 0); osd_put(d, i, " s "); }
        else { i = osd_fix(d, i, m, 0, 4, 0); osd_put(d, i, " ms"); }
        return;
    }
    default: osd_db(d, i, v); return;                                 /* makeup, signed */
    }
}

/* ---- pad CCs: what the voice received, through hooks A/B's own
 * arithmetic. Labelled by MIDI CHANNEL: a channel can reach any number of
 * pads. The continuous ones in the module's own format for these parameters
 * (the screen's "%d.%01d%%" of the 0-1000 value): the jump arms deliver
 * (cc7 * 8063) >> 10; the sends, and Sustain on a Clip/Slicer voice, deliver
 * the float v14/16383, i.e. v14 * 1000 / 16383 in the same units. -------- */
static const char *const chr_pad_cont[11] = {
    "Density", "Grain Size", "Window", "Scatter", "Pan Rnd",    /* 79-83, Granular */
    "Delay Send", "Reverb Send", 0, "Sustain", 0, "Loop Crossfade",
};

static int32_t osd_jump1000(uint32_t v14) { return (int32_t)(((v14 >> 7) * 8063u) >> 10); }
static int32_t osd_float1000(uint32_t v14) { return (int32_t)((v14 * 1000u + 8191u) / 16383u); }
static int osd_permille(char *d, int i, int32_t p) { i = osd_fix(d, i, p, 1, 5, 0); return osd_put(d, i, "%"); }

static void osd_compose_pad(uint32_t chan, uint32_t cc, uint32_t cls, uint32_t v14, osd_text_t *tx)
{
    v14 &= 0x3fffu;
    uint32_t on = v14 >= 8192u;
    char *d = tx->t[1];
    int i = osd_put(tx->t[0], 0, "CH ");
    osd_fix(tx->t[0], i, (int32_t)(chan + 1u), 0, 0, 0);
    tx->n = 2;
    if (cc >= 79 && cc <= 89 && cc != 86 && cc != 88) {
        i = osd_put(d, 0, chr_pad_cont[cc - 79]);
        i = osd_put(d, i, " ");
        int floaty = cc == 84 || cc == 85 || (cc == 87 && cls == 2);
        osd_permille(d, i, floaty ? osd_float1000(v14) : osd_jump1000(v14));
        return;
    }
    switch (cc) {
    case 86: i = osd_put(d, 0, "Reverse  "); osd_onoff(d, i, on); break;
    case 88: i = osd_put(d, 0, "Loop Mode  "); osd_name(d, i, chr_loop_names[(v14 * 3u) >> 14], 13); break;
    case 90: i = osd_put(d, 0, "Play Through  "); osd_onoff(d, i, on); break;
    case 91: {
        const char *const *lab = (const char *const *)(uintptr_t)CHR_LFO_LABELS;
        i = osd_put(d, 0, "LFO Wave  ");
        osd_name(d, i, lab[(v14 * 10u) >> 14], 10);
        break;
    }
    case 92: i = osd_put(d, 0, "LFO Sync  "); osd_onoff(d, i, on); break;
    default: i = osd_put(d, 0, "LFO Retrigger  "); osd_onoff(d, i, on); break;
    }
}

static void osd_compose_dly(uint32_t cc, uint32_t value, osd_text_t *tx)
{
    osd_put(tx->t[0], 0, "DELAY");
    tx->n = 2;
    if (cc == 111) {                          /* hook C: (cc7 * 8063) >> 10 */
        int i = osd_put(tx->t[1], 0, "Filter Width ");
        osd_permille(tx->t[1], i, (int32_t)((value * 8063u) >> 10));
        return;
    }
    int i = osd_put(tx->t[1], 0, cc == 108 ? "Beat Sync  " : cc == 109 ? "Ping-Pong  " : "Filter  ");
    osd_onoff(tx->t[1], i, value);
}

/* The key a pop-up's "last shown" text is kept under: one per parameter. */
static uint32_t osd_key(uint32_t kind, uint32_t slot, uint32_t ctrl)
{
    switch (kind) {
    case OSD_CHR: return (slot & 3u) * 3u + (ctrl == OSD_C_AMOUNT ? 0u : ctrl == OSD_C_TONE ? 1u : 2u);
    case OSD_EQ: return 12u + (ctrl < 9u ? ctrl : 8u);
#ifdef CHR_CMP
    case OSD_COMP: if (ctrl >= 7u) return 272u;                             /* CC 47, 48 */
                   return ctrl == 6u ? 21u : 22u + (ctrl < 6u ? ctrl : 5u);   /* 21: CC 46 */
    case OSD_CLIP: return 273u + (slot & 15u);                               /* per pad position */
#else
    case OSD_COMP: return ctrl == 6u ? 21u : 22u + (ctrl < 6u ? ctrl : 5u);   /* 21: CC 46 */
#endif
    case OSD_DLY: return 28u + (ctrl >= 108u && ctrl <= 111u ? ctrl - 108u : 0u);
    default:                                  /* pads: 32 + channel * 15 + (cc - 79) */
        return 32u + (slot & 15u) * 15u + (ctrl >= 79u && ctrl <= 93u ? ctrl - 79u : 0u);
    }
}

/* ---- the clock: lost / taken over / a real tempo change -------------------- */
static int osd_clk_ok(uint32_t clk)
{
    return clk >= 0x24000000u && clk < 0x24080000u - 0x3000u && !(clk & 3u);
}

static int osd_bpm_ok(float t) { return t > 20.0f && t < 1000.0f; }   /* hook J's test */

static void osd_clock_text(osd_text_t *tx, const char *top, const char *what, uint32_t bpm)
{
    osd_put(tx->t[0], 0, top);
    int i = osd_put(tx->t[1], 0, what);
    i = osd_fix(tx->t[1], i, (int32_t)bpm, 0, 3, 0);
    osd_put(tx->t[1], i, " BPM");
    tx->n = 2;
}

/* Every UI tick. Fires (into *tx) only on:
 *  - a transition of the source byte from external to internal: CLOCK OFF,
 *    once, with the tempo the module now runs on (hook J's own rule);
 *  - a transition to external: CLOCK / Synced, once, when the incoming tempo
 *    has settled (0.5 s on one whole BPM), or after 2 s regardless;
 *  - while external, a NEW tempo: the whole BPM differs from the last shown,
 *    by at least 0.75 BPM, and has held for 0.5 s -- so a wobbling estimate
 *    never fires it.
 * The first tick, and any tick where the clock object moved, only takes a
 * baseline: nothing fires at boot. A preset load changes the tempo, not the
 * source, and while the module's own clock is in charge no tempo pop-up fires. */
static int osd_clock_watch(chr_osd_t *o, chr_osd2_t *x, uint32_t now, osd_text_t *tx)
{
    uint32_t clk = o->clk;
    if (!osd_clk_ok(clk)) return 0;
    uint32_t src = *(volatile uint8_t *)(uintptr_t)(clk + CHR_CLK_SOURCE);
    float live = *(volatile float *)(uintptr_t)(clk + CHR_CLK_LIVE);
    uint32_t r = osd_bpm_ok(live) ? (uint32_t)osd_round(live) : 0u;
    if (o->cmagic != CHR_OSD_CMAGIC || o->clk_was != clk) {
        o->cmagic = CHR_OSD_CMAGIC; o->clk_was = clk; o->clk_src = src;
        x->sync_pend = 0; x->tempo_shown = src ? r : 0u; x->cand = r; x->cand_since = now;
        return 0;
    }
    uint32_t was = o->clk_src != 0u, is = src != 0u;
    o->clk_src = src;
    if (was && !is) {
        float conf = *(volatile float *)(uintptr_t)(clk + CHR_CLK_CONF);
        int hold = *(volatile uint32_t *)CHR_CLK_MODE == CHR_CLK_MAGIC
                && *(volatile uint32_t *)CHR_CLK_HELD == CHR_CLK_MAGIC && osd_bpm_ok(live);
        float t = hold ? live : conf;
        x->sync_pend = 0; x->tempo_shown = 0;
        osd_clock_text(tx, "CLOCK OFF", hold ? "Holding " : "Internal ", osd_bpm_ok(t) ? (uint32_t)osd_round(t) : 0u);
        return 1;
    }
    if (!was && is) {
        x->sync_pend = 1; x->sync_since = now; x->cand = r; x->cand_since = now;
        return 0;
    }
    if (!is) return 0;
    if (r != x->cand) { x->cand = r; x->cand_since = now; }
    int steady = r != 0u && now - x->cand_since >= CHR_OSD_STEADY;
    if (x->sync_pend) {
        if (!steady && now - x->sync_since < CHR_OSD_SYNC_MAX) return 0;
        x->sync_pend = 0; x->tempo_shown = r;
        osd_clock_text(tx, "CLOCK", "Synced ", r);
        return 1;
    }
    if (!steady || r == x->tempo_shown) return 0;
    float dv = live - (float)x->tempo_shown;
    if (dv < 0.0f) dv = -dv;
    if (x->tempo_shown && dv < 0.75f) return 0;
    x->tempo_shown = r;
    osd_clock_text(tx, "CLOCK", "Tempo ", r);
    return 1;
}

/* ---- drawing ---------------------------------------------------------------- */
static int osd_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

typedef struct { uint16_t *p[2]; } osd_bufs_t;

/* The top layer's two buffers, or 0 if they are not the 240 x 320 ARGB1555
 * buffers this was traced against. Descriptor: +0 data, +4 w, +6 h. */
static int osd_buffers(uint8_t *display, osd_bufs_t *b)
{
    const uint32_t offs[2] = { CHR_DISPLAY_TOP_A, CHR_DISPLAY_TOP_B };
    for (int k = 0; k < 2; k++) {
        uint8_t *dsc = display + offs[k];
        uint32_t data = *(volatile uint32_t *)dsc;
        uint16_t w = *(volatile uint16_t *)(dsc + 4), h = *(volatile uint16_t *)(dsc + 6);
        if (!data || (data & 1u) || w != CHR_SCREEN_H || h != CHR_SCREEN_W) return 0;
        b->p[k] = (uint16_t *)(uintptr_t)data;
    }
    return 1;
}

/* landscape (X right, Y down) -> portrait buffer element X*240 + (239 - Y) */
#define OSD_AT(buf, X, Y) ((buf)[(uint32_t)(X) * CHR_SCREEN_H + (uint32_t)(CHR_SCREEN_H - 1 - (Y))])

/* THE FLICKER FIX (ch11). The stock UI, on any screen with an overlay, blits
 * its whole overlay canvas into the top layer's back buffer on every render
 * (0x0805b598 -> 0x08042bf8, rect 0,0,w,h) and swaps it in (0x080428bc). Up
 * to ch10 the OSD noticed the damage and REDREW: filled the box with the
 * background, then drew the glyphs -- in BOTH buffers, including the one the
 * LTDC was scanning out. Every render, the visible glyphs went to background
 * and back: constant flicker. Now the box is a one-bit mask built once per
 * text change, and every tick each box pixel is made what the mask says by
 * writing ONLY the pixels that differ. A pixel that is right is never
 * touched, a glyph pixel is never set to background on its way to white, and
 * the buffer on screen (which already holds the box) gets no writes at all.
 * At the end, only pixels that still hold our own colour go back to
 * transparent, so anything the stock UI drew there stays. */
static void osd_mask(chr_osd2_t *x, const char text[2][28])
{
    const uint8_t *font = (const uint8_t *)(uintptr_t)CHR_FONT;
    for (int c = 0; c < OSD_BOX_W; c++) x->col[c] = 0;
    for (int l = 0; l < 2; l++) {
        int n = osd_len(text[l]);
        if (n > OSD_COLS) n = OSD_COLS;
        int lx = OSD_PADX + (12 * (OSD_COLS - n)) / 2, ly = OSD_PADY + 16 * l;
        for (int i = 0; i < n; i++) {
            uint32_t ch = (uint8_t)text[l][i];
            if (ch < 0x20u || ch > 0x7fu) ch = '?';
            const uint8_t *g = font + (ch - 0x20u) * 8u;
            for (int gy = 0; gy < 8; gy++) {
                uint32_t row = g[gy];
                uint64_t bits = (uint64_t)3u << (ly + 2 * gy);
                for (int gx = 0; gx < 6; gx++)
                    if (row & (0x80u >> gx)) {
                        int c = lx + 12 * i + 2 * gx;
                        x->col[c] |= bits;
                        x->col[c + 1] |= bits;
                    }
            }
        }
    }
}

/* show = 1: every box pixel to the mask's colour, writing only those that
 * differ. show = 0: every box pixel that still holds our colour to
 * transparent. Returns the number of pixels written. */
static int osd_sync(chr_osd2_t *x, osd_bufs_t *b, int show)
{
    int writes = 0;
#ifdef CHR_CMP
    uint16_t fg, bg;                          /* b24: the theme's; a hide clears what was drawn */
    if (show) {
        fg = thm_osd_col(THO_BOX_FG, CHR_OSD_FG);
        bg = thm_osd_col(THO_BOX_BG, CHR_OSD_BG);
        x->pad[0] = (uint32_t)fg << 16 | bg;
    } else {
        if (!x->pad[0]) return 0;
        fg = (uint16_t)(x->pad[0] >> 16);
        bg = (uint16_t)x->pad[0];
    }
#else
    const uint16_t fg = CHR_OSD_FG, bg = CHR_OSD_BG;
#endif
    for (int k = 0; k < 2; k++)
        for (int c = 0; c < OSD_BOX_W; c++) {
            uint64_t m = x->col[c];
            int X = OSD_X0 + c, w0 = writes;
            for (int r = 0; r < OSD_BOX_H; r++, m >>= 1) {
                volatile uint16_t *p = &OSD_AT(b->p[k], X, OSD_Y0 + r);
                uint16_t want = (m & 1u) ? fg : bg, cur = *p;
                if (show ? cur != want : cur == want) { *p = show ? want : 0; writes++; }
            }
#ifndef CHR_HOST
            if (writes != w0) {               /* clean this column's lines to SDRAM */
                uintptr_t lo = (uintptr_t)&OSD_AT(b->p[k], X, OSD_Y0 + OSD_BOX_H - 1) & ~(uintptr_t)31;
                uintptr_t hi = (uintptr_t)&OSD_AT(b->p[k], X, OSD_Y0);
                for (uintptr_t a = lo; a <= hi; a += 32) *(volatile uint32_t *)CHR_DCCMVAC = (uint32_t)a;
            }
#endif
        }
#ifndef CHR_HOST
    if (writes) __asm__ volatile ("dsb" ::: "memory");
#endif
    return writes;
}

/* If the stock UI's blit into the top layer is still running, let it finish
 * first: otherwise it could land on top of what we just put right. Bounded;
 * only for the DMA2D this was traced against. */
static void osd_blit_wait(uint8_t *display)
{
#ifndef CHR_HOST
    uint32_t inst = *(volatile uint32_t *)(display + CHR_DISPLAY_DMA2D);
    if (inst != CHR_DMA2D) return;
    for (int n = 0; n < 200000 && (*(volatile uint32_t *)(uintptr_t)inst & 1u); n++) { }
#else
    (void)display;
#endif
}

static uint32_t osd_hash2(const char *a, const char *b)
{
    uint32_t h = 2166136261u;
    for (const char *p = a; ; p++) { h = (h ^ (uint8_t)*p) * 16777619u; if (!*p) break; }
    for (const char *p = b; ; p++) { h = (h ^ (uint8_t)*p) * 16777619u; if (!*p) break; }
    return h | 1u;                    /* never 0, which means "nothing shown yet" */
}

/* A new request, taken once: (seq even, different from the last taken). One
 * already older than the timeout when first seen (left in RAM across a warm
 * reset, say) is consumed without being shown. */
static int osd_take(volatile osd_req_t *r, uint32_t *done, uint32_t now, uint32_t *kind, uint32_t *slot,
                    uint32_t *ctrl, uint32_t *value, uint32_t *type)
{
    if (r->magic != CHR_OSD_RMAGIC) return 0;
    uint32_t s1 = r->seq;
    if ((s1 & 1u) || s1 == *done) return 0;  /* mid-write, or already taken */
    *kind = r->kind; *slot = r->slot; *ctrl = r->ctrl; *value = r->value; *type = r->type;
    uint32_t t0 = r->t0;
    if (r->seq != s1) return 0;
    *done = s1;
    return now - t0 <= CHR_OSD_CYCLES;
}

/* Offer a composed pop-up: shown only if its text differs from what was last
 * shown for that parameter (rule (a)); `show` is 0 while the OSD is Off, when
 * nothing is shown and so nothing is remembered as shown. */
static int osd_offer(chr_osd2_t *x, uint32_t key, osd_text_t *tx, osd_text_t *out, int show, uint32_t salt)
{
    if (tx->n != 2 || !show) return 0;
    uint32_t h = osd_hash2(tx->t[0], tx->t[1]) ^ (salt * 0x9e3779b1u);
    if (!h) h = 1u;
    if (key < OSD_KEYS) {
        if (x->last[key] == h) return 0;
        x->last[key] = h;
    }
    *out = *tx;
    return 1;
}

#ifdef CHR_CMP
static void cmp_ui_tick(uint32_t now);           /* cmp_engine.h */
static void cmp_osd_compose(uint32_t ctrl, uint32_t value, uint32_t type, osd_text_t *tx);
static void clip_osd_compose(uint32_t idx, uint32_t mode, uint32_t val, osd_text_t *tx);   /* clip_tempo.h */
static void thm_ui_tick(void);                   /* theme.h */
static void chs_ui_tick(uint32_t now);           /* chr_save.h */
#endif
#ifdef CHR_SC
static void clip_row_tick(void);                 /* sidechain.h: the Clip Tempo row (b21) */
#endif

/* UI side: hook O calls this every UI tick with r0 = the display object. */
void chr_osd_tick(uint8_t *display)
{
    chr_osd_t *o = osd_ram();
    chr_osd2_t *x = osd_ram2();
    osd_bufs_t b;
#ifdef CHR_CMP
    cmp_ui_tick(osd_now());                      /* the compressor Type's saved setting */
    chs_ui_tick(osd_now());                      /* the Character chain's (b24) */
    thm_ui_tick();                               /* the Theme row: the palette, the CLUT, a repaint */
#endif
#ifdef CHR_SC
    clip_row_tick();                             /* CC 112's Clip Tempo, into the pad's row */
#endif
    if (x->tmagic != CHR_OSD_TMAGIC) {          /* first tick after power-up */
        for (int k = 0; k < OSD_KEYS; k++) x->last[k] = 0;
        for (int c = 0; c < OSD_BOX_W; c++) x->col[c] = 0;
        x->tempo_shown = 0; x->sync_pend = 0; x->cand = 0;
        x->pad[0] = (uint32_t)CHR_OSD_FG << 16 | CHR_OSD_BG;   /* what a box is drawn in until one is */
        x->tmagic = CHR_OSD_TMAGIC;
    }
    if (o->umagic != CHR_OSD_UMAGIC) {          /* first tick after boot */
        o->umagic = CHR_OSD_UMAGIC; o->drawn = 0; o->shown = 0; o->cmagic = 0;
        o->active = 0; o->pend = 0;
        o->rq_done = 1u; o->prq_done = 1u;      /* odd: never a finished sequence */
    }
    uint32_t now = osd_now();
    int show = o->off != CHR_OSD_OFFMAGIC;
    osd_text_t nt, tx;
    int fresh = 0;
    uint32_t kind, slot, ctrl, value, type;

    if (osd_clock_watch(o, x, now, &tx) && show) { nt = tx; fresh = 1; }   /* always watched */
    if (osd_take(&o->rq, &o->rq_done, now, &kind, &slot, &ctrl, &value, &type)) {
        tx.t[0][0] = tx.t[1][0] = 0; tx.n = 0;
        switch (kind) {
        case OSD_CHR: case OSD_EQ: osd_compose_chr_eq(kind, slot, ctrl, value, type, &tx); break;
#ifdef CHR_CMP
        case OSD_COMP: cmp_osd_compose(ctrl, value, type, &tx); break;
        case OSD_CLIP: clip_osd_compose(slot, ctrl, value, &tx); break;
#else
        case OSD_COMP: osd_compose_comp(ctrl, value, &tx); break;
#endif
        case OSD_DLY: osd_compose_dly(ctrl, value, &tx); break;
        default: break;
        }
        uint32_t salt = 0;
#ifdef CHR_CMP
        /* b27: a line ending "N/A" never changes, so rule (a) showed it once
         * per power-on (the owner: Thunder's CC 42/43 showed no pop-up). Its
         * value counts as news: every turn shows; a resend does not. */
        int n = 0;
        while (n < 27 && tx.t[1][n]) n++;
        if (n >= 3 && tx.t[1][n - 3] == 'N' && tx.t[1][n - 2] == '/' && tx.t[1][n - 1] == 'A') salt = value + 1u;
#ifdef CHR_SC
        if (kind == OSD_CLIP && ctrl - CLIP_SHOW_JCV < 8u) salt = value + 1u;   /* b24: each turn of CC 117 shows */
#endif
#endif
        if (osd_offer(x, osd_key(kind, slot, ctrl), &tx, &nt, show, salt)) fresh = 1;
    }
    if (osd_take(&o->prq, &o->prq_done, now, &kind, &slot, &ctrl, &value, &type) && kind == OSD_PAD) {
        o->pend = 1; o->pend_since = now; o->pend_snap = type;
        o->pend_chan = (uint8_t)slot; o->pend_cc = (uint8_t)ctrl;
    }
    if (o->pend) {                              /* has a voice that supports it taken it? */
        uint32_t aseq = *(volatile uint32_t *)&o->acc_seq;
        uint32_t acc = *(volatile uint32_t *)&o->acc_cc;
        uint32_t v14 = *(volatile uint32_t *)&o->acc_val;
        if ((aseq & 0xffu) != o->pend_snap && (acc & 0xffu) == o->pend_cc) {
            o->pend = 0;
            tx.t[0][0] = tx.t[1][0] = 0;
            osd_compose_pad(o->pend_chan, o->pend_cc, (acc >> 8) & 3u, v14, &tx);
            if (osd_offer(x, osd_key(OSD_PAD, o->pend_chan, o->pend_cc), &tx, &nt, show, 0)) fresh = 1;
        } else if (now - o->pend_since > CHR_OSD_PAD_WAIT) {
            o->pend = 0;                        /* no pad took it: nothing to show */
        }
    }
    if (fresh) {                                /* the newest pop-up replaces the current one */
        for (int l = 0; l < 2; l++) {
            int i = 0;
            for (; nt.t[l][i] && i < OSD_COLS; i++) o->text[l][i] = nt.t[l][i];
            o->text[l][i] = 0;
        }
        osd_mask(x, o->text);
        o->active = 1; o->act_t0 = now;
    }
    if (o->active && now - o->act_t0 > CHR_OSD_CYCLES) o->active = 0;   /* retired, once */
    if (!show) o->active = 0;
    if (!o->active && !o->drawn) return;
    if (!osd_buffers(display, &b)) return;
    osd_blit_wait(display);
    if (o->active) { osd_sync(x, &b, 1); o->drawn = 1; }   /* every tick: repairs, never redraws */
    else { osd_sync(x, &b, 0); o->drawn = 0; }
}

#endif
