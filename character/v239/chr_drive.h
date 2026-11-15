/*
 * Drive bank (slot 2 of 4): Tube2, Drive, Mackity, Huge. Ported from
 * Airwindows by Chris Johnson -- see the MIT notice in character.c.
 * Conventions as in chr_saturation.h.
 */
#ifndef CHR_DRIVE_H
#define CHR_DRIVE_H

#include "chr_port.h"

#ifndef CHR_NCH
#define CHR_NCH(R) ((R) ? 2 : 1)
#endif

/* ---- Drive 1: Tube2 ------------------------------------------------------
 * Amount -> Tube B = amt (powerfactor 10, the gentlest, down to 1, the
 * hardest). Tone -> Input A = max(0.1, tone): the input pad, centre 0.5 is
 * the plugin default. The original's powerfactor is an integer, so turning
 * Tube steps the curve; here the "original Tube" section is computed for the
 * two neighbouring integers and blended by the fraction, which equals the
 * original exactly at every integer and glides between them. 48 kHz is below
 * Tube2's 1.9x-oversample branches, so those are absent, as in the original
 * at this rate. */
typedef struct { real prevE; } tube2_ch;
typedef struct { tube2_ch ch[2]; } tube2_t;

static inline real tube2_core(real x, int pf)
{
    real asymPad = (real)pf;
    real gainscaling = (real)1.0f / (real)(pf + 1);
    real outputscaling = (real)1.0f + ((real)1.0f / (real)pf);
    x /= asymPad;
    real sharpen = -x;
    if (sharpen > (real)0.0f) sharpen = (real)1.0f + R_SQRT(sharpen);
    else sharpen = (real)1.0f - R_SQRT(-sharpen);
    x -= x * R_FABS(x) * sharpen * (real)0.25f;
    x *= asymPad;
    real factor = x;
    for (int k = 0; k < pf; k++) factor *= x;
    if ((pf % 2 == 1) && (x != (real)0.0f)) factor = (factor / x) * R_FABS(x);
    factor *= gainscaling;
    x -= factor;
    x *= outputscaling;
    return x;
}

static void m_tube2(tube2_t *s, real amt, real tone, float *L, float *R, int n)
{
    real inputPad = tone < (real)0.1f ? (real)0.1f : tone;
    real iterations = (real)1.0f - amt;
    real pff = ((real)9.0f * iterations) + (real)1.0f;
    int pf0 = (int)pff;
    if (pf0 < 1) pf0 = 1;
    if (pf0 > 10) pf0 = 10;
    real frac = pff - (real)pf0;
    int pf1 = pf0 < 10 ? pf0 + 1 : 10;
    if (frac < (real)0.0f) frac = (real)0.0f;
    real gs = ((real)1.0f - frac) / (real)(pf0 + 1) + frac / (real)(pf1 + 1);
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        tube2_ch st = s->ch[c];
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            if (inputPad < (real)1.0f) x *= inputPad;
            if (x > (real)1.0f) x = (real)1.0f;
            if (x < (real)-1.0f) x = (real)-1.0f;
            real y = tube2_core(x, pf0);
            if (frac > (real)0.0f) y = y * ((real)1.0f - frac) + tube2_core(x, pf1) * frac;
            x = y;
            real slew = st.prevE - x;
            st.prevE = x;
            if (slew > (real)0.0f) slew = (real)1.0f + (R_SQRT(slew) * (real)0.5f);
            else slew = (real)1.0f - (R_SQRT(-slew) * (real)0.5f);
            x -= x * R_FABS(x) * slew * gs;
            if (x > (real)0.52f) x = (real)0.52f;
            if (x < (real)-0.52f) x = (real)-0.52f;
            x *= (real)1.923076923076923f;
            buf[i] = (float)x;
        }
        s->ch[c] = st;
    }
}

/* ---- Drive 2: Drive ------------------------------------------------------
 * Amount -> Drive A = amt (driveone = (2A)^2, 0..4). Tone -> Highpass B =
 * tone, the plugin's whole control (ch11; chr1-10 used 0.3*tone, a subsonic
 * clean-up only). Drive's highpass is in series, so this IS the output's low
 * end; its two states alternate, so the one-pole runs at an effective 24 kHz.
 * The corner across the range is measured on this code (CHARACTER-DESIGN.md
 * section 11.3's table and chr_osd.h's osd_drive_hz10). Output C = 1,
 * Dry/Wet D = 1. */
