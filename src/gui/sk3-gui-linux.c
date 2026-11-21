#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

#include "api.h"
#include "skred.h"
#include "udp.h"
#include "sk3_gui_common.h"

/* UI Dimensions */
#define DEFAULT_WIDTH    640
#define DEFAULT_HEIGHT   580
#define COLLAPSED_HEIGHT 115

/* Atoms */
static Atom atom_wm_delete_window;
static Atom atom_clipboard;
static Atom atom_utf8_string;
static Atom atom_targets;
static Atom atom_sk3_paste;
static Atom atom_xdnd_aware;
static Atom atom_xdnd_enter;
static Atom atom_xdnd_position;
static Atom atom_xdnd_status;
static Atom atom_xdnd_action_copy;
static Atom atom_xdnd_drop;
static Atom atom_xdnd_finished;
static Atom atom_xdnd_selection;
static Atom atom_uri_list;

/* Display & Window */
static Display *dpy = NULL;
static int screen = 0;
static Window win = 0;
static Pixmap backbuffer = 0;
static GC gc = 0;
static Visual *visual = NULL;
static Colormap cmap = 0;
static int win_w = DEFAULT_WIDTH;
static int win_h = COLLAPSED_HEIGHT; /* Start COLLAPSED by default! */
static int is_collapsed = 1;          /* Default: NO output area initially */
static int prev_expanded_h = DEFAULT_HEIGHT;

/* Fonts */
static sk3_font_size_t cur_font_size = SK3_FONT_SIZE_MEDIUM;
static XftFont *xft_font_ui = NULL;
static XftFont *xft_font_mono = NULL;
static XftDraw *xft_draw = NULL;

/* Themes */
static sk3_theme_t cur_theme = SK3_THEME_DARK;
typedef struct {
    unsigned long bg_pixel;
    unsigned long panel_pixel;
    unsigned long border_pixel;
    unsigned long btn_pixel;
    unsigned long btn_active_pixel;
    unsigned long input_bg_pixel;
    unsigned long cursor_pixel;
    unsigned long status_ok_pixel;
    unsigned long status_err_pixel;
    unsigned long sel_bg_pixel;
    XftColor text_color;
    XftColor text_dim;
    XftColor status_ok;
    XftColor status_err;
} gui_palette_t;

static gui_palette_t pal;

/* State */
static char udp_port_str[16] = "60440";
static char tcp_port_str[16] = "60440";
static char status_banner[256] = "Starting audio engine...";
static int server_running = 0;
static int active_udp_port = 0;
static int active_tcp_port = 0;

/* Command Input & Blinking Cursor */
#define MAX_CMD_LEN 2048
static char cmd_buf[MAX_CMD_LEN] = {0};
static int cursor_pos = 0;
static int cursor_visible = 1;
static int active_focus = 2; /* 0: udp, 1: tcp, 2: cmd input, 3: log */

/* Command History */
#define MAX_HISTORY 128
static char *history[MAX_HISTORY];
static int history_count = 0;
static int history_idx = -1;
static char current_draft[MAX_CMD_LEN] = {0};

/* Output Log */
#define LOG_MAX_LINES 2000
static char *log_lines[LOG_MAX_LINES];
static int log_line_count = 0;
static int scroll_offset = 0;
static int lines_per_page = 20;

/* Text Selection in Log */
static int sel_active = 0;
static int sel_start_line = -1;
static int sel_start_col = -1;
static int sel_end_line = -1;
static int sel_end_col = -1;
static int drag_anchor_line = -1;
static int drag_anchor_col = -1;
static int is_mouse_dragging = 0;

/* Clipboard Buffer */
static char *clipboard_owned_text = NULL;

/* Forward declarations */
static void load_fonts(void);
static void update_palette(void);
static void render_all(void);
static void start_engine(void);
static void execute_command(const char *cmd);
static void append_log(const char *text);
static void set_clipboard(const char *text);
static void request_paste(void);

static XftColor parse_xft_color(const char *name) {
    XftColor col;
    XftColorAllocName(dpy, visual, cmap, name, &col);
    return col;
}

static unsigned long parse_pixel(const char *hex) {
    XColor c;
    XParseColor(dpy, cmap, hex, &c);
    XAllocColor(dpy, cmap, &c);
    return c.pixel;
}

static void update_palette(void) {
    switch (cur_theme) {
        case SK3_THEME_AMBER:
            pal.bg_pixel = parse_pixel("#120e06");
            pal.panel_pixel = parse_pixel("#1c160a");
            pal.border_pixel = parse_pixel("#403014");
            pal.btn_pixel = parse_pixel("#281e0e");
            pal.btn_active_pixel = parse_pixel("#3c2c16");
            pal.input_bg_pixel = parse_pixel("#181208");
            pal.cursor_pixel = parse_pixel("#ffb000");
            pal.status_ok_pixel = parse_pixel("#ffc020");
            pal.status_err_pixel = parse_pixel("#ff3333");
            pal.sel_bg_pixel = parse_pixel("#503408");
            pal.text_color = parse_xft_color("#ffb000");
            pal.text_dim = parse_xft_color("#996600");
            pal.status_ok = parse_xft_color("#ffc020");
            pal.status_err = parse_xft_color("#ff3333");
            break;
        case SK3_THEME_GREEN:
            pal.bg_pixel = parse_pixel("#081208");
            pal.panel_pixel = parse_pixel("#0e200e");
            pal.border_pixel = parse_pixel("#143c14");
            pal.btn_pixel = parse_pixel("#122a12");
            pal.btn_active_pixel = parse_pixel("#1c421c");
            pal.input_bg_pixel = parse_pixel("#0c1a0c");
            pal.cursor_pixel = parse_pixel("#39ff14");
            pal.status_ok_pixel = parse_pixel("#00ff66");
            pal.status_err_pixel = parse_pixel("#ff3333");
            pal.sel_bg_pixel = parse_pixel("#004d1a");
            pal.text_color = parse_xft_color("#39ff14");
            pal.text_dim = parse_xft_color("#1c8008");
            pal.status_ok = parse_xft_color("#00ff66");
            pal.status_err = parse_xft_color("#ff3333");
            break;
        case SK3_THEME_LIGHT:
            pal.bg_pixel = parse_pixel("#f2f3f5");
            pal.panel_pixel = parse_pixel("#ffffff");
            pal.border_pixel = parse_pixel("#cccccc");
            pal.btn_pixel = parse_pixel("#e4e6eb");
            pal.btn_active_pixel = parse_pixel("#d0d3d9");
            pal.input_bg_pixel = parse_pixel("#ffffff");
            pal.cursor_pixel = parse_pixel("#000000");
            pal.status_ok_pixel = parse_pixel("#008020");
            pal.status_err_pixel = parse_pixel("#cc0000");
            pal.sel_bg_pixel = parse_pixel("#b3d7ff");
            pal.text_color = parse_xft_color("#1a1a1a");
            pal.text_dim = parse_xft_color("#666666");
            pal.status_ok = parse_xft_color("#008020");
            pal.status_err = parse_xft_color("#cc0000");
            break;
        case SK3_THEME_DARK:
        default:
            pal.bg_pixel = parse_pixel("#1a1a1c");
            pal.panel_pixel = parse_pixel("#242428");
            pal.border_pixel = parse_pixel("#3a3a40");
            pal.btn_pixel = parse_pixel("#2e2e34");
            pal.btn_active_pixel = parse_pixel("#42424b");
            pal.input_bg_pixel = parse_pixel("#202024");
            pal.cursor_pixel = parse_pixel("#39ff14");
            pal.status_ok_pixel = parse_pixel("#39ff14");
            pal.status_err_pixel = parse_pixel("#ff4444");
            pal.sel_bg_pixel = parse_pixel("#264f78");
            pal.text_color = parse_xft_color("#e0e0e0");
            pal.text_dim = parse_xft_color("#88888e");
            pal.status_ok = parse_xft_color("#39ff14");
            pal.status_err = parse_xft_color("#ff4444");
            break;
    }
}

