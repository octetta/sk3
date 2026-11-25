#include "skode-internal.h"
#include "sk_error.h"
#include "sk_meta.h"

#include "miniaudio.h"

float *mw_free(float *f) {
    if (f) free(f);
    return NULL;
}

static uint32_t mw_le_u32(const unsigned char *p) {
  return ((uint32_t)p[0]) |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

int mw_get_smpl_loop_mem(const void *data, size_t data_size, int frames,
                         mw_smpl_loop_t *loop) {
  const unsigned char *bytes = (const unsigned char *)data;
  size_t pos = 12;

  if (!loop) return 0;
  memset(loop, 0, sizeof(*loop));
  if (!bytes || data_size < 12 || frames <= 0) return 0;
  if (memcmp(bytes, "RIFF", 4) != 0 || memcmp(bytes + 8, "WAVE", 4) != 0)
    return 0;

  while (pos + 8 <= data_size) {
    const unsigned char *chunk = bytes + pos;
    uint32_t chunk_size = mw_le_u32(chunk + 4);
    size_t data_start = pos + 8;
    size_t next_chunk = data_start + (size_t)chunk_size + (size_t)(chunk_size & 1u);

    if (next_chunk < data_start || data_start > data_size) break;

    if (memcmp(chunk, "smpl", 4) == 0) {
      const unsigned char *header;
      const unsigned char *sample_loop;
      uint32_t sample_loop_count;
      uint32_t type;
      uint32_t start;
      uint32_t end;
      uint32_t play_count;
      int end_exclusive;

      if (chunk_size < 60 || data_start + 60 > data_size) break;
      header = bytes + data_start;
      sample_loop_count = mw_le_u32(header + 28);
      if (sample_loop_count == 0) break;
      sample_loop = header + 36;

      type = mw_le_u32(sample_loop + 4);
      start = mw_le_u32(sample_loop + 8);
      end = mw_le_u32(sample_loop + 12);
      play_count = mw_le_u32(sample_loop + 20);

      if (start >= (uint32_t)frames || end < start) break;
      end_exclusive = end >= (uint32_t)frames ? frames : (int)end + 1;
      if (end_exclusive <= (int)start) break;

      loop->found = 1;
      loop->start = (int)start;
      loop->end = end_exclusive;
      loop->type = (int)type;
      loop->play_count = (int)play_count;
      return 1;
    }

    if (next_chunk <= pos) break;
    pos = next_chunk;
  }

  return 0;
}

static float *mw_decode(ma_decoder *decoder, const char *label,
                        int *frames_out, wav_t *w, int ch,
                        char *out, int len, float *pSamples,
                        ma_uint64 frameCount) {
  ma_uint64 j = 0;
  if (out != NULL && len > 0) {
    snprintf(out, len, "Loaded %llu frames / %d channels / %d sample rate\n",
      frameCount,
      decoder->outputChannels,
      decoder->outputSampleRate);
  }
  w->data_right = NULL;
  w->data_channels = NULL;
  (void)label;
  w->SamplesRate = decoder->outputSampleRate;
  *frames_out = (int)frameCount;

  if (ch == -1 && decoder->outputChannels > 2 && (decoder->outputChannels % 2 == 0)) {
    float **planar = (float **)malloc(decoder->outputChannels * sizeof(float *));
    if (planar) {
      int ok = 1;
      for (ma_uint32 c = 0; c < decoder->outputChannels; c++) {
        planar[c] = (float *)malloc(frameCount * sizeof(float));
        if (!planar[c]) ok = 0;
      }
      if (ok) {
        for (ma_uint64 i = 0; i < frameCount; i++) {
          for (ma_uint32 c = 0; c < decoder->outputChannels; c++) {
            planar[c][i] = pSamples[i * decoder->outputChannels + c];
          }
        }
        w->data_channels = planar;
        w->data_right = planar[1];
        w->Channels = decoder->outputChannels;
        free(pSamples);
        return planar[0];
      } else {
        for (ma_uint32 c = 0; c < decoder->outputChannels; c++) {
          if (planar[c]) free(planar[c]);
        }
        free(planar);
      }
    }
  }

  if (ch == -1 && decoder->outputChannels == 2) {
    float *pRight = (float *)malloc(frameCount * sizeof(float));
    if (pRight) {
      for (ma_uint64 i = 0; i < frameCount; i++) {
        pSamples[i] = pSamples[i * 2];
        pRight[i] = pSamples[i * 2 + 1];
      }
      w->data_right = pRight;
      w->Channels = 2;
      return pSamples;
    }
  }

  if (ch >= (int)decoder->outputChannels) ch = (int)decoder->outputChannels - 1;
  w->Channels = decoder->outputChannels;
  for (ma_uint64 i = 0; i < frameCount * decoder->outputChannels; i += decoder->outputChannels) {
    if (ch == -1) {
      float a = 0;
      for (ma_uint32 k = 0; k < decoder->outputChannels; k++) a += pSamples[i + k];
      pSamples[j] = a / (float)decoder->outputChannels;
    } else {
      pSamples[j] = pSamples[i + ch];
    }
    j++;
  }
  return pSamples;
}

float *mw_get_str(char *filename, int *frames_out, wav_t *w, int ch, char *out, int len) {
  ma_result result;
  ma_decoder decoder;
  ma_decoder_config decoderConfig;

  // We want interleaved float32 output
  decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);

  result = ma_decoder_init_file(filename, &decoderConfig, &decoder);
  if (result != MA_SUCCESS) {
    if (out != NULL && len > 0) {
      snprintf(out, len, "Could not load file: %s\n", filename);
    }
    *frames_out = 0;
    return NULL;
  }
  float* pSamples = NULL;
  ma_uint64 frameCount = 0;
  result = ma_decode_file(filename, &decoderConfig, &frameCount, (void**)&pSamples);
  if (result == MA_SUCCESS) {
    // pSamples is now your interleaved float32 array
    // frameCount * channels = total number of floats
    if (out != NULL && len > 0) {
      snprintf(out, len, "Loaded %llu frames / %d channels / %d sample rate\n",
        frameCount,
        decoder.outputChannels,
        decoder.outputSampleRate);
    }
  } else {
    *frames_out = 0;
    return NULL;
  }
  pSamples = mw_decode(&decoder, filename, frames_out, w, ch, out, len,
                       pSamples, frameCount);
  ma_decoder_uninit(&decoder);
  return pSamples;
}

