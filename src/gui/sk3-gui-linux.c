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
#include <poll.h>

#include "api.h"
#include "skred.h"
#include "udp.h"
#include "sk3_gui_common.h"

/* UI Dimensions */
#define DEFAULT_WIDTH            640
#define DEFAULT_HEIGHT           560
#define DEFAULT_COLLAPSED_HEIGHT 110

/* Atoms */
static Atom atom_wm_delete_window;
static Atom atom_clipboard;
static Atom atom_utf8_string;
static Atom atom_net_wm_name;
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

/* Geometry tracking */
typedef struct { int x, y, w, h; } rect_t;
static rect_t rendered_udp_box = {0};
static rect_t rendered_tcp_box = {0};
static rect_t rendered_input_box = {0};
static int rendered_log_x = 0;
static int rendered_log_y = 0;
static int rendered_log_w = 0;
static int rendered_log_h = 0;
static int rendered_char_w = 8;
static int rendered_line_h = 16;
static int cur_buf_w = 0;
static int cur_buf_h = 0;

/* Display & Window */
static Display *dpy = NULL;
static int screen = 0;
static Window win = 0;
static Pixmap backbuffer = 0;
static GC gc = 0;
static Visual *visual = NULL;
static Colormap cmap = 0;
static int win_w = DEFAULT_WIDTH;
static int win_h = DEFAULT_COLLAPSED_HEIGHT; /* Start COLLAPSED by default! */
static int is_collapsed = 1;                 /* Default: NO output area initially */
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
static int confirm_restart_active = 0;
static int confirm_exit_active = 0;
static int log_scroll_x = 0;
static int in_scroll_x = 0;
static int active_udp_port = 0;
static int active_tcp_port = 0;

static void save_gui_config(void) {
    sk3_gui_config_t cfg;
    cfg.theme = (int)cur_theme;
    cfg.font_size = (int)cur_font_size;
    cfg.window_width = win_w;
    cfg.window_height = is_collapsed ? prev_expanded_h : win_h;
    cfg.prev_expanded_h = prev_expanded_h;
    cfg.console_collapsed = is_collapsed;
    sk3_gui_config_save(&cfg);
}

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
static void toggle_console(void);

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
            pal.panel_pixel = parse_pixel("#1e170a");
            pal.border_pixel = parse_pixel("#403014");
            pal.btn_pixel = parse_pixel("#281e0e");
            pal.btn_active_pixel = parse_pixel("#3c2c16");
            pal.input_bg_pixel = parse_pixel("#141006");
            pal.cursor_pixel = parse_pixel("#ffb000");
            pal.status_ok_pixel = parse_pixel("#ffc020");
            pal.status_err_pixel = parse_pixel("#ff3333");
            pal.sel_bg_pixel = parse_pixel("#604010");
            pal.text_color = parse_xft_color("#ffb000");
            pal.text_dim = parse_xft_color("#996600");
            pal.status_ok = parse_xft_color("#ffc020");
            pal.status_err = parse_xft_color("#ff3333");
            break;
        case SK3_THEME_GREEN:
            pal.bg_pixel = parse_pixel("#081208");
            pal.panel_pixel = parse_pixel("#0f220f");
            pal.border_pixel = parse_pixel("#184818");
            pal.btn_pixel = parse_pixel("#122a12");
            pal.btn_active_pixel = parse_pixel("#1c421c");
            pal.input_bg_pixel = parse_pixel("#0a180a");
            pal.cursor_pixel = parse_pixel("#39ff14");
            pal.status_ok_pixel = parse_pixel("#00ff66");
            pal.status_err_pixel = parse_pixel("#ff3333");
            pal.sel_bg_pixel = parse_pixel("#005018");
            pal.text_color = parse_xft_color("#39ff14");
            pal.text_dim = parse_xft_color("#1c8008");
            pal.status_ok = parse_xft_color("#00ff66");
            pal.status_err = parse_xft_color("#ff3333");
            break;
        case SK3_THEME_LIGHT:
            pal.bg_pixel = parse_pixel("#eaedf1");
            pal.panel_pixel = parse_pixel("#ffffff");
            pal.border_pixel = parse_pixel("#c0c4cc");
            pal.btn_pixel = parse_pixel("#dce0e6");
            pal.btn_active_pixel = parse_pixel("#c8cdd6");
            pal.input_bg_pixel = parse_pixel("#ffffff");
            pal.cursor_pixel = parse_pixel("#1a1a1a");
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
    if (win) {
        XSetWindowBackground(dpy, win, pal.bg_pixel);
    }
}

