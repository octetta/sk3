#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "skred.h"

#define IDC_STATUS_TEXT  101
#define IDC_UDP_PORT     102
#define IDC_TCP_PORT     103
#define IDC_BTN_RESTART  104
#define IDC_CMD_INPUT    105
#define IDC_BTN_SEND     106
#define IDC_LOG_OUTPUT   107
#define IDC_BTN_CLEAR    108
#define IDC_BTN_TEST     109
#define IDC_BTN_PANIC    110

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
static HFONT g_hFontUI = NULL;
static HFONT g_hFontBold = NULL;
static HFONT g_hFontMono = NULL;
static HBRUSH g_hBrushGreen = NULL;

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

static void execute_command(HWND hwnd, const char *raw_cmd) {
    if (!raw_cmd || strlen(raw_cmd) == 0) return;
    
    // Copy into buffer and trim
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", raw_cmd);
    
    // Add to history if unique
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
    
    // Log prompt
    char prompt[2200];
    snprintf(prompt, sizeof(prompt), "\r\n> %s\r\n", buf);
    append_log(g_hLogOutput, prompt);
    
    // Execute command via skred API
    int r = skred_command(buf);
    
    // Read and log engine output
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

static LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Fonts
            g_hFontUI = CreateFontA(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontBold = CreateFontA(17, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
            g_hFontMono = CreateFontA(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                     DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                     CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
            g_hBrushGreen = CreateSolidBrush(RGB(0, 130, 0));

            // Status label
            g_hStatusText = CreateWindowExA(0, "STATIC", "Starting audio engine...",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                20, 16, 560, 24, hwnd, (HMENU)IDC_STATUS_TEXT, NULL, NULL);
            SendMessageA(g_hStatusText, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            // Server Ports config
            HWND hUdpLbl = CreateWindowExA(0, "STATIC", "UDP Port:",
                WS_CHILD | WS_VISIBLE,
                20, 52, 70, 22, hwnd, NULL, NULL, NULL);
            SendMessageA(hUdpLbl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hUdpEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "60440",
                WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_TABSTOP,
                92, 50, 75, 24, hwnd, (HMENU)IDC_UDP_PORT, NULL, NULL);
            SendMessageA(hUdpEdit, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hTcpLbl = CreateWindowExA(0, "STATIC", "TCP Port:",
                WS_CHILD | WS_VISIBLE,
                185, 52, 70, 22, hwnd, NULL, NULL, NULL);
            SendMessageA(hTcpLbl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hTcpEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "60440",
                WS_CHILD | WS_VISIBLE | ES_NUMBER | WS_TABSTOP,
                257, 50, 75, 24, hwnd, (HMENU)IDC_TCP_PORT, NULL, NULL);
            SendMessageA(hTcpEdit, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hBtnRestart = CreateWindowExA(0, "BUTTON", "Restart Server",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                350, 48, 120, 28, hwnd, (HMENU)IDC_BTN_RESTART, NULL, NULL);
            SendMessageA(hBtnRestart, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            // Command Input Section
            HWND hCmdLbl = CreateWindowExA(0, "STATIC", "Skode Command (Enter to execute, Up/Down for history):",
                WS_CHILD | WS_VISIBLE,
                20, 90, 560, 20, hwnd, NULL, NULL, NULL);
            SendMessageA(hCmdLbl, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            g_hCmdInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                20, 114, 460, 26, hwnd, (HMENU)IDC_CMD_INPUT, NULL, NULL);
            SendMessageA(g_hCmdInput, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
            g_oldEditProc = (WNDPROC)SetWindowLongPtrA(g_hCmdInput, GWLP_WNDPROC, (LONG_PTR)CmdInputSubclassProc);

            HWND hBtnSend = CreateWindowExA(0, "BUTTON", "Send",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                490, 113, 90, 28, hwnd, (HMENU)IDC_BTN_SEND, NULL, NULL);
            SendMessageA(hBtnSend, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            // Quick actions
            HWND hBtnTest = CreateWindowExA(0, "BUTTON", "Test Tone",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                20, 150, 95, 26, hwnd, (HMENU)IDC_BTN_TEST, NULL, NULL);
            SendMessageA(hBtnTest, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hBtnPanic = CreateWindowExA(0, "BUTTON", "Silence (O Z0)",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                125, 150, 115, 26, hwnd, (HMENU)IDC_BTN_PANIC, NULL, NULL);
            SendMessageA(hBtnPanic, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            HWND hBtnClear = CreateWindowExA(0, "BUTTON", "Clear Log",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP,
                490, 150, 90, 26, hwnd, (HMENU)IDC_BTN_CLEAR, NULL, NULL);
            SendMessageA(hBtnClear, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);

            // Output Log area
            g_hLogOutput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP,
                20, 185, 560, 315, hwnd, (HMENU)IDC_LOG_OUTPUT, NULL, NULL);
            SendMessageA(g_hLogOutput, WM_SETFONT, (WPARAM)g_hFontMono, TRUE);

            // Start audio engine
            start_engine(hwnd);
            SetFocus(g_hCmdInput);
            return 0;
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
            } else if (id == IDC_BTN_TEST) {
                execute_command(hwnd, "v0 w1 f440 a-6 l1;");
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_PANIC) {
                execute_command(hwnd, "O Z0; S-1;");
                SetFocus(g_hCmdInput);
            } else if (id == IDC_BTN_CLEAR) {
                SetWindowTextA(g_hLogOutput, "");
                SetFocus(g_hCmdInput);
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            HWND hCtl = (HWND)lParam;
            if (hCtl == g_hStatusText) {
                SetTextColor(hdc, RGB(0, 130, 0));
                SetBkMode(hdc, TRANSPARENT);
                return (LRESULT)GetStockObject(NULL_BRUSH);
            }
            break;
        }

        case WM_DESTROY: {
            skred_stop();
            if (g_hFontUI) DeleteObject(g_hFontUI);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontMono) DeleteObject(g_hFontMono);
            if (g_hBrushGreen) DeleteObject(g_hBrushGreen);
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

    // Enable DPI awareness if available
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

    // Fixed-size window centered on screen
    int width = 616;
    int height = 560;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - width) / 2;
    int y = (screenH - height) / 2;
    if (x < 0) x = 0;
    if (y < 0) y = 0;

    HWND hwnd = CreateWindowExA(
        WS_EX_APPWINDOW,
        "Sk3GuiClass",
        "sk3 - Audio Engine & Control Server",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
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

// Fallback entry point when compiled without /subsystem:windows
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;
    return WinMain(GetModuleHandleA(NULL), NULL, GetCommandLineA(), SW_SHOWDEFAULT);
}