float *mw_get_mem(const void *data, size_t data_size, const char *label,
                  int *frames_out, wav_t *w, int ch, char *out, int len) {
  ma_result result;
  ma_decoder decoder;
  ma_decoder_config decoderConfig;
  float* pSamples = NULL;
  ma_uint64 frameCount = 0;

  decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
  result = ma_decoder_init_memory(data, data_size, &decoderConfig, &decoder);
  if (result != MA_SUCCESS) {
    if (out != NULL && len > 0) {
      snprintf(out, len, "Could not load memory file: %s\n",
               label ? label : "(memory)");
    }
    *frames_out = 0;
    return NULL;
  }

  result = ma_decode_memory(data, data_size, &decoderConfig, &frameCount,
                            (void**)&pSamples);
  if (result != MA_SUCCESS) {
    ma_decoder_uninit(&decoder);
    *frames_out = 0;
    return NULL;
  }

  pSamples = mw_decode(&decoder, label, frames_out, w, ch, out, len,
                       pSamples, frameCount);
  ma_decoder_uninit(&decoder);
  return pSamples;
}

float *mw_get(char *filename, int *frames_out, wav_t *w, int ch) {
  return mw_get_str(filename, frames_out, w, ch, NULL, 0);
}


static int skode_path_has_dir(const char *path) {
  return path && (strchr(path, '/') || strchr(path, '\\'));
}

static int skode_asset_try_read(const char *path, int real_only,
    void **data, size_t *size, char *resolved, size_t resolved_size) {
  if (!path || path[0] == '\0') return 0;
  if (real_only) {
    if (!skred_vfs_read_real_file(path, data, size)) return 0;
  } else {
    if (!skred_vfs_read_file(path, data, size)) return 0;
  }
  if (resolved && resolved_size > 0) {
    snprintf(resolved, resolved_size, "%s", path);
  }
  return 1;
}

int skode_asset_read(const char *path, skode_asset_kind_t kind,
    void **data, size_t *size, char *resolved, size_t resolved_size) {
  char candidate[1024];
  int has_dir;
  const char *fallback_dir = NULL;

  if (!data || !size) return 0;
  *data = NULL;
  *size = 0;
  if (resolved && resolved_size > 0) resolved[0] = '\0';
  if (!path || path[0] == '\0') return 0;

  /*
   * Search order:
   *   mounted VFS, real cwd, type-specific dir in mounted VFS, then
   *   type-specific real subdir.
   * A file: prefix bypasses the mounted zip and reads the real filesystem.
   */
  if (strncmp(path, "file:", 5) == 0) {
    return skode_asset_try_read(path + 5, 1, data, size,
      resolved, resolved_size);
  }

  has_dir = skode_path_has_dir(path);
  if (skode_asset_try_read(path, 0, data, size, resolved, resolved_size))
    return 1;
  if (skode_asset_try_read(path, 1, data, size, resolved, resolved_size))
    return 1;

  if (has_dir) {
    char parent_candidate[1024];
    snprintf(parent_candidate, sizeof(parent_candidate), "../%s", path);
    if (skode_asset_try_read(parent_candidate, 1, data, size, resolved, resolved_size))
      return 1;
    return 0;
  }

  const char *dirs[] = { "wav", "sk", "ks" };
  const char *primary_dir = NULL;
  
  switch (kind) {
    case SKODE_ASSET_SKODE: primary_dir = "sk"; break;
    case SKODE_ASSET_WAVE: primary_dir = "wav"; break;
    case SKODE_ASSET_KSYNTH: primary_dir = "ks"; break;
    default: break;
  }

  for (int i = -1; i < 3; i++) {
    fallback_dir = (i == -1) ? primary_dir : dirs[i];
    if (!fallback_dir) continue;
    if (i >= 0 && primary_dir && strcmp(fallback_dir, primary_dir) == 0) continue; // Already tried primary

    snprintf(candidate, sizeof(candidate), "%s/%s", fallback_dir, path);
    if (skode_asset_try_read(candidate, 0, data, size, resolved, resolved_size)) return 1;

    char vfs_candidate[2048];
    snprintf(vfs_candidate, sizeof(vfs_candidate), "/%s", candidate);
    if (skode_asset_try_read(vfs_candidate, 0, data, size, resolved, resolved_size)) {
      if (resolved && resolved_size > 0)
        snprintf(resolved, resolved_size, "%s", vfs_candidate);
      return 1;
    }
    if (skode_asset_try_read(candidate, 1, data, size, resolved, resolved_size)) return 1;
  }
  return 0;
}

