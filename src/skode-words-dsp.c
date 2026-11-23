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
  char family_str[32];
  char preset_str[32];
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

  /* Frequency notation: f440, F440, f:440, f=440 */
  if ((val[0] == 'f' || val[0] == 'F') && (val[1] == ':' || val[1] == '=' || (isdigit((unsigned char)val[1]) && isdigit((unsigned char)val[2])))) {
    const char *p = val + 1;
    while (*p == ' ' || *p == ':' || *p == '=') p++;
    float hz = (float)atof(p);
    if (hz > 0.0f) {
      if (out_midi_note) *out_midi_note = sk_ftom(hz);
      if (out_has_pitch) *out_has_pitch = 1;
      return hz;
    }
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
    if (oct >= -2 && oct <= 9) {
      float midi = (float)((oct + 1) * 12 + semi);
      if (out_midi_note) *out_midi_note = midi;
      if (out_has_pitch) *out_has_pitch = 1;
      return sk_mtof(midi);
    }
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

/* YIN Fundamental Pitch Estimator
 * Implements de Cheveigné & Kawahara (2002) YIN algorithm with Cumulative Mean
 * Normalized Difference Function (CMNDF) and parabolic interpolation.
 * Robustly estimates fundamental frequency from 35 Hz to 2400 Hz on arbitrary audio
 * (including live microphone recordings and untagged PCM samples), completely immune
 * to harmonic octave errors, DC bias, and amplitude decay.
 */
static float sk_detect_pitch_yin(const float *audio, int len, float sample_rate, float pitch_hint) {
  if (!audio || len < 512 || sample_rate < 1000.0f) return 0.0f;

  int win_size = 1024;
  if (win_size > len / 3) win_size = len / 3;
  if (win_size < 256) win_size = 256;

  int min_tau = (int)(sample_rate / 2400.0f); /* max 2400 Hz */
  int max_tau = (int)(sample_rate / 35.0f);   /* min 35 Hz */
  if (min_tau < 4) min_tau = 4;
  if (max_tau >= 1800) max_tau = 1800;

  int start_pos = (int)(sample_rate * 0.18f); /* skip 180ms attack */
  if (start_pos + win_size + max_tau >= len) start_pos = len / 4;
  if (start_pos + win_size + max_tau >= len) start_pos = 0;
  if (start_pos + win_size + max_tau >= len) {
    win_size = (len - start_pos) / 3;
    max_tau = win_size;
  }
  if (min_tau >= max_tau) return 0.0f;

  /* If pitch hint is provided and valid, narrow the search range to ±18% of expected period */
  if (pitch_hint >= 35.0f && pitch_hint <= 2400.0f) {
    int expected_tau = (int)(sample_rate / pitch_hint + 0.5f);
    int delta = (int)(expected_tau * 0.18f);
    if (delta < 4) delta = 4;
    int h_min = expected_tau - delta;
    int h_max = expected_tau + delta;
    if (h_min > min_tau && h_min < max_tau) min_tau = h_min;
    if (h_max < max_tau && h_max > min_tau) max_tau = h_max;
  }

  /* 1. Difference function d(tau) */
  float d[2048];
  float cmndf[2048];
  memset(d, 0, sizeof(d));
  memset(cmndf, 0, sizeof(cmndf));

  for (int tau = 0; tau <= max_tau; tau++) {
    float sum = 0.0f;
    for (int j = 0; j < win_size; j++) {
      float diff = audio[start_pos + j] - audio[start_pos + j + tau];
      sum += diff * diff;
    }
    d[tau] = sum;
  }

  /* 2. Cumulative Mean Normalized Difference Function cmndf(tau) */
  cmndf[0] = 1.0f;
  float running_sum = 0.0f;
  for (int tau = 1; tau <= max_tau; tau++) {
    running_sum += d[tau];
    cmndf[tau] = (running_sum > 1e-9f) ? (d[tau] * (float)tau / running_sum) : 1.0f;
  }

  /* 3. Absolute thresholding (theta = 0.20) on local minima */
  const float threshold = 0.20f;
  int tau_best = -1;
  for (int tau = min_tau + 1; tau < max_tau - 1; tau++) {
    if (cmndf[tau] < cmndf[tau - 1] && cmndf[tau] <= cmndf[tau + 1]) {
      if (cmndf[tau] < threshold) {
        tau_best = tau;
        break;
      }
    }
  }

  /* Fallback to global minimum of local valleys if none below threshold */
  if (tau_best < 0) {
    float min_val = 1e9f;
    for (int tau = min_tau + 1; tau < max_tau - 1; tau++) {
      if (cmndf[tau] < cmndf[tau - 1] && cmndf[tau] <= cmndf[tau + 1]) {
        if (cmndf[tau] < min_val) {
          min_val = cmndf[tau];
          tau_best = tau;
        }
      }
    }
  }

  if (tau_best <= 0) return 0.0f;

  /* 4. Parabolic interpolation around minimum for fractional sample accuracy */
  float frac_tau = (float)tau_best;
  if (tau_best > min_tau && tau_best < max_tau) {
    float y0 = cmndf[tau_best - 1];
    float y1 = cmndf[tau_best];
    float y2 = cmndf[tau_best + 1];
    float denom = 2.0f * (2.0f * y1 - y0 - y2);
    if (fabsf(denom) > 1e-6f) {
      frac_tau += (y2 - y0) / denom;
    }
  }

  if (frac_tau <= 1.0f) return 0.0f;
  return sample_rate / frac_tau;
}

static int find_sustain_loop_correlate(float *audio, int len, float freq, float sample_rate, int *loop_start, int *loop_end, float *best_corr_out, float *detected_freq_out) {
  if (!audio || len < 512 || sample_rate < 1000.0f) return 0;

  /* If pitch is unknown or <= 20Hz, detect fundamental period via YIN */
  if (freq < 20.0f) {
    freq = sk_detect_pitch_yin(audio, len, sample_rate, 0.0f);
    if (freq < 20.0f) freq = 220.0f;
  } else {
    /* Refine nominal frequency with YIN using pitch as hint */
    float refined = sk_detect_pitch_yin(audio, len, sample_rate, freq);
    if (refined >= 20.0f) freq = refined;
  }

  if (detected_freq_out) *detected_freq_out = freq;

  float period = sample_rate / freq;
  if (period < 2.0f || period > (float)(len / 3)) return 0;

  /* Skip initial attack transient (~18% of buffer or at least 150ms) */
  int search_start = (int)(sample_rate * 0.18f);
  if (search_start > len / 3) search_start = len / 4;
  if (search_start + (int)(period * 3.0f) >= len) return 0;

  /* Target loop duration ~220-350ms, containing integer k periods */
  float target_sec = 0.28f;
  int k = (int)(target_sec * freq + 0.5f);
  if (k < 2) k = 2;

  int max_end = len - (int)(sample_rate * 0.18f); /* leave release tail */
  if (max_end < search_start + (int)(period * 3.0f)) max_end = len - (int)(sample_rate * 0.04f);

  int best_s = -1, best_e = -1;
  float best_score = -1e9f;
  float best_corr = 0.0f;
  int corr_win = (int)period;
  if (corr_win < 32) corr_win = 32;
  if (corr_win > 256) corr_win = 256;

  int start_scan_end = search_start + (int)(period * 3.0f);
  if (start_scan_end >= len - 2) start_scan_end = len - 2;

  for (int s_idx = search_start; s_idx < start_scan_end; s_idx++) {
    if (audio[s_idx] <= 0.0f && audio[s_idx + 1] > 0.0f) {
      float s_slope = audio[s_idx + 1] - audio[s_idx];
      int cur_k = k;
      int nominal_end = s_idx + (int)(cur_k * period + 0.5f);
      while (nominal_end >= max_end && cur_k > 1) {
        cur_k--;
        nominal_end = s_idx + (int)(cur_k * period + 0.5f);
      }
      if (cur_k < 1 || nominal_end >= max_end) continue;

      int radius = (int)(period * 0.6f);
      if (radius < 2) radius = 2;
      int scan_min = nominal_end - radius;
      int scan_max = nominal_end + radius;
      if (scan_min <= s_idx + (int)period) scan_min = s_idx + (int)period + 1;
      if (scan_max >= len - corr_win - 2) scan_max = len - corr_win - 2;

      for (int e_idx = scan_min; e_idx <= scan_max; e_idx++) {
        if (audio[e_idx] <= 0.0f && audio[e_idx + 1] > 0.0f) {
          float e_slope = audio[e_idx + 1] - audio[e_idx];

          float sum_xy = 0.0f, sum_xx = 0.0f, sum_yy = 0.0f;
          for (int w = 0; w < corr_win; w++) {
            float x = audio[s_idx + w];
            float y = audio[e_idx + w];
            sum_xy += x * y;
            sum_xx += x * x;
            sum_yy += y * y;
          }
          float denom = sqrtf(sum_xx * sum_yy) + 1e-9f;
          float corr = sum_xy / denom;

          float slope_diff = fabsf(e_slope - s_slope) / (fabsf(s_slope) + fabsf(e_slope) + 1e-6f);
          float val_diff = fabsf(audio[s_idx] - audio[e_idx]);

          float score = corr * 10.0f - slope_diff * 3.0f - val_diff * 5.0f;
          if (score > best_score) {
            best_score = score;
            best_corr = corr;
            best_s = s_idx;
            best_e = e_idx;
          }
        }
      }
    }
  }

  if (best_e > best_s + 8 && best_s >= 0) {
    /* Apply a smooth 64-sample equal-power micro-crossfade at the loop boundary
     * to eliminate any micro-transient or click upon wraparound */
    int xfade_len = 64;
    if (xfade_len > (best_e - best_s) / 4) xfade_len = (best_e - best_s) / 4;
    if (xfade_len > 4 && best_s >= xfade_len) {
      for (int i = 0; i < xfade_len; i++) {
        float t = (float)i / (float)xfade_len;
        float g_out = cosf(t * 1.5707963f);
        float g_in = sinf(t * 1.5707963f);
        int tail_idx = best_e - xfade_len + i;
        int pre_idx = best_s - xfade_len + i;
        if (tail_idx < len && pre_idx >= 0) {
          audio[tail_idx] = audio[tail_idx] * g_out + audio[pre_idx] * g_in;
        }
      }
    }

    if (loop_start) *loop_start = best_s;
    if (loop_end) *loop_end = best_e;
    if (best_corr_out) *best_corr_out = best_corr;
    return 1;
  }
  return 0;
}

static int find_sustain_loop(float *audio, int len, float freq, float sample_rate, int *loop_start, int *loop_end) {
  return find_sustain_loop_correlate(audio, len, freq, sample_rate, loop_start, loop_end, NULL, NULL);
}

int skode_exec_wave_findloop(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!ctx || !ctx->parse) return -1;

  int wave_slot = -1;
  float pitch_hint = 0.0f;

  if (argc > 0 && skode_double_to_int(arg[0], &wave_slot)) {
    /* Slot explicitly passed */
  } else if (ctx->voice >= 0 && ctx->voice < synth_config.voice_max) {
    wave_slot = sv.wave_table_index[ctx->voice];
  } else {
    wave_slot = 0;
  }

  if (!skode_wave_valid(wave_slot) || !sw.data[wave_slot] || sw.size[wave_slot] < 512) {
    ctx->printf(ctx, "# findloop: invalid or empty wave slot %d\n", wave_slot);
    return -1;
  }

  if (argc > 1) {
    float a1 = (float)arg[1];
    if (a1 >= 20.0f) {
      pitch_hint = a1; /* explicit frequency in Hz */
    } else if (a1 > 0.0f) {
      pitch_hint = sk_mtof(a1); /* explicit MIDI note number */
    }
  } else if (sw.midi_note[wave_slot] > 0.0f) {
    pitch_hint = sk_mtof(sw.midi_note[wave_slot]);
  } else if (ctx->parse) {
    /* Try parsing pitch/root from metadata string in parse register */
    const char *str = ands_string(ctx->parse);
    if (str && str[0] != '\0') {
      sk_meta_info_t meta;
      if (sk_parse_metadata(str, &meta) && meta.has_root_note) {
        pitch_hint = sk_mtof(meta.root_note);
        sw.midi_note[wave_slot] = meta.root_note;
      }
    }
  }

  float *audio = sw.data[wave_slot];
  int len = sw.size[wave_slot];
  float rate = sw.rate[wave_slot] > 0.0f ? sw.rate[wave_slot] : (float)MAIN_SAMPLE_RATE;

  int loop_start = 0, loop_end = 0;
  float best_corr = 0.0f;
  float detected_freq = 0.0f;
  int ok = find_sustain_loop_correlate(audio, len, pitch_hint, rate, &loop_start, &loop_end, &best_corr, &detected_freq);

  if (!ok) {
    ctx->printf(ctx, "# findloop: no stable sustain loop detected for w%d (%d samples)\n", wave_slot, len);
    return 0;
  }

  /* If pitch was not previously known, store detected pitch in wave slot */
  if (sw.midi_note[wave_slot] <= 0.0f && detected_freq > 20.0f) {
    sw.midi_note[wave_slot] = sk_ftom(detected_freq);
  }

  /* Apply loop points directly to wavetable slot.
   * NOTE: PCM samples MUST retain one_shot = 1!
   * In sk3, one_shot=0 specifies single-cycle oscillator mode (1 period = table_size),
   * which causes a multi-thousand sample buffer to play at hyper-speed (high-pitched whine).
   * Sample looping is governed strictly by sw.loop_enabled. */
  wave_loop_points_set(wave_slot, loop_start, loop_end);
  sw.loop_enabled[wave_slot] = 1;
  sw.one_shot[wave_slot] = 1;

  float root_note = sw.midi_note[wave_slot] > 0.0f ? sw.midi_note[wave_slot] : (detected_freq > 20.0f ? sk_ftom(detected_freq) : 69.0f);

  /* Format standard metadata string into ctx->parse */
  char meta_buf[256];
  snprintf(meta_buf, sizeof(meta_buf), "[mode:sample loop:%d,%d one_shot:1 rate:%.0f root:%.1f]",
           loop_start, loop_end, rate, root_note);
  ands_string_from_external(ctx->parse, meta_buf, (int)strlen(meta_buf));

  ctx->printf(ctx, "# findloop: w%d (pitch=%.1f Hz, MIDI %.1f) loop points set to %d..%d (len=%d samples, %.2f ms, corr=%.3f)\n",
              wave_slot, detected_freq > 0.0f ? detected_freq : (root_note > 0.0f ? sk_mtof(root_note) : 0.0f),
              root_note, loop_start, loop_end, loop_end - loop_start,
              (double)(loop_end - loop_start) * 1000.0 / rate, best_corr);
  return 0;
}

