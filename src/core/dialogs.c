#include "common.h"

/* ==== Dialog: Open ==== */
BOOL DoOpenFile(void) {
    if (!MaybeSave()) return FALSE;
    wchar_t path[MAX_PATH] = L"";
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hMain;
    ofn.lpstrFilter = L"Editor Files (*.edt)\0*.edt\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"edt";

    if (GetOpenFileNameW(&ofn)) {
        if (LoadFileFrom(path)) {
            lstrcpynW(currentFile, path, MAX_PATH);
            SetTitle();
            SetFocus(hEdit);
        } else {
            MessageBoxW(hMain, L"Gagal membuka file.", L"Error", MB_ICONERROR);
        }
    }
    return TRUE;
}

/* ==== Dialog: Save / Save As ==== */
BOOL DoSaveFile(BOOL saveAs) {
    wchar_t path[MAX_PATH];
    if (saveAs || currentFile[0] == 0) {
        lstrcpynW(path, currentFile, MAX_PATH);
        OPENFILENAMEW ofn;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner   = hMain;
        ofn.lpstrFilter = L"Editor Files (*.edt)\0*.edt\0Text Files (*.txt)\0*.txt\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile   = path;
        ofn.nMaxFile    = MAX_PATH;
        ofn.Flags       = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        ofn.lpstrDefExt = L"edt";
        if (!GetSaveFileNameW(&ofn)) return FALSE;
        lstrcpynW(currentFile, path, MAX_PATH);
    }
    if (SaveFileTo(currentFile)) {
        modified = FALSE;
        SetTitle();
        SetFocus(hEdit);
        return TRUE;
    }
    MessageBoxW(hMain, L"Gagal menyimpan file.", L"Error", MB_ICONERROR);
    return FALSE;
}

/* ==== Konfirmasi jika ada perubahan belum disimpan ==== */
BOOL MaybeSave(void) {
    if (!modified) return TRUE;
    int r = MessageBoxW(hMain, L"Simpan perubahan?", L"Japanese Text Editor",
                        MB_YESNOCANCEL | MB_ICONQUESTION);
    if (r == IDCANCEL) return FALSE;
    if (r == IDNO)     return TRUE;
    return DoSaveFile(FALSE);
}
