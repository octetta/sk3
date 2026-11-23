#include "synth.h"
#include "synth-internal.h"
#include "util.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "control-events.h"
#include <time.h>
#define SIZE_SINE (4096)
#include "sk_dsp.h"
#include "dwg.h"
#include "vendor/ksynth/ksynth.h"
#include "dwg_sources.h"
#include "esq1_sources.h"
#include "drums_sources.h"

static const int s_mip_max_harmonics[WAVETABLE_MIP_LEVELS] = {
    256, 128, 64, 32, 16, 8, 4, 1
};

typedef sk_cplx_t synth_cplx_t;

static inline void synth_fft_radix2(synth_cplx_t *buf, int n, int inverse) {
    sk_fft_radix2(buf, n, inverse);
}

int wave_generate_mipmaps_ksynth(int w, const double *harmonics, int num_harmonics) {
    if (wave_invalid(w) || !sw.data[w] || sw.size[w] <= 0 || !harmonics || num_harmonics <= 0) {
        return -1;
    }
    if (!sw.mip_levels || !sw.mip_data[0]) return -1;

    if (sw.mip_levels[w] > 0) {
        for (int lvl = 0; lvl < WAVETABLE_MIP_LEVELS; lvl++) {
            if (sw.mip_data[lvl] && sw.mip_data[lvl][w]) {
                free(sw.mip_data[lvl][w]);
                sw.mip_data[lvl][w] = NULL;
            }
        }
        sw.mip_levels[w] = 0;
    }

    int N = sw.size[w];
    int is_pow2 = (N >= 64) && ((N & (N - 1)) == 0);
    if (!is_pow2) {
        return wave_generate_mipmaps_fft(w);
    }

    // Determine fundamental sign from quarter-cycle test without sin() calls
    int q1 = N / 4;
    int q3 = (3 * N) / 4;
    float sign = (sw.data[w][q1] - sw.data[w][q3] < 0.0f) ? -1.0f : 1.0f;

    synth_cplx_t *work = (synth_cplx_t *)calloc(N, sizeof(synth_cplx_t));
    if (!work) return -1;

    for (int lvl = 0; lvl < WAVETABLE_MIP_LEVELS; lvl++) {
        int max_h = s_mip_max_harmonics[lvl];
        int M = num_harmonics < max_h ? num_harmonics : max_h;
        if (M > N / 2 - 1) M = N / 2 - 1;
        if (M < 1) M = 1;

        memset(work, 0, N * sizeof(synth_cplx_t));
        for (int k = 1; k <= M; k++) {
            float amp = (float)harmonics[k - 1] * sign;
            work[k].im = -amp;
            work[N - k].im = amp;
        }

        synth_fft_radix2(work, N, 1);

        float *buf = (float *)malloc(N * sizeof(float));
        if (!buf) {
            free(work);
            return -1;
        }

        float pk = 0.0f;
        for (int i = 0; i < N; i++) {
            buf[i] = work[i].re;
            if (fabsf(work[i].re) > pk) pk = fabsf(work[i].re);
        }

        if (pk > 1e-10f) {
            float scale = 1.0f / pk;
            for (int i = 0; i < N; i++) buf[i] *= scale;
        }
        sw.mip_data[lvl][w] = buf;
    }
    free(work);
    sw.mip_levels[w] = WAVETABLE_MIP_LEVELS;
    return 0;
}