static void load_fonts(void) {
    if (xft_font_ui) XftFontClose(dpy, xft_font_ui);
    if (xft_font_mono) XftFontClose(dpy, xft_font_mono);

    int pt = (cur_font_size == SK3_FONT_SIZE_SMALL) ? 11 :
             (cur_font_size == SK3_FONT_SIZE_LARGE) ? 18 : 14;

    char pattern_mono[64];
    snprintf(pattern_mono, sizeof(pattern_mono), "monospace:size=%d", pt);
    xft_font_mono = XftFontOpenName(dpy, screen, pattern_mono);
    if (!xft_font_mono) xft_font_mono = XftFontOpenName(dpy, screen, "fixed");

    char pattern_ui[64];
    snprintf(pattern_ui, sizeof(pattern_ui), "sans:size=%d", pt > 12 ? pt - 1 : pt);
    xft_font_ui = XftFontOpenName(dpy, screen, pattern_ui);
    if (!xft_font_ui) xft_font_ui = xft_font_mono;
}

static void append_log(const char *text) {
    if (!text || !text[0]) return;
    const char *p = text;
    while (*p) {
        const char *next = strchr(p, '\n');
        size_t len = next ? (size_t)(next - p) : strlen(p);
        if (len > 0 && p[len - 1] == '\r') len--;
        
        char *line = (char *)malloc(len + 1);
        if (line) {
            memcpy(line, p, len);
            line[len] = '\0';
            if (log_line_count < LOG_MAX_LINES) {
                log_lines[log_line_count++] = line;
            } else {
                free(log_lines[0]);
                memmove(&log_lines[0], &log_lines[1], sizeof(char *) * (LOG_MAX_LINES - 1));
                log_lines[LOG_MAX_LINES - 1] = line;
            }
        }
        if (!next) break;
        p = next + 1;
    }
    if (log_line_count > lines_per_page) {
        scroll_offset = log_line_count - lines_per_page;
    } else {
        scroll_offset = 0;
    }
    render_all();
}

static void execute_command(const char *raw_cmd) {
    if (!raw_cmd) return;
    while (*raw_cmd == ' ' || *raw_cmd == '\t') raw_cmd++;
    if (!raw_cmd[0]) return;

    if (history_count == 0 || strcmp(history[history_count - 1], raw_cmd) != 0) {
        if (history_count < MAX_HISTORY) {
            history[history_count++] = strdup(raw_cmd);
        } else {
            free(history[0]);
            memmove(&history[0], &history[1], sizeof(char *) * (MAX_HISTORY - 1));
            history[MAX_HISTORY - 1] = strdup(raw_cmd);
        }
    }
    history_idx = -1;
    current_draft[0] = '\0';

    char prompt[MAX_CMD_LEN + 16];
    snprintf(prompt, sizeof(prompt), "> %s", raw_cmd);
    append_log(prompt);

    char run_buf[MAX_CMD_LEN];
    snprintf(run_buf, sizeof(run_buf), "%s", raw_cmd);
    int r = skred_command(run_buf);
    char *engine_log = skred_log();
    if (engine_log && engine_log[0]) {
        append_log(engine_log);
    }
    if (r > 0) {
        char r_str[32];
        snprintf(r_str, sizeof(r_str), "r = %d", r);
        append_log(r_str);
    }

    cmd_buf[0] = '\0';
    cursor_pos = 0;
    cursor_visible = 1;
    render_all();
}

static void start_engine(void) {
    int req_udp = atoi(udp_port_str);
    int req_tcp = atoi(tcp_port_str);
    if (req_udp < 0) req_udp = 0;
    if (req_tcp < 0) req_tcp = 0;

    skred_stop();
    skred_set_audio_device(-1, -1);
    int r = skred_start(128, 256, req_udp);
    int hp = 0;
    if (req_tcp > 0) {
        hp = skred_http_start(req_tcp);
    }
    skred_logger(1);

    if (r == 0) {
        server_running = 1;
        active_udp_port = (req_udp > 0) ? skred_udp_info() : 0;
        active_tcp_port = (hp > 0) ? hp : 0;

        sk3_gui_format_status(1, active_udp_port, active_tcp_port, skred_version(),
                              status_banner, sizeof(status_banner));

        char banner[512];
        char banner_udp[64] = "UDP Port: (not opened)\n";
        char banner_tcp[64] = "TCP Port: (not opened)\n";
        if (active_udp_port > 0) snprintf(banner_udp, sizeof(banner_udp), "UDP Port: %d (Opcode dispatch & datagrams)\n", active_udp_port);
        if (active_tcp_port > 0) snprintf(banner_tcp, sizeof(banner_tcp), "TCP Port: %d (Raw TCP, HTTP/1.1, WebSocket)\n", active_tcp_port);

        snprintf(banner, sizeof(banner),
            "=== sk3 v%s Audio Engine ===\n"
            "Status: Active (256 voices, 48kHz audio)\n"
            "%s%s"
            "Engine ready for Skode commands.",
            skred_version(), banner_udp, banner_tcp);
        append_log(banner);
    } else {
        server_running = 0;
        active_udp_port = 0;
        active_tcp_port = 0;
        snprintf(status_banner, sizeof(status_banner), "AUDIO START FAILED (Error %d)", r);
        append_log("Error: Failed to initialize audio output device.");
    }
    render_all();
}

