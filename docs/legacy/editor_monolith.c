#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

/* ==== IDs ==== */
#define ID_EDIT          101
#define ID_FONT_COMBO    102
#define ID_SIZE_COMBO    103

#define IDM_NEW          201
#define IDM_OPEN         202
#define IDM_SAVE         203
#define IDM_SAVEAS       204
#define IDM_EXIT         205

#define TOOLBAR_HEIGHT   44

/* ==== Globals ==== */
HWND hMain = NULL, hEdit = NULL, hFontCombo = NULL, hSizeCombo = NULL;
HFONT hCurrentFont = NULL, hUICtrlFont = NULL;
HACCEL hAccel = NULL;
wchar_t currentFile[MAX_PATH] = L"";
BOOL modified = FALSE;
BOOL suppressChange = FALSE;

/* Daftar font halus yang bagus untuk Bahasa Jepang */
static const wchar_t* japaneseFonts[] = {
    L"Yu Gothic UI",          /* default Win10/11 */
    L"Yu Gothic",
    L"Meiryo UI",
    L"Meiryo",
    L"MS UI Gothic",
    L"MS PGothic",
    L"MS Gothic",
    L"Yu Mincho",
    L"MS PMincho",
    L"MS Mincho",
    L"BIZ UD Gothic",         /* UD = universal design, sangat halus */
    L"BIZ UD Mincho",
    L"UD Digi Kyokasho NK-R", /* untuk pembelajaran */
    L"UD Digi Kyokasho N-R",
    L"Segoe UI",
    L"Arial",
    L"Consolas"
};
static const int numFonts = (int)(sizeof(japaneseFonts) / sizeof(japaneseFonts[0]));

static int sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36, 48, 72};
static const int numSizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

static int currentFontIndex = 0;
static int currentSizeIndex = 6; /* = 16 pt */

/* ==== Forward ==== */
BOOL MaybeSave(void);
BOOL DoSaveFile(BOOL saveAs);
BOOL DoOpenFile(void);

/* ==== Title ==== */
static void SetTitle(void) {
    wchar_t title[MAX_PATH + 64];
    const wchar_t* name = currentFile[0] ? currentFile : L"Untitled";
    wsprintfW(title, L"%s%s - Japanese Text Editor", modified ? L"* " : L"", name);
    SetWindowTextW(hMain, title);
}

/* ==== Terapkan font ke edit control ==== */
static void ApplyFont(HWND hwnd) {
    if (hCurrentFont) { DeleteObject(hCurrentFont); hCurrentFont = NULL; }

    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hwnd, hdc);

    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight        = -MulDiv(sizes[currentSizeIndex], dpi, 72); /* pt -> px */
    lf.lfWeight        = FW_NORMAL;
    lf.lfCharSet       = DEFAULT_CHARSET;
    lf.lfQuality       = CLEARTYPE_QUALITY;   /* <<< INI KUNCI ANTI "8-BIT" */
    lf.lfOutPrecision  = OUT_TT_PRECIS;       /* pakai TrueType/OpenType */
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfPitchAndFamily= DEFAULT_PITCH | FF_DONTCARE;
    lstrcpynW(lf.lfFaceName, japaneseFonts[currentFontIndex], LF_FACESIZE);

    hCurrentFont = CreateFontIndirectW(&lf);
    if (hEdit) SendMessageW(hEdit, WM_SETFONT, (WPARAM)hCurrentFont, TRUE);
}

/* ==== Save file (UTF-8 dengan BOM) ==== */
static BOOL SaveFileTo(const wchar_t* path) {
    int len = GetWindowTextLengthW(hEdit);
    wchar_t* wbuf = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    if (!wbuf) return FALSE;
    GetWindowTextW(hEdit, wbuf, len + 1);

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, len, NULL, 0, NULL, NULL);
    char* u8 = NULL;
    if (u8len > 0) {
        u8 = (char*)malloc(u8len);
        if (!u8) { free(wbuf); return FALSE; }
        WideCharToMultiByte(CP_UTF8, 0, wbuf, len, u8, u8len, NULL, NULL);
    }
    free(wbuf);

    HANDLE hf = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) { free(u8); return FALSE; }

    DWORD written;
    BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
    WriteFile(hf, bom, 3, &written, NULL);
    if (u8len > 0) WriteFile(hf, u8, u8len, &written, NULL);
    CloseHandle(hf);
    free(u8);
    return TRUE;
}

