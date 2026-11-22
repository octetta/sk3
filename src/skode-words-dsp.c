#include "skode-internal.h"
#include "scope-ipc.h"
#include "sk_dsp.h"

static int word_exec_ab(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`ab val` set voice amplitude bend normalized value (-1.0 to 1.0)

@enddoc */
      if (argc) amp_bend_set(voice, (float)arg[0]);
      return 0;
}

static int word_exec_abp(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`abp range[,offset]` set voice amplitude bend range in dB and optional offset

@enddoc */
      if (argc) {
        float range = (float)arg[0];
        float offset = argc > 1 ? (float)arg[1] : 0.0f;
        amp_bend_param_set(voice, range, offset);
      }
      return 0;
}

static int word_exec_A(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`A x,y,z` set voice amplitude modulation (AM) using voice `x`, depth 'y', and offset 'z'

@enddoc */
      if (argc < 2) {
        amp_mod_set(voice, -1, 0, 0);
      } else if (x_valid) {
        float a = 0;
        if (argc > 2) a = arg[2];
        amp_mod_set(voice, x, arg[1], a);
      }
      return 0;
}

static int word_exec_b(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`b [0|1|2]` set voice waveform playback direction:
0=forward, 1=backward, 2=ping-pong

@enddoc */
      if (argc == 0) { wave_dir(voice, -1); } else { wave_dir(voice, x); } return 0;
}

static int word_exec_B(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`B [0|1]` set voice waveform looping 0=off, 1=on

@enddoc */
      if (argc == 0) { wave_loop(voice, -1); } else { wave_loop(voice, x); } return 0;
}

static int word_exec_BC(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`BC count` set the number of one-shot loop repeats; 0 means unlimited

@enddoc */
      if (argc && x_valid) wave_loop_count(voice, x);
      return 0;
}

static int word_exec_c(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`c mode, amount` set voice CZ / phase distortion with `mode` (below) and a
signed `amount` from -1.0 to 1.0. 0.0 is the undistorted phase for every mode.
  0 = off
  1 = saw -> pulse
  2 = folded sine
  3 = triangle
  4 = double sine
  5 = saw -> triangle
  6 = resonant 1
  7 = resonant 2

@enddoc */
      if (argc == 0) {
        cz_set(voice, 0, 0);
      } else if (argc == 1) {
        cz_set(voice, x, 0);
      } else {
        cz_set(voice, x, arg[1]);
      }
      return 0;
}

static int word_exec_C(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc < 2) {
        cmod_set(voice, -1, 0);
      } else if (x_valid) {
        cmod_set(voice, x, arg[1]);
      }
      return 0;
}

static int word_exec_ct(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`ct attack,decay,sustain,release` sets the phase-distortion envelope. The
neutral `ct 0 0 1 0` disables it.

@enddoc */
      if (argc == 0) {
        envelope_configure_e(&sv.cz_envelope[voice], 0.0f, 0.0f, 1.0f, 0.0f);
        sv.use_cz_envelope[voice] = 0;
      } else if (argc >= 2) {
        float a = (float)arg[0];
        float d = (float)arg[1];
        float s = (argc > 2) ? (float)arg[2] : 0.0f;
        float r = (argc > 3) ? (float)arg[3] : 0.0f;
        envelope_configure_e(&sv.cz_envelope[voice], a, d, s, r);
        sv.use_cz_envelope[voice] = !(a == 0 && d == 0 && s == 1 && r == 0);
      }
      return 0;
}

static int word_exec_cte(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  (void)self; (void)atom; (void)voice; (void)arg; (void)argc;
  double *data = s ? ands_data(s) : NULL;
  int len = s ? ands_data_len(s) : 0;
  if (len > 0) {
      envelope_configure_multistage_e(&sv.cz_envelope[voice], data, len);
      sv.use_cz_envelope[voice] = 1;
  }
  return 0;
}

static int word_exec_cd(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`cd depth` sets the signed amount added by the phase-distortion envelope.

@enddoc */
      if (argc) sv.cz_env_depth[voice] = arg[0];
      return 0;
}

static int word_exec_fb(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`fb val` set voice frequency bend normalized value (-1.0 to 1.0)

@enddoc */
      if (argc) freq_bend_set(voice, (float)arg[0]);
      return 0;
}

static int word_exec_fbp(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
/* @doc

`fbp range[,offset]` set voice frequency bend range in semitones and optional offset

@enddoc */
      if (argc) {
        float range = (float)arg[0];
        float offset = argc > 1 ? (float)arg[1] : 0.0f;
        freq_bend_param_set(voice, range, offset);
      }
      return 0;
}

static int word_exec_ft(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc == 0) {
        envelope_configure_e(&sv.filter_envelope[voice], 0.0f, 0.0f, 1.0f, 0.0f);
        sv.use_filter_envelope[voice] = 0;
      } else if (argc >= 2) {
        float a = (float)arg[0];
        float d = (float)arg[1];
        float s = (argc > 2) ? (float)arg[2] : 0.0f;
        float r = (argc > 3) ? (float)arg[3] : 0.0f;
        envelope_configure_e(&sv.filter_envelope[voice], a, d, s, r);
        sv.use_filter_envelope[voice] = !(a==0 && d==0 && s==1 && r==0);
      }
      return 0;
}

static int word_exec_fte(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  (void)self; (void)atom; (void)voice; (void)arg; (void)argc;
  double *data = s ? ands_data(s) : NULL;
  int len = s ? ands_data_len(s) : 0;
  if (len > 0) {
      envelope_configure_multistage_e(&sv.filter_envelope[voice], data, len);
      sv.use_filter_envelope[voice] = 1;
  }
  return 0;
}

static int word_exec_fd(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) sv.filter_env_depth[voice] = arg[0];
      return 0;
}

static int word_exec_F(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc <= 1) {
        freq_mod_set(voice, -1, 0, 0);
      } else if (x_valid) {
        float a = 0;
        if (argc > 2) a = arg[2];
        freq_mod_set(voice, x, arg[1], a);
      }
      return 0;
}

static int word_exec_FF(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) freq_mod_mode_set(voice, x);
      return 0;
}

static int word_exec_FB(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) freq_feedback_set(voice, arg[0]);
      return 0;
}

static int word_exec_g(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc == 0 || arg[0] <= 0) {
        sv.glissando_enable[voice] = 0;
        sv.glissando_time[voice] = 0.0;
        sv.glissando_mode[voice] = 0;
      } else {
        sv.glissando_enable[voice] = 1;
        sv.glissando_time[voice] = arg[0];
        sv.glissando_mode[voice] = (argc > 1) ? (int)arg[1] : 0;
      }
      return 0;
}

static int word_exec_G(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        for (int i = 0; i < SYNTH_VOICE_LINKS_MAX; i++) {
          int link;
          if (i < argc && skode_double_to_int(arg[i], &link) && skode_voice_valid(link))
            sv.link_midi[i][voice] = (float)link;
          else
            sv.link_midi[i][voice] = -1.0f;
        }
      }
      return 0;
}

static int word_exec_h(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc > 0) {
        float ratio = (float)arg[0];
        int mode = sv.sample_hold_mode[voice];
        if (argc > 1 && isfinite(arg[1])) mode = (int)arg[1];
        sv.sample_hold_ratio[voice] = ratio;
        sv.sample_hold_mode[voice] = mode;
      }
      return 0;
}

static int word_exec_H(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        for (int i = 0; i < SYNTH_VOICE_LINKS_MAX; i++) {
          int link;
          if (i < argc && skode_double_to_int(arg[i], &link) && skode_voice_valid(link))
            sv.link_velo[i][voice] = (float)link;
          else
            sv.link_velo[i][voice] = -1.0f;
        }
      }
      return 0;
    // TODO re-allocate the data/array buffer with the arg
}

static int word_exec_L(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        double seconds = arg[0];
        if (!isfinite(seconds) || seconds <= 0.0) {
          sv.link_trig[voice] = -1.0f;
          sv.link_trig_samp[voice] = 0;
        } else {
          long double samples = (long double)seconds * (long double)MAIN_SAMPLE_RATE;
          sv.link_trig[voice] = (float)seconds;
          sv.link_trig_samp[voice] =
            samples >= (long double)UINT64_MAX ? UINT64_MAX : (uint64_t)samples;
        }
      }
      return 0;
}

static int word_exec_J(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc > 0) {
        int mode = (int)x;
        int character = sv.filter_mode[voice] / 10;
        if (argc > 1 && isfinite(arg[1])) character = (int)arg[1];
        sv.filter_mode[voice] = (character * 10) + (mode % 10);
        mmf_set_params(&skred_global_engine, voice,
          sv.filter_freq[voice],
          sv.filter_res[voice]);
      }
      return 0;
}

static int word_exec_K(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) { mmf_set_freq(&skred_global_engine, voice, arg[0]); }
      return 0;
}

static int word_exec____l(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc && isfinite(arg[0])) envelope_velocity(voice, arg[0]);
      return 0;
}
static int word_exec_O(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  (void)self; (void)atom; (void)voice;
  for (int i = 0; i < synth_config.voice_max; i++) {
    envelope_velocity(i, 0.0);
  }
  return 0;
}

static int word_exec_l(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
    #if 1
      if (argc) skode_linked_velocity(voice, arg[0], SAMPLE_COUNT_GET());
    #else
      if (argc) {
        uint64_t now = SAMPLE_COUNT_GET();
        double vel = arg[0];
        skode_envelope_velocity(voice, vel, now);
        for (int i = 0; i < SYNTH_VOICE_LINKS_MAX; i++) {
          if (sv.link_velo[i][voice] >= 0)
            skode_envelope_velocity((int)sv.link_velo[i][voice], vel, now);
        }
      }
    #endif
      return 0;
}

static int word_exec_M(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        float bpm = arg[0];
        float sub = (argc >= 2 && arg[1] > 0.0f) ? arg[1] : 16.0f;
        if (tempo_set_subdivision(bpm, sub) != 0)
          ctx->printf(ctx, "# tempo must be between %g and %g BPM\n",
            (double)SEQ_TEMPO_MIN_BPM, (double)SEQ_TEMPO_MAX_BPM);
        else
          skred_control_pattern_event(SKRED_CONTROL_EVENT_TEMPO_CHANGE, SAMPLE_COUNT_GET(), -1, 0);
      }
      return 0;
}

static int word_exec_N(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        if (isnan(arg[0])) {
          // do nothing
        } else {
          sv.midi_transpose[voice] = arg[0];
        }
        if (argc > 1) sv.midi_cents[voice] = arg[1];
      }
      return 0;
}

