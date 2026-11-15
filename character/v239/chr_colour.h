/*
 * Colour bank (slot 3 of 4): Channel9, IronOxide5, ToTape8, Coils2.
 * Ported from Airwindows by Chris Johnson -- see the MIT notice in
 * character.c. Conventions as in chr_saturation.h, except that these four
 * share per-sample state between channels (flutter sweeps, flip toggles, a
 * mono bass detector), so they run sample-interleaved with that state in
 * locals for the block. On a mono bus R is read as a copy of L and never
 * written.
 */
#ifndef CHR_COLOUR_H
#define CHR_COLOUR_H

#include "chr_port.h"

#ifndef CHR_NCH
#define CHR_NCH(R) ((R) ? 2 : 1)
#endif

/* ---- Colour 1: Channel9 --------------------------------------------------
 * Amount -> Drive B = amt (0-200% in the plugin's own display). Tone ->
 * Console Type A = tone, five stepped voicings: Neve, API, SSL (centre),
 * Teac, Mackie. Output C = 1. Changing the console type swaps the slew
 * threshold, the highpass and the ultrasonic lowpass in one step, as in the
 * original; the 40 ms control smoothing does not make that step continuous.
 *
 * DELIBERATE DEVIATION (chr10): the slew clamp cannot hold a Nyquist limit
 * cycle. In the original, once a 48 kHz signal stops abruptly the clamp can
 * lock into a two-sample cycle that sustains itself with no input at all:
 * the clamp fires every sample, reversing direction each time (output
 * +-0.618 x threshold, the clamp's own estimate at 1.146 x threshold, so it
 * keeps firing) -- a 24 kHz tone for ever. Every console does it inside the
 * loop; Neve and API also put it on the output, because their ultrasonic
 * cutoffs (28811, 27216 Hz) assume higher sample rates and the original
 * switches both filters off above Nyquist (`fq < 0.49999`), while SSL, Teac
 * and Mackie's own lowpass happens to hide it. Capping the cutoff was tried
 * first: it silences the output but leaves the cycle running inside the loop.
 * So the loop is broken where it sustains: after CHR_CH9_RUN consecutive
 * clamps that each reverse the one before, the next reversal is not applied,
 * and with nothing to feed it the cycle is gone. 32 is 0.67 ms; on programme
 * material it never triggers -- the output is bit-identical to the original
 * at every console -- and a lock-up dies within 33 samples. (Gating it on the
 * input's own direction was tried and dropped: Channel9's two alternating
 * highpass states make a decaying input alternate too.)
 * CHR_CH9_NOALT 0 builds the original behaviour, for the tests. Measurements:
 * CHARACTER-DESIGN.md section 6, Channel9. */
#ifndef CHR_CH9_NOALT
#define CHR_CH9_NOALT 1
#endif
#ifndef CHR_CH9_RUN
#define CHR_CH9_RUN 32
#endif
typedef struct { real iirA, iirB, lastA, lastB, lastC, a7, a8, a9, a10, b7, b8, b9, b10, lastS, run; } channel9_ch;
typedef struct { channel9_ch ch[2]; uint32_t flip; } channel9_t;

