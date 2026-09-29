@echo off
REM Build ulang LALA Chat Overlay sebagai RELEASE.
REM Nol usah buka Visual Studio - jalanin ini aja.
setlocal
cd /d "%~dp0"

echo === Konfigurasi (Release, WAJIB) ===
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto gagal

echo.
echo === Build ===
cmake --build build --config Release --parallel
if errorlevel 1 goto gagal

echo.
echo === Cek: DLL jangan minta CRT debug ===
findstr /M /C:"MSVCP140D.dll" build\Release\yt-chat-overlay.dll >nul 2>&1
if not errorlevel 1 (
  echo GAGAL: DLL masih DEBUG - minta MSVCP140D.dll
  goto gagal
)
echo LULUS: DLL Release, aman dipakai di PC orang lain.

echo.
echo Hasil ada di: build\Release\
echo   yt-chat-overlay.dll
echo   LalaLiveChatOverlay.exe
echo   LalaLiveChatOverlaySetup.exe
goto selesai

:gagal
echo.
echo BUILD GAGAL - baca error di atas.
pause
exit /b 1

:selesai
pause