static int word_exec_ds(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) delay_send_set(&skred_global_engine, voice, arg[0]);
      return 0;
}

static int word_exec_DG(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      {
        int bus = 1;
        int bits, native;
        if (argc > 0) skode_double_to_int(arg[0], &bus);
        delay_grit_get(&skred_global_engine, bus, &bits, &native);
        if (argc > 1 && isfinite(arg[1])) skode_double_to_int(arg[1], &bits);
        if (argc > 2 && isfinite(arg[2])) skode_double_to_int(arg[2], &native);
        delay_grit_set(&skred_global_engine, bus, bits, native);
      }
      return 0;
}

static int word_exec_DL(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      {
        int bus = 1;
        int coarse, fine, feedback, mod_freq, mod_depth, level;
        if (argc > 0) skode_double_to_int(arg[0], &bus);
        delay_params_get(&skred_global_engine, bus, &coarse, &fine, &feedback, &mod_freq, &mod_depth, &level);
        if (argc > 1 && isfinite(arg[1])) skode_double_to_int(arg[1], &coarse);
        if (argc > 2 && isfinite(arg[2])) skode_double_to_int(arg[2], &fine);
        if (argc > 3 && isfinite(arg[3])) skode_double_to_int(arg[3], &feedback);
        if (argc > 4 && isfinite(arg[4])) skode_double_to_int(arg[4], &mod_freq);
        if (argc > 5 && isfinite(arg[5])) skode_double_to_int(arg[5], &mod_depth);
        if (argc > 6 && isfinite(arg[6])) skode_double_to_int(arg[6], &level);
        delay_params_set(&skred_global_engine, bus, coarse, fine, feedback, mod_freq, mod_depth, level);
      }
      return 0;
}

static int word_exec_DL_q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        int bus = 1;
        skode_double_to_int(arg[0], &bus);
        ctx->printf(ctx, "%s", delay_bus_format(bus));
      } else {
        ctx->printf(ctx, "%s", delay_format());
      }
      return 0;
}

static int word_exec_DD(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      {
        int bus = 1;
        int damping, hp;
        if (argc > 0) skode_double_to_int(arg[0], &bus);
        delay_damping_get(&skred_global_engine, bus, &damping, &hp);
        if (argc > 1 && isfinite(arg[1])) skode_double_to_int(arg[1], &damping);
        if (argc > 2 && isfinite(arg[2])) skode_double_to_int(arg[2], &hp);
        delay_damping_set(&skred_global_engine, bus, damping, hp);
      }
      return 0;
}

static int word_exec_DF(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      {
        int bus = 1;
        int on;
        if (argc > 0) skode_double_to_int(arg[0], &bus);
        on = delay_freeze_get(&skred_global_engine, bus);
        if (argc > 1 && isfinite(arg[1])) skode_double_to_int(arg[1], &on);
        delay_freeze_set(&skred_global_engine, bus, on);
      }
      return 0;
}

static int word_exec_DP(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      {
        int bus = 1;
        int on;
        if (argc > 0) skode_double_to_int(arg[0], &bus);
        on = delay_pingpong_get(&skred_global_engine, bus);
        if (argc > 1 && isfinite(arg[1])) skode_double_to_int(arg[1], &on);
        delay_pingpong_set(&skred_global_engine, bus, on);
      }
      return 0;
}

static int word_exec_DT(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc == 2) {
        int bus;
        if (skode_double_to_int(arg[0], &bus) && isfinite(arg[1]))
          delay_time_ms_set(&skred_global_engine, bus, (float)arg[1]);
      }
      return 0;
}

static int word_exec_DS(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc == 3) {
        int bus;
        if (skode_double_to_int(arg[0], &bus) && isfinite(arg[1]) && isfinite(arg[2]))
          delay_time_sync_set(&skred_global_engine, bus, (float)arg[1], (float)arg[2]);
      }
      return 0;
}

static int word_exec_GS(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      global_status_show(ctx, argc > 0 && arg[0] > 0.0);
      return 0;
}

static int word_exec_P(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc < 2) {
        pan_mod_set(voice, -1, 0, 0);
      } else if (x_valid) {
        float a = 0;
        if (argc > 2) a = arg[2];
        pan_mod_set(voice, x, arg[1], a);
      }
      return 0;
}

static int word_exec_q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc > 0) {
        int bits = (int)x;
        int curve = sv.quantize[voice] / 100;
        if (argc > 1 && isfinite(arg[1])) curve = (int)arg[1];
        wave_quant(voice, (curve * 100) + (bits % 100));
      }
      return 0;
}

static int word_exec_Q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) { mmf_set_res(&skred_global_engine, voice, arg[0]); }
      return 0;
}

static int word_exec_r(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) synth_record_track_set(voice, x);
      return 0;
}

static int word_exec_rt(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc && x > 0 && x <= RECORD_TRACK_MAX) {
        synth_track_name_set(x, ands_string(ctx->parse));
        #ifdef SCOPE
        scope_ipc_track_metadata_set(x, synth_track_name_get(x),
          synth_track_volume_db_get(x));
        #endif
      }
      return 0;
}

static int word_exec_rv(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc > 1 && x > 0 && x <= RECORD_TRACK_MAX) {
        synth_track_volume_set(x, arg[1]);
        #ifdef SCOPE
        scope_ipc_track_metadata_set(x, synth_track_name_get(x),
          synth_track_volume_db_get(x));
        #endif
      }
      return 0;
}

static int word_exec_s(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        if (arg[0] <= 0) {
          sv.smoother_enable[voice] = 0;
        } else {
          sv.smoother_enable[voice] = 1;
          sv.smoother_smoothing[voice] = arg[0];
          sv.smoother_mode[voice] = (argc > 1) ? (int)arg[1] : 0;
        }
      }
      return 0;
}

static int word_exec_S(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc && x_valid) wave_reset(x);
      return 0;
}

static int word_exec_t(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc == 0) {
        envelope_set(voice, 0.0f, 0.0f, 1.0f, 0.0f);
      } else if (argc >= 2) {
        float a = (float)arg[0];
        float d = (float)arg[1];
        float s_val = (argc > 2) ? (float)arg[2] : 0.0f;
        float r = (argc > 3) ? (float)arg[3] : 0.0f;
        envelope_set(voice, a, d, s_val, r);
      }
      return 0;
}

static int word_exec_te(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  (void)self; (void)atom; (void)voice; (void)arg; (void)argc;
  double *data = s ? ands_data(s) : NULL;
  int len = s ? ands_data_len(s) : 0;
  if (len > 0) {
      envelope_configure_multistage_e(&sv.amp_envelope[voice], data, len);
      sv.use_amp_envelope[voice] = 1;
      sv.amp_envelope_mode[voice] = 0;
  }
  return 0;
}

static int word_exec_T(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  (void)self; (void)s; (void)arg;
  if (argc != 0) return -1;
  int voice = ctx->voice;
  envelope_velocity(voice, 1);
  for (int i = 0; i < SYNTH_VOICE_LINKS_MAX; i++) {
    if (sv.link_velo[i][voice] >= 0) envelope_velocity(sv.link_velo[i][voice], 1);
  }
  return 0;
}

static int word_exec_HS(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  (void)self; (void)s;
  int voice = ctx->voice;
  if (argc < 1) return -1;
  for (int i = 0; i < SYNTH_VOICE_LINKS_MAX; i++) {
    int link;
    if (i < argc && skode_double_to_int(arg[i], &link) && skode_voice_valid(link))
      sv.link_sync[i][voice] = (float)link;
    else
      sv.link_sync[i][voice] = -1.0f;
  }
  osc_hard_sync_sanitize(voice);
  return 0;
}

static int word_exec_vc(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) voice_control_events_set(voice, x != 0);
      return 0;
}

static int word_exec_V(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        volume_set(arg[0]);
        #ifdef SCOPE
        scope_ipc_track_metadata_set(0, synth_track_name_get(0),
          synth_track_volume_db_get(0));
        #endif
      }
      return 0;
}

static int word_exec_vt(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      skode_copy_string(sv.text[voice], TEXT_MAX, ands_string(ctx->parse));
      return 0;
}

static int word_exec_WL(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc > 2 && x_valid && skode_wave_valid(x)) {
        int start, end;
        if (skode_double_to_int(arg[1], &start) &&
            skode_double_to_int(arg[2], &end)) {
          wave_loop_points_set(x, start, end);
        }
      }
      return 0;
}

static int word_exec_VS(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc >= 2) {
        int start, end;
        if (skode_double_to_int(arg[0], &start) &&
            skode_double_to_int(arg[1], &end)) {
          if (voice_wave_range_set(voice, start, end) != 0) {
            ctx->printf(ctx,
              "# VS rejected for v%d: %d..%d must be within 0..%d\n",
              voice, start, end, sv.table_size[voice]);
          }
        }
      } else if (argc == 0) {
        voice_wave_range_reset(voice);
      }
      return 0;
}

static int word_exec_VL(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc >= 2) {
        int start, end;
        if (skode_double_to_int(arg[0], &start) &&
            skode_double_to_int(arg[1], &end)) {
          if (voice_loop_points_set(voice, start, end) != 0) {
            ctx->printf(ctx,
              "# VL rejected for v%d: %d..%d must be within VS %d..%d\n",
              voice, start, end, sv.wave_range_start[voice],
              sv.wave_range_end[voice]);
          }
        }
      } else if (argc == 0) {
        voice_loop_points_reset(voice);
      }
      return 0;
}

static int word_exec_VC(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  (void)self; (void)atom;
  int wave = sv.wave_table_index[voice];

  if (!skode_wave_valid(wave) || !sw.cues || !sw.num_cues || !sw.cues[wave]) {
    if (argc == 0) voice_wave_range_reset(voice);
    return 0;
  }

  if (argc >= 1) {
    int cue_idx;
    if (skode_double_to_int(arg[0], &cue_idx)) {
      if (cue_idx >= 0 && cue_idx < sw.num_cues[wave]) {
        sk_cue_t *c = &sw.cues[wave][cue_idx];
        int start = (int)c->start;
        int end = (int)c->end;
        if (end <= start) end = sw.size[wave];
        voice_wave_range_set(voice, start, end);
        if (c->type == SK_CUE_LOOP_SUSTAIN || c->type == SK_CUE_LOOP_RELEASE) {
          voice_loop_points_set(voice, (int)c->start, (int)c->end);
        }
      } else {
        ctx->printf(ctx, "# cue index %d out of range (0..%d)\n", cue_idx, sw.num_cues[wave] - 1);
      }
    }
  } else if (ands_string_fresh(ctx->parse) && strlen(ands_string(ctx->parse)) > 0) {
    const char *label = ands_string(ctx->parse);
    int found = 0;
    for (int i = 0; i < sw.num_cues[wave]; i++) {
      if (strcmp(sw.cues[wave][i].label, label) == 0) {
        sk_cue_t *c = &sw.cues[wave][i];
        int start = (int)c->start;
        int end = (int)c->end;
        if (end <= start) end = sw.size[wave];
        voice_wave_range_set(voice, start, end);
        if (c->type == SK_CUE_LOOP_SUSTAIN || c->type == SK_CUE_LOOP_RELEASE) {
          voice_loop_points_set(voice, (int)c->start, (int)c->end);
        }
        found = 1;
        break;
      }
    }
    if (!found) {
      ctx->printf(ctx, "# cue label '%s' not found on wave %d\n", label, wave);
    }
  } else {
    voice_wave_range_reset(voice);
  }
  return 0;
}