static void m_channel9(channel9_t *s, real amt, real tone, float *L, float *R, int n)
{
    real iirAmount, threshold, cutoff;
    switch ((int32_t)(tone * (real)4.999f)) {
    case 0: iirAmount = (real)0.005832f; threshold = (real)0.33362176f; cutoff = (real)28811.0f; break;
    case 1: iirAmount = (real)0.004096f; threshold = (real)0.59969536f; cutoff = (real)27216.0f; break;
    case 2: iirAmount = (real)0.004913f; threshold = (real)0.84934656f; cutoff = (real)23011.0f; break;
    case 3: iirAmount = (real)0.009216f; threshold = (real)0.149f; cutoff = (real)18544.0f; break;
    default: iirAmount = (real)0.011449f; threshold = (real)0.092f; cutoff = (real)19748.0f; break;
    }
    real localiirAmount = iirAmount / (real)CHR_OVERALLSCALE;
    real density = amt * (real)2.0f;
    real phattity = density - (real)1.0f;
    if (density > (real)1.0f) density = (real)1.0f;
    if (phattity < (real)0.0f) phattity = (real)0.0f;
    real nonLin = (real)5.0f - density;
    real fq = cutoff / (real)CHR_SR;
    real K = R_TAN((real)3.14159265358979f * fq);
    real qa = (real)1.618033988749894848204586f, qb = (real)0.618033988749894848204586f;
    real norm = (real)1.0f / ((real)1.0f + K / qa + K * K);
    real a2 = K * K * norm, a3 = (real)2.0f * a2, a4 = a2;
    real a5 = (real)2.0f * (K * K - (real)1.0f) * norm, a6 = ((real)1.0f - K / qa + K * K) * norm;
    norm = (real)1.0f / ((real)1.0f + K / qb + K * K);
    real b2 = K * K * norm, b3 = (real)2.0f * b2, b4 = b2;
    real b5 = (real)2.0f * (K * K - (real)1.0f) * norm, b6 = ((real)1.0f - K / qb + K * K) * norm;
    int filt = fq < (real)0.49999f;
    uint32_t flip0 = s->flip;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        channel9_ch st = s->ch[c];
        uint32_t flip = flip0;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            if (filt) {
                real t = a2 * x + a3 * st.a7 + a4 * st.a8 - a5 * st.a9 - a6 * st.a10;
                st.a8 = st.a7; st.a7 = x; x = t; st.a10 = st.a9; st.a9 = x;
            }
            real dielectricScale = R_FABS((real)2.0f - ((x + nonLin) / nonLin));
            if (flip) {
                st.iirA = (st.iirA * ((real)1.0f - (localiirAmount * dielectricScale))) + (x * localiirAmount * dielectricScale);
                x = x - st.iirA;
            } else {
                st.iirB = (st.iirB * ((real)1.0f - (localiirAmount * dielectricScale))) + (x * localiirAmount * dielectricScale);
                x = x - st.iirB;
            }
            real dry = x;
            if (x > (real)1.0f) x = (real)1.0f;
            if (x < (real)-1.0f) x = (real)-1.0f;
            real phat = R_SIN_HALF(x * (real)1.57079633f);
            x *= (real)1.2533141373155f;
            real ax = R_FABS(x);
            real dist = R_SIN_HALF(x * ax) / ((ax == (real)0.0f) ? (real)1.0f : ax);
            x = dist;
            if (density < (real)1.0f) x = (dry * ((real)1.0f - density)) + (dist * density);
            if (phattity > (real)0.0f) x = (x * ((real)1.0f - phattity)) + (phat * phattity);
            real clamp = (st.lastB - st.lastC) * (real)0.381966011250105f;
            clamp -= (st.lastA - st.lastB) * (real)0.6180339887498948482045f;
            clamp += x - st.lastA;
            st.lastC = st.lastB;
            st.lastB = st.lastA;
            st.lastA = x;
#if CHR_CH9_NOALT
            {
                /* after CHR_CH9_RUN clamps in a row, each reversing the one
                 * before, the next reversal is not applied */
                real s0 = st.lastS;
                int ok = st.run < (real)CHR_CH9_RUN;
                st.lastS = (real)0.0f;
                if (clamp > threshold && (s0 >= (real)0.0f || ok)) { x = st.lastB + threshold; st.lastS = (real)1.0f; }
                if (-clamp > threshold && (s0 <= (real)0.0f || ok)) { x = st.lastB - threshold; st.lastS = (real)-1.0f; }
                /* consecutive clamps, each reversing the one before */
                st.run = (st.lastS != (real)0.0f && st.lastS == -s0) ? st.run + (real)1.0f : (real)0.0f;
            }
#else
            if (clamp > threshold) x = st.lastB + threshold;
            if (-clamp > threshold) x = st.lastB - threshold;
#endif
            st.lastA = (st.lastA * (real)0.381966011250105f) + (x * (real)0.6180339887498948482045f);
            flip = !flip;
            if (filt) {                              /* output C = 1: no trim */
                real t = b2 * x + b3 * st.b7 + b4 * st.b8 - b5 * st.b9 - b6 * st.b10;
                st.b8 = st.b7; st.b7 = x; x = t; st.b10 = st.b9; st.b9 = x;
            }
            buf[i] = (float)x;
        }
        st.iirA = chr_flush_r(st.iirA); st.iirB = chr_flush_r(st.iirB);
        st.a9 = chr_flush_r(st.a9); st.a10 = chr_flush_r(st.a10);
        st.b9 = chr_flush_r(st.b9); st.b10 = chr_flush_r(st.b10);
        s->ch[c] = st;
    }
    s->flip = flip0 ^ (uint32_t)(n & 1);
}

