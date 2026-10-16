#include "common.h"
#include <wchar.h>   /* wcslen, wcsncmp, wmemcpy */

/* Catatan kompatibilitas: editor memakai EDIT control biasa (bukan RichEdit),
 * jadi pesan EM_FINDTEXTW / EM_EXSETSEL / EM_GETSELTEXT TIDAK tersedia.
 * Pencarian dilakukan manual di salinan buffer (lihat DoFindStep). */

/* ==== Find/Replace untuk dokumen panjang ====
 * Dialog modeless (satu instance, dua mode):
 *   - Find     (Ctrl+F / menu Edit): Cari Selanjutnya / Sebelumnya
 *   - Replace  (Ctrl+H / menu Edit): + Ganti Satu / Ganti Semua
 *
 * Catatan desain (ramah dokumen besar):
 *   - Pencarian manual di salinan buffer + seleksi via EM_SETSEL —
 *     tanpa pesan RichEdit (EDIT biasa tidak punya EM_FINDTEXTW).
 *   - "Ganti Semua" menyalin teks SEKALI (O(n)), mengganti in-memory,
 *     lalu menulis balik satu kali; bukan loop cari-ganti per kejadian.
 *   - Enter di kolom Find = cari maju; Shift+Enter = cari mundur.
 *   - Enter di kolom Replace = ganti satu lalu lanjut cari.
 *   - Opsi "Bedakan besar/kecil"; pencarian otomatis wrap-around.
 *   - Saat dialog terbuka, F3 / Shift+F3 mengulang cari terakhir. */

static HWND g_hFindDlg = NULL;    /* dialog aktif (NULL = tertutup) */
static BOOL g_withRepl = FALSE;   /* TRUE = mode Replace, FALSE = mode Find */
static BOOL g_matchCase = FALSE;  /* opsi Beda Besar/Kecil */
static int  g_lastFrom = -1;      /* posisi mulai cari berikutnya (-1 = caret) */
static int  g_lastDir  = 1;       /* arah pencarian terakhir (F3 / Shift+F3) */

#define FIND_BUF 4096             /* maks panjang kata cari/ganti */

/* Status keberadaan dialog: pakai IsWindow (bukan pointer statis) supaya
 * tetap benar walau window hancur tanpa WM_DESTROY sampai ke kita. */
BOOL FindDialogOpen(void) { return g_hFindDlg != NULL && IsWindow(g_hFindDlg); }
HWND FindDlgHandle(void)  { return FindDialogOpen() ? g_hFindDlg : NULL; }

/* ---- ambil teks kolom edit (trim spasi/tab tepi) ---- */
static void GetEditStr(HWND hBox, wchar_t *out, int outChars) {
    out[0] = 0;
    if (!hBox) return;
    wchar_t tmp[FIND_BUF];
    GetWindowTextW(hBox, tmp, FIND_BUF);
    const wchar_t *s = tmp;
    while (*s == L' ' || *s == L'\t') s++;
    lstrcpynW(out, s, outChars);
    size_t n = wcslen(out);
    while (n > 0 && (out[n-1] == L' ' || out[n-1] == L'\t')) out[--n] = 0;
}

/* ---- label status kecil di dasar dialog ---- */
static void SetStatus(const wchar_t *text) {
    if (!g_hFindDlg) return;
    HWND st = GetDlgItem(g_hFindDlg, IDC_FIND_STATUS);
    if (st) SetWindowTextW(st, text);
}

/* ---- posisi mulai pencarian "maju" berikutnya (lewati hasil yang baru) ---- */
static int NextStartPos(int foundMin, int foundMax) {
    int len = GetWindowTextLengthW(hEdit);
    int next = foundMax + 1;
    if (next <= foundMin) next = foundMin + 1;
    if (next >= len) return -1;   /* mentok akhir dokumen */
    return next;
}

