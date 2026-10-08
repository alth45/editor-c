#include "common.h"
#include "webview2_min.h"

#include <oleauto.h>
#include <shlwapi.h>
#include <shlobj.h>

#ifdef _MSC_VER
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#endif

/* ==== State WebView2 ==== */
static HMODULE hWv2Loader = NULL;
static Wv2CreateEnvWithOptions_t pWv2CreateEnv = NULL;
static ICoreWebView2Environment *gEnv = NULL;
static ICoreWebView2Controller *gCtl = NULL;
static ICoreWebView2 *gWeb = NULL;
static BOOL wv2InitStarted = FALSE;
static BOOL wv2Failed = FALSE;
static wchar_t pendingUrl[2048] = L"";
static wchar_t wv2UserData[MAX_PATH] = L"";

/* ==== Helper URL ==== */
static void NormalizeUrl(const wchar_t *in, wchar_t *out, int outChars) {
    if (!in || !in[0]) { out[0] = 0; return; }
    if (wcsstr(in, L"://"))
        lstrcpynW(out, in, outChars);
    else
        wsprintfW(out, L"https://%s", in);
}

/* ==== Forward ==== */
static void Wv2EnsureInit(void);
static void Wv2Resize(void);
static void Wv2DoNavigate(const wchar_t *url);

/* ==== Riwayat URL (dropdown di sidebar) ====
 * Array pointer, index 0 = terbaru. Maks HISTORY_MAX entri, duplikat
 * dipindah ke atas, bukan diduplikasi. */
static wchar_t *gHistory[HISTORY_MAX];
static int gHistoryCount = 0;

/* URL yang layak dicatat (tolak kosong / about:blank / data:) */
static BOOL HistoryUsable(const wchar_t *u) {
    if (!u || !u[0]) return FALSE;
    if (_wcsnicmp(u, L"about:", 6) == 0) return FALSE;
    if (_wcsnicmp(u, L"data:", 5) == 0)  return FALSE;
    if (_wcsnicmp(u, L"edge:", 5) == 0)  return FALSE;
    return TRUE;
}

/* Isi ulang combobox dari array + teks placeholder.
 * CBS_DROPDOWNLIST tidak bisa SetWindowText sembarang: teks harus salah
 * satu item. Jadi placeholder ikut jadi item index 0. */
static void HistoryFillCombo(void) {
    if (!hHistoryCombo) return;
    SendMessageW(hHistoryCombo, CB_RESETCONTENT, 0, 0);
    if (gHistoryCount == 0) {
        SendMessageW(hHistoryCombo, CB_ADDSTRING, 0, (LPARAM)L"Belum ada riwayat");
    } else {
        wchar_t t[64];
        wsprintfW(t, L"Riwayat URL (%d) — pilih untuk buka", gHistoryCount);
        SendMessageW(hHistoryCombo, CB_ADDSTRING, 0, (LPARAM)t);
        for (int i = 0; i < gHistoryCount; i++)
            SendMessageW(hHistoryCombo, CB_ADDSTRING, 0, (LPARAM)gHistory[i]);
    }
    SendMessageW(hHistoryCombo, CB_SETCURSEL, 0, 0);
}

/* Catat URL: buang duplikat (pindah ke paling atas), buang entri terlama */
static void HistoryAdd(const wchar_t *url) {
    wchar_t u[2048];
    if (!HistoryUsable(url)) return;
    lstrcpynW(u, url, 2048);

    for (int i = 0; i < gHistoryCount; i++) {
        if (_wcsicmp(gHistory[i], u) != 0) continue;
        if (i == 0) return;                 /* sudah di paling atas */
        wchar_t *hit = gHistory[i];          /* geser entri lama ke bawah */
        memmove(&gHistory[1], &gHistory[0], i * sizeof(wchar_t *));
        gHistory[0] = hit;
        HistoryFillCombo();
        return;
    }

    if (gHistoryCount == HISTORY_MAX) {      /* buang yang terlama */
        free(gHistory[HISTORY_MAX - 1]);
        gHistory[HISTORY_MAX - 1] = NULL;
        gHistoryCount--;
    }
    memmove(&gHistory[1], &gHistory[0], gHistoryCount * sizeof(wchar_t *));
    gHistory[0] = (wchar_t *)malloc(2048 * sizeof(wchar_t));
    if (!gHistory[0]) return;
    lstrcpynW(gHistory[0], u, 2048);
    gHistoryCount++;
    HistoryFillCombo();
}

void ClearUrlHistory(void) {
    for (int i = 0; i < gHistoryCount; i++) free(gHistory[i]);
    ZeroMemory(gHistory, sizeof(gHistory));
    gHistoryCount = 0;
    HistoryFillCombo();
}

