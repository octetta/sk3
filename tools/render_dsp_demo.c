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

static void render_phoneme_demo(const char *filename, int sample_rate) {
    printf("Rendering full phoneme inventory catalog demo...\n");
    const char *dsl =
        /* 1. Monophthong Vowels */
        "[IY:240] [_:80] [IH:240] [_:80] [EH:240] [_:80] [AE:240] [_:80] "
        "[AA:240] [_:80] [AH:240] [_:80] [AO:240] [_:80] [UH:240] [_:80] "
        "[UW:240] [_:80] [ER:240] [_:250] "
        /* 2. Diphthongs */
        "[EY:260] [_:80] [AY:260] [_:80] [OY:260] [_:80] [AW:260] [_:80] [OW:260] [_:250] "
        /* 3. Nasals */
        "[M:250] [_:80] [N:250] [_:80] [NG:250] [_:250] "
        /* 4. Liquids & Semivowels / Glides */
        "[L:250] [_:80] [R:250] [_:80] [W:250] [_:80] [Y:250] [_:250] "
        /* 5. Fricatives (Voiceless & Voiced) */
        "[F:220] [_:80] [V:220] [_:80] [TH:220] [_:80] [DH:220] [_:80] "
        "[S:220] [_:80] [Z:220] [_:80] [SH:220] [_:80] [ZH:220] [_:80] [HH:220] [_:250] "
        /* 6. Stops / Plosives (with vowel release) */
        "[P:60 AA:150] [_:80] [B:60 AA:150] [_:80] [T:60 AA:150] [_:80] "
        "[D:60 AA:150] [_:80] [K:60 AA:150] [_:80] [G:60 AA:150] [_:250] "
        /* 7. Affricates */
        "[CH:70 AA:150] [_:80] [JH:70 AA:150] [_:400]";

    printf("Compiling phoneme catalog DSL...\n");
    sk_phoneme_frame_t frames[SK_PHONEME_MAX_TOKENS];
    int num_frames = sk_phoneme_compile_dsl(dsl, frames, SK_PHONEME_MAX_TOKENS, 120.0f);
    printf("Compiled %d catalog frames\n", num_frames);

    int max_samples = sample_rate * 20; /* 20 sec buffer */
    float *float_buf = (float *)calloc(max_samples, sizeof(float));
    sk_rng_t rng;
    sk_rng_init(&rng, 13579);

    int num_samples = sk_phoneme_render_frames(frames, num_frames, float_buf, max_samples, (float)sample_rate, &rng);
    printf("Rendered %d audio samples for phoneme catalog\n", num_samples);

    int16_t *pcm = (int16_t *)malloc(num_samples * sizeof(int16_t));
    for (int i = 0; i < num_samples; i++) {
        pcm[i] = (int16_t)(float_buf[i] * 28000.0f);
    }

    write_wav_file(filename, pcm, num_samples, sample_rate);
    free(float_buf);
    free(pcm);
}

