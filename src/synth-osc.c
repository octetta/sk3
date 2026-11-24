#include "synth.h"
#include "synth-internal.h"
#include "util.h"
#include "sk_dsp.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "control-events.h"

void audio_rng_init(uint64_t *rng, uint64_t seed) {
  sk_rng_init((sk_rng_t *)rng, seed);
}

// Generate next random number (full 64-bit range)
uint64_t audio_rng_next(uint64_t *rng) {
  return sk_rng_next((sk_rng_t *)rng);
}

// Generate random float in range [-1.0, 1.0] for audio
float audio_rng_float(uint64_t *rng) {
  return sk_rng_float((sk_rng_t *)rng);
}

float audio_rng_raw_float(uint64_t raw) {
  uint32_t val = (uint32_t)(raw >> 32);
  return (float)((int32_t)val) / 2147483648.0f;
}

float osc_get_phase_inc(skred_engine_t *engine, int v, float f) {
  float g = f;
  if (sv.freq_bend) {
    float semitones = sv.freq_bend[v] * sv.freq_bend_range[v] + sv.freq_bend_offset[v];
    if (semitones != 0.0f) {
      g *= powf(2.0f, semitones / 12.0f);
    }
  }
  if (sv.one_shot[v]) {
    if (sv.offset_hz[v] > 0.0f) {
      g /= sv.offset_hz[v];
      return (g * (float)sv.table_size[v]) / (float)MAIN_SAMPLE_RATE;
    }
    float rate = sv.table_rate[v] > 0.0f ? sv.table_rate[v] : (float)MAIN_SAMPLE_RATE;
    return (g / 440.0f) * (rate / (float)MAIN_SAMPLE_RATE);
  }
  return (g * (float)sv.table_size[v]) / (float)MAIN_SAMPLE_RATE;
}

static const float s_mip_freq_thresholds[WAVETABLE_MIP_LEVELS - 1] = {
    55.0f,    /* Level 0: <= 55 Hz (A1) -> 256 harmonics */
    110.0f,   /* Level 1: <= 110 Hz (A2) -> 128 harmonics */
    220.0f,   /* Level 2: <= 220 Hz (A3) -> 64 harmonics */
    440.0f,   /* Level 3: <= 440 Hz (A4) -> 32 harmonics */
    880.0f,   /* Level 4: <= 880 Hz (A5) -> 16 harmonics */
    1760.0f,  /* Level 5: <= 1760 Hz (A6) -> 8 harmonics */
    3520.0f   /* Level 6: <= 3520 Hz (A7) -> 4 harmonics */
              /* Level 7: > 3520 Hz -> 1 harmonic (fundamental) */
};

void osc_update_wavetable_mip(int voice) {
  if (voice_invalid(voice)) return;
  int w = sv.wave_table_index[voice];
  if (w < 0 || w >= synth_config.wave_table_max) return;

  if (!sw.mip_levels || sw.mip_levels[w] <= 0 || sv.one_shot[voice] || sw.one_shot[w]) {
    sv.mip_level[voice] = -1;
    sv.table[voice] = sw.data[w];
    return;
  }

  float freq = sv.freq[voice];
  if (sv.table_size[voice] > 0 && fabsf(sv.phase_inc[voice]) > 0.0f) {
    freq = (fabsf(sv.phase_inc[voice]) * (float)MAIN_SAMPLE_RATE) / (float)sv.table_size[voice];
  }
  if (freq < 0.0f) freq = 0.0f;

  int cur_level = sv.mip_level[voice];
  int level;
  if (cur_level < 0 || cur_level >= WAVETABLE_MIP_LEVELS) {
    level = 0;
    while (level < WAVETABLE_MIP_LEVELS - 1 && freq > s_mip_freq_thresholds[level]) {
      level++;
    }
  } else {
    level = cur_level;
    while (level < WAVETABLE_MIP_LEVELS - 1 && freq > s_mip_freq_thresholds[level] * 1.05f) {
      level++;
    }
    while (level > 0 && freq < s_mip_freq_thresholds[level - 1] * 0.95f) {
      level--;
    }
  }

  sv.mip_level[voice] = level;
  if (sw.mip_data[level] && sw.mip_data[level][w]) {
    sv.table[voice] = sw.mip_data[level][w];
  } else {
    sv.table[voice] = sw.data[w];
  }
}

void osc_set_freq(skred_engine_t *engine, int v, float f) {
  sv.phase_inc[v] = osc_get_phase_inc(engine, v, f);
  osc_update_wavetable_mip(v);
}

// Fast power approximation using bit manipulation (delegates to sk_dsp)
static inline float fast_pow(float a, float b) {
    return sk_fast_pow(a, b);
}

float cz_phasor(int n, float p, float d, float active_start, float active_end) {
    return sk_cz_phasor(n, p, d, active_start, active_end);
}

int osc_loop_crossings(double distance, double loop_length) {
    double crossings = distance / loop_length;
    return crossings >= (double)(INT_MAX - 1) ?
        INT_MAX : (int)crossings + 1;
}

void osc_reclassify(skred_engine_t *engine, int voice) {
    if (voice_invalid(voice)) return;
    sv.playback_class[voice] =
        sv.table[voice] != NULL &&
        sv.table_size[voice] > 0 &&
        !sv.one_shot[voice] &&
        sv.direction[voice] == 0 &&
        !sv.loop_active[voice] &&
        sv.wave_range_start[voice] == 0 &&
        sv.wave_range_end[voice] == sv.table_size[voice]
        ? OSC_PLAYBACK_CYCLE_SIMPLE
        : OSC_PLAYBACK_GENERAL;
}