/* ---- salinan dokumen sekali pakai (EDIT biasa: tanpa EM_FINDTEXT) ---- */
static wchar_t *GetDocCopy(int *outLen) {
    int len = GetWindowTextLengthW(hEdit);
    if (len < 0) len = 0;
    wchar_t *buf = (wchar_t *)malloc(((size_t)len + 1) * sizeof(wchar_t));
    if (!buf) { if (outLen) *outLen = 0; return NULL; }
    buf[0] = 0;
    GetWindowTextW(hEdit, buf, len + 1);
    if (outLen) *outLen = len;
    return buf;
}

/* ---- cocok di posisi i? ---- */
static BOOL MatchAt(const wchar_t *doc, int i, const wchar_t *needle, size_t nl) {
    return g_matchCase ? (wcsncmp(doc + i, needle, nl) == 0)
                       : (_wcsnicmp(doc + i, needle, nl) == 0);
}

/* ---- cari maju pertama pada [from, docLen) ---- */
static int FindForwardPos(const wchar_t *doc, int docLen,
                          const wchar_t *needle, size_t nl, int from) {
    if (from < 0) from = 0;
    if (nl == 0 || from + (int)nl > docLen) return -1;
    for (int i = from; i + (int)nl <= docLen; i++)
        if (MatchAt(doc, i, needle, nl)) return i;
    return -1;
}

/* ---- cari mundur terakhir pada [0, from) ---- */
static int FindBackwardPos(const wchar_t *doc, int docLen,
                           const wchar_t *needle, size_t nl, int from) {
    if (from > docLen) from = docLen;
    if (nl == 0 || from < (int)nl) return -1;
    for (int i = from - (int)nl; i >= 0; i--)
        if (MatchAt(doc, i, needle, nl)) return i;
    return -1;
}

/* ---- flag arah pencarian (penanda internal, bukan flag RichEdit).
 * g_matchCase dibaca langsung oleh MatchAt(); tidak ada flag Win32. ---- */

/* ---- Cari satu kejadian. dir: +1 maju, -1 mundur. Wrap-around otomatis.
 * EDIT control biasa tidak punya EM_FINDTEXTW, jadi cari manual di
 * salinan buffer + seleksi via EM_SETSEL (bukan EM_EXSETSEL). ---- */
static void DoFindStep(int dir) {
    if (!hEdit || !g_hFindDlg) return;
    wchar_t needle[FIND_BUF];
    GetEditStr(GetDlgItem(g_hFindDlg, IDC_FIND_EDIT), needle, FIND_BUF);
    if (!needle[0]) { SetStatus(L"Kata cari kosong"); return; }

    int docLen = 0;
    wchar_t *doc = GetDocCopy(&docLen);
    if (!doc) { SetStatus(L"Memori tidak cukup"); return; }
    if (docLen <= 0) { free(doc); SetStatus(L"Dokumen kosong"); return; }
    size_t nl = wcslen(needle);

    /* titik awal: lanjut dari pencarian terakhir, atau dari caret/seleksi */
    DWORD sel = (DWORD)SendMessageW(hEdit, EM_GETSEL, 0, 0);
    int curMin = (int)LOWORD(sel), curMax = (int)HIWORD(sel);
    int from;
    if (g_lastFrom >= 0) {
        from = g_lastFrom;
        if (from > docLen) from = docLen;
    } else {
        from = (dir > 0) ? curMax : curMin;
    }

    int pos = (dir > 0) ? FindForwardPos(doc, docLen, needle, nl, from)
                        : FindBackwardPos(doc, docLen, needle, nl, from);
    BOOL wrapped = FALSE;
    if (pos < 0) {
        /* wrap-around: ulangi dari ujung lain dokumen */
        pos = (dir > 0) ? FindForwardPos(doc, docLen, needle, nl, 0)
                        : FindBackwardPos(doc, docLen, needle, nl, docLen);
        if (pos < 0) {
            free(doc);
            wchar_t m[256 + FIND_BUF];
            wsprintfW(m, L"\"%s\" tidak ditemukan", needle);
            SetStatus(m);
            g_lastFrom = -1;
            return;
        }
        wrapped = TRUE;
    }

    int foundMin = pos, foundMax = pos + (int)nl;
    free(doc);

    SendMessageW(hEdit, EM_SETSEL, foundMin, foundMax);
    SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
    UpdateStatusBar();
    g_lastFrom = (dir > 0) ? NextStartPos(foundMin, foundMax) : foundMin;
    g_lastDir  = dir;
    SetStatus(wrapped ? L"Ditemukan (lanjutan dari awal dokumen)" : L"");
}

