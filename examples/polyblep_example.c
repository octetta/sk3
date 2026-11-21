/**
 * PolyBLEP (Polynomial Bandlimited Step) Anti-Aliased Oscillators Example
 * Demonstrates:
 * 1. High-frequency Sawtooth pitch sweep (Naive Aliased vs PolyBLEP Anti-Aliased)
 * 2. Continuous Pulse Width Modulation (PWM sweep) without tables or mipmaps
 * 3. PolyBLEP Triangle wave
 *
 * Compile: gcc -O2 examples/polyblep_example.c -lm -o polyblep_example
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include "../src/sk_dsp.h"

static void write_wav_16(const char *filename, const int16_t *pcm, int n, int sr) {
    FILE *f = fopen(filename, "wb");
    if (!f) return;
    int data_size = n * sizeof(int16_t);
    int file_size = 36 + data_size;
    int byte_rate = sr * 2;
    int16_t fmt = 1, ch = 1, align = 2, bps = 16;
    int chunk1_size = 16;

    fwrite("RIFF", 1, 4, f); fwrite(&file_size, 4, 1, f); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); fwrite(&chunk1_size, 4, 1, f); fwrite(&fmt, 2, 1, f);
    fwrite(&ch, 2, 1, f);   fwrite(&sr, 4, 1, f);         fwrite(&byte_rate, 4, 1, f);
    fwrite(&align, 2, 1, f); fwrite(&bps, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&data_size, 4, 1, f); fwrite(pcm, 2, n, f);
    fclose(f);
    printf("Wrote %s (%.2f seconds)\n", filename, (double)n / sr);
}

int main(void) {
    int sr = 48000;
    int sweep_samples = sr * 3; /* 3-second sweep */
    int pwm_samples = sr * 3;   /* 3-second PWM sweep */
    int total_samples = sweep_samples * 2 + pwm_samples + sr;

    int16_t *out = (int16_t *)calloc(total_samples, sizeof(int16_t));
    int out_idx = 0;

    /* 1. Naive Aliased Sawtooth Sweep (440 Hz -> 5000 Hz) */
    printf("1. Rendering Naive Aliased Sawtooth Sweep (440 Hz -> 5000 Hz)...\n");
    float phase = 0.0f;
    for (int i = 0; i < sweep_samples; i++) {
        float frac = (float)i / (float)sweep_samples;
        float freq = 440.0f * powf(5000.0f / 440.0f, frac);
        float dt = freq / (float)sr;

        /* Naive saw: severe Nyquist aliasing foldover */
        float s = 2.0f * phase - 1.0f;
        phase += dt;
        if (phase >= 1.0f) phase -= 1.0f;

        out[out_idx++] = (int16_t)(sk_softclip_cubic(s * 0.85f, 1.0f) * 26000.0f);
    }

    /* 0.5s pause */
    for (int i = 0; i < sr / 2; i++) out[out_idx++] = 0;

    /* 2. PolyBLEP Anti-Aliased Sawtooth Sweep (440 Hz -> 5000 Hz) */
    printf("2. Rendering PolyBLEP Anti-Aliased Sawtooth Sweep (440 Hz -> 5000 Hz)...\n");
    sk_polyblep_osc_t osc;
    sk_polyblep_osc_init(&osc, (float)sr);
    osc.wave = SK_POLYBLEP_SAW;

    for (int i = 0; i < sweep_samples; i++) {
        float frac = (float)i / (float)sweep_samples;
        float freq = 440.0f * powf(5000.0f / 440.0f, frac);
        sk_polyblep_osc_set_freq(&osc, freq);

        float s = sk_polyblep_osc_step(&osc);
        out[out_idx++] = (int16_t)(sk_softclip_cubic(s * 0.85f, 1.0f) * 26000.0f);
    }

    /* 0.5s pause */
    for (int i = 0; i < sr / 2; i++) out[out_idx++] = 0;

    /* 3. PolyBLEP Continuous Pulse Width Modulation (PWM: 10% -> 90% -> 10% at 220 Hz) */
    printf("3. Rendering PolyBLEP Continuous PWM Sweep at 220 Hz...\n");
    osc.wave = SK_POLYBLEP_PULSE;
    sk_polyblep_osc_set_freq(&osc, 220.0f);

    for (int i = 0; i < pwm_samples; i++) {
        float frac = (float)i / (float)pwm_samples;
        /* LFO sweeping PWM between 0.10 and 0.90 */
        float pwm = 0.50f + 0.38f * sinf(frac * 4.0f * (float)M_PI);
        sk_polyblep_osc_set_pwm(&osc, pwm);

        float s = sk_polyblep_osc_step(&osc);
        out[out_idx++] = (int16_t)(sk_softclip_cubic(s * 0.85f, 1.0f) * 26000.0f);
    }

    write_wav_16("polyblep_demo.wav", out, out_idx, sr);
    free(out);
    return 0;
}
