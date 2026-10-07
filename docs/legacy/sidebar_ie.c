#define COBJMACROS 1
#include "common.h"

#include <exdisp.h>
#include <mshtml.h>
#include <oleauto.h>

/* Link tambahan: OLE untuk embed Internet Explorer (Trident) */
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")

/* ==== Helper: isi URL bar + navigasi ==== */
static void NormalizeUrl(const wchar_t *in, wchar_t *out, int outChars) {
    if (!in || !in[0]) { out[0] = 0; return; }
    if (wcsstr(in, L"://"))
        lstrcpynW(out, in, outChars);
    else
        wsprintfW(out, L"https://%s", in);
}

/* ==== Embed IE via AtlAxWin (tanpa WebView2 SDK) ==== */
typedef HRESULT (WINAPI *AtlAxWinInit_t)(void);
typedef HRESULT (WINAPI *AtlAxGetControl_t)(HWND, IUnknown **);
typedef HRESULT (WINAPI *AtlAxGetHost_t)(HWND, IUnknown **);

static HMODULE hAtlDll = NULL;
static BOOL atlInitOK = FALSE;

static BOOL EnsureAtlAxWin(HWND parent) {
    static BOOL classRegistered = FALSE;
    if (classRegistered && atlInitOK) return TRUE;

    if (!hAtlDll) hAtlDll = LoadLibraryW(L"atl.dll");
    if (!hAtlDll) {
        MessageBoxW(parent, L"atl.dll tidak ditemukan.",
                    L"Sidebar Web", MB_ICONWARNING);
        return FALSE;
    }
    AtlAxWinInit_t pInit =
        (AtlAxWinInit_t)GetProcAddress(hAtlDll, "AtlAxWinInit");
    if (!pInit) return FALSE;
    if (FAILED(pInit())) return FALSE;
    atlInitOK = TRUE;
    classRegistered = TRUE;
    return TRUE;
}

static BOOL CreateBrowserControl(HWND parent) {
    if (hBrowser) return TRUE;
    if (!EnsureAtlAxWin(parent)) return FALSE;

    /* Window text HARUS ProgID/CLSID + NULL kedua (double-null).
     * L"Shell.Explorer.2" = Internet Explorer (Trident). */
    static const wchar_t prog[] = L"Shell.Explorer.2";
    wchar_t name[(sizeof(prog) / sizeof(wchar_t)) + 1];
    memcpy(name, prog, sizeof(prog));
    name[sizeof(prog) / sizeof(wchar_t)] = 0;

    static const wchar_t *clsNames[] = {
        L"AtlAxWin90", L"AtlAxWin80", L"AtlAxWin70", L"AtlAxWin"
    };
    for (int i = 0; i < 4; i++) {
        hBrowser = CreateWindowExW(0, clsNames[i], name,
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, URLBAR_HEIGHT, 100, 100, parent, (HMENU)ID_BROWSER,
            GetModuleHandleW(NULL), NULL);
        if (hBrowser) break;
    }
    if (!hBrowser) {
        MessageBoxW(parent, L"Gagal membuat kontrol browser (AtlAxWin).",
                    L"Sidebar Web", MB_ICONWARNING);
        return FALSE;
    }
    /* Paksa buat control-nya sekarang juga */
    ShowWindow(hBrowser, SW_SHOW);
    UpdateWindow(hBrowser);
    MSG m;
    for (int i = 0; i < 20 && PeekMessageW(&m, hBrowser, 0, 0, PM_REMOVE); i++) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return TRUE;
}