void NavigateHistorySelection(void) {
    if (!hHistoryCombo) return;
    int sel = (int)SendMessageW(hHistoryCombo, CB_GETCURSEL, 0, 0);
    /* index 0 = placeholder/header, URL mulai index 1 */
    if (sel == CB_ERR || sel <= 0) return;
    int idx = sel - 1;
    if (idx < 0 || idx >= gHistoryCount) return;
    NavigateSidebar(gHistory[idx]);
}

/* Poll get_Source(): URL bar + riwayat ikut update saat user berpindah
 * halaman lewat link/redirect (bukan cuma dari tombol Go). */
static void Wv2PollSource(void) {
    LPWSTR src = NULL;
    if (!gWeb) return;
    if (FAILED(gWeb->lpVtbl->get_Source(gWeb, &src)) || !src) return;
    if (src[0] && _wcsicmp(src, pendingUrl) != 0) {
        lstrcpynW(pendingUrl, src, 2048);
        /* jangan timpa teks saat user sedang mengetik di URL bar */
        if (hUrlEdit && GetFocus() != hUrlEdit) SetWindowTextW(hUrlEdit, src);
        HistoryAdd(src);
    }
    CoTaskMemFree(src);
}

/* ==== Handler: Environment selesai dibuat ==== */
static HRESULT STDMETHODCALLTYPE EnvQ(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *t,
    REFIID riid, void **pp) {
    if (!pp) return E_POINTER;
    *pp = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IWv2EnvCompletedHandler)) {
        *pp = t;
        t->lpVtbl->AddRef(t);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE EnvA(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *t) {
    return (ULONG)InterlockedIncrement(&t->ref);
}
static ULONG STDMETHODCALLTYPE EnvR(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *t) {
    LONG r = InterlockedDecrement(&t->ref);
    if (r == 0) free(t);
    return (ULONG)r;
}

/* ==== Handler: Controller selesai dibuat ==== */
static HRESULT STDMETHODCALLTYPE CtlQ(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *t,
    REFIID riid, void **pp) {
    if (!pp) return E_POINTER;
    *pp = NULL;
    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IWv2CtlCompletedHandler)) {
        *pp = t;
        t->lpVtbl->AddRef(t);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE CtlA(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *t) {
    return (ULONG)InterlockedIncrement(&t->ref);
}
static ULONG STDMETHODCALLTYPE CtlR(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *t) {
    LONG r = InterlockedDecrement(&t->ref);
    if (r == 0) free(t);
    return (ULONG)r;
}
static HRESULT STDMETHODCALLTYPE CtlInvoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *t,
    HRESULT res, ICoreWebView2Controller *ctl);

static HRESULT STDMETHODCALLTYPE EnvInvoke(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *t,
    HRESULT res, ICoreWebView2Environment *env) {
    (void)t;
    if (FAILED(res) || !env) {
        wv2Failed = TRUE;
        MessageBoxW(hSidebar ? hSidebar : hMain,
            L"WebView2 Runtime tidak ditemukan.\n"
            L"Install Microsoft Edge / WebView2 Runtime.",
            L"Sidebar Web", MB_ICONWARNING);
        return res;
    }
    env->lpVtbl->AddRef(env);
    if (gEnv) gEnv->lpVtbl->Release(gEnv);
    gEnv = env;

    /* Minta controller untuk parent = hSidebar */
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *h =
        (ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *)
        calloc(1, sizeof(*h));
    static const Wv2CtlHandlerVtbl ctlVtbl = { CtlQ, CtlA, CtlR, CtlInvoke };
    if (!h) return E_OUTOFMEMORY;
    h->lpVtbl = &ctlVtbl;
    h->ref = 1;
    return gEnv->lpVtbl->CreateCoreWebView2Controller(gEnv, hSidebar, h);
}

static HRESULT STDMETHODCALLTYPE CtlInvoke(
    ICoreWebView2CreateCoreWebView2ControllerCompletedHandler *t,
    HRESULT res, ICoreWebView2Controller *ctl) {
    (void)t;
    if (FAILED(res) || !ctl) {
        wv2Failed = TRUE;
        return res;
    }
    ctl->lpVtbl->AddRef(ctl);
    if (gCtl) gCtl->lpVtbl->Release(gCtl);
    gCtl = ctl;

    ICoreWebView2 *web = NULL;
    if (SUCCEEDED(gCtl->lpVtbl->get_CoreWebView2(gCtl, &web)) && web) {
        if (gWeb) gWeb->lpVtbl->Release(gWeb);
        gWeb = web;
    }
    gCtl->lpVtbl->put_IsVisible(gCtl, TRUE);
    Wv2Resize();

    /* Poll Source -> riwayat + URL bar ikut update saat klik link */
    if (hSidebar) SetTimer(hSidebar, ID_POLL_TIMER, 1000, NULL);

    /* Navigasi pending (atau default) */
    if (pendingUrl[0]) Wv2DoNavigate(pendingUrl);
    else Wv2DoNavigate(L"https://www.bing.com/");
    return S_OK;
}

