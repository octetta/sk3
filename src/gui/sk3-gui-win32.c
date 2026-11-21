#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "skred.h"
#include "sk3_gui_common.h"

#define IDC_STATUS_TEXT    101
#define IDC_UDP_PORT       102
#define IDC_TCP_PORT       103
#define IDC_BTN_RESTART    104
#define IDC_CMD_INPUT      105
#define IDC_BTN_SEND       106
#define IDC_LOG_OUTPUT     107
#define IDC_BTN_CLEAR      108
#define IDC_BTN_TEST       109
#define IDC_BTN_PANIC      110
#define IDC_BTN_TOGGLE     111
#define IDC_BTN_THEME      112
#define IDC_BTN_FONT       113
#define IDC_BTN_COPY_LOG   114

#define DEFAULT_WIDTH  630
#define DEFAULT_HEIGHT 580
#define COLLAPSED_HEIGHT 125

#define MAX_HISTORY 128
static char *history[MAX_HISTORY];
static int history_count = 0;
static int history_idx = -1;
static char current_draft[2048] = {0};

static WNDPROC g_oldEditProc = NULL;
static HWND g_hMainWnd = NULL;
static HWND g_hLogOutput = NULL;
static HWND g_hCmdInput = NULL;
static HWND g_hStatusText = NULL;
static HWND g_hBtnToggle = NULL;
static HWND g_hBtnTheme = NULL;
static HWND g_hBtnFont = NULL;

static HFONT g_hFontUI = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontMono = NULL;

static sk3_font_size_t g_curFontSize = SK3_FONT_SIZE_MEDIUM;
static sk3_theme_t g_curTheme = SK3_THEME_DARK;
static int g_isCollapsed = 0;
static int g_prevExpandedH = DEFAULT_HEIGHT;

typedef struct {
    COLORREF bg;
    COLORREF panel;
    COLORREF text;
    COLORREF text_dim;
    COLORREF status_ok;
    COLORREF status_err;
    HBRUSH hBrushBg;
    HBRUSH hBrushPanel;
} win_palette_t;

static win_palette_t g_pal;

static void update_win_palette(void) {
    if (g_pal.hBrushBg) DeleteObject(g_pal.hBrushBg);
    if (g_pal.hBrushPanel) DeleteObject(g_pal.hBrushPanel);

    switch (g_curTheme) {
        case SK3_THEME_AMBER:
            g_pal.bg = RGB(18, 14, 6);
            g_pal.panel = RGB(28, 22, 10);
            g_pal.text = RGB(255, 176, 0);
            g_pal.text_dim = RGB(180, 120, 0);
            g_pal.status_ok = RGB(255, 192, 32);
            g_pal.status_err = RGB(255, 60, 60);
            break;
        case SK3_THEME_GREEN:
            g_pal.bg = RGB(8, 18, 8);
            g_pal.panel = RGB(14, 32, 14);
            g_pal.text = RGB(57, 255, 20);
            g_pal.text_dim = RGB(28, 140, 10);
            g_pal.status_ok = RGB(0, 255, 102);
            g_pal.status_err = RGB(255, 60, 60);
            break;
        case SK3_THEME_LIGHT:
            g_pal.bg = RGB(242, 243, 245);
            g_pal.panel = RGB(255, 255, 255);
            g_pal.text = RGB(26, 26, 26);
            g_pal.text_dim = RGB(100, 100, 100);
            g_pal.status_ok = RGB(0, 140, 30);
            g_pal.status_err = RGB(200, 0, 0);
            break;
        case SK3_THEME_DARK:
        default:
            g_pal.bg = RGB(26, 26, 28);
            g_pal.panel = RGB(36, 36, 40);
            g_pal.text = RGB(224, 224, 224);
            g_pal.text_dim = RGB(140, 140, 145);
            g_pal.status_ok = RGB(57, 255, 20);
            g_pal.status_err = RGB(255, 68, 68);
            break;
    }
    g_pal.hBrushBg = CreateSolidBrush(g_pal.bg);
    g_pal.hBrushPanel = CreateSolidBrush(g_pal.panel);
}

