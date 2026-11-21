/**
 * Phonetic DSL Speech Synthesizer Example
 * Demonstrates speech synthesis from bracketed DSL strings across DECtalk, SAM, and Kraftwerk voice personas.
 * Compile: gcc -O2 examples/speech_example.c -lm -o speech_example
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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
    const char *phrase = 
        "[HH:70 EH1:110@125>135 L:70 OW1:150@120>100] [_:150] "
        "[AY1:110@125] [_:60] [AE1:90@120 M:60] [_:80] "
        "[S:70 P:50 IY1:120@130 K:60 IH0:70 NG:70] [_:350]";

    printf("Compiling Phonetic DSL: %s\n", phrase);
    sk_phoneme_frame_t frames[SK_PHONEME_MAX_TOKENS];
    int num_frames = sk_phoneme_compile_dsl(phrase, frames, SK_PHONEME_MAX_TOKENS, 120.0f);
    printf("Compiled %d acoustic frames\n", num_frames);

    /* Render with DECtalk 'Perfect Paul' */
    sk_speech_voice_t paul = sk_speech_voice_paul();
    int buf_len = sr * 5;
    float *float_buf = (float *)calloc(buf_len, sizeof(float));
    sk_rng_t rng;
    sk_rng_init(&rng, 98765);

    int samples = sk_phoneme_render_frames_voice(frames, num_frames, float_buf, buf_len, (float)sr, &paul, &rng);
    int16_t *pcm = (int16_t *)malloc(samples * sizeof(int16_t));
    for (int i = 0; i < samples; i++) {
        pcm[i] = (int16_t)(float_buf[i] * 28000.0f);
    }

    write_wav_16("hello_paul.wav", pcm, samples, sr);
    free(float_buf);
    free(pcm);
    return 0;
}