/* Button bounds */
typedef struct {
    int x, y, w, h;
    char label[32];
    int id;
} btn_t;

enum {
    BTN_RESTART = 1,
    BTN_TOGGLE_CONSOLE,
    BTN_THEME,
    BTN_FONT,
    BTN_SEND,
    BTN_TEST,
    BTN_SILENCE,
    BTN_COPY_LOG,
    BTN_CLEAR_LOG
};

#define MAX_BUTTONS 16
static btn_t buttons[MAX_BUTTONS];
static int num_buttons = 0;

static void add_btn_auto(int id, int x, int y, const char *label) {
    if (num_buttons >= MAX_BUTTONS) return;

    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xft_font_ui, (const FcChar8 *)label, (int)strlen(label), &ext);
    int w = ext.width + 18;
    if (w < 60) w = 60;
    int h = xft_font_ui->height + 8;
    if (h < 26) h = 26;

    buttons[num_buttons].id = id;
    buttons[num_buttons].x = x;
    buttons[num_buttons].y = y;
    buttons[num_buttons].w = w;
    buttons[num_buttons].h = h;
    snprintf(buttons[num_buttons].label, sizeof(buttons[num_buttons].label), "%s", label);
    num_buttons++;
}

static void draw_button(const btn_t *b) {
    XSetForeground(dpy, gc, pal.btn_pixel);
    XFillRectangle(dpy, backbuffer, gc, b->x, b->y, b->w, b->h);
    XSetForeground(dpy, gc, pal.border_pixel);
    XDrawRectangle(dpy, backbuffer, gc, b->x, b->y, b->w, b->h);

    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xft_font_ui, (const FcChar8 *)b->label, (int)strlen(b->label), &ext);
    int tx = b->x + (b->w - ext.width) / 2;
    int ty = b->y + (b->h + ext.height) / 2 - 2;
    XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_ui, tx, ty, (const FcChar8 *)b->label, (int)strlen(b->label));
}

