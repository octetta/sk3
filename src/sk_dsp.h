/*
 * sk_dsp.h - Header-Only Modern Audio DSP & Physical Modeling Synthesis Library
 * Version 1.5.0
 *
 * Repository: https://github.com/octetta/sk3
 * Path: src/sk_dsp.h
 *
 * Copyright (c) 2026 Joseph Ellis Stewart / Octetta
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef SK_DSP_H
#define SK_DSP_H

#define SK_DSP_VERSION_MAJOR 1
#define SK_DSP_VERSION_MINOR 5
#define SK_DSP_VERSION_PATCH 0
#define SK_DSP_VERSION_STRING "1.5.0"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "sk_meta.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* =========================================================================
 * 1. Numerical Safety & Clamping
 * ========================================================================= */

static inline float sk_clampf(float val, float min_val, float max_val) {
    if (isnan(val)) return min_val;
    return fmaxf(min_val, fminf(val, max_val));
}

static inline double sk_clampd(double val, double min_val, double max_val) {
    if (isnan(val)) return min_val;
    return fmax(min_val, fmin(val, max_val));
}

static inline int sk_clampi(int val, int min_val, int max_val) {
    return val < min_val ? min_val : (val > max_val ? max_val : val);
}

static inline double sk_safe_val(double v) {
    if (isnan(v) || isinf(v)) return 0.0;
    if (v > 1e6) return 1e6;
    if (v < -1e6) return -1e6;
    return v;
}

static inline float sk_denormal_flush(float v) {
    return (fabsf(v) < 1e-15f) ? 0.0f : v;
}

/* =========================================================================
 * 2. Pitch, Decibel, Panning & Fast Transcendental Math
 * ========================================================================= */

/* Fast power approximation using IEEE 754 bit manipulation (~10x faster than powf) */
static inline float sk_fast_pow(float a, float b) {
    if (a <= 0.0f) return 0.0f;
    union { float f; int32_t i; } u;
    u.f = a;
    u.i = (int32_t)(b * (float)(u.i - 1065353216) + 1065353216.0f);
    return u.f;
}

static inline float sk_mtof(float midi_note) {
    return 440.0f * powf(2.0f, (midi_note - 69.0f) / 12.0f);
}

static inline float sk_ftom(float freq_hz) {
    if (freq_hz <= 0.0f) return 0.0f;
    return 69.0f + 12.0f * log2f(freq_hz / 440.0f);
}

/* Decibel <-> Linear amplitude conversions */
static inline float sk_db_to_linear(float db) {
    return powf(10.0f, db * 0.05f);
}

static inline float sk_linear_to_db(float linear) {
    return linear > 0.00001f ? 20.0f * log10f(linear) : -100.0f;
}

/* Constant-power stereo panning law (equal acoustic power: L^2 + R^2 = 1.0) */
static inline void sk_pan_equal_power(float pan, float *left, float *right) {
    pan = sk_clampf(pan, -1.0f, 1.0f);
    float angle = (pan + 1.0f) * (0.25f * (float)M_PI);
    if (left) *left = cosf(angle);
    if (right) *right = sinf(angle);
}

/* Linear stereo panning law */
static inline void sk_pan_linear(float pan, float *left, float *right) {
    pan = sk_clampf(pan, -1.0f, 1.0f);
    if (left) *left = (1.0f - pan) * 0.5f;
    if (right) *right = (1.0f + pan) * 0.5f;
}

static inline double sk_hz_to_phase_inc(double hz, double sr) {
    return (sr > 0.0) ? (2.0 * M_PI * hz / sr) : 0.0;
}

static inline double sk_ms_to_samples(double ms, double sr) {
    return ms * 0.001 * sr;
}

static inline double sk_samples_to_ms(double samples, double sr) {
    return (sr > 0.0) ? (samples * 1000.0 / sr) : 0.0;
}

/* =========================================================================
 * 3. PRNG & Audio Noise Generators (808/606 & Retro Hardware LFSRs)
 * ========================================================================= */

/* 64-bit Knuth MMIX Linear Congruential Generator */
typedef struct {
    uint64_t state;
} sk_rng_t;

static inline void sk_rng_init(sk_rng_t *rng, uint64_t seed) {
    rng->state = seed ? seed : 1ULL;
}

static inline uint64_t sk_rng_next(sk_rng_t *rng) {
    rng->state = rng->state * 6364136223846793005ULL + 1442695040888963407ULL;
    return rng->state;
}

static inline float sk_rng_float(sk_rng_t *rng) {
    uint32_t val = (uint32_t)(sk_rng_next(rng) >> 32);
    return (float)((int32_t)val) / 2147483648.0f;
}

/* 808/606 1-bit XOR clock shift register noise */
static inline float sk_metallic_noise_step(uint32_t clock) {
    uint32_t hh = (clock * 13) ^ (clock >> 5) ^ (clock * 193);
    return (hh & 128) ? 0.7f : -0.7f;
}

/* 6-oscillator metallic cluster buzz (ratios: 2.43, 3.01, 3.52, 4.11, 5.23, 6.78) */
static inline double sk_metallic_cluster_sample(double phase_inc, int sample_idx) {
    static const double ff[6] = {2.43, 3.01, 3.52, 4.11, 5.23, 6.78};
    double ss = 0.0;
    for (int j = 0; j < 6; j++) {
        ss += (sin((double)sample_idx * phase_inc * ff[j]) > 0.0) ? 1.0 : -1.0;
    }
    return ss / 6.0;
}

/* Maximum feedback taps for 2..31 bit LFSR polynomials */
static const uint8_t sk_max_lfsr_taps[32] = {
    0,  0,  1,  1,  1,  2,  1,  1,  2,  4,
    3,  2,  1,  1,  1,  1,  4,  3,  7,  1,
    3,  2,  1,  5,  1,  3,  1,  2,  3,  2,
    1,  3
};

/* Vintage Hardware LFSR Noise State (SID, POKEY, NES, Custom) */
typedef struct {
    uint32_t lfsr;
    int type;     /* 0=SID, 1..4=POKEY (4/5/9/17-bit), 5=NES 15-bit, 6=NES 7-bit, 7=Custom */
    int bits;     /* 2..31 */
    int tap;      /* feedback tap */
    int dac;      /* 0=1-bit (-1.0f or +1.0f), 1=8-bit DAC */
} sk_lfsr_noise_t;

static inline void sk_lfsr_noise_init(sk_lfsr_noise_t *n, int type) {
    n->type = sk_clampi(type, 0, 7);
    n->lfsr = (n->type == 0) ? 0x7FFFFF : 1;
    n->dac = 0;
    switch (n->type) {
        case 0: n->bits = 23; n->tap = 17; n->dac = 1; break;
        case 1: n->bits = 4;  n->tap = 2; break;
        case 2: n->bits = 5;  n->tap = 2; break;
        case 3: n->bits = 9;  n->tap = 4; break;
        case 4: n->bits = 17; n->tap = 4; break;
        case 5: n->bits = 15; n->tap = 1; break;
        case 6: n->bits = 15; n->tap = 6; break;
        case 7: n->bits = 16; n->tap = 4; break;
    }
}

static inline void sk_lfsr_noise_set_custom(sk_lfsr_noise_t *n, int bits, int tap, int dac) {
    if (bits < 2) bits = 2;
    if (bits > 31) bits = 31;
    if (tap < 1 || tap >= bits) tap = sk_max_lfsr_taps[bits];
    n->type = 7;
    n->bits = bits;
    n->tap = tap;
    n->dac = (dac != 0) ? 1 : 0;
    if (n->lfsr == 0) n->lfsr = 1;
}

static inline float sk_lfsr_noise_step(sk_lfsr_noise_t *n) {
    uint32_t lfsr = n->lfsr;
    int mode = n->type;
    if (lfsr == 0) {
        lfsr = (mode == 0) ? 0x7FFFFF : 1;
    }
    float out = 0.0f;
    switch (mode) {
        case 0: { /* MOS 6581/8580 SID 23-bit */
            uint32_t feedback = ((lfsr >> 22) ^ (lfsr >> 17)) & 1;
            lfsr = ((lfsr << 1) | feedback) & 0x7FFFFF;
            uint8_t dac = (uint8_t)(
                (((lfsr >> 22) & 1) << 7) |
                (((lfsr >> 20) & 1) << 6) |
                (((lfsr >> 16) & 1) << 5) |
                (((lfsr >> 13) & 1) << 4) |
                (((lfsr >> 11) & 1) << 3) |
                (((lfsr >>  7) & 1) << 2) |
                (((lfsr >>  4) & 1) << 1) |
                (((lfsr >>  2) & 1) << 0)
            );
            out = (float)dac * 0.00784313725f - 1.0f;
            break;
        }
        case 1: { /* POKEY 4-bit (15-step) */
            uint32_t feedback = ((lfsr >> 3) ^ (lfsr >> 2)) & 1;
            lfsr = ((lfsr << 1) | feedback) & 0x0F;
            out = (lfsr & 1) ? 1.0f : -1.0f;
            break;
        }
        case 2: { /* POKEY 5-bit (31-step) */
            uint32_t feedback = ((lfsr >> 4) ^ (lfsr >> 2)) & 1;
            lfsr = ((lfsr << 1) | feedback) & 0x1F;
            out = (lfsr & 1) ? 1.0f : -1.0f;
            break;
        }
        case 3: { /* POKEY 9-bit (511-step) */
            uint32_t feedback = ((lfsr >> 8) ^ (lfsr >> 4)) & 1;
            lfsr = ((lfsr << 1) | feedback) & 0x1FF;
            out = (lfsr & 1) ? 1.0f : -1.0f;
            break;
        }
        case 4: { /* POKEY 17-bit (131071-step) */
            uint32_t feedback = ((lfsr >> 16) ^ (lfsr >> 4)) & 1;
            lfsr = ((lfsr << 1) | feedback) & 0x1FFFF;
            out = (lfsr & 1) ? 1.0f : -1.0f;
            break;
        }
        case 5: { /* NES 15-bit long */
            uint32_t feedback = (lfsr ^ (lfsr >> 1)) & 1;
            lfsr = ((lfsr >> 1) | (feedback << 14)) & 0x7FFF;
            out = (lfsr & 1) ? 1.0f : -1.0f;
            break;
        }
        case 6: { /* NES 7-bit metallic loop */
            uint32_t feedback = (lfsr ^ (lfsr >> 6)) & 1;
            lfsr = ((lfsr >> 1) | (feedback << 14)) & 0x7FFF;
            out = (lfsr & 1) ? 1.0f : -1.0f;
            break;
        }
        case 7:
        default: {
            int bits = sk_clampi(n->bits, 2, 31);
            int tap = n->tap;
            if (tap < 1 || tap >= bits) tap = sk_max_lfsr_taps[bits];
            uint32_t mask = (bits >= 31) ? 0x7FFFFFFF : ((1U << bits) - 1);
            uint32_t feedback = ((lfsr >> (bits - 1)) ^ (lfsr >> tap)) & 1;
            lfsr = ((lfsr << 1) | feedback) & mask;
            if (n->dac == 1) {
                uint8_t dac = (uint8_t)(lfsr & 0xFF);
                out = (float)dac * 0.00784313725f - 1.0f;
            } else {
                out = (lfsr & 1) ? 1.0f : -1.0f;
            }
            break;
        }
    }
    n->lfsr = lfsr;
    return out;
}

/* =========================================================================
 * 4. Complex Phasor Rotator & Modal Resonator Bank
 * ========================================================================= */

/* Damped complex phasor rotator for modal synthesis (damped sinusoid) */
typedef struct {
    double re, im;     /* Current complex phasor state */
    double c, sn;      /* cos(w), sin(w) step increments */
    double decay;      /* exponential decay per sample (1.0 = sustain, <1.0 = decay) */
    double amp;        /* mode amplitude */
} sk_rotator_t;

static inline void sk_rotator_init(sk_rotator_t *r, double freq_hz, double amp, double t60_sec, double sr) {
    r->re = 1.0;
    r->im = 0.0;
    r->amp = amp;
    if (freq_hz <= 0.0 || freq_hz >= 0.5 * sr || sr <= 0.0) {
        r->c = 1.0; r->sn = 0.0; r->decay = 0.0; r->amp = 0.0;
        return;
    }
    double w = 2.0 * M_PI * freq_hz / sr;
    r->c = cos(w);
    r->sn = sin(w);
    r->decay = (t60_sec > 0.0) ? pow(10.0, -3.0 / (t60_sec * sr)) : 1.0;
}

static inline double sk_rotator_step(sk_rotator_t *r) {
    double out = r->amp * r->im;
    double nre = r->decay * (r->re * r->c - r->im * r->sn);
    double nim = r->decay * (r->re * r->sn + r->im * r->c);
    r->re = nre;
    r->im = nim;
    return out;
}

/* Modal bank renderer: renders sum of N damped sinusoids into an output buffer */
static inline void sk_modal_bank_render(const double *modes, int num_modes, double *out, int num_samples, double sr) {
    if (!modes || num_modes <= 0 || !out || num_samples <= 0) return;
    for (int i = 0; i < num_samples; i++) out[i] = 0.0;

    for (int j = 0; j < num_modes; j++) {
        double f = modes[3 * j];
        double amp = modes[3 * j + 1];
        double t60 = modes[3 * j + 2];
        if (f <= 0.0 || f >= 0.5 * sr) continue;

        sk_rotator_t rot;
        sk_rotator_init(&rot, f, amp, t60, sr);
        for (int i = 0; i < num_samples; i++) {
            out[i] += sk_rotator_step(&rot);
        }
    }
}

/* =========================================================================
 * 5. Filters: Chamberlin SVF, Biquad DF-II, 2-Pole Resonant, 1-Pole RC, Delay Filters
 * ========================================================================= */

/* Chamberlin Digital State-Variable Filter (SVF) */
typedef struct {
    double s0, s1;         /* Lowpass, Bandpass states */
    double f_coeff, damp;  /* Tuning coefficients */
} sk_svf_t;

static inline void sk_svf_init(sk_svf_t *f, double freq_hz, double q, double sr) {
    f->s0 = 0.0;
    f->s1 = 0.0;
    double q_val = (q < 0.01) ? 0.01 : q;
    f->damp = 1.0 / q_val;
    f->f_coeff = 2.0 * sin(M_PI * freq_hz / sr);
    if (f->f_coeff > 1.99) f->f_coeff = 1.99;
}

static inline void sk_svf_step(sk_svf_t *f, double in, double *lp, double *bp, double *hp) {
    double high = in - f->s0 - f->damp * f->s1;
    f->s1 += f->f_coeff * high;
    f->s0 += f->f_coeff * f->s1;
    f->s0 = sk_safe_val(f->s0);
    f->s1 = sk_safe_val(f->s1);
    if (hp) *hp = high;
    if (bp) *bp = f->s1;
    if (lp) *lp = f->s0;
}

static inline double sk_svf_step_bp(sk_svf_t *f, double in) {
    double bp;
    sk_svf_step(f, in, NULL, &bp, NULL);
    return bp;
}

static inline double sk_svf_step_lp(sk_svf_t *f, double in) {
    double lp;
    sk_svf_step(f, in, &lp, NULL, NULL);
    return lp;
}

/* Robert Bristow-Johnson (RBJ) Audio EQ Cookbook Direct Form II Biquad */
typedef enum {
    SK_BIQUAD_LOWPASS,
    SK_BIQUAD_HIGHPASS,
    SK_BIQUAD_BANDPASS,
    SK_BIQUAD_NOTCH,
    SK_BIQUAD_ALLPASS,
    SK_BIQUAD_LP_BP
} sk_biquad_type_t;

typedef struct {
    float b0, b1, b2, a1, a2;
    float x1, x2, y1, y2;
    float drive;
} sk_biquad_t;

static inline void sk_biquad_set(sk_biquad_t *b, sk_biquad_type_t type, float f, float q, float sr) {
    if (!b) return;
    if (sr <= 0.0f || f <= 0.0f) {
        b->b0 = 1.0f; b->b1 = b->b2 = b->a1 = b->a2 = 0.0f;
        return;
    }
    float omega = 2.0f * (float)M_PI * f / sr;
    float sin_omega = sinf(omega);
    float cos_omega = cosf(omega);
    float q_val = q < 0.01f ? 0.01f : q;
    float alpha = sin_omega / (2.0f * q_val);

    float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f, b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;

    switch (type) {
        case SK_BIQUAD_LOWPASS:
            b0 = (1.0f - cos_omega) * 0.5f;
            b1 = 1.0f - cos_omega;
            b2 = (1.0f - cos_omega) * 0.5f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_omega;
            a2 = 1.0f - alpha;
            break;
        case SK_BIQUAD_HIGHPASS:
            b0 = (1.0f + cos_omega) * 0.5f;
            b1 = -(1.0f + cos_omega);
            b2 = (1.0f + cos_omega) * 0.5f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_omega;
            a2 = 1.0f - alpha;
            break;
        case SK_BIQUAD_BANDPASS:
            b0 = alpha;
            b1 = 0.0f;
            b2 = -alpha;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_omega;
            a2 = 1.0f - alpha;
            break;
        case SK_BIQUAD_NOTCH:
            b0 = 1.0f;
            b1 = -2.0f * cos_omega;
            b2 = 1.0f;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_omega;
            a2 = 1.0f - alpha;
            break;
        case SK_BIQUAD_ALLPASS:
            b0 = 1.0f - alpha;
            b1 = -2.0f * cos_omega;
            b2 = 1.0f + alpha;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_omega;
            a2 = 1.0f - alpha;
            break;
        case SK_BIQUAD_LP_BP:
            b0 = ((1.0f - cos_omega) * 0.5f) + alpha;
            b1 = 1.0f - cos_omega;
            b2 = ((1.0f - cos_omega) * 0.5f) - alpha;
            a0 = 1.0f + alpha;
            a1 = -2.0f * cos_omega;
            a2 = 1.0f - alpha;
            break;
    }

    float inv_a0 = 1.0f / a0;
    b->b0 = b0 * inv_a0;
    b->b1 = b1 * inv_a0;
    b->b2 = b2 * inv_a0;
    b->a1 = a1 * inv_a0;
    b->a2 = a2 * inv_a0;
}

static inline void sk_biquad_init(sk_biquad_t *b, sk_biquad_type_t type, float f, float q, float sr) {
    if (!b) return;
    b->x1 = b->x2 = b->y1 = b->y2 = 0.0f;
    b->drive = 0.0f;
    sk_biquad_set(b, type, f, q, sr);
}

static inline float sk_biquad_step(sk_biquad_t *b, float in) {
    float out = b->b0 * in + b->b1 * b->x1 + b->b2 * b->x2 - b->a1 * b->y1 - b->a2 * b->y2;
    b->x2 = b->x1;
    b->x1 = in;
    b->y2 = b->y1;
    b->y1 = out;
    return out;
}

/* 2-Pole Cascaded 1-Pole Resonant Lowpass Filter (vintage Moog style) */
typedef struct {
    double b0, b1;
    double ct, rs;
} sk_resonant2p_t;

static inline void sk_resonant2p_init(sk_resonant2p_t *f, double cutoff_norm, double resonance) {
    f->b0 = 0.0;
    f->b1 = 0.0;
    f->ct = (cutoff_norm > 0.95) ? 0.95 : (cutoff_norm < 0.0 ? 0.0 : cutoff_norm);
    f->rs = (resonance > 3.98) ? 3.98 : (resonance < 0.0 ? 0.0 : resonance);
}

static inline double sk_resonant2p_step(sk_resonant2p_t *f, double in) {
    double inp = in - (f->rs * f->b1);
    f->b0 += f->ct * (inp - f->b0);
    f->b1 += f->ct * (f->b0 - f->b1);
    f->b0 = sk_safe_val(f->b0);
    f->b1 = sk_safe_val(f->b1);
    return f->b1;
}

/* 1-Pole RC Lowpass / Damping Filter */
typedef struct {
    float y1;
    float coeff;
} sk_onepole_t;

static inline void sk_onepole_init_lp(sk_onepole_t *f, float fc, float sr) {
    f->y1 = 0.0f;
    if (sr <= 0.0f || fc >= sr * 0.5f) {
        f->coeff = 1.0f;
        return;
    }
    float x = expf(-2.0f * (float)M_PI * fc / sr);
    f->coeff = 1.0f - x;
}

static inline float sk_onepole_step(sk_onepole_t *f, float in) {
    f->y1 += f->coeff * (in - f->y1);
    return f->y1;
}

/* Logarithmic frequency mapping for delay feedback damping lowpass */
static inline float sk_delay_damping_coeff(float amount_norm, float sr) {
    amount_norm = sk_clampf(amount_norm, 0.0f, 1.0f);
    if (sr <= 0.0f) sr = 48000.0f;
    /* 20kHz (no damping) down to 500Hz (dark) */
    float fc = 20000.0f * powf(500.0f / 20000.0f, amount_norm);
    float x = expf(-2.0f * (float)M_PI * fc / sr);
    return 1.0f - x;
}

/* Logarithmic frequency mapping for delay feedback highpass (DC / bass buildup blocker) */
static inline float sk_delay_hp_coeff(float amount_norm, float sr) {
    amount_norm = sk_clampf(amount_norm, 0.0f, 1.0f);
    if (sr <= 0.0f) sr = 48000.0f;
    /* 20Hz (off) up to 2kHz (thin) */
    float fc = 20.0f * powf(2000.0f / 20.0f, amount_norm);
    float rc = 1.0f / (2.0f * (float)M_PI * fc);
    float dt = 1.0f / sr;
    return rc / (rc + dt);
}

/* =========================================================================
 * 6. Waveshaping, Distortion, Companding & Phase Warping
 * ========================================================================= */

/* Cubic soft-clipping with C1-continuous boundary at peak unity */
static inline float sk_softclip_cubic(float in, float drive) {
    if (drive <= 0.0f) return in;
    float d = in * drive;
    if (d > 1.0f) d = 1.0f;
    else if (d < -1.0f) d = -1.0f;
    return (d - (d * d * d) * (1.0f / 3.0f)) * 1.5f;
}

/* Simple stair-step quantizer function */
static inline float sk_quantize(float in, float levels) {
    if (levels <= 0.0f) return in;
    return floorf(in * levels) / levels;
}

static inline double sk_quantized(double in, double levels) {
    if (levels <= 0.0) return in;
    return floor(in * levels) / levels;
}

