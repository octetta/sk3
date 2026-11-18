/*
 * Hardware LFSR Noise Generator (w5 + NT opcode) tests.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "api.h"
#include "skode.h"
#include "synth.h"
#include "synth-types.h"

static int failures;

static void expect(int condition, const char *message) {
  if (condition) return;
  fprintf(stderr, "FAIL noise test: %s\n", message);
  failures++;
}

static void setup(void) {
  synth_init(8);
  wave_table_init(0);
  voice_init();
  synth_sample_rate_set(48000);
}

static void teardown(void) {
  wave_free();
  synth_free();
}

static void cmd(const char *s) {
  char copy[256];
  snprintf(copy, sizeof(copy), "%s", s);
  if (skred_command(copy) != 0) fprintf(stderr, "command failed: %s\n", s);
}

static void render_frames(int n, float *out) {
  for (int i = 0; i < n; i++) {
    float buf[2] = {0, 0};
    synth(&skred_global_engine, buf, NULL, 1, 2, NULL);
    if (out) out[i] = buf[0];
  }
}

int main(void) {
  setup();

  /* Test 1: Default w5 voice init and SID preset 0 */
  cmd("v0 w5 a0 f440 l1");
  expect(sv.wave_table_index[0] == WAVE_TABLE_NOISE, "voice 0 is WAVE_TABLE_NOISE (w5)");
  expect(sv.noise_type[0] == 0, "default noise_type is 0 (SID)");
  expect(sv.noise_bits[0] == 23, "SID bit width is 23");
  expect(sv.noise_dac[0] == 1, "SID uses 8-bit DAC output");

  float sid_samples[512];
  render_frames(512, sid_samples);
  int non_zero = 0;
  for (int i = 0; i < 512; i++) {
    if (fabsf(sid_samples[i]) > 0.001f) non_zero++;
    expect(sid_samples[i] >= -1.05f && sid_samples[i] <= 1.05f, "SID output bounded in [-1, 1]");
  }
  expect(non_zero > 300, "SID noise produces active samples");

  /* Test 2: POKEY presets (1..4) */
  cmd("v0 NT 1");
  expect(sv.noise_type[0] == 1, "preset 1 is POKEY 4-bit");
  expect(sv.noise_bits[0] == 4, "POKEY 4-bit has bits=4");
  expect(sv.noise_dac[0] == 0, "POKEY uses 1-bit pulse mode");

  cmd("v0 NT 2");
  expect(sv.noise_type[0] == 2, "preset 2 is POKEY 5-bit");
  expect(sv.noise_bits[0] == 5, "POKEY 5-bit has bits=5");

  cmd("v0 NT 3");
  expect(sv.noise_type[0] == 3, "preset 3 is POKEY 9-bit");
  expect(sv.noise_bits[0] == 9, "POKEY 9-bit has bits=9");

  cmd("v0 NT 4");
  expect(sv.noise_type[0] == 4, "preset 4 is POKEY 17-bit");
  expect(sv.noise_bits[0] == 17, "POKEY 17-bit has bits=17");

  /* Test 3: NES presets (5..6) */
  cmd("v0 NT 5");
  expect(sv.noise_type[0] == 5, "preset 5 is NES 15-bit");
  expect(sv.noise_bits[0] == 15, "NES 15-bit has bits=15");
  expect(sv.noise_tap[0] == 1, "NES 15-bit has tap=1");

  cmd("v0 NT 6");
  expect(sv.noise_type[0] == 6, "preset 6 is NES 7-bit loop");
  expect(sv.noise_bits[0] == 15, "NES 7-bit has bits=15");
  expect(sv.noise_tap[0] == 6, "NES 7-bit has tap=6");

  /* Test 4: Custom configuration (preset 7) */
  cmd("v0 NT 7 12 5 0");
  expect(sv.noise_type[0] == 7, "mode 7 is custom");
  expect(sv.noise_bits[0] == 12, "custom bits=12");
  expect(sv.noise_tap[0] == 5, "custom tap=5");
  expect(sv.noise_dac[0] == 0, "custom dac=0");

  /* Auto maximal-length tap when omitted */
  cmd("v0 NT 7 16");
  expect(sv.noise_type[0] == 7, "mode 7 with auto tap");
  expect(sv.noise_bits[0] == 16, "custom bits=16");
  expect(sv.noise_tap[0] == 4, "maximal tap for 16 is 4");
  expect(sv.noise_dac[0] == 1, "default dac is 1");

  /* Test 5: NT arity 0 resets to preset 0 (SID) */
  cmd("v0 NT");
  expect(sv.noise_type[0] == 0, "NT resets to SID preset 0");
  expect(sv.noise_bits[0] == 23, "reset bits=23");
  expect(sv.noise_tap[0] == 17, "reset tap=17");
  expect(sv.noise_dac[0] == 1, "reset dac=1");

  /* Test 6: Pitch tracking (f 100 vs f 1000) */
  cmd("v0 s0 NT 1 f100 l1");
  int low_transitions = 0;
  float prev_val = sv.noise_sample[0];
  for (int i = 0; i < 4800; i++) {
    render_frames(1, NULL);
    if (sv.noise_sample[0] != prev_val) {
      low_transitions++;
      prev_val = sv.noise_sample[0];
    }
  }

  cmd("v0 s0 NT 1 f1000 l1");
  int high_transitions = 0;
  prev_val = sv.noise_sample[0];
  for (int i = 0; i < 4800; i++) {
    render_frames(1, NULL);
    if (sv.noise_sample[0] != prev_val) {
      high_transitions++;
      prev_val = sv.noise_sample[0];
    }
  }

  expect(low_transitions > 0, "f100 produces LFSR steps");
  expect(high_transitions >= low_transitions * 5, "f1000 produces proportionally more steps than f100");

  teardown();

  if (failures == 0) {
    printf("All noise tests passed successfully!\n");
    return 0;
  }
  fprintf(stderr, "%d noise test failures\n", failures);
  return 1;
}