static void render_speech_demo(const char *filename, int sample_rate) {
    printf("Rendering historic speech synthesis showcase demo...\n");
    const char *dsl = 
        /* Part 1: Baseline phrases: "SKRED", "HELLO", "WE WILL ROCK YOU" */
        "[S:80 K:50 R:80 EH1:140@130>115 D:80] [_:350] "
        "[HH:70@115 EH1:110@130>138 L:80@135 OW1:200@115>95] [_:350] "
        "[W:70 IH0:80 L:70] [_:100] [R:80 AA1:140@125>115 K:70] [_:100] [Y:70 UW1:220@110>95] [_:600] "

        /* Part 2: Bell Labs 1939 Voder (Homer Dudley / Pedro the Voder) */
        /* "GOOD AFTERNOON RADIO AUDIENCE" with wrist-bar pitch inflection */
        "[G:60 UH1:90@100>108 D:50] [_:60] "
        "[AE1:110@108>118 F:70 T:50 ER0:100@125>135 N:60 UW1:160@142>125 N:70] [_:120] "
        "[R:70 EY1:110@130>122 D:50 IY0:90@120>112 OW1:150@112>100] [_:120] "
        "[AA1:120@126>118 D:50 IY0:90@115 AH0:80@105 N:60 S:120@100>90] [_:350] "
        /* Historic Voder test phrase: "SHE SAW A SHOT" */
        "[SH:130 IY1:150@135>142] [_:80] "
        "[S:120 AO1:160@130>120] [_:80] "
        "[AH0:90@115] [_:70] "
        "[SH:120 AA1:160@115>95 T:70] [_:600] "

        /* Part 3: 1961 IBM 7094 / Max Mathews - "DAISY BELL" */
        /* Sung melody with exact pitch overrides and vocal vibrato */
        "[D:60 EY1:180@349~ Z:60 IY1:220@294~] [_:140] " /* Dai - sy (F4, D4) */
        "[D:60 EY1:180@233~ Z:60 IY1:220@175~] [_:160] " /* Dai - sy (Bb3, F3) */
        "[G:60 IH0:130@196 V:50] [_:60] "                /* give (G3) */
        "[M:60 IY1:140@220~] [_:60] "                     /* me (A3) */
        "[Y:60 UH0:100@233 R:60] [_:60] "                /* your (Bb3) */
        "[AE1:120@262 N:50] [S:60 ER0:130@294~] [_:80] " /* an - swer (C4, D4) */
        "[D:70 UW1:320@233~] [_:600] "                    /* do (Bb3) */

        /* Part 4: Kraftwerk tribute - "I AM THE OPERATOR WITH MY POCKET CALCULATOR" + "ONE TWO THREE FOUR" */
        /* Strict monotone robotic pitch @ 110 Hz */
        "[AY1:120@110] [_:60] [AE0:100@110 M:70] [_:60] "
        "[DH:50 AH0:70@110] [_:50] "
        "[AA1:90@110 P:50 ER0:80@110 EY1:100@110 T:50 ER0:110@110] [_:80] "
        "[W:50 IH0:80@110 TH:60] [_:60] [M:50 AY1:110@110] [_:60] "
        "[P:50 AA1:90@110 K:50 AH0:70@110 T:50] [_:60] "
        "[K:50 AE1:90@110 L:50 K:50 Y:50 UH0:80@110 L:50 EY1:100@110 T:50 ER0:140@110] [_:300] "
        "[W:60 AH1:110@110 N:70] [_:140] "
        "[T:60 UW1:130@110] [_:140] "
        "[TH:70 R:60 IY1:140@110] [_:140] "
        "[F:70 AO1:120@110 R:70] [_:500]";

    printf("Compiling historic speech DSL...\n");
    sk_phoneme_frame_t frames[SK_PHONEME_MAX_TOKENS];
    int num_frames = sk_phoneme_compile_dsl(dsl, frames, SK_PHONEME_MAX_TOKENS, 120.0f);
    printf("Compiled %d speech frames\n", num_frames);

    int max_samples = sample_rate * 35; /* 35 sec buffer */
    float *float_buf = (float *)calloc(max_samples, sizeof(float));
    sk_rng_t rng;
    sk_rng_init(&rng, 98765);

    int num_samples = sk_phoneme_render_frames(frames, num_frames, float_buf, max_samples, (float)sample_rate, &rng);
    printf("Rendered %d speech audio samples (%.2f seconds)\n", num_samples, (double)num_samples / sample_rate);

    int16_t *pcm = (int16_t *)malloc(num_samples * sizeof(int16_t));
    for (int i = 0; i < num_samples; i++) {
        pcm[i] = (int16_t)(float_buf[i] * 28000.0f);
    }

    write_wav_file(filename, pcm, num_samples, sample_rate);
    free(float_buf);
    free(pcm);
}