/* ---- F3 / Shift+F3: ulangi pencarian terakhir.
 * dir = 0 -> pakai arah cari terakhir; +1/-1 -> paksa arah.
 * Dipakai WndProc (F3 global saat dialog terbuka). ---- */
void FindRepeatLast(int dir) {
    if (!FindDialogOpen()) return;
    DoFindStep(dir != 0 ? dir : g_lastDir);
}

/* (deklarasi maju — definisi lengkap di bawah, dekat ShowFindDialog) */
static LRESULT CALLBACK FindDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp);
static void FinishFindDialog(HWND hDlg);
void CloseFindDialog(void);

/* ---- seleksi saat ini cocok dengan needle?
 * EDIT biasa: ambil slice via EM_GETSEL + GetWindowTextW (tanpa EM_GETSELTEXT
 * yang hanya milik RichEdit). ---- */
static BOOL SelectionMatchesNeedle(const wchar_t *needle) {
    DWORD sel = (DWORD)SendMessageW(hEdit, EM_GETSEL, 0, 0);
    int a = (int)LOWORD(sel), b = (int)HIWORD(sel);
    if (b <= a || b - a >= FIND_BUF) return FALSE;
    int docLen = 0;
    wchar_t *doc = GetDocCopy(&docLen);
    if (!doc) return FALSE;
    if (a < 0) a = 0;
    if (b > docLen) b = docLen;
    int n = b - a;
    wchar_t got[FIND_BUF];
    got[0] = 0;
    if (n > 0 && n < FIND_BUF) {
        wcsncpy(got, doc + a, n);
        got[n] = 0;
    }
    free(doc);
    if (n <= 0) return FALSE;
    return g_matchCase ? (wcscmp(got, needle) == 0)
                       : (_wcsicmp(got, needle) == 0);
}

/* ---- tulis ulang seluruh isi edit secara bulk (untuk Ganti Semua) ---- */
static void SetEditTextBulk(const wchar_t *text) {
    suppressChange = TRUE;               /* cegah banjir EN_CHANGE */
    SetWindowTextW(hEdit, text);
    suppressChange = FALSE;
    modified = TRUE;
    InvalidateStatusBarCache();
    SetTitle();
}

/* ---- Ganti satu kejadian pada seleksi; opsional lanjut cari berikutnya ---- */
static void DoReplaceOne(BOOL findNextAfter) {
    if (!hEdit || !g_hFindDlg) return;
    wchar_t needle[FIND_BUF], repl[FIND_BUF];
    GetEditStr(GetDlgItem(g_hFindDlg, IDC_FIND_EDIT), needle, FIND_BUF);
    GetEditStr(GetDlgItem(g_hFindDlg, IDC_REPLACE_EDIT), repl, FIND_BUF);
    if (!needle[0]) { SetStatus(L"Kata cari kosong"); return; }

    if (!SelectionMatchesNeedle(needle)) {
        /* belum ada kandidat pada seleksi: cari dulu dari caret */
        DoFindStep(+1);
        if (!SelectionMatchesNeedle(needle))
            return;   /* tidak ditemukan — status sudah ditampilkan DoFindStep */
    }

    int a = (int)LOWORD((DWORD)SendMessageW(hEdit, EM_GETSEL, 0, 0));
    SendMessageW(hEdit, EM_REPLACESEL, TRUE, (LPARAM)repl);

    int newEnd = a + (int)wcslen(repl);
    int docLen = GetWindowTextLengthW(hEdit);
    g_lastFrom = (newEnd < docLen) ? newEnd : -1;

    if (findNextAfter) {
        /* tampilkan kandidat berikutnya supaya user bisa Enter-Enter-Enter */
        DoFindStep(+1);
    } else {
        SetStatus(L"");
        SetFocus(hEdit);
    }
}

