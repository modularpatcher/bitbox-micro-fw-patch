/*
 * clip_tempo.h -- 2.3.10's Clip Tempo (CHR_CMP builds only): a Clip pad fitted
 * to the tempo. Vinyl plays it at clock tempo / its own tempo, so its loop is
 * the right length for the bar and Clip Sync stops jumping; the pitch moves
 * with the speed, as on a turntable. Jungle fits it the way Akai's S-series
 * samplers stretched time -- two playheads at the clip's own pitch, crossfaded
 * every cycle, one repeating or skipping a little -- so the pitch stays put and
 * the Pitch knob and MIDI notes transpose it. Key Lock does the same smoothly:
 * before each join it searches for the place the two playheads' waveforms line
 * up, and crossfades there briefly, so the joins are hidden; it also looks
 * ahead for drum hits and moves each join so a hit plays once, intact. Manual makes any
 * of them follow the pad's own Manual tempo instead of the clock, free of the bar, until it is
 * let go, when the loop drops back in time.
 *
 * Entry points, each reached from a shim in hook N's 2.3.10 variant:
 *   clip_rate    the segment renderer's pitch factor (0x080b57f6), every block
 *   clip_read    the segment renderer's sample read (0x080b58fc): Jungle, Key Lock,
 *                and the Jungle Modes in any method
 *   clip_beats   the clip timing's Beat Count read (0x080b42ac)
 *   clip_cc      CC 104-119 on a pad's channel (hook B's no-match exit)
 *   clip_loaded  after every completed preset load (hook P's FinalizeBankLoading
 *                wrapper), on the UI thread
 *
 * Controls, all on the Clip pad's MIDI channel:
 *   CC 112  a press (any value above 0) steps Stock (Off) -> Vinyl -> Jungle -> Key Lock -> Stock
 *   CC 113  a press steps Manual on and off (from Off: Vinyl, Manual)
 *   CC 114  0-127 sets the pad's Manual tempo, 1-255 in steps of 2; on the clock it
 *           does nothing and says "Clock BPM  N/A" (b32). Preset BPM is the
 *           Settings row alone
 *   CC 115  0-127 sets Jungle's cycle, 20-2000 samples (exponential)
 *   CC 116  a press steps Jungle Modern <-> Old Skool
 *   CC 117  0-127 picks the Jungle Mode, 25 steps (value x 25 / 128)
 *   CC 118  Roll while held (64 and up), CC 119 Pitch Roll, CC 104 Freeze
 * Every pad has a hidden copy of the Clip voice, even a Sample pad, and it
 * hears the pad's CCs too; its setter keeps +0x29 at 1 only when the pad
 * really is a Clip or Slicer pad (0x080b748a), so any other pad shows "Clip
 * pads only" and is left alone.
 *
 * The rate. Stock reads the file at its own speed and holds each slice for its
 * musical length at the clock (out/slice = musical length; CLIP-STRETCH-
 * FEASIBILITY.md section 1.5), which is why it jumps. Vinyl multiplies the
 * read rate by m = F x BPM / (T x 3000 x sr): F the file's frames (pad+0x23c),
 * T its length in 960-per-beat ticks (pad+0xb90), sr the file's rate / 48000
 * (pad+0x1f0), BPM the clock's live tempo (Manual: the pad's Manual tempo,
 * CLIP_MAN_RAM). T x 3000 / BPM is the loop's musical length in output samples, so a
 * slice then reads exactly its own audio in exactly its hold. (Stock's clip
 * tempo at pad+0x258 assumes 48 kHz and would be wrong for 44.1 kHz files.)
 * In Jungle and Key Lock the segment keeps that rate as its position X --
 * stock's slicing, loop restarts and Vinyl's steering all run on it unchanged
 * -- and only the sound is made differently (clip_read).
 *
 * The beat count. With Beat Count on Auto stock guesses T again from the
 * CURRENT tempo whenever the timing is recomputed (every tempo change and loop
 * restart), rounded to a power of two -- so at half tempo an 8-beat loop is
 * taken for 4 beats and Vinyl plays it at its own speed. While a pad is in
 * Vinyl or Jungle, clip_beats answers that read with a count made once, by
 * stock's own rule, at the BPM the preset was saved with (read from the song
 * store when it loads, so neither the clock nor a CC 114 edit since can move
 * it), and kept until the file changes. A Beat Count set on the pad wins.
 *
 * The switch to Vinyl lands on the beat: every playing segment's read position
 * is moved to start + (pos - start) x new rate / old rate, the place its slice
 * would have reached at the new rate in the same musical time. In Vinyl the
 * speed is the tempo's alone (Pitch and note transposition are Jungle's), and
 * each slice is steered to end exactly when the next starts (clip_rate). In
 * Manual the reverse: stock's queued next slice is moved to when the playing
 * one arrives there, so the loop runs free of the clock's grid. Leaving Manual
 * starts the slice the clock's bar says should be playing, at that point in
 * it, and fades the old one out over stock's 256 samples (clip_drop_in).
 *
 * The mode is kept per pad position (pad+0x18, the message target id), so it
 * carries across preset loads when Keep CCs is ON and is cleared when it is
 * OFF. Power-on clears it (the RAM's magic and checksum fail).
 *
 * The Jungle Modes (2.3.10-b13) reshape how a clip plays -- re-sequenced,
 * reversed, harmonised, half-time -- on top of Vinyl, Jungle or Key Lock, and
 * the hold-buttons roll, pitch-roll or freeze it. None of them touches X: each
 * is a place to read from instead, worked out from X on the clip's own 16th
 * grid (clip_piece), and the method plays from there. Where that place jumps,
 * the old sound fades out over 256 samples while the new fades in (a raised
 * cosine, stock's own segment fade): its playheads are carried on, frozen,
 * in one of a few tails (clip_tail_t). Let go, the clip is back where X is.
 */
#ifndef CLIP_TEMPO_H
#define CLIP_TEMPO_H

#define CLIP_CC          112u
#define CLIP_CC_MANUAL   113u
#define CLIP_CC_BPM      114u
#define CLIP_CC_CYCLE    115u
#define CLIP_CC_ALGO     116u
#define CLIP_CC_FREEZE   104u         /* the Jungle Modes' hold-buttons and encoder */
#define CLIP_CC_REWIND   105u         /* Pull Up (b34; "Rewind" before) */
#define CLIP_CC_BRAKE    102u         /* b36: the other deck buttons, held as Pull Up is ... */
#define CLIP_CC_POWER    107u         /* ... (CLIP_B_REW, the kind in deck[]) */
#define CLIP_CC_SCRATCH  78u          /* below the range: nclip_cc lets it in alone */
#define CLIP_CC_JMODE    117u
#define CLIP_GATE_MARK   0x2au         /* b16: a CC the Gate hook sent carries this in its low 7 bits */
#define CLIP_CC_ROLL     118u
#define CLIP_CC_PROLL    119u
#define CLIP_CC_FIRST    102u         /* patch_micro.py: nclip_cc's range (b36: 104 before) */
#define CLIP_CC_LAST     119u
#define CLIP_RAM_ADDR    0x2406bb00u  /* patch RAM, the gap above the compressor types */
#define CLIP_RAM_BYTES   0x500u       /* to the compressor store at 0x2406c000 (patch_micro.py) */
#define CLIP_MAGIC       0x434c5033u  /* "CLP3" */
#define CLIP_KEEP_RAM    0x2406a400u  /* hook P's Keep CCs state: +0 is KEEP_ON when the row is ON */
#define CLIP_KEEP_ON     0x4b454550u
#define CLIP_PADS        16u
/* the mode byte: the method in its low bits, Manual as a flag */
#define CLIP_OFF         0u
#define CLIP_VINYL       1u
#define CLIP_JUNGLE      2u
#define CLIP_KEYLOCK     3u
#define CLIP_METHOD      3u
#define CLIP_MANUAL      4u
#define CLIP_MAN_RAM     0x2406a6b0u  /* (b27) patch RAM: float[CLIP_PADS], each pad's Manual tempo, then
                                         a magic word (patch_micro.py: CLIP_MAN_RAM) */
#define CLIP_MAN_MAGIC   0x434d414eu  /* "CMAN" */
#define CLIP_SHOW_MANNA  10u          /* (b30) CC 114 on a pad on the clock: "Clock BPM  N/A" (b31) */

#define CLIP_ELAST_RAM   0x2406c100u  /* (b28) patch RAM: float[CLIP_PADS][4], a held button's last e
                                         per segment (patch_micro.py: CLIP_ELAST_RAM) */
/* what a pop-up shows */
#define CLIP_SHOW_OFF    0u
#define CLIP_SHOW_ON     1u           /* the pad's mode, method and Manual */
#define CLIP_SHOW_ONLY   3u           /* not a Clip pad */
#define CLIP_SHOW_CYCLE  5u           /* Jungle's cycle */
#define CLIP_SHOW_ALGO   6u           /* Jungle's Modern / Old Skool */
#define CLIP_SHOW_BTN    9u           /* (b19) a hold-button: which, and pressed or let go */
#define CLIP_SHOW_JMODE  7u           /* the Jungle Mode */
#define CLIP_M_MIN       (1.0f / 256.0f)
#define CLIP_M_MAX       8.0f
#define CLIP_CYC_MIN     20u          /* Jungle's cycle, file frames */
#define CLIP_CYC_MAX     2000u
#define CLIP_CYC_DEF     1000u
#define CLIP_MODERN      0u           /* fractional cycles, equal-power fades */
#define CLIP_OLDSKOOL    1u           /* whole cycles, straight fades, the Akai step table */
/* Key Lock: a join every CLIP_KL_CYCLE output samples on sustained sound
 * (fewer at big stretches, so a join never moves the sound more than
 * CLIP_KL_SEP frames). While there are hits about -- for CLIP_KL_HOLD cycles
 * after the scan last found one -- joins come every CLIP_KL_HITCYC, closer at
 * big stretches so the sound strays no more than CLIP_KL_HITDEV frames from
 * the beat (but no closer than CLIP_KL_HITMINL). Each B is searched for within
 * +-CLIP_KL_NEAR frames of where Jungle would put it (+-CLIP_KL_HITNEAR among
 * hits, where timing matters more), and out to +-CLIP_KL_SEEK only when that
 * matches clearly better (CLIP_KL_FAR): a low note's cycle can be longer than
 * the near reach. Crossfades are CLIP_KL_FADE. Up to CLIP_KL_PLAN before each
 * join, the coming audio is scanned for hits and the join moved clear of them. */
#define CLIP_KL_CYCLE    2400.0f      /* 50 ms */
#define CLIP_KL_HITCYC   1200.0f      /* 25 ms */
#define CLIP_KL_HITDEV   384.0f       /* 8 ms */
#define CLIP_KL_HITMINL  768.0f
#define CLIP_KL_HOLD     12u          /* cycles: about a quarter of a second */
#define CLIP_KL_HITNEAR  128
#define CLIP_KL_FADE     384.0f
#define CLIP_KL_SEP      1400.0f
#define CLIP_KL_SEEK     512
#define CLIP_KL_NEAR     256
#define CLIP_KL_FAR      1.2f         /* the far reach must match this much better */
#define CLIP_KL_WIN      512
#define CLIP_KL_DEC      8            /* the coarse search: 8-frame sums, every 16th frame */
#define CLIP_KL_FINE     15           /* the fine one: +-15 frames around it ... */
#define CLIP_KL_FINEW    64           /* ... comparing 64 frames */
#define CLIP_KL_PLAN     640.0f       /* output samples a join may move for a hit (at most 0.4 of a cycle) */
#define CLIP_KL_PLANMIN  700.0f       /* cycles shorter than this are not planned */
#define CLIP_KL_ENV      16           /* frames per step of the hit scan's energy */
#define CLIP_KL_ENVN     256          /* steps: 4096 frames read at most */
#define CLIP_KL_HIST     24           /* steps of history a hit must stand out from */
#define CLIP_KL_HITX     3.0f         /* a hit: a step this many times the history's loudest */
#define CLIP_KL_HITMIN   0.0064f      /* and above about -40 dB */
#define CLIP_KL_GUARD    64.0f        /* frames kept clear either side of a hit */

/* the BPM a preset was saved with, read at its load (patch_micro.py: SONG_STORE) */
#define CLIP_SONG_STORE  0x24015f28u  /* the UI SessionMgr + 0x2278: the preset's song store */
#define CLIP_P_TEMPO     0x66u        /* globtempo: Preset BPM */

/* The Jungle Modes, in the encoder's order (CC 117: value x 25 / 128).
 * Time-based first, pitch-shifted towards the end, Half-Time last. */
#define CLIP_JMODES      25u
#define CLIP_JM_OFF      0u
#define CLIP_JM_ROTATE   1u           /* 1-8: re-sequenced on the 16th grid (clip_seqs) */
#define CLIP_JM_UKG      8u
#define CLIP_JM_SUBTLE   9u           /* 9, 10, 18: chops from the hit pool (b17) */
#define CLIP_JM_LIFT     10u
#define CLIP_JM_SWELL    11u
#define CLIP_JM_REVERB   12u          /* (b18) a reversed reverb laid over the groove */
#define CLIP_JM_MIXS     13u          /* 13-15: the mixes (b18) */
#define CLIP_JM_MIXG     14u
#define CLIP_JM_MIXW     15u
#define CLIP_JM_PROLL    16u
#define CLIP_JM_CALL     17u
#define CLIP_JM_STABS    18u
#define CLIP_JM_FIFTH    19u          /* 19-23: alternate grains pitched (clip_harm) */
#define CLIP_JM_OCT5     23u
#define CLIP_JM_HALF     24u
/* the hold-buttons and what goes with them, per pad position (btn) */
#define CLIP_B_ROLL      0x01u
#define CLIP_B_PROLL     0x02u
#define CLIP_B_FREEZE    0x04u
#define CLIP_B_REW       0x80u        /* a deck button: Pull Up (b17), Power Down, Brake, Baby Scratch (b36) */
#define CLIP_B_HELD      0x87u
#define CLIP_B_TAKE      0x08u        /* a button went down: the next read takes the press point */
#define CLIP_B_UP        0x10u        /* Pitch Roll rises on this press (each press turns it over) */
#define CLIP_B_NEW1      0x20u        /* the pad's newest segment is in playhead array 1 */
/* b36: which deck button CLIP_B_REW is, per pad position (deck[], a nibble each) */
#define CLIP_D_PULLUP    0u
#define CLIP_D_POWER     1u
#define CLIP_D_BRAKE     2u
#define CLIP_D_SCRATCH   3u
#define CLIP_B_PASS      0x40u        /* pass[] counts the loop's passes */
/* Every jump of the place the clip is read from is a join: the old sound
 * carries on, its playheads frozen at the gains they had, fading out over
 * CLIP_TAIL_N output samples while the new fades in, cos^2 / sin^2 (stock's
 * own segment fade is 256). A few tails are shared by every pad; with none
 * free a join is cut hard. */
#define CLIP_TAIL_N      256u
#define CLIP_TAILS       7u
typedef struct {
    float o[2];                      /* the old playheads, frames from X */
    float r[2];                      /* their rates */
    float g[2];                      /* and gains, as they were cut */
    uint16_t k;                      /* output samples into the fade */
    uint8_t own;                     /* 0x80 | playhead array << 4 | pad position; 0 = free */
    uint8_t stock;                   /* the old sound was read stock's way, not the modes' (clip_head_w) */
} clip_tail_t;
_Static_assert(sizeof(clip_tail_t) == 28, "clip_tail_t is 28 bytes");

typedef struct {
    uint32_t magic;
    uint32_t sum;                    /* magic ^ the four mode words */
    uint8_t mode[CLIP_PADS];         /* per pad position: method | CLIP_MANUAL */
    uint32_t clk;                    /* the clock object: hook B's r9 when a CC arrived */
    float show_clip[CLIP_PADS];      /* for the pop-up: the clip's own tempo ... */
    float show_bpm[CLIP_PADS];       /* ... and the tempo it plays at, when it was switched */
    float last_rate[CLIP_PADS][4];   /* a last slice's rate, fixed when it starts (per segment) */
    int32_t frz_f[CLIP_PADS];        /* the frames the held beat count was made for */
    uint16_t frz_b[CLIP_PADS];       /* the held beat count, 0 = none yet */
    uint8_t deck[CLIP_PADS / 2];     /* b36: each pad position's deck button, CLIP_D_* a nibble (b30-b32: unused) */
    float ref_bpm;                   /* the preset's saved BPM at its load: the beat counts' tempo */
    uint16_t jcyc[CLIP_PADS];        /* Jungle's cycle per pad position, file frames */
    uint8_t jalg[CLIP_PADS];         /* Jungle's algorithm per pad position: Modern / Old Skool */
    float jr[CLIP_PADS][4];          /* Jungle, Key Lock: the playheads' rate per segment, set by clip_rate */
    /* 2.3.10-b13, the Jungle Modes */
    uint8_t jmode[CLIP_PADS];        /* per pad position: CLIP_JM_* */
    uint8_t btn[CLIP_PADS];          /* CLIP_B_* */
    uint16_t pass[CLIP_PADS];        /* passes of the loop its newest segment is in */
    uint16_t plast[CLIP_PADS];       /* the 16th its newest segment was last at */
    uint16_t rpass[CLIP_PADS];       /* a button's press point: the pass ... */
    int32_t rx[CLIP_PADS];           /* ... and X there ... */
    int32_t rsrc[CLIP_PADS];         /* ... and where the clip was playing from */
    clip_tail_t tail[CLIP_TAILS];
    uint32_t hp;                     /* 2.3.10-b17: the hit pool's block in SDRAM (clip_hpb) */
} clip_ram_t;
_Static_assert(sizeof(clip_ram_t) <= CLIP_RAM_BYTES, "Clip Tempo has 0x500 bytes of patch RAM");
static uint32_t clip_deck(const clip_ram_t *c, int idx) { return (c->deck[idx >> 1] >> (4u * (idx & 1))) & 15u; }
static void clip_deck_set(clip_ram_t *c, int idx, uint32_t k)
{
    uint32_t sh = 4u * (uint32_t)(idx & 1);
    c->deck[idx >> 1] = (uint8_t)((c->deck[idx >> 1] & ~(15u << sh)) | (k & 15u) << sh);
}
/* the deck button a CC is, 15: none */
static uint32_t clip_deck_of(uint32_t cc)
{
    return cc == CLIP_CC_REWIND ? CLIP_D_PULLUP : cc == CLIP_CC_POWER ? CLIP_D_POWER
         : cc == CLIP_CC_BRAKE ? CLIP_D_BRAKE : cc == CLIP_CC_SCRATCH ? CLIP_D_SCRATCH : 15u;
}

/* 2.3.10-b17, the hit pool: where hits and syllables start in each Clip pad's
 * file, for the chop modes and Pitch Roll's snare. It is scanned in the audio
 * thread a little ahead of X (clip_hp_scan): a 4-band filterbank's level
 * rise, summed (spectral flux: syllables change the spectrum even where the
 * level barely moves), peaking well above its recent median, at least a 16th
 * apart. Until it holds a few, the clip's 16th grid stands in. It lives in
 * SDRAM, taken once from the firmware's allocator (clip_hp_alloc). */
#define CLIP_HP_N        64u          /* onsets per pad */
#define CLIP_HP_HOP      256          /* frames a scan step */
#define CLIP_HP_SUB      16           /* frames an onset is placed to */
#define CLIP_HP_MINN     4u           /* fewer than this: the 16th grid */
#define CLIP_HP_MAGIC    0x48495450u  /* "HITP" */
#define CLIP_HP_KEEP     0x100000u    /* SDRAM the pool must leave free */
#define CLIP_SD_CURSOR   0x24000088u  /* the allocator 0x08042dc0: its cursor ... */
#define CLIP_SD_LIMIT    0x2400008cu  /* ... and its limit, past which it halts the module */
typedef struct {
    double csrc;                     /* the chop playing: where it plays from ... */
    uint32_t ckey;                   /* ... and which it is (0: none) */
    int32_t on[CLIP_HP_N];           /* onsets, file frames */
    int32_t F;                       /* the file's frames they are for; 0: empty */
    int32_t cur;                     /* the next frame to scan */
    int32_t scanned;                 /* frames scanned since it (re)started */
    int32_t last;                    /* the last onset found; -1: none */
    int32_t cand[5];                 /* the last five steps' onset candidates ... */
    float flux[5];                   /* ... and their flux, oldest first */
    float hist[16];                  /* recent flux, for its median */
    float lp[3];                     /* the filterbank's one-pole states */
    float le[4];                     /* the last step's band levels (log) */
    float sub;                       /* its last 16 frames' energy */
    float fmax;                      /* the flux's decaying peak */
    float rend;                      /* Rewind let go: X frames from its press to the 16th it drops in on */
    float rl, rr;                    /* Rewind's low-pass */
    uint8_t n, hi, warm, rew;        /* onsets kept; hist's next; steps since a restart; Rewind 1: let go, 2: dropping in */
    double rvdone;                   /* Reverse Reverb: the last downbeat a swell was made for */
    float lpr[2];                    /* b19: the pitch each playhead array's last piece played at */
    double ue, up;                   /* b36, Power Down and Brake let go: the e the spin-up starts at (< 0: none) ... */
    float us;                        /* ... the place and the speed it starts from */
} clip_hp_t;

/* 2.3.10-b18, Reverse Reverb: the swell into a downbeat is the reverb's tail
 * after the downbeat's beat, played backwards, has gone in -- the tail
 * turned round, so it rises out of nothing into the hit. Made in the audio
 * thread the bar before (clip_rv_step), in one of two slots every pad shares:
 * a small Schroeder reverb (six damped combs, two allpasses) and the swell. */
#define CLIP_RV_SLOTS    2u
#define CLIP_RV_MAX      32768        /* samples: a swell, and the beat that makes it, at most */
#define CLIP_RV_STEP     512          /* reverb samples a block */
#define CLIP_RV_DL       (1557 + 1617 + 1491 + 1422 + 1277 + 1356 + 556 + 341)
typedef struct {
    double adb;                      /* the downbeat it rises into (A) */
    float dpk, ypk;                  /* the beat's peak and the tail's: the swell peaks as the hit does */
    float g[6], cs[6];               /* the combs' feedback (from the decay time) and damping states */
    int32_t D, N, k;                 /* the beat's samples, the swell's; done in this phase */
    int32_t ci[6], ai[2];            /* the delay lines' places */
    int8_t own;                      /* the pad position it is for, -1: free */
    uint8_t ph;                      /* 1: the beat going in backwards, 2: the tail coming out, 3: ready */
    uint8_t kind, spare;             /* 1: short (half a beat, 1 s), 2: medium (a beat, 1.8 s) */
    float dl[CLIP_RV_DL];
    float buf[CLIP_RV_MAX];          /* the swell, in time order, ending at the downbeat */
} clip_rv_t;

typedef struct {
    uint32_t magic, self, end, spare;    /* end: the allocator's cursor just after this block */
    clip_hp_t p[CLIP_PADS];
    clip_rv_t rv[CLIP_RV_SLOTS];
} clip_hpb_t;

/* Jungle's and Key Lock's playheads: two slots per pad, each serving one of its segments
 * (tag); slot 0 for every pad in one free gap of patch RAM, slot 1 in another
 * (patch_micro.py: JGL_RAM). Positions are offsets from X, the segment's own
 * position, in file frames: a playhead starts (v - r) x L behind or ahead of
 * X and lands on it as its fade-in ends, so the loop stays wherever X is --
 * in time. */
#define CLIP_JGL_ADDR0   0x2406a200u  /* above the OSD's UI-side area */
#define CLIP_JGL_ADDR1   0x2406ef00u  /* above the Character slots */
#define CLIP_JGL_SLOTS   2u
typedef struct {
    float off[2];                    /* the two playheads, frames from X (the read place: clip_read) */
    float t, L;                      /* output samples into the cycle; its length (one playhead: its rate) */
    int32_t xpos;                    /* where X will be at the next block: else start again */
    uint8_t tag;                     /* 0x80 | CLIP_T_ENG | piece class << 2 | the segment's slot; 0 is free */
    uint8_t a;                       /* the playhead fading out */
    uint8_t plan;                    /* Key Lock: this cycle's join has been placed for hits; harmony: each
                                        playhead's interval (2 bits each) */
    uint8_t hit;                     /* Key Lock: cycles left in hit mode (CLIP_KL_HOLD after a hit);
                                        harmony: the incoming playhead's interval */
} clip_jgl_t;
_Static_assert(sizeof(clip_jgl_t) == 24, "clip_jgl_t is 24 bytes");
#define CLIP_JGL_BYTES   (CLIP_PADS * 24u)   /* each of the two arrays */
#define CLIP_T_ENG       0x40u        /* Vinyl: the Jungle Modes play this segment (else stock reads it);
                                         Jungle, Key Lock: one has, so it reads their way (clip_head_w) */