/* Bit-depth quantization with linear, companded (mu-law / tape), or dithered curves */
static inline float sk_quantize_bits_curve(float v, int bits, int curve, uint64_t *rng_state) {
    if (bits <= 0) return v;
    if (bits > 24) bits = 24;
    int levels = (1 << bits) - 1;
    if (levels < 1) levels = 1;

    if (curve == 1) {
        /* Cheap rational compander — boosts low-level detail before quantizing,
           un-boosts after: turns linear staircase into tape-style saturation. */
        const float k = 4.0f;
        float sign = v < 0.0f ? -1.0f : 1.0f;
        float av = fabsf(v);
        if (av > 1.0f) av = 1.0f;
        float companded = av * (1.0f + k) / (1.0f + k * av);
        int iv = (int)roundf(companded * (float)levels);
        float q = (float)iv * (1.0f / (float)levels);
        float expanded = q / (1.0f + k * (1.0f - q));
        return sign * expanded;
    }

    if (curve == 2 && rng_state) {
        /* Triangular dither before rounding (TPDF) */
        float t1 = sk_rng_float((sk_rng_t *)rng_state);
        float t2 = sk_rng_float((sk_rng_t *)rng_state);
        float dither = (t1 - t2) * (0.5f / (float)levels);
        int iv = (int)roundf((v + dither) * (float)levels);
        return (float)iv * (1.0f / (float)levels);
    }

    int iv = (int)roundf(v * (float)levels);
    return (float)iv * (1.0f / (float)levels);
}

/* Casio CZ-style Phase Distortion Warping */
static inline float sk_cz_phasor(int shape, float p, float d, float active_start, float active_end) {
    const float length = active_end - active_start;
    if (length <= 0.0f) return p;
    float phase = (p - active_start) / length;
    d = sk_clampf(d, -1.0f, 1.0f);
    const float amount = fabsf(d);

    switch (shape) {
        case 1: { /* saw -> pulse */
            const float breakpoint = 0.5f + 0.499f * d;
            const float inv_d = 0.5f / breakpoint;
            const float inv_1_minus_d = 0.5f / (1.0f - breakpoint);
            if (phase < breakpoint) {
                phase *= inv_d;
            } else {
                phase = 0.5f + (phase - breakpoint) * inv_1_minus_d;
            }
            break;
        }
        case 2: { /* square (folded sine) */
            const float half_d = d * 0.499f;
            const float scale = 0.5f / (0.5f - half_d);
            if (phase < 0.5f) {
                phase *= scale;
            } else {
                phase = 1.0f - (1.0f - phase) * scale;
            }
            break;
        }
        case 3: { /* triangle */
            const float half_d = d * 0.499f;
            const float scale = 0.5f / (0.5f - half_d);
            if (phase < 0.5f) {
                phase *= scale;
            } else {
                phase = 0.5f + (phase - 0.5f) * scale;
            }
            break;
        }
        case 4: { /* double sine */
            const float doubled = fmodf(phase * 2.0f, 1.0f);
            const float target = d < 0.0f ? 1.0f - doubled : doubled;
            phase += amount * (target - phase);
            break;
        }
        case 5: { /* saw -> triangle */
            const float half_d = d * 0.499f;
            const float scale1 = 0.5f / (0.5f - half_d);
            const float scale2 = 0.5f / (0.5f + half_d);
            if (phase < 0.5f) {
                phase *= scale1;
            } else {
                phase = 0.5f + (phase - 0.5f) * scale2;
            }
            break;
        }
        case 6: { /* resonant 1 */
            float exponent = 1.0f + 4.0f * amount;
            phase = sk_fast_pow(phase, d < 0.0f ? 1.0f / exponent : exponent);
            break;
        }
        case 7: { /* resonant 2 */
            float exponent = 1.0f + 8.0f * amount;
            phase = sk_fast_pow(phase, d < 0.0f ? 1.0f / exponent : exponent);
            break;
        }
        default:
            return p;
    }

    phase = sk_clampf(phase, 0.0f, 1.0f);
    return active_start + phase * length;
}

/* =========================================================================
 * 7. FFT & Spectral Analysis
 * ========================================================================= */

typedef struct { float re, im; } sk_cplx_t;

/* In-place Radix-2 Cooley-Tukey complex FFT/IFFT */
static inline void sk_fft_radix2(sk_cplx_t *buf, int n, int inverse) {
    if (!buf || n < 2) return;
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) {
            sk_cplx_t tmp = buf[i];
            buf[i] = buf[j];
            buf[j] = tmp;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = (inverse ? 2.0f : -2.0f) * (float)M_PI / (float)len;
        sk_cplx_t wlen;
        wlen.re = cosf(ang);
        wlen.im = sinf(ang);
        for (int i = 0; i < n; i += len) {
            sk_cplx_t w = { 1.0f, 0.0f };
            for (int k = 0; k < len / 2; k++) {
                sk_cplx_t u = buf[i + k];
                sk_cplx_t t = buf[i + k + len / 2];
                sk_cplx_t v;
                v.re = t.re * w.re - t.im * w.im;
                v.im = t.re * w.im + t.im * w.re;
                buf[i + k].re         = u.re + v.re;
                buf[i + k].im         = u.im + v.im;
                buf[i + k + len/2].re = u.re - v.re;
                buf[i + k + len/2].im = u.im - v.im;
                float nwre = w.re * wlen.re - w.im * wlen.im;
                float nwim = w.re * wlen.im + w.im * wlen.re;
                w.re = nwre; w.im = nwim;
            }
        }
    }
    if (inverse) {
        float inv_n = 1.0f / (float)n;
        for (int i = 0; i < n; i++) {
            buf[i].re *= inv_n;
            buf[i].im *= inv_n;
        }
    }
}

/* =========================================================================
 * 8. Interpolation, Delay Line & Buffer Looping Tools
 * ========================================================================= */

static inline float sk_lerp(float a, float b, float frac) {
    return a + frac * (b - a);
}

static inline double sk_lerpd(double a, double b, double frac) {
    return a + frac * (b - a);
}

/* Linear interpolation read from a circular delay buffer */
static inline float sk_delay_read_linear(const float *buf, int buf_size, int write_pos, float delay_frames) {
    if (!buf || buf_size <= 2) return 0.0f;
    if (delay_frames < 1.0f) delay_frames = 1.0f;
    if (delay_frames > (float)(buf_size - 2)) delay_frames = (float)(buf_size - 2);

    float read = (float)write_pos - delay_frames;
    while (read < 0.0f) read += (float)buf_size;
    while (read >= (float)buf_size) read -= (float)buf_size;
    int i0 = (int)read;
    int i1 = i0 + 1;
    if (i1 >= buf_size) i1 = 0;
    float frac = read - (float)i0;
    return buf[i0] + frac * (buf[i1] - buf[i0]);
}

/* Zero-crossing detector with epsilon threshold */
static inline int sk_find_zero_crossings(const double *s, int n, double thresh, int *out_indices, int max_out) {
    if (!s || n < 2 || max_out <= 0) return 0;
    int cnt = 0;
    for (int i = 0; i < n - 1 && cnt < max_out; i++) {
        double v0 = s[i], v1 = s[i + 1];
        if ((v0 >= 0.0 && v1 < 0.0) || (v0 < 0.0 && v1 >= 0.0) || fabs(v0) <= thresh) {
            if (out_indices) out_indices[cnt] = i;
            cnt++;
        }
    }
    return cnt;
}

/* Loop crossfade boundary blend */
static inline void sk_crossfade_seam(double *s, int n, int start, int end, int xfade) {
    if (!s || n <= 0 || start < 0 || end > n) return;
    if (xfade > end - start) xfade = end - start;
    if (xfade < 1) return;
    for (int i = 0; i < xfade; i++) {
        double fade_out = (double)(xfade - i) / (double)xfade;
        double fade_in  = (double)i / (double)xfade;
        int ei = end - xfade + i;
        int si = start + i;
        if (ei >= 0 && ei < n && si >= 0 && si < n) {
            s[ei] = s[ei] * fade_out + s[si] * fade_in;
        }
    }
}

/* Glissando / Portamento parameter calculation:
 * mode: 0 = exponential/geometric, 1 = linear frequency, 2 = period-linear
 */
static inline void sk_glissando_calc(int mode, float from_inc, float target_inc, float frames, float *delta, float *speed) {
    if (!delta || !speed) return;
    *delta = 0.0f;
    *speed = 1.0f;
    if (frames <= 0.0f || from_inc <= 0.00001f || target_inc <= 0.00001f) return;

    if (mode == 2) {
        float p1 = 1.0f / from_inc;
        float p2 = 1.0f / target_inc;
        *delta = (p2 - p1) / frames;
    } else if (mode == 1) {
        *delta = (target_inc - from_inc) / frames;
    } else {
        *speed = powf(target_inc / from_inc, 1.0f / frames);
    }
}

/* Steps glissando/portamento frequency phase increment.
 * Returns 1 if gliding continues, 0 if target reached.
 */
static inline int sk_glissando_step(float *current_inc, float target_inc, int mode, float delta, float speed) {
    if (!current_inc) return 0;
    if (mode == 2) {
        float p = (1.0f / *current_inc) + delta;
        if (p <= 0.0f) {
            *current_inc = target_inc;
            return 0;
        }
        *current_inc = 1.0f / p;
        if ((delta > 0.0f && *current_inc <= target_inc) ||
            (delta < 0.0f && *current_inc >= target_inc)) {
            *current_inc = target_inc;
            return 0;
        }
        return 1;
    } else if (mode == 1) {
        *current_inc += delta;
        if ((delta > 0.0f && *current_inc >= target_inc) ||
            (delta < 0.0f && *current_inc <= target_inc)) {
            *current_inc = target_inc;
            return 0;
        }
        return 1;
    } else {
        if (speed == 1.0f) {
            *current_inc = target_inc;
            return 0;
        }
        *current_inc *= speed;
        if ((speed > 1.0f && *current_inc >= target_inc) ||
            (speed < 1.0f && *current_inc <= target_inc)) {
            *current_inc = target_inc;
            return 0;
        }
        return 1;
    }
}

/* =========================================================================
 * 9. Physical Modeling & Plucked String Synthesis (Karplus-Strong)
 * ========================================================================= */

#define SK_KARPLUS_MAX_LEN 2048

typedef struct {
    float buffer[SK_KARPLUS_MAX_LEN];
    int size;               /* Buffer length (must be <= SK_KARPLUS_MAX_LEN) */
    int write_pos;
    int int_delay;          /* Integer delay portion */
    float frac_delay;       /* Sub-sample delay [0.0, 1.0) */
    float damping;          /* 1-pole lowpass filter weight (0.0 to 1.0) */
    float feedback;         /* Loop decay gain (typically 0.98 to 0.9995) */
    float filter_state;     /* 1-pole lowpass memory */
    float allpass_in;       /* Fractional allpass input memory x[n-1] */
    float allpass_out;      /* Fractional allpass output memory y[n-1] */
    float allpass_coeff;    /* Allpass 'a' coefficient */
    float dc_x;             /* DC blocker input memory */
    float dc_y;             /* DC blocker output memory */
    float drum_blend;       /* Karplus-Strong drum blend: 1.0 = string, 0.5 = drum membrane, 0.0 = noise */
    uint32_t rng_state;     /* Internal PRNG state for drum sign inversion */
    float sample_rate;
} sk_karplus_t;

/* Initialize Karplus-Strong physical modeling engine */
static inline void sk_karplus_init(sk_karplus_t *ks, float sample_rate) {
    if (!ks) return;
    memset(ks, 0, sizeof(*ks));
    ks->size = SK_KARPLUS_MAX_LEN;
    ks->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    ks->damping = 0.5f;
    ks->feedback = 0.992f;
    ks->drum_blend = 1.0f;
    ks->rng_state = 123456789u;
}

/* Tune Karplus-Strong to target frequency */
static inline void sk_karplus_tune(sk_karplus_t *ks, float freq) {
    if (!ks) return;
    if (freq < 20.0f) freq = 20.0f;
    float max_freq = ks->sample_rate * 0.45f;
    if (freq > max_freq) freq = max_freq;

    /* Total loop delay in samples D = Fs / f */
    float total_delay = ks->sample_rate / freq;

    /* Subtract lowpass filter delay (approximately damping * 0.5) */
    float lp_delay = ks->damping * 0.5f;
    float delay_needed = total_delay - lp_delay;
    if (delay_needed < 2.0f) delay_needed = 2.0f;
    if (delay_needed > (float)(ks->size - 4)) delay_needed = (float)(ks->size - 4);

    int idelay = (int)delay_needed;
    float frac = delay_needed - (float)idelay;

    if (frac < 0.1f && idelay > 2) {
        idelay -= 1;
        frac += 1.0f;
    }
    float ap_coeff = (1.0f - frac) / (1.0f + frac + 1e-6f);
    if (ap_coeff > 0.99f) ap_coeff = 0.99f;
    if (ap_coeff < -0.99f) ap_coeff = -0.99f;

    ks->int_delay = idelay;
    ks->frac_delay = frac;
    ks->allpass_coeff = ap_coeff;
}

/* Pluck the string with noise burst, pick position, and strike brightness */
static inline void sk_karplus_pluck(sk_karplus_t *ks, float freq, float velocity, float pick_pos, float brightness, sk_rng_t *rng) {
    if (!ks) return;
    sk_karplus_tune(ks, freq);

    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    pick_pos = sk_clampf(pick_pos, 0.02f, 0.98f);
    brightness = sk_clampf(brightness, 0.0f, 1.0f);

    /* Burst length corresponds to one string period */
    int burst_len = ks->int_delay;
    if (burst_len < 4) burst_len = 4;
    if (burst_len > ks->size) burst_len = ks->size;

    int pick_delay = (int)(pick_pos * (float)burst_len);
    if (pick_delay < 1) pick_delay = 1;
    if (pick_delay >= burst_len) pick_delay = burst_len - 1;

    /* Reset loop state; set write_pos to int_delay so read_pos starts at index 0 */
    ks->write_pos = ks->int_delay;
    ks->filter_state = 0.0f;
    ks->allpass_in = 0.0f;
    ks->allpass_out = 0.0f;
    ks->dc_x = 0.0f;
    ks->dc_y = 0.0f;

    if (freq < 150.0f) {
        /* Acoustic Bass / Cello Pizzicato:
         * Fleshy fingertip displacement wave.
         * A human finger plucking a heavy wound bass string displaces it into a rounded
         * asymmetric triangle, followed by a smooth fingertip release rather than
         * sharp white noise. This eliminates synthetic FM/plastic buzz and gives deep,
         * warm acoustic body response. */
        float p_pos = (float)pick_delay;
        float inv_p = 1.0f / (p_pos + 1e-4f);
        float inv_tail = 1.0f / ((float)(burst_len - pick_delay) + 1e-4f);
        float smooth_width = (float)burst_len * 0.12f;
        if (smooth_width < 4.0f) smooth_width = 4.0f;

        float exc_filter = 0.0f;
        float exc_coeff = 0.04f + 0.15f * brightness;

        for (int i = 0; i < burst_len; i++) {
            float fi = (float)i;
            float tri;
            if (fi <= p_pos) {
                tri = fi * inv_p;
            } else {
                tri = (float)(burst_len - i) * inv_tail;
            }
            /* Smooth rounded crest using cubic hermite blend */
            float dist_from_peak = fabsf(fi - p_pos);
            if (dist_from_peak < smooth_width) {
                float u = dist_from_peak / smooth_width;
                tri = tri * (0.85f + 0.15f * (3.0f * u * u - 2.0f * u * u * u));
            }
            /* Subtle string winding friction texture */
            float noise = rng ? sk_rng_float(rng) : (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f);
            exc_filter = (1.0f - exc_coeff) * exc_filter + exc_coeff * noise;
            ks->buffer[i] = (tri * 0.90f + exc_filter * 0.10f * brightness) * velocity;
        }
        for (int i = burst_len; i < ks->size; i++) {
            ks->buffer[i] = 0.0f;
        }
        return;
    }

    /* Lowpass filter for excitation brightness */
    float exc_filter = 0.0f;
    float exc_coeff = 0.1f + 0.85f * brightness;

    float raw_burst[SK_KARPLUS_MAX_LEN];
    for (int i = 0; i < burst_len; i++) {
        float noise = rng ? sk_rng_float(rng) : (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f);
        exc_filter = (1.0f - exc_coeff) * exc_filter + exc_coeff * noise;
        raw_burst[i] = exc_filter;
    }

    /* Fill delay buffer with pick-comb-filtered burst */
    for (int i = 0; i < ks->size; i++) {
        if (i < burst_len) {
            float prev = (i >= pick_delay) ? raw_burst[i - pick_delay] : 0.0f;
            ks->buffer[i] = (raw_burst[i] - prev) * velocity;
        } else {
            ks->buffer[i] = 0.0f;
        }
    }
}

/* Strike Karplus-Strong as a drum membrane (Karplus & Strong 1983 drum extension)
 * In the original 1983 CMJ paper "Digital Synthesis of Plucked-String and Drum Timbres",
 * Kevin Karplus & Alex Strong proved that replacing the averaging lowpass filter with
 * a random sign inversion filter:
 *
 *     y[n] = + 0.5 * (x[n] + x[n-1])   with probability b (blend)
 *     y[n] = - 0.5 * (x[n] + x[n-1])   with probability 1 - b
 *
 * breaks harmonic phase coherence and creates a decaying modal burst that sounds
 * remarkably like a physical drum membrane or tom.
 *
 * Parameters:
 *   freq: fundamental drum tuning in Hz (e.g. 50-150 Hz)
 *   velocity: impact force [0.0, 1.0]
 *   drum_blend: 0.5 for standard drum membrane, 1.0 for pure string, 0.0 for harsh noise
 *   brightness: excitation filter cutoff [0.0, 1.0]
 *   rng: optional PRNG pointer for reproducible strikes
 */
static inline void sk_karplus_strike_drum(sk_karplus_t *ks, float freq, float velocity, float drum_blend, float brightness, sk_rng_t *rng) {
    if (!ks) return;
    ks->drum_blend = sk_clampf(drum_blend, 0.0f, 1.0f);
    ks->feedback = 0.988f; /* Slightly faster decay for drums than strings */
    ks->damping = 0.55f;
    sk_karplus_pluck(ks, freq, velocity, 0.5f, brightness, rng);
}

/* Step Karplus-Strong string or drum synthesis by 1 sample */
static inline float sk_karplus_step(sk_karplus_t *ks) {
    if (!ks) return 0.0f;

    /* Read from delay line */
    int read_pos = ks->write_pos - ks->int_delay;
    if (read_pos < 0) read_pos += ks->size;

    float sample = ks->buffer[read_pos];

    /* 1-pole lowpass loss filter */
    float lp_out = (1.0f - ks->damping) * sample + ks->damping * ks->filter_state;
    ks->filter_state = lp_out;

    /* Karplus-Strong drum synthesis extension (Karplus & Strong 1983):
     * With probability (1.0 - drum_blend), randomly invert the sign of the filtered sample.
     * When drum_blend == 1.0: pure plucked string.
     * When drum_blend == 0.5: random sign inversion 50% of the time, producing an authentic
     * inharmonic decaying drum membrane sound. */
    if (ks->drum_blend < 0.999f) {
        ks->rng_state = ks->rng_state * 1664525u + 1013904223u;
        float r = (float)(ks->rng_state >> 16) / 65536.0f;
        if (r > ks->drum_blend) {
            lp_out = -lp_out;
        }
    }

    /* 1st-order allpass fractional delay filter: y[n] = a * (x[n] - y[n-1]) + x[n-1] */
    float ap_out = ks->allpass_coeff * (lp_out - ks->allpass_out) + ks->allpass_in;
    ks->allpass_in = lp_out;
    ks->allpass_out = ap_out;

    /* Feedback attenuation */
    float next_sample = ap_out * ks->feedback;

    /* DC blocker to prevent integration drift (y[n] = x[n] - x[n-1] + R * y[n-1], R = 0.995) */
    float dc_out = next_sample - ks->dc_x + 0.995f * ks->dc_y;
    ks->dc_x = next_sample;
    ks->dc_y = dc_out;

    /* Write back into delay line */
    ks->buffer[ks->write_pos] = dc_out;
    ks->write_pos++;
    if (ks->write_pos >= ks->size) ks->write_pos = 0;

    return sample;
}

/* =========================================================================
 * 10. Vocal Tract & Formant Synthesis
 * ========================================================================= */

#define SK_FORMANTS_COUNT 5

typedef enum {
    SK_VOWEL_A = 0, /* /ɑ/ father */
    SK_VOWEL_E = 1, /* /e/ bed */
    SK_VOWEL_I = 2, /* /i/ meet */
    SK_VOWEL_O = 3, /* /o/ boat */
    SK_VOWEL_U = 4, /* /u/ boot */
    SK_VOWEL_COUNT = 5
} sk_vowel_id_t;

/* Single formant bandpass resonator (constant 0 dB peak) */
typedef struct {
    float freq;
    float bandwidth;
    float gain;
    float a1, a2, b0;
    float x1, x2;
    float y1, y2;
} sk_formant_resonator_t;

typedef struct {
    sk_formant_resonator_t formants[SK_FORMANTS_COUNT];
    float sample_rate;
    float current_x;
    float current_y;
} sk_formant_bank_t;

/* Rosenberg-Klatt C1-continuous glottal pulse generator for natural voiced excitation without clicks.
 * Uses an asymmetric shape with rapid closing phase to produce natural high-frequency harmonic brilliance. */
static inline float sk_glottal_pulse(float phase, float open_quotient) {
    phase = sk_clampf(phase, 0.0f, 1.0f);
    open_quotient = sk_clampf(open_quotient, 0.2f, 0.85f);

    if (phase >= open_quotient) {
        return 0.0f; /* Glottis closed */
    }

    /* Asymmetric glottal pulse: 80% opening rise, 20% rapid closing snap */
    float t_open = open_quotient * 0.80f;
    float t_close = open_quotient - t_open;

    if (phase < t_open) {
        /* Opening phase: smooth raised-cosine rise from 0.0 to 1.0 */
        float theta = (float)M_PI * (phase / t_open);
        return 0.5f * (1.0f - cosf(theta));
    } else {
        /* Closing phase: rapid cosine fall from 1.0 to 0.0 (C0 and C1 continuous at closure) */
        float theta = ((float)M_PI * 0.5f) * ((phase - t_open) / t_close);
        return cosf(theta);
    }
}

/* Initialize single formant resonator */
static inline void sk_formant_resonator_set(sk_formant_resonator_t *res, float freq, float bw, float gain, float sample_rate) {
    if (!res) return;
    if (sample_rate < 1000.0f) sample_rate = 48000.0f;
    if (freq < 50.0f) freq = 50.0f;
    if (freq > sample_rate * 0.45f) freq = sample_rate * 0.45f;
    if (bw < 10.0f) bw = 10.0f;

    res->freq = freq;
    res->bandwidth = bw;
    res->gain = gain;

    float r = expf(-(float)M_PI * bw / sample_rate);
    float omega = 2.0f * (float)M_PI * freq / sample_rate;

    res->a1 = -2.0f * r * cosf(omega);
    res->a2 = r * r;
    res->b0 = (1.0f - r) * 0.5f;
}

