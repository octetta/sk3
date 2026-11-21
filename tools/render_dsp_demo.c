#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "../src/sk_dsp.h"

static void write_wav_file(const char *filename, const int16_t *samples, int num_samples, int sample_rate) {
    FILE *f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Could not open %s for writing\n", filename);
        return;
    }

    int num_channels = 1;
    int bits_per_sample = 16;
    int byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    int block_align = num_channels * (bits_per_sample / 8);
    int data_chunk_size = num_samples * sizeof(int16_t);
    int file_size = 36 + data_chunk_size;

    /* RIFF Header */
    fwrite("RIFF", 1, 4, f);
    fwrite(&file_size, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    /* fmt chunk */
    fwrite("fmt ", 1, 4, f);
    int subchunk1_size = 16;
    int16_t audio_format = 1; /* PCM */
    int16_t channels = num_channels;
    fwrite(&subchunk1_size, 4, 1, f);
    fwrite(&audio_format, 2, 1, f);
    fwrite(&channels, 2, 1, f);
    fwrite(&sample_rate, 4, 1, f);
    fwrite(&byte_rate, 4, 1, f);
    int16_t align = block_align;
    int16_t bps = bits_per_sample;
    fwrite(&align, 2, 1, f);
    fwrite(&bps, 2, 1, f);

    /* data chunk */
    fwrite("data", 1, 4, f);
    fwrite(&data_chunk_size, 4, 1, f);
    fwrite(samples, sizeof(int16_t), num_samples, f);

    fclose(f);
    printf("Successfully wrote %s (%d samples, %.2f seconds)\n",
           filename, num_samples, (double)num_samples / sample_rate);
}

static void render_pluck_demo(const char *filename, int sample_rate) {
    printf("Rendering Karplus-Strong string pluck demo...\n");
    float freqs[] = { 130.81f, 196.00f, 261.63f, 329.63f, 392.00f, 493.88f, 523.25f, 659.25f };
    int num_notes = sizeof(freqs) / sizeof(freqs[0]);

    float note_duration = 0.5f; /* 0.5 sec per note */
    int note_samples = (int)(note_duration * sample_rate);
    int total_samples = note_samples * num_notes + sample_rate; /* +1s decay tail */

    int16_t *out = (int16_t *)calloc(total_samples, sizeof(int16_t));

    sk_karplus_t ks;
    sk_karplus_init(&ks, (float)sample_rate);
    ks.damping = 0.35f;   /* Bright string sustain */
    ks.feedback = 0.995f; /* Long natural decay */

    sk_rng_t rng;
    sk_rng_init(&rng, 12345);

    int write_idx = 0;
    for (int n = 0; n < num_notes; n++) {
        float f = freqs[n];
        float pick_pos = 0.15f + 0.05f * (float)n; /* Vary pick position along string */
        float brightness = 0.5f + 0.06f * (float)n; /* Progressively brighter */
        sk_karplus_pluck(&ks, f, 0.9f, pick_pos, brightness, &rng);

        for (int i = 0; i < note_samples; i++) {
            float s = sk_karplus_step(&ks);
            /* Soft clip and scale to 16-bit PCM */
            s = sk_softclip_cubic(s, 1.0f);
            out[write_idx++] = (int16_t)(s * 28000.0f);
        }
    }

    /* Ring out the final note */
    for (int i = 0; i < sample_rate; i++) {
        float s = sk_karplus_step(&ks);
        s = sk_softclip_cubic(s, 1.0f);
        out[write_idx++] = (int16_t)(s * 28000.0f);
    }

    write_wav_file(filename, out, total_samples, sample_rate);
    free(out);
}

