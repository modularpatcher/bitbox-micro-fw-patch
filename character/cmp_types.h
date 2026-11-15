/*
 * cmp_types.h -- the compressor Type switch (2.3.10, built with -DCHR_CMP).
 * Included by character.c. See COMPRESSOR-DESIGN.md, "Type switch (2.3.10)".
 *
 * CC 47 on channel 1 picks the master compressor: Stock / Logical4 / VariMu /
 * Thunder, in four bands of 32. Only the active type runs; a change
 * crossfades over 30 ms. Each type keeps its own settings, so CC 41-46 mean
 * the same knob positions on every type:
 *
 *   CC   Stock (hook Dr)  Logical4      VariMu     Thunder
 *   41   Threshold        Threshold     Intensity  Thunder
 *   42   Ratio            Ratio         n/a        n/a
 *   43   Attack           Speed         Speed      n/a
 *   44   Release          Dry/Wet       Dry/Wet    n/a
 *   45   Makeup           Makeup        Output     Output
 *   46   Auto makeup      Auto makeup   Auto       Auto     (cmp_engine.h)
 *
 * Stock is hook Dr + hook E exactly as in 2.3.9: hook N's `ncomp` shim goes
 * straight to hook E whenever the store says Stock and nothing is fading, so
 * that path is bit-identical. Otherwise `cmp_block` runs here instead of
 * the stock DSP, on bus 0 (the stock DSP's own buffer, fetched the stock way).
 *
 * The three Airwindows types are ports of Logical4, VariMu and Thunder by
 * Chris Johnson, used under the MIT licence (see the notice in character.c;
 * every source file carries "Airwindows uses the MIT license"). Double ->
 * float, dither and denormal noise dropped, sample rate fixed at 48 kHz.
 * Two changes that are not just precision, both documented where they are:
 *   - Logical4's power-sag delay lines (500-entry mirrored rings, read 2
 *     samples back) are a 3-sample history with the original's exact wrap
 *     rule; a stage that switches in starts its window from zero.
 *   - VariMu and Thunder's mu recursions are rearranged algebraically
 *     (c*(s*s-1)+1)/(s*s) == c + (1-c)/(s*s), so float keeps the +1 at
 *     start-up, when s is 10000 and s*s is past float's integer range.
 *
 * Every type ends in a copy of the stock compressor's safety limiter: peak
 * over the block, a target of 1/peak above 0 dBFS, and the stock gain ramp
 * (attack 192 samples, release 36000) -- so all four types protect the
 * outputs the same way.
 */
#ifndef CMP_TYPES_H
#define CMP_TYPES_H

#include "chr_port.h"

#define CMP_STOCK    0u
#define CMP_LOGICAL4 1u
#define CMP_VARIMU   2u
#define CMP_THUNDER  3u
#define CMP_NTYPES   4u

/* ---- Logical4 ---------------------------------------------------------------
 * A..E are the plugin's own parameters, 0..1: Threshold, Ratio, Speed, Makeup,
 * Dry/Wet. Three ButterComp stages with a power sag ahead of each; Ratio
 * picks and interpolates dry -> A -> B -> C, and B and C run only when Ratio
 * reaches them. The two channels are linked (each converges toward whichever
 * is more compressed), so the processing is interleaved, L then R. */
#define L4_FPOLD     ((real)0.618033988749894848204586f)
#define L4_FPNEW     ((real)0.381966011250105151795414f)   /* 1 - FPOLD */
#define L4_INTENSITY ((real)0.0445556f)
#define L4_POWERSAG  ((real)0.003300223685324102874217f)
/* (int)(2.42 * overallscale) at 48 kHz: the sag's window, the same for all
 * three stages. The ring it reads (d[g+offset], g counting down 499..0, the
 * value mirrored at d[g+499]) gives 2 samples back, except at g = 498 and
 * 499, where it gives 3 -- the mirror is 499 entries away, the cycle 500. */