/* where each segment reads from, frames from X, as its last block left it
 * (patch_micro.py: CLIP_DV_RAM): [playhead array][pad position] */
#define CLIP_DV_ADDR     0x2406a380u  /* between slot 0's playheads and Keep CCs */
#define CLIP_DV_BYTES    (2u * CLIP_PADS * 4u)
/* what a stretch of the clip is (clip_piece); moving from one to another is
 * a join, as is any jump in where it reads */
#define CLIP_K_PLAIN     0u           /* at X, or re-sequenced */
#define CLIP_K_ANSWER    1u           /* Call & Answer's answer, an octave up in time */
#define CLIP_K_HALF      2u
#define CLIP_K_FREEZE    3u
#define CLIP_K_REV       4u           /* Reverse Swell */
#define CLIP_K_ROLL      5u
#define CLIP_K_PROLL     6u
#define CLIP_K_HARM      7u           /* 7-11: the five intervals */
#define CLIP_K_SILENT    12u          /* (b17) a chop mode's gap */
#define CLIP_K_STAB      13u          /* (b17) Chop Stabs' root + fifth */
#define CLIP_K_REW       14u          /* (b17) Rewind */
#define CLIP_K_NONE      15u          /* a segment not yet played */
/* how it is played */
#define CLIP_E_FLAT      0u           /* one playhead at the place itself */
#define CLIP_E_JGL       1u           /* Jungle's cycles (Modern or Old Skool) */
#define CLIP_E_KL        2u           /* Key Lock */
#define CLIP_E_MOD       3u           /* Modern cycles whatever the method: harmony, Freeze, a Vinyl octave */
#define CLIP_JGL_MIN_L   32.0f        /* output samples: at most four cycles a block */
#define CLIP_JGL_MAX_N   128

#ifdef CHR_HOST
static void clip_x_timing(void *pad, uint32_t clk) { (void)pad; (void)clk; }
static int32_t clip_x_len(void *smp) { (void)smp; return 0; }
static int32_t clip_x_start(void *pad, void *note, int32_t delay, uint32_t k, uint32_t clk,
                            int32_t notenum, int32_t flag)
{ (void)pad; (void)note; (void)delay; (void)k; (void)clk; (void)notenum; (void)flag; return -1; }
static int32_t clip_x_getparam(void *store, uint32_t id) { (void)store; (void)id; return 0; }
static void *clip_x_alloc(uint32_t n) { (void)n; return 0; }
static uint32_t clip_x_sread(void *strm, uint32_t a1, uint32_t lo, uint32_t hi, uint32_t slot, uint32_t a5,
                             uint32_t a6, uint32_t n)
{ (void)strm; (void)a1; (void)lo; (void)hi; (void)slot; (void)a5; (void)a6; (void)n; return 1; }
static uint32_t clip_x_read(float rate, void *pad, void *a3, int32_t pos, int32_t poshi, int32_t n,
                            int32_t *adv, float *frac, void *src, void *dst, void **out)
{ (void)rate; (void)pad; (void)a3; (void)pos; (void)poshi; (void)n; (void)adv; (void)frac;
  (void)src; (void)dst; (void)out; return 0; }
#else
extern void clip_x_timing(void *pad, uint32_t clk);           /* 0x080b40fc: the clip timing */
extern int32_t clip_x_len(void *smp);                         /* 0x080a45ac: the file's frames */
extern int32_t clip_x_start(void *pad, void *note, int32_t delay, uint32_t k, uint32_t clk,
                            int32_t notenum, int32_t flag);   /* 0x080b490c: start slice k */
extern int32_t clip_x_getparam(void *store, uint32_t id);     /* 0x080894d0: GetParam */
extern void *clip_x_alloc(uint32_t n);                         /* 0x08042dc0: the SDRAM allocator */
extern uint32_t clip_x_sread(void *strm, uint32_t a1, uint32_t lo, uint32_t hi, uint32_t slot, uint32_t a5,
                             uint32_t a6, uint32_t n);        /* 0x0809a008: the streamer's read */
extern uint32_t clip_x_read(float rate, void *pad, void *a3, int32_t pos, int32_t poshi, int32_t n,
                            int32_t *adv, float *frac, void *src, void *dst,
                            void **out);                      /* 0x080b55ac: read and resample */
#endif

static clip_ram_t *clip_ram(void) { return (clip_ram_t *)(uintptr_t)CLIP_RAM_ADDR; }

static uint32_t clip_sum(const clip_ram_t *c)
{
    const uint32_t *w = (const uint32_t *)c->mode, *m = (const uint32_t *)c->jmode;
    return c->magic ^ w[0] ^ w[1] ^ w[2] ^ w[3] ^ m[0] ^ m[1] ^ m[2] ^ m[3];
}

static int clip_ok(const clip_ram_t *c) { return c->magic == CLIP_MAGIC && c->sum == clip_sum(c); }

static void clip_reset(clip_ram_t *c)
{
    for (uint32_t i = 0; i < CLIP_PADS; i++) {
        c->mode[i] = CLIP_OFF; c->show_clip[i] = 0.0f; c->show_bpm[i] = 0.0f;
        c->frz_f[i] = 0; c->frz_b[i] = 0;
        c->jcyc[i] = CLIP_CYC_DEF; c->jalg[i] = CLIP_MODERN;
        for (uint32_t j = 0; j < 4u; j++) c->last_rate[i][j] = 0.0f;
        c->jmode[i] = CLIP_JM_OFF; c->btn[i] = 0;
    }
    for (uint32_t i = 0; i < CLIP_TAILS; i++) c->tail[i].own = 0;
    c->clk = 0; c->ref_bpm = 0.0f;
    for (uint32_t i = 0; i < CLIP_PADS / 2u; i++) c->deck[i] = 0u;
    c->magic = CLIP_MAGIC;
    c->sum = clip_sum(c);
}

/* the pad's position, 0..15, from its message target id (row | column << 8);
 * -1 for anything else (an unplaced voice is 0xffff) */
static int clip_idx(const uint8_t *pad)
{
    uint32_t id = *(const uint32_t *)(pad + 0x18);
    if ((id & ~0x0303u) != 0u) return -1;
    return (int)((id & 3u) * 4u + ((id >> 8) & 3u));
}

/* a pointer into RAM the firmware allocates from: the internal SRAMs or the
 * external SDRAM at 0xC0000000 -- where, on the module, the pool of sample
 * objects lives (b9 accepted only the internal SRAMs, so on hardware every
 * clip read as 0 frames: Jungle was silent, b8's held beat count inert) */
static int clip_ram_ptr(uint32_t a)
{
    return !(a & 3u) && ((a >= 0x20000000u && a < 0x38000000u) || (a >= 0xc0000000u && a < 0xd0000000u));
}

/* The hit pool's block, if it is this boot's: taken from the allocator, whose
 * cursor has passed it -- after a restart that leaves patch RAM as it was, the
 * same address is the firmware's again until the pool is taken anew. */
static clip_hpb_t *clip_hpb(const clip_ram_t *c)
{
    uint32_t a = c->hp, e = a + (uint32_t)sizeof(clip_hpb_t);
    if (!clip_ram_ptr(a) || !clip_ram_ptr(e) || e < a) return 0;
    clip_hpb_t *b = (clip_hpb_t *)(uintptr_t)a;
    uint32_t cur = *(volatile const uint32_t *)(uintptr_t)CLIP_SD_CURSOR;
    return b->magic == CLIP_HP_MAGIC && b->self == a && b->end >= e && cur >= b->end ? b : 0;
}

static clip_hp_t *clip_hp(const clip_ram_t *c, int idx)
{
    clip_hpb_t *b = idx >= 0 && idx < (int)CLIP_PADS ? clip_hpb(c) : 0;
    return b ? &b->p[idx] : 0;
}

static void clip_hp_clear(clip_hp_t *h) { h->n = 0; h->F = 0; h->rew = 0; h->ckey = 0; h->rvdone = -1.0; }

/* UI side, at a preset load: take the pool once, if the allocator can spare it
 * with room left (past its limit it halts the module); empty every pad's */
static void clip_hp_alloc(clip_ram_t *c)
{
    clip_hpb_t *b = clip_hpb(c);
    if (!b) {
        uint32_t need = ((uint32_t)sizeof(clip_hpb_t) + 15u) & ~15u;
        uint32_t cur = *(volatile const uint32_t *)(uintptr_t)CLIP_SD_CURSOR;
        uint32_t lim = *(volatile const uint32_t *)(uintptr_t)CLIP_SD_LIMIT;
        c->hp = 0;
        if (lim <= cur || lim - cur < need + CLIP_HP_KEEP) return;
        uint32_t a = (uint32_t)(uintptr_t)clip_x_alloc(need);
        if (!clip_ram_ptr(a) || (a & 7u)) return;
        b = (clip_hpb_t *)(uintptr_t)a;
        b->magic = CLIP_HP_MAGIC;
        b->self = a;
        uint32_t now = *(volatile const uint32_t *)(uintptr_t)CLIP_SD_CURSOR;
        b->end = now > a ? now : a + need;
        c->hp = a;
    }
    for (uint32_t i = 0; i < CLIP_PADS; i++) clip_hp_clear(&b->p[i]);
    for (uint32_t i = 0; i < CLIP_RV_SLOTS; i++) b->rv[i].own = -1;
}

/* the file's frames, from the sample object (pad+0x24; none loaded: 0) */
static int32_t clip_frames(const uint8_t *pad)
{
    uint32_t smp = *(const uint32_t *)(pad + 0x24);
    if (!clip_ram_ptr(smp)) return 0;
    return clip_x_len((void *)(uintptr_t)smp);
}

/* a Clip pad: the voice is in use (+0x29) and not as a Slicer (+0x254) */
static int clip_is_clip(const uint8_t *pad) { return pad[0x29] != 0 && pad[0x254] == 0; }

static int clip_clk_ok(uint32_t clk) { return clk >= 0x24000000u && clk < 0x24080000u - 0x3000u && !(clk & 3u); }

static float clip_bpm_at(uint32_t clk, uint32_t off)
{
    if (!clip_clk_ok(clk)) return 0.0f;
    float b = *(volatile const float *)(uintptr_t)(clk + off);
    return (b >= 0.5f && b <= 1000.0f) ? b : 0.0f;      /* NaN fails too */
}
static float clip_bpm(uint32_t clk) { return clip_bpm_at(clk, 0x18u); }      /* the live tempo */
static float clip_preset(uint32_t clk) { return clip_bpm_at(clk, 0x13cu); }  /* Preset BPM */

/* b30, the owner: CC 113 toggles Clock <-> Manual, the stretch move; Manual
 * is the pad's own tempo, set by CC 114 and kept, so each push back into it
 * is the tempo last dialled (the first, the one playing then: no jump).
 * Preset BPM, the preset's own tempo, is the Settings row alone (b32; CC 9
 * in b30-b31) and no pad mode: on the module's own clock, the pads on Clock
 * follow it anyway. */
static float *clip_man(void) { return (float *)(uintptr_t)CLIP_MAN_RAM; }
static float clip_man_bpm(int idx)
{
    if (*(volatile const uint32_t *)(uintptr_t)(CLIP_MAN_RAM + 4u * CLIP_PADS) != CLIP_MAN_MAGIC) return 0.0f;
    float b = idx >= 0 && idx < CLIP_PADS ? clip_man()[idx] : 0.0f;
    return b >= 0.5f && b <= 1000.0f ? b : 0.0f;          /* NaN fails too */
}
static void clip_man_set(int idx, float b)
{
    volatile uint32_t *m = (volatile uint32_t *)(uintptr_t)(CLIP_MAN_RAM + 4u * CLIP_PADS);
    if (*m != CLIP_MAN_MAGIC) {                           /* power-on: no pad has one yet */
        for (int i = 0; i < CLIP_PADS; i++) clip_man()[i] = 0.0f;
        *m = CLIP_MAN_MAGIC;
    }
    if (idx >= 0 && idx < CLIP_PADS) clip_man()[idx] = b;
}

/* the tempo a pad in this mode plays to */
static float clip_mode_bpm(uint32_t clk, uint32_t mode, int idx)
{
    if (mode & CLIP_MANUAL) {
        float p = clip_man_bpm(idx);
        if (p > 0.0f) return p;
    }
    return clip_bpm(clk);
}

/* m = F x BPM / (T x 3000 x sr), or 1 while anything it needs is missing */
static float clip_mult(const uint8_t *pad_, float bpm)
{
    const uint8_t *pad = __builtin_assume_aligned(pad_, 4);  /* the voice and its fields are word-aligned */
    int64_t t; float sr; int32_t f;
    __builtin_memcpy(&t, pad + 0xb90, 8);
    __builtin_memcpy(&sr, pad + 0x1f0, 4);
    __builtin_memcpy(&f, pad + 0x23c, 4);
    if (!(bpm > 0.0f) || t <= 0 || t > 0x7fffffff || f <= 0 || !(sr > 0.0f && sr < 8.0f)) return 1.0f;
    float m = ((float)f * bpm) / ((float)(int32_t)t * 3000.0f * sr);   /* 32-bit: no runtime library */
    if (!(m >= CLIP_M_MIN)) m = CLIP_M_MIN;             /* NaN lands here too */
    if (m > CLIP_M_MAX) m = CLIP_M_MAX;
    return m;
}

static int clip_on(const clip_ram_t *c, const uint8_t *pad, int *idx)
{
    *idx = clip_idx(pad);
    return *idx >= 0 && (c->mode[*idx] & CLIP_METHOD) != CLIP_OFF && clip_is_clip(pad);
}

/* Jungle and Key Lock: two playheads at the clip's own pitch */
static int clip_heads(uint32_t mode) { return (mode & CLIP_METHOD) >= CLIP_JUNGLE; }

/* the Jungle Modes built so far; the others play as Off until their build */
static int clip_jm_built(uint32_t m)
{
    return m < CLIP_JMODES;
}

/* a Jungle Mode or a hold-button is playing pad position idx, or Rewind,
 * let go, is still on its way to the beat it drops in on */
static int clip_jm_live(const clip_ram_t *c, int idx)
{
    uint32_t m = c->jmode[idx];
    if ((m != CLIP_JM_OFF && clip_jm_built(m)) || (c->btn[idx] & CLIP_B_HELD)) return 1;
    const clip_hp_t *h = clip_hp(c, idx);
    return h && h->rew != 0u;
}

/* the tempo beat counts are made at: the preset's saved BPM, else Preset BPM
 * as the clock holds it, else the clock's tempo */
static float clip_ref(const clip_ram_t *c, uint32_t clk)
{
    if (c->ref_bpm >= 1.0f && c->ref_bpm <= 1000.0f) return c->ref_bpm;
    float r = clip_preset(clk);
    return r > 0.0f ? r : clip_bpm(clk);
}

/* Stock's own Auto guess (0x080b40fc), made at `bpm`: the file's length in
 * beats, rounded to the nearest multiple of the largest power of two beats it
 * holds. Returns beats, 0 if anything is missing. */
static uint32_t clip_guess(float sr, int32_t frames, float bpm)
{
    if (!(sr > 0.0f && sr < 8.0f) || frames <= 0 || !(bpm > 0.0f)) return 0u;
    float beats = (float)frames / (sr * 48000.0f) * (bpm / 60.0f);
    if (!(beats > 0.0f && beats < 60000.0f)) return 0u;
    int32_t ticks = (int32_t)(beats * 960.0f + 0.5f);
    int32_t n = (int32_t)((float)ticks / 960.0f + 0.5f);
    int32_t g = 1;
    if (n >= 2) while (g * 2 <= n) g *= 2;
    int32_t grid = g * 960, q = ticks / grid;
    int32_t lo = q * grid, hi = lo + grid;
    int32_t t = (ticks - lo < hi - ticks) ? lo : hi;   /* stock: a tie goes up */
    if (t <= 0) t = grid;
    return (uint32_t)(t / 960);
}

/* The clip timing's read of Beat Count (pad+0xb80, 0 = Auto), every time stock
 * recomputes the timing. clk is the clock (0x080b40fc's second argument). */
uint32_t clip_beats(uint8_t *pad, uint32_t clk)
{
    uint32_t b = *(const uint32_t *)(pad + 0xb80);
    clip_ram_t *c = clip_ram();
    int idx;
    if (b || !clip_ok(c) || !clip_on(c, pad, &idx)) return b;
    int32_t f = clip_frames(pad);
    if (f <= 0) return 0u;
    if (c->frz_b[idx] == 0u || c->frz_f[idx] != f) {
        float ref = clip_ref(c, clk);
        float sr;
        __builtin_memcpy(&sr, (const uint8_t *)__builtin_assume_aligned(pad, 4) + 0x1f0, 4);
        uint32_t g = clip_guess(sr, f, ref);
        if (g == 0u || g > 0xffffu) return 0u;
        c->frz_b[idx] = (uint16_t)g;
        c->frz_f[idx] = f;
    }
    return c->frz_b[idx];
}

/* The start of slice k, as the segment start (0x080b490c) places it. */
static int64_t clip_slice_start(const uint8_t *pad_, uint32_t k)
{
    const uint8_t *pad = __builtin_assume_aligned(pad_, 4);
    int64_t sync; uint32_t nmark, marks; float src;
    __builtin_memcpy(&sync, pad + 0x220, 8);
    __builtin_memcpy(&nmark, pad + 0x250, 4);
    __builtin_memcpy(&marks, pad + 0x24c, 4);
    __builtin_memcpy(&src, pad + 0x1fc, 4);
    if (pad[0x1f5] && sync == 0xf0000 && nmark != 0u) {          /* Loop sync with slice markers */
        if (k >= nmark || marks < 0x20000000u || marks >= 0xd0000000u || (marks & 3u)) return -1;
        return *(const int32_t *)(uintptr_t)(marks + 4u * k);
    }
    return (int64_t)(int32_t)((float)k * src);
}

/* the note (pad+0x260 or +0x670) whose segment list (+0xc8, count +0xd8)
 * holds segment slot si, else 0 */
static const uint8_t *clip_note_of(const uint8_t *p, uint32_t si)
{
    for (uint32_t n = 0; n < 2u; n++) {
        const uint8_t *nt = __builtin_assume_aligned(p + (n ? 0x670u : 0x260u), 4);
        int32_t cnt, l[4];
        __builtin_memcpy(&cnt, nt + 0xd8, 4);
        __builtin_memcpy(l, nt + 0xc8, 16);
        if (cnt < 0 || cnt > 4) continue;
        for (int32_t j = 0; j < cnt; j++)
            if ((uint32_t)l[j] == si) return nt;
    }
    return 0;
}

/* the queued, not yet started segment of slice k (stock queues the next one
 * early, grid-rounded); its countdown runs from the start of this block,
 * because segments are appended and the playing one renders first. Only one
 * of seg's own note: stock queues the next slice and the loop's restart in
 * the note that plays it (0x080b5f86), and a segment of the other note -- a
 * retrigger waiting for its launch quantise -- is not ours to move. */
static uint8_t *clip_queued(const uint8_t *p, const uint8_t *seg, uint32_t k)
{
    const uint8_t *own = clip_note_of(p, (uint32_t)(seg - (p + 0x30u)) / 0x70u);
    for (uint32_t i = 0; i < 4u; i++) {
        uint8_t *q = __builtin_assume_aligned((uint8_t *)p + 0x30u + i * 0x70u, 4);
        int32_t d; uint32_t qk;
        __builtin_memcpy(&d, q + 0x20, 4);
        __builtin_memcpy(&qk, q + 0x34, 4);
        if (q != seg && q[0] && !q[1] && d > 0 && qk == k && (!own || clip_note_of(p, i) == own)) return q;
    }
    return 0;
}

/* Manual: stock queued the next slice on the clock's grid; move it to when
 * this one arrives there at the free-running rate, and end this one's hold
 * then, so the loop runs free of the clock. */
static void clip_free(const uint8_t *p, uint8_t *seg, float rate)
{
    int64_t end, pos; int32_t fi, tail; uint32_t k; float frac;
    __builtin_memcpy(&end, seg + 0x10, 8);
    __builtin_memcpy(&pos, seg + 8, 8);
    __builtin_memcpy(&frac, seg + 0x1c, 4);
    __builtin_memcpy(&fi, seg + 0x24, 4);
    __builtin_memcpy(&tail, seg + 0x30, 4);
    __builtin_memcpy(&k, seg + 0x34, 4);
    if (fi < 0) fi = 0;
    if (end != 0) {
        /* the loop's last slice: stock sets its hold from this rate itself, and
         * queues the restart 256 samples before it ends -- rounded to the grid.
         * Launch Mode Trigger (+0x1f8 == 1) plays the clip once and queues no
         * restart (0x080b5f44, 0x080b618e): a slice 0 waiting then is the next
         * note's launch, and moving it to this one's end silenced the pad. */
        if (p[0x1f8] == 1u) return;
        uint8_t *q = clip_queued(p, seg, 0u);
        int64_t left = end - pos;
        if (!q || left <= 0 || left > 0x7fffffff) return;
        float w = ((float)(int32_t)left - frac) / rate + (float)tail - 256.0f;
        int32_t d = w > 1.0f ? (w < 2.0e9f ? (int32_t)(w + 0.5f) : 0x7fffffff) : 1;
        __builtin_memcpy(q + 0x20, &d, 4);
        return;
    }
    uint8_t *q = clip_queued(p, seg, k + 1u);
    int64_t left = clip_slice_start(p, k + 1u) - pos;
    if (!q || left <= 0 || left > 0x7fffffff) return;
    float w = ((float)(int32_t)left - frac) / rate;
    int32_t d = w > 1.0f ? (w < 2.0e9f ? (int32_t)(w + 0.5f) : 0x7fffffff) : 1;
    int32_t h = d - fi;
    if (h < 0) h = 0;
    __builtin_memcpy(q + 0x20, &d, 4);
    __builtin_memcpy(seg + 0x28, &h, 4);
}

/* Jungle's playheads for segment slot si of pad position idx: the pair's slot
 * that serves it, else (make) a free one -- free, or serving a segment that
 * no longer plays. None left: 0, and that segment plays as Vinyl. */
static clip_jgl_t *clip_jgl(const uint8_t *pad, int idx, uint32_t si, int make)
{
    clip_jgl_t *j[CLIP_JGL_SLOTS] = {
        (clip_jgl_t *)(uintptr_t)CLIP_JGL_ADDR0 + idx,
        (clip_jgl_t *)(uintptr_t)CLIP_JGL_ADDR1 + idx,
    };
    for (uint32_t k = 0; k < CLIP_JGL_SLOTS; k++)
        if ((j[k]->tag & 0x83u) == (0x80u | si)) return j[k];
    if (!make) return 0;
    for (uint32_t k = 0; k < CLIP_JGL_SLOTS; k++) {
        uint32_t t = j[k]->tag;
        const uint8_t *s = pad + 0x30u + (t & 3u) * 0x70u;
        if (!(t & 0x80u) || !s[0] || s[1]) {
            j[k]->tag = (uint8_t)(0x80u | CLIP_K_NONE << 2 | si);
            j[k]->xpos = -1;                         /* start afresh at the first read */
            return j[k];
        }
    }
    return 0;
}

/* The segment renderer's pitch factor, every block, for every live segment of
 * every Clip/Slicer pad (seg = the segment, its base rate at +0x18 = the file's
 * rate ratio x the MIDI note's). In Vinyl the speed is the tempo's alone. In
 * Jungle and Key Lock the segment's position runs at that same rate and the playheads at
 * the stock rate -- Pitch knob, modulation and note, pf x base -- which is
 * left for clip_read. Not in Manual, while a slice is in its fade-in or hold
 * the rate is steered to arrive where the next segment starts at the moment
 * it starts, so slices meet whatever the clock does (ramps, an external
 * clock's jitter, a hold set at an older tempo). The loop's last slice, whose
 * hold stock recomputes from this rate every block (and its restart from
 * that), and the fade-outs run at the steady rate m. */