int skode_is_text(const void *data, size_t size, char *diag, size_t diag_size) {
  if (diag && diag_size > 0) diag[0] = '\0';
  if (!data || size == 0) return 1;

  const unsigned char *bytes = (const unsigned char *)data;

  /* Check known binary container / file signatures */
  if (size >= 4 && bytes[0] == 'P' && bytes[1] == 'K' &&
      (bytes[2] == 0x03 || bytes[2] == 0x05 || bytes[2] == 0x07)) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is a ZIP archive (use %%z or GS< to mount/restore)");
    }
    return 0;
  }

  if (size >= 12 && memcmp(bytes, "RIFF", 4) == 0 && memcmp(bytes + 8, "WAVE", 4) == 0) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is a WAV audio file (use w< or /w to load into wave slot)");
    }
    return 0;
  }

  if (size >= 4 && memcmp(bytes, "fLaC", 4) == 0) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is a FLAC audio file (use w< or /w to load into wave slot)");
    }
    return 0;
  }

  if ((size >= 3 && memcmp(bytes, "ID3", 3) == 0) ||
      (size >= 2 && bytes[0] == 0xFF && (bytes[1] & 0xE0) == 0xE0)) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is an MP3 audio file (use w< or /w to load into wave slot)");
    }
    return 0;
  }

  if (size >= 4 && memcmp(bytes, "OggS", 4) == 0) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is an Ogg audio file (use w< or /w to load into wave slot)");
    }
    return 0;
  }

  if (size >= 4 && memcmp(bytes, "MThd", 4) == 0) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is a MIDI file");
    }
    return 0;
  }

  if (size >= 4 && memcmp(bytes, "\x7f\x45\x4c\x46", 4) == 0) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is an ELF binary executable");
    }
    return 0;
  }

  if (size >= 4 && (memcmp(bytes, "\xfe\xed\xfa\xce", 4) == 0 ||
                    memcmp(bytes, "\xce\xfa\xed\xfe", 4) == 0 ||
                    memcmp(bytes, "\xfe\xed\xfa\xcf", 4) == 0 ||
                    memcmp(bytes, "\xcf\xfa\xed\xfe", 4) == 0 ||
                    memcmp(bytes, "\xca\xfe\xba\xbe", 4) == 0)) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is a Mach-O binary executable");
    }
    return 0;
  }

  if (size >= 2 && bytes[0] == 0x1F && bytes[1] == 0x8B) {
    if (diag && diag_size > 0) {
      snprintf(diag, diag_size, "file is a gzip compressed file");
    }
    return 0;
  }

  /*
   * Inspection of first line or up to 256 bytes:
   * Scan until first newline with non-whitespace content, or up to min(size, 256).
   */
  size_t inspect_len = size < 256 ? size : 256;
  size_t line_end = 0;
  int has_content = 0;
  for (size_t i = 0; i < inspect_len; i++) {
    if (bytes[i] == '\n' || bytes[i] == '\r') {
      if (has_content) {
        line_end = i + 1;
        break;
      }
    } else if (bytes[i] > ' ') {
      has_content = 1;
    }
  }
  if (line_end > 0) {
    inspect_len = line_end;
  }

  for (size_t i = 0; i < inspect_len; i++) {
    unsigned char c = bytes[i];
    if (c == '\0') {
      if (diag && diag_size > 0) {
        snprintf(diag, diag_size, "file contains null bytes (binary file)");
      }
      return 0;
    }
    if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
      if (diag && diag_size > 0) {
        snprintf(diag, diag_size, "file contains control characters 0x%02X (binary file)", c);
      }
      return 0;
    }
    if (c == 127) {
      if (diag && diag_size > 0) {
        snprintf(diag, diag_size, "file contains non-printable character 0x7F (binary file)");
      }
      return 0;
    }
  }

  return 1;
}

int skode_load_buffer(skode_t *ctx, const char *text, size_t text_len,
    const char *label,
    int verbose) {
  if (ctx == NULL) return SK_ERR_INVALID_PARAM;
  if (!text || text_len == 0) return SK_OK;

  char diag[256];
  if (!skode_is_text(text, text_len, diag, sizeof(diag))) {
    ctx->printf(ctx, "# cannot load %s: %s\n",
                label ? label : "(buffer)", diag);
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "%s: %s",
                    label ? label : "(buffer)", diag);
    return SK_ERR_INVALID_PARAM;
  }

  int r = 0;
  skode_t *loader = (skode_t *)calloc(1, sizeof(*loader));
  if (!loader) {
    ctx->printf(ctx, "# cannot allocate patch loader\n");
    return -1;
  }
  skode_init(loader);
  loader->log_enable = 1;
  if (text) {
    int line_no = 1;
    size_t pos = 0;
    while (pos < text_len) {
      size_t start = pos;
      int in_string = 0;
      int in_array = 0;
      int in_comment = 0;
      
      while (pos < text_len) {
        char c = text[pos];
        if (in_comment) {
          if (c == '\n' || c == '\r') break;
        } else if (in_string) {
          if (c == ']') in_string = 0;
        } else if (in_array) {
          if (c == ')') in_array = 0;
        } else {
          if (c == '[') in_string = 1;
          else if (c == '(') in_array = 1;
          else if (c == '#') in_comment = 1;
          else if (c == '\n' || c == '\r') break;
        }
        pos++;
      }
      size_t len = pos - start;
      
      char *chunk = (char *)malloc(len + 1);
      if (!chunk) {
        ctx->printf(ctx, "# allocation failed\n");
        r = -1;
        break;
      }
      memcpy(chunk, text + start, len);
      chunk[len] = '\0';
      
      if (verbose) {
         char preview[128];
         snprintf(preview, sizeof(preview), "%s", chunk);
         for (int i=0; preview[i]; i++) if (preview[i] == '\n' || preview[i] == '\r') preview[i] = ' ';
         ctx->printf(ctx, "# %s # (%d)\n", preview, line_no);
      }
      
      r = skode_consume(chunk, loader);
      
      if (loader->log_len > 0) {
        if (strstr(loader->log, "contains an immediate-only word") ||
            strstr(loader->log, "could not be compiled")) {
          r = -1;
        }
        ctx->printf(ctx, "%s", loader->log);
      }
      if (r != 0) {
        ctx->printf(ctx, "# load aborted at %s:%d\n",
          label ? label : "(unknown)", line_no);
        free(chunk);
        r = -1;
        break;
      }
      free(chunk);
      
      for (size_t i = start; i < pos; i++) {
          if (text[i] == '\n') line_no++;
      }
      
      while (pos < text_len && (text[pos] == '\n' || text[pos] == '\r')) {
          if (text[pos] == '\n') line_no++;
          pos++;
      }
    }
  } else {
    ctx->printf(ctx, "# cannot load %s\n", label ? label : "(null)");
    r = -1;
  }
  if (r == 0) {
    ctx->voice = loader->voice;
    ctx->pattern = loader->pattern;
    ctx->step = loader->step;
    ctx->flag = loader->flag;
    ctx->trace = loader->trace;
    ctx->verbose = loader->verbose;
    memcpy(ctx->string_slot, loader->string_slot, sizeof(ctx->string_slot));
    if (ctx->parse && loader->parse) {
      int count = ands_data_len(loader->parse);
      if (count > ands_data_cap(ctx->parse)) ands_data_resize(ctx->parse, count);
      memcpy(ands_data(ctx->parse), ands_data(loader->parse), count * sizeof(double));
      ands_data_len_set(ctx->parse, count);
    }
  }
  skode_free(loader);
  free(loader);
  return r;
}

