#include <stdio.h>
#include <string.h>
#include <math.h>

#include "api.h"
#include "skode.h"
#include "synth.h"
#include "synth-types.h"
#include "skode-internal.h"

static int failures;

static void expect(int condition, const char *message) {
  if (condition) return;
  fprintf(stderr, "FAIL audio commands: %s\n", message);
  failures++;
}

static void consume_skode(skode_t *ctx, const char *command) {
  char copy[256];
  snprintf(copy, sizeof(copy), "%s", command);
  expect(skode_consume(copy, ctx) == 0, "direct Skode audio command failed");
}

int main(void) {
  expect(skred_audio_command("v0 f440") == 0,
         "non-audio command was intercepted");

  expect(skred_audio_command("/a?") == 1,
         "/a? was not handled");
  expect(strstr(skred_audio_message(), "audio: stopped") != NULL,
         "/a? did not report stopped state");
  expect(strstr(skred_audio_message(), "channels:") != NULL,
         "/a? did not report channel metrics");
  expect(strstr(skred_audio_message(), "requested-callback-frames:") != NULL,
         "/a? did not report callback frame metrics");
  expect(strstr(skred_audio_message(), "device-buffer:") != NULL,
         "/a? did not report effective device buffer metrics");
  expect(strstr(skred_audio_message(), "capture-waves: w7..w14") != NULL,
         "/a? did not report capture wave mapping");
  expect(strstr(skred_audio_message(), "delay:") != NULL,
         "/a? did not report delay status");
  expect(strstr(skred_audio_message(), "perf:") != NULL,
         "/a? did not report performance metrics");
  expect(strstr(skred_audio_message(), "suspected-glitches:") != NULL,
         "/a? did not report suspected glitch metrics");
  expect(strstr(skred_audio_message(), "clipped-samples") != NULL,
         "/a? did not report output integrity metrics");

  skred_performance_metrics_t metrics;
  expect(skred_performance_metrics(&metrics) == 0,
         "performance metrics API rejected output pointer");
  expect(metrics.callbacks == 0,
         "performance metrics should start with zero callbacks");
  expect(skred_performance_metrics(NULL) < 0,
         "performance metrics API accepted null output pointer");
  skred_performance_reset();

  synth_sample_rate_set(48000);
  expect(strstr(skred_audio_status(), "rate: 48000") != NULL,
         "status did not report runtime sample rate");
  synth_sample_rate_set(SKRED_DEFAULT_SAMPLE_RATE);

  expect(skred_audio_command("/ain off") == 1,
         "/ain off was not accepted");
  expect(strstr(skred_audio_message(), "requested: [off]") != NULL,
         "/ain off did not update requested state");

  expect(skred_audio_command("/aout default") == 1,
         "/aout default was not accepted");
  expect(strstr(skred_audio_message(), "requested: [default]") != NULL,
         "/aout default did not update requested state");

  expect(skred_audio_command("/ain") < 0,
         "missing /ain argument was accepted");
  expect(strstr(skred_audio_message(), "usage: /ain") != NULL,
         "missing /ain argument did not produce usage");

  expect(skred_audio_command("/aout off") < 0,
         "/aout off was accepted");
  expect(skred_audio_command("/aout -3") < 0,
         "invalid negative output selection was accepted");

  skred_logger(1);
  expect(skred_command("/a?") == 0,
         "skred_command did not preserve successful command semantics");
  expect(strstr(skred_log(), "audio: stopped") != NULL,
         "skred_command did not mirror audio status into the Skode log");
  size_t status_log_length = strlen(skred_log());
  expect(status_log_length > 0 && skred_log()[status_log_length - 1] == '\n',
         "mirrored audio status did not end with a newline");

  expect(skred_command("/ain") == 0,
         "invalid audio command escaped the unified command boundary");
  expect(strstr(skred_log(), "usage: /ain") != NULL,
         "audio command error was not mirrored into the Skode log");

  expect(skred_command("log 1") == 0,
         "non-audio command did not fall through to Skode");

  skode_t audio_skode = SKODE_EMPTY();
  skode_init(&audio_skode);
  audio_skode.log_enable = 1;
  consume_skode(&audio_skode, "/ai -2");
  expect(strstr(audio_skode.log, "requested: [off]") != NULL,
         "Skode /ai did not disable capture");
  consume_skode(&audio_skode, "/ao -1");
  expect(strstr(audio_skode.log, "requested: [default]") != NULL,
         "Skode /ao did not select default output");
  consume_skode(&audio_skode, "[aud]:/ai -2; aud");
  expect(strstr(audio_skode.log, "requested: [off]") != NULL,
         "immediate macro could not invoke Skode audio selection");
  consume_skode(&audio_skode, "/a?");
  expect(strstr(audio_skode.log, "audio: stopped") != NULL,
         "Skode /a? did not report status");
  skode_free(&audio_skode);

  /* skred_command() owns a SKODE_EMPTY() context and depends on lazy parser
     initialization. Exercise dictionary-backed atoms through that exact API
     path so browser/WASM builds cannot silently lose the core voice words. */
  synth_init(8);
  wave_table_init(0);
  voice_init();
  expect(skred_command("v0a0f440l1") == 0,
         "compact core command failed through unified command boundary");
  expect(strstr(skred_log(), "unknown atom") == NULL,
         "compact core command lost dictionary-backed atoms");
  expect(sv.user_amp[0] == 0.0f && sv.freq[0] == 440.0f,
         "compact core command did not update voice state");
  synth_free();

  /* Test n>d note-to-data conversion with rest '-' notation */
  {
    skode_t sk_ntod = SKODE_EMPTY();
    skode_init(&sk_ntod);
    skode_consume("[C3 - D3 - E3] n>d", &sk_ntod);
    double *data = ands_data(sk_ntod.parse);
    int len = ands_data_len(sk_ntod.parse);
    expect(len == 5, "n>d with rests produced expected length");
    expect(data && data[0] == 48.0, "n>d first note is C3 (48)");
    expect(data && isnan(data[1]), "n>d second element is rest (NAN)");
    expect(data && data[2] == 50.0, "n>d third note is D3 (50)");
    expect(data && isnan(data[3]), "n>d fourth element is rest (NAN)");
    expect(data && data[4] == 52.0, "n>d fifth note is E3 (52)");
    skode_free(&sk_ntod);
  }

  synth_init(8);
  wave_table_init(0);
  voice_init();
  synth_sample_rate_set(48000);
  skode_t audio_skode2 = SKODE_EMPTY();
  skode_init(&audio_skode2);
  const char *drum_groove_code =
    "M 120 v0 w82 f440 a0 v1 w83 f440 a-2 v2 w84 f440 a-7 "
    "v3 w15 a-6 N-12 t .01 .4 0 0 G4 H4 "
    "y0 [v0l1 v3n48l1] x0 [v2l0.4] x1 [v2l0.4] x2 [v1l1] x4 Z1";
  skode_consume((char*)drum_groove_code, &audio_skode2);
  skode_consume("v0l1 v1l1", &audio_skode2);
  float max_val = 0.0f;
  int non_zero_samples = 0;
  for (int f = 0; f < 375; f++) {
    float buf[256];
    memset(buf, 0, sizeof(buf));
    synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
    for (int i = 0; i < 256; i++) {
      float a = buf[i] < 0 ? -buf[i] : buf[i];
      if (a > 1e-5f) non_zero_samples++;
      if (a > max_val) max_val = a;
    }
  }
  float inc_440 = osc_get_phase_inc(&skred_global_engine,0, 440.0f);
  float inc_220 = osc_get_phase_inc(&skred_global_engine,0, 220.0f);
  expect(fabsf(inc_220 - (inc_440 * 0.5f)) < 1e-4f, "halving frequency halves phase increment for drum patch");
  expect(max_val > 0.1f, "drum groove renders strong audio amplitude");
  skode_free(&audio_skode2);
  wave_free();
  synth_free();

  static const char *demo_patterns[] = {
    "M 124 v0 w82 a0 v1 w83 a0 y0 [v0l1] x0 [v1l1] x4 Z1",
    "M 140 v0 w82 a0 v1 w83 a0 y0 [v0l1 z*3] x0 [v1l1] x4 Z1",
    "M 120 v0 w82 a0 v1 w83 a0 y0 [v0l1 z%1] x0 [v1l1 z%2] x4 Z1"
  };
  for (int d = 0; d < 3; d++) {
    synth_init(8); wave_table_init(0); voice_init(); seq_init();
    synth_sample_rate_set(48000);
    skode_t sk_demo = SKODE_EMPTY();
    skode_init(&sk_demo);
    skode_consume((char*)demo_patterns[d], &sk_demo);
    skode_consume("v0l1 v1l1 v2l1 v3l1", &sk_demo);
    float d_max = 0.0f;
    for (int f = 0; f < 500; f++) {
      float buf[256]; memset(buf, 0, sizeof(buf));
      synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
      for (int i = 0; i < 256; i++) {
        float a = buf[i] < 0 ? -buf[i] : buf[i];
        if (a > d_max) d_max = a;
      }
    }
    expect(d_max > 0.01f, "demo pattern renders audio");
    skode_free(&sk_demo); wave_free(); synth_free();
  }

  // Verify filter drive jd and combined LP+BP filter mode j6
  {
    synth_init(8); wave_table_init(0); voice_init();
    skode_t sk_filt = SKODE_EMPTY();
    skode_init(&sk_filt);
    skode_consume("v0 j6 k1200 q7 jd 1.5", &sk_filt);
    expect(sv.filter_mode[0] % 10 == FILTER_LP_BP, "j6 sets FILTER_LP_BP combined mode");
    expect(sv.filter[0].drive == 1.5f, "jd 1.5 sets voice filter drive");
    skode_consume("v0 jd 0.0", &sk_filt);
    expect(sv.filter[0].drive == 0.0f, "jd 0.0 resets voice filter drive to clean");
    skode_free(&sk_filt); wave_free(); synth_free();
  }

  // Verify stereo sample playback and l0 stopping both channels
  {
    synth_init(8); wave_table_init(0); voice_init();
    synth_sample_rate_set(48000);
    skode_t sk_stereo = SKODE_EMPTY();
    skode_init(&sk_stereo);

    int len = 48000;
    float *t_left = (float *)calloc(len, sizeof(float));
    float *t_right = (float *)calloc(len, sizeof(float));
    for (int i = 0; i < len; i++) {
      t_left[i] = 0.5f;
      t_right[i] = 0.8f;
    }
    wave_install_memory_stereo(100, t_left, t_right, len, 48000.0f, 0, "stereo-test", 60.0f, 0.0f);

    wave_set(0, 100);
    freq_set(0, 440.0f);
    amp_set(0, 0.0f);
    envelope_velocity(0, 1.0f);
    float buf[256];
    float l_active = 0.0f, r_active = 0.0f;
    for (int b = 0; b < 10; b++) {
      memset(buf, 0, sizeof(buf));
      synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
      for (int i = 0; i < 128; i++) {
        if (fabsf(buf[i * 2 + 0]) > l_active) l_active = fabsf(buf[i * 2 + 0]);
        if (fabsf(buf[i * 2 + 1]) > r_active) r_active = fabsf(buf[i * 2 + 1]);
      }
    }
    expect(l_active > 0.1f, "stereo sample left channel active on l1");
    expect(r_active > 0.1f, "stereo sample right channel active on l1");

    // Release voice with l0 and process beyond release stage
    envelope_velocity(0, 0.0f);
    for (int f = 0; f < 20; f++) {
      memset(buf, 0, sizeof(buf));
      synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
    }
    float l_silent = 0.0f, r_silent = 0.0f;
    for (int i = 0; i < 128; i++) {
      if (fabsf(buf[i * 2 + 0]) > l_silent) l_silent = fabsf(buf[i * 2 + 0]);
      if (fabsf(buf[i * 2 + 1]) > r_silent) r_silent = fabsf(buf[i * 2 + 1]);
    }
    expect(l_silent < 1e-4f, "stereo sample left channel mutes completely after l0");
    expect(r_silent < 1e-4f, "stereo sample right channel mutes completely after l0");

    skode_free(&sk_stereo);
    wave_free();
    synth_free();
  }

  // Verify l. (pause / resume) toggle on one-shot sample playback
  {
    synth_init(8); wave_table_init(0); voice_init();
    synth_sample_rate_set(48000);
    skode_t sk_pause = SKODE_EMPTY();
    skode_init(&sk_pause);

    int len = 48000;
    float *t_left = (float *)calloc(len, sizeof(float));
    for (int i = 0; i < len; i++) t_left[i] = (float)i / (float)len;
    wave_install_memory(101, t_left, len, 48000.0f, 1 /* one-shot */, "ramp-sample", 60.0f, 0.0f);

    // Trigger voice with velocity 1.0 (Skode: v0 w101 l1)
    consume_skode(&sk_pause, "v0 w101 a0 l1");
    expect(sv.paused[0] == 0, "voice 0 starts in non-paused state");

    // Advance 128 frames
    float buf[256];
    synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
    double phase_before_pause = sv.phase[0];
    expect(phase_before_pause > 10.0, "playback phase advances during playback");

    // Send l. to PAUSE (TIE)
    consume_skode(&sk_pause, "v0 l.");
    expect(sv.paused[0] == 1, "l. toggles voice 0 into paused state");

    // Advance 5 blocks (640 frames) while paused: phase MUST NOT advance!
    for (int b = 0; b < 5; b++) {
      synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
    }
    expect(sv.phase[0] == phase_before_pause, "playback phase is frozen during pause");
    expect(sv.pause_gain[0] == 0.0f, "pause micro-fade has ramped to complete silence");

    // Send l. to UNPAUSE / RESUME
    consume_skode(&sk_pause, "v0 l.");
    expect(sv.paused[0] == 0, "second l. unpauses voice 0");

    // Advance 1 block: phase advances from the exact frozen phase
    synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
    expect(sv.phase[0] > phase_before_pause, "playback phase resumes advancing from paused position");
    expect(sv.pause_gain[0] > 0.0f, "pause micro-fade ramps back in smoothly");

    // Retrigger with l1: clears pause and resets phase to start
    consume_skode(&sk_pause, "v0 l1");
    expect(sv.paused[0] == 0, "l1 clears pause flag");

    skode_free(&sk_pause);
    wave_free();
    synth_free();
  }

  // Verify cue point control plane events
  {
    synth_init(8); wave_table_init(0); voice_init();
    synth_sample_rate_set(48000);
    skode_t sk_cue = SKODE_EMPTY();
    skode_init(&sk_cue);

    int len = 48000;
    float *t_left = (float *)calloc(len, sizeof(float));
    for (int i = 0; i < len; i++) t_left[i] = 0.5f;
    wave_install_memory(102, t_left, len, 48000.0f, 1, "cue-sample", 60.0f, 0.0f);

    // Install 3 cue points into slot 102
    sw.cues[102] = (sk_cue_t *)calloc(3, sizeof(sk_cue_t));
    sw.num_cues[102] = 3;
    sw.cues[102][0].start = 64;   sw.cues[102][0].end = 128;  sw.cues[102][0].type = SK_CUE_SLICE; snprintf(sw.cues[102][0].label, 32, "slice1");
    sw.cues[102][1].start = 128;  sw.cues[102][1].end = 256;  sw.cues[102][1].type = SK_CUE_SLICE; snprintf(sw.cues[102][1].label, 32, "slice2");
    sw.cues[102][2].start = 256;  sw.cues[102][2].end = 512;  sw.cues[102][2].type = SK_CUE_WORD;  snprintf(sw.cues[102][2].label, 32, "word1");

    // Enable voice control events for voice 0
    voice_control_events_set(0, 1);
    skred_control_event_clear();

    // Trigger voice (Skode: v0 w102 l1)
    consume_skode(&sk_cue, "v0 w102 a0 l1");

    // Render 4 blocks (512 frames) to cross cue points
    float buf[256];
    for (int b = 0; b < 4; b++) {
      synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
    }

    // Poll control events and verify cue events
    skred_control_event_t events[16];
    int count = skred_control_event_poll(events, 16);
    int cue_event_count = 0;
    for (int i = 0; i < count; i++) {
      if (events[i].type == SKRED_CONTROL_EVENT_CUE) {
        cue_event_count++;
        expect(events[i].voice == 0, "cue event voice is 0");
        expect(events[i].tag == 102, "cue event tag is wave 102");
      }
    }
    expect(cue_event_count >= 2, "control plane emitted cue events for crossed cues");

    // Test Jump to Cue (jc / [label] jc)
    consume_skode(&sk_cue, "v0 jc1");
    expect(sv.phase[0] == 128.0, "v0 jc1 jumps phase to cue index 1");

    consume_skode(&sk_cue, "v0 [word1] jc");
    expect(sv.phase[0] == 256.0, "v0 [word1] jc jumps phase to cue label 'word1'");

    // Test Jump to Cue while paused
    consume_skode(&sk_cue, "v0 l.");
    expect(sv.paused[0] == 1, "voice is paused");
    consume_skode(&sk_cue, "v0 jc0");
    expect(sv.phase[0] == 64.0, "v0 jc0 jumps phase while paused");
    expect(sv.paused[0] == 1, "voice remains paused after jc");

    // Test compact waveform display with cues and playhead
    consume_skode(&sk_cue, "v0 w102 VW");

    skode_free(&sk_cue);
    wave_free();
    synth_free();
  }

  // Verify multi-channel wave loading and channel pair selection (cp)
  {
    synth_init(8); wave_table_init(0); voice_init();
    synth_sample_rate_set(48000);
    skode_t sk_mc = SKODE_EMPTY();
    skode_init(&sk_mc);

    int len = 48000;
    float **planar = (float **)malloc(4 * sizeof(float *));
    for (int c = 0; c < 4; c++) {
      planar[c] = (float *)calloc(len, sizeof(float));
      float val = (c == 0) ? 0.2f : (c == 1) ? 0.3f : (c == 2) ? 0.6f : 0.8f;
      for (int i = 0; i < len; i++) planar[c][i] = val;
    }
    wave_install_memory_multich(103, planar, 4, len, 48000.0f, 0, "4ch-stems", 60.0f, 0.0f);
    expect(sw.channels[103] == 4, "wave 103 reports 4 channels");

    // Voice 0 plays Pair 0 (Ch 0 & Ch 1)
    consume_skode(&sk_mc, "v0 w103 cp0 a0 l1");
    expect(sv.table[0][0] == 0.2f, "pair 0 left table is ch0 (0.2)");
    expect(sv.table_right[0][0] == 0.3f, "pair 0 right table is ch1 (0.3)");
    float buf[256];
    for (int b = 0; b < 10; b++) {
      memset(buf, 0, sizeof(buf));
      synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
    }
    expect(buf[0] > 0.01f && buf[1] > 0.01f, "pair 0 outputs active stereo audio");
    float ratio_p0 = buf[1] / buf[0];
    expect(fabsf(ratio_p0 - 1.5f) < 0.05f, "pair 0 channel ratio matches 0.3 / 0.2 = 1.5");

    // Voice 0 switches to Pair 1 (Ch 2 & Ch 3)
    consume_skode(&sk_mc, "v0 cp1");
    expect(sv.channel_pair[0] == 1, "v0 cp1 sets channel_pair to 1");
    expect(sv.table[0][0] == 0.6f, "pair 1 left table is ch2 (0.6)");
    expect(sv.table_right[0][0] == 0.8f, "pair 1 right table is ch3 (0.8)");
    for (int b = 0; b < 10; b++) {
      memset(buf, 0, sizeof(buf));
      synth(&skred_global_engine, buf, NULL, 128, 2, NULL);
    }
    expect(buf[0] > 0.01f && buf[1] > 0.01f, "pair 1 outputs active stereo audio");
    float ratio_p1 = buf[1] / buf[0];
    expect(fabsf(ratio_p1 - (0.8f / 0.6f)) < 0.05f, "pair 1 channel ratio matches 0.8 / 0.6 = 1.33");

    // Verify voice_format reflects cp1 when non-zero
    char fmt[256];
    voice_format(0, fmt, sizeof(fmt), 0);
    expect(strstr(fmt, "cp1") != NULL, "voice_format includes cp1 when pair is 1");

    // Reset back to cp0 and verify suppression in voice_format
    consume_skode(&sk_mc, "v0 cp0");
    voice_format(0, fmt, sizeof(fmt), 0);
    expect(strstr(fmt, "cp") == NULL, "voice_format suppresses cp when pair is 0");

    skode_free(&sk_mc);
    wave_free();
    synth_free();
  }

  // Verify skred_set_string and skred_get_string for drag-and-drop ANDS integration
  {
    const char *test_path = "/tmp/skode_test_sample.wav";
    expect(skred_set_string(test_path) == 1, "skred_set_string succeeds");
    expect(strcmp(skred_get_string(), test_path) == 0, "skred_get_string matches path");
    skred_logger(1);
    skred_command("?s;");
    char *log = skred_log();
    expect(log && strstr(log, test_path) != NULL, "ands query_string ?s outputs set path");
  }

  if (failures) {
    fprintf(stderr, "%d audio command test failure(s)\n", failures);
    return 1;
  }
  printf("Audio command tests passed\n");
  return 0;
}