/*
 * Hard sync: when voice `master` wraps its oscillator, every voice listed in
 * its link_sync_0..5 slots has its phase put back where a fresh note-on would
 * start it (same rules as osc_trigger, minus the envelope/loop bookkeeping).
 * The voice mix loop is single-threaded, so writing the slave's phase here is
 * safe.  A slave rendered earlier in the same sample than its master simply
 * picks the reset up one sample later.
 */
static inline double osc_sync_start_phase(int v) {
    if (sv.one_shot[v]) {
        return sv.direction[v] == 1 ? (double)sv.wave_range_end_f[v] - 1e-6
                                    : (double)sv.wave_range_start_f[v];
    }
    if (sv.direction[v] == 1)
        return sv.loop_active[v] ? (double)sv.loop_end[v] - 1e-6
                                 : (double)sv.wave_range_end_f[v] - 1e-6;
    return sv.loop_active[v] ? (double)sv.loop_start[v]
                             : (double)sv.wave_range_start_f[v];
}

static inline float osc_lookup_value(int v, double phase) {
    if (!sv.table[v] || sv.table_size[v] <= 0) return 0.0f;
    const bool one_shot = sv.one_shot[v];
    const float wave_range_start = sv.wave_range_start_f[v];
    const float wave_range_end = sv.wave_range_end_f[v];
    const double span = (double)wave_range_end - (double)wave_range_start;
    double p = phase;
    if (span > 0.0) {
        if (p < (double)wave_range_start) {
            p = (double)wave_range_end - fmod((double)wave_range_start - p, span);
        } else if (p >= (double)wave_range_end) {
            p = (double)wave_range_start + fmod(p - (double)wave_range_start, span);
        }
    }
    int idx = (int)p;
    if (idx >= (int)wave_range_end) idx = (int)wave_range_end - 1;
    if (idx < (int)wave_range_start) idx = (int)wave_range_start;

    if (sv.interpolate[v]) {
        float frac = (float)(p - (double)idx);
        int next_idx = idx + 1;
        if (next_idx >= (int)wave_range_end)
            next_idx = one_shot ? (int)wave_range_end - 1 : (int)wave_range_start;
        float s1 = sv.table[v][idx];
        float s2 = sv.table[v][next_idx];
        return s1 + frac * (s2 - s1);
    }
    return sv.table[v][idx];
}

void osc_hard_sync_fire(int master, double d) {
    if (d < 0.0) d = 0.0;
    else if (d >= 1.0) d = 1.0 - 1e-6;

    for (int i = 0; i < SYNTH_VOICE_LINKS_MAX; i++) {
        if (sv.link_sync[i][master] < 0.0f) continue;
        int v = (int)sv.link_sync[i][master];
        if (v == master || voice_invalid(v) || sv.finished[v] || !sv.table[v]) continue;

        double start = osc_sync_start_phase(v);
        double sinc = (double)sv.phase_inc[v];

        /* Sub-sample phase before reset at the instant of wrap (elapsed time 1 - d): */
        double phase_prior = sv.phase[v] + (1.0 - d) * sinc;
        float y_prior = osc_lookup_value(v, phase_prior);
        float y_start = osc_lookup_value(v, start);
        float delta = y_start - y_prior;

        /* PolyBLEP residual: smooths step discontinuity */
        float blep = (float)(delta * (d - 0.5 * d * d - 0.5));

        /* Sub-sample phase after reset advanced by fraction d to sample end: */
        sv.phase[v] = start + d * sinc;
        sv.sync_blep[v] += blep;
        sv.pingpong_reverse[v] = sv.direction[v] == 1;
    }
}

/* drop self-links so a voice can never sync itself */
void osc_hard_sync_sanitize(int voice) {
    if (voice_invalid(voice)) return;
    for (int i = 0; i < SYNTH_VOICE_LINKS_MAX; i++) {
        if ((int)sv.link_sync[i][voice] == voice) sv.link_sync[i][voice] = -1.0f;
    }
}

static inline double osc_cycle_phase_next(int voice, float phase_inc) {
    const double end = (double)sv.table_size[voice];
    double phase = sv.phase[voice] + (double)phase_inc;

    if (phase >= end) {
        phase -= end;
        if (phase >= end) phase = fmod(phase, end);
        double d = (phase_inc > 0.0f) ? (phase / (double)phase_inc) : 0.0;
        osc_hard_sync_fire(voice, d);
    } else if (phase < 0.0) {
        phase += end;
        if (phase < 0.0) {
            phase = fmod(phase, end);
            if (phase < 0.0) phase += end;
        }
        double d = (phase_inc < 0.0f) ? ((end - phase) / (double)(-phase_inc)) : 0.0;
        osc_hard_sync_fire(voice, d);
    }
    return phase;
}

