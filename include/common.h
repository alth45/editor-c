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

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

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

#define IDM_NEW          201
#define IDM_OPEN         202
#define IDM_SAVE         203
#define IDM_SAVEAS       204
#define IDM_EXIT         205
#define IDM_LOOKUP       206

#define TOOLBAR_HEIGHT   44
#define SIDEBAR_WIDTH    420
#define URLBAR_HEIGHT    44

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
extern BOOL sidebarVisible;
extern int sidebarWidth;

/* Daftar font + ukuran (definisi ada di globals.c) */
extern const wchar_t *japaneseFonts[];
extern const int numFonts;

extern int sizes[];
extern const int numSizes;

extern int currentFontIndex;
extern int currentSizeIndex;

/* ==== API antar modul ==== */
void SetTitle(void);
void ApplyFont(HWND hwnd);
void CreateStatusBar(HWND parent);
void UpdateStatusBar(void);
int StatusBarHeight(void);

BOOL SaveFileTo(const wchar_t *path);
BOOL LoadFileFrom(const wchar_t *path);

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

/* Lookup Jepang (lookup.c) */
void LookupSelection(void);
void LookupWord(const wchar_t *word);

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

