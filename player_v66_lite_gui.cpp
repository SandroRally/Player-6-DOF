/*
 * GoPro Motion Player LITE - C++ GUI Edition
 * Finestra grafica Win32 identica alla versione Python/Tkinter.
 * Compilare con Visual Studio Community come "Applicazione desktop di Windows".
 */
#include "player_core.h"
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")
#pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#pragma comment(linker, "/ENTRY:WinMainCRTStartup")

// ==================== CONTROL IDS ====================
#define ID_EDIT_TELEM    101
#define ID_BTN_BROWSE_T  102
#define ID_EDIT_VIDEO    103
#define ID_BTN_BROWSE_V  104
#define ID_BTN_VLC       105
#define ID_BTN_DEOVR_LOCAL 106
#define ID_BTN_DEOVR_WEB 117
#define ID_BTN_UP        107
#define ID_CHK_TOPMOST   108
#define ID_BTN_PLAY      109
#define ID_BTN_PAUSE     110
#define ID_BTN_STOP      111
#define ID_LABEL_STATUS  112
#define ID_LABEL_FOOTER  113
#define ID_LABEL_TITLE   114
#define ID_EDIT_OFFSET   115
#define ID_BTN_APPLY_OFFSET 116
#define ID_LABEL_INSTR   118
#define ID_TIMER_STATUS  201
#define ID_TIMER_VLC_RESUME 202

// ==================== GLOBALS ====================
static MotionPlayer g_player;
static HWND g_hwnd = NULL;
static HWND g_hEditTelem, g_hEditVideo, g_hEditOffset;
static HWND g_hBtnVlc, g_hBtnDeovrLocal, g_hBtnDeovrWeb, g_hBtnUp;
static HWND g_hBtnPlay, g_hBtnPause, g_hBtnStop;
static HWND g_hLblStatus, g_hLblFooter, g_hLblTitle, g_hLblInstr;
static HWND g_hChkTop;
static HFONT g_fontUI, g_fontTitle, g_fontSmall, g_fontBtn;
static HBRUSH g_brBg, g_brEdit, g_brGreen, g_brGray, g_brOrange;
static COLORREF COL_BG = RGB(43,43,43);
static COLORREF COL_FG = RGB(255,255,255);
static COLORREF COL_GREEN = RGB(0,255,136);
static COLORREF COL_GRAY = RGB(64,64,64);
static COLORREF COL_ORANGE = RGB(255,136,0);
static COLORREF COL_EDIT_BG = RGB(55,55,55);

// ==================== HELPERS ====================
static void SetStatus(const wchar_t* txt) {
    SetWindowTextW(g_hLblStatus, txt);
}
static void SetStatusA(const char* txt) {
    SetWindowTextA(g_hLblStatus, txt);
}

static void UpdateInstructions() {
    if (g_player.player_mode == "VLC") {
        SetWindowTextW(g_hLblInstr, L"VLC Mode: Video opens in a new window.\nGame: Forza Motorsport 7 (Port 5300)");
    } else if (g_player.player_mode == "DEOVR_LOCAL") {
        SetWindowTextW(g_hLblInstr, L"DeoVR Local: Open DeoVR and play ANY video first to connect.\nGame: Forza Motorsport 7 (Port 5300)");
    } else if (g_player.player_mode == "DEOVR_WEB") {
        SetWindowTextW(g_hLblInstr, L"DeoVR Web: Open DeoVR, find the Web video you want to sync and play it.\nGame: Forza Motorsport 7 (Port 5300)");
    }
}

static void UpdatePlayerButtons() {
    InvalidateRect(g_hBtnVlc, NULL, TRUE);
    InvalidateRect(g_hBtnDeovrLocal, NULL, TRUE);
    InvalidateRect(g_hBtnDeovrWeb, NULL, TRUE);
}