float clip_rate(const uint8_t *pad, uint8_t *seg_, float pf)
{
    const clip_ram_t *c = clip_ram();
    int idx;
    if (c->magic != CLIP_MAGIC || !clip_on(c, pad, &idx)) return pf;
    uint8_t *seg = __builtin_assume_aligned(seg_, 4);
    const uint8_t *p = __builtin_assume_aligned(pad, 4);
    float base, sr;
    __builtin_memcpy(&base, seg + 0x18, 4);
    __builtin_memcpy(&sr, p + 0x1f0, 4);
    if (!(base > 0.0f && base < 1024.0f) || !(sr > 0.0f && sr < 8.0f)) return pf;
    uint32_t mode = c->mode[idx];
    if (!clip_heads(mode) && clip_jm_live(c, idx)) {          /* Vinyl, a Jungle Mode on: a playhead pair */
        uint32_t si = (uint32_t)(seg - (p + 0x30u)) / 0x70u;
        if (si < 4u) clip_jgl(p, idx, si, 1);
    } else if (clip_heads(mode)) {
        uint32_t si = (uint32_t)(seg - (p + 0x30u)) / 0x70u;
        if (si < 4u && clip_jgl(p, idx, si, 1)) {
            float r = pf * base;
            if (!(r >= CLIP_M_MIN)) r = CLIP_M_MIN;
            if (r > CLIP_M_MAX) r = CLIP_M_MAX;
            clip_ram()->jr[idx][si] = r;
        }
    }
    float steady = clip_mult(pad, clip_mode_bpm(c->clk, mode, idx)) * sr;   /* total rate, no Pitch, no note */
    if (mode & CLIP_MANUAL) {
        clip_free(p, seg, steady);
        return steady / base;
    }
    float rate = steady;
    int64_t end, pos; int32_t fi, hold; uint32_t k; float frac;
    __builtin_memcpy(&end, seg + 0x10, 8);
    __builtin_memcpy(&fi, seg + 0x24, 4);
    __builtin_memcpy(&hold, seg + 0x28, 4);
    __builtin_memcpy(&k, seg + 0x34, 4);
    __builtin_memcpy(&pos, seg + 8, 8);
    __builtin_memcpy(&frac, seg + 0x1c, 4);
    /* Only a slice with a next one is steered. The loop's last slice runs at
     * the steady rate it had when it started: stock recomputes its hold from
     * the rate every block and, when that moves by more than 4 samples,
     * cancels and requeues the restart from it (0x080b5864, 5aa8's
     * local_64), so a rate that followed a ramp, or steering, would keep
     * moving the restart -- a block of silence there under a rising tempo.
     * The next pass picks the new tempo up and is steered back onto the grid. */
    if (end != 0) {
        uint32_t si = (uint32_t)(seg - (p + 0x30u)) / 0x70u;
        float *lr = &clip_ram()->last_rate[idx][si & 3u];
        if (fi >= 256 || !(*lr > 0.5f * steady && *lr < 2.0f * steady)) *lr = steady;   /* its first block */
        return *lr / base;
    }
    if (hold > 0) {
        const uint8_t *q = clip_queued(p, seg, k + 1u);
        int32_t wait = 0;
        if (q) __builtin_memcpy(&wait, q + 0x20, 4);
        int64_t left = clip_slice_start(pad, k + 1u) - pos;
        int32_t t = wait > 0 ? wait : hold + (fi > 0 ? fi : 0);   /* there as the next one starts */
        if (left > 0 && left < 0x7fffffff) {
            float want = ((float)(int32_t)left - frac) / (float)t;
            if (want > 0.5f * steady && want < 2.0f * steady) {
                rate = want;
                /* and fade out exactly as it starts: stock fixed this hold when
                 * the slice began, and grid-rounded the next start later, so
                 * under a changing tempo the two drift apart (a gap of silence
                 * or a doubled overlap). Stock never recomputes it for a slice
                 * with a next one. */
                if (wait > 0) {
                    int32_t h = wait - (fi > 0 ? fi : 0);
                    if (h < 0) h = 0;
                    __builtin_memcpy(seg + 0x28, &h, 4);
                }
            }
        }
    }
    return rate / base;
}

/* ---- Jungle: the sound (clip_read) ------------------------------------------
 * Each cycle the output crossfades from the playhead fading out (A) to the one
 * fading in (B); at the cycle's end B carries on as the next A and A restarts
 * as the next B. Both read at the stock rate r. B starts (v - r) x L frames
 * from X -- behind it when the clip is stretched longer, so a little audio
 * repeats; ahead when shorter, so a little is skipped -- and, moving r a
 * sample against X's v, it is exactly on X when its fade-in ends: the loop is
 * where Vinyl would have it, in time, whatever the tempo does.
 *
 * The cycle, from the stretch s = r / v and the pad's cycle C (file frames),
 * in playhead frames: Modern, P = C / |s - 1| and L = P x s, fractional, its
 * boundaries carried from cycle to cycle; equal-power fades. Old Skool, whole
 * frames: stretched longer, L = P + C with P from the Akai samplers' own step
 * table for 1.2 <= s <= 20 (C / (s - 1) otherwise); shorter, L = P x s
 * truncated; straight-line fades. Output samples = L / r, at least
 * CLIP_JGL_MIN_L. Unstretched (s within 1e-4 of 1): one playhead, no cycles. */
static float clip_jgl_cycle(const clip_ram_t *c, int idx, float r, float v, int *flat)
{
    float s = r / v, d = s - 1.0f;
    float C = (float)c->jcyc[idx];
    if (!(C >= (float)CLIP_CYC_MIN && C <= (float)CLIP_CYC_MAX)) C = (float)CLIP_CYC_DEF;
    *flat = !(d > 1e-4f || d < -1e-4f);
    if (*flat) return 1.0e9f;
    float Lh;
    if (c->jalg[idx] == CLIP_OLDSKOOL) {
        if (d > 0.0f) {
            float P;
            if (s >= 1.2f && s <= 20.0f) {
                int32_t pct = (int32_t)(100.0f * s + 0.5f);
                int32_t q = 25600 / (pct - 100);
                int32_t T = (int32_t)(7.8125f * (float)q);
                float frac4 = (float)(int32_t)((float)(T - T / 100) / 1980.0f * 10000.0f + 0.5f) / 10000.0f;
                P = (float)(int32_t)(frac4 * C);
            } else {
                P = (float)(int32_t)(C / d);
            }
            Lh = P + C;
        } else {
            Lh = (float)(int32_t)(C / -d * s);
        }
    } else {
        Lh = C / (d > 0.0f ? d : -d) * s;
    }
    float L = Lh / r;
    if (!(L >= CLIP_JGL_MIN_L)) L = CLIP_JGL_MIN_L;
    if (L > 1.0e9f) L = 1.0e9f;
    return L;
}

typedef struct { int32_t cap, frames; float *l, *r; uint8_t f0, f1, f2, f3; uint32_t spare; } clip_desc_t;

/* One playhead's `len` frames at file position p (frames, from 0) and rate r,
 * through stock's own read and resampler, into (l, r_) -- copied there when
 * stock answers from its source buffer (rate 1, no fraction). */
/* 2.3.10-b19: meant to catch a read that found no block -- and wrong, found
 * in b22. The streamer's vtable starts at 0x080ca5fc (0x080ca5f4/f8 are its
 * offset and type words), so 0x080ca600 is its +4, 0x0809a008, which copies
 * the caller's audio INTO the cache: a write. The read is +8 (0x080ca604 ->
 * 0x0809afc0), and on a block it lacks it fills the caller's buffer with
 * silence, counts the miss (streamer +0x9700) and asks the loader for it;
 * the segment read (0x080b55ac) ignores its result and plays the buffer. So
 * a miss was always silence, never stale audio, and this wrapper has never
 * fired (b15's counters, the same entry, read 0 on the module). Left in
 * place, inert: the flag rises only if a write fails, outside any read. */
#define CLIP_MISS_ADDR   0x2406a640u  /* patch RAM: one word (patch_micro.py: CLIP_MISS_RAM) */

uint32_t clip_sread(void *strm, uint32_t a1, uint32_t lo, uint32_t hi, uint32_t slot, uint32_t a5, uint32_t a6,
                    uint32_t n)
{
    uint32_t got = clip_x_sread(strm, a1, lo, hi, slot, a5, a6, n);
    if (!got) *(volatile uint32_t *)(uintptr_t)CLIP_MISS_ADDR = 1u;
    return got;
}

static void clip_jgl_head(void *pad, void *a3, double p, float r, int32_t len, void *src,
                          float *l, float *rr, int stereo, int32_t frames)
{
    if (p < 0.0) p = 0.0;
    int32_t ip = (int32_t)p;
    float fr = (float)(p - (double)ip);
    if (ip >= frames) { for (int32_t i = 0; i < len; i++) { l[i] = 0.0f; if (stereo) rr[i] = 0.0f; } return; }
    clip_desc_t d = { len, len, l, rr, 0, 1, 0, 0, 0 };
    int32_t adv = 0;
    void *out = &d;
    volatile uint32_t *miss = (volatile uint32_t *)(uintptr_t)CLIP_MISS_ADDR;
    *miss = 0u;
    if (!clip_x_read(r, pad, a3, ip, 0, len, &adv, &fr, src, &d, &out) || *miss) {
        for (int32_t i = 0; i < len; i++) { l[i] = 0.0f; if (stereo) rr[i] = 0.0f; }
        return;
    }
    if (out != (void *)&d) {
        const clip_desc_t *o = out;
        for (int32_t i = 0; i < len; i++) { l[i] = o->l[i]; if (stereo) rr[i] = o->r[i]; }
    }
}

/* ---- Key Lock ----------------------------------------------------------------
 * Jungle's two playheads, with the joins hidden: B's start is moved, within
 * reach of where Jungle would put it, to where its waveform best matches A's
 * from now on (the correlation over CLIP_KL_WIN frames, scaled by B's energy),
 * and the crossfade is a short raised cosine, after which B plays alone. B
 * still lands within that reach of X, so the loop stays in time (about 5 ms;
 * 10 for a low note), and the error never builds up: every B is placed from X.
 *
 * Drum hits. A join's crossfade over a hit doubles it; so does a join just
 * after one while slowing down (B, behind, plays the hit again), and one just
 * before one while speeding up skips it. So before each join the sounding
 * playhead's coming audio is scanned for hits, and the join moved, by up to
 * CLIP_KL_PLAN, to the nearest moment that keeps every hit clear. Timing: the
 * sounding playhead strays from X by up to the cycle's length x |r - v| (B
 * starts that far behind or ahead and closes on X), so while there are hits
 * about the cycles are short enough to keep that within CLIP_KL_HITDEV, and
 * the search's reach is narrower. The scan and the search fall in different
 * blocks. */
static float clip_kl_cycle(float r, float v, int hit, int *flat)
{
    float d = r / v - 1.0f, g = v - r;
    *flat = !(d > 1e-4f || d < -1e-4f);
    if (*flat) return 1.0e9f;
    if (g < 0.0f) g = -g;
    float L = hit ? CLIP_KL_HITCYC : CLIP_KL_CYCLE, cap = hit ? CLIP_KL_HITDEV : CLIP_KL_SEP;
    if (g * L > cap) L = cap / g;
    if (hit && L < CLIP_KL_HITMINL) L = CLIP_KL_HITMINL;
    if (!(L >= CLIP_JGL_MIN_L)) L = CLIP_JGL_MIN_L;
    return L;
}

/* the fade, in output samples: CLIP_KL_FADE, or half a short cycle */
static float clip_kl_fade(float L) { return L < 2.0f * CLIP_KL_FADE ? 0.5f * L : CLIP_KL_FADE; }

/* how far a cycle's join may move for a hit */
static float clip_kl_move(float L) { return L * 0.4f < CLIP_KL_PLAN ? L * 0.4f : CLIP_KL_PLAN; }

/* `len` frames of the file from frame p at its own speed, summed to mono and
 * in sums of `dec` frames (len a multiple of it), in chunks through the
 * playhead read (l, rr: CLIP_JGL_MAX_N frames of scratch). A whole frame and
 * rate 1, so stock hands its source buffer over without resampling (about 75
 * instructions a frame otherwise). */
static void clip_kl_mono(void *pad, void *a3, int32_t p, int32_t len, int32_t dec, void *src, float *l, float *rr,
                         int stereo, int32_t frames, float *out)
{
    for (int32_t i = 0; i < len / dec; i++) out[i] = 0.0f;
    for (int32_t k = 0; k < len; k += CLIP_JGL_MAX_N) {
        int32_t n = len - k < CLIP_JGL_MAX_N ? len - k : CLIP_JGL_MAX_N;
        clip_jgl_head(pad, a3, (double)(p + k), 1.0f, n, src, l, rr, stereo, frames);
        for (int32_t i = 0; i < n; i++) out[(k + i) / dec] += stereo ? l[i] + rr[i] : l[i];
    }
}

/* q[k] for k in [0, n): c x |c| / e, where c is A's correlation with B shifted
 * by k x step and e is B's energy there */
static void clip_kl_score(const float *a, const float *b, int32_t w, int32_t n, int32_t step, float *q)
{
    float e = 0.0f;
    for (int32_t i = 0; i < w; i++) e += b[i] * b[i];
    for (int32_t k = 0; k < n; k++) {
        const float *bk = b + k * step;
        if (k) {                                     /* slide the energy along */
            for (int32_t i = 0; i < step; i++) {
                e -= bk[i - step] * bk[i - step];
                e += bk[w - step + i] * bk[w - step + i];
            }
        }
        float c = 0.0f;
        for (int32_t i = 0; i < w; i++) c += a[i] * bk[i];
        q[k] = c * (c < 0.0f ? -c : c) / (e + 1e-9f);
    }
}

/* How far to move B, starting at xb, so it continues A (at xa) most alike:
 * a coarse search over every 16th frame of 8-frame sums -- the near reach
 * unless the far one matches clearly better (among hits, a narrower near reach
 * and never the far) -- then a fine one at every frame.
 * Both are read from whole frames; the answer keeps B's distance from A as
 * found, fractions included. */
__attribute__((noinline)) static float clip_kl_seek(void *pad, void *a3, double xa, double xb, void *src, float *l, float *rr,
                          int stereo, int32_t frames, int hits)
{
    enum { W = CLIP_KL_WIN, S = CLIP_KL_SEEK, D = CLIP_KL_DEC, F = CLIP_KL_FINE, NF = CLIP_KL_FINEW, K = S / D + 1 };
    const int32_t Q = hits ? CLIP_KL_HITNEAR : CLIP_KL_NEAR;   /* among hits: near only */
    _Static_assert(W % CLIP_JGL_MAX_N == 0 && (2 * S) % CLIP_JGL_MAX_N == 0
                   && NF + 2 * F + 2 <= (W + 2 * S) / D && F >= 2 * D - 1,
                   "Key Lock's search fits its buffers and its fine step covers the coarse");
    double xb0 = xb;                                 /* near the file's start: searched from S */
    if (xa < 0.0) xa = 0.0;
    if (xb < (double)S) xb = (double)S;
    int32_t ia = (int32_t)xa, ib = (int32_t)xb;
    float ad[W / D > NF ? W / D : NF], bd[(W + 2 * S) / D + 1], q[K];
    clip_kl_mono(pad, a3, ia, W, D, src, l, rr, stereo, frames, ad);
    clip_kl_mono(pad, a3, ib - S, W + 2 * S, D, src, l, rr, stereo, frames, bd);
    bd[(W + 2 * S) / D] = 0.0f;
    clip_kl_score(ad, bd, W / D, K, 2, q);           /* candidate k: B moved 2D x k - S */
    int32_t kb = 0, kn = -1;
    for (int32_t k = 0; k < K; k++) {
        int32_t o = 2 * D * k - S;
        if (q[k] > q[kb]) kb = k;
        if (o >= -Q && o <= Q && (kn < 0 || q[k] > q[kn])) kn = k;
    }
    if (kn >= 0 && (hits || !(q[kb] > CLIP_KL_FAR * q[kn]))) kb = kn;
    int32_t kc = 2 * D * kb - S;
    /* fine: NF frames of A against B from kc - F to kc + F, at every frame */
    float qf[2 * F + 1];
    clip_kl_mono(pad, a3, ia, NF, 1, src, l, rr, stereo, frames, ad);
    clip_kl_mono(pad, a3, ib + kc - F, NF + 2 * F + 2, 1, src, l, rr, stereo, frames, bd);
    clip_kl_score(ad, bd, NF, 2 * F + 1, 1, qf);
    int32_t kf = 0;
    for (int32_t k = 1; k < 2 * F + 1; k++) if (qf[k] > qf[kf]) kf = k;
    return (float)((xa - (double)ia) + (double)(ib + kc - F + kf) - xb0);
}

/* Place the coming join clear of hits. pa is the sounding playhead (the next
 * A) now, t and L the cycle's time and its join. Hits: a 16-frame step of the
 * coming audio CLIP_KL_HITX times louder than the loudest of the 24 before it
 * (so a steady note, even a low one, never counts) and above -40 dB. Returns
 * the join's new time, the nearest to L at which no hit falls where the join
 * would fade, repeat or skip; L when there is none or no hit. A hit found
 * (re)starts hit mode, *hit = CLIP_KL_HOLD cycles. Kept out of line, so its scan buffer is on the stack
 * only while it runs, never under the search. */
__attribute__((noinline)) static float clip_kl_plan(void *pad, void *a3, double pa, float r, float v, float t, float L, void *src,
                          float *l, float *rr, int stereo, int32_t frames, uint8_t *hit)
{
    enum { E = CLIP_KL_ENV, M = CLIP_KL_ENVN, H = CLIP_KL_HIST };
    float g = v - r, mv = clip_kl_move(L);
    int fl;
    float sep = g * clip_kl_cycle(r, v, 1, &fl);    /* B from A at the join, if there are hits */
    float F = CLIP_KL_FADE * r, G = CLIP_KL_GUARD, S = (float)CLIP_KL_HITNEAR;
    float lo, hi;                                    /* frames from A at the join that must hold no hit */
    if (g < 0.0f) { lo = sep - S - G; hi = F + G; }  /* B behind: it repeats [sep, 0] */
    else { lo = -G; hi = sep + S + F + G; }          /* B ahead: [0, sep] is skipped */
    float t1 = L + mv;                               /* the latest join */
    double from = pa + (double)lo - (double)(H * E); /* with history for the first steps */
    if (from < 0.0) from = 0.0;
    int32_t p0 = (int32_t)from;
    float a0 = (float)(pa - (double)p0);             /* A now, frames from p0 */
    int32_t len = ((int32_t)(a0 + r * (t1 - t) + hi) / E + 1) * E;
    if (len > M * E) len = M * E;
    float e[M];
    for (int32_t i = 0; i < len / E; i++) e[i] = 0.0f;
    for (int32_t k = 0; k < len; k += CLIP_JGL_MAX_N) {
        int32_t n = len - k < CLIP_JGL_MAX_N ? len - k : CLIP_JGL_MAX_N;
        clip_jgl_head(pad, a3, (double)(p0 + k), 1.0f, n, src, l, rr, stereo, frames);
        for (int32_t i = 0; i < n; i++) { float x = stereo ? l[i] + rr[i] : l[i]; e[(k + i) / E] += x * x; }
    }
    float hits[16];
    int32_t nh = 0;
    for (int32_t i = H; i < len / E && nh < 16; i++) {
        float top = 0.0f;
        for (int32_t k = 1; k <= H; k++) if (e[i - k] > top) top = e[i - k];
        if (e[i] > CLIP_KL_HITX * top && e[i] > CLIP_KL_HITMIN) { hits[nh++] = (float)(i * E); i += H; }
    }
    if (!nh) return L;
    *hit = CLIP_KL_HOLD;
    float best = -1.0f, bd = 0.0f, end = (float)len;
    for (float tj = t; tj <= t1; tj += (float)E / r) {
        float q = a0 + r * (tj - t);                 /* A at this join, frames from p0 */
        if (q + hi > end) break;                     /* beyond what was scanned */
        int ok = 1;
        for (int32_t h = 0; h < nh && ok; h++) { float d = hits[h] - q; ok = !(d >= lo && d <= hi); }
        float dd = tj > L ? tj - L : L - tj;
        if (ok && (best < 0.0f || dd < bd)) { best = tj; bd = dd; }
    }
    return best < 0.0f ? L : best;
}

/* ---- the Jungle Modes: where the clip is read from, and how -------------------
 * A piece is a stretch of one block that plays one way: where it reads at its
 * first sample (x, file frames), how fast that place moves (pv; X moves at v)
 * and the playheads' rate (pr, the pitch), what it is (cls) and how it is
 * played (eng). clip_piece works it out from A -- X counted from the loop's
 * first pass, so 2-bar frames and phrases carry across a short loop's
 * restarts -- on the clip's own 16th grid. */
typedef struct {
    double x;
    double x2;                       /* CLIP_K_STAB: the fifth's place */
    float pv, pr;
    float cyc;                       /* CLIP_E_MOD: the cycle, file frames */
    float g;                         /* its gain (b19: Rewind fading out at the end of its reach) */
    int32_t lim;                     /* output samples to its end */
    uint8_t cls, eng, rev;           /* rev: Reverse Swell, read backwards from x */
    uint8_t harm;                    /* CLIP_E_MOD: the interval set (5: none) */
} clip_pc_t;

typedef struct {
    double A, X, F;                  /* X from the first pass, X, the loop: file frames */
    double f16;                      /* frames a 16th */
    double e, src;                   /* a held button: X frames since its press point; where it plays from */
    float v, rb;                     /* X's rate; the pitch rate (Vinyl: v) */
    float cyc;                       /* the pad's Jungle cycle */
    uint32_t method, jm, btn, dk;    /* dk: the deck button (CLIP_D_*) */
    clip_hp_t *hp;                   /* the pad's hit pool, 0: none */
    int idx;                         /* its position (the chops' choices differ per pad) */
    double rend;                     /* Rewind let go: the e it drops back in at; 0: held, or none */
} clip_mx_t;

typedef struct {
    void *pad, *a3, *src;
    float *bl, *br;                  /* CLIP_JGL_MAX_N frames of scratch */
    int stereo;
    int32_t frames;
    double F;                        /* a Jungle Mode's loop, which its reads run on round; 0: none */
    int canon;                       /* its reads start a rate on, from a whole frame too (clip_head_w) */
} clip_io_t;

/* Re-sequencing (modes 1-8 and Call & Answer): over a frame of `span` 16ths,
 * event (o, s) plays from the frame's 16th s at its 16th o, on until the next
 * event, so 16ths that run on in order are one stretch and only real jumps are
 * joins. Swing (a percentage) starts an event on an off-16th later; the audio
 * is never stretched for it. From 16th `answer` on, Call & Answer's events are
 * an octave up. Our own rules, on the grid. */
typedef struct { uint8_t span, n, swing, answer; uint8_t ev[6][2]; } clip_seq_t;
static const clip_seq_t clip_seqs[9] = {
    { 16, 2, 0, 0,  { {0, 4}, {12, 0} } },                                    /* Rotate */
    { 16, 3, 0, 0,  { {0, 0}, {8, 12}, {12, 12} } },                          /* Tail Loop */
    { 16, 2, 0, 0,  { {0, 0}, {8, 4} } },                                     /* Half Repeat */
    { 32, 3, 0, 0,  { {0, 0}, {16, 4}, {24, 4} } },                           /* Two-Bar Repeat */
    { 16, 3, 0, 0,  { {0, 0}, {6, 0}, {12, 0} } },                            /* Threes */
    { 16, 6, 0, 0,  { {0, 0}, {3, 4}, {6, 4}, {8, 0}, {11, 4}, {14, 4} } },   /* Riddim */
    { 16, 3, 0, 0,  { {0, 4}, {6, 4}, {12, 4} } },                            /* Figure Loop */
    { 16, 4, 58, 0, { {0, 0}, {8, 6}, {10, 0}, {12, 12} } },                  /* UKG */
    { 32, 5, 0, 16, { {0, 0}, {16, 0}, {20, 0}, {24, 12}, {28, 8} } },        /* Call & Answer */
};

/* the intervals, modes 19-23: grains in turn at these rates */
static const float clip_harms[5][3] = {
    { 1.0f, 1.5f, 1.0f },                        /* Fifth Up */
    { 1.0f, 1.33333333f, 1.0f },                 /* Fourth Up */
    { 1.0f, 1.18920712f, 1.0f },                 /* Minor Third, 2^(3/12) */
    { 1.0f, 0.5f, 1.0f },                        /* Octave Down */
    { 1.0f, 1.5f, 2.0f },                        /* Octave + Fifth: three in turn */
};
static uint32_t clip_harm_n(uint32_t h) { return h == 4u ? 3u : h < 4u ? 2u : 1u; }
static float clip_harm_x(uint32_t h, uint32_t i) { return h < 5u && i < 3u ? clip_harms[h][i] : 1.0f; }

static uint32_t clip_eng_of(uint32_t cls, uint32_t method)
{
    if (cls == CLIP_K_ROLL || (cls == CLIP_K_PROLL && method == CLIP_VINYL) || cls == CLIP_K_REV || cls == CLIP_K_SILENT
        || cls == CLIP_K_STAB
        || cls == CLIP_K_REW) return CLIP_E_FLAT;
    if (cls == CLIP_K_FREEZE || (cls >= CLIP_K_HARM && cls < CLIP_K_NONE)) return CLIP_E_MOD;
    if (cls == CLIP_K_PROLL) return method == CLIP_KEYLOCK ? CLIP_E_KL : method == CLIP_JUNGLE ? CLIP_E_JGL : CLIP_E_FLAT;
    if (method == CLIP_VINYL) return cls == CLIP_K_ANSWER ? CLIP_E_MOD : CLIP_E_FLAT;
    return method == CLIP_KEYLOCK ? CLIP_E_KL : CLIP_E_JGL;
}

