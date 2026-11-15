/*
 * Destruction bank (slot 1 of 4, first in the chain): Pockey2, DeRez3,
 * RingModulator, Fracture2. Ported from Airwindows by Chris Johnson -- see the
 * MIT notice in character.c. Conventions as in chr_saturation.h.
 */
#ifndef CHR_DESTRUCTION_H
#define CHR_DESTRUCTION_H

#include "chr_port.h"

#ifndef CHR_NCH
#define CHR_NCH(R) ((R) ? 2 : 1)
#endif

/* ---- Destruction 1: Pockey2 ----------------------------------------------
 * Amount -> DeRez B = 1 - amt: 16 bits (Amount ~0) down to 4 bits (Amount
 * full), through Pockey2's mu-law style encode/decode. Tone -> DeFreq A =
 * tone: sample-and-hold every 1 (Tone 0) .. 4 (centre) .. 34 (Tone 1)
 * samples, with its slew-dependent blur. Dry/Wet C = 1. The hold counter is
 * shared by both channels as in the original. */
typedef struct { real lastSample, heldSample, previousHeld; } pockey2_ch;
typedef struct { pockey2_ch ch[2]; int32_t position; } pockey2_t;

static void m_pockey2(pockey2_t *s, real amt, real tone, float *L, float *R, int n)
{
    real A = tone, B = (real)1.0f - amt;
    int32_t freq = (int32_t)R_FLOOR(R_POW(A, (real)3.0f) * (real)32.0f * (real)CHR_OVERALLSCALE);
    real rez = (real)4.0f + (B * (real)12.0f);
    int32_t rezFactor = (int32_t)R_EXP2(rez);                  /* pow(2, rez) */
    real rf = (real)rezFactor, irf = (real)1.0f / rf;
    const real ilog255 = (real)1.0f / R_LOG((real)255.0f);
    int32_t pos0 = s->position, pos = pos0;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        pockey2_ch st = s->ch[c];
        pos = pos0;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            real dry = x;
            R_CLIP(x, (real)-1.0f, (real)1.0f);
            if (x > (real)0.0f) x = R_LOG_FAST((real)1.0f + ((real)255.0f * R_FABS(x))) * ilog255;
            else if (x < (real)0.0f) x = -R_LOG_FAST((real)1.0f + ((real)255.0f * R_FABS(x))) * ilog255;
            x *= rf;
#ifdef CHR_FASTF
            x = __builtin_truncf(x);                   /* the two floors below, exactly: toward zero */
#else
            if (x > (real)0.0f) x = R_FLOOR(x);
            else if (x < (real)0.0f) x = -R_FLOOR(-x);
#endif
            x *= irf;                                  /* rf is a power of two: exact */
            R_CLIP(x, (real)-1.0f, (real)1.0f);
            if (x > (real)0.0f) x = (R_EXP2((real)8.0f * R_FABS(x)) - (real)1.0f) / (real)255.0f;   /* pow(256,|x|) */
            else if (x < (real)0.0f) x = -(R_EXP2((real)8.0f * R_FABS(x)) - (real)1.0f) / (real)255.0f;
            real blur = (real)0.618033988749894848204586f - (R_FABS(x - st.lastSample) * (real)CHR_OVERALLSCALE);
            R_CLIP_LO(blur, (real)0.0f);               /* never -0: a constant minus a magnitude */
            if (pos < 1) { pos = freq; st.heldSample = x; }
            x = st.heldSample;
            st.lastSample = dry;
            pos--;
            x = (x * blur) + (st.previousHeld * ((real)1.0f - blur));
            st.previousHeld = st.heldSample;
            buf[i] = (float)x;                                      /* wet C = 1 */
        }
        CHR_STATE_CHECK(buf, st.lastSample, st.heldSample, st.previousHeld);
        s->ch[c] = st;
    }
    s->position = pos;
}

