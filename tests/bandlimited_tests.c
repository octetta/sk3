/*
 * Bandlimited wavetable playback and opcode WB tests.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#include "api.h"
#include "skode.h"
#include "synth.h"
#include "synth-types.h"

static int failures;

static void expect(int condition, const char *message) {
  if (condition) return;
  fprintf(stderr, "FAIL bandlimited test: %s\n", message);
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

int main(void) {
  setup();

  /* Test 1: PolyBLEP waveforms (w1..w4) are zero-table memory analytic oscillators */
  expect(sw.mip_levels[WAVE_TABLE_SQR] == 0, "square wave has 0 mip levels (polyblep)");
  expect(sw.data[WAVE_TABLE_SQR] == NULL, "square wave table memory is unallocated (zero table)");
  expect(sw.data[WAVE_TABLE_SAW_DOWN] == NULL, "saw-down table memory is unallocated (zero table)");
  cmd("v0 w1 a1 l1");
  expect(sv.source_type[0] == OSC_SOURCE_POLYBLEP, "v0 w1 sets source_type to OSC_SOURCE_POLYBLEP");

  /* Test 1b: Mipmap tables generated for standard DWG periodic wavetables (e.g. w15) */
  expect(sw.mip_levels[WAVE_TABLE_KRG1] == 8, "dwg-strings wave (w15) has 8 mip levels");
  for (int lvl = 0; lvl < 8; lvl++) {
    expect(sw.mip_data[lvl][WAVE_TABLE_KRG1] != NULL, "w15 mip table is allocated");
  }

  /* Test 2: Level 7 of periodic table has valid fundamental data */
  int sz = sw.size[WAVE_TABLE_KRG1];
  float *lvl7 = sw.mip_data[7][WAVE_TABLE_KRG1];
  float *lvl0 = sw.mip_data[0][WAVE_TABLE_KRG1];
  expect(lvl7 != NULL, "level 7 exists");
  expect(lvl0 != NULL, "level 0 exists");
  expect(sz > 0, "w15 size > 0");

  /* Test 3: Frequency-based mip level selection on periodic wavetables */
  cmd("v0 w15 a1 l1"); // w15 = dwg-strings
  
  // A1 (55 Hz) -> Level 0
  cmd("v0 f50");
  expect(sv.mip_level[0] == 0, "50 Hz selects mip level 0");
  expect(sv.table[0] == sw.mip_data[0][WAVE_TABLE_KRG1], "sv.table points to mip level 0");

  // A2 (100 Hz) -> Level 1
  cmd("v0 f100");
  expect(sv.mip_level[0] == 1, "100 Hz selects mip level 1");
  expect(sv.table[0] == sw.mip_data[1][WAVE_TABLE_KRG1], "sv.table points to mip level 1");

  // A3 (200 Hz) -> Level 2
  cmd("v0 f200");
  expect(sv.mip_level[0] == 2, "200 Hz selects mip level 2");

  // A4 (400 Hz) -> Level 3
  cmd("v0 f400");
  expect(sv.mip_level[0] == 3, "400 Hz selects mip level 3");

  // A5 (800 Hz) -> Level 4
  cmd("v0 f800");
  expect(sv.mip_level[0] == 4, "800 Hz selects mip level 4");

  // A6 (1600 Hz) -> Level 5
  cmd("v0 f1600");
  expect(sv.mip_level[0] == 5, "1600 Hz selects mip level 5");

  // A7 (3200 Hz) -> Level 6
  cmd("v0 f3200");
  expect(sv.mip_level[0] == 6, "3200 Hz selects mip level 6");

  // A8 (5000 Hz) -> Level 7
  cmd("v0 f5000");
  expect(sv.mip_level[0] == 7, "5000 Hz selects mip level 7");
  expect(sv.table[0] == sw.mip_data[7][WAVE_TABLE_KRG1], "sv.table points to mip level 7");

  /* Test 4: Hysteresis stability around boundary */
  // First establish level 3 at 400 Hz
  cmd("v0 f400");
  expect(sv.mip_level[0] == 3, "400 Hz is level 3");

  // Vibrato up to 455 Hz (< 440 * 1.05 = 462 Hz) should NOT cross boundary yet
  cmd("v0 f455");
  expect(sv.mip_level[0] == 3, "455 Hz stays in level 3 due to +5% hysteresis");

  // Push past hysteresis threshold (470 Hz > 462 Hz) -> switches to level 4
  cmd("v0 f470");
  expect(sv.mip_level[0] == 4, "470 Hz crosses into level 4");

  // Vibrato down to 430 Hz (> 440 * 0.95 = 418 Hz) should stay in level 4
  cmd("v0 f430");
  expect(sv.mip_level[0] == 4, "430 Hz stays in level 4 due to -5% hysteresis");

  // Push below hysteresis threshold (400 Hz < 418 Hz) -> switches down to level 3
  cmd("v0 f400");
  expect(sv.mip_level[0] == 3, "400 Hz crosses back into level 3");

  /* Test 5: Playback direction backward preserves correct mip level */
  cmd("v0 b1 f400"); // backward direction
  expect(sv.mip_level[0] == 3, "backward playback selects correct mip level");

  /* Test 6: Custom wavetable and WB opcode */
  int custom_slot = 100;
  sw.size[custom_slot] = 2048;
  sw.data[custom_slot] = (float *)malloc(2048 * sizeof(float));
  sw.is_heap[custom_slot] = 1;
  sw.rate[custom_slot] = 48000.0f;
  // Fill with sharp unbandlimited sawtooth
  for (int i = 0; i < 2048; i++) {
    float ph = (float)i / 2048.0f;
    sw.data[custom_slot][i] = 2.0f * ph - 1.0f;
  }
  sw.mip_levels[custom_slot] = 0;

  // Execute WB on slot 100
  cmd("100 WB");
  expect(sw.mip_levels[custom_slot] == 8, "WB opcode generates 8 mip levels for custom slot");
  for (int lvl = 0; lvl < 8; lvl++) {
    expect(sw.mip_data[lvl][custom_slot] != NULL, "custom slot mip table allocated");
  }

  // Switch voice 1 to custom slot and check mip selection
  cmd("v1 w100 f5000 a1");
  expect(sv.mip_level[1] == 7, "voice playing custom table at 5000 Hz selects level 7");
  expect(sv.table[1] == sw.mip_data[7][custom_slot], "voice table points to custom mip level 7");

#ifdef KSYNTH
  /* Test 7: Ksynth k>w auto-stores harmonics and WB uses them */
  cmd("[P: ~2048; H: !16; H: H+1; A: 1%H; W: w P $ A] k!");
  cmd("101 k>w");
  expect(sw.harmonics != NULL && sw.harmonics[101] != NULL, "k>w auto-stores harmonics array in sw.harmonics");
  expect(sw.num_harmonics != NULL && sw.num_harmonics[101] == 16, "k>w auto-stores harmonic count (16)");

  // Run WB on slot 101
  cmd("101 WB");
  expect(sw.mip_levels[101] == 8, "WB generates 8 mip levels from auto-stored harmonics");
  for (int lvl = 0; lvl < 8; lvl++) {
    expect(sw.mip_data[lvl][101] != NULL, "slot 101 mip table allocated");
  }
#endif

  teardown();

  if (failures > 0) {
    fprintf(stderr, "bandlimited tests failed with %d error(s)\n", failures);
    return 1;
  }
  printf("All bandlimited wavetable playback tests passed successfully!\n");
  return 0;
}
