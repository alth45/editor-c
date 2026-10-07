@echo off
REM Build dari root proyek (word-c\)
cd /d %~dp0..
gcc -municode -DUNICODE -D_UNICODE -O2 -Iinclude -o editor.exe src\core\globals.c src\ui\font.c src\core\file_io.c src\core\dialogs.c src\ui\window.c src\ui\statusbar.c src\ui\lookup.c src\web\sidebar_wv2.c src\main.c -lcomctl32 -lcomdlg32 -lgdi32 -luser32 -lole32 -loleaut32 -luuid -lshlwapi -lshell32
if %errorlevel%==0 echo BUILD OK: editor.exe
pause