int skode_load_name(skode_t *ctx, const char *name, int verbose) {
  skode_t caller_storage = SKODE_EMPTY();
  if (ctx == NULL) {
    ctx = &caller_storage;
    skode_init(ctx);
  }
  if (!name || name[0] == '\0') {
    ctx->printf(ctx, "# cannot load empty filename\n");
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "cannot load empty filename");
    return SK_ERR_INVALID_PARAM;
  }
  char file[1024];
  char resolved[1024];
  void *data = NULL;
  size_t size = 0;
  snprintf(file, sizeof(file), "%s", name);
  if (!skode_asset_read(file, SKODE_ASSET_SKODE, &data, &size,
      resolved, sizeof(resolved))) {
    ctx->printf(ctx, "# cannot load %s\n", file);
    skode_set_error(ctx, SK_ERR_NOT_FOUND, "cannot load %s", file);
    return SK_ERR_NOT_FOUND;
  }
  int r = skode_load_buffer(ctx, (const char *)data, size,
    resolved[0] ? resolved : file, verbose);
  skred_vfs_free_file(data);
  return r;
}

int skode_load(skode_t *ctx, int voice, int n, int verbose) {
  (void)voice;
  skode_t caller_storage = SKODE_EMPTY();
  if (ctx == NULL) {
    ctx = &caller_storage;
    skode_init(ctx);
  }
  char file[1024];
  char resolved[1024];
  void *data = NULL;
  size_t size = 0;
  sprintf(file, "%d.sk", n);
  if (!skode_asset_read(file, SKODE_ASSET_SKODE, &data, &size,
      resolved, sizeof(resolved))) {
    ctx->printf(ctx, "# cannot load %s\n", file);
    skode_set_error(ctx, SK_ERR_NOT_FOUND, "cannot load %s", file);
    return SK_ERR_NOT_FOUND;
  }
  int r = skode_load_buffer(ctx, (const char *)data, size,
    resolved[0] ? resolved : file, verbose);
  skred_vfs_free_file(data);
  return r;
}

extern synth_sample_t sampling;

void wave_install_memory_stereo(int wave_slot, float *table_left, float *table_right, int len,
                                float rate, int one_shot, const char *name,
                                float midi_note, float offset_hz) {
  skode_copy_string(sw.name[wave_slot], WAVE_NAME_MAX, name ? name : "data");
  sw.is_heap[wave_slot] = 1;
  sw.data[wave_slot] = table_left;
  if (sw.data_right) sw.data_right[wave_slot] = table_right;
  if (sw.channels) sw.channels[wave_slot] = table_right ? 2 : 1;
  sw.size[wave_slot] = len;
  sw.rate[wave_slot] = rate;
  sw.one_shot[wave_slot] = one_shot != 0;
  sw.loop_enabled[wave_slot] = 0;
  sw.loop_start[wave_slot] = 0;
  sw.loop_end[wave_slot] = len;
  sw.direction[wave_slot] = 0.0f;
  sw.midi_note[wave_slot] = midi_note;
  sw.offset_hz[wave_slot] = offset_hz;
  if (sw.eval_ms) sw.eval_ms[wave_slot] = 0.0f;
  if (sw.source) {
    if (sw.source_is_heap && sw.source_is_heap[wave_slot] && sw.source[wave_slot]) {
      free((void *)sw.source[wave_slot]);
    }
    sw.source[wave_slot] = NULL;
  }
  if (sw.source_is_heap) sw.source_is_heap[wave_slot] = 0;
}

void wave_install_memory_multich(int wave_slot, float **data_channels, int channels, int len,
                                 float rate, int one_shot, const char *name,
                                 float midi_note, float offset_hz) {
  wave_install_memory_stereo(wave_slot, data_channels[0], data_channels[1], len,
                             rate, one_shot, name, midi_note, offset_hz);
  if (sw.channels) sw.channels[wave_slot] = channels;
  if (sw.channels_data) {
    if (sw.channels_data[wave_slot]) {
      for (int c = 2; c < sw.channels[wave_slot]; c++) {
        if (sw.channels_data[wave_slot][c]) free(sw.channels_data[wave_slot][c]);
      }
      free(sw.channels_data[wave_slot]);
    }
    sw.channels_data[wave_slot] = data_channels;
  }
}

void wave_install_memory(int wave_slot, float *table, int len,
                         float rate, int one_shot, const char *name,
                         float midi_note, float offset_hz) {
  wave_install_memory_stereo(wave_slot, table, NULL, len, rate, one_shot, name, midi_note, offset_hz);
}

