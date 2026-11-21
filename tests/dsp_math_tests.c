#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include "sk_dsp.h"

static int g_pass = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        exit(1); \
    } else { \
        g_pass++; \
    } \
} while (0)

static void test_clamping_and_safety(void) {
    TEST_ASSERT(sk_clampf(1.5f, 0.0f, 1.0f) == 1.0f, "sk_clampf upper bound");
    TEST_ASSERT(sk_clampf(-0.5f, 0.0f, 1.0f) == 0.0f, "sk_clampf lower bound");
    TEST_ASSERT(sk_clampf(0.5f, 0.0f, 1.0f) == 0.5f, "sk_clampf within bound");
    TEST_ASSERT(sk_clampf(NAN, 0.0f, 1.0f) == 0.0f, "sk_clampf NaN guard");

    TEST_ASSERT(sk_clampi(15, 0, 10) == 10, "sk_clampi upper bound");
    TEST_ASSERT(sk_clampi(-5, 0, 10) == 0, "sk_clampi lower bound");

    TEST_ASSERT(sk_safe_val(NAN) == 0.0, "sk_safe_val NaN");
    TEST_ASSERT(sk_safe_val(INFINITY) == 0.0, "sk_safe_val Inf");
    TEST_ASSERT(sk_safe_val(1e7) == 1e6, "sk_safe_val clamp high");
    TEST_ASSERT(sk_safe_val(-1e7) == -1e6, "sk_safe_val clamp low");
    TEST_ASSERT(sk_safe_val(42.5) == 42.5, "sk_safe_val normal");
}

static void test_pitch_and_fast_pow(void) {
    float a4 = sk_mtof(69.0f);
    TEST_ASSERT(fabsf(a4 - 440.0f) < 1e-4f, "sk_mtof(69) == 440");
    float m69 = sk_ftom(440.0f);
    TEST_ASSERT(fabsf(m69 - 69.0f) < 1e-4f, "sk_ftom(440) == 69");

    float p1 = sk_fast_pow(2.0f, 3.0f);
    TEST_ASSERT(fabsf(p1 - 8.0f) < 0.2f, "sk_fast_pow(2, 3) ~ 8");
    float p0 = sk_fast_pow(0.0f, 2.0f);
    TEST_ASSERT(p0 == 0.0f, "sk_fast_pow(0, 2) == 0");

    double w440 = sk_hz_to_phase_inc(440.0, 44100.0);
    TEST_ASSERT(fabs(w440 - (2.0 * M_PI * 440.0 / 44100.0)) < 1e-6, "sk_hz_to_phase_inc");
}

static void test_noise_and_rng(void) {
    sk_rng_t rng;
    sk_rng_init(&rng, 12345);
    for (int i = 0; i < 100; i++) {
        float r = sk_rng_float(&rng);
        TEST_ASSERT(r >= -1.0f && r <= 1.0f, "sk_rng_float in range [-1, 1]");
    }

    float m1 = sk_metallic_noise_step(10);
    TEST_ASSERT(fabsf(m1) == 0.7f, "sk_metallic_noise_step is ±0.7");
}

static void test_rotator_and_modal_bank(void) {
    sk_rotator_t rot;
    sk_rotator_init(&rot, 440.0, 1.0, 0.5, 48000.0);
    double initial = sk_rotator_step(&rot);
    TEST_ASSERT(initial == 0.0, "rotator first sample is 0.0 (sin(0))");

    double max_val = 0.0;
    for (int i = 0; i < 1000; i++) {
        double v = fabs(sk_rotator_step(&rot));
        if (v > max_val) max_val = v;
    }
    TEST_ASSERT(max_val > 0.5, "rotator generates oscillation");

    double modes[] = { 800.0, 0.5, 0.1, 1200.0, 0.3, 0.05 };
    double out[2000];
    sk_modal_bank_render(modes, 2, out, 2000, 48000.0);
    TEST_ASSERT(out[0] == 0.0, "modal bank sample 0 is 0.0");
    double pk = 0.0;
    for (int i = 0; i < 2000; i++) {
        if (fabs(out[i]) > pk) pk = fabs(out[i]);
    }
    TEST_ASSERT(pk > 0.01, "modal bank produced output");
}