static int word_exec_VW(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      {
        int target_voice = voice;
        int w = WAVE_DISPLAY_DEFAULT_WIDTH;
        int h = WAVE_DISPLAY_DEFAULT_HEIGHT;
        if (argc == 1) {
          int parsed_voice;
          if (skode_double_to_int(arg[0], &parsed_voice)) target_voice = parsed_voice;
        } else if (argc == 2) {
          w = wave_display_dim(arg[0], w, WAVE_DISPLAY_MIN_WIDTH, WAVE_DISPLAY_MAX_WIDTH);
          h = wave_display_dim(arg[1], h, WAVE_DISPLAY_MIN_HEIGHT, WAVE_DISPLAY_MAX_HEIGHT);
        } else if (argc >= 3) {
          int parsed_voice;
          if (skode_double_to_int(arg[0], &parsed_voice)) target_voice = parsed_voice;
          w = wave_display_dim(arg[1], w, WAVE_DISPLAY_MIN_WIDTH, WAVE_DISPLAY_MAX_WIDTH);
          h = wave_display_dim(arg[2], h, WAVE_DISPLAY_MIN_HEIGHT, WAVE_DISPLAY_MAX_HEIGHT);
        }
        if (target_voice >= 0 && target_voice < synth_config.voice_max) {
          int wave = sv.wave_table_index[target_voice];
          if (skode_wave_valid(wave)) {
            // ctx->printf(ctx, "# wave [%d..%d)\n", sv.wave_range_start[target_voice], sv.wave_range_end[target_voice]);
            char label[96];
            snprintf(label, sizeof(label), "voice %d wave %d", target_voice, wave);
            wavetable_waveform_show(ctx, wave, w, h,
              sv.loop_start[target_voice], sv.loop_end[target_voice], label);
          }
        }
      }
      return 0;
}

static int word_exec_w(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc && wave_set(voice, x) == 0) {
        int n;
        if (argc > 1) {
          if (skode_double_to_int(arg[1], &n)) sv.interpolate[voice] = n != 0;
        }
        if (argc > 2) {
          if (skode_double_to_int(arg[2], &n)) sv.one_shot[voice] = n != 0;
        } else sv.one_shot[voice] = sw.one_shot[x];
        osc_reclassify(&skred_global_engine, voice);
      }
      return 0;
}

static int word_exec_w_gtd(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (x_valid && skode_wave_valid(x) && sw.data[x] && sw.size[x] > 0) {
        if (sw.size[x] > ands_data_cap(ctx->parse)) ands_data_resize(ctx->parse, sw.size[x]);
        double *data = ands_data(ctx->parse);
        if (!data || sw.size[x] > ands_data_cap(ctx->parse)) return 0;
        for (int i=0; i<sw.size[x]; i++) data[i] = sw.data[x][i];
        ands_data_len_set(ctx->parse, sw.size[x]);
      }
      return 0;
}

static int word_exec_w_gtr(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (x_valid && skode_wave_valid(x) && sw.data[x] && sw.size[x] > 0) {
        int valid = 1;
        int sample_state = atomic_load_int(&sampling.state);
        if (sample_state == SAMPLE_STATE_ARMED ||
            sample_state == SAMPLE_STATE_RECORDING) {
          valid = 0;
          ctx->printf(ctx, "# recording buffer busy\n");
        } else if (sw.size[x] > sampling.capacity) {
          skode_sample_alloc(sw.size[x]);
          valid = sampling.where != NULL && sampling.capacity >= sw.size[x];
        }
        if (valid) {
          sampling.offset = 0;
          sampling.trim = 0;
          for (int i=0; i<sw.size[x]; i++) sampling.where[i] = sw.data[x][i];
          sampling.len = sw.size[x];
          sampling.channels = 1;
          atomic_store_int(&sampling.state, SAMPLE_STATE_COMPLETE);
        }
      }
      return 0;
}

static int word_exec_w_gtw(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (!ands_string_fresh(ctx->parse) ||
          !ands_string(ctx->parse)[0]) {
        ctx->printf(ctx, "# w>w requires [filename]\n");
      } else if (!x_valid || !skode_wave_valid(x) ||
                 !sw.data[x] || sw.size[x] <= 0) {
        ctx->printf(ctx, "# invalid wavetable for w>w\n");
      } else {
        double stored_rate = sw.rate[x];
        ma_uint32 sample_rate = MAIN_SAMPLE_RATE;
        if (isfinite(stored_rate) && stored_rate >= 1.0 &&
            stored_rate <= (double)UINT32_MAX - 0.5) {
          sample_rate = (ma_uint32)(stored_rate + 0.5);
        }
        skode_write_wav(ctx, ands_string(ctx->parse), sw.data[x],
                        sw.size[x], 1, sample_rate, 0);
      }
      return 0;
}

static int word_exec_w_bang(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      {
        if (atomic_load_int(&sampling.state) != SAMPLE_STATE_COMPLETE ||
            !sampling.where || sampling.offset < 0 || sampling.trim < 0 ||
            sampling.offset > sampling.len ||
            sampling.trim > sampling.len - sampling.offset) {
          ctx->printf(ctx, "# invalid recording bounds\n");
          return 0;
        }
        int channels = sampling.channels == 2 ? 2 : 1;
        int new_len = sampling.len - sampling.offset - sampling.trim;
        memmove(sampling.where,
                sampling.where + (size_t)sampling.offset * channels,
                (size_t)new_len * channels * sizeof(float));
        sampling.len = new_len;
        sampling.trim = 0;
        sampling.offset = 0;
      }
      return 0;
}

static int word_exec_w_star(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (atomic_load_int(&sampling.state) == SAMPLE_STATE_COMPLETE) {
        sampling.offset = 0;
        sampling.trim = 0;
      } else {
        ctx->printf(ctx, "# recording buffer is not complete\n");
      }
      return 0;
}

static int word_exec_w_gt(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (atomic_load_int(&sampling.state) != SAMPLE_STATE_COMPLETE) {
        ctx->printf(ctx, "# recording buffer is not complete\n");
        return 0;
      }
      if (argc == 0) x = 1;
      if (argc == 0 || x_valid) {
        long long next = (long long)sampling.offset + x;
        if (next < 0) next = 0;
        if (next > sampling.len) next = sampling.len;
        sampling.offset = (int)next;
        if (sampling.trim > sampling.len - sampling.offset)
          sampling.trim = sampling.len - sampling.offset;
      }
      return 0;
}

static int word_exec_w_lt(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (atomic_load_int(&sampling.state) != SAMPLE_STATE_COMPLETE) {
        ctx->printf(ctx, "# recording buffer is not complete\n");
        return 0;
      }
      if (argc == 0) x = 1;
      if (argc == 0 || x_valid) {
        long long next = (long long)sampling.trim + x;
        int max_trim = sampling.len - sampling.offset;
        if (max_trim < 0) max_trim = 0;
        if (next < 0) next = 0;
        if (next > max_trim) next = max_trim;
        sampling.trim = (int)next;
      }
      return 0;
}

static int word_exec_w_lt_gt(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      {
        float arg0 = -1;
        float arg1 = -1;
        int margin = 0;
        if (argc > 0) arg0 = arg[0];
        if (argc > 1) arg1 = arg[1];
        if (argc > 2) skode_double_to_int(arg[2], &margin);
        record_find_trim(argc, arg0, arg1, margin);
      }
      return 0;
}

static int word_exec_W(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        int w = WAVE_DISPLAY_DEFAULT_WIDTH;
        int h = WAVE_DISPLAY_DEFAULT_HEIGHT;
        int m = 0;
        int wave_max = synth_config.wave_table_max - 1;
        int show_record_buffer = (arg[0] < 0 || isnan(arg[0]));
        if (show_record_buffer) {
          if (argc > 1) {
            w = wave_display_dim(arg[1], w, WAVE_DISPLAY_MIN_WIDTH, WAVE_DISPLAY_MAX_WIDTH);
          }
          if (argc > 2) {
            h = wave_display_dim(arg[2], h, WAVE_DISPLAY_MIN_HEIGHT, WAVE_DISPLAY_MAX_HEIGHT);
          }
        } else if (argc == 2) {
          if (isnan(arg[1])) m = wave_max;
          else if (!skode_double_to_int(arg[1], &m)) m = x;
          if (m < x) m = x;
          if (m > wave_max) m = wave_max;
        } else if (argc >= 3) {
          w = wave_display_dim(arg[1], w, WAVE_DISPLAY_MIN_WIDTH, WAVE_DISPLAY_MAX_WIDTH);
          h = wave_display_dim(arg[2], h, WAVE_DISPLAY_MIN_HEIGHT, WAVE_DISPLAY_MAX_HEIGHT);
        }
        if (!show_record_buffer && skode_wave_valid(x)) {
          if (argc != 2) {
            wavetable_waveform_show(ctx, x, w, h, sw.loop_start[x],
              sw.loop_end[x], NULL);
          } else {
            ctx->printf(ctx, "# Wavetable Summary [%d..%d]\n", x, m);
            for (int i=x; i<=m; i++) {
              wavetable_summary_show(ctx, i);
            }
          }
        } else {
          if (atomic_load_int(&sampling.state) != SAMPLE_STATE_COMPLETE) {
            ctx->printf(ctx, "# recording buffer is not complete\n");
          } else if (sampling.where) {
            if ((sampling.offset > sampling.len) || (sampling.len - sampling.trim <= 0)) {
              ctx->printf(ctx,"NO!\n");
              ctx->printf(ctx, "offset:%d\n", sampling.offset);
              ctx->printf(ctx, "trim:%d\n", sampling.trim);
              ctx->printf(ctx, "len:%d\n", sampling.len);
              ctx->printf(ctx, "where:%p\n", sampling.where);
              ctx->printf(ctx, "state:%d\n",
                          atomic_load_int(&sampling.state));
            } else {
              float *display = sampling.where;
              float *mono = NULL;
              if (sampling.channels == 2) {
                mono = malloc((size_t)sampling.len * sizeof(float));
                if (mono) {
                  for (int i = 0; i < sampling.len; i++)
                    mono[i] = record_frame_mono(i);
                  display = mono;
                }
              }
              print_wave_stats(ctx, "recording", display, sampling.len,
                               (float)MAIN_SAMPLE_RATE);
              print_audio_braille_labeled(ctx, display, sampling.len, w, h,
                sampling.offset, sampling.len - sampling.trim);
              free(mono);
              int len = sampling.len - sampling.offset - sampling.trim;
              ctx->printf(ctx, "# recording channels %d\n",
                          sampling.channels == 2 ? 2 : 1);
              ctx->printf(ctx,"# found start %d end %d |%d| %gms\n",
                sampling.offset, sampling.len - sampling.trim, len,
                SAMPLES_TO_MSEC(len));
              ctx->printf(ctx,"+offset %d -trim %d = |%d| %gms\n",
                sampling.offset, sampling.trim, len,
                SAMPLES_TO_MSEC(len));
            }
          }
        }
      } else if (argc == 0) {
        int c = 0;
        ctx->printf(ctx, "# Wavetable Summary (MAX %d)\n", synth_config.wave_table_max);
        for (int i=0; i<synth_config.wave_table_max; i++) {
          wavetable_summary_show(ctx, i);
          c++;
        }
      }
      return 0;
}

