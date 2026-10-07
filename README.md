# Japanese Text Editor (word-c)

Editor teks Win32 (C murni + MinGW/GCC) yang dioptimalkan untuk
Bahasa Jepang: font halus (ClearType), DPI-aware, plus **sidebar web
WebView2 (Chromium)** di sisi kanan untuk riset/baca referensi sambil nulis.

## Fitur saat ini

- Edit multiline (New / Open / Save / Save As / Exit, `Ctrl+N/O/S`)
- Indikator `*` kalau ada perubahan belum disimpan + konfirmasi saat keluar
- Combo font Jepang (Yu Gothic UI, Meiryo, MS Gothic/Mincho, BIZ UD, dll)
- Combo ukuran 8–72 pt, render ClearType + TrueType/OpenType
- Save UTF-8 dengan BOM; load otomatis: UTF-16 LE / UTF-8 BOM /
  UTF-8 tanpa BOM / ANSI (Shift-JIS di locale JP)
- Toolbar: font + size + tombol **Show Web / Hide Web** + **Jisho**
- Sidebar kanan (420px): form URL + tombol **Go** + render WebView2
  - Ketik `google.com` otomatis jadi `https://google.com`
  - `Enter` di URL bar = Go
  - Default awal `https://www.bing.com/`
- Status bar bawah (4 panel): `Ln/Col` caret | jumlah karakter |
  encoding (`UTF-8` / `UTF-8 BOM` / `UTF-16 LE` / `ANSI`) |
  nama file (`*` kalau belum disimpan)
- Lookup Jepang: blok kata (atau letakkan caret di kata) lalu
  klik **Jisho** di toolbar / klik kanan **Lookup di Jisho** / `Ctrl+J`.
  Sidebar otomatis terbuka dan buka `https://jisho.org/search/<kata>`
  (URL-encoded UTF-8). Tanda baca `、。 「」 『』` tidak ikut.

## Struktur folder

```
word-c/
  include/
    common.h          ID, extern global, deklarasi antar modul
    webview2_min.h    Definisi COM WebView2 minimal untuk MinGW
  src/
    main.c            wWinMain (menu, accelerator, message loop)
    core/
      globals.c       Global + daftar font + ukuran
      file_io.c       SaveFileTo() / LoadFileFrom()
      dialogs.c       Open/Save dialog + MaybeSave()
    ui/
      window.c        WndProc (toolbar, layout, command)
      font.c          ApplyFont()
      statusbar.c     SetTitle() + status bar
      lookup.c        Lookup Jepang -> Jisho.org di sidebar
    web/
      sidebar_wv2.c   Sidebar + WebView2 (init/navigate/resize)
  scripts/
    build.bat         Build editor.exe
    get_wv2loader.bat Download WebView2Loader.dll resmi
  docs/
    legacy/           Arsip: editor monolit + sidebar IE lama
  editor.exe          Hasil build
  WebView2Loader.dll  Wajib sejajar editor.exe (x64)
```

## Cara build

Kebutuhan: MinGW-w64 GCC (`gcc --version`), Windows 10/11 64-bit.

```bat
scripts\get_wv2loader.bat
scripts\build.bat
```

`get_wv2loader.bat` mengunduh NuGet `Microsoft.Web.WebView2`
versi pinned dan meng-copy `runtimes/win-x64/native_uap/WebView2Loader.dll`
ke folder `editor.exe`. Manual dari root:

```bat
gcc -municode -DUNICODE -D_UNICODE -O2 -Iinclude -o editor.exe src\core\globals.c src\ui\font.c src\core\file_io.c src\core\dialogs.c src\ui\window.c src\ui\statusbar.c src\ui\lookup.c src\web\sidebar_wv2.c src\main.c -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -lole32 -loleaut32 -luuid -lshlwapi -lshell32
```

> `-municode` wajib (entry `wWinMain`). Tanpa itu linker error `undefined reference to WinMain`.

## Cara pakai

1. Jalankan `editor.exe`
2. Klik **Show Web**, ketik `jisho.org` lalu **Go** / `Enter`
3. Blok kata Jepang -> klik **Jisho** / `Ctrl+J` untuk lookup
4. Resize window: editor menyusut, sidebar tetap di kanan

## Troubleshooting

| Gejala | Penyebab / solusi |
|---|---|
| Popup `WebView2Loader.dll tidak ditemukan` | DLL belum sejajar `editor.exe`. Jalankan `scripts\get_wv2loader.bat` |
| Popup `WebView2 Runtime tidak ditemukan` | Install Edge / WebView2 Runtime Evergreen |
| Klik Go tidak tampil apa-apa | Runtime belum siap; klik Go sekali lagi. Cek `WebView2Loader.dll` x64 (bukan x86) |
| Situs modern rusak | Itu gejala engine IE lama (`docs\legacy\sidebar_ie.c`). Pakai build WebView2 (`src\web\sidebar_wv2.c`) |

## Saran fitur berikutnya

### Selesai
- [x] Status bar: `Ln/Col`, jumlah karakter, encoding file, nama file
- [x] Lookup Jepang: seleksi/kata -> Jisho.org di sidebar (`Jisho`, klik kanan, `Ctrl+J`)
- [x] Struktur folder modular: `src/core`, `src/ui`, `src/web`, `include/`, `scripts/`, `docs/`

### Prioritas tinggi (kecil, dampak besar)
- [ ] Find/Replace (`Ctrl+F` / `Ctrl+H`) untuk dokumen panjang
- [ ] Tombol sidebar: Back / Forward / Reload / Home + `F5` reload
- [ ] Word wrap toggle + zoom `Ctrl+scroll`

### Prioritas menengah
- [ ] Splitter geser untuk lebar sidebar + simpan lebar/URL/font ke `settings.ini`
- [ ] Recent files + drag-and-drop file ke editor + buka file via argumen CLI
- [ ] Autosave/backup (`*.bak`) + restore sesi terakhir
- [ ] Shortcut: `Ctrl+W` toggle sidebar, `Alt+Left` back, `F5` reload web

### Nice to have
- [ ] Export HTML/PDF + Print
- [ ] Dark mode + bold/italic + line number (butuh RichEdit/custom draw)
- [ ] Riwayat URL (dropdown) di sidebar
- [ ] Text-to-speech Jepang untuk cek pelafalan
- [ ] Tab multi-dokumen