static void test_filters(void) {
    sk_svf_t svf;
    sk_svf_init(&svf, 1000.0, 2.0, 48000.0);
    double bp = sk_svf_step_bp(&svf, 1.0);
    TEST_ASSERT(!isnan(bp) && !isinf(bp), "SVF step finite");

    sk_biquad_t bq;
    sk_biquad_init(&bq, SK_BIQUAD_LOWPASS, 1000.0f, 0.707f, 48000.0f);
    float out = sk_biquad_step(&bq, 1.0f);
    TEST_ASSERT(!isnan(out) && !isinf(out), "Biquad step finite");

    sk_resonant2p_t rf;
    sk_resonant2p_init(&rf, 0.3, 1.5);
    double r_out = sk_resonant2p_step(&rf, 1.0);
    TEST_ASSERT(!isnan(r_out) && !isinf(r_out), "Resonant 2p step finite");
}

static void test_waveshapers_and_cz(void) {
    float clean = sk_softclip_cubic(0.2f, 1.0f);
    TEST_ASSERT(fabsf(clean - 0.296f) < 1e-2f, "sk_softclip_cubic linear range");
    float clipped = sk_softclip_cubic(5.0f, 1.0f);
    TEST_ASSERT(fabsf(clipped - 1.0f) < 1e-4f, "sk_softclip_cubic saturation clamp");

    float q = sk_quantize(0.37f, 4.0f);
    TEST_ASSERT(q == 0.25f, "sk_quantize 4 levels");

    float p = sk_cz_phasor(1, 0.2f, 0.5f, 0.0f, 1.0f);
    TEST_ASSERT(p >= 0.0f && p <= 1.0f, "sk_cz_phasor in range [0, 1]");
}

static void test_fft(void) {
    const int N = 8;
    sk_cplx_t buf[8];
    for (int i = 0; i < N; i++) {
        buf[i].re = (float)i;
        buf[i].im = 0.0f;
    }
    sk_fft_radix2(buf, N, 0); // Forward FFT
    sk_fft_radix2(buf, N, 1); // Inverse FFT
    for (int i = 0; i < N; i++) {
        TEST_ASSERT(fabsf(buf[i].re - (float)i) < 1e-4f, "FFT -> IFFT roundtrip identity");
    }
}

static void test_buffer_tools(void) {
    double wave[100];
    for (int i = 0; i < 100; i++) wave[i] = sin(2.0 * M_PI * i / 20.0);
    int zc[10];
    int nzc = sk_find_zero_crossings(wave, 100, 0.01, zc, 10);
    TEST_ASSERT(nzc >= 8, "Zero crossings detected");

    sk_crossfade_seam(wave, 100, 10, 90, 5);
    TEST_ASSERT(!isnan(wave[89]), "Crossfade seam finite");
}

static void test_decibels_and_panning(void) {
    TEST_ASSERT(fabsf(sk_db_to_linear(0.0f) - 1.0f) < 1e-4f, "sk_db_to_linear(0) == 1");
    TEST_ASSERT(fabsf(sk_db_to_linear(-6.0206f) - 0.5f) < 1e-3f, "sk_db_to_linear(-6) ~ 0.5");
    TEST_ASSERT(fabsf(sk_linear_to_db(1.0f)) < 1e-4f, "sk_linear_to_db(1) == 0");
    TEST_ASSERT(fabsf(sk_linear_to_db(0.5f) - (-6.0206f)) < 1e-3f, "sk_linear_to_db(0.5) ~ -6");

    float l, r;
    sk_pan_equal_power(-1.0f, &l, &r);
    TEST_ASSERT(fabsf(l - 1.0f) < 1e-4f && fabsf(r) < 1e-4f, "Equal power pan full left");
    sk_pan_equal_power(1.0f, &l, &r);
    TEST_ASSERT(fabsf(l) < 1e-4f && fabsf(r - 1.0f) < 1e-4f, "Equal power pan full right");
    sk_pan_equal_power(0.0f, &l, &r);
    TEST_ASSERT(fabsf(l * l + r * r - 1.0f) < 1e-4f, "Equal power pan center sum of squares == 1");

    sk_pan_linear(0.0f, &l, &r);
    TEST_ASSERT(fabsf(l - 0.5f) < 1e-4f && fabsf(r - 0.5f) < 1e-4f, "Linear pan center");
}

