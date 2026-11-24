#include "skode-internal.h"
#include <time.h>
#include <string.h>
#include <stdlib.h>

#ifdef KSYNTH
ks_ctx *skode_ks_ctx(skode_t *ctx) {
  if (!ctx) return NULL;
  if (!ctx->ks) {
    ctx->ks = ks_create(16 * 1024 * 1024, 10000000, (double)MAIN_SAMPLE_RATE);
    if (!ctx->ks) {
      ctx->printf(ctx, "# ksynth context allocation failed\n");
    } else {
      bind_scalar(ctx->ks, 'S', (double)MAIN_SAMPLE_RATE);
      bind_scalar(ctx->ks, 'R', (double)MAIN_SAMPLE_RATE);
    }
  }
  return ctx->ks;
}

void skode_ks_result_clear(skode_t *ctx) {
  if (!ctx || !ctx->ks_result) return;
  if (ctx->ks) k_free(ctx->ks, (K)ctx->ks_result);
  ctx->ks_result = NULL;
}

int skode_ks_eval(skode_t *ctx, char *cmd, int len) {
  if (!ctx || !cmd || len < 0) return 0;
  ks_ctx *ks = skode_ks_ctx(ctx);
  if (!ks) return 0;

  if (len >= 2 && strncmp(cmd, "\\X", 2) == 0) {
    skode_ks_result_clear(ctx);
    ks_clear_vars(ks);
    return 1;
  }



  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  simple_mutex_lock(&skode_ks_eval_mutex);
  K next = ks_eval(ks, cmd, (size_t)len);
  simple_mutex_unlock(&skode_ks_eval_mutex);

  clock_gettime(CLOCK_MONOTONIC, &t1);
  ctx->ks_last_eval_ms = (float)((t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) * 1e-6);

  if (ks->last_status != KS_OK) {
    ctx->printf(ctx, "# ksynth status %d error %s\n",
                ks->last_status, ks_strerror(ks->last_status));
  }
  skode_ks_result_clear(ctx);
  ctx->ks_result = next;

  if (next != NULL && ctx->parse) {
    K m = k_get_var_str(ks, "META");
    if (!m) m = k_get_var_str(ks, "M");
    if (m && k_is_string(m)) {
      const char *s = (const char *)m->f;
      if (s) {
        ands_string_from_external(ctx->parse, (char *)s, (int)strlen(s));
      }
    }
    K k_cues = k_get_var_str(ks, "CUES");
    if (!k_cues) k_cues = k_get_var_str(ks, "CUE");
    if (k_cues) {
      char cue_buf[512];
      cue_buf[0] = '\0';
      if (k_is_string(k_cues)) {
        const char *cs = (const char *)k_cues->f;
        if (cs && cs[0]) {
          snprintf(cue_buf, sizeof(cue_buf), " %s", cs);
        }
      } else if (k_cues->n > 0) {
        size_t cpos = 0;
        for (int i = 0; i < (int)k_cues->n && cpos < sizeof(cue_buf) - 32; i++) {
          int offset = (int)k_cues->f[i];
          if (offset < 0) offset = 0;
          cpos += snprintf(cue_buf + cpos, sizeof(cue_buf) - cpos, " cue:%d,cue%d", offset, i);
        }
      }
      if (cue_buf[0]) {
        const char *cur = ands_string(ctx->parse);
        if (cur && cur[0]) {
          char merged[1024];
          snprintf(merged, sizeof(merged), "%s%s", cur, cue_buf);
          ands_string_from_external(ctx->parse, merged, (int)strlen(merged));
        } else {
          const char *trimmed = cue_buf + (cue_buf[0] == ' ' ? 1 : 0);
          ands_string_from_external(ctx->parse, (char *)trimmed, (int)strlen(trimmed));
        }
      }
    }
  }

  return next != NULL;
}

int skode_ks_result_to_data(skode_t *ctx) {
  if (!ctx || !ctx->ks_result || k_is_func((K)ctx->ks_result)) return 0;
  K result = (K)ctx->ks_result;
  if (k_is_string(result)) {
    const char *s = (const char *)result->f;
    if (s && ctx->parse) {
      ands_string_from_external(ctx->parse, (char *)s, (int)strlen(s));
    }
    return 1;
  }
  size_t len = (size_t)result->n;
  if (len) {
    int dlen = ands_data_cap(ctx->parse);
    if ((int)len > dlen) {
      //ctx->printf(ctx, "# resize %d -> %d\n", dlen, (int)len);
      ands_data_resize(ctx->parse, (int)len);
    }
    double *g = ands_data(ctx->parse);
    for (int i=0; i<(int)len; i++) g[i] = result->f[i];
    ands_data_len_set(ctx->parse, (int)len);
  }
  return len > 0;
}

