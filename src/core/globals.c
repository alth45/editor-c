#include "common.h"

/* ==== Globals ==== */
HWND hMain = NULL, hEdit = NULL, hFontCombo = NULL, hSizeCombo = NULL;
HWND hStatus = NULL;
HFONT hCurrentFont = NULL, hUICtrlFont = NULL;
HACCEL hAccel = NULL;
wchar_t currentFile[MAX_PATH] = L"";
wchar_t currentEncoding[32] = L"UTF-8";
BOOL modified = FALSE;
BOOL suppressChange = FALSE;

/* Sidebar / browser (dibuat di sidebar.c) */
HWND hToggleBtn = NULL, hSidebar = NULL, hUrlEdit = NULL, hGoBtn = NULL, hBrowser = NULL;
HWND hLookupBtn = NULL;
HWND hHistoryCombo = NULL, hHistoryClearBtn = NULL;
BOOL sidebarVisible = FALSE;
int sidebarWidth = SIDEBAR_WIDTH;

/* Daftar font halus yang bagus untuk Bahasa Jepang */
const wchar_t *japaneseFonts[] = {
    L"Yu Gothic UI",          /* default Win10/11 */
    L"Yu Gothic",
    L"Meiryo UI",
    L"Meiryo",
    L"MS UI Gothic",
    L"MS PGothic",
    L"MS Gothic",
    L"Yu Mincho",
    L"MS PMincho",
    L"MS Mincho",
    L"BIZ UD Gothic",         /* UD = universal design, sangat halus */
    L"BIZ UD Mincho",
    L"UD Digi Kyokasho NK-R", /* untuk pembelajaran */
    L"UD Digi Kyokasho N-R",
    L"Segoe UI",
    L"Arial",
    L"Consolas"
};
const int numFonts = (int)(sizeof(japaneseFonts) / sizeof(japaneseFonts[0]));

int sizes[] = {8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 28, 32, 36, 48, 72};
const int numSizes = (int)(sizeof(sizes) / sizeof(sizes[0]));

int currentFontIndex = 0;
int currentSizeIndex = 6; /* = 16 pt */

int FindFontIndex(const wchar_t *name) {
    if (!name || !name[0]) return 0;
    for (int i = 0; i < numFonts; i++)
        if (_wcsicmp(japaneseFonts[i], name) == 0) return i;
    return 0;
}

int FindSizeIndex(int pt) {
    for (int i = 0; i < numSizes; i++)
        if (sizes[i] == pt) return i;
    /* fallback: ukuran terdekat */
    int best = 0, bestDiff = 1000000;
    for (int i = 0; i < numSizes; i++) {
        int d = abs(sizes[i] - pt);
        if (d < bestDiff) { bestDiff = d; best = i; }
    }
    return best;
}