/* ==== Cari + load WebView2Loader.dll ==== */
static BOOL Wv2LoadLoader(void) {
    if (pWv2CreateEnv) return TRUE;
    /* 1) di folder exe */
    hWv2Loader = LoadLibraryW(L"WebView2Loader.dll");
    /* 2) di subfolder umum */
    if (!hWv2Loader) hWv2Loader = LoadLibraryW(L".\\WebView2Loader.dll");
    /* 3) system32 (kalau runtime terinstall global kadang ada) */
    if (!hWv2Loader) {
        wchar_t sys[MAX_PATH];
        GetSystemDirectoryW(sys, MAX_PATH);
        wcscat(sys, L"\\WebView2Loader.dll");
        hWv2Loader = LoadLibraryW(sys);
    }
    if (!hWv2Loader) {
        MessageBoxW(hSidebar ? hSidebar : hMain,
            L"WebView2Loader.dll tidak ditemukan.\n"
            L"Copy WebView2Loader.dll (x64) ke folder editor.exe",
            L"Sidebar Web", MB_ICONWARNING);
        return FALSE;
    }
    pWv2CreateEnv = (Wv2CreateEnvWithOptions_t)
        GetProcAddress(hWv2Loader, "CreateCoreWebView2EnvironmentWithOptions");
    if (!pWv2CreateEnv) return FALSE;
    return TRUE;
}

static void Wv2EnsureInit(void) {
    if (wv2InitStarted || gEnv || wv2Failed) return;
    if (!hSidebar) return;
    if (!Wv2LoadLoader()) { wv2Failed = TRUE; return; }

    wv2InitStarted = TRUE;

    /* userDataFolder: %LOCALAPPDATA%\JpTextEditor\WV2 */
    if (!wv2UserData[0]) {
        wchar_t lad[MAX_PATH] = L"";
        if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA,
                                       NULL, 0, lad))) {
            wsprintfW(wv2UserData, L"%s\\JpTextEditor\\WV2", lad);
            CreateDirectoryW(lad, NULL);
            wchar_t sub[MAX_PATH];
            wsprintfW(sub, L"%s\\JpTextEditor", lad);
            CreateDirectoryW(sub, NULL);
            CreateDirectoryW(wv2UserData, NULL);
        }
    }

    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *h =
        (ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler *)
        calloc(1, sizeof(*h));
    static const Wv2EnvHandlerVtbl envVtbl = { EnvQ, EnvA, EnvR, EnvInvoke };
    if (!h) { wv2Failed = TRUE; return; }
    h->lpVtbl = &envVtbl;
    h->ref = 1;

    HRESULT hr = pWv2CreateEnv(NULL,
        wv2UserData[0] ? wv2UserData : NULL, NULL, h);
    if (FAILED(hr)) {
        wv2Failed = TRUE;
        h->lpVtbl->Release(h);
        MessageBoxW(hSidebar, L"Gagal init WebView2.",
                    L"Sidebar Web", MB_ICONWARNING);
    }
}

static void Wv2Resize(void) {
    if (!gCtl || !hSidebar) return;
    RECT rc;
    GetClientRect(hSidebar, &rc);
    RECT b;
    b.left = 0; b.top = URLBAR_HEIGHT;
    b.right = rc.right - rc.left;
    b.bottom = rc.bottom - rc.top;
    if (b.bottom < b.top) b.bottom = b.top;
    if (b.right < 0) b.right = 0;
    gCtl->lpVtbl->put_Bounds(gCtl, b);
}

static void Wv2DoNavigate(const wchar_t *url) {
    wchar_t fixed[2048];
    NormalizeUrl(url, fixed, 2048);
    if (!fixed[0]) return;
    lstrcpynW(pendingUrl, fixed, 2048);
    if (hUrlEdit) SetWindowTextW(hUrlEdit, fixed);
    HistoryAdd(fixed);
    if (gWeb) gWeb->lpVtbl->Navigate(gWeb, fixed);
}

/* ==== Public API (dipakai window.c via common.h) ==== */
void NavigateSidebar(const wchar_t *url) {
    if (!hSidebar) return;
    Wv2EnsureInit();
    Wv2DoNavigate(url);
}

void NavigateSidebarFromEdit(void) {
    wchar_t url[2048];
    if (!hUrlEdit) return;
    GetWindowTextW(hUrlEdit, url, 2048);
    wchar_t *s = url;
    while (*s == L' ' || *s == L'\t') s++;
    size_t n = wcslen(s);
    while (n > 0 && (s[n-1] == L' ' || s[n-1] == L'\t')) s[--n] = 0;
    NavigateSidebar(s);
    SetFocus(hUrlEdit);
}

