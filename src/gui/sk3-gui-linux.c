#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

#include "api.h"
#include "skred.h"
#include "sk3_gui_common.h"

/* UI Dimensions */
#define DEFAULT_WIDTH  640
#define DEFAULT_HEIGHT 580
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
static int win_h = DEFAULT_HEIGHT;
static int is_collapsed = 0;
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
    XftColor text_color;
    XftColor text_dim;
    XftColor status_ok;
    XftColor status_err;
    XftColor input_bg;
    XftColor input_border;
    XftColor cursor_color;
} gui_palette_t;

static gui_palette_t pal;

/* State */
static char udp_port_str[16] = "60440";
static char tcp_port_str[16] = "60440";
static char status_banner[256] = "Starting audio engine...";
static int server_running = 0;

/* Command Input */
#define MAX_CMD_LEN 2048
static char cmd_buf[MAX_CMD_LEN] = {0};
static int cursor_pos = 0;
static int active_focus = 2; /* 0: udp, 1: tcp, 2: cmd input */

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
static int scroll_offset = 0; /* from top */
static int lines_per_page = 20;

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

/* Color helper */
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
            pal.text_color = parse_xft_color("#ffb000");
            pal.text_dim = parse_xft_color("#996600");
            pal.status_ok = parse_xft_color("#ffc020");
            pal.status_err = parse_xft_color("#ff3333");
            pal.input_bg = parse_xft_color("#161006");
            pal.input_border = parse_xft_color("#ffb000");
            pal.cursor_color = parse_xft_color("#ffb000");
            break;
        case SK3_THEME_GREEN:
            pal.bg_pixel = parse_pixel("#081208");
            pal.panel_pixel = parse_pixel("#0e200e");
            pal.border_pixel = parse_pixel("#143c14");
            pal.btn_pixel = parse_pixel("#122a12");
            pal.btn_active_pixel = parse_pixel("#1c421c");
            pal.text_color = parse_xft_color("#39ff14");
            pal.text_dim = parse_xft_color("#1c8008");
            pal.status_ok = parse_xft_color("#00ff66");
            pal.status_err = parse_xft_color("#ff3333");
            pal.input_bg = parse_xft_color("#0a180a");
            pal.input_border = parse_xft_color("#39ff14");
            pal.cursor_color = parse_xft_color("#39ff14");
            break;
        case SK3_THEME_LIGHT:
            pal.bg_pixel = parse_pixel("#f2f3f5");
            pal.panel_pixel = parse_pixel("#ffffff");
            pal.border_pixel = parse_pixel("#cccccc");
            pal.btn_pixel = parse_pixel("#e4e6eb");
            pal.btn_active_pixel = parse_pixel("#d0d3d9");
            pal.text_color = parse_xft_color("#1a1a1a");
            pal.text_dim = parse_xft_color("#666666");
            pal.status_ok = parse_xft_color("#008020");
            pal.status_err = parse_xft_color("#cc0000");
            pal.input_bg = parse_xft_color("#ffffff");
            pal.input_border = parse_xft_color("#999999");
            pal.cursor_color = parse_xft_color("#000000");
            break;
        case SK3_THEME_DARK:
        default:
            pal.bg_pixel = parse_pixel("#1a1a1c");
            pal.panel_pixel = parse_pixel("#242428");
            pal.border_pixel = parse_pixel("#3a3a40");
            pal.btn_pixel = parse_pixel("#2e2e34");
            pal.btn_active_pixel = parse_pixel("#42424b");
            pal.text_color = parse_xft_color("#e0e0e0");
            pal.text_dim = parse_xft_color("#88888e");
            pal.status_ok = parse_xft_color("#39ff14");
            pal.status_err = parse_xft_color("#ff4444");
            pal.input_bg = parse_xft_color("#202024");
            pal.input_border = parse_xft_color("#4a4a52");
            pal.cursor_color = parse_xft_color("#39ff14");
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
    // Auto-scroll to bottom
    if (log_line_count > lines_per_page) {
        scroll_offset = log_line_count - lines_per_page;
    } else {
        scroll_offset = 0;
    }
    render_all();
}

static void execute_command(const char *raw_cmd) {
    if (!raw_cmd) return;
    // Trim
    while (*raw_cmd == ' ' || *raw_cmd == '\t') raw_cmd++;
    if (!raw_cmd[0]) return;

    // Add to history
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
    render_all();
}

