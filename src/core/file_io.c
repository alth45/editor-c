#include "common.h"

/* ==== Save file (UTF-8 dengan BOM) ==== */
BOOL SaveFileTo(const wchar_t *path) {
    int len = GetWindowTextLengthW(hEdit);
    wchar_t *wbuf = (wchar_t *)malloc((len + 1) * sizeof(wchar_t));
    if (!wbuf) return FALSE;
    GetWindowTextW(hEdit, wbuf, len + 1);

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wbuf, len, NULL, 0, NULL, NULL);
    char *u8 = NULL;
    if (u8len > 0) {
        u8 = (char *)malloc(u8len);
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

    wchar_t *wbuf = NULL;
    int wlen = 0;

    if (size >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
        /* UTF-16 LE */
        lstrcpynW(currentEncoding, L"UTF-16 LE", 32);
        wlen = (int)((size - 2) / 2);
        wbuf = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf) { memcpy(wbuf, data + 2, wlen * 2); wbuf[wlen] = 0; }
    } else if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        /* UTF-8 dengan BOM */
        lstrcpynW(currentEncoding, L"UTF-8 BOM", 32);
        int u8len = (int)(size - 3);
        wlen = MultiByteToWideChar(CP_UTF8, 0, (char *)data + 3, u8len, NULL, 0);
        wbuf = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf) {
            MultiByteToWideChar(CP_UTF8, 0, (char *)data + 3, u8len, wbuf, wlen);
            wbuf[wlen] = 0;
        }
    } else {
        /* Coba UTF-8 tanpa BOM dulu, jika gagal -> ANSI/Shift-JIS */
        wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   (char *)data, size, NULL, 0);
        UINT cp = (wlen > 0) ? CP_UTF8 : CP_ACP;
        if (wlen > 0) lstrcpynW(currentEncoding, L"UTF-8", 32);
        else lstrcpynW(currentEncoding, L"ANSI", 32);
        DWORD flags = (wlen > 0) ? MB_ERR_INVALID_CHARS : 0;
        if (wlen <= 0) wlen = MultiByteToWideChar(cp, 0, (char *)data, size, NULL, 0);
        wbuf = (wchar_t *)malloc((wlen + 1) * sizeof(wchar_t));
        if (wbuf) {
            MultiByteToWideChar(cp, flags, (char *)data, size, wbuf, wlen);
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