static void render_vowel_demo(const char *filename, int sample_rate) {
    printf("Rendering vocal formant & Rosenberg glottal morph demo...\n");
    float duration = 5.0f;
    int total_samples = (int)(duration * sample_rate);

    int16_t *out = (int16_t *)calloc(total_samples, sizeof(int16_t));

    sk_formant_bank_t fb;
    sk_formant_bank_init(&fb, (float)sample_rate);

    float f0 = 130.81f; /* C3 fundamental vocal pitch */
    float phase = 0.0f;
    float lfo_phase = 0.0f;

    for (int i = 0; i < total_samples; i++) {
        float t = (float)i / (float)sample_rate;

        /* Add gentle vibrato (5 Hz, ±2 Hz) */
        float vib = sinf(2.0f * (float)M_PI * 5.0f * t) * 2.5f;
        float current_f0 = f0 + vib;
        float phase_inc = current_f0 / (float)sample_rate;

        phase += phase_inc;
        if (phase >= 1.0f) phase -= 1.0f;

        /* Rosenberg glottal pulse excitation */
        float glottal = sk_glottal_pulse(phase, 0.65f);

        /* Vowel trajectory over time: U -> O -> A -> E -> I */
        /* Map progress 0.0 to 1.0 to 2D formant space */
        float progress = t / duration;
        float x, y;
        if (progress < 0.25f) {
            /* U (-0.8, -0.6) -> O (-0.6, 0.0) */
            float frac = progress / 0.25f;
            x = sk_lerp(-0.8f, -0.6f, frac);
            y = sk_lerp(-0.6f, 0.0f, frac);
        } else if (progress < 0.50f) {
            /* O (-0.6, 0.0) -> A (0.0, 0.9) */
            float frac = (progress - 0.25f) / 0.25f;
            x = sk_lerp(-0.6f, 0.0f, frac);
            y = sk_lerp(0.0f, 0.9f, frac);
        } else if (progress < 0.75f) {
            /* A (0.0, 0.9) -> E (0.5, 0.2) */
            float frac = (progress - 0.50f) / 0.25f;
            x = sk_lerp(0.0f, 0.5f, frac);
            y = sk_lerp(0.9f, 0.2f, frac);
        } else {
            /* E (0.5, 0.2) -> I (0.9, -0.7) */
            float frac = (progress - 0.75f) / 0.25f;
            x = sk_lerp(0.5f, 0.9f, frac);
            y = sk_lerp(0.2f, -0.7f, frac);
        }

        sk_formant_bank_set_xy(&fb, x, y);

        /* Filter glottal excitation through formant bank */
        float s = sk_formant_bank_step(&fb, glottal);

        /* Soft clip and scale */
        s = sk_softclip_cubic(s * 0.7f, 1.0f);
        out[i] = (int16_t)(s * 26000.0f);
    }

    write_wav_file(filename, out, total_samples, sample_rate);
    free(out);
}

static void render_speech_demo(const char *filename, int sample_rate) {
    printf("Rendering phoneme DSL speech synthesis demo...\n");
    const char *dsl = 
        "[S:80 K:50 R:80 EH:140@130 D:80] [_:300] "
        "[HH:70@115 EH:110@135 L:80@130 OW:200@105] [_:300] "
        "[W:70 IH:80 L:70] [_:100] [R:80 AA:140@120 K:70] [_:100] [Y:70 UW:220@105] [_:400]";

    printf("Compiling phoneme DSL: %s\n", dsl);
    sk_phoneme_frame_t frames[SK_PHONEME_MAX_TOKENS];
    int num_frames = sk_phoneme_compile_dsl(dsl, frames, SK_PHONEME_MAX_TOKENS, 120.0f);
    printf("Compiled into %d acoustic phoneme frames\n", num_frames);

    int max_samples = sample_rate * 8; /* 8 sec buffer */
    float *float_buf = (float *)calloc(max_samples, sizeof(float));
    sk_rng_t rng;
    sk_rng_init(&rng, 98765);

    int num_samples = sk_phoneme_render_frames(frames, num_frames, float_buf, max_samples, (float)sample_rate, &rng);
    printf("Rendered %d audio samples\n", num_samples);

    int16_t *pcm = (int16_t *)malloc(num_samples * sizeof(int16_t));
    for (int i = 0; i < num_samples; i++) {
        pcm[i] = (int16_t)(float_buf[i] * 28000.0f);
    }

    write_wav_file(filename, pcm, num_samples, sample_rate);
    free(float_buf);
    free(pcm);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int sample_rate = 48000;
    render_pluck_demo("pluck_demo.wav", sample_rate);
    render_vowel_demo("vowel_demo.wav", sample_rate);
    render_speech_demo("speech_demo.wav", sample_rate);
    printf("DSP demo rendering complete!\n");
    return 0;
}