/* ==== Load file (deteksi BOM) ==== */
static BOOL LoadFileFrom(const wchar_t* path) {
    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return FALSE;

    DWORD size = GetFileSize(hf, NULL);
    if (size == INVALID_FILE_SIZE) { CloseHandle(hf); return FALSE; }

    BYTE* data = (BYTE*)malloc(size + 4);
    if (!data) { CloseHandle(hf); return FALSE; }
    DWORD read = 0;
    ReadFile(hf, data, size, &read, NULL);
    CloseHandle(hf);
    data[size] = 0; data[size+1] = 0; data[size+2] = 0; data[size+3] = 0;

    wchar_t* wbuf = NULL;
    int wlen = 0;

    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        /* UTF-16 LE */
        wlen = (int)((size - 2) / 2);
        wbuf = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf) { memcpy(wbuf, data + 2, wlen * 2); wbuf[wlen] = 0; }
    } else if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        /* UTF-8 dengan BOM */
        int u8len = (int)(size - 3);
        wlen = MultiByteToWideChar(CP_UTF8, 0, (char*)data + 3, u8len, NULL, 0);
        wbuf = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf) {
            MultiByteToWideChar(CP_UTF8, 0, (char*)data + 3, u8len, wbuf, wlen);
            wbuf[wlen] = 0;
        }
    } else {
        /* Coba UTF-8 tanpa BOM dulu, jika gagal -> ANSI/Shift-JIS */
        wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   (char*)data, size, NULL, 0);
        UINT cp = (wlen > 0) ? CP_UTF8 : CP_ACP;
        DWORD flags = (wlen > 0) ? MB_ERR_INVALID_CHARS : 0;
        if (wlen <= 0) wlen = MultiByteToWideChar(cp, 0, (char*)data, size, NULL, 0);
        wbuf = (wchar_t*)malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf) {
            MultiByteToWideChar(cp, flags, (char*)data, size, wbuf, wlen);
            wbuf[wlen] = 0;
        }
    }
    free(data);
    if (!wbuf) return FALSE;

    suppressChange = TRUE;
    SetWindowTextW(hEdit, wbuf);
    suppressChange = FALSE;
    modified = FALSE;
    free(wbuf);
    return TRUE;
}