typedef struct { real iirA, iirB; } drive_ch;
typedef struct { drive_ch ch[2]; uint32_t flip; } drive_t;

static void m_drive(drive_t *s, real amt, real tone, float *L, float *R, int n)
{
    real driveone = R_POW(amt * (real)2.0f, (real)2.0f);
    real B = tone;
    real iirAmount = R_POW(B, (real)3.0f) / (real)CHR_OVERALLSCALE;
    const real glitch = (real)0.60f;
    uint32_t flip0 = s->flip;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        drive_ch st = s->ch[c];
        uint32_t flip = flip0;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            if (flip) { st.iirA = (st.iirA * ((real)1.0f - iirAmount)) + (x * iirAmount); x -= st.iirA; }
            else      { st.iirB = (st.iirB * ((real)1.0f - iirAmount)) + (x * iirAmount); x -= st.iirB; }
            flip = !flip;
            if (x > (real)1.0f) x = (real)1.0f;
            if (x < (real)-1.0f) x = (real)-1.0f;
            real out = driveone;
            while (out > glitch) {
                out -= glitch;
                x -= (x * (R_FABS(x) * glitch) * (R_FABS(x) * glitch));
                x *= ((real)1.0f + glitch);
            }
            x -= (x * (R_FABS(x) * out) * (R_FABS(x) * out));
            x *= ((real)1.0f + out);
            buf[i] = (float)x;                       /* output C = 1, wet D = 1 */
        }
        s->ch[c] = st;
    }
    s->flip = flip0 ^ (uint32_t)(n & 1);
}

/* ---- Drive 3: Mackity ----------------------------------------------------
 * Amount -> In Trim A = 0.1 + 0.3*amt (inTrim = (10A)^2: 0 dB .. +24 dB into
 * the console curve). Tone -> how much of that trim Out Pad takes back:
 * outPad = inTrim^-tone, so 0 none, centre half (in dB), 1 all of it --
 * Mackity's Out Pad only ever attenuates, as here. */
typedef struct { real iirA, iirB, a7, a8, a9, a10, b7, b8, b9, b10; } mackity_ch;
typedef struct { mackity_ch ch[2]; } mackity_t;

static void m_mackity(mackity_t *s, real amt, real tone, float *L, float *R, int n)
{
    real A = (real)0.1f + (real)0.3f * amt;
    real inTrim = A * (real)10.0f;
    inTrim *= inTrim;
    real outPad = R_POW(inTrim, -tone);
    real iirAmountA = (real)0.001860867f / (real)CHR_OVERALLSCALE;
    real iirAmountB = (real)0.000287496f / (real)CHR_OVERALLSCALE;
    real fq = (real)19160.0f / (real)CHR_SR;
    real qa = (real)0.431684981684982f, qb = (real)1.1582298f;
    real K = R_TAN((real)3.14159265358979f * fq);
    real norm = (real)1.0f / ((real)1.0f + K / qa + K * K);
    real a2 = K * K * norm, a3 = (real)2.0f * a2, a4 = a2;
    real a5 = (real)2.0f * (K * K - (real)1.0f) * norm, a6 = ((real)1.0f - K / qa + K * K) * norm;
    norm = (real)1.0f / ((real)1.0f + K / qb + K * K);
    real b2 = K * K * norm, b3 = (real)2.0f * b2, b4 = b2;
    real b5 = (real)2.0f * (K * K - (real)1.0f) * norm, b6 = ((real)1.0f - K / qb + K * K) * norm;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        mackity_ch st = s->ch[c];
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            st.iirA = (st.iirA * ((real)1.0f - iirAmountA)) + (x * iirAmountA);
            x -= st.iirA;
            if (inTrim != (real)1.0f) x *= inTrim;
            real o = a2 * x + a3 * st.a7 + a4 * st.a8 - a5 * st.a9 - a6 * st.a10;
            st.a8 = st.a7; st.a7 = x; x = o; st.a10 = st.a9; st.a9 = x;
            if (x > (real)1.0f) x = (real)1.0f;
            if (x < (real)-1.0f) x = (real)-1.0f;
            real x2 = x * x;
            x -= (x2 * x2 * x) * (real)0.1768f;      /* pow(x,5), sign kept */
            o = b2 * x + b3 * st.b7 + b4 * st.b8 - b5 * st.b9 - b6 * st.b10;
            st.b8 = st.b7; st.b7 = x; x = o; st.b10 = st.b9; st.b9 = x;
            st.iirB = (st.iirB * ((real)1.0f - iirAmountB)) + (x * iirAmountB);
            x -= st.iirB;
            if (outPad != (real)1.0f) x *= outPad;
            buf[i] = (float)x;
        }
        st.iirA = chr_flush_r(st.iirA); st.iirB = chr_flush_r(st.iirB);
        st.a9 = chr_flush_r(st.a9); st.a10 = chr_flush_r(st.a10);
        st.b9 = chr_flush_r(st.b9); st.b10 = chr_flush_r(st.b10);
        s->ch[c] = st;
    }
}

