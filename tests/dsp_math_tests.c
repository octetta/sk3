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
    printf("All %d sk_dsp checks passed successfully!\n", g_pass);
    return 0;
}
