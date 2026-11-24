#include "skode-internal.h"
#include "miniaudio.h"
#include <math.h>



float record_frame_level(int frame) {
  int channels = sampling.channels == 2 ? 2 : 1;
  size_t index = (size_t)frame * (size_t)channels;
  float level = fabsf(sampling.where[index]);
  if (channels == 2) {
    float right = fabsf(sampling.where[index + 1]);
    if (right > level) level = right;
  }
  return level;
}


void normalize_buffer(float* pSamples, ma_uint32 frameCount, ma_uint32 channels) {
    float maxAmp = 0.0f;
    ma_uint32 sampleCount = frameCount * channels;

    // Find the absolute peak
    for (ma_uint32 i = 0; i < sampleCount; ++i) {
        float absValue = fabsf(pSamples[i]);
        if (absValue > maxAmp) maxAmp = absValue;
    }

    // Only scale if the peak exceeds our target (e.g., 0.95 for headroom)
    if (maxAmp > 0.0f) {
        float scale = 0.95f / maxAmp;
        for (ma_uint32 i = 0; i < sampleCount; ++i) {
            pSamples[i] *= scale;
        }
    }
}

static void write_u16_le(FILE *f, uint16_t v) {
  unsigned char b[2];
  b[0] = (unsigned char)(v & 0xFF);
  b[1] = (unsigned char)((v >> 8) & 0xFF);
  fwrite(b, 1, 2, f);
}

static void write_u32_le(FILE *f, uint32_t v) {
  unsigned char b[4];
  b[0] = (unsigned char)(v & 0xFF);
  b[1] = (unsigned char)((v >> 8) & 0xFF);
  b[2] = (unsigned char)((v >> 16) & 0xFF);
  b[3] = (unsigned char)((v >> 24) & 0xFF);
  fwrite(b, 1, 4, f);
}

static void write_chunk_hdr(FILE *f, const char *id, uint32_t size) {
  fwrite(id, 1, 4, f);
  write_u32_le(f, size);
}