static int word_exec_XM(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc) {
        sv.ring_osc[voice] = x_valid && skode_voice_valid(x) ? x : -1;
        if (argc > 1) sv.ring_amount[voice] = arg[1];
        else sv.ring_amount[voice] = 0.0;
      }
      return 0;
}

static int word_exec_v_q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  if (x_valid && skode_voice_valid(x)) voice = x;
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
  voice_show_connected(ctx, voice, ctx->verbose);
  return 0;
}

static int word_exec__q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  if (x_valid && skode_voice_valid(x)) voice = x;
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
  voice_show_connected(ctx, voice, ctx->verbose);
  return 0;
}

static int word_exec__bs_bs(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  if (x_valid && skode_voice_valid(x)) voice = x;
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
  voice_show_connected(ctx, voice, 1);
  return 0;
}

static int word_exec_v_q_bs_q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
  voice_show_all(ctx, voice, ctx->verbose); return 0;
}

static int word_exec__q_bs_q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      voice_show_all(ctx, voice, ctx->verbose); return 0;
}

static int word_exec_v_q_q_q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
  voice_show_names(ctx, voice); return 0;
}

static int word_exec__q_q_q(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
  voice_show_names(ctx, voice); return 0;
}

static int word_exec__qr(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      record_tracks_show(ctx); return 0;
}

static int word_exec__gt(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (x_valid && skode_voice_valid(x)) voice_copy(voice, x);
      return 0;
}

static int word_exec__slash(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      wave_default(voice);
      return 0;
}

static int word_exec__slashwex(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  uint32_t atom = ands_atom_num(s);
  int voice = ctx->voice;
  int x = 0;
  int x_valid = argc > 0 && skode_double_to_int(arg[0], &x);
  (void)x_valid;
  (void)voice;
  (void)self; (void)atom; (void)voice; (void)x; (void)x_valid;
      if (argc && x >= 200 && x <=999) wave_table_dynamic_expand(x);
      return 0;
}


/* 'on' and 'off' are gate-on / gate-off aliases for l1 / l0 */
static int word_exec_on(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  (void)self; (void)s; (void)arg; (void)argc;
  skode_linked_velocity(ctx->voice, 1.0, SAMPLE_COUNT_GET());
  return 0;
}
static int word_exec_off(const skode_word_t *self, skode_t *ctx, ands_t *s, double *arg, int argc) {
  (void)self; (void)s; (void)arg; (void)argc;
  skode_linked_velocity(ctx->voice, 0.0, SAMPLE_COUNT_GET());
  return 0;
}

