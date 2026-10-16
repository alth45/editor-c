#include "common.h"

/* ==== Entry point ==== */
int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR cmdLine, int nCmdShow) {
    (void)hPrev; (void)cmdLine;

    /* COM untuk embed browser (AtlAxWin / IWebBrowser2) */
    OleInitialize(NULL);

    /* DPI awareness biar font tetap tajam di monitor HiDPI */
    SetProcessDPIAware();

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES | ICC_BAR_CLASSES };
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

    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, IDM_SELECTALL, L"Select &All\tCtrl+A");
    AppendMenuW(hEditMenu, MF_STRING, IDM_FIND,      L"&Find...\tCtrl+F");
    AppendMenuW(hEditMenu, MF_STRING, IDM_REPLACE,   L"&Replace...\tCtrl+H");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hEditMenu, MF_STRING, IDM_LOOKUP,    L"Lookup di &Jisho\tCtrl+J");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");

    /* Accelerator */
    ACCEL accels[] = {
        { FVIRTKEY | FCONTROL, 'N', IDM_NEW  },
        { FVIRTKEY | FCONTROL, 'O', IDM_OPEN },
        { FVIRTKEY | FCONTROL, 'S', IDM_SAVE },
        { FVIRTKEY | FCONTROL, 'J', IDM_LOOKUP },
        { FVIRTKEY | FCONTROL, 'A', IDM_SELECTALL },
        { FVIRTKEY | FCONTROL, 'F', IDM_FIND },
        { FVIRTKEY | FCONTROL, 'H', IDM_REPLACE },
    };
    hAccel = CreateAcceleratorTableW(accels,
                (int)(sizeof(accels) / sizeof(accels[0])));

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
        /* Dialog Find/Replace modeless owned-POPUP:
         *  - IsDialogMessageW hanya untuk pesan milik dialog (Tab/Enter/Esc).
         *  - IsChild() FALSE untuk owned popup, jadi cek juga: msg.hwnd
         *    adalah dialog itu sendiri ATAU child dari dialog. Pesan lain
         *    (WM_KEYDOWN Ctrl+F/H/J, menu, dsb) TETAP boleh jalan sebagai
         *    accelerator: kalau IsDialogMessageW tidak memakannya,
         *    teruskan ke TranslateAcceleratorW. Tanpa fallback ini Ctrl+F/H
         *    tampak "mati" saat fokus ada di popup (itulah bug yang dilapor:
         *    Ctrl+F tidak memunculkan apa-apa padahal dialog belum ada /
         *    fokusnya pindah). */
        BOOL dlgMsg = FALSE;
        if (FindDialogOpen()) {
            HWND hDlg = FindDlgHandle();
            if (hDlg && (msg.hwnd == hDlg || IsChild(hDlg, msg.hwnd))) {
                dlgMsg = TRUE;
                if (IsDialogMessageW(hDlg, &msg))
                    continue;
                /* tidak dimakan dialog -> boleh jadi accelerator/menu */
                if (TranslateAcceleratorW(hMain, hAccel, &msg))
                    continue;
            }
        }
        if (!dlgMsg) {
            /* Ctrl+F / Ctrl+H / Ctrl+J / ... selalu bisa, dialog terbuka atau tidak */
            if (TranslateAcceleratorW(hMain, hAccel, &msg))
                continue;
        }
        /* Enter di URL bar = Go (edit single-line di dalam STATIC parent) */
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN &&
            hUrlEdit && (msg.hwnd == hUrlEdit)) {
            NavigateSidebarFromEdit();
            continue;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    OleUninitialize();
    return (int)msg.wParam;
}