/* the pitch a piece of class cls played at, against the pad's (the old sound
 * of a join, for its tail) */
static float clip_cls_pitch(uint32_t cls) { return cls == CLIP_K_ANSWER ? 2.0f : 1.0f; }

static double clip_wrap(double x, double F) { return x - __builtin_floor(x / F) * F; }

/* d brought within half a loop of 0 */
static double clip_near(double d, double F) { return F > 0.0 ? d - __builtin_floor(d / F + 0.5) * F : d; }

/* the clip's 16th grid: F file frames to a loop of T ticks (960 a beat), so
 * F x 240 / T frames a 16th. None for slice markers (the loop does not start
 * at 0) or while anything is missing. */
static int clip_grid(const uint8_t *pad_, double *F, double *f16)
{
    const uint8_t *pad = __builtin_assume_aligned(pad_, 4);
    int64_t T, sync; int32_t f; uint32_t nmark;
    __builtin_memcpy(&T, pad + 0xb90, 8);
    __builtin_memcpy(&f, pad + 0x23c, 4);
    __builtin_memcpy(&sync, pad + 0x220, 8);
    __builtin_memcpy(&nmark, pad + 0x250, 4);
    if (pad[0x1f5] && sync == 0xf0000 && nmark != 0u) return 0;
    if (T < 240 || T > 0x7fffffff || f <= 0) return 0;
    *F = (double)f;
    *f16 = (double)f * 240.0 / (double)(int32_t)T;
    return *f16 >= 16.0;
}

/* One playhead, as clip_jgl_head, but the Jungle Modes' way. Stock's
 * resampler (0x0809ff64) starts a frame on, sample i at p + 1 + i x r (b19,
 * measured: b13 took it for a rate on, the same only at rate 1); from its
 * source buffer (rate 1, a whole frame) it starts at the frame itself.
 * canon reads that one a frame on too, so a playhead whose place comes to a
 * whole frame does not jump a sample (a join's tail is rebuilt from X, and
 * lands either side of one); without it, stock's and b12's way. While a mode
 * plays (F file frames, else 0) its reads run on round the loop, as the
 * mode's place does: across the end in two -- the samples clear of its last
 * frames, and those from a few frames into its start -- and the samples
 * between, at the seam, drawn from the 16 frames either side of it, read as
 * they are (b19: stock's resampler holds its last frame where its fetch runs
 * past the file's end, and b13 drew the seam straight from the samples either
 * side -- flat, if the block ended there: at twice the speed, a step). */
#define CLIP_WRAP_GUARD  4.0          /* frames kept clear of either end by the resampler's reads */
static void clip_head_w(const clip_io_t *io, double p, float r, int32_t len, float *l, float *rr, int canon)
{
    double F = io->F;
    if (F > 0.0) p = clip_wrap(p, F);
    if (canon && r == 1.0f && p == __builtin_floor(p)) p += 1.0;
    if (!(F > 32.0) || p + (double)r * (double)(len + 1) + CLIP_WRAP_GUARD < F - 1.0) {
        clip_jgl_head(io->pad, io->a3, p, r, len, io->src, l, rr, io->stereo, io->frames);
        return;
    }
    double s1 = (F - 2.0 - CLIP_WRAP_GUARD - p) / (double)r, s2 = (F + CLIP_WRAP_GUARD - 1.0 - p) / (double)r;
    int32_t m1 = s1 < 0.0 ? 0 : (int32_t)s1 + 1;                                 /* sample m1: near the end */
    int32_t m2 = s2 <= 0.0 ? 0 : (int32_t)s2 + ((double)(int32_t)s2 < s2);    /* sample m2: clear of the start */
    if (m1 > len) m1 = len;
    if (m2 > len) m2 = len;
    if (m2 < m1) m2 = m1;
    if (m1) clip_jgl_head(io->pad, io->a3, p, r, m1, io->src, l, rr, io->stereo, io->frames);
    if (m2 < len) {
        double q = p + (double)r * (double)m2 - F;
        if (canon && r == 1.0f && q == __builtin_floor(q)) q += 1.0;
        clip_jgl_head(io->pad, io->a3, q, r, len - m2, io->src, l + m2, rr ? rr + m2 : 0, io->stereo, io->frames);
    }
    if (m2 > m1) {
        float sl[32], sr[32];                    /* frames F - 16 .. F - 1, then 0 .. 15: straight copies */
        clip_jgl_head(io->pad, io->a3, F - 16.0, 1.0f, 16, io->src, sl, sr, io->stereo, io->frames);
        clip_jgl_head(io->pad, io->a3, 0.0, 1.0f, 16, io->src, sl + 16, sr + 16, io->stereo, io->frames);
        for (int32_t i = m1; i < m2; i++) {      /* stock's own cubic (0x0809ff64), as if the loop ran on */
            double q = p + 1.0 + (double)r * (double)i - (F - 16.0);   /* sample i, as the resampler places it */
            int32_t k = (int32_t)q;
            float u = (float)(q - (double)k);
            if (k < 1) { k = 1; u = 0.0f; }
            if (k > 28) { k = 28; u = 1.0f; }
            for (int c = 0; c < (io->stereo ? 2 : 1); c++) {
                const float *y = (c ? sr : sl) + k - 1;
                float a0 = y[3] - y[2] - y[0] + y[1], a1 = y[0] - y[1] - a0, a2 = y[2] - y[0];
                float v = ((a0 * u + a1) * u + a2) * u + y[1];
                if (c) rr[i] = v; else l[i] = v;
            }
        }
    }
}

/* ---- 2.3.10-b17: the chops -------------------------------------------------
 * Subtle Chops, Lift Chops and Chop Stabs play hits from the pool at places on
 * the 16th grid, and Pitch Roll rolls the bar's snare. Every choice is made
 * from the bar and the event by a hash, so a piece worked out again later in
 * the same chop finds the same one; the source of the chop playing is kept
 * besides (the pool grows as it is scanned). Our own rules, from the
 * prototypes the owner listened to (jungle-modes-reference). */
static uint32_t clip_hash(uint32_t a, uint32_t b)
{
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca77u;
    h ^= h >> 16; h *= 0x85ebca6bu; h ^= h >> 13; h *= 0xc2b2ae35u; h ^= h >> 16;
    return h;
}
static float clip_u01(uint32_t h) { return (float)(h >> 8) * (1.0f / 16777216.0f); }

static int clip_hp_use(const clip_mx_t *m)
{
    return m->hp && m->hp->n >= CLIP_HP_MINN && m->hp->F == (int32_t)m->F;
}

/* the hit at or before file frame x, round the loop (the grid: x's 16th) */
static double clip_hit_here(const clip_mx_t *m, double x)
{
    if (!clip_hp_use(m)) return __builtin_floor(x / m->f16) * m->f16;
    const clip_hp_t *h = m->hp;
    double best = x, bd = 1.0e30;
    for (uint32_t i = 0; i < h->n; i++) {
        double d = clip_wrap(x - (double)h->on[i], m->F);
        if (d < bd) { bd = d; best = (double)h->on[i]; }
    }
    return best;
}

/* the hit nearest x within maxd frames, else -1 (the grid: x itself) */
static double clip_hit_near(const clip_mx_t *m, double x, double maxd)
{
    if (!clip_hp_use(m)) return x;
    const clip_hp_t *h = m->hp;
    double best = -1.0, bd = maxd;
    for (uint32_t i = 0; i < h->n; i++) {
        double d = clip_near((double)h->on[i] - x, m->F);
        if (d < 0.0) d = -d;
        if (d <= bd) { bd = d; best = (double)h->on[i]; }
    }
    return best;
}

/* frames from hit s to the next, at most a beat (the grid: a 16th) */
static double clip_hit_len(const clip_mx_t *m, double s)
{
    double beat = 4.0 * m->f16, best = beat;
    if (!clip_hp_use(m)) return m->f16;
    const clip_hp_t *h = m->hp;
    for (uint32_t i = 0; i < h->n; i++) {
        double d = clip_wrap((double)h->on[i] - s, m->F);
        if (d > 1.0 && d < best) best = d;
    }
    return best;
}

/* a hit, chosen by hash r, more than lo frames from `here` and at most `back`
 * before it or `ahead` after, round the loop; -1 if none. b19: the last 2
 * bars and a beat ahead -- the audio the preset's cache still holds, or is
 * about to; from 8 bars either side a chop could find none (b18's clicks on a
 * long loop in a busy preset) */
#define CLIP_HIT_BACK    32.0         /* 16ths */
#define CLIP_HIT_AHEAD   4.0
static int clip_hit_in(double d, double lo, double back, double ahead)
{
    return (d > lo || d < -lo) && d > -back && d < ahead;
}
static double clip_hit_pick(const clip_mx_t *m, double here, double lo, double back, double ahead, uint32_t r)
{
    double f = m->f16, F = m->F;
    if (!clip_hp_use(m)) {                           /* the grid's 16ths within reach */
        double nl = __builtin_floor(F / f + 0.5);
        int32_t a = (int32_t)(ahead / f), b = (int32_t)(back / f), cnt = 0;
        if (b > (int32_t)nl - 1) b = (int32_t)nl - 1;
        for (int32_t d = -b; d <= a; d++) cnt += clip_hit_in(clip_near((double)d * f, F), lo, back, ahead);
        if (!cnt) return -1.0;
        int32_t k = (int32_t)(r % (uint32_t)cnt);
        for (int32_t d = -b; d <= a; d++)
            if (clip_hit_in(clip_near((double)d * f, F), lo, back, ahead) && !k--) return clip_wrap(here + (double)d * f, F);
        return -1.0;
    }
    const clip_hp_t *h = m->hp;
    uint32_t cnt = 0;
    for (uint32_t i = 0; i < h->n; i++) cnt += clip_hit_in(clip_near((double)h->on[i] - here, F), lo, back, ahead);
    if (!cnt) return -1.0;
    uint32_t k = r % cnt;
    for (uint32_t i = 0; i < h->n; i++)
        if (clip_hit_in(clip_near((double)h->on[i] - here, F), lo, back, ahead) && !k--) return (double)h->on[i];
    return -1.0;
}

/* the chop playing's source, kept while it plays */
static int clip_chop_kept(const clip_mx_t *m, uint32_t key, double *src)
{
    if (!m->hp || m->hp->ckey != key) return 0;
    *src = m->hp->csrc;
    return 1;
}
static double clip_chop_keep(const clip_mx_t *m, uint32_t key, double src)
{
    if (m->hp) { m->hp->ckey = key; m->hp->csrc = src; }
    return src;
}
static uint32_t clip_chop_key(const clip_mx_t *m, int32_t ib, uint32_t ev)
{
    return 0x80000000u | (m->jm & 31u) << 26 | ((uint32_t)ib & 0xfffffu) << 6 | (ev & 63u);
}

/* the 2-step grids: Lift Chops' (mostly off-16ths, 58% swing) and Chop
 * Stabs' (60%); bar B answers bar A */
static const uint8_t clip_lift_r[2][7] = { { 0, 3, 5, 7, 10, 13, 15 }, { 1, 3, 6, 7, 9, 11, 14 } };
static const uint8_t clip_stab_r[2][6] = { { 0, 3, 6, 10, 11, 14 }, { 0, 2, 6, 9, 12, 14 } };

/* what event j of bar ib plays from: a random hit from 8 bars either side
 * (swap of the time), else the hit at the place itself */
static double clip_chop_pick(const clip_mx_t *m, int32_t ib, uint32_t j, const uint8_t *rh, double sw, float swap)
{
    double f = m->f16;
    double t = ((double)ib * 16.0 + (double)rh[j]) * f + ((rh[j] & 1u) ? sw : 0.0);
    double here = clip_hit_here(m, clip_wrap(t, m->F));
    uint32_t h = clip_hash((uint32_t)ib, (uint32_t)m->idx << 12 | m->jm << 6 | j);
    if (clip_u01(h) < swap) {
        double p = clip_hit_pick(m, here, 0.0, CLIP_HIT_BACK * f, CLIP_HIT_AHEAD * f, clip_hash(h, 7u));
        if (p >= 0.0) return p;
    }
    return here;
}

/* Lift Chops and Chop Stabs at A: the chop playing, or the clip as played
 * between them. b20, the owner's choice: Chop Stabs is the bar as recorded,
 * its 2-step accents turned into 16th-long root + fifth stabs of the audio
 * at that very place, a reversed pickup into each 4th downbeat */
static void clip_chops(const clip_mx_t *m, clip_pc_t *pc, double *xa, double *x2a, double *bnd, int *at_x)
{
    double A = m->A, f = m->f16, F = m->F, bar = __builtin_floor(A / (16.0 * f)), b0 = bar * 16.0 * f;
    int32_t ib = (int32_t)bar;
    int lift = m->jm == CLIP_JM_LIFT;
    uint32_t nn = lift ? 7u : 6u;
    const uint8_t *rh = lift ? clip_lift_r[ib & 1] : clip_stab_r[ib & 1];
    const uint8_t *ra = lift ? clip_lift_r[0] : clip_stab_r[0];
    double sw = (lift ? 0.16 : 0.2) * f, t[8];
    for (uint32_t k = 0; k < nn; k++) t[k] = b0 + (double)rh[k] * f + ((rh[k] & 1u) ? sw : 0.0);
    t[nn] = b0 + 16.0 * f;
    int32_t j = -1;
    for (uint32_t k = 0; k < nn; k++) if (t[k] <= A) j = (int32_t)k;
    float v = m->v;
    if (!lift && (ib & 3) == 3) {                    /* Chop Stabs: a reversed pickup into each 4th downbeat (b18) */
        double te = b0 + 16.0 * f, here, np = 2.0 * f;
        uint32_t key = clip_chop_key(m, ib, 62u);
        if (!clip_chop_kept(m, key, &here)) here = clip_chop_keep(m, key, clip_hit_here(m, clip_wrap(te - np, F)));
        double cap = clip_hit_len(m, here) * (double)v / (double)m->rb;
        if (np > cap) np = cap;
        if (A >= te - np) {
            pc->cls = CLIP_K_REV; pc->rev = 1; pc->pr = m->rb; pc->pv = m->rb;
            *xa = here + (double)m->rb * ((np - (A - (te - np))) / (double)v);
            *bnd = te; *at_x = 0;
            return;
        }
        t[nn] = te - np;                             /* the last stab gives way to it */
    }
    *at_x = 1;                                       /* between chops, as played (b19: no gaps, the owner's choice) */
    if (j < 0) { *bnd = t[0]; return; }
    double slot = t[j + 1] - t[j], n = slot;
    uint32_t h = clip_hash((uint32_t)ib, (uint32_t)m->idx << 12 | m->jm << 6 | 32u | (uint32_t)j);
    float ratio = 1.0f;
    uint32_t cls = CLIP_K_ROLL;
    if (lift) {                                      /* short; bar B lifts half an octave, now and then one drops */
        if ((ib & 1) && clip_u01(clip_hash(h, 1u)) < 0.5f) ratio = 2.0f;
        else if (clip_u01(clip_hash(h, 2u)) < 0.08f) ratio = 0.5f;
        if (clip_u01(clip_hash(h, 3u)) < 0.1f) cls = CLIP_K_REV;
    } else {                                         /* every accent a 16th-long stab of its own place */
        cls = CLIP_K_STAB;
        if (n > f) n = f;
    }
    uint32_t key = clip_chop_key(m, ib, (uint32_t)j);
    double src;
    if (!lift) {
        src = clip_wrap(t[j], F);                    /* the bar as recorded */
    } else if (!clip_chop_kept(m, key, &src)) {
        /* b19, the owner's choice: more in order -- the bar's own hits mostly,
         * and bar B answering bar A with most of its chops */
        if ((ib & 1) && clip_u01(clip_hash(h, 4u)) < 0.85f) src = clip_chop_pick(m, ib - 1, (uint32_t)j, ra, sw, 0.15f);
        else src = clip_chop_pick(m, ib, (uint32_t)j, rh, sw, 0.15f);
        src = clip_chop_keep(m, key, src);
    }
    float pr = m->rb * ratio;
    if (cls == CLIP_K_REV) {                         /* no further than the hit's own length */
        double cap = clip_hit_len(m, src) * (double)v / (double)pr;
        if (n > cap) n = cap;
    }
    /* At the clip's own pitch a chop plays as the clip does, in time (Jungle's
     * and Key Lock's stretch). Resampled, it reads its audio at the pad's own
     * speed: slower than the clip, the gap it fills is longer than that audio,
     * so it stops at its musical length rather than run into the next hit. */
    int plain = cls == CLIP_K_ROLL && ratio == 1.0f;
    if (!plain && m->rb > v) n *= (double)v / (double)m->rb;
    if (t[j] + n > t[j + 1]) n = t[j + 1] - t[j];
    if (A >= t[j] + n) { *bnd = t[j + 1]; return; }
    double el = (A - t[j]) / (double)v;              /* output samples into the chop */
    *at_x = 0;
    *bnd = t[j] + n;
    if (plain) {                                     /* a jump, as the re-sequencing modes make */
        pc->cls = CLIP_K_PLAIN;
        *xa = src + (A - t[j]);
        return;
    }
    pc->cls = (uint8_t)cls; pc->pr = pr; pc->pv = pr;
    if (cls == CLIP_K_REV) {
        pc->rev = 1;
        *xa = src + (double)pr * (n / (double)v - el);
    } else {
        *xa = src + (double)pr * el;
        *x2a = src + 1.5 * (double)pr * el;
    }
    (void)F;
}

/* Subtle Chops at A: the bar as played, but for one or two 16ths or 8ths
 * (never on a beat, different every bar; two or three in a phrase's last bar)
 * played from a hit within 8 bars */
static const uint8_t clip_sub_k[12] = { 1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15 };
static void clip_subtle(const clip_mx_t *m, clip_pc_t *pc, double *xa, double *bnd, int *at_x)
{
    double A = m->A, f = m->f16, bar = __builtin_floor(A / (16.0 * f)), b0 = bar * 16.0 * f;
    double u = (A - b0) / f;
    int32_t ib = (int32_t)bar;
    uint32_t h = clip_hash((uint32_t)ib, (uint32_t)m->idx << 12 | 0x900u);
    uint32_t ne = ((ib & 3) == 3 ? 2u : 1u) + (h & 1u), got = 0;
    uint8_t ks[3], kl[3];
    for (uint32_t q = 0; got < ne && q < 24u; q++) {
        uint32_t k = clip_sub_k[clip_hash(h, q) % 12u], dup = 0;
        for (uint32_t i = 0; i < got; i++) dup |= ks[i] == k;
        if (dup) continue;
        ks[got] = (uint8_t)k;
        kl[got] = (uint8_t)((k & 1u) == 0u && clip_u01(clip_hash(h, 40u + k)) < 0.5f ? 2u : 1u);
        got++;
    }
    int32_t act = -1;
    double nxt = 16.0;
    for (uint32_t i = 0; i < got; i++) {
        if ((double)ks[i] <= u && u < (double)(ks[i] + kl[i]) && (act < 0 || ks[i] > ks[act])) act = (int32_t)i;
        if ((double)ks[i] > u && (double)ks[i] < nxt) nxt = (double)ks[i];
    }
    *bnd = b0 + nxt * f;
    if (act < 0) return;                             /* as played, at X */
    double ts = b0 + (double)ks[act] * f, te = b0 + (double)(ks[act] + kl[act]) * f;
    uint32_t key = clip_chop_key(m, ib, ks[act]);
    double src;
    if (!clip_chop_kept(m, key, &src))
        src = clip_chop_keep(m, key, clip_hit_pick(m, clip_wrap(ts, m->F), f, CLIP_HIT_BACK * f, CLIP_HIT_AHEAD * f,
                                                    clip_hash(h, 60u + ks[act])));
    if (src < 0.0) return;                           /* no hit near enough: as played */
    if (te < *bnd) *bnd = te;
    *xa = src + (A - ts);                            /* played as the clip is, in time: a jump, as Rotate's */
    *at_x = 0;
}

/* The deck buttons (b36; Pull Up alone b17-b35): where the record is, in X
 * frames of place since the press (p), and its speed there (s: 1 playing,
 * below 0 backwards). Lengths follow the tempo the pad plays to, in 16ths,
 * capped in seconds so a slow Manual tempo cannot drag them out:
 *   Pull Up -- a brake to a stop over 0.3 of a beat, flicked back to
 *     CLIP_REW_PEAK over an 8th, then slower and slower to a stop as the
 *     motor fights it, 2.5 beats, at most 2 s (b34-b35 dragged on at half
 *     speed; b17-b33 held twice the speed)
 *   Power Down -- the deck switched off, coasting to a stop: 2 bars, at most 6 s
 *   Brake -- its Stop button: 3 16ths, at most 0.5 s
 *   Baby Scratch -- rocked forward a 16th and back a 16th, over and over
 * Power Down and Brake, let go, spin up as its motor does (clip_dk_at). */
#define CLIP_REW_PEAK    1.6f
typedef struct { float s; double p; } clip_dk_t;
static double clip_dk_len(const clip_mx_t *m, double n16, double secs)
{
    double a = n16 * m->f16, b = secs * 48000.0 * (double)m->v;
    return a < b ? a : b;
}
static clip_dk_t clip_dk(const clip_mx_t *m, double e)
{
    const float h = 1.57079632679489661923f;
    const double k = 0.63661977236758134308;                     /* 2 / pi */
    double f = m->f16;
    clip_dk_t d;
    if (m->dk == CLIP_D_SCRATCH) {
        float a = (float)(e / f) * 2.0f * h;
        d.s = h * chr_sinf(a);
        d.p = 0.5 * f * (1.0 - (double)chr_cosf(a));
        return d;
    }
    if (m->dk == CLIP_D_POWER || m->dk == CLIP_D_BRAKE) {         /* a steady drag: the speed falls in a line */
        double L = m->dk == CLIP_D_POWER ? clip_dk_len(m, 32.0, 6.0) : clip_dk_len(m, 3.0, 0.5);
        if (e >= L) { d.s = 0.0f; d.p = 0.5 * L; return d; }
        d.s = (float)(1.0 - e / L);
        d.p = e - e * e / (2.0 * L);
        return d;
    }
    double eb = 1.2 * f, ef = 2.0 * f, D = clip_dk_len(m, 10.0, 2.0), P = (double)CLIP_REW_PEAK;
    if (e < eb) {
        d.s = chr_cosf((float)(e / eb) * h);
        d.p = k * eb * (double)chr_sinf((float)(e / eb) * h);
        return d;
    }
    double p = k * eb;
    if (e < eb + ef) {
        d.s = -CLIP_REW_PEAK * chr_sinf((float)((e - eb) / ef) * h);
        d.p = p - P * k * ef * (1.0 - (double)chr_cosf((float)((e - eb) / ef) * h));
        return d;
    }
    p -= P * k * ef;
    double w = (e - eb - ef) / D;
    if (w >= 1.0) { d.s = 0.0f; d.p = p - 0.5 * P * D; return d; }
    d.s = (float)(-P * (1.0 - w));
    d.p = p - P * D * (w - 0.5 * w * w);
    return d;
}
/* ... and once Power Down or Brake is let go (hp->ue on): the spin-up, its
 * speed rising in a line to 1 by rend */
static clip_dk_t clip_dk_at(const clip_mx_t *m, double e)
{
    const clip_hp_t *h = m->hp;
    if ((m->dk == CLIP_D_POWER || m->dk == CLIP_D_BRAKE) && m->rend > 0.0 && h && h->ue >= 0.0 && e >= h->ue) {
        double U = m->rend - h->ue, u = e - h->ue;
        if (!(U > 1.0)) U = 1.0;
        if (u > U) u = U;
        clip_dk_t d;
        d.s = h->us + (1.0f - h->us) * (float)(u / U);
        d.p = h->up + (double)h->us * u + (1.0 - (double)h->us) * u * u / (2.0 * U);
        return d;
    }
    return clip_dk(m, e);
}
#define CLIP_REW_LP      0.3675f      /* one pole at 3.5 kHz: 1 - e^(-2 pi 3500 / 48000) */
/* b36: a deck piece's level from the speed its playhead keeps in L (clip_eng_run),
 * against the pitch rate r: for its slice's fade-out and its join's tail */
static float clip_rew_level(const clip_jgl_t *j, float r)
{
    float q = (j->L < 0.0f ? -j->L : j->L) / r;
    return r > 0.0f && q < 1.0f ? q : 1.0f;
}
#define CLIP_REW_LPX     0.45815f     /* its 2 pi 3500 / 48000 */

#define CLIP_JM_CHAOS    31u          /* not on the encoder: Mix Wild's chaos */