int skode_write_wav_riff(skode_t *ctx, const char *filename,
                         const float *samples, int frames,
                         uint32_t channels, uint32_t sample_rate,
                         int normalize, int has_loop, int loop_start, int loop_end,
                         float root_note, const sk_cue_t *cues, int num_cues) {
    const float *where = samples;
    float *copy = NULL;

    if (!filename || !filename[0]) {
        if (ctx) ctx->printf(ctx, "# WAV output requires [filename]\n");
        return 0;
    }
    if (!samples || frames <= 0) {
        if (ctx) ctx->printf(ctx, "# no samples to write to %s\n", filename);
        return 0;
    }
    if (channels < 1 || channels > AUDIO_CHANNELS) {
        if (ctx) ctx->printf(ctx, "# invalid WAV channel count\n");
        return 0;
    }
    if (sample_rate == 0) sample_rate = MAIN_SAMPLE_RATE;
    if (normalize) {
        size_t sample_count = (size_t)frames * channels;
        copy = (float *)malloc(sample_count * sizeof(float));
        if (!copy) {
            if (ctx) ctx->printf(ctx, "# allocation failed writing %s\n", filename);
            return 0;
        }
        memcpy(copy, samples, sample_count * sizeof(float));
        normalize_buffer(copy, (ma_uint32)frames, channels);
        where = copy;
    }

    FILE *f = fopen(filename, "wb");
    if (!f) {
        if (ctx) ctx->printf(ctx, "# cannot open %s\n", filename);
        free(copy);
        return 0;
    }

    /* 1. Reserve 12 bytes for RIFF header */
    fwrite("RIFF\0\0\0\0WAVE", 1, 12, f);

    /* 2. fmt chunk (16 bytes, IEEE float tag 3) */
    write_chunk_hdr(f, "fmt ", 16);
    write_u16_le(f, 3); /* IEEE float */
    write_u16_le(f, (uint16_t)channels);
    write_u32_le(f, sample_rate);
    write_u32_le(f, sample_rate * channels * sizeof(float));
    write_u16_le(f, (uint16_t)(channels * sizeof(float)));
    write_u16_le(f, 32);

    /* 3. data chunk */
    uint32_t data_bytes = (uint32_t)(frames * channels * sizeof(float));
    write_chunk_hdr(f, "data", data_bytes);
    fwrite(where, sizeof(float), (size_t)frames * channels, f);
    if (data_bytes & 1) {
        unsigned char pad = 0;
        fwrite(&pad, 1, 1, f);
    }

    /* 4. smpl chunk (if loop enabled or root note specified) */
    if (has_loop || root_note > 0.0f) {
        uint32_t smpl_size = 36 + (has_loop ? 24 : 0);
        write_chunk_hdr(f, "smpl", smpl_size);
        write_u32_le(f, 0); /* manufacturer */
        write_u32_le(f, 0); /* product */
        write_u32_le(f, sample_rate > 0 ? (uint32_t)(1000000000ULL / sample_rate) : 0);
        write_u32_le(f, root_note > 0.0f ? (uint32_t)(root_note + 0.5f) : 60);
        write_u32_le(f, 0); /* midi pitch fraction */
        write_u32_le(f, 0); /* smpte format */
        write_u32_le(f, 0); /* smpte offset */
        write_u32_le(f, has_loop ? 1 : 0);
        write_u32_le(f, 0); /* sampler data */
        if (has_loop) {
            write_u32_le(f, 0); /* cue point id */
            write_u32_le(f, 0); /* type: forward */
            write_u32_le(f, (uint32_t)loop_start);
            write_u32_le(f, (uint32_t)(loop_end > loop_start ? loop_end - 1 : loop_start));
            write_u32_le(f, 0); /* fraction */
            write_u32_le(f, 0); /* play count */
        }
    }

    /* 5. cue chunk (if cues present) */
    if (num_cues > 0 && cues) {
        uint32_t cue_size = 4 + (uint32_t)num_cues * 24;
        write_chunk_hdr(f, "cue ", cue_size);
        write_u32_le(f, (uint32_t)num_cues);
        for (int j = 0; j < num_cues; j++) {
            write_u32_le(f, (uint32_t)(j + 1));
            write_u32_le(f, (uint32_t)(j + 1));
            fwrite("data", 1, 4, f);
            write_u32_le(f, 0);
            write_u32_le(f, 0);
            write_u32_le(f, cues[j].start);
        }

        /* 6. LIST adtl chunk for labels and slice regions */
        uint32_t adtl_body_size = 4; /* "adtl" */
        for (int j = 0; j < num_cues; j++) {
            size_t txt_len = strlen(cues[j].label) + 1;
            uint32_t sub_sz = 4 + (uint32_t)txt_len;
            uint32_t pad = (sub_sz & 1) ? 1 : 0;
            adtl_body_size += 8 + sub_sz + pad;

            if (cues[j].type == SK_CUE_SLICE || cues[j].end > cues[j].start) {
                adtl_body_size += 8 + 20;
            }
        }

        write_chunk_hdr(f, "LIST", adtl_body_size);
        fwrite("adtl", 1, 4, f);
        for (int j = 0; j < num_cues; j++) {
            size_t txt_len = strlen(cues[j].label) + 1;
            uint32_t sub_sz = 4 + (uint32_t)txt_len;
            write_chunk_hdr(f, "labl", sub_sz);
            write_u32_le(f, (uint32_t)(j + 1));
            fwrite(cues[j].label, 1, txt_len, f);
            if (sub_sz & 1) {
                unsigned char pad = 0;
                fwrite(&pad, 1, 1, f);
            }

            if (cues[j].type == SK_CUE_SLICE || cues[j].end > cues[j].start) {
                write_chunk_hdr(f, "ltxt", 20);
                write_u32_le(f, (uint32_t)(j + 1));
                write_u32_le(f, cues[j].end - cues[j].start);
                fwrite("rgn ", 1, 4, f);
                write_u16_le(f, 0);
                write_u16_le(f, 0);
                write_u16_le(f, 0);
                write_u16_le(f, 0);
            }
        }
        if (adtl_body_size & 1) {
            unsigned char pad = 0;
            fwrite(&pad, 1, 1, f);
        }
    }

    /* 7. Backpatch RIFF total file size */
    long total_bytes = ftell(f);
    if (total_bytes >= 8) {
        fseek(f, 4, SEEK_SET);
        write_u32_le(f, (uint32_t)(total_bytes - 8));
    }
    fclose(f);
    free(copy);

    if (ctx) {
        ctx->printf(ctx, "# wrote %d frames (%u channel%s, %d cue%s%s) to %s at %u Hz\n",
                    frames, channels, channels == 1 ? "" : "s",
                    num_cues, num_cues == 1 ? "" : "s",
                    has_loop ? ", loop" : "",
                    filename, sample_rate);
    }
    return 1;
}