#define L4_OFFSET    2

/* b5: the original's clamps (`if (x < lo) x = lo;`) as IEEE maxNum/minNum,
 * one VMAXNM/VMINNM each on the M7 instead of a compare, a status move and a
 * select. The same value for every real number; they differ only for a NaN
 * (a fault the engine's guard catches either way) and the sign of an exact
 * zero (which changes no value downstream). Measured against b4 sample by
 * sample: tests/test_comp_types_dsp.py, COMPRESSOR-DESIGN.md Appendix D, b5. */
#ifdef CHR_REF
#define L4_MAX(a, b) fmax((a), (b))
#define L4_MIN(a, b) fmin((a), (b))
#else
#define L4_MAX(a, b) __builtin_fmaxf((a), (b))
#define L4_MIN(a, b) __builtin_fminf((a), (b))
#endif

typedef struct {
    real apos[2], aneg[2], bpos[2], bneg[2];   /* controlA/Bpos/neg, L and R */
    real tpos[2], tneg[2];                     /* targetpos/neg */
    real avg[2], nvg[2];
    real sag[2];                               /* the power sag's control */
    real h[2][3];                              /* its delay line: 1, 2, 3 back */
} l4_stage_t;

typedef struct {
    l4_stage_t s[3];
    int32_t gcount;
    uint32_t flip;                             /* fpFlip: starts true */
    uint32_t live;                             /* bit k: stage k ran last sample */
} logical4_t;

typedef struct {
    real inputgain, inv_compout, outputgain, wet, ratio, invratio;
    real rem[3], div[3];
    int sel;
} l4_coef_t;

static void l4_init(logical4_t *s)
{
    for (int k = 0; k < 3; k++) {
        l4_stage_t *t = &s->s[k];
        for (int c = 0; c < 2; c++) {
            t->apos[c] = t->aneg[c] = t->bpos[c] = t->bneg[c] = (real)1.0f;
            t->tpos[c] = t->tneg[c] = (real)1.0f;
            t->avg[c] = t->nvg[c] = (real)0.0f;
            t->sag[c] = (real)0.0f;
            t->h[c][0] = t->h[c][1] = t->h[c][2] = (real)0.0f;
        }
    }
    s->gcount = 0;
    s->flip = 1;
    s->live = 1;
}

static void l4_coef(l4_coef_t *k, real A, real B, real C, real D, real E)
{
    k->inputgain = R_POW((real)10.0f, (-((A * (real)40.0f) - (real)20.0f)) / (real)20.0f);
    k->inv_compout = (real)1.0f / k->inputgain;
    real attackspeed = ((C * C) * (real)99.0f) + (real)1.0f;
    attackspeed = (real)10.0f / R_SQRT(attackspeed);
    real os = (real)CHR_OVERALLSCALE;
    real d0 = ((real)0.000782f * attackspeed) / os;
    real d1 = ((real)0.000819f * attackspeed) / os;
    real d2 = (real)0.000857f / os;
    k->rem[0] = d0; k->div[0] = (real)1.0f - d0;
    k->rem[1] = d1; k->div[1] = (real)1.0f - d1;
    k->rem[2] = d2 * attackspeed; k->div[2] = (real)1.0f - d2;   /* sic: C's divisor ignores Speed */
    real ratio = R_SQRT(((B * B) * (real)15.0f) + (real)1.0f) - (real)1.0f;
    if (ratio > (real)2.99999f) ratio = (real)2.99999f;
    if (ratio < (real)0.0f) ratio = (real)0.0f;
    int sel = (int)R_FLOOR(ratio);
    k->sel = sel;
    k->ratio = ratio - (real)sel;
    k->invratio = (real)1.0f - k->ratio;
    k->outputgain = R_POW((real)10.0f, ((D * (real)40.0f) - (real)20.0f) / (real)20.0f);
    k->wet = E;
}