static inline float sk_formant_resonator_step(sk_formant_resonator_t *res, float in) {
    if (!res) return 0.0f;
    /* 2-pole 2-zero constant-peak bandpass: y[n] = b0*(x[n] - x[n-2]) - a1*y[n-1] - a2*y[n-2]
     * Rejects DC/sub-bass thrum completely while normalizing peak formant resonance to 0 dB */
    float y = res->b0 * (in - res->x2) - res->a1 * res->y1 - res->a2 * res->y2;
    res->x2 = res->x1;
    res->x1 = in;
    res->y2 = res->y1;
    res->y1 = y;
    return y * res->gain;
}

/* Initialize formant filter bank */
static inline void sk_formant_bank_init(sk_formant_bank_t *fb, float sample_rate) {
    if (!fb) return;
    memset(fb, 0, sizeof(*fb));
    fb->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
}

/* Configure formant bank to standard canonical vowel */
static inline void sk_formant_bank_set_vowel(sk_formant_bank_t *fb, sk_vowel_id_t vowel) {
    if (!fb) return;

    /* Formant frequencies (Hz) for A, E, I, O, U: F1, F2, F3, F4, F5 */
    static const float F[5][5] = {
        { 730.0f, 1090.0f, 2440.0f, 3500.0f, 4500.0f }, /* A */
        { 530.0f, 1840.0f, 2480.0f, 3500.0f, 4500.0f }, /* E */
        { 270.0f, 2290.0f, 3010.0f, 3500.0f, 4500.0f }, /* I */
        { 500.0f,  850.0f, 2250.0f, 3500.0f, 4500.0f }, /* O */
        { 300.0f,  870.0f, 2240.0f, 3500.0f, 4500.0f }  /* U */
    };
    /* Bandwidths (Hz) */
    static const float B[5][5] = {
        { 130.0f,  100.0f, 170.0f, 250.0f, 300.0f }, /* A */
        { 100.0f,  100.0f, 150.0f, 250.0f, 300.0f }, /* E */
        {  60.0f,  100.0f, 200.0f, 250.0f, 300.0f }, /* I */
        {  80.0f,   80.0f, 100.0f, 250.0f, 300.0f }, /* O */
        {  60.0f,   80.0f, 100.0f, 250.0f, 300.0f }  /* U */
    };
    /* Calibrated Relative Gains for crisp high-end brilliance */
    static const float G[5][5] = {
        { 1.0f, 0.70f, 0.55f, 0.38f, 0.28f }, /* A */
        { 1.0f, 0.75f, 0.60f, 0.40f, 0.30f }, /* E */
        { 1.0f, 0.65f, 0.55f, 0.38f, 0.28f }, /* I */
        { 1.0f, 0.65f, 0.35f, 0.25f, 0.20f }, /* O */
        { 1.0f, 0.55f, 0.30f, 0.25f, 0.20f }  /* U */
    };

    int v = (int)vowel;
    if (v < 0 || v >= SK_VOWEL_COUNT) v = 0;

    for (int i = 0; i < SK_FORMANTS_COUNT; i++) {
        sk_formant_resonator_set(&fb->formants[i], F[v][i], B[v][i], G[v][i], fb->sample_rate);
    }
}

/* Continuous 2D Vowel Morphing:
 * X in [-1.0, 1.0]: Back (-1 = /u/, /o/) to Front (+1 = /i/, /e/)
 * Y in [-1.0, 1.0]: Close (-1 = /i/, /u/) to Open (+1 = /a/)
 */
static inline void sk_formant_bank_set_xy(sk_formant_bank_t *fb, float x, float y) {
    if (!fb) return;
    x = sk_clampf(x, -1.0f, 1.0f);
    y = sk_clampf(y, -1.0f, 1.0f);
    fb->current_x = x;
    fb->current_y = y;

    /* Normalize to [0, 1] */
    float nx = (x + 1.0f) * 0.5f;
    float ny = (y + 1.0f) * 0.5f;

    /* F1 mainly controlled by Openness (Y): 250 Hz (closed) to 750 Hz (open) */
    float f1 = 250.0f + 500.0f * ny;
    /* F2 mainly controlled by Frontness (X): 800 Hz (back) to 2300 Hz (front) */
    float f2 = 800.0f + 1500.0f * nx;
    /* F3 influenced by both: 2200 Hz to 3000 Hz */
    float f3 = 2200.0f + 800.0f * nx;
    float f4 = 3500.0f;
    float f5 = 4500.0f;

    sk_formant_resonator_set(&fb->formants[0], f1, 100.0f, 1.00f, fb->sample_rate);
    sk_formant_resonator_set(&fb->formants[1], f2, 110.0f, 0.70f, fb->sample_rate);
    sk_formant_resonator_set(&fb->formants[2], f3, 170.0f, 0.55f, fb->sample_rate);
    sk_formant_resonator_set(&fb->formants[3], f4, 250.0f, 0.35f, fb->sample_rate);
    sk_formant_resonator_set(&fb->formants[4], f5, 300.0f, 0.25f, fb->sample_rate);
}

/* Step formant bank: processes input through parallel resonators with alternating signs
 * (+F1 - F2 + F3 - F4 + F5) per Klatt/Holmes standard to eliminate destructive phase cancellation in inter-formant valleys */
static inline float sk_formant_bank_step(sk_formant_bank_t *fb, float in) {
    if (!fb) return in;
    float s1 = sk_formant_resonator_step(&fb->formants[0], in);
    float s2 = sk_formant_resonator_step(&fb->formants[1], in);
    float s3 = sk_formant_resonator_step(&fb->formants[2], in);
    float s4 = sk_formant_resonator_step(&fb->formants[3], in);
    float s5 = sk_formant_resonator_step(&fb->formants[4], in);
    return s1 - s2 + s3 - s4 + s5;
}

/* =========================================================================
 * 11. Phoneme DSL & Standalone Speech Synthesis Engine
 * ========================================================================= */

#define SK_PHONEME_MAX_TOKENS 512

typedef enum {
    SK_PHO_VOICED   = (1 << 0),
    SK_PHO_FRIC     = (1 << 1),
    SK_PHO_STOP     = (1 << 2),
    SK_PHO_NASAL    = (1 << 3),
    SK_PHO_ASP      = (1 << 4)
} sk_phoneme_flags_t;

typedef struct {
    char code[4];          /* ARPAbet code: "AA", "S", "K", "R", "EH", "D", etc. */
    uint16_t flags;
    float default_ms;      /* Default duration in milliseconds */
    float voice_gain;      /* Glottal pulse level (0.0 to 1.0) */
    float noise_gain;      /* Aspiration/frication noise level (0.0 to 1.0) */
    float noise_freq;      /* Frication filter center frequency in Hz */
    float noise_bw;        /* Frication filter bandwidth in Hz */
    float f1, f2, f3, f4, f5;  /* Formant target frequencies in Hz */
    float b1, b2, b3, b4, b5;  /* Formant bandwidths in Hz */
    float g1, g2, g3, g4, g5;  /* Formant relative gains */
} sk_phoneme_def_t;

/* Acoustic parameter frame (compiled from DSL token) */
typedef struct {
    float duration_ms;
    float pitch_hz;
    float pitch_end_hz;    /* Optional pitch glide target (0 or == pitch_hz for steady; != for inflection) */
    float voice_gain;
    float noise_gain;
    float noise_freq;
    float noise_bw;
    float vibrato_depth;   /* Vibrato modulation depth (e.g. 0.02 = 2%) */
    float vibrato_rate;    /* Vibrato speed in Hz (e.g. 5.5 Hz) */
    uint16_t flags;
    float f1, f2, f3, f4, f5;
    float b1, b2, b3, b4, b5;
    float g1, g2, g3, g4, g5;
} sk_phoneme_frame_t;

/* Static ARPAbet acoustic reference table */
static inline const sk_phoneme_def_t *sk_phoneme_lookup(const char *code) {
    static const sk_phoneme_def_t TABLE[] = {
        /* VOWELS (F1..F5, B1..B5, G1..G5) */
        { "AA", SK_PHO_VOICED, 140.0f, 1.0f, 0.0f, 0.0f, 0.0f, 730, 1090, 2440, 3500, 4500, 130, 100, 170, 250, 300, 1.0f, 0.70f, 0.55f, 0.38f, 0.28f },
        { "AE", SK_PHO_VOICED, 140.0f, 1.0f, 0.0f, 0.0f, 0.0f, 660, 1720, 2410, 3500, 4500, 100, 100, 200, 250, 300, 1.0f, 0.75f, 0.60f, 0.40f, 0.30f },
        { "AH", SK_PHO_VOICED, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f, 640, 1190, 2390, 3500, 4500, 100, 100, 180, 250, 300, 1.0f, 0.70f, 0.55f, 0.38f, 0.28f },
        { "AO", SK_PHO_VOICED, 140.0f, 1.0f, 0.0f, 0.0f, 0.0f, 570,  840, 2410, 3500, 4500, 100,  90, 160, 250, 300, 1.0f, 0.65f, 0.45f, 0.30f, 0.22f },
        { "EH", SK_PHO_VOICED, 120.0f, 1.0f, 0.0f, 0.0f, 0.0f, 530, 1840, 2480, 3500, 4500, 100, 100, 150, 250, 300, 1.0f, 0.75f, 0.60f, 0.40f, 0.30f },
        { "ER", SK_PHO_VOICED, 150.0f, 1.0f, 0.0f, 0.0f, 0.0f, 490, 1350, 1690, 3500, 4500, 100, 100, 150, 250, 300, 1.0f, 0.70f, 0.65f, 0.38f, 0.28f },
        { "EY", SK_PHO_VOICED, 150.0f, 1.0f, 0.0f, 0.0f, 0.0f, 480, 2000, 2600, 3500, 4500,  90, 100, 160, 250, 300, 1.0f, 0.75f, 0.55f, 0.38f, 0.28f },
        { "IH", SK_PHO_VOICED, 110.0f, 1.0f, 0.0f, 0.0f, 0.0f, 390, 1990, 2550, 3500, 4500,  90, 100, 180, 250, 300, 1.0f, 0.70f, 0.55f, 0.38f, 0.28f },
        { "IY", SK_PHO_VOICED, 130.0f, 1.0f, 0.0f, 0.0f, 0.0f, 270, 2290, 3010, 3500, 4500,  60, 100, 200, 250, 300, 1.0f, 0.65f, 0.55f, 0.38f, 0.28f },
        { "OW", SK_PHO_VOICED, 150.0f, 1.0f, 0.0f, 0.0f, 0.0f, 500,  850, 2250, 3500, 4500,  80,  80, 100, 250, 300, 1.0f, 0.65f, 0.35f, 0.25f, 0.20f },
        { "UH", SK_PHO_VOICED, 110.0f, 1.0f, 0.0f, 0.0f, 0.0f, 440, 1020, 2240, 3500, 4500,  80,  80, 100, 250, 300, 1.0f, 0.65f, 0.35f, 0.25f, 0.20f },
        { "UW", SK_PHO_VOICED, 130.0f, 1.0f, 0.0f, 0.0f, 0.0f, 300,  870, 2240, 3500, 4500,  60,  80, 100, 250, 300, 1.0f, 0.55f, 0.30f, 0.25f, 0.20f },
        /* DIPHTHONGS */
        { "AY", SK_PHO_VOICED, 160.0f, 1.0f, 0.0f, 0.0f, 0.0f, 650, 1700, 2500, 3500, 4500, 100, 100, 160, 250, 300, 1.0f, 0.70f, 0.55f, 0.38f, 0.28f },
        { "AW", SK_PHO_VOICED, 160.0f, 1.0f, 0.0f, 0.0f, 0.0f, 680, 1150, 2400, 3500, 4500, 100, 100, 160, 250, 300, 1.0f, 0.65f, 0.45f, 0.35f, 0.25f },
        { "OY", SK_PHO_VOICED, 160.0f, 1.0f, 0.0f, 0.0f, 0.0f, 520, 1600, 2400, 3500, 4500,  90, 100, 160, 250, 300, 1.0f, 0.70f, 0.55f, 0.38f, 0.28f },

        /* FRICATIVES */
        { "S",  SK_PHO_FRIC,   80.0f, 0.0f, 0.35f, 6500, 2400, 300, 1500, 2500, 3500, 4500,  80, 100, 150, 250, 300, 0.1f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "SH", SK_PHO_FRIC,   90.0f, 0.0f, 0.32f, 3600, 1800, 300, 1800, 2800, 3500, 4500,  80, 100, 150, 250, 300, 0.1f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "Z",  SK_PHO_VOICED|SK_PHO_FRIC, 80.0f, 0.6f, 0.25f, 6500, 2400, 300, 1500, 2500, 3500, 4500, 80, 100, 150, 250, 300, 0.6f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "ZH", SK_PHO_VOICED|SK_PHO_FRIC, 90.0f, 0.6f, 0.25f, 3600, 1800, 300, 1800, 2800, 3500, 4500, 80, 100, 150, 250, 300, 0.6f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "F",  SK_PHO_FRIC,   70.0f, 0.0f, 0.22f, 4500, 3000, 300, 1200, 2400, 3500, 4500,  80, 100, 150, 250, 300, 0.1f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "V",  SK_PHO_VOICED|SK_PHO_FRIC, 70.0f, 0.7f, 0.18f, 4500, 3000, 300, 1200, 2400, 3500, 4500, 80, 100, 150, 250, 300, 0.7f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "TH", SK_PHO_FRIC,   70.0f, 0.0f, 0.18f, 5000, 2800, 300, 1400, 2400, 3500, 4500,  80, 100, 150, 250, 300, 0.1f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "DH", SK_PHO_VOICED|SK_PHO_FRIC, 60.0f, 0.65f, 0.18f, 4000, 2500, 300, 1400, 2400, 3500, 4500, 80, 100, 150, 250, 300, 0.65f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "HH", SK_PHO_ASP,    60.0f, 0.0f, 0.25f, 2200, 2000, 500, 1500, 2500, 3500, 4500, 100, 100, 150, 250, 300, 0.2f, 0.20f, 0.10f, 0.05f, 0.05f },

        /* AFFRICATES */
        { "CH", SK_PHO_STOP|SK_PHO_FRIC, 80.0f, 0.0f, 0.35f, 3800, 1800, 300, 1800, 2800, 3500, 4500, 80, 100, 150, 250, 300, 0.1f, 0.10f, 0.10f, 0.05f, 0.05f },
        { "JH", SK_PHO_VOICED|SK_PHO_STOP|SK_PHO_FRIC, 80.0f, 0.6f, 0.25f, 3800, 1800, 300, 1800, 2800, 3500, 4500, 80, 100, 150, 250, 300, 0.6f, 0.10f, 0.10f, 0.05f, 0.05f },

        /* STOPS / PLOSIVES (with closure & crisp transient release burst) */
        { "P",  SK_PHO_STOP,   60.0f, 0.0f, 0.30f, 1500,  800, 300,  800, 2200, 3500, 4500,  80, 100, 150, 250, 300, 0.1f, 0.10f, 0.05f, 0.02f, 0.02f },
        { "B",  SK_PHO_VOICED|SK_PHO_STOP, 60.0f, 0.5f, 0.25f, 1500, 800, 250,  800, 2200, 3500, 4500,  80, 100, 150, 250, 300, 0.5f, 0.10f, 0.05f, 0.02f, 0.02f },
        { "T",  SK_PHO_STOP,   60.0f, 0.0f, 0.38f, 5200, 1800, 300, 1800, 2600, 3500, 4500,  80, 100, 150, 250, 300, 0.1f, 0.10f, 0.05f, 0.02f, 0.02f },
        { "D",  SK_PHO_VOICED|SK_PHO_STOP, 60.0f, 0.6f, 0.28f, 4500, 1800, 250, 1800, 2600, 3500, 4500,  80, 100, 150, 250, 300, 0.6f, 0.10f, 0.05f, 0.02f, 0.02f },
        { "K",  SK_PHO_STOP,   70.0f, 0.0f, 0.34f, 2800, 1200, 300, 2400, 2700, 3500, 4500,  80, 100, 150, 250, 300, 0.1f, 0.10f, 0.05f, 0.02f, 0.02f },
        { "G",  SK_PHO_VOICED|SK_PHO_STOP, 70.0f, 0.6f, 0.26f, 2500, 1200, 250, 2400, 2700, 3500, 4500,  80, 100, 150, 250, 300, 0.6f, 0.10f, 0.05f, 0.02f, 0.02f },

        /* NASALS */
        { "M",  SK_PHO_VOICED|SK_PHO_NASAL, 90.0f, 0.85f, 0.0f, 0.0f, 0.0f, 250, 1100, 2200, 3500, 4500, 50, 100, 150, 250, 300, 1.0f, 0.25f, 0.12f, 0.05f, 0.02f },
        { "N",  SK_PHO_VOICED|SK_PHO_NASAL, 90.0f, 0.85f, 0.0f, 0.0f, 0.0f, 250, 1500, 2200, 3500, 4500, 50, 100, 150, 250, 300, 1.0f, 0.25f, 0.12f, 0.05f, 0.02f },
        { "NG", SK_PHO_VOICED|SK_PHO_NASAL, 90.0f, 0.85f, 0.0f, 0.0f, 0.0f, 250, 1800, 2200, 3500, 4500, 50, 100, 150, 250, 300, 1.0f, 0.25f, 0.12f, 0.05f, 0.02f },

        /* GLIDES & LIQUIDS */
        { "L",  SK_PHO_VOICED, 80.0f, 0.90f, 0.0f, 0.0f, 0.0f, 380, 1200, 2700, 3500, 4500, 60, 100, 150, 250, 300, 0.9f, 0.50f, 0.35f, 0.15f, 0.10f },
        { "R",  SK_PHO_VOICED, 80.0f, 0.90f, 0.0f, 0.0f, 0.0f, 420, 1300, 1600, 3500, 4500, 60, 100, 120, 250, 300, 0.9f, 0.55f, 0.45f, 0.20f, 0.10f },
        { "W",  SK_PHO_VOICED, 80.0f, 0.90f, 0.0f, 0.0f, 0.0f, 300,  800, 2200, 3500, 4500, 60,  80, 100, 250, 300, 0.9f, 0.40f, 0.25f, 0.15f, 0.10f },
        { "Y",  SK_PHO_VOICED, 80.0f, 0.90f, 0.0f, 0.0f, 0.0f, 280, 2200, 3000, 3500, 4500, 60,  90, 150, 250, 300, 0.9f, 0.45f, 0.35f, 0.15f, 0.10f },

        /* PAUSE / SILENCE */
        { "_",  0,             80.0f, 0.0f, 0.0f, 0.0f, 0.0f, 300, 1200, 2400, 3500, 4500, 100, 100, 150, 250, 300, 0.0f, 0.0f,  0.0f,  0.0f,  0.0f }
    };
    static const int COUNT = sizeof(TABLE) / sizeof(TABLE[0]);

    if (!code || !code[0]) return NULL;

    /* Clean code: strip any trailing stress digit ('0', '1', '2') */
    char clean[8];
    int len = 0;
    while (code[len] && len < 7) {
        clean[len] = code[len];
        len++;
    }
    clean[len] = '\0';
    if (len > 1 && (clean[len - 1] >= '0' && clean[len - 1] <= '2')) {
        clean[len - 1] = '\0';
    }

    for (int i = 0; i < COUNT; i++) {
        if (strcasecmp(TABLE[i].code, clean) == 0) {
            return &TABLE[i];
        }
    }
    return NULL;
}

/* Compile a phonetic DSL string (e.g. "[|hello| HH:50 AH:60 |world| W:70 ER:90]") into flat frame array.
   Optional err_token captures the first unrecognized phoneme token for diagnostics.
   Optional out_cues captures word/phrase markers demarcated with |label|. */
static inline int sk_phoneme_compile_dsl_markers(const char *dsl, sk_phoneme_frame_t *out_frames, int max_frames,
                                                float default_pitch, char *err_token, size_t err_token_sz,
                                                sk_cue_t *out_cues, int max_cues, int *out_num_cues) {
    if (!dsl || !out_frames || max_frames <= 0) return 0;
    if (default_pitch < 40.0f) default_pitch = 120.0f;
    if (err_token && err_token_sz > 0) err_token[0] = '\0';
    if (out_num_cues) *out_num_cues = 0;

    int frame_count = 0;
    const char *p = dsl;

    while (*p && frame_count < max_frames) {
        /* Skip brackets, spaces, commas */
        while (*p && (*p == '[' || *p == ']' || *p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == ',')) {
            p++;
        }
        if (!*p) break;

        /* Marker tokens: |word| or |phrase| */
        if (*p == '|') {
            p++;
            const char *m_start = p;
            while (*p && *p != '|' && *p != ']' && *p != '\n' && *p != '\r') p++;
            int m_len = (int)(p - m_start);
            if (*p == '|') p++; /* skip closing '|' */
            if (m_len > 0 && out_cues && out_num_cues && *out_num_cues < max_cues) {
                sk_cue_t *c = &out_cues[*out_num_cues];
                memset(c, 0, sizeof(*c));
                c->type = SK_CUE_WORD;
                int clen = m_len < (int)sizeof(c->label) - 1 ? m_len : (int)sizeof(c->label) - 1;
                memcpy(c->label, m_start, clen);
                c->label[clen] = '\0';
                c->start = (uint32_t)frame_count; /* temporary frame index */
                (*out_num_cues)++;
            }
            continue;
        }

        /* Read phoneme code */
        char code[8];
        int c_idx = 0;
        int stress = -1; /* -1 = none, 0 = unstressed, 1 = primary, 2 = secondary */
        while (*p && *p != ':' && *p != '@' && *p != '~' && *p != '*' && *p != ' ' && *p != '\t' && *p != ',' && *p != ']' && *p != '|' && c_idx < 7) {
            code[c_idx++] = *p++;
        }
        code[c_idx] = '\0';
        if (c_idx == 0) break;

        /* Detect ARPAbet numeric stress suffix (e.g. "EH1", "AA0", "UW2") */
        if (c_idx > 1 && (code[c_idx - 1] >= '0' && code[c_idx - 1] <= '2')) {
            stress = code[c_idx - 1] - '0';
        }

        const sk_phoneme_def_t *def = sk_phoneme_lookup(code);
        if (!def) {
            /* Record unknown token for caller diagnostic */
            if (err_token && err_token[0] == '\0' && err_token_sz > 0) {
                snprintf(err_token, err_token_sz, "%s", code);
            }
            /* Fallback to pause if unknown */
            def = sk_phoneme_lookup("_");
        }

        float dur = def ? def->default_ms : 80.0f;
        float pitch = default_pitch;
        float pitch_end = 0.0f;
        float vib_depth = 0.0f;
        float vib_rate = 5.5f;
        float amp_scale = 1.0f;

        /* Linguistic stress factors (applied to both default and explicit durations/pitches) */
        float stress_dur_mult = 1.0f;
        float stress_pitch_mult = 1.0f;
        if (stress == 1) {
            stress_dur_mult = 1.25f;       /* Primary stress: +25% duration */
            stress_pitch_mult = 1.15f;     /* +15% pitch accent */
            amp_scale *= 1.20f;            /* +20% voice intensity */
        } else if (stress == 0) {
            stress_dur_mult = 0.75f;       /* Unstressed: -25% duration */
            stress_pitch_mult = 0.95f;
            amp_scale *= 0.80f;            /* -20% voice intensity */
        } else if (stress == 2) {
            stress_dur_mult = 1.10f;       /* Secondary stress: +10% duration */
            stress_pitch_mult = 1.05f;
            amp_scale *= 1.05f;
        }

        /* Parse optional modifiers in any order (:dur, @pitch, @start>end, ~rate, *amp) */
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r' && *p != ',' && *p != ']' && *p != '|') {
            if (*p == ':') {
                p++;
                char *end;
                float parsed_dur = strtof(p, &end);
                if (end > p && parsed_dur > 0.0f) {
                    dur = parsed_dur;
                    p = end;
                }
            } else if (*p == '@') {
                p++;
                char *end;
                float parsed_pitch = strtof(p, &end);
                if (end > p && parsed_pitch > 20.0f) {
                    pitch = parsed_pitch;
                    p = end;
                    if (*p == '>' || *p == '<') {
                        p++;
                        float parsed_end = strtof(p, &end);
                        if (end > p && parsed_end > 20.0f) {
                            pitch_end = parsed_end;
                            p = end;
                        }
                    }
                }
            } else if (*p == '~') {
                p++;
                vib_depth = 0.035f; /* 3.5% clear audible vocal vibrato wobble */
                vib_rate = 5.5f;
                char *end;
                float parsed_rate = strtof(p, &end);
                if (end > p && parsed_rate > 0.5f) {
                    vib_rate = parsed_rate;
                    p = end;
                }
            } else if (*p == '*') {
                p++;
                char *end;
                float parsed_amp = strtof(p, &end);
                if (end > p && parsed_amp >= 0.0f) {
                    amp_scale *= parsed_amp;
                    p = end;
                }
            } else {
                p++;
            }
        }

        dur *= stress_dur_mult;
        if (pitch == default_pitch) {
            pitch *= stress_pitch_mult;
        }

        /* Fill frame */
        sk_phoneme_frame_t *f = &out_frames[frame_count++];
        memset(f, 0, sizeof(*f));
        f->duration_ms = dur;
        f->pitch_hz = pitch;
        f->pitch_end_hz = pitch_end > 20.0f ? pitch_end : pitch;
        f->vibrato_depth = vib_depth;
        f->vibrato_rate = vib_rate;

        if (def) {
            f->flags = def->flags;
            f->voice_gain = def->voice_gain * amp_scale;
            f->noise_gain = def->noise_gain * amp_scale;
            f->noise_freq = def->noise_freq;
            f->noise_bw = def->noise_bw;
            f->f1 = def->f1; f->f2 = def->f2; f->f3 = def->f3; f->f4 = def->f4; f->f5 = def->f5;
            f->b1 = def->b1; f->b2 = def->b2; f->b3 = def->b3; f->b4 = def->b4; f->b5 = def->b5;
            f->g1 = def->g1; f->g2 = def->g2; f->g3 = def->g3; f->g4 = def->g4; f->g5 = def->g5;
        }
    }

    return frame_count;
}

