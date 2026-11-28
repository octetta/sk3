#include "skode-internal.h"
#include "synth-alloc.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int skode_exec_slashmd(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  int x = 0;
  if (argc == 0) x = skred_midi_debug_get() ? 0 : 1;
  else if (!skode_double_to_int(arg[0], &x)) return 0;
  skred_midi_debug_set(x);
  ctx->printf(ctx, "# midi debug %s\n", skred_midi_debug_get() ? "on" : "off");
  return 0;
}

int skode_exec_wait(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)s;
  int x = 0;
  if (argc > 0 && skode_double_to_int(arg[0], &x) && x >= 0) {
    sk_sleep(x);
  }
  return 0;
}

int skode_exec_clr(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)arg; (void)argc;
  ands_arg_clear(s);
  return 1;
}

int skode_exec_drop(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)arg; (void)argc;
  ands_arg_drop(s);
  return 1;
}

int skode_exec_dup(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)arg; (void)argc;
  ands_arg_dup(s);
  return 1;
}

int skode_exec_over(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)arg; (void)argc;
  ands_arg_over(s);
  return 1;
}

int skode_exec_rot(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)arg; (void)argc;
  ands_arg_rot(s);
  return 1;
}

int skode_exec_swap(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)arg; (void)argc;
  ands_arg_swap(s);
  return 1;
}

int skode_exec_I(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)s; (void)arg; (void)argc;
  return 0;
}

int skode_exec_udp(skode_t *ctx, ands_t *s, const double *arg, int argc) {
#ifdef UDP
  (void)s;
  if (argc) {
    ctx->printf(ctx, "# udp [%d] %d/%d\n", ctx->which, ctx->ip, ctx->port);
  }
#else
  (void)ctx; (void)s; (void)arg; (void)argc;
#endif
  return 0;
}

int skode_exec_puts(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)arg; (void)argc; (void)s;
  const char *str = ands_string(ctx->parse);
  if (str && str[0] != '\0') {
    ctx->printf(ctx, "# %s\n", str);
  } else {
    ctx->printf(ctx, "#\n");
  }
  return 0;
}

int skode_exec_log(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  int x = 0;
  if (argc > 0 && skode_double_to_int(arg[0], &x)) {
    ctx->log_enable = x ? 1 : 0;
  }
  return 0;
}

int skode_exec_GS_gt(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  if (!ands_string_fresh(ctx->parse) || !ands_string(ctx->parse)[0])
    ctx->printf(ctx, "# GS> requires [filename.zip]\n");
  else
    (void)skode_session_save(ctx, ands_string(ctx->parse));
  return 0;
}

int skode_exec_GS_lt(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  if (!ands_string_fresh(ctx->parse) || !ands_string(ctx->parse)[0])
    ctx->printf(ctx, "# GS< requires [filename.zip]\n");
  else
    (void)skode_session_load(ctx, ands_string(ctx->parse));
  return 0;
}

int skode_exec_x(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (argc) {
    const char *source = ands_string(ctx->parse);
    event_program_t program;
    int source_only = source[0] == '\0' || source[0] == '-';
    char hint[16] = "";
    skode_compile_result_t result = source_only ?
      SKODE_COMPILE_OK :
      skode_compile_program_describe(source, &program, hint, sizeof(hint));
    if (result == SKODE_COMPILE_OK) {
      for (int i = 0; i < argc; i++) {
        int x = 0;
        int x_valid = skode_double_to_int(arg[i], &x);
        if (isnan(arg[i]) || !x_valid || x < 0) {
          ctx->step++;
          x = ctx->step;
        } else {
          ctx->step = x;
        }
        if (x >= 0) {
          seq_step_set(ctx->pattern, ctx->step, source,
            source_only ? NULL : &program);
        }
      }
    } else if (result == SKODE_COMPILE_IMMEDIATE_ONLY) {
      if (hint[0])
        ctx->printf(ctx,
          "# pattern %d step %d: '%s' is immediate-only and cannot be compiled into step [%s]\n"
          "# use 'ce N' in the step and bind N with '/ceb 4 N'\n", ctx->pattern, ctx->step, hint, source);
      else
        ctx->printf(ctx,
          "# pattern %d step %d: step [%s] contains an immediate-only word\n"
          "# use 'ce N' in the step and bind N with '/ceb 4 N'\n", ctx->pattern, ctx->step, source);
      return -1;
    } else {
      ctx->printf(ctx, "# pattern %d step %d: step could not be compiled (%d): [%s]\n",
        ctx->pattern, ctx->step, result, source);
      return -1;
    }
  }
  return 0;
}