/* the buttons playing: Rewind until it has dropped back in, and a roll for
 * its 4 beats, as the owner heard it -- past its beat of 32nds the loop plays
 * on, held or not; 32nds for longer turn into a buzz (b17) */
static uint32_t clip_held(const clip_mx_t *m)
{
    uint32_t held = m->btn & CLIP_B_HELD;
    if ((held & CLIP_B_REW) && m->rend > 0.0 && m->e >= m->rend) held &= ~CLIP_B_REW;
    if (!(held & (CLIP_B_FREEZE | CLIP_B_REW)) && m->e >= 16.0 * m->f16) held &= ~(CLIP_B_ROLL | CLIP_B_PROLL);
    return held;
}

/* The Jungle Mode, or a held button, at A: the piece that plays from there. */
static void clip_piece_jm(const clip_mx_t *m, clip_pc_t *pc)
{
    double A = m->A, f = m->f16, xa = A, x2a = A, bnd = 1.0e30;
    float v = m->v;
    int at_x = 1;                                    /* plays at X itself */
    pc->pv = v; pc->pr = m->rb; pc->cyc = m->cyc; pc->g = 1.0f;
    pc->cls = CLIP_K_PLAIN; pc->rev = 0; pc->harm = 5;
    uint32_t held = clip_held(m), jm = m->jm;
    if (held & CLIP_B_FREEZE) {                      /* X stops: an 8th's grain at the press point */
        pc->cls = CLIP_K_FREEZE; pc->pv = 0.0f; pc->cyc = (float)(2.0 * f);
        xa = m->src; at_x = 0;
    } else if (held & CLIP_B_REW) {                  /* a deck button (clip_dk); let go, on to the next 16th or spun up */
        double e = m->e, st = 32.0 * (double)v;      /* the speed is held for 32 samples at a time */
        clip_dk_t d = clip_dk_at(m, e + 0.5 * st);
        float s = d.s, as = s < 0.0f ? -s : s;
        float rr = as * m->rb;
        if (rr < 1.0e-3f * m->rb) rr = 1.0e-3f * m->rb;
        pc->cls = CLIP_K_REW; pc->pr = rr; pc->pv = rr; pc->rev = s < 0.0f;
        pc->g = as < 1.0f ? as : 1.0f;               /* b36: a cartridge's level follows the record's speed */
        double back = -(double)(m->rb / v) * clip_dk_at(m, e).p;   /* file frames behind the press point */
        xa = m->src - back;
        bnd = A + st;
        at_x = 0;
        /* stopped, silent; and (b19) at most 2 bars behind X, then it fades out
         * over a beat's worth -- further, the audio is long gone from the
         * preset's cache */
        double lag = e + back;
        if (!(as > 0.0f) || lag >= 36.0 * f) {
            pc->cls = CLIP_K_SILENT; pc->pv = v; pc->pr = v; pc->rev = 0; pc->g = 1.0f;
            at_x = 1; bnd = 1.0e30;
        } else if (lag > 32.0 * f) {
            pc->g *= (float)(1.0 - (lag - 32.0 * f) / (4.0 * f));
        }
        if (m->rend > 0.0 && A + (m->rend - e) < bnd) bnd = A + (m->rend - e);
    } else if (held) {                               /* the 8th from the press point: 8ths for 2 beats, 16ths for 1, then 32nds */
        double e = m->e, st, len;
        int32_t q;
        if (e < 8.0 * f) { q = (int32_t)(e / (2.0 * f)); st = 2.0 * f * q; len = 2.0 * f; }
        else if (e < 12.0 * f) { q = (int32_t)((e - 8.0 * f) / f); st = 8.0 * f + f * q; len = f; q += 4; }
        else { q = (int32_t)((e - 12.0 * f) / (0.5 * f)); st = 12.0 * f + 0.5 * f * q; len = 0.5 * f; q += 8; }
        pc->cls = CLIP_K_ROLL;
        if (held & CLIP_B_PROLL) {                   /* each repeat a semitone on, to an octave (b17) */
            int32_t s = q > 12 ? 12 : q;
            pc->pr *= chr_exp2f((float)((m->btn & CLIP_B_UP) ? s : -s) * (1.0f / 12.0f));
            pc->cls = CLIP_K_PROLL;
        }
        if (m->method == CLIP_VINYL) {               /* resampled, as a turntable's */
            pc->pv = pc->pr;
            xa = m->src + (double)pc->pr * ((e - st) / (double)v);
        } else {                                     /* b19: played as the clip is, in time -- resampled at its own
                                                        speed, a slow tempo's repeat ran on into audio not cached */
            if (pc->cls == CLIP_K_ROLL) pc->cls = CLIP_K_PLAIN;
            xa = m->src + (e - st);
        }
        bnd = A + (st + len - e);
        at_x = 0;
    } else if (jm != CLIP_JM_OFF) {
        double S = __builtin_floor(A / f);
        double bar = __builtin_floor(S / 16.0), k = S - bar * 16.0;
        int32_t ib = (int32_t)bar;
        if ((jm >= CLIP_JM_ROTATE && jm <= CLIP_JM_UKG) || jm == CLIP_JM_CALL) {
            const clip_seq_t *q = &clip_seqs[jm == CLIP_JM_CALL ? 8u : jm - CLIP_JM_ROTATE];
            double base = __builtin_floor(S / (double)q->span) * (double)q->span;
            double sw = q->swing ? ((double)q->swing * 0.02 - 1.0) * f : 0.0;   /* an off-16th's delay */
            double te = base * f;
            uint32_t i = 0;
            bnd = (base + (double)q->span) * f;
            for (uint32_t e = 1; e < q->n; e++) {
                double t = (base + (double)q->ev[e][0]) * f + ((q->ev[e][0] & 1u) ? sw : 0.0);
                if (t > A) { bnd = t; break; }
                i = e; te = t;
            }
            xa = (base + (double)q->ev[i][1]) * f + (A - te);
            at_x = q->ev[i][0] == q->ev[i][1] && te == (base + (double)q->ev[i][0]) * f;
            if (q->answer && q->ev[i][0] >= q->answer) { pc->cls = CLIP_K_ANSWER; pc->pr *= 2.0f; }
        } else if (jm == CLIP_JM_SWELL) {            /* the beat into each downbeat: the downbeat's own, reversed */
            if (k >= 12.0) {
                double db = (bar + 1.0) * 16.0 * f;
                pc->cls = CLIP_K_REV; pc->rev = 1;
                xa = db + (double)pc->pr * ((db - A) / (double)v);
                bnd = db; at_x = 0;
            } else {
                bnd = (bar * 16.0 + 12.0) * f;
            }
        } else if (jm == CLIP_JM_PROLL) {            /* a phrase's last bar: beats 3-4 roll its snare, a semitone a repeat */
            if ((ib & 3) == 3 && k >= 8.0) {
                double a8 = (bar * 16.0 + 8.0) * f, e = A - a8, st, len;
                int32_t q;
                if (e < 4.0 * f) { q = (int32_t)(e / f); st = f * q; len = f; }
                else { q = (int32_t)((e - 4.0 * f) / (0.5 * f)); st = 4.0 * f + 0.5 * f * q; len = 0.5 * f; q += 4; }
                int32_t s = ((ib >> 2) & 1) ? -(q + 1) : q + 1;      /* rising one phrase, falling the next */
                pc->pr *= chr_exp2f((float)s * (1.0f / 12.0f));
                pc->pv = pc->pr;
                pc->cls = CLIP_K_PROLL;
                /* the snare: the hit nearest beat 2 (b17), else beat 2's 16th */
                double sn = (bar * 16.0 + 4.0) * f, hs;
                uint32_t key = clip_chop_key(m, ib, 63u);
                if (!clip_chop_kept(m, key, &hs)) {
                    hs = clip_hit_near(m, clip_wrap(sn, m->F), 4.0 * f);
                    hs = clip_chop_keep(m, key, hs >= 0.0 ? hs : sn);
                }
                if (m->method == CLIP_VINYL) xa = hs + (double)pc->pr * ((e - st) / (double)v);
                else { pc->pv = v; xa = hs + (e - st); }     /* b19: in time, as the buttons */
                bnd = a8 + st + len; at_x = 0;
            } else {
                bnd = ((ib & 3) == 3 ? bar * 16.0 + 8.0 : bar * 16.0 + 16.0) * f;
            }
        } else if (jm == CLIP_JM_CHAOS) {           /* Mix Wild's chaos: 6 16ths in 10 played another way */
            double s0 = S * f, e = A - s0;
            uint32_t h = clip_hash((uint32_t)(int32_t)S, (uint32_t)m->idx << 12 | 0xc40u);
            bnd = s0 + f;
            if (clip_u01(h) < 0.6f) {
                uint32_t act = clip_hash(h, 1u) & 3u;
                at_x = 0;
                if (act == 0u) {                     /* the 16th before, again */
                    xa = s0 - f + e;
                } else if (act == 1u) {              /* one from anywhere in the bar */
                    xa = (bar * 16.0 + (double)(clip_hash(h, 2u) & 15u)) * f + e;
                } else if (act == 2u) {              /* its first half, twice */
                    double hf = 0.5 * f;
                    int q = e >= hf;
                    pc->cls = CLIP_K_ROLL; pc->pv = pc->pr;
                    xa = s0 + (double)pc->pr * ((e - (q ? hf : 0.0)) / (double)v);
                    bnd = s0 + (q ? f : hf);
                } else if (clip_hash(h, 3u) & 1u) {  /* an octave up, in time */
                    pc->cls = CLIP_K_ANSWER; pc->pr *= 2.0f;
                    xa = A;
                } else {                             /* an octave down, resampled */
                    pc->cls = CLIP_K_ROLL; pc->pr *= 0.5f; pc->pv = pc->pr;
                    xa = s0 + (double)pc->pr * (e / (double)v);
                }
            }
        } else if (jm == CLIP_JM_SUBTLE) {
            clip_subtle(m, pc, &xa, &bnd, &at_x);
        } else if (jm == CLIP_JM_LIFT || jm == CLIP_JM_STABS) {
            clip_chops(m, pc, &xa, &x2a, &bnd, &at_x);
        } else if (jm >= CLIP_JM_FIFTH && jm <= CLIP_JM_OCT5) {
            pc->harm = (uint8_t)(jm - CLIP_JM_FIFTH);
            pc->cls = (uint8_t)(CLIP_K_HARM + pc->harm);
        } else if (jm == CLIP_JM_HALF) {             /* the loop over two passes: X's place at half its speed */
            pc->cls = CLIP_K_HALF;
            pc->pv = 0.5f * v;
            if (m->method == CLIP_VINYL) pc->pr = pc->pv;   /* an octave down, as a turntable would */
            xa = 0.5 * A; at_x = 0;
        }
    }
    pc->x = at_x ? m->X : clip_wrap(xa, m->F);
    pc->x2 = pc->cls == CLIP_K_STAB ? clip_wrap(x2a, m->F) : pc->x;
    pc->eng = (uint8_t)clip_eng_of(pc->cls, m->method);
    double w = (bnd - A) / (double)v;
    pc->lim = !(w > 1.0) ? 1 : w > 4096.0 ? 4096 : (int32_t)w + ((double)(int32_t)w < w);
}

/* the mixes (b18): which mode plays this bar (Mix Subtle, Mix Wild) or pair
 * of bars (Mix Gentle), chosen by a hash of the bar and the pad -- the
 * prototypes the owner listened to */
static const uint8_t clip_mix_s[4] = { CLIP_JM_FIFTH, CLIP_JM_ROTATE, 3u, CLIP_JM_SUBTLE };
static const uint8_t clip_mix_g[7] = { CLIP_JM_OFF, CLIP_JM_FIFTH, CLIP_JM_SWELL, CLIP_JM_ROTATE, 3u, CLIP_JM_SUBTLE,
                                       CLIP_JM_CALL };
static const uint8_t clip_mix_w[11] = { CLIP_JM_OCT5, 22u, 5u, 6u, 7u, CLIP_JM_UKG, CLIP_JM_LIFT, CLIP_JM_STABS,
                                        CLIP_JM_CHAOS, 2u, CLIP_JM_FIFTH };
static uint32_t clip_mix_pick(const clip_mx_t *m, uint32_t jm, int32_t ib)
{
    uint32_t salt = (uint32_t)m->idx << 12;
    if (jm == CLIP_JM_MIXS) {                        /* plain mostly; 3 bars in 10, never a phrase's first two */
        uint32_t h = clip_hash((uint32_t)ib, salt | 0xd00u);
        return (ib & 7) < 2 || clip_u01(h) >= 0.3f ? CLIP_JM_OFF : clip_mix_s[clip_hash(h, 1u) % 4u];
    }
    if (jm == CLIP_JM_MIXG) return clip_mix_g[clip_hash((uint32_t)(ib >> 1), salt | 0xd10u) % 7u];
    return clip_mix_w[clip_hash((uint32_t)ib, salt | 0xd20u) % 11u];
}

static void clip_piece(const clip_mx_t *m, clip_pc_t *pc)
{
    uint32_t jm = m->jm;
    if (jm < CLIP_JM_MIXS || jm > CLIP_JM_MIXW || clip_held(m)) { clip_piece_jm(m, pc); return; }
    double A = m->A, f = m->f16, bar = __builtin_floor(A / (16.0 * f)), k = A / f - bar * 16.0;
    int32_t ib = (int32_t)bar, every = jm == CLIP_JM_MIXS ? 4 : jm == CLIP_JM_MIXG ? 2 : 1;
    if ((ib + 1) % every == 0 && k >= 14.0) {        /* half a beat into the downbeat: its own, reversed */
        double db = (bar + 1.0) * 16.0 * f;
        float v = m->v;
        pc->pv = v; pc->pr = m->rb; pc->cyc = m->cyc; pc->harm = 5; pc->g = 1.0f;
        pc->cls = CLIP_K_REV; pc->rev = 1;
        pc->x = clip_wrap(db + (double)pc->pr * ((db - A) / (double)v), m->F);
        pc->x2 = pc->x;
        pc->eng = (uint8_t)clip_eng_of(CLIP_K_REV, m->method);
        double w = (db - A) / (double)v;
        pc->lim = !(w > 1.0) ? 1 : w > 4096.0 ? 4096 : (int32_t)w + ((double)(int32_t)w < w);
        return;
    }
    clip_mx_t *mw = (clip_mx_t *)m;                  /* the bar's mode, played as itself */
    mw->jm = clip_mix_pick(m, jm, ib);
    clip_piece_jm(mw, pc);
    mw->jm = jm;
}

/* The piece a segment was playing, played on from X + dv: a segment stock is
 * fading out (its hold spent) makes no new join, so it carries its old sound
 * under the next one's fade-in, as it does without a mode -- jumping too, the
 * two fades took a drum hit on the slice's downbeat out between them. */
static void clip_piece_on(clip_pc_t *pc, uint32_t cls, uint32_t method, const clip_jgl_t *j, double X, float dv,
                          float v, float r, const clip_mx_t *m)
{
    pc->cls = (uint8_t)cls;
    pc->eng = (uint8_t)clip_eng_of(cls, method);
    pc->rev = cls == CLIP_K_REV;
    pc->harm = (uint8_t)(cls >= CLIP_K_HARM && cls < CLIP_K_SILENT ? cls - CLIP_K_HARM : 5u);
    pc->lim = 4096;
    pc->g = 1.0f;
    pc->x = m->F > 0.0 ? clip_wrap(X + (double)dv, m->F) : X + (double)dv;
    pc->cyc = cls == CLIP_K_FREEZE ? (float)(2.0 * m->f16) : m->cyc;
    pc->x2 = cls == CLIP_K_STAB ? (m->F > 0.0 ? clip_wrap(pc->x + (double)j->off[1], m->F) : pc->x + (double)j->off[1])
                                : pc->x;
    if (pc->eng == CLIP_E_FLAT) {
        pc->pv = pc->pr = j->L < 0.0f ? -j->L : j->L;    /* one playhead keeps its rate in L (Rewind: backwards, < 0) */
        if (cls == CLIP_K_REW) pc->rev = j->L < 0.0f;
    } else {
        pc->pv = cls == CLIP_K_HALF ? 0.5f * v : cls == CLIP_K_FREEZE ? 0.0f : v;
        pc->pr = r * clip_cls_pitch(cls);
    }
}

/* Modern cycles whatever the method (harmony, Freeze, an octave in Vinyl),
 * from the incoming playhead's rate rb, the place's v and C file frames, as
 * Jungle's: C / |rb/v - 1| x (rb/v) / rb. Never stuck on one grain at the
 * clip's own speed (|rb/v - 1| taken as at least 0.25); frozen (v = 0) a
 * grain is C frames. */
static float clip_mod_cycle(float C, float rb, float v)
{
    float L;
    if (!(v > 1e-6f)) {
        L = C / rb;
    } else {
        float d = rb / v - 1.0f;
        if (d < 0.0f) d = -d;
        if (d < 0.25f) d = 0.25f;
        L = C / (d * v);
    }
    if (!(L >= CLIP_JGL_MIN_L)) L = CLIP_JGL_MIN_L;
    if (L > 1.0e9f) L = 1.0e9f;
    return L;
}

/* Start segment j's playheads at the piece's place: Jungle's A there and B
 * (v - r) x L off; Key Lock the same with B searched for -- or, for a join a
 * mode makes (seek 0, so no search in its block), B alone at the place and
 * the first search at its first join; Modern cycles with the first interval
 * next; one playhead keeps its rate in L. */
static void clip_eng_fresh(const clip_ram_t *c, int idx, const clip_io_t *io, clip_jgl_t *j, const clip_pc_t *pc,
                           int seek)
{
    float r = pc->pr, v = pc->pv;
    int flat;
    j->a = 0;
    j->plan = j->hit = 0;
    j->off[0] = 0.0f;
    j->t = 0.0f;
    if (pc->eng == CLIP_E_FLAT) {
        j->off[1] = 0.0f;
        j->L = r;
        return;
    }
    if (pc->eng == CLIP_E_MOD) {
        uint32_t hi = 1u % clip_harm_n(pc->harm);
        float rb = r * clip_harm_x(pc->harm, hi);
        j->hit = (uint8_t)hi;
        j->plan = (uint8_t)(hi << 2);
        j->L = clip_mod_cycle(pc->cyc, rb, v);
        j->off[1] = (v - rb) * j->L;
        return;
    }
    int kl = pc->eng == CLIP_E_KL;
    j->L = kl ? clip_kl_cycle(r, v, 0, &flat) : clip_jgl_cycle(c, idx, r, v, &flat);
    j->off[1] = flat ? 0.0f : (v - r) * j->L;
    if (kl && !flat) {
        if (seek) {
            j->off[1] += clip_kl_seek(io->pad, io->a3, pc->x, pc->x + (double)j->off[1], io->src, io->bl, io->br,
                                      io->stereo, io->frames, 0);
        } else {
            j->off[1] = 0.0f;
            j->t = clip_kl_fade(j->L);
        }
    }
}

/* n output samples of piece pc into (ol, orr): one playhead (backwards for
 * Reverse Swell: read forwards, then turned round), or Jungle's, Key Lock's or
 * the Modern cycles. */
static void clip_eng_run(const clip_ram_t *c, int idx, const clip_io_t *io, clip_jgl_t *j, const clip_pc_t *pc,
                         int32_t n, float *ol, float *orr)
{
    void *pad = io->pad, *a3 = io->a3, *src = io->src;
    float *bl = io->bl, *br = io->br;
    int stereo = io->stereo;
    int32_t frames = io->frames;
    float v = pc->pv, r = pc->pr;
    if (pc->eng == CLIP_E_FLAT) {
        j->L = r;
        if (pc->cls == CLIP_K_SILENT) {                  /* a chop mode's gap */
            for (int32_t i = 0; i < n; i++) { ol[i] = 0.0f; if (stereo) orr[i] = 0.0f; }
            return;
        }
        if (pc->cls == CLIP_K_STAB) {                    /* root + fifth, the fifth resampled half as fast again */
            clip_head_w(io, pc->x, r, n, ol, orr, io->canon);
            clip_head_w(io, pc->x2, 1.5f * r, n, bl, br, io->canon);
            for (int32_t i = 0; i < n; i++) {
                ol[i] = 0.7f * ol[i] + 0.5f * bl[i];
                if (stereo) orr[i] = 0.7f * orr[i] + 0.5f * br[i];
            }
            j->off[1] = (float)clip_near(pc->x2 + 1.5 * (double)r * (double)n - (pc->x + (double)r * (double)n), io->F);
            return;
        }
        if (!pc->rev) {
            clip_head_w(io, pc->x, r, n, ol, orr, io->canon);
            return;
        }
        clip_head_w(io, pc->x - (double)r * (double)(n - 1), r, n, ol, orr, io->canon);
        for (int32_t i = 0, k = n - 1; i < k; i++, k--) {
            float t = ol[i]; ol[i] = ol[k]; ol[k] = t;
            if (stereo) { t = orr[i]; orr[i] = orr[k]; orr[k] = t; }
        }
        if (pc->cls == CLIP_K_REW) j->L = -r;            /* Rewind keeps its direction in L's sign */
        return;
    }
    int kl = pc->eng == CLIP_E_KL, mod = pc->eng == CLIP_E_MOD;
    int oldskool = pc->eng == CLIP_E_JGL && c->jalg[idx] == CLIP_OLDSKOOL;
    int32_t done = 0;
    int flat;
    /* b27: a flat cycle (the clip at its own tempo, 1e9 frames long) ends as
     * soon as the tempo moves. b26 kept it: CC 113 starts Manual at the
     * clock's tempo, so a clip at its own tempo began flat and, slowed, the
     * playhead ran on at full speed through the clip's end (the owner: beeps
     * below about 73 BPM, the playback back at the pad's start). */
    if (!mod && j->L >= 1.0e8f) {
        float d = r / v - 1.0f;
        if (d > 1e-4f || d < -1e-4f) j->L = j->t;
    }
    /* b35: and a long Jungle cycle (near the clip's own tempo the stretch is
     * small, so C / |s - 1| is seconds) is shortened as soon as the stretch
     * asks for one under half what is left of it -- the owner: up to a second
     * between CC 113 and Manual's tempo, the old cycle playing out first. The
     * fade keeps its place (u = t / L) and ends within one new cycle. */
    if (pc->eng == CLIP_E_JGL && j->L < 1.0e8f && j->L > j->t) {
        int fl;
        float Ln = clip_jgl_cycle(c, idx, r, v, &fl);
        if (!fl && j->L - j->t > 2.0f * Ln) {
            float q = j->t / j->L, L2 = Ln / (1.0f - q);
            j->t = q * L2;
            j->L = L2;
        }
    }
    while (done < n) {
        double x = pc->x + (double)v * (double)done;
        if (kl && !j->plan && j->L >= CLIP_KL_PLANMIN && j->L < 1.0e8f && j->t >= j->L - clip_kl_move(j->L)) {
            j->plan = 1;                                         /* Key Lock: the join placed clear of hits */
            j->L = clip_kl_plan(pad, a3, x + (double)j->off[j->a ^ 1u], r, v, j->t, j->L, src, bl, br, stereo,
                                frames, &j->hit);
        }
        if (j->t >= j->L) {                                      /* the cycle's end: B carries on */
            j->t -= j->L;
            if (j->t >= j->L || j->t < 0.0f) j->t = 0.0f;
            j->a ^= 1u;
            uint32_t nb = j->a ^ 1u;
            if (mod) {                                           /* the next interval comes in */
                uint32_t hi = (j->hit + 1u) % clip_harm_n(pc->harm);
                float rb = r * clip_harm_x(pc->harm, hi);
                j->hit = (uint8_t)hi;
                j->plan = (uint8_t)((j->plan & ~(3u << (2u * nb))) | hi << (2u * nb));
                j->L = clip_mod_cycle(pc->cyc, rb, v);
                j->off[nb] = (v - rb) * j->L;
            } else {
                j->L = kl ? clip_kl_cycle(r, v, j->hit != 0, &flat) : clip_jgl_cycle(c, idx, r, v, &flat);
                j->plan = 0;
                j->off[nb] = flat ? j->off[j->a] : (v - r) * j->L;
                if (kl && !flat)
                    j->off[nb] += clip_kl_seek(pad, a3, x + (double)j->off[j->a], x + (double)j->off[nb], src, bl,
                                               br, stereo, frames, j->hit != 0);
                if (j->hit) j->hit--;
            }
        }
        float rest = j->L - j->t;
        int32_t len = n - done;
        if (rest < (float)len) { len = (int32_t)rest; if ((float)len < rest) len++; }
        float fade = kl ? clip_kl_fade(j->L) : 0.0f;
        if (kl && j->t < fade && fade - j->t < (float)len) {   /* to the fade's end, then B alone */
            len = (int32_t)(fade - j->t);
            if ((float)len < fade - j->t) len++;
        }
        if (len < 1) len = 1;
        uint32_t a = j->a, b = a ^ 1u;
        float ra = r, rb = r;
        if (mod) {
            ra = r * clip_harm_x(pc->harm, (j->plan >> (2u * a)) & 3u);
            rb = r * clip_harm_x(pc->harm, (j->plan >> (2u * b)) & 3u);
        }
        if (kl && j->t >= fade) {                                /* Key Lock after its fade: B alone */
            clip_head_w(io, x + (double)j->off[b], r, len, ol + done, stereo ? orr + done : 0, io->canon);
        } else {
        clip_head_w(io, x + (double)j->off[a], ra, len, ol + done, stereo ? orr + done : 0, io->canon);
        clip_head_w(io, x + (double)j->off[b], rb, len, bl, br, io->canon);
        float L = kl ? fade : j->L, u0 = j->t / L, du = 1.0f / L;
        if (kl) {                                                /* raised cosine: cos^2 / sin^2, by rotation */
            const float h = 1.57079632679489661923f;
            float gc = chr_cosf(u0 * h), gs = chr_sinf(u0 * h);
            float dc = chr_cosf(du * h), ds = chr_sinf(du * h);
            for (int32_t i = 0; i < len; i++) {
                float ga = gc * gc, gb = gs * gs;
                ol[done + i] = ga * ol[done + i] + gb * bl[i];
                if (stereo) orr[done + i] = ga * orr[done + i] + gb * br[i];
                float nc = gc * dc - gs * ds;
                gs = gs * dc + gc * ds;
                gc = nc;
            }
        } else if (oldskool) {
            for (int32_t i = 0; i < len; i++) {
                float u = u0 + du * (float)i, ga = 1.0f - u;
                ol[done + i] = ga * ol[done + i] + u * bl[i];
                if (stereo) orr[done + i] = ga * orr[done + i] + u * br[i];
            }
        } else {                                                 /* cos / sin, by rotation */
            const float h = 1.57079632679489661923f;
            float gc = chr_cosf(u0 * h), gs = chr_sinf(u0 * h);
            float dc = chr_cosf(du * h), ds = chr_sinf(du * h);
            for (int32_t i = 0; i < len; i++) {
                ol[done + i] = gc * ol[done + i] + gs * bl[i];
                if (stereo) orr[done + i] = gc * orr[done + i] + gs * br[i];
                float nc = gc * dc - gs * ds;
                gs = gs * dc + gc * ds;
                gc = nc;
            }
        }
        }
        j->off[a] += (ra - v) * (float)len;
        j->off[b] += (rb - v) * (float)len;
        j->t += (float)len;
        done += len;
    }
}