static void UpdateFooter() {
    wchar_t buf[128];
    swprintf(buf, 128, L"UDP: %S:%d | Output: %dHz",
             g_player.udp_ip.c_str(), g_player.udp_port, g_player.settings.output_freq);
    SetWindowTextW(g_hLblFooter, buf);
}

static std::wstring BrowseFile(HWND parent, const wchar_t* title, const wchar_t* filter) {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) return std::wstring(path);
    return L"";
}

// ==================== LOAD TELEMETRY ====================
static void DoLoadTelemetry() {
    std::wstring path = BrowseFile(g_hwnd, L"Seleziona File Telemetria JSON",
                                   L"JSON Files\0*.json\0All Files\0*.*\0");
    if (path.empty()) return;
    SetWindowTextW(g_hEditTelem, path.c_str());
    if (g_player.load_file(path)) {
        wchar_t buf[256];
        swprintf(buf, 256, L"Caricato: %d campioni | %.1fs | %dHz",
                 (int)g_player.samples.size(), g_player.get_duration(), g_player.detected_freq);
        SetStatus(buf);
        UpdateFooter();
        EnableWindow(g_hBtnPlay, TRUE);
    } else {
        SetStatus(L"ERRORE: impossibile caricare il file");
        MessageBoxW(g_hwnd, L"Impossibile caricare il file JSON.", L"Errore", MB_OK | MB_ICONERROR);
    }
}

// ==================== LOAD VIDEO ====================
static void DoLoadVideo() {
    std::wstring path = BrowseFile(g_hwnd, L"Seleziona File Video",
                                   L"Video Files\0*.mp4;*.mkv;*.avi\0All Files\0*.*\0");
    if (!path.empty()) {
        SetWindowTextW(g_hEditVideo, path.c_str());
        g_player.video_path = path;
    }
}

// ==================== PLAY / PAUSE / STOP ====================
static void DoPlay() {
    if (g_player.samples.empty()) return;
    // Leggi video path dall'edit
    wchar_t buf[MAX_PATH]; GetWindowTextW(g_hEditVideo, buf, MAX_PATH);
    g_player.video_path = buf;
    g_player.play();
    EnableWindow(g_hBtnPlay, FALSE);
    EnableWindow(g_hBtnPause, TRUE);
    EnableWindow(g_hBtnStop, TRUE);
    SetWindowTextW(g_hBtnPause, L"PAUSE");
    SetStatus(L"Playing...");
    // Se offset negativo VLC, riprendi video dopo |offset| ms
    if (g_player.player_mode == "VLC" && g_player.vlc.available && g_player.settings.video_offset < 0) {
        SetTimer(g_hwnd, ID_TIMER_VLC_RESUME, (UINT)(fabs(g_player.settings.video_offset) * 1000), NULL);
    }
    // Timer per aggiornare lo status
    SetTimer(g_hwnd, ID_TIMER_STATUS, 500, NULL);
}

static void DoStop() {
    g_player.stop();
    KillTimer(g_hwnd, ID_TIMER_STATUS);
    KillTimer(g_hwnd, ID_TIMER_VLC_RESUME);
    EnableWindow(g_hBtnPlay, TRUE);
    EnableWindow(g_hBtnPause, FALSE);
    EnableWindow(g_hBtnStop, FALSE);
    wchar_t buf[128]; swprintf(buf, 128, L"Fermato | Pacchetti inviati: %d", g_player.packets_sent.load());
    SetStatus(buf);
}

static void DoTogglePause() {
    g_player.toggle_pause();
    if (g_player.is_paused.load()) {
        SetWindowTextW(g_hBtnPause, L"RESUME");
        SetStatus(L"In pausa");
    } else {
        SetWindowTextW(g_hBtnPause, L"PAUSE");
        SetStatus(L"Playing...");
    }
}

static void DoUpCommand() {
    SetStatus(L"Invio comando UP...");
    std::thread t([&]() { g_player.send_up_command(); SetStatus(L"Comando UP inviato"); });
    t.detach();
}