static void update_mono_font(void) {
    if (g_hFontMono) DeleteObject(g_hFontMono);
    int pt = (g_curFontSize == SK3_FONT_SIZE_SMALL) ? 13 :
             (g_curFontSize == SK3_FONT_SIZE_LARGE) ? 20 : 16;
    g_hFontMono = CreateFontA(pt, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
    if (g_hCmdInput) SendMessageA(g_hCmdInput, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
    if (g_hLogOutput) SendMessageA(g_hLogOutput, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);
}

static void append_log(HWND hLog, const char *text) {
    if (!hLog || !text) return;
    int len = GetWindowTextLengthA(hLog);
    SendMessageA(hLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
    SendMessageA(hLog, EM_REPLACESEL, FALSE, (LPARAM)text);
    SendMessageA(hLog, EM_SCROLLCARET, 0, 0);
}

static void append_log_normalized(HWND hLog, const char *text) {
    if (!hLog || !text) return;
    size_t in_len = strlen(text);
    char *norm = (char *)malloc(in_len * 2 + 3);
    if (!norm) return;
    size_t j = 0;
    for (size_t i = 0; i < in_len; i++) {
        if (text[i] == '\n' && (i == 0 || text[i-1] != '\r')) {
            norm[j++] = '\r';
        }
        norm[j++] = text[i];
    }
    norm[j] = '\0';
    append_log(hLog, norm);
    free(norm);
}

static void copy_log_to_clipboard(HWND hLog) {
    if (!hLog) return;
    int len = GetWindowTextLengthA(hLog);
    if (len <= 0) return;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (!hMem) return;
    char *p = (char *)GlobalLock(hMem);
    GetWindowTextA(hLog, p, len + 1);
    GlobalUnlock(hMem);

    if (OpenClipboard(g_hMainWnd)) {
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hMem);
        CloseClipboard();
        append_log(hLog, "\r\n[Log copied to clipboard]\r\n");
    } else {
        GlobalFree(hMem);
    }
}

static void execute_command(HWND hwnd, const char *raw_cmd) {
    if (!raw_cmd || strlen(raw_cmd) == 0) return;
    
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", raw_cmd);
    
    if (history_count == 0 || strcmp(history[history_count - 1], buf) != 0) {
        if (history_count < MAX_HISTORY) {
            history[history_count++] = strdup(buf);
        } else {
            free(history[0]);
            memmove(&history[0], &history[1], sizeof(char *) * (MAX_HISTORY - 1));
            history[MAX_HISTORY - 1] = strdup(buf);
        }
    }
    history_idx = -1;
    current_draft[0] = '\0';
    
    char prompt[2200];
    snprintf(prompt, sizeof(prompt), "\r\n> %s\r\n", buf);
    append_log(g_hLogOutput, prompt);
    
    int r = skred_command(buf);
    char *log = skred_log();
    if (log && strlen(log) > 0) {
        append_log_normalized(g_hLogOutput, log);
    }
    if (r > 0) {
        char r_buf[64];
        snprintf(r_buf, sizeof(r_buf), "r = %d\r\n", r);
        append_log(g_hLogOutput, r_buf);
    }
    
    SetDlgItemTextA(hwnd, IDC_CMD_INPUT, "");
}

static LRESULT CALLBACK CmdInputSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_KEYDOWN) {
        if (wParam == VK_RETURN) {
            char cmd[2048];
            GetWindowTextA(hwnd, cmd, sizeof(cmd));
            execute_command(g_hMainWnd, cmd);
            return 0;
        } else if (wParam == VK_UP) {
            if (history_count > 0) {
                if (history_idx == -1) {
                    GetWindowTextA(hwnd, current_draft, sizeof(current_draft));
                    history_idx = history_count - 1;
                } else if (history_idx > 0) {
                    history_idx--;
                }
                SetWindowTextA(hwnd, history[history_idx]);
                int len = lstrlenA(history[history_idx]);
                SendMessageA(hwnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
            }
            return 0;
        } else if (wParam == VK_DOWN) {
            if (history_idx != -1) {
                history_idx++;
                if (history_idx < history_count) {
                    SetWindowTextA(hwnd, history[history_idx]);
                    int len = lstrlenA(history[history_idx]);
                    SendMessageA(hwnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
                } else {
                    history_idx = -1;
                    SetWindowTextA(hwnd, current_draft);
                    int len = lstrlenA(current_draft);
                    SendMessageA(hwnd, EM_SETSEL, (WPARAM)len, (LPARAM)len);
                }
            }
            return 0;
        }
    } else if (uMsg == WM_CHAR) {
        if (wParam == '\r') {
            return 0; // prevent beep
        } else if (wParam == 1) { // Ctrl+A
            SendMessageA(hwnd, EM_SETSEL, 0, -1);
            return 0;
        }
    }
    return CallWindowProcA(g_oldEditProc, hwnd, uMsg, wParam, lParam);
}

static void start_engine(HWND hwnd) {
    char udp_str[32] = {0};
    char tcp_str[32] = {0};
    GetDlgItemTextA(hwnd, IDC_UDP_PORT, udp_str, sizeof(udp_str));
    GetDlgItemTextA(hwnd, IDC_TCP_PORT, tcp_str, sizeof(tcp_str));
    
    int udp_port = atoi(udp_str);
    int tcp_port = atoi(tcp_str);
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
    
    char status[256];
    if (r == 0) {
        snprintf(status, sizeof(status), "● RUNNING: sk3 v%s (UDP: %d, TCP: %d)",
                 skred_version(), udp_port, hp > 0 ? hp : tcp_port);
        SetDlgItemTextA(hwnd, IDC_STATUS_TEXT, status);
        
        char banner[512];
        snprintf(banner, sizeof(banner),
            "=== sk3 v%s Audio Engine ===\r\n"
            "Status: Active (256 voices, 48kHz audio)\r\n"
            "UDP Port: %d (Opcode dispatch & datagrams)\r\n"
            "TCP Port: %d (Raw TCP, HTTP/1.1, WebSocket)\r\n"
            "Engine ready for Skode commands.\r\n",
            skred_version(), udp_port, hp > 0 ? hp : tcp_port);
        append_log(g_hLogOutput, banner);
    } else {
        snprintf(status, sizeof(status), "○ AUDIO START FAILED (Error %d)", r);
        SetDlgItemTextA(hwnd, IDC_STATUS_TEXT, status);
        append_log(g_hLogOutput, "Error: Failed to initialize audio output device.\r\n");
    }
}

static void toggle_console(HWND hwnd) {
    g_isCollapsed = !g_isCollapsed;
    RECT rc;
    GetWindowRect(hwnd, &rc);
    int cur_w = rc.right - rc.left;

    if (g_isCollapsed) {
        g_prevExpandedH = rc.bottom - rc.top;
        SetWindowPos(hwnd, NULL, 0, 0, cur_w, COLLAPSED_HEIGHT, SWP_NOMOVE | SWP_NOZORDER);
        SetWindowTextA(g_hBtnToggle, "Console ▼");
    } else {
        int target_h = (g_prevExpandedH > COLLAPSED_HEIGHT + 100) ? g_prevExpandedH : DEFAULT_HEIGHT;
        SetWindowPos(hwnd, NULL, 0, 0, cur_w, target_h, SWP_NOMOVE | SWP_NOZORDER);
        SetWindowTextA(g_hBtnToggle, "Console ▲");
    }
}

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            update_win_palette();

            g_hFontUI = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontBold = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");

            // Status label
            g_hStatusText = CreateWindowExA(0, "STATIC", "Starting audio engine...",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                20, 14, 580, 24, hwnd, (HMENU)IDC_STATUS_TEXT, NULL, NULL);
            SendMessageA(g_hStatusText, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            // Server Ports config
            HWND hUdpLbl = CreateWindowExA(0, "STATIC", "UDP:",
                WS_CHILD | WS_VISIBLE,
                20, 48, 35, 22, hwnd, NULL, NULL, NULL);
            SendMessageA(hUdpLbl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hUdpEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "60440",
                WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_TABSTOP,
                58, 46, 60, 24, hwnd, (HMENU)IDC_UDP_PORT, NULL, NULL);
            SendMessageA(hUdpEdit, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hTcpLbl = CreateWindowExA(0, "STATIC", "TCP:",
                WS_CHILD | WS_VISIBLE,
                126, 48, 35, 22, hwnd, NULL, NULL, NULL);
            SendMessageA(hTcpLbl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hTcpEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "60440",
                WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_TABSTOP,
                164, 46, 60, 24, hwnd, (HMENU)IDC_TCP_PORT, NULL, NULL);
            SendMessageA(hTcpEdit, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hBtnRestart = CreateWindowExA(0, "BUTTON", "Restart",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                232, 45, 68, 26, hwnd, (HMENU)IDC_BTN_RESTART, NULL, NULL);
            SendMessageA(hBtnRestart, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            g_hBtnToggle = CreateWindowExA(0, "BUTTON", "Console ▲",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                306, 45, 88, 26, hwnd, (HMENU)IDC_BTN_TOGGLE, NULL, NULL);
            SendMessageA(g_hBtnToggle, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            g_hBtnTheme = CreateWindowExA(0, "BUTTON", "Theme: Dark",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                400, 45, 95, 26, hwnd, (HMENU)IDC_BTN_THEME, NULL, NULL);
            SendMessageA(g_hBtnTheme, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            g_hBtnFont = CreateWindowExA(0, "BUTTON", "Font: M",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                501, 45, 68, 26, hwnd, (HMENU)IDC_BTN_FONT, NULL, NULL);
            SendMessageA(g_hBtnFont, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            // Command Input Section (Monospaced!)
            HWND hCmdLbl = CreateWindowExA(0, "STATIC", "Skode Command (Enter to run, Up/Down history):",
                WS_CHILD | WS_VISIBLE,
                20, 84, 560, 18, hwnd, NULL, NULL, NULL);
            SendMessageA(hCmdLbl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            g_hCmdInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 105, 480, 26, hwnd, (HMENU)IDC_CMD_INPUT, NULL, NULL);
            g_oldEditProc = (WNDPROC)SetWindowLongPtrA(g_hCmdInput, GWLP_WNDPROC, (LONG_PTR)CmdInputSubclassProc);

            HWND hBtnSend = CreateWindowExA(0, "BUTTON", "Send",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                508, 104, 80, 28, hwnd, (HMENU)IDC_BTN_SEND, NULL, NULL);
            SendMessageA(hBtnSend, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            // Quick actions
            HWND hBtnTest = CreateWindowExA(0, "BUTTON", "Test Tone",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                20, 140, 90, 26, hwnd, (HMENU)IDC_BTN_TEST, NULL, NULL);
            SendMessageA(hBtnTest, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hBtnPanic = CreateWindowExA(0, "BUTTON", "Silence (O Z0)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                116, 140, 115, 26, hwnd, (HMENU)IDC_BTN_PANIC, NULL, NULL);
            SendMessageA(hBtnPanic, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hBtnCopyLog = CreateWindowExA(0, "BUTTON", "Copy Log",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                418, 140, 80, 26, hwnd, (HMENU)IDC_BTN_COPY_LOG, NULL, NULL);
            SendMessageA(hBtnCopyLog, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hBtnClear = CreateWindowExA(0, "BUTTON", "Clear Log",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                508, 140, 80, 26, hwnd, (HMENU)IDC_BTN_CLEAR, NULL, NULL);
            SendMessageA(hBtnClear, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            // Output Log area (Monospaced!)
            g_hLogOutput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP,
                20, 174, 570, 350, hwnd, (HMENU)IDC_LOG_OUTPUT, NULL, NULL);

            update_mono_font();

            // Enable Drag and Drop
            DragAcceptFiles(hwnd, TRUE);

            start_engine(hwnd);
            SetFocus(g_hCmdInput);
            return 0;
        }

        case WM_SIZE: {
            int w = LOWORD(lParam);
            int h = HIWORD(lParam);
            if (w <= 0 || h <= 0) break;

            if (g_hCmdInput) {
                SetWindowPos(g_hCmdInput, NULL, 20, 105, w - 120, 26, SWP_NOZORDER);
            }
            HWND hSend = GetDlgItem(hwnd, IDC_BTN_SEND);
            if (hSend) {
                SetWindowPos(hSend, NULL, w - 90, 104, 70, 28, SWP_NOZORDER);
            }
            HWND hClear = GetDlgItem(hwnd, IDC_BTN_CLEAR);
            if (hClear) {
                SetWindowPos(hClear, NULL, w - 90, 140, 70, 26, SWP_NOZORDER);
            }
            HWND hCopy = GetDlgItem(hwnd, IDC_BTN_COPY_LOG);
            if (hCopy) {
                SetWindowPos(hCopy, NULL, w - 175, 140, 80, 26, SWP_NOZORDER);
            }
            if (g_hLogOutput) {
                int log_h = h - 186;
                if (log_h < 40) log_h = 40;
                SetWindowPos(g_hLogOutput, NULL, 20, 174, w - 40, log_h, SWP_NOZORDER);
            }
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wParam;
            char filepath[MAX_PATH];
            if (DragQueryFileA(hDrop, 0, filepath, sizeof(filepath))) {
                char log_msg[512] = {0};
                char cmd[2048] = {0};
                sk3_gui_format_file_drop(filepath, log_msg, sizeof(log_msg), cmd, sizeof(cmd));
                if (log_msg[0]) append_log_normalized(g_hLogOutput, log_msg);
                if (cmd[0]) SetDlgItemTextA(hwnd, IDC_CMD_INPUT, cmd);
            }
            DragFinish(hDrop);
            return 0;
        }

        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            if (hCtl == g_hStatusText) {
                SetTextColor(hdc, g_pal.status_ok);
                SetBkColor(hdc, g_pal.bg);
                return (LRESULT)g_pal.hBrushBg;
            } else if (hCtl == g_hLogOutput || hCtl == g_hCmdInput) {
                SetTextColor(hdc, g_pal.text);
                SetBkColor(hdc, g_pal.panel);
                return (LRESULT)g_pal.hBrushPanel;
            } else {
                SetTextColor(hdc, g_pal.text_dim);
                SetBkColor(hdc, g_pal.bg);
                return (LRESULT)g_pal.hBrushBg;
            }
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDC_BTN_SEND) {
                char cmd[2048];
                GetDlgItemTextA(hwnd, IDC_CMD_INPUT, cmd, sizeof(cmd));
                execute_command(hwnd, cmd);
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_RESTART) {
                start_engine(hwnd);
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_TOGGLE) {
                toggle_console(hwnd);
            } else if (id == IDC_BTN_THEME) {
                g_curTheme = (g_curTheme + 1) % SK3_THEME_COUNT;
                update_win_palette();
                SetWindowTextA(g_hBtnTheme, sk3_theme_name(g_curTheme));
                InvalidateRect(hwnd, NULL, TRUE);
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_FONT) {
                g_curFontSize = (g_curFontSize + 1) % SK3_FONT_SIZE_COUNT;
                update_mono_font();
                SetWindowTextA(g_hBtnFont, (g_curFontSize == SK3_FONT_SIZE_SMALL) ? "Font: S" :
                                           (g_curFontSize == SK3_FONT_SIZE_LARGE) ? "Font: L" : "Font: M");
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_TEST) {
                execute_command(hwnd, "v0 w1 f440 a-6 l1;");
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_PANIC) {
                execute_command(hwnd, "O Z0; S-1;");
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_COPY_LOG) {
                copy_log_to_clipboard(g_hLogOutput);
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_CLEAR) {
                SetWindowTextA(g_hLogOutput, "");
                SetFocus(g_hCmdInput);
            }
            return 0;
        }

        case WM_DESTROY: {
            skred_stop();
            if (g_hFontUI) DeleteObject(g_hFontUI);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontMono) DeleteObject(g_hFontMono);
            if (g_pal.hBrushBg) DeleteObject(g_pal.hBrushBg);
            if (g_pal.hBrushPanel) DeleteObject(g_pal.hBrushPanel);
            for (int i = 0; i < history_count; i++) {
                if (history[i]) free(history[i]);
            }
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;

    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDPIAwareFunc)(void);
        SetProcessDPIAwareFunc setDpi = (SetProcessDPIAwareFunc)GetProcAddress(hUser32, "SetProcessDPIAware");
        if (setDpi) setDpi();
    }

    InitCommonControls();

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "Sk3GuiClass";
    wc.hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(1));
    wc.hIconSm = (HICON)LoadImageA(hInstance, MAKEINTRESOURCEA(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!wc.hIcon) wc.hIcon = LoadIconA(NULL, IDI_APPLICATION);

    if (!RegisterClassExA(&wc)) {
        return 1;
    }

    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW | WS_EX_ACCEPTFILES,
        "Sk3GuiClass",
        "sk3 - Audio Engine & Control Server",
        WS_OVERLAPPEDWINDOW,
        x, y, width, height,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) return 1;

    g_hMainWnd = hwnd;
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    return (int)msg.wParam;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return WinMain(GetModuleHandleA(NULL), NULL, GetCommandLineA(), SW_SHOWDEFAULT);
}
