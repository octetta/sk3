/**
 * 2D Vocal Formant Morphing Example
 * Demonstrates continuous vocal tract morphing across the vowel space (/u/ -> /o/ -> /a/ -> /e/ -> /i/).
 * Compile: gcc -O2 examples/vowel_example.c -lm -o vowel_example
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
    float dur = 5.0f;
    int total_samples = (int)(dur * sr);
    int16_t *out = (int16_t *)calloc(total_samples, sizeof(int16_t));

    sk_formant_bank_t fb;
    sk_formant_bank_init(&fb, (float)sr);

    float f0 = 130.81f; /* C3 vocal pitch */
    float phase = 0.0f;
    float prev_glottal = 0.0f;

    for (int i = 0; i < total_samples; i++) {
        float t = (float)i / (float)sr;
        float current_f0 = f0 + sinf(2.0f * (float)M_PI * 5.0f * t) * 2.5f; /* 5 Hz vibrato */
        phase += current_f0 / (float)sr;
        if (phase >= 1.0f) phase -= 1.0f;

        float glottal = sk_glottal_pulse(phase, 0.65f);
        float glottal_diff = glottal - prev_glottal;
        prev_glottal = glottal;

        /* 2D Vowel trajectory: U (-0.8, -0.6) -> O (-0.6, 0.0) -> A (0.0, 0.9) -> E (0.5, 0.2) -> I (0.9, -0.7) */
        float p = t / dur;
        float x, y;
        if (p < 0.25f) {
            float fr = p / 0.25f;
            x = sk_lerp(-0.8f, -0.6f, fr); y = sk_lerp(-0.6f, 0.0f, fr);
        } else if (p < 0.50f) {
            float fr = (p - 0.25f) / 0.25f;
            x = sk_lerp(-0.6f, 0.0f, fr); y = sk_lerp(0.0f, 0.9f, fr);
        } else if (p < 0.75f) {
            float fr = (p - 0.50f) / 0.25f;
            x = sk_lerp(0.0f, 0.5f, fr); y = sk_lerp(0.9f, 0.2f, fr);
        } else {
            float fr = (p - 0.75f) / 0.25f;
            x = sk_lerp(0.5f, 0.9f, fr); y = sk_lerp(0.2f, -0.7f, fr);
        }
        sk_formant_bank_set_xy(&fb, x, y);

        /* Filter differentiated glottal pulse through formant bank */
        float s = sk_formant_bank_step(&fb, glottal_diff * 26.0f) * 1.4f + glottal_diff * 1.0f;
        out[i] = (int16_t)(sk_softclip_cubic(s * 3.6f, 1.0f) * 24000.0f);
    }

    write_wav_16("vowel_demo.wav", out, total_samples, sr);
    free(out);
    return 0;
}