int rec_load(skode_t *ctx, int wave_slot, int one_shot, int channel, int verbose) {
  if (verbose) ctx->printf(ctx, "# rec_load(ctx, %d, %d, %d, %d)\n", wave_slot, one_shot, channel, verbose);
  if (!skode_wave_valid(wave_slot)) {
    ctx->printf(ctx, "# invalid slot %d\n", wave_slot);
    return -1;
  }
  if (atomic_load_int(&sampling.state) != SAMPLE_STATE_COMPLETE) {
    ctx->printf(ctx, "# recording buffer is not complete\n");
    return -1;
  }
  if (!sampling.where || sampling.offset < 0 || sampling.trim < 0 ||
      sampling.offset > sampling.len ||
      sampling.trim > sampling.len - sampling.offset) {
    ctx->printf(ctx, "# invalid recording bounds\n");
    return -1;
  }
  int channels = sampling.channels == 2 ? 2 : 1;
  int data_len = sampling.len - sampling.offset - sampling.trim;
  if (data_len <= 0) {
    ctx->printf(ctx, "# no data len\n");
    return SK_ERR_INVALID_PARAM;
  }
  if (channel < -1 || channel >= channels) {
    ctx->printf(ctx, "# recording channel must be -1..%d\n", channels - 1);
    return -1;
  }
  if (sw.readonly[wave_slot] == 1) {
    ctx->printf(ctx, "# cannot write to w%d r/o\n", wave_slot);
    return -1;
  }
  if (sw.refcount[wave_slot] > 0) {
    ctx->printf(ctx, "# cannot write to w%d ref > 0\n", wave_slot);
    return -1;
  } else {
    wave_free_one(wave_slot);
  }
  float *table = calloc(data_len, sizeof(float));
  if (!table) {
    ctx->printf(ctx, "# allocation failed\n");
    return -1;
  }
  float *table_r = NULL;
  if (channels == 2 && channel == -1) {
    table_r = calloc(data_len, sizeof(float));
  }
  for (int i=0; i<data_len; i++) {
    size_t frame = (size_t)(sampling.offset + i) * (size_t)channels;
    if (channels == 1) {
      table[i] = sampling.where[frame];
    } else if (channel >= 0) {
      table[i] = sampling.where[frame + (size_t)channel];
    } else if (table_r) {
      table[i] = sampling.where[frame];
      table_r[i] = sampling.where[frame + 1];
    } else {
      table[i] = 0.5f * (sampling.where[frame] + sampling.where[frame + 1]);
    }
  }
  normalize_preserve_zero(table, data_len);
  if (table_r) normalize_preserve_zero(table_r, data_len);
  int len = data_len;
  char wave_name[WAVE_NAME_MAX];
  snprintf(wave_name, sizeof(wave_name), "data[%d]", data_len);
  wave_install_memory_stereo(wave_slot, table, table_r, len, (float)MAIN_SAMPLE_RATE,
    one_shot, wave_name, one_shot ? 69.0f : 0.0f, 0.0f);
  char *name = "data";
  if (verbose) ctx->printf(ctx, "# read %d frames from %s to %d (ch:%d)\n",
              data_len, name, wave_slot, table_r ? 2 : 1);
  return 0;
}

int data_load(skode_t *ctx, int wave_slot, int one_shot, float rate, float offset) {
  if (ctx == NULL) return SK_ERR_INVALID_PARAM;
  if (0) ctx->printf(ctx, "# data_load(ctx, %d, %d, %g, %g)\n", wave_slot, one_shot, rate, offset);
  if (!skode_wave_valid(wave_slot)) {
    ctx->printf(ctx, "# invalid slot %d\n", wave_slot);
    return -1;
  }
  double *data = ands_data(ctx->parse);
  int data_len = ands_data_len(ctx->parse);
  if (data == NULL) {
    ctx->printf(ctx, "# no data\n");
    return SK_ERR_NOT_FOUND;
  }
  if (data_len <= 0) {
    ctx->printf(ctx, "# no data len\n");
    return SK_ERR_INVALID_PARAM;
  }
  if (sw.readonly[wave_slot] == 1) {
    ctx->printf(ctx, "# cannot write to w%d r/o\n", wave_slot);
    return -1;
  }

  /* Parse metadata from active ands_string if present */
  sk_meta_info_t meta;
  int has_meta = 0;
  if (ctx->parse) {
    const char *str = ands_string(ctx->parse);
    if (str && str[0] != '\0') {
      has_meta = sk_parse_metadata(str, &meta);
    }
  }

  if (has_meta) {
    if (meta.has_rate && (!isfinite(rate) || rate <= 0 || rate == (float)MAIN_SAMPLE_RATE)) {
      rate = meta.rate;
    }
    if (meta.has_one_shot) {
      one_shot = meta.one_shot;
    }
  }

  if (!isfinite(rate) || rate <= 0) {
    rate = (float)MAIN_SAMPLE_RATE;
  }
  if (rate <= 0) {
    ctx->printf(ctx, "# invalid rate %g > 0\n", rate);
    return -1;
  }
  if (sw.refcount[wave_slot] > 0) {
    ctx->printf(ctx, "# cannot write to w%d ref > 0\n", wave_slot);
    return -1;
  } else {
    wave_free_one(wave_slot);
  }
  float *table = calloc(data_len, sizeof(float));
  if (!table) {
    ctx->printf(ctx, "# allocation failed\n");
    return -1;
  }
  for (int i=0; i<data_len; i++) table[i] = (float)data[i];
  int len = data_len;
    char wave_name[WAVE_NAME_MAX];
    snprintf(wave_name, sizeof(wave_name), "data[%d]", data_len);
    float offset_hz = offset > 0 ? (float)len / rate * 440.0f : 0.0f;
    wave_install_memory(wave_slot, table, len, rate, one_shot, wave_name,
      offset > 0 ? 69.0f : 0.0f, offset_hz);

  /* Apply metadata loop points, root note, and mipmaps */
  if (has_meta) {
    if (meta.has_loop) {
      sw.loop_enabled[wave_slot] = 1;
      sw.loop_start[wave_slot] = meta.loop_start;
      sw.loop_end[wave_slot] = meta.loop_end >= len ? len : meta.loop_end;
    }
    if (meta.has_root_note) {
      sw.midi_note[wave_slot] = meta.root_note;
    }
    if (meta.num_cues > 0 && sw.cues && sw.num_cues) {
      if (sw.cues[wave_slot]) free(sw.cues[wave_slot]);
      sw.cues[wave_slot] = (sk_cue_t *)malloc(meta.num_cues * sizeof(sk_cue_t));
      if (sw.cues[wave_slot]) {
        memcpy(sw.cues[wave_slot], meta.cues, meta.num_cues * sizeof(sk_cue_t));
        sw.num_cues[wave_slot] = meta.num_cues;
      }
    }
    if (meta.mode == SK_WAVE_MODE_OSC) {
      sw.one_shot[wave_slot] = 0;
      wave_generate_mipmaps(wave_slot);
    }
  }

    char *name = "data";
    int channels = 1;
    if (0) ctx->printf(ctx, "# read %d frames from %s to %d (ch:%d sr:%g)\n", len, name, wave_slot, channels, rate);
  return 0;
}