/* ---- Colour 2: IronOxide5 ------------------------------------------------
 * Amount -> Input Trim A = 0.5 + amt/3 (0 .. +12 dB into the tape) with
 * Output Trim F = 0.5 - amt/6 (-6 dB at full: half the drive taken back).
 * Tone -> tape speed, Tape High B = Tape Low C = tone, the plugin's whole
 * control (ch11; chr1-10 used 0.3 + 0.55*tone). The plugin displays it as
 * B^4 * 148.5 + 1.5 ips: 1.5 ips (Tone 0), 10.8 (centre), 150 (Tone 1); the
 * maths below uses its own x1.1 of that, as the original does. No buffer or
 * loop depends on the speed, so the ends cost nothing more. Flutter D = 0.5 and Noise E = 0.5 are the plugin defaults; the
 * flutter and the slew "dust" use Airwindows' own xorshift generator, kept
 * because it is part of the sound (the dither that also used it is gone).
 * Inv/Dry/Wet G = 1. The flutter, the flip and the generator are shared by
 * both channels and do not depend on the audio, so each channel's pass
 * replays them from the same starting values. */
typedef struct { real fastA, fastB, slowA, slowB, iirA, iirB, prev; } ironoxide5_ch;
typedef struct {
    real dL[264], dR[264], flL[100], flR[100];
    ironoxide5_ch ch[2];
    real sweep, rateof, nextmax;
    int32_t gcount, fstoredcount;
    uint32_t flip, fpdL, fpdR;
} ironoxide5_t;

static void m_ironoxide5_init(ironoxide5_t *s)
{
    s->rateof = (real)0.5f; s->nextmax = (real)0.5f;
    s->fpdL = 0x2545f491u; s->fpdR = 0x6c8e9cf5u;
}

static inline real iron_taps(const real *d, int count)
{
    real temp = d[count + 127];
    temp += d[count + 113]; temp += d[count + 109]; temp += d[count + 107];
    temp += d[count + 103]; temp += d[count + 101]; temp += d[count + 97];
    temp += d[count + 89]; temp += d[count + 83];
    temp /= (real)2.0f;
    temp += d[count + 79]; temp += d[count + 73]; temp += d[count + 71];
    temp += d[count + 67]; temp += d[count + 61]; temp += d[count + 59];
    temp += d[count + 53]; temp += d[count + 47]; temp += d[count + 43];
    temp += d[count + 41]; temp += d[count + 37]; temp += d[count + 31];
    temp += d[count + 29];
    temp /= (real)2.0f;
    temp += d[count + 23]; temp += d[count + 19]; temp += d[count + 17];
    temp += d[count + 13]; temp += d[count + 11];
    temp /= (real)2.0f;
    temp += d[count + 7]; temp += d[count + 5]; temp += d[count + 3];
    temp /= (real)2.0f;
    temp += d[count + 2]; temp += d[count + 1];
    return temp;
}

static inline real iron_sat(real x)
{
    real b = R_FABS(x);
    if (b > (real)1.57079633f) b = (real)1.57079633f;
    b = R_SIN_HALF(b);
    return x > (real)0.0f ? b : -b;
}