static void start_engine(void) {
    int udp_port = atoi(udp_port_str);
    int tcp_port = atoi(tcp_port_str);
    if (udp_port <= 0) udp_port = 60440;
    if (tcp_port <= 0) tcp_port = 60440;

    skred_stop();
    skred_set_audio_device(-1, -1);
    int r = skred_start(128, 256, udp_port);
    int hp = 0;
    if (tcp_port > 0) {
        hp = skred_http_start(tcp_port);
    }
    skred_logger(1);

    if (r == 0) {
        server_running = 1;
        snprintf(status_banner, sizeof(status_banner), "● RUNNING: sk3 v%s (UDP: %d, TCP: %d)",
                 skred_version(), udp_port, hp > 0 ? hp : tcp_port);
        char banner[512];
        snprintf(banner, sizeof(banner),
            "=== sk3 v%s Audio Engine ===\n"
            "Status: Active (256 voices, 48kHz audio)\n"
            "UDP Port: %d (Opcode dispatch & datagrams)\n"
            "TCP Port: %d (Raw TCP, HTTP/1.1, WebSocket)\n"
            "Engine ready for Skode commands.",
            skred_version(), udp_port, hp > 0 ? hp : tcp_port);
        append_log(banner);
    } else {
        server_running = 0;
        snprintf(status_banner, sizeof(status_banner), "○ AUDIO START FAILED (Error %d)", r);
        append_log("Error: Failed to initialize audio output device.");
    }
    render_all();
}

