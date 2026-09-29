@echo off
title Lala Live Chat Overlay - Plugin Updater & Dock Installer
cd /d "%~dp0"

:: Check for Administrator privileges
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Meminta izin Administrator (UAC)...
    powershell -Command "Start-Process cmd.exe -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)

echo ========================================================
echo     LALA LIVE CHAT OVERLAY - OBS PLUGIN & DOCK UPDATE
echo ========================================================
echo.

echo [1/4] Menutup OBS Studio jika sedang berjalan...
taskkill /F /IM obs64.exe >nul 2>&1
taskkill /F /IM obs.exe >nul 2>&1
timeout /t 2 /nobreak >nul

:: Bersihkan folder plugin versi usang agar tidak bentrok
if exist "%APPDATA%\obs-studio\plugins\lala-chat-overlay" rmdir /S /Q "%APPDATA%\obs-studio\plugins\lala-chat-overlay" >nul 2>&1
if exist "C:\ProgramData\obs-studio\plugins\lala-chat-overlay" rmdir /S /Q "C:\ProgramData\obs-studio\plugins\lala-chat-overlay" >nul 2>&1
if exist "C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll" del /F /Q "C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll" >nul 2>&1

echo [2/4] Menyalin ke Program Files (C:\Program Files\obs-studio\obs-plugins\64bit\)...
if exist "C:\Program Files\obs-studio\obs-plugins\64bit" (
    copy /Y "%~dp0yt-chat-overlay.dll" "C:\Program Files\obs-studio\obs-plugins\64bit\yt-chat-overlay.dll"
    if %errorlevel% equ 0 (
        echo [OK] Berhasil disalin ke Program Files.
    ) else (
        echo [INFO] Melewati Program Files (terkunci atau tidak ditemukan).
    )
)

echo [3/4] Menyalin ke ProgramData (C:\ProgramData\obs-studio\plugins\yt-chat-overlay\bin\64bit\)...
set "PROGDATA_DIR=C:\ProgramData\obs-studio\plugins\yt-chat-overlay\bin\64bit"
if not exist "%PROGDATA_DIR%" mkdir "%PROGDATA_DIR%"
copy /Y "%~dp0yt-chat-overlay.dll" "%PROGDATA_DIR%\yt-chat-overlay.dll"
if %errorlevel% equ 0 (
    echo [OK] Berhasil disalin ke ProgramData Plugins.
)

echo [4/4] Menyalin ke AppData User Plugins...
set "APPDATA_DIR=%APPDATA%\obs-studio\plugins\yt-chat-overlay\bin\64bit"
if not exist "%APPDATA_DIR%" mkdir "%APPDATA_DIR%"
copy /Y "%~dp0yt-chat-overlay.dll" "%APPDATA_DIR%\yt-chat-overlay.dll"
if %errorlevel% equ 0 (
    echo [OK] Berhasil disalin ke AppData User Plugins.
)

echo.
echo ========================================================
echo UPDATE SELESAI DENGAN SUKSES!
echo.
echo Sekarang silakan buka OBS Studio:
echo  1. Menu DOCKS  -> Lala Live Chat Overlay  (Dock di samping OBS)
echo  2. Menu TOOLS  -> Lala Live Chat Overlay  (Jendela Pengaturan Lengkap)
echo ========================================================
echo.
pause
