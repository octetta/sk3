#include "api.h"
#include "skred.h"
#include "synth-state.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "vendor/uedit/uedit.h"
#include "miniz.h"

static void usage(void) {
  printf("Usage: sk3 [options] [file.sk | file.zip]\n");
  printf("  [file]                   Load .sk script or mount .zip bundle at startup\n");
  printf("  -c <command>             Execute Skode command after loading file\n");
  printf("  -n                       Disable interactive line editor (for subprocess mode)\n");
  printf("  -p<udp-port>             UDP control port (0 = disable UDP, default 60440)\n");
  printf("  -e<events-port>          UDP events broadcast port (0 = disable, default 0)\n");
  printf("  -H<http-port>            HTTP POST control port (0 = disable HTTP, default 60440)\n");
  printf("  -o<device-id>            Select output audio device\n");
  printf("  -i<device-id>            Select input audio device\n");
  printf("  -l                       List available audio input and output devices\n");
  printf("  -v<voice-count>          Voice count (1 to 256, default 256)\n");
  printf("  -r<frames>               Requested buffer frame size (default 128)\n");
  exit(1);
}

// Standard Base64 Encoding Table
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// Helper to base64 encode a buffer
static void base64_encode(const unsigned char *src, size_t len, char *out) {
  size_t i = 0, j = 0;
  for (i = 0; i < len; i += 3) {
    uint32_t octet_a = i < len ? src[i] : 0;
    uint32_t octet_b = i + 1 < len ? src[i + 1] : 0;
    uint32_t octet_c = i + 2 < len ? src[i + 2] : 0;
    uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

    out[j++] = b64_table[(triple >> 18) & 0x3F];
    out[j++] = b64_table[(triple >> 12) & 0x3F];
    out[j++] = i + 1 < len ? b64_table[(triple >> 6) & 0x3F] : '=';
    out[j++] = i + 2 < len ? b64_table[triple & 0x3F] : '=';
  }
  out[j] = '\0';
}

#define WAVE_CHUNK_SIZE 200

static int handle_wave_command(const char *line) {
  int wave_idx = 0;
  sscanf(line, "-wave %d", &wave_idx);

  // Bounds check (assume wave_idx 0-255 is safe, or check sw.data)
  if (wave_idx < 0 || wave_idx >= synth_config.wave_table_max) return 1;

  float *wave_data_f = sw.data[wave_idx];
  size_t wave_len = sw.size[wave_idx] * sizeof(float);

  if (!wave_data_f || wave_len == 0) {
    printf("~WAVE:START\n~WAVE:END\n");
    return 1;
  }

  uLongf comp_len = compressBound(wave_len);
  unsigned char *comp_data = malloc(comp_len);

  if (compress(comp_data, &comp_len, (const unsigned char*)wave_data_f, wave_len) == MZ_OK) {
    size_t b64_len = 4 * ((comp_len + 2) / 3) + 1;
    char *b64_str = malloc(b64_len);
    base64_encode(comp_data, comp_len, b64_str);

    size_t remaining = strlen(b64_str);
    size_t offset = 0;
    char chunk_buf[WAVE_CHUNK_SIZE + 32];

    printf("~WAVE:START\n");

    while (remaining > 0) {
      size_t to_copy = remaining > WAVE_CHUNK_SIZE ? WAVE_CHUNK_SIZE : remaining;
      snprintf(chunk_buf, sizeof(chunk_buf), "~WAVE:%.*s\n", (int)to_copy, b64_str + offset);
      printf("%s", chunk_buf);
      offset += to_copy;
      remaining -= to_copy;
    }

    printf("~WAVE:END\n");
    free(b64_str);
  }
  free(comp_data);
  return 1;
}

static int is_zip_file(const char *path) {
  if (!path) return 0;
  const char *ext = strrchr(path, '.');
  if (ext && (strcasecmp(ext, ".zip") == 0)) return 1;
  FILE *f = fopen(path, "rb");
  if (!f) return 0;
  unsigned char hdr[4];
  size_t n = fread(hdr, 1, 4, f);
  fclose(f);
  return (n >= 4 && hdr[0] == 'P' && hdr[1] == 'K' &&
          (hdr[2] == 3 || hdr[2] == 5 || hdr[2] == 7));
}

static int run_command(const char *line) {
  if (strncmp(line, "-wave", 5) == 0) {
    return handle_wave_command(line);
  }

  char buf[4096];
  snprintf(buf, sizeof(buf), "%s", line);
  int r = skred_command(buf);
  char *log = skred_log();
  if (strlen(log)) printf("%s", log);
  if (r > 0) printf("r = %d\n", r);
  return r;
}