static void test_retro_lfsr_noise(void) {
    sk_lfsr_noise_t n;
    /* Test all presets 0..6 */
    for (int t = 0; t <= 6; t++) {
        sk_lfsr_noise_init(&n, t);
        for (int i = 0; i < 50; i++) {
            float s = sk_lfsr_noise_step(&n);
            TEST_ASSERT(s >= -1.0f && s <= 1.0f, "LFSR noise sample bounded");
        }
    }
    /* Custom LFSR */
    sk_lfsr_noise_set_custom(&n, 12, 5, 1);
    TEST_ASSERT(n.bits == 12 && n.tap == 5 && n.dac == 1, "Custom LFSR config");
    float cs = sk_lfsr_noise_step(&n);
    TEST_ASSERT(cs >= -1.0f && cs <= 1.0f, "Custom LFSR step bounded");
}

static void test_companded_and_dithered_quantize(void) {
    float q_lin = sk_quantize_bits_curve(0.35f, 4, 0, NULL);
    TEST_ASSERT(q_lin >= 0.0f && q_lin <= 1.0f, "Linear quantize bounded");

    float q_comp = sk_quantize_bits_curve(0.35f, 4, 1, NULL);
    TEST_ASSERT(q_comp >= 0.0f && q_comp <= 1.0f, "Companded quantize bounded");

    uint64_t rng = 987654321ULL;
    float q_dith = sk_quantize_bits_curve(0.35f, 4, 2, &rng);
    TEST_ASSERT(q_dith >= -1.0f && q_dith <= 1.0f, "Dithered quantize bounded");
}

