#ifndef SK3_GUI_COMMON_H
#define SK3_GUI_COMMON_H

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

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

/* Helper to inspect a dropped file path and format a helpful Skode command or action */
static inline void sk3_gui_format_file_drop(const char *filepath, char *out_log, size_t out_log_sz,
                                            char *out_cmd, size_t out_cmd_sz) {
    if (!filepath || !filepath[0]) return;
    
    if (out_log && out_log_sz > 0) {
        snprintf(out_log, out_log_sz, "\n[File dropped: %s]\n", filepath);
    }
    
    if (out_cmd && out_cmd_sz > 0) {
        const char *dot = strrchr(filepath, '.');
        if (dot) {
            if (strcasecmp(dot, ".skode") == 0 || strcasecmp(dot, ".sk") == 0) {
                snprintf(out_cmd, out_cmd_sz, "load \"%s\"", filepath);
                return;
            } else if (strcasecmp(dot, ".wav") == 0 || strcasecmp(dot, ".aif") == 0 ||
                       strcasecmp(dot, ".flac") == 0 || strcasecmp(dot, ".raw") == 0) {
                snprintf(out_cmd, out_cmd_sz, "w0 \"%s\"", filepath);
                return;
            }
        }
        snprintf(out_cmd, out_cmd_sz, "\"%s\"", filepath);
    }
}

#endif /* SK3_GUI_COMMON_H */