static inline float osc_sample_at_phase(skred_engine_t *engine, int voice, double phase, float phase_offset,
    uint64_t current_sample, int table_size, float wave_range_start,
    float wave_range_end, bool one_shot, float active_start, float active_end) {
    double final_phase;
    (void)current_sample;

    /* @stage osc_cz_phase_distort
     * @title            Casio CZ Cosine Phase Distortion
     * @scope            voice
     * @rate             sample
     * @phase            osc_eval
     * @reads            [sv.cz_mode[voice], sv.cz_distortion[voice], sv.cz_mod_osc[voice], sv.cz_mod_depth[voice], sv.cz_envelope[voice], sv.cz_env_depth[voice], sv.sample]
     * @writes           []
     * @opcodes          [c, C, ct, cd]
     * @consumes         [phase, current_sample, active_start, active_end]
     * @produces         [final_phase]
     * @depends          []
     * @condition        sv.cz_mode[voice] != 0
     */
    if (sv.cz_mode[voice]) {
      float amount = sv.cz_distortion[voice];
      int dv = sv.cz_mod_osc[voice];
      if (dv >= 0)
        amount += sv.sample[dv] * sv.cz_mod_depth[voice];
      if (sv.cz_envelope[voice].is_active)
        amount += envelope_step_e(&sv.cz_envelope[voice], current_sample) *
          sv.cz_env_depth[voice];
      amount = fmaxf(-1.0f, fminf(1.0f, amount));
      final_phase = cz_phasor(sv.cz_mode[voice], (float)phase, amount,
        active_start, active_end);
    } else {
      final_phase = phase;
    }
    /* @end osc_cz_phase_distort */

    /* @stage osc_phase_mod_offset
     * @title            Frequency/Phase Modulation Offset Wrapping
     * @scope            voice
     * @rate             sample
     * @phase            osc_eval
     * @reads            []
     * @writes           []
     * @opcodes          [F, FF]
     * @consumes         [final_phase, phase_offset, wave_range_start, wave_range_end]
     * @produces         [final_phase]
     * @depends          [osc_cz_phase_distort]
     * @condition        phase_offset != 0.0f
     */
    /*
     * FF2 phase modulation changes only the wavetable lookup position. The
     * persistent oscillator phase continues at phase_inc, so bipolar
     * modulation and feedback do not pull the operator off pitch.
     * phase_offset is expressed in radians (the conventional FM index unit).
     */
    if (phase_offset != 0.0f) {
      const double span = (double)wave_range_end - (double)wave_range_start;
      double offset = (double)phase_offset * span / (double)FM_TWO_PI;
      final_phase = (double)wave_range_start +
        fmod(final_phase - (double)wave_range_start + offset, span);
      if (final_phase < (double)wave_range_start) final_phase += span;
    }
    /* @end osc_phase_mod_offset */

    /* @stage osc_polyblep_core
     * @title            PolyBLEP Band-Limited Procedural Oscillator
     * @scope            voice
     * @rate             sample
     * @phase            osc_eval
     * @reads            [sv.source_type[voice], sv.wave_table_index[voice], sv.phase_inc[voice], sv.pulse_width[voice], sv.use_pwm_envelope[voice], sv.pwm_envelope[voice], sv.pwm_env_depth[voice], sv.pwm_mod_osc[voice], sv.pwm_mod_depth[voice], sv.sample, sv.sample_r]
     * @writes           [sv.sample_r[voice]]
     * @opcodes          [pw, PW, pwt, pwd]
     * @consumes         [final_phase, table_size, current_sample]
     * @produces         [s, sv.sample_r[voice]]
     * @depends          [osc_phase_mod_offset]
     * @condition        sv.source_type && sv.source_type[voice] == OSC_SOURCE_POLYBLEP && table_size > 0
     */
    if (sv.source_type && sv.source_type[voice] == OSC_SOURCE_POLYBLEP && table_size > 0) {
        int w = sv.wave_table_index[voice];
        if (w == WAVE_TABLE_SQR || w == WAVE_TABLE_SAW_DOWN || w == WAVE_TABLE_SAW_UP ||
            w == WAVE_TABLE_TRI || w == WAVE_TABLE_CENTERED_PULSE) {
            float p = (float)(final_phase / (double)table_size);
            while (p >= 1.0f) p -= 1.0f;
            while (p < 0.0f) p += 1.0f;
            float dt = fabsf(sv.phase_inc[voice]) / (float)table_size;
            if (dt <= 0.0f) dt = 1e-6f;
            float s = 0.0f;
            switch (w) {
                case WAVE_TABLE_SQR:
                case WAVE_TABLE_CENTERED_PULSE: {
                    float pwm = sv.pulse_width ? sv.pulse_width[voice] : 0.5f;
                    if (sv.use_pwm_envelope && sv.use_pwm_envelope[voice] && sv.pwm_envelope) {
                        float env_val = envelope_step_e(&sv.pwm_envelope[voice], current_sample);
                        pwm += env_val * sv.pwm_env_depth[voice];
                    }
                    if (sv.pwm_mod_osc && sv.pwm_mod_osc[voice] >= 0 && sv.sample) {
                        int mod_v = sv.pwm_mod_osc[voice];
                        pwm += sv.sample[mod_v] * sv.pwm_mod_depth[voice];
                    }
                    if (pwm < 0.01f) pwm = 0.01f;
                    if (pwm > 0.99f) pwm = 0.99f;
                    s = sk_polyblep_pulse(p, dt, pwm);
                    break;
                }
                case WAVE_TABLE_SAW_DOWN:
                    s = -sk_polyblep_saw(p, dt);
                    break;
                case WAVE_TABLE_SAW_UP:
                    s = sk_polyblep_saw(p, dt);
                    break;
                case WAVE_TABLE_TRI:
                    s = 2.0f * fabsf(2.0f * p - 1.0f) - 1.0f;
                    break;
                default:
                    break;
            }
            if (sv.sample_r) sv.sample_r[voice] = s;
            return s;
        }
    }
    /* @end osc_polyblep_core */

    /* @stage osc_wavetable_lookup
     * @title            Wavetable Interpolation & Stereo Right Channel Lookup
     * @scope            voice
     * @rate             sample
     * @phase            osc_eval
     * @reads            [sv.interpolate[voice], sv.table[voice], sv.table_right[voice], sv.sample_r]
     * @writes           [sv.sample_r[voice]]
     * @opcodes          [w, int]
     * @consumes         [final_phase, wave_range_start, wave_range_end, one_shot]
     * @produces         [s_left, s_right, sv.sample_r[voice]]
     * @depends          [osc_phase_mod_offset]
     */
    int idx = (int)final_phase;
    if (idx >= (int)wave_range_end) idx = (int)wave_range_end - 1;
    if (idx < (int)wave_range_start) idx = (int)wave_range_start;

    float s_left;
    float s_right;
    if (sv.interpolate[voice]) {
        float frac = (float)(final_phase - (double)idx);
        int next_idx = idx + 1;
        if (next_idx >= (int)wave_range_end)
            next_idx = one_shot ? (int)wave_range_end - 1 :
                (int)wave_range_start;
        float sample1 = sv.table[voice][idx];
        float sample2 = sv.table[voice][next_idx];
        s_left = sample1 + frac * (sample2 - sample1);
        if (sv.table_right && sv.table_right[voice]) {
            float r1 = sv.table_right[voice][idx];
            float r2 = sv.table_right[voice][next_idx];
            s_right = r1 + frac * (r2 - r1);
        } else {
            s_right = s_left;
        }
    } else {
        s_left = sv.table[voice][idx];
        if (sv.table_right && sv.table_right[voice]) {
            s_right = sv.table_right[voice][idx];
        } else {
            s_right = s_left;
        }
    }
    if (sv.sample_r) sv.sample_r[voice] = s_right;
    return s_left;
    /* @end osc_wavetable_lookup */
}