static void load_fonts(void) {
    if (xft_font_ui) XftFontClose(dpy, xft_font_ui);
    if (xft_font_mono) XftFontClose(dpy, xft_font_mono);

    int pt_mono = (cur_font_size == SK3_FONT_SIZE_SMALL) ? 10 :
                  (cur_font_size == SK3_FONT_SIZE_LARGE) ? 15 : 12;
    int pt_ui = (cur_font_size == SK3_FONT_SIZE_SMALL) ? 10 :
                (cur_font_size == SK3_FONT_SIZE_LARGE) ? 14 : 12;

    char pattern_mono[64];
    snprintf(pattern_mono, sizeof(pattern_mono), "monospace:size=%d", pt_mono);
    xft_font_mono = XftFontOpenName(dpy, screen, pattern_mono);
    if (!xft_font_mono) xft_font_mono = XftFontOpenName(dpy, screen, "fixed");

    char pattern_ui[64];
    snprintf(pattern_ui, sizeof(pattern_ui), "sans:size=%d", pt_ui);
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

        sk3_gui_format_status(1, req_udp, active_udp_port, req_tcp, active_tcp_port, skred_version(),
                              status_banner, sizeof(status_banner));

        char banner[512];
        char banner_udp[96] = "UDP Port: (disabled)\n";
        char banner_tcp[96] = "TCP Port: (disabled)\n";
        if (req_udp > 0) {
            if (active_udp_port > 0) snprintf(banner_udp, sizeof(banner_udp), "UDP Port: %d (Opcode dispatch & datagrams)\n", active_udp_port);
            else snprintf(banner_udp, sizeof(banner_udp), "UDP Port: FAILED to bind %d (already in use or permission denied)\n", req_udp);
        }
        if (req_tcp > 0) {
            if (active_tcp_port > 0) snprintf(banner_tcp, sizeof(banner_tcp), "TCP Port: %d (Raw TCP, HTTP/1.1, WebSocket)\n", active_tcp_port);
            else snprintf(banner_tcp, sizeof(banner_tcp), "TCP Port: FAILED to bind %d (already in use or permission denied)\n", req_tcp);
        }

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
    BTN_EXIT,
    BTN_TOGGLE_CONSOLE,
    BTN_THEME,
    BTN_FONT,
    BTN_SEND,
    BTN_SILENCE,
    BTN_COPY_LOG,
    BTN_CLEAR_LOG,
    BTN_MACRO_0,
    BTN_MACRO_1,
    BTN_MACRO_2,
    BTN_MACRO_3,
    BTN_MACRO_4,
    BTN_MACRO_5,
    BTN_MACRO_6,
    BTN_MACRO_7,
    BTN_MACRO_8,
    BTN_MACRO_9,
    BTN_MODAL_CONFIRM_RESTART,
    BTN_MODAL_CANCEL_RESTART,
    BTN_MODAL_CONFIRM_EXIT,
    BTN_MODAL_CANCEL_EXIT
};

#define MAX_BUTTONS 32
static btn_t buttons[MAX_BUTTONS];
static int num_buttons = 0;

static void add_btn_exact(int id, int x, int y, int w, int h, const char *label) {
    if (num_buttons >= MAX_BUTTONS) return;
    buttons[num_buttons].id = id;
    buttons[num_buttons].x = x;
    buttons[num_buttons].y = y;
    buttons[num_buttons].w = w;
    buttons[num_buttons].h = h;
    snprintf(buttons[num_buttons].label, sizeof(buttons[num_buttons].label), "%s", label);
    num_buttons++;
}

static void add_btn_auto(int id, int x, int y, const char *label) {
    if (num_buttons >= MAX_BUTTONS) return;

    XGlyphInfo ext;
    XftTextExtentsUtf8(dpy, xft_font_ui, (const FcChar8 *)label, (int)strlen(label), &ext);
    int w = ext.width + 20;
    if (w < 50) w = 50;
    int ui_h = xft_font_ui->ascent + xft_font_ui->descent;
    int h = ui_h + 12;
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
    int ty = b->y + (b->h + xft_font_ui->ascent - xft_font_ui->descent) / 2;
    XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_ui, tx, ty, (const FcChar8 *)b->label, (int)strlen(b->label));
}

static void draw_text_with_braille(int start_x, int baseline_y, int char_w,
                                   const char *text, int text_len,
                                   XftColor *tc, unsigned long fg_pixel) {
    if (!text || text_len <= 0) return;
    int p = 0;
    int cur_x = start_x;
    int run_start = 0;

    int dot_d = (char_w >= 10) ? 2 : 1;
    int y_top = baseline_y - xft_font_mono->ascent + 2;
    int y_span = xft_font_mono->ascent + xft_font_mono->descent - 4;
    if (y_span < 8) y_span = 8;
    int row_y[4] = {
        y_top + (y_span * 1) / 8,
        y_top + (y_span * 3) / 8,
        y_top + (y_span * 5) / 8,
        y_top + (y_span * 7) / 8
    };

    while (p < text_len) {
        unsigned char c0 = (unsigned char)text[p];
        if (c0 == 0xE2 && p + 2 < text_len && ((unsigned char)text[p+1] & 0xFC) == 0xA0) {
            if (p > run_start) {
                int run_len = p - run_start;
                XftDrawStringUtf8(xft_draw, tc, xft_font_mono, cur_x, baseline_y,
                                  (const FcChar8 *)(text + run_start), run_len);
                XGlyphInfo ext;
                XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)(text + run_start), run_len, &ext);
                cur_x += (ext.xOff > 0) ? ext.xOff : run_len * char_w;
            }

            unsigned char c1 = (unsigned char)text[p+1];
            unsigned char c2 = (unsigned char)text[p+2];
            unsigned int pat = ((c1 & 0x03) << 6) | (c2 & 0x3F);

            XSetForeground(dpy, gc, fg_pixel);
            int cx0 = cur_x + (char_w * 3) / 10;
            int cx1 = cur_x + (char_w * 7) / 10;
            if (pat & 0x01) XFillRectangle(dpy, backbuffer, gc, cx0, row_y[0], dot_d, dot_d);
            if (pat & 0x02) XFillRectangle(dpy, backbuffer, gc, cx0, row_y[1], dot_d, dot_d);
            if (pat & 0x04) XFillRectangle(dpy, backbuffer, gc, cx0, row_y[2], dot_d, dot_d);
            if (pat & 0x08) XFillRectangle(dpy, backbuffer, gc, cx1, row_y[0], dot_d, dot_d);
            if (pat & 0x10) XFillRectangle(dpy, backbuffer, gc, cx1, row_y[1], dot_d, dot_d);
            if (pat & 0x20) XFillRectangle(dpy, backbuffer, gc, cx1, row_y[2], dot_d, dot_d);
            if (pat & 0x40) XFillRectangle(dpy, backbuffer, gc, cx0, row_y[3], dot_d, dot_d);
            if (pat & 0x80) XFillRectangle(dpy, backbuffer, gc, cx1, row_y[3], dot_d, dot_d);

            cur_x += char_w;
            p += 3;
            run_start = p;
        } else {
            p++;
        }
    }

    if (p > run_start) {
        XftDrawStringUtf8(xft_draw, tc, xft_font_mono, cur_x, baseline_y,
                          (const FcChar8 *)(text + run_start), p - run_start);
    }
}