int skode_exec_x_data(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!ctx) return -1;
  int target_pattern = ctx->pattern;
  if (argc > 0) {
    int p = 0;
    if (skode_double_to_int(arg[0], &p) && p >= 0 && p < PATTERNS_MAX) {
      target_pattern = p;
    }
  }

  double *data = ands_data(ctx->parse);
  int data_len = ands_data_len(ctx->parse);
  if (!data || data_len <= 0) return 0;

  const char *source = ands_string(ctx->parse);
  event_program_t program;
  int source_only = source[0] == '\0' || source[0] == '-';
  char hint[16] = "";
  skode_compile_result_t result = source_only ?
    SKODE_COMPILE_OK :
    skode_compile_program_describe(source, &program, hint, sizeof(hint));
  if (result == SKODE_COMPILE_OK) {
    for (int i = 0; i < data_len; i++) {
      int step = 0;
      int step_valid = skode_double_to_int(data[i], &step);
      if (isnan(data[i]) || !step_valid || step < 0) {
        ctx->step++;
        step = ctx->step;
      } else {
        ctx->step = step;
      }
      if (step >= 0) {
        seq_step_set(target_pattern, step, source,
          source_only ? NULL : &program);
      }
    }
  } else if (result == SKODE_COMPILE_IMMEDIATE_ONLY) {
    if (hint[0])
      ctx->printf(ctx,
        "# pattern %d: '%s' is immediate-only and cannot be compiled into step [%s]\n"
        "# use 'ce N' in the step and bind N with '/ceb 4 N'\n", target_pattern, hint, source);
    else
      ctx->printf(ctx,
        "# pattern %d: step [%s] contains an immediate-only word\n"
        "# use 'ce N' in the step and bind N with '/ceb 4 N'\n", target_pattern, source);
    return -1;
  } else {
    ctx->printf(ctx, "# pattern %d: step could not be compiled (%d): [%s]\n",
      target_pattern, result, source);
    return -1;
  }
  return 0;
}

int skode_exec_x_range(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!ctx || argc < 2) return -1;
  int start = 0, end = 0;
  if (!skode_double_to_int(arg[0], &start) || !skode_double_to_int(arg[1], &end)) return -1;
  int target_pattern = ctx->pattern;
  if (argc > 2) {
    int p = 0;
    if (skode_double_to_int(arg[2], &p) && p >= 0 && p < PATTERNS_MAX) {
      target_pattern = p;
    }
  }
  const char *source = ands_string(ctx->parse);
  if (!source || !source[0]) return 0;
  event_program_t program;
  int source_only = source[0] == '\0' || source[0] == '-';
  char hint[16] = "";
  skode_compile_result_t result = source_only ?
    SKODE_COMPILE_OK :
    skode_compile_program_describe(source, &program, hint, sizeof(hint));
  if (result != SKODE_COMPILE_OK) return -1;

  int step_dir = (start <= end) ? 1 : -1;
  for (int step = start; ; step += step_dir) {
    if (step >= 0) {
      seq_step_set(target_pattern, step, source, source_only ? NULL : &program);
    }
    if (step == end) break;
  }
  return 0;
}

int skode_exec_slashm_(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  synth_voice_bench(ctx->voice);
  return 0;
}

int skode_exec_slashq(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  ctx->quit = -1;
  return 0;
}

int skode_exec_slashf(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  int x = 0;
  if (argc > 0 && skode_double_to_int(arg[0], &x)) {
    ctx->flag = x;
  } else {
    ctx->printf(ctx, "# /f%d\n", ctx->flag);
  }
  return 0;
}