/* ==== Dialog: Open ==== */
BOOL DoOpenFile(void) {
    if (!MaybeSave()) return FALSE;
    wchar_t path[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hMain;
    ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"txt";

    if (GetOpenFileNameW(&ofn)) {
        if (LoadFileFrom(path)) {
            lstrcpynW(currentFile, path, MAX_PATH);
            SetTitle();
            SetFocus(hEdit);
        } else {
            MessageBoxW(hMain, L"Gagal membuka file.", L"Error", MB_ICONERROR);
        }
    }
    return TRUE;
}

/* ==== Dialog: Save / Save As ==== */
BOOL DoSaveFile(BOOL saveAs) {
    wchar_t path[MAX_PATH];
    if (saveAs || currentFile[0] == 0) {
        lstrcpynW(path, currentFile, MAX_PATH);
        OPENFILENAMEW ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = hMain;
        ofn.lpstrFilter = L"Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile   = path;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        ofn.lpstrDefExt = L"txt";
        if (!GetSaveFileNameW(&ofn)) return FALSE;
        lstrcpynW(currentFile, path, MAX_PATH);
    }
    if (SaveFileTo(currentFile)) {
        modified = FALSE;
        SetTitle();
        SetFocus(hEdit);
        return TRUE;
    }
    MessageBoxW(hMain, L"Gagal menyimpan file.", L"Error", MB_ICONERROR);
    return FALSE;
}

/* ==== Konfirmasi jika ada perubahan belum disimpan ==== */
BOOL MaybeSave(void) {
    if (!modified) return TRUE;
    int r = MessageBoxW(hMain, L"Simpan perubahan?", L"Japanese Text Editor",
                        MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return FALSE;
    if (r == IDNO)     return TRUE;
    return DoSaveFile(FALSE);
}

/* ==== Window Procedure ==== */
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {

    case WM_CREATE: {
        /* Combobox font */
        hFontCombo = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            10, 8, 240, 300, hwnd, (HMENU)ID_FONT_COMBO, NULL, NULL);
        for (int i = 0; i < numFonts; i++)
            SendMessageW(hFontCombo, CB_ADDSTRING, 0, (LPARAM)japaneseFonts[i]);
        SendMessageW(hFontCombo, CB_SETCURSEL, currentFontIndex, 0);

        /* Combobox size */
        hSizeCombo = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            260, 8, 80, 300, hwnd, (HMENU)ID_SIZE_COMBO, NULL, NULL);
        wchar_t sz[16];
        for (int i = 0; i < numSizes; i++) {
            wsprintfW(sz, L"%d", sizes[i]);
            SendMessageW(hSizeCombo, CB_ADDSTRING, 0, (LPARAM)sz);
        }
        SendMessageW(hSizeCombo, CB_SETCURSEL, currentSizeIndex, 0);

        /* Font untuk UI (biar tetap halus) */
        HDC hdc = GetDC(hwnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hwnd, hdc);
        hUICtrlFont = CreateFontW(-MulDiv(10, dpi, 72), 0,0,0, FW_NORMAL,
            0,0,0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Yu Gothic UI");
        SendMessageW(hFontCombo, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
        SendMessageW(hSizeCombo, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);

        /* Edit control */
        hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            0, TOOLBAR_HEIGHT, 100, 100, hwnd, (HMENU)ID_EDIT, NULL, NULL);

        ApplyFont(hwnd);
        SetFocus(hEdit);
        return 0;
    }

    case WM_SIZE: {
        int w = LOWORD(lp);
        int h = HIWORD(lp);
        MoveWindow(hEdit, 0, TOOLBAR_HEIGHT, w, h - TOOLBAR_HEIGHT, TRUE);
        return 0;
    }

    case WM_COMMAND: {
        WORD id   = LOWORD(wp);
        WORD code = HIWORD(wp);

        if (id == ID_FONT_COMBO && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(hFontCombo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) { currentFontIndex = sel; ApplyFont(hwnd); }
            SetFocus(hEdit);
            return 0;
        }
        if (id == ID_SIZE_COMBO && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(hSizeCombo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) { currentSizeIndex = sel; ApplyFont(hwnd); }
            SetFocus(hEdit);
            return 0;
        }
        if (id == ID_EDIT && code == EN_CHANGE) {
            if (!suppressChange) {
                if (!modified) { modified = TRUE; SetTitle(); }
            }
            return 0;
        }

        switch (id) {
        case IDM_NEW:
            if (MaybeSave()) {
                suppressChange = TRUE;
                SetWindowTextW(hEdit, L"");
                suppressChange = FALSE;
                currentFile[0] = 0;
                modified = FALSE;
                SetTitle();
                SetFocus(hEdit);
            }
            return 0;
        case IDM_OPEN:   DoOpenFile();   return 0;
        case IDM_SAVE:   DoSaveFile(FALSE); return 0;
        case IDM_SAVEAS: DoSaveFile(TRUE);  return 0;
        case IDM_EXIT:   PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
        }
        return 0;
    }

    case WM_CLOSE:
        if (MaybeSave()) DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (hCurrentFont) DeleteObject(hCurrentFont);
        if (hUICtrlFont)  DeleteObject(hUICtrlFont);
        if (hAccel)       DestroyAcceleratorTable(hAccel);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ==== Entry point ==== */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmdLine, int nCmdShow) {
    (void)hPrev; (void)cmdLine;

    /* DPI awareness biar font tetap tajam di monitor HiDPI */
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    /* Menu */
    HMENU hMenuBar = CreateMenu();
    HMENU hFile    = CreatePopupMenu();
    AppendMenuW(hFile, MF_STRING,    IDM_NEW,    L"&New\tCtrl+N");
    AppendMenuW(hFile, MF_STRING,    IDM_OPEN,   L"&Open...\tCtrl+O");
    AppendMenuW(hFile, MF_STRING,    IDM_SAVE,   L"&Save\tCtrl+S");
    AppendMenuW(hFile, MF_STRING,    IDM_SAVEAS, L"Save &As...");
    AppendMenuW(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFile, MF_STRING,    IDM_EXIT,   L"E&xit");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hFile, L"&File");

    /* Accelerator */
    ACCEL accels[] = {
        { FVIRTKEY | FCONTROL, 'N', IDM_NEW  },
        { FVIRTKEY | FCONTROL, 'O', IDM_OPEN },
        { FVIRTKEY | FCONTROL, 'S', IDM_SAVE },
    };
    hAccel = CreateAcceleratorTableW(accels, 3);

    /* Register class */
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"JpTextEditorWnd";
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    RegisterClassExW(&wc);

    hMain = CreateWindowExW(0, L"JpTextEditorWnd",
        L"Japanese Text Editor - Untitled",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 900, 650,
        NULL, hMenuBar, hInst, NULL);

    ShowWindow(hMain, nCmdShow);
    UpdateWindow(hMain);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        if (!TranslateAcceleratorW(hMain, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return (int)msg.wParam;
}