static void render_all(void) {
    if (!win || win_w <= 0 || win_h <= 0) return;

    if (!backbuffer || cur_buf_w != win_w || cur_buf_h != win_h) {
        if (backbuffer) XFreePixmap(dpy, backbuffer);
        if (xft_draw) XftDrawDestroy(xft_draw);
        backbuffer = XCreatePixmap(dpy, win, win_w, win_h, DefaultDepth(dpy, screen));
        xft_draw = XftDrawCreate(dpy, backbuffer, visual, cmap);
        cur_buf_w = win_w;
        cur_buf_h = win_h;
    }

    XSetForeground(dpy, gc, pal.bg_pixel);
    XFillRectangle(dpy, backbuffer, gc, 0, 0, win_w, win_h);
    num_buttons = 0;

    int ui_h = xft_font_ui->ascent + xft_font_ui->descent;
    int mono_h = xft_font_mono->ascent + xft_font_mono->descent;
    int btn_h = ui_h + 12;
    if (btn_h < 26) btn_h = 26;

    // Clean Confirmation Modal Screen: renders on a solid background with NO background widgets behind it
    if (confirm_restart_active || confirm_exit_active) {
        int is_exit = confirm_exit_active;
        int mw = (win_w > 440) ? 420 : win_w - 24;
        int mh = (win_h > 150) ? 130 : win_h - 16;
        int mx = (win_w - mw) / 2;
        int my = (win_h - mh) / 2;

        // Modal panel card
        XSetForeground(dpy, gc, pal.panel_pixel);
        XFillRectangle(dpy, backbuffer, gc, mx, my, mw, mh);
        XSetForeground(dpy, gc, pal.status_err_pixel);
        XDrawRectangle(dpy, backbuffer, gc, mx, my, mw, mh);

        // Title: "Restart Audio Engine" or "Exit sk3"
        int title_y = my + ((mh > 100) ? 22 : 16) + xft_font_ui->ascent;
        const char *m_title = is_exit ? "Exit sk3" : "Restart Audio Engine";
        XGlyphInfo ext_t;
        XftTextExtentsUtf8(dpy, xft_font_ui, (const FcChar8 *)m_title, (int)strlen(m_title), &ext_t);
        XftDrawStringUtf8(xft_draw, &pal.status_err, xft_font_ui,
                          mx + (mw - ext_t.width) / 2, title_y,
                          (const FcChar8 *)m_title, (int)strlen(m_title));

        // Question: "Do you really want to do this?"
        int prompt_y = title_y + xft_font_ui->descent + 8 + xft_font_ui->ascent;
        const char *m_prompt = "Do you really want to do this?";
        XGlyphInfo ext_p;
        XftTextExtentsUtf8(dpy, xft_font_ui, (const FcChar8 *)m_prompt, (int)strlen(m_prompt), &ext_p);
        XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_ui,
                          mx + (mw - ext_p.width) / 2, prompt_y,
                          (const FcChar8 *)m_prompt, (int)strlen(m_prompt));

        // Buttons: [Action] and [Cancel]
        int b_y = my + mh - btn_h - 12;
        int b_w = 95;
        int b_act_x = mx + (mw / 2) - b_w - 12;
        int b_cancel_x = mx + (mw / 2) + 12;

        add_btn_exact(is_exit ? BTN_MODAL_CONFIRM_EXIT : BTN_MODAL_CONFIRM_RESTART,
                      b_act_x, b_y, b_w, btn_h, is_exit ? "Exit" : "Restart");
        add_btn_exact(is_exit ? BTN_MODAL_CANCEL_EXIT : BTN_MODAL_CANCEL_RESTART,
                      b_cancel_x, b_y, b_w, btn_h, "Cancel");

        draw_button(&buttons[0]);
        draw_button(&buttons[1]);

        XCopyArea(dpy, backbuffer, win, gc, 0, 0, win_w, win_h, 0, 0);
        XFlush(dpy);
        return;
    }

    // Row 1 (y = 12): Status indicator & Banner on left, Theme & Font buttons right-aligned
    int r1_y = 12;
    int r1_h = btn_h;

    // Status LED circle
    int led_d = 12;
    int led_y = r1_y + (r1_h - led_d) / 2;
    int cur_req_udp = atoi(udp_port_str);
    int cur_req_tcp = atoi(tcp_port_str);
    int any_port_failed = (cur_req_udp > 0 && active_udp_port == 0) || (cur_req_tcp > 0 && active_tcp_port == 0);
    XSetForeground(dpy, gc, (server_running && !any_port_failed) ? pal.status_ok_pixel : pal.status_err_pixel);
    XFillArc(dpy, backbuffer, gc, 16, led_y, led_d, led_d, 0, 360 * 64);
    XSetForeground(dpy, gc, pal.border_pixel);
    XDrawArc(dpy, backbuffer, gc, 16, led_y, led_d, led_d, 0, 360 * 64);

    // Right-aligned top buttons: Font and Theme
    int cur_r = win_w - 16;
    add_btn_auto(BTN_FONT, 0, r1_y, sk3_font_size_name(cur_font_size));
    buttons[num_buttons - 1].x = cur_r - buttons[num_buttons - 1].w;
    cur_r = buttons[num_buttons - 1].x - 8;

    add_btn_auto(BTN_THEME, 0, r1_y, sk3_theme_name(cur_theme));
    buttons[num_buttons - 1].x = cur_r - buttons[num_buttons - 1].w;

    // Status Banner Text
    int banner_ty = r1_y + (r1_h + xft_font_ui->ascent - xft_font_ui->descent) / 2;
    XftDrawStringUtf8(xft_draw, (server_running && !any_port_failed) ? &pal.status_ok : &pal.status_err,
                      xft_font_ui, 36, banner_ty, (const FcChar8 *)status_banner, (int)strlen(status_banner));

    // Row 2: Server Controls & Console Toggle
    int r2_y = r1_y + r1_h + 8;
    int cur_x = 16;

    // UDP status dot, label & box
    int req_udp_val = atoi(udp_port_str);
    unsigned long udp_dot_col = (active_udp_port > 0) ? pal.status_ok_pixel :
                                (req_udp_val > 0) ? pal.status_err_pixel : pal.border_pixel;
    XSetForeground(dpy, gc, udp_dot_col);
    int dot_d = 7;
    int dot_y = r2_y + (btn_h - dot_d) / 2;
    XFillArc(dpy, backbuffer, gc, cur_x, dot_y, dot_d, dot_d, 0, 360 * 64);
    cur_x += dot_d + 6;

    XGlyphInfo ext_lbl;
    XftTextExtentsUtf8(dpy, xft_font_ui, (const FcChar8 *)"UDP:", 4, &ext_lbl);
    int ty_row2 = r2_y + (btn_h + xft_font_ui->ascent - xft_font_ui->descent) / 2;
    XftDrawStringUtf8(xft_draw, (active_udp_port > 0) ? &pal.text_color : &pal.text_dim,
                      xft_font_ui, cur_x, ty_row2, (const FcChar8 *)"UDP:", 4);
    cur_x += ext_lbl.width + 6;

    XGlyphInfo ext_5d;
    XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)"88888", 5, &ext_5d);
    int port_box_w = ext_5d.width + 16;
    if (port_box_w < 55) port_box_w = 55;

    rendered_udp_box.x = cur_x;
    rendered_udp_box.y = r2_y;
    rendered_udp_box.w = port_box_w;
    rendered_udp_box.h = btn_h;

    XSetForeground(dpy, gc, (active_focus == 0) ? pal.input_bg_pixel : pal.panel_pixel);
    XFillRectangle(dpy, backbuffer, gc, cur_x, r2_y, port_box_w, btn_h);
    unsigned long udp_border = (req_udp_val > 0 && active_udp_port == 0) ? pal.status_err_pixel :
                               (active_focus == 0) ? pal.status_ok_pixel : pal.border_pixel;
    XSetForeground(dpy, gc, udp_border);
    XDrawRectangle(dpy, backbuffer, gc, cur_x, r2_y, port_box_w, btn_h);

    int port_ty = r2_y + (btn_h + xft_font_mono->ascent - xft_font_mono->descent) / 2;
    XftColor *udp_text_col = (req_udp_val > 0 && active_udp_port == 0) ? &pal.status_err : &pal.text_color;
    XftDrawStringUtf8(xft_draw, udp_text_col, xft_font_mono, cur_x + 6, port_ty, (const FcChar8 *)udp_port_str, (int)strlen(udp_port_str));

    if (active_focus == 0 && cursor_visible) {
        XGlyphInfo ext_u;
        XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)udp_port_str, (int)strlen(udp_port_str), &ext_u);
        int cx = cur_x + 6 + ext_u.xOff;
        int cur_h = mono_h + 2;
        int cur_y = r2_y + (btn_h - cur_h) / 2;
        XSetForeground(dpy, gc, pal.cursor_pixel);
        XFillRectangle(dpy, backbuffer, gc, cx, cur_y, 2, cur_h);
    }
    cur_x += port_box_w + 14;

    // TCP status dot, label & box
    int req_tcp_val = atoi(tcp_port_str);
    unsigned long tcp_dot_col = (active_tcp_port > 0) ? pal.status_ok_pixel :
                                (req_tcp_val > 0) ? pal.status_err_pixel : pal.border_pixel;
    XSetForeground(dpy, gc, tcp_dot_col);
    XFillArc(dpy, backbuffer, gc, cur_x, dot_y, dot_d, dot_d, 0, 360 * 64);
    cur_x += dot_d + 6;

    XftTextExtentsUtf8(dpy, xft_font_ui, (const FcChar8 *)"TCP:", 4, &ext_lbl);
    XftDrawStringUtf8(xft_draw, (active_tcp_port > 0) ? &pal.text_color : &pal.text_dim,
                      xft_font_ui, cur_x, ty_row2, (const FcChar8 *)"TCP:", 4);
    cur_x += ext_lbl.width + 6;

    rendered_tcp_box.x = cur_x;
    rendered_tcp_box.y = r2_y;
    rendered_tcp_box.w = port_box_w;
    rendered_tcp_box.h = btn_h;

    XSetForeground(dpy, gc, (active_focus == 1) ? pal.input_bg_pixel : pal.panel_pixel);
    XFillRectangle(dpy, backbuffer, gc, cur_x, r2_y, port_box_w, btn_h);
    unsigned long tcp_border = (req_tcp_val > 0 && active_tcp_port == 0) ? pal.status_err_pixel :
                               (active_focus == 1) ? pal.status_ok_pixel : pal.border_pixel;
    XSetForeground(dpy, gc, tcp_border);
    XDrawRectangle(dpy, backbuffer, gc, cur_x, r2_y, port_box_w, btn_h);

    XftColor *tcp_text_col = (req_tcp_val > 0 && active_tcp_port == 0) ? &pal.status_err : &pal.text_color;
    XftDrawStringUtf8(xft_draw, tcp_text_col, xft_font_mono, cur_x + 6, port_ty, (const FcChar8 *)tcp_port_str, (int)strlen(tcp_port_str));

    if (active_focus == 1 && cursor_visible) {
        XGlyphInfo ext_t;
        XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)tcp_port_str, (int)strlen(tcp_port_str), &ext_t);
        int cx = cur_x + 6 + ext_t.xOff;
        int cur_h = mono_h + 2;
        int cur_y = r2_y + (btn_h - cur_h) / 2;
        XSetForeground(dpy, gc, pal.cursor_pixel);
        XFillRectangle(dpy, backbuffer, gc, cx, cur_y, 2, cur_h);
    }
    cur_x += port_box_w + 14;

    // Restart button
    add_btn_auto(BTN_RESTART, cur_x, r2_y, "Restart");
    cur_x += buttons[num_buttons - 1].w + 8;

    // Exit button
    add_btn_auto(BTN_EXIT, cur_x, r2_y, "Exit");
    cur_x += buttons[num_buttons - 1].w + 8;

    // Toggle Console button
    add_btn_auto(BTN_TOGGLE_CONSOLE, cur_x, r2_y, is_collapsed ? "Show" : "Hide");

    int min_collapsed_h = r2_y + btn_h + 12;

    if (!is_collapsed && win_h > min_collapsed_h + 40) {
        // Divider line
        int div_y = r2_y + btn_h + 8;
        XSetForeground(dpy, gc, pal.border_pixel);
        XDrawLine(dpy, backbuffer, gc, 16, div_y, win_w - 16, div_y);

        // Command Input Field (starts directly under divider line)
        int input_y = div_y + 8;
        int input_h = mono_h + 10;
        if (input_h < 28) input_h = 28;

        XGlyphInfo ext_send;
        XftTextExtentsUtf8(dpy, xft_font_ui, (const FcChar8 *)"Send", 4, &ext_send);
        int send_w = ext_send.width + 24;
        int input_w = win_w - 32 - send_w - 8;
        if (input_w < 100) input_w = 100;

        rendered_input_box.x = 16;
        rendered_input_box.y = input_y;
        rendered_input_box.w = input_w;
        rendered_input_box.h = input_h;

        XSetForeground(dpy, gc, pal.input_bg_pixel);
        XFillRectangle(dpy, backbuffer, gc, 16, input_y, input_w, input_h);
        XSetForeground(dpy, gc, (active_focus == 2) ? pal.status_ok_pixel : pal.border_pixel);
        XDrawRectangle(dpy, backbuffer, gc, 16, input_y, input_w, input_h);

        // Set clipping for command input box
        XRectangle in_clip;
        in_clip.x = 18;
        in_clip.y = input_y + 2;
        in_clip.width = (input_w > 4) ? (input_w - 4) : 10;
        in_clip.height = (input_h > 4) ? (input_h - 4) : 10;
        XftDrawSetClipRectangles(xft_draw, 0, 0, &in_clip, 1);
        XSetClipRectangles(dpy, gc, 0, 0, &in_clip, 1, Unsorted);

        XGlyphInfo ext_cursor = {0};
        if (cursor_pos > 0) {
            XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)cmd_buf, cursor_pos, &ext_cursor);
        }
        int cur_advance = ext_cursor.xOff;
        in_scroll_x = 0;
        if (cur_advance > input_w - 30) {
            in_scroll_x = cur_advance - (input_w - 30);
        }

        // Render command text
        int cmd_ty = input_y + (input_h + xft_font_mono->ascent - xft_font_mono->descent) / 2;
        if (cmd_buf[0]) {
            draw_text_with_braille(22 - in_scroll_x, cmd_ty, rendered_char_w, cmd_buf, (int)strlen(cmd_buf),
                                   &pal.text_color, pal.text_color.pixel);
        }

        // Draw blinking cursor if input is focused
        if (active_focus == 2 && cursor_visible) {
            int cx = 22 - in_scroll_x + cur_advance;
            int cur_h = mono_h + 2;
            int cur_y = input_y + (input_h - cur_h) / 2;
            XSetForeground(dpy, gc, pal.cursor_pixel);
            XFillRectangle(dpy, backbuffer, gc, cx, cur_y, 2, cur_h);
        }

        XftDrawSetClip(xft_draw, None);
        XSetClipMask(dpy, gc, None);

        // Send Button
        add_btn_auto(BTN_SEND, 16 + input_w + 8, input_y, "Send");
        buttons[num_buttons - 1].h = input_h;

        // Action Buttons Row: Macro Buttons e!0..e!9, Silence, Copy Log, Clear Log
        int qy = input_y + input_h + 8;

        // Right-aligned buttons: Clear Log, Copy Log, and Silence
        int q_right = win_w - 16;
        add_btn_auto(BTN_CLEAR_LOG, 0, qy, "Clear Log");
        buttons[num_buttons - 1].x = q_right - buttons[num_buttons - 1].w;
        q_right = buttons[num_buttons - 1].x - 6;

        add_btn_auto(BTN_COPY_LOG, 0, qy, "Copy Log");
        buttons[num_buttons - 1].x = q_right - buttons[num_buttons - 1].w;
        q_right = buttons[num_buttons - 1].x - 6;

        add_btn_auto(BTN_SILENCE, 0, qy, "Silence");
        buttons[num_buttons - 1].x = q_right - buttons[num_buttons - 1].w;
        int silence_x = buttons[num_buttons - 1].x;

        // Macro buttons e!0 through e!9 to the left of Silence
        int m_avail = (silence_x - 8) - 16;
        int gap = 4;
        int m_btn_w = (m_avail - 9 * gap) / 10;
        if (m_btn_w > 40) m_btn_w = 40;
        if (m_btn_w < 16) m_btn_w = 16;

        if (m_avail >= 10 * 16) {
            int mx = 16;
            for (int i = 0; i < 10; i++) {
                if (mx + m_btn_w > silence_x - 6) break;
                char mlabel[8];
                snprintf(mlabel, sizeof(mlabel), "%d", i);
                add_btn_exact(BTN_MACRO_0 + i, mx, qy, m_btn_w, btn_h, mlabel);
                mx += m_btn_w + gap;
            }
        }

        // Output Log Area
        int log_y = qy + btn_h + 8;
        int log_h = win_h - log_y - 12;
        if (log_h > 40) {
            rendered_log_x = 16;
            rendered_log_y = log_y;
            rendered_log_w = win_w - 32;
            rendered_log_h = log_h;

            // Fill background of output log area with pal.panel_pixel!
            XSetForeground(dpy, gc, pal.panel_pixel);
            XFillRectangle(dpy, backbuffer, gc, 16, log_y, win_w - 32, log_h);
            XSetForeground(dpy, gc, pal.border_pixel);
            XDrawRectangle(dpy, backbuffer, gc, 16, log_y, win_w - 32, log_h);

            XGlyphInfo ext_m;
            XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)"M", 1, &ext_m);
            int char_w = ext_m.xOff > 0 ? ext_m.xOff : ext_m.width;
            if (char_w <= 0) char_w = 8;
            rendered_char_w = char_w;

            int line_h = mono_h + 4;
            if (line_h <= 0) line_h = 16;
            rendered_line_h = line_h;

            lines_per_page = (log_h - 10) / line_h;
            if (lines_per_page < 1) lines_per_page = 1;

            if (scroll_offset + lines_per_page > log_line_count) {
                scroll_offset = log_line_count - lines_per_page;
            }
            if (scroll_offset < 0) scroll_offset = 0;

            // Hardware clip to output log area so text never spills outside its rectangle!
            XRectangle log_clip;
            log_clip.x = rendered_log_x + 2;
            log_clip.y = rendered_log_y + 2;
            int text_area_w = rendered_log_w - 4;
            if (log_line_count > lines_per_page && text_area_w > 20) {
                text_area_w -= 16; /* Leave margin for scrollbar track */
            }
            log_clip.width = (text_area_w > 0) ? text_area_w : 10;
            log_clip.height = (rendered_log_h > 4) ? (rendered_log_h - 4) : 10;

            XftDrawSetClipRectangles(xft_draw, 0, 0, &log_clip, 1);
            XSetClipRectangles(dpy, gc, 0, 0, &log_clip, 1, Unsorted);

            int draw_y = log_y + 5 + xft_font_mono->ascent;
            for (int i = 0; i < lines_per_page; i++) {
                int l_idx = scroll_offset + i;
                if (l_idx >= log_line_count) break;
                const char *line = log_lines[l_idx];
                if (line) {
                    int line_len = (int)strlen(line);
                    // Draw selection highlight rectangle behind text if active!
                    if (sel_active && l_idx >= sel_start_line && l_idx <= sel_end_line) {
                        int c1 = (l_idx == sel_start_line) ? sel_start_col : 0;
                        int c2 = (l_idx == sel_end_line) ? sel_end_col : line_len;
                        if (c1 < 0) c1 = 0;
                        if (c2 > line_len) c2 = line_len;
                        if (c1 < c2) {
                            int hx = 24 - log_scroll_x + c1 * char_w;
                            int hw = (c2 - c1) * char_w;
                            int hy = draw_y - xft_font_mono->ascent;
                            XSetForeground(dpy, gc, pal.sel_bg_pixel);
                            XFillRectangle(dpy, backbuffer, gc, hx, hy, hw, line_h);
                        }
                    }

                    XftColor *tc = &pal.text_color;
                    if (line[0] == '>') tc = &pal.status_ok;
                    else if (strncmp(line, "Error", 5) == 0 || strncmp(line, "AUDIO START FAILED", 18) == 0) tc = &pal.status_err;
                    else if (line[0] == '[') tc = &pal.text_dim;
                    draw_text_with_braille(24 - log_scroll_x, draw_y, char_w, line, line_len, tc, tc->pixel);
                }
                draw_y += line_h;
            }

            // Restore clipping before drawing scrollbar
            XftDrawSetClip(xft_draw, None);
            XSetClipMask(dpy, gc, None);

            // Scrollbar track & thumb
            if (log_line_count > lines_per_page) {
                int sb_x = win_w - 28;
                int sb_y = log_y + 4;
                int sb_h = log_h - 8;
                XSetForeground(dpy, gc, pal.btn_pixel);
                XFillRectangle(dpy, backbuffer, gc, sb_x, sb_y, 6, sb_h);

                float thumb_ratio = (float)lines_per_page / (float)log_line_count;
                int thumb_h = (int)(thumb_ratio * sb_h);
                if (thumb_h < 15) thumb_h = 15;
                float pos_ratio = (float)scroll_offset / (float)(log_line_count - lines_per_page);
                int thumb_y = sb_y + (int)(pos_ratio * (sb_h - thumb_h));

                XSetForeground(dpy, gc, pal.border_pixel);
                XFillRectangle(dpy, backbuffer, gc, sb_x, thumb_y, 6, thumb_h);
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

            skred_set_string(path);
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
    log_scroll_x = 0;
    int target_h;
    if (is_collapsed) {
        prev_expanded_h = win_h;
        target_h = DEFAULT_COLLAPSED_HEIGHT;
    } else {
        target_h = (prev_expanded_h > DEFAULT_COLLAPSED_HEIGHT + 80) ? prev_expanded_h : DEFAULT_HEIGHT;
        active_focus = 2; /* Automatically focus input when expanded */
        cursor_visible = 1;
    }
    win_h = target_h;
    XResizeWindow(dpy, win, win_w, target_h);

    if (backbuffer) XFreePixmap(dpy, backbuffer);
    if (xft_draw) XftDrawDestroy(xft_draw);
    backbuffer = XCreatePixmap(dpy, win, win_w, win_h, DefaultDepth(dpy, screen));
    xft_draw = XftDrawCreate(dpy, backbuffer, visual, cmap);
    cur_buf_w = win_w;
    cur_buf_h = win_h;
    render_all();
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
    atom_net_wm_name = XInternAtom(dpy, "_NET_WM_NAME", False);
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

    // Load saved configuration from system appropriate config text file
    sk3_gui_config_t cfg;
    sk3_gui_config_init_defaults(&cfg);
    if (sk3_gui_config_load(&cfg) == 0) {
        cur_theme = (sk3_theme_t)cfg.theme;
        cur_font_size = (sk3_font_size_t)cfg.font_size;
        win_w = cfg.window_width;
        prev_expanded_h = cfg.prev_expanded_h;
        is_collapsed = cfg.console_collapsed;
        win_h = is_collapsed ? DEFAULT_COLLAPSED_HEIGHT : cfg.window_height;
    } else {
        win_w = DEFAULT_WIDTH;
        win_h = DEFAULT_HEIGHT;
        is_collapsed = 0;
        prev_expanded_h = DEFAULT_HEIGHT;
    }

    update_palette();
    load_fonts();

    XSetWindowAttributes swa;
    swa.background_pixel = pal.bg_pixel;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                     StructureNotifyMask | PointerMotionMask | FocusChangeMask;

    win = XCreateWindow(dpy, root, 100, 100, win_w, win_h, 0,
                        CopyFromParent, InputOutput, visual,
                        CWBackPixel | CWEventMask, &swa);

    char win_title[128];
    snprintf(win_title, sizeof(win_title), "sk3 v%s - Audio Engine & Control Server", skred_version());
    XStoreName(dpy, win, win_title);
    XChangeProperty(dpy, win, atom_net_wm_name, atom_utf8_string, 8, PropModeReplace,
                    (const unsigned char *)win_title, (int)strlen(win_title));
    XClassHint class_hint = { "sk3-gui", "Sk3" };
    XSetClassHint(dpy, win, &class_hint);

    // Set WM hints to accept keyboard input and initialize in normal state
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
        sh->min_width = 500;
        sh->min_height = DEFAULT_COLLAPSED_HEIGHT;
        XSetWMNormalHints(dpy, win, sh);
        XFree(sh);
    }

    XSetWMProtocols(dpy, win, &atom_wm_delete_window, 1);

    Atom xdnd_version = 5;
    XChangeProperty(dpy, win, atom_xdnd_aware, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&xdnd_version, 1);

    XGCValues gcv;
    gcv.graphics_exposures = False;
    gc = XCreateGC(dpy, win, GCGraphicsExposures, &gcv);
    cur_buf_w = win_w;
    cur_buf_h = win_h;
    backbuffer = XCreatePixmap(dpy, win, win_w, win_h, DefaultDepth(dpy, screen));
    xft_draw = XftDrawCreate(dpy, backbuffer, visual, cmap);

    XMapWindow(dpy, win);
    start_engine();
    render_all();
    XFlush(dpy);

    int running = 1;
    int x11_fd = ConnectionNumber(dpy);
    struct timeval last_blink;
    gettimeofday(&last_blink, NULL);

    while (running) {
        while (XPending(dpy) > 0) {
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
                if (ev.xconfigure.width != cur_buf_w || ev.xconfigure.height != cur_buf_h) {
                    win_w = ev.xconfigure.width;
                    win_h = ev.xconfigure.height;
                    // Automatically track collapsed state based on window height
                    if (win_h > DEFAULT_COLLAPSED_HEIGHT + 60) {
                        is_collapsed = 0;
                        prev_expanded_h = win_h;
                    } else {
                        is_collapsed = 1;
                    }
                    if (backbuffer) XFreePixmap(dpy, backbuffer);
                    if (xft_draw) XftDrawDestroy(xft_draw);
                    backbuffer = XCreatePixmap(dpy, win, win_w, win_h, DefaultDepth(dpy, screen));
                    xft_draw = XftDrawCreate(dpy, backbuffer, visual, cmap);
                    cur_buf_w = win_w;
                    cur_buf_h = win_h;
                    render_all();
                }
                break;

            case ButtonPress: {
                XSetInputFocus(dpy, win, RevertToNone, CurrentTime);
                int mx = ev.xbutton.x;
                int my = ev.xbutton.y;

                if (confirm_restart_active || confirm_exit_active) {
                    int clicked_btn = 0;
                    for (int i = 0; i < num_buttons; i++) {
                        btn_t *b = &buttons[i];
                        if (mx >= b->x && mx <= b->x + b->w && my >= b->y && my <= b->y + b->h) {
                            clicked_btn = b->id;
                            break;
                        }
                    }
                    if (clicked_btn == BTN_MODAL_CONFIRM_RESTART) {
                        confirm_restart_active = 0;
                        start_engine();
                    } else if (clicked_btn == BTN_MODAL_CONFIRM_EXIT) {
                        confirm_exit_active = 0;
                        save_gui_config();
                        running = 0;
                    } else if (clicked_btn == BTN_MODAL_CANCEL_RESTART || clicked_btn == BTN_MODAL_CANCEL_EXIT) {
                        confirm_restart_active = 0;
                        confirm_exit_active = 0;
                        render_all();
                    }
                    break;
                }

                if (ev.xbutton.button == 4) {
                    if (ev.xbutton.state & ShiftMask) {
                        if (log_scroll_x > 0) log_scroll_x -= 30;
                        if (log_scroll_x < 0) log_scroll_x = 0;
                    } else {
                        if (scroll_offset > 0) scroll_offset -= 3;
                        if (scroll_offset < 0) scroll_offset = 0;
                    }
                    render_all();
                    break;
                } else if (ev.xbutton.button == 5) {
                    if (ev.xbutton.state & ShiftMask) {
                        log_scroll_x += 30;
                    } else {
                        if (scroll_offset + lines_per_page < log_line_count) scroll_offset += 3;
                    }
                    render_all();
                    break;
                } else if (ev.xbutton.button == 6) {
                    if (log_scroll_x > 0) log_scroll_x -= 30;
                    if (log_scroll_x < 0) log_scroll_x = 0;
                    render_all();
                    break;
                } else if (ev.xbutton.button == 7) {
                    log_scroll_x += 30;
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
                    confirm_restart_active = 1;
                    render_all();
                } else if (clicked_btn == BTN_EXIT) {
                    confirm_exit_active = 1;
                    render_all();
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
                } else if (clicked_btn >= BTN_MACRO_0 && clicked_btn <= BTN_MACRO_9) {
                    int slot = clicked_btn - BTN_MACRO_0;
                    char mcmd[16];
                    snprintf(mcmd, sizeof(mcmd), "e!%d;", slot);
                    execute_command(mcmd);
                    active_focus = 2;
                } else if (clicked_btn == BTN_SILENCE) {
                    execute_command("O Z0;");
                    active_focus = 2;
                } else if (clicked_btn == BTN_COPY_LOG) {
                    copy_full_log();
                    active_focus = 2;
                } else if (clicked_btn == BTN_CLEAR_LOG) {
                    for (int i = 0; i < log_line_count; i++) free(log_lines[i]);
                    log_line_count = 0;
                    scroll_offset = 0;
                    log_scroll_x = 0;
                    sel_active = 0;
                    render_all();
                    active_focus = 2;
                } else {
                    if (mx >= rendered_udp_box.x && mx <= rendered_udp_box.x + rendered_udp_box.w &&
                        my >= rendered_udp_box.y && my <= rendered_udp_box.y + rendered_udp_box.h) {
                        active_focus = 0;
                        cursor_visible = 1;
                        gettimeofday(&last_blink, NULL);
                        sel_active = 0;
                    } else if (mx >= rendered_tcp_box.x && mx <= rendered_tcp_box.x + rendered_tcp_box.w &&
                               my >= rendered_tcp_box.y && my <= rendered_tcp_box.y + rendered_tcp_box.h) {
                        active_focus = 1;
                        cursor_visible = 1;
                        gettimeofday(&last_blink, NULL);
                        sel_active = 0;
                    } else if (!is_collapsed &&
                               mx >= rendered_input_box.x && mx <= rendered_input_box.x + rendered_input_box.w &&
                               my >= rendered_input_box.y && my <= rendered_input_box.y + rendered_input_box.h) {
                        active_focus = 2;
                        cursor_visible = 1;
                        gettimeofday(&last_blink, NULL);
                        sel_active = 0;
                        int clk_x = mx - 22 + in_scroll_x;
                        if (clk_x <= 0) {
                            cursor_pos = 0;
                        } else {
                            int best_pos = 0;
                            int best_diff = clk_x;
                            int clen = (int)strlen(cmd_buf);
                            for (int p = 1; p <= clen; p++) {
                                XGlyphInfo ext_p = {0};
                                XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)cmd_buf, p, &ext_p);
                                int diff = abs(clk_x - ext_p.xOff);
                                if (diff < best_diff) {
                                    best_diff = diff;
                                    best_pos = p;
                                }
                            }
                            cursor_pos = best_pos;
                        }
                    } else if (!is_collapsed &&
                               mx >= rendered_log_x && mx <= rendered_log_x + rendered_log_w &&
                               my >= rendered_log_y && my <= rendered_log_y + rendered_log_h) {
                        active_focus = 3;
                        int cl = scroll_offset + (my - (rendered_log_y + 5)) / rendered_line_h;
                        if (cl < 0) cl = 0;
                        if (cl >= log_line_count) cl = log_line_count > 0 ? log_line_count - 1 : 0;
                        int cc = (mx - (24 - log_scroll_x) + rendered_char_w / 2) / rendered_char_w;
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

                    int cur_l = scroll_offset + (my - (rendered_log_y + 5)) / rendered_line_h;
                    if (cur_l < 0) cur_l = 0;
                    if (cur_l >= log_line_count) cur_l = log_line_count > 0 ? log_line_count - 1 : 0;
                    int cur_c = (mx - (24 - log_scroll_x) + rendered_char_w / 2) / rendered_char_w;
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

                if (confirm_restart_active || confirm_exit_active) {
                    if (ksym == XK_Escape) {
                        confirm_restart_active = 0;
                        confirm_exit_active = 0;
                        render_all();
                    } else if (ksym == XK_Return || ksym == XK_KP_Enter) {
                        if (confirm_exit_active) {
                            confirm_exit_active = 0;
                            save_gui_config();
                            running = 0;
                        } else {
                            confirm_restart_active = 0;
                            start_engine();
                        }
                    }
                    break;
                }

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
                        cursor_visible = 1;
                        gettimeofday(&last_blink, NULL);
                        render_all();
                    } else if (len > 0 && kbuf[0] >= '0' && kbuf[0] <= '9') {
                        size_t l = strlen(p_str);
                        if (l < 5) {
                            p_str[l] = kbuf[0];
                            p_str[l + 1] = '\0';
                            cursor_visible = 1;
                            gettimeofday(&last_blink, NULL);
                            render_all();
                        }
                    } else if (ksym == XK_Return || ksym == XK_KP_Enter) {
                        confirm_restart_active = 1;
                        render_all();
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
                    confirm_exit_active = 1;
                    render_all();
                } else if (ev.xclient.message_type == atom_xdnd_enter) {
                    handle_xdnd_enter(&ev.xclient);
                } else if (ev.xclient.message_type == atom_xdnd_position) {
                    handle_xdnd_position(&ev.xclient);
                } else if (ev.xclient.message_type == atom_xdnd_drop) {
                    handle_xdnd_drop(&ev.xclient);
                }
                break;
            }
            if (!running) break;
        }

        if (!running) break;

        // Check cursor blinking timer when no events are queued
        struct timeval now;
        gettimeofday(&now, NULL);
        long elapsed_ms = (now.tv_sec - last_blink.tv_sec) * 1000 + (now.tv_usec - last_blink.tv_usec) / 1000;
        int timeout_ms = 500 - (int)elapsed_ms;
        if (timeout_ms <= 0) {
            cursor_visible = !cursor_visible;
            last_blink = now;
            if (active_focus == 0 || active_focus == 1 || (!is_collapsed && active_focus == 2)) {
                render_all();
            }
            timeout_ms = 500;
        }

        struct pollfd pfd;
        pfd.fd = x11_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;
        poll(&pfd, 1, timeout_ms);
    }

    save_gui_config();
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
