/*
 * chr_port.h -- the one switch between the firmware build and the host
 * reference build of the ported effects.
 *
 *   firmware / host-float:  real = float, maths from chr_math.h
 *   host reference (-DCHR_REF): real = double, maths from the host libm
 *
 * Every port is written once against `real` and the R_* macros, so the
 * reference build is the original Airwindows algorithm in double precision
 * with libm, and the firmware build is the same algorithm in float with the
 * approximations. tests/test_character_dsp.py compares the two.
 *
 * All numeric literals in the ports carry an `f` suffix so the float build
 * never silently promotes to double (the firmware build adds
 * -Werror=double-promotion to enforce it). In the reference build those
 * float constants widen exactly, so the reference differs from the original
 * source only by constants rounded to float -- around 1e-8 relative, well
 * under every tolerance the tests use.
 */
#ifndef CHR_PORT_H
#define CHR_PORT_H

#include "chr_math.h"

#ifdef CHR_REF
#include <math.h>
typedef double real;
#define R_SIN(x)    sin(x)
#define R_SIN_HALF(x) sin(x)      /* caller guarantees |x| <= pi/2 */
#define R_COS_HALF(x) cos(x)      /* caller guarantees 0 <= x <= pi/2 */
#define R_COS(x)    cos(x)
#define R_TAN(x)    tan(x)
#define R_POW(x, y) pow((x), (y))
#define R_SQRT(x)   sqrt(x)
#define R_LOG(x)    log(x)
#define R_LOG_FAST(x) log(x)
#define R_EXP(x)    exp(x)
#define R_EXP2(x)   exp2(x)
#define R_FLOOR(x)  floor(x)
#define R_FABS(x)   fabs(x)
#else
typedef float real;
#define R_SIN(x)    chr_sinf(x)
#define R_SIN_HALF(x) chr_sin_half(x)
#define R_COS_HALF(x) chr_cos_half(x)
#define R_COS(x)    chr_cosf(x)
#define R_TAN(x)    chr_tanf(x)
#define R_POW(x, y) chr_powf((x), (y))
#define R_SQRT(x)   chr_sqrtf(x)
#define R_LOG(x)    chr_logf(x)
#define R_LOG_FAST(x) chr_log_fast(x)
#define R_EXP(x)    chr_expf(x)
#define R_EXP2(x)   chr_exp2f(x)     /* exact at integers */
#define R_FLOOR(x)  chr_floorf(x)
#define R_FABS(x)   chr_fabsf(x)
#endif

/* The module runs at 48 kHz (EQ object +0x20 = 48000, block +0x24 = 128).
 * Airwindows scales its time constants by overallscale = SR / 44100. */
#define CHR_SR           48000.0f
#define CHR_OVERALLSCALE (48000.0f / 44100.0f)

/* Airwindows' fpd generator: xorshift32, advanced once per sample per channel
 * by the dither block. The dither itself is dropped; the generator is kept
 * only where an effect uses it as part of its sound (flutter, tape dust). */
static inline uint32_t chr_xorshift(uint32_t x)
{
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}
/* Once-per-block flush of recursive state that has decayed below anything
 * audible, standing in for the originals' per-sample 1.18e-37 checks (the
 * firmware also runs the FPU flush-to-zero: FPSCR FZ|DN set at reset). */
static inline real chr_flush_r(real x) { return R_FABS(x) < (real)1.0e-30f ? (real)0.0f : x; }

static inline real chr_maxr(real a, real b) { return a > b ? a : b; }
static inline real chr_minr(real a, real b) { return a < b ? a : b; }

#define CHR_U32_MAX_R ((real)4294967295.0f)

#endif