/* Extensible Generator Registry Architecture */
typedef int (*sk_gen_fn)(skode_t *ctx, const char *domain, const char *instrument, const dsl_params_t *params, float *out_buf, int total_samples, float rate);

typedef struct {
  char domain[32];
  sk_gen_fn handler;
  char description[64];
} sk_gen_entry_t;

#define SK_GEN_REGISTRY_MAX 32
static sk_gen_entry_t s_gen_registry[SK_GEN_REGISTRY_MAX];
static int s_gen_registry_count = 0;

int sk_gen_register(const char *domain, sk_gen_fn handler, const char *description) {
  if (!domain || !handler) return -1;
  for (int i = 0; i < s_gen_registry_count; i++) {
    if (strcasecmp(s_gen_registry[i].domain, domain) == 0) {
      s_gen_registry[i].handler = handler;
      if (description) strncpy(s_gen_registry[i].description, description, sizeof(s_gen_registry[i].description) - 1);
      return 0;
    }
  }
  if (s_gen_registry_count >= SK_GEN_REGISTRY_MAX) return -1;
  strncpy(s_gen_registry[s_gen_registry_count].domain, domain, sizeof(s_gen_registry[s_gen_registry_count].domain) - 1);
  s_gen_registry[s_gen_registry_count].handler = handler;
  if (description) strncpy(s_gen_registry[s_gen_registry_count].description, description, sizeof(s_gen_registry[s_gen_registry_count].description) - 1);
  s_gen_registry_count++;
  return 0;
}