/* ---- Destruction 2: DeRez3 -----------------------------------------------
 * Its real controls: Rate A, Rez B (bit depth, bits = 15B + 1), Dry/Wet C.
 *   Amount -> Rate A = 1 - 0.85*amt. Rate runs the wrong way for an Amount
 *     (A = 1 is full rate, i.e. clean), hence the inversion. The floor is NOT
 *     needed for safety -- DeRez3 clamps its own step at 0.0005, so A = 0
 *     still plays (updates about 24 times a second) -- it is a usability
 *     choice: below A = 0.15 (about 150 updates a second) the bus turns into
 *     a slow bezier smear with little of the music left. Removing it would be
 *     a one-number change.
 *   Tone -> Rez B = 1 - 0.75*tone: 16 bits at Tone 0, about 10.4 bits at the
 *     centre, about 4.75 bits at Tone 127 -- inside Rez's real range (B 0..1
 *     = 1..16 bits); the sweep stops short of the 1-bit end. The plugin's
 *     default (B = 1.0, 16 bits) sits at Tone 0, not the centre: a centre of
 *     "no crush" would leave half the knob doing nothing.
 *   Dry/Wet C = 1 (the plugin default: full wet; DeRez3's 0.5 would be full
 *     dry AND full wet together).
 * DeRez3 ramps its controls across each call from the previous call's values.
 * Its sample-and-hold cycle accumulator is a DOUBLE in every build: in float
 * the running sum drifts against the 1.0 threshold and the hold instants slip
 * by whole samples (chr1 measured this as a 66% rms "error" against the
 * reference and had to compare DeRez3 statistically). With a double sum, and
 * the cube written as A*A*A rather than through the approximate pow, the
 * float build tracks the reference sample for sample. */
enum { bez_A, bez_B, bez_C, bez_In, bez_UnIn, bez_Samp, bez_ch_total };
typedef struct { real bez[bez_ch_total]; } derez3_ch;
typedef struct { double cycle; derez3_ch ch[2]; real rezA, bitA, wetA; uint32_t primed; } derez3_t;

static void m_derez3_init(derez3_t *s) { s->cycle = 1.0; }

static void m_derez3(derez3_t *s, real amt, real tone, float *L, float *R, int n)
{
    real A = (real)1.0f - (real)0.85f * amt, B = (real)1.0f - (real)0.75f * tone, C = (real)1.0f;
    real rezB = (A * A * A) / (real)CHR_OVERALLSCALE;             /* pow(A, 3) */
    real bitB = (B * (real)15.0f) + (real)1.0f;
    real wetB = C * (real)2.0f;
    if (!s->primed) { s->rezA = rezB; s->bitA = bitB; s->wetA = wetB; s->primed = 1; }
    real rezA = s->rezA, bitA = s->bitA, wetA = s->wetA;
    double cycle0 = s->cycle, cycle = cycle0;
    if (!(cycle0 >= 0.0 && cycle0 <= 2.0)) cycle0 = 1.0;          /* NaN or nonsense: restart */
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        derez3_ch st = s->ch[c];
        cycle = cycle0;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            real dry = x;
            real temp = (real)(n - 1 - i) / (real)n;
            real rez = (rezA * temp) + (rezB * ((real)1.0f - temp));
            real bit = (bitA * temp) + (bitB * ((real)1.0f - temp));
            real wet = (wetA * temp) + (wetB * ((real)1.0f - temp));
            R_CLIP_LO(rez, (real)0.0005f);
            real bitFactor = R_EXP2(bit);                        /* pow(2, bit) */
            real dryg = (real)2.0f - wet;
            /* the 0 bounds meet only +0: wet is a blend of two positive
             * values, dryg a difference (equal operands give +0) */
            R_CLIP(wet, (real)0.0f, (real)1.0f);
            R_CLIP(dryg, (real)0.0f, (real)1.0f);
            x *= bitFactor;
            x = R_FLOOR(x + ((real)0.5f / bitFactor));
            x /= bitFactor;
            cycle += (double)rez;
            st.bez[bez_Samp] += (x * rez);
            if (cycle > 1.0) {
                cycle -= 1.0;
                st.bez[bez_C] = st.bez[bez_B];
                st.bez[bez_B] = st.bez[bez_A];
                st.bez[bez_A] = x;
                st.bez[bez_Samp] = (real)0.0f;
            }
            real cy = (real)cycle;
            real CB = (st.bez[bez_C] * ((real)1.0f - cy)) + (st.bez[bez_B] * cy);
            real BA = (st.bez[bez_B] * ((real)1.0f - cy)) + (st.bez[bez_A] * cy);
            real CBA = (st.bez[bez_B] + (CB * ((real)1.0f - cy)) + (BA * cy)) * (real)0.5f;
            buf[i] = (float)((wet * CBA) + (dryg * dry));
        }
        /* bez_Samp only accumulates (and In/UnIn are unused): not output */
        CHR_STATE_CHECK(buf, st.bez[bez_A], st.bez[bez_B], st.bez[bez_C], rezA, bitA, wetA);
        s->ch[c] = st;
    }
    s->cycle = cycle;
    s->rezA = rezB; s->bitA = bitB; s->wetA = wetB;
}

