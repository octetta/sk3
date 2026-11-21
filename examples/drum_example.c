/**
 * Real-Time Percussion Synthesizer Example
 * Demonstrates analog 808/909 drum modeling (Kick, Snare, Closed/Open Hats, Clap, Cowbell, Tom)
 * and physical Karplus-Strong drum membrane synthesis.
 *
 * Compile: gcc -O2 examples/drum_example.c -lm -o drum_example
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
    float bpm = 124.0f;
    float step_sec = (60.0f / bpm) / 4.0f; /* 16th note step */
    int step_samples = (int)(step_sec * (float)sr);
    int total_steps = 32; /* 2 bars of 16-step beat */
    int total_samples = step_samples * total_steps + sr; /* +1s tail */

    int16_t *out = (int16_t *)calloc(total_samples, sizeof(int16_t));

    /* Initialize drum synthesizer voices */
    sk_drum_kick_t kick;
    sk_drum_snare_t snare;
    sk_drum_hat_t hat;
    sk_drum_clap_t clap;
    sk_drum_cowbell_t cowbell;
    sk_drum_tom_t tom;
    sk_karplus_t ks_drum;

    sk_drum_kick_init(&kick, (float)sr);
    sk_drum_snare_init(&snare, (float)sr);
    sk_drum_hat_init(&hat, (float)sr);
    sk_drum_clap_init(&clap, (float)sr);
    sk_drum_cowbell_init(&cowbell, (float)sr);
    sk_drum_tom_init(&tom, (float)sr, 110.0f);
    sk_karplus_init(&ks_drum, (float)sr);

    /* 16-step rhythmic patterns (2 bars) */
    /* Kick: four-on-the-floor with syncopation */
    const int kick_pat[32]  = { 1, 0, 0, 0,  1, 0, 0, 1,  0, 0, 1, 0,  0, 1, 0, 0,
                                1, 0, 0, 0,  1, 0, 0, 1,  0, 0, 1, 0,  0, 0, 1, 0 };
    /* Snare / Clap on 5 and 13 */
    const int snare_pat[32] = { 0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 0,
                                0, 0, 0, 0,  1, 0, 0, 0,  0, 0, 0, 0,  1, 0, 0, 1 };
    /* Hi-Hat: 1=closed, 2=open, 0=none */
    const int hat_pat[32]   = { 1, 0, 1, 0,  1, 0, 2, 0,  1, 0, 1, 0,  1, 0, 2, 0,
                                1, 0, 1, 0,  1, 0, 2, 0,  1, 1, 1, 0,  1, 0, 2, 0 };
    /* Cowbell accents */
    const int cb_pat[32]    = { 0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 1, 0,  0, 0, 1, 0,
                                0, 0, 0, 1,  0, 0, 0, 0,  0, 0, 1, 0,  0, 1, 0, 0 };
    /* Tom fills */
    const int tom_pat[32]   = { 0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,
                                0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 1, 1, 0 };

    int out_idx = 0;
    for (int step = 0; step < total_steps; step++) {
        /* Triggers for this step */
        if (kick_pat[step]) sk_drum_kick_trigger(&kick, 0.95f);
        if (snare_pat[step]) {
            sk_drum_snare_trigger(&snare, 0.85f);
            if (step % 8 == 4) sk_drum_clap_trigger(&clap, 0.70f);
        }
        if (hat_pat[step] == 1) {
            sk_drum_hat_choke(&hat); /* Closed hat chokes any ringing open hat */
            sk_drum_hat_trigger(&hat, 0.65f, 0);
        } else if (hat_pat[step] == 2) {
            sk_drum_hat_trigger(&hat, 0.75f, 1);
        }
        if (cb_pat[step]) sk_drum_cowbell_trigger(&cowbell, 0.70f);
        if (tom_pat[step]) sk_drum_tom_trigger(&tom, 0.80f);

        /* Step audio samples */
        for (int i = 0; i < step_samples; i++) {
            float s_kick = sk_drum_kick_step(&kick);
            float s_snare = sk_drum_snare_step(&snare);
            float s_hat = sk_drum_hat_step(&hat);
            float s_clap = sk_drum_clap_step(&clap);
            float s_cb = sk_drum_cowbell_step(&cowbell);
            float s_tom = sk_drum_tom_step(&tom);

            float mix = (s_kick * 0.8f) +
                        (s_snare * 0.7f) +
                        (s_hat * 0.45f) +
                        (s_clap * 0.5f) +
                        (s_cb * 0.4f) +
                        (s_tom * 0.6f);

            /* Master saturation bus */
            float out_sample = sk_softclip_cubic(mix * 0.40f, 1.0f);
            out[out_idx++] = (int16_t)(out_sample * 25000.0f);
        }
    }

    /* Render ringing tail */
    for (int i = 0; i < sr; i++) {
        float mix = (sk_drum_kick_step(&kick) * 0.8f) +
                    (sk_drum_snare_step(&snare) * 0.7f) +
                    (sk_drum_hat_step(&hat) * 0.45f) +
                    (sk_drum_clap_step(&clap) * 0.5f) +
                    (sk_drum_cowbell_step(&cowbell) * 0.4f) +
                    (sk_drum_tom_step(&tom) * 0.6f);
        float out_sample = sk_softclip_cubic(mix * 0.40f, 1.0f);
        out[out_idx++] = (int16_t)(out_sample * 25000.0f);
    }

    write_wav_16("drum_demo.wav", out, out_idx, sr);
    free(out);
    return 0;
}