int wave_generate_mipmaps_fft(int w) {
    if (wave_invalid(w) || !sw.data[w] || sw.size[w] <= 0) return -1;
    if (!sw.mip_levels || !sw.mip_data[0]) return -1;

    if (sw.mip_levels[w] > 0) {
        for (int lvl = 0; lvl < WAVETABLE_MIP_LEVELS; lvl++) {
            if (sw.mip_data[lvl] && sw.mip_data[lvl][w]) {
                free(sw.mip_data[lvl][w]);
                sw.mip_data[lvl][w] = NULL;
            }
        }
        sw.mip_levels[w] = 0;
    }

    int N = sw.size[w];
    int is_pow2 = (N >= 64) && ((N & (N - 1)) == 0);

    if (is_pow2) {
        synth_cplx_t *spec = (synth_cplx_t *)malloc(N * sizeof(synth_cplx_t));
        synth_cplx_t *work = (synth_cplx_t *)malloc(N * sizeof(synth_cplx_t));
        if (!spec || !work) {
            free(spec);
            free(work);
            return -1;
        }

        for (int i = 0; i < N; i++) {
            spec[i].re = sw.data[w][i];
            spec[i].im = 0.0f;
        }
        synth_fft_radix2(spec, N, 0);

        for (int lvl = 0; lvl < WAVETABLE_MIP_LEVELS; lvl++) {
            int max_h = s_mip_max_harmonics[lvl];
            int M = max_h < (N / 2) ? max_h : (N / 2 - 1);
            if (M < 1) M = 1;

            memcpy(work, spec, N * sizeof(synth_cplx_t));
            work[0].re = 0.0f;
            work[0].im = 0.0f;
            for (int k = M + 1; k <= N / 2; k++) {
                work[k].re = 0.0f;
                work[k].im = 0.0f;
                work[N - k].re = 0.0f;
                work[N - k].im = 0.0f;
            }

            synth_fft_radix2(work, N, 1);

            float *buf = (float *)malloc(N * sizeof(float));
            if (!buf) {
                free(spec);
                free(work);
                return -1;
            }

            float pk = 0.0f;
            for (int i = 0; i < N; i++) {
                buf[i] = work[i].re;
                if (fabsf(work[i].re) > pk) pk = fabsf(work[i].re);
            }

            if (pk > 1e-10f) {
                float scale = 1.0f / pk;
                for (int i = 0; i < N; i++) buf[i] *= scale;
            }
            sw.mip_data[lvl][w] = buf;
        }
        free(spec);
        free(work);
    } else {
        int max_harm = s_mip_max_harmonics[0];
        if (max_harm > N / 2) max_harm = N / 2;
        if (max_harm < 1) max_harm = 1;

        double *a_k = (double *)calloc(max_harm + 1, sizeof(double));
        double *b_k = (double *)calloc(max_harm + 1, sizeof(double));
        if (!a_k || !b_k) {
            free(a_k);
            free(b_k);
            return -1;
        }

        double inv_N = 2.0 / (double)N;
        for (int k = 1; k <= max_harm; k++) {
            double re = 0.0, im = 0.0;
            double w_k = 2.0 * M_PI * (double)k / (double)N;
            for (int i = 0; i < N; i++) {
                double ph = w_k * (double)i;
                re += (double)sw.data[w][i] * cos(ph);
                im += (double)sw.data[w][i] * sin(ph);
            }
            a_k[k] = re * inv_N;
            b_k[k] = im * inv_N;
        }

        for (int lvl = 0; lvl < WAVETABLE_MIP_LEVELS; lvl++) {
            int max_h = s_mip_max_harmonics[lvl];
            int M = max_h < max_harm ? max_h : max_harm;
            if (M < 1) M = 1;

            float *buf = (float *)malloc(N * sizeof(float));
            if (!buf) {
                free(a_k);
                free(b_k);
                return -1;
            }

            double pk = 0.0;
            for (int i = 0; i < N; i++) {
                double acc = 0.0;
                for (int k = 1; k <= M; k++) {
                    double ph = 2.0 * M_PI * (double)k * (double)i / (double)N;
                    acc += a_k[k] * cos(ph) + b_k[k] * sin(ph);
                }
                buf[i] = (float)acc;
                if (fabs(acc) > pk) pk = fabs(acc);
            }

            if (pk > 1e-10) {
                float scale = (float)(1.0 / pk);
                for (int i = 0; i < N; i++) buf[i] *= scale;
            }
            sw.mip_data[lvl][w] = buf;
        }
        free(a_k);
        free(b_k);
    }

    sw.mip_levels[w] = WAVETABLE_MIP_LEVELS;
    return 0;
}

