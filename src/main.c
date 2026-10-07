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

    /* Accelerator */
    ACCEL accels[] = {
        { FVIRTKEY | FCONTROL, 'N', IDM_NEW  },
        { FVIRTKEY | FCONTROL, 'O', IDM_OPEN },
        { FVIRTKEY | FCONTROL, 'S', IDM_SAVE },
        { FVIRTKEY | FCONTROL, 'J', IDM_LOOKUP },
    };
    hAccel = CreateAcceleratorTableW(accels, 4);

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
        /* TranslateAccelerator dulu supaya Ctrl+J/N/O/S tidak masuk ke edit */
        if (TranslateAcceleratorW(hMain, hAccel, &msg))
            continue;
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