/* Ambil IWebBrowser2 yang di-host AtlAxWin */
static IWebBrowser2 *GetBrowserObject(void) {
    if (!hBrowser) return NULL;
    if (!hAtlDll) hAtlDll = LoadLibraryW(L"atl.dll");
    if (!hAtlDll) return NULL;

    IUnknown *pUnk = NULL;
    HRESULT hr = E_FAIL;

    /* 1) AtlAxGetControl -> langsung IUnknown dari control */
    AtlAxGetControl_t pGetCtl =
        (AtlAxGetControl_t)GetProcAddress(hAtlDll, "AtlAxGetControl");
    if (pGetCtl) hr = pGetCtl(hBrowser, &pUnk);

    /* 2) AtlAxGetHost -> IAxWinHostWindow -> QueryControl */
    if (FAILED(hr) || !pUnk) {
        AtlAxGetHost_t pGetHost =
            (AtlAxGetHost_t)GetProcAddress(hAtlDll, "AtlAxGetHost");
        if (pGetHost) {
            IUnknown *pHostUnk = NULL;
            if (SUCCEEDED(pGetHost(hBrowser, &pHostUnk)) && pHostUnk) {
                /* IAxWinHostWindow::QueryControl */
                IUnknown *pCtl = NULL;
                HRESULT (STDMETHODCALLTYPE *QCtrl)(IUnknown *, REFIID, void **) =
                    *(HRESULT (STDMETHODCALLTYPE **)(IUnknown *, REFIID, void **))
                    (*(void ***)pHostUnk + 4);
                if (SUCCEEDED(QCtrl(pHostUnk, &IID_IUnknown, (void **)&pCtl)))
                    pUnk = pCtl;
                pHostUnk->lpVtbl->Release(pHostUnk);
                hr = pUnk ? S_OK : E_FAIL;
            }
        }
    }

    /* 3) WM_ATLGETCONTROL (nilai resmi = WM_USER + 7) */
    if ((FAILED(hr) || !pUnk)) {
        LRESULT lr = SendMessageW(hBrowser, WM_USER + 7, 0, (LPARAM)&pUnk);
        if (lr != 0) { if (pUnk) pUnk->lpVtbl->Release(pUnk); pUnk = NULL; }
        hr = pUnk ? S_OK : E_FAIL;
    }

    if (FAILED(hr) || !pUnk) return NULL;

    IWebBrowser2 *pWeb = NULL;
    hr = pUnk->lpVtbl->QueryInterface(pUnk, &IID_IWebBrowser2,
                                      (void **)&pWeb);
    pUnk->lpVtbl->Release(pUnk);
    if (FAILED(hr) || !pWeb) return NULL;
    return pWeb;
}

