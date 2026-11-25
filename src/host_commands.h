#ifndef SKRED_HOST_COMMANDS_H
#define SKRED_HOST_COMMANDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>

#ifdef _WIN32
  #include <windows.h>
  #include <direct.h>
  #include <io.h>
  #define skred_mkdir(p) _mkdir(p)
  #define skred_unlink(p) _unlink(p)
  #ifndef strcasecmp
    #define strcasecmp _stricmp
  #endif
  #ifndef strncasecmp
    #define strncasecmp _strnicmp
  #endif
#else
  #include <unistd.h>
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <dirent.h>
  #define skred_mkdir(p) mkdir(p, 0700)
  #define skred_unlink(p) unlink(p)
#endif

#include "api.h"
#include "exp-vfs/skred_vfs.h"
#include "exp-vfs/miniz.h"
#include "exp-vfs/miniz_zip.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void (*print)(void *userdata, const char *fmt, ...);
    void (*restart)(void *userdata, int voices, int bufsize);
    void (*stop)(void *userdata);
    void (*start)(void *userdata);
    void (*clear)(void *userdata);
    void (*quit)(void *userdata);
    void *userdata;
} skred_host_env_t;

/* Global staging directory path for active host session */
static char s_host_staging_dir[512] = {0};

/* Initialize staging directory for the process */
static inline void skred_host_staging_init(void) {
    if (s_host_staging_dir[0] != '\0') return;
#ifdef _WIN32
    char temp_base[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, temp_base);
    if (len > 0 && len < MAX_PATH) {
        snprintf(s_host_staging_dir, sizeof(s_host_staging_dir), "%ssk3-%lu", temp_base, (unsigned long)GetCurrentProcessId());
    } else {
        snprintf(s_host_staging_dir, sizeof(s_host_staging_dir), "sk3-temp");
    }
#else
    snprintf(s_host_staging_dir, sizeof(s_host_staging_dir), "/tmp/sk3-%d", (int)getpid());
#endif
    skred_mkdir(s_host_staging_dir);
    skred_vfs_set_staging_dir(s_host_staging_dir);
}

/* Cleanup staging directory and any staged files on exit */
static inline void skred_host_staging_cleanup(void) {
    if (s_host_staging_dir[0] == '\0') return;
#ifdef _WIN32
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*.*", s_host_staging_dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                char filepath[MAX_PATH];
                snprintf(filepath, sizeof(filepath), "%s\\%s", s_host_staging_dir, fd.cFileName);
                DeleteFileA(filepath);
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
    RemoveDirectoryA(s_host_staging_dir);
#else
    DIR *d = opendir(s_host_staging_dir);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s/%s", s_host_staging_dir, dir->d_name);
                unlink(filepath);
            }
        }
        closedir(d);
    }
    rmdir(s_host_staging_dir);
#endif
    skred_vfs_set_staging_dir(NULL);
    s_host_staging_dir[0] = '\0';
}

static inline const char *skred_host_basename(const char *path) {
    if (!path) return "";
    const char *p1 = strrchr(path, '/');
    const char *p2 = strrchr(path, '\\');
    const char *base = p1 > p2 ? p1 : p2;
    return base ? base + 1 : path;
}

static inline void skred_host_extract_path(const char *str, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!str) return;

    while (isspace((unsigned char)*str)) str++;
    if (!*str) return;

    char raw[1024];
    size_t ri = 0;

    if (*str == '"' || *str == '\'') {
        char quote = *str++;
        while (*str && *str != quote && ri < sizeof(raw) - 1) {
            if (*str == '\\' && (*(str + 1) == quote || *(str + 1) == '\\')) {
                str++;
            }
            raw[ri++] = *str++;
        }
    } else if (*str == '[') {
        str++;
        while (*str && *str != ']' && ri < sizeof(raw) - 1) {
            raw[ri++] = *str++;
        }
    } else {
        while (*str && ri < sizeof(raw) - 1) {
            if (*str == '\\' && isspace((unsigned char)*(str + 1))) {
                str++;
            }
            raw[ri++] = *str++;
        }
        while (ri > 0 && isspace((unsigned char)raw[ri - 1])) {
            ri--;
        }
    }
    raw[ri] = '\0';

    /* Tilde (~) expansion */
    if (raw[0] == '~' && (raw[1] == '/' || raw[1] == '\\' || raw[1] == '\0')) {
#ifdef _WIN32
        const char *home = getenv("USERPROFILE");
        if (!home || !home[0]) home = getenv("HOME");
#else
        const char *home = getenv("HOME");
#endif
        if (home && home[0]) {
            if (raw[1] == '\0') {
                snprintf(out, out_sz, "%s", home);
            } else {
                snprintf(out, out_sz, "%s%s", home, raw + 1);
            }
            return;
        }
    }

    snprintf(out, out_sz, "%s", raw);
}

static inline bool skred_host_copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return false;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return false;
    }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return false;
        }
    }
    fclose(in);
    fclose(out);
    return true;
}

typedef struct {
    char name[256];
    size_t size;
} skred_staged_file_t;

/* Cached disk directory files for indexed reference */
static skred_staged_file_t s_dir_files[512];
static int s_dir_count = 0;
static char s_last_filter[128] = {0};