int skode_exec_slashff(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!argc) return 0;
  int index;
  if (!skode_double_to_int(arg[0], &index) ||
      index < 0 || index >= SKRED_FOREIGN_FUNCTION_MAX) return 0;
  (void)skode_foreign_function(ctx, index, (double *)arg + 1, argc - 1);
  return 0;
}

int skode_exec_slasht(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  int x = 0;
  if (argc == 0) x = (ctx->trace) ? 0 : 1;
  else if (!skode_double_to_int(arg[0], &x)) return 0;
  ctx->trace = x;
  ands_trace_set(s, x > 1);
  return 0;
}

int skode_exec_slashv(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  int x = 0;
  if (argc == 0) x = (ctx->verbose) ? 0 : 1;
  else if (!skode_double_to_int(arg[0], &x)) return 0;
  ctx->verbose = x;
  return 0;
}

int skode_exec_slashth_q(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  ctx->printf(ctx, "%s", skred_thread_status());
  return 0;
}

int skode_exec_slashth_bang(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  skred_performance_reset();
  ctx->printf(ctx, "# performance counters reset\n");
  return 0;
}

int skode_exec_slashs(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  int x = 0;
  if (argc == 0) {
    system_show(ctx);
  } else {
    if (!skode_double_to_int(arg[0], &x)) x = 0;
    switch (x) {
      default:
      case 0: system_show(ctx); break;
      case 2: audio_show(ctx); break;
      case 3: ctx->printf(ctx, "%s", synth_stats()); break;
      case 5: skode_show(ctx); break;
      case 7:
        simple_mutex_lock(&skode_extra_mutex);
        for (int i=0; i<STRING_BUF_IDX_MAX; i++) {
          if (strlen(EXTRA_PTR(i)))
            ctx->printf(ctx, "# [%s] e>%d\n", EXTRA_PTR(i), i);
        }
        simple_mutex_unlock(&skode_extra_mutex);
        break;
      case 1: show_threads(ctx); break;
      case 4: show_stats(ctx); break;
      case 6: ctx->printf(ctx, "%s", seq_stats()); break;
      case 8: synth_mem_report(ctx); break;
    }
  }
  return 0;
}

int skode_exec_slashh(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  skode_help(ctx, (double *)arg, argc);
  return 0;
}

int skode_exec_slashl(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (argc) {
    int x = 0;
    int verbose = 0;
    if (!skode_double_to_int(arg[0], &x)) return 0;
    if (argc > 1) skode_double_to_int(arg[1], &verbose);
    return skode_load(ctx, ctx->voice, x, verbose);
  }
  return 0;
}

int skode_exec_slashls(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (strlen(ands_string(ctx->parse))) {
    int verbose = 0;
    if (argc > 0) skode_double_to_int(arg[0], &verbose);
    return skode_load_name(ctx, ands_string(ctx->parse), verbose);
  } else {
    ctx->printf(ctx, "# /ls requires [filename]\n");
    return SK_ERR_INVALID_PARAM;
  }
}

int skode_exec_gt_u(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  const char *fmt = ands_string(ctx->parse);
  int a_idx = 0;
  char fallback_fmt[256] = "";
  
  if (strlen(fmt) == 0 && argc > 0) {
      int macro_idx = (int)arg[argc - 1];
      if (macro_idx >= 0 && macro_idx < 128 && skode_extra_copy(macro_idx, fallback_fmt, sizeof(fallback_fmt)) == 0) {
          fmt = fallback_fmt;
          argc--; // Consume the last argument so it isn't formatted
      }
  }

  if (fmt && strlen(fmt) > 0) {
      char buf[256];
      buf[0] = '\0';
      char *out = buf;
      int remaining = sizeof(buf) - 1;
      
      while (*fmt && remaining > 0) {
          if (*fmt == '%' && *(fmt+1) == 'g') {
              int n = snprintf(out, remaining, "%g", a_idx < argc ? arg[a_idx] : 0.0);
              if (n > 0) { out += n; remaining -= n; }
              a_idx++;
              fmt += 2;
          } else if (*fmt == '%' && *(fmt+1) == 'd') {
              int n = snprintf(out, remaining, "%d", a_idx < argc ? (int)arg[a_idx] : 0);
              if (n > 0) { out += n; remaining -= n; }
              a_idx++;
              fmt += 2;
          } else {
              *out++ = *fmt++;
              remaining--;
          }
      }
      *out = '\0';
#ifdef UDP
      skred_udp_events_emit(buf);
#else
      (void)buf;
#endif
  }
  return 1;
}