static void render_voices_demo(const char *filename, int sample_rate) {
    printf("Rendering DECtalk & SAM voice personas demo...\n");
    const char *phrase = 
        "[HH:70 EH1:110@125>135 L:70 OW1:150@120>100] [_:150] "
        "[AY1:110@125] [_:60] [AE1:90@120 M:60] [_:80] "
        "[S:70 P:50 IY1:120@130 K:60 IH0:70 NG:70] [_:350]";

    sk_phoneme_frame_t frames[SK_PHONEME_MAX_TOKENS];
    int num_frames = sk_phoneme_compile_dsl(phrase, frames, SK_PHONEME_MAX_TOKENS, 120.0f);

    struct {
        const char *name;
        sk_speech_voice_t voice;
    } personas[] = {
        { "Perfect Paul (DECtalk Male)", sk_speech_voice_paul() },
        { "Beautiful Betty (DECtalk Female)", sk_speech_voice_betty() },
        { "Huge Harry (DECtalk Deep Male)", sk_speech_voice_harry() },
        { "SAM (1982 Commodore 64 / SoftVoice)", sk_speech_voice_sam() },
        { "Kit the Kid (Child / Elf)", sk_speech_voice_elf() },
        { "Robot Monotone (Kraftwerk)", sk_speech_voice_robot() }
    };
    int num_personas = sizeof(personas) / sizeof(personas[0]);

    int max_total_samples = sample_rate * 25;
    float *total_float = (float *)calloc(max_total_samples, sizeof(float));
    int total_write_idx = 0;

    sk_rng_t rng;
    sk_rng_init(&rng, 54321);

    for (int p = 0; p < num_personas; p++) {
        printf("  Rendering voice %d/%d: %s\n", p + 1, num_personas, personas[p].name);
        int buf_len = sample_rate * 4;
        float *buf = (float *)calloc(buf_len, sizeof(float));
        int samples = sk_phoneme_render_frames_voice(frames, num_frames, buf, buf_len, (float)sample_rate, &personas[p].voice, &rng);

        for (int i = 0; i < samples && total_write_idx < max_total_samples; i++) {
            total_float[total_write_idx++] = buf[i];
        }
        /* Inter-persona silence */
        int pause_samples = (int)(0.4f * sample_rate);
        total_write_idx += pause_samples;
        if (total_write_idx > max_total_samples) total_write_idx = max_total_samples;
        free(buf);
    }

    int16_t *pcm = (int16_t *)malloc(total_write_idx * sizeof(int16_t));
    for (int i = 0; i < total_write_idx; i++) {
        pcm[i] = (int16_t)(total_float[i] * 28000.0f);
    }

    write_wav_file(filename, pcm, total_write_idx, sample_rate);
    free(total_float);
    free(pcm);
}