/* One power sag, channel c of stage t, on sample x. */
static inline __attribute__((always_inline)) real l4_sag(l4_stage_t *t, int c, real x, int32_t g)
{
    real d = R_FABS(x) * (L4_INTENSITY - ((t->apos[c] + t->bpos[c] + t->aneg[c] + t->bneg[c]) * L4_POWERSAG));
    real old = g >= 498 ? t->h[c][2] : t->h[c][1];
    t->h[c][2] = t->h[c][1]; t->h[c][1] = t->h[c][0]; t->h[c][0] = d;
    real ctl = t->sag[c];
    ctl += d / (real)L4_OFFSET;
    ctl -= old / (real)L4_OFFSET;
    ctl -= (real)0.000001f;
    ctl = L4_MAX(ctl, (real)0.0f);                                      /* if (ctl < 0) ctl = 0 */
    real clamp = (real)1.0f - L4_MAX(ctl - (real)1.0f, (real)0.0f);     /* if (ctl > 1) clamp -= ctl - 1 */
    ctl = L4_MIN(ctl, (real)1.0f);                                      /*              ctl = 1 */
    clamp = L4_MAX(clamp, (real)0.5f);                                  /* if (clamp < 0.5) ... */
    t->sag[c] = ctl;
    real thickness = (((real)1.0f - ctl) * (real)2.0f) - (real)1.0f;
    real out = R_FABS(thickness);
    real br = L4_MIN(R_FABS(x), (real)1.57079633f);
    if (thickness > (real)0.0f) br = R_SIN_HALF(br);
    else br = (real)1.0f - R_COS_HALF(br);
    if (x > (real)0.0f) x = (x * ((real)1.0f - out)) + (br * out);
    else x = (x * ((real)1.0f - out)) - (br * out);
    return x * clamp;                                                   /* x 1 is exact */
}

/* One ButterComp, channel c of stage k, on sample x (already gained for
 * stage A). Returns the clipped, compressed sample; the caller divides by
 * compoutgain for this stage's output. `quirk`: stage C's right channel
 * updates the LEFT positive target (targetposCL) and never its own -- the
 * original's typo, kept: it is part of how Logical4 sounds at high Ratio. */
static inline __attribute__((always_inline)) real l4_comp(l4_stage_t *t, int c, real x, real rem, real div, uint32_t flip, int quirk)
{
    int o = c ^ 1;
    real inputpos = (x * L4_FPOLD) + (t->avg[c] * L4_FPNEW) + (real)1.0f;
    t->avg[c] = x;
    inputpos = L4_MAX(inputpos, (real)0.001f);
    real outputpos = L4_MIN(inputpos / (real)2.0f, (real)1.0f);
    inputpos *= inputpos;
    real dr = L4_MIN(rem * (inputpos + (real)1.0f), (real)1.0f);
    real dd = (real)1.0f - dr;
    real *tp = quirk ? &t->tpos[0] : &t->tpos[c];
    *tp *= dd;
    *tp += (inputpos * dr);
    real ip = (real)1.0f / t->tpos[c];
    real calcpos = ip * ip;

    real inputneg = (-x * L4_FPOLD) + (t->nvg[c] * L4_FPNEW) + (real)1.0f;
    t->nvg[c] = -x;
    inputneg = L4_MAX(inputneg, (real)0.001f);
    real outputneg = L4_MIN(inputneg / (real)2.0f, (real)1.0f);
    inputneg *= inputneg;
    dr = L4_MIN(rem * (inputneg + (real)1.0f), (real)1.0f);
    dd = (real)1.0f - dr;
    t->tneg[c] *= dd;
    t->tneg[c] += (inputneg * dr);
    real in = (real)1.0f / t->tneg[c];
    real calcneg = in * in;

    /* >= : the original replaces an exactly-zero sample with a tiny positive
     * one (its denormal guard, dropped here), so zero takes the positive bank. */
    /* The link: `if (o > c) o = (o + c) * 0.5` is exactly o = min(o, (o + c) * 0.5)
     * -- if o <= c the sum is at least 2o, which rounds to no less than 2o, so the
     * halfway point is never below o; if o > c it is the original's value. */
    if (x >= (real)0.0f) {
        if (flip) {
            t->apos[c] *= div; t->apos[c] += (calcpos * rem);
            t->apos[o] = L4_MIN(t->apos[o], (t->apos[o] + t->apos[c]) * (real)0.5f);
        } else {
            t->bpos[c] *= div; t->bpos[c] += (calcpos * rem);
            t->bpos[o] = L4_MIN(t->bpos[o], (t->bpos[o] + t->bpos[c]) * (real)0.5f);
        }
    } else {
        if (flip) {
            t->aneg[c] *= div; t->aneg[c] += (calcneg * rem);
            t->aneg[o] = L4_MIN(t->aneg[o], (t->aneg[o] + t->aneg[c]) * (real)0.5f);
        } else {
            t->bneg[c] *= div; t->bneg[c] += (calcneg * rem);
            t->bneg[o] = L4_MIN(t->bneg[o], (t->bneg[o] + t->bneg[c]) * (real)0.5f);
        }
    }
    real tm = flip ? (t->apos[c] * outputpos) + (t->aneg[c] * outputneg)
                   : (t->bpos[c] * outputpos) + (t->bneg[c] * outputneg);
    x *= tm;                                                            /* x 1 is exact */
    return L4_MAX(L4_MIN(x, (real)36.0f), (real)-36.0f);                /* +-36, as the original */
}