int skode_exec_slashws(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (strlen(ands_string(ctx->parse))) {
    char *file_name = ands_string(ctx->parse);
    int wave_slot = EXT_SAMPLE_000;
    int ch = -1;
    if (argc >= 1) {
      if (!skode_double_to_int(arg[0], &wave_slot)) return 0;
      if (argc > 1) {
        if (!skode_double_to_int(arg[1], &ch)) ch = -1;
      }
    }
    wave_load_string(ctx, file_name, wave_slot, ch, 1);
  }
  return 0;
}

int skode_exec_slashw(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  int file_num = 0;
  int wave_slot = EXT_SAMPLE_000;
  int ch = -1;
  if (argc >= 2) {
    if (!skode_double_to_int(arg[0], &file_num) ||
        !skode_double_to_int(arg[1], &wave_slot)) return 0;
    if (argc > 2 && !skode_double_to_int(arg[2], &ch)) ch = -1;
  } else if (argc == 1) {
    if (!skode_double_to_int(arg[0], &file_num)) return 0;
    wave_slot = EXT_SAMPLE_000;
  }
  if (argc) wave_load(ctx, file_num, wave_slot, ch, 1);
  return 0;
}

int skode_exec_pctz(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  int verbose = 0;
  if (argc > 0) skode_double_to_int(arg[0], &verbose);
  if (strlen(ands_string(ctx->parse))) {
    if (skred_vfs_mount(ands_string(ctx->parse))) {
      ctx->printf(ctx, "# vfs %s\n", skred_vfs_status());
      void *data = NULL;
      size_t size = 0;
      char resolved[1024];
      if (skode_asset_read("main.sk", SKODE_ASSET_SKODE, &data, &size, resolved, sizeof(resolved))) {
        ctx->printf(ctx, "# auto-loading %s\n", resolved[0] ? resolved : "main.sk");
        skode_load_buffer(ctx, (const char *)data, size, resolved[0] ? resolved : "main.sk", verbose);
        skred_vfs_free_file(data);
      }
    } else {
      ctx->printf(ctx, "# cannot mount %s\n", ands_string(ctx->parse));
    }
  } else {
    ctx->printf(ctx, "# %%z requires [zip-or-directory]\n");
  }
  return 0;
}

int skode_exec_pctzu(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  skred_vfs_unmount();
  ctx->printf(ctx, "# vfs %s\n", skred_vfs_status());
  return 0;
}

int skode_exec_pctpwd(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  ctx->printf(ctx, "# vfs %s\n", skred_vfs_status());
  return 0;
}

int skode_exec_pctcat(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  if (strlen(ands_string(ctx->parse))) {
    void *data = NULL;
    size_t size = 0;
    char resolved[1024];
    if (skode_asset_read(ands_string(ctx->parse), SKODE_ASSET_ANY,
        &data, &size, resolved, sizeof(resolved))) {
      const char *text = (const char *)data;
      size_t pos = 0;
      while (pos < size) {
        char line[1024];
        size_t start = pos;
        size_t len;
        while (pos < size && text[pos] != '\n' && text[pos] != '\r') pos++;
        len = pos - start;
        while (pos < size && (text[pos] == '\n' || text[pos] == '\r')) pos++;
        if (len >= sizeof(line)) len = sizeof(line) - 1;
        memcpy(line, text + start, len);
        line[len] = '\0';
        for (size_t i = 0; i < len; i++) {
          if (!isprint((unsigned char)line[i]) && line[i] != '\t') {
            line[i] = '\0';
            break;
          }
        }
        ctx->printf(ctx, "%s\n", line);
      }
      skred_vfs_free_file(data);
    }
  }
  return 0;
}