static void m_ironoxide5(ironoxide5_t *s, real amt, real tone, float *L, float *R, int n)
{
    real A = (real)0.5f + amt / (real)3.0f, F = (real)0.5f - amt / (real)6.0f;
    real B = tone, C = B, D = (real)0.5f, E = (real)0.5f;
    real inputgain = R_POW((real)10.0f, ((A * (real)36.0f) - (real)18.0f) / (real)20.0f);
    real outputgain = R_POW((real)10.0f, ((F * (real)36.0f) - (real)18.0f) / (real)20.0f);
    real ips = (((B * B) * (B * B) * (real)148.5f) + (real)1.5f) * (real)1.1f;
    if (ips < (real)1.0f || ips > (real)200.0f) ips = (real)33.0f;
    real tempRandy = (real)0.04f + ((real)0.11f / R_SQRT(ips));
    real lps = (((C * C) * (C * C) * (real)148.5f) + (real)1.5f) * (real)1.1f;
    if (lps < (real)1.0f || lps > (real)200.0f) lps = (real)33.0f;
    real iirAmount = lps / (real)430.0f;
    real fastTaper = ips / (real)15.0f;
    real slowTaper = (real)2.0f / (lps * lps);
    real lowspeedscale = ((real)5.0f / ips);
    real depth = R_POW(D, (real)2.0f) * (real)CHR_OVERALLSCALE;
    real fluttertrim = (real)0.00581f / (real)CHR_OVERALLSCALE;
    real sweeptrim = ((real)0.0005f * depth) / (real)CHR_OVERALLSCALE;
    const real tupi = (real)3.141592653589793238f * (real)2.0f;
    real newrate = (real)0.006f / (real)CHR_OVERALLSCALE;
    real oldrate = (real)1.0f - newrate;
    iirAmount /= (real)CHR_OVERALLSCALE;
    lowspeedscale *= (real)CHR_OVERALLSCALE;
    fastTaper = (real)1.0f + (fastTaper / (real)CHR_OVERALLSCALE);
    slowTaper = (real)1.0f + (slowTaper / (real)CHR_OVERALLSCALE);
    real ifast = (real)1.0f / fastTaper, islow = (real)1.0f / slowTaper;
    real noise = E * (real)0.5f;
    real sweep = s->sweep, rateof = s->rateof, nextmax = s->nextmax;
    int32_t gcount = s->gcount, fstoredcount = s->fstoredcount;
    uint32_t flip = s->flip, fpdL = s->fpdL, fpdR = s->fpdR;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        real *d = c ? s->dR : s->dL, *fl = c ? s->flR : s->flL;
        ironoxide5_ch st = s->ch[c];
        sweep = s->sweep; rateof = s->rateof; nextmax = s->nextmax;
        gcount = s->gcount; fstoredcount = s->fstoredcount;
        flip = s->flip; fpdL = s->fpdL; fpdR = s->fpdR;
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            real dry = x;
            real flutterrandy = (real)fpdL / CHR_U32_MAX_R;
            if (fstoredcount < 0 || fstoredcount > 30) fstoredcount = 30;
            int32_t flutcount = fstoredcount;
            fl[flutcount + 31] = fl[flutcount] = x;
            real offset = ((real)1.0f + R_SIN(sweep)) * depth;
            real fo = R_FLOOR(offset);
            real flooroffset = offset - fo;
            flutcount += (int32_t)fo;
            x = fl[flutcount] * ((real)1.0f - flooroffset) + fl[flutcount + 1] * flooroffset;
            rateof = (nextmax * newrate) + (rateof * oldrate);
            sweep += rateof * fluttertrim;
            sweep += sweep * sweeptrim;
            if (sweep >= tupi) { sweep = (real)0.0f; nextmax = (real)0.02f + (flutterrandy * (real)0.98f); }
            fstoredcount--;
            if (flip) { st.iirA = (st.iirA * ((real)1.0f - iirAmount)) + (x * iirAmount); x -= st.iirA; }
            else      { st.iirB = (st.iirB * ((real)1.0f - iirAmount)) + (x * iirAmount); x -= st.iirB; }
            x = iron_sat(x * inputgain);
            if (gcount < 0 || gcount > 131) gcount = 131;
            int32_t count = gcount;
            d[count + 131] = d[count] = x;
            if (flip) {
                st.fastA = st.fastA * ifast; st.slowA = st.slowA * islow; st.fastA += x;
                st.slowA += iron_taps(d, count + 3) / (real)128.0f;
                x = st.fastA - (st.slowA * islow);
            } else {
                st.fastB = st.fastB * ifast; st.slowB = st.slowB * islow; st.fastB += x;
                st.slowB += iron_taps(d, count + 3) / (real)128.0f;
                x = st.fastB - (st.slowB * islow);
            }
            x *= ifast;
            x /= lowspeedscale;
            x = iron_sat(x);
            real randy = ((real)0.55f + tempRandy + (((real)fpdR / CHR_U32_MAX_R) * tempRandy)) * noise;
            x *= ((real)1.0f - randy); x += (st.prev * randy); st.prev = dry;
            flip = !flip;
            if (outputgain != (real)1.0f) x *= outputgain;
            /* invdrywet == 1: no scaling; dry == 1 - invdrywet == 0: nothing added */
            gcount--;
            fpdL = chr_xorshift(fpdL); fpdR = chr_xorshift(fpdR);
            buf[i] = (float)x;
        }
        s->ch[c] = st;
    }
    s->sweep = sweep; s->rateof = rateof; s->nextmax = nextmax;
    s->gcount = gcount; s->fstoredcount = fstoredcount;
    s->flip = flip; s->fpdL = fpdL; s->fpdR = fpdR;
}

/* ---- Colour 3: ToTape8 ---------------------------------------------------
 * Amount -> Input A = 0.5 + 0.5*amt (0 .. +12 dB into the tape) with Output
 * I = 0.5/sqrt(inputGain): half the drive taken back, in dB. Tone -> Tilt B
 * (the Dubly encode/decode balance; centre is the plugin default). Shape C,
 * Flutter D, FlutSpd E, HeadBmp G, HeadFrq H all at the plugin defaults
 * (0.5). Bias F = 0.5 is the plugin default AND switches the bias routine
 * off (|bias| < 0.001), so it is not ported. 48 kHz gives spacing 1, so the
 * ClipOnly2 latency buffer is a single sample. The two flutter sweeps are
 * cross-coupled (each picks its next speed by looking at the other) but do
 * not depend on the audio, so each channel's pass replays both from the same
 * starting values and reads only its own. */