/* b5: with the clamps as maxNum/minNum (L4_MAX/L4_MIN) a NaN in the state no
 * longer reaches the output -- min/max return the number, and the +-36 clip
 * turns the stage's NaN into full scale -- so the engine's output guard would
 * never see it and the type would stay broken. The engine checks the state
 * itself once per block instead (cmp_run_aw): x * 0 is 0 for every finite x
 * and NaN for a NaN or an infinity, so one sum tells. */
_Static_assert(sizeof(l4_stage_t) == 24 * sizeof(real), "l4_stage_t is 24 reals, packed");
static int l4_state_finite(const logical4_t *s)
{
    const real *p = &s->s[0].apos[0];
    real acc = (real)0.0f;
    for (int i = 0; i < 3 * 24; i++) acc += p[i] * (real)0.0f;
    return acc == acc;
}

static void l4_stage_wake(l4_stage_t *t)
{
    for (int c = 0; c < 2; c++) {
        t->sag[c] = (real)0.0f;
        t->h[c][0] = t->h[c][1] = t->h[c][2] = (real)0.0f;
    }
}

/* In place, n samples, with the coefficients for this piece. R may not be 0:
 * the engine always has both channels (the stock fetch makes a mono bus
 * stereo, as it does for the stock compressor). */
static void m_logical4(logical4_t *s, const l4_coef_t *k, float *L, float *R, int n)
{
    logical4_t st = *s;
    for (int k2 = 1; k2 < 3; k2++)                 /* a stage switching in: fresh window */
        if (k->sel >= k2 && !(st.live & (1u << k2))) l4_stage_wake(&st.s[k2]);
    st.live = 1u | (k->sel >= 1 ? 2u : 0u) | (k->sel >= 2 ? 4u : 0u);
    for (int i = 0; i < n; i++) {
        real xL = (real)L[i], xR = (real)R[i];
        real dryL = xL, dryR = xR;
        st.gcount--;
        if (st.gcount < 0 || st.gcount > 499) st.gcount = 499;
        int32_t g = st.gcount;
        uint32_t flip = st.flip;
        real oA[2], oB[2] = { (real)0.0f, (real)0.0f }, oC[2] = { (real)0.0f, (real)0.0f };

        xL = l4_sag(&st.s[0], 0, xL, g);
        xR = l4_sag(&st.s[0], 1, xR, g);
        if (k->inputgain != (real)1.0f) xL *= k->inputgain;
        xL = l4_comp(&st.s[0], 0, xL, k->rem[0], k->div[0], flip, 0);
        oA[0] = xL * k->inv_compout;
        if (k->inputgain != (real)1.0f) xR *= k->inputgain;
        xR = l4_comp(&st.s[0], 1, xR, k->rem[0], k->div[0], flip, 0);
        oA[1] = xR * k->inv_compout;
        if (k->sel > 0) {
            xL = l4_sag(&st.s[1], 0, xL, g);
            xR = l4_sag(&st.s[1], 1, xR, g);
            xL = l4_comp(&st.s[1], 0, xL, k->rem[1], k->div[1], flip, 0);
            oB[0] = xL * k->inv_compout;
            xR = l4_comp(&st.s[1], 1, xR, k->rem[1], k->div[1], flip, 0);
            oB[1] = xR * k->inv_compout;
            if (k->sel > 1) {
                xL = l4_sag(&st.s[2], 0, xL, g);
                xR = l4_sag(&st.s[2], 1, xR, g);
                xL = l4_comp(&st.s[2], 0, xL, k->rem[2], k->div[2], flip, 0);
                oC[0] = xL * k->inv_compout;
                xR = l4_comp(&st.s[2], 1, xR, k->rem[2], k->div[2], flip, 1);
                oC[1] = xR * k->inv_compout;
            }
        }
        real yL, yR;
        switch (k->sel) {
        case 0:  yL = (dryL * k->invratio) + (oA[0] * k->ratio); yR = (dryR * k->invratio) + (oA[1] * k->ratio); break;
        case 1:  yL = (oA[0] * k->invratio) + (oB[0] * k->ratio); yR = (oA[1] * k->invratio) + (oB[1] * k->ratio); break;
        default: yL = (oB[0] * k->invratio) + (oC[0] * k->ratio); yR = (oB[1] * k->invratio) + (oC[1] * k->ratio); break;
        }
        if (k->outputgain != (real)1.0f) { yL *= k->outputgain; yR *= k->outputgain; }
        if (k->wet != (real)1.0f) {
            yL = (yL * k->wet) + (dryL * ((real)1.0f - k->wet));
            yR = (yR * k->wet) + (dryR * ((real)1.0f - k->wet));
        }
        st.flip = !flip;
        L[i] = (float)yL;
        R[i] = (float)yR;
    }
    *s = st;
}