int main(int argc, char **argv) {
  int use_editor = 1;
  unsigned int voice_count = 256;
  unsigned int requested_frames = 128;
  int udp_port = 60440;
  int events_port = 0; // Default off
  int http_port = 60440; // Default 60440 (0 = disable HTTP)
  int output_device = -1;
  int input_device = -1;
  const char *file_to_load = NULL;
  const char *cmd_to_run = NULL;

  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-') {
      if (strcmp(argv[i], "-n") == 0) {
        use_editor = 0;
      } else if (strcmp(argv[i], "-l") == 0) {
        skred_enumerate_devices(0);
        skred_enumerate_devices(1);
        printf("# Output Devices\n");
        for (int d = 0; d < skred_devices(0); d++) printf("%d %s\n", skred_device_idx(0, d), skred_device_str(0, d));
        printf("# Input Devices\n");
        for (int d = 0; d < skred_devices(1); d++) printf("%d %s\n", skred_device_idx(1, d), skred_device_str(1, d));
        exit(0);
      } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
        usage();
      } else if (strcmp(argv[i], "-c") == 0) {
        if (i + 1 < argc) {
          cmd_to_run = argv[++i];
        } else {
          usage();
        }
      } else if (strncmp(argv[i], "-c", 2) == 0 && argv[i][2] != '\0') {
        cmd_to_run = argv[i] + 2;
      } else if (strcmp(argv[i], "-v") == 0) {
        if (i + 1 < argc) voice_count = (unsigned int)atoi(argv[++i]);
        else usage();
      } else if (strncmp(argv[i], "-v", 2) == 0 && argv[i][2] != '\0') {
        voice_count = (unsigned int)atoi(&argv[i][2]);
      } else if (strcmp(argv[i], "-r") == 0) {
        if (i + 1 < argc) requested_frames = (unsigned int)atoi(argv[++i]);
        else usage();
      } else if (strncmp(argv[i], "-r", 2) == 0 && argv[i][2] != '\0') {
        requested_frames = (unsigned int)atoi(&argv[i][2]);
      } else if (strcmp(argv[i], "-p") == 0) {
        if (i + 1 < argc) udp_port = (int)strtol(argv[++i], NULL, 0);
        else usage();
      } else if (strncmp(argv[i], "-p", 2) == 0 && argv[i][2] != '\0') {
        udp_port = (int)strtol(&argv[i][2], NULL, 0);
      } else if (strcmp(argv[i], "-e") == 0) {
        if (i + 1 < argc) events_port = (int)strtol(argv[++i], NULL, 0);
        else usage();
      } else if (strncmp(argv[i], "-e", 2) == 0 && argv[i][2] != '\0') {
        events_port = (int)strtol(&argv[i][2], NULL, 0);
      } else if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--http") == 0) {
        if (i + 1 < argc) http_port = (int)strtol(argv[++i], NULL, 0);
        else usage();
      } else if (strncmp(argv[i], "-H", 2) == 0 && argv[i][2] != '\0') {
        http_port = (int)strtol(&argv[i][2], NULL, 0);
      } else if (strcmp(argv[i], "-i") == 0) {
        skred_enumerate_devices(1);
        if (i + 1 < argc) input_device = atoi(argv[++i]);
        else usage();
      } else if (strncmp(argv[i], "-i", 2) == 0 && argv[i][2] != '\0') {
        skred_enumerate_devices(1);
        input_device = atoi(&argv[i][2]);
      } else if (strcmp(argv[i], "-o") == 0) {
        skred_enumerate_devices(0);
        if (i + 1 < argc) output_device = atoi(argv[++i]);
        else usage();
      } else if (strncmp(argv[i], "-o", 2) == 0 && argv[i][2] != '\0') {
        skred_enumerate_devices(0);
        output_device = atoi(&argv[i][2]);
      } else {
        usage();
      }
    } else {
      if (!file_to_load) {
        file_to_load = argv[i];
      } else {
        usage();
      }
    }
  }

  if (file_to_load) {
    FILE *f = fopen(file_to_load, "rb");
    if (!f) {
      fprintf(stderr, "sk3: error: cannot open '%s'\n", file_to_load);
      return 1;
    }
    fclose(f);
  }

  if (!use_editor) {
    // Force line buffering: buffers are flushed at every '\n'
    setvbuf(stdin,  NULL, _IOLBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);
  }

  printf("# sk3 v%s with %s\n", skred_version(), skred_features());

  skred_set_audio_device(output_device, input_device);

  if (skred_start(requested_frames, voice_count, udp_port) != 0) {
    return 1;
  }

  if (events_port > 0) {
    skred_udp_events_start(events_port);
  }

  if (http_port > 0) {
    int hp = skred_http_start(http_port);
    if (hp > 0) {
      printf("# sk3 HTTP POST server listening on port %d\n", hp);
    }
  }

  skred_logger(1);

  if (file_to_load) {
    char load_cmd[4096];
    if (is_zip_file(file_to_load)) {
      snprintf(load_cmd, sizeof(load_cmd), "[%s] %%z", file_to_load);
    } else {
      snprintf(load_cmd, sizeof(load_cmd), "[%s] /ls", file_to_load);
    }
    run_command(load_cmd);
  }

  if (cmd_to_run) {
    run_command(cmd_to_run);
  }

  if (use_editor) {
    uedit_load_history(HISTORY_FILE);
  }

  while (1) {
    char line[1024];
    char *out = NULL;
    if (use_editor) {
      int r = uedit("# ", line, sizeof(line) - 1);
      if (r == 0) continue;
      if (r < 0) break;
      out = line;
    } else {
      out = fgets(line, (int)sizeof(line) - 1, stdin);
      if (out == NULL) break;
      if (strlen(line) == 0) continue;
    }
    if (strlen(out) == 0) continue;
    int r = run_command(out);
    if (r == 0) continue;
    if (r < 0) break;
  }

  skred_control_dispatch_stop();
  skred_udp_events_stop();
  skred_stop();

  if (use_editor) {
    uedit_save_history(HISTORY_FILE);
  }

  return 0;
}