sk_gen_fn sk_gen_lookup(const char *domain) {
  if (!domain) return NULL;
  for (int i = 0; i < s_gen_registry_count; i++) {
    if (strcasecmp(s_gen_registry[i].domain, domain) == 0) {
      return s_gen_registry[i].handler;
    }
  }
  return NULL;
}

static int sk_gen_drum_render(skode_t *ctx, const char *domain, const char *instrument, const dsl_params_t *p, float *buf, int total_samples, float rate) {
  (void)ctx; (void)domain;
  if (p->preset == DSL_DRUM_SNARE) {
    sk_drum_snare_t sn;
    sk_drum_snare_init(&sn, rate);
    if (p->f1 > 20.0f) sn.f1 = p->f1;
    if (p->f2 > 20.0f) sn.f2 = p->f2;
    if (p->snap >= 0.0f) sn.snappy_mix = sk_clampf(p->snap, 0.0f, 1.0f);
    if (p->decay > 0.01f) sn.body_decay_coeff = expf(-1.0f / (p->decay * rate));
    if (p->tail > 0.01f) sn.noise_decay_coeff = expf(-1.0f / (p->tail * rate));
    if (p->freq > 100.0f) sk_biquad_init(&sn.noise_filter, SK_BIQUAD_BANDPASS, p->freq, 1.5f, rate);
    sk_drum_snare_trigger(&sn, p->velocity > 0.0f ? p->velocity : 1.0f);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_snare_step(&sn);
  } else if (p->preset == DSL_DRUM_HAT) {
    sk_drum_hat_t h;
    sk_drum_hat_init(&h, rate);
    sk_drum_hat_trigger(&h, p->velocity > 0.0f ? p->velocity : 1.0f, p->hat_open);
    if (p->decay > 0.005f) h.amp_decay_coeff = expf(-1.0f / (p->decay * rate));
    for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_hat_step(&h);
  } else if (p->preset == DSL_DRUM_CLAP) {
    sk_drum_clap_t cl;
    sk_drum_clap_init(&cl, rate);
    if (p->spread > 1.0f) cl.burst_spacing = (int)(p->spread * 0.001f * rate);
    if (p->tail > 0.01f) cl.tail_decay_coeff = expf(-1.0f / (p->tail * rate));
    if (p->freq > 100.0f) sk_biquad_init(&cl.filter, SK_BIQUAD_BANDPASS, p->freq, 1.2f, rate);
    sk_drum_clap_trigger(&cl, p->velocity > 0.0f ? p->velocity : 1.0f);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_clap_step(&cl);
  } else if (p->preset == DSL_DRUM_TOM) {
    sk_drum_tom_t tm;
    sk_drum_tom_init(&tm, rate, p->freq > 20.0f ? p->freq : 100.0f);
    if (p->decay > 0.01f) tm.amp_decay_coeff = expf(-1.0f / (p->decay * rate));
    if (p->pdecay > 0.001f) tm.pitch_decay_coeff = expf(-1.0f / (p->pdecay * rate));
    sk_drum_tom_trigger(&tm, p->velocity > 0.0f ? p->velocity : 1.0f);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_tom_step(&tm);
  } else if (p->preset == DSL_DRUM_COWBELL) {
    sk_drum_cowbell_t cb;
    sk_drum_cowbell_init(&cb, rate);
    if (p->f1 > 20.0f) cb.inc1 = p->f1 / rate;
    if (p->f2 > 20.0f) cb.inc2 = p->f2 / rate;
    if (p->decay > 0.01f) cb.amp_decay_coeff = expf(-1.0f / (p->decay * rate));
    sk_drum_cowbell_trigger(&cb, p->velocity > 0.0f ? p->velocity : 1.0f);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_cowbell_step(&cb);
  } else if (p->preset == DSL_DRUM_MEMBRANE || (instrument && (strstr(instrument, "membrane") || strstr(instrument, "acoustic") || strstr(instrument, "bassdrum")))) {
    /* Physical 2D Bessel circular membrane with tension drop, strike contact, and air cavity */
    sk_drum_membrane_t mem;
    float strike_pos = (p->brightness > 0.0f) ? p->brightness : 0.25f;
    float tension_drop = (p->drop > 0.0f) ? p->drop : 0.45f;
    float base_f = (p->freq > 20.0f) ? p->freq : 80.0f;
    float decay = (p->decay > 0.05f) ? p->decay : 0.80f;
    sk_drum_membrane_init(&mem, rate, base_f, decay, tension_drop, strike_pos);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_membrane_step(&mem);
  } else {
    /* Kick drum */
    sk_drum_kick_t k;
    sk_drum_kick_init(&k, rate);
    if (p->freq > 20.0f) k.start_freq = p->freq;
    if (p->end_freq > 10.0f) k.end_freq = p->end_freq;
    if (p->decay > 0.01f) k.amp_decay_coeff = expf(-1.0f / (p->decay * rate));
    if (p->pdecay > 0.001f) k.pitch_decay_coeff = expf(-1.0f / (p->pdecay * rate));
    if (p->click >= 0.0f) k.click_level = sk_clampf(p->click, 0.0f, 1.0f);
    if (p->drive > 0.0f) k.drive = sk_clampf(p->drive, 0.1f, 10.0f);
    sk_drum_kick_trigger(&k, p->velocity > 0.0f ? p->velocity : 1.0f);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_kick_step(&k);
  }
  return 0;
}