int wav_parse_riff_metadata(const void *data, size_t data_size, int frames,
                            sk_meta_info_t *out_meta) {
  const unsigned char *bytes = (const unsigned char *)data;
  size_t pos = 12;

  if (!out_meta) return 0;
  sk_meta_init(out_meta);
  out_meta->mode = SK_WAVE_MODE_SAMPLE;

  if (!bytes || data_size < 12 || frames <= 0) return 0;
  if (memcmp(bytes, "RIFF", 4) != 0 || memcmp(bytes + 8, "WAVE", 4) != 0)
    return 0;

  typedef struct {
    uint32_t id;
    uint32_t sample_offset;
  } raw_cue_pt_t;
  raw_cue_pt_t raw_cues[SK_MAX_CUES];
  int raw_cue_count = 0;

  while (pos + 8 <= data_size) {
    const unsigned char *chunk = bytes + pos;
    uint32_t chunk_size = mw_le_u32(chunk + 4);
    size_t data_start = pos + 8;
    size_t next_chunk = data_start + (size_t)chunk_size + (size_t)(chunk_size & 1u);

    if (next_chunk < data_start || data_start > data_size) break;

    if (memcmp(chunk, "smpl", 4) == 0 && chunk_size >= 36) {
      const unsigned char *header = bytes + data_start;
      uint32_t unity_note = mw_le_u32(header + 12);
      if (unity_note <= 127) {
        out_meta->root_note = (float)unity_note;
        out_meta->has_root_note = 1;
      }
      uint32_t sample_loop_count = mw_le_u32(header + 28);
      for (uint32_t i = 0; i < sample_loop_count && (36 + (i + 1) * 24 <= chunk_size); i++) {
        const unsigned char *sample_loop = header + 36 + i * 24;
        uint32_t type = mw_le_u32(sample_loop + 4);
        uint32_t start = mw_le_u32(sample_loop + 8);
        uint32_t end = mw_le_u32(sample_loop + 12);
        int end_exclusive = end >= (uint32_t)frames ? frames : (int)end + 1;
        if (start < (uint32_t)frames && end_exclusive > (int)start) {
          if (i == 0) {
            out_meta->has_loop = 1;
            out_meta->loop_start = (int)start;
            out_meta->loop_end = end_exclusive;
          }
          if (out_meta->num_cues < SK_MAX_CUES) {
            sk_cue_t *c = &out_meta->cues[out_meta->num_cues++];
            c->start = start;
            c->end = (uint32_t)end_exclusive;
            c->type = (i == 0) ? SK_CUE_LOOP_SUSTAIN : SK_CUE_LOOP_RELEASE;
            snprintf(c->label, sizeof(c->label), "loop%u", (unsigned int)i);
          }
        }
      }
    } else if (memcmp(chunk, "cue ", 4) == 0 && chunk_size >= 4) {
      const unsigned char *cdata = bytes + data_start;
      uint32_t num_cues = mw_le_u32(cdata);
      for (uint32_t j = 0; j < num_cues && (4 + (j + 1) * 24 <= chunk_size); j++) {
        if (raw_cue_count >= SK_MAX_CUES) break;
        const unsigned char *cp = cdata + 4 + j * 24;
        raw_cues[raw_cue_count].id = mw_le_u32(cp + 0);
        raw_cues[raw_cue_count].sample_offset = mw_le_u32(cp + 20);
        raw_cue_count++;
      }
    } else if (memcmp(chunk, "LIST", 4) == 0 && chunk_size >= 4) {
      const unsigned char *ldata = bytes + data_start;
      if (chunk_size >= 4 && memcmp(ldata, "adtl", 4) == 0) {
        size_t lpos = 4;
        while (lpos + 8 <= chunk_size) {
          const unsigned char *sub = ldata + lpos;
          uint32_t sub_sz = mw_le_u32(sub + 4);
          size_t sub_data_start = lpos + 8;
          size_t next_sub = sub_data_start + sub_sz + (sub_sz & 1u);
          if (next_sub > chunk_size) break;

          if (memcmp(sub, "labl", 4) == 0 && sub_sz >= 4) {
            uint32_t cid = mw_le_u32(ldata + sub_data_start);
            const char *txt = (const char *)(ldata + sub_data_start + 4);
            size_t txt_len = sub_sz - 4;
            /* Check if this cid matches an existing cue in out_meta */
            int found = 0;
            for (int k = 0; k < out_meta->num_cues; k++) {
              if (strcmp(out_meta->cues[k].label, "loop0") == 0 && cid == 0) {
                snprintf(out_meta->cues[k].label, sizeof(out_meta->cues[k].label), "%.*s", (int)txt_len, txt);
                found = 1;
                break;
              }
            }
            /* Match with raw_cues */
            for (int r = 0; r < raw_cue_count; r++) {
              if (raw_cues[r].id == cid) {
                if (out_meta->num_cues < SK_MAX_CUES) {
                  sk_cue_t *c = &out_meta->cues[out_meta->num_cues++];
                  c->start = raw_cues[r].sample_offset;
                  c->end = c->start;
                  c->type = SK_CUE_POINT;
                  snprintf(c->label, sizeof(c->label), "%.*s", (int)txt_len, txt);
                }
                found = 1;
                break;
              }
            }
          } else if (memcmp(sub, "ltxt", 4) == 0 && sub_sz >= 8) {
            uint32_t cid = mw_le_u32(ldata + sub_data_start);
            uint32_t slen = mw_le_u32(ldata + sub_data_start + 4);
            for (int k = 0; k < out_meta->num_cues; k++) {
              for (int r = 0; r < raw_cue_count; r++) {
                if (raw_cues[r].id == cid && out_meta->cues[k].start == raw_cues[r].sample_offset) {
                  out_meta->cues[k].end = out_meta->cues[k].start + slen;
                  out_meta->cues[k].type = SK_CUE_SLICE;
                  break;
                }
              }
            }
          }
          if (next_sub <= lpos) break;
          lpos = next_sub;
        }
      }
    }

    if (next_chunk <= pos) break;
    pos = next_chunk;
  }

  /* Any raw_cues that didn't get a labl */
  for (int r = 0; r < raw_cue_count; r++) {
    int already = 0;
    for (int k = 0; k < out_meta->num_cues; k++) {
      if (out_meta->cues[k].start == raw_cues[r].sample_offset) {
        already = 1;
        break;
      }
    }
    if (!already && out_meta->num_cues < SK_MAX_CUES) {
      sk_cue_t *c = &out_meta->cues[out_meta->num_cues++];
      c->start = raw_cues[r].sample_offset;
      c->end = c->start;
      c->type = SK_CUE_POINT;
      snprintf(c->label, sizeof(c->label), "cue%u", raw_cues[r].id);
    }
  }

  return (out_meta->has_loop || out_meta->has_root_note || out_meta->num_cues > 0);
}

