/*
 * Saturation bank (slot 4 of 4, last in the chain): PurestDrive, Spiral2,
 * Density3, Mojo. Ported from Airwindows by Chris Johnson -- see the MIT
 * notice in character.c. Double -> float, dither and denormal-noise dropped,
 * state per channel, sample rate fixed at 48 kHz.
 *
 * Calling convention for every mode in every bank:
 *   void m_NAME(NAME_t *s, real amt, real tone, float *L, float *R, int n)
 * in place, amt and tone in [0, 1] (already smoothed by the engine, constant
 * for the block), R == 0 means the bus is mono this block (process L only).
 * State is zeroed by the engine before first use.
 *
 * Processing is CHANNEL-MAJOR: the whole block of L, then the whole block of
 * R, each with its channel's state copied into a local struct for the loop.
 * Patch RAM is uncached device memory (see CHARACTER-DESIGN.md), so state is
 * read once and written once per block instead of every sample. Where the
 * originals share per-sample state between channels (a `flip` toggle), each
 * channel replays it from the same starting value, which is exactly what
 * the interleaved original does.
 */
#ifndef CHR_SATURATION_H
#define CHR_SATURATION_H

#include "chr_port.h"

/* Tone convention for one-knob originals: Tone centre (0.5) is neutral. */
static inline real chr_tone_db(real tone, real lo_db, real hi_db)
{
    return tone < (real)0.5f ? ((real)1.0f - (real)2.0f * tone) * lo_db
                             : ((real)2.0f * tone - (real)1.0f) * hi_db;
}
static inline real chr_db2lin(real db) { return R_POW((real)10.0f, db / (real)20.0f); }

#define CHR_NCH(R) ((R) ? 2 : 1)

/* ---- Saturation 1: PurestDrive -------------------------------------------
 * Amount -> intensity A (0..1). Tone -> drive into the sine with matching
 * make-down: -6 dB (Tone 0) .. 0 dB (centre) .. +12 dB (Tone 1). The sine is
 * applied to (x*g) and the result divided by g, so level stays put while the
 * curve bites harder. PurestDrive itself has no second control. */
typedef struct { real prev; } purestdrive_ch;
typedef struct { purestdrive_ch ch[2]; } purestdrive_t;

static void m_purestdrive(purestdrive_t *s, real amt, real tone, float *L, float *R, int n)
{
    real intensity = amt;
    real g = chr_db2lin(chr_tone_db(tone, (real)-6.0f, (real)12.0f));
    real ig = (real)1.0f / g;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        purestdrive_ch st = s->ch[c];
        for (int i = 0; i < n; i++) {
            real dry = (real)buf[i] * g;
            real x = R_SIN(dry);
            real apply = (R_FABS(st.prev + x) / (real)2.0f) * intensity;
            x = (dry * ((real)1.0f - apply)) + (x * apply);
            st.prev = R_SIN(dry);
            buf[i] = (float)(x * ig);
        }
        s->ch[c] = st;
    }
}

/* ---- Saturation 2: Spiral2 -----------------------------------------------
 * Amount -> Input A from 0.5 (unity, the plugin default) to 1.0 (+12 dB).
 * Tone -> Presence C (0..1, the plugin default 0.5 at centre). Highpass
 * B = 0, Dry/Wet E = 1. Output D = gain^-0.5: half the input boost taken back
 * in dB (the plugin's own Output control, which only ever attenuates), so
 * turning Amount up adds saturation rather than mostly level. */
typedef struct { real iirA, iirB, prev; } spiral2_ch;
typedef struct { spiral2_ch ch[2]; uint32_t flip; } spiral2_t;

static void m_spiral2(spiral2_t *s, real amt, real tone, float *L, float *R, int n)
{
    real A = (real)0.5f + (real)0.5f * amt;
    real gain = R_POW(A * (real)2.0f, (real)2.0f);
    real iirAmount = (real)0.0f;                 /* B = 0: pow(0,3)/overallscale */
    real presence = tone;
    real output = R_SQRT((real)1.0f / gain);
    uint32_t flip0 = s->flip;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        spiral2_ch st = s->ch[c];
        uint32_t flip = flip0;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            real dry = x;
            if (gain != (real)1.0f) { x *= gain; st.prev *= gain; }
            if (flip) { st.iirA = (st.iirA * ((real)1.0f - iirAmount)) + (x * iirAmount); x -= st.iirA; }
            else      { st.iirB = (st.iirB * ((real)1.0f - iirAmount)) + (x * iirAmount); x -= st.iirB; }
            real ap = R_FABS(st.prev);
            real presenceSample = R_SIN(x * ap) / ((st.prev == (real)0.0f) ? (real)1.0f : ap);
            real ax = R_FABS(x);
            x = R_SIN(x * ax) / ((ax == (real)0.0f) ? (real)1.0f : ax);
            if (output < (real)1.0f) { x *= output; presenceSample *= output; }
            if (presence > (real)0.0f) x = (x * ((real)1.0f - presence)) + (presenceSample * presence);
            st.prev = dry;
            flip = !flip;
            buf[i] = (float)x;
        }
        s->ch[c] = st;
    }
    s->flip = flip0 ^ (uint32_t)(n & 1);
}