float osc_next_at(skred_engine_t *engine, int voice, float phase_inc, float phase_offset,
    uint64_t current_sample) {
    if (sv.finished[voice]) return 0.0f;
    if (sv.paused && sv.paused[voice]) {
        double phase = sv.phase[voice];
        float s = osc_sample_at_phase(engine, voice, phase, phase_offset, current_sample,
            sv.table_size[voice], 0.0f, (float)sv.table_size[voice], sv.one_shot[voice],
            sv.loop_start_f[voice], sv.loop_end_f[voice]);
        return s;
    }

    if (sv.playback_class[voice] == OSC_PLAYBACK_CYCLE_SIMPLE) {
        double phase = osc_cycle_phase_next(voice, phase_inc);
        if (!isfinite(phase)) {
            sv.phase[voice] = 0.0;
            return 0.0f;
        }
        sv.phase[voice] = phase;
        float s = osc_sample_at_phase(engine, voice, phase, phase_offset, current_sample,
            sv.table_size[voice], 0.0f, (float)sv.table_size[voice], false,
            0.0f, (float)sv.table_size[voice]);
        if (sv.sync_blep[voice] != 0.0f) {
            s += sv.sync_blep[voice];
            if (sv.sample_r) sv.sample_r[voice] += sv.sync_blep[voice];
            sv.sync_blep[voice] = 0.0f;
        }
        return s;
    }

    const int table_size = sv.table_size[voice];
    const bool one_shot = sv.one_shot[voice];
    const bool loop_active = sv.loop_active[voice];
    const float wave_range_start = sv.wave_range_start_f[voice];
    const float wave_range_end = sv.wave_range_end_f[voice];
    
    const bool pingpong = sv.direction[voice] == 2;
    bool pingpong_reverse = pingpong && sv.pingpong_reverse[voice];
    double phase_step = phase_inc;
    if (pingpong) phase_step = pingpong_reverse ? -fabs(phase_step) : fabs(phase_step);
    else if (sv.direction[voice]) phase_step = -phase_step;
    bool reverse_step = phase_step < 0.0;

    double phase = sv.phase[voice] + phase_step;
    
    if (!isfinite(phase)) {
        sv.phase[voice] = wave_range_start;
        sv.finished[voice] = one_shot;
        return 0.0f;
    }
    
    // Get loop boundaries (precomputed if available)
    const bool valid_loop = loop_active && sv.loop_valid[voice];
    const double loop_start = valid_loop ? (double)sv.loop_start_f[voice] : (double)wave_range_start;
    const double loop_end = valid_loop ? (double)sv.loop_end_f[voice] : (double)wave_range_end;
    const double loop_length = valid_loop ? (double)sv.loop_length[voice] : (double)(wave_range_end - wave_range_start);

    bool sync_wrap = false;   /* oscillator completed a cycle this sample */
    double wrap_fraction = 0.0;

    // Wrap phase
    if (!one_shot) {
        if (pingpong && loop_length > 0.0 && (phase >= loop_end || phase < loop_start)) {
            double offset = fmod(phase - loop_start, loop_length * 2.0);
            if (offset < 0.0) offset += loop_length * 2.0;
            if (offset >= loop_length) {
                phase = loop_end - (offset - loop_length);
                sv.pingpong_reverse[voice] = 1;
            } else {
                phase = loop_start + offset;
                sv.pingpong_reverse[voice] = 0;
            }
        } else if (phase >= loop_end) {
            double excess = phase - loop_end;
            phase = loop_start + fmod(excess, loop_length);
            sync_wrap = true;
            wrap_fraction = (fabs(phase_step) > 0.0) ? (fmod(excess, loop_length) / fabs(phase_step)) : 0.0;
        } else if (phase < loop_start) {
            double excess = loop_start - phase;
            phase = loop_end - fmod(excess, loop_length);
            sync_wrap = true;
            wrap_fraction = (fabs(phase_step) > 0.0) ? (fmod(excess, loop_length) / fabs(phase_step)) : 0.0;
        }
    } else if (pingpong && loop_active &&
               ((!reverse_step && phase >= loop_end) ||
                (reverse_step && phase < loop_start))) {
        int guard = 32;
        while (guard-- > 0 &&
               ((!reverse_step && phase >= loop_end) ||
                (reverse_step && phase < loop_start))) {
            if (sv.loop_stop_requested[voice]) {
                sv.loop_active[voice] = 0;
                sv.loop_stop_requested[voice] = 0;
                sv.loop_release_tail[voice] = 0;
                if (!reverse_step && phase >= (double)wave_range_end) {
                    phase = (double)wave_range_end - 1e-6;
                    sv.finished[voice] = 1;
                } else if (reverse_step && phase < (double)wave_range_start) {
                    phase = (double)wave_range_start;
                    sv.finished[voice] = 1;
                }
                break;
            }
            if (sv.loop_bounded[voice] && sv.loop_remaining[voice] <= 0) {
                sv.loop_active[voice] = 0;
                sv.loop_stop_requested[voice] = 0;
                sv.loop_release_tail[voice] = 1;
                sv.loop_ended[voice] = 1;
                if (!reverse_step && phase >= (double)wave_range_end) {
                    phase = (double)wave_range_end - 1e-6;
                    sv.finished[voice] = 1;
                } else if (reverse_step && phase < (double)wave_range_start) {
                    phase = (double)wave_range_start;
                    sv.finished[voice] = 1;
                }
                break;
            }
            if (sv.loop_bounded[voice]) sv.loop_remaining[voice]--;
            if (!reverse_step) {
                phase = loop_end - (phase - loop_end);
                sv.pingpong_reverse[voice] = 1;
            } else {
                phase = loop_start + (loop_start - phase);
                sv.pingpong_reverse[voice] = 0;
            }
            pingpong_reverse = sv.pingpong_reverse[voice] != 0;
            phase_step = pingpong_reverse ? -fabs(phase_step) : fabs(phase_step);
            reverse_step = phase_step < 0.0;
        }
    } else if (!reverse_step && phase >= loop_end) {
        if (!loop_active) {
            phase = loop_end - 1e-6;
            sv.finished[voice] = 1;
        } else if (sv.loop_stop_requested[voice]) {
            sv.loop_active[voice] = 0;
            sv.loop_stop_requested[voice] = 0;
            sv.loop_release_tail[voice] = 0;
            if (phase >= (double)wave_range_end) {
                phase = (double)wave_range_end - 1e-6;
                sv.finished[voice] = 1;
            }
        } else if (sv.loop_bounded[voice]) {
            int crossings = osc_loop_crossings(phase - loop_end, loop_length);
            int wraps = crossings < sv.loop_remaining[voice] ?
                crossings : sv.loop_remaining[voice];
            phase -= (double)wraps * loop_length;
            sv.loop_remaining[voice] -= wraps;
            if (wraps > 0) sync_wrap = true;
            if (wraps < crossings) {
                sv.loop_active[voice] = 0;
                sv.loop_stop_requested[voice] = 0;
                sv.loop_release_tail[voice] = 1;
                sv.loop_ended[voice] = 1;
                if (phase >= (double)wave_range_end) {
                    phase = (double)wave_range_end - 1e-6;
                    sv.finished[voice] = 1;
                }
            }
        } else {
            double excess = phase - loop_start;
            phase = loop_start + fmod(excess, loop_length);
            sync_wrap = true;
            wrap_fraction = (fabs(phase_step) > 0.0) ? (fmod(excess, loop_length) / fabs(phase_step)) : 0.0;
        }
    } else if (reverse_step && phase < loop_start) {
        if (!loop_active) {
            phase = loop_start;
            sv.finished[voice] = 1;
        } else if (sv.loop_stop_requested[voice]) {
            sv.loop_active[voice] = 0;
            sv.loop_stop_requested[voice] = 0;
            sv.loop_release_tail[voice] = 0;
            if (phase < (double)wave_range_start) {
                phase = (double)wave_range_start;
                sv.finished[voice] = 1;
            }
        } else if (sv.loop_bounded[voice]) {
            int crossings = osc_loop_crossings(loop_start - phase, loop_length);
            int wraps = crossings < sv.loop_remaining[voice] ?
                crossings : sv.loop_remaining[voice];
            phase += (double)wraps * loop_length;
            sv.loop_remaining[voice] -= wraps;
            if (wraps > 0) {
                sync_wrap = true;
                wrap_fraction = 0.0;
            }
            if (wraps < crossings) {
                sv.loop_active[voice] = 0;
                sv.loop_stop_requested[voice] = 0;
                sv.loop_release_tail[voice] = 1;
                sv.loop_ended[voice] = 1;
                if (phase < (double)wave_range_start) {
                    phase = (double)wave_range_start;
                    sv.finished[voice] = 1;
                }
            }
        } else {
            double excess = loop_end - phase;
            phase = loop_end - fmod(excess, loop_length);
            sync_wrap = true;
            wrap_fraction = (fabs(phase_step) > 0.0) ? (fmod(excess, loop_length) / fabs(phase_step)) : 0.0;
        }
    }
    
    double old_phase = sv.phase[voice];
    if (sync_wrap) osc_hard_sync_fire(voice, wrap_fraction);
    sv.phase[voice] = phase;

    int wave = sv.wave_table_index[voice];
    if (sw.num_cues && sw.num_cues[wave] > 0 && sw.cues && sw.cues[wave]) {
        int n_cues = sw.num_cues[wave];
        const sk_cue_t *cues = sw.cues[wave];
        int cursor = (sv.cue_cursor ? sv.cue_cursor[voice] : 0);
        if (cursor < 0) cursor = 0;
        while (cursor < n_cues) {
            uint32_t c_start = cues[cursor].start;
            if (old_phase < (double)c_start && phase >= (double)c_start) {
                skred_control_cue_event(current_sample, voice, wave, cursor,
                                        c_start, cues[cursor].end, cues[cursor].type);
                cursor++;
            } else if (phase < (double)c_start) {
                break;
            } else {
                cursor++;
            }
        }
        if (sync_wrap || phase < old_phase) {
            cursor = 0;
            while (cursor < n_cues && cues[cursor].start < (uint32_t)phase) cursor++;
        }
        if (sv.cue_cursor) sv.cue_cursor[voice] = cursor;
    }
    float s = osc_sample_at_phase(engine, voice, phase, phase_offset, current_sample,
        table_size, wave_range_start, wave_range_end, one_shot,
        (float)loop_start, (float)loop_end);
    if (sv.sync_blep[voice] != 0.0f) {
        s += sv.sync_blep[voice];
        if (sv.sample_r) sv.sample_r[voice] += sv.sync_blep[voice];
        sv.sync_blep[voice] = 0.0f;
    }
    return s;
}