static inline const char *skred_host_strcasestr(const char *haystack, const char *needle) {
    if (!haystack || !needle) return NULL;
    if (!*needle) return haystack;
    size_t nlen = strlen(needle);
    while (*haystack) {
        if (strncasecmp(haystack, needle, nlen) == 0) return haystack;
        haystack++;
    }
    return NULL;
}

static inline bool skred_host_match_filter(const char *name, const char *filter) {
    if (!name || !*name) return false;
    if (!filter || !*filter) return true;

    const char *dot = strrchr(name, '.');
    const char *ext = dot ? dot + 1 : "";
    const char *f = (*filter == '.') ? filter + 1 : filter;

    /* Audio group: .wav, .mp3, .flac (case-insensitive) */
    if (strcasecmp(f, "audio") == 0 || strcasecmp(f, "wav") == 0 ||
        strcasecmp(f, "wave") == 0 || strcasecmp(f, "sound") == 0 ||
        strcasecmp(f, "sounds") == 0) {
        return (strcasecmp(ext, "wav") == 0 || strcasecmp(ext, "mp3") == 0 || strcasecmp(ext, "flac") == 0);
    }

    /* Skode group */
    if (strcasecmp(f, "sk") == 0 || strcasecmp(f, "skode") == 0) {
        return (strcasecmp(ext, "sk") == 0 || strcasecmp(ext, "skode") == 0);
    }

    /* KSynth group */
    if (strcasecmp(f, "ks") == 0 || strcasecmp(f, "ksynth") == 0) {
        return (strcasecmp(ext, "ks") == 0 || strcasecmp(ext, "ksynth") == 0);
    }

    /* Zip archive group */
    if (strcasecmp(f, "zip") == 0) {
        return (strcasecmp(ext, "zip") == 0);
    }

    /* Direct extension match (e.g. mp3, flac, mid) */
    if (strcasecmp(ext, f) == 0) {
        return true;
    }

    /* Case-insensitive substring match in filename */
    return (skred_host_strcasestr(name, filter) != NULL);
}

static inline void skred_host_seed_rand(void) {
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)(time(NULL) ^ (uintptr_t)&seeded));
        seeded = true;
    }
}

static inline void skred_host_print_file_entry(const skred_host_env_t *env, int idx, const char *name, size_t sz) {
    if (sz < 1024) env->print(env->userdata, "  %d. %s (%zu B)\n", idx, name, sz);
    else if (sz < 1024 * 1024) env->print(env->userdata, "  %d. %s (%.1f KB)\n", idx, name, (double)sz / 1024.0);
    else env->print(env->userdata, "  %d. %s (%.1f MB)\n", idx, name, (double)sz / (1024.0 * 1024.0));
}

static inline int skred_host_staging_cmp(const void *a, const void *b) {
    const skred_staged_file_t *fa = (const skred_staged_file_t *)a;
    const skred_staged_file_t *fb = (const skred_staged_file_t *)b;
    return strcasecmp(fa->name, fb->name);
}

static inline int skred_host_scan_disk_dir(skred_staged_file_t *out_files, int max_files) {
    if (!out_files || max_files <= 0) return 0;
    int count = 0;
#ifdef _WIN32
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "*.*");
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                    if (count < max_files) {
                        strncpy(out_files[count].name, fd.cFileName, sizeof(out_files[count].name) - 1);
                        out_files[count].name[sizeof(out_files[count].name) - 1] = '\0';
                        out_files[count].size = ((size_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                        count++;
                    }
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR *d = opendir(".");
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                struct stat st;
                if (stat(dir->d_name, &st) == 0 && !S_ISDIR(st.st_mode)) {
                    if (count < max_files) {
                        strncpy(out_files[count].name, dir->d_name, sizeof(out_files[count].name) - 1);
                        out_files[count].name[sizeof(out_files[count].name) - 1] = '\0';
                        out_files[count].size = (size_t)st.st_size;
                        count++;
                    }
                }
            }
        }
        closedir(d);
    }
#endif
    if (count > 1) {
        qsort(out_files, count, sizeof(skred_staged_file_t), skred_host_staging_cmp);
    }
    s_dir_count = count;
    for (int i = 0; i < count; i++) {
        s_dir_files[i] = out_files[i];
    }
    return count;
}


static inline int skred_host_staging_get_files(skred_staged_file_t *out_files, int max_files) {
    if (!out_files || max_files <= 0) return 0;
    skred_host_staging_init();
    int count = 0;
#ifdef _WIN32
    char search_pattern[MAX_PATH];
    snprintf(search_pattern, sizeof(search_pattern), "%s\\*.*", s_host_staging_dir);
    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(search_pattern, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(fd.cFileName, ".") != 0 && strcmp(fd.cFileName, "..") != 0) {
                if (count < max_files) {
                    strncpy(out_files[count].name, fd.cFileName, sizeof(out_files[count].name) - 1);
                    out_files[count].name[sizeof(out_files[count].name) - 1] = '\0';
                    out_files[count].size = ((size_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                    count++;
                }
            }
        } while (FindNextFileA(hFind, &fd));
        FindClose(hFind);
    }
#else
    DIR *d = opendir(s_host_staging_dir);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
                if (count < max_files) {
                    strncpy(out_files[count].name, dir->d_name, sizeof(out_files[count].name) - 1);
                    out_files[count].name[sizeof(out_files[count].name) - 1] = '\0';
                    char filepath[1024];
                    snprintf(filepath, sizeof(filepath), "%s/%s", s_host_staging_dir, dir->d_name);
                    struct stat st;
                    out_files[count].size = (stat(filepath, &st) == 0) ? (size_t)st.st_size : 0;
                    count++;
                }
            }
        }
        closedir(d);
    }