static int sk_gen_bell_render(skode_t *ctx, const char *domain, const char *instrument, const dsl_params_t *p, float *buf, int total_samples, float rate) {
  (void)ctx; (void)domain; (void)instrument;
  sk_bell_type_t btype = SK_BELL_CHURCH;
  if (p->preset == DSL_BELL_TUBULAR) btype = SK_BELL_TUBULAR;
  else if (p->preset == DSL_BELL_VIBE) btype = SK_BELL_VIBE;
  else if (p->preset == DSL_BELL_TIBETAN) btype = SK_BELL_TIBETAN;
  else if (p->preset == DSL_BELL_AGOGO) btype = SK_BELL_AGOGO;

  sk_drum_bell_t b;
  sk_drum_bell_init(&b, rate, btype, p->freq > 20.0f ? p->freq : 440.0f);
  if (p->decay > 0.05f) {
    sk_drum_bell_set_preset(&b, btype, p->freq > 20.0f ? p->freq : 440.0f, p->decay);
  }
  if (p->mallet >= 0.0f) {
    float cutoff = p->freq > 100.0f ? p->freq * 4.0f : 3500.0f;
    if (cutoff > rate * 0.45f) cutoff = rate * 0.45f;
    sk_biquad_init(&b.strike_filter, SK_BIQUAD_BANDPASS, cutoff, 2.0f, rate);
  }
  sk_drum_bell_trigger(&b, p->velocity > 0.0f ? p->velocity : 1.0f);
  for (int i = 0; i < total_samples; i++) buf[i] = sk_drum_bell_step(&b);
  return 0;
}