/* Button bounds */
typedef struct {
    int x, y, w, h;
    const char *label;
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

static void add_btn(int id, int x, int y, int w, int h, const char *label) {
    if (num_buttons < MAX_BUTTONS) {
        buttons[num_buttons].id = id;
        buttons[num_buttons].x = x;
        buttons[num_buttons].y = y;
        buttons[num_buttons].w = w;
        buttons[num_buttons].h = h;
        buttons[num_buttons].label = label;
        num_buttons++;
    }
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

    // Fill background
    XSetForeground(dpy, gc, pal.bg_pixel);
    XFillRectangle(dpy, backbuffer, gc, 0, 0, win_w, win_h);
    num_buttons = 0;

    // Status Banner (Top)
    XftDrawStringUtf8(xft_draw, server_running ? &pal.status_ok : &pal.status_err,
                      xft_font_ui, 20, 26, (const FcChar8 *)status_banner, (int)strlen(status_banner));

    // Server Controls Row (y = 48)
    XftDrawStringUtf8(xft_draw, &pal.text_dim, xft_font_ui, 20, 68, (const FcChar8 *)"UDP:", 4);
    
    // UDP Input box
    XSetForeground(dpy, gc, (active_focus == 0) ? pal.border_pixel : pal.panel_pixel);
    XFillRectangle(dpy, backbuffer, gc, 55, 50, 65, 26);
    XSetForeground(dpy, gc, (active_focus == 0) ? pal.status_ok.color.alpha : pal.border_pixel);
    XDrawRectangle(dpy, backbuffer, gc, 55, 50, 65, 26);
    XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_mono, 60, 68, (const FcChar8 *)udp_port_str, (int)strlen(udp_port_str));

    XftDrawStringUtf8(xft_draw, &pal.text_dim, xft_font_ui, 130, 68, (const FcChar8 *)"TCP:", 4);
    
    // TCP Input box
    XSetForeground(dpy, gc, (active_focus == 1) ? pal.border_pixel : pal.panel_pixel);
    XFillRectangle(dpy, backbuffer, gc, 165, 50, 65, 26);
    XSetForeground(dpy, gc, (active_focus == 1) ? pal.status_ok.color.alpha : pal.border_pixel);
    XDrawRectangle(dpy, backbuffer, gc, 165, 50, 65, 26);
    XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_mono, 170, 68, (const FcChar8 *)tcp_port_str, (int)strlen(tcp_port_str));

    // Server Action Buttons
    add_btn(BTN_RESTART, 240, 50, 80, 26, "Restart");
    add_btn(BTN_TOGGLE_CONSOLE, 330, 50, 110, 26, is_collapsed ? "Console ▼" : "Console ▲");
    add_btn(BTN_THEME, 450, 50, 95, 26, sk3_theme_name(cur_theme));
    add_btn(BTN_FONT, 555, 50, 65, 26, (cur_font_size == SK3_FONT_SIZE_SMALL) ? "Font: S" :
                                        (cur_font_size == SK3_FONT_SIZE_LARGE) ? "Font: L" : "Font: M");

    if (!is_collapsed && win_h > COLLAPSED_HEIGHT + 100) {
        // Divider
        XSetForeground(dpy, gc, pal.border_pixel);
        XDrawLine(dpy, backbuffer, gc, 20, 90, win_w - 20, 90);

        // Skode Command Input Label
        XftDrawStringUtf8(xft_draw, &pal.text_dim, xft_font_ui, 20, 114,
                          (const FcChar8 *)"Skode Command (Enter to run, Up/Down for history):", 51);

        // Command Input Field (Monospaced)
        int input_w = win_w - 120;
        XSetForeground(dpy, gc, pal.input_bg.color.alpha);
        XFillRectangle(dpy, backbuffer, gc, 20, 124, input_w, 28);
        XSetForeground(dpy, gc, (active_focus == 2) ? pal.btn_active_pixel : pal.border_pixel);
        XDrawRectangle(dpy, backbuffer, gc, 20, 124, input_w, 28);

        // Render command text
        if (cmd_buf[0]) {
            XftDrawStringUtf8(xft_draw, &pal.text_color, xft_font_mono, 26, 143,
                              (const FcChar8 *)cmd_buf, (int)strlen(cmd_buf));
        }

        // Draw cursor if input is focused
        if (active_focus == 2) {
            XGlyphInfo ext_cursor = {0};
            if (cursor_pos > 0) {
                XftTextExtentsUtf8(dpy, xft_font_mono, (const FcChar8 *)cmd_buf, cursor_pos, &ext_cursor);
            }
            int cx = 26 + ext_cursor.width;
            XSetForeground(dpy, gc, pal.cursor_color.color.alpha);
            XFillRectangle(dpy, backbuffer, gc, cx, 128, 2, 20);
        }

        // Send Button
        add_btn(BTN_SEND, win_w - 90, 124, 70, 28, "Send");

        // Quick Actions Row
        add_btn(BTN_TEST, 20, 162, 90, 26, "Test Tone");
        add_btn(BTN_SILENCE, 120, 162, 120, 26, "Silence (O Z0)");
        add_btn(BTN_COPY_LOG, win_w - 180, 162, 80, 26, "Copy Log");
        add_btn(BTN_CLEAR_LOG, win_w - 90, 162, 70, 26, "Clear Log");

        // Output Log Area (Monospace)
        int log_y = 198;
        int log_h = win_h - log_y - 15;
        if (log_h > 40) {
            XSetForeground(dpy, gc, pal.panel_pixel);
            XFillRectangle(dpy, backbuffer, gc, 20, log_y, win_w - 40, log_h);
            XSetForeground(dpy, gc, pal.border_pixel);
            XDrawRectangle(dpy, backbuffer, gc, 20, log_y, win_w - 40, log_h);

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
                    XftColor *tc = &pal.text_color;
                    if (line[0] == '>') tc = &pal.status_ok;
                    else if (strncmp(line, "Error", 5) == 0) tc = &pal.status_err;
                    else if (line[0] == '[') tc = &pal.text_dim;
                    XftDrawStringUtf8(xft_draw, tc, xft_font_mono, 28, draw_y,
                                      (const FcChar8 *)line, (int)strlen(line));
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

    // Blit to screen
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
    if (XGetWindowProperty(dpy, win, sev->property, 0, 4096, True, AnyPropertyType,
                           &type, &format, &nitems, &bytes_after, &prop) == Success && prop) {
        // Insert paste text at cursor in cmd_buf
        size_t plen = strlen((char *)prop);
        size_t cur_len = strlen(cmd_buf);
        if (cur_len + plen < MAX_CMD_LEN - 1) {
            memmove(cmd_buf + cursor_pos + plen, cmd_buf + cursor_pos, cur_len - cursor_pos + 1);
            memcpy(cmd_buf + cursor_pos, prop, plen);
            cursor_pos += (int)plen;
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
    resp.data.l[1] = 1; /* accept flag */
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

            // Decode file:// URI to path
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

static void copy_full_log(void) {
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

    // Initialize Atoms
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

    // Create Window
    XSetWindowAttributes swa;
    swa.background_pixel = pal.bg_pixel;
    swa.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | ButtonReleaseMask |
                     StructureNotifyMask | PointerMotionMask;

    win = XCreateWindow(dpy, root, 100, 100, win_w, win_h, 0,
                        CopyFromParent, InputOutput, visual,
                        CWBackPixel | CWEventMask, &swa);

    // Set Window Title and WM_CLASS
    XStoreName(dpy, win, "sk3 - Audio Engine & Control Server");
    XClassHint class_hint = { "sk3-gui", "Sk3" };
    XSetClassHint(dpy, win, &class_hint);

    // Enable WM close notification
    XSetWMProtocols(dpy, win, &atom_wm_delete_window, 1);

    // Register XDnD Awareness (version 5)
    Atom xdnd_version = 5;
    XChangeProperty(dpy, win, atom_xdnd_aware, XA_ATOM, 32,
                    PropModeReplace, (unsigned char *)&xdnd_version, 1);

    // Create GC and Backbuffer
    gc = XCreateGC(dpy, win, 0, NULL);
    backbuffer = XCreatePixmap(dpy, win, win_w, win_h, DefaultDepth(dpy, screen));
    xft_draw = XftDrawCreate(dpy, backbuffer, visual, cmap);

    XMapWindow(dpy, win);
    start_engine();

    int running = 1;
    XEvent ev;
    while (running) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case Expose:
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
                int mx = ev.xbutton.x;
                int my = ev.xbutton.y;

                // Mouse wheel scrolling in log view
                if (ev.xbutton.button == 4) { // Scroll Up
                    if (scroll_offset > 0) scroll_offset -= 3;
                    if (scroll_offset < 0) scroll_offset = 0;
                    render_all();
                    break;
                } else if (ev.xbutton.button == 5) { // Scroll Down
                    if (scroll_offset + lines_per_page < log_line_count) scroll_offset += 3;
                    render_all();
                    break;
                }

                // Check clicks on buttons
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
                } else if (clicked_btn == BTN_TEST) {
                    execute_command("v0 w1 f440 a-6 l1;");
                } else if (clicked_btn == BTN_SILENCE) {
                    execute_command("O Z0; S-1;");
                } else if (clicked_btn == BTN_COPY_LOG) {
                    copy_full_log();
                } else if (clicked_btn == BTN_CLEAR_LOG) {
                    for (int i = 0; i < log_line_count; i++) free(log_lines[i]);
                    log_line_count = 0;
                    scroll_offset = 0;
                    render_all();
                } else {
                    // Focus checking
                    if (my >= 50 && my <= 76) {
                        if (mx >= 55 && mx <= 120) active_focus = 0; // UDP
                        else if (mx >= 165 && mx <= 230) active_focus = 1; // TCP
                    } else if (!is_collapsed && my >= 124 && my <= 152 && mx >= 20 && mx <= win_w - 100) {
                        active_focus = 2; // Command input
                    }
                    render_all();
                }
                break;
            }

            case KeyPress: {
                char kbuf[32] = {0};
                KeySym ksym = 0;
                int len = XLookupString(&ev.xkey, kbuf, sizeof(kbuf) - 1, &ksym, NULL);
                int is_ctrl = (ev.xkey.state & ControlMask) != 0;

                if (active_focus == 2) { // Command input
                    if (is_ctrl && (ksym == XK_v || ksym == XK_V)) {
                        request_paste();
                    } else if (is_ctrl && (ksym == XK_c || ksym == XK_C)) {
                        set_clipboard(cmd_buf);
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
                                strncpy(current_draft, cmd_buf, sizeof(current_draft));
                                history_idx = history_count - 1;
                            } else if (history_idx > 0) {
                                history_idx--;
                            }
                            strncpy(cmd_buf, history[history_idx], sizeof(cmd_buf) - 1);
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
                    } else if (len > 0 && (unsigned char)kbuf[0] >= 32) {
                        size_t clen = strlen(cmd_buf);
                        if (clen < MAX_CMD_LEN - 2) {
                            memmove(cmd_buf + cursor_pos + 1, cmd_buf + cursor_pos, clen - cursor_pos + 1);
                            cmd_buf[cursor_pos] = kbuf[0];
                            cursor_pos++;
                            render_all();
                        }
                    }
                } else if (active_focus == 0 || active_focus == 1) { // Ports
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
