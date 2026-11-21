#ifndef SK3_GUI_COMMON_H
#define SK3_GUI_COMMON_H

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#ifdef _WIN32
  #include <direct.h>
  #ifndef strcasecmp
    #define strcasecmp _stricmp
  #endif
#else
  #include <sys/stat.h>
  #include <sys/types.h>
  #include <unistd.h>
  #include <strings.h>
#endif

/* Forward declaration for engine API */
int skred_set_string(const char *str);

typedef enum {
    SK3_FONT_SIZE_SMALL = 0,   /* 11 pt */
    SK3_FONT_SIZE_MEDIUM = 1,  /* 14 pt */
    SK3_FONT_SIZE_LARGE = 2,   /* 18 pt */
    SK3_FONT_SIZE_COUNT = 3
} sk3_font_size_t;

typedef enum {
    SK3_THEME_DARK = 0,    /* Dark charcoal + light grey */
    SK3_THEME_AMBER = 1,   /* Retro CRT Amber on black */
    SK3_THEME_GREEN = 2,   /* Retro Matrix Green on black */
    SK3_THEME_LIGHT = 3,   /* Clean light background */
    SK3_THEME_COUNT = 4
} sk3_theme_t;

static inline const char *sk3_font_size_name(sk3_font_size_t sz) {
    switch (sz) {
        case SK3_FONT_SIZE_SMALL:  return "Font: S (11px)";
        case SK3_FONT_SIZE_MEDIUM: return "Font: M (14px)";
        case SK3_FONT_SIZE_LARGE:  return "Font: L (18px)";
        default: return "Font: M";
    }
}

static inline const char *sk3_theme_name(sk3_theme_t th) {
    switch (th) {
        case SK3_THEME_DARK:  return "Theme: Dark";
        case SK3_THEME_AMBER: return "Theme: Amber";
        case SK3_THEME_GREEN: return "Theme: Green";
        case SK3_THEME_LIGHT: return "Theme: Light";
        default: return "Theme: Dark";
    }
}

/* Format status text omitting ports that were not opened */
static inline void sk3_gui_format_status(int audio_ok, int udp_port_opened, int tcp_port_opened,
                                         const char *version, char *out_status, size_t out_status_sz) {
    if (!audio_ok) {
        snprintf(out_status, out_status_sz, "AUDIO START FAILED");
        return;
    }
    if (udp_port_opened > 0 && tcp_port_opened > 0) {
        snprintf(out_status, out_status_sz, "RUNNING: sk3 v%s (UDP: %d, TCP: %d)",
                 version, udp_port_opened, tcp_port_opened);
    } else if (udp_port_opened > 0) {
        snprintf(out_status, out_status_sz, "RUNNING: sk3 v%s (UDP: %d)",
                 version, udp_port_opened);
    } else if (tcp_port_opened > 0) {
        snprintf(out_status, out_status_sz, "RUNNING: sk3 v%s (TCP: %d)",
                 version, tcp_port_opened);
    } else {
        snprintf(out_status, out_status_sz, "RUNNING: sk3 v%s", version);
    }
}

/* Helper to store a dropped file path into the skred context's ANDS string buffer */
static inline void sk3_gui_format_file_drop(const char *filepath, char *out_log, size_t out_log_sz,
                                            char *out_cmd, size_t out_cmd_sz) {
    if (!filepath || !filepath[0]) return;
    skred_set_string(filepath);
    if (out_log && out_log_sz > 0) out_log[0] = '\0';
    if (out_cmd && out_cmd_sz > 0) out_cmd[0] = '\0';
}

/* Configuration Persistence */
typedef struct {
    int theme;             /* 0: Dark, 1: Amber, 2: Green, 3: Light */
    int font_size;         /* 0: Small, 1: Medium, 2: Large */
    int window_width;      /* e.g. 700 */
    int window_height;     /* e.g. 560 or collapsed */
    int prev_expanded_h;   /* e.g. 560 */
    int console_collapsed; /* 0: show (expanded), 1: hide (collapsed) */
} sk3_gui_config_t;

static inline void sk3_gui_ensure_dir(const char *dir) {
#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static inline void sk3_gui_get_config_path(char *out_path, size_t out_path_sz) {
    if (!out_path || out_path_sz == 0) return;
    out_path[0] = '\0';
#ifdef _WIN32
    const char *appdata = getenv("APPDATA");
    char dir[512];
    if (appdata && appdata[0]) {
        snprintf(dir, sizeof(dir), "%s\\sk3", appdata);
        sk3_gui_ensure_dir(dir);
        snprintf(out_path, out_path_sz, "%s\\sk3-gui.conf", dir);
    } else {
        snprintf(out_path, out_path_sz, "sk3-gui.conf");
    }
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    char dir[1024];
    if (home && home[0]) {
        snprintf(dir, sizeof(dir), "%s/Library/Application Support", home);
        sk3_gui_ensure_dir(dir);
        snprintf(dir, sizeof(dir), "%s/Library/Application Support/sk3", home);
        sk3_gui_ensure_dir(dir);
        snprintf(out_path, out_path_sz, "%s/sk3-gui.conf", dir);
    } else {
        snprintf(out_path, out_path_sz, "sk3-gui.conf");
    }
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    char dir[1024];
    if (xdg && xdg[0]) {
        snprintf(dir, sizeof(dir), "%s/sk3", xdg);
        sk3_gui_ensure_dir(dir);
        snprintf(out_path, out_path_sz, "%s/sk3-gui.conf", dir);
    } else {
        const char *home = getenv("HOME");
        if (home && home[0]) {
            snprintf(dir, sizeof(dir), "%s/.config", home);
            sk3_gui_ensure_dir(dir);
            snprintf(dir, sizeof(dir), "%s/.config/sk3", home);
            sk3_gui_ensure_dir(dir);
            snprintf(out_path, out_path_sz, "%s/sk3-gui.conf", dir);
        } else {
            snprintf(out_path, out_path_sz, "sk3-gui.conf");
        }
    }
#endif
}