static void render_all(void) {
    if (!backbuffer || win_w <= 0 || win_h <= 0) return;

    XSetForeground(dpy, gc, pal.bg_pixel);
    XFillRectangle(dpy, backbuffer, gc, 0, 0, win_w, win_h);
    num_buttons = 0;

    // Draw graphical circle for status indicator (NO UNICODE SYMBOLS!)
    XSetForeground(dpy, gc, server_running ? pal.status_ok_pixel : pal.status_err_pixel);
    XFillArc(dpy, backbuffer, gc, 20, 16, 12, 12, 0, 360 * 64);
    XSetForeground(dpy, gc, pal.border_pixel);
    XDrawArc(dpy, backbuffer, gc, 20, 16, 12, 12, 0, 360 * 64);

    // Status Banner Text
    XftDrawStringUtf8(xft_draw, server_running ? &pal.status_ok : &pal.status_err,
                      xft_font_ui, 40, 26, (const FcChar8 *)status_banner, (int)strlen(status_banner));

    // Server Controls Row (y = 50)
    int cy = 50;
    XftDrawStringUtf8(xft_draw, &pal.text_dim, xft_font_ui, 20, cy + 18, (const FcChar8 *)"UDP:", 4);
    
    // UDP Input box
    XSetForeground(dpy, gc, (active_focus == 0) ? pal.input_bg_pixel : pal.panel_pixel);
    XFillRectangle(dpy, backbuffer, gc, 55, cy, 60, 26);
    XSetForeground(dpy, gc, (active_focus == 0) ? pal.status_ok_pixel : pal.border_pixel);
    XDrawRectangle(dpy, backbuffer, gc, 55, cy, 60, 26);
    XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_mono, 60, cy + 18, (const FcChar8 *)udp_port_str, (int)strlen(udp_port_str));

    XftDrawStringUtf8(xft_draw, &pal.text_dim, xft_font_ui, 122, cy + 18, (const FcChar8 *)"TCP:", 4);
    
    // TCP Input box
    XSetForeground(dpy, gc, (active_focus == 1) ? pal.input_bg_pixel : pal.panel_pixel);
    XFillRectangle(dpy, backbuffer, gc, 155, cy, 60, 26);
    XSetForeground(dpy, gc, (active_focus == 1) ? pal.status_ok_pixel : pal.border_pixel);
    XDrawRectangle(dpy, backbuffer, gc, 155, cy, 60, 26);
    XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_mono, 160, cy + 18, (const FcChar8 *)tcp_port_str, (int)strlen(tcp_port_str));

    // Dynamic button positions on top row
    int bx = 224;
    add_btn_auto(BTN_RESTART, bx, cy, "Restart");
    bx += buttons[num_buttons - 1].w + 8;

    add_btn_auto(BTN_TOGGLE_CONSOLE, bx, cy, is_collapsed ? "Console [Show]" : "Console [Hide]");
    bx += buttons[num_buttons - 1].w + 8;

    add_btn_auto(BTN_THEME, bx, cy, sk3_theme_name(cur_theme));
    bx += buttons[num_buttons - 1].w + 8;

    const char *f_name = (cur_font_size == SK3_FONT_SIZE_SMALL) ? "Font: S" :
                         (cur_font_size == SK3_FONT_SIZE_LARGE) ? "Font: L" : "Font: M";
    add_btn_auto(BTN_FONT, bx, cy, f_name);

    if (!is_collapsed && win_h > COLLAPSED_HEIGHT + 80) {
        // Divider
        XSetForeground(dpy, gc, pal.border_pixel);
        XDrawLine(dpy, backbuffer, gc, 20, 88, win_w - 20, 88);

        // Skode Command Input Label
        XftDrawStringUtf8(xft_draw, &pal.text_dim, xft_font_ui, 20, 108,
                          (const FcChar8 *)"Skode Command (Enter to run, Up/Down for history):", 51);

        // Command Input Field
        int send_w = 70;
        int input_w = win_w - 40 - send_w - 10;
        if (input_w < 100) input_w = 100;
        int input_y = 118;
        int input_h = xft_font_mono->height + 10;
        if (input_h < 28) input_h = 28;

        XSetForeground(dpy, gc, pal.input_bg_pixel);
        XFillRectangle(dpy, backbuffer, gc, 20, input_y, input_w, input_h);
        XSetForeground(dpy, gc, (active_focus == 2) ? pal.status_ok_pixel : pal.border_pixel);
        XDrawRectangle(dpy, backbuffer, gc, 20, input_y, input_w, input_h);

        // Render command text
        if (cmd_buf[0]) {
            XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_mono, 26, input_y + xft_font_mono->ascent + 5,
                              (const FcChar8 *)cmd_buf, (int)strlen(cmd_buf));
        }

        // Draw blinking cursor if input is focused
        if (active_focus == 2 && cursor_visible) {
            XGlyphInfo ext_cursor = {0};
            if (cursor_pos > 0) {
                XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)cmd_buf, cursor_pos, &ext_cursor);
            }
            int cx = 26 + ext_cursor.width;
            XSetForeground(dpy, gc, pal.cursor_pixel);
            XFillRectangle(dpy, backbuffer, gc, cx, input_y + 4, 2, input_h - 8);
        }

        // Send Button
        add_btn_auto(BTN_SEND, 20 + input_w + 10, input_y, "Send");

        // Quick Actions Row
        int qy = input_y + input_h + 10;
        int qx = 20;
        add_btn_auto(BTN_TEST, qx, qy, "Test Tone");
        qx += buttons[num_buttons - 1].w + 8;
        add_btn_auto(BTN_SILENCE, qx, qy, "Silence (O Z0)");

        // Right-aligned buttons: Clear Log and Copy Log
        int right_x = win_w - 20;
        add_btn_auto(BTN_CLEAR_LOG, 0, qy, "Clear Log");
        buttons[num_buttons - 1].x = right_x - buttons[num_buttons - 1].w;
        right_x = buttons[num_buttons - 1].x - 8;

        add_btn_auto(BTN_COPY_LOG, 0, qy, "Copy Log");
        buttons[num_buttons - 1].x = right_x - buttons[num_buttons - 1].w;

        // Output Log Area
        int log_y = qy + buttons[num_buttons - 1].h + 10;
        int log_h = win_h - log_y - 15;
        if (log_h > 40) {
            XSetForeground(dpy, gc, pal.panel_pixel);
            XFillRectangle(dpy, backbuffer, gc, 20, log_y, win_w - 40, log_h);
            XSetForeground(dpy, gc, pal.border_pixel);
            XDrawRectangle(dpy, backbuffer, gc, 20, log_y, win_w - 40, log_h);

            XGlyphInfo ext_m;
            XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)"M", 1, &ext_m);
            int char_w = ext_m.xOff > 0 ? ext_m.xOff : ext_m.width;
            if (char_w <= 0) char_w = 8;

            int line_h = xft_font_mono->height + 2;
            if (line_h <= 0) line_h = 16;
            lines_per_page = (log_h - 10) / line_h;
            if (lines_per_page < 1) lines_per_page = 1;

            if (scroll_offset + lines_per_page > log_line_count) {
                scroll_offset = log_line_count - lines_per_page;
            }
            if (scroll_offset < 0) scroll_offset = 0;

            int draw_y = log_y + xft_font_mono->ascent + 6;
            for (int i = 0; i < lines_per_page; i++) {
                int l_idx = scroll_offset + i;
                if (l_idx >= log_line_count) break;
                const char *line = log_lines[l_idx];
                if (line) {
                    int line_len = (int)strlen(line);
                    // Draw text selection highlight if active!
                    if (sel_active && l_idx >= sel_start_line && l_idx <= sel_end_line) {
                        int c1 = (l_idx == sel_start_line) ? sel_start_col : 0;
                        int c2 = (l_idx == sel_end_line) ? sel_end_col : line_len;
                        if (c1 < 0) c1 = 0;
                        if (c2 > line_len) c2 = line_len;
                        if (c1 < c2) {
                            int hx = 28 + c1 * char_w;
                            int hw = (c2 - c1) * char_w;
                            int hy = draw_y - xft_font_mono->ascent;
                            XSetForeground(dpy, gc, pal.sel_bg_pixel);
                            XFillRectangle(dpy, backbuffer, gc, hx, hy, hw, line_h);
                        }
                    }

                    XftColor *tc = &pal.text_color;
                    if (line[0] == '>') tc = &pal.status_ok;
                    else if (strncmp(line, "Error", 5) == 0) tc = &pal.status_err;
                    else if (line[0] == '[') tc = &pal.text_dim;
                    XftDrawStringUtf8(xft_draw, tc, xft_font_mono, 28, draw_y,
                                      (const FcChar8 *)line, line_len);
                }
                draw_y += line_h;
            }

            // Scrollbar track & thumb
            if (log_line_count > lines_per_page) {
                int sb_x = win_w - 32;
                int sb_y = log_y + 4;
                int sb_h = log_h - 8;
                XSetForeground(dpy, gc, pal.btn_pixel);
                XFillRectangle(dpy, backbuffer, gc, sb_x, sb_y, 8, sb_h);

                float thumb_ratio = (float)lines_per_page / (float)log_line_count;
                int thumb_h = (int)(thumb_ratio * sb_h);
                if (thumb_h < 15) thumb_h = 15;
                float pos_ratio = (float)scroll_offset / (float)(log_line_count - lines_per_page);
                int thumb_y = sb_y + (int)(pos_ratio * (sb_h - thumb_h));

                XSetForeground(dpy, gc, pal.border_pixel);
                XFillRectangle(dpy, backbuffer, gc, sb_x, thumb_y, 8, thumb_h);
            }
        }
    }

    // Draw buttons
    for (int i = 0; i < num_buttons; i++) {
        draw_button(&buttons[i]);
    }

    XCopyArea(dpy, backbuffer, win, gc, 0, 0, win_w, win_h, 0, 0);
    XFlush(dpy);
}