static void wave_load_apply_metadata(skode_t *ctx, const char *name,
                                     const void *data, size_t data_size,
                                     int wave_index, int len, float sample_rate) {
  mw_smpl_loop_t loop;
  int has_smpl = mw_get_smpl_loop_mem(data, data_size, len, &loop);
  if (has_smpl) {
    sw.loop_enabled[wave_index] = 1;
    sw.loop_start[wave_index] = loop.start;
    sw.loop_end[wave_index] = loop.end;
    sw.direction[wave_index] = loop.type == 1 ? 2.0f :
      (loop.type == 2 ? 1.0f : 0.0f);
    ctx->printf(ctx, "# smpl loop %d..%d type:%d play:%d\n",
      loop.start, loop.end, loop.type, loop.play_count);
  }

  sk_meta_info_t meta;
  sk_meta_init(&meta);
  meta.mode = SK_WAVE_MODE_SAMPLE;
  meta.rate = sample_rate;
  meta.has_rate = 1;
  meta.one_shot = 1;
  meta.has_one_shot = 1;

  if (wav_parse_riff_metadata(data, data_size, len, &meta)) {
    if (meta.has_loop && !has_smpl) {
      sw.loop_enabled[wave_index] = 1;
      sw.loop_start[wave_index] = meta.loop_start;
      sw.loop_end[wave_index] = meta.loop_end;
    }
    if (meta.has_root_note) {
      sw.midi_note[wave_index] = meta.root_note;
    }
    if (meta.num_cues > 0 && sw.cues && sw.num_cues) {
      if (sw.cues[wave_index]) free(sw.cues[wave_index]);
      sw.cues[wave_index] = (sk_cue_t *)malloc(meta.num_cues * sizeof(sk_cue_t));
      if (sw.cues[wave_index]) {
        memcpy(sw.cues[wave_index], meta.cues, meta.num_cues * sizeof(sk_cue_t));
        sw.num_cues[wave_index] = meta.num_cues;
      }
      ctx->printf(ctx, "# loaded %d cues\n", meta.num_cues);
    }
  }

  /* Always populate ands_string with metadata string */
  if (ctx && ctx->parse) {
    char meta_str[512];
    sk_format_metadata(&meta, meta_str, sizeof(meta_str));
    ands_string_from_external(ctx->parse, meta_str, (int)strlen(meta_str));
  }
}

static void wave_load_apply_smpl_loop(skode_t *ctx, const char *name,
                                      const void *data, size_t data_size,
                                      int wave_index, int len) {
  wave_load_apply_metadata(ctx, name, data, data_size, wave_index, len,
                           sw.rate && sw.rate[wave_index] > 0 ? sw.rate[wave_index] : (float)MAIN_SAMPLE_RATE);
}

int wave_load_string(skode_t *ctx, char *name, int wave_index, int ch, int normalize) {
  (void)normalize;
  if (ctx == NULL) return SK_ERR_INVALID_PARAM;
  if (!skode_wave_valid(wave_index)) {
    ctx->printf(ctx, "# invalid slot %d\n", wave_index);
    return -1;
  }
  if (sw.readonly[wave_index] == 1) {
    ctx->printf(ctx, "# cannot write to w%d r/o\n", wave_index);
    return -1;
  }
  if (sw.refcount[wave_index] > 0) {
    ctx->printf(ctx, "# cannot write to w%d ref > 0\n", wave_index);
    return -1;
  } else {
    wave_free_one(wave_index);
  }
  void *data = NULL;
  size_t data_size = 0;
  char resolved[1024];
  if (!skode_asset_read(name, SKODE_ASSET_WAVE, &data, &data_size,
      resolved, sizeof(resolved))) {
    ctx->printf(ctx, "# cannot open %s\n", name);
    skode_set_error(ctx, SK_ERR_NOT_FOUND, "cannot open %s", name);
    return SK_ERR_NOT_FOUND;
  }
  if (data_size >= 4 && ((const unsigned char *)data)[0] == 'P' &&
      ((const unsigned char *)data)[1] == 'K' &&
      (((const unsigned char *)data)[2] == 3 || ((const unsigned char *)data)[2] == 5 || ((const unsigned char *)data)[2] == 7)) {
    ctx->printf(ctx, "# cannot load %s: file is a ZIP archive, not an audio file\n", resolved);
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "%s: file is a ZIP archive", resolved);
    skred_vfs_free_file(data);
    return SK_ERR_INVALID_PARAM;
  }
  char text_diag[256];
  if (skode_is_text(data, data_size, text_diag, sizeof(text_diag))) {
    ctx->printf(ctx, "# cannot load %s: file is a text script, not an audio file\n", resolved);
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "%s: file is a text script", resolved);
    skred_vfs_free_file(data);
    return SK_ERR_INVALID_PARAM;
  }
  wav_t wav;
  int len;
  char out[4096];
  float *table = mw_get_mem(data, data_size, resolved, &len, &wav, ch, out, sizeof(out));
  if (table == NULL) {
    ctx->printf(ctx, "# cannot read %s\n", resolved);
    skode_set_error(ctx, SK_ERR_IO, "cannot read %s", resolved);
    skred_vfs_free_file(data);
    return SK_ERR_IO;
  } else {
    if (wav.Channels > 2 && wav.data_channels) {
      wave_install_memory_multich(wave_index, wav.data_channels, wav.Channels, len, (float)wav.SamplesRate, 1,
        resolved, 69.0f, (float)len / (float)wav.SamplesRate * 440.0f);
      for (int c = 0; c < wav.Channels; c++) {
        normalize_preserve_zero(wav.data_channels[c], len);
      }
    } else if (wav.Channels == 2 && wav.data_right) {
      wave_install_memory_stereo(wave_index, table, wav.data_right, len, (float)wav.SamplesRate, 1,
        resolved, 69.0f, (float)len / (float)wav.SamplesRate * 440.0f);
      normalize_preserve_zero(wav.data_right, len);
    } else {
      wave_install_memory(wave_index, table, len, (float)wav.SamplesRate, 1,
        resolved, 69.0f, (float)len / (float)wav.SamplesRate * 440.0f);
    }
    wave_load_apply_metadata(ctx, resolved, data, data_size, wave_index, len, (float)wav.SamplesRate);
    if (0) ctx->printf(ctx, "# read %d frames from %s to %d (ch:%d sr:%d)\n",
      len, resolved, wave_index, wav.Channels, wav.SamplesRate);
    normalize_preserve_zero(table, len);
  }
  skred_vfs_free_file(data);
  return 0;
}

