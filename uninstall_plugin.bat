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
echo     PEMBERSIH & UNINSTALLER PLUGIN OBS STUDIO
echo ========================================================
echo.
echo Pilih opsi pembersihan plugin:
echo.
echo  [1] Hapus Plugin Lama (Bersihkan versi lama yang nyangkut,
echo      tetap pertahankan plugin terbaru)
echo.
echo  [2] Hapus Total (Hapus semua file plugin chat overlay dari OBS)
echo.
echo  [3] Buka Folder Plugin OBS di Windows Explorer (Hapus manual)
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
echo [1/3] Menutup OBS Studio...
taskkill /F /IM obs64.exe >nul 2>&1
taskkill /F /IM obs.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [2/3] Menghapus folder versi lama di AppData...
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

echo [3/3] Menghapus folder versi lama di ProgramData...
if exist "C:\ProgramData\obs-studio\plugins\lala-chat-overlay" (
    rmdir /S /Q "C:\ProgramData\obs-studio\plugins\lala-chat-overlay"
    echo  - Menghapus: C:\ProgramData\obs-studio\plugins\lala-chat-overlay
)
if exist "C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll" (
    del /F /Q "C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll"
    echo  - Menghapus: C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll
)

:: Salin DLL versi terbaru ke Program Files, ProgramData, dan AppData
if exist "%~dp0yt-chat-overlay.dll" (
    echo.
    echo Memasang file Lala Live Chat Overlay terbaru ke OBS...
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
echo Versi lama yang nyangkut telah berhasil dihapus.
echo Buka kembali OBS Studio Anda.
echo ========================================================
echo.
pause
exit /b

:CLEAN_ALL
cls
echo ========================================================
echo   MENGHAPUS SEMUA PLUGIN CHAT OVERLAY DARI OBS...
echo ========================================================
echo.
echo [1/4] Menutup OBS Studio...
taskkill /F /IM obs64.exe >nul 2>&1
taskkill /F /IM obs.exe >nul 2>&1
timeout /t 2 /nobreak >nul

echo [2/4] Membersihkan AppData Plugins...
rmdir /S /Q "%APPDATA%\obs-studio\plugins\lala-chat-overlay" >nul 2>&1
rmdir /S /Q "%APPDATA%\obs-studio\plugins\lala-chatstream" >nul 2>&1
rmdir /S /Q "%APPDATA%\obs-studio\plugins\chatstream-obs" >nul 2>&1
rmdir /S /Q "%APPDATA%\obs-studio\plugins\yt-chat-overlay" >nul 2>&1
echo  - Selesai membersihkan folder AppData.

echo [3/4] Membersihkan ProgramData Plugins...
rmdir /S /Q "C:\ProgramData\obs-studio\plugins\lala-chat-overlay" >nul 2>&1
rmdir /S /Q "C:\ProgramData\obs-studio\plugins\yt-chat-overlay" >nul 2>&1
echo  - Selesai membersihkan folder ProgramData.

echo [4/4] Membersihkan Program Files OBS Plugins...
del /F /Q "C:\Program Files\obs-studio\obs-plugins\64bit\yt-chat-overlay.dll" >nul 2>&1
del /F /Q "C:\Program Files\obs-studio\obs-plugins\64bit\lala-chat-overlay.dll" >nul 2>&1
echo  - Selesai membersihkan folder Program Files.

echo.
echo ========================================================
echo SEMUA PLUGIN CHAT BERHASIL DI-UNINSTALL DARI OBS!
echo Jika ingin memasang versi terbaru kembali, jalankan
echo LalaLiveChatOverlaySetup.exe.
echo ========================================================
echo.
pause
exit /b

:OPEN_FOLDERS
explorer "%APPDATA%\obs-studio\plugins"
explorer "C:\ProgramData\obs-studio\plugins"
exit /b