int skode_write_wav(skode_t *ctx, const char *filename,
                    const float *samples, int frames,
                    ma_uint32 channels, ma_uint32 sample_rate,
                    int normalize) {
    return skode_write_wav_riff(ctx, filename, samples, frames, channels, sample_rate,
                                normalize, 0, 0, 0, 0.0f, NULL, 0);
}

int skode_export_wave_wav(skode_t *ctx, const char *filename, int wave_slot) {
    if (!ctx || !filename || !filename[0]) {
        if (ctx) ctx->printf(ctx, "# WAV output requires [filename]\n");
        return 0;
    }
    if (!skode_wave_valid(wave_slot) || !sw.data[wave_slot] || sw.size[wave_slot] <= 0) {
        ctx->printf(ctx, "# invalid wavetable for w>w\n");
        return 0;
    }

    int frames = sw.size[wave_slot];
    int channels = (sw.channels && sw.channels[wave_slot] > 1) ? sw.channels[wave_slot] :
                   ((sw.data_right && sw.data_right[wave_slot]) ? 2 : 1);

    double stored_rate = sw.rate ? sw.rate[wave_slot] : (double)MAIN_SAMPLE_RATE;
    uint32_t sample_rate = (isfinite(stored_rate) && stored_rate >= 1.0) ?
                           (uint32_t)(stored_rate + 0.5) : MAIN_SAMPLE_RATE;

    float *interleaved = NULL;
    if (channels > 1) {
        interleaved = (float *)malloc((size_t)frames * channels * sizeof(float));
        if (!interleaved) {
            ctx->printf(ctx, "# allocation failed exporting %s\n", filename);
            return 0;
        }
        for (int i = 0; i < frames; i++) {
            for (int ch = 0; ch < channels; ch++) {
                float val = 0.0f;
                if (sw.channels_data && sw.channels_data[wave_slot] && sw.channels_data[wave_slot][ch]) {
                    val = sw.channels_data[wave_slot][ch][i];
                } else if (ch == 0) {
                    val = sw.data[wave_slot][i];
                } else if (ch == 1 && sw.data_right && sw.data_right[wave_slot]) {
                    val = sw.data_right[wave_slot][i];
                }
                interleaved[i * channels + ch] = val;
            }
        }
    }

    const float *samples = interleaved ? interleaved : sw.data[wave_slot];

    int has_loop = (sw.loop_enabled && sw.loop_enabled[wave_slot] &&
                    sw.loop_end && sw.loop_start &&
                    sw.loop_end[wave_slot] > sw.loop_start[wave_slot]) ? 1 : 0;
    int loop_start = has_loop ? sw.loop_start[wave_slot] : 0;
    int loop_end = has_loop ? sw.loop_end[wave_slot] : 0;
    float root_note = (sw.midi_note && sw.midi_note[wave_slot] > 0.0f) ? sw.midi_note[wave_slot] : 0.0f;

    int num_cues = (sw.num_cues && sw.cues && sw.cues[wave_slot]) ? sw.num_cues[wave_slot] : 0;
    const sk_cue_t *cues = num_cues > 0 ? sw.cues[wave_slot] : NULL;

    int res = skode_write_wav_riff(ctx, filename, samples, frames, (uint32_t)channels,
                                   sample_rate, 0, has_loop, loop_start, loop_end,
                                   root_note, cues, num_cues);
    if (interleaved) free(interleaved);
    return res;
}