/* Clipboard Handling */
static void set_clipboard(const char *text) {
    if (!text) return;
    free(clipboard_owned_text);
    clipboard_owned_text = strdup(text);
    XSetSelectionOwner(dpy, atom_clipboard, win, CurrentTime);
    XSetSelectionOwner(dpy, XA_PRIMARY, win, CurrentTime);
}

static void handle_selection_request(XSelectionRequestEvent *req) {
    XSelectionEvent ev = {0};
    ev.type = SelectionNotify;
    ev.display = req->display;
    ev.requestor = req->requestor;
    ev.selection = req->selection;
    ev.target = req->target;
    ev.property = req->property;
    ev.time = req->time;

    if (req->target == atom_targets) {
        Atom targets[] = { atom_targets, atom_utf8_string, XA_STRING };
        XChangeProperty(req->display, req->requestor, req->property, XA_ATOM, 32,
                        PropModeReplace, (unsigned char *)targets, 3);
    } else if (req->target == atom_utf8_string || req->target == XA_STRING) {
        const char *data = clipboard_owned_text ? clipboard_owned_text : "";
        XChangeProperty(req->display, req->requestor, req->property, req->target, 8,
                        PropModeReplace, (const unsigned char *)data, (int)strlen(data));
    } else {
        ev.property = None;
    }
    XSendEvent(req->display, req->requestor, False, 0, (XEvent *)&ev);
}

static void request_paste(void) {
    XConvertSelection(dpy, atom_clipboard, atom_utf8_string, atom_sk3_paste, win, CurrentTime);
}

static void handle_paste_notify(XSelectionEvent *sev) {
    if (sev->property == None) return;
    Atom type;
    int format;
    unsigned long nitems, bytes_after;
    unsigned char *prop = NULL;
    if (XGetWindowProperty(dpy, win, atom_sk3_paste, 0, 4096, True, AnyPropertyType,
                           &type, &format, &nitems, &bytes_after, &prop) == Success && prop) {
        size_t plen = strlen((char *)prop);
        size_t cur_len = strlen(cmd_buf);
        if (cur_len + plen < MAX_CMD_LEN - 1) {
            memmove(cmd_buf + cursor_pos + plen, cmd_buf + cursor_pos, cur_len - cursor_pos + 1);
            memcpy(cmd_buf + cursor_pos, prop, plen);
            cursor_pos += (int)plen;
            cursor_visible = 1;
        }
        XFree(prop);
        render_all();
    }
}

/* XDnD Drag & Drop Handling */
static Window xdnd_source = 0;

static void handle_xdnd_enter(XClientMessageEvent *cme) {
    xdnd_source = cme->data.l[0];
}

static void handle_xdnd_position(XClientMessageEvent *cme) {
    xdnd_source = cme->data.l[0];
    XClientMessageEvent resp = {0};
    resp.type = ClientMessage;
    resp.window = xdnd_source;
    resp.message_type = atom_xdnd_status;
    resp.format = 32;
    resp.data.l[0] = win;
    resp.data.l[1] = 1;
    resp.data.l[2] = 0;
    resp.data.l[3] = 0;
    resp.data.l[4] = atom_xdnd_action_copy;
    XSendEvent(dpy, xdnd_source, False, 0, (XEvent *)&resp);
    XFlush(dpy);
}

static void handle_xdnd_drop(XClientMessageEvent *cme) {
    (void)cme;
    XConvertSelection(dpy, atom_xdnd_selection, atom_uri_list, atom_sk3_paste, win, CurrentTime);
}

static void handle_dropped_uri_list(const char *uri_data) {
    if (!uri_data) return;
    const char *p = uri_data;
    while (*p) {
        if (strncmp(p, "file://", 7) == 0) {
            const char *end = strchr(p, '\r');
            if (!end) end = strchr(p, '\n');
            size_t len = end ? (size_t)(end - p) : strlen(p);
            char uri[1024];
            if (len >= sizeof(uri)) len = sizeof(uri) - 1;
            strncpy(uri, p, len);
            uri[len] = '\0';

            char path[1024] = {0};
            const char *src = uri + 7;
            char *dst = path;
            while (*src && dst < path + sizeof(path) - 1) {
                if (*src == '%' && src[1] && src[2]) {
                    int val = 0;
                    if (sscanf(src + 1, "%2x", &val) == 1) {
                        *dst++ = (char)val;
                        src += 3;
                        continue;
                    }
                }
                *dst++ = *src++;
            }
            *dst = '\0';

            char log_msg[512] = {0};
            char cmd[MAX_CMD_LEN] = {0};
            sk3_gui_format_file_drop(path, log_msg, sizeof(log_msg), cmd, sizeof(cmd));
            if (log_msg[0]) append_log(log_msg);
            if (cmd[0]) {
                snprintf(cmd_buf, sizeof(cmd_buf), "%s", cmd);
                cursor_pos = (int)strlen(cmd_buf);
                cursor_visible = 1;
                active_focus = 2;
                render_all();
            }
            break;
        }
        const char *next = strchr(p, '\n');
        if (!next) break;
        p = next + 1;
    }

    if (xdnd_source) {
        XClientMessageEvent fin = {0};
        fin.type = ClientMessage;
        fin.window = xdnd_source;
        fin.message_type = atom_xdnd_finished;
        fin.format = 32;
        fin.data.l[0] = win;
        fin.data.l[1] = 1;
        fin.data.l[2] = atom_xdnd_action_copy;
        XSendEvent(dpy, xdnd_source, False, 0, (XEvent *)&fin);
        XFlush(dpy);
    }
}

static char *get_selected_log_text(void) {
    if (!sel_active || sel_start_line < 0 || sel_end_line >= log_line_count) return NULL;
    size_t cap = 2048;
    char *res = (char *)malloc(cap);
    if (!res) return NULL;
    res[0] = '\0';
    size_t cur_len = 0;

    for (int l = sel_start_line; l <= sel_end_line; l++) {
        const char *line = log_lines[l];
        if (!line) continue;
        int llen = (int)strlen(line);
        int c1 = (l == sel_start_line) ? sel_start_col : 0;
        int c2 = (l == sel_end_line) ? sel_end_col : llen;
        if (c1 < 0) c1 = 0;
        if (c2 > llen) c2 = llen;
        if (c1 < c2) {
            int seg_len = c2 - c1;
            while (cur_len + seg_len + 2 >= cap) {
                cap *= 2;
                char *n = (char *)realloc(res, cap);
                if (!n) { free(res); return NULL; }
                res = n;
            }
            memcpy(res + cur_len, line + c1, seg_len);
            cur_len += seg_len;
            if (l < sel_end_line) {
                res[cur_len++] = '\n';
            }
            res[cur_len] = '\0';
        }
    }
    return res;
}