int skode_exec_pctcd(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s; (void)arg; (void)argc;
  ctx->printf(ctx, "# [%s] %%cd\n", ands_string(ctx->parse));
  if (strlen(ands_string(ctx->parse))) {
    if (!skred_chdir(ands_string(ctx->parse)))
      ctx->printf(ctx, "# cannot cd %s\n", ands_string(ctx->parse));
  }
  ctx->printf(ctx, "# vfs %s\n", skred_vfs_status());
  return 0;
}

int skode_exec_pctls(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  int p = -1;
  int x = 0;
  if (argc && skode_double_to_int(arg[0], &x)) p = x;
  SkredDirent *entry;
  SkredDir *dp = skred_opendir(".");
  if (dp) {
    int index = -100;
    if (argc > 1) {
      index = (int)arg[1];
    }
    int count = 0;
    while ((entry = skred_readdir(dp))) {
      char *name = entry->d_name;
      int f = 0;
      switch (p) {
        default:
        case -1: f = 1; break;
        case 0: f = (strstr(name, ".sk") != NULL); break;
        case 1: f = (strstr(name, ".wav") != NULL); break;
        case 2: f = (strstr(name, ".mp3") != NULL); break;
        case 3: f = (strstr(name, ".ks") != NULL); break;
        case 4: f = (strstr(name, ".flac") != NULL); break;
        case 5: f = (strstr(name, ".zip") != NULL); break;
        case 6: f = (strstr(name, ".pnl") != NULL); break;
      }
      if (f) {
        if (index == -100) {
          ctx->printf(ctx,
            "# [%s%s] # %d\n",
            name, entry->is_directory ? "/" : "", count);
        }
        if (index == count) {
          ctx->printf(ctx, "# [%s%s] # %d\n",
            name, entry->is_directory ? "/" : "", index);
          ands_string_from_external(ctx->parse, name, strlen(name));
        }
        count++;
        if (index == -1) {
          if (rand() % count == 0) {
            ands_string_from_external(ctx->parse, name, strlen(name));
          }
        }
      }
    }
    skred_closedir(dp);
  }
  return 0;
}

int skode_exec_qtime(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)ctx; (void)arg; (void)argc;
  ands_return_push(s, (double)SAMPLE_COUNT_GET());
  return 0;
}

int skode_exec_qvoice(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)arg; (void)argc;
  ands_return_push(s, (double)ctx->voice);
  return 0;
}

int skode_exec_qpattern(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)arg; (void)argc;
  ands_return_push(s, (double)ctx->pattern);
  return 0;
}

int skode_exec_qstep(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)arg; (void)argc;
  ands_return_push(s, (double)ctx->step);
  return 0;
}

int skode_exec_qerr(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)arg; (void)argc;
  int err = ctx ? ctx->last_error : 0;
  ands_return_push(s, (double)err);
  return 0;
}

#ifdef _WIN32
#ifndef popen
#define popen _popen
#endif
#ifndef pclose
#define pclose _pclose
#endif
#endif

typedef struct {
  char code[16];
  char payload[1024];
  int used;
} skode_clip_slot_t;

static skode_clip_slot_t g_local_clips[16];
static int g_local_clip_idx = 0;

static void skode_gen_short_code(char *out, size_t sz) {
  const char chars[] = "abcdefghjkmnpqrstuvwxyz23456789";
  for (size_t i = 0; i < sz - 1; i++) {
    out[i] = chars[rand() % (sizeof(chars) - 1)];
  }
  out[sz - 1] = '\0';
}