enum { hdb_a0, hdb_a1, hdb_a2, hdb_b1, hdb_b2 };
typedef struct {
    real iirEnc, compEnc, avgEnc, iirDec, compDec, avgDec, iirMid, iirLow, headBump;
    real hAs1, hAs2, hBs1, hBs2, last;
    uint32_t wasPos, wasNeg;
} totape8_ch;
typedef struct { real sweepL, sweepR, nextmaxL, nextmaxR; uint32_t fpdL, fpdR; } totape8_flut;
typedef struct {
    real dL[1002], dR[1002];
    totape8_ch ch[2];
    totape8_flut fl;
    int32_t gcount;
} totape8_t;

static void m_totape8_init(totape8_t *s)
{
    for (int c = 0; c < 2; c++) s->ch[c].compEnc = s->ch[c].compDec = (real)1.0f;
    s->fl.sweepL = s->fl.sweepR = (real)3.14159265358979f;
    s->fl.nextmaxL = s->fl.nextmaxR = (real)0.5f;
    s->fl.fpdL = 0x2545f491u; s->fl.fpdR = 0x6c8e9cf5u;
}

/* One sample of both flutter sweeps, in the original's order; returns the
 * delay offset for channel `right` only (the other's sine is not needed). */
static inline real tape_flutter(totape8_flut *f, int right, real depth, real freq)
{
    const real TWO_PI = (real)3.14159265358979f * (real)2.0f;
    real off = (real)0.0f;
    if (right == 0) off = depth + (depth * R_SIN(f->sweepL));   /* right < 0: no offset */
    f->sweepL += f->nextmaxL * freq;
    if (f->sweepL > TWO_PI) {
        f->sweepL -= TWO_PI;
        real flutA = (real)0.24f + ((real)f->fpdL / CHR_U32_MAX_R * (real)0.74f);
        f->fpdL = chr_xorshift(f->fpdL);
        real flutB = (real)0.24f + ((real)f->fpdL / CHR_U32_MAX_R * (real)0.74f);
        real sr = R_SIN(f->sweepR + f->nextmaxR);
        if (R_FABS(flutA - sr) < R_FABS(flutB - sr)) f->nextmaxL = flutA; else f->nextmaxL = flutB;
    }
    if (right > 0) off = depth + (depth * R_SIN(f->sweepR));
    f->sweepR += f->nextmaxR * freq;
    if (f->sweepR > TWO_PI) {
        f->sweepR -= TWO_PI;
        real flutA = (real)0.24f + ((real)f->fpdR / CHR_U32_MAX_R * (real)0.74f);
        f->fpdR = chr_xorshift(f->fpdR);
        real flutB = (real)0.24f + ((real)f->fpdR / CHR_U32_MAX_R * (real)0.74f);
        real sl = R_SIN(f->sweepL + f->nextmaxL);
        if (R_FABS(flutA - sl) < R_FABS(flutB - sl)) f->nextmaxR = flutA; else f->nextmaxR = flutB;
    }
    return off;
}

static inline real tape_clip(real x, totape8_ch *st)
{
    if (x > (real)4.0f) x = (real)4.0f;
    if (x < (real)-4.0f) x = (real)-4.0f;
    if (st->wasPos) {
        if (x < st->last) st->last = (real)0.7058208f + (x * (real)0.2609148f);
        else st->last = (real)0.2491717f + (st->last * (real)0.7390851f);
    }
    st->wasPos = 0;
    if (x > (real)0.9549925859f) { st->wasPos = 1; x = (real)0.7058208f + (st->last * (real)0.2609148f); }
    if (st->wasNeg) {
        if (x > st->last) st->last = (real)-0.7058208f + (x * (real)0.2609148f);
        else st->last = (real)-0.2491717f + (st->last * (real)0.7390851f);
    }
    st->wasNeg = 0;
    if (x < (real)-0.9549925859f) { st->wasNeg = 1; x = (real)-0.7058208f + (st->last * (real)0.2609148f); }
    real out = st->last;                 /* spacing 1: one sample of latency */
    st->last = x;
    return out;
}

