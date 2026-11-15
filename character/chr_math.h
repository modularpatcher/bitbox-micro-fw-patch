/*
 * chr_math.h -- the Character chain's own float maths. No libc is linked into
 * the firmware hooks, so every transcendental the ported effects need is
 * approximated here. Accuracy targets (checked by tests/test_character_dsp.py
 * against the host libm in double precision):
 *
 *   sin/cos      absolute error < 2e-7 for |x| <= 64 (measured 1.2e-7)
 *   exp          relative error < 2e-6 for |x| <= 20 (1.0e-6; the float
 *                product x*log2(e) is the limit, not the polynomial)
 *   log          error < 2e-7, absolute where |log x| < 1, relative beyond
 *   pow          relative error < 1e-5 where |y log x| < 40 (6.2e-6)
 *   tanh         absolute error < 2e-7 (1.1e-7)
 *
 * Float audio is accurate to ~6e-8 relative, so these sit at the same order
 * as the rounding the port already accepts by going from double to float.
 *
 * Everything is static inline and branch-light. Inputs that would be NaN in
 * libm (log of <= 0, pow of a negative base) return finite values instead:
 * a NaN reaching the master bus is silence, which is worse than an error.
 */
#ifndef CHR_MATH_H
#define CHR_MATH_H

#include <stdint.h>

#define CHR_PI      3.14159265358979323846f
#define CHR_HALF_PI 1.57079632679489661923f

/* 2.3.10 (-DCHR_FAST, the chr10 object; firmware and host float, never the
 * double reference): clamps become vminnm/vmaxnm, one instruction each with
 * no flags, instead of a vcmp, a vmrs that waits for it and a branch. For an
 * ordered value the result is the same bits whenever the bound is not zero
 * (+0 and -0 are the only equal values with different bits). A NaN differs:
 * a compare lets it through, vminnm/vmaxnm return the bound -- so in this
 * build every effect checks its own state (CHR_STATE_CHECK, chr_port.h).
 * Without CHR_FAST everything below is the 2.3.9 code. */
#if defined(CHR_FAST) && !defined(CHR_REF)
#define CHR_FASTF 1
#define CHR_CLAMP_IN(x, lo, hi) __builtin_fmaxf(__builtin_fminf((x), (hi)), (lo))
#else
#define CHR_CLAMP_IN(x, lo, hi) chr_clampf((x), (lo), (hi))
#endif

static inline float chr_fabsf(float x) { return __builtin_fabsf(x); }

static inline float chr_sqrtf(float x) { return x > 0.0f ? __builtin_sqrtf(x) : 0.0f; }
#ifdef CHR_FASTF
/* The effects' per-sample square roots (R_SQRT_FX). The same bits as
 * chr_sqrtf for every input, NaN and -0 included: both give +0 for anything
 * not above zero. The compressor keeps chr_sqrtf: swapping it there moves
 * Logical4's register allocation and costs it spills. */
static inline float chr_sqrtf_fx(float x) { return __builtin_sqrtf(__builtin_fmaxf(x, 0.0f)); }
#else
#define chr_sqrtf_fx chr_sqrtf
#endif

static inline float chr_floorf(float x) { return __builtin_floorf(x); }

static inline float chr_ceilf(float x) { return __builtin_ceilf(x); }