/* ---- Ganti Semua: satu salinan O(n) + tulis balik satu kali ---- */
static void DoReplaceAll(void) {
    if (!hEdit || !g_hFindDlg) return;
    wchar_t needle[FIND_BUF], repl[FIND_BUF];
    GetEditStr(GetDlgItem(g_hFindDlg, IDC_FIND_EDIT), needle, FIND_BUF);
    GetEditStr(GetDlgItem(g_hFindDlg, IDC_REPLACE_EDIT), repl, FIND_BUF);
    if (!needle[0]) { SetStatus(L"Kata cari kosong"); return; }

    int len = GetWindowTextLengthW(hEdit);
    if (len <= 0) { SetStatus(L"Dokumen kosong"); return; }

    wchar_t *buf = (wchar_t *)malloc(((size_t)len + 1) * sizeof(wchar_t));
    if (!buf) { SetStatus(L"Memori tidak cukup"); return; }
    GetWindowTextW(hEdit, buf, len + 1);

    size_t nl = wcslen(needle), rl = wcslen(repl);
    size_t cap = (size_t)len + 16;
    wchar_t *out = (wchar_t *)malloc(cap * sizeof(wchar_t));
    if (!out) { free(buf); SetStatus(L"Memori tidak cukup"); return; }

    long count = 0;
    size_t i = 0, o = 0;
    while (buf[i]) {
        BOOL match = g_matchCase ? (wcsncmp(buf + i, needle, nl) == 0)
                                 : (_wcsnicmp(buf + i, needle, nl) == 0);
        if (match) {
            if (o + rl + 1 > cap) {
                size_t ncap = cap * 2 + rl * 4;
                wchar_t *nb = (wchar_t *)realloc(out, ncap * sizeof(wchar_t));
                if (!nb) break;
                out = nb; cap = ncap;
            }
            wmemcpy(out + o, repl, rl);
            o += rl; i += nl; count++;
        } else {
            if (o + 2 > cap) {
                size_t ncap = cap * 2 + 16;
                wchar_t *nb = (wchar_t *)realloc(out, ncap * sizeof(wchar_t));
                if (!nb) break;
                out = nb; cap = ncap;
            }
            out[o++] = buf[i++];
        }
    }
    out[o] = 0;
    free(buf);

    if (count == 0) {
        free(out);
        wchar_t m[256 + FIND_BUF];
        wsprintfW(m, L"\"%s\" tidak ditemukan", needle);
        SetStatus(m);
        return;
    }

    /* jaga posisi caret relatif agar view tidak melompat ke awal dokumen */
    DWORD sel = (DWORD)SendMessageW(hEdit, EM_GETSEL, 0, 0);
    int caret = (int)LOWORD(sel);
    if (caret > (int)o) caret = (int)o;

    SetEditTextBulk(out);
    free(out);

    SendMessageW(hEdit, EM_SETSEL, caret, caret);
    SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
    g_lastFrom = -1;

    wchar_t m[128];
    wsprintfW(m, L"%ld occurrences diganti", count);
    SetStatus(m);
    SetFocus(hEdit);
}

/* ---- tinggi client dialog yang pas untuk mode saat ini (DPI-aware) ---- */
static int DlgHeightForMode(void) {
    /* baris: find, case, [repl, repl-btns], nav, hint, status */
    int rows = g_withRepl ? 7 : 4;
    return MulDiv(rows * 32 + 14, GetCachedDpiY(), 96);
}