/* Backward-compatible wrapper */
static inline int sk_phoneme_compile_dsl_ex(const char *dsl, sk_phoneme_frame_t *out_frames, int max_frames,
                                            float default_pitch, char *err_token, size_t err_token_sz) {
    return sk_phoneme_compile_dsl_markers(dsl, out_frames, max_frames, default_pitch, err_token, err_token_sz, NULL, 0, NULL);
}

/* Convert frame-relative cues to sample offsets and slice ranges */
static inline void sk_phoneme_cues_to_samples(sk_cue_t *cues, int num_cues,
                                              const sk_phoneme_frame_t *frames, int num_frames,
                                              float sample_rate, uint32_t total_samples) {
    if (!cues || num_cues <= 0 || !frames || num_frames <= 0 || sample_rate <= 0.0f) return;
    for (int i = 0; i < num_cues; i++) {
        int f_idx = (int)cues[i].start;
        if (f_idx > num_frames) f_idx = num_frames;
        double ms = 0.0;
        for (int f = 0; f < f_idx; f++) {
            ms += frames[f].duration_ms;
        }
        uint32_t start_samp = (uint32_t)(ms * 0.001 * sample_rate);
        if (start_samp > total_samples) start_samp = total_samples;
        cues[i].start = start_samp;
    }
    for (int i = 0; i < num_cues; i++) {
        if (i + 1 < num_cues) {
            cues[i].end = cues[i + 1].start > cues[i].start ? cues[i + 1].start - 1 : cues[i].start;
        } else {
            cues[i].end = total_samples > 0 ? total_samples - 1 : 0;
        }
    }
}

static inline int sk_phoneme_compile_dsl(const char *dsl, sk_phoneme_frame_t *out_frames, int max_frames, float default_pitch) {
    return sk_phoneme_compile_dsl_ex(dsl, out_frames, max_frames, default_pitch, NULL, 0);
}

/* Voice configuration parameters (inspired by DECtalk, SAM, & Kraftwerk) */
typedef struct {
    float pitch_scale;        /* Fundamental frequency multiplier (1.0 = normal, 1.6 = female, 0.75 = deep) */
    float formant_scale;      /* Vocal tract length / throat scale (1.0 = normal, 1.25 = child/elf, 0.88 = giant) */
    float speed_scale;        /* Tempo / cadence multiplier (1.0 = normal, 1.2 = fast, 0.8 = slow) */
    float breathiness;        /* Glottal aspiration noise mixed with voiced sounds (0.0 = clean, 0.08 = breathy) */
    float pitch_declination;  /* Sentence pitch downdrift in Hz over phrase (e.g. 10.0 Hz) */
    float vowel_level;        /* Vowel voiced resonance gain knob (default 1.0) */
    float consonant_level;    /* Frication / noise consonant gain knob (default 1.0) */
    float robot_buzz;         /* 0.0 = natural human glottal pulse, 1.0 = Kraftwerk / Speak & Spell buzz */
} sk_speech_voice_t;

static inline sk_speech_voice_t sk_speech_voice_default(void) {
    sk_speech_voice_t v = { 1.0f, 1.0f, 1.0f, 0.0f, 6.0f, 1.0f, 1.0f, 0.0f };
    return v;
}

static inline sk_speech_voice_t sk_speech_voice_paul(void) {
    /* DECtalk 'Perfect Paul' standard adult male */
    sk_speech_voice_t v = { 1.0f, 1.0f, 1.0f, 0.0f, 8.0f, 1.05f, 0.70f, 0.0f };
    return v;
}

static inline sk_speech_voice_t sk_speech_voice_harry(void) {
    /* DECtalk 'Huge Harry' deep resonant male */
    sk_speech_voice_t v = { 0.72f, 0.88f, 0.95f, 0.0f, 6.0f, 1.20f, 0.70f, 0.0f };
    return v;
}

static inline sk_speech_voice_t sk_speech_voice_betty(void) {
    /* DECtalk 'Beautiful Betty' female voice */
    sk_speech_voice_t v = { 1.60f, 1.15f, 1.05f, 0.0f, 10.0f, 1.0f, 0.70f, 0.0f };
    return v;
}

static inline sk_speech_voice_t sk_speech_voice_sam(void) {
    /* Retro 1982 SAM (Software Automatic Mouth) / C64 voice */
    sk_speech_voice_t v = { 1.12f, 1.05f, 1.15f, 0.0f, 0.0f, 1.10f, 0.80f, 0.35f };
    return v;
}

static inline sk_speech_voice_t sk_speech_voice_elf(void) {
    /* High-pitched child / elf / 'Kit the Kid' */
    sk_speech_voice_t v = { 1.85f, 1.30f, 1.15f, 0.0f, 12.0f, 0.95f, 0.70f, 0.0f };
    return v;
}

static inline sk_speech_voice_t sk_speech_voice_robot(void) {
    /* Pure robotic monotone (Kraftwerk - The Robots / Computer World) */
    sk_speech_voice_t v = { 0.92f, 1.0f, 1.0f, 0.0f, 0.0f, 1.15f, 0.95f, 0.85f };
    return v;
}