/* ---- the mu recursion VariMu and Thunder share -------------------------------
 * coef/speed per bank, `sense` the detector value, `thr` the threshold.
 * Rearranged from the original's divisions (see the header): identical in
 * exact arithmetic, stable in float. */
static inline void mu_step(real *coef, real *speed, real sense, real thr, real release, real fastest)
{
    real s = *speed;
    if (sense > thr) {
        real vary = thr / sense;
        real attack = R_SQRT(R_FABS(s));
        real target = vary < thr ? thr : vary;
        *coef = *coef + (target - *coef) / attack;            /* (c*(a-1) + t) / a */
    } else {
        *coef = *coef + ((real)1.0f - *coef) / (s * s);         /* (c*(s*s-1) + 1) / (s*s) */
    }
    *speed = (s - (real)1.0f) + (sense * release + fastest) / s; /* (s*(s-1) + |sense*rel| + f) / s */
}

/* ---- VariMu -----------------------------------------------------------------
 * A Intensity, B Speed, C Output, D Dry/Wet, all 0..1. The detector is the
 * square of the smaller of this sample and the last (per channel). Output is
 * applied only when outGain * C < 1, as in the original: the top of the
 * Output knob, where outGain * C would boost, does nothing. */
typedef struct {
    real speedA[2], speedB[2], coefA[2], coefB[2], prev[2];
    uint32_t flip;
} varimu_t;

