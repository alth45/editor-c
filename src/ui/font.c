#include "common.h"

/* ==== Terapkan font ke edit control ==== */
void ApplyFont(HWND hwnd) {
    if (hCurrentFont) { DeleteObject(hCurrentFont); hCurrentFont = NULL; }

    HDC hdc = GetDC(hwnd);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hwnd, hdc);

    LOGFONTW lf;
    ZeroMemory(&lf, sizeof(lf));
    lf.lfHeight        = -MulDiv(sizes[currentSizeIndex], dpi, 72); /* pt -> px */
    lf.lfWeight        = FW_NORMAL;
    lf.lfCharSet       = DEFAULT_CHARSET;
    lf.lfQuality       = CLEARTYPE_QUALITY;   /* <<< INI KUNCI ANTI "8-BIT" */
    lf.lfOutPrecision  = OUT_TT_PRECIS;       /* pakai TrueType/OpenType */
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfPitchAndFamily= DEFAULT_PITCH | FF_DONTCARE;
    lstrcpynW(lf.lfFaceName, japaneseFonts[currentFontIndex], LF_FACESIZE);

    hCurrentFont = CreateFontIndirectW(&lf);
    if (hEdit) SendMessageW(hEdit, WM_SETFONT, (WPARAM)hCurrentFont, TRUE);
}
