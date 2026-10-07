#include "common.h"

/* Lookup kata Jepang ke Jisho.org di sidebar.
 * - Kalau ada seleksi: pakai teks seleksi
 * - Kalau tidak: ambil kata di posisi caret (huruf/angka/JP) */

static BOOL IsWordChar(wchar_t c) {
    if (c == 0 || c == L'\r' || c == L'\n' || c == L' ' || c == L'\t')
        return FALSE;
    if (c == L'(' || c == L')' || c == L'[' || c == L']' ||
        c == L'{' || c == L'}' || c == L'<' || c == L'>' ||
        c == L'"' || c == L'\'' || c == L',' || c == L'.' ||
        c == L';' || c == L':' || c == L'!' || c == L'?' ||
        c == 0x3001 || c == 0x3002 || /* 、 。 */
        c == 0x300C || c == 0x300D || /* 「 」 */
        c == 0x300E || c == 0x300F)   /* 『 』 */
        return FALSE;
    return TRUE;
}

void LookupWord(const wchar_t *word) {
    wchar_t w[256];
    const wchar_t *s = word;
    while (*s == L' ' || *s == L'\t') s++;
    lstrcpynW(w, s, 256);
    size_t n = wcslen(w);
    while (n > 0 && (w[n-1] == L' ' || w[n-1] == L'\t')) w[--n] = 0;
    if (w[0] == 0) {
        MessageBoxW(hMain, L"Blok kata dulu lalu klik Jisho.",
                    L"Lookup Jepang", MB_ICONINFORMATION);
        return;
    }
    /* Buka sidebar kalau masih tertutup */
    if (!sidebarVisible) ToggleSidebar();

    /* URL-encode sederhana: biarkan alnum + JP, encode sisanya */
    wchar_t enc[1024];
    int p = 0;
    for (const wchar_t *q = w; *q && p < 1000; q++) {
        if ((*q >= L'a' && *q <= L'z') || (*q >= L'A' && *q <= L'Z') ||
            (*q >= L'0' && *q <= L'9') || *q == L'-' || *q == L'_') {
            enc[p++] = *q;
        } else {
            char u8[8] = {0};
            int u8len = WideCharToMultiByte(CP_UTF8, 0, q, 1,
                                            u8, sizeof(u8)-1, NULL, NULL);
            for (int i = 0; i < u8len && p < 1000; i++) {
                wsprintfW(enc + p, L"%%%02X", (unsigned char)u8[i]);
                p += 3;
            }
        }
    }
    enc[p] = 0;

    wchar_t url[1200];
    wsprintfW(url, L"https://jisho.org/search/%s", enc);
    NavigateSidebar(url);
    SetFocus(hEdit);
}

void LookupSelection(void) {
    if (!hEdit) return;
    DWORD sel = (DWORD)SendMessageW(hEdit, EM_GETSEL, 0, 0);
    int a = LOWORD(sel), b = HIWORD(sel);
    if (a != b) {
        int n = b - a;
        if (n > 200) { a = b - 200; n = 200; }
        int total = GetWindowTextLengthW(hEdit);
        wchar_t *all = (wchar_t *)malloc((total + 1) * sizeof(wchar_t));
        if (!all) return;
        GetWindowTextW(hEdit, all, total + 1);
        wchar_t w[256];
        wcsncpy(w, all + a, n);
        w[n] = 0;
        free(all);
        LookupWord(w);
        return;
    }
    /* Tidak ada seleksi: ambil kata di caret */
    int caret = a;
    int line = (int)SendMessageW(hEdit, EM_LINEFROMCHAR, caret, 0);
    int ls = (int)SendMessageW(hEdit, EM_LINEINDEX, line, 0);
    int ll = (int)SendMessageW(hEdit, EM_LINELENGTH, caret, 0);
    if (ll <= 0) {
        MessageBoxW(hMain, L"Blok kata dulu lalu klik Jisho.",
                    L"Lookup Jepang", MB_ICONINFORMATION);
        return;
    }
    wchar_t *lb = (wchar_t *)malloc((ll + 1) * sizeof(wchar_t));
    if (!lb) return;
    *(WORD *)lb = (WORD)ll;
    SendMessageW(hEdit, EM_GETLINE, line, (LPARAM)lb);
    lb[ll] = 0;
    int pos = caret - ls;
    int s = pos, e = pos;
    while (s > 0 && IsWordChar(lb[s-1])) s--;
    while (e < ll && IsWordChar(lb[e])) e++;
    if (s == e) {
        free(lb);
        MessageBoxW(hMain, L"Blok kata dulu lalu klik Jisho.",
                    L"Lookup Jepang", MB_ICONINFORMATION);
        return;
    }
    wchar_t w[256];
    int n = e - s;
    if (n > 200) n = 200;
    wcsncpy(w, lb + s, n);
    w[n] = 0;
    free(lb);
    LookupWord(w);
}