static void render_percussion_demo(const char *filename, int sample_rate) {
    printf("Rendering real-time percussion demo (808/909 voices + Karplus-Strong drum)...\n");

    float bpm = 124.0f;
    float step_sec = (60.0f / bpm) / 4.0f; /* 16th note step = 120.9 ms */
    int step_samples = (int)(step_sec * (float)sample_rate);
    int total_steps = 64; /* 4 bars */
    int total_samples = step_samples * total_steps + sample_rate * 2;

    int16_t *out = (int16_t *)calloc(total_samples, sizeof(int16_t));

    sk_drum_kick_t kick;
    sk_drum_snare_t snare;
    sk_drum_hat_t hat;
    sk_drum_clap_t clap;
    sk_drum_cowbell_t cowbell;
    sk_drum_tom_t tom_lo, tom_mid, tom_hi;
    sk_karplus_t ks_drum;

    sk_drum_kick_init(&kick, (float)sample_rate);
    sk_drum_snare_init(&snare, (float)sample_rate);
    sk_drum_hat_init(&hat, (float)sample_rate);
    sk_drum_clap_init(&clap, (float)sample_rate);
    sk_drum_cowbell_init(&cowbell, (float)sample_rate);
    sk_drum_tom_init(&tom_lo, (float)sample_rate, 85.0f);
    sk_drum_tom_init(&tom_mid, (float)sample_rate, 130.0f);
    sk_drum_tom_init(&tom_hi, (float)sample_rate, 195.0f);
    sk_karplus_init(&ks_drum, (float)sample_rate);

    /* 64-step pattern:
     * Bar 1-2: Kick, snare, hats, cowbell groove
     * Bar 3: Add claps and tom polyrhythms
     * Bar 4: Karplus-Strong physical drum membrane fills & roll */
    const int kick_pat[64] = {
        1, 0, 0, 0,  1, 0, 0, 1,  0, 0, 1, 0,  0, 1, 0, 0,
        1, 0, 0, 0,  1, 0, 0, 1,  0, 0, 1, 0,  0, 0, 1, 0,
        1, 0, 0, 0,  1, 0, 0, 1,  0, 0, 1, 0,  0, 1, 0, 0,
        1, 0, 0, 1,  1, 0, 1, 0,  1, 1, 0, 0,  0, 0, 0, 0
    };
    const int snare_pat[64] = {
        0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,
        0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 1,
        0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,
        0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 1, 0,  1, 0, 1, 1
    };
    const int hat_pat[64] = {
        1, 0, 1, 0,  1, 0, 2, 0,  1, 0, 1, 0,  1, 0, 2, 0,
        1, 0, 1, 0,  1, 0, 2, 0,  1, 1, 1, 0,  1, 0, 2, 0,
        1, 0, 1, 0,  1, 0, 2, 0,  1, 0, 1, 0,  1, 0, 2, 0,
        1, 1, 1, 1,  1, 1, 2, 0,  1, 0, 1, 0,  0, 0, 2, 0
    };
    const int cb_pat[64] = {
        0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 1, 0,
        0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 1, 0,  0, 1, 0, 0,
        0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 1, 0,
        0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
    };
    const int clap_pat[64] = {
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,
        0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0
    };
    const int tom_pat[64] = {
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 2, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  1, 2, 3, 0,  1, 2, 3, 0
    };
    const int ks_drum_pat[64] = {
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
        0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1,  0, 1, 1, 1
    };

    int out_idx = 0;
    for (int step = 0; step < total_steps; step++) {
        if (kick_pat[step]) sk_drum_kick_trigger(&kick, 0.95f);
        if (snare_pat[step]) sk_drum_snare_trigger(&snare, 0.85f);
        if (clap_pat[step]) sk_drum_clap_trigger(&clap, 0.75f);
        if (cb_pat[step]) sk_drum_cowbell_trigger(&cowbell, 0.65f);

        if (hat_pat[step] == 1) {
            sk_drum_hat_choke(&hat);
            sk_drum_hat_trigger(&hat, 0.60f, 0);
        } else if (hat_pat[step] == 2) {
            sk_drum_hat_trigger(&hat, 0.70f, 1);
        }

        if (tom_pat[step] == 1) sk_drum_tom_trigger(&tom_lo, 0.85f);
        else if (tom_pat[step] == 2) sk_drum_tom_trigger(&tom_mid, 0.85f);
        else if (tom_pat[step] == 3) sk_drum_tom_trigger(&tom_hi, 0.85f);

        if (ks_drum_pat[step]) {
            sk_karplus_strike_drum(&ks_drum, 95.0f, 0.90f, 0.50f, 0.70f, NULL);
        }

        for (int i = 0; i < step_samples; i++) {
            float s_k = sk_drum_kick_step(&kick);
            float s_s = sk_drum_snare_step(&snare);
            float s_h = sk_drum_hat_step(&hat);
            float s_c = sk_drum_clap_step(&clap);
            float s_b = sk_drum_cowbell_step(&cowbell);
            float s_t = sk_drum_tom_step(&tom_lo) + sk_drum_tom_step(&tom_mid) + sk_drum_tom_step(&tom_hi);
            float s_kd = sk_karplus_step(&ks_drum);

            float mix = (s_k * 0.85f) +
                        (s_s * 0.70f) +
                        (s_h * 0.45f) +
                        (s_c * 0.50f) +
                        (s_b * 0.40f) +
                        (s_t * 0.55f) +
                        (s_kd * 0.65f);

            float sample = sk_softclip_cubic(mix, 1.0f);
            out[out_idx++] = (int16_t)(sample * 26000.0f);
        }
    }

    /* 1.5s decay tail */
    int tail_samples = (int)(1.5f * sample_rate);
    for (int i = 0; i < tail_samples; i++) {
        float mix = (sk_drum_kick_step(&kick) * 0.85f) +
                    (sk_drum_snare_step(&snare) * 0.70f) +
                    (sk_drum_hat_step(&hat) * 0.45f) +
                    (sk_drum_clap_step(&clap) * 0.50f) +
                    (sk_drum_cowbell_step(&cowbell) * 0.40f) +
                    (sk_drum_tom_step(&tom_lo) * 0.55f) +
                    (sk_karplus_step(&ks_drum) * 0.65f);
        float sample = sk_softclip_cubic(mix, 1.0f);
        out[out_idx++] = (int16_t)(sample * 26000.0f);
    }

    write_wav_file(filename, out, out_idx, sample_rate);
    free(out);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int sample_rate = 48000;
    render_pluck_demo("pluck_demo.wav", sample_rate);
    render_percussion_demo("percussion_demo.wav", sample_rate);
    render_vowel_demo("vowel_demo.wav", sample_rate);
    render_phoneme_demo("phoneme_demo.wav", sample_rate);
    render_speech_demo("speech_demo.wav", sample_rate);
    render_voices_demo("voices_demo.wav", sample_rate);
    printf("DSP demo rendering complete!\n");
    return 0;
}