float osc_next(skred_engine_t *engine, int voice, float phase_inc) {
    return osc_next_at(engine, voice, phase_inc, 0.0f, SAMPLE_COUNT_GET());
}

void osc_set_wave_table_index(skred_engine_t *engine, int voice, int wave) {
  // if we were using a r/w wave table, adjust ref count
  int old = sv.wave_table_index[voice];
  // old == -1 means voice was never assigned a wave table yet (e.g. first
  // call from voice_reset).  Guard before indexing to avoid sw.readonly[-1].
  if (old != wave) {
    if (old >= 0 && sw.refcount) sw.refcount[old]--;
    if (wave >= 0 && sw.refcount) sw.refcount[wave]++;
  }

  if (wave >= WAVE_TABLE_CAP_1 && wave <= WAVE_TABLE_CAP_2) {
    sv.wave_table_index[voice] = wave;
    if (sv.source_type) sv.source_type[voice] = OSC_SOURCE_CAPTURE;
    sv.finished[voice] = 0;
    sv.table[voice] = NULL;
    if (sv.table_right) sv.table_right[voice] = NULL;
    return;
  }
  if (wave >= WAVE_TABLE_SILENCE && wave <= WAVE_TABLE_CONTROL) {
    sv.wave_table_index[voice] = wave;
    if (sv.source_type) sv.source_type[voice] = OSC_SOURCE_UTILITY;
    sv.finished[voice] = 0;
    sv.table[voice] = NULL;
    if (sv.table_right) sv.table_right[voice] = NULL;
    return;
  }
  if (wave == WAVE_TABLE_NOISE || wave == WAVE_TABLE_NOISE_ALT) {
    sv.wave_table_index[voice] = wave;
    if (sv.source_type) sv.source_type[voice] = OSC_SOURCE_NOISE;
    sv.finished[voice] = 0;
    sv.table[voice] = NULL;
    if (sv.table_right) sv.table_right[voice] = NULL;
    return;
  }
  if ((wave >= WAVE_TABLE_SQR && wave <= WAVE_TABLE_TRI) ||
      wave == WAVE_TABLE_COSINE || wave == WAVE_TABLE_CENTERED_PULSE) {
    sv.wave_table_index[voice] = wave;
    if (sv.source_type) sv.source_type[voice] = OSC_SOURCE_POLYBLEP;
    sv.finished[voice] = 0;
    sv.table[voice] = NULL;
    if (sv.table_right) sv.table_right[voice] = NULL;
    sv.table_rate[voice] = (float)MAIN_SAMPLE_RATE;
    sv.table_size[voice] = SIZE_SINE;
    sv.table_size_rate[voice] = (float)SIZE_SINE / (float)MAIN_SAMPLE_RATE;
    sv.one_shot[voice] = 0;
    sv.loop_override[voice] = 0;
    sv.wave_range_override[voice] = 0;
    sv.wave_range_start[voice] = 0;
    sv.wave_range_end[voice]   = SIZE_SINE;
    sv.wave_range_start_f[voice] = 0.0f;
    sv.wave_range_end_f[voice]   = (float)SIZE_SINE;
    sv.loop_start[voice] = 0;
    sv.loop_enabled[voice] = 1;
    sv.loop_active[voice] = 1;
    sv.loop_bounded[voice] = 0;
    sv.loop_remaining[voice] = 0;
    sv.loop_stop_requested[voice] = 0;
    sv.loop_release_tail[voice] = 0;
    sv.loop_ended[voice] = 0;
    sv.loop_valid[voice] = 1;
    sv.loop_start_f[voice] = 0.0f;
    sv.loop_end_f[voice] = (float)SIZE_SINE;
    sv.loop_length[voice] = (float)SIZE_SINE;
    sv.phase_inc[voice] = osc_get_phase_inc(engine, voice, sv.freq[voice]);
    return;
  }

  if (sw.data[wave] && sw.size[wave] && sw.rate[wave] > 0.0) {
    sv.wave_table_index[voice] = wave;
    if (sv.source_type) {
      if ((wave >= WAVE_TABLE_SQR && wave <= WAVE_TABLE_TRI) || wave == WAVE_TABLE_CENTERED_PULSE) {
        sv.source_type[voice] = OSC_SOURCE_POLYBLEP;
      } else if (sw.one_shot[wave] || (wave >= WAVE_TABLE_DRUM_01 && wave <= WAVE_TABLE_DRUM_18) || wave >= EXT_SAMPLE_000) {
        sv.source_type[voice] = OSC_SOURCE_SAMPLE;
      } else {
        sv.source_type[voice] = OSC_SOURCE_WAVETABLE;
      }
    }
    int update_freq = 0;
    if (sw.one_shot[wave]) sv.finished[voice] = 1;
    else sv.finished[voice] = 0;
    if (
      sv.table_rate[voice] != sw.rate[wave] ||
      sv.table_size[voice] != sw.size[wave]
      ) update_freq = 1;
    sv.table_rate[voice] = sw.rate[wave];
    sv.table_size[voice] = sw.size[wave];
    sv.table_size_rate[voice] = (float)sv.table_size[voice] / MAIN_SAMPLE_RATE;
    int pair = sv.channel_pair ? sv.channel_pair[voice] : 0;
    int ch_left = pair * 2;
    int ch_right = pair * 2 + 1;
    if (sw.channels_data && sw.channels_data[wave] && ch_right < sw.channels[wave]) {
      sv.table[voice] = sw.channels_data[wave][ch_left];
      if (sv.table_right) sv.table_right[voice] = sw.channels_data[wave][ch_right];
    } else if (sw.data_right && sw.channels && sw.channels[wave] == 2 && pair == 0) {
      sv.table[voice] = sw.data[wave];
      if (sv.table_right) sv.table_right[voice] = sw.data_right[wave];
    } else {
      sv.table[voice] = sw.data[wave];
      if (sv.table_right) sv.table_right[voice] = NULL;
    }
    sv.one_shot[voice] = sw.one_shot[wave];
    sv.loop_override[voice] = 0;
    sv.wave_range_override[voice] = 0;
    sv.wave_range_start[voice] = 0;
    sv.wave_range_end[voice]   = sv.table_size[voice];
    sv.wave_range_start_f[voice] = 0.0f;
    sv.wave_range_end_f[voice]   = (float)sv.table_size[voice];
    sv.loop_start[voice] = sw.loop_start[wave];
    sv.loop_enabled[voice] = sw.loop_enabled[wave];
    sv.loop_active[voice] = sv.loop_enabled[voice];
    sv.loop_bounded[voice] = sv.loop_count[voice] > 0;
    sv.loop_remaining[voice] = sv.loop_count[voice];
    sv.loop_stop_requested[voice] = 0;
    sv.loop_release_tail[voice] = 0;
    sv.loop_ended[voice] = 0;
    sv.loop_end[voice] = sw.loop_end[wave];
    sv.midi_note[voice] = sw.midi_note[wave];
    sv.offset_hz[voice] = sw.offset_hz[wave];
    sv.direction[voice] = sw.direction[wave];
    //
    int start = sv.loop_start[voice];
    int end = sv.loop_end[voice];
    sv.loop_start_f[voice] = (float)start;
    sv.loop_end_f[voice] = (float)end;
    if (end > start) {
      sv.loop_valid[voice] = 1;
      sv.loop_length[voice] = (float)(end - start);
    } else {
      sv.loop_valid[voice] = 0;
      sv.loop_length[voice] = (float)sv.table_size[voice];
    }
    if (update_freq) {
      osc_set_freq(engine, voice, sv.freq[voice]);
    }
    osc_update_wavetable_mip(voice);
    osc_reclassify(engine, voice);
  }
}