static int sk_gen_string_render(skode_t *ctx, const char *domain, const char *instrument, const dsl_params_t *p, float *buf, int total_samples, float rate) {
  (void)ctx; (void)domain; (void)instrument;
  sk_karplus_t ks;
  sk_karplus_init(&ks, rate);
  sk_rng_t rng;
  sk_rng_init(&rng, 54321ULL);
  if (p->damping > 0.0f) ks.damping = sk_clampf(p->damping, 0.01f, 0.99f);
  if (p->feedback > 0.0f) ks.feedback = sk_clampf(p->feedback, 0.85f, 0.999f);
  sk_karplus_pluck(&ks, p->freq > 20.0f ? p->freq : 440.0f,
                   p->velocity > 0.0f ? p->velocity : 1.0f,
                   p->pick > 0.0f ? p->pick : 0.25f,
                   p->brightness >= 0.0f ? p->brightness : 0.6f,
                   &rng);
  for (int i = 0; i < total_samples; i++) buf[i] = sk_karplus_step(&ks);

  /* Acoustic instrument wooden soundbox body coupling */
  if (p->preset == DSL_STRING_BASS) {
    sk_soundbox_t box;
    sk_soundbox_init_upright_bass(&box, rate);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_soundbox_step(&box, buf[i]);
  } else if (p->preset == DSL_STRING_PIZZ) {
    sk_soundbox_t box;
    sk_soundbox_init_cello(&box, rate);
    for (int i = 0; i < total_samples; i++) buf[i] = sk_soundbox_step(&box, buf[i]);
  }
  return 0;
}