static void copy_full_log(void) {
    if (sel_active) {
        char *sel_text = get_selected_log_text();
        if (sel_text && sel_text[0]) {
            set_clipboard(sel_text);
            free(sel_text);
            append_log("[Selection copied to clipboard]");
            return;
        }
        if (sel_text) free(sel_text);
    }

    size_t total_len = 0;
    for (int i = 0; i < log_line_count; i++) {
        if (log_lines[i]) total_len += strlen(log_lines[i]) + 1;
    }
    if (total_len == 0) return;
    char *buf = (char *)malloc(total_len + 1);
    if (!buf) return;
    buf[0] = '\0';
    for (int i = 0; i < log_line_count; i++) {
        if (log_lines[i]) {
            strcat(buf, log_lines[i]);
            strcat(buf, "\n");
        }
    }
    set_clipboard(buf);
    free(buf);
    append_log("[Log copied to clipboard]");
}

static void toggle_console(void) {
    is_collapsed = !is_collapsed;
    if (is_collapsed) {
        prev_expanded_h = win_h;
        win_h = COLLAPSED_HEIGHT;
    } else {
        win_h = (prev_expanded_h > COLLAPSED_HEIGHT + 100) ? prev_expanded_h : DEFAULT_HEIGHT;
        active_focus = 2; /* Automatically focus input when expanded */
        cursor_visible = 1;
    }
    XResizeWindow(dpy, win, win_w, win_h);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    dpy = XOpenDisplay(NULL);
    if (!dpy) {
        fprintf(stderr, "sk3-gui: could not open X display\n");
        return 1;
    }

    screen = DefaultScreen(dpy);
    Window root = RootWindow(dpy, screen);
    visual = DefaultVisual(dpy, screen);
    cmap = DefaultColormap(dpy, screen);

    atom_wm_delete_window = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
    atom_clipboard = XInternAtom(dpy, "CLIPBOARD", False);
    atom_utf8_string = XInternAtom(dpy, "UTF8_STRING", False);
    atom_targets = XInternAtom(dpy, "TARGETS", False);
    atom_sk3_paste = XInternAtom(dpy, "SK3_PASTE_PROP", False);

    atom_xdnd_aware = XInternAtom(dpy, "XdndAware", False);
    atom_xdnd_enter = XInternAtom(dpy, "XdndEnter", False);
    atom_xdnd_position = XInternAtom(dpy, "XdndPosition", False);
    atom_xdnd_status = XInternAtom(dpy, "XdndStatus", False);
    atom_xdnd_action_copy = XInternAtom(dpy, "XdndActionCopy", False);
    atom_xdnd_drop = XInternAtom(dpy, "XdndDrop", False);
    atom_xdnd_finished = XInternAtom(dpy, "XdndFinished", False);
    atom_xdnd_selection = XInternAtom(dpy, "XdndSelection", False);
    atom_uri_list = XInternAtom(dpy, "text/uri-list", False);

    update_palette();
    load_fonts();

    // Create Window with COLLAPSED_HEIGHT by default (no output area initially!)
    win_w = DEFAULT_WIDTH;
    win_h = COLLAPSED_HEIGHT;
    is_collapsed = 1;

    XSetWindowAttributes swa;
    swa.background_pixel = pal.bg_pixel;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                     StructureNotifyMask | PointerMotionMask | FocusChangeMask;

    win = XCreateWindow(dpy, root, 100, 100, win_w, win_h, 0,
                        CopyFromParent, InputOutput, visual,
                        CWBackPixel | CWEventMask, &swa);

    XStoreName(dpy, win, "sk3 - Audio Engine & Control Server");
    XClassHint class_hint = { "sk3-gui", "Sk3" };
    XSetClassHint(dpy, win, &class_hint);

    // Set WM hints to accept keyboard input
    XWMHints *hints = XAllocWMHints();
    if (hints) {
        hints->flags = InputHint | StateHint;
        hints->input = True;
        hints->initial_state = NormalState;
        XSetWMHints(dpy, win, hints);
        XFree(hints);
    }

    // Set Window Size Hints
    XSizeHints *sh = XAllocSizeHints();
    if (sh) {
        sh->flags = PMinSize;
        sh->min_width = 540;
        sh->min_height = COLLAPSED_HEIGHT;
        XSetWMNormalHints(dpy, win, sh);
        XFree(sh);
    }

    XSetWMProtocols(dpy, win, &atom_wm_delete_window, 1);

    Atom xdnd_version = 5;
    XChangeProperty(dpy, win, atom_xdnd_aware, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&xdnd_version, 1);

    gc = XCreateGC(dpy, win, 0, NULL);
    backbuffer = XCreatePixmap(dpy, win, win_w, win_h, DefaultDepth(dpy, screen));
    xft_draw = XftDrawCreate(dpy, backbuffer, visual, cmap);

    XMapWindow(dpy, win);
    start_engine();

    int running = 1;
    int x11_fd = ConnectionNumber(dpy);
    struct timeval last_blink;
    gettimeofday(&last_blink, NULL);

    while (running) {
        // Handle cursor blinking timer when no events are queued
        while (XPending(dpy) == 0) {
            struct timeval now;
            gettimeofday(&now, NULL);
            long elapsed_ms = (now.tv_sec - last_blink.tv_sec) * 1000 + (now.tv_usec - last_blink.tv_usec) / 1000;
            long remain_ms = 500 - elapsed_ms;
            if (remain_ms <= 0) {
                cursor_visible = !cursor_visible;
                last_blink = now;
                if (!is_collapsed && active_focus == 2) {
                    render_all();
                }
                remain_ms = 500;
            }

            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(x11_fd, &fds);
            struct timeval tv;
            tv.tv_sec = remain_ms / 1000;
            tv.tv_usec = (remain_ms % 1000) * 1000;
            int sr = select(x11_fd + 1, &fds, NULL, NULL, &tv);
            if (sr > 0) break;
        }

        XEvent ev;
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case Expose:
                render_all();
                break;

            case FocusIn:
                render_all();
                break;

            case ConfigureNotify:
                if (ev.xconfigure.width != win_w || ev.xconfigure.height != win_h) {
                    win_w = ev.xconfigure.width;
                    win_h = ev.xconfigure.height;
                    if (backbuffer) XFreePixmap(dpy, backbuffer);
                    if (xft_draw) XftDrawDestroy(xft_draw);
                    backbuffer = XCreatePixmap(dpy, win, win_w, win_h, DefaultDepth(dpy, screen));
                    xft_draw = XftDrawCreate(dpy, backbuffer, visual, cmap);
                    render_all();
                }
                break;

            case ButtonPress: {
                XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
                int mx = ev.xbutton.x;
                int my = ev.xbutton.y;

                if (ev.xbutton.button == 4) {
                    if (scroll_offset > 0) scroll_offset -= 3;
                    if (scroll_offset < 0) scroll_offset = 0;
                    render_all();
                    break;
                } else if (ev.xbutton.button == 5) {
                    if (scroll_offset + lines_per_page < log_line_count) scroll_offset += 3;
                    render_all();
                    break;
                }

                int clicked_btn = 0;
                for (int i = 0; i < num_buttons; i++) {
                    btn_t *b = &buttons[i];
                    if (mx >= b->x && mx <= b->x + b->w && my >= b->y && my <= b->y + b->h) {
                        clicked_btn = b->id;
                        break;
                    }
                }

                if (clicked_btn == BTN_RESTART) {
                    start_engine();
                } else if (clicked_btn == BTN_TOGGLE_CONSOLE) {
                    toggle_console();
                } else if (clicked_btn == BTN_THEME) {
                    cur_theme = (cur_theme + 1) % SK3_THEME_COUNT;
                    update_palette();
                    render_all();
                } else if (clicked_btn == BTN_FONT) {
                    cur_font_size = (cur_font_size + 1) % SK3_FONT_SIZE_COUNT;
                    load_fonts();
                    render_all();
                } else if (clicked_btn == BTN_SEND) {
                    execute_command(cmd_buf);
                    active_focus = 2;
                } else if (clicked_btn == BTN_TEST) {
                    execute_command("v0 w1 f440 a-6 l1;");
                    active_focus = 2;
                } else if (clicked_btn == BTN_SILENCE) {
                    execute_command("O Z0; S-1;");
                    active_focus = 2;
                } else if (clicked_btn == BTN_COPY_LOG) {
                    copy_full_log();
                    active_focus = 2;
                } else if (clicked_btn == BTN_CLEAR_LOG) {
                    for (int i = 0; i < log_line_count; i++) free(log_lines[i]);
                    log_line_count = 0;
                    scroll_offset = 0;
                    sel_active = 0;
                    render_all();
                    active_focus = 2;
                } else {
                    if (my >= 45 && my <= 80) {
                        if (mx >= 55 && mx <= 120) active_focus = 0; // UDP
                        else if (mx >= 155 && mx <= 220) active_focus = 1; // TCP
                        sel_active = 0;
                    } else if (!is_collapsed && my >= 110 && my <= 155) {
                        active_focus = 2; // Command input
                        cursor_visible = 1;
                        gettimeofday(&last_blink, NULL);
                        sel_active = 0;
                    } else if (!is_collapsed && my > 190 && my < win_h - 15 && mx >= 20 && mx <= win_w - 40) {
                        active_focus = 3; // Log area selection
                        int line_h = xft_font_mono->height + 2;
                        if (line_h <= 0) line_h = 16;
                        XGlyphInfo ext_m;
                        XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)"M", 1, &ext_m);
                        int char_w = ext_m.xOff > 0 ? ext_m.xOff : ext_m.width;
                        if (char_w <= 0) char_w = 8;

                        int qy = 118 + xft_font_mono->height + 20;
                        int log_y = qy + 36;
                        int cl = scroll_offset + (my - log_y) / line_h;
                        if (cl < 0) cl = 0;
                        if (cl >= log_line_count) cl = log_line_count > 0 ? log_line_count - 1 : 0;
                        int cc = (mx - 28) / char_w;
                        if (cc < 0) cc = 0;

                        drag_anchor_line = cl;
                        drag_anchor_col = cc;
                        sel_start_line = cl;
                        sel_start_col = cc;
                        sel_end_line = cl;
                        sel_end_col = cc;
                        sel_active = 0;
                        is_mouse_dragging = 1;
                    }
                    render_all();
                }
                break;
            }

            case MotionNotify: {
                if (is_mouse_dragging && (ev.xmotion.state & Button1Mask)) {
                    int my = ev.xmotion.y;
                    int mx = ev.xmotion.x;
                    int line_h = xft_font_mono->height + 2;
                    if (line_h <= 0) line_h = 16;
                    XGlyphInfo ext_m;
                    XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)"M", 1, &ext_m);
                    int char_w = ext_m.xOff > 0 ? ext_m.xOff : ext_m.width;
                    if (char_w <= 0) char_w = 8;

                    int qy = 118 + xft_font_mono->height + 20;
                    int log_y = qy + 36;
                    int cur_l = scroll_offset + (my - log_y) / line_h;
                    if (cur_l < 0) cur_l = 0;
                    if (cur_l >= log_line_count) cur_l = log_line_count > 0 ? log_line_count - 1 : 0;
                    int cur_c = (mx - 28) / char_w;
                    if (cur_c < 0) cur_c = 0;

                    if (drag_anchor_line < cur_l || (drag_anchor_line == cur_l && drag_anchor_col <= cur_c)) {
                        sel_start_line = drag_anchor_line;
                        sel_start_col = drag_anchor_col;
                        sel_end_line = cur_l;
                        sel_end_col = cur_c;
                    } else {
                        sel_start_line = cur_l;
                        sel_start_col = cur_c;
                        sel_end_line = drag_anchor_line;
                        sel_end_col = drag_anchor_col;
                    }
                    sel_active = (sel_start_line != sel_end_line || sel_start_col != sel_end_col);
                    render_all();
                }
                break;
            }

            case ButtonRelease: {
                if (ev.xbutton.button == 1 && is_mouse_dragging) {
                    is_mouse_dragging = 0;
                    if (sel_active) {
                        char *sel_text = get_selected_log_text();
                        if (sel_text) {
                            set_clipboard(sel_text);
                            free(sel_text);
                        }
                    }
                    render_all();
                }
                break;
            }

            case KeyPress: {
                char kbuf[64] = {0};
                KeySym ksym = 0;
                int len = XLookupString(&ev.xkey, kbuf, sizeof(kbuf) - 1, &ksym, NULL);
                int is_ctrl = (ev.xkey.state & ControlMask) != 0;

                cursor_visible = 1;
                gettimeofday(&last_blink, NULL);

                if (is_ctrl && (ksym == XK_c || ksym == XK_C)) {
                    if (sel_active) {
                        char *sel_text = get_selected_log_text();
                        if (sel_text) {
                            set_clipboard(sel_text);
                            free(sel_text);
                            append_log("[Selection copied to clipboard]");
                        }
                    } else if (active_focus == 2) {
                        set_clipboard(cmd_buf);
                    } else {
                        copy_full_log();
                    }
                    break;
                }

                if (active_focus == 2) {
                    if (is_ctrl && (ksym == XK_v || ksym == XK_V)) {
                        request_paste();
                    } else if (is_ctrl && (ksym == XK_x || ksym == XK_X)) {
                        set_clipboard(cmd_buf);
                        cmd_buf[0] = '\0';
                        cursor_pos = 0;
                        render_all();
                    } else if (is_ctrl && (ksym == XK_a || ksym == XK_A)) {
                        cursor_pos = (int)strlen(cmd_buf);
                        render_all();
                    } else if (ksym == XK_Return || ksym == XK_KP_Enter) {
                        execute_command(cmd_buf);
                    } else if (ksym == XK_Up) {
                        if (history_count > 0) {
                            if (history_idx == -1) {
                                snprintf(current_draft, sizeof(current_draft), "%s", cmd_buf);
                                history_idx = history_count - 1;
                            } else if (history_idx > 0) {
                                history_idx--;
                            }
                            snprintf(cmd_buf, sizeof(cmd_buf), "%s", history[history_idx]);
                            cursor_pos = (int)strlen(cmd_buf);
                            render_all();
                        }
                    } else if (ksym == XK_Down) {
                        if (history_idx != -1) {
                            history_idx++;
                            if (history_idx < history_count) {
                                snprintf(cmd_buf, sizeof(cmd_buf), "%s", history[history_idx]);
                            } else {
                                history_idx = -1;
                                snprintf(cmd_buf, sizeof(cmd_buf), "%s", current_draft);
                            }
                            cursor_pos = (int)strlen(cmd_buf);
                            render_all();
                        }
                    } else if (ksym == XK_Left) {
                        if (cursor_pos > 0) cursor_pos--;
                        render_all();
                    } else if (ksym == XK_Right) {
                        if (cursor_pos < (int)strlen(cmd_buf)) cursor_pos++;
                        render_all();
                    } else if (ksym == XK_Home) {
                        cursor_pos = 0;
                        render_all();
                    } else if (ksym == XK_End) {
                        cursor_pos = (int)strlen(cmd_buf);
                        render_all();
                    } else if (ksym == XK_BackSpace) {
                        if (cursor_pos > 0) {
                            memmove(cmd_buf + cursor_pos - 1, cmd_buf + cursor_pos, strlen(cmd_buf) - cursor_pos + 1);
                            cursor_pos--;
                            render_all();
                        }
                    } else if (ksym == XK_Delete) {
                        if (cursor_pos < (int)strlen(cmd_buf)) {
                            memmove(cmd_buf + cursor_pos, cmd_buf + cursor_pos + 1, strlen(cmd_buf) - cursor_pos);
                            render_all();
                        }
                    } else if (!is_ctrl && len > 0 && (unsigned char)kbuf[0] >= 32) {
                        size_t clen = strlen(cmd_buf);
                        if (clen + len < MAX_CMD_LEN - 1) {
                            memmove(cmd_buf + cursor_pos + len, cmd_buf + cursor_pos, clen - cursor_pos + 1);
                            memcpy(cmd_buf + cursor_pos, kbuf, len);
                            cursor_pos += len;
                            render_all();
                        }
                    }
                } else if (active_focus == 0 || active_focus == 1) {
                    char *p_str = (active_focus == 0) ? udp_port_str : tcp_port_str;
                    if (ksym == XK_BackSpace) {
                        size_t l = strlen(p_str);
                        if (l > 0) p_str[l - 1] = '\0';
                        render_all();
                    } else if (len > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') {
                        size_t l = strlen(p_str);
                        if (l < 5) {
                            p_str[l] = kbuf[0];
                            p_str[l + 1] = '\0';
                            render_all();
                        }
                    } else if (ksym == XK_Return || ksym == XK_KP_Enter) {
                        start_engine();
                    }
                }
                break;
            }

            case SelectionRequest:
                handle_selection_request(&ev.xselectionrequest);
                break;

            case SelectionNotify:
                if (ev.xselection.property == atom_sk3_paste) {
                    if (ev.xselection.target == atom_uri_list) {
                        Atom type;
                        int format;
                        unsigned long nitems, bytes_after;
                        unsigned char *prop = NULL;
                        if (XGetWindowProperty(dpy, win, atom_sk3_paste, 0, 4096, True, AnyPropertyType,
                                               &type, &format, &nitems, &bytes_after, &prop) == Success && prop) {
                            handle_dropped_uri_list((char *)prop);
                            XFree(prop);
                        }
                    } else {
                        handle_paste_notify(&ev.xselection);
                    }
                }
                break;

            case ClientMessage:
                if ((Atom)ev.xclient.data.l[0] == atom_wm_delete_window) {
                    running = 0;
                } else if (ev.xclient.message_type == atom_xdnd_enter) {
                    handle_xdnd_enter(&ev.xclient);
                } else if (ev.xclient.message_type == atom_xdnd_position) {
                    handle_xdnd_position(&ev.xclient);
                } else if (ev.xclient.message_type == atom_xdnd_drop) {
                    handle_xdnd_drop(&ev.xclient);
                }
                break;
        }
    }

    skred_stop();

    if (xft_font_ui) XftFontClose(dpy, xft_font_ui);
    if (xft_font_mono) XftFontClose(dpy, xft_font_mono);
    if (xft_draw) XftDrawDestroy(xft_draw);
    if (backbuffer) XFreePixmap(dpy, backbuffer);
    if (gc) XFreeGC(dpy, gc);
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);

    return 0;
}