void NavigateSidebar(const wchar_t *url) {
    wchar_t fixed[2048];
    NormalizeUrl(url, fixed, 2048);
    if (!fixed[0]) return;
    if (hUrlEdit) SetWindowTextW(hUrlEdit, fixed);
    if (!hBrowser && hSidebar) {
        if (!CreateBrowserControl(hSidebar)) return;
        LayoutSidebar();
    }
    if (!hBrowser) return;

    /* Control kadang belum siap tepat setelah CreateWindow,
     * coba beberapa kali dengan pompa pesan. */
    IWebBrowser2 *pWeb = NULL;
    for (int t = 0; t < 30 && !pWeb; t++) {
        pWeb = GetBrowserObject();
        if (pWeb) break;
        MSG m;
        while (PeekMessageW(&m, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
        Sleep(50);
    }
    if (!pWeb) {
        MessageBoxW(hSidebar ? hSidebar : hMain,
            L"Browser control belum siap / gagal dibuat.\n"
            L"Coba klik Go sekali lagi.",
            L"Sidebar Web", MB_ICONWARNING);
        return;
    }
    BSTR bUrl = SysAllocString(fixed);
    VARIANT vEmpty;
    VariantInit(&vEmpty);
    pWeb->lpVtbl->Navigate(pWeb, bUrl, &vEmpty, &vEmpty, &vEmpty, &vEmpty);
    SysFreeString(bUrl);
    pWeb->lpVtbl->Release(pWeb);
}

void NavigateSidebarFromEdit(void) {
    wchar_t url[2048];
    GetWindowTextW(hUrlEdit, url, 2048);
    wchar_t *s = url;
    while (*s == L' ' || *s == L'\t') s++;
    size_t n = wcslen(s);
    while (n > 0 && (s[n-1] == L' ' || s[n-1] == L'\t')) s[--n] = 0;
    NavigateSidebar(s);
    SetFocus(hUrlEdit);
}

void LayoutSidebar(void) {
    if (!hSidebar) return;
    RECT rc;
    GetClientRect(hSidebar, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int goW = 72, pad = 8;
    int editW = max(60, w - goW - pad * 3);
    if (hUrlEdit) MoveWindow(hUrlEdit, pad, 10, editW, 24, TRUE);
    if (hGoBtn) MoveWindow(hGoBtn, pad + editW + pad, 8, goW, 28, TRUE);
    if (hBrowser) MoveWindow(hBrowser, 0, URLBAR_HEIGHT,
                             w, max(0, h - URLBAR_HEIGHT), TRUE);
}

/* Proc container sidebar: teruskan WM_COMMAND ke WndProc utama
 * (STATIC bawaan tidak meneruskan, jadi tombol Go tidak jalan). */
static LRESULT CALLBACK SidebarProc(HWND hwnd, UINT msg,
                                    WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        LayoutSidebar();
        return 0;
    case WM_COMMAND:
        /* Teruskan ke window utama supaya ditangani di WndProc */
        if (hMain) SendMessageW(hMain, WM_COMMAND, wp, lp);
        return 0;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void RegisterSidebarClass(void) {
    static BOOL done = FALSE;
    if (done) return;
    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SidebarProc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"JpEditorSidebar";
    RegisterClassExW(&wc);
    done = TRUE;
}

void LayoutMainWindow(void) {
    if (!hMain || !hEdit) return;
    RECT rc;
    GetClientRect(hMain, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int bodyH = max(0, h - TOOLBAR_HEIGHT);
    if (sidebarVisible && hSidebar) {
        int sbW = min(sidebarWidth, max(240, w / 2));
        MoveWindow(hEdit, 0, TOOLBAR_HEIGHT,
                   max(0, w - sbW), bodyH, TRUE);
        MoveWindow(hSidebar, w - sbW, TOOLBAR_HEIGHT, sbW, bodyH, TRUE);
        ShowWindow(hSidebar, SW_SHOW);
        LayoutSidebar();
    } else {
        MoveWindow(hEdit, 0, TOOLBAR_HEIGHT, w, bodyH, TRUE);
        if (hSidebar) ShowWindow(hSidebar, SW_HIDE);
    }
}

void ToggleSidebar(void) {
    sidebarVisible = !sidebarVisible;
    if (sidebarVisible && !hSidebar) CreateSidebar(hMain);
    if (hToggleBtn) SetWindowTextW(hToggleBtn,
        sidebarVisible ? L"Hide Web" : L"Show Web");
    LayoutMainWindow();
    if (sidebarVisible && hUrlEdit) SetFocus(hUrlEdit);
    else SetFocus(hEdit);
}

void CreateSidebar(HWND parent) {
    if (hSidebar) return;
    RegisterSidebarClass();
    HINSTANCE hInst = GetModuleHandleW(NULL);
    hSidebar = CreateWindowExW(WS_EX_CLIENTEDGE, L"JpEditorSidebar", NULL,
        WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, TOOLBAR_HEIGHT, sidebarWidth, 100,
        parent, NULL, hInst, NULL);
    hUrlEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"https://",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        8, 10, 200, 24, hSidebar, (HMENU)ID_URL_EDIT, hInst, NULL);
    hGoBtn = CreateWindowW(L"BUTTON", L"Go",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        220, 8, 72, 28, hSidebar, (HMENU)ID_GO_BTN, hInst, NULL);
    if (hUICtrlFont) {
        SendMessageW(hUrlEdit, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
        SendMessageW(hGoBtn, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
    }
    CreateBrowserControl(hSidebar);
    ShowWindow(hSidebar, sidebarVisible ? SW_SHOW : SW_HIDE);
    LayoutSidebar();
}

void DestroySidebar(void) {
    hBrowser = NULL;
    hUrlEdit = NULL;
    hGoBtn = NULL;
    hSidebar = NULL;
}