/* Standalone renderer: renders compiled phoneme frames into PCM float audio buffer with voice persona */
static inline int sk_phoneme_render_frames_voice(const sk_phoneme_frame_t *frames, int num_frames,
                                                float *out_audio, int max_samples, float sample_rate,
                                                const sk_speech_voice_t *voice, sk_rng_t *rng) {
    if (!frames || num_frames <= 0 || !out_audio || max_samples <= 0) return 0;
    if (sample_rate < 1000.0f) sample_rate = 48000.0f;

    sk_speech_voice_t v = voice ? *voice : sk_speech_voice_default();
    if (v.pitch_scale <= 0.0f) v.pitch_scale = 1.0f;
    if (v.formant_scale <= 0.0f) v.formant_scale = 1.0f;
    if (v.speed_scale <= 0.0f) v.speed_scale = 1.0f;
    if (v.vowel_level <= 0.0f) v.vowel_level = 1.0f;
    if (v.consonant_level <= 0.0f) v.consonant_level = 1.0f;

    sk_formant_bank_t fb;
    sk_formant_bank_init(&fb, sample_rate);

    sk_biquad_t noise_filter;
    sk_biquad_init(&noise_filter, SK_BIQUAD_BANDPASS, 4000.0f, 1.5f, sample_rate);

    float current_f1 = frames[0].f1 * v.formant_scale;
    float current_f2 = frames[0].f2 * v.formant_scale;
    float current_f3 = frames[0].f3 * v.formant_scale;
    float current_f4 = frames[0].f4 * v.formant_scale;
    float current_f5 = frames[0].f5 * v.formant_scale;
    float current_b1 = frames[0].b1;
    float current_b2 = frames[0].b2;
    float current_b3 = frames[0].b3;
    float current_b4 = frames[0].b4;
    float current_b5 = frames[0].b5;
    float current_g1 = frames[0].g1;
    float current_g2 = frames[0].g2;
    float current_g3 = frames[0].g3;
    float current_g4 = frames[0].g4;
    float current_g5 = frames[0].g5;
    float current_vg = 0.0f;
    float current_ng = 0.0f;
    float current_pitch = frames[0].pitch_hz * v.pitch_scale;

    float phase = 0.0f;
    float prev_glottal = 0.0f;
    float prev_voiced = 0.0f;
    float prev_noise = 0.0f;
    int out_idx = 0;

    for (int fi = 0; fi < num_frames && out_idx < max_samples; fi++) {
        const sk_phoneme_frame_t *fr = &frames[fi];
        int frame_samples = (int)(fr->duration_ms * 0.001f * sample_rate / v.speed_scale);
        if (frame_samples < 1) frame_samples = 1;

        int is_stop = (fr->flags & SK_PHO_STOP) != 0;
        int closure_samples = is_stop ? (int)(frame_samples * 0.70f) : 0;
        int burst_samples = frame_samples - closure_samples;

        /* Sentence declination: natural gentle pitch drop over phrase */
        float sentence_declination = v.pitch_declination * (1.0f - (float)fi / (float)num_frames);

        /* Update noise filter center frequency smoothly without zeroing delay registers */
        if (fr->noise_freq > 200.0f) {
            float n_freq = fr->noise_freq * v.formant_scale;
            if (n_freq > sample_rate * 0.45f) n_freq = sample_rate * 0.45f;
            float q = n_freq / (fr->noise_bw > 50.0f ? fr->noise_bw : 50.0f);
            if (q < 0.5f) q = 0.5f;
            if (q > 8.0f) q = 8.0f;
            sk_biquad_set(&noise_filter, SK_BIQUAD_BANDPASS, n_freq, q, sample_rate);
        }

        /* Coarticulation slew coefficient: ~8ms fast natural transition time */
        float slew = 1.0f - expf(-1.0f / (0.008f * sample_rate));

        /* Target formants scaled by vocal tract length */
        float target_f1 = fr->f1 * v.formant_scale;
        float target_f2 = fr->f2 * v.formant_scale;
        float target_f3 = fr->f3 * v.formant_scale;
        float target_f4 = fr->f4 * v.formant_scale;
        float target_f5 = (fr->f5 > 1000.0f ? fr->f5 : 4500.0f) * v.formant_scale;

        for (int i = 0; i < frame_samples && out_idx < max_samples; i++) {
            float progress = (float)i / (float)frame_samples;

            /* Compute pitch target with optional inflection glide and declination */
            float target_pitch = fr->pitch_hz;
            if (fr->pitch_end_hz > 20.0f && fr->pitch_end_hz != fr->pitch_hz) {
                target_pitch = sk_lerp(fr->pitch_hz, fr->pitch_end_hz, progress);
            }
            target_pitch = (target_pitch * v.pitch_scale) + sentence_declination;

            /* Apply vibrato if enabled */
            if (fr->vibrato_depth > 0.0f) {
                float t = (float)out_idx / sample_rate;
                float vib = sinf(2.0f * (float)M_PI * fr->vibrato_rate * t) * fr->vibrato_depth;
                target_pitch *= (1.0f + vib);
            }

            /* Slew formant frequencies, bandwidths, gains, and pitch */
            current_f1 += slew * (target_f1 - current_f1);
            current_f2 += slew * (target_f2 - current_f2);
            current_f3 += slew * (target_f3 - current_f3);
            current_f4 += slew * (target_f4 - current_f4);
            current_f5 += slew * (target_f5 - current_f5);

            current_b1 += slew * (fr->b1 - current_b1);
            current_b2 += slew * (fr->b2 - current_b2);
            current_b3 += slew * (fr->b3 - current_b3);
            current_b4 += slew * (fr->b4 - current_b4);
            current_b5 += slew * ((fr->b5 > 50.0f ? fr->b5 : 300.0f) - current_b5);

            current_g1 += slew * (fr->g1 - current_g1);
            current_g2 += slew * (fr->g2 - current_g2);
            current_g3 += slew * (fr->g3 - current_g3);
            current_g4 += slew * (fr->g4 - current_g4);
            current_g5 += slew * ((fr->g5 > 0.01f ? fr->g5 : 0.25f) - current_g5);

            current_pitch += slew * (target_pitch - current_pitch);

            float target_vg = fr->voice_gain;
            float target_ng = fr->noise_gain;

            /* Stop consonant handling: silence during closure, snappy exponential attack on burst release */
            if (is_stop) {
                if (i < closure_samples) {
                    target_ng = 0.0f;
                    if (!(fr->flags & SK_PHO_VOICED)) target_vg = 0.0f;
                    current_ng = 0.0f;
                } else {
                    float burst_frac = burst_samples > 0 ? (float)(i - closure_samples) / (float)burst_samples : 1.0f;
                    float burst_env = expf(-burst_frac * 6.0f); /* Fast exponential transient decay */
                    target_ng = fr->noise_gain * 1.6f * burst_env;
                    current_ng = target_ng; /* Instantaneous attack pop */
                }
            } else {
                current_ng += slew * (target_ng - current_ng);
            }

            current_vg += slew * (target_vg - current_vg);

            /* Update formant resonators with continuously slewed parameters */
            sk_formant_resonator_set(&fb.formants[0], current_f1, current_b1, current_g1, sample_rate);
            sk_formant_resonator_set(&fb.formants[1], current_f2, current_b2, current_g2, sample_rate);
            sk_formant_resonator_set(&fb.formants[2], current_f3, current_b3, current_g3, sample_rate);
            sk_formant_resonator_set(&fb.formants[3], current_f4, current_b4, current_g4, sample_rate);
            sk_formant_resonator_set(&fb.formants[4], current_f5, current_b5, current_g5, sample_rate);

            /* Glottal / carrier excitation */
            float phase_inc = current_pitch / sample_rate;
            phase += phase_inc;
            if (phase >= 1.0f) phase -= 1.0f;

            float glottal = sk_glottal_pulse(phase, 0.65f);

            /* Kraftwerk / Speak & Spell robotic impulse buzz */
            if (v.robot_buzz > 0.0f) {
                float buzz = 0.0f;
                if (phase < 0.08f) {
                    buzz = 1.0f - (phase / 0.08f);
                } else if (phase < 0.14f) {
                    buzz = -0.4f * (1.0f - (phase - 0.08f) / 0.06f);
                }
                glottal = sk_lerp(glottal, buzz, v.robot_buzz);
            }
            glottal *= current_vg;

            /* Random noise generator */
            float raw_noise = rng ? sk_rng_float(rng) : (((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f);

            /* Add glottal breathiness if voice has aspiration */
            if (v.breathiness > 0.0f && current_vg > 0.0f) {
                glottal += raw_noise * v.breathiness * current_vg;
            }

            /* Differentiate glottal volume velocity: U'(t) physically excites acoustic resonances with ZERO DC */
            float glottal_diff = (glottal - prev_glottal);
            prev_glottal = glottal;

            /* Direct aspiration coupling for "HH" into formant bank */
            float formant_in = glottal_diff * 26.0f;
            if (fr->flags & SK_PHO_ASP) {
                formant_in += raw_noise * current_ng * 0.35f;
            }

            /* Resonate glottal flow derivative through 5-pole DC-rejecting formant bank */
            float voiced_raw = sk_formant_bank_step(&fb, formant_in);

            /* Acoustic lip radiation pre-emphasis filter (+6 dB/octave presence shelf) */
            float lip_rad = (voiced_raw - prev_voiced);
            prev_voiced = voiced_raw;
            float voiced = (voiced_raw * 0.70f + lip_rad * 3.5f) * v.vowel_level;

            /* Shaped noise for consonants: tuned bandpass + high-pass air shelf for crisp, natural sibilance without harsh hiss */
            float consonant_noise = 0.0f;
            if (fr->flags & (SK_PHO_FRIC | SK_PHO_STOP)) {
                float bp_noise = sk_biquad_step(&noise_filter, raw_noise);
                float hp_noise = (raw_noise - prev_noise) * 0.5f;
                consonant_noise = (bp_noise * 0.88f + hp_noise * 0.12f) * current_ng * 0.16f * v.consonant_level;
            }
            prev_noise = raw_noise;

            /* Mix articulate resonant vowels + lip presence + crisp consonants */
            float sample = voiced + consonant_noise;

            /* Soft-clip to prevent clipping */
            out_audio[out_idx++] = sk_softclip_cubic(sample, 1.0f);
        }
    }

    return out_idx;
}

/* Standalone renderer: renders compiled phoneme frames into PCM float audio buffer with default voice */
static inline int sk_phoneme_render_frames(const sk_phoneme_frame_t *frames, int num_frames,
                                          float *out_audio, int max_samples, float sample_rate, sk_rng_t *rng) {
    return sk_phoneme_render_frames_voice(frames, num_frames, out_audio, max_samples, sample_rate, NULL, rng);
}

/* =========================================================================
 * 12. Real-Time Percussion Synthesizers (Analog 808/909 & Modal Idiophones)
 * =========================================================================
 *
 * Real-time, zero-allocation, sample-accurate percussion synthesis engines.
 * Each voice implements the physical and circuit models of classic acoustic
 * and electronic drums:
 *
 * 1. KICK DRUM (sk_drum_kick_t):
 *    - Bridged-T / twin-T resonant circuit model with exponential pitch envelope
 *      sweep (f_start -> f_end), beater transient click, and cubic soft-clip drive.
 *
 * 2. SNARE DRUM (sk_drum_snare_t):
 *    - Dual-mode membrane body (two detuned sine modes producing physical acoustic
 *      beating) coupled with bandpass-filtered noise for bottom snare wires and stick snap.
 *
 * 3. HI-HAT (sk_drum_hat_t):
 *    - 6-oscillator inharmonic metallic square wave cluster (TR-808/606 circuit)
 *      passed through a high-Q bandpass/highpass filter with closed/open decay
 *      and physical pedal choke capability.
 *
 * 4. HANDCLAP (sk_drum_clap_t):
 *    - Multi-burst transient sequence (3 sharp bursts spaced ~10ms apart to model
 *      multiple hands striking) followed by a diffuse filtered reverberant tail.
 *
 * 5. TOM DRUM (sk_drum_tom_t):
 *    - Resonant circular membrane (fundamental f0 + Bessel circular mode 1.59*f0)
 *      with dynamic pitch drop on impact.
 *
 * 6. COWBELL (sk_drum_cowbell_t):
 *    - Dual square waves at TR-808 frequencies (540 Hz and 800 Hz) bandpassed
 *      at 800 Hz with snappy punch.
 *
 * 7. CYMBALS - CRASH & RIDE (sk_drum_cymbal_t):
 *    - TR-808 6-oscillator square cluster split into multi-band body, high sizzle,
 *      and shaped noise wash, plus modal bell ping for ride cymbals.
 *
 * 8. RIMSHOT (sk_drum_rimshot_t):
 *    - TR-808 twin-T high-Q resonant bandpass circuits excited by narrow impulse
 *      with highpass stick strike click.
 *
 * 9. CLAVES (sk_drum_clave_t):
 *    - TR-808 resonant twin-T circuit (~2450 Hz) with short woody exponential decay.
 *
 * 10. MARACAS (sk_drum_maraca_t):
 *    - Shaped high-frequency noise bursts with linear bead acceleration attack.
 *
 * 11. CONGA / BONGO (sk_drum_conga_t):
 *    - Tunable resonant dual modes with skin pitch-drop and hand slap transient.
 *
 * 12. SIMMONS SDS-V SYNTH TOM (sk_drum_simmons_t):
 *    - 1980s hexagonal disco/synth tom with wide quadratic pitch drop, noise rattle, and click.
 *
 * 13. ANALOG ZAP / LASER (sk_drum_zap_t):
 *    - Space zap / disco laser with exponential frequency sweep and saturation.
 * ========================================================================= */

/* -------------------------------------------------------------------------
 * 12.1. Analog Kick Drum Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float phase;
    float pitch_env;            /* Decays exponentially from 1.0 to 0.0 */
    float pitch_decay_coeff;    /* e^(-1 / (pitch_decay_sec * sr)) */
    float amp_env;              /* Decays exponentially from 1.0 to 0.0 */
    float amp_decay_coeff;      /* e^(-1 / (amp_decay_sec * sr)) */
    float click_env;            /* Fast transient click envelope */
    float click_decay_coeff;
    float start_freq;           /* Punch/attack pitch in Hz (e.g. 150 Hz) */
    float end_freq;             /* Settled sub-bass pitch in Hz (e.g. 48 Hz) */
    float click_level;          /* Transient beater click amplitude [0.0, 1.0] */
    float drive;                /* Non-linear cubic drive [1.0 = clean, >1.0 = punchy/saturated] */
} sk_drum_kick_t;

/* Initialize kick drum with default 808/909 tuning and decay characteristics */
static inline void sk_drum_kick_init(sk_drum_kick_t *k, float sample_rate) {
    if (!k) return;
    memset(k, 0, sizeof(*k));
    k->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    k->start_freq = 150.0f;
    k->end_freq = 48.0f;
    k->click_level = 0.30f;
    k->drive = 1.25f;

    /* Pitch decay ~35 ms */
    k->pitch_decay_coeff = expf(-1.0f / (0.035f * k->sample_rate));
    /* Amplitude decay ~350 ms */
    k->amp_decay_coeff = expf(-1.0f / (0.350f * k->sample_rate));
    /* Click decay ~4 ms */
    k->click_decay_coeff = expf(-1.0f / (0.004f * k->sample_rate));
}

/* Trigger kick drum with strike velocity [0.0, 1.0] */
static inline void sk_drum_kick_trigger(sk_drum_kick_t *k, float velocity) {
    if (!k) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    k->pitch_env = 1.0f;
    k->amp_env = velocity;
    k->click_env = velocity * k->click_level;
    k->phase = 0.0f;
}

/* Step kick drum by 1 sample */
static inline float sk_drum_kick_step(sk_drum_kick_t *k) {
    if (!k || k->amp_env < 1e-4f) return 0.0f;

    /* Instantaneous frequency = end_freq + (start_freq - end_freq) * pitch_env */
    float cur_freq = k->end_freq + (k->start_freq - k->end_freq) * k->pitch_env;
    float phase_inc = cur_freq / k->sample_rate;
    k->phase += phase_inc;
    if (k->phase >= 1.0f) k->phase -= 1.0f;

    /* Body sine wave */
    float body = sinf(k->phase * 2.0f * (float)M_PI) * k->amp_env;

    /* Click transient: short burst with high frequency click tone */
    float click = sinf(k->phase * 16.0f * (float)M_PI) * k->click_env;

    /* Decay envelopes */
    k->pitch_env *= k->pitch_decay_coeff;
    k->amp_env *= k->amp_decay_coeff;
    k->click_env *= k->click_decay_coeff;

    /* Combine body and click, pass through soft-clip saturation */
    float mixed = body + click;
    return sk_softclip_cubic(mixed * k->drive, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.2. Analog Snare Drum Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float phase1, phase2;
    float body_env;
    float body_decay_coeff;
    float noise_env;
    float noise_decay_coeff;
    float click_env;
    float click_decay_coeff;
    float f1, f2;               /* Dual resonant body modes (e.g. 175 Hz and 224 Hz) */
    float snappy_mix;           /* Mix between body (0.0) and snappy noise wires (1.0) */
    sk_biquad_t noise_filter;   /* Bandpass filter for snare wires */
    sk_rng_t rng;
} sk_drum_snare_t;

/* Initialize snare drum */
static inline void sk_drum_snare_init(sk_drum_snare_t *s, float sample_rate) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    s->f1 = 175.0f;
    s->f2 = 224.0f;
    s->snappy_mix = 0.55f;

    /* Body decay ~85 ms */
    s->body_decay_coeff = expf(-1.0f / (0.085f * s->sample_rate));
    /* Snare wire noise decay ~200 ms */
    s->noise_decay_coeff = expf(-1.0f / (0.200f * s->sample_rate));
    /* Impact snap click decay ~3 ms */
    s->click_decay_coeff = expf(-1.0f / (0.003f * s->sample_rate));

    /* Snare rattle bandpass: center 2800 Hz, Q=1.5 */
    sk_biquad_init(&s->noise_filter, SK_BIQUAD_BANDPASS, 2800.0f, 1.5f, s->sample_rate);
    sk_rng_init(&s->rng, 987654321ULL);
}

/* Trigger snare drum */
static inline void sk_drum_snare_trigger(sk_drum_snare_t *s, float velocity) {
    if (!s) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    s->body_env = velocity;
    s->noise_env = velocity;
    s->click_env = velocity * 0.4f;
    s->phase1 = 0.0f;
    s->phase2 = 0.0f;
}

/* Step snare drum by 1 sample */
static inline float sk_drum_snare_step(sk_drum_snare_t *s) {
    if (!s || (s->body_env < 1e-4f && s->noise_env < 1e-4f)) return 0.0f;

    /* Dual sine body modes with detuning */
    s->phase1 += s->f1 / s->sample_rate;
    if (s->phase1 >= 1.0f) s->phase1 -= 1.0f;

    s->phase2 += s->f2 / s->sample_rate;
    if (s->phase2 >= 1.0f) s->phase2 -= 1.0f;

    float body = (sinf(s->phase1 * 2.0f * (float)M_PI) +
                  sinf(s->phase2 * 2.0f * (float)M_PI)) * 0.5f * s->body_env;

    /* Filtered noise for snare rattle */
    float raw_noise = sk_rng_float(&s->rng);
    float filtered_noise = sk_biquad_step(&s->noise_filter, raw_noise) * s->noise_env;

    /* Stick strike click */
    float click = raw_noise * s->click_env;

    /* Decay envelopes */
    s->body_env *= s->body_decay_coeff;
    s->noise_env *= s->noise_decay_coeff;
    s->click_env *= s->click_decay_coeff;

    float out = (1.0f - s->snappy_mix) * body + s->snappy_mix * filtered_noise + click;
    return sk_softclip_cubic(out * 1.2f, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.3. Analog Hi-Hat Engine (Closed / Open with Choke)
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float phases[6];
    float phase_incs[6];
    float amp_env;
    float amp_decay_coeff;
    float choke_coeff;
    int is_open;
    sk_biquad_t bandpass;
    sk_biquad_t highpass;
} sk_drum_hat_t;

/* Initialize hi-hat with 6-oscillator cluster and 808 bandpass/highpass filter */
static inline void sk_drum_hat_init(sk_drum_hat_t *h, float sample_rate) {
    if (!h) return;
    memset(h, 0, sizeof(*h));
    h->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;

    /* TR-808 exact metallic cluster frequencies: 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 Hz */
    static const float freqs[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    for (int i = 0; i < 6; i++) {
        h->phase_incs[i] = freqs[i] / h->sample_rate;
    }

    /* Fast choke coefficient (~5 ms clamp) */
    h->choke_coeff = expf(-1.0f / (0.005f * h->sample_rate));

    /* Filter chain: 7500 Hz bandpass + 7000 Hz highpass for brilliant sizzle */
    sk_biquad_init(&h->bandpass, SK_BIQUAD_BANDPASS, 7500.0f, 2.0f, h->sample_rate);
    sk_biquad_init(&h->highpass, SK_BIQUAD_HIGHPASS, 7000.0f, 0.707f, h->sample_rate);
}

/* Trigger hi-hat: open = 0 for closed hat (~50 ms decay), open = 1 for open hat (~350 ms decay) */
static inline void sk_drum_hat_trigger(sk_drum_hat_t *h, float velocity, int open) {
    if (!h) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    h->is_open = open;
    h->amp_env = velocity;

    float decay_sec = open ? 0.350f : 0.050f;
    h->amp_decay_coeff = expf(-1.0f / (decay_sec * h->sample_rate));
}

/* Choke hi-hat (e.g. closed hat hit immediately clamps an open hat ring) */
static inline void sk_drum_hat_choke(sk_drum_hat_t *h) {
    if (!h) return;
    h->amp_decay_coeff = h->choke_coeff;
    h->is_open = 0;
}

/* Step hi-hat by 1 sample */
static inline float sk_drum_hat_step(sk_drum_hat_t *h) {
    if (!h || h->amp_env < 1e-4f) return 0.0f;

    /* Sum of 6 square wave oscillators: sgn(sin(theta)) */
    float cluster = 0.0f;
    for (int i = 0; i < 6; i++) {
        h->phases[i] += h->phase_incs[i];
        if (h->phases[i] >= 1.0f) h->phases[i] -= 1.0f;
        cluster += (h->phases[i] < 0.5f) ? 0.1666f : -0.1666f;
    }

    /* Dual filter stage: bandpass then highpass */
    float bp = sk_biquad_step(&h->bandpass, cluster);
    float hp = sk_biquad_step(&h->highpass, bp);

    /* Apply amplitude envelope */
    float out = hp * h->amp_env * 2.2f;
    h->amp_env *= h->amp_decay_coeff;

    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.4. Analog Handclap Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    int sample_counter;
    int burst_spacing;
    int burst_count;
    float burst_env;
    float tail_env;
    float tail_decay_coeff;
    sk_biquad_t filter;
    sk_rng_t rng;
} sk_drum_clap_t;

/* Initialize 808-style handclap with 3 multi-bursts + reverberant tail */
static inline void sk_drum_clap_init(sk_drum_clap_t *c, float sample_rate) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    c->burst_spacing = (int)(0.011f * c->sample_rate); /* 11 ms between hand claps */
    c->tail_decay_coeff = expf(-1.0f / (0.220f * c->sample_rate)); /* 220 ms tail */

    /* 1100 Hz bandpass filter (Q=1.2) */
    sk_biquad_init(&c->filter, SK_BIQUAD_BANDPASS, 1100.0f, 1.2f, c->sample_rate);
    sk_rng_init(&c->rng, 5551212ULL);
}

/* Trigger handclap */
static inline void sk_drum_clap_trigger(sk_drum_clap_t *c, float velocity) {
    if (!c) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    c->sample_counter = 0;
    c->burst_count = 0;
    c->burst_env = velocity;
    c->tail_env = 0.0f;
}

/* Step handclap by 1 sample */
static inline float sk_drum_clap_step(sk_drum_clap_t *c) {
    if (!c || (c->burst_env < 1e-4f && c->tail_env < 1e-4f && c->burst_count >= 3)) return 0.0f;

    /* Trigger pre-bursts (0 ms, 11 ms, 22 ms) */
    if (c->burst_count < 3) {
        if (c->sample_counter % c->burst_spacing == 0) {
            c->burst_env = 1.0f;
            c->burst_count++;
            if (c->burst_count == 3) {
                c->tail_env = 1.0f; /* Start tail after 3rd burst */
            }
        }
    }
    c->sample_counter++;

    /* Fast burst decay (~3 ms) */
    c->burst_env *= 0.992f;
    /* Longer diffused tail decay */
    c->tail_env *= c->tail_decay_coeff;

    float raw_noise = sk_rng_float(&c->rng);
    float env = c->burst_env + c->tail_env * 0.85f;
    float filtered = sk_biquad_step(&c->filter, raw_noise * env);

    return sk_softclip_cubic(filtered * 1.8f, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.5. Resonant Tom Drum Engine (Membrane Modes & Pitch Drop)
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float phase1, phase2;
    float pitch_env;
    float pitch_decay_coeff;
    float amp_env;
    float amp_decay_coeff;
    float tune_freq;
} sk_drum_tom_t;

/* Initialize resonant tom with fundamental tuning (e.g. low=80 Hz, mid=130 Hz, hi=200 Hz) */
static inline void sk_drum_tom_init(sk_drum_tom_t *t, float sample_rate, float tune_freq) {
    if (!t) return;
    memset(t, 0, sizeof(*t));
    t->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    t->tune_freq = tune_freq > 20.0f ? tune_freq : 100.0f;

    /* Pitch drop decay ~45 ms */
    t->pitch_decay_coeff = expf(-1.0f / (0.045f * t->sample_rate));
    /* Amplitude decay ~260 ms */
    t->amp_decay_coeff = expf(-1.0f / (0.260f * t->sample_rate));
}

/* Trigger tom */
static inline void sk_drum_tom_trigger(sk_drum_tom_t *t, float velocity) {
    if (!t) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    t->pitch_env = 1.0f;
    t->amp_env = velocity;
    t->phase1 = 0.0f;
    t->phase2 = 0.0f;
}

/* Step tom by 1 sample */
static inline float sk_drum_tom_step(sk_drum_tom_t *t) {
    if (!t || t->amp_env < 1e-4f) return 0.0f;

    /* Pitch drops from 1.4*tune_freq to tune_freq */
    float f0 = t->tune_freq * (1.0f + 0.40f * t->pitch_env);
    /* Secondary circular membrane mode (Bessel zero ratio ~1.59) */
    float f1 = f0 * 1.59f;

    t->phase1 += f0 / t->sample_rate;
    if (t->phase1 >= 1.0f) t->phase1 -= 1.0f;

    t->phase2 += f1 / t->sample_rate;
    if (t->phase2 >= 1.0f) t->phase2 -= 1.0f;

    float body = (sinf(t->phase1 * 2.0f * (float)M_PI) * 0.75f +
                  sinf(t->phase2 * 2.0f * (float)M_PI) * 0.25f) * t->amp_env;

    t->pitch_env *= t->pitch_decay_coeff;
    t->amp_env *= t->amp_decay_coeff;

    return sk_softclip_cubic(body * 1.2f, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.6. TR-808 Dual Square-Wave Cowbell Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float phase1, phase2;
    float inc1, inc2;
    float amp_env;
    float amp_decay_coeff;
    sk_biquad_t filter;
} sk_drum_cowbell_t;

/* Initialize 808 cowbell with 540 Hz and 800 Hz square wave oscillators and 800 Hz bandpass */
static inline void sk_drum_cowbell_init(sk_drum_cowbell_t *c, float sample_rate) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;

    /* TR-808 cowbell dual square oscillators */
    c->inc1 = 540.0f / c->sample_rate;
    c->inc2 = 800.0f / c->sample_rate;

    /* Envelope decay ~160 ms */
    c->amp_decay_coeff = expf(-1.0f / (0.160f * c->sample_rate));

    /* Bandpass resonator at 800 Hz, Q=2.2 */
    sk_biquad_init(&c->filter, SK_BIQUAD_BANDPASS, 800.0f, 2.2f, c->sample_rate);
}

/* Trigger cowbell */
static inline void sk_drum_cowbell_trigger(sk_drum_cowbell_t *c, float velocity) {
    if (!c) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    c->amp_env = velocity;
    c->phase1 = 0.0f;
    c->phase2 = 0.0f;
}

/* Step cowbell by 1 sample */
static inline float sk_drum_cowbell_step(sk_drum_cowbell_t *c) {
    if (!c || c->amp_env < 1e-4f) return 0.0f;

    c->phase1 += c->inc1;
    if (c->phase1 >= 1.0f) c->phase1 -= 1.0f;

    c->phase2 += c->inc2;
    if (c->phase2 >= 1.0f) c->phase2 -= 1.0f;

    /* Dual square waves: sgn(sin) */
    float sq1 = (c->phase1 < 0.5f) ? 0.5f : -0.5f;
    float sq2 = (c->phase2 < 0.5f) ? 0.5f : -0.5f;

    float filtered = sk_biquad_step(&c->filter, (sq1 + sq2) * 0.5f);
    float out = filtered * c->amp_env * 2.5f;

    c->amp_env *= c->amp_decay_coeff;
    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.7. Analog Crash & Ride Cymbal Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    int is_ride;
    float phases[6];
    float phase_incs[6];
    float amp_env_body;
    float amp_decay_body;
    float amp_env_sizzle;
    float amp_decay_sizzle;
    float amp_env_wash;
    float amp_decay_wash;
    float tone;                 /* 0.0 = dark body, 1.0 = bright shimmer */
    float sizzle;               /* noise wash mix [0.0, 1.0] */
    float ping_env;             /* ride bell ping envelope */
    float ping_decay;
    float ping_phase1, ping_phase2;
    float ping_inc1, ping_inc2;
    sk_biquad_t bp_body;
    sk_biquad_t bp_sizzle;
    sk_biquad_t hp_wash;
    sk_rng_t rng;
} sk_drum_cymbal_t;

static inline void sk_drum_cymbal_init(sk_drum_cymbal_t *c, float sample_rate, int is_ride) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    c->is_ride = is_ride;
    c->tone = 0.5f;
    c->sizzle = is_ride ? 0.35f : 0.65f;

    /* Base 6-oscillator metallic cluster frequencies (TR-808/606):
     * 205.3, 304.4, 369.6, 522.7, 540.0, 800.0 Hz */
    float scale = is_ride ? 1.25f : 1.0f;
    static const float base_f[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
    for (int i = 0; i < 6; i++) {
        c->phase_incs[i] = (base_f[i] * scale) / c->sample_rate;
    }

    /* Ride bell ping oscillators (587.33 Hz D5 & 845 Hz bell overtone) */
    c->ping_inc1 = 587.33f / c->sample_rate;
    c->ping_inc2 = 845.0f / c->sample_rate;

    /* Multi-band filter network */
    sk_biquad_init(&c->bp_body, SK_BIQUAD_BANDPASS, is_ride ? 4200.0f : 3400.0f, 2.2f, c->sample_rate);
    sk_biquad_init(&c->bp_sizzle, SK_BIQUAD_BANDPASS, is_ride ? 8200.0f : 7200.0f, 1.8f, c->sample_rate);
    sk_biquad_init(&c->hp_wash, SK_BIQUAD_HIGHPASS, 6500.0f, 0.707f, c->sample_rate);
    sk_rng_init(&c->rng, 77712399ULL);

    float body_t = is_ride ? 0.75f : 1.20f;
    float sizzle_t = is_ride ? 1.40f : 2.20f;
    float wash_t = is_ride ? 0.90f : 1.60f;
    float ping_t = 0.45f;

    c->amp_decay_body = expf(-1.0f / (body_t * c->sample_rate));
    c->amp_decay_sizzle = expf(-1.0f / (sizzle_t * c->sample_rate));
    c->amp_decay_wash = expf(-1.0f / (wash_t * c->sample_rate));
    c->ping_decay = expf(-1.0f / (ping_t * c->sample_rate));
}

static inline void sk_drum_cymbal_trigger(sk_drum_cymbal_t *c, float velocity) {
    if (!c) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    c->amp_env_body = velocity;
    c->amp_env_sizzle = velocity;
    c->amp_env_wash = velocity;
    c->ping_env = c->is_ride ? velocity : 0.0f;
}

static inline float sk_drum_cymbal_step(sk_drum_cymbal_t *c) {
    if (!c || (c->amp_env_body < 1e-4f && c->amp_env_sizzle < 1e-4f && c->amp_env_wash < 1e-4f && c->ping_env < 1e-4f)) {
        return 0.0f;
    }

    /* 6 square wave cluster */
    float cluster = 0.0f;
    for (int i = 0; i < 6; i++) {
        c->phases[i] += c->phase_incs[i];
        if (c->phases[i] >= 1.0f) c->phases[i] -= 1.0f;
        cluster += (c->phases[i] < 0.5f) ? 0.1666f : -0.1666f;
    }

    float body = sk_biquad_step(&c->bp_body, cluster) * c->amp_env_body;
    float sizzle = sk_biquad_step(&c->bp_sizzle, cluster) * c->amp_env_sizzle;

    float raw_noise = sk_rng_float(&c->rng);
    float wash = sk_biquad_step(&c->hp_wash, raw_noise) * c->amp_env_wash;

    c->amp_env_body *= c->amp_decay_body;
    c->amp_env_sizzle *= c->amp_decay_sizzle;
    c->amp_env_wash *= c->amp_decay_wash;

    float metallic = body * (1.0f - c->tone * 0.6f) + sizzle * (0.4f + c->tone * 0.8f);
    float out = metallic * 1.8f + wash * c->sizzle * 1.4f;

    if (c->is_ride && c->ping_env > 1e-4f) {
        c->ping_phase1 += c->ping_inc1;
        if (c->ping_phase1 >= 1.0f) c->ping_phase1 -= 1.0f;
        c->ping_phase2 += c->ping_inc2;
        if (c->ping_phase2 >= 1.0f) c->ping_phase2 -= 1.0f;

        float ping_sig = (sinf(c->ping_phase1 * 2.0f * (float)M_PI) * 0.65f +
                          sinf(c->ping_phase2 * 2.0f * (float)M_PI) * 0.35f) * c->ping_env;
        c->ping_env *= c->ping_decay;
        out += ping_sig * 1.2f;
    }

    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.8. TR-808 Resonant Rimshot Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float res_freq;
    float decay_sec;
    float click_level;
    sk_biquad_t shell_bp;       /* ~1750 Hz, Q=7.0 */
    sk_biquad_t ping_bp;        /* ~4400 Hz, Q=5.0 */
    sk_biquad_t click_hp;       /* ~3200 Hz */
    float env_shell;
    float decay_shell;
    float env_ping;
    float decay_ping;
    float env_click;
    float decay_click;
    sk_rng_t rng;
} sk_drum_rimshot_t;

static inline void sk_drum_rimshot_init(sk_drum_rimshot_t *r, float sample_rate, float res_freq) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    r->res_freq = res_freq > 200.0f ? res_freq : 1750.0f;
    r->decay_sec = 0.038f;
    r->click_level = 0.50f;

    sk_biquad_init(&r->shell_bp, SK_BIQUAD_BANDPASS, r->res_freq, 7.0f, r->sample_rate);
    sk_biquad_init(&r->ping_bp, SK_BIQUAD_BANDPASS, r->res_freq * 2.51f, 5.0f, r->sample_rate);
    sk_biquad_init(&r->click_hp, SK_BIQUAD_HIGHPASS, 3200.0f, 0.707f, r->sample_rate);
    sk_rng_init(&r->rng, 10293847ULL);

    r->decay_shell = expf(-1.0f / (r->decay_sec * r->sample_rate));
    r->decay_ping = expf(-1.0f / (0.012f * r->sample_rate));
    r->decay_click = expf(-1.0f / (0.007f * r->sample_rate));
}

static inline void sk_drum_rimshot_trigger(sk_drum_rimshot_t *r, float velocity) {
    if (!r) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    r->env_shell = velocity;
    r->env_ping = velocity * 0.7f;
    r->env_click = velocity;
}

static inline float sk_drum_rimshot_step(sk_drum_rimshot_t *r) {
    if (!r || (r->env_shell < 1e-4f && r->env_ping < 1e-4f && r->env_click < 1e-4f)) return 0.0f;

    /* Impulse excitations into high-Q resonators */
    float imp = (r->env_shell > 0.95f) ? 1.0f : 0.0f;
    float shell = sk_biquad_step(&r->shell_bp, imp + r->env_shell * 0.15f) * r->env_shell;
    float ping = sk_biquad_step(&r->ping_bp, imp) * r->env_ping;

    float raw_noise = sk_rng_float(&r->rng);
    float click = sk_biquad_step(&r->click_hp, raw_noise) * r->env_click * r->click_level;

    r->env_shell *= r->decay_shell;
    r->env_ping *= r->decay_ping;
    r->env_click *= r->decay_click;

    float out = (shell * 2.2f + ping * 1.5f + click * 1.8f);
    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.9. TR-808 Bridged-T Claves Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float freq;
    float decay_sec;
    sk_biquad_t filter;         /* High-Q Twin-T bandpass ~2450 Hz, Q=12.0 */
    float env;
    float decay_coeff;
    int triggered;
} sk_drum_clave_t;

static inline void sk_drum_clave_init(sk_drum_clave_t *c, float sample_rate, float freq) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    c->freq = freq > 100.0f ? freq : 2450.0f;
    c->decay_sec = 0.040f;

    sk_biquad_init(&c->filter, SK_BIQUAD_BANDPASS, c->freq, 12.0f, c->sample_rate);
    c->decay_coeff = expf(-1.0f / (c->decay_sec * c->sample_rate));
}

static inline void sk_drum_clave_trigger(sk_drum_clave_t *c, float velocity) {
    if (!c) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    c->env = velocity;
    c->triggered = 1;
}

static inline float sk_drum_clave_step(sk_drum_clave_t *c) {
    if (!c || c->env < 1e-4f) return 0.0f;

    float imp = c->triggered ? 1.0f : 0.0f;
    c->triggered = 0;

    float sig = sk_biquad_step(&c->filter, imp) * c->env;
    c->env *= c->decay_coeff;

    return sk_softclip_cubic(sig * 4.5f, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.10. TR-808 Maracas / Shaker Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float freq;
    float attack_sec;
    float decay_sec;
    float env;
    float attack_step;
    float decay_coeff;
    int state;                  /* 0 = idle, 1 = attack, 2 = decay */
    sk_biquad_t hp_filter;
    sk_biquad_t bp_filter;
    sk_rng_t rng;
} sk_drum_maraca_t;

static inline void sk_drum_maraca_init(sk_drum_maraca_t *m, float sample_rate, float freq) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    m->freq = freq > 1000.0f ? freq : 7500.0f;
    m->attack_sec = 0.005f;
    m->decay_sec = 0.045f;

    sk_biquad_init(&m->hp_filter, SK_BIQUAD_HIGHPASS, 6000.0f, 0.707f, m->sample_rate);
    sk_biquad_init(&m->bp_filter, SK_BIQUAD_BANDPASS, m->freq, 2.0f, m->sample_rate);
    sk_rng_init(&m->rng, 98765432ULL);

    int attack_samples = (int)(m->attack_sec * m->sample_rate);
    if (attack_samples < 1) attack_samples = 1;
    m->attack_step = 1.0f / (float)attack_samples;
    m->decay_coeff = expf(-1.0f / (m->decay_sec * m->sample_rate));
}

static inline void sk_drum_maraca_trigger(sk_drum_maraca_t *m, float velocity) {
    if (!m) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    m->env = 0.0f;
    m->state = 1; /* attack phase */
}

static inline float sk_drum_maraca_step(sk_drum_maraca_t *m) {
    if (!m || m->state == 0) return 0.0f;

    if (m->state == 1) {
        m->env += m->attack_step;
        if (m->env >= 1.0f) {
            m->env = 1.0f;
            m->state = 2; /* decay phase */
        }
    } else if (m->state == 2) {
        m->env *= m->decay_coeff;
        if (m->env < 1e-4f) {
            m->env = 0.0f;
            m->state = 0;
            return 0.0f;
        }
    }

    float raw_noise = sk_rng_float(&m->rng);
    float hp = sk_biquad_step(&m->hp_filter, raw_noise);
    float bp = sk_biquad_step(&m->bp_filter, hp);

    float out = bp * m->env * 2.8f;
    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.11. TR-808 Tunable Conga / Bongo Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float tune_freq;
    float phase1, phase2;
    float pitch_env;
    float pitch_decay;
    float amp_env;
    float amp_decay;
    float slap_env;
    float slap_decay;
    float slap_mix;
    float drop;
    sk_biquad_t slap_bp;
    sk_rng_t rng;
} sk_drum_conga_t;

static inline void sk_drum_conga_init(sk_drum_conga_t *c, float sample_rate, float tune_freq) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    c->tune_freq = tune_freq > 30.0f ? tune_freq : 220.0f;
    c->drop = 0.25f;
    c->slap_mix = 0.35f;

    c->pitch_decay = expf(-1.0f / (0.016f * c->sample_rate));
    c->amp_decay = expf(-1.0f / (0.220f * c->sample_rate));
    c->slap_decay = expf(-1.0f / (0.010f * c->sample_rate));

    sk_biquad_init(&c->slap_bp, SK_BIQUAD_BANDPASS, 2600.0f, 2.5f, c->sample_rate);
    sk_rng_init(&c->rng, 54321098ULL);
}