/* URL sesi untuk disimpan ke .edt (pendingUrl = sumber kebenaran) */
void GetSidebarUrl(wchar_t *out, int outChars) {
    if (!out || outChars <= 0) return;
    if (hUrlEdit && GetFocus() == hUrlEdit) {
        GetWindowTextW(hUrlEdit, out, outChars);
        return;
    }
    lstrcpynW(out, pendingUrl, outChars);
}

void SetSidebarUrl(const wchar_t *url) {
    lstrcpynW(pendingUrl, url ? url : L"", 2048);
    if (hUrlEdit) SetWindowTextW(hUrlEdit, pendingUrl);
}

void LayoutSidebar(void) {
    if (!hSidebar) return;
    RECT rc;
    GetClientRect(hSidebar, &rc);
    int w = rc.right - rc.left;
    int goW = 72, clearW = 64, pad = 8;
    int editW = max(60, w - goW - pad * 3);
    if (hUrlEdit) MoveWindow(hUrlEdit, pad, 10, editW, 24, TRUE);
    if (hGoBtn) MoveWindow(hGoBtn, pad + editW + pad, 8, goW, 28, TRUE);
    /* Baris 2: riwayat */
    int histW = max(60, w - clearW - pad * 3);
    if (hHistoryCombo) MoveWindow(hHistoryCombo, pad, 44, histW, 240, TRUE);
    if (hHistoryClearBtn) MoveWindow(hHistoryClearBtn, pad + histW + pad,
                                     43, clearW, 26, TRUE);
    /* WebView2 tidak pakai child HWND hBrowser, jadi cukup resize bounds */
    Wv2Resize();
}

static LRESULT CALLBACK SidebarProc(HWND hwnd, UINT msg,
                                    WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_SIZE:
        LayoutSidebar();
        return 0;
    case WM_TIMER:
        if (wp == ID_POLL_TIMER) { Wv2PollSource(); return 0; }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, ID_POLL_TIMER);
        return 0;
    case WM_COMMAND:
        if (hMain) SendMessageW(hMain, WM_COMMAND, wp, lp);
        return 0;
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
    int sbH = StatusBarHeight();
    if (hStatus) {
        MoveWindow(hStatus, 0, h - sbH, w, sbH, TRUE);
        /* Panel terakhir stretch */
        int parts[4];
        parts[0] = 110;
        parts[1] = 220;
        parts[2] = 330;
        parts[3] = w - 8;
        SendMessageW(hStatus, SB_SETPARTS, 4, (LPARAM)parts);
    }
    int bodyH = max(0, h - TOOLBAR_HEIGHT - sbH);
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
    if (sidebarVisible) {
        Wv2EnsureInit();
        if (hUrlEdit) SetFocus(hUrlEdit);
    } else SetFocus(hEdit);
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
    /* Baris 2: dropdown riwayat URL + tombol Hapus */
    hHistoryCombo = CreateWindowW(L"COMBOBOX", NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL |
        CBS_DROPDOWNLIST | CBS_HASSTRINGS,
        8, 44, 200, 240, hSidebar, (HMENU)ID_HISTORY_COMBO, hInst, NULL);
    hHistoryClearBtn = CreateWindowW(L"BUTTON", L"Hapus",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        216, 43, 64, 26, hSidebar, (HMENU)ID_HISTORY_CLEAR, hInst, NULL);
    if (hUICtrlFont) {
        SendMessageW(hUrlEdit, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
        SendMessageW(hGoBtn, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
        SendMessageW(hHistoryCombo, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
        SendMessageW(hHistoryClearBtn, WM_SETFONT, (WPARAM)hUICtrlFont, TRUE);
    }
    HistoryFillCombo();
    ShowWindow(hSidebar, sidebarVisible ? SW_SHOW : SW_HIDE);
    LayoutSidebar();
    /* Init WebView2 async (butuh Runtime Edge) */
    Wv2EnsureInit();
}

void DestroySidebar(void) {
    if (hSidebar) KillTimer(hSidebar, ID_POLL_TIMER);
    if (gWeb) { gWeb->lpVtbl->Release(gWeb); gWeb = NULL; }
    if (gCtl) { gCtl->lpVtbl->Close(gCtl); gCtl->lpVtbl->Release(gCtl); gCtl = NULL; }
    if (gEnv) { gEnv->lpVtbl->Release(gEnv); gEnv = NULL; }
    wv2InitStarted = FALSE;
    pendingUrl[0] = 0;
    hBrowser = NULL;
    hUrlEdit = NULL;
    hGoBtn = NULL;
    hHistoryCombo = NULL;
    hHistoryClearBtn = NULL;
    hSidebar = NULL;
}