static int sk_gen_wind_render(skode_t *ctx, const char *domain, const char *instrument, const dsl_params_t *p, float *buf, int total_samples, float rate) {
  (void)ctx; (void)domain; (void)instrument;
  sk_flute_t fl;
  sk_flute_init(&fl, rate);
  sk_flute_set_freq(&fl, p->freq > 20.0f ? p->freq : 523.25f);
  if (p->damping > 0.0f) fl.bore_damping = sk_clampf(p->damping, 0.02f, 0.8f);
  if (p->noise >= 0.0f) fl.noise_gain = sk_clampf(p->noise, 0.0f, 0.5f);
  sk_flute_set_pressure(&fl, p->pressure > 0.0f ? p->pressure : 0.75f);

  int release_start = total_samples - (int)(rate * 0.25f);
  if (release_start < (int)(rate * 0.4f)) release_start = (int)(rate * 0.4f);
  for (int i = 0; i < total_samples; i++) {
    if (i == release_start) sk_flute_set_pressure(&fl, 0.0f);
    buf[i] = sk_flute_step(&fl);
  }
  return 0;
}

static int sk_gen_horn_render(skode_t *ctx, const char *domain, const char *instrument, const dsl_params_t *p, float *buf, int total_samples, float rate) {
  (void)ctx; (void)domain; (void)instrument;
  sk_brass_t br;
  sk_brass_init(&br, rate);
  sk_brass_set_freq(&br, p->freq > 20.0f ? p->freq : 233.08f);
  if (p->damping > 0.0f) br.bore_damping = sk_clampf(p->damping, 0.02f, 0.8f);
  if (p->brassiness >= 0.0f) br.brassiness = sk_clampf(p->brassiness, 0.0f, 1.0f);
  sk_brass_set_pressure(&br, p->pressure > 0.0f ? p->pressure : 0.75f);

  int release_start = total_samples - (int)(rate * 0.25f);
  if (release_start < (int)(rate * 0.4f)) release_start = (int)(rate * 0.4f);
  for (int i = 0; i < total_samples; i++) {
    if (i == release_start) sk_brass_set_pressure(&br, 0.0f);
    buf[i] = sk_brass_step(&br);
  }
  return 0;
}

