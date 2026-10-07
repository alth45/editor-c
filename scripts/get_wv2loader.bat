@echo off
REM Download WebView2Loader.dll x64 resmi dari NuGet (jalankan dari scripts\)
cd /d %~dp0..
set ROOT=%CD%
set PKG=%ROOT%\wv2.nupkg
set URL=https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2903.40
echo Downloading WebView2 package...
powershell -NoProfile -Command "(New-Object Net.WebClient).DownloadFile('%URL%','%PKG%')"
if errorlevel 1 ( echo DOWNLOAD GAGAL & pause & exit /b 1 )
dir "%PKG%"
copy "%PKG%" "%TEMP%\wv2.zip" /y
powershell -NoProfile -Command "Remove-Item -Recurse -Force $env:TEMP\wv2pkg -ErrorAction SilentlyContinue; Expand-Archive -Force $env:TEMP\wv2.zip $env:TEMP\wv2pkg"
dir "%TEMP%\wv2pkg\runtimes\win-x64\native_uap"
copy "%TEMP%\wv2pkg\runtimes\win-x64\native_uap\WebView2Loader.dll" "%ROOT%\WebView2Loader.dll" /y
dir "%ROOT%\WebView2Loader.dll"
echo SELESAI
pause
