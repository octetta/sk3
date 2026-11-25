#ifndef SKRED_HOST_COMMANDS_H
#define SKRED_HOST_COMMANDS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdarg.h>

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
        "  -add <path>                  Attach file/zip to virtual staging environment\n"
        "  -del <name>                  Remove attached file from staging environment\n"
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
    bool is_exit = (strcmp(cmd_line, "exit") == 0 || strcmp(cmd_line, "quit") == 0);
    bool is_slash_dev = (strncmp(cmd_line, "/als", 4) == 0 ||
                         strncmp(cmd_line, "/aout", 5) == 0 ||
                         strncmp(cmd_line, "/ain", 4) == 0 ||
                         strncmp(cmd_line, "/a?", 3) == 0 ||
                         strncmp(cmd_line, "/mls", 4) == 0 ||
                         strncmp(cmd_line, "/mout", 5) == 0 ||
                         strncmp(cmd_line, "/min", 4) == 0 ||
                         strncmp(cmd_line, "/m?", 3) == 0);

    if (!is_dot_or_dash && !is_exit && !is_slash_dev) {
        return 0; /* Forward to skred_command() */
    }

    /* Tokenize command */
    char line_copy[1024];
    strncpy(line_copy, cmd_line, sizeof(line_copy) - 1);
    line_copy[sizeof(line_copy) - 1] = '\0';

    char *saveptr = NULL;
    char *action = strtok_r(line_copy, " \t\r\n", &saveptr);
    if (!action) return 1;

    char *arg1 = strtok_r(NULL, " \t\r\n", &saveptr);
    char *arg2 = strtok_r(NULL, " \t\r\n", &saveptr);

    /* Normalize leading . or - */
    const char *norm = action;
    if (norm[0] == '.' || norm[0] == '-' || norm[0] == '/') {
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

    if (strcmp(norm, "help") == 0) {
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
        if (!arg1) {
            env->print(env->userdata, "Usage: -add <filepath>\n");
            return 1;
        }
        /* Strip surrounding brackets or quotes if present */
        char path_clean[512];
        strncpy(path_clean, arg1, sizeof(path_clean) - 1);
        path_clean[sizeof(path_clean) - 1] = '\0';
        size_t plen = strlen(path_clean);
        if (plen >= 2 && ((path_clean[0] == '[' && path_clean[plen-1] == ']') ||
                          (path_clean[0] == '"' && path_clean[plen-1] == '"') ||
                          (path_clean[0] == '\'' && path_clean[plen-1] == '\''))) {
            path_clean[plen-1] = '\0';
            memmove(path_clean, path_clean + 1, plen - 1);
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
            env->print(env->userdata, "Attached [%s] to staging area (available for /w, ks, etc.)\n", bname);
        } else {
            env->print(env->userdata, "Error: Could not copy [%s] to staging area.\n", path_clean);
        }
        return 1;
    }

    if (strcmp(norm, "del") == 0 || strcmp(norm, "rm") == 0 || strcmp(norm, "remove") == 0) {
        if (!arg1) {
            env->print(env->userdata, "Usage: -del <filename>\n");
            return 1;
        }
        char target_path[1024];
        snprintf(target_path, sizeof(target_path), "%s/%s", s_host_staging_dir, skred_host_basename(arg1));
        if (skred_unlink(target_path) == 0) {
            env->print(env->userdata, "Removed [%s] from staging area.\n", skred_host_basename(arg1));
        } else {
            env->print(env->userdata, "Error: Could not find or delete [%s] in staging area.\n", skred_host_basename(arg1));
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