static int record_frames_cross_zero(int a, int b) {
  int channels = sampling.channels == 2 ? 2 : 1;
  size_t ai = (size_t)a * (size_t)channels;
  size_t bi = (size_t)b * (size_t)channels;
  for (int channel = 0; channel < channels; channel++) {
    float av = sampling.where[ai + (size_t)channel];
    float bv = sampling.where[bi + (size_t)channel];
    if (av == 0.0f || av * bv <= 0.0f) return 1;
  }
  return 0;
}

static int record_trim_run_above(int start, int count, float threshold) {
  if (start < 0 || count <= 0 || start + count > sampling.len) return 0;
  for (int i = 0; i < count; i++) {
    if (record_frame_level(start + i) <= threshold) return 0;
  }
  return 1;
}

void record_find_trim(int argc, float arg0, float arg1, int margin) {
  if (atomic_load_int(&sampling.state) != SAMPLE_STATE_COMPLETE ||
      !sampling.where || sampling.len <= 0 ||
      sampling.len > sampling.capacity) {
    return;
  }
  int lead0 = -1;
  int trail0 = -1;
  float tleft = RECORD_TRIM_DEFAULT_THRESHOLD;
  float tright = RECORD_TRIM_DEFAULT_THRESHOLD;
  int run = RECORD_TRIM_CONSECUTIVE_SAMPLES;
  if (run > sampling.len) run = sampling.len;
  if (argc > 0 && isfinite(arg0)) {
    tleft = fabsf(arg0);
    tright = tleft;
  }
  if (argc > 1 && isfinite(arg1)) tright = fabsf(arg1);
  if (margin < 0) margin = 0;

  // 1. Find the first audible sample index
  int first_audible = -1;
  for (int i = 0; i <= sampling.len - run; i++) {
    if (record_trim_run_above(i, run, tleft)) {
      first_audible = i;
      break;
    }
  }

  if (first_audible > 0) {
    lead0 = first_audible - margin;
    if (lead0 < 0) lead0 = 0;
    // Look backward into silence/margin to find the closest zero crossing.
    while (lead0 > 0) {
      if (record_frames_cross_zero(lead0, lead0 - 1)) {
        break;
      }
      lead0--;
    }
  } else if (first_audible == 0) {
    lead0 = 0; // Starts immediately with audio
  }

  // 2. Find the last audible sample index
  int last_audible = -1;
  for (int i = sampling.len - run; i >= 0; i--) {
    if (record_trim_run_above(i, run, tright)) {
      last_audible = i + run - 1;
      break;
    }
  }

  if (last_audible >= 0 && last_audible < sampling.len - 1) {
    int end_idx = last_audible + margin;
    if (end_idx >= sampling.len) end_idx = sampling.len - 1;
    // Look forward into silence/margin to find the closest zero crossing.
    while (end_idx < sampling.len - 1) {
      if (record_frames_cross_zero(end_idx, end_idx + 1)) {
        break;
      }
      end_idx++;
    }
    // Calculate total trailing samples to remove from the tail end
    trail0 = sampling.len - 1 - end_idx;
  }

  if (lead0 > 0) sampling.offset = lead0;
  if (trail0 > 0) sampling.trim = trail0;
}