static void test_delay_tools(void) {
    float d0 = sk_delay_damping_coeff(0.0f, 48000.0f);
    float d1 = sk_delay_damping_coeff(1.0f, 48000.0f);
    TEST_ASSERT(d0 > 0.0f && d1 > 0.0f && d0 != d1, "Damping coefficients distinct");

    float hp0 = sk_delay_hp_coeff(0.0f, 48000.0f);
    float hp1 = sk_delay_hp_coeff(1.0f, 48000.0f);
    TEST_ASSERT(hp0 > 0.0f && hp1 > 0.0f && hp0 != hp1, "HP coefficients distinct");

    float ring_buf[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    float s = sk_delay_read_linear(ring_buf, 16, 5, 2.5f);
    /* write_pos 5, delay 2.5 => read 2.5 => lerp(buf[2], buf[3], 0.5) = 2.5 */
    TEST_ASSERT(fabsf(s - 2.5f) < 1e-4f, "sk_delay_read_linear fractional read");
}

static void test_glissando_tools(void) {
    float delta, speed;
    sk_glissando_calc(1, 0.01f, 0.02f, 10.0f, &delta, &speed);
    TEST_ASSERT(fabsf(delta - 0.001f) < 1e-5f, "glissando linear delta");

    float cur = 0.01f;
    int active = sk_glissando_step(&cur, 0.02f, 1, delta, speed);
    TEST_ASSERT(active == 1, "glissando step active");
    TEST_ASSERT(cur > 0.01f, "glissando incremented");
}

static void test_karplus_strong(void) {
    sk_karplus_t ks;
    sk_karplus_init(&ks, 48000.0f);
    TEST_ASSERT(ks.size == SK_KARPLUS_MAX_LEN, "karplus init size");

    sk_rng_t rng;
    sk_rng_init(&rng, 54321);

    /* Pluck at 440 Hz */
    sk_karplus_pluck(&ks, 440.0f, 0.8f, 0.2f, 0.7f, &rng);
    TEST_ASSERT(ks.int_delay > 0, "karplus tuned int_delay");

    /* Render 2000 samples and verify decay and stability */
    float max_amp = 0.0f;
    float last_amp = 0.0f;
    for (int i = 0; i < 2000; i++) {
        float s = sk_karplus_step(&ks);
        TEST_ASSERT(!isnan(s) && !isinf(s), "karplus output is finite");
        TEST_ASSERT(fabsf(s) <= 2.0f, "karplus output bounded");
        if (fabsf(s) > max_amp) max_amp = fabsf(s);
        if (i == 1999) last_amp = fabsf(s);
    }
    TEST_ASSERT(max_amp > 0.1f, "karplus produced oscillation");
    TEST_ASSERT(last_amp < max_amp, "karplus decayed over time");
}

static void test_formants_and_glottal(void) {
    /* Test Rosenberg glottal pulse */
    float g0 = sk_glottal_pulse(0.0f, 0.6f);
    TEST_ASSERT(g0 == 0.0f, "glottal pulse starts at 0");
    float g_mid = sk_glottal_pulse(0.3f, 0.6f);
    TEST_ASSERT(g_mid > 0.0f && g_mid <= 1.0f, "glottal pulse open phase");
    float g_closed = sk_glottal_pulse(0.8f, 0.6f);
    TEST_ASSERT(g_closed == 0.0f, "glottal pulse closed phase");

    /* Test Formant Bank canonical vowels */
    sk_formant_bank_t fb;
    sk_formant_bank_init(&fb, 48000.0f);
    for (int v = 0; v < SK_VOWEL_COUNT; v++) {
        sk_formant_bank_set_vowel(&fb, (sk_vowel_id_t)v);
        float out = sk_formant_bank_step(&fb, 1.0f);
        TEST_ASSERT(!isnan(out) && !isinf(out), "formant step finite for vowel");
    }

    /* Test 2D XY morphing */
    sk_formant_bank_set_xy(&fb, -0.5f, 0.5f);
    float out_xy = sk_formant_bank_step(&fb, 0.8f);
    TEST_ASSERT(!isnan(out_xy) && !isinf(out_xy), "formant step finite for XY morph");
}

static void test_phoneme_dsl(void) {
    sk_phoneme_frame_t frames[16];
    const char *dsl = "[S:50 K:40 R:60 EH:100@130 D:50]";
    int n = sk_phoneme_compile_dsl(dsl, frames, 16, 120.0f);
    TEST_ASSERT(n == 5, "compiled 5 phonemes from DSL");
    TEST_ASSERT((frames[0].flags & SK_PHO_FRIC) != 0, "frame 0 is fricative");
    TEST_ASSERT(frames[3].pitch_hz == 130.0f, "frame 3 pitch is 130");
    TEST_ASSERT((frames[4].flags & SK_PHO_STOP) != 0, "frame 4 is stop plosive");

    float out[4800];
    sk_rng_t rng;
    sk_rng_init(&rng, 1111);
    int samples = sk_phoneme_render_frames(frames, n, out, 4800, 48000.0f, &rng);
    TEST_ASSERT(samples > 0, "rendered speech samples");
    for (int i = 0; i < samples; i++) {
        TEST_ASSERT(!isnan(out[i]) && !isinf(out[i]), "speech samples finite");
        TEST_ASSERT(fabsf(out[i]) <= 1.0f, "speech samples clamped");
    }
}

int main(void) {
    printf("Running sk_dsp math test suite...\n");
    test_clamping_and_safety();
    test_pitch_and_fast_pow();
    test_noise_and_rng();
    test_rotator_and_modal_bank();
    test_filters();
    test_waveshapers_and_cz();
    test_fft();
    test_buffer_tools();
    test_decibels_and_panning();
    test_retro_lfsr_noise();
    test_companded_and_dithered_quantize();
    test_delay_tools();
    test_glissando_tools();
    test_karplus_strong();
    test_formants_and_glottal();
    test_phoneme_dsl();
    printf("All %d sk_dsp checks passed successfully!\n", g_pass);
    return 0;
}