static void voice_loop_points_apply(int voice, int start, int end);

static void voice_wave_range_apply(int voice, int start, int end) {
  sv.wave_range_start[voice] = start;
  sv.wave_range_end[voice]   = end;
  sv.wave_range_start_f[voice] = (float)start;
  sv.wave_range_end_f[voice]   = (float)end;
}

int voice_wave_range_set(int voice, int start, int end) {
    if (voice_invalid(voice) || sv.table_size[voice] < 2 ||
        start < 0 || end <= start || end > sv.table_size[voice]) {
        return SYNTH_INVALID_VOICE;
    }
    voice_wave_range_apply(voice, start, end);
    if (sv.loop_start[voice] < start || sv.loop_end[voice] > end ||
        sv.loop_end[voice] <= sv.loop_start[voice]) {
      voice_loop_points_apply(voice, start, end);
    }
    sv.wave_range_override[voice] = 1;
    osc_reclassify(&skred_global_engine, voice);
    return 0;
}

int voice_wave_range_reset(int voice) {
    if (voice_invalid(voice)) return SYNTH_INVALID_VOICE;
    voice_wave_range_apply(voice, 0, sv.table_size[voice]);
    sv.wave_range_override[voice] = 0;
    osc_reclassify(&skred_global_engine, voice);
    return 0;
}

