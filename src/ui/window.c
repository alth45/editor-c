#include "common.h"

/* Subclass edit: update Ln/Col saat caret pindah (klik/panah), bukan cuma saat ketik */
static WNDPROC gOldEditProc = NULL;

static LRESULT CALLBACK EditSubProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_RBUTTONUP) {
        /* Klik kanan di edit: popup Lookup + Select All */
        HMENU pop = CreatePopupMenu();
        AppendMenuW(pop, MF_STRING, IDM_SELECTALL, L"Select All\tCtrl+A");
        AppendMenuW(pop, MF_STRING, IDM_LOOKUP, L"Lookup di Jisho\tCtrl+J");
        POINT pt;
        pt.x = LOWORD(lp); pt.y = HIWORD(lp);
        ClientToScreen(hwnd, &pt);
        TrackPopupMenu(pop, TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                       pt.x, pt.y, 0, hMain, NULL);
        DestroyMenu(pop);
        /* Teruskan juga agar menu bawaan edit tetap bisa muncul bila perlu */
    }
    LRESULT r = CallWindowProcW(gOldEditProc, hwnd, msg, wp, lp);
    switch (msg) {
    case WM_KEYUP:
    case WM_LBUTTONUP:
    case WM_SETFOCUS:
        UpdateStatusBar();
        break;
    }
    return r;
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

        /* Tombol show/hide sidebar, sejajar toolbar */
        hToggleBtn = CreateWindowW(L"BUTTON", L"Show Web",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            350, 8, 110, 28, hwnd, (HMENU)ID_TOGGLE_SIDEBAR,
            GetModuleHandleW(NULL), NULL);

        /* Tombol lookup Jisho, di kanan tombol sidebar */
        hLookupBtn = CreateWindowW(L"BUTTON", L"Jisho",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            468, 8, 80, 28, hwnd, (HMENU)ID_LOOKUP_BTN,
            GetModuleHandleW(NULL), NULL);

        /* Font untuk UI (biar tetap halus) */
        HDC hdc = GetDC(hwnd);
        int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
        ReleaseDC(hwnd, hdc);
        hUICtrlFont = CreateFontW(-MulDiv(10, dpi, 72), 0,0,0, FW_NORMAL,
            0,0,0, DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Yu Gothic UI");
        SendMessageW(hFontCombo, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
        SendMessageW(hSizeCombo, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
        SendMessageW(hToggleBtn, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
        SendMessageW(hLookupBtn, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);

        /* Edit control */
        hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", NULL,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL |
            ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
            0, TOOLBAR_HEIGHT, 100, 100, hwnd, (HMENU)ID_EDIT, NULL, NULL);

        /* Status bar bawah */
        CreateStatusBar(hwnd);
        LayoutMainWindow();

        /* Subclass edit untuk tracking caret (Ln/Col) */
        gOldEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC,
                                                  (LONG_PTR)EditSubProc);

        ApplyFont(hwnd);
        SetFocus(hEdit);
        return 0;
    }

    case WM_SIZE: {
        LayoutMainWindow();
        return 0;
    }

    case WM_COMMAND: {
        WORD id   = LOWORD(wp);
        WORD code = HIWORD(wp);

        if (id == ID_TOGGLE_SIDEBAR) {
            ToggleSidebar();
            return 0;
        }
        if (id == ID_LOOKUP_BTN || id == IDM_LOOKUP) {
            LookupSelection();
            return 0;
        }
        if (id == ID_GO_BTN) {
            NavigateSidebarFromEdit();
            return 0;
        }
        /* Pilih URL dari dropdown riwayat -> langsung buka */
        if (id == ID_HISTORY_COMBO && code == CBN_SELCHANGE) {
            NavigateHistorySelection();
            return 0;
        }
        /* Tombol Hapus: bersihkan riwayat URL */
        if (id == ID_HISTORY_CLEAR) {
            ClearUrlHistory();
            return 0;
        }
        if (id == ID_URL_EDIT && code == EN_CHANGE) {
            return 0;
        }
        /* Enter di URL bar = Go */
        if (id == ID_URL_EDIT && code == EN_UPDATE) {
            return 0;
        }
        if (id == ID_FONT_COMBO && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(hFontCombo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) {
                currentFontIndex = sel;
                ApplyFont(hwnd);
                if (!modified) { modified = TRUE; SetTitle(); }
                else UpdateStatusBar();
            }
            SetFocus(hEdit);
            return 0;
        }
        if (id == ID_SIZE_COMBO && code == CBN_SELCHANGE) {
            int sel = (int)SendMessageW(hSizeCombo, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) {
                currentSizeIndex = sel;
                ApplyFont(hwnd);
                if (!modified) { modified = TRUE; SetTitle(); }
                else UpdateStatusBar();
            }
            SetFocus(hEdit);
            return 0;
        }
        if (id == ID_EDIT && code == EN_CHANGE) {
            if (!suppressChange) {
                if (!modified) { modified = TRUE; SetTitle(); }
                else UpdateStatusBar();
            }
            return 0;
        }
        if (id == ID_EDIT && code == EN_UPDATE) {
            UpdateStatusBar();
            return 0;
        }

        switch (id) {
        case IDM_NEW:
            if (MaybeSave()) {
                suppressChange = TRUE;
                SetWindowTextW(hEdit, L"");
                suppressChange = FALSE;
                currentFile[0] = 0;
                lstrcpynW(currentEncoding, L"UTF-8", 32);
                modified = FALSE;
                SetTitle();
                SetFocus(hEdit);
            }
            return 0;
        case IDM_OPEN:   DoOpenFile();   return 0;
        case IDM_SAVE:   DoSaveFile(FALSE); return 0;
        case IDM_SAVEAS: DoSaveFile(TRUE);  return 0;
        case IDM_LOOKUP: LookupSelection(); return 0;
        case IDM_SELECTALL:
            if (hEdit) {
                SendMessageW(hEdit, EM_SETSEL, 0, -1);
                SetFocus(hEdit);
                UpdateStatusBar();
            }
            return 0;
        case IDM_EXIT:   PostMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
        }
        return 0;
    }

    case WM_CLOSE:
        if (MaybeSave()) DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        DestroySidebar();
        if (hCurrentFont) DeleteObject(hCurrentFont);
        if (hUICtrlFont)  DeleteObject(hUICtrlFont);
        if (hAccel)       DestroyAcceleratorTable(hAccel);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