int skode_ks_bind_values(skode_t *ctx, int variable,
                         const double *values, size_t len) {
  if (variable < 0 || variable >= 26) {
    ctx->printf(ctx, "# invalid ksynth variable %d (expected 0..25)\n",
                variable);
    return 0;
  }
  if (!values || len == 0) {
    ctx->printf(ctx, "# no data to bind\n");
    return 0;
  }
  if (len > 1000000) {
    ctx->printf(ctx, "# ksynth vector too large: %zu\n", len);
    return 0;
  }
  ks_ctx *ks = skode_ks_ctx(ctx);
  if (!ks) return 0;
  ks_status status = ks_bind_vector(ks, (char)('A' + variable), values, len);
  if (status != KS_OK) {
    ctx->printf(ctx, "# bind %c status %d error %s\n",
                (char)('A' + variable), status, ks_strerror(status));
    return 0;
  }
  return 1;
}

int ksynth_loader(skode_t *ctx, const char *text, size_t text_len,
    const char *label, int verbose) {
  if (!ctx || !text || text_len == 0) return 0;

  char diag[256];
  if (!skode_is_text(text, text_len, diag, sizeof(diag))) {
    ctx->printf(ctx, "# cannot load %s: %s\n",
                label ? label : "(buffer)", diag);
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "%s: %s",
                    label ? label : "(buffer)", diag);
    return SK_ERR_INVALID_PARAM;
  }

  struct timespec t0, t1;
  clock_gettime(CLOCK_MONOTONIC, &t0);

  if (ctx->ks_last_source) {
    free(ctx->ks_last_source);
    ctx->ks_last_source = NULL;
  }
  ctx->ks_last_source = (char *)malloc(text_len + 1);
  if (ctx->ks_last_source) {
    memcpy(ctx->ks_last_source, text, text_len);
    ctx->ks_last_source[text_len] = '\0';
  }

  size_t pos = 0;
  (void)label;
  while (pos < text_len) {
    char line[1024];
    size_t start = pos;
    size_t len;
    while (pos < text_len && text[pos] != '\n' && text[pos] != '\r') pos++;
    len = pos - start;
    while (pos < text_len && (text[pos] == '\n' || text[pos] == '\r')) pos++;
    if (len >= sizeof(line)) len = sizeof(line) - 1;
    memcpy(line, text + start, len);
    line[len] = '\0';
    if (verbose) ctx->printf(ctx, "  %s\n", line);
    if (len > 0) skode_ks_eval(ctx, line, (int)len);
  }

  clock_gettime(CLOCK_MONOTONIC, &t1);
  ctx->ks_last_eval_ms = (float)((t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) * 1e-6);
  return 0;
}

int ksynth_load_name(skode_t *ctx, char *file, int verbose) {
  void *data = NULL;
  size_t size = 0;
  char resolved[1024];
  if (!skode_asset_read(file, SKODE_ASSET_KSYNTH, &data, &size,
      resolved, sizeof(resolved))) {
    ctx->printf(ctx, "# cannot load %s\n", file ? file : "(null)");
    skode_set_error(ctx, SK_ERR_NOT_FOUND, "cannot load %s", file ? file : "(null)");
    return SK_ERR_NOT_FOUND;
  }
  int r = ksynth_loader(ctx, (const char *)data, size, resolved, verbose);
  skred_vfs_free_file(data);
  return r;
}

int ksynth_load(skode_t *ctx, int n, int verbose) {
  char file[1024];
  char resolved[1024];
  void *data = NULL;
  size_t size = 0;
  sprintf(file, "%d.ks", n);
  if (!skode_asset_read(file, SKODE_ASSET_KSYNTH, &data, &size,
      resolved, sizeof(resolved))) {
    ctx->printf(ctx, "# cannot load %d.ks or ks/%d.ks\n", n, n);
    skode_set_error(ctx, SK_ERR_NOT_FOUND, "cannot load %d.ks", n);
    return SK_ERR_NOT_FOUND;
  }
  int r = ksynth_loader(ctx, (const char *)data, size, resolved, verbose);
  skred_vfs_free_file(data);
  return r;
}
#endif