// ==================== DRAW OWNER-DRAW BUTTON ====================
static void DrawButton(DRAWITEMSTRUCT* dis, COLORREF bg, COLORREF fg) {
    HBRUSH br = CreateSolidBrush(bg);
    FillRect(dis->hDC, &dis->rcItem, br);
    DeleteObject(br);
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, fg);
    wchar_t text[64]; GetWindowTextW(dis->hwndItem, text, 64);
    SelectObject(dis->hDC, g_fontBtn);
    DrawTextW(dis->hDC, text, -1, &dis->rcItem, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    if (dis->itemState & ODS_FOCUS) {
        RECT r = dis->rcItem;
        InflateRect(&r, -2, -2);
        DrawFocusRect(dis->hDC, &r);
    }
}

// ==================== WINDOW PROCEDURE ====================
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_fontUI = CreateFontW(-14,0,0,0,FW_NORMAL,0,0,0,0,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        g_fontTitle = CreateFontW(-22,0,0,0,FW_BOLD,0,0,0,0,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        g_fontSmall = CreateFontW(-11,0,0,0,FW_NORMAL,0,0,0,0,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        g_fontBtn = CreateFontW(-14,0,0,0,FW_BOLD,0,0,0,0,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        g_brBg = CreateSolidBrush(COL_BG);
        g_brEdit = CreateSolidBrush(COL_EDIT_BG);
        int x=25, w=600, y=15;
        // Titolo
        g_hLblTitle = CreateWindowW(L"STATIC",L"GoPro Motion Player",WS_CHILD|WS_VISIBLE|SS_CENTER,
            x,y,w,30,hwnd,(HMENU)ID_LABEL_TITLE,NULL,NULL); y+=45;
        SendMessage(g_hLblTitle,WM_SETFONT,(WPARAM)g_fontTitle,TRUE);
        // Telemetry
        CreateWindowW(L"STATIC",L"Telemetry File:",WS_CHILD|WS_VISIBLE,x,y,120,20,hwnd,NULL,NULL,NULL); y+=22;
        g_hEditTelem = CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            x,y,480,24,hwnd,(HMENU)ID_EDIT_TELEM,NULL,NULL);
        CreateWindowW(L"BUTTON",L"Browse",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            x+490,y-2,100,28,hwnd,(HMENU)ID_BTN_BROWSE_T,NULL,NULL); y+=38;
        // Video
        CreateWindowW(L"STATIC",L"Video File:",WS_CHILD|WS_VISIBLE,x,y,120,20,hwnd,NULL,NULL,NULL); y+=22;
        g_hEditVideo = CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_AUTOHSCROLL,
            x,y,480,24,hwnd,(HMENU)ID_EDIT_VIDEO,NULL,NULL);
        CreateWindowW(L"BUTTON",L"Browse",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            x+490,y-2,100,28,hwnd,(HMENU)ID_BTN_BROWSE_V,NULL,NULL); y+=42;
        // Player mode
        CreateWindowW(L"STATIC",L"Video Player:",WS_CHILD|WS_VISIBLE,x,y,120,20,hwnd,NULL,NULL,NULL); y+=25;
        g_hBtnVlc = CreateWindowW(L"BUTTON",L"VLC Player",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            x,y,100,32,hwnd,(HMENU)ID_BTN_VLC,NULL,NULL);
        g_hBtnDeovrLocal = CreateWindowW(L"BUTTON",L"DeoVR Local",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            x+110,y,100,32,hwnd,(HMENU)ID_BTN_DEOVR_LOCAL,NULL,NULL);
        g_hBtnDeovrWeb = CreateWindowW(L"BUTTON",L"DeoVR Web",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            x+220,y,100,32,hwnd,(HMENU)ID_BTN_DEOVR_WEB,NULL,NULL);
        g_hBtnUp = CreateWindowW(L"BUTTON",L"UP",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW,
            x+330,y,60,32,hwnd,(HMENU)ID_BTN_UP,NULL,NULL);
        g_hChkTop = CreateWindowW(L"BUTTON",L"Always on Top",WS_CHILD|WS_VISIBLE|BS_AUTOCHECKBOX,
            x+400,y+6,140,20,hwnd,(HMENU)ID_CHK_TOPMOST,NULL,NULL);
        CheckDlgButton(hwnd, ID_CHK_TOPMOST, BST_CHECKED);
        y+=55;
        // Controlli Play/Pause/Stop
        int bw=170, bh=42, gap=10, totalw=bw*3+gap*2, sx=x+(w-totalw)/2;
        g_hBtnPlay = CreateWindowW(L"BUTTON",L"PLAY",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_DISABLED,
            sx,y,bw,bh,hwnd,(HMENU)ID_BTN_PLAY,NULL,NULL);
        g_hBtnPause = CreateWindowW(L"BUTTON",L"PAUSE",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_DISABLED,
            sx+bw+gap,y,bw,bh,hwnd,(HMENU)ID_BTN_PAUSE,NULL,NULL);
        g_hBtnStop = CreateWindowW(L"BUTTON",L"STOP",WS_CHILD|WS_VISIBLE|BS_OWNERDRAW|WS_DISABLED,
            sx+2*(bw+gap),y,bw,bh,hwnd,(HMENU)ID_BTN_STOP,NULL,NULL);
        y+=60;
        
        // Live Offset
        CreateWindowW(L"STATIC",L"Live Sync Offset (s):",WS_CHILD|WS_VISIBLE|SS_RIGHT,
            x+80,y,150,20,hwnd,NULL,NULL,NULL);
        g_hEditOffset = CreateWindowExW(0,L"EDIT",L"0.00",WS_CHILD|WS_VISIBLE|WS_BORDER|ES_CENTER|ES_AUTOHSCROLL,
            x+240,y,60,24,hwnd,(HMENU)ID_EDIT_OFFSET,NULL,NULL);
        CreateWindowW(L"BUTTON",L"Apply",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
            x+310,y-2,80,28,hwnd,(HMENU)ID_BTN_APPLY_OFFSET,NULL,NULL);
        y+=40;
        
        // Status
        g_hLblStatus = CreateWindowW(L"STATIC",L"Ready - Load a telemetry file",
            WS_CHILD|WS_VISIBLE|SS_CENTER,x,y,w,25,hwnd,(HMENU)ID_LABEL_STATUS,NULL,NULL); y+=30;
            
        // Instructions
        g_hLblInstr = CreateWindowW(L"STATIC",L"Select a video player Mode.",
            WS_CHILD|WS_VISIBLE|SS_CENTER,x,y,w,40,hwnd,(HMENU)ID_LABEL_INSTR,NULL,NULL); y+=45;
            
        // Footer
        g_hLblFooter = CreateWindowW(L"STATIC",L"UDP: 127.0.0.1:5300 | Output: 60Hz",
            WS_CHILD|WS_VISIBLE|SS_CENTER,x,y,w,18,hwnd,(HMENU)ID_LABEL_FOOTER,NULL,NULL);
        // Set font su tutti i child
        EnumChildWindows(hwnd, [](HWND h, LPARAM lp) -> BOOL {
            SendMessage(h, WM_SETFONT, (WPARAM)lp, TRUE); return TRUE;
        }, (LPARAM)g_fontUI);
        SendMessage(g_hLblTitle, WM_SETFONT, (WPARAM)g_fontTitle, TRUE);
        SendMessage(g_hLblFooter, WM_SETFONT, (WPARAM)g_fontSmall, TRUE);
        UpdateInstructions();
        // Callback status dal player
        g_player.on_status = [](const char* txt) {
            // Post message al thread UI (convertiamo char* ASCII/UTF-8 in wchar_t se necessario, 
            // ma qui txt e' solitamente tecnico/inglese. Per sicurezza convertiamo.)
            int len = MultiByteToWideChar(CP_UTF8, 0, txt, -1, NULL, 0);
            wchar_t* copy = (wchar_t*)malloc(len * sizeof(wchar_t));
            MultiByteToWideChar(CP_UTF8, 0, txt, -1, copy, len);
            PostMessage(g_hwnd, WM_APP + 1, 0, (LPARAM)copy);
        };
        // Inizializza VLC
        if (g_player.vlc.init()) {
            wchar_t msg[512];
            swprintf(msg, 512, L"VLC trovato: %s", g_player.vlc.found_path.c_str());
            SetStatus(msg);
        } else {
            SetStatus(L"VLC non trovato - installa VLC 64-bit per riprodurre video");
            MessageBoxW(hwnd,
                L"VLC Player non trovato!\n\n"
                L"Il programma funziona comunque per la telemetria UDP,\n"
                L"ma per riprodurre video devi installare VLC 64-bit.\n\n"
                L"Scarica da: https://www.videolan.org/vlc/\n"
                L"Assicurati di installare la versione a 64 bit.",
                L"VLC non trovato", MB_OK | MB_ICONWARNING);
        }
        // Always on top
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        return 0;
    }
    case WM_APP + 1: {
        // Aggiorna status dal thread di playback
        wchar_t* txt = (wchar_t*)lParam;
        if (txt) { SetWindowTextW(g_hLblStatus, txt); free(txt); }
        // Controlla se il playback e' finito
        if (!g_player.is_playing.load()) {
            KillTimer(hwnd, ID_TIMER_STATUS);
            EnableWindow(g_hBtnPlay, TRUE);
            EnableWindow(g_hBtnPause, FALSE);
            EnableWindow(g_hBtnStop, FALSE);
        }
        return 0;
    }
    case WM_TIMER:
        if (wParam == ID_TIMER_STATUS) {
            if (!g_player.is_playing.load()) {
                KillTimer(hwnd, ID_TIMER_STATUS);
                wchar_t buf[128]; swprintf(buf, 128, L"Terminato | Pacchetti: %d", g_player.packets_sent.load());
                SetStatus(buf);
                EnableWindow(g_hBtnPlay, TRUE);
                EnableWindow(g_hBtnPause, FALSE);
                EnableWindow(g_hBtnStop, FALSE);
            }
        } else if (wParam == ID_TIMER_VLC_RESUME) {
            KillTimer(hwnd, ID_TIMER_VLC_RESUME);
            g_player.launch_vlc_delayed();
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_BROWSE_T: DoLoadTelemetry(); break;
        case ID_BTN_BROWSE_V: DoLoadVideo(); break;
        case ID_BTN_VLC:
            g_player.player_mode = "VLC";
            UpdatePlayerButtons();
            UpdateInstructions();
            break;
        case ID_BTN_DEOVR_LOCAL:
            g_player.player_mode = "DEOVR_LOCAL";
            UpdatePlayerButtons();
            UpdateInstructions();
            break;
        case ID_BTN_DEOVR_WEB:
            g_player.player_mode = "DEOVR_WEB";
            UpdatePlayerButtons();
            UpdateInstructions();
            break;
        case ID_BTN_UP: DoUpCommand(); break;
        case ID_BTN_PLAY: DoPlay(); break;
        case ID_BTN_PAUSE: DoTogglePause(); break;
        case ID_BTN_STOP: DoStop(); break;
        case ID_BTN_APPLY_OFFSET: {
            wchar_t buf[64]; GetWindowTextW(g_hEditOffset, buf, 64);
            g_player.live_offset_sec = _wtof(buf);
            break;
        }
        case ID_CHK_TOPMOST:
            SetWindowPos(hwnd, IsDlgButtonChecked(hwnd, ID_CHK_TOPMOST) ? HWND_TOPMOST : HWND_NOTOPMOST,
                         0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            break;
        }
        return 0;
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        int id = (int)dis->CtlID;
        bool disabled = (dis->itemState & ODS_DISABLED) != 0;
        if (id == ID_BTN_VLC) {
            bool active = g_player.player_mode == "VLC";
            DrawButton(dis, active ? COL_GREEN : COL_GRAY, active ? RGB(0,0,0) : COL_FG);
        } else if (id == ID_BTN_DEOVR_LOCAL) {
            bool active = g_player.player_mode == "DEOVR_LOCAL";
            DrawButton(dis, active ? COL_GREEN : COL_GRAY, active ? RGB(0,0,0) : COL_FG);
        } else if (id == ID_BTN_DEOVR_WEB) {
            bool active = g_player.player_mode == "DEOVR_WEB";
            DrawButton(dis, active ? COL_GREEN : COL_GRAY, active ? RGB(0,0,0) : COL_FG);
        } else if (id == ID_BTN_UP) {
            DrawButton(dis, COL_ORANGE, RGB(0,0,0));
        } else if (id == ID_BTN_PLAY) {
            DrawButton(dis, disabled ? RGB(50,50,50) : RGB(0,180,80),
                       disabled ? RGB(100,100,100) : COL_FG);
        } else if (id == ID_BTN_PAUSE) {
            DrawButton(dis, disabled ? RGB(50,50,50) : RGB(180,180,0),
                       disabled ? RGB(100,100,100) : RGB(0,0,0));
        } else if (id == ID_BTN_STOP) {
            DrawButton(dis, disabled ? RGB(50,50,50) : RGB(200,50,50),
                       disabled ? RGB(100,100,100) : COL_FG);
        }
        return TRUE;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, COL_BG);
        HWND ctrl = (HWND)lParam;
        if (ctrl == g_hLblTitle) SetTextColor(hdc, COL_GREEN);
        else if (ctrl == g_hLblFooter) SetTextColor(hdc, RGB(136,136,136));
        else SetTextColor(hdc, COL_FG);
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)g_brBg;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkColor(hdc, COL_EDIT_BG);
        SetTextColor(hdc, COL_FG);
        return (LRESULT)g_brEdit;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT r; GetClientRect(hwnd, &r);
        FillRect(hdc, &r, g_brBg);
        return 1;
    }
    case WM_DESTROY:
        g_player.stop();
        g_player.vlc.cleanup();
        g_player.deovr.disconnect();
        DeleteObject(g_fontUI); DeleteObject(g_fontTitle);
        DeleteObject(g_fontSmall); DeleteObject(g_fontBtn);
        DeleteObject(g_brBg); DeleteObject(g_brEdit);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ==================== WINMAIN ====================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"MotionPlayerGUI";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    int winW = 670, winH = 480;
    int scrW = GetSystemMetrics(SM_CXSCREEN), scrH = GetSystemMetrics(SM_CYSCREEN);
    g_hwnd = CreateWindowExW(0, L"MotionPlayerGUI",
        L"GoPro Motion Player LITE - C++ Edition",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (scrW - winW) / 2, (scrH - winH) / 2, winW, winH,
        NULL, NULL, hInst, NULL);

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // Auto-load se passato come argomento
    LPWSTR lpCmdLineW = GetCommandLineW();
    int argc;
    LPWSTR* argv = CommandLineToArgvW(lpCmdLineW, &argc);
    if (argc > 1) {
        std::wstring path = argv[1];
        SetWindowTextW(g_hEditTelem, path.c_str());
        if (g_player.load_file(path)) {
            wchar_t buf[256];
            swprintf(buf, 256, L"Caricato: %d campioni | %.1fs | %dHz",
                     (int)g_player.samples.size(), g_player.get_duration(), g_player.detected_freq);
            SetStatus(buf);
            UpdateFooter();
            EnableWindow(g_hBtnPlay, TRUE);
        }
    }
    LocalFree(argv);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    WSACleanup();
    return (int)msg.wParam;
}
