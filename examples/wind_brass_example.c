/**
 * Physical Wind & Brass Synthesis Example
 * Demonstrates:
 * 1. Flute physical model (Perry Cook jet-drive, breath turbulence, vibrato, overblowing)
 * 2. Brass physical model (Helmholtz lip-reed valve, flared horn bell, brassy shock blare)
 *
 * Compile: gcc -O2 examples/wind_brass_example.c -lm -o wind_brass_example
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

    /* Flute melody: G4, A4, B4, C5, D5, E5 (airy, expressive, vibrato) */
    float flute_notes[] = { 392.00f, 440.00f, 493.88f, 523.25f, 587.33f, 659.25f };
    int num_flute = 6;
    int flute_note_samples = (int)(0.55f * sr);

    /* Brass fanfare: C4, G4, C5, E5, G5 (trumpet fanfare with crescendo blare) */
    float brass_notes[] = { 261.63f, 392.00f, 523.25f, 659.25f, 783.99f };
    int num_brass = 5;
    int brass_note_samples = (int)(0.60f * sr);

    int total_samples = (num_flute * flute_note_samples) + (sr / 2) +
                        (num_brass * brass_note_samples) + sr;
    int16_t *out = (int16_t *)calloc(total_samples, sizeof(int16_t));
    int out_idx = 0;

    /* 1. Render Flute Melody */
    sk_flute_t flute;
    sk_flute_init(&flute, (float)sr);

    for (int n = 0; n < num_flute; n++) {
        sk_flute_set_freq(&flute, flute_notes[n]);
        sk_flute_set_pressure(&flute, 0.70f + 0.05f * n);

        int play_samples = (int)(flute_note_samples * 0.85f);
        int rest_samples = flute_note_samples - play_samples;

        for (int i = 0; i < play_samples; i++) {
            float s = sk_flute_step(&flute);
            out[out_idx++] = (int16_t)(s * 16000.0f);
        }
        sk_flute_set_pressure(&flute, 0.0f);
        for (int i = 0; i < rest_samples; i++) {
            float s = sk_flute_step(&flute);
            out[out_idx++] = (int16_t)(s * 16000.0f);
        }
    }

    /* Inter-instrument pause (0.5s) */
    int pause_samples = sr / 2;
    for (int i = 0; i < pause_samples; i++) {
        out[out_idx++] = 0;
    }

    /* 2. Render Brass Fanfare */
    sk_brass_t brass;
    sk_brass_init(&brass, (float)sr);
    brass.brassiness = 0.65f; /* Brassy fortissimo blare */

    for (int n = 0; n < num_brass; n++) {
        sk_brass_set_freq(&brass, brass_notes[n]);
        /* Crescendo across notes */
        float pressure = 0.55f + 0.09f * (float)n;
        sk_brass_set_pressure(&brass, pressure);

        int play_samples = (int)(brass_note_samples * 0.82f);
        int rest_samples = brass_note_samples - play_samples;

        for (int i = 0; i < play_samples; i++) {
            float s = sk_brass_step(&brass);
            out[out_idx++] = (int16_t)(s * 16000.0f);
        }
        sk_brass_set_pressure(&brass, 0.0f);
        for (int i = 0; i < rest_samples; i++) {
            float s = sk_brass_step(&brass);
            out[out_idx++] = (int16_t)(s * 16000.0f);
        }
    }

    /* Decay tail */
    for (int i = 0; i < sr / 2; i++) {
        float s = sk_brass_step(&brass);
        out[out_idx++] = (int16_t)(s * 16000.0f);
    }

    write_wav_16("wind_brass_demo.wav", out, out_idx, sr);
    free(out);
    return 0;
}