/* ---- layout child: posisi & visibilitas panel Replace sesuai mode ---- */
static void LayoutFindDialog(void) {
    if (!g_hFindDlg) return;
    int y = 10;

    MoveWindow(GetDlgItem(g_hFindDlg, IDC_FIND_LABEL), 10, y + 4,  70, 20, TRUE);
    MoveWindow(GetDlgItem(g_hFindDlg, IDC_FIND_EDIT),  84, y,     210, 24, TRUE);
    y += 30;
    MoveWindow(GetDlgItem(g_hFindDlg, IDC_MATCH_CASE), 84, y,     180, 20, TRUE);
    y += 28;

    HWND rl = GetDlgItem(g_hFindDlg, IDC_REPLACE_LABEL);
    HWND re = GetDlgItem(g_hFindDlg, IDC_REPLACE_EDIT);
    HWND r1 = GetDlgItem(g_hFindDlg, IDC_REPLACE_ONE);
    HWND ra = GetDlgItem(g_hFindDlg, IDC_REPLACE_ALL);
    int show = g_withRepl ? SW_SHOW : SW_HIDE;
    ShowWindow(rl, show); ShowWindow(re, show);
    ShowWindow(r1, show); ShowWindow(ra, show);
    if (g_withRepl) {
        MoveWindow(rl, 10, y + 4,  70, 20, TRUE);
        MoveWindow(re, 84, y,     210, 24, TRUE);
        y += 30;
        MoveWindow(r1, 84, y,     105, 26, TRUE);
        MoveWindow(ra, 195, y,    105, 26, TRUE);
        y += 34;
    }

    MoveWindow(GetDlgItem(g_hFindDlg, IDC_FIND_PREV), 84,  y, 105, 26, TRUE);
    MoveWindow(GetDlgItem(g_hFindDlg, IDC_FIND_NEXT), 195, y, 105, 26, TRUE);
    y += 32;
    MoveWindow(GetDlgItem(g_hFindDlg, IDC_WRAP_LABEL),  10, y, 300, 16, TRUE);
    y += 20;
    MoveWindow(GetDlgItem(g_hFindDlg, IDC_FIND_STATUS), 10, y, 300, 16, TRUE);

    /* sesuaikan tinggi window (client) terhadap mode */
    RECT rcC, rcW;
    GetClientRect(g_hFindDlg, &rcC);
    GetWindowRect(g_hFindDlg, &rcW);
    int want = DlgHeightForMode();
    if (rcC.bottom != want) {
        int frame = (rcW.bottom - rcW.top) - rcC.bottom;
        MoveWindow(g_hFindDlg, rcW.left, rcW.top,
                   rcW.right - rcW.left, want + frame, TRUE);
    }
}

/* ---- tutup dialog (modeless): DestroyWindow; WM_DESTROY set NULL.
 *   PENTING: jangan pernah memanggil EndDialog pada window modeless —
 *   itu merusak state internal user32 dan membuat popup berikutnya
 *   gagal muncul. ---- */
static void FinishFindDialog(HWND hDlg) {
    if (hDlg && IsWindow(hDlg)) DestroyWindow(hDlg);
}