static inline float chr_minf(float a, float b) { return a < b ? a : b; }
static inline float chr_maxf(float a, float b) { return a > b ? a : b; }
static inline float chr_clampf(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static inline uint32_t chr_f2u(float f) { union { float f; uint32_t u; } v; v.f = f; return v.u; }
static inline float chr_u2f(uint32_t u) { union { float f; uint32_t u; } v; v.u = u; return v.f; }

/* finite: not NaN, not +-inf */
static inline int chr_finite(float x) { return (chr_f2u(x) & 0x7f800000u) != 0x7f800000u; }

/* Fused multiply-add. __builtin_fmaf becomes one vfma.f32 on the M7 and one
 * fmadd on the host, both correctly rounded, so the host float build and
 * the firmware stay bit-identical while the kernels get cheaper. */
#define CHR_FMA(a, b, c) __builtin_fmaf((a), (b), (c))

/* sin on [-pi/2, pi/2]: odd Taylor series through x^13. Truncation error at
 * pi/2 is x^15/15! ~ 6.7e-10, far below float rounding. */
static inline float chr_sin_kernel(float r)
{
    float r2 = r * r;
    float p = -1.6059043836821614599e-10f;          /* -1/13! */
    p = CHR_FMA(p, r2, 2.5052108385441718775e-8f);  /*  1/11! */
    p = CHR_FMA(p, r2, -2.7557319223985890653e-6f); /* -1/9!  */
    p = CHR_FMA(p, r2, 1.9841269841269841270e-4f);  /*  1/7!  */
    p = CHR_FMA(p, r2, -8.3333333333333333333e-3f); /* -1/5!  */
    p = CHR_FMA(p, r2, 1.6666666666666666667e-1f);  /*  1/3!  */
    return CHR_FMA(-(r * r2), p, r);
}

/* sin and cos for arguments the caller has ALREADY clamped to [-pi/2, pi/2]
 * (cos: [0, pi/2]) -- the "bridge rectifier" shape Airwindows uses
 * everywhere. No range reduction, a third of the cost of chr_sinf. */
static inline float chr_sin_half(float x) { return chr_sin_kernel(x); }
static inline float chr_cos_half(float x) { return chr_sin_kernel(1.57079632679489661923f - x); }

/* sin(x): reduce by the nearest multiple of pi using a two-part pi (Cody-Waite)
 * so the reduction itself loses nothing for |x| up to ~1e4; then
 * sin(x) = (-1)^k sin(x - k pi). Inputs beyond +-1e5 are clamped: no audio
 * path here produces them, and the clamp keeps k inside an int. */
static inline float chr_sinf(float x)
{
    x = CHR_CLAMP_IN(x, -1.0e5f, 1.0e5f);
    float kf = __builtin_roundevenf(x * 0.31830988618379067154f);   /* 1/pi */
    int32_t k = (int32_t)kf;
    float r = CHR_FMA(-kf, 3.140625f, x);            /* pi high part */
    r = CHR_FMA(-kf, 9.67653589793e-4f, r);          /* pi low part */
    float s = chr_sin_kernel(CHR_CLAMP_IN(r, -1.5707964f, 1.5707964f));
    return (k & 1) ? -s : s;
}

static inline float chr_cosf(float x)
{
    /* cos(x) = sin(pi/2 - |r|) after reducing |x| by the nearest multiple of pi */
    x = chr_fabsf(CHR_CLAMP_IN(x, -1.0e5f, 1.0e5f));
    float kf = __builtin_roundevenf(x * 0.31830988618379067154f);
    int32_t k = (int32_t)kf;
    float r = CHR_FMA(-kf, 3.140625f, x);
    r = CHR_FMA(-kf, 9.67653589793e-4f, r);          /* r in [-pi/2, pi/2] */
    float c = chr_sin_kernel(1.57079632679489661923f - chr_fabsf(r));
    return (k & 1) ? -c : c;
}

/* Used only when designing coefficients, once per block. */
static inline float chr_tanf(float x)
{
    float c = chr_cosf(x);
    if (chr_fabsf(c) < 1.0e-12f) c = c < 0.0f ? -1.0e-12f : 1.0e-12f;
    return chr_sinf(x) / c;
}

/* 2^x: split into integer n and fraction f in [-0.5, 0.5], minimax-ish
 * polynomial (Taylor of e^(f ln2) through degree 7, error < 1.5e-8 rel). */
static inline float chr_exp2_kernel(float x)
{
    float nf = __builtin_roundevenf(x);
    float f = (x - nf) * 0.69314718055994530942f;    /* f ln2, |.| <= 0.347 */
    float p = 1.9841269841269841270e-4f;             /* 1/7! */
    p = CHR_FMA(p, f, 1.3888888888888888889e-3f);           /* 1/6! */
    p = CHR_FMA(p, f, 8.3333333333333333333e-3f);           /* 1/5! */
    p = CHR_FMA(p, f, 4.1666666666666666667e-2f);           /* 1/4! */
    p = CHR_FMA(p, f, 1.6666666666666666667e-1f);           /* 1/3! */
    p = CHR_FMA(p, f, 0.5f);
    p = CHR_FMA(p, f, 1.0f);
    p = CHR_FMA(p, f, 1.0f);
    int32_t n = (int32_t)nf;
    return p * chr_u2f((uint32_t)(n + 127) << 23);
}

static inline float chr_exp2f(float x) { return chr_exp2_kernel(chr_clampf(x, -126.0f, 127.0f)); }

/* R_EXP2 in the effects. The compressor reaches chr_exp2f through chr_expf
 * and chr_powf and keeps the compare clamp, NaN behaviour and all. */
static inline float chr_exp2f_fx(float x) { return chr_exp2_kernel(CHR_CLAMP_IN(x, -126.0f, 127.0f)); }

static inline float chr_expf(float x)
{
    return chr_exp2f(x * 1.44269504088896340736f);
}

/* natural log. x <= 0 (or denormal) returns log(1e-30) rather than -inf/NaN. */
static inline float chr_logf(float x)
{
    if (!(x > 1.0e-30f)) x = 1.0e-30f;
    if (x > 1.0e30f) x = 1.0e30f;
    uint32_t u = chr_f2u(x);
    int32_t e = (int32_t)((u >> 23) & 0xff) - 127;
    float m = chr_u2f((u & 0x007fffffu) | 0x3f800000u);   /* [1, 2) */
    if (m > 1.41421356237f) { m *= 0.5f; e += 1; }        /* [0.707, 1.414) */
    float s = (m - 1.0f) / (m + 1.0f);                    /* |s| <= 0.1716 */
    float s2 = s * s;
    float p = 1.0f / 11.0f;
    p = CHR_FMA(p, s2, 1.0f / 9.0f);
    p = CHR_FMA(p, s2, 1.0f / 7.0f);
    p = CHR_FMA(p, s2, 1.0f / 5.0f);
    p = CHR_FMA(p, s2, 1.0f / 3.0f);
    p = CHR_FMA(p, s2, 1.0f);
    return 2.0f * s * p + (float)e * 0.69314718055994530942f;
}

/* Natural log without a division, for the per-sample mu-law style curves
 * (Pockey2's encoder, ToTape8's Dubly compander): log2(m), m in [1,2), as
 * (m-1)*q(m-1) with q a degree-6 least-squares fit on Chebyshev nodes --
 * absolute error 1.3e-6 in log2, exact 0 at 1. Measured in mathtest. */
static inline float chr_log_fast(float x)
{
#ifdef CHR_FASTF
    x = __builtin_fmaxf(x, 1.0e-30f);                /* same bits, NaN included */
#else
    if (!(x > 1.0e-30f)) x = 1.0e-30f;
#endif
    uint32_t u = chr_f2u(x);
    float e = (float)((int32_t)((u >> 23) & 0xff) - 127);
    float t = chr_u2f((u & 0x007fffffu) | 0x3f800000u) - 1.0f;   /* [0, 1) */
    float q = 2.049034609e-02f;
    q = CHR_FMA(q, t, -9.606624943e-02f);
    q = CHR_FMA(q, t, 2.155885327e-01f);
    q = CHR_FMA(q, t, -3.392477733e-01f);
    q = CHR_FMA(q, t, 4.777059308e-01f);
    q = CHR_FMA(q, t, -7.211627342e-01f);
    q = CHR_FMA(q, t, 1.442693257e+00f);
    return CHR_FMA(t, q, e) * 0.69314718055994530942f;
}

/* x^y for x >= 0. A negative base is treated as its magnitude: every call in
 * the ports is pow(non-negative, y). 0^y = 0 for y > 0, 1 for y == 0. */
static inline float chr_powf(float x, float y)
{
    x = chr_fabsf(x);
    if (x == 0.0f) return y == 0.0f ? 1.0f : 0.0f;
    return chr_exp2f(y * chr_logf(x) * 1.44269504088896340736f);
}

static inline float chr_tanhf(float x)
{
    float a = chr_fabsf(x);
    if (a < 0.02f) {                                 /* series, avoids 1-t loss */
        float a2 = x * x;
        return x * (1.0f - a2 * (1.0f / 3.0f - a2 * (2.0f / 15.0f)));
    }
    if (a > 9.0f) return x > 0.0f ? 1.0f : -1.0f;
    float t = chr_expf(-2.0f * a);
    float r = (1.0f - t) / (1.0f + t);
    return x > 0.0f ? r : -r;
}

/* Flush tiny state to zero so recursive filters never crawl into denormals.
 * (The M7 FPU handles denormals in hardware, but the firmware's FPSCR may
 * have flush-to-zero off, and a decaying state is cheaper as a true zero.) */
static inline float chr_flush(float x) { return chr_fabsf(x) < 1.0e-20f ? 0.0f : x; }

#endif