#endif
    if (count > 1) {
        qsort(out_files, count, sizeof(skred_staged_file_t), skred_host_staging_cmp);
    }
    return count;
}

static inline void skred_host_cmd_help(const skred_host_env_t *env) {
    env->print(env->userdata,
        "HOST ENVIRONMENT COMMANDS:\n"
        "  -restart [voices] [bufsize]  Restart audio engine (e.g. -restart 32 256)\n"
        "  -stop                        Halt audio output\n"
        "  -start                       Resume audio output\n"
        "  -status                      Display engine & audio telemetry\n"
        "  -panic                       Silence all active voices\n"
        "  -clear                       Clear terminal screen\n"
        "  -als                         List audio output and input devices\n"
        "  -aout <index>                Select audio output device\n"
        "  -ain <index|off>             Select audio input device (-ain off to disable)\n"
        "  -mls                         List MIDI input and output ports\n"
        "  -min <index|off>             Open MIDI input port (-min off to close)\n"
        "  -mout <index|off>            Open MIDI output port (-mout off to close)\n"
        "  -mstatus                     Display active MIDI port connections\n"
        "  /mv <ch> <voice> [bend]      Route MIDI channel (. = any) to voice (e.g. /mv . 0)\n"
        "  /mvd <ch> <voice>            Remove MIDI voice route\n"
        "  /mR                          Display active MIDI routes (/mC to clear)\n"
        "  -ls [filter]                 Browse disk files (e.g. -ls audio, -ls wav, -ls ks)\n"
        "  -tray                        View session staging tray (aliased: -crate, -files)\n"
        "  -add <path|indices|all>      Stage file(s) into tray (e.g. -add 0, -add 0 1, -add all)\n"
        "  -del <name|index|all>        Remove file(s) from staging tray\n"
        "  -pick <idx|?|rand> [flt] [c] Load file into string buffer and optionally run cmd\n"
        "  -zip <archive.zip>           Pack current staging tray into zip (aliased: -pack)\n"
        "  -cd [dir]                    Change directory (defaults to home ~; supports spaces)\n"
        "  -pwd                         Display current working directory\n"
        "  -help                        Show this cheat sheet\n"
        "  -quit (or exit)              Exit application\n");
}

/*
 * Returns 1 if handled by host, 0 if it should be forwarded to skred_command()
 */