/* ---- joins: the old sound's tail ------------------------------------------------ */
static clip_tail_t *clip_tail_of(clip_ram_t *c, uint32_t own)
{
    for (uint32_t i = 0; i < CLIP_TAILS; i++)
        if (c->tail[i].own == own) return &c->tail[i];
    return 0;
}

static clip_tail_t *clip_tail_new(clip_ram_t *c, uint32_t own)
{
    clip_tail_t *tl = clip_tail_of(c, own);
    for (uint32_t i = 0; !tl && i < CLIP_TAILS; i++)
        if (!c->tail[i].own) tl = &c->tail[i];
    if (tl) {
        tl->k = 0;
        tl->own = (uint8_t)own;
        tl->g[1] = 0.0f;
        tl->stock = 0;
    }
    return tl;
}

/* Cut segment own's sound into a tail: its playheads where they are (dv, the
 * place, plus their offsets: frames from X), at their rates and their gains
 * now. eng, pr and harm are the piece it was playing; rev, it was playing
 * backwards, and its tail does too (a negative rate), so a Reverse Swell
 * turns round into its downbeat smoothly. */
static void clip_tail_cut(clip_ram_t *c, uint32_t own, const clip_jgl_t *j, uint32_t eng, int oldskool, float pr,
                          uint32_t harm, float dv, int rev, int stock, uint32_t cls)
{
    clip_tail_t *tl = clip_tail_new(c, own);
    if (!tl) return;                                     /* none free: a hard cut */
    tl->stock = (uint8_t)stock;
    uint32_t a = j->a & 1u, b = a ^ 1u;
    if (eng == CLIP_E_FLAT) {                            /* a gap's tail is silence: the new sound only fades in */
        tl->o[0] = dv; tl->r[0] = rev ? -j->L : j->L;
        tl->g[0] = cls == CLIP_K_SILENT ? 0.0f : cls == CLIP_K_STAB ? 0.7f : 1.0f;
        if (cls == CLIP_K_STAB) { tl->o[1] = dv + j->off[1]; tl->r[1] = 1.5f * j->L; tl->g[1] = 0.5f; }
        return;
    }
    const float h = 1.57079632679489661923f;
    float ga, gb;
    if (eng == CLIP_E_KL) {
        float fade = clip_kl_fade(j->L);
        if (j->t >= fade) { ga = 0.0f; gb = 1.0f; }
        else { float gc = chr_cosf(j->t / fade * h), gs = chr_sinf(j->t / fade * h); ga = gc * gc; gb = gs * gs; }
    } else {
        float u = j->t / j->L;
        if (!(u >= 0.0f)) u = 0.0f;
        if (u > 1.0f) u = 1.0f;
        if (oldskool) { ga = 1.0f - u; gb = u; }
        else { ga = chr_cosf(u * h); gb = chr_sinf(u * h); }
    }
    float ra = pr, rb = pr;
    if (eng == CLIP_E_MOD) {
        ra = pr * clip_harm_x(harm, (j->plan >> (2u * a)) & 3u);
        rb = pr * clip_harm_x(harm, (j->plan >> (2u * b)) & 3u);
    }
    tl->o[0] = dv + j->off[a]; tl->r[0] = ra; tl->g[0] = ga;
    tl->o[1] = dv + j->off[b]; tl->r[1] = rb; tl->g[1] = gb;
}

/* Under the n samples just played into (ol, orr), from X: segment own's tail,
 * if it has one, fading out (cos^2) as they fade in (sin^2). */
static void clip_tail_mix(clip_ram_t *c, uint32_t own, const clip_io_t *io, double X, float v, int32_t n,
                          float *ol, float *orr)
{
    clip_tail_t *tl = clip_tail_of(c, own);
    if (!tl) return;
    int32_t m = (int32_t)CLIP_TAIL_N - (int32_t)tl->k;
    if (m > n) m = n;
    if (m <= 0) { tl->own = 0; return; }
    const float h = 1.57079632679489661923f, du = h / (float)CLIP_TAIL_N;
    float u0 = (float)tl->k * du, dc = chr_cosf(du), ds = chr_sinf(du);
    int stereo = io->stereo;
    float *bl = io->bl, *br = io->br;
    for (uint32_t p = 0; p < 2u; p++) {
        float g = tl->g[p];
        int on = g > 1e-4f;
        if (p && !on) break;
        if (on && tl->r[p] < 0.0f) {                     /* backwards: read forwards, then turn it round */
            float rr = -tl->r[p];
            clip_head_w(io, X + (double)tl->o[p] - (double)rr * (double)(m - 1), rr, m, bl, br, !tl->stock);
            for (int32_t i = 0, k = m - 1; i < k; i++, k--) {
                float t = bl[i]; bl[i] = bl[k]; bl[k] = t;
                if (stereo) { t = br[i]; br[i] = br[k]; br[k] = t; }
            }
        } else if (on) {
            clip_head_w(io, X + (double)tl->o[p], tl->r[p], m, bl, br, !tl->stock);
        }
        float gc = chr_cosf(u0), gs = chr_sinf(u0);
        for (int32_t i = 0; i < m; i++) {
            float go = gc * gc * g;
            if (!p) {                                    /* the new sound fades in ... */
                float gi = gs * gs;
                ol[i] *= gi;
                if (stereo) orr[i] *= gi;
            }
            if (on) {                                    /* ... and the old fades out */
                ol[i] += go * bl[i];
                if (stereo) orr[i] += go * br[i];
            }
            float nc = gc * dc - gs * ds;
            gs = gs * dc + gc * ds;
            gc = nc;
        }
        tl->o[p] += (tl->r[p] - v) * (float)m;
    }
    tl->k = (uint16_t)(tl->k + m);
    if (tl->k >= CLIP_TAIL_N) tl->own = 0;
}

/* ---- 2.3.10-b17: the hit pool's scan ------------------------------------------ */
/* the median of 16 */
static float clip_hp_med(const float *x)
{
    float s[16];
    for (int i = 0; i < 16; i++) {
        float t = x[i];
        int k = i - 1;
        while (k >= 0 && s[k] > t) { s[k + 1] = s[k]; k--; }
        s[k + 1] = t;
    }
    return 0.5f * (s[7] + s[8]);
}

/* keep onset x, unless one is within `gap`; full, it replaces the one
 * farthest from the scan */
static void clip_hp_add(clip_hp_t *h, int32_t x, double F, double gap)
{
    uint32_t far = 0;
    double fd = -1.0;
    for (uint32_t i = 0; i < h->n; i++) {
        double d = clip_near((double)(h->on[i] - x), F);
        if (d < gap && d > -gap) return;
        double c = clip_near((double)(h->on[i] - h->cur), F);
        if (c < 0.0) c = -c;
        if (c > fd) { fd = c; far = i; }
    }
    if (h->n < CLIP_HP_N) h->on[h->n++] = x;
    else h->on[far] = x;
}

/* One step of the scan: CLIP_HP_HOP frames from h->cur through four bands
 * (one-pole splits at about 300 Hz, 1.5 kHz and 5 kHz), their energy's rise
 * in log, summed. A step whose flux is the highest of the five round it and
 * well above the recent median is an onset, placed at the 16 frames whose
 * energy rose most, 16 early. */