int wave_try_open_number(int file_num, char *name, int len) {
  void *data = NULL;
  size_t size = 0;
  char resolved[1024];
  snprintf(name, len, "%d.wav", file_num);
  if (skode_asset_read(name, SKODE_ASSET_WAVE, &data, &size, resolved, sizeof(resolved))) {
    snprintf(name, len, "%s", resolved);
    skred_vfs_free_file(data);
    return 0;
  }
  snprintf(name, len, "%d.mp3", file_num);
  if (skode_asset_read(name, SKODE_ASSET_WAVE, &data, &size, resolved, sizeof(resolved))) {
    snprintf(name, len, "%s", resolved);
    skred_vfs_free_file(data);
    return 0;
  }
  snprintf(name, len, "%d.flac", file_num);
  if (skode_asset_read(name, SKODE_ASSET_WAVE, &data, &size, resolved, sizeof(resolved))) {
    snprintf(name, len, "%s", resolved);
    skred_vfs_free_file(data);
    return 0;
  }
  return -1;
}

int wave_load(skode_t *ctx, int file_num, int wave_index, int ch, int normalize) {
  (void)normalize;
  if (ctx == NULL) return SK_ERR_INVALID_PARAM;
  if (!skode_wave_valid(wave_index)) {
    ctx->printf(ctx, "# invalid slot %d\n", wave_index);
    return -1;
  }
  if (sw.readonly[wave_index] == 1) {
    ctx->printf(ctx, "# cannot write to w%d r/o\n", wave_index);
    return -1;
  }
  if (sw.refcount[wave_index] > 0) {
    ctx->printf(ctx, "# cannot write to w%d ref > 0\n", wave_index);
    return -1;
  } else {
    wave_free_one(wave_index);
  }
  char name[1024];
  if (wave_try_open_number(file_num, name, sizeof(name)) < 0) {
    ctx->printf(ctx, "# cannot open %d.wav or wav/%d.wav\n", file_num, file_num);
    return -1;
  }
  void *data = NULL;
  size_t data_size = 0;
  char resolved[1024];
  if (!skode_asset_read(name, SKODE_ASSET_WAVE, &data, &data_size,
      resolved, sizeof(resolved))) {
    ctx->printf(ctx, "# cannot open %s\n", name);
    skode_set_error(ctx, SK_ERR_NOT_FOUND, "cannot open %s", name);
    return SK_ERR_NOT_FOUND;
  }
  if (data_size >= 4 && ((const unsigned char *)data)[0] == 'P' &&
      ((const unsigned char *)data)[1] == 'K' &&
      (((const unsigned char *)data)[2] == 3 || ((const unsigned char *)data)[2] == 5 || ((const unsigned char *)data)[2] == 7)) {
    ctx->printf(ctx, "# cannot load %s: file is a ZIP archive, not an audio file\n", resolved);
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "%s: file is a ZIP archive", resolved);
    skred_vfs_free_file(data);
    return SK_ERR_INVALID_PARAM;
  }
  char text_diag[256];
  if (skode_is_text(data, data_size, text_diag, sizeof(text_diag))) {
    ctx->printf(ctx, "# cannot load %s: file is a text script, not an audio file\n", resolved);
    skode_set_error(ctx, SK_ERR_INVALID_PARAM, "%s: file is a text script", resolved);
    skred_vfs_free_file(data);
    return SK_ERR_INVALID_PARAM;
  }
  wav_t wav;
  int len;
  char out[4096];
  float *table = mw_get_mem(data, data_size, resolved, &len, &wav, ch, out, sizeof(out));
  if (table == NULL) {
    ctx->printf(ctx, "# cannot read %s\n", resolved);
    skode_set_error(ctx, SK_ERR_IO, "cannot read %s", resolved);
    skred_vfs_free_file(data);
    return SK_ERR_IO;
  } else {
    if (wav.Channels > 2 && wav.data_channels) {
      wave_install_memory_multich(wave_index, wav.data_channels, wav.Channels, len, (float)wav.SamplesRate, 1,
        resolved, 69.0f, (float)len / (float)wav.SamplesRate * 440.0f);
      for (int c = 0; c < wav.Channels; c++) {
        normalize_preserve_zero(wav.data_channels[c], len);
      }
    } else if (wav.Channels == 2 && wav.data_right) {
      wave_install_memory_stereo(wave_index, table, wav.data_right, len, (float)wav.SamplesRate, 1,
        resolved, 69.0f, (float)len / (float)wav.SamplesRate * 440.0f);
      normalize_preserve_zero(wav.data_right, len);
    } else {
      wave_install_memory(wave_index, table, len, (float)wav.SamplesRate, 1,
        resolved, 69.0f, (float)len / (float)wav.SamplesRate * 440.0f);
    }
    wave_load_apply_metadata(ctx, resolved, data, data_size, wave_index, len, (float)wav.SamplesRate);
    if (0) ctx->printf(ctx, "# read %d frames from %s to %d (ch:%d sr:%d)\n",
      len, resolved, wave_index, wav.Channels, wav.SamplesRate);
    normalize_preserve_zero(table, len);
  }
  skred_vfs_free_file(data);
  return 0;
}