/* ---- prosedure dialog (modeless, sudah jadi WndProc class) ---- */
static LRESULT CALLBACK FindDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_INITDIALOG: {
        /* font UI kadang belum siap saat dipanggil lewat jalur lain —
         * buat cadangan lokal supaya dialog tetap terbaca normal */
        static HFONT sFallbackFont = NULL;
        HFONT hf = hUICtrlFont;
        if (!hf) {
            if (!sFallbackFont) {
                int dpi = GetCachedDpiY();
                sFallbackFont = CreateFontW(-MulDiv(9, dpi, 72), 0, 0, 0, FW_NORMAL,
                    0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
            }
            hf = sFallbackFont;
        }
        SendMessageW(hDlg, WM_SETFONT, (WPARAM)hf, TRUE);
        static const int kidIds[] = {
            IDC_FIND_LABEL, IDC_FIND_EDIT, IDC_MATCH_CASE,
            IDC_FIND_NEXT, IDC_FIND_PREV, IDC_FIND_STATUS,
            IDC_REPLACE_LABEL, IDC_REPLACE_EDIT,
            IDC_REPLACE_ONE, IDC_REPLACE_ALL, IDC_WRAP_LABEL,
        };
        for (unsigned i = 0; i < sizeof(kidIds)/sizeof(kidIds[0]); i++) {
            HWND k = GetDlgItem(hDlg, kidIds[i]);
            if (k) SendMessageW(k, WM_SETFONT, (WPARAM)hf, TRUE);
        }

        /* prefill kata cari dari seleksi editor (praktis utk cari cepat) */
        if (hEdit) {
            DWORD sel = (DWORD)SendMessageW(hEdit, EM_GETSEL, 0, 0);
            int a = (int)LOWORD(sel), b = (int)HIWORD(sel);
            if (b > a && b - a < FIND_BUF - 1) {
                int docLen = 0;
                wchar_t *doc = GetDocCopy(&docLen);
                if (doc) {
                    if (a < 0) a = 0;
                    if (b > docLen) b = docLen;
                    wchar_t w[FIND_BUF];
                    w[0] = 0;
                    if (b > a) {
                        int n = b - a;
                        if (n >= FIND_BUF) n = FIND_BUF - 1;
                        wcsncpy(w, doc + a, n);
                        w[n] = 0;
                    }
                    free(doc);
                    if (w[0] && !wcschr(w, L'\r') && !wcschr(w, L'\n'))
                        SetWindowTextW(GetDlgItem(hDlg, IDC_FIND_EDIT), w);
                }
            }
        }
        CheckDlgButton(hDlg, IDC_MATCH_CASE,
                       g_matchCase ? BST_CHECKED : BST_UNCHECKED);
        LayoutFindDialog();
        SendDlgItemMessageW(hDlg, IDC_FIND_EDIT, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(hDlg, IDC_FIND_EDIT));
        return TRUE;
    }

    case WM_COMMAND: {
        WORD id = LOWORD(wp), code = HIWORD(wp);
        switch (id) {
        case IDC_FIND_NEXT:
            g_lastFrom = -1;           /* tombol eksplisit: mulai dari caret */
            DoFindStep(+1);
            return TRUE;
        case IDC_FIND_PREV:
            g_lastFrom = -1;
            DoFindStep(-1);
            return TRUE;
        case IDC_REPLACE_ONE:
            DoReplaceOne(FALSE);
            return TRUE;
        case IDC_REPLACE_ALL:
            DoReplaceAll();
            return TRUE;
        case IDC_MATCH_CASE:
            if (code == BN_CLICKED) {
                g_matchCase = (IsDlgButtonChecked(hDlg, IDC_MATCH_CASE)
                               == BST_CHECKED);
                g_lastFrom = -1;
            }
            return TRUE;
        case IDC_FIND_EDIT:
            if (code == EN_CHANGE) g_lastFrom = -1;
            return TRUE;
        case IDOK:                      /* Enter default -> cari maju */
            DoFindStep(+1);
            return TRUE;
        case IDCANCEL:                   /* Esc / X -> tutup */
            FinishFindDialog(hDlg);
            return TRUE;
        }
        /* Kontrol tanpa penanganan eksplisit (mis. BS_AUTOCHECKBOX yang
         * tidak dikode sendiri) harus di-pass ke DefDlgProcW agar status
         * dialog tetap sehat — jangan kembalikan FALSE mentah. */
        return DefDlgProcW(hDlg, msg, wp, lp);
    }

    case WM_CLOSE:
        FinishFindDialog(hDlg);
        return TRUE;

    case WM_DESTROY:
        if (g_hFindDlg == hDlg) g_hFindDlg = NULL;
        g_lastFrom = -1;
        return TRUE;
    }
    return DefDlgProcW(hDlg, msg, wp, lp);
}

/* ---- subclass kolom edit dialog: Enter = aksi, F3 = ulang cari.
 * Enter/F3 harus ditangkap lewat subclass karena IsDialogMessageW di
 * message loop menelan VK_RETURN (IDOK) dan VK_F3 sebelum WndProc. ---- */
static WNDPROC g_oldFindEditProc = NULL;
static LRESULT CALLBACK FindEditSubProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_KEYDOWN && wp == VK_RETURN) {
        BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        int id = (int)(INT_PTR)GetWindowLongPtrW(hwnd, GWLP_ID);
        if (id == IDC_FIND_EDIT) {
            g_lastFrom = -1;
            DoFindStep(shift ? -1 : +1);
        } else if (id == IDC_REPLACE_EDIT) {
            DoReplaceOne(TRUE);          /* Enter = ganti satu + lanjut cari */
        }
        return 0;
    }
    if (msg == WM_KEYDOWN && wp == VK_F3) {
        BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        FindRepeatLast(shift ? -1 : +1);
        return 0;
    }
    return CallWindowProcW(g_oldFindEditProc, hwnd, msg, wp, lp);
}