static inline real tape_dubly(real x, real *iir, real *avg, real *comp, real freq,
                              real k1, real k2, real amount)
{
    *iir = (*iir * ((real)1.0f - freq)) + (x * freq);
    real highPart = ((x - *iir) * k1);
    highPart += *avg; *avg = (x - *iir) * k2;
    if (highPart > (real)1.0f) highPart = (real)1.0f;
    if (highPart < (real)-1.0f) highPart = (real)-1.0f;
    real dubly = R_FABS(highPart);
    if (dubly > (real)0.0f) {
        real adjust = R_LOG_FAST((real)1.0f + ((real)255.0f * dubly)) / (real)2.40823996531f;
        if (adjust > (real)0.0f) dubly /= adjust;
        *comp = (*comp * ((real)1.0f - freq)) + (dubly * freq);
        x += ((highPart * *comp) * amount);
    }
    return x;
}

/* toTape basic algorithm + HeadBump, one channel */
static inline real tape_body(real x, totape8_ch *st, real midFreq, real subFreq,
                             real bumpDrive, real bumpMix, real bumpCubic,
                             const real *hA, const real *hB)
{
    st->iirMid = (st->iirMid * ((real)1.0f - midFreq)) + (x * midFreq);
    real highs = x - st->iirMid;
    real lows = st->iirMid;
    if (subFreq > (real)0.0f) { st->iirLow = (st->iirLow * ((real)1.0f - subFreq)) + (lows * subFreq); lows -= st->iirLow; }
    if (lows > (real)1.57079633f) lows = (real)1.57079633f;
    if (lows < (real)-1.57079633f) lows = (real)-1.57079633f;
    lows = R_SIN_HALF(lows);
    real thin = R_FABS(highs) * (real)1.57079633f;
    if (thin > (real)1.57079633f) thin = (real)1.57079633f;
    thin = (real)1.0f - R_COS_HALF(thin);
    if (highs < (real)0.0f) thin = -thin;
    highs -= thin;
    real hb = (real)0.0f;
    if (bumpMix > (real)0.0f) {
        st->headBump += (lows * bumpDrive);
        st->headBump -= (st->headBump * st->headBump * st->headBump * bumpCubic);
        real biq = (st->headBump * hA[hdb_a0]) + st->hAs1;
        st->hAs1 = (st->headBump * hA[hdb_a1]) - (biq * hA[hdb_b1]) + st->hAs2;
        st->hAs2 = (st->headBump * hA[hdb_a2]) - (biq * hA[hdb_b2]);
        hb = (biq * hB[hdb_a0]) + st->hBs1;
        st->hBs1 = (biq * hB[hdb_a1]) - (hb * hB[hdb_b1]) + st->hBs2;
        st->hBs2 = (biq * hB[hdb_a2]) - (hb * hB[hdb_b2]);
    }
    return lows + highs + (hb * bumpMix);
}