typedef struct {
    real threshold, makeup, release, fastest, output, wet;
    int force;                   /* auto makeup: apply `output` even at or above 1 */
} vm_coef_t;

static void vm_init(varimu_t *s)
{
    for (int c = 0; c < 2; c++) {
        s->speedA[c] = s->speedB[c] = (real)10000.0f;
        s->coefA[c] = s->coefB[c] = (real)1.0f;
        s->prev[c] = (real)0.0f;
    }
    s->flip = 0;
}

static void vm_coef(vm_coef_t *k, real A, real B, real C, real D)
{
    real oneA = (real)1.0f - A;
    k->threshold = (real)1.001f - ((real)1.0f - (oneA * oneA * oneA));
    real m = R_SQRT((real)1.0f / k->threshold);
    m = (m + R_SQRT(m)) / (real)2.0f;
    m = R_SQRT(m);
    k->makeup = m;
    real outGain = R_SQRT(m);
    real b = (real)1.15f - B;
    k->release = ((b * b) * (b * b) * b) * (real)32768.0f;
    k->release /= ((real)2.0f * (real)CHR_OVERALLSCALE);       /* VariMu's overallscale is 2 * SR/44100 */
    k->fastest = R_SQRT(k->release);
    k->output = outGain * C;
    k->wet = D;
    k->force = 0;
}

/* SC In (2.3.11, CHR_SC): a gate's or a note's level, detected in place of the
 * mix; NULL, the type detects itself. The gain still multiplies the mix. */
#ifdef CHR_SC
#define CMP_SC_PARAM , const float *key
#define CMP_SC_ARG , sc
#else
#define CMP_SC_PARAM
#define CMP_SC_ARG
#endif

static void m_varimu(varimu_t *s, const vm_coef_t *k, float *L, float *R, int n CMP_SC_PARAM)
{
    varimu_t st = *s;
    for (int i = 0; i < n; i++) {
        real x[2] = { (real)L[i], (real)R[i] };
        real dry[2] = { x[0], x[1] };
        for (int c = 0; c < 2; c++) {
#ifdef CHR_SC
            real d = key ? (real)*key : x[c];       /* what the detector hears */
#else
            real d = x[c];
#endif
            real sq = R_FABS(d) > R_FABS(st.prev[c]) ? st.prev[c] * st.prev[c] : d * d;
            st.prev[c] = d;
            x[c] *= k->makeup;
            if (st.flip) mu_step(&st.coefA[c], &st.speedA[c], sq, k->threshold, k->release, k->fastest);
            else         mu_step(&st.coefB[c], &st.speedB[c], sq, k->threshold, k->release, k->fastest);
        }
        for (int c = 0; c < 2; c++) {
            real co = st.flip ? st.coefA[c] : st.coefB[c];
            x[c] *= (co + co * co) / (real)2.0f;
            if (k->force || k->output < (real)1.0f) x[c] *= k->output;
            if (k->wet < (real)1.0f) x[c] = (dry[c] * ((real)1.0f - k->wet)) + (x[c] * k->wet);
        }
        st.flip = !st.flip;
        L[i] = (float)x[0];
        R[i] = (float)x[1];
    }
    *s = st;
}

/* ---- Thunder ----------------------------------------------------------------
 * A Thunder, B Output (0..1, attenuation only). A linked mu compressor on
 * the highpassed signal plus a FathomFive-style sub reinforcement steered by
 * the summed lows. Unlike VariMu, the original never toggles `flip` (it is
 * false from the constructor on), so only its B bank of the mu recursion
 * ever runs; the port keeps that one bank. */
typedef struct {
    real speed, coef;
    real gate[2], iirA[2], iirB[2], iirAM, iirBM, iirCM;
} thunder_t;

typedef struct { real thunder, threshold, makeup, release, fastest, eq, dcblock, basstrim, out; } th_coef_t;