static inline void sk_drum_conga_trigger(sk_drum_conga_t *c, float velocity) {
    if (!c) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    c->pitch_env = 1.0f;
    c->amp_env = velocity;
    c->slap_env = velocity * c->slap_mix;
    c->phase1 = 0.0f;
    c->phase2 = 0.0f;
}

static inline float sk_drum_conga_step(sk_drum_conga_t *c) {
    if (!c || (c->amp_env < 1e-4f && c->slap_env < 1e-4f)) return 0.0f;

    float f0 = c->tune_freq * (1.0f + c->drop * c->pitch_env);
    float f1 = f0 * 1.58f;

    c->phase1 += f0 / c->sample_rate;
    if (c->phase1 >= 1.0f) c->phase1 -= 1.0f;

    c->phase2 += f1 / c->sample_rate;
    if (c->phase2 >= 1.0f) c->phase2 -= 1.0f;

    float body = (sinf(c->phase1 * 2.0f * (float)M_PI) * 0.80f +
                  sinf(c->phase2 * 2.0f * (float)M_PI) * 0.20f) * c->amp_env;

    float raw_noise = sk_rng_float(&c->rng);
    float slap = sk_biquad_step(&c->slap_bp, raw_noise) * c->slap_env;

    c->pitch_env *= c->pitch_decay;
    c->amp_env *= c->amp_decay;
    c->slap_env *= c->slap_decay;

    float out = body * 1.5f + slap * 1.8f;
    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.12. 1980s Simmons SDS-V Hexagonal Synth Tom Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float start_freq;
    float end_freq;
    float pitch_env;
    float pitch_decay;
    float amp_env;
    float amp_decay;
    float click_env;
    float click_decay;
    float noise_env;
    float noise_decay;
    float noise_mix;
    float click_mix;
    float phase;
    sk_biquad_t noise_filter;
    sk_rng_t rng;
} sk_drum_simmons_t;

static inline void sk_drum_simmons_init(sk_drum_simmons_t *s, float sample_rate) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    s->start_freq = 550.0f;
    s->end_freq = 75.0f;
    s->noise_mix = 0.25f;
    s->click_mix = 0.40f;

    s->pitch_decay = expf(-1.0f / (0.075f * s->sample_rate));
    s->amp_decay = expf(-1.0f / (0.450f * s->sample_rate));
    s->click_decay = expf(-1.0f / (0.005f * s->sample_rate));
    s->noise_decay = expf(-1.0f / (0.120f * s->sample_rate));

    sk_biquad_init(&s->noise_filter, SK_BIQUAD_LOWPASS, 1200.0f, 1.5f, s->sample_rate);
    sk_rng_init(&s->rng, 84729103ULL);
}

static inline void sk_drum_simmons_trigger(sk_drum_simmons_t *s, float velocity) {
    if (!s) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    s->pitch_env = 1.0f;
    s->amp_env = velocity;
    s->click_env = velocity * s->click_mix;
    s->noise_env = velocity * s->noise_mix;
    s->phase = 0.0f;
}

