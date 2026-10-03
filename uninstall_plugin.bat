@echo off
title Lala Live Chat Overlay - Pembersih & Uninstaller Plugin OBS
cd /d "%~dp0"

:: Cek Hak Akses Administrator
net session >nul 2>&1
if %errorLevel% neq 0 (
    echo Meminta izin Administrator (UAC)...
    powershell -Command "Start-Process cmd.exe -ArgumentList '/c \"\"%~f0\"\"' -Verb RunAs"
    exit /b
)

:MENU
cls
echo ========================================================
echo     PEMBERSIH & UNINSTALLER LALA LIVE CHAT OVERLAY
echo ========================================================
echo.
echo Pilih opsi pembersihan:
echo.
echo  [1] Bersihkan Plugin Lama yang Nyangkut di OBS
echo      (Menghapus sisa versi usang, pasang versi terbaru)
echo.
echo  [2] Hapus Total / Uninstall Penuh
echo      (Hapus semua plugin OBS, aplikasi standalone,
echo       shortcut desktop/start menu, dan registry)
echo.
echo  [3] Buka Folder Plugin OBS di Windows Explorer (Manual)
echo.
echo  [4] Batal / Keluar
echo.
echo ========================================================
set /p opt="Masukkan pilihan (1/2/3/4): "

if "%opt%"=="1" goto CLEAN_OLD
if "%opt%"=="2" goto CLEAN_ALL
if "%opt%"=="3" goto OPEN_FOLDERS
if "%opt%"=="4" exit /b
goto MENU

:CLEAN_OLD
cls
echo ========================================================
echo   MEMBERSIHKAN PLUGIN LAMA YANG NYANGKUT DI OBS...
echo ========================================================
echo.
echo [1/4] Menutup proses aplikasi & OBS Studio...
taskkill /F /IM LalaLiveChatOverlay.exe >nul 2>&1
taskkill /F /IM YouTubeChatOverlay.exe >nul 2>&1
taskkill /F /IM obs64.exe >nul 2>&1
taskkill /F /IM obs.exe >nul 2>&1
taskkill /F /IM obs-browser-page.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [2/4] Menghapus folder versi lama di AppData...
if exist "%APPDATA%\obs-studio\plugins\lala-chat-overlay" (
    rmdir /S /Q "%APPDATA%\obs-studio\plugins\lala-chat-overlay"
    echo  - Menghapus: %APPDATA%\obs-studio\plugins\lala-chat-overlay
)
if exist "%APPDATA%\obs-studio\plugins\lala-chatstream" (
    rmdir /S /Q "%APPDATA%\obs-studio\plugins\lala-chatstream"
    echo  - Menghapus: %APPDATA%\obs-studio\plugins\lala-chatstream
)
if exist "%APPDATA%\obs-studio\plugins\chatstream-obs" (
    rmdir /S /Q "%APPDATA%\obs-studio\plugins\chatstream-obs"
    echo  - Menghapus: %APPDATA%\obs-studio\plugins\chatstream-obs
)

echo [3/4] Menghapus folder versi lama di ProgramData...
if exist "C:\ProgramData\obs-studio\plugins\lala-chat-overlay" (
    rmdir /S /Q "C:\ProgramData\obs-studio\plugins\lala-chat-overlay"
    echo  - Menghapus: C:\ProgramData\obs-studio\plugins\lala-chat-overlay
)
if exist "C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll" (
    del /F /Q "C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll"
    echo  - Menghapus: C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll
)

echo [4/4] Memasang file plugin terbaru ke OBS jika tersedia...
if exist "%~dp0yt-chat-overlay.dll" (
    copy /Y "%~dp0yt-chat-overlay.dll" "C:\Program Files\obs-studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
    if not exist "C:\ProgramData\obs-studio\plugins\yt-chat-overlay\bin\64bit" mkdir "C:\ProgramData\obs-studio\plugins\yt-chat-overlay\bin\64bit"
    copy /Y "%~dp0yt-chat-overlay.dll" "C:\ProgramData\obs-studio\plugins\yt-chat-overlay\bin\64bit\yt-chat-overlay.dll" >nul 2>&1
    if not exist "%APPDATA%\obs-studio\plugins\yt-chat-overlay\bin\64bit" mkdir "%APPDATA%\obs-studio\plugins\yt-chat-overlay\bin\64bit"
    copy /Y "%~dp0yt-chat-overlay.dll" "%APPDATA%\obs-studio\plugins\yt-chat-overlay\bin\64bit\yt-chat-overlay.dll" >nul 2>&1
    echo  - File plugin resmi berhasil diperbarui di seluruh folder OBS.
)