/* ---- Destruction 3: RingModulator ----------------------------------------
 * Its real controls: Freq A (left carrier), Freq B (right carrier), Soar C,
 * Dry/Wet D.
 *   Amount -> Dry/Wet D = amt (the plugin squares it: wet = D^2).
 *   Tone -> both carriers: B (right) = tone, A (left) = 0.99 * tone. The
 *     carrier is pow(Freq, 5) radians a sample, so the left runs about 5%
 *     lower than the right at every setting (0.99^5): a fixed detune that
 *     keeps the stereo wide, as the plugin's two independent knobs allow.
 *     Centre ~ 230 Hz (plugin default 0.5 = 220 Hz); Tone 127 ~ 7 kHz.
 *     Tone fully down stops the carriers: the wet path is then silent.
 *   Soar C = 0.0: plugin default.
 * The carrier increment ramps across each call from the previous call's
 * value, as in the original. */
typedef struct { real sinePos, incA; } ringmod_ch;
typedef struct { ringmod_ch ch[2]; uint32_t primed; } ringmod_t;

static void m_ringmod(ringmod_t *s, real amt, real tone, float *L, float *R, int n)
{
    real Fr[2];
    Fr[0] = (real)0.99f * tone; Fr[1] = tone;
    real soar = (real)0.3f - ((real)0.0f * (real)0.3f);
    real s2 = soar * soar;
    real wet = R_POW(amt, (real)2.0f);
    const real TWO_PI = (real)6.283185307179586f;
    const real PI = (real)3.14159265358979f, HP = (real)1.57079632679489661923f;
#ifdef CHR_FASTF
    (void)HP;                                    /* the fold below needs only PI */
#endif
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        ringmod_ch st = s->ch[c];
        /* a NaN phase would silence the wet path for ever (the clamp below
         * swallows it), so a non-finite or out-of-range phase restarts at 0 */
        if (!(st.sinePos >= (real)0.0f && st.sinePos <= (real)6.3f)) st.sinePos = (real)0.0f;
        if (!(st.incA >= (real)0.0f && st.incA <= (real)1.0f)) st.incA = (real)0.0f;
        real incB = R_POW(Fr[c], (real)5.0f) / (real)CHR_OVERALLSCALE;
        if (!s->primed) st.incA = incB;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            real dry = x;
            real temp = (real)(n - 1 - i) / (real)n;
            real inc = (st.incA * temp) + (incB * ((real)1.0f - temp));
            st.sinePos += inc;
            if (st.sinePos > TWO_PI) st.sinePos -= TWO_PI;
            /* sinePos is kept in [0, 2pi]: fold it to [-pi/2, pi/2] by hand
             * rather than paying for the general range reduction */
            real ph = st.sinePos, sg = (real)1.0f;
            if (ph > PI) { ph -= PI; sg = (real)-1.0f; }
#ifdef CHR_FASTF
            /* the line below, for every float: above HP, PI - ph is exact
             * (Sterbenz) and smaller; at or below it, PI - ph rounds to at
             * least HP = PI/2 (checked over all of [0, pi]) */
            ph = __builtin_fminf(ph, PI - ph);
#else
            if (ph > HP) ph = PI - ph;
#endif
            real sn = sg * R_SIN_HALF(ph);
            real snM = R_FABS(sn) + s2;
            real inM = R_FABS(x);
            if (inM < snM) { inM = R_FABS(sn); snM = R_FABS(x) + s2; }
            /* sqrt(inM/snM)*snM == sqrt(inM*snM) for snM > 0; and the
             * original's four sign cases reduce to sign(x)*sign(sn) times the
             * same clipped magnitude (zero when either sign is zero) */
            real mag = R_MAXR(R_SQRT_FX(inM * snM) - soar, (real)0.0f);   /* NaN -> 0 either way */
            real sx = x > (real)0.0f ? (real)1.0f : (x < (real)0.0f ? (real)-1.0f : (real)0.0f);
            real ss = sn > (real)0.0f ? (real)1.0f : (sn < (real)0.0f ? (real)-1.0f : (real)0.0f);
            x = sx * ss * mag;
            if (wet != (real)1.0f) x = (x * wet) + (dry * ((real)1.0f - wet));
            buf[i] = (float)x;
        }
        st.incA = incB;
        s->ch[c] = st;
    }
    s->primed = 1;
}