static void th_init(thunder_t *s)
{
    s->speed = (real)10000.0f;
    s->coef = (real)1.0f;
    for (int c = 0; c < 2; c++) { s->gate[c] = s->iirA[c] = s->iirB[c] = (real)0.0f; }
    s->iirAM = s->iirBM = s->iirCM = (real)0.0f;
}

static void th_coef(th_coef_t *k, real A, real B)
{
    k->thunder = A * (real)0.4f;
    k->threshold = (real)1.0f - (k->thunder * (real)2.0f);
    if (k->threshold < (real)0.01f) k->threshold = (real)0.01f;
    k->makeup = (real)1.0f / k->threshold;
    real b = (real)1.28f - k->thunder;
    k->release = ((b * b) * (b * b) * b) * (real)32768.0f;
    k->release /= (real)CHR_OVERALLSCALE;
    k->fastest = R_SQRT(k->release);
    k->eq = ((real)0.0275f / (real)CHR_SR) * (real)32000.0f;
    k->dcblock = k->eq / (real)300.0f;
    k->basstrim = ((real)0.01f / k->eq) + (real)1.0f;
    k->out = B;
}

static void m_thunder(thunder_t *s, const th_coef_t *k, float *L, float *R, int n CMP_SC_PARAM)
{
    thunder_t st = *s;
    const real EQ = k->eq, th = k->thunder;
    for (int i = 0; i < n; i++) {
        real xL = (real)L[i] * k->makeup, xR = (real)R[i] * k->makeup;
        if (st.gate[0] < R_FABS(xL)) st.gate[0] = xL; else st.gate[0] -= k->dcblock;   /* sic: signed */
        if (st.gate[1] < R_FABS(xR)) st.gate[1] = xR; else st.gate[1] -= k->dcblock;

        st.iirA[0] += (xL * EQ * th);
        st.iirA[0] -= (st.iirA[0] * st.iirA[0] * st.iirA[0] * EQ);
        if (st.iirA[0] > st.gate[0]) st.iirA[0] -= k->dcblock;
        if (st.iirA[0] < -st.gate[0]) st.iirA[0] += k->dcblock;
        real rL = st.iirA[0] * k->basstrim;
        st.iirB[0] = (st.iirB[0] * ((real)1.0f - EQ)) + (rL * EQ);
        rL = st.iirB[0];

        st.iirA[1] += (xR * EQ * th);
        st.iirA[1] -= (st.iirA[1] * st.iirA[1] * st.iirA[1] * EQ);
        if (st.iirA[1] > st.gate[1]) st.iirA[1] -= k->dcblock;
        if (st.iirA[1] < -st.gate[1]) st.iirA[1] += k->dcblock;
        real rR = st.iirA[1] * k->basstrim;
        st.iirB[1] = (st.iirB[1] * ((real)1.0f - EQ)) + (rR * EQ);
        rR = st.iirB[1];

        st.iirAM += ((xL + xR) * EQ * th);
        st.iirAM -= (st.iirAM * st.iirAM * st.iirAM * EQ);
        real rM = st.iirAM * k->basstrim;
        st.iirBM = (st.iirBM * ((real)1.0f - EQ)) + (rM * EQ);
        rM = st.iirBM;
        st.iirCM = (st.iirCM * ((real)1.0f - EQ)) + (rM * EQ);
        rM = R_FABS(st.iirCM);
        real rML = R_FABS(rL), rMR = R_FABS(rR);
        if (rM > rML) rML = rM;
        if (rM > rMR) rMR = rM;
        if (rML > (real)1.0f) rML = (real)1.0f;
        if (rMR > (real)1.0f) rMR = (real)1.0f;
        (void)rML; (void)rMR;                     /* computed by the original, then unused */

        xL -= (st.iirB[0] * th);
        xR -= (st.iirB[1] * th);
        real sense = R_FABS(xL);
        if (R_FABS(xR) > sense) sense = R_FABS(xR);
#ifdef CHR_SC
        if (key) sense = (real)*key * k->makeup;   /* SC In: the key, as the mix would reach it */
#endif
        mu_step(&st.coef, &st.speed, sense, k->threshold, k->release, k->fastest);
        real coefficient = st.coef * st.coef;
        xL *= coefficient;
        xR *= coefficient;
        xL += (rL * rM);
        xR += (rR * rM);
        if (k->out != (real)1.0f) { xL *= k->out; xR *= k->out; }
        L[i] = (float)xL;
        R[i] = (float)xR;
    }
    *s = st;
}