static void m_totape8(totape8_t *s, real amt, real tone, float *L, float *R, int n)
{
    const real PI = (real)3.14159265358979f;
    real A = (real)0.5f + (real)0.5f * amt, B = tone, C = (real)0.5f, D = (real)0.5f;
    real E = (real)0.5f, G = (real)0.5f, H = (real)0.5f;
    real inputGain = R_POW(A * (real)2.0f, (real)2.0f);
    real dublyAmount = B * (real)2.0f;
    real outlyAmount = ((real)1.0f - B) * (real)-2.0f;
    if (outlyAmount < (real)-1.0f) outlyAmount = (real)-1.0f;
    real iirEncFreq = ((real)1.0f - C) / (real)CHR_OVERALLSCALE;
    real iirDecFreq = C / (real)CHR_OVERALLSCALE;
    real iirMidFreq = ((C * (real)0.618f) + (real)0.382f) / (real)CHR_OVERALLSCALE;
    real flutDepth = R_POW(D, (real)6.0f) * (real)CHR_OVERALLSCALE * (real)50.0f;
    if (flutDepth > (real)498.0f) flutDepth = (real)498.0f;
    real flutFrequency = ((real)0.02f * R_POW(E, (real)3.0f)) / (real)CHR_OVERALLSCALE;
    real headBumpDrive = (G * (real)0.1f) / (real)CHR_OVERALLSCALE;
    real headBumpMix = G * (real)0.5f;
    real subCurve = R_SIN(G * PI);
    real iirSubFreq = (subCurve * (real)0.008f) / (real)CHR_OVERALLSCALE;
    real hfreq = (((H * H) * (real)175.0f) + (real)25.0f) / (real)CHR_SR;
    real reso = (real)0.618033988749894848204586f;
    real hA[5], hB[5];
    real K = R_TAN(PI * hfreq);
    real norm = (real)1.0f / ((real)1.0f + K / reso + K * K);
    hA[hdb_a0] = K / reso * norm; hA[hdb_a1] = (real)0.0f; hA[hdb_a2] = -hA[hdb_a0];
    hA[hdb_b1] = (real)2.0f * (K * K - (real)1.0f) * norm; hA[hdb_b2] = ((real)1.0f - K / reso + K * K) * norm;
    K = R_TAN(PI * hfreq * (real)0.9375f);
    norm = (real)1.0f / ((real)1.0f + K / reso + K * K);
    hB[hdb_a0] = K / reso * norm; hB[hdb_a1] = (real)0.0f; hB[hdb_a2] = -hB[hdb_a0];
    hB[hdb_b1] = (real)2.0f * (K * K - (real)1.0f) * norm; hB[hdb_b2] = ((real)1.0f - K / reso + K * K) * norm;
    real outputGain = (real)1.0f / R_SQRT(inputGain);       /* I*2 with I = 0.5/sqrt(gain) */
    real bumpCubic = (real)0.0618f / R_SQRT((real)CHR_OVERALLSCALE);
    totape8_flut fl = s->fl;
    int32_t gcount = s->gcount;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        real *d = c ? s->dR : s->dL;
        totape8_ch st = s->ch[c];
        fl = s->fl;
        gcount = s->gcount;
#ifndef CHR_REF
        /* Firmware build only: between wraps a sweep advances by a constant
         * step, so its sine is carried by a rotation (4 multiplies) instead of
         * a sine per sample, and re-anchored exactly at every call and every
         * wrap. Measured against the reference: inside ToTape8's tolerance. */
        real rs = R_SIN(c ? fl.sweepR : fl.sweepL), rc = R_COS(c ? fl.sweepR : fl.sweepL);
        real rinc = (c ? fl.nextmaxR : fl.nextmaxL) * flutFrequency;
        real ris = R_SIN(rinc), ric = R_COS(rinc);
#endif
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            if (inputGain != (real)1.0f) x *= inputGain;
            x = tape_dubly(x, &st.iirEnc, &st.avgEnc, &st.compEnc, iirEncFreq, (real)2.848f, (real)1.152f, dublyAmount);
            if (flutDepth > (real)0.0f) {
                if (gcount < 0 || gcount > 999) gcount = 999;
                d[gcount] = x;
                int32_t count = gcount;
#ifdef CHR_REF
                real offset = tape_flutter(&fl, c, flutDepth, flutFrequency);
#else
                real offset = flutDepth + (flutDepth * rs);
                real before = c ? fl.nextmaxR : fl.nextmaxL;
                tape_flutter(&fl, -1, flutDepth, flutFrequency);
                real ns = rs * ric + rc * ris;
                rc = rc * ric - rs * ris; rs = ns;
                real after = c ? fl.nextmaxR : fl.nextmaxL;
                if (after != before) {               /* own sweep wrapped: new speed */
                    real sw = c ? fl.sweepR : fl.sweepL;
                    rs = R_SIN(sw); rc = R_COS(sw);
                    rinc = after * flutFrequency; ris = R_SIN(rinc); ric = R_COS(rinc);
                }
#endif
                real fo = R_FLOOR(offset);
                count += (int32_t)fo;
                x = (d[count - ((count > 999) ? 1000 : 0)] * ((real)1.0f - (offset - fo)));
                x += (d[count + 1 - ((count + 1 > 999) ? 1000 : 0)] * (offset - fo));
                gcount--;
            }
            /* bias F = 0.5: bias == 0, the bias routine does not run */
            x = tape_body(x, &st, iirMidFreq, iirSubFreq, headBumpDrive, headBumpMix, bumpCubic, hA, hB);
            x = tape_dubly(x, &st.iirDec, &st.avgDec, &st.compDec, iirDecFreq, (real)2.628f, (real)1.372f, outlyAmount);
            if (outputGain != (real)1.0f) x *= outputGain;
            x = tape_clip(x, &st);
            buf[i] = (float)x;
        }
        s->ch[c] = st;
    }
    if (!R) {                            /* mono: R's flutter still has to move */
        /* tape_flutter advanced both sweeps in the L pass already */
    }
    s->fl = fl;
    s->gcount = gcount;
}

