#include "common.h"

/* ==== Format .edt (sesi editor) ====
 * UTF-8, baris 1: "[JpEditor v1]"
 * header key=value sampai baris kosong, lalu body teks mentah:
 *   font=<nama>   size=<pt>   url=<web>
 * .txt biasa tetap bisa dibuka (tanpa header -> teks polos).
 */

#define EDT_MAGIC L"[JpEditor v1]"

static BOOL IsEdtPath(const wchar_t *path) {
    size_t n = wcslen(path);
    if (n < 4) return FALSE;
    return _wcsicmp(path + n - 4, L".edt") == 0;
}

/* tulis string UTF-8; return FALSE jika gagal */
static BOOL WriteU8(HANDLE hf, const wchar_t *w, int wlen) {
    DWORD written = 0;
    if (wlen < 0) wlen = (int)wcslen(w);
    if (wlen == 0) return TRUE;
    int u8len = WideCharToMultiByte(CP_UTF8, 0, w, wlen, NULL, 0, NULL, NULL);
    if (u8len <= 0) return FALSE;
    char *u8 = (char *)malloc(u8len);
    if (!u8) return FALSE;
    WideCharToMultiByte(CP_UTF8, 0, w, wlen, u8, u8len, NULL, NULL);
    BOOL ok = WriteFile(hf, u8, u8len, &written, NULL) &&
              written == (DWORD)u8len;
    free(u8);
    return ok;
}

/* ==== Save: .edt = header + teks; .txt = teks UTF-8 BOM ==== */
BOOL SaveFileTo(const wchar_t *path) {
    int len = GetWindowTextLengthW(hEdit);
    wchar_t *wbuf = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!wbuf) return FALSE;
    GetWindowTextW(hEdit, wbuf, len + 1);

    HANDLE hf = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) { free(wbuf); return FALSE; }

    BOOL ok = TRUE;
    if (IsEdtPath(path)) {
        /* header sesi */
        wchar_t url[2048] = L"";
        GetSidebarUrl(url, 2048);
        wchar_t head[2600];
        wsprintfW(head, L"%s\r\nfont=%s\r\nsize=%d\r\nurl=%s\r\n\r\n",
                  EDT_MAGIC, japaneseFonts[currentFontIndex],
                  sizes[currentSizeIndex], url);
        ok = WriteU8(hf, head, -1) && WriteU8(hf, wbuf, len);
        lstrcpynW(currentEncoding, L"EDT", 32);
    } else {
        int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, len,
                                        NULL, 0, NULL, NULL);
        char *u8 = NULL;
        if (u8len > 0) {
            u8 = (char *)malloc(u8len);
            if (!u8) { free(wbuf); CloseHandle(hf); return FALSE; }
            WideCharToMultiByte(CP_UTF8, 0, wbuf, len, u8, u8len, NULL, NULL);
        }
        DWORD written;
        BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
        ok = WriteFile(hf, bom, 3, &written, NULL);
        if (ok && u8len > 0)
            ok = WriteFile(hf, u8, u8len, &written, NULL);
        free(u8);
        lstrcpynW(currentEncoding, L"UTF-8 BOM", 32);
    }
    CloseHandle(hf);
    free(wbuf);
    return ok;
}

/* decode buffer mentah -> UTF-16 (deteksi BOM, spt sebelumnya) */
static wchar_t *DecodeBytes(const BYTE *data, DWORD size) {
    wchar_t *wbuf = NULL;
    int wlen = 0;

    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        /* UTF-16 LE */
        lstrcpynW(currentEncoding, L"UTF-16 LE", 32);
        wlen = (int)((size - 2) / 2);
        wbuf = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf) { memcpy(wbuf, data + 2, wlen * 2); wbuf[wlen] = 0; }
    } else {
        DWORD off = 0;
        if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
            off = 3; /* BOM dilewati, encoding ditimpa EDT bila perlu */
        /* Coba UTF-8 tanpa BOM dulu, jika gagal -> ANSI/Shift-JIS */
        wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   (char *)data + off, size - off, NULL, 0);
        UINT cp = (wlen > 0) ? CP_UTF8 : CP_ACP;
        if (wlen > 0) lstrcpynW(currentEncoding, L"UTF-8", 32);
        else lstrcpynW(currentEncoding, L"ANSI", 32);
        DWORD flags = (wlen > 0) ? MB_ERR_INVALID_CHARS : 0;
        if (wlen <= 0)
            wlen = MultiByteToWideChar(cp, 0, (char *)data + off,
                                       size - off, NULL, 0);
        wbuf = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf) {
            MultiByteToWideChar(cp, flags, (char *)data + off,
                                size - off, wbuf, wlen);
            wbuf[wlen] = 0;
        }
    }
    return wbuf;
}