int skode_exec_ds(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_ds(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_DG(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_DG(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_DL(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_DL(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_DL_q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_DL_q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_DD(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_DD(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_DF(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_DF(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_DP(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_DP(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_DT(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_DT(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_DS(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_DS(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_GS(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_GS(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_cte(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_cte(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_ft(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_ft(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_fte(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_fte(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_fd(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_fd(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_G(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_G(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_h(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_h(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_H(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_H(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_L(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_L(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_J(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_J(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_K(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_K(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec____l(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec____l(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_O(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_O(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_on(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_on(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_off(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_off(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_bc(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_Q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_Q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_r(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_r(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_rt(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_rt(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_rv(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_rv(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_te(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_te(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_vc(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_vc(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_V(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_V(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_vt(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_vt(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_WL(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_WL(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_VL(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_VL(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_VS(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_VS(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_VC(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_VC(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_VW(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_VW(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_w_gtd(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_w_gtd(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_w_gtr(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_w_gtr(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_w_gtw(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_w_gtw(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_w_bang(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_w_bang(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_w_star(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_w_star(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_w_gt(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_w_gt(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_w_lt(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_w_lt(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_w_lt_gt(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_w_lt_gt(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_W(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_W(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_slashwex(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec__slashwex(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_v_q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_v_q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec__q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec__q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec__bs_bs(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec__bs_bs(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_v_q_bs_q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_v_q_bs_q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec__q_bs_q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec__q_bs_q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec_v_q_q_q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec_v_q_q_q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec__q_q_q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec__q_q_q(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec__qr(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec__qr(NULL, ctx, s, (double*)arg, argc);
}
int skode_exec__slash(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  return word_exec__slash(NULL, ctx, s, (double*)arg, argc);
}

int skode_exec_tts(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!ctx || !ctx->parse) return -1;
  const char *phrase = ands_string(ctx->parse);
  if (!phrase || phrase[0] == '\0') {
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "tts! requires bracketed phonetic string");
    ctx->printf(ctx, "# tts: error: requires bracketed phonetic DSL string, e.g. [HH:70 EH1:110 L:70 OW1:150] tts!\n");
    return SK_ERR_INVALID_PARAM;
  }

  int wave_slot = -1;
  int voice_id = 0;       /* 0=Paul, 1=Harry, 2=Betty, 3=SAM, 4=Robot, 5=Elf */
  float base_pitch = 120.0f;
  float rate = (float)MAIN_SAMPLE_RATE;
  if (rate < 1000.0f) rate = 48000.0f;

  if (argc == 1) {
    int v;
    if (skode_double_to_int(arg[0], &v)) {
      if (v >= EXT_SAMPLE_000 && skode_wave_valid(v)) {
        wave_slot = v;
      } else if (v >= 0 && v <= 5) {
        voice_id = v;
      } else if (skode_wave_valid(v)) {
        wave_slot = v;
      }
    }
  } else if (argc >= 2) {
    int v0, v1;
    if (skode_double_to_int(arg[0], &v0) && skode_wave_valid(v0)) {
      wave_slot = v0;
    }
    if (skode_double_to_int(arg[1], &v1) && v1 >= 0 && v1 <= 5) {
      voice_id = v1;
    }
    if (argc >= 3 && arg[2] > 20.0 && arg[2] < 2000.0) {
      base_pitch = (float)arg[2];
    }
  }

  /* Compile phonetic DSL string into acoustic parameter frames */
  sk_phoneme_frame_t frames[SK_PHONEME_MAX_TOKENS];
  sk_cue_t cues[SK_MAX_CUES];
  int num_cues = 0;
  char bad_token[32] = {0};
  int num_frames = sk_phoneme_compile_dsl_markers(phrase, frames, SK_PHONEME_MAX_TOKENS, base_pitch,
                                                  bad_token, sizeof(bad_token),
                                                  cues, SK_MAX_CUES, &num_cues);
  if (bad_token[0] != '\0') {
    ctx->printf(ctx, "# tts: warning: unrecognized phoneme token '%s' (fallback to silence)\n", bad_token);
  }
  if (num_frames <= 0) {
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "tts: no valid phonemes parsed from [%s]", phrase);
    ctx->printf(ctx, "# tts: error: no valid phonemes parsed from [%s]\n", phrase);
    return SK_ERR_INVALID_PARAM;
  }

  /* Compute duration with safety cap of 30.0s (prevent OOM) */
  float total_ms = 0.0f;
  for (int i = 0; i < num_frames; i++) {
    total_ms += frames[i].duration_ms;
  }
  if (total_ms > 30000.0f) {
    ctx->printf(ctx, "# tts: duration %.1f ms exceeds 30s limit; clamping\n", total_ms);
    total_ms = 30000.0f;
  }

  int total_samples = (int)((total_ms / 1000.0f) * rate) + (int)(rate * 0.1f);
  if (total_samples <= 0 || total_samples > (int)(rate * 30.0f)) {
    ctx->printf(ctx, "# tts: invalid sample count %d\n", total_samples);
    return 0;
  }

  /* Select voice persona */
  sk_speech_voice_t voice;
  switch (voice_id) {
    case 1: voice = sk_speech_voice_harry(); break;
    case 2: voice = sk_speech_voice_betty(); break;
    case 3: voice = sk_speech_voice_sam(); break;
    case 4: voice = sk_speech_voice_robot(); break;
    case 5: voice = sk_speech_voice_elf(); break;
    case 0:
    default: voice = sk_speech_voice_paul(); break;
  }

  float *audio_buf = (float *)calloc((size_t)total_samples, sizeof(float));
  if (!audio_buf) {
    ctx->printf(ctx, "# tts: memory allocation failed\n");
    return 0;
  }

  sk_rng_t rng;
  sk_rng_init(&rng, 1234567);
  int rendered = sk_phoneme_render_frames_voice(frames, num_frames, audio_buf, total_samples, rate, &voice, &rng);
  if (rendered <= 0) {
    free(audio_buf);
    ctx->printf(ctx, "# tts: synthesis produced 0 samples\n");
    return 0;
  }

  /* Peak normalize speech audio buffer so output matches full-scale oscillator levels */
  normalize_preserve_zero(audio_buf, rendered);

  /* Resize and write directly into skode data buffer (ands_data) */
  ands_data_resize(ctx->parse, rendered);
  double *d = ands_data(ctx->parse);
  if (!d || ands_data_cap(ctx->parse) < rendered) {
    free(audio_buf);
    ctx->printf(ctx, "# tts: failed to allocate data buffer\n");
    return 0;
  }
  for (int i = 0; i < rendered; i++) {
    d[i] = (double)audio_buf[i];
  }
  ands_data_len_set(ctx->parse, rendered);
  free(audio_buf);

  /* Convert cues to sample offsets and format universal metadata string */
  sk_phoneme_cues_to_samples(cues, num_cues, frames, num_frames, rate, (uint32_t)rendered);
  sk_meta_info_t meta;
  sk_meta_init(&meta);
  meta.mode = SK_WAVE_MODE_SAMPLE;
  meta.rate = rate;
  meta.has_rate = 1;
  meta.one_shot = 1;
  meta.has_one_shot = 1;
  meta.num_cues = num_cues;
  for (int i = 0; i < num_cues; i++) {
    meta.cues[i] = cues[i];
  }
  char meta_str[512];
  sk_format_metadata(&meta, meta_str, sizeof(meta_str));
  ands_string_from_external(ctx->parse, meta_str, (int)strlen(meta_str));

  ctx->printf(ctx, "# tts: rendered %d samples (%.2fs) to data\n", rendered, (double)rendered / rate);

  /* If a valid wave slot was provided, install directly as one-shot sample */
  if (wave_slot >= 0 && skode_wave_valid(wave_slot)) {
    data_load(ctx, wave_slot, 1 /* one_shot */, rate, 0.0f);
    ctx->printf(ctx, "# tts: installed to w%d\n", wave_slot);
  }

  return 0;
}

/* -------------------------------------------------------------------------
 * Physical Modeling Sample Synthesis DSL & Wave Renderer
 * ------------------------------------------------------------------------- */

typedef enum {
  DSL_FAM_DRUM = 0,
  DSL_FAM_BELL,
  DSL_FAM_STRING,
  DSL_FAM_WIND
} dsl_family_t;

typedef enum {
  DSL_DRUM_KICK = 0,
  DSL_DRUM_SNARE,
  DSL_DRUM_HAT,
  DSL_DRUM_CLAP,
  DSL_DRUM_TOM,
  DSL_DRUM_COWBELL,
  DSL_DRUM_MEMBRANE,

  DSL_BELL_CHURCH,
  DSL_BELL_TUBULAR,
  DSL_BELL_VIBE,
  DSL_BELL_TIBETAN,
  DSL_BELL_AGOGO,

  DSL_STRING_GUITAR,
  DSL_STRING_BASS,
  DSL_STRING_HARP,
  DSL_STRING_KOTO,
  DSL_STRING_SITAR,
  DSL_STRING_PIZZ,
  DSL_STRING_RAW,

  DSL_WIND_BRASS,
  DSL_WIND_FLUTE
} dsl_preset_t;

typedef struct {
  dsl_family_t family;
  dsl_preset_t preset;
  char preset_name[32];
  float freq;
  float midi_note;
  int has_pitch;
  float duration;     /* duration in seconds (0 = auto) */
  float decay;        /* decay scale or seconds */
  float pdecay;       /* pitch decay seconds */
  float click;        /* click level */
  float drive;        /* saturation drive */
  float end_freq;     /* sub pitch */
  float snap;         /* snare snap */
  float tail;         /* snare / clap tail seconds */
  float f1, f2;       /* modal freqs */
  int hat_open;       /* hihat open */
  float spread;       /* clap burst spacing ms */
  int bursts;         /* clap burst count */
  float drop;         /* tom pitch drop */
  float blend;        /* kdrum membrane blend */
  float mallet;       /* bell mallet hardness */
  float brightness;   /* excitation brightness */
  float pick;         /* string pick pos */
  float damping;      /* string/bore damping */
  float feedback;     /* string feedback loop */
  float pressure;     /* wind blowing pressure */
  float brassiness;   /* brass non-linear factor */
  float noise;        /* breath noise */
  float vibrato;      /* vibrato depth */
  float vibrato_rate; /* vibrato freq */
  float velocity;     /* strike velocity */
  int loop;           /* loop enabled (-1 = auto, 0 = off, 1 = on) */
} dsl_params_t;

static float parse_dsl_pitch(const char *val, float default_val, float *out_midi_note, int *out_has_pitch) {
  if (!val || !*val) return default_val;
  while (*val == ' ' || *val == ':' || *val == '=') val++;
  if (!*val) return default_val;

  /* MIDI note notation: n60, N60, n:60 */
  if (val[0] == 'n' || val[0] == 'N') {
    const char *p = val + 1;
    while (*p == ' ' || *p == ':' || *p == '=') p++;
    float n = (float)atof(p);
    if (out_midi_note) *out_midi_note = n;
    if (out_has_pitch) *out_has_pitch = 1;
    return sk_mtof(n);
  }

  /* Note names: c4, a4, f#3, eb2, etc. */
  char c = (char)tolower((unsigned char)val[0]);
  if (c >= 'a' && c <= 'g') {
    static const int semitones[7] = { 9, 11, 0, 2, 4, 5, 7 }; /* a=9, b=11, c=0, d=2, e=4, f=5, g=7 */
    int semi = semitones[c - 'a'];
    const char *p = val + 1;
    if (*p == '#' || *p == 's' || *p == 'S') {
      semi++;
      p++;
    } else if (*p == 'b' || *p == 'B') {
      semi--;
      p++;
    }
    int oct = 4;
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
      oct = atoi(p);
    }
    float midi = (float)((oct + 1) * 12 + semi);
    if (out_midi_note) *out_midi_note = midi;
    if (out_has_pitch) *out_has_pitch = 1;
    return sk_mtof(midi);
  }

  /* Numerical value */
  float num = (float)atof(val);
  if (num > 0.0f) {
    if (num >= 20.0f) {
      /* Frequency in Hz */
      if (out_midi_note) *out_midi_note = sk_ftom(num);
      if (out_has_pitch) *out_has_pitch = 1;
      return num;
    } else {
      /* Small number treated as MIDI note */
      if (out_midi_note) *out_midi_note = num;
      if (out_has_pitch) *out_has_pitch = 1;
      return sk_mtof(num);
    }
  }
  return default_val;
}

static int find_sustain_loop(const float *audio, int len, float freq, float sample_rate, int *loop_start, int *loop_end) {
  if (!audio || len < 256 || freq < 20.0f || sample_rate < 1000.0f) return 0;

  float period = sample_rate / freq;
  if (period < 2.0f || period > (float)(len / 3)) return 0;

  /* Skip initial attack transient (~18% of buffer or at least 150ms) */
  int search_start = (int)(sample_rate * 0.18f);
  if (search_start > len / 3) search_start = len / 4;
  if (search_start + (int)(period * 3.0f) >= len) return 0;

  /* Find first positive-going zero crossing at or after search_start */
  int start_idx = -1;
  for (int i = search_start; i < search_start + (int)(period * 2.5f) && i < len - 2; i++) {
    if (audio[i] <= 0.0f && audio[i + 1] > 0.0f) {
      start_idx = i;
      break;
    }
  }
  if (start_idx < 0) return 0;

  /* Target loop duration ~250-350ms, containing integer k periods */
  float target_sec = 0.28f;
  int k = (int)(target_sec * freq + 0.5f);
  if (k < 3) k = 3;

  int max_end = len - (int)(sample_rate * 0.04f); /* leave at least 40ms tail */
  int nominal_end = start_idx + (int)(k * period + 0.5f);
  while (nominal_end >= max_end && k > 1) {
    k--;
    nominal_end = start_idx + (int)(k * period + 0.5f);
  }
  if (k < 1 || nominal_end >= max_end) return 0;

  /* Search around nominal_end for the zero crossing with matching slope */
  float start_slope = audio[start_idx + 1] - audio[start_idx];
  int radius = (int)(period * 0.6f);
  if (radius < 2) radius = 2;

  int best_end = -1;
  float best_diff = 1e9f;

  int scan_min = nominal_end - radius;
  int scan_max = nominal_end + radius;
  if (scan_min <= start_idx + (int)period) scan_min = start_idx + (int)period + 1;
  if (scan_max >= len - 2) scan_max = len - 2;

  for (int j = scan_min; j <= scan_max; j++) {
    if (audio[j] <= 0.0f && audio[j + 1] > 0.0f) {
      float slope = audio[j + 1] - audio[j];
      float diff = fabsf(slope - start_slope) + fabsf(audio[j] - audio[start_idx]);
      if (diff < best_diff) {
        best_diff = diff;
        best_end = j;
      }
    }
  }

  if (best_end > start_idx + 8) {
    if (loop_start) *loop_start = start_idx;
    if (loop_end) *loop_end = best_end;
    return 1;
  }
  return 0;
}

int skode_exec_sample_render(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!ctx || !ctx->parse) return -1;
  const char *raw_dsl = ands_string(ctx->parse);
  if (!raw_dsl || raw_dsl[0] == '\0') {
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "/render requires bracketed instrument DSL string");
    ctx->printf(ctx, "# render: error: requires bracketed instrument DSL string, e.g. [drum:kick] /render\n");
    return SK_ERR_INVALID_PARAM;
  }

  /* Strip optional outer brackets and leading/trailing whitespace */
  while (*raw_dsl == ' ' || *raw_dsl == '\t') raw_dsl++;
  char dsl_buf[512];
  size_t dsl_len = strlen(raw_dsl);
  if (dsl_len >= sizeof(dsl_buf)) dsl_len = sizeof(dsl_buf) - 1;
  memcpy(dsl_buf, raw_dsl, dsl_len);
  dsl_buf[dsl_len] = '\0';

  char *str = dsl_buf;
  if (*str == '[') str++;
  char *end = str + strlen(str) - 1;
  while (end > str && (*end == ']' || isspace((unsigned char)*end))) {
    *end = '\0';
    end--;
  }

  /* Initialize parameter structure with neutral defaults */
  dsl_params_t p;
  memset(&p, 0, sizeof(p));
  p.family = DSL_FAM_DRUM;
  p.preset = DSL_DRUM_KICK;
  strcpy(p.preset_name, "drum:kick");
  p.freq = 150.0f;
  p.end_freq = 48.0f;
  p.decay = 0.35f;
  p.pdecay = 0.035f;
  p.click = 0.30f;
  p.drive = 1.25f;
  p.mallet = 0.40f;
  p.velocity = 1.0f;
  p.loop = -1; /* auto */

  /* Parse tokens */
  char *tok = strtok(str, " \t,;");
  int first_tok = 1;

  while (tok) {
    if (first_tok) {
      first_tok = 0;
      char ident[64];
      strncpy(ident, tok, sizeof(ident) - 1);
      ident[sizeof(ident) - 1] = '\0';
      for (size_t i = 0; i < strlen(ident); i++) ident[i] = (char)tolower((unsigned char)ident[i]);

      char *colon = strchr(ident, ':');
      char *fam_str = ident;
      char *pre_str = colon ? colon + 1 : ident;
      if (colon) *colon = '\0';

      /* Match family and preset */
      if (strcmp(fam_str, "drum") == 0 || strcmp(fam_str, "perc") == 0 ||
          strcmp(pre_str, "kick") == 0 || strcmp(pre_str, "snare") == 0 ||
          strcmp(pre_str, "hat") == 0 || strcmp(pre_str, "hihat") == 0 ||
          strcmp(pre_str, "clap") == 0 || strcmp(pre_str, "handclap") == 0 ||
          strcmp(pre_str, "tom") == 0 || strcmp(pre_str, "cowbell") == 0 ||
          strcmp(pre_str, "membrane") == 0 || strcmp(pre_str, "kdrum") == 0) {
        p.family = DSL_FAM_DRUM;
        if (strcmp(pre_str, "snare") == 0) {
          p.preset = DSL_DRUM_SNARE;
          strcpy(p.preset_name, "drum:snare");
          p.f1 = 175.0f; p.f2 = 224.0f; p.snap = 0.55f; p.decay = 0.085f; p.tail = 0.200f; p.freq = 2800.0f;
        } else if (strcmp(pre_str, "hat") == 0 || strcmp(pre_str, "hihat") == 0) {
          p.preset = DSL_DRUM_HAT;
          strcpy(p.preset_name, "drum:hat");
          p.hat_open = 0; p.decay = 0.050f;
        } else if (strcmp(pre_str, "clap") == 0 || strcmp(pre_str, "handclap") == 0) {
          p.preset = DSL_DRUM_CLAP;
          strcpy(p.preset_name, "drum:clap");
          p.bursts = 3; p.spread = 11.0f; p.tail = 0.220f; p.freq = 1100.0f;
        } else if (strcmp(pre_str, "tom") == 0) {
          p.preset = DSL_DRUM_TOM;
          strcpy(p.preset_name, "drum:tom");
          p.freq = 100.0f; p.decay = 0.260f; p.pdecay = 0.045f; p.drop = 0.40f;
          p.has_pitch = 1; p.midi_note = sk_ftom(p.freq);
        } else if (strcmp(pre_str, "cowbell") == 0) {
          p.preset = DSL_DRUM_COWBELL;
          strcpy(p.preset_name, "drum:cowbell");
          p.f1 = 540.0f; p.f2 = 800.0f; p.decay = 0.160f; p.freq = 800.0f;
        } else if (strcmp(pre_str, "membrane") == 0 || strcmp(pre_str, "kdrum") == 0) {
          p.preset = DSL_DRUM_MEMBRANE;
          strcpy(p.preset_name, "drum:membrane");
          p.freq = 80.0f; p.blend = 0.50f; p.damping = 0.55f; p.feedback = 0.988f; p.brightness = 0.50f;
          p.has_pitch = 1; p.midi_note = sk_ftom(p.freq);
        } else {
          p.preset = DSL_DRUM_KICK;
          strcpy(p.preset_name, "drum:kick");
          p.freq = 150.0f; p.end_freq = 48.0f; p.decay = 0.35f; p.pdecay = 0.035f; p.click = 0.30f; p.drive = 1.25f;
        }
      } else if (strcmp(fam_str, "bell") == 0 || strcmp(fam_str, "metal") == 0 ||
                 strcmp(pre_str, "church") == 0 || strcmp(pre_str, "tubular") == 0 ||
                 strcmp(pre_str, "chime") == 0 || strcmp(pre_str, "vibe") == 0 ||
                 strcmp(pre_str, "vibraphone") == 0 || strcmp(pre_str, "glock") == 0 ||
                 strcmp(pre_str, "marimba") == 0 || strcmp(pre_str, "tibetan") == 0 ||
                 strcmp(pre_str, "bowl") == 0 || strcmp(pre_str, "agogo") == 0) {
        p.family = DSL_FAM_BELL;
        p.decay = 1.0f;
        p.has_pitch = 1;
        if (strcmp(pre_str, "tubular") == 0 || strcmp(pre_str, "chime") == 0) {
          p.preset = DSL_BELL_TUBULAR;
          strcpy(p.preset_name, "bell:tubular");
          p.freq = 440.0f; p.midi_note = 69.0f;
        } else if (strcmp(pre_str, "vibe") == 0 || strcmp(pre_str, "vibraphone") == 0 ||
                   strcmp(pre_str, "glock") == 0 || strcmp(pre_str, "marimba") == 0) {
          p.preset = DSL_BELL_VIBE;
          strcpy(p.preset_name, "bell:vibe");
          p.freq = 440.0f; p.midi_note = 69.0f;
        } else if (strcmp(pre_str, "tibetan") == 0 || strcmp(pre_str, "bowl") == 0) {
          p.preset = DSL_BELL_TIBETAN;
          strcpy(p.preset_name, "bell:tibetan");
          p.freq = 220.0f; p.midi_note = 57.0f;
          p.loop = 1; /* Tibetan singing bowl sustains by default */
        } else if (strcmp(pre_str, "agogo") == 0) {
          p.preset = DSL_BELL_AGOGO;
          strcpy(p.preset_name, "bell:agogo");
          p.freq = 587.33f; p.midi_note = 74.0f;
        } else {
          p.preset = DSL_BELL_CHURCH;
          strcpy(p.preset_name, "bell:church");
          p.freq = 220.0f; p.midi_note = 57.0f;
        }
      } else if (strcmp(fam_str, "string") == 0 || strcmp(fam_str, "pluck") == 0 ||
                 strcmp(pre_str, "guitar") == 0 || strcmp(pre_str, "bass") == 0 ||
                 strcmp(pre_str, "harp") == 0 || strcmp(pre_str, "koto") == 0 ||
                 strcmp(pre_str, "zither") == 0 || strcmp(pre_str, "sitar") == 0 ||
                 strcmp(pre_str, "pizz") == 0 || strcmp(pre_str, "raw") == 0 ||
                 strcmp(pre_str, "clean") == 0) {
        p.family = DSL_FAM_STRING;
        p.has_pitch = 1;
        if (strcmp(pre_str, "bass") == 0) {
          p.preset = DSL_STRING_BASS;
          strcpy(p.preset_name, "string:bass");
          p.freq = 82.41f; p.midi_note = 40.0f; p.pick = 0.35f; p.damping = 0.55f; p.feedback = 0.995f; p.brightness = 0.50f;
        } else if (strcmp(pre_str, "harp") == 0) {
          p.preset = DSL_STRING_HARP;
          strcpy(p.preset_name, "string:harp");
          p.freq = 440.0f; p.midi_note = 69.0f; p.pick = 0.50f; p.damping = 0.25f; p.feedback = 0.997f; p.brightness = 0.80f;
        } else if (strcmp(pre_str, "koto") == 0 || strcmp(pre_str, "zither") == 0) {
          p.preset = DSL_STRING_KOTO;
          strcpy(p.preset_name, "string:koto");
          p.freq = 329.63f; p.midi_note = 64.0f; p.pick = 0.12f; p.damping = 0.30f; p.feedback = 0.992f; p.brightness = 0.75f;
        } else if (strcmp(pre_str, "sitar") == 0) {
          p.preset = DSL_STRING_SITAR;
          strcpy(p.preset_name, "string:sitar");
          p.freq = 220.0f; p.midi_note = 57.0f; p.pick = 0.15f; p.damping = 0.65f; p.feedback = 0.990f; p.brightness = 0.85f;
        } else if (strcmp(pre_str, "pizz") == 0) {
          p.preset = DSL_STRING_PIZZ;
          strcpy(p.preset_name, "string:pizz");
          p.freq = 440.0f; p.midi_note = 69.0f; p.pick = 0.40f; p.damping = 0.70f; p.feedback = 0.980f; p.brightness = 0.40f;
        } else if (strcmp(pre_str, "raw") == 0 || strcmp(pre_str, "clean") == 0) {
          p.preset = DSL_STRING_RAW;
          strcpy(p.preset_name, "string:raw");
          p.freq = 440.0f; p.midi_note = 69.0f; p.pick = 0.25f; p.damping = 0.50f; p.feedback = 0.992f; p.brightness = 0.60f;
        } else {
          p.preset = DSL_STRING_GUITAR;
          strcpy(p.preset_name, "string:guitar");
          p.freq = 261.63f; p.midi_note = 60.0f; p.pick = 0.25f; p.damping = 0.45f; p.feedback = 0.994f; p.brightness = 0.70f;
        }
      } else if (strcmp(fam_str, "horn") == 0 || strcmp(fam_str, "brass") == 0 ||
                 strcmp(fam_str, "wind") == 0 || strcmp(fam_str, "flute") == 0 ||
                 strcmp(pre_str, "trumpet") == 0 || strcmp(pre_str, "trombone") == 0 ||
                 strcmp(pre_str, "tuba") == 0 || strcmp(pre_str, "pipe") == 0 ||
                 strcmp(pre_str, "recorder") == 0) {
        p.family = DSL_FAM_WIND;
        p.has_pitch = 1;
        p.duration = 1.2f;
        p.loop = 1; /* Continuous wind/horn instruments sustain loop by default */
        if (strcmp(pre_str, "flute") == 0 || strcmp(pre_str, "pipe") == 0 || strcmp(pre_str, "recorder") == 0 ||
            strcmp(fam_str, "flute") == 0) {
          p.preset = DSL_WIND_FLUTE;
          strcpy(p.preset_name, "wind:flute");
          p.freq = 523.25f; p.midi_note = 72.0f; p.pressure = 0.70f; p.noise = 0.12f; p.vibrato = 0.03f; p.vibrato_rate = 5.5f; p.damping = 0.35f;
        } else {
          p.preset = DSL_WIND_BRASS;
          strcpy(p.preset_name, "horn:brass");
          p.freq = 233.08f; p.midi_note = 58.0f; p.pressure = 0.75f; p.brassiness = 0.50f; p.damping = 0.25f;
        }
      }
    } else {
      /* Key-value or shorthand token */
      char *sep = strchr(tok, ':');
      if (!sep) sep = strchr(tok, '=');
      if (sep) {
        *sep = '\0';
        char *key = tok;
        char *val = sep + 1;
        for (size_t i = 0; i < strlen(key); i++) key[i] = (char)tolower((unsigned char)key[i]);

        if (strcmp(key, "f") == 0 || strcmp(key, "freq") == 0 || strcmp(key, "hz") == 0 || strcmp(key, "pitch") == 0) {
          p.freq = parse_dsl_pitch(val, p.freq, &p.midi_note, &p.has_pitch);
        } else if (strcmp(key, "n") == 0 || strcmp(key, "note") == 0 || strcmp(key, "midi") == 0) {
          p.freq = parse_dsl_pitch(val, p.freq, &p.midi_note, &p.has_pitch);
        } else if (strcmp(key, "d") == 0 || strcmp(key, "decay") == 0 || strcmp(key, "t60") == 0) {
          p.decay = (float)atof(val);
        } else if (strcmp(key, "dur") == 0 || strcmp(key, "duration") == 0 || strcmp(key, "len") == 0) {
          p.duration = (float)atof(val);
        } else if (strcmp(key, "pdecay") == 0) {
          p.pdecay = (float)atof(val);
        } else if (strcmp(key, "end") == 0) {
          p.end_freq = parse_dsl_pitch(val, p.end_freq, NULL, NULL);
        } else if (strcmp(key, "c") == 0 || strcmp(key, "click") == 0) {
          p.click = (float)atof(val);
        } else if (strcmp(key, "drive") == 0) {
          p.drive = (float)atof(val);
        } else if (strcmp(key, "snap") == 0 || strcmp(key, "snappy") == 0) {
          p.snap = (float)atof(val);
        } else if (strcmp(key, "tail") == 0) {
          p.tail = (float)atof(val);
        } else if (strcmp(key, "f1") == 0) {
          p.f1 = parse_dsl_pitch(val, p.f1, NULL, NULL);
        } else if (strcmp(key, "f2") == 0) {
          p.f2 = parse_dsl_pitch(val, p.f2, NULL, NULL);
        } else if (strcmp(key, "open") == 0) {
          p.hat_open = atoi(val);
          if (p.hat_open && p.decay <= 0.05f) p.decay = 0.35f;
        } else if (strcmp(key, "spread") == 0 || strcmp(key, "space") == 0) {
          p.spread = (float)atof(val);
        } else if (strcmp(key, "bursts") == 0 || strcmp(key, "count") == 0) {
          p.bursts = atoi(val);
        } else if (strcmp(key, "drop") == 0) {
          p.drop = (float)atof(val);
        } else if (strcmp(key, "blend") == 0) {
          p.blend = (float)atof(val);
        } else if (strcmp(key, "m") == 0 || strcmp(key, "mallet") == 0) {
          p.mallet = (float)atof(val);
        } else if (strcmp(key, "b") == 0 || strcmp(key, "bright") == 0 || strcmp(key, "brightness") == 0) {
          p.brightness = (float)atof(val);
        } else if (strcmp(key, "p") == 0 || strcmp(key, "pick") == 0) {
          p.pick = (float)atof(val);
        } else if (strcmp(key, "damp") == 0 || strcmp(key, "damping") == 0) {
          p.damping = (float)atof(val);
        } else if (strcmp(key, "fb") == 0 || strcmp(key, "feedback") == 0) {
          p.feedback = (float)atof(val);
        } else if (strcmp(key, "pr") == 0 || strcmp(key, "press") == 0 || strcmp(key, "pressure") == 0) {
          p.pressure = (float)atof(val);
        } else if (strcmp(key, "br") == 0 || strcmp(key, "brass") == 0 || strcmp(key, "brassiness") == 0) {
          p.brassiness = (float)atof(val);
        } else if (strcmp(key, "air") == 0 || strcmp(key, "noise") == 0) {
          p.noise = (float)atof(val);
        } else if (strcmp(key, "vib") == 0 || strcmp(key, "vibrato") == 0) {
          p.vibrato = (float)atof(val);
        } else if (strcmp(key, "vibr") == 0 || strcmp(key, "vibrate") == 0) {
          p.vibrato_rate = (float)atof(val);
        } else if (strcmp(key, "vel") == 0 || strcmp(key, "velocity") == 0) {
          p.velocity = (float)atof(val);
        } else if (strcmp(key, "loop") == 0) {
          p.loop = atoi(val);
        }
      } else {
        /* Shorthand without key: e.g. "n60", "a4", "c3" */
        if (tok[0] == 'n' || tok[0] == 'N' || (tolower((unsigned char)tok[0]) >= 'a' && tolower((unsigned char)tok[0]) <= 'g')) {
          p.freq = parse_dsl_pitch(tok, p.freq, &p.midi_note, &p.has_pitch);
        }
      }
    }
    tok = strtok(NULL, " \t,;");
  }

  /* Compute duration with full natural decay to silence */
  float duration = p.duration;
  if (duration <= 0.0f) {
    switch (p.preset) {
      case DSL_DRUM_KICK: {
        duration = p.decay * 6.5f + 0.15f;
        break;
      }
      case DSL_DRUM_SNARE: {
        float max_t = p.decay > p.tail ? p.decay : p.tail;
        duration = max_t * 6.5f + 0.15f;
        break;
      }
      case DSL_DRUM_HAT: {
        duration = p.decay * 6.5f + 0.10f;
        break;
      }
      case DSL_DRUM_CLAP: {
        duration = (p.bursts * p.spread * 0.001f) + p.tail * 6.5f + 0.15f;
        break;
      }
      case DSL_DRUM_TOM: {
        duration = p.decay * 6.5f + 0.15f;
        break;
      }
      case DSL_DRUM_COWBELL: {
        duration = p.decay * 6.5f + 0.10f;
        break;
      }
      case DSL_DRUM_MEMBRANE: {
        duration = 2.20f;
        break;
      }

      case DSL_BELL_CHURCH: {
        float sc = p.decay > 0.05f ? p.decay : 1.0f;
        duration = 5.5f * sc;
        break;
      }
      case DSL_BELL_TUBULAR: {
        float sc = p.decay > 0.05f ? p.decay : 1.0f;
        duration = 4.5f * sc;
        break;
      }
      case DSL_BELL_VIBE: {
        float sc = p.decay > 0.05f ? p.decay : 1.0f;
        duration = 4.0f * sc;
        break;
      }
      case DSL_BELL_TIBETAN: {
        float sc = p.decay > 0.05f ? p.decay : 1.0f;
        duration = 7.0f * sc;
        break;
      }
      case DSL_BELL_AGOGO: {
        float sc = p.decay > 0.05f ? p.decay : 1.0f;
        duration = 1.8f * sc;
        break;
      }

      case DSL_STRING_GUITAR:
      case DSL_STRING_BASS:
      case DSL_STRING_HARP:
      case DSL_STRING_KOTO:
      case DSL_STRING_SITAR:
      case DSL_STRING_PIZZ:
      case DSL_STRING_RAW: {
        duration = 3.5f;
        break;
      }

      case DSL_WIND_BRASS:
      case DSL_WIND_FLUTE: {
        duration = 1.5f;
        break;
      }
      default:
        duration = 1.5f;
        break;
    }
  }
  duration = sk_clampf(duration, 0.02f, 15.0f);

  float rate = (float)MAIN_SAMPLE_RATE;
  if (rate < 1000.0f) rate = 48000.0f;
  int total_samples = (int)(duration * rate);
  if (total_samples <= 0 || total_samples > (int)(rate * 15.0f)) {
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "render: invalid sample count %d", total_samples);
    return SK_ERR_INVALID_PARAM;
  }

  float *buf = (float *)calloc((size_t)total_samples, sizeof(float));
  if (!buf) {
    ctx->printf(ctx, "# render: memory allocation failed\n");
    return 0;
  }

  /* Synthesis execution */
  if (p.family == DSL_FAM_DRUM) {
    switch (p.preset) {
      case DSL_DRUM_KICK: {
        sk_drum_kick_t k;
        sk_drum_kick_init(&k, rate);
        if (p.freq > 20.0f) k.start_freq = p.freq;
        if (p.end_freq > 10.0f) k.end_freq = p.end_freq;
        if (p.decay > 0.01f) k.amp_decay_coeff = expf(-1.0f / (p.decay * rate));
        if (p.pdecay > 0.001f) k.pitch_decay_coeff = expf(-1.0f / (p.pdecay * rate));
        if (p.click >= 0.0f) k.click_level = sk_clampf(p.click, 0.0f, 1.0f);
        if (p.drive > 0.0f) k.drive = sk_clampf(p.drive, 0.1f, 10.0f);
        sk_drum_kick_trigger(&k, p.velocity > 0.0f ? p.velocity : 1.0f);
        for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_kick_step(&k);
        break;
      }
      case DSL_DRUM_SNARE: {
        sk_drum_snare_t sn;
        sk_drum_snare_init(&sn, rate);
        if (p.f1 > 20.0f) sn.f1 = p.f1;
        if (p.f2 > 20.0f) sn.f2 = p.f2;
        if (p.snap >= 0.0f) sn.snappy_mix = sk_clampf(p.snap, 0.0f, 1.0f);
        if (p.decay > 0.01f) sn.body_decay_coeff = expf(-1.0f / (p.decay * rate));
        if (p.tail > 0.01f) sn.noise_decay_coeff = expf(-1.0f / (p.tail * rate));
        if (p.freq > 100.0f) sk_biquad_init(&sn.noise_filter, SK_BIQUAD_BANDPASS, p.freq, 1.5f, rate);
        sk_drum_snare_trigger(&sn, p.velocity > 0.0f ? p.velocity : 1.0f);
        for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_snare_step(&sn);
        break;
      }
      case DSL_DRUM_HAT: {
        sk_drum_hat_t h;
        sk_drum_hat_init(&h, rate);
        sk_drum_hat_trigger(&h, p.velocity > 0.0f ? p.velocity : 1.0f, p.hat_open);
        if (p.decay > 0.005f) h.amp_decay_coeff = expf(-1.0f / (p.decay * rate));
        for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_hat_step(&h);
        break;
      }
      case DSL_DRUM_CLAP: {
        sk_drum_clap_t cl;
        sk_drum_clap_init(&cl, rate);
        if (p.spread > 1.0f) cl.burst_spacing = (int)(p.spread * 0.001f * rate);
        if (p.tail > 0.01f) cl.tail_decay_coeff = expf(-1.0f / (p.tail * rate));
        if (p.freq > 100.0f) sk_biquad_init(&cl.filter, SK_BIQUAD_BANDPASS, p.freq, 1.2f, rate);
        sk_drum_clap_trigger(&cl, p.velocity > 0.0f ? p.velocity : 1.0f);
        for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_clap_step(&cl);
        break;
      }
      case DSL_DRUM_TOM: {
        sk_drum_tom_t tm;
        sk_drum_tom_init(&tm, rate, p.freq > 20.0f ? p.freq : 100.0f);
        if (p.decay > 0.01f) tm.amp_decay_coeff = expf(-1.0f / (p.decay * rate));
        if (p.pdecay > 0.001f) tm.pitch_decay_coeff = expf(-1.0f / (p.pdecay * rate));
        sk_drum_tom_trigger(&tm, p.velocity > 0.0f ? p.velocity : 1.0f);
        for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_tom_step(&tm);
        break;
      }
      case DSL_DRUM_COWBELL: {
        sk_drum_cowbell_t cb;
        sk_drum_cowbell_init(&cb, rate);
        if (p.f1 > 20.0f) cb.inc1 = p.f1 / rate;
        if (p.f2 > 20.0f) cb.inc2 = p.f2 / rate;
        if (p.decay > 0.01f) cb.amp_decay_coeff = expf(-1.0f / (p.decay * rate));
        if (p.freq > 100.0f) sk_biquad_init(&cb.filter, SK_BIQUAD_BANDPASS, p.freq, 2.2f, rate);
        sk_drum_cowbell_trigger(&cb, p.velocity > 0.0f ? p.velocity : 1.0f);
        for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_cowbell_step(&cb);
        break;
      }
      case DSL_DRUM_MEMBRANE: {
        sk_karplus_t ks;
        sk_karplus_init(&ks, rate);
        sk_rng_t rng;
        sk_rng_init(&rng, 12345ULL);
        if (p.damping > 0.0f) ks.damping = sk_clampf(p.damping, 0.01f, 0.99f);
        if (p.feedback > 0.0f) ks.feedback = sk_clampf(p.feedback, 0.8f, 0.999f);
        sk_karplus_strike_drum(&ks, p.freq > 20.0f ? p.freq : 80.0f,
                               p.velocity > 0.0f ? p.velocity : 1.0f,
                               p.blend >= 0.0f ? p.blend : 0.5f,
                               p.brightness >= 0.0f ? p.brightness : 0.5f,
                               &rng);
        for (int i = 0; i < total_samples; i++) buf[i] = sk_karplus_step(&ks);
        break;
      }
      default:
        break;
    }
  } else if (p.family == DSL_FAM_BELL) {
    sk_bell_type_t btype = SK_BELL_CHURCH;
    if (p.preset == DSL_BELL_TUBULAR) btype = SK_BELL_TUBULAR;
    else if (p.preset == DSL_BELL_VIBE) btype = SK_BELL_VIBE;
    else if (p.preset == DSL_BELL_TIBETAN) btype = SK_BELL_TIBETAN;
    else if (p.preset == DSL_BELL_AGOGO) btype = SK_BELL_AGOGO;

    sk_drum_bell_t b;
    sk_drum_bell_init(&b, rate, btype, p.freq > 20.0f ? p.freq : 440.0f);
    if (p.decay > 0.05f) {
      sk_drum_bell_set_preset(&b, btype, p.freq > 20.0f ? p.freq : 440.0f, p.decay);
    }
    if (p.mallet >= 0.0f) {
      float cutoff = p.freq > 100.0f ? p.freq * 4.0f : 3500.0f;
      if (cutoff > rate * 0.45f) cutoff = rate * 0.45f;
      sk_biquad_init(&b.strike_filter, SK_BIQUAD_BANDPASS, cutoff, 2.0f, rate);
    }
    sk_drum_bell_trigger(&b, p.velocity > 0.0f ? p.velocity : 1.0f);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_bell_step(&b);
  } else if (p.family == DSL_FAM_STRING) {
    sk_karplus_t ks;
    sk_karplus_init(&ks, rate);
    sk_rng_t rng;
    sk_rng_init(&rng, 54321ULL);
    if (p.damping > 0.0f) ks.damping = sk_clampf(p.damping, 0.01f, 0.99f);
    if (p.feedback > 0.0f) ks.feedback = sk_clampf(p.feedback, 0.85f, 0.999f);
    sk_karplus_pluck(&ks, p.freq > 20.0f ? p.freq : 440.0f,
                     p.velocity > 0.0f ? p.velocity : 1.0f,
                     p.pick > 0.0f ? p.pick : 0.25f,
                     p.brightness >= 0.0f ? p.brightness : 0.6f,
                     &rng);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_karplus_step(&ks);
  } else if (p.family == DSL_FAM_WIND) {
    if (p.preset == DSL_WIND_FLUTE) {
      sk_flute_t fl;
      sk_flute_init(&fl, rate);
      sk_flute_set_freq(&fl, p.freq > 20.0f ? p.freq : 523.25f);
      if (p.damping > 0.0f) fl.bore_damping = sk_clampf(p.damping, 0.02f, 0.8f);
      if (p.noise >= 0.0f) fl.noise_gain = sk_clampf(p.noise, 0.0f, 0.5f);
      if (p.vibrato >= 0.0f) fl.vibrato_depth = sk_clampf(p.vibrato, 0.0f, 0.2f);
      if (p.vibrato_rate > 0.1f) fl.vibrato_inc = p.vibrato_rate / rate;
      sk_flute_set_pressure(&fl, p.pressure > 0.0f ? p.pressure : 0.70f);

      int release_start = total_samples - (int)(rate * 0.05f);
      if (release_start < 0) release_start = 0;
      for (int i = 0; i < total_samples; i++) {
        if (i == release_start) sk_flute_set_pressure(&fl, 0.0f);
        buf[i] = sk_flute_step(&fl);
      }
    } else {
      sk_brass_t br;
      sk_brass_init(&br, rate);
      sk_brass_set_freq(&br, p.freq > 20.0f ? p.freq : 233.08f);
      if (p.damping > 0.0f) br.bore_damping = sk_clampf(p.damping, 0.02f, 0.8f);
      if (p.brassiness >= 0.0f) br.brassiness = sk_clampf(p.brassiness, 0.0f, 1.0f);
      sk_brass_set_pressure(&br, p.pressure > 0.0f ? p.pressure : 0.75f);

      int release_start = total_samples - (int)(rate * 0.05f);
      if (release_start < 0) release_start = 0;
      for (int i = 0; i < total_samples; i++) {
        if (i == release_start) sk_brass_set_pressure(&br, 0.0f);
        buf[i] = sk_brass_step(&br);
      }
    }
  }

  /* Smooth 15ms cosine anti-click taper at the very end of the buffer */
  int taper_samples = (int)(rate * 0.015f);
  if (taper_samples > total_samples / 4) taper_samples = total_samples / 4;
  for (int i = 0; i < taper_samples; i++) {
    int idx = total_samples - taper_samples + i;
    float phase = (float)i / (float)taper_samples;
    float window = 0.5f * (1.0f + cosf((float)M_PI * phase));
    buf[idx] *= window;
  }

  /* Normalize audio to standard -0.1 dB level */
  normalize_preserve_zero(buf, total_samples);

  /* Loop points calculation for sustained instruments */
  int loop_start = 0;
  int loop_end = 0;
  int has_loop = 0;
  if (p.loop == 1) {
    has_loop = find_sustain_loop(buf, total_samples, p.freq, rate, &loop_start, &loop_end);
  }

  /* Populate skode data array */
  ands_data_resize(ctx->parse, total_samples);
  double *d = ands_data(ctx->parse);
  if (!d || ands_data_cap(ctx->parse) < total_samples) {
    free(buf);
    ctx->printf(ctx, "# render: failed to allocate data buffer\n");
    return 0;
  }
  for (int i = 0; i < total_samples; i++) {
    d[i] = (double)buf[i];
  }
  ands_data_len_set(ctx->parse, total_samples);
  free(buf);

  /* Format universal metadata string */
  sk_meta_info_t meta;
  sk_meta_init(&meta);
  meta.mode = SK_WAVE_MODE_SAMPLE;
  meta.rate = rate;
  meta.has_rate = 1;
  if (has_loop) {
    meta.has_loop = 1;
    meta.loop_start = loop_start;
    meta.loop_end = loop_end;
    meta.one_shot = 0;
    meta.has_one_shot = 1;
  } else {
    meta.one_shot = 1;
    meta.has_one_shot = 1;
  }
  if (p.has_pitch) {
    meta.has_root_note = 1;
    meta.root_note = p.midi_note;
  }

  char meta_str[512];
  sk_format_metadata(&meta, meta_str, sizeof(meta_str));
  ands_string_from_external(ctx->parse, meta_str, (int)strlen(meta_str));

  if (has_loop) {
    ctx->printf(ctx, "# phz: %s [%d samples, %.2fs, loop: %d..%d]\n",
                p.preset_name, total_samples, (double)total_samples / rate, loop_start, loop_end);
  } else {
    ctx->printf(ctx, "# phz: %s [%d samples, %.2fs, one-shot]\n",
                p.preset_name, total_samples, (double)total_samples / rate);
  }

  /* Install to wave slot if slot argument provided */
  int wave_slot = -1;
  if (argc > 0 && skode_double_to_int(arg[0], &wave_slot)) {
    if (skode_wave_valid(wave_slot)) {
      int one_shot = meta.one_shot;
      if (argc > 1) {
        int os;
        if (skode_double_to_int(arg[1], &os)) one_shot = os;
      }
      data_load(ctx, wave_slot, one_shot, rate, 0.0f);
      ctx->printf(ctx, "# phz: installed to w%d\n", wave_slot);
    }
  }

  return 0;
}

int skode_exec_data_to_wave(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!ctx || !ctx->parse) return -1;
  int wave_slot = EXT_SAMPLE_000;
  int one_shot = 1;                     /* d>w defaults to ONE-SHOT mode */
  float rate = (float)MAIN_SAMPLE_RATE;
  float offset = 0.0f;

  if (argc > 0 && !skode_double_to_int(arg[0], &wave_slot)) return 0;
  if (argc > 1) {
    if (arg[1] == 0.0 || arg[1] == 1.0) {
      one_shot = (int)arg[1];
    } else {
      rate = (float)arg[1];
    }
  }
  if (argc > 2) {
    if (arg[1] > 1.0) {
      skode_double_to_int(arg[2], &one_shot);
    } else {
      rate = (float)arg[2];
    }
  }
  if (argc > 3) offset = (float)arg[3];

  return data_load(ctx, wave_slot, one_shot, rate, offset);
}

void skode_register_words_dsp(skode_vocab_t *vocab) {
  (void)vocab;
}