static void clip_hp_step(clip_hp_t *h, const clip_io_t *io, double F, double f16)
{
    float *l = io->bl, *r = io->br;
    float E[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, best = -1.0f, sub = h->sub;
    int32_t at = h->cur;
    for (int32_t c = 0; c < CLIP_HP_HOP; c += CLIP_JGL_MAX_N) {
        clip_head_w(io, (double)h->cur + (double)c, 1.0f, CLIP_JGL_MAX_N, l, io->stereo ? r : 0, 0);
        for (int32_t q = 0; q < CLIP_JGL_MAX_N; q += CLIP_HP_SUB) {
            float e = 0.0f;
            for (int32_t i = q; i < q + CLIP_HP_SUB; i++) {
                float x = io->stereo ? 0.5f * (l[i] + r[i]) : l[i];
                h->lp[0] += 0.0385f * (x - h->lp[0]);
                h->lp[1] += 0.178f * (x - h->lp[1]);
                h->lp[2] += 0.48f * (x - h->lp[2]);
                float b0 = h->lp[0], b1 = h->lp[1] - h->lp[0], b2 = h->lp[2] - h->lp[1], b3 = x - h->lp[2];
                E[0] += b0 * b0; E[1] += b1 * b1; E[2] += b2 * b2; E[3] += b3 * b3;
                e += x * x;
            }
            if (e - sub > best) { best = e - sub; at = h->cur + c + q; }
            sub = e;
        }
    }
    h->sub = sub;
    float fl = 0.0f;
    for (int k = 0; k < 4; k++) {
        float le = chr_logf(E[k] * (1.0f / (float)CLIP_HP_HOP) + 1.0e-8f);
        if (h->warm && le > h->le[k]) fl += le - h->le[k];
        h->le[k] = le;
    }
    for (int k = 0; k < 4; k++) { h->flux[k] = h->flux[k + 1]; h->cand[k] = h->cand[k + 1]; }
    h->flux[4] = fl;
    h->cand[4] = (int32_t)clip_wrap((double)(at - CLIP_HP_SUB), F);
    h->hist[h->hi & 15u] = fl;
    h->hi = (uint8_t)((h->hi + 1u) & 15u);
    if (h->warm < 255u) h->warm++;
    if (h->warm >= 6u) {
        float fc = h->flux[2], th = 1.5f * clip_hp_med(h->hist) + 0.1f * h->fmax;
        int peak = fc >= h->flux[0] && fc >= h->flux[1] && fc >= h->flux[3] && fc >= h->flux[4];
        double gap = h->last >= 0 ? clip_near((double)(h->cand[2] - h->last), F) : 1.0e30;
        if (gap < 0.0) gap = -gap;
        if (peak && fc > th && fc > 1.0f && gap >= 0.75 * f16) {
            clip_hp_add(h, h->cand[2], F, 0.75 * f16);
            h->last = h->cand[2];
        }
    }
    h->fmax = h->fmax * 0.9982f > fl ? h->fmax * 0.9982f : fl;
}

/* Once a block for a pad whose mode uses the pool: keep the scan up to a few
 * beats ahead of X, two steps a block while it is behind; out of reach (a
 * jump, or X faster than the scan), start it again at X. A loop wholly
 * scanned with room to spare is done. */
static void clip_hp_scan(clip_hp_t *h, const clip_io_t *io0, double X, double F, double f16)
{
    if (!(F > 0.0 && F < 2.0e9 && f16 > 0.0)) return;
    clip_io_t io = *io0;
    io.F = F; io.canon = 0;
    if (h->F != (int32_t)F) {
        h->n = 0; h->F = (int32_t)F; h->ckey = 0;
        h->scanned = 0; h->warm = 0; h->last = -1; h->cur = (int32_t)clip_wrap(X, F);
    }
    if (h->scanned >= h->F && h->n < CLIP_HP_N) return;
    double lead = 8.0 * f16, d = clip_near((double)h->cur - X, F);
    if (d < -4.0 * f16 || d > lead + 4.0 * f16) {
        h->cur = (int32_t)clip_wrap(X, F);
        h->scanned = 0; h->warm = 0; h->last = -1;
        d = 0.0;
    }
    if (d >= lead) return;
    for (int k = d < 0.5 * lead ? 2 : 1; k > 0; k--) {
        clip_hp_step(h, &io, F, f16);
        h->cur = (int32_t)clip_wrap((double)h->cur + (double)CLIP_HP_HOP, F);
        if (h->scanned < 0x7fff0000) h->scanned += CLIP_HP_HOP;
    }
}

/* the modes that play from the pool */
static int clip_hp_mode(uint32_t jm)
{
    return jm == CLIP_JM_SUBTLE || jm == CLIP_JM_LIFT || jm == CLIP_JM_STABS || jm == CLIP_JM_PROLL
           || (jm >= CLIP_JM_MIXS && jm <= CLIP_JM_MIXW);
}

/* ---- 2.3.10-b18: Reverse Reverb ------------------------------------------------ */
static const uint16_t clip_rv_cd[6] = { 1557, 1617, 1491, 1422, 1277, 1356 };   /* the combs, samples */
static const uint16_t clip_rv_ad[2] = { 556, 341 };                             /* the allpasses */

/* the phrase's bars a swell rises into: the 3rd (short) and the 5th (medium) of each 8 */
static uint32_t clip_rv_kind(int32_t ib) { int32_t q = ib & 7; return q == 2 ? 1u : q == 4 ? 2u : 0u; }

static void clip_rv_start(clip_rv_t *r, int idx, double adb, uint32_t kind, double f, float v)
{
    double beat = 4.0 * f / (double)v, n = (kind == 1u ? 0.5 : 1.0) * beat;
    float rt = kind == 1u ? 1.0f : 1.8f;             /* seconds to -60 dB */
    r->own = (int8_t)idx; r->adb = adb; r->kind = (uint8_t)kind; r->ph = 1; r->k = 0;
    r->D = beat > (double)CLIP_RV_MAX ? CLIP_RV_MAX : (int32_t)beat;
    r->N = n > (double)CLIP_RV_MAX ? CLIP_RV_MAX : (int32_t)n;
    r->dpk = 0.0f; r->ypk = 0.0f;
    for (int i = 0; i < 6; i++) {
        r->g[i] = chr_exp2f(-9.96578428f * (float)clip_rv_cd[i] / (rt * 48000.0f));   /* 10^(-3 d / (rt sr)) */
        r->cs[i] = 0.0f; r->ci[i] = 0;
    }
    r->ai[0] = r->ai[1] = 0;
    for (int i = 0; i < CLIP_RV_DL; i++) r->dl[i] = 0.0f;
}

static float clip_rv_tick(clip_rv_t *r, float x)
{
    float *d = r->dl, y = 0.0f;
    int off = 0;
    for (int i = 0; i < 6; i++) {                    /* damped combs, in parallel */
        float o = d[off + r->ci[i]];
        r->cs[i] = 0.7f * o + 0.3f * r->cs[i];
        d[off + r->ci[i]] = x + r->g[i] * r->cs[i];
        if (++r->ci[i] >= clip_rv_cd[i]) r->ci[i] = 0;
        y += o;
        off += clip_rv_cd[i];
    }
    for (int i = 0; i < 2; i++) {                    /* then two allpasses */
        float o = d[off + r->ai[i]];
        d[off + r->ai[i]] = y + 0.5f * o;
        y = o - y;
        if (++r->ai[i] >= clip_rv_ad[i]) r->ai[i] = 0;
        off += clip_rv_ad[i];
    }
    return y;
}

/* CLIP_RV_STEP samples of a swell's making: the downbeat's beat, read as the
 * pad plays it, into the reverb backwards; then the tail, kept turned round */
static void clip_rv_step(clip_rv_t *r, const clip_io_t *io0, double F, float rb)
{
    clip_io_t io = *io0;
    io.F = F; io.canon = 0;
    int32_t todo = CLIP_RV_STEP;
    float *l = io.bl, *rr = io.br;
    while (todo > 0 && r->ph == 1u) {
        int32_t left = r->D - r->k;
        if (left <= 0) { r->ph = 2; r->k = 0; break; }
        int32_t len = left < CLIP_JGL_MAX_N ? left : CLIP_JGL_MAX_N;
        if (len > todo) len = todo;
        clip_head_w(&io, clip_wrap(r->adb, F) + (double)rb * (double)(r->D - r->k - len), rb, len, l,
                    io.stereo ? rr : 0, 0);
        for (int32_t i = len - 1; i >= 0; i--) {
            float x = io.stereo ? 0.5f * (l[i] + rr[i]) : l[i];
            float a = x < 0.0f ? -x : x;
            if (a > r->dpk) r->dpk = a;
            clip_rv_tick(r, x);
        }
        r->k += len; todo -= len;
    }
    while (todo > 0 && r->ph == 2u) {
        int32_t left = r->N - r->k;
        if (left <= 0) { r->ph = 3; break; }
        int32_t len = left < todo ? left : todo;
        for (int32_t i = 0; i < len; i++) {
            float y = clip_rv_tick(r, 0.0f), a = y < 0.0f ? -y : y;
            if (a > r->ypk) r->ypk = a;
            r->buf[r->N - 1 - (r->k + i)] = y;
        }
        r->k += len; todo -= len;
        if (r->k >= r->N) r->ph = 3;
    }
}

/* Once a block for a Reverse Reverb pad's newest segment: let a slot go once
 * its swell has played (or it is stale, or not ready in time), take one in
 * the first half of the bar before a swell's downbeat, and make it. Returns
 * the pad's slot, if any. */
static clip_rv_t *clip_rv_run(clip_hpb_t *b, int idx, const clip_io_t *io, const clip_mx_t *m, float rb)
{
    clip_rv_t *r = 0;
    for (uint32_t k = 0; k < CLIP_RV_SLOTS; k++)
        if (b->rv[k].own == idx) r = &b->rv[k];
    double f = m->f16, A = m->A, bar = __builtin_floor(A / (16.0 * f));
    if (r && (m->jm != CLIP_JM_REVERB || A >= r->adb || A < r->adb - 32.0 * f
              || (r->ph != 3u && A >= r->adb - (double)r->N * (double)m->v))) {
        r->own = -1;
        r = 0;
    }
    if (m->jm != CLIP_JM_REVERB) return 0;
    uint32_t kind = clip_rv_kind((int32_t)bar + 1);
    double adb = (bar + 1.0) * 16.0 * f;
    clip_hp_t *h = &b->p[idx];
    /* b19: made in the beat before its swell starts, so the beat it is made
     * from is at most two ahead (b18 began a bar early) */
    double na = (kind == 1u ? 2.0 : 4.0) * f;
    if (!r && kind && h->rvdone != adb && A >= adb - na - 4.0 * f && A < adb - na) {
        for (uint32_t k = 0; !r && k < CLIP_RV_SLOTS; k++)
            if (b->rv[k].own < 0) r = &b->rv[k];
        if (r) { clip_rv_start(r, idx, adb, kind, f, m->v); h->rvdone = adb; }
    }
    if (r && r->ph < 3u) clip_rv_step(r, io, m->F, rb);
    return r;
}

/* the ready swell over the n samples just played, from A: faded in over its
 * length, out over its last 192, peaking as the beat it was made from */
static void clip_rv_mix(const clip_rv_t *r, double A, float v, int32_t n, float *ol, float *orr, int stereo)
{
    double d0 = (r->adb - A) / (double)v;            /* output samples to the downbeat */
    if (r->ph != 3u || !(r->ypk > 0.0f) || d0 <= 0.0 || d0 - (double)n >= (double)r->N) return;
    float g = r->dpk / r->ypk;
    const float pi = 3.14159265358979f;
    for (int32_t i = 0; i < n; i++) {
        double d = d0 - (double)i;
        if (d <= 0.0) break;
        if (d > (double)r->N) continue;
        int32_t j = r->N - (int32_t)(d + 0.5);
        if (j < 0 || j >= r->N) continue;
        float w = 0.5f - 0.5f * chr_cosf(pi * (float)j / (float)r->N);
        if (d < 192.0) w *= 0.5f - 0.5f * chr_cosf(pi * (float)d * (1.0f / 192.0f));
        float y = g * w * r->buf[j];
        ol[i] += y;
        if (stereo) orr[i] += y;
    }
}

/* The loop's pass for playhead array k of pad position idx, its X at 16th sx
 * of sl: kept by the pad's newest segment, which a fresh segment becomes; a
 * fresh start more than half the loop back is the next pass. The older of two
 * overlapping segments, still more than half a loop past the newest, is a
 * pass behind. The first count is made from the clock's bar, so phrases fall
 * where the song's do. */
static uint16_t clip_pass0(uint32_t clk, double X, double F, const uint8_t *pad_)
{
    const uint8_t *pad = __builtin_assume_aligned(pad_, 4);
    int64_t T;
    __builtin_memcpy(&T, pad + 0xb90, 8);
    if (!clip_clk_ok(clk) || T <= 0 || T > 0x7fffffff || !(F > 0.0)) return 0;
    uint32_t lo = *(volatile const uint32_t *)(uintptr_t)(clk + 0x30u);
    int32_t hi = *(volatile const int32_t *)(uintptr_t)(clk + 0x34u);
    double tick = (double)lo + (double)hi * 4294967296.0, Tt = (double)(int32_t)T;
    double p = __builtin_floor((tick - X * Tt / F) / Tt + 0.5);
    if (!(p >= 0.0 && p < 1.0e15)) return 0;
    return (uint16_t)(uint32_t)(p - __builtin_floor(p / 65536.0) * 65536.0);
}

static uint16_t clip_pass(clip_ram_t *c, int idx, uint32_t k, int fresh, uint32_t sx, uint32_t sl, double X, double F,
                          const uint8_t *pad)
{
    uint32_t b = c->btn[idx], me = k ? CLIP_B_NEW1 : 0u;
    if (!(b & CLIP_B_PASS)) {
        c->pass[idx] = clip_pass0(c->clk, X, F, pad);
        c->btn[idx] = (uint8_t)((b & ~CLIP_B_NEW1) | CLIP_B_PASS | me);
        c->plast[idx] = (uint16_t)sx;
        return c->pass[idx];
    }
    if (fresh) {
        if (sx + sl / 2u < c->plast[idx]) c->pass[idx]++;
        c->btn[idx] = (uint8_t)((b & ~CLIP_B_NEW1) | me);
        c->plast[idx] = (uint16_t)sx;
        return c->pass[idx];
    }
    if ((b & CLIP_B_NEW1) == me) {
        c->plast[idx] = (uint16_t)sx;
        return c->pass[idx];
    }
    return sx > c->plast[idx] + sl / 2u ? (uint16_t)(c->pass[idx] - 1u) : c->pass[idx];
}

/* The segment renderer's read (stock 0x080b55ac's arguments): for a Jungle
 * or Key Lock segment, or one a Jungle Mode plays, the piece by piece sound
 * into stock's destination buffer, joins faded; X advanced at `rate` exactly
 * as stock's resampler would advance it. Anything else: stock's read,
 * untouched. */
#ifdef CHR_SC
static void clip_cv_flip(clip_ram_t *c, const uint8_t *pad);   /* sidechain.h: CV Action Flip In n */
static void clip_row_mark(int idx, uint32_t method);           /* sidechain.h: the Clip Tempo row (b21) */
static uint32_t clip_cv_input(int idx);                         /* sidechain.h: the Flip In n turning it (b24) */
void clip_kind(const uint8_t *pad);                             /* sidechain.h: is it a Clip pad, for gdoes (b25) */
#endif

uint32_t clip_read(float rate, uint8_t *pad, void *a3, int32_t pos, int32_t poshi, int32_t n,
                   int32_t *adv, float *frac, void *src, void *dst_, void **out)
{
    clip_ram_t *c = clip_ram();
#ifdef CHR_SC
    clip_kind(pad);                                           /* b25: a Clip pad, for the Gate hook */
    clip_cv_flip(c, pad);                                     /* b20: before this block's read */
#endif
    int idx;
    uint8_t *seg = (uint8_t *)frac - 0x1c;
    uint32_t si = (uint32_t)(seg - (pad + 0x30u)) / 0x70u;
    clip_jgl_t *j = 0;
    uint32_t method = 0;
    int live = 0;
    if (c->magic == CLIP_MAGIC && clip_on(c, pad, &idx)
        && si < 4u && seg == pad + 0x30u + si * 0x70u && n > 0 && n <= CLIP_JGL_MAX_N && rate > 0.0f) {
        method = c->mode[idx] & CLIP_METHOD;
        live = clip_jm_live(c, idx);
        j = clip_jgl(pad, idx, si, 0);
        if (j && !clip_heads(method) && !live && !(j->tag & CLIP_T_ENG)) j = 0;   /* Vinyl: stock's read */
    }
    float r = j && clip_heads(method) ? c->jr[idx][si] : rate;
    if (!j || !(r > 0.0f && r <= CLIP_M_MAX))
        return clip_x_read(rate, pad, a3, pos, poshi, n, adv, frac, src, dst_, out);
    int32_t frames = clip_frames(pad);
    if (frames <= 0 || pos > frames + 0xff) return 0;   /* as stock: past the end */
    clip_desc_t *dst = dst_;
    uint32_t smp = *(const uint32_t *)(pad + 0x24);
    int stereo = *(volatile const uint8_t *)(uintptr_t)(smp + 0x1cu) != 0;
    float *ol = dst->l, *orr = dst->r;
    float bl[CLIP_JGL_MAX_N], br[CLIP_JGL_MAX_N];
    float v = rate, x0f = *frac;
    clip_io_t io = { pad, a3, src, bl, br, stereo, frames, 0.0, 0 };
    uint32_t ka = j == (clip_jgl_t *)(uintptr_t)CLIP_JGL_ADDR0 + idx ? 0u : 1u;   /* its playhead array */
    uint32_t own = 0x80u | ka << 4 | (uint32_t)idx;
    float *dv = (float *)(uintptr_t)CLIP_DV_ADDR + ka * CLIP_PADS + (uint32_t)idx;
    int fresh = j->xpos != pos;
    if (fresh && clip_heads(method)) j->tag &= (uint8_t)~CLIP_T_ENG;   /* a new segment reads b12's way ... */
    int was = (j->tag & CLIP_T_ENG) != 0;                    /* ... until a mode plays it (Vinyl: engaged) */
    double X0 = (double)pos + (double)x0f;
    clip_mx_t m;
    m.F = 0.0; m.f16 = 0.0; m.cyc = (float)CLIP_CYC_DEF;
    m.hp = 0; m.idx = idx; m.rend = 0.0;
    int grid = clip_grid(pad, &m.F, &m.f16);
    if (grid) {
        double sx = __builtin_floor(X0 / m.f16), sl = __builtin_floor(m.F / m.f16 + 0.5);
        uint32_t pass = clip_pass(c, idx, ka, fresh, sx < 0.0 ? 0u : sx > 65535.0 ? 65535u : (uint32_t)sx,
                                  sl > 65535.0 ? 65535u : (uint32_t)sl, X0, m.F, pad);
        m.A = (double)pass * m.F + X0;
        m.method = method;
        m.jm = live && clip_jm_built(c->jmode[idx]) ? c->jmode[idx] : CLIP_JM_OFF;
        m.btn = live ? c->btn[idx] : 0u;
        m.dk = clip_deck(c, idx);
        m.v = v; m.rb = r;
        m.cyc = (float)c->jcyc[idx];
        if (!(m.cyc >= (float)CLIP_CYC_MIN && m.cyc <= (float)CLIP_CYC_MAX)) m.cyc = (float)CLIP_CYC_DEF;
        m.e = 0.0; m.src = 0.0;
        m.X = X0;
        m.hp = clip_hp(c, idx); m.idx = idx; m.rend = 0.0;
        if (live && m.hp && m.hp->rew && !(m.btn & CLIP_B_HELD)) m.btn |= CLIP_B_REW;   /* Rewind let go: on to the beat */
        if (m.btn & CLIP_B_TAKE) {                               /* a button went down: its press point */
            clip_pc_t p0;
            uint32_t bt = m.btn;
            m.btn = 0;
            clip_piece(&m, &p0);
            m.btn = bt;
            c->rsrc[idx] = (int32_t)p0.x;
            c->rpass[idx] = (uint16_t)pass;
            c->rx[idx] = (int32_t)X0;
            c->btn[idx] &= (uint8_t)~CLIP_B_TAKE;
            if (m.hp) { m.hp->rl = 0.0f; m.hp->rr = 0.0f; m.hp->rew = 0; m.hp->ue = -1.0; }
            for (uint32_t s4 = 0; s4 < 4u; s4++) ((float *)(uintptr_t)CLIP_ELAST_RAM)[4u * (uint32_t)idx + s4] = -1.0f;
        }
        if (m.btn & CLIP_B_HELD) {
            m.src = (double)c->rsrc[idx];
            m.e = (double)(uint16_t)(pass - c->rpass[idx]) * m.F + (X0 - (double)c->rx[idx]);
            if (!(m.e >= 0.0)) m.e = 0.0;
            /* b28, the owner's recording: a beep is one block of 128 samples
             * played over and over -- the piece's place, made from e, was the
             * same block after block (e held at 0, or X held, across the loop
             * coming round). Freeze, which needs no e, never beeped. So e
             * never stands still while a button plays: one block of X on at
             * the least. A press starts it afresh (above). */
            float *el = (float *)(uintptr_t)CLIP_ELAST_RAM + 4u * (uint32_t)idx + (si & 3u);
            double step = (double)v * (double)n;
            if (*el >= 0.0f && *el < 1.0e9f && m.e < (double)*el + 0.25 * step) m.e = (double)*el + step;
            *el = (float)m.e;
        }
        if (m.hp && m.hp->rew) {                                 /* the 16th it drops back in on */
            if (m.hp->rew == 1u && (m.dk == CLIP_D_POWER || m.dk == CLIP_D_BRAKE)) {
                /* b36: spun up as its motor does (1.5 beats, at most 1.5 s: the
                 * Technics' 0.7 s at 134), from where it is; stopped, and so
                 * silent, it is first put where the spin-up lands on X -- at
                 * most 4 16ths ahead, the audio the cache holds */
                double U = clip_dk_len(&m, 6.0, 1.5);
                clip_dk_t d = clip_dk(&m, m.e);
                double up = d.p;
                if (!(d.s > 0.02f)) {
                    double kx = (double)(m.v / m.rb), x0 = (double)c->rx[idx] - m.src;
                    up = kx * (x0 + m.e + U) - 0.5 * U;
                    double cap = kx * (x0 + m.e + 4.0 * m.f16);
                    if (up > cap) up = cap;
                    d.s = 0.0f;
                }
                m.hp->ue = m.e; m.hp->up = up; m.hp->us = d.s;
                m.hp->rend = (float)(m.e + U);
                m.hp->rew = 2;
            }
            if (m.hp->rew == 1u) {                               /* b22: a 16th, not a beat (the owner's choice:
                                                                    below the clip's tempo a beat ran on ~1.5 s) */
                double end = __builtin_ceil(m.A / m.f16) * m.f16;
                m.hp->rend = (float)(m.e + (end - m.A));
                m.hp->rew = 2;
            }
            m.rend = (double)m.hp->rend;
            if (!(m.rend > m.e)) { m.hp->rew = 0; m.btn &= ~CLIP_B_REW; m.rend = 0.0; }
        }
    } else {
        live = 0;                                                /* slice markers: the modes stand aside */
    }
    if (fresh) {
        clip_tail_t *tl = clip_tail_of(c, own);                  /* the slot's last segment's */
        if (tl) tl->own = 0;
    }
    int oldskool = c->jalg[idx] == CLIP_OLDSKOOL;
    uint32_t cls = (j->tag >> 2) & 15u;
    clip_hp_t *hp = grid ? m.hp : 0;
    if (grid && (live || (cls != CLIP_K_PLAIN && cls != CLIP_K_NONE) || *dv != 0.0f || clip_tail_of(c, own)))
        io.F = m.F;                                              /* a mode's reads run on round the loop */
    io.canon = io.F > 0.0 || was;
    double mA = m.A, me = m.e;
    int32_t hold;
    __builtin_memcpy(&hold, (const uint8_t *)__builtin_assume_aligned(seg, 4) + 0x28, 4);
    int fading = !fresh && hold <= 0 && cls < CLIP_K_NONE && (live || cls != CLIP_K_PLAIN);
    /* the pitch the last piece played at: a Pitch Roll repeat's, in Jungle
     * and Key Lock, is the method's at a semitone or more off (b19) */
    float lpr = !fresh && hp && hp->lpr[ka] > 0.0f && hp->lpr[ka] < CLIP_M_MAX ? hp->lpr[ka] : r * clip_cls_pitch(cls);
    int newest = ka == ((c->btn[idx] & CLIP_B_NEW1) ? 1u : 0u);
    if (hp && live && !fading && clip_hp_mode(m.jm) && newest)
        clip_hp_scan(hp, &io, X0, m.F, m.f16);                    /* once a block: the pad's newest segment */
    clip_hpb_t *hb = hp && live && !fading && newest ? clip_hpb(c) : 0;
    clip_rv_t *rv = hb ? clip_rv_run(hb, idx, &io, &m, r) : 0;
    int32_t done = 0;
    while (done < n) {
        clip_pc_t pc;
        double Xd = X0 + (double)v * (double)done;
        if (fading) {
            clip_piece_on(&pc, cls, method, j, Xd, *dv, v, cls == CLIP_K_PROLL ? lpr : r, &m);
            if (cls == CLIP_K_REW) pc.g = clip_rew_level(j, r);  /* b36: a deck's fade-out at its level */
        } else if (live) {
            m.A = mA + (double)v * (double)done;
            m.e = me + (double)v * (double)done;
            m.X = Xd;
            clip_piece(&m, &pc);
        } else {
            pc.x = Xd; pc.pv = v; pc.pr = r; pc.cyc = 0.0f; pc.lim = n; pc.g = 1.0f;
            pc.cls = CLIP_K_PLAIN; pc.rev = 0; pc.harm = 5;
            pc.eng = (uint8_t)clip_eng_of(CLIP_K_PLAIN, method);
        }
        int32_t len = n - done;
        if (pc.lim < len) len = pc.lim;
        if (fresh) {
            int32_t fi;
            __builtin_memcpy(&fi, (const uint8_t *)__builtin_assume_aligned(seg, 4) + 0x24, 4);
            if (!clip_heads(method) && !(j->tag & CLIP_T_ENG) && fi < 256) {
                clip_tail_t *tl = clip_tail_new(c, own);         /* Vinyl: stock was playing it, at X */
                if (tl) { tl->o[0] = 0.0f; tl->r[0] = v; tl->g[0] = 1.0f; tl->stock = 1; }
            }
            clip_eng_fresh(c, idx, &io, j, &pc, 1);
            fresh = 0;
        } else if (pc.cls != cls || clip_near(pc.x - (Xd + (double)*dv), m.F) > 2.0
                   || clip_near(pc.x - (Xd + (double)*dv), m.F) < -2.0) {
            if (cls < CLIP_K_NONE) {                             /* a join: the old sound into a tail */
                float gl = cls == CLIP_K_REW ? clip_rew_level(j, r) : 1.0f;
                clip_tail_cut(c, own, j, clip_eng_of(cls, method), oldskool,
                              cls == CLIP_K_PROLL ? lpr : r * clip_cls_pitch(cls),
                              cls >= CLIP_K_HARM && cls < CLIP_K_SILENT ? cls - CLIP_K_HARM : 5u, *dv,
                              cls == CLIP_K_REV, !was, cls);
                clip_tail_t *tq = cls == CLIP_K_REW ? clip_tail_of(c, own) : 0;
                if (tq) tq->g[0] *= gl;                          /* b36: a deck's tail at its level */
            }
            clip_eng_fresh(c, idx, &io, j, &pc, 0);
        }
        cls = pc.cls;
        lpr = pc.pr;
        clip_eng_run(c, idx, &io, j, &pc, len, ol + done, stereo ? orr + done : 0);
        if (pc.cls == CLIP_K_REW && hp && (m.dk == CLIP_D_PULLUP || m.dk == CLIP_D_SCRATCH)) {   /* Pull Up's,
                                                                    Baby Scratch's top end dulled (b36: not the decks') */
            /* b34: 3.5 kHz, lowered as it speeds up (3.5 kHz / the speed), so
             * the sped-up sound is never brighter than the break: 1 - e^-x */
            float sp = m.rb > 0.0f ? pc.pr / m.rb : 1.0f, a = CLIP_REW_LP;
            if (sp > 1.0f) { float x = CLIP_REW_LPX / sp; a = x - 0.5f * x * x + x * x * x * (1.0f / 6.0f); }
            float yl = hp->rl, yr = hp->rr;
            for (int32_t i = 0; i < len; i++) {
                yl += a * (ol[done + i] - yl); ol[done + i] = yl;
                if (stereo) { yr += a * (orr[done + i] - yr); orr[done + i] = yr; }
            }
            hp->rl = yl; hp->rr = yr;
        }
        if (pc.g < 1.0f) {                                       /* and fading at the end of its reach */
            for (int32_t i = 0; i < len; i++) { ol[done + i] *= pc.g; if (stereo) orr[done + i] *= pc.g; }
        }
        clip_tail_mix(c, own, &io, Xd, v, len, ol + done, stereo ? orr + done : 0);
        double xe = pc.rev ? pc.x - (double)pc.pr * (double)len : pc.x + (double)pc.pv * (double)len;
        *dv = (float)clip_near(xe - (Xd + (double)v * (double)len), m.F);
        done += len;
    }
    if (rv) clip_rv_mix(rv, mA, v, n, ol, stereo ? orr : 0, stereo);   /* Reverse Reverb, over the groove */
    if (hp) hp->lpr[ka] = lpr;
    /* Vinyl hands the segment back to stock's read once it plays at X again,
     * its last join faded -- unless at rate 1, where stock would read from its
     * source buffer, a frame sooner than the modes do (clip_head_w). Jungle
     * and Key Lock keep reading the modes' way once a mode has played it. */
    uint32_t eng = 0u;
    if (clip_heads(method) ? io.canon
                           : (live || cls != CLIP_K_PLAIN || *dv > 0.5f || *dv < -0.5f || clip_tail_of(c, own)
                              || (v == 1.0f && was)))
        eng = CLIP_T_ENG;
    j->tag = (uint8_t)(0x80u | eng | cls << 2 | si);
    /* X: where stock's resampler (0x0809ff64) would leave it, step for step in
     * single precision, so its slicing, holds and the steering run on exactly
     * what Vinyl plays. Stock stops early only when its source buffer runs out
     * (frames needed against the buffer's capacity). */
    const clip_desc_t *sd = src;
    int32_t need = (int32_t)((float)n * v) + 8, whole = 0;
    if (need > sd->cap) need = sd->cap;
    float f = x0f;
    for (int32_t i = 0; i < n; i++) {
        f += v;
        if (f >= 1.0f) {
            int32_t k = (int32_t)f;
            whole += k;
            f -= (float)k;
            if (need <= whole + 4) break;
        }
    }
    *adv = whole;
    *frac = f;
    j->xpos = pos + whole;
    dst->frames = dst->cap;
    dst->f0 = 0;
    dst->f1 = stereo ? 1u : 0u;
    *out = dst;
    return 1;
}

/* Move every playing segment to where its slice would be at the new rate:
 * start + (pos - start) x new / old, the same musical time into the slice.
 * Off's rate is the segment's base x the Pitch knob's factor (modulation
 * aside); Vinyl's is m x the file's rate. */
static void clip_remap(uint8_t *pad_, int to_vinyl, float m)
{
    uint8_t *pad = __builtin_assume_aligned(pad_, 4);
    float sr; int32_t pitch;
    __builtin_memcpy(&sr, pad + 0x1f0, 4);
    __builtin_memcpy(&pitch, pad + 0xa80, 4);
    if (!(sr > 0.0f && sr < 8.0f) || pitch < -48000 || pitch > 48000) return;
    float pf = chr_exp2f((float)pitch * (1.0f / 12000.0f));
    for (uint32_t i = 0; i < 4u; i++) {
        uint8_t *s = __builtin_assume_aligned(pad + 0x30u + i * 0x70u, 4);
        int32_t delay; uint32_t k; int64_t pos, end; float frac, base;
        __builtin_memcpy(&delay, s + 0x20, 4);
        if (!s[0] || s[1] || delay > 0) continue;                 /* idle, finishing, or not started */
        __builtin_memcpy(&base, s + 0x18, 4);
        float off = base * pf, vin = m * sr;
        if (!(off > 0.0f && vin > 0.0f)) continue;
        float ratio = to_vinyl ? vin / off : off / vin;
        if (ratio == 1.0f) continue;
        __builtin_memcpy(&k, s + 0x34, 4);
        __builtin_memcpy(&pos, s + 8, 8);
        __builtin_memcpy(&end, s + 0x10, 8);
        __builtin_memcpy(&frac, s + 0x1c, 4);
        int64_t start = clip_slice_start(pad, k);
        if (start < 0 || pos < start || pos - start > 0x7fffffff) continue;
        double rel = ((double)(int32_t)(pos - start) + (double)frac) * (double)ratio;
        double whole = __builtin_floor(rel);
        if (!(whole >= 0.0 && whole < 2147483647.0)) continue;
        int64_t np = start + (int32_t)whole;
        float nf = (float)(rel - whole);
        if (end != 0 && np >= end) { np = end - 1; nf = 0.0f; }
        __builtin_memcpy(s + 8, &np, 8);
        __builtin_memcpy(s + 0x1c, &nf, 4);
    }
}

/* Leaving Manual: start the slice the clock's bar says should be playing, at
 * that point in it (the clock's transport tick +0x30, modulo the loop's T),
 * and fade the old segments out over stock's 256 samples. The note keeps its
 * segment list at +0xc8 (count +0xd8); stock queues the next slice itself once
 * the old one has finished. Nothing happens for slice markers or a pad with no
 * free segment. */
static void clip_drop_in(uint8_t *pad_, uint32_t clk, float bpm)
{
    uint8_t *pad = __builtin_assume_aligned(pad_, 4);
    uint8_t *note = 0;
    for (uint32_t n = 0; n < 2u; n++) {
        uint8_t *nt = __builtin_assume_aligned(pad + (n ? 0x670u : 0x260u), 4);
        int32_t cnt;
        __builtin_memcpy(&cnt, nt + 0xd8, 4);
        if (nt[0] && cnt > 0 && cnt <= 4) { note = nt; break; }
    }
    if (!note || !clip_clk_ok(clk) || !(bpm > 0.0f)) return;
    int64_t T, tps, tick, sync; uint32_t N, nmark; float src;
    __builtin_memcpy(&T, pad + 0xb90, 8);
    __builtin_memcpy(&tps, pad + 0x208, 8);
    __builtin_memcpy(&N, pad + 0x210, 4);
    __builtin_memcpy(&src, pad + 0x1fc, 4);
    __builtin_memcpy(&sync, pad + 0x220, 8);
    __builtin_memcpy(&nmark, pad + 0x250, 4);
    __builtin_memcpy(&tick, (const uint8_t *)(uintptr_t)(clk + 0x30u), 8);
    if (pad[0x1f5] && sync == 0xf0000 && nmark != 0u) return;
    if (T <= 0 || T > 0x7fffffff || tps <= 0 || tps > T || N == 0u || N > 1024u
        || tick < 0 || tick > 0x7fffffff || !(src > 0.0f)) return;
    int32_t ph = (int32_t)tick % (int32_t)T, tp = (int32_t)tps;
    uint32_t k = (uint32_t)(ph / tp);
    if (k >= N) k = N - 1u;
    int32_t rem = ph - (int32_t)k * tp;
    int32_t old[4], nold;
    __builtin_memcpy(&nold, note + 0xd8, 4);
    __builtin_memcpy(old, note + 0xc8, 16);
    int32_t notenum;
    __builtin_memcpy(&notenum, note + 4, 4);
    int32_t si = clip_x_start(pad, note, 0, k, clk, notenum, 0);
    if (si < 0 || si > 3) return;
    uint8_t *s = __builtin_assume_aligned(pad + 0x30u + (uint32_t)si * 0x70u, 4);
    int64_t pos;
    __builtin_memcpy(&pos, s + 8, 8);
    pos += (int32_t)((float)rem / (float)tp * src);
    float zero = 0.0f;
    __builtin_memcpy(s + 8, &pos, 8);
    __builtin_memcpy(s + 0x1c, &zero, 4);
    int64_t end;
    __builtin_memcpy(&end, s + 0x10, 8);
    if (end == 0) {                         /* a slice with a next one: hold to its end */
        float spt = 3000.0f / bpm;          /* output samples per tick */
        float h = (float)(tp - rem) * spt - 256.0f;
        int32_t hi = h > 0.0f ? (h < 2.0e9f ? (int32_t)h : 0x7fffffff) : 0;
        __builtin_memcpy(s + 0x28, &hi, 4);
    }
    int32_t keep[4], nk = 0;
    for (int32_t j = 0; j < nold && j < 4; j++) {
        if (old[j] < 0 || old[j] > 3 || old[j] == si) continue;
        uint8_t *o = __builtin_assume_aligned(pad + 0x30u + (uint32_t)old[j] * 0x70u, 4);
        int32_t d;
        __builtin_memcpy(&d, o + 0x20, 4);
        if (d > 0) { o[0] = 0; o[1] = 0; continue; }      /* queued: cancelled, as stock does */
        int64_t z = 0; int32_t zh = 0;
        __builtin_memcpy(o + 0x10, &z, 8);   /* not the last slice: stock stops resetting its hold */
        __builtin_memcpy(o + 0x28, &zh, 4);  /* straight into its fade-out */
        if (nk < 3) keep[nk++] = old[j];
    }
    keep[nk++] = si;
    for (int32_t j = 0; j < 4; j++) {
        int32_t v = j < nk ? keep[j] : -1;
        __builtin_memcpy(note + 0xc8 + 4 * j, &v, 4);
    }
    __builtin_memcpy(note + 0xd8, &nk, 4);
}

/* CC 115: 0-127 to 20-2000 frames, 20 x 100^(v/127) */
static uint32_t clip_cycle_of(uint32_t v)
{
    float c = 20.0f * chr_exp2f((float)v * (6.64385618977472436f / 127.0f));
    uint32_t n = (uint32_t)(c + 0.5f);
    return n < CLIP_CYC_MIN ? CLIP_CYC_MIN : n > CLIP_CYC_MAX ? CLIP_CYC_MAX : n;
}

/* The Jungle Modes' controls: CC 117 picks the mode (value x 25 / 128); CC
 * 118, 119 and 104 are Roll, Pitch Roll and Freeze, held from 64 up; so are
 * the deck buttons, 105 Pull Up, 107 Power Down, 102 Brake and 78 Baby
 * Scratch (b36), one at a time: a press of another takes over, and only the
 * one playing lets go. A press, or letting one go while another is still
 * held, takes a new press point. Each press of Pitch Roll turns its direction
 * over, rising first. `quiet`: sent by a gate, so no pop-up but "Clip pads
 * only" (b16, owner). */
static void clip_jm_cc(uint8_t *pad, uint32_t clk, uint32_t cc, uint32_t v, uint32_t quiet)
{
    uint32_t dk = clip_deck_of(cc);
    uint32_t bit = cc == CLIP_CC_ROLL ? CLIP_B_ROLL : cc == CLIP_CC_PROLL ? CLIP_B_PROLL
                 : cc == CLIP_CC_FREEZE ? CLIP_B_FREEZE : dk < 15u ? CLIP_B_REW : 0u;
    if (!bit && cc != CLIP_CC_JMODE) return;          /* 103, 106 and 108-111 are not ours */
#ifdef CHR_SC
    clip_kind(pad);                                   /* b25: a gate's Hold FX on a pad now not a Clip pad */
#endif
    clip_ram_t *c = clip_ram();
    if (!clip_ok(c)) clip_reset(c);
    if (clip_clk_ok(clk)) c->clk = clk;
    int idx = clip_idx(pad);
    if (idx < 0) return;

    if (!clip_is_clip(pad)) {
        chr_osd_note(OSD_CLIP, (uint32_t)idx, CLIP_SHOW_ONLY, 0u, 0u);
        return;
    }
    if (bit) {
        uint32_t b = c->btn[idx], was = b & CLIP_B_HELD;
        clip_hp_t *h = clip_hp(c, idx);
        if (bit == CLIP_B_REW && v < 64u && dk != clip_deck(c, idx)) return;   /* another deck button's let-go */
        if (v >= 64u) {
            if (bit == CLIP_B_PROLL && !(b & bit)) b ^= CLIP_B_UP;
            if (bit == CLIP_B_REW) {
                if ((b & CLIP_B_REW) && dk != clip_deck(c, idx)) b |= CLIP_B_TAKE;   /* another takes over */
                clip_deck_set(c, idx, dk);
            }
            b |= bit;
            if (h) h->rew = 0;
        } else {
            b &= ~bit;
            /* a deck button let go alone: it plays on to the next 16th, or spins
             * up (clip_read); with no pool, straight back in, as the others */
            if (bit == CLIP_B_REW && (was & CLIP_B_REW) && !(b & CLIP_B_HELD) && h) h->rew = 1;
        }
        if (!quiet && (b & bit) != (c->btn[idx] & bit)) {   /* b19: every press and every let-go shows */
            uint32_t bi = bit == CLIP_B_ROLL ? 0u : bit == CLIP_B_PROLL ? 1u : bit == CLIP_B_FREEZE ? 2u : 3u;
            chr_osd_note(OSD_CLIP, (uint32_t)idx, CLIP_SHOW_BTN,
                         bi | ((b & bit) ? 4u : 0u) | ((b & CLIP_B_UP) ? 8u : 0u) | (bi == 3u ? dk << 4 : 0u), 0u);
        }
        if ((b & CLIP_B_HELD) && (b & CLIP_B_HELD) != was) b |= CLIP_B_TAKE;
        c->btn[idx] = (uint8_t)b;
        return;
    }
#ifdef CHR_SC
    /* b24 (owner): a CV input on Flip In owns the mode, so CC 117 leaves it;
     * the pop-up names the input and the mode playing */
    uint32_t n = clip_cv_input(idx);
    if (n) {
        if (!quiet) chr_osd_note(OSD_CLIP, (uint32_t)idx, CLIP_SHOW_JCV + n - 1u, v, 0u);
        return;
    }
#endif
    c->jmode[idx] = (uint8_t)(v * CLIP_JMODES / 128u);
    c->sum = clip_sum(c);
    if (!quiet) chr_osd_note(OSD_CLIP, (uint32_t)idx, CLIP_SHOW_JMODE, 0u, 0u);
}

#ifdef CHR_SC
/* Pad idx's method and Manual from `was` to `now` (clip_cc; b21, also the
 * Clip Tempo row's clip_row): the playing segments moved onto the new rate,
 * the held beat count made or dropped, the pop-up's tempos noted. */
static void clip_method_go(uint8_t *pad, clip_ram_t *c, int idx, uint32_t was, uint32_t now)
{
    uint32_t wm = was & CLIP_METHOD;
    float sr;
    __builtin_memcpy(&sr, (const uint8_t *)__builtin_assume_aligned(pad, 4) + 0x1f0, 4);
    if (wm == CLIP_OFF) {
        /* the beat count, made now at Preset BPM. The move onto Vinyl's rate
         * is made with the slicing the playing segments were started with,
         * and the held count's T; then stock retimes with the held count. */
        c->frz_b[idx] = 0u;
        c->mode[idx] = (uint8_t)now;
        c->sum = clip_sum(c);
        if (!(now & CLIP_MANUAL)) {
            int64_t t0, t;
            __builtin_memcpy(&t0, pad + 0xb90, 8);
            t = (int64_t)clip_beats(pad, c->clk) * 960;
            if (t > 0) __builtin_memcpy(pad + 0xb90, &t, 8);
            clip_remap(pad, 1, clip_mult(pad, clip_bpm(c->clk)));
            __builtin_memcpy(pad + 0xb90, &t0, 8);
        }
        clip_x_timing(pad, c->clk);
    } else if (now == CLIP_OFF) {
        if (!(was & CLIP_MANUAL)) clip_remap(pad, 0, clip_mult(pad, clip_bpm(c->clk)));
        c->mode[idx] = (uint8_t)now;
        c->sum = clip_sum(c);
        c->frz_b[idx] = 0u;
        clip_x_timing(pad, c->clk);               /* stock's own guess again */
    } else {
        c->mode[idx] = (uint8_t)now;
        c->sum = clip_sum(c);
        if ((now & CLIP_METHOD) != wm) {                          /* another method: its playheads start afresh */
            ((clip_jgl_t *)(uintptr_t)CLIP_JGL_ADDR0 + idx)->xpos = -1;
            ((clip_jgl_t *)(uintptr_t)CLIP_JGL_ADDR1 + idx)->xpos = -1;
        }
        if ((was & CLIP_MANUAL) && !(now & CLIP_MANUAL)) clip_drop_in(pad, c->clk, clip_bpm(c->clk));
    }
    float bpm = clip_mode_bpm(c->clk, now, idx);
    float m = clip_mult(pad, bpm);
    for (uint32_t i = 0; i < 4u; i++) c->last_rate[idx][i] = m * sr;     /* a last slice already playing */
    c->show_bpm[idx] = bpm;
    c->show_clip[idx] = m > 0.0f ? bpm / m : 0.0f;
}
#endif

/* A CC that hook B does not handle, on any pad's hidden Clip voice. A press
 * (any value above 0) of CC 112, 113 or 116 steps this pad's setting, the
 * release does nothing; CC 114 sets the Manual tempo, CC 115 Jungle's cycle; CC
 * 104 and 117-119 are the Jungle Modes', 78, 102, 105 and 107 the deck buttons'. */
#ifdef CHR_AUX
static void aux_throw_cc(uint8_t *pad, uint32_t cc, uint32_t v);   /* aux.h: CC 103 / 106, Dub Throw (a5) */
#endif

void clip_cc(uint8_t *pad, uint32_t clk, uint32_t cc, uint32_t val14)
{
    if (cc != CLIP_CC_SCRATCH && (cc < CLIP_CC_FIRST || cc > CLIP_CC_LAST)) return;
    uint32_t v = (val14 >> 7) & 0x7fu;
#ifdef CHR_AUX
    if (cc == 103u || cc == 106u) { aux_throw_cc(pad, cc, v); return; }
#endif
#ifdef CHR_SC
    uint32_t quiet = (val14 & 0x7fu) == CLIP_GATE_MARK;   /* the Gate hook's gbtn */
#else
    uint32_t quiet = 0u;
#endif
    if (cc < CLIP_CC || cc > CLIP_CC_ALGO) { clip_jm_cc(pad, clk, cc, v, quiet); return; }
    if (cc != CLIP_CC_BPM && cc != CLIP_CC_CYCLE && v == 0u) return;
    clip_ram_t *c = clip_ram();
    if (!clip_ok(c)) clip_reset(c);
    if (clip_clk_ok(clk)) c->clk = clk;
    int idx = clip_idx(pad);
    if (idx < 0) return;
    if (cc == CLIP_CC_BPM) {
        /* b30, the owner: Manual's tempo, the pad's own, and nothing else --
         * the full 1-255 in steps of 2 (Jungle's sound is a slowed-down one).
         * On the clock it does nothing, and says so. */
        if (!clip_is_clip(pad)) return;           /* a Sample pad's hidden copy: no pop-up */
        uint32_t bpm = 1u + 2u * v;
        if ((c->mode[idx] & CLIP_METHOD) && (c->mode[idx] & CLIP_MANUAL)) {
            clip_man_set(idx, (float)bpm);
            float m = clip_mult(pad, (float)bpm);
            c->show_bpm[idx] = (float)bpm;
            c->show_clip[idx] = (float)bpm / m;
            chr_osd_note(OSD_CLIP, (uint32_t)idx, CLIP_SHOW_ON, 0u, 0u);
        } else {
            chr_osd_note(OSD_CLIP, (uint32_t)idx, CLIP_SHOW_MANNA, v, 0u);
        }
        return;
    }
    if (!clip_is_clip(pad)) { chr_osd_note(OSD_CLIP, (uint32_t)idx, CLIP_SHOW_ONLY, 0u, 0u); return; }
    if (cc == CLIP_CC_CYCLE || cc == CLIP_CC_ALGO) {
        if (cc == CLIP_CC_CYCLE) c->jcyc[idx] = (uint16_t)clip_cycle_of(v);
        else c->jalg[idx] = c->jalg[idx] == CLIP_OLDSKOOL ? CLIP_MODERN : CLIP_OLDSKOOL;
        chr_osd_note(OSD_CLIP, (uint32_t)idx, cc == CLIP_CC_CYCLE ? CLIP_SHOW_CYCLE : CLIP_SHOW_ALGO, 0u, 0u);
        return;
    }
    uint32_t was = c->mode[idx], now;
    uint32_t wm = was & CLIP_METHOD;
    if (cc == CLIP_CC)
        now = wm == CLIP_OFF ? CLIP_VINYL : wm == CLIP_KEYLOCK ? CLIP_OFF : ((wm + 1u) | (was & CLIP_MANUAL));
    else
        now = wm == CLIP_OFF ? (CLIP_VINYL | CLIP_MANUAL) : (was ^ CLIP_MANUAL);   /* Clock <-> Manual */
    if (cc != CLIP_CC && (now & CLIP_MANUAL) && !(clip_man_bpm(idx) > 0.0f)) {
        float b = clip_bpm(c->clk);                            /* the first time: the tempo playing, no jump */
        if (!(b > 0.0f)) b = clip_preset(c->clk);
        clip_man_set(idx, b > 0.0f ? b : 120.0f);
    }
#ifdef CHR_SC
    clip_method_go(pad, c, idx, was, now);
    if ((now & CLIP_METHOD) != wm) clip_row_mark(idx, now);  /* b21: the row shows it; b23: Keep CCs carries it */
#else                                                        /* 2.3.10-2.3.11's objects, as frozen */
    float sr;
    __builtin_memcpy(&sr, (const uint8_t *)__builtin_assume_aligned(pad, 4) + 0x1f0, 4);
    if (wm == CLIP_OFF) {
        /* the beat count, made now at Preset BPM. The move onto Vinyl's rate
         * is made with the slicing the playing segments were started with,
         * and the held count's T; then stock retimes with the held count. */
        c->frz_b[idx] = 0u;
        c->mode[idx] = (uint8_t)now;
        c->sum = clip_sum(c);
        if (!(now & CLIP_MANUAL)) {
            int64_t t0, t;
            __builtin_memcpy(&t0, pad + 0xb90, 8);
            t = (int64_t)clip_beats(pad, c->clk) * 960;
            if (t > 0) __builtin_memcpy(pad + 0xb90, &t, 8);
            clip_remap(pad, 1, clip_mult(pad, clip_bpm(c->clk)));
            __builtin_memcpy(pad + 0xb90, &t0, 8);
        }
        clip_x_timing(pad, c->clk);
    } else if (now == CLIP_OFF) {
        if (!(was & CLIP_MANUAL)) clip_remap(pad, 0, clip_mult(pad, clip_bpm(c->clk)));
        c->mode[idx] = (uint8_t)now;
        c->sum = clip_sum(c);
        c->frz_b[idx] = 0u;
        clip_x_timing(pad, c->clk);               /* stock's own guess again */
    } else {
        c->mode[idx] = (uint8_t)now;
        c->sum = clip_sum(c);
        if ((now & CLIP_METHOD) != wm) {                          /* another method: its playheads start afresh */
            ((clip_jgl_t *)(uintptr_t)CLIP_JGL_ADDR0 + idx)->xpos = -1;
            ((clip_jgl_t *)(uintptr_t)CLIP_JGL_ADDR1 + idx)->xpos = -1;
        }
        if ((was & CLIP_MANUAL) && !(now & CLIP_MANUAL)) clip_drop_in(pad, c->clk, clip_bpm(c->clk));
    }
    float bpm = clip_mode_bpm(c->clk, now, idx);
    float m = clip_mult(pad, bpm);
    for (uint32_t i = 0; i < 4u; i++) c->last_rate[idx][i] = m * sr;     /* a last slice already playing */
    c->show_bpm[idx] = bpm;
    c->show_clip[idx] = m > 0.0f ? bpm / m : 0.0f;
#endif
    chr_osd_note(OSD_CLIP, (uint32_t)idx, now == CLIP_OFF ? CLIP_SHOW_OFF : CLIP_SHOW_ON, 0u, 0u);
}

/* After every completed preset load (UI thread): Keep CCs OFF puts every pad
 * back to Off (b21: its method is the preset's), its Jungle Mode too, as stock behaves; ON keeps them for the
 * new preset's pads. The held beat counts are made again for the new files
 * either way, at the BPM this preset was saved with, the buttons are let go
 * and the loops' passes counted afresh. Jungle's cycle and algorithm stay as
 * set. */
void clip_loaded(void)
{
    clip_ram_t *c = clip_ram();
    if (!clip_ok(c)) clip_reset(c);
    int32_t b = clip_x_getparam((void *)(uintptr_t)CLIP_SONG_STORE, CLIP_P_TEMPO);
    c->ref_bpm = (b >= 1 && b <= 1000) ? (float)b : 0.0f;
    clip_hp_alloc(c);                                  /* the hit pool, taken once; every pad's emptied */
    for (uint32_t i = 0; i < CLIP_PADS; i++) { c->frz_b[i] = 0u; c->btn[i] = 0u; }
    for (uint32_t i = 0; i < CLIP_TAILS; i++) c->tail[i].own = 0;
    if (*(volatile const uint32_t *)(uintptr_t)CLIP_KEEP_RAM == CLIP_KEEP_ON) return;
#ifdef CHR_SC
    /* b21: the method is the Clip Tempo row's, which the load posts (clip_row);
     * Manual and the Jungle Mode go back to Off, as stock behaves */
    for (uint32_t i = 0; i < CLIP_PADS; i++) { c->mode[i] &= CLIP_METHOD; c->jmode[i] = CLIP_JM_OFF; }
#else
    for (uint32_t i = 0; i < CLIP_PADS; i++) { c->mode[i] = CLIP_OFF; c->jmode[i] = CLIP_JM_OFF; }
#endif
    c->sum = clip_sum(c);
}

/* The pop-up (UI side). CC 112/113: the method, and the Jungle Mode if one is
 * on ("VINYL  Two-Bar Repeat"), over the tempo the pad follows, no arrow (b32)
 * -- Vinyl's with its shift ("Clock  134  0.0st"), Jungle's with its algorithm
 * ("Manual  89  Modern"), Key Lock's alone ("Clock  134"); Stock: "CLIP TEMPO"
 * over "Stock" (1010's own timing) or "Clip pads only". CC 114 on the clock:
 * "Clock BPM  N/A".
 * CC 117: the pad's method ("VINYL", "JUNGLE MANUAL", ...) over the Jungle
 * Mode's name, " (not yet)" after one of a later build, "  N/A" in Stock,
 * since the modes need a method. While a CV input owns the mode (b24), the
 * name the CV set, after the input: "CV 3: Rotate". CC 115/116 say N/A where they do nothing. */
static int clip_osd_num(char *d, int i, float v, int sign)
{
    char b[12]; int n = 0;
    if (sign) b[n++] = v < 0.0f ? '-' : '+';
    if (v < 0.0f) v = -v;
    int32_t t = (int32_t)(v * 10.0f + 0.5f);
    char r[8]; int m = 0;
    int32_t whole = t / 10;
    do { r[m++] = (char)('0' + whole % 10); whole /= 10; } while (whole && m < 6);
    while (m) b[n++] = r[--m];
    if (t % 10 || sign) { b[n++] = '.'; b[n++] = (char)('0' + t % 10); }
    b[n] = 0;
    return osd_put(d, i, b);                     /* osd_put keeps the line to 27 */
}

/* a whole number in four places, so a line's length, and so where it is
 * centred, stays put as the number grows */
static int clip_osd_num4(char *d, int i, uint32_t v)
{
    char b[5];
    for (int k = 3; k >= 0; k--) { b[k] = (char)('0' + v % 10u); v /= 10u; }
    for (int k = 0; k < 3 && b[k] == '0'; k++) b[k] = ' ';
    b[4] = 0;
    return osd_put(d, i, b);
}

static float clip_osd_whole(float v)
{
    float w = (float)(int32_t)(v + 0.5f);
    return v - w < 0.15f && w - v < 0.15f ? w : v;
}

static const char *const clip_jm_names[CLIP_JMODES] = {
    "Off", "Rotate", "Tail Loop", "Half Repeat", "Two-Bar Repeat", "Threes", "Riddim", "Figure Loop", "UKG",
    "Subtle Chops", "Lift Chops", "Reverse Swell", "Reverse Reverb", "Mix Subtle", "Mix Gentle", "Mix Wild",
    "Pitch Roll", "Call & Answer", "Chop Stabs", "Fifth Up", "Fourth Up", "Minor Third", "Octave Down",
    "Octave + Fifth", "Half-Time",
};

/* b27: a line longer than the box (Jungle's "Manual  134>255  Old Skool" was
 * 26 of OSD_COLS 24) closes its double spaces up */
static void clip_osd_fit(char *d)
{
    int n = 0;
    while (d[n]) n++;
    if (n <= OSD_COLS) return;
    int o = 0;
    for (int i = 0; d[i]; i++)
        if (!(d[i] == ' ' && d[i + 1] == ' ')) d[o++] = d[i];
    d[o] = 0;
}

static void clip_osd_compose(uint32_t idx, uint32_t show, uint32_t val, osd_text_t *tx)
{
    const clip_ram_t *c = clip_ram();
    uint32_t i0 = idx < CLIP_PADS ? idx : 0u;
    uint32_t mode = c->mode[i0];
    int heads = clip_heads(mode), kl = (mode & CLIP_METHOD) == CLIP_KEYLOCK;
    const char *alg = kl ? "" : c->jalg[i0] == CLIP_OLDSKOOL ? "Old Skool" : "Modern";
    tx->n = 2;
    static const char *const meth[4][2] = {
        { "STOCK", "STOCK" }, { "VINYL", "VINYL MANUAL" }, { "JUNGLE", "JUNGLE MANUAL" },
        { "KEY LOCK", "KEY LOCK MANUAL" },
    };
    uint32_t tsrc = (mode & CLIP_MANUAL) ? 1u : 0u;      /* Clock, Manual */
    const char *mname = meth[mode & CLIP_METHOD][tsrc];
    if (show == CLIP_SHOW_MANNA) {                   /* b30: CC 114 off Manual does nothing */
        static const char *const top4[4] = { "CLIP TEMPO", "VINYL", "JUNGLE", "KEY LOCK" };
        osd_put(tx->t[0], 0, top4[mode & CLIP_METHOD]);
        osd_put(tx->t[1], 0, "Clock BPM  N/A");    /* b31, the owner: the pad is on the clock */
        return;
    }
#ifdef CHR_SC
    int at = 0;
    if (show - CLIP_SHOW_JCV < 8u) {                 /* b24: the input first, then as CC 117's */
        char d[2] = { (char)('1' + (show - CLIP_SHOW_JCV)), 0 };
        at = osd_put(tx->t[1], 0, "CV ");
        at = osd_put(tx->t[1], at, d);
        at = osd_put(tx->t[1], at, ": ");
        show = CLIP_SHOW_JMODE;
    }
#else
    const int at = 0;
#endif
    if (show == CLIP_SHOW_JMODE) {                   /* the method over the mode's name */
        uint32_t jm = c->jmode[i0] < CLIP_JMODES ? c->jmode[i0] : 0u;
        osd_put(tx->t[0], 0, mname);
        int i = osd_put(tx->t[1], at, clip_jm_names[jm]);
        /* Stock: the modes need a method. The name stays, so each turn's text
         * differs and shows (a pop-up repeats only on new text) */
        if (!(mode & CLIP_METHOD)) osd_put(tx->t[1], i, "  N/A");
        else if (!clip_jm_built(jm)) osd_put(tx->t[1], i, " (not yet)");
#ifdef CHR_SC
        clip_osd_fit(tx->t[1]);                      /* "CV 3: Reverse Reverb  N/A" is 25 */
#endif
        return;
    }
    if (show == CLIP_SHOW_BTN) {                     /* the method over the button and what it does now */
        static const char *const bn[4] = { "Roll", "Pitch Roll", "Freeze", "" };
        static const char *const dn[4] = { "Pull Up", "Power Down", "Brake", "Baby Scratch" };   /* b36 */
        osd_put(tx->t[0], 0, mname);
        int i = osd_put(tx->t[1], 0, (val & 3u) == 3u ? dn[(val >> 4) & 3u] : bn[val & 3u]);
        i = osd_put(tx->t[1], i, "  ");
        if (!(val & 4u)) osd_put(tx->t[1], i, "Off");
        else if (!(mode & CLIP_METHOD)) osd_put(tx->t[1], i, "N/A");
        else osd_put(tx->t[1], i, (val & 3u) == 1u ? ((val & 8u) ? "Up" : "Down") : "On");
        return;
    }
    if (show == CLIP_SHOW_CYCLE || show == CLIP_SHOW_ALGO) {
        /* Jungle's own; the cycle also sets the harmony modes' grains in Vinyl
         * and Key Lock. N/A where it does nothing: the cycle in Stock, Modern /
         * Old Skool outside Jungle -- the value kept, so each change shows */
        uint32_t m = mode & CLIP_METHOD;
        osd_put(tx->t[0], 0, m == CLIP_JUNGLE ? "JUNGLE" : mname);
        int i;
        if (show == CLIP_SHOW_ALGO) {
            i = osd_put(tx->t[1], 0, c->jalg[i0] == CLIP_OLDSKOOL ? "Old Skool" : "Modern");
            if (m != CLIP_JUNGLE) osd_put(tx->t[1], i, "  N/A");
            return;
        }
        i = m == CLIP_JUNGLE ? osd_put(tx->t[1], 0, c->jalg[i0] == CLIP_OLDSKOOL ? "Old Skool  " : "Modern  ") : 0;
        i = osd_put(tx->t[1], i, "Cycle ");                  /* in Jungle, its algorithm first */
        i = clip_osd_num4(tx->t[1], i, c->jcyc[i0] < 10000u ? c->jcyc[i0] : 9999u);
        if (m == CLIP_OFF) osd_put(tx->t[1], i, "  N/A");
        return;
    }
    if (show == CLIP_SHOW_ONLY || show != CLIP_SHOW_ON || !(mode & CLIP_METHOD)) {
        osd_put(tx->t[0], 0, "CLIP TEMPO");
        osd_put(tx->t[1], 0, show == CLIP_SHOW_ONLY ? "Clip pads only" : "Stock");
        return;
    }
    /* the method, and the Jungle Mode playing: at most 8 + 2 + 14 = 24 */
    static const char *const top[4] = { "STOCK", "VINYL", "JUNGLE", "KEY LOCK" };
    uint32_t jm = c->jmode[i0];
    int i = osd_put(tx->t[0], 0, top[mode & CLIP_METHOD]);
    if (jm != CLIP_JM_OFF && jm < CLIP_JMODES && clip_jm_built(jm)) {
        i = osd_put(tx->t[0], i, "  ");
        osd_put(tx->t[0], i, clip_jm_names[jm]);
    }
    /* a tempo within 0.15 of a whole BPM shows as it: an external clock is
     * measured a hair off (the NOME's 134 reads 133.94), which is not news */
    float clip = clip_osd_whole(c->show_clip[i0]), bpm = clip_osd_whole(c->show_bpm[i0]);
    char *d = tx->t[1];
    i = osd_put(d, 0, tsrc ? "Manual" : "Clock");   /* b27: Clock named too */
    if (!(clip > 0.0f && bpm > 0.0f && clip < 10000.0f)) {      /* no clock or clip yet */
        if (heads && !kl) { i = osd_put(d, i, "  "); osd_put(d, i, alg); }
        return;
    }
    i = osd_put(d, i, "  ");
    i = clip_osd_num(d, i, bpm, 0);                  /* b32, the owner: the tempo it follows, no arrow */
    if (kl) return;
    i = osd_put(d, i, "  ");
    if (heads) { osd_put(d, i, alg); clip_osd_fit(d); return; }
    float st = 12.0f * chr_logf(bpm / clip) * 1.44269504088896340736f;
    i = st < 0.05f && st > -0.05f ? osd_put(d, i, "0.0") : clip_osd_num(d, i, st, 1);   /* no "-0.0" */
    osd_put(d, i, "st");
    clip_osd_fit(d);
}

#endif