static void sk_gen_init_registry(void) {
  static int inited = 0;
  if (inited) return;
  inited = 1;
  sk_gen_register("drum", sk_gen_drum_render, "Percussion & drum physical models");
  sk_gen_register("perc", sk_gen_drum_render, "Percussion & drum physical models");
  sk_gen_register("bell", sk_gen_bell_render, "Modal bells and idiophones");
  sk_gen_register("metal", sk_gen_bell_render, "Metallic bells and resonant plates");
  sk_gen_register("string", sk_gen_string_render, "Plucked strings with wooden body cavity");
  sk_gen_register("pluck", sk_gen_string_render, "Plucked strings with wooden body cavity");
  sk_gen_register("wind", sk_gen_wind_render, "Blown acoustic pipes and flutes");
  sk_gen_register("flute", sk_gen_wind_render, "Blown acoustic pipes and flutes");
  sk_gen_register("horn", sk_gen_horn_render, "Lip-reed brass instruments");
  sk_gen_register("brass", sk_gen_horn_render, "Lip-reed brass instruments");
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
  p.loop = 0; /* default to natural one-shot for all physical models */

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
      strncpy(p.family_str, fam_str, sizeof(p.family_str) - 1);
      strncpy(p.preset_str, pre_str, sizeof(p.preset_str) - 1);

      /* Match family and preset */
      if (strcmp(fam_str, "drum") == 0 || strcmp(fam_str, "perc") == 0 ||
          strcmp(pre_str, "kick") == 0 || strcmp(pre_str, "snare") == 0 ||
          strcmp(pre_str, "hat") == 0 || strcmp(pre_str, "hihat") == 0 ||
          strcmp(pre_str, "clap") == 0 || strcmp(pre_str, "handclap") == 0 ||
          strcmp(pre_str, "tom") == 0 || strcmp(pre_str, "cowbell") == 0 ||
          strcmp(pre_str, "membrane") == 0 || strcmp(pre_str, "kdrum") == 0 ||
          strcmp(pre_str, "bassdrum") == 0) {
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
        } else if (strcmp(pre_str, "membrane") == 0 || strcmp(pre_str, "kdrum") == 0 || strcmp(pre_str, "bassdrum") == 0) {
          p.preset = DSL_DRUM_MEMBRANE;
          strcpy(p.preset_name, "drum:membrane");
          p.freq = (strcmp(pre_str, "bassdrum") == 0) ? 55.0f : 80.0f;
          p.drop = 0.45f;
          p.decay = 0.80f;
          p.brightness = 0.25f;
          p.has_pitch = 1;
          p.midi_note = sk_ftom(p.freq);
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
        p.duration = 1.8f;
        p.loop = 0; /* Natural acoustic one-shot by default */
        if (strcmp(pre_str, "flute") == 0 || strcmp(pre_str, "pipe") == 0 || strcmp(pre_str, "recorder") == 0 ||
            strcmp(fam_str, "flute") == 0) {
          p.preset = DSL_WIND_FLUTE;
          strcpy(p.preset_name, "wind:flute");
          p.freq = 523.25f; p.midi_note = 72.0f; p.pressure = 0.75f; p.noise = 0.05f; p.damping = 0.25f;
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

  /* Synthesis execution via extensible registry */
  sk_gen_init_registry();
  sk_gen_fn gen_handler = sk_gen_lookup(p.family_str);
  if (!gen_handler) {
    if (p.family == DSL_FAM_DRUM) gen_handler = sk_gen_drum_render;
    else if (p.family == DSL_FAM_BELL) gen_handler = sk_gen_bell_render;
    else if (p.family == DSL_FAM_STRING) gen_handler = sk_gen_string_render;
    else if (p.family == DSL_FAM_WIND) {
      if (p.preset == DSL_WIND_FLUTE) gen_handler = sk_gen_wind_render;
      else gen_handler = sk_gen_horn_render;
    }
  }
  if (gen_handler) {
    gen_handler(ctx, p.family_str, p.preset_str, &p, buf, total_samples, rate);
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
    ctx->printf(ctx, "# gen: %s [%d samples, %.2fs, loop: %d..%d]\n",
                p.preset_name, total_samples, (double)total_samples / rate, loop_start, loop_end);
  } else {
    ctx->printf(ctx, "# gen: %s [%d samples, %.2fs, one-shot]\n",
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
      ctx->printf(ctx, "# gen: installed to w%d\n", wave_slot);
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