/* ---- helper pembuat kontrol (dialog dibangun manual, tanpa .rc) ---- */
static HWND MakeStatic(HWND parent, const wchar_t *text, int x, int y, int w, int id) {
    return CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP,
        x, y, w, 20, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}
static HWND MakeEditBox(HWND parent, int x, int y, int w, int id) {
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        x, y, w, 24, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}
static HWND MakeButton(HWND parent, const wchar_t *text, int x, int y,
                       int w, int id, DWORD style) {
    return CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | style,
        x, y, w, 26, parent, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
}

void ShowFindDialog(BOOL withReplace) {
    if (!hEdit) return;

    /* BUG FIX: dulu, saat dialog sudah ada, fungsi ini hanya memanggil
     * SetForegroundWindow(). Kalau window lama sudah hancur/tersembunyi
     * (pointer statis tidak di-reset), popup baru TIDAK PERNAH dibuat —
     * itulah sebabnya Ctrl+F / menu Edit tampak "tidak memunculkan apa-apa".
     * Sekarang: kalau masih terbuka cukup angkat + fokus; kalau stale,
     * tutup bersih lalu jatuh ke jalur pembuatan window baru di bawah. */
    if (g_hFindDlg && IsWindow(g_hFindDlg)) {
        /* Sudah terbuka: sinkronkan mode (Find<->Replace), angkat + fokus.
         * Tanpa ini Ctrl+F kedua / klik menu tampak "mati". */
        if (g_withRepl != withReplace) {
            g_withRepl = withReplace;
            SetWindowTextW(g_hFindDlg,
                withReplace ? L"Ganti / Replace" : L"Cari / Find");
            LayoutFindDialog();
        }
        ShowWindow(g_hFindDlg, SW_SHOW);
        BringWindowToTop(g_hFindDlg);
        SetForegroundWindow(g_hFindDlg);
        UpdateWindow(g_hFindDlg);
        SetFocus(GetDlgItem(g_hFindDlg, IDC_FIND_EDIT));
        return;
    }
    if (g_hFindDlg) {          /* pointer basi -> bersihkan */
        g_hFindDlg = NULL;
    }

    g_withRepl = withReplace;
    g_lastFrom = -1;
    HINSTANCE hInst = GetModuleHandleW(NULL);

    /* class dialog didaftarkan sekali (cek nama, bukan atom, agar aman
     * terhadap kegagalan registrasi sebelumnya).
     * PENTING: cbWndExtra = DLGWINDOWEXTRA wajib karena FindDlgProc
     * memanggil DefDlgProcW (butuh slot DWL_*). Tanpa ini, state dialog
     * rusak dan popup bisa gagal muncul / tombol tidak merespons. */
    static BOOL sRegistered = FALSE;
    if (!sRegistered) {
        WNDCLASSEXW wc;
        ZeroMemory(&wc, sizeof(wc));
        wc.cbSize        = sizeof(wc);
        wc.lpfnWndProc   = FindDlgProc;
        wc.cbWndExtra    = DLGWINDOWEXTRA;
        wc.hInstance     = hInst;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"JpEditorFindDlg";
        if (!RegisterClassExW(&wc)) {
            /* mungkin sudah terdaftar oleh percobaan sebelumnya — cek class */
            WNDCLASSW qc;
            if (!GetClassInfoW(hInst, L"JpEditorFindDlg", &qc)) {
                g_withRepl = FALSE;
                return;   /* registrasi benar-benar gagal */
            }
        }
        sRegistered = TRUE;
    }

    int clientH = DlgHeightForMode();
    int dpiY = GetCachedDpiY();
    int dlgW = MulDiv(320, dpiY, 96);
    /* Modeless owned POPUP (bukan WS_CHILD): tidak ter-clipping/tertutup
     * hEdit, punya caption sendiri, dan notifikasi kontrolnya masuk ke
     * FindDlgProc (bukan ke WndProc utama). hMain jadi owner. */
    g_hFindDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,
        L"JpEditorFindDlg",
        withReplace ? L"Ganti / Replace" : L"Cari / Find",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        0, 0, dlgW, clientH,
        hMain, NULL, hInst, NULL);
    if (!g_hFindDlg) { g_withRepl = FALSE; return; }

    /* NOTE: FindDlgProc sudah jadi WndProc class (lihat registrasi di atas),
     * jadi JANGAN SetWindowLongPtr(DWLP_DLGPROC) lagi di sini. */

    /* kontrol dibuat di posisi kasar; LayoutFindDialog menata ulang +
     * mengatur visibilitas panel Replace sesuai mode */
    MakeStatic(g_hFindDlg, L"Cari:",  10, 14,  70, IDC_FIND_LABEL);
    HWND hFindEdit = MakeEditBox(g_hFindDlg, 84, 10, 210, IDC_FIND_EDIT);
    MakeButton(g_hFindDlg, L"Bedakan besar/kecil", 84, 40, 180,
               IDC_MATCH_CASE, BS_AUTOCHECKBOX);
    MakeStatic(g_hFindDlg, L"Ganti:", 10, 74,  70, IDC_REPLACE_LABEL);
    HWND hReplEdit = MakeEditBox(g_hFindDlg, 84, 70, 210, IDC_REPLACE_EDIT);
    MakeButton(g_hFindDlg, L"Ganti Satu",  84, 100, 105, IDC_REPLACE_ONE, BS_PUSHBUTTON);
    MakeButton(g_hFindDlg, L"Ganti Semua", 195, 100, 105, IDC_REPLACE_ALL, BS_PUSHBUTTON);
    MakeButton(g_hFindDlg, L"<< Sebelumnya", 84, 138, 105, IDC_FIND_PREV, BS_PUSHBUTTON);
    MakeButton(g_hFindDlg, L"Selanjutnya >>", 195, 138, 105, IDC_FIND_NEXT, BS_PUSHBUTTON);
    MakeStatic(g_hFindDlg, L"Enter = cari, Shift+Enter = mundur, F3 = ulang",
               10, 168, 300, IDC_WRAP_LABEL);
    MakeStatic(g_hFindDlg, L"", 10, 188, 300, IDC_FIND_STATUS);

    /* Enter/Shift+Enter langsung memicu cari/ganti */
    g_oldFindEditProc = (WNDPROC)SetWindowLongPtrW(hFindEdit, GWLP_WNDPROC,
                                                   (LONG_PTR)FindEditSubProc);
    SetWindowLongPtrW(hReplEdit, GWLP_WNDPROC, (LONG_PTR)FindEditSubProc);

    SendMessageW(g_hFindDlg, WM_INITDIALOG, 0, 0);

    /* posisikan dialog owned-POPUP di kanan-atas area klien utama.
     * Karena ini POPUP, koordinatnya SCREEN -> konversi dari client hMain. */
    RECT rcC;
    GetClientRect(hMain, &rcC);
    POINT pt = { rcC.right - dlgW - 16, TOOLBAR_HEIGHT + 8 };
    if (pt.x < 0) pt.x = 0;
    ClientToScreen(hMain, &pt);
    SetWindowPos(g_hFindDlg, HWND_TOP, pt.x, pt.y, 0, 0,
                 SWP_NOSIZE | SWP_SHOWWINDOW);

    /* pastikan visible & repaint — inti perbaikan bug "popup tidak muncul" */
    ShowWindow(g_hFindDlg, SW_SHOW);
    UpdateWindow(g_hFindDlg);
    SetFocus(GetDlgItem(g_hFindDlg, IDC_FIND_EDIT));
}

/* ---- tutup dialog dari luar (WM_DESTROY main window) ---- */
void CloseFindDialog(void) {
    if (g_hFindDlg && IsWindow(g_hFindDlg)) DestroyWindow(g_hFindDlg);
    g_hFindDlg = NULL;
}