static inline float sk_drum_simmons_step(sk_drum_simmons_t *s) {
    if (!s || (s->amp_env < 1e-4f && s->click_env < 1e-4f && s->noise_env < 1e-4f)) return 0.0f;

    /* Quadratic pitch drop curve: gives punchy synth-disco drop */
    float drop_ratio = s->pitch_env * s->pitch_env;
    float current_f = s->end_freq + (s->start_freq - s->end_freq) * drop_ratio;

    s->phase += current_f / s->sample_rate;
    if (s->phase >= 1.0f) s->phase -= 1.0f;

    /* Mixed sine & triangle wave with saturation */
    float s_val = sinf(s->phase * 2.0f * (float)M_PI);
    float tri_val = (s->phase < 0.5f) ? (4.0f * s->phase - 1.0f) : (3.0f - 4.0f * s->phase);
    float vco = (s_val * 0.65f + tri_val * 0.35f) * s->amp_env;

    /* Resonant swept lowpass noise */
    float raw_noise = sk_rng_float(&s->rng);
    float noise_cutoff = current_f * 2.8f;
    if (noise_cutoff > s->sample_rate * 0.45f) noise_cutoff = s->sample_rate * 0.45f;
    sk_biquad_init(&s->noise_filter, SK_BIQUAD_LOWPASS, noise_cutoff, 1.8f, s->sample_rate);
    float filtered_noise = sk_biquad_step(&s->noise_filter, raw_noise) * s->noise_env;

    /* Stick click */
    float click = (s->phase < 0.5f ? 1.0f : -1.0f) * s->click_env;

    s->pitch_env *= s->pitch_decay;
    s->amp_env *= s->amp_decay;
    s->click_env *= s->click_decay;
    s->noise_env *= s->noise_decay;

    float out = (vco * 1.5f + filtered_noise * 1.4f + click * 0.8f);
    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.13. Analog Space Zap / Disco Laser Engine
 * ------------------------------------------------------------------------- */
typedef struct {
    float sample_rate;
    float start_freq;
    float end_freq;
    float phase;
    float amp_env;
    float amp_decay;
    float t;
    float dt;
    float drive;
} sk_drum_zap_t;

static inline void sk_drum_zap_init(sk_drum_zap_t *z, float sample_rate) {
    if (!z) return;
    memset(z, 0, sizeof(*z));
    z->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    z->start_freq = 3200.0f;
    z->end_freq = 45.0f;
    z->drive = 1.4f;

    float dur = 0.085f;
    z->dt = 1.0f / (dur * z->sample_rate);
    z->amp_decay = expf(-1.0f / (dur * z->sample_rate));
}

static inline void sk_drum_zap_trigger(sk_drum_zap_t *z, float velocity) {
    if (!z) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    z->amp_env = velocity;
    z->t = 0.0f;
    z->phase = 0.0f;
}

static inline float sk_drum_zap_step(sk_drum_zap_t *z) {
    if (!z || z->amp_env < 1e-4f || z->t >= 1.0f) return 0.0f;

    /* Exponential frequency sweep: start_f * (end_f / start_f)^t */
    float current_f = z->start_freq * powf(z->end_freq / z->start_freq, z->t);

    z->phase += current_f / z->sample_rate;
    if (z->phase >= 1.0f) z->phase -= 1.0f;

    float s_val = sinf(z->phase * 2.0f * (float)M_PI);
    float out = s_val * z->amp_env * z->drive;

    z->t += z->dt;
    z->amp_env *= z->amp_decay;

    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.14. Casio VL-Tone VL-1 Drum Engine (sk_drum_vl1_t)
 * -------------------------------------------------------------------------
 * Authentic circuit emulation of the NEC D1867G single-chip microcontroller:
 *   - Type 0 (KICK / "Po"): 1-bit CMOS 50% square wave with rapid exponential
 *     downward pitch chirp (240 Hz -> 52 Hz) and 1-pole RC speaker lowpass roll-off.
 *   - Type 1 (SNARE / "Ka"): High-rate 1-bit shift register pseudo-random noise burst
 *     mixed with a short tonal pop, bandpass filtered at 2.2 kHz.
 *   - Type 2 (HAT / "Chi"): 16 ms micro-slice of highpass-filtered 1-bit noise (7.0 kHz).
 */
typedef enum {
    SK_VL1_KICK = 0,    /* "Po" - 1-bit square chirp */
    SK_VL1_SNARE = 1,   /* "Ka" - 1-bit LFSR noise burst */
    SK_VL1_HAT = 2      /* "Chi" - 1-bit micro highpass tick */
} sk_vl1_type_t;

typedef struct {
    float sample_rate;
    int type;
    float phase;
    float start_freq;
    float end_freq;
    float pitch_env;
    float pitch_decay;
    float amp_env;
    float amp_decay;
    float filter_lp;
    float filter_coeff;
    sk_biquad_t biquad;
    uint32_t lfsr;
} sk_drum_vl1_t;

static inline void sk_drum_vl1_init(sk_drum_vl1_t *v, float sample_rate, int type) {
    if (!v) return;
    memset(v, 0, sizeof(*v));
    v->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    v->type = type;
    v->lfsr = 0x12345;

    if (type == SK_VL1_KICK) {
        v->start_freq = 240.0f;
        v->end_freq = 52.0f;
        v->pitch_decay = expf(-1.0f / (0.038f * v->sample_rate));
        v->amp_decay = expf(-1.0f / (0.090f * v->sample_rate));
        /* Single-pole RC lowpass filter around 450 Hz */
        v->filter_coeff = 1.0f - expf(-2.0f * (float)M_PI * 450.0f / v->sample_rate);
    } else if (type == SK_VL1_SNARE) {
        v->start_freq = 170.0f;
        v->end_freq = 110.0f;
        v->pitch_decay = expf(-1.0f / (0.025f * v->sample_rate));
        v->amp_decay = expf(-1.0f / (0.048f * v->sample_rate));
        sk_biquad_init(&v->biquad, SK_BIQUAD_BANDPASS, 2200.0f, 1.8f, v->sample_rate);
    } else { /* HAT */
        v->amp_decay = expf(-1.0f / (0.018f * v->sample_rate));
        sk_biquad_init(&v->biquad, SK_BIQUAD_HIGHPASS, 7000.0f, 1.4f, v->sample_rate);
    }
}

static inline void sk_drum_vl1_trigger(sk_drum_vl1_t *v, float velocity) {
    if (!v) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    v->amp_env = velocity;
    v->pitch_env = 1.0f;
    v->phase = 0.0f;
    v->filter_lp = 0.0f;
    v->lfsr = 0x12345;
}

static inline float sk_drum_vl1_step(sk_drum_vl1_t *v) {
    if (!v || v->amp_env < 1e-4f) return 0.0f;

    float out = 0.0f;
    if (v->type == SK_VL1_KICK) {
        float cur_freq = v->end_freq + (v->start_freq - v->end_freq) * v->pitch_env;
        v->phase += cur_freq / v->sample_rate;
        if (v->phase >= 1.0f) v->phase -= 1.0f;

        float sqr = (v->phase < 0.5f) ? 1.0f : -1.0f;
        v->filter_lp += v->filter_coeff * (sqr - v->filter_lp);
        out = v->filter_lp * v->amp_env * 1.6f;

        v->pitch_env *= v->pitch_decay;
        v->amp_env *= v->amp_decay;
    } else if (v->type == SK_VL1_SNARE) {
        /* 17-bit LFSR noise */
        uint32_t bit = ((v->lfsr >> 0) ^ (v->lfsr >> 3)) & 1;
        v->lfsr = (v->lfsr >> 1) | (bit << 16);
        float noise = (v->lfsr & 1) ? 0.85f : -0.85f;

        float cur_freq = v->end_freq + (v->start_freq - v->end_freq) * v->pitch_env;
        v->phase += cur_freq / v->sample_rate;
        if (v->phase >= 1.0f) v->phase -= 1.0f;
        float pop = ((v->phase < 0.5f) ? 0.35f : -0.35f) * v->pitch_env;

        float filtered = sk_biquad_step(&v->biquad, noise + pop);
        out = filtered * v->amp_env * 1.8f;

        v->pitch_env *= v->pitch_decay;
        v->amp_env *= v->amp_decay;
    } else { /* HAT */
        uint32_t bit = ((v->lfsr >> 0) ^ (v->lfsr >> 5)) & 1;
        v->lfsr = (v->lfsr >> 1) | (bit << 16);
        float noise = (v->lfsr & 1) ? 0.9f : -0.9f;

        float filtered = sk_biquad_step(&v->biquad, noise);
        out = filtered * v->amp_env * 1.6f;

        v->amp_env *= v->amp_decay;
    }

    return sk_softclip_cubic(out, 1.0f);
}

/* -------------------------------------------------------------------------
 * 12.15. Casio SK-1 8-Bit PCM Lo-Fi Drum Engine (sk_drum_sk1_t)
 * -------------------------------------------------------------------------
 * Authentic circuit emulation of the 1985 Casio SK-1 PCM percussion ROM:
 *   - Native 9.375 kHz sample-and-hold decimation
 *   - 8-bit companded DAC quantization (256 discrete levels)
 *   - 4-pole post-DAC analog reconstruction filter (3.8 kHz cutoff)
 */
typedef enum {
    SK_SK1_KICK = 0,
    SK_SK1_SNARE = 1,
    SK_SK1_HAT = 2,
    SK_SK1_BONGO_HI = 3,
    SK_SK1_BONGO_LO = 4
} sk_sk1_type_t;

typedef struct {
    float sample_rate;
    int type;
    float phase;
    float start_freq;
    float end_freq;
    float pitch_env;
    float pitch_decay;
    float amp_env;
    float amp_decay;
    sk_biquad_t lowpass1;
    sk_biquad_t lowpass2;
    sk_biquad_t bandpass;
    sk_rng_t rng;
    float decimate_phase;
    float decimate_inc;
    float held_sample;
} sk_drum_sk1_t;

static inline void sk_drum_sk1_init(sk_drum_sk1_t *s, float sample_rate, int type) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    s->type = type;
    sk_rng_init(&s->rng, 9375ULL);

    /* 9375 Hz native SK-1 sample rate */
    s->decimate_inc = 9375.0f / s->sample_rate;
    if (s->decimate_inc > 1.0f) s->decimate_inc = 1.0f;

    /* 4-pole reconstruction filter around 3.8 kHz */
    sk_biquad_init(&s->lowpass1, SK_BIQUAD_LOWPASS, 3800.0f, 0.707f, s->sample_rate);
    sk_biquad_init(&s->lowpass2, SK_BIQUAD_LOWPASS, 3800.0f, 0.707f, s->sample_rate);

    if (type == SK_SK1_KICK) {
        s->start_freq = 135.0f;
        s->end_freq = 46.0f;
        s->pitch_decay = expf(-1.0f / (0.035f * s->sample_rate));
        s->amp_decay = expf(-1.0f / (0.240f * s->sample_rate));
    } else if (type == SK_SK1_SNARE) {
        s->start_freq = 185.0f;
        s->end_freq = 120.0f;
        s->pitch_decay = expf(-1.0f / (0.020f * s->sample_rate));
        s->amp_decay = expf(-1.0f / (0.130f * s->sample_rate));
        sk_biquad_init(&s->bandpass, SK_BIQUAD_BANDPASS, 2600.0f, 1.5f, s->sample_rate);
    } else if (type == SK_SK1_HAT) {
        s->amp_decay = expf(-1.0f / (0.045f * s->sample_rate));
        sk_biquad_init(&s->bandpass, SK_BIQUAD_HIGHPASS, 4800.0f, 1.0f, s->sample_rate);
    } else if (type == SK_SK1_BONGO_HI) {
        s->start_freq = 380.0f;
        s->end_freq = 280.0f;
        s->pitch_decay = expf(-1.0f / (0.030f * s->sample_rate));
        s->amp_decay = expf(-1.0f / (0.160f * s->sample_rate));
    } else { /* BONGO_LO */
        s->start_freq = 260.0f;
        s->end_freq = 175.0f;
        s->pitch_decay = expf(-1.0f / (0.035f * s->sample_rate));
        s->amp_decay = expf(-1.0f / (0.180f * s->sample_rate));
    }
}

static inline void sk_drum_sk1_trigger(sk_drum_sk1_t *s, float velocity) {
    if (!s) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    s->amp_env = velocity;
    s->pitch_env = 1.0f;
    s->phase = 0.0f;
    s->decimate_phase = 1.0f; /* force immediate sample */
    s->held_sample = 0.0f;
}

static inline float sk_drum_sk1_step(sk_drum_sk1_t *s) {
    if (!s || s->amp_env < 1e-4f) return 0.0f;

    s->decimate_phase += s->decimate_inc;
    if (s->decimate_phase >= 1.0f) {
        s->decimate_phase -= 1.0f;

        float raw = 0.0f;
        if (s->type == SK_SK1_KICK) {
            float cur_freq = s->end_freq + (s->start_freq - s->end_freq) * s->pitch_env;
            s->phase += cur_freq / 9375.0f;
            if (s->phase >= 1.0f) s->phase -= 1.0f;
            float s_val = sinf(s->phase * 2.0f * (float)M_PI);
            float tri = (s->phase < 0.5f) ? (4.0f * s->phase - 1.0f) : (3.0f - 4.0f * s->phase);
            raw = (s_val * 0.7f + tri * 0.3f) * 1.5f;
        } else if (s->type == SK_SK1_SNARE) {
            float noise = sk_rng_float(&s->rng);
            float cur_freq = s->end_freq + (s->start_freq - s->end_freq) * s->pitch_env;
            s->phase += cur_freq / 9375.0f;
            if (s->phase >= 1.0f) s->phase -= 1.0f;
            float snap = sinf(s->phase * 2.0f * (float)M_PI) * s->pitch_env * 0.5f;
            raw = sk_biquad_step(&s->bandpass, noise * 0.9f + snap) * 1.6f;
        } else if (s->type == SK_SK1_HAT) {
            float noise = sk_rng_float(&s->rng);
            raw = sk_biquad_step(&s->bandpass, noise) * 1.4f;
        } else { /* BONGO HI / LO */
            float cur_freq = s->end_freq + (s->start_freq - s->end_freq) * s->pitch_env;
            s->phase += cur_freq / 9375.0f;
            if (s->phase >= 1.0f) s->phase -= 1.0f;
            float s_val = sinf(s->phase * 2.0f * (float)M_PI);
            float h2 = sinf(s->phase * 4.0f * (float)M_PI) * 0.25f;
            raw = (s_val + h2) * 1.3f;
        }

        /* 8-bit companded DAC quantization (256 discrete levels) */
        float companded = sk_softclip_cubic(raw, 1.0f);
        int q = (int)(companded * 127.0f);
        if (q > 127) q = 127;
        if (q < -128) q = -128;
        s->held_sample = (float)q / 127.0f;

        s->pitch_env *= s->pitch_decay;
    }

    /* 4-pole reconstruction filter */
    float f1 = sk_biquad_step(&s->lowpass1, s->held_sample);
    float f2 = sk_biquad_step(&s->lowpass2, f1);

    s->amp_env *= s->amp_decay;
    return f2 * s->amp_env;
}

/* -------------------------------------------------------------------------
 * 12.7. Modal Metallic Idiophone & Bell Engine (sk_drum_bell_t)
 * -------------------------------------------------------------------------
 *
 * Real-time physical modal synthesis of struck metallic objects (church bells,
 * tubular chimes, vibraphones, Tibetan singing bowls, and agogo bells).
 *
 * Unlike 2D drums (which pitch-drop on impact and vibrate at Bessel zeros),
 * 3D struck metallic solids possess structural stiffness governed by
 * Euler-Bernoulli beam and shell mechanics (inharmonic quadratic modes) with
 * independent mode damping and zero pitch-glide.
 */

#define SK_BELL_MAX_MODES 16

typedef enum {
    SK_BELL_CHURCH = 0,   /* Tuned bronze church bell (Hum, Prime, Tierce, Quint, Nominal, Decim) */
    SK_BELL_TUBULAR = 1,  /* Orchestral tubular bell / clock chime (Euler-Bernoulli bar flexural modes) */
    SK_BELL_VIBE = 2,     /* Vibraphone / Glockenspiel tuned bar with mallet impact */
    SK_BELL_TIBETAN = 3,  /* Tibetan singing bowl (beating modal doublets with meditative sustain) */
    SK_BELL_AGOGO = 4     /* Tuned Latin agogo bell / metallic cowbell */
} sk_bell_type_t;

typedef struct {
    sk_rotator_t modes[SK_BELL_MAX_MODES];
    int num_modes;
    float sample_rate;
    float base_freq;
    float strike_env;
    float strike_decay_coeff;
    float clapper_env;
    float clapper_decay_coeff;
    sk_bell_type_t type;
    sk_biquad_t strike_filter;
    sk_biquad_t clapper_filter;
    sk_rng_t rng;
} sk_drum_bell_t;

/* Configure modal bell parameters according to acoustic preset */
static inline void sk_drum_bell_set_preset(sk_drum_bell_t *b, sk_bell_type_t type, float base_freq, float decay_scale) {
    if (!b) return;
    if (base_freq < 20.0f) base_freq = 440.0f;
    if (decay_scale <= 0.05f) decay_scale = 1.0f;
    b->type = type;
    b->base_freq = base_freq;

    /* Mode ratio tables: [ratio, amp, t60_base_sec]
     * Church bells: cast bronze doublet pairs with slight acoustic detuning create
     * the majestic beating / warble characteristic of real cathedral bells. */
    static const double CHURCH[14][3] = {
        { 0.500, 0.90, 6.5 },  /* Hum doublet 1 */
        { 0.504, 0.85, 6.5 },  /* Hum doublet 2 (beating ~0.9 Hz) */
        { 1.000, 1.00, 4.8 },  /* Prime / Strike doublet 1 */
        { 1.006, 0.95, 4.8 },  /* Prime / Strike doublet 2 (beating ~1.3 Hz) */
        { 1.183, 0.75, 3.8 },  /* Tierce doublet 1 (minor third) */
        { 1.192, 0.70, 3.8 },  /* Tierce doublet 2 (beating ~2.0 Hz) */
        { 1.506, 0.60, 3.0 },  /* Quint doublet 1 (fifth) */
        { 1.515, 0.55, 3.0 },  /* Quint doublet 2 */
        { 2.000, 0.65, 2.2 },  /* Nominal doublet 1 (octave) */
        { 2.012, 0.60, 2.2 },  /* Nominal doublet 2 (beating ~2.6 Hz) */
        /* Bronze rim shear modal cluster (transient inharmonic sparkle) */
        { 2.684, 0.45, 0.40 }, /* Superquint */
        { 3.245, 0.35, 0.25 }, /* Octave-tierce */
        { 4.150, 0.25, 0.15 }, /* Upper rim flexure */
        { 5.380, 0.18, 0.08 }  /* High rim shear ring */
    };
    static const double TUBULAR[5][3] = {
        {  1.000, 1.00, 3.5 },
        {  2.756, 0.65, 2.5 },
        {  5.404, 0.40, 1.8 },
        {  8.933, 0.25, 1.2 },
        { 13.345, 0.15, 0.7 }
    };
    static const double VIBE[4][3] = {
        { 1.00, 1.00, 2.8 },
        { 3.98, 0.45, 1.5 },
        { 9.20, 0.20, 0.8 },
        { 14.5, 0.10, 0.4 }
    };
    static const double TIBETAN[5][3] = {
        { 1.000, 0.80, 5.5 }, /* Mode 1a */
        { 1.008, 0.80, 5.5 }, /* Mode 1b (beats with 1a at ~1.8 Hz) */
        { 2.710, 0.50, 3.8 }, /* Mode 2a */
        { 2.724, 0.50, 3.8 }, /* Mode 2b (beats with 2a at ~3.5 Hz) */
        { 5.120, 0.30, 2.2 }  /* Mode 3 */
    };
    static const double AGOGO[4][3] = {
        { 1.00, 1.00, 0.45 },
        { 1.48, 0.70, 0.35 },
        { 2.55, 0.40, 0.22 },
        { 4.10, 0.20, 0.14 }
    };

    const double (*table)[3] = CHURCH;
    int count = 14;
    switch (type) {
        case SK_BELL_CHURCH:  table = CHURCH;  count = 14; break;
        case SK_BELL_TUBULAR: table = TUBULAR; count = 5;  break;
        case SK_BELL_VIBE:    table = VIBE;    count = 4;  break;
        case SK_BELL_TIBETAN: table = TIBETAN; count = 5;  break;
        case SK_BELL_AGOGO:   table = AGOGO;   count = 4;  break;
    }
    b->num_modes = count;

    for (int i = 0; i < count; i++) {
        double f = base_freq * table[i][0];
        double amp = table[i][1];
        double t60 = table[i][2] * decay_scale;
        sk_rotator_init(&b->modes[i], f, amp, t60, b->sample_rate);
    }
}

/* Initialize modal bell with default church bell tuning */
static inline void sk_drum_bell_init(sk_drum_bell_t *b, float sample_rate, sk_bell_type_t type, float base_freq) {
    if (!b) return;
    memset(b, 0, sizeof(*b));
    b->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    b->strike_decay_coeff = expf(-1.0f / (0.003f * b->sample_rate)); /* 3ms strike transient */
    sk_biquad_init(&b->strike_filter, SK_BIQUAD_BANDPASS, 3500.0f, 2.0f, b->sample_rate);

    /* Heavy iron clapper impact for church bell */
    sk_biquad_init(&b->clapper_filter, SK_BIQUAD_BANDPASS, 85.0f, 1.8f, b->sample_rate);
    b->clapper_decay_coeff = expf(-1.0f / (0.045f * b->sample_rate)); /* 45ms heavy bronze inertia thump */

    sk_rng_init(&b->rng, 88877711ULL);

    sk_drum_bell_set_preset(b, type, base_freq, 1.0f);
}

/* Trigger modal bell with strike velocity [0.0, 1.0] */
static inline void sk_drum_bell_trigger(sk_drum_bell_t *b, float velocity) {
    if (!b) return;
    velocity = sk_clampf(velocity, 0.0f, 1.0f);
    b->strike_env = velocity * 0.40f;
    b->clapper_env = (b->type == SK_BELL_CHURCH) ? (velocity * 0.85f) : 0.0f;

    /* Re-excite mode phasors.
     * Church bell doublets are orthogonal spatial standing waves on the circular rim:
     * Mode 2k is initialized at phase 0 (re=1, im=0), while doublet partner 2k+1
     * is excited in phase quadrature (re=0, im=1) with non-zero spatial offset.
     * This eliminates synthetic FM-like sinusoidal cancellation tremolo and produces
     * the rich spatial acoustic swirl of real bronze bells. */
    for (int i = 0; i < b->num_modes; i++) {
        if (b->type == SK_BELL_CHURCH && (i < 10) && (i % 2 == 1)) {
            b->modes[i].re = 0.0;
            b->modes[i].im = 1.0;
        } else {
            b->modes[i].re = 1.0;
            b->modes[i].im = 0.0;
        }
    }
}

/* Step modal bell by 1 sample */
static inline float sk_drum_bell_step(sk_drum_bell_t *b) {
    if (!b) return 0.0f;

    /* Sum mode resonators */
    double mode_sum = 0.0;
    for (int i = 0; i < b->num_modes; i++) {
        mode_sum += sk_rotator_step(&b->modes[i]);
    }

    /* Hammer / mallet strike click */
    float strike = 0.0f;
    if (b->strike_env > 1e-4f) {
        float raw_noise = sk_rng_float(&b->rng);
        strike = sk_biquad_step(&b->strike_filter, raw_noise) * b->strike_env;
        b->strike_env *= b->strike_decay_coeff;
    }

    /* Heavy clapper impact thump for cathedral bells */
    float clapper = 0.0f;
    if (b->clapper_env > 1e-4f) {
        float raw_noise = sk_rng_float(&b->rng);
        clapper = sk_biquad_step(&b->clapper_filter, raw_noise) * b->clapper_env;
        b->clapper_env *= b->clapper_decay_coeff;
    }

    float out = (float)mode_sum * 0.35f + strike + clapper;
    return sk_softclip_cubic(out, 1.0f);
}

/* =========================================================================
 * 12b. Circular Acoustic Drum Membrane (Bessel Modes & Tension Pitch-Drop)
 * ========================================================================= */

typedef struct {
    float sample_rate;
    float base_freq;
    float tension_drop;     /* Initial tension jump ratio (e.g. 0.35 - 0.70) */
    float tension_decay;    /* Tension drop per-sample decay */
    float tension_env;      /* Current tension drop envelope */
    float phases[8];
    float mode_amps[8];
    float mode_decays[8];
    float mode_envs[8];
    sk_biquad_t air_cavity;    /* Drum shell cavity resonance */
    sk_biquad_t strike_filter; /* Beater impact filter */
    float strike_env;
    float strike_decay;
    sk_rng_t rng;
} sk_drum_membrane_t;

static inline void sk_drum_membrane_init(sk_drum_membrane_t *m, float sample_rate, float base_freq, float decay_sec, float tension_drop, float strike_pos) {
    if (!m) return;
    memset(m, 0, sizeof(*m));
    m->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    m->base_freq = base_freq > 20.0f ? base_freq : 80.0f;
    m->tension_drop = sk_clampf(tension_drop, 0.0f, 1.5f);
    m->tension_decay = expf(-1.0f / (0.028f * m->sample_rate)); /* ~28ms tension drop */
    m->tension_env = 1.0f;

    /* Bessel mode ratios for ideal clamped circular membrane:
     * (0,1)=1.000, (1,1)=1.593, (2,1)=2.135, (0,2)=2.295,
     * (3,1)=2.653, (1,2)=2.917, (2,2)=3.500, (0,3)=3.598 */
    static const float BESSEL_DAMP_SCALE[8] = {
        1.00f, 0.65f, 0.48f, 0.42f, 0.32f, 0.26f, 0.20f, 0.16f
    };

    strike_pos = sk_clampf(strike_pos, 0.0f, 1.0f); /* 0=center, 1=rim */

    /* Center strike excites radial symmetric modes (0,1), (0,2), (0,3);
     * Rim strike excites azimuthal modes (1,1), (2,1), (3,1) */
    float base_amps[8] = {
        1.00f * (1.0f - 0.4f * strike_pos),
        0.55f * (0.3f + 0.7f * strike_pos),
        0.40f * (0.2f + 0.8f * strike_pos),
        0.50f * (1.0f - 0.5f * strike_pos),
        0.30f * (0.2f + 0.8f * strike_pos),
        0.25f * (0.3f + 0.7f * strike_pos),
        0.18f * (0.2f + 0.8f * strike_pos),
        0.15f * (1.0f - 0.6f * strike_pos)
    };

    if (decay_sec < 0.05f) decay_sec = 0.5f;

    for (int i = 0; i < 8; i++) {
        m->phases[i] = 0.0f;
        m->mode_amps[i] = base_amps[i];
        m->mode_envs[i] = base_amps[i];
        float t60_m = decay_sec * BESSEL_DAMP_SCALE[i];
        m->mode_decays[i] = (t60_m > 0.001f) ? expf(-1.0f / (t60_m * m->sample_rate / 3.0f)) : 0.0f;
    }

    /* Air cavity / shell resonance tuned around fundamental * 0.92 */
    float cavity_f = m->base_freq * 0.92f;
    if (cavity_f < 35.0f) cavity_f = 35.0f;
    if (cavity_f > 300.0f) cavity_f = 300.0f;
    sk_biquad_init(&m->air_cavity, SK_BIQUAD_BANDPASS, cavity_f, 2.2f, m->sample_rate);

    /* Mallet / beater impact filter */
    float beater_f = 1200.0f + 1800.0f * (1.0f - strike_pos);
    if (beater_f > m->sample_rate * 0.45f) beater_f = m->sample_rate * 0.45f;
    sk_biquad_init(&m->strike_filter, SK_BIQUAD_BANDPASS, beater_f, 1.8f, m->sample_rate);
    m->strike_env = 0.5f;
    m->strike_decay = expf(-1.0f / (0.005f * m->sample_rate)); /* 5ms impact click */
    sk_rng_init(&m->rng, 7712398ULL);
}

static inline float sk_drum_membrane_step(sk_drum_membrane_t *m) {
    if (!m) return 0.0f;

    static const float BESSEL_RATIOS[8] = {
        1.000f, 1.593f, 2.135f, 2.295f, 2.653f, 2.917f, 3.500f, 3.598f
    };

    /* Pitch drop from tension relaxation */
    float f_now = m->base_freq * (1.0f + m->tension_drop * m->tension_env);
    m->tension_env *= m->tension_decay;

    float membrane_sum = 0.0f;
    float nyquist = 0.48f * m->sample_rate;

    for (int i = 0; i < 8; i++) {
        if (m->mode_envs[i] < 1e-6f) continue;
        float fi = f_now * BESSEL_RATIOS[i];
        if (fi < nyquist) {
            m->phases[i] += 2.0f * (float)M_PI * fi / m->sample_rate;
            if (m->phases[i] >= 2.0f * (float)M_PI) m->phases[i] -= 2.0f * (float)M_PI;
            membrane_sum += sinf(m->phases[i]) * m->mode_envs[i];
        }
        m->mode_envs[i] *= m->mode_decays[i];
    }

    /* Mallet / beater impact transient */
    float strike = 0.0f;
    if (m->strike_env > 1e-4f) {
        float noise = sk_rng_float(&m->rng);
        strike = sk_biquad_step(&m->strike_filter, noise) * m->strike_env;
        m->strike_env *= m->strike_decay;
    }

    /* Air cavity resonance */
    float cavity = sk_biquad_step(&m->air_cavity, membrane_sum) * 0.45f;

    float out = (membrane_sum * 0.40f + cavity + strike * 0.35f);
    return sk_softclip_cubic(out, 1.0f);
}

/* =========================================================================
 * 12c. Acoustic Instrument Wooden Soundbox Body Resonator (Bass, Cello)
 * ========================================================================= */

typedef struct {
    sk_biquad_t air_cavity;    /* Helmholtz cavity resonance */
    sk_biquad_t wood_mode1;    /* Primary top plate wood mode */
    sk_biquad_t wood_mode2;    /* Secondary back plate wood mode */
    float air_gain;
    float wood_gain;
    float direct_gain;
} sk_soundbox_t;

static inline void sk_soundbox_init(sk_soundbox_t *b, float sample_rate, float air_freq, float air_q, float wood1_freq, float wood1_q, float wood2_freq, float wood2_q, float air_gain, float wood_gain, float direct_gain) {
    if (!b) return;
    sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    sk_biquad_init(&b->air_cavity, SK_BIQUAD_BANDPASS, air_freq, air_q, sample_rate);
    sk_biquad_init(&b->wood_mode1, SK_BIQUAD_BANDPASS, wood1_freq, wood1_q, sample_rate);
    sk_biquad_init(&b->wood_mode2, SK_BIQUAD_BANDPASS, wood2_freq, wood2_q, sample_rate);
    b->air_gain = air_gain;
    b->wood_gain = wood_gain;
    b->direct_gain = direct_gain;
}

static inline void sk_soundbox_init_upright_bass(sk_soundbox_t *b, float sample_rate) {
    /* Upright double bass: air cavity ~62 Hz (warm acoustic boom), wood ~98 Hz & ~165 Hz (soundboard resonances) */
    sk_soundbox_init(b, sample_rate, 62.0f, 3.8f, 98.0f, 4.2f, 165.0f, 3.2f, 0.65f, 0.50f, 0.40f);
}

static inline void sk_soundbox_init_cello(sk_soundbox_t *b, float sample_rate) {
    /* Cello: air ~125 Hz, wood ~220 Hz & ~380 Hz */
    sk_soundbox_init(b, sample_rate, 125.0f, 3.2f, 220.0f, 3.8f, 380.0f, 2.8f, 0.50f, 0.45f, 0.40f);
}

static inline float sk_soundbox_step(sk_soundbox_t *b, float input) {
    if (!b) return input;
    float air = sk_biquad_step(&b->air_cavity, input);
    float w1 = sk_biquad_step(&b->wood_mode1, input);
    float w2 = sk_biquad_step(&b->wood_mode2, input);
    return b->direct_gain * input + b->air_gain * air + b->wood_gain * (w1 + 0.6f * w2);
}

/* =========================================================================
 * 13. Physical Wind & Brass Synthesizers (Aeroacoustic Jet & Lip-Reed)
 * =========================================================================
 *
 * Physical modeling synthesis of wind and brass instruments:
 *
 * 1. AEROACOUSTIC FLUTE & BLOWN PIPE (sk_flute_t):
 *    - Implements Perry Cook, Coltman, and McIntyre-Schumacher-Woodhouse (MSW)
 *      jet-drive physics. A planar air jet is blown across an embouchure labium
 *      edge, forming a non-linear velocity-dependent pressure split:
 *
 *          P_inj = tanh(P_breath - P_bore_refl)
 *
 *      coupled to an acoustic cylindrical bore delay line of length L = Fs / f0.
 *    - Features: breath turbulence noise, overblowing (register jump to 2*f0),
 *      reflection damping, and natural vibrato/tremolo.
 *
 * 2. BRASS & LIP-REED INSTRUMENT (sk_brass_t):
 *    - Implements the Helmholtz / Fletcher & Rossing pressure-controlled lip-valve
 *      coupled to a flared horn bell. The player's lips act as a 2-pole mass-spring
 *      oscillator driven by differential pressure delta_P = P_mouth - P_bore_refl.
 *    - Flared bell filter radiates high frequencies into the room while reflecting
 *      inverted low frequencies back into the tube to sustain lip oscillation.
 *    - Non-linear shock wave steepening (Burgers' equation) generates the authentic
 *      raspy blare and brilliance of a trumpet/trombone at fortissimo dynamics.
 * ========================================================================= */

#define SK_WIND_MAX_DELAY 4096

/* -------------------------------------------------------------------------
 * 13.1. Flute / Blown Pipe Physical Model
 * ------------------------------------------------------------------------- */
typedef struct {
    float phase;
    float phase_inc;
    float freq;
    float breath_pressure;
    float target_pressure;
    float pressure_slew;
    float noise_gain;
    float chiff_env;
    float bore_damping;
    sk_biquad_t chiff_filter;   /* ~3200 Hz attack chiff */
    sk_biquad_t body_formant;   /* ~1100 Hz warm wooden bore resonance */
    sk_biquad_t air_filter;     /* ~4200 Hz breath lowpass */
    float dc_x, dc_y;
    float sample_rate;
    sk_rng_t rng;
} sk_flute_t;

/* Initialize physical flute engine */
static inline void sk_flute_init(sk_flute_t *fl, float sample_rate) {
    if (!fl) return;
    memset(fl, 0, sizeof(*fl));
    fl->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    fl->freq = 523.25f;
    fl->phase_inc = fl->freq / fl->sample_rate;
    fl->bore_damping = 0.25f;
    fl->noise_gain = 0.05f;
    fl->pressure_slew = 0.003f;
    fl->chiff_env = 1.0f;
    sk_rng_init(&fl->rng, 777123ULL);

    sk_biquad_init(&fl->chiff_filter, SK_BIQUAD_BANDPASS, 3200.0f, 2.2f, fl->sample_rate);
    sk_biquad_init(&fl->body_formant, SK_BIQUAD_BANDPASS, 1100.0f, 2.5f, fl->sample_rate);
    sk_biquad_init(&fl->air_filter,   SK_BIQUAD_LOWPASS,  4200.0f, 0.707f, fl->sample_rate);
}

/* Tune flute to target frequency in Hz */
static inline void sk_flute_set_freq(sk_flute_t *fl, float freq_hz) {
    if (!fl) return;
    fl->freq = sk_clampf(freq_hz, 40.0f, fl->sample_rate * 0.45f);
    fl->phase_inc = fl->freq / fl->sample_rate;
}

/* Set blowing pressure [0.0 = silence, 0.4 = soft flute, 0.75 = normal, >0.85 = overblow] */
static inline void sk_flute_set_pressure(sk_flute_t *fl, float pressure) {
    if (!fl) return;
    fl->target_pressure = sk_clampf(pressure, 0.0f, 1.2f);
}

/* Step physical flute model by 1 sample */
static inline float sk_flute_step(sk_flute_t *fl) {
    if (!fl) return 0.0f;

    /* Slew blowing pressure toward target */
    fl->breath_pressure += fl->pressure_slew * (fl->target_pressure - fl->breath_pressure);
    if (fl->breath_pressure < 1e-4f && fl->target_pressure < 1e-4f) return 0.0f;

    /* Overblowing when blowing pressure exceeds 0.85 (smooth octave jump) */
    float oct_mult = 1.0f;
    if (fl->breath_pressure > 0.85f) {
        float overblow = (fl->breath_pressure - 0.85f) * 2.85f;
        if (overblow > 1.0f) overblow = 1.0f;
        oct_mult = 1.0f + overblow;
    }

    fl->phase += fl->phase_inc * oct_mult;
    if (fl->phase >= 1.0f) fl->phase -= 1.0f;

    /* Open cylindrical acoustic pipe harmonic series (1/n roll-off with odd dominance) */
    float p = fl->phase * 2.0f * (float)M_PI;
    float h1 = sinf(p);
    float h2 = sinf(2.0f * p) * 0.10f; /* subtle embouchure asymmetry */
    float h3 = sinf(3.0f * p) * 0.25f; /* strong odd harmonic */
    float h4 = sinf(4.0f * p) * 0.04f;
    float h5 = sinf(5.0f * p) * 0.09f;
    float h6 = sinf(6.0f * p) * 0.02f;
    float h7 = sinf(7.0f * p) * 0.035f;
    float pipe_tone = h1 + h2 + h3 + h4 + h5 + h6 + h7;

    /* Transient chiff (initial 35ms attack burst) */
    float raw_noise = sk_rng_float(&fl->rng);
    float chiff = sk_biquad_step(&fl->chiff_filter, raw_noise) * fl->chiff_env * 0.20f;
    fl->chiff_env *= 0.9985f;

    /* Subtle breath air */
    float filtered_air = sk_biquad_step(&fl->air_filter, raw_noise);
    float breath_noise = filtered_air * fl->noise_gain * fl->breath_pressure * 0.30f;

    float excited = (pipe_tone * 0.85f + chiff + breath_noise) * fl->breath_pressure;

    /* Resonate through wooden body */
    float body = sk_biquad_step(&fl->body_formant, excited);
    float mixed = excited * 0.70f + body * 0.35f;

    /* DC blocker to prevent integration bias */
    float out = mixed - fl->dc_x + 0.995f * fl->dc_y;
    fl->dc_x = mixed;
    fl->dc_y = out;

    return sk_softclip_cubic(out * 1.4f, 1.0f);
}

/* -------------------------------------------------------------------------
 * 13.2. Brass / Lip-Reed Instrument Model
 * ------------------------------------------------------------------------- */
typedef struct {
    float phase;
    float phase_inc;
    float freq;
    float mouth_pressure;
    float target_pressure;
    float pressure_slew;
    float brassiness;
    float bore_damping;
    sk_biquad_t body_formant;   /* 580 Hz bore resonance */
    sk_biquad_t throat_formant; /* 1450 Hz horn throat */
    sk_biquad_t bell_formant;   /* 2800 Hz flared bell radiation */
    float dc_x, dc_y;
    float sample_rate;
    sk_rng_t rng;
} sk_brass_t;

/* Initialize physical brass instrument engine */
static inline void sk_brass_init(sk_brass_t *br, float sample_rate) {
    if (!br) return;
    memset(br, 0, sizeof(*br));
    br->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    br->freq = 233.08f;
    br->phase_inc = br->freq / br->sample_rate;
    br->pressure_slew = 0.003f;
    br->brassiness = 0.50f;
    br->bore_damping = 0.25f;
    sk_rng_init(&br->rng, 123456ULL);

    sk_biquad_init(&br->body_formant,   SK_BIQUAD_BANDPASS, 580.0f,  2.5f, br->sample_rate);
    sk_biquad_init(&br->throat_formant, SK_BIQUAD_BANDPASS, 1450.0f, 3.0f, br->sample_rate);
    sk_biquad_init(&br->bell_formant,   SK_BIQUAD_BANDPASS, 2800.0f, 2.2f, br->sample_rate);
}

/* Tune brass instrument to target pitch in Hz */
static inline void sk_brass_set_freq(sk_brass_t *br, float freq_hz) {
    if (!br) return;
    br->freq = sk_clampf(freq_hz, 30.0f, br->sample_rate * 0.45f);
    br->phase_inc = br->freq / br->sample_rate;
}

/* Set mouth blowing pressure [0.0 = rest, 0.4 = piano, 0.7 = forte, 1.0 = fortissimo blare] */
static inline void sk_brass_set_pressure(sk_brass_t *br, float pressure) {
    if (!br) return;
    br->target_pressure = sk_clampf(pressure, 0.0f, 1.2f);
}

/* Step physical brass model by 1 sample */
static inline float sk_brass_step(sk_brass_t *br) {
    if (!br) return 0.0f;

    /* Slew mouth blowing pressure */
    br->mouth_pressure += br->pressure_slew * (br->target_pressure - br->mouth_pressure);
    if (br->mouth_pressure < 1e-4f && br->target_pressure < 1e-4f) return 0.0f;

    /* Advance lip phase */
    br->phase += br->phase_inc;
    if (br->phase >= 1.0f) br->phase -= 1.0f;

    /* Asymmetric lip pulse with pressure-dependent duty cycle (0.28 to 0.43) */
    float duty = 0.28f + 0.15f * br->mouth_pressure;
    float raw_lip = 0.0f;
    if (br->phase < duty) {
        float p = br->phase / duty;
        raw_lip = sinf(p * (float)M_PI);
        /* Non-linear shock wave steepening in horn */
        float steep = 1.0f + br->brassiness * 2.5f * br->mouth_pressure;
        raw_lip = powf(raw_lip, 1.0f / steep);
    } else {
        /* Closed lip phase with subtle mechanical rebound */
        float p = (br->phase - duty) / (1.0f - duty);
        raw_lip = -0.06f * sinf(p * (float)M_PI) * (1.0f - p);
    }

    /* Subtle breath turbulence */
    float breath_noise = sk_rng_float(&br->rng) * 0.015f * br->mouth_pressure;
    float excited = (raw_lip + breath_noise) * br->mouth_pressure;

    /* Resonate through the acoustic brass horn body formants */
    float f_body = sk_biquad_step(&br->body_formant, excited);
    float f_throat = sk_biquad_step(&br->throat_formant, excited);
    float f_bell = sk_biquad_step(&br->bell_formant, excited);

    /* Bore damping attenuates high-frequency bell radiation */
    float bell_gain = 0.25f * (1.0f - sk_clampf(br->bore_damping, 0.0f, 0.8f));
    float mixed = excited * 0.35f + f_body * 0.40f + f_throat * 0.30f + f_bell * bell_gain;

    /* DC blocker */
    float out = mixed - br->dc_x + 0.995f * br->dc_y;
    br->dc_x = mixed;
    br->dc_y = out;

    return sk_softclip_cubic(out * 1.8f, 1.0f);
}

/* =========================================================================
 * 14. PolyBLEP (Polynomial Bandlimited Step) Anti-Aliased Oscillators
 * =========================================================================
 *
 * Implements low-order Polynomial Bandlimited Step (PolyBLEP) algorithms
 * (Välimäki & Huovilainen 2007).
 *
 * Naive digital sawtooth and pulse waveforms alias harshly due to their
 * infinite harmonic step discontinuities. PolyBLEP subtracts a 2nd-order
 * polynomial residual across the 1 sample before and after the transition:
 *
 *     B(t) = + (2*t - t^2 - 1)   for  0 <= phase < dt   (t = phase / dt)
 *     B(t) = + (t^2 + 2*t + 1)   for  1-dt < phase < 1  (t = (phase - 1) / dt)
 *
 * Features:
 * - Zero memory footprint (no wavetables or mipmaps required).
 * - Continuous, artifact-free Pulse Width Modulation (PWM).
 * - Continuous audio-rate FM without interpolation jitter.
 * ========================================================================= */

/* Compute 2-sample PolyBLEP correction residual for phase in [0.0, 1.0) and phase increment dt */
static inline float sk_polyblep(float phase, float dt) {
    if (dt <= 0.0f) return 0.0f;
    if (phase < dt) {
        float t = phase / dt;
        return t + t - t * t - 1.0f;
    } else if (phase > 1.0f - dt) {
        float t = (phase - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

/* Bandlimited sawtooth wave with zero table lookups */
static inline float sk_polyblep_saw(float phase, float dt) {
    /* Naive rising sawtooth [-1.0, 1.0] */
    float naive = 2.0f * phase - 1.0f;
    /* Subtract PolyBLEP residual at phase = 0 jump discontinuity */
    return naive - sk_polyblep(phase, dt);
}

/* Bandlimited pulse wave with continuous pulse-width modulation (PWM) */
static inline float sk_polyblep_pulse(float phase, float dt, float pwm) {
    pwm = sk_clampf(pwm, 0.01f, 0.99f);
    /* Naive pulse */
    float naive = (phase < pwm) ? 1.0f : -1.0f;

    /* Rising edge transition at phase = 0 */
    float blep1 = sk_polyblep(phase, dt);

    /* Falling edge transition at phase = pwm */
    float phase_pwm = phase - pwm;
    if (phase_pwm < 0.0f) phase_pwm += 1.0f;
    float blep2 = sk_polyblep(phase_pwm, dt);

    /* Correct both edges and remove DC bias */
    float out = naive + blep1 - blep2;
    float dc_offset = 2.0f * pwm - 1.0f;
    return out - dc_offset;
}

/* Bandlimited triangle wave (integrated PolyBLEP square) */
static inline float sk_polyblep_triangle(float phase, float dt, float *integrator_state) {
    float pulse = sk_polyblep_pulse(phase, dt, 0.5f);
    /* Leaky one-pole integrator with DC rejection */
    float state = integrator_state ? *integrator_state : 0.0f;
    state = 0.998f * state + pulse * (4.0f * dt);
    if (integrator_state) *integrator_state = state;
    return state;
}

typedef enum {
    SK_POLYBLEP_SAW = 0,
    SK_POLYBLEP_PULSE = 1,
    SK_POLYBLEP_TRIANGLE = 2,
    SK_POLYBLEP_SINE = 3
} sk_polyblep_wave_t;

/* Turnkey PolyBLEP Anti-Aliased Oscillator */
typedef struct {
    float phase;
    float phase_inc;
    float freq;
    float pwm;
    sk_polyblep_wave_t wave;
    float tri_state;
    float sample_rate;
} sk_polyblep_osc_t;

/* Initialize PolyBLEP oscillator */
static inline void sk_polyblep_osc_init(sk_polyblep_osc_t *osc, float sample_rate) {
    if (!osc) return;
    memset(osc, 0, sizeof(*osc));
    osc->sample_rate = sample_rate > 1000.0f ? sample_rate : 48000.0f;
    osc->pwm = 0.50f;
    osc->wave = SK_POLYBLEP_SAW;
}

/* Set oscillator frequency in Hz */
static inline void sk_polyblep_osc_set_freq(sk_polyblep_osc_t *osc, float freq_hz) {
    if (!osc) return;
    freq_hz = sk_clampf(freq_hz, 1.0f, osc->sample_rate * 0.48f);
    osc->freq = freq_hz;
    osc->phase_inc = freq_hz / osc->sample_rate;
}

/* Set oscillator pulse width [0.01, 0.99] */
static inline void sk_polyblep_osc_set_pwm(sk_polyblep_osc_t *osc, float pwm) {
    if (!osc) return;
    osc->pwm = sk_clampf(pwm, 0.01f, 0.99f);
}

/* Step PolyBLEP oscillator by 1 sample */
static inline float sk_polyblep_osc_step(sk_polyblep_osc_t *osc) {
    if (!osc) return 0.0f;

    float out = 0.0f;
    float p = osc->phase;
    float dt = osc->phase_inc;

    switch (osc->wave) {
        case SK_POLYBLEP_SAW:
            out = sk_polyblep_saw(p, dt);
            break;
        case SK_POLYBLEP_PULSE:
            out = sk_polyblep_pulse(p, dt, osc->pwm);
            break;
        case SK_POLYBLEP_TRIANGLE:
            out = sk_polyblep_triangle(p, dt, &osc->tri_state);
            break;
        case SK_POLYBLEP_SINE:
        default:
            out = sinf(p * 2.0f * (float)M_PI);
            break;
    }

    osc->phase += dt;
    if (osc->phase >= 1.0f) osc->phase -= 1.0f;

    return out;
}

/* =========================================================================
 * 15. Acoustic-Phonetic Speech-to-Phoneme Analyzer (Pure C, Zero Dependencies)
 * =========================================================================
 *
 * Real-time / offline acoustic-phonetic analyzer that transcribes raw PCM
 * audio into ARPAbet phonetic DSL tokens without machine learning or Python.
 *
 * Architecture:
 * 1. Pre-emphasis & decimation: Input audio is decimated 3:1 to ~16 kHz
 *    to place the formant frequencies F1..F3 into optimal LPC pole range.
 * 2. 25ms Hamming-windowed frames with 10ms hop.
 * 3. Feature Extraction:
 *    - Short-time RMS energy (detects silence / pause).
 *    - Zero-Crossing Rate (ZCR) (discriminates voiced vowels from unvoiced fricatives).
 *    - Normalized autocorrelation peak in pitch range [70 Hz, 400 Hz] (F0 & periodicity).
 * 4. 10-Pole Linear Predictive Coding (Levinson-Durbin recursion):
 *    - Inverts the speech spectral envelope into all-pole vocal tract transfer function.
 *    - Peak-picking extracts formant frequencies F1, F2, F3.
 * 5. Nearest-Codebook Classifier:
 *    - Voiced frames: computes weighted Euclidean distance to all ARPAbet vowels in TABLE:
 *      d = 2.0*(F1 - target_F1)^2 + 1.2*(F2 - target_F2)^2 + 0.3*(F3 - target_F3)^2
 *    - Unvoiced frames: classifies high-frequency vs mid-frequency energy into S, SH, F, T.
 * 6. Temporal Run-Length Segmentation:
 *    - Merges consecutive identical frame classifications into timed DSL tokens:
 *      "[S:80 EH:140@125 D:60]".
 * ========================================================================= */

#define SK_LPC_ORDER 10
#define SK_ANALYSIS_SR 16000.0f

/* Solve 10th-order all-pole filter via Levinson-Durbin recursion */
static inline void sk_lpc_levinson(const float *r, int order, float *out_a) {
    if (!r || order <= 0 || !out_a) return;
    out_a[0] = 1.0f;
    for (int i = 1; i <= order; i++) out_a[i] = 0.0f;
    float e = r[0];
    if (e < 1e-12f) return;

    float a_prev[SK_LPC_ORDER + 1];
    for (int i = 1; i <= order; i++) {
        float s = 0.0f;
        for (int j = 1; j < i; j++) {
            s += out_a[j] * r[i - j];
        }
        float k = -(r[i] + s) / e;
        if (k > 0.999f) k = 0.999f;
        if (k < -0.999f) k = -0.999f;

        memcpy(a_prev, out_a, (order + 1) * sizeof(float));
        for (int j = 1; j < i; j++) {
            out_a[j] = a_prev[j] + k * a_prev[i - j];
        }
        out_a[i] = k;
        e *= (1.0f - k * k);
        if (e < 1e-12f) break;
    }
}

/* Extract F1, F2, F3 formant peaks from LPC polynomial coefficients at sample rate sr */
static inline int sk_lpc_extract_formants(const float *a, int order, float sr, float *f1, float *f2, float *f3) {
    if (!a || order < 2 || !f1 || !f2 || !f3) return 0;
    *f1 = 500.0f; *f2 = 1500.0f; *f3 = 2500.0f;

    /* Evaluate |A(e^jw)| at 128 discrete frequencies from 200 Hz to 3500 Hz */
    float spectrum[128];
    float freqs[128];
    int num_pts = 128;
    float f_min = 200.0f;
    float f_max = 3500.0f;
    float df = (f_max - f_min) / (float)(num_pts - 1);

    for (int i = 0; i < num_pts; i++) {
        float f = f_min + (float)i * df;
        freqs[i] = f;
        float omega = 2.0f * (float)M_PI * f / sr;
        float re = 1.0f;
        float im = 0.0f;
        for (int k = 1; k <= order; k++) {
            float phase = omega * (float)k;
            re += a[k] * cosf(phase);
            im -= a[k] * sinf(phase);
        }
        float mag_sq = re * re + im * im;
        spectrum[i] = (mag_sq > 1e-12f) ? (1.0f / sqrtf(mag_sq)) : 0.0f;
    }

    /* Peak picking */
    float peak_f[8];
    int num_peaks = 0;
    for (int i = 1; i < num_pts - 1 && num_peaks < 8; i++) {
        if (spectrum[i] > spectrum[i - 1] && spectrum[i] > spectrum[i + 1] && spectrum[i] > 1.2f) {
            peak_f[num_peaks] = freqs[i];
            num_peaks++;
        }
    }

    if (num_peaks >= 1) *f1 = peak_f[0];
    if (num_peaks >= 2) *f2 = peak_f[1];
    else *f2 = *f1 * 2.2f;
    if (num_peaks >= 3) *f3 = peak_f[2];
    else *f3 = *f2 * 1.5f;

    return num_peaks;
}

/* Transcribe raw PCM float audio into phoneme DSL string */
static inline int sk_speech_analyze_pcm(const float *pcm, int num_samples, float sample_rate,
                                       char *out_dsl, int max_dsl_len) {
    if (!pcm || num_samples <= 0 || !out_dsl || max_dsl_len <= 16) return 0;
    out_dsl[0] = '\0';

    /* Step 1: Pre-emphasis filter: y[n] = x[n] - 0.95*x[n-1] and decimation to ~16 kHz */
    int decimation = (int)(sample_rate / SK_ANALYSIS_SR);
    if (decimation < 1) decimation = 1;
    float analysis_sr = sample_rate / (float)decimation;

    int decimated_len = num_samples / decimation;
    if (decimated_len < 400) return 0; /* Too short to analyze */

    /* Local fixed buffers for framing */
    #define SK_WIN_LEN 400 /* 25 ms @ 16 kHz */
    #define SK_HOP_LEN 160 /* 10 ms @ 16 kHz */

    int num_frames = (decimated_len - SK_WIN_LEN) / SK_HOP_LEN;
    if (num_frames < 1) return 0;

    /* Classify frame by frame into compact token list */
    typedef struct {
        char code[4];
        float pitch;
        int is_voiced;
    } sk_frame_tag_t;

    int max_tags = num_frames < 2048 ? num_frames : 2048;
    sk_frame_tag_t *tags = (sk_frame_tag_t *)calloc(max_tags, sizeof(sk_frame_tag_t));
    if (!tags) return 0;

    float hamming[SK_WIN_LEN];
    for (int i = 0; i < SK_WIN_LEN; i++) {
        hamming[i] = 0.54f - 0.46f * cosf(2.0f * (float)M_PI * (float)i / (float)(SK_WIN_LEN - 1));
    }

    for (int fi = 0; fi < max_tags; fi++) {
        int start_idx = fi * SK_HOP_LEN * decimation;
        float frame[SK_WIN_LEN];
        float energy = 0.0f;
        int zcr_count = 0;
        float prev_sample = 0.0f;

        /* Decimate and pre-emphasize frame */
        for (int i = 0; i < SK_WIN_LEN; i++) {
            int src_idx = start_idx + i * decimation;
            float raw = (src_idx < num_samples) ? pcm[src_idx] : 0.0f;
            float filtered = raw - 0.95f * prev_sample;
            prev_sample = raw;

            float w_sample = filtered * hamming[i];
            frame[i] = w_sample;
            energy += w_sample * w_sample;
            if (i > 0 && ((frame[i] >= 0.0f && frame[i - 1] < 0.0f) || (frame[i] < 0.0f && frame[i - 1] >= 0.0f))) {
                zcr_count++;
            }
        }
        float rms = sqrtf(energy / (float)SK_WIN_LEN);
        float zcr = (float)zcr_count / (float)SK_WIN_LEN;

        /* Autocorrelation for pitch and LPC */
        float r[SK_LPC_ORDER + 1];
        for (int lag = 0; lag <= SK_LPC_ORDER; lag++) {
            float sum = 0.0f;
            for (int i = 0; i < SK_WIN_LEN - lag; i++) {
                sum += frame[i] * frame[i + lag];
            }
            r[lag] = sum;
        }

        /* Pitch search via autocorrelation in range [70 Hz, 350 Hz] */
        int min_lag = (int)(analysis_sr / 350.0f);
        int max_lag = (int)(analysis_sr / 70.0f);
        if (max_lag >= SK_WIN_LEN) max_lag = SK_WIN_LEN - 1;

        float best_r = 0.0f;
        int best_lag = 0;
        for (int lag = min_lag; lag <= max_lag; lag++) {
            float sum = 0.0f;
            for (int i = 0; i < SK_WIN_LEN - lag; i++) {
                sum += frame[i] * frame[i + lag];
            }
            if (sum > best_r) {
                best_r = sum;
                best_lag = lag;
            }
        }

        float norm_pitch_corr = (r[0] > 1e-8f) ? (best_r / r[0]) : 0.0f;
        float pitch_hz = (best_lag > 0) ? (analysis_sr / (float)best_lag) : 120.0f;
        int is_voiced = (norm_pitch_corr > 0.35f && zcr < 0.22f && rms > 0.008f);

        /* Decision logic */
        if (rms < 0.006f) {
            strcpy(tags[fi].code, "_");
            tags[fi].pitch = 0.0f;
            tags[fi].is_voiced = 0;
        } else if (is_voiced) {
            /* 10-th order LPC formant extraction */
            float lpc_a[SK_LPC_ORDER + 1];
            sk_lpc_levinson(r, SK_LPC_ORDER, lpc_a);

            float f1 = 500.0f, f2 = 1500.0f, f3 = 2500.0f;
            sk_lpc_extract_formants(lpc_a, SK_LPC_ORDER, analysis_sr, &f1, &f2, &f3);

            /* Match against vowel ARPAbet codebook */
            static const char *VOWELS[] = { "AA", "AE", "AH", "AO", "EH", "ER", "EY", "IH", "IY", "OW", "UH", "UW" };
            int num_vowels = sizeof(VOWELS) / sizeof(VOWELS[0]);
            float min_dist = 1e12f;
            const char *best_vowel = "AH";

            for (int v = 0; v < num_vowels; v++) {
                const sk_phoneme_def_t *d = sk_phoneme_lookup(VOWELS[v]);
                if (!d) continue;
                float df1 = (f1 - d->f1);
                float df2 = (f2 - d->f2);
                float df3 = (f3 - d->f3);
                float dist = 2.0f * (df1 * df1) + 1.2f * (df2 * df2) + 0.3f * (df3 * df3);
                if (dist < min_dist) {
                    min_dist = dist;
                    best_vowel = VOWELS[v];
                }
            }
            strcpy(tags[fi].code, best_vowel);
            tags[fi].pitch = pitch_hz;
            tags[fi].is_voiced = 1;
        } else {
            /* Unvoiced consonant */
            if (zcr > 0.32f) {
                strcpy(tags[fi].code, "S");
            } else if (zcr > 0.22f) {
                strcpy(tags[fi].code, "SH");
            } else if (rms > 0.04f) {
                strcpy(tags[fi].code, "T");
            } else {
                strcpy(tags[fi].code, "F");
            }
            tags[fi].pitch = 0.0f;
            tags[fi].is_voiced = 0;
        }
    }

    /* Step 2: Temporal Run-Length Grouping into DSL Syntax */
    int dsl_pos = 0;
    out_dsl[0] = '\0';

    int cur_start = 0;
    while (cur_start < max_tags && dsl_pos < max_dsl_len - 32) {
        int cur_end = cur_start + 1;
        float pitch_sum = tags[cur_start].pitch;
        int pitch_count = tags[cur_start].is_voiced ? 1 : 0;

        while (cur_end < max_tags && strcmp(tags[cur_end].code, tags[cur_start].code) == 0) {
            if (tags[cur_end].is_voiced) {
                pitch_sum += tags[cur_end].pitch;
                pitch_count++;
            }
            cur_end++;
        }

        int duration_ms = (cur_end - cur_start) * 10;
        float avg_pitch = (pitch_count > 0) ? (pitch_sum / (float)pitch_count) : 0.0f;

        char token[64];
        if (strcmp(tags[cur_start].code, "_") == 0) {
            snprintf(token, sizeof(token), "[_:%d] ", duration_ms);
        } else if (tags[cur_start].is_voiced && avg_pitch > 40.0f) {
            snprintf(token, sizeof(token), "[%s:%d@%d] ", tags[cur_start].code, duration_ms, (int)(avg_pitch + 0.5f));
        } else {
            snprintf(token, sizeof(token), "[%s:%d] ", tags[cur_start].code, duration_ms);
        }

        int tok_len = (int)strlen(token);
        if (dsl_pos + tok_len < max_dsl_len - 2) {
            strcpy(out_dsl + dsl_pos, token);
            dsl_pos += tok_len;
        }

        cur_start = cur_end;
    }

    free(tags);
    #undef SK_WIN_LEN
    #undef SK_HOP_LEN

    return dsl_pos;
}

#ifdef __cplusplus
}
#endif

#endif /* SK_DSP_H */