static inline int skred_host_execute(const char *cmd_line, const skred_host_env_t *env) {
    if (!cmd_line || !env) return 0;
    while (isspace((unsigned char)*cmd_line)) cmd_line++;
    if (!*cmd_line) return 0;

    /* Baseline rule: strings starting with . or - are host commands */
    bool is_dot_or_dash = (*cmd_line == '.' || *cmd_line == '-');
    bool is_exit = (strcmp(cmd_line, "exit") == 0 || strcmp(cmd_line, "quit") == 0 || strcmp(cmd_line, "pwd") == 0 || strcmp(cmd_line, "help") == 0);
    bool is_slash_dev = (strncmp(cmd_line, "/als", 4) == 0 ||
                         strncmp(cmd_line, "/aout", 5) == 0 ||
                         strncmp(cmd_line, "/ain", 4) == 0 ||
                         strncmp(cmd_line, "/a?", 3) == 0 ||
                         strncmp(cmd_line, "/mls", 4) == 0 ||
                         strncmp(cmd_line, "/mout", 5) == 0 ||
                         strncmp(cmd_line, "/min", 4) == 0 ||
                         strncmp(cmd_line, "/m?", 3) == 0 ||
                         strcmp(cmd_line, "/?") == 0 ||
                         strcmp(cmd_line, "/h") == 0 ||
                         strcmp(cmd_line, "/help") == 0);

    if (!is_dot_or_dash && !is_exit && !is_slash_dev) {
        return 0; /* Forward to skred_command() */
    }

    /* Extract action and remainder of the command line */
    char action[64];
    size_t ai = 0;
    const char *p = cmd_line;
    while (*p && !isspace((unsigned char)*p) && ai < sizeof(action) - 1) {
        action[ai++] = *p++;
    }
    action[ai] = '\0';

    while (isspace((unsigned char)*p)) p++;
    const char *remainder = p;

    char arg1_buf[256] = {0};
    char arg2_buf[256] = {0};
    char *arg1 = NULL;
    char *arg2 = NULL;
    if (*remainder) {
        if (sscanf(remainder, "%255s %255s", arg1_buf, arg2_buf) >= 1) {
            arg1 = arg1_buf;
        }
        if (arg2_buf[0]) {
            arg2 = arg2_buf;
        }
    }

    /* Normalize leading ., -, or / */
    const char *norm = action;
    while (*norm == '.' || *norm == '-' || *norm == '/') {
        norm++;
    }

    if (strcmp(norm, "quit") == 0 || strcmp(norm, "exit") == 0) {
        if (env->quit) env->quit(env->userdata);
        return 1;
    }

    if (strcmp(norm, "clear") == 0) {
        if (env->clear) env->clear(env->userdata);
        return 1;
    }

    if (strcmp(norm, "help") == 0 || strcmp(norm, "h") == 0 || strcmp(norm, "?") == 0) {
        skred_host_cmd_help(env);
        return 1;
    }

    if (strcmp(norm, "panic") == 0) {
        skred_command("all_voices_gate_off");
        env->print(env->userdata, "System: Silence all voices.\n");
        return 1;
    }

    if (strcmp(norm, "stop") == 0) {
        if (env->stop) {
            env->stop(env->userdata);
        } else {
            skred_audio_disconnect();
        }
        env->print(env->userdata, "System: Audio stopped.\n");
        return 1;
    }

    if (strcmp(norm, "start") == 0) {
        if (env->start) {
            env->start(env->userdata);
        } else {
            skred_audio_reconnect();
        }
        env->print(env->userdata, "System: Audio started.\n");
        return 1;
    }

    if (strcmp(norm, "restart") == 0) {
        int voices = 0;
        int bufsize = 0;
        if (arg1) voices = atoi(arg1);
        if (arg2) bufsize = atoi(arg2);
        if (env->restart) {
            env->restart(env->userdata, voices, bufsize);
        } else {
            skred_stop();
            skred_start(bufsize > 0 ? (unsigned int)bufsize : 128,
                        voices > 0 ? (unsigned int)voices : 256,
                        0);
        }
        env->print(env->userdata, "System: Engine restarted (voices=%d, bufsize=%d).\n",
                   voices > 0 ? voices : 256, bufsize > 0 ? bufsize : 128);
        return 1;
    }

    if (strcmp(norm, "status") == 0 || strcmp(norm, "astatus") == 0 || strcmp(norm, "a?") == 0) {
        char *status = skred_audio_status();
        if (status && status[0]) {
            env->print(env->userdata, "%s\n", status);
        } else {
            env->print(env->userdata, "System: Audio engine active.\n");
        }
        return 1;
    }

    /* --- Audio Device Controls --- */
    if (strcmp(norm, "als") == 0) {
        if (skred_audio_refresh() != 0) {
            env->print(env->userdata, "Error: Audio device refresh failed.\n");
            return 1;
        }
        int out_count = skred_devices(0);
        int in_count = skred_devices(1);
        env->print(env->userdata, "# output devices\n");
        for (int i = 0; i < out_count; i++) {
            env->print(env->userdata, "  %d = %s\n", skred_device_idx(0, i), skred_device_str(0, i));
        }
        env->print(env->userdata, "# input devices\n");
        for (int i = 0; i < in_count; i++) {
            env->print(env->userdata, "  %d = %s\n", skred_device_idx(1, i), skred_device_str(1, i));
        }
        return 1;
    }

    if (strcmp(norm, "aout") == 0 || strcmp(norm, "ao") == 0) {
        if (!arg1) {
            env->print(env->userdata, "Usage: -aout <device-index>\n");
            return 1;
        }
        int idx = atoi(arg1);
        if (skred_audio_select(0, idx) == 0) {
            env->print(env->userdata, "Audio output set to device %d (%s).\n", idx, skred_device_str(0, idx));
        } else {
            env->print(env->userdata, "Error: Failed to set audio output device %d.\n", idx);
        }
        return 1;
    }

    if (strcmp(norm, "ain") == 0 || strcmp(norm, "ai") == 0) {
        if (!arg1) {
            env->print(env->userdata, "Usage: -ain <device-index|off>\n");
            return 1;
        }
        int idx = (strcasecmp(arg1, "off") == 0) ? -2 : atoi(arg1);
        if (skred_audio_select(1, idx) == 0) {
            if (idx == -2) env->print(env->userdata, "Audio input disabled.\n");
            else env->print(env->userdata, "Audio input set to device %d (%s).\n", idx, skred_device_str(1, idx));
        } else {
            env->print(env->userdata, "Error: Failed to set audio input device %s.\n", arg1);
        }
        return 1;
    }

    /* --- MIDI Device Controls --- */
    if (strcmp(norm, "mls") == 0) {
        skred_midi_init("sk3");
        int in_count = skred_midi_input_count();
        int out_count = skred_midi_output_count();
        env->print(env->userdata, "# midi inputs\n");
        if (in_count == 0) {
            env->print(env->userdata, "  (no MIDI inputs found)\n");
        } else {
            char name[128];
            for (int i = 0; i < in_count; i++) {
                skred_midi_input_name(i, name, sizeof(name));
                env->print(env->userdata, "  %d = %s\n", i, name);
            }
        }
        env->print(env->userdata, "# midi outputs\n");
        if (out_count == 0) {
            env->print(env->userdata, "  (no MIDI outputs found)\n");
        } else {
            char name[128];
            for (int i = 0; i < out_count; i++) {
                skred_midi_output_name(i, name, sizeof(name));
                env->print(env->userdata, "  %d = %s\n", i, name);
            }
        }
        return 1;
    }

    if (strcmp(norm, "min") == 0 || strcmp(norm, "mi") == 0) {
        if (!arg1) {
            env->print(env->userdata, "Usage: -min <port-index|off>\n");
            return 1;
        }
        if (strcasecmp(arg1, "off") == 0 || strcmp(arg1, "-1") == 0) {
            skred_midi_input_close();
            env->print(env->userdata, "MIDI input closed.\n");
        } else {
            int idx = atoi(arg1);
            if (skred_midi_input_open(idx) == 0) {
                char name[128] = {0};
                skred_midi_input_name(idx, name, sizeof(name));
                env->print(env->userdata, "MIDI input connected to port %d (%s).\n", idx, name);
            } else {
                env->print(env->userdata, "Error: Failed to open MIDI input port %d.\n", idx);
            }
        }
        return 1;
    }

    if (strcmp(norm, "mout") == 0 || strcmp(norm, "mo") == 0) {
        if (!arg1) {
            env->print(env->userdata, "Usage: -mout <port-index|off>\n");
            return 1;
        }
        if (strcasecmp(arg1, "off") == 0 || strcmp(arg1, "-1") == 0) {
            skred_midi_output_close();
            env->print(env->userdata, "MIDI output closed.\n");
        } else {
            int idx = atoi(arg1);
            if (skred_midi_output_open(idx) == 0) {
                char name[128] = {0};
                skred_midi_output_name(idx, name, sizeof(name));
                env->print(env->userdata, "MIDI output connected to port %d (%s).\n", idx, name);
            } else {
                env->print(env->userdata, "Error: Failed to open MIDI output port %d.\n", idx);
            }
        }
        return 1;
    }

    if (strcmp(norm, "mstatus") == 0 || strcmp(norm, "m?") == 0) {
        char *status = skred_midi_status();
        if (status && status[0]) {
            env->print(env->userdata, "%s\n", status);
        } else {
            env->print(env->userdata, "MIDI: In=%d, Out=%d\n",
                       skred_midi_input_running(), skred_midi_output_running());
        }
        return 1;
    }

    /* --- File Attachment Staging Controls --- */
    if (strcmp(norm, "add") == 0) {
        if (!remainder || !*remainder) {
            env->print(env->userdata, "Usage: -add <path | index | indices | all>\n");
            return 1;
        }

        /* Check for "all" keyword */
        if (strcasecmp(remainder, "all") == 0) {
            if (s_dir_count == 0) {
                skred_host_scan_disk_dir(s_dir_files, 512);
            }
            if (s_dir_count == 0) {
                env->print(env->userdata, "No files in current directory to add.\n");
                return 1;
            }
            skred_host_staging_init();
            int added = 0;
            const char *flt = s_last_filter[0] ? s_last_filter : NULL;
            for (int i = 0; i < s_dir_count; i++) {
                if (skred_host_match_filter(s_dir_files[i].name, flt)) {
                    char target[1024];
                    snprintf(target, sizeof(target), "%s/%s", s_host_staging_dir, s_dir_files[i].name);
                    if (skred_host_copy_file(s_dir_files[i].name, target)) {
                        added++;
                    }
                }
            }
            if (flt) {
                env->print(env->userdata, "Added %d matching file(s) (filter: %s) to staging tray.\n", added, flt);
            } else {
                env->print(env->userdata, "Added %d file(s) to staging tray.\n", added);
            }
            return 1;
        }

        /* Check if arguments are integer indices or ranges (e.g. "0", "0 1 2", "0..3", "0-3") */
        bool is_index_list = true;
        const char *chk = remainder;
        while (*chk) {
            while (isspace((unsigned char)*chk)) chk++;
            if (!*chk) break;
            if (!isdigit((unsigned char)*chk)) {
                is_index_list = false;
                break;
            }
            while (isdigit((unsigned char)*chk)) chk++;
            if (*chk == '.' && *(chk + 1) == '.') {
                chk += 2;
                if (!isdigit((unsigned char)*chk)) { is_index_list = false; break; }
                while (isdigit((unsigned char)*chk)) chk++;
            } else if (*chk == '-' && isdigit((unsigned char)*(chk + 1))) {
                chk++;
                while (isdigit((unsigned char)*chk)) chk++;
            }
            if (*chk && !isspace((unsigned char)*chk)) {
                is_index_list = false;
                break;
            }
        }

        if (is_index_list) {
            if (s_dir_count == 0) {
                skred_host_scan_disk_dir(s_dir_files, 512);
            }
            if (s_dir_count == 0) {
                env->print(env->userdata, "No files available. Run -ls to browse files first.\n");
                return 1;
            }
            skred_host_staging_init();
            int added = 0;
            const char *cur = remainder;
            while (*cur) {
                while (isspace((unsigned char)*cur)) cur++;
                if (!*cur) break;
                int start_idx = 0;
                int end_idx = 0;
                char *endp = NULL;
                start_idx = (int)strtol(cur, &endp, 10);
                end_idx = start_idx;
                if (endp && *endp == '.' && *(endp + 1) == '.') {
                    endp += 2;
                    end_idx = (int)strtol(endp, &endp, 10);
                } else if (endp && *endp == '-' && isdigit((unsigned char)*(endp + 1))) {
                    endp++;
                    end_idx = (int)strtol(endp, &endp, 10);
                }
                cur = endp;

                if (start_idx > end_idx) { int tmp = start_idx; start_idx = end_idx; end_idx = tmp; }
                for (int i = start_idx; i <= end_idx; i++) {
                    if (i >= 0 && i < s_dir_count) {
                        char target[1024];
                        snprintf(target, sizeof(target), "%s/%s", s_host_staging_dir, s_dir_files[i].name);
                        if (skred_host_copy_file(s_dir_files[i].name, target)) {
                            env->print(env->userdata, "Attached [%s] to staging tray\n", s_dir_files[i].name);
                            added++;
                        }
                    } else {
                        env->print(env->userdata, "Error: Index %d out of range (0..%d).\n", i, s_dir_count - 1);
                    }
                }
            }
            return 1;
        }

        /* Otherwise, treat as file path */
        char path_clean[1024] = {0};
        skred_host_extract_path(remainder, path_clean, sizeof(path_clean));
        if (path_clean[0] == '\0') {
            env->print(env->userdata, "Usage: -add <filepath>\n");
            return 1;
        }

        /* Check if zip file */
        const char *ext = strrchr(path_clean, '.');
        if (ext && strcasecmp(ext, ".zip") == 0) {
            if (skred_vfs_mount(path_clean)) {
                env->print(env->userdata, "Mounted archive [%s] to VFS.\n", skred_host_basename(path_clean));
                return 1;
            }
        }

        /* Copy file into staging dir */
        skred_host_staging_init();
        const char *bname = skred_host_basename(path_clean);
        char target_path[1024];
        snprintf(target_path, sizeof(target_path), "%s/%s", s_host_staging_dir, bname);
        if (skred_host_copy_file(path_clean, target_path)) {
            env->print(env->userdata, "Attached [%s] to staging area (available for /ws, /ls, /ks, etc.)\n", bname);
        } else {
            env->print(env->userdata, "Error: Could not copy [%s] to staging area.\n", path_clean);
        }
        return 1;
    }

    if (strcmp(norm, "del") == 0 || strcmp(norm, "rm") == 0 || strcmp(norm, "remove") == 0) {
        char del_name[1024] = {0};
        skred_host_extract_path(remainder, del_name, sizeof(del_name));
        if (del_name[0] == '\0') {
            env->print(env->userdata, "Usage: -del <filename|index|all>\n");
            return 1;
        }

        if (strcasecmp(del_name, "all") == 0) {
            skred_host_staging_cleanup();
            skred_host_staging_init();
            env->print(env->userdata, "Cleared all files from staging tray.\n");
            return 1;
        }

        const char *target_name = del_name;
        char *endptr = NULL;
        long num_idx = strtol(del_name, &endptr, 10);
        skred_staged_file_t files[256];
        if (endptr && *endptr == '\0' && num_idx >= 0) {
            int count = skred_host_staging_get_files(files, 256);
            if (num_idx < count) {
                target_name = files[num_idx].name;
            } else {
                env->print(env->userdata, "Error: Index %ld out of range (0..%d). Run -tray to view files.\n",
                           num_idx, count > 0 ? count - 1 : 0);
                return 1;
            }
        }

        char target_path[1024];
        snprintf(target_path, sizeof(target_path), "%s/%s", s_host_staging_dir, skred_host_basename(target_name));
        if (skred_unlink(target_path) == 0) {
            env->print(env->userdata, "Removed [%s] from staging area.\n", skred_host_basename(target_name));
        } else {
            env->print(env->userdata, "Error: Could not find or delete [%s] in staging area.\n", skred_host_basename(target_name));
        }
        return 1;
    }

    /* --- File Selection Into String Buffer --- */
    if (strcmp(norm, "pick") == 0 || strcmp(norm, "get") == 0 || strcmp(norm, "use") == 0 || strcmp(norm, "file") == 0) {
        if (!arg1) {
            env->print(env->userdata, "Usage: -pick <index|?|rand> [filter] [command] (e.g. -pick 0 /ws 0 or -pick ? audio /ws 0)\n");
            return 1;
        }

        char chosen_file[256] = {0};
        const char *tail = NULL;
        bool is_random = (strcmp(arg1, "?") == 0 || strcasecmp(arg1, "rand") == 0 || strcasecmp(arg1, "random") == 0);

        if (is_random) {
            skred_host_seed_rand();
            const char *filter = NULL;
            char filter_buf[128] = {0};
            const char *rest = remainder;
            while (*rest && isspace((unsigned char)*rest)) rest++;
            while (*rest && !isspace((unsigned char)*rest)) rest++;
            while (*rest && isspace((unsigned char)*rest)) rest++;

            if (*rest) {
                if (*rest == '/' || *rest == '-' || *rest == '.') {
                    tail = rest;
                } else {
                    size_t fi = 0;
                    while (*rest && !isspace((unsigned char)*rest) && fi < sizeof(filter_buf) - 1) {
                        filter_buf[fi++] = *rest++;
                    }
                    filter_buf[fi] = '\0';
                    filter = filter_buf;
                    while (*rest && isspace((unsigned char)*rest)) rest++;
                    if (*rest) tail = rest;
                }
            }

            /* Gather candidates from staging tray */
            skred_staged_file_t tray_files[256];
            int tray_count = skred_host_staging_get_files(tray_files, 256);
            int candidates[512];
            int num_cand = 0;
            for (int i = 0; i < tray_count; i++) {
                if (skred_host_match_filter(tray_files[i].name, filter)) {
                    candidates[num_cand++] = i;
                }
            }

            if (num_cand > 0) {
                int r_idx = candidates[rand() % num_cand];
                snprintf(chosen_file, sizeof(chosen_file), "%s", tray_files[r_idx].name);
            } else {
                /* Tray has no matches; fall back to disk browser */
                if (s_dir_count == 0) {
                    skred_host_scan_disk_dir(s_dir_files, 512);
                }
                int disk_cand[512];
                int num_disk_cand = 0;
                for (int i = 0; i < s_dir_count; i++) {
                    if (skred_host_match_filter(s_dir_files[i].name, filter)) {
                        disk_cand[num_disk_cand++] = i;
                    }
                }
                if (num_disk_cand > 0) {
                    int r_idx = disk_cand[rand() % num_disk_cand];
                    snprintf(chosen_file, sizeof(chosen_file), "%s", s_dir_files[r_idx].name);
                } else {
                    env->print(env->userdata, "Error: No files found matching filter '%s'.\n", filter ? filter : "*");
                    return 1;
                }
            }
        } else {
            int idx = atoi(arg1);
            skred_staged_file_t files[256];
            int count = skred_host_staging_get_files(files, 256);
            if (count > 0 && idx >= 0 && idx < count) {
                snprintf(chosen_file, sizeof(chosen_file), "%s", files[idx].name);
            } else if (s_dir_count > 0 && idx >= 0 && idx < s_dir_count) {
                snprintf(chosen_file, sizeof(chosen_file), "%s", s_dir_files[idx].name);
            } else {
                env->print(env->userdata, "Error: Index %d out of range (0..%d). Run -tray or -ls to view files.\n",
                           idx, (count > 0 ? count : (s_dir_count > 0 ? s_dir_count : 1)) - 1);
                return 1;
            }

            const char *p = remainder;
            while (*p && isspace((unsigned char)*p)) p++;
            while (*p && !isspace((unsigned char)*p)) p++;
            while (*p && isspace((unsigned char)*p)) p++;
            if (*p) tail = p;
        }

        skred_set_string(chosen_file);

        if (tail && *tail) {
            env->print(env->userdata, "String buffer: [%s] -> executing '%s'\n", chosen_file, tail);
            char run_buf[4096];
            snprintf(run_buf, sizeof(run_buf), "%s", tail);
            int r = skred_command(run_buf);
            char *log = skred_log();
            if (log && log[0]) env->print(env->userdata, "%s", log);
            if (r > 0) env->print(env->userdata, "r = %d\n", r);
        } else {
            env->print(env->userdata, "String buffer: [%s] (ready for /ws, /ls, /ks, etc.)\n", chosen_file);
        }
        return 1;
    }

    /* --- Session Staging Tray Inspection --- */
    if (strcmp(norm, "tray") == 0 || strcmp(norm, "crate") == 0 ||
        strcmp(norm, "files") == 0 || strcmp(norm, "staged") == 0) {
        skred_staged_file_t files[256];
        int count = skred_host_staging_get_files(files, 256);

        env->print(env->userdata, "# [tray: %s] (%d files)\n", s_host_staging_dir, count);
        if (count == 0) {
            env->print(env->userdata, "  (staging tray is empty; use -add to stage files)\n");
        } else {
            for (int i = 0; i < count; i++) {
                skred_host_print_file_entry(env, i, files[i].name, files[i].size);
            }
        }
        return 1;
    }

    /* --- Disk Browser --- */
    if (strcmp(norm, "ls") == 0) {
        /* If arg1 is purely digits and in range of s_dir_files, pick convenience */
        char *endptr = NULL;
        long idx = (arg1 && *arg1) ? strtol(arg1, &endptr, 10) : -1;
        if (arg1 && endptr && *endptr == '\0' && idx >= 0 && s_dir_count > 0 && idx < s_dir_count) {
            const char *fname = s_dir_files[idx].name;
            skred_set_string(fname);
            const char *tail = remainder;
            while (*tail && isspace((unsigned char)*tail)) tail++;
            while (*tail && !isspace((unsigned char)*tail)) tail++;
            while (*tail && isspace((unsigned char)*tail)) tail++;
            if (*tail) {
                env->print(env->userdata, "String buffer: [%s] -> executing '%s'\n", fname, tail);
                char run_buf[4096];
                snprintf(run_buf, sizeof(run_buf), "%s", tail);
                int r = skred_command(run_buf);
                char *log = skred_log();
                if (log && log[0]) env->print(env->userdata, "%s", log);
                if (r > 0) env->print(env->userdata, "r = %d\n", r);
            } else {
                env->print(env->userdata, "String buffer: [%s] (ready for /ws, /ls, /ks, etc.)\n", fname);
            }
            return 1;
        }

        const char *filter = arg1;
        skred_host_scan_disk_dir(s_dir_files, 512);

        char cwd[1024];
        if (!getcwd(cwd, sizeof(cwd))) snprintf(cwd, sizeof(cwd), ".");

        if (filter && *filter) {
            strncpy(s_last_filter, filter, sizeof(s_last_filter) - 1);
            s_last_filter[sizeof(s_last_filter) - 1] = '\0';
            int match_count = 0;
            for (int i = 0; i < s_dir_count; i++) {
                if (skred_host_match_filter(s_dir_files[i].name, filter)) {
                    match_count++;
                }
            }
            env->print(env->userdata, "# [disk: %s] (filter: %s, %d of %d files)\n", cwd, filter, match_count, s_dir_count);
            if (match_count == 0) {
                env->print(env->userdata, "  (no matching files in working directory)\n");
            } else {
                for (int i = 0; i < s_dir_count; i++) {
                    if (skred_host_match_filter(s_dir_files[i].name, filter)) {
                        skred_host_print_file_entry(env, i, s_dir_files[i].name, s_dir_files[i].size);
                    }
                }
            }
        } else {
            s_last_filter[0] = '\0';
            env->print(env->userdata, "# [disk: %s] (%d files)\n", cwd, s_dir_count);
            if (s_dir_count == 0) {
                env->print(env->userdata, "  (no files in working directory)\n");
            } else {
                for (int i = 0; i < s_dir_count; i++) {
                    skred_host_print_file_entry(env, i, s_dir_files[i].name, s_dir_files[i].size);
                }
            }
        }
        if (skred_vfs_mode() != SKRED_VFS_DISK) {
            env->print(env->userdata, "# mounted archive: %s (cwd: %s)\n", skred_vfs_root(), skred_getcwd());
        }
        return 1;
    }

    /* --- Pack Staging Tray into Zip Archive --- */
    if (strcmp(norm, "zip") == 0 || strcmp(norm, "pack") == 0) {
        char zip_name[1024] = {0};
        skred_host_extract_path(remainder, zip_name, sizeof(zip_name));
        if (zip_name[0] == '\0') {
            snprintf(zip_name, sizeof(zip_name), "session.zip");
        }
        if (!strstr(zip_name, ".zip") && !strstr(zip_name, ".ZIP")) {
            strncat(zip_name, ".zip", sizeof(zip_name) - strlen(zip_name) - 1);
        }

        skred_staged_file_t files[256];
        int count = skred_host_staging_get_files(files, 256);
        if (count == 0) {
            env->print(env->userdata, "Error: Staging tray is empty. Use -add to stage files before zipping.\n");
            return 1;
        }

        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        if (!mz_zip_writer_init_file(&zip, zip_name, 0)) {
            env->print(env->userdata, "Error: Failed to create zip file '%s'.\n", zip_name);
            return 1;
        }

        int added = 0;
        for (int i = 0; i < count; i++) {
            char src_path[1024];
            snprintf(src_path, sizeof(src_path), "%s/%s", s_host_staging_dir, files[i].name);
            if (mz_zip_writer_add_file(&zip, files[i].name, src_path, NULL, 0, MZ_DEFAULT_LEVEL)) {
                added++;
            }
        }

        if (!mz_zip_writer_finalize_archive(&zip)) {
            mz_zip_writer_end(&zip);
            env->print(env->userdata, "Error: Failed to finalize zip archive '%s'.\n", zip_name);
            return 1;
        }
        mz_zip_writer_end(&zip);
        env->print(env->userdata, "Packed %d file(s) from staging tray into [%s].\n", added, zip_name);
        return 1;
    }

    /* --- Directory Navigation --- */
    if (strcmp(norm, "cd") == 0) {
        char target_dir[1024] = {0};
        skred_host_extract_path(remainder, target_dir, sizeof(target_dir));
        if (target_dir[0] == '\0') {
#ifdef _WIN32
            const char *home = getenv("USERPROFILE");
            if (!home || !home[0]) home = getenv("HOME");
#else
            const char *home = getenv("HOME");
#endif
            if (home && home[0]) {
                snprintf(target_dir, sizeof(target_dir), "%s", home);
            } else {
                snprintf(target_dir, sizeof(target_dir), "/");
            }
        }

        s_dir_count = 0; /* Invalidate cached disk directory */
        s_last_filter[0] = '\0';

        if (skred_vfs_mode() != SKRED_VFS_DISK) {
            if (skred_chdir(target_dir)) {
                env->print(env->userdata, "VFS directory changed: %s (cwd: %s)\n", skred_vfs_root(), skred_getcwd());
            } else {
                env->print(env->userdata, "Error: VFS directory '%s' not found in archive.\n", target_dir);
            }
            return 1;
        }

#ifdef _WIN32
        int chdir_res = _chdir(target_dir);
#else
        int chdir_res = chdir(target_dir);
#endif
        if (chdir_res == 0) {
            skred_chdir(target_dir);
            char cwd[1024];
            if (getcwd(cwd, sizeof(cwd))) {
                env->print(env->userdata, "Current directory: %s\n", cwd);
            } else {
                env->print(env->userdata, "Current directory changed to %s\n", target_dir);
            }
        } else {
            env->print(env->userdata, "Error: Cannot change directory to '%s'\n", target_dir);
        }
        return 1;
    }

    if (strcmp(norm, "pwd") == 0) {
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd))) {
            env->print(env->userdata, "Working directory: %s\n", cwd);
        }
        if (skred_vfs_mode() != SKRED_VFS_DISK) {
            env->print(env->userdata, "VFS root: %s (cwd: %s)\n", skred_vfs_root(), skred_getcwd());
        }
        return 1;
    }

    /* Fallback for unrecognized host command */
    env->print(env->userdata, "Unknown host command: %s. Type -help for available commands.\n", action);
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SKRED_HOST_COMMANDS_H */
