#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _MSC_VER
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

/* ==== IDs ==== */
#define ID_EDIT             101
#define ID_FONT_COMBO       102
#define ID_SIZE_COMBO       103
#define ID_TOGGLE_SIDEBAR   104
#define ID_URL_EDIT         105
#define ID_GO_BTN           106
#define ID_BROWSER          107
#define ID_STATUSBAR        108
#define ID_LOOKUP_BTN       109
#define ID_HISTORY_COMBO    110
#define ID_HISTORY_CLEAR    111
#define ID_POLL_TIMER       1

#define IDM_NEW          201
#define IDM_OPEN         202
#define IDM_SAVE         203
#define IDM_SAVEAS       204
#define IDM_EXIT         205
#define IDM_LOOKUP       206
#define IDM_SELECTALL    207

#define TOOLBAR_HEIGHT   44
#define SIDEBAR_WIDTH    420
/* 2 baris di sidebar: (1) URL + Go, (2) dropdown riwayat + Hapus */
#define URLBAR_HEIGHT    76
#define HISTORY_MAX      20

/* ==== Globals (definisi ada di globals.c) ==== */
extern HWND hMain, hEdit, hFontCombo, hSizeCombo, hStatus;
extern HFONT hCurrentFont, hUICtrlFont;
extern HACCEL hAccel;
extern wchar_t currentFile[MAX_PATH];
extern BOOL modified;
extern BOOL suppressChange;
extern wchar_t currentEncoding[32];

/* Sidebar / browser */
extern HWND hToggleBtn, hSidebar, hUrlEdit, hGoBtn, hBrowser, hLookupBtn;
extern HWND hHistoryCombo, hHistoryClearBtn;
extern BOOL sidebarVisible;
extern int sidebarWidth;

/* Daftar font + ukuran (definisi ada di globals.c) */
extern const wchar_t *japaneseFonts[];
extern const int numFonts;

extern int sizes[];
extern const int numSizes;

extern int currentFontIndex;
extern int currentSizeIndex;

/* Cari index font/size yang valid (fallback 0) */
int FindFontIndex(const wchar_t *name);
int FindSizeIndex(int pt);

/* ==== API antar modul ==== */
void SetTitle(void);
void ApplyFont(HWND hwnd);
void ApplyFontSelection(void);
void CreateStatusBar(HWND parent);
void UpdateStatusBar(void);
int StatusBarHeight(void);

BOOL SaveFileTo(const wchar_t *path);
BOOL LoadFileFrom(const wchar_t *path);
/* URL sesi web (sidebar) untuk disimpan ke .edt */
void GetSidebarUrl(wchar_t *out, int outChars);
void SetSidebarUrl(const wchar_t *url);

BOOL MaybeSave(void);
BOOL DoSaveFile(BOOL saveAs);
BOOL DoOpenFile(void);

/* Sidebar (sidebar.c) */
void CreateSidebar(HWND parent);
void DestroySidebar(void);
void ToggleSidebar(void);
void LayoutMainWindow(void);
void LayoutSidebar(void);
void NavigateSidebar(const wchar_t *url);
void NavigateSidebarFromEdit(void);

/* Riwayat URL di sidebar (sidebar_wv2.c) */
void NavigateHistorySelection(void);
void ClearUrlHistory(void);

/* Lookup Jepang (lookup.c) */
void LookupSelection(void);
void LookupWord(const wchar_t *word);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