int wave_generate_mipmaps(int w) {
    if (wave_invalid(w)) return -1;
    if (sw.num_harmonics && sw.num_harmonics[w] > 0 && sw.harmonics && sw.harmonics[w]) {
        return wave_generate_mipmaps_ksynth(w, sw.harmonics[w], sw.num_harmonics[w]);
    }
    return wave_generate_mipmaps_fft(w);
}

#ifdef KSYNTH
static void synth_eval_and_register_ks_wave(int w, const char *name, const char *ks_code, int is_drum) {
  if (wave_invalid(w)) return;

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  if (is_drum) {
    // Reset PRNG seed to 1 so noise generation in ksynth ('r') is 100% deterministic
    srand(1);
  }

  size_t mem_limit = is_drum ? (16 * 1024 * 1024) : (4 * 1024 * 1024);
  double sr = (MAIN_SAMPLE_RATE > 0) ? (double)MAIN_SAMPLE_RATE : 44100.0;
  ks_ctx *ctx = ks_create(mem_limit, 1000000000, sr);
  if (!ctx) return;

  K result = NULL;
  const char *text = ks_code;
  size_t text_len = strlen(text);
  size_t pos = 0;

  while (pos < text_len) {
    char line[1024];
    size_t start = pos;
    while (pos < text_len && text[pos] != '\n' && text[pos] != '\r') pos++;
    size_t len = pos - start;
    while (pos < text_len && (text[pos] == '\n' || text[pos] == '\r')) pos++;
    if (len == 0) continue;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    memcpy(line, text + start, len);
    line[len] = '\0';
    result = ks_eval(ctx, line, len);
    if (ctx->last_status != KS_OK) {
      result = NULL;
      break;
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &t1);
  float elapsed_ms = (float)((t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) * 1e-6);

  if (result && result->n > 0) {
    int size = result->n;
    float *buf = (float *)malloc(size * sizeof(float));
    if (buf) {
      for (int s = 0; s < size; s++) {
        buf[s] = (float)result->f[s];
      }

      if (sw.data[w] && sw.is_heap[w]) {
        free(sw.data[w]);
      }
      if (sw.source && sw.source_is_heap && sw.source_is_heap[w] && sw.source[w]) {
        free((void *)sw.source[w]);
      }
      if (sw.source) {
        sw.source[w] = ks_code;
      }
      if (sw.source_is_heap) {
        sw.source_is_heap[w] = 0;
      }
      if (sw.eval_ms) {
        sw.eval_ms[w] = elapsed_ms;
      }
      strncpy(sw.name[w], name, WAVE_NAME_MAX);
      sw.data[w] = buf;
      sw.size[w] = size;
      sw.rate[w] = (float)sr;
      sw.is_heap[w] = 1;
      sw.readonly[w] = 1;

      K k_o = k_get(ctx, 'O');
      int one_shot = (k_o && k_o->n == 1 && k_o->f[0] > 0) ? 1 : 0;
      sw.one_shot[w] = one_shot;

      K k_s = k_get(ctx, 'S');
      if (k_s && k_s->n == 1) sw.rate[w] = (float)k_s->f[0];

      K k_l = k_get(ctx, 'L');
      sw.loop_start[w] = (k_l && k_l->n == 1) ? (int)k_l->f[0] : 0;

      K k_e = k_get(ctx, 'E');
      sw.loop_end[w] = (k_e && k_e->n == 1) ? (int)k_e->f[0] : (one_shot ? 0 : size - 1);
      if (one_shot) {
        sw.loop_start[w] = 0;
        sw.loop_end[w] = 0;
      }

      K k_meta = k_get_var_str(ctx, "META");
      if (!k_meta) k_meta = k_get_var_str(ctx, "M");
      int is_sample = 0;
      int is_osc = 0;
      if (k_meta && k_is_string(k_meta)) {
        sk_meta_info_t meta;
        sk_parse_metadata((const char *)k_meta->f, &meta);
        if (meta.has_rate && meta.rate > 0) sw.rate[w] = meta.rate;
        if (meta.has_loop) {
          sw.loop_enabled[w] = 1;
          sw.loop_start[w] = meta.loop_start;
          sw.loop_end[w] = meta.loop_end >= size ? size : meta.loop_end;
        }
        if (meta.has_one_shot) sw.one_shot[w] = meta.one_shot;
        if (meta.has_root_note) sw.midi_note[w] = meta.root_note;
        if (meta.mode == SK_WAVE_MODE_SAMPLE) is_sample = 1;
        else if (meta.mode == SK_WAVE_MODE_OSC) is_osc = 1;

        if (meta.num_cues > 0 && sw.cues && sw.num_cues) {
          if (sw.cues[w]) free(sw.cues[w]);
          sw.cues[w] = (sk_cue_t *)malloc(meta.num_cues * sizeof(sk_cue_t));
          if (sw.cues[w]) {
            memcpy(sw.cues[w], meta.cues, meta.num_cues * sizeof(sk_cue_t));
            sw.num_cues[w] = meta.num_cues;
          }
        }
      }

      if (is_osc) {
        wave_generate_mipmaps(w);
      } else if (!is_sample && !sw.one_shot[w] && !is_drum) {
        K k_a = k_get(ctx, 'A');
        if (k_a && k_a->n > 0) {
          if (sw.harmonics) {
            if (sw.harmonics[w]) free(sw.harmonics[w]);
            sw.harmonics[w] = (double *)malloc(k_a->n * sizeof(double));
            if (sw.harmonics[w]) {
              memcpy(sw.harmonics[w], k_a->f, k_a->n * sizeof(double));
              if (sw.num_harmonics) sw.num_harmonics[w] = k_a->n;
            }
          }
          wave_generate_mipmaps_ksynth(w, k_a->f, k_a->n);
        } else if (size >= 64) {
          wave_generate_mipmaps_fft(w);
        }
      }
    }
  }

  ks_destroy(ctx);
}
#else
static void synth_eval_and_register_ks_wave(int w, const char *name, const char *ks_code, int is_drum) {
  (void)w;
  (void)name;
  (void)ks_code;
  (void)is_drum;
}
#endif

static void synth_init_ksynth_dwg(void) {
  for (int idx = 0; DWG_PATCH_SOURCES[idx].slot != 0; idx++) {
    const dwg_patch_source_t *p = &DWG_PATCH_SOURCES[idx];
    synth_eval_and_register_ks_wave(p->slot, p->name, p->ks_code, 0);
  }
}

static void synth_init_centered_krg2(void) {
  int src = WAVE_TABLE_KRG2;
  int dst = WAVE_TABLE_KRG2_CENTERED;
  if (sw.data[src] && sw.size[src] > 0) {
    int sz = sw.size[src];
    float *buf = (float *)malloc(sz * sizeof(float));
    if (buf) {
      for (int off = 0; off < sz; off++) {
        buf[off] = sw.data[src][(off + sz * 3 / 4) % sz];
      }
      strncpy(sw.name[dst], "centered krg2", WAVE_NAME_MAX);
      sw.data[dst] = buf;
      sw.size[dst] = sz;
      sw.rate[dst] = sw.rate[src];
      sw.is_heap[dst] = 1;
      sw.readonly[dst] = 1;
      sw.one_shot[dst] = 0;
      sw.loop_start[dst] = 0;
      sw.loop_end[dst] = sz - 1;
    }
  }
}

static void synth_init_ksynth_esq1(void) {
  for (int idx = 0; ESQ1_PATCH_SOURCES[idx].slot != 0; idx++) {
    const esq1_patch_source_t *p = &ESQ1_PATCH_SOURCES[idx];
    synth_eval_and_register_ks_wave(p->slot, p->name, p->ks_code, 0);
  }
}

static void synth_init_ksynth_drums(void) {
  for (int idx = 0; DRUM_PATCH_SOURCES[idx].slot != 0; idx++) {
    const drum_patch_source_t *p = &DRUM_PATCH_SOURCES[idx];
    synth_eval_and_register_ks_wave(p->slot, p->name, p->ks_code, 1);
  }
}

static void synth_init_ksynth_base_waves(void) {
#ifdef KSYNTH
  static const struct {
    int slot;
    const char *name;
    const char *ks_code;
  } base_waves[] = {
    {
      WAVE_TABLE_SINE, "sine",
      "/ Fundamental bandlimited sine wave\n"
      "P: ~4096\n"
      "W: w s P\n"
    },
    {
      WAVE_TABLE_SQR, "square",
      "/ Bandlimited square wave: odd harmonics 1/h\n"
      "P: ~4096\n"
      "H: !64\n"
      "H: H+1\n"
      "M: a(s(H*1.5707963))\n"
      "A: (1%H)*M\n"
      "W: w P $ A\n"
    },
    {
      WAVE_TABLE_SAW_DOWN, "saw-down",
      "/ Bandlimited sawtooth: rising ramp -1 up to 1\n"
      "P: ~4096\n"
      "H: !64\n"
      "H: H+1\n"
      "A: 1%H\n"
      "W: w 0 - P $ A\n"
    },
    {
      WAVE_TABLE_SAW_UP, "saw-up",
      "/ Bandlimited sawtooth: falling ramp 1 down to -1\n"
      "P: ~4096\n"
      "H: !64\n"
      "H: H+1\n"
      "A: 1%H\n"
      "W: w P $ A\n"
    },
    {
      WAVE_TABLE_TRI, "triangle",
      "/ Bandlimited triangle wave: alternating odd harmonics 1/h^2\n"
      "P: ~4096\n"
      "H: !64\n"
      "H: H+1\n"
      "M: s(H*1.5707963)\n"
      "A: (1%(H*H))*M\n"
      "W: w P $ A\n"
    },
    { 0, NULL, NULL }
  };

  for (int idx = 0; base_waves[idx].name != NULL; idx++) {
    int w = base_waves[idx].slot;
    synth_eval_and_register_ks_wave(w, base_waves[idx].name, base_waves[idx].ks_code, 0);
    if (sw.data[w]) {
      sw.rate[w] = MAIN_SAMPLE_RATE;
    }
  }
#endif
}

void wave_table_init(int flag) {
  (void)flag;

  for (int i = 0 ; i < synth_config.wave_table_max; i++) {
    sw.data[i] = NULL;
    sw.size[i] = 0;
    sw.is_heap[i] = 0;
    sw.direction[i] = 0;
    sw.readonly[i] = 0;
    sw.refcount[i] = 0;
    if (sw.mip_levels) sw.mip_levels[i] = 0;
    for (int lvl = 0; lvl < WAVETABLE_MIP_LEVELS; lvl++) {
      if (sw.mip_data[lvl]) sw.mip_data[lvl][i] = NULL;
    }
    if (sw.data_right) sw.data_right[i] = NULL;
    if (sw.channels) sw.channels[i] = 1;
    if (sw.eval_ms) sw.eval_ms[i] = 0.0f;
    if (sw.source) sw.source[i] = NULL;
    if (sw.source_is_heap) sw.source_is_heap[i] = 0;
  }

  synth_init_ksynth_base_waves();

  uint64_t white_noise;
  audio_rng_init(&white_noise, 1);
  for (int w = WAVE_TABLE_SINE; w <= WAVE_TABLE_KRG2_CENTERED; w++) {
    if (wave_invalid(w)) continue;
    if (sw.data[w] != NULL) continue;
    if (w >= WAVE_TABLE_KRG1 && w <= WAVE_TABLE_KRG16) continue;
    if (w == WAVE_TABLE_KRG2_CENTERED) continue;
    int size = SIZE_SINE;
    char *name = "?";
    switch (w) {
      case WAVE_TABLE_SINE:  name = "sine"; break;
      case WAVE_TABLE_COSINE: name = "cosine"; break;
      case WAVE_TABLE_CENTERED_PULSE: name = "centered pulse"; break;
      case WAVE_TABLE_SQR:   name = "square"; break;
      case WAVE_TABLE_SAW_DOWN: name = "saw-down"; break;
      case WAVE_TABLE_SAW_UP: name = "saw-up"; break;
      case WAVE_TABLE_TRI:   name = "triangle"; break;
      case WAVE_TABLE_NOISE: name = "noise"; break;
      case WAVE_TABLE_NOISE_ALT: name = "noise-alt"; break; // not used, here for laziness in experiment
      case WAVE_TABLE_CAP_1: name = "input-1-left"; break;
      case WAVE_TABLE_CAP_2: name = "input-1-right"; break;
      case WAVE_TABLE_SILENCE: name = "silence"; break;
      case WAVE_TABLE_UNITY: name = "unity"; break;
      case WAVE_TABLE_IMPULSE: name = "impulse"; break;
      case WAVE_TABLE_METRO: name = "metro"; break;
      case WAVE_TABLE_DELAY_TAP: name = "delay-tap"; break;
      case WAVE_TABLE_CONTROL: name = "control"; break;
      case WAVE_TABLE_KRG17: name = "krg-17"; size = 2048; break;
      case WAVE_TABLE_KRG18: name = "krg-18"; size = 2048; break;
      case WAVE_TABLE_KRG19: name = "krg-19"; size = 2048; break;
      case WAVE_TABLE_KRG20: name = "krg-20"; size = 2048; break;
      case WAVE_TABLE_KRG21: name = "krg-21"; size = 2048; break;
      case WAVE_TABLE_KRG22: name = "krg-22"; size = 2048; break;
      case WAVE_TABLE_KRG23: name = "krg-23"; size = 2048; break;
      case WAVE_TABLE_KRG24: name = "krg-24"; size = 2048; break;
      case WAVE_TABLE_KRG25: name = "krg-25"; size = 2048; break;
      case WAVE_TABLE_KRG26: name = "krg-26"; size = 2048; break;
      case WAVE_TABLE_KRG27: name = "krg-27"; size = 2048; break;
      case WAVE_TABLE_KRG28: name = "krg-28"; size = 2048; break;
      case WAVE_TABLE_KRG29: name = "krg-29"; size = 2048; break;
      case WAVE_TABLE_KRG30: name = "krg-30"; size = 2048; break;
      case WAVE_TABLE_KRG31: name = "krg-31"; size = 2048; break;
      case WAVE_TABLE_KRG32: name = "krg-32"; size = 2048; break;
      default: name = "?"; break;
    }
    strncpy(sw.name[w], name, WAVE_NAME_MAX);
    if ((w >= WAVE_TABLE_CAP_1 && w <= WAVE_TABLE_CONTROL) || w == WAVE_TABLE_NOISE || w == WAVE_TABLE_NOISE_ALT) {
      sw.data[w] = NULL;
      sw.size[w] = 0;
      sw.rate[w] = MAIN_SAMPLE_RATE;
      sw.readonly[w] = 1;
      continue;
    }
    sw.data[w] = (float *)malloc(size * sizeof(float));
    if (!sw.data[w]) {
      sw.size[w] = 0;
      sw.is_heap[w] = 0;
      continue;
    }
    sw.is_heap[w] = 1;
    sw.size[w] = size;
    sw.rate[w] = MAIN_SAMPLE_RATE;
    sw.one_shot[w] = 0;
    sw.loop_start[w] = 0;
    sw.loop_end[w] = size-1;
    sw.readonly[w] = 1;
    int off = 0;
    float phase = 0;
    float delta = 1.0f / (float)size;
    while (phase < 1.0f) {
      float sine = sinf(2.0f * (float) M_PI * phase);
      float f;
      switch (w) {
        case WAVE_TABLE_SINE: f = sine; break;
        case WAVE_TABLE_COSINE: f = -cosf(2.0f * (float) M_PI * phase); break;
        case WAVE_TABLE_CENTERED_PULSE: f = (phase > 0.25f && phase < 0.75f) ? 1.0f : -1.0f; break;
        case WAVE_TABLE_SQR: f = (phase < 0.5) ? 1.0f : -1.0f; break;
        case WAVE_TABLE_SAW_DOWN: f = 2.0f * phase - 1.0f; break;
        case WAVE_TABLE_SAW_UP: f = 1.0f - 2.0f * phase; break;
        case WAVE_TABLE_TRI: f = (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase); break;
        case WAVE_TABLE_NOISE: f = audio_rng_float(&white_noise); break;
        case WAVE_TABLE_NOISE_ALT: f = audio_rng_float(&white_noise); break;
        case WAVE_TABLE_KRG17: f = W17[off]; break;
        case WAVE_TABLE_KRG18: f = W18[off]; break;
        case WAVE_TABLE_KRG19: f = W19[off]; break;
        case WAVE_TABLE_KRG20: f = W20[off]; break;
        case WAVE_TABLE_KRG21: f = W21[off]; break;
        case WAVE_TABLE_KRG22: f = W22[off]; break;
        case WAVE_TABLE_KRG23: f = W23[off]; break;
        case WAVE_TABLE_KRG24: f = W24[off]; break;
        case WAVE_TABLE_KRG25: f = W25[off]; break;
        case WAVE_TABLE_KRG26: f = W26[off]; break;
        case WAVE_TABLE_KRG27: f = W27[off]; break;
        case WAVE_TABLE_KRG28: f = W28[off]; break;
        case WAVE_TABLE_KRG29: f = W29[off]; break;
        case WAVE_TABLE_KRG30: f = W30[off]; break;
        case WAVE_TABLE_KRG31: f = W31[off]; break;
        case WAVE_TABLE_KRG32: f = W32[off]; break;
        default: f = 0; break;
      }
      sw.data[w][off++] = f;
      phase += delta;
    }
  }

  synth_init_ksynth_dwg();
  synth_init_centered_krg2();
  synth_init_ksynth_esq1();
  synth_init_ksynth_drums();

  for (int w = WAVE_TABLE_SINE; w <= WAVE_TABLE_KRG32; w++) {
    if (wave_invalid(w)) continue;
    if (w == WAVE_TABLE_NOISE || w == WAVE_TABLE_NOISE_ALT) continue;
    if (w >= WAVE_TABLE_CAP_1 && w <= WAVE_TABLE_CONTROL) continue;
    if (sw.data[w] && sw.size[w] > 0 && (!sw.mip_levels || sw.mip_levels[w] <= 0)) {
      wave_generate_mipmaps_fft(w);
    }
  }
}


void wave_free_one(int i) {
  if (wave_invalid(i)) return;
  if (sw.mip_levels && sw.mip_levels[i] > 0) {
    for (int lvl = 0; lvl < WAVETABLE_MIP_LEVELS; lvl++) {
      if (sw.mip_data[lvl] && sw.mip_data[lvl][i]) {
        free(sw.mip_data[lvl][i]);
        sw.mip_data[lvl][i] = NULL;
      }
    }
    sw.mip_levels[i] = 0;
  }
  if (sw.data[i]) {
    if (sw.is_heap[i]) {
      free(sw.data[i]);
    }
    sw.data[i] = NULL;
    sw.size[i] = 0;
    sw.refcount[i] = 0;
  }
  if (sw.data_right && sw.data_right[i]) {
    if (sw.is_heap[i]) {
      free(sw.data_right[i]);
    }
    sw.data_right[i] = NULL;
  }
  if (sw.channels) sw.channels[i] = 1;
  if (sw.cues && sw.cues[i]) {
    free(sw.cues[i]);
    sw.cues[i] = NULL;
  }
  if (sw.num_cues) sw.num_cues[i] = 0;
}

void wave_free(void) {
  for (int i = 0; i < synth_config.wave_table_max; i++) wave_free_one(i);
}