/* ---- Colour 4: Coils2 ----------------------------------------------------
 * Its real controls: Amount -> Saturate A = amt, Tone -> Cheapness B = tone,
 * Dry/Wet C = 1 (fixed, as asked). The plugin's defaults are A = B = 0. A
 * transformer model: the mids around 600 Hz are notched out, the remainder is
 * driven through a sine with hysteresis and added back at distScaling =
 * (1 - A)^2. Note what that does at the top: as A -> 1 the added distortion
 * is clipped at a smaller and smaller level, so the very top of Amount is
 * subtler again, not harder (the plugin's own behaviour). */
typedef struct { real a7, a8, b7, b8, hyst; } coils2_ch;
typedef struct { coils2_ch ch[2]; } coils2_t;

static void m_coils2(coils2_t *s, real amt, real tone, float *L, float *R, int n)
{
    real A = amt, B = tone;
    real distScaling = R_POW((real)1.0f - A, (real)2.0f);
    if (distScaling < (real)0.0001f) distScaling = (real)0.0001f;
    real idist = (real)1.0f / distScaling;
    real fa = (real)600.0f / (real)CHR_SR;
    real qa = (real)0.01f + (R_POW(B, (real)2.0f) * (real)0.5f);
    real iirAmount = qa / (real)CHR_OVERALLSCALE;
    real K = R_TAN((real)3.14159265358979f * fa);
    real norm = (real)1.0f / ((real)1.0f + K / qa + K * K);
    real a2 = K / qa * norm, a4 = -a2;
    real a5 = (real)2.0f * (K * K - (real)1.0f) * norm, a6 = ((real)1.0f - K / qa + K * K) * norm;
    real fb = ((real)21890.0f - (qa * (real)890.0f)) / (real)CHR_SR;
    real qb = (real)0.89f;
    K = R_TAN((real)3.14159265358979f * fb);
    norm = (real)1.0f / ((real)1.0f + K / qb + K * K);
    real b2 = K * K * norm, b3 = (real)2.0f * b2, b4 = b2;
    real b5 = (real)2.0f * (K * K - (real)1.0f) * norm, b6 = ((real)1.0f - K / qb + K * K) * norm;
    int fA = fa < (real)0.49999f, fB = fb < (real)0.49999f;
    for (int c = 0; c < CHR_NCH(R); c++) {
        float *buf = c ? R : L;
        coils2_ch st = s->ch[c];
        for (int i = 0; i < n; i++) {
            real x = (real)buf[i];
            real dry = x;
            if (fA) {
                real t = (x * a2) + st.a7;
                st.a7 = -(t * a5) + st.a8;
                st.a8 = (x * a4) - (t * a6);
                x = t;
            }
            real diff = (dry - x) * idist;
            if (fB) {
                real t = (diff * b2) + st.b7;
                st.b7 = (diff * b3) - (t * b5) + st.b8;
                st.b8 = (diff * b4) - (t * b6);
                diff = t;
            }
            st.hyst = (st.hyst * ((real)1.0f - iirAmount)) + (diff * iirAmount);
            if (R_FABS(st.hyst) < (real)1.18e-37f) st.hyst = (real)0.0f; else diff -= st.hyst;
            if (diff > (real)1.571f) diff = (real)1.571f; else if (diff < (real)-1.571f) diff = (real)-1.571f;
            if (st.hyst > (real)1.571f) st.hyst = (real)1.571f; else if (st.hyst < (real)-1.571f) st.hyst = (real)-1.571f;
            x += (R_SIN_HALF(diff) - R_SIN_HALF(st.hyst)) * distScaling;
            buf[i] = (float)x;                                      /* wet C = 1 */
        }
        s->ch[c] = st;
    }
}

#define CHR_STATES_COLOUR channel9_t a; ironoxide5_t b; totape8_t c; coils2_t d;
#define CHR_PRESENT_COLOUR 0x0f
#define CHR_INIT_COLOUR(type, st) do { switch (type) { \
    case 1: m_ironoxide5_init((ironoxide5_t *)(st)); break; \
    case 2: m_totape8_init((totape8_t *)(st)); break; \
    } } while (0)
#define CHR_RUN_COLOUR(type, st, amt, tone, L, R, n) do { switch (type) { \
    case 0: m_channel9((channel9_t *)(st), amt, tone, L, R, n); return 1; \
    case 1: m_ironoxide5((ironoxide5_t *)(st), amt, tone, L, R, n); return 1; \
    case 2: m_totape8((totape8_t *)(st), amt, tone, L, R, n); return 1; \
    case 3: m_coils2((coils2_t *)(st), amt, tone, L, R, n); return 1; \
    } } while (0)

#endif