/* ---- the safety limiter: the stock compressor's last stage ------------------
 * FUN_080a16c0 (set target) and FUN_080a1598 / FUN_0809f96c (apply the ramp),
 * read from the 2.3.4 image: a linear attack to the target over `att`
 * samples, then a linear release back to unity over `rel`. */
typedef struct {
    float gain, start, target;
    int32_t left, len, att, rel;
    uint32_t state;                  /* 0 idle (unity), 1 attack, 2 release */
} cmp_ramp_t;

#define CMP_LIM_ATTACK  192          /* the stock limiter's: PROOF_A_VAL */
#define CMP_LIM_RELEASE 36000        /* PROOF_R_VAL */

static void ramp_reset(cmp_ramp_t *r, int32_t att, int32_t rel)
{
    r->gain = r->start = r->target = 1.0f;
    r->left = 0; r->len = 0; r->att = att; r->rel = rel; r->state = 0;
}

static void ramp_target(cmp_ramp_t *r, float t)
{
    if (r->state == 0) {
        r->target = t; r->len = r->att; r->start = 1.0f; r->left = r->att; r->state = 1;
    } else if (r->state == 1) {
        if (t < r->target) { r->target = t; r->start = r->gain; r->left = r->att; r->len = r->att; }
    } else if (r->state == 2) {
        if (t < r->gain) { r->start = r->gain; r->target = t; r->state = 1; r->left = r->att; r->len = r->att; }
    }
}

static void ramp_mul(float g0, float g1, float *b, int n)
{
    float inc = (g1 - g0) / (float)n;
    for (int i = 0; i < n; i++) { b[i] = b[i] * g0; g0 = g0 + inc; }
}

static void ramp_apply(cmp_ramp_t *r, float *L, float *R, int n)
{
    int i = 0;
    while (n > 0) {
        while (r->left == 0) {
            if (r->state == 1) {                     /* attack done: release to unity */
                r->start = r->gain; r->target = 1.0f; r->state = 2;
                r->left = r->rel; r->len = r->rel;
                if (r->left == 0) { r->state = 0; return; }
            } else {
                if (r->state == 2) { r->len = 0; r->start = r->target = r->gain = 1.0f; r->state = 0; }
                return;                              /* idle: unity, nothing to do */
            }
        }
        int k = n < r->left ? n : r->left;
        float end = r->target;
        if (k < r->left) {
            float lf = (float)r->len;
            end = ((float)(r->left - k) * r->start) / lf + ((float)(r->len - (r->left - k)) * r->target) / lf;
        }
        ramp_mul(r->gain, end, L + i, k);
        if (R) ramp_mul(r->gain, end, R + i, k);
        n -= k; i += k;
        r->left -= k;
        float lf = (float)r->len;
        r->gain = ((float)(r->len - r->left) * r->target) / lf + ((float)r->left * r->start) / lf;
    }
}

static void cmp_limit(cmp_ramp_t *r, float *L, float *R, int n)
{
    float peak = 0.0f;
    for (int i = 0; i < n; i++) {
        float a = chr_fabsf(L[i]), b = chr_fabsf(R[i]);
        if (a > peak) peak = a;
        if (b > peak) peak = b;
    }
    if (peak > 1.0f) ramp_target(r, 1.0f / peak);
    ramp_apply(r, L, R, n);
}

#endif