/* ---- Destruction 4: Fracture2 --------------------------------------------
 * Amount -> Drive A = 0.1 + 0.9*amt (density 1 .. 10 into the sine fold).
 * Tone -> Thresh C (0..1; centre is the plugin default 0.5): how much of the
 * folded sine survives above pi/2. Fractre B = 0 (no extra stages: with the
 * stages the signal grows doubly-exponentially and leaves float range), Output
 * D = 1, Dry/Wet E = 1. Fracture2 ramps its controls across each block. */
typedef struct { real densityA, thresholdA, outputA, wetA; uint32_t primed; } fracture2_t;

static void m_fracture2(fracture2_t *s, real amt, real tone, float *L, float *R, int n)
{
    real densityB = ((real)0.1f + (real)0.9f * amt) * (real)10.0f;
    real thresholdB = tone, outputB = (real)1.0f, wetB = (real)1.0f;
    if (!s->primed) { s->densityA = densityB; s->thresholdA = thresholdB; s->outputA = outputB; s->wetA = wetB; s->primed = 1; }
    const real hp = (real)1.57079632679489661923f;
    real dA = s->densityA, tA = s->thresholdA, oA = s->outputA, wA = s->wetA;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            real dry = x;
            real temp = (real)(n - 1 - i) / (real)n;
            real density = (dA * temp) + (densityB * ((real)1.0f - temp));
            real threshold = (tA * temp) + (thresholdB * ((real)1.0f - temp));
            real output = (oA * temp) + (outputB * ((real)1.0f - temp));
            real wet = (wA * temp) + (wetB * ((real)1.0f - temp));
            x *= density;
            if (x > hp) x = (R_SIN(x) * threshold) + ((real)1.0f * ((real)1.0f - threshold));
            else if (x < -hp) x = (R_SIN(x) * threshold) + ((real)-1.0f * ((real)1.0f - threshold));
            else x = R_SIN_HALF(x);
            x *= output;
            if (wet != (real)1.0f) x = (x * wet) + (dry * ((real)1.0f - wet));
            buf[i] = (float)x;
        }
        CHR_STATE_CHECK(buf, dA, tA, oA, wA);
    }
    s->densityA = densityB; s->thresholdA = thresholdB; s->outputA = outputB; s->wetA = wetB;
}

#define CHR_STATES_DESTRUCTION pockey2_t a; derez3_t b; ringmod_t c; fracture2_t d;
#define CHR_PRESENT_DESTRUCTION 0x0f
#define CHR_INIT_DESTRUCTION(type, st) do { if ((type) == 1) m_derez3_init((derez3_t *)(st)); } while (0)
#define CHR_RUN_DESTRUCTION(type, st, amt, tone, L, R, n) do { switch (type) { \
    case 0: m_pockey2((pockey2_t *)(st), amt, tone, L, R, n); return 1; \
    case 1: m_derez3((derez3_t *)(st), amt, tone, L, R, n); return 1; \
    case 2: m_ringmod((ringmod_t *)(st), amt, tone, L, R, n); return 1; \
    case 3: m_fracture2((fracture2_t *)(st), amt, tone, L, R, n); return 1; \
    } } while (0)

#endif