echo.
echo ========================================================
echo PEMBERSIHAN SELESAI!
echo Versi lama yang nyangkut telah berhasil dibersihkan.
echo Buka kembali OBS Studio Anda.
echo ========================================================
echo.
pause
exit /b

:CLEAN_ALL
cls
echo ========================================================
echo   MENGHAPUS TOTAL LALA LIVE CHAT OVERLAY & PLUGIN OBS...
echo ========================================================
echo.
echo [1/6] Menutup semua proses Lala Live Chat & OBS Studio...
taskkill /F /IM LalaLiveChatOverlay.exe >nul 2>&1
taskkill /F /IM YouTubeChatOverlay.exe >nul 2>&1
taskkill /F /IM obs64.exe >nul 2>&1
taskkill /F /IM obs.exe >nul 2>&1
taskkill /F /IM obs-browser-page.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [2/6] Membersihkan AppData Plugins...
rmdir /S /Q "%APPDATA%\obs-studio\plugins\lala-chat-overlay" >nul 2>&1
rmdir /S /Q "%APPDATA%\obs-studio\plugins\lala-chatstream" >nul 2>&1
rmdir /S /Q "%APPDATA%\obs-studio\plugins\chatstream-obs" >nul 2>&1
rmdir /S /Q "%APPDATA%\obs-studio\plugins\yt-chat-overlay" >nul 2>&1
echo  - Selesai membersihkan folder AppData Plugins.

echo [3/6] Membersihkan ProgramData Plugins...
rmdir /S /Q "C:\ProgramData\obs-studio\plugins\lala-chat-overlay" >nul 2>&1
rmdir /S /Q "C:\ProgramData\obs-studio\plugins\yt-chat-overlay" >nul 2>&1
echo  - Selesai membersihkan folder ProgramData Plugins.

echo [4/6] Membersihkan Program Files & Steam OBS Plugins...
del /F /Q "C:\Program Files\obs-studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
del /F /Q "C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll" >nul 2>&1
del /F /Q "C:\Program Files\obs-studio\obs-plugins\yt-chat-overlay.dll" >nul 2>&1
del /F /Q "C:\Program Files (x86)\obs-studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
del /F /Q "C:\Program Files (x86)\Steam\steamapps\common\OBS Studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
del /F /Q "D:\SteamLibrary\steamapps\common\OBS Studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
del /F /Q "E:\SteamLibrary\steamapps\common\OBS Studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
del /F /Q "F:\SteamLibrary\steamapps\common\OBS Studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
del /F /Q "G:\SteamLibrary\steamapps\common\OBS Studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
echo  - Selesai membersihkan folder Program Files & Steam OBS.

echo [5/6] Membersihkan Aplikasi Standalone & Pintasan...
rmdir /S /Q "%LOCALAPPDATA%\LalaLiveChatOverlay" >nul 2>&1
rmdir /S /Q "C:\Program Files\Lala Live Chat Overlay" >nul 2>&1
del /F /Q "%USERPROFILE%\Desktop\Lala Live Chat Overlay.lnk" >nul 2>&1
del /F /Q "%PUBLIC%\Desktop\Lala Live Chat Overlay.lnk" >nul 2>&1
del /F /Q "%APPDATA%\Microsoft\Windows\Start Menu\Programs\Lala Live Chat Overlay.lnk" >nul 2>&1
del /F /Q "%PROGRAMDATA%\Microsoft\Windows\Start Menu\Programs\Lala Live Chat Overlay.lnk" >nul 2>&1
echo  - Selesai membersihkan file aplikasi dan pintasan shortcut.

echo [6/6] Membersihkan Entri Registry Windows...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\LalaLiveChatOverlay" /f >nul 2>&1
reg delete "HKLM\Software\Microsoft\Windows\CurrentVersion\Uninstall\LalaLiveChatOverlay" /f >nul 2>&1
reg delete "HKCU\Software\LalaLiveChatOverlay" /f >nul 2>&1
reg delete "HKLM\Software\LalaLiveChatOverlay" /f >nul 2>&1
echo  - Selesai membersihkan registry.

echo.
echo ========================================================
echo PEMBERSIHAN TOTAL SELESAI TANPA SISA!
echo Seluruh file plugin, aplikasi, shortcut, dan registry
echo berhasil dihapus secara bersih.
echo ========================================================
echo.
pause
exit /b

:OPEN_FOLDERS
explorer "%APPDATA%\obs-studio\plugins"
explorer "C:\ProgramData\obs-studio\plugins"
exit /b