/* ---- Drive 4: Huge -------------------------------------------------------
 * Exactly its two controls: Amount -> Huge A = amt, Tone -> Nasty B = tone.
 * Huge scales the input by A before adding odd Chebyshev harmonics (3rd to
 * 13th), each gated by its own slew limiter whose threshold Nasty sets. So,
 * as in the plugin, low Amounts are also QUIETER (A = 0.5 is -6 dB of the
 * dry signal before the harmonics); the plugin's own default is A = B = 0.2. */
typedef struct { real prev[6], limit[6], prevOut; } huge_ch;
typedef struct { huge_ch ch[2]; } huge_t;

static void m_huge(huge_t *s, real amt, real tone, float *L, float *R, int n)
{
    real huge = amt;
    real threshold = (tone * (real)0.05f) + (real)0.05f;
    real attack = (threshold * (real)8.0f) / (real)CHR_OVERALLSCALE;
    real release = attack;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        huge_ch st = s->ch[c];
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i] * huge;
            /* The Chebyshev terms are only bounded on [-1, 1]: a hot master bus
             * (+6 dBFS) drives the 13th-order term to ~20x. Their input is
             * clamped; the dry path is not. A deviation from the original. */
            real xc = x > (real)1.0f ? (real)1.0f : (x < (real)-1.0f ? (real)-1.0f : x);
            /* The six terms are Chebyshev T3, T5 .. T13 of xc, written out in
             * the source as power series; the recurrence T(n+1) = 2x T(n) -
             * T(n-1) gives the same polynomials in a third of the arithmetic. */
            real o[6];
            real t0 = (real)1.0f, t1 = xc, tx = xc + xc;
            const real w[6] = { (real)-0.36f, (real)0.2025f, (real)-0.1444f,
                                (real)0.1225f, (real)-0.1024f, (real)0.09f };
            for (int k = 0; k < 6; k++) {          /* step twice: odd orders only */
                real t2 = tx * t1 - t0; t0 = t1; t1 = t2;
                t2 = tx * t1 - t0; t0 = t1; t1 = t2;
                o[k] = t1 * w[k];
            }
            for (int k = 0; k < 6; k++) {
                real slew = R_FABS(o[k] - st.prev[k]);
                st.prev[k] = o[k];
                if (slew < threshold) st.limit[k] += release;
                else st.limit[k] -= attack;
            }
            /* a limit over 1 pins itself and every lower order to 1; a limit
             * under 0 pins itself and every higher order to 0 -- the
             * original's cascade, in its order */
            for (int k = 0; k < 6; k++)
                if (st.limit[k] > (real)1.0f) for (int j = 0; j <= k; j++) st.limit[j] = (real)1.0f;
            for (int k = 0; k < 6; k++)
                if (st.limit[k] < (real)0.0f) for (int j = k; j < 6; j++) st.limit[j] = (real)0.0f;
            real cheb = (real)0.0f;
            for (int k = 0; k < 6; k++) cheb += o[k] * st.limit[k];
            x += ((cheb + st.prevOut) * (real)0.5f);
            st.prevOut = cheb;
            buf[i] = (float)x;
        }
        s->ch[c] = st;
    }
}

#define CHR_STATES_DRIVE tube2_t a; drive_t b; mackity_t c; huge_t d;
#define CHR_PRESENT_DRIVE 0x0f
#define CHR_INIT_DRIVE(type, st) do { (void)(type); (void)(st); } while (0)
#define CHR_RUN_DRIVE(type, st, amt, tone, L, R, n) do { switch (type) { \
    case 0: m_tube2((tube2_t *)(st), amt, tone, L, R, n); return 1; \
    case 1: m_drive((drive_t *)(st), amt, tone, L, R, n); return 1; \
    case 2: m_mackity((mackity_t *)(st), amt, tone, L, R, n); return 1; \
    case 3: m_huge((huge_t *)(st), amt, tone, L, R, n); return 1; \
    } } while (0)

#endif
