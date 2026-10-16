#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>
#include <richedit.h>   /* EM_SETREDRAW, EM_GETSELTEXT, CHARRANGE */
#include <stdio.h>
#include <stdlib.h>

/* Fallback bila richedit.h versi SDK tidak menyediakan definisi edit-control */
#ifndef EM_SETREDRAW
#define EM_SETREDRAW (WM_USER + 11)
#endif
#ifndef EM_GETSELTEXT
#define EM_GETSELTEXT (WM_USER + 16)
#endif
/* CHARRANGE: sudah didefinisikan oleh richedit.h modern; fallback hanya
 * untuk SDK/headers lama yang belum punya typedef tersebut. */
#if !defined(RICHEDIT_H) && !defined(_RICHEDIT_) && !defined(__RPC_WIN32__)
#ifndef _CHARRANGE_FALLBACK_
#define _CHARRANGE_FALLBACK_
typedef struct _charrange { LONG cpMin; LONG cpMax; } CHARRANGE;
#endif
#endif

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

/* Dialog Find/Replace (findreplace.c) */
#define IDD_FIND            300
#define IDC_FIND_LABEL      301
#define IDC_FIND_EDIT       302
#define IDC_FIND_NEXT       303
#define IDC_FIND_PREV       304
#define IDC_MATCH_CASE      305
#define IDC_WRAP_LABEL      306
#define IDC_FIND_STATUS     307
#define IDC_REPLACE_LABEL   308
#define IDC_REPLACE_EDIT    309
#define IDC_REPLACE_ONE     310
#define IDC_REPLACE_ALL     311

#define IDM_NEW          201
#define IDM_OPEN         202
#define IDM_SAVE         203
#define IDM_SAVEAS       204
#define IDM_EXIT         205
#define IDM_LOOKUP       206
#define IDM_SELECTALL    207
#define IDM_FIND         208
#define IDM_REPLACE      209

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
int  GetCachedDpiY(void);   /* DPI proses (cached), lihat font.c */
void CreateStatusBar(HWND parent);
void UpdateStatusBar(void);
int StatusBarHeight(void);
/* Reset cache teks panel status (dipakai saat dokumen/file/encoding ganti) */
void InvalidateStatusBarCache(void);

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

/* Find/Replace (findreplace.c) — mode FALSE = Find (Ctrl+F), TRUE = Replace (Ctrl+H) */
void ShowFindDialog(BOOL withReplace);
/* True jika dialog Find/Replace sedang terbuka (dipakai untuk blokir accelerator) */
BOOL FindDialogOpen(void);
/* Handle dialog aktif (NULL kalau tertutup) — utk IsDialogMessageW di message loop */
HWND FindDlgHandle(void);
/* Ulangi pencarian terakhir searah dir (+1 maju / -1 mundur; 0 = arah terakhir)
   — dipakai F3 global saat dialog Find/Replace terbuka */
void FindRepeatLast(int dir);
/* Tutup dialog Find/Replace (dipanggil saat main window ditutup) */
void CloseFindDialog(void);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