static void voice_loop_points_apply(int voice, int start, int end) {
  sv.loop_start[voice] = start;
  sv.loop_end[voice] = end;
  sv.loop_start_f[voice] = (float)start;
  sv.loop_end_f[voice] = (float)end;
  if (end > start) {
    sv.loop_valid[voice] = 1;
    sv.loop_length[voice] = end - start;
  } else {
    sv.loop_valid[voice] = 0;
    sv.loop_length[voice] = sv.wave_range_end[voice] - sv.wave_range_start[voice];
  }
}

static void voice_loop_points_apply_default(int voice, int start, int end) {
  if (start < sv.wave_range_start[voice] || end <= start ||
      end > sv.wave_range_end[voice]) {
    start = sv.wave_range_start[voice];
    end = sv.wave_range_end[voice];
  }
  voice_loop_points_apply(voice, start, end);
}

int voice_loop_points_set(int voice, int start, int end) {
  if (voice_invalid(voice) || sv.table_size[voice] < 2 ||
      start < sv.wave_range_start[voice] || end <= start ||
      end > sv.wave_range_end[voice]) {
    return SYNTH_INVALID_VOICE;
  }
  voice_loop_points_apply(voice, start, end);
  sv.loop_override[voice] = 1;
  osc_reclassify(&skred_global_engine, voice);
  return 0;
}