int skode_exec_share(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!ctx) return -1;
  const char *payload = ands_string(ctx->parse);
  if (!payload || !payload[0]) {
    ctx->printf(ctx, "# /share: string buffer is empty. Usage: [your code] /share\n");
    return -1;
  }
  char code[16] = {0};
  if (argc > 0) {
    int num = 0;
    if (skode_double_to_int(arg[0], &num)) {
      snprintf(code, sizeof(code), "%d", num);
    }
  }
  if (!code[0]) {
    skode_gen_short_code(code, 5); // 4-char code
  }

  // Cache locally
  int slot = g_local_clip_idx;
  g_local_clip_idx = (g_local_clip_idx + 1) % 16;
  snprintf(g_local_clips[slot].code, sizeof(g_local_clips[slot].code), "%s", code);
  snprintf(g_local_clips[slot].payload, sizeof(g_local_clips[slot].payload), "%s", payload);
  g_local_clips[slot].used = 1;

#if !defined(__EMSCRIPTEN__)
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "curl -s -m 2 -d @- \"https://ntfy.sh/sk-%s\" > /dev/null 2>&1", code);
  FILE *fp = popen(cmd, "w");
  if (fp) {
    fwrite(payload, 1, strlen(payload), fp);
    pclose(fp);
  }
#endif

  ctx->printf(ctx, "# ====================================================\n");
  ctx->printf(ctx, "# Published to public clipboard! Code: %s\n", code);
  ctx->printf(ctx, "# Load in CLI/GUI with: [%s] /load\n", code);
  ctx->printf(ctx, "# Open in web: https://octetta.github.io/skred-web/?c=%s\n", code);
  ctx->printf(ctx, "# ====================================================\n");
  return 0;
}

int skode_exec_load(skode_t *ctx, ands_t *s, const double *arg, int argc) {
  (void)s;
  if (!ctx) return -1;
  char code[32] = {0};
  if (argc > 0) {
    int num = 0;
    if (skode_double_to_int(arg[0], &num)) {
      snprintf(code, sizeof(code), "%d", num);
    }
  }
  if (!code[0]) {
    const char *str = ands_string(ctx->parse);
    if (str && str[0]) {
      snprintf(code, sizeof(code), "%.31s", str);
    }
  }
  if (!code[0]) {
    ctx->printf(ctx, "# /load: code missing. Usage: [code] /load or 1234 /load\n");
    return -1;
  }

  // Check local cache first
  for (int i = 0; i < 16; i++) {
    if (g_local_clips[i].used && strcmp(g_local_clips[i].code, code) == 0) {
      ctx->printf(ctx, "# Loaded from local cache [%s]: %s\n", code, g_local_clips[i].payload);
      return skode_load_buffer(ctx, g_local_clips[i].payload, strlen(g_local_clips[i].payload), code, 0);
    }
  }

#if defined(__EMSCRIPTEN__)
  ctx->printf(ctx, "# /load: clipboard [%s] not found in local cache\n", code);
  return -1;
#else
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "curl -s -m 2 \"https://ntfy.sh/sk-%s/raw?poll=1\"", code);
  FILE *fp = popen(cmd, "r");
  if (!fp) {
    ctx->printf(ctx, "# /load: error invoking curl\n");
    return -1;
  }
  char buf[8192];
  size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
  pclose(fp);
  buf[n] = '\0';

  while (n > 0 && (buf[n - 1] == '\n' || buf[n - 1] == '\r' || buf[n - 1] == ' ')) {
    buf[--n] = '\0';
  }

  if (n == 0) {
    ctx->printf(ctx, "# /load: clipboard [%s] is empty or expired\n", code);
    return -1;
  }
  if (strncmp(buf, "{\"code\":", 8) == 0 || strncmp(buf, "<!DOCTYPE", 9) == 0 || strncmp(buf, "<html", 5) == 0) {
    ctx->printf(ctx, "# /load: clipboard service rate limited or unavailable\n");
    return -1;
  }

  ctx->printf(ctx, "# Loaded from clipboard [%s]: %s\n", code, buf);
  return skode_load_buffer(ctx, buf, (size_t)n, code, 0);
#endif
}

void skode_register_words_system(skode_vocab_t *vocab) {
  (void)vocab;
}