/* ==== Load: .edt (header sesi) atau teks biasa ==== */
BOOL LoadFileFrom(const wchar_t *path) {
    HANDLE hf = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hf == INVALID_HANDLE_VALUE) return FALSE;

    DWORD size = GetFileSize(hf, NULL);
    if (size == INVALID_FILE_SIZE) { CloseHandle(hf); return FALSE; }

    BYTE *data = (BYTE *)malloc(size + 4);
    if (!data) { CloseHandle(hf); return FALSE; }
    DWORD read = 0;
    ReadFile(hf, data, size, &read, NULL);
    CloseHandle(hf);
    data[size] = 0; data[size+1] = 0; data[size+2] = 0; data[size+3] = 0;

    wchar_t *wbuf = DecodeBytes(data, size);
    free(data);
    if (!wbuf) return FALSE;

    /* .edt: baris 1 magic -> parse font/size/url sampai baris kosong */
    if (wcsncmp(wbuf, EDT_MAGIC, wcslen(EDT_MAGIC)) == 0) {
        wchar_t fontName[LF_FACESIZE] = L"";
        int pt = -1;
        wchar_t url[2048] = L"";
        wchar_t *body = wbuf;

        wchar_t *p = wcsstr(wbuf, L"\n");
        if (p) {
            body = p + 1;
            while (*body) {
                wchar_t *eol = wcsstr(body, L"\n");
                size_t llen = eol ? (size_t)(eol - body) : wcslen(body);
                while (llen > 0 && (body[llen-1] == L'\r' ||
                                    body[llen-1] == L' ' ||
                                    body[llen-1] == L'\t'))
                    llen--;
                if (llen == 0) { /* baris kosong = awal body */
                    body = eol ? eol + 1 : body + llen;
                    break;
                }
                if (llen > 5 && _wcsnicmp(body, L"font=", 5) == 0) {
                    size_t n = llen - 5;
                    if (n >= LF_FACESIZE) n = LF_FACESIZE - 1;
                    wcsncpy(fontName, body + 5, n);
                    fontName[n] = 0;
                } else if (llen > 5 && _wcsnicmp(body, L"size=", 5) == 0) {
                    pt = _wtoi(body + 5);
                } else if (llen > 4 && _wcsnicmp(body, L"url=", 4) == 0) {
                    size_t n = llen - 4;
                    if (n >= 2048) n = 2047;
                    wcsncpy(url, body + 4, n);
                    url[n] = 0;
                }
                if (!eol) { body += llen; break; }
                body = eol + 1;
            }
        }
        currentFontIndex = FindFontIndex(fontName);
        if (pt > 0) currentSizeIndex = FindSizeIndex(pt);
        ApplyFontSelection();
        SetSidebarUrl(url);
        if (url[0]) {
            if (!sidebarVisible) ToggleSidebar();
            else NavigateSidebar(url);
        }
        lstrcpynW(currentEncoding, L"EDT", 32);

        suppressChange = TRUE;
        SetWindowTextW(hEdit, body);
        suppressChange = FALSE;
        modified = FALSE;
        free(wbuf);
        return TRUE;
    }

    /* teks biasa: label ulang khusus BOM agar status akurat */
    if (wcsncmp(wbuf, L"\xFEFF", 1) == 0)
        lstrcpynW(currentEncoding, L"UTF-8 BOM", 32);

    suppressChange = TRUE;
    SetWindowTextW(hEdit, wbuf);
    suppressChange = FALSE;
    modified = FALSE;
    free(wbuf);
    return TRUE;
}
