/**
 * Karplus-Strong String Pluck Synthesis Example
 * Demonstrates physically-modeled string plucks with variable pick position and brightness.
 * Compile: gcc -O2 examples/pluck_example.c -lm -o pluck_example
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
    float notes[] = { 130.81f, 196.00f, 261.63f, 329.63f, 392.00f, 493.88f, 523.25f, 659.25f };
    int num_notes = 8;
    int note_len = (int)(0.45f * sr);
    int total_len = note_len * num_notes + sr;

    int16_t *out = (int16_t *)calloc(total_len, sizeof(int16_t));
    sk_karplus_t ks;
    sk_karplus_init(&ks, (float)sr);
    ks.damping = 0.35f;
    ks.feedback = 0.995f;

    sk_rng_t rng;
    sk_rng_init(&rng, 12345);

    int idx = 0;
    for (int n = 0; n < num_notes; n++) {
        float pick_pos = 0.15f + 0.05f * n;
        float brightness = 0.5f + 0.06f * n;
        sk_karplus_pluck(&ks, notes[n], 0.9f, pick_pos, brightness, &rng);
        for (int i = 0; i < note_len; i++) {
            float s = sk_karplus_step(&ks);
            out[idx++] = (int16_t)(sk_softclip_cubic(s, 1.0f) * 28000.0f);
        }
    }
    for (int i = 0; i < sr; i++) {
        float s = sk_karplus_step(&ks);
        out[idx++] = (int16_t)(sk_softclip_cubic(s, 1.0f) * 28000.0f);
    }

    write_wav_16("pluck_demo.wav", out, total_len, sr);
    free(out);
    return 0;
}