int voice_loop_points_reset(int voice) {
  if (voice_invalid(voice)) return SYNTH_INVALID_VOICE;
  int wave = sv.wave_table_index[voice];
  if (wave_invalid(wave) || !sw.data[wave])
    return SYNTH_INVALID_VOICE;
  voice_loop_points_apply_default(voice, sw.loop_start[wave], sw.loop_end[wave]);
  sv.loop_override[voice] = 0;
  osc_reclassify(&skred_global_engine, voice);
  return 0;
}

int wave_loop_points_set(int wave, int start, int end) {
  if (wave_invalid(wave) || !sw.data[wave] ||
      sw.size[wave] < 2 || start < 0 || end <= start ||
      end > sw.size[wave]) {
    return -1;
  }
  sw.loop_start[wave] = start;
  sw.loop_end[wave] = end;
  for (int voice = 0; voice < synth_config.voice_max; voice++) {
    if (sv.wave_table_index[voice] == wave && !sv.loop_override[voice])
      voice_loop_points_apply_default(voice, start, end);
  }
  return 0;
}

void osc_trigger(skred_engine_t *engine, int voice) {
    sv.finished[voice] = 0;
    if (sv.paused) sv.paused[voice] = 0;
    if (sv.pause_gain) sv.pause_gain[voice] = 1.0f;
    if (sv.cue_cursor) sv.cue_cursor[voice] = 0;
    sv.loop_active[voice] = sv.loop_enabled[voice];
    sv.loop_bounded[voice] = sv.loop_count[voice] > 0;
    sv.loop_remaining[voice] = sv.loop_count[voice];
    sv.loop_stop_requested[voice] = 0;
    sv.loop_release_tail[voice] = 0;
    sv.loop_ended[voice] = 0;
    sv.pingpong_reverse[voice] = sv.direction[voice] == 1;
    osc_update_wavetable_mip(voice);
    osc_reclassify(engine, voice);
    
    if (sv.one_shot[voice]) {
        if (sv.direction[voice] == 1) {
            sv.phase[voice] = (double)sv.wave_range_end_f[voice] - 1e-6;
        } else {
            sv.phase[voice] = (double)sv.wave_range_start_f[voice];
        }
    } else {
        // Preserve direction, but start at appropriate boundary
        if (sv.direction[voice] == 1) {
            // Backward playback: start at loop end
            sv.phase[voice] = sv.loop_active[voice]
                ? (double)sv.loop_end[voice] - 1e-6  // or sv.loop_end_f[voice]
                : (double)sv.wave_range_end_f[voice] - 1e-6;
        } else {
            // Forward playback: start at loop start
            sv.phase[voice] = sv.loop_active[voice]
                ? (double)sv.loop_start[voice]  // or sv.loop_start_f[voice]
                : (double)sv.wave_range_start_f[voice];
        }
    }
    if (sv.wave_table_index[voice] == WAVE_TABLE_NOISE) {
        if (sv.noise_lfsr[voice] == 0) {
            sv.noise_lfsr[voice] = (sv.noise_type[voice] == 0) ? 0x7FFFFF : 1;
        }
        osc_noise_step(voice);
    }
}

float quantize_bits_curve(float v, int bits, int curve, uint64_t *rng) {
  return sk_quantize_bits_curve(v, bits, curve, rng);
}

int osc_noise_set_type(int voice, int type) {
    if (voice_invalid(voice) || type < 0 || type > 7) return -1;
    sv.noise_type[voice] = type;
    sk_lfsr_noise_t n;
    sk_lfsr_noise_init(&n, type);
    sv.noise_bits[voice] = n.bits;
    sv.noise_tap[voice] = n.tap;
    sv.noise_dac[voice] = n.dac;
    if (sv.noise_lfsr[voice] == 0) {
        sv.noise_lfsr[voice] = (int)n.lfsr;
    }
    return 0;
}

int osc_noise_set_custom(int voice, int bits, int tap, int dac) {
    if (voice_invalid(voice)) return -1;
    sk_lfsr_noise_t n;
    n.lfsr = (uint32_t)sv.noise_lfsr[voice];
    sk_lfsr_noise_set_custom(&n, bits, tap, dac);
    sv.noise_type[voice] = n.type;
    sv.noise_bits[voice] = n.bits;
    sv.noise_tap[voice] = n.tap;
    sv.noise_dac[voice] = n.dac;
    if (sv.noise_lfsr[voice] == 0) {
        sv.noise_lfsr[voice] = (int)n.lfsr;
    }
    return 0;
}

void osc_noise_step(int voice) {
    sk_lfsr_noise_t n;
    n.lfsr = (uint32_t)sv.noise_lfsr[voice];
    n.type = sv.noise_type[voice];
    n.bits = sv.noise_bits[voice];
    n.tap = sv.noise_tap[voice];
    n.dac = sv.noise_dac[voice];

    sv.noise_sample[voice] = sk_lfsr_noise_step(&n);
    sv.noise_lfsr[voice] = (int)n.lfsr;
}