static inline void sk3_gui_config_init_defaults(sk3_gui_config_t *cfg) {
    if (!cfg) return;
    cfg->theme = (int)SK3_THEME_DARK;
    cfg->font_size = (int)SK3_FONT_SIZE_MEDIUM;
    cfg->window_width = 700;
    cfg->window_height = 560;
    cfg->prev_expanded_h = 560;
    cfg->console_collapsed = 0;
}

static inline int sk3_gui_config_save(const sk3_gui_config_t *cfg) {
    if (!cfg) return -1;
    char path[1024];
    sk3_gui_get_config_path(path, sizeof(path));
    if (!path[0]) return -1;

    FILE *f = fopen(path, "w");
    if (!f) return -1;

    const char *th_str = (cfg->theme == SK3_THEME_AMBER) ? "amber" :
                         (cfg->theme == SK3_THEME_GREEN) ? "green" :
                         (cfg->theme == SK3_THEME_LIGHT) ? "light" : "dark";
    const char *fs_str = (cfg->font_size == SK3_FONT_SIZE_SMALL) ? "small" :
                         (cfg->font_size == SK3_FONT_SIZE_LARGE) ? "large" : "medium";

    fprintf(f, "# sk3-gui configuration\n");
    fprintf(f, "theme=%s\n", th_str);
    fprintf(f, "font_size=%s\n", fs_str);
    fprintf(f, "window_width=%d\n", cfg->window_width > 0 ? cfg->window_width : 700);
    fprintf(f, "window_height=%d\n", cfg->window_height > 0 ? cfg->window_height : 560);
    fprintf(f, "prev_expanded_h=%d\n", cfg->prev_expanded_h > 0 ? cfg->prev_expanded_h : 560);
    fprintf(f, "console_collapsed=%d\n", cfg->console_collapsed ? 1 : 0);
    fclose(f);
    return 0;
}

static inline int sk3_gui_config_load(sk3_gui_config_t *cfg) {
    if (!cfg) return -1;
    char path[1024];
    sk3_gui_get_config_path(path, sizeof(path));
    if (!path[0]) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || *p == '\r' || *p == '\n' || !*p) continue;

        char *eq = strchr(p, '=');
        if (!eq) eq = strchr(p, ':');
        if (!eq) continue;

        *eq = '\0';
        char *key = p;
        char *val = eq + 1;

        /* Trim key */
        char *kend = key + strlen(key) - 1;
        while (kend >= key && (*kend == ' ' || *kend == '\t' || *kend == '\r' || *kend == '\n')) *kend-- = '\0';

        /* Trim val */
        while (*val == ' ' || *val == '\t') val++;
        char *vend = val + strlen(val) - 1;
        while (vend >= val && (*vend == ' ' || *vend == '\t' || *vend == '\r' || *vend == '\n')) *vend-- = '\0';

        if (strcasecmp(key, "theme") == 0) {
            if (strcasecmp(val, "dark") == 0 || strcmp(val, "0") == 0) cfg->theme = SK3_THEME_DARK;
            else if (strcasecmp(val, "amber") == 0 || strcmp(val, "1") == 0) cfg->theme = SK3_THEME_AMBER;
            else if (strcasecmp(val, "green") == 0 || strcmp(val, "2") == 0) cfg->theme = SK3_THEME_GREEN;
            else if (strcasecmp(val, "light") == 0 || strcmp(val, "3") == 0) cfg->theme = SK3_THEME_LIGHT;
        } else if (strcasecmp(key, "font_size") == 0 || strcasecmp(key, "font") == 0) {
            if (strcasecmp(val, "small") == 0 || strcasecmp(val, "s") == 0 || strcmp(val, "0") == 0) cfg->font_size = SK3_FONT_SIZE_SMALL;
            else if (strcasecmp(val, "large") == 0 || strcasecmp(val, "l") == 0 || strcmp(val, "2") == 0) cfg->font_size = SK3_FONT_SIZE_LARGE;
            else cfg->font_size = SK3_FONT_SIZE_MEDIUM;
        } else if (strcasecmp(key, "window_width") == 0 || strcasecmp(key, "width") == 0) {
            int w = atoi(val);
            if (w >= 400 && w <= 3840) cfg->window_width = w;
        } else if (strcasecmp(key, "window_height") == 0 || strcasecmp(key, "height") == 0) {
            int h = atoi(val);
            if (h >= 100 && h <= 2160) cfg->window_height = h;
        } else if (strcasecmp(key, "prev_expanded_h") == 0 || strcasecmp(key, "expanded_height") == 0) {
            int h = atoi(val);
            if (h >= 200 && h <= 2160) cfg->prev_expanded_h = h;
        } else if (strcasecmp(key, "console_collapsed") == 0 || strcasecmp(key, "collapsed") == 0) {
            if (strcasecmp(val, "1") == 0 || strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0) cfg->console_collapsed = 1;
            else cfg->console_collapsed = 0;
        } else if (strcasecmp(key, "console_show") == 0 || strcasecmp(key, "console_visible") == 0) {
            if (strcasecmp(val, "1") == 0 || strcasecmp(val, "true") == 0 || strcasecmp(val, "yes") == 0) cfg->console_collapsed = 0;
            else cfg->console_collapsed = 1;
        }
    }
    fclose(f);
    return 0;
}

#endif /* SK3_GUI_COMMON_H */