/* ---- Saturation 3: Density3 ----------------------------------------------
 * Its real controls: Amount -> Density A = amt over its FULL range (density
 * = 5A, 0..5; the plugin displays it as -1..4), Tone -> Highpass B = tone.
 * Output C = 1 and Dry/Wet D = 1: plugin defaults. Density3 is neutral at
 * density 1 (Amount ~25, CC value 25): below that it blends toward a
 * "starved" curve (quieter, thinner -- including the very bottom of Amount),
 * above it toward a sine-shaped saturation, fully from density 2. Its
 * highpass is in series with the signal: Tone above zero removes bass from
 * the OUTPUT (the plugin default is B = 0, off; Tone centre is ~900 Hz). */
typedef struct { real iir; } density3_ch;
typedef struct { density3_ch ch[2]; } density3_t;

static void m_density3(density3_t *s, real amt, real tone, float *L, float *R, int n)
{
    real density = amt * (real)5.0f;
    real iirAmount = R_POW(tone, (real)3.0f) / (real)CHR_OVERALLSCALE;
    const real HP = (real)1.57079632679489661923f;
    real mix = R_FABS(density - (real)1.0f);
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        density3_ch st = s->ch[c];
        /* the min/max clamp below swallows a NaN and would pin the output at
         * full scale for ever, so a non-finite state is reset here instead */
        if (iirAmount == (real)0.0f || !(R_FABS(st.iir) < (real)1.0e9f)) st.iir = (real)0.0f;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            st.iir = (st.iir * ((real)1.0f - iirAmount)) + (x * iirAmount);
            x -= st.iir;
            real altered = x;
            if (density > (real)1.0f) {
                altered = chr_maxr(chr_minr(x * density * HP, HP), -HP);
                real X = altered * altered;
                real temp = altered * X;
                altered -= (temp * (real)(1.0f / 6.0f)); temp *= X;
                altered += (temp * (real)(1.0f / 120.0f)); temp *= X;
                altered -= (temp * (real)(1.0f / 5040.0f)); temp *= X;
                altered += (temp * (real)(1.0f / 362880.0f)); temp *= X;
                altered -= (temp * (real)(1.0f / 39916800.0f));
            }
            if (density < (real)1.0f) {
                altered = chr_maxr(chr_minr(x, (real)1.0f), (real)-1.0f);
                real polarity = altered;
                real X = x * altered;
                real temp = X;
                altered = (temp * (real)(1.0f / 2.0f)); temp *= X;
                altered -= (temp * (real)(1.0f / 24.0f)); temp *= X;
                altered += (temp * (real)(1.0f / 720.0f)); temp *= X;
                altered -= (temp * (real)(1.0f / 40320.0f)); temp *= X;
                altered += (temp * (real)(1.0f / 3628800.0f));
                altered *= ((polarity < (real)0.0f) ? (real)-1.0f : (real)1.0f);
            }
            if (density > (real)2.0f) x = altered;
            else x = (x * ((real)1.0f - mix)) + (altered * mix);
            buf[i] = (float)x;                     /* output C = 1, wet D = 1 */
        }
        s->ch[c] = st;
    }
}

/* ---- Saturation 4: Mojo --------------------------------------------------
 * Amount -> Input A = 0.5 + 0.5*amt, i.e. 0 dB .. +12 dB into the fold.
 * Tone -> how much of that boost is taken back afterwards: 0 none, centre
 * half (in dB), 1 all of it. Mojo has no output control of its own. */
typedef struct { uint32_t unused; } mojo_t;

static void m_mojo(mojo_t *s, real amt, real tone, float *L, float *R, int n)
{
    (void)s;
    real A = (real)0.5f + (real)0.5f * amt;
    real gain = R_POW((real)10.0f, ((A * (real)24.0f) - (real)12.0f) / (real)20.0f);
    real post = R_POW(gain, -tone);
    const real hp = (real)3.14159265358979f * (real)0.5f;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            if (gain != (real)1.0f) x *= gain;
            real mojo = R_SQRT(R_SQRT(R_FABS(x)));    /* pow(|x|, 0.25), exactly */
            if (mojo > (real)0.0f) x = (R_SIN(x * mojo * hp) / mojo) * (real)0.987654321f;
            buf[i] = (float)(x * post);
        }
    }
}

#endif
