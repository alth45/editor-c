#include "common.h"

/* ==== Title ==== */
void SetTitle(void) {
    wchar_t title[MAX_PATH + 64];
    const wchar_t *name = currentFile[0] ? currentFile : L"Untitled";
    wsprintfW(title, L"%s%s - Japanese Text Editor", modified ? L"* " : L"", name);
    SetWindowTextW(hMain, title);
    UpdateStatusBar();
}

/* ==== Status bar: Ln/Col | Chars | Encoding | File ==== */
void CreateStatusBar(HWND parent) {
    hStatus = CreateWindowExW(0, STATUSCLASSNAMEW, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, parent, (HMENU)ID_STATUSBAR,
        GetModuleHandleW(NULL), NULL);
    if (!hStatus) return;
    /* 4 panel: Ln/Col | Chars | Encoding | File (sisa) */
    int parts[4] = { 110, 220, 330, -1 };
    SendMessageW(hStatus, SB_SETPARTS, 4, (LPARAM)parts);
    if (hUICtrlFont)
        SendMessageW(hStatus, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
    UpdateStatusBar();
}

int StatusBarHeight(void) {
    if (!hStatus) return 0;
    RECT rc;
    GetWindowRect(hStatus, &rc);
    int h = rc.bottom - rc.top;
    return (h > 0 && h < 60) ? h : 26;
}

void UpdateStatusBar(void) {
    if (!hStatus || !hEdit) return;

    /* --- Ln/Col dari posisi caret --- */
    DWORD sel = (DWORD)SendMessageW(hEdit, EM_GETSEL, 0, 0);
    int caret = LOWORD(sel);
    int line = (int)SendMessageW(hEdit, EM_LINEFROMCHAR, caret, 0);
    int lineStart = (int)SendMessageW(hEdit, EM_LINEINDEX, line, 0);
    wchar_t sPos[64];
    wsprintfW(sPos, L"Ln %d, Col %d", line + 1, caret - lineStart + 1);

    /* --- Jumlah karakter --- */
    int len = GetWindowTextLengthW(hEdit);
    wchar_t sChars[64];
    wsprintfW(sChars, L"%d chars", len);

    /* --- Encoding --- */
    wchar_t sEnc[64];
    wsprintfW(sEnc, L"%s", currentEncoding);

    /* --- Nama file (+ * kalau modified) --- */
    wchar_t sFile[MAX_PATH + 8];
    const wchar_t *name = currentFile[0] ? currentFile : L"Untitled";
    wsprintfW(sFile, L"%s%s", modified ? L"* " : L"", name);

    SendMessageW(hStatus, SB_SETTEXTW, 0, (LPARAM)sPos);
    SendMessageW(hStatus, SB_SETTEXTW, 1, (LPARAM)sChars);
    SendMessageW(hStatus, SB_SETTEXTW, 2, (LPARAM)sEnc);
    SendMessageW(hStatus, SB_SETTEXTW, 3, (LPARAM)sFile);
}
