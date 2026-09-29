#include <windows.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

using namespace Gdiplus;

#define IDI_APP_ICON       101
#define IDR_LOGO_PNG       102
#define IDR_ICON_PNG       103
#define IDR_PLUGIN_DLL     104
#define IDR_APP_EXE        105

static const wchar_t* INSTALLER_WND_CLASS = L"LalaLiveChatOverlayInstallerClass";

class InstallerWizard {
public:
    InstallerWizard() {
        GdiplusStartupInput gsi;
        GdiplusStartup(&m_gdiToken, &gsi, nullptr);

        m_hBgBrush = CreateSolidBrush(RGB(24, 24, 37));        // #181825 Catppuccin Base
        m_hCardBrush = CreateSolidBrush(RGB(30, 30, 46));      // #1e1e2e Surface 0
        m_hBtnBrush = CreateSolidBrush(RGB(49, 50, 68));       // #313244 Surface 1
        m_hAccentBrush = CreateSolidBrush(RGB(137, 180, 250)); // #89b4fa Blue Accent

        m_hFontTitle = CreateFontW(22, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        m_hFontSubtitle = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        m_hFontBold = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        m_hFontNormal = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        m_hFontSmall = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

        HINSTANCE hInst = GetModuleHandleW(nullptr);
        m_pLogoBmp = LoadBitmapFromResource(hInst, IDR_LOGO_PNG);
        if (!m_pLogoBmp) {
            m_pLogoBmp = LoadBitmapFromResource(hInst, IDR_ICON_PNG);
        }
    }

    ~InstallerWizard() {
        if (m_pLogoBmp) delete m_pLogoBmp;
        DeleteObject(m_hBgBrush);
        DeleteObject(m_hCardBrush);
        DeleteObject(m_hBtnBrush);
        DeleteObject(m_hAccentBrush);
        DeleteObject(m_hFontTitle);
        DeleteObject(m_hFontSubtitle);
        DeleteObject(m_hFontBold);
        DeleteObject(m_hFontNormal);
        DeleteObject(m_hFontSmall);
        GdiplusShutdown(m_gdiToken);
    }

    static Bitmap* LoadBitmapFromResource(HMODULE hMod, int resId) {
        HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resId), (LPCWSTR)RT_RCDATA);
        if (!hRes) return nullptr;
        HGLOBAL hData = LoadResource(hMod, hRes);
        if (!hData) return nullptr;
        DWORD size = SizeofResource(hMod, hRes);
        void* ptr = LockResource(hData);
        if (!ptr || size == 0) return nullptr;

        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!hMem) return nullptr;
        void* pMem = GlobalLock(hMem);
        memcpy(pMem, ptr, size);
        GlobalUnlock(hMem);

        IStream* pStream = nullptr;
        Bitmap* bmp = nullptr;
        if (CreateStreamOnHGlobal(hMem, TRUE, &pStream) == S_OK) {
            bmp = Bitmap::FromStream(pStream);
            pStream->Release();
        }
        return bmp;
    }

    static bool ExtractResourceToFile(HMODULE hMod, int resId, const std::wstring& destPath) {
        HRSRC hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resId), (LPCWSTR)RT_RCDATA);
        if (!hRes) return false;
        HGLOBAL hData = LoadResource(hMod, hRes);
        if (!hData) return false;
        DWORD size = SizeofResource(hMod, hRes);
        void* ptr = LockResource(hData);
        if (!ptr || size == 0) return false;

        size_t lastSlash = destPath.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            std::wstring parentDir = destPath.substr(0, lastSlash);
            SHCreateDirectoryExW(nullptr, parentDir.c_str(), nullptr);
        }

        HANDLE hFile = CreateFileW(destPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return false;

        DWORD written = 0;
        BOOL ok = WriteFile(hFile, ptr, size, &written, nullptr);
        CloseHandle(hFile);
        return (ok && written == size);
    }

    std::wstring DetectObsPath() {
        // 1. Program Files
        std::wstring p1 = L"C:\\Program Files\\obs-studio";
        if (IsObsDirValid(p1)) return p1;

        // 2. Program Files (x86)
        std::wstring p2 = L"C:\\Program Files (x86)\\obs-studio";
        if (IsObsDirValid(p2)) return p2;

        // 3. Steam OBS
        std::wstring pSteam = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\OBS Studio";
        if (IsObsDirValid(pSteam)) return pSteam;

        // 4. Registry HKLM
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\OBS Studio", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            wchar_t regPath[MAX_PATH] = {0};
            DWORD bufSize = sizeof(regPath);
            if (RegQueryValueExW(hKey, nullptr, nullptr, nullptr, (LPBYTE)regPath, &bufSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                if (IsObsDirValid(regPath)) return std::wstring(regPath);
            }
            RegCloseKey(hKey);
        }

        // 5. Default fallback
        return L"C:\\Program Files\\obs-studio";
    }

    static bool IsObsDirValid(const std::wstring& path) {
        if (path.empty()) return false;
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) return false;

        std::wstring exe1 = path + L"\\bin\\64bit\\obs64.exe";
        if (GetFileAttributesW(exe1.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

        std::wstring plugins = path + L"\\obs-plugins";
        if (GetFileAttributesW(plugins.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

        return true;
    }

    static std::wstring GetDefaultAppPath() {
        wchar_t progFiles[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, progFiles))) {
            return std::wstring(progFiles) + L"\\LalaLiveChatOverlay";
        }
        wchar_t localApp[MAX_PATH];
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localApp))) {
            return std::wstring(localApp) + L"\\Programs\\LalaLiveChatOverlay";
        }
        return L"C:\\Program Files\\LalaLiveChatOverlay";
    }

    static bool CreateShortcut(const std::wstring& targetPath, const std::wstring& shortcutPath, const std::wstring& description, const std::wstring& iconPath) {
        CoInitialize(nullptr);
        IShellLinkW* psl = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID*)&psl);
        if (SUCCEEDED(hr)) {
            psl->SetPath(targetPath.c_str());
            psl->SetDescription(description.c_str());
            if (!iconPath.empty()) {
                psl->SetIconLocation(iconPath.c_str(), 0);
            }
            IPersistFile* ppf = nullptr;
            hr = psl->QueryInterface(IID_IPersistFile, (LPVOID*)&ppf);
            if (SUCCEEDED(hr)) {
                hr = ppf->Save(shortcutPath.c_str(), TRUE);
                ppf->Release();
            }
            psl->Release();
        }
        return SUCCEEDED(hr);
    }

    static std::wstring BrowseForFolder(HWND hwndOwner, const wchar_t* title) {
        BROWSEINFOW bi = { 0 };
        bi.hwndOwner = hwndOwner;
        bi.lpszTitle = title;
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (pidl != 0) {
            wchar_t path[MAX_PATH];
            if (SHGetPathFromIDListW(pidl, path)) {
                CoTaskMemFree(pidl);
                return std::wstring(path);
            }
            CoTaskMemFree(pidl);
        }
        return L"";
    }

    void ShowWizard() {
        HINSTANCE hInst = GetModuleHandleW(nullptr);

        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = WndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = m_hBgBrush;
        wc.lpszClassName = INSTALLER_WND_CLASS;
        wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_APP_ICON));

        RegisterClassExW(&wc);

        int dlgW = 680;
        int dlgH = 680;
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);

        m_hwnd = CreateWindowExW(
            WS_EX_APPWINDOW,
            INSTALLER_WND_CLASS,
            L"Lala Live Chat Overlay - Installer Wizard",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            (screenW - dlgW) / 2, (screenH - dlgH) / 2, dlgW, dlgH,
            nullptr, nullptr, hInst, this
        );

        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
        InstallerWizard* self = (InstallerWizard*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

        switch (msg) {
            case WM_NCCREATE: {
                CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
                SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
                return DefWindowProcW(hwnd, msg, wParam, lParam);
            }
            case WM_CREATE: {
                if (self) {
                    self->m_hwnd = hwnd;
                    self->CreateControls(hwnd);
                }
                return 0;
            }
            case WM_COMMAND: {
                if (!self) break;
                int id = LOWORD(wParam);

                if (id == 3001) { // Checkbox Install Plugin
                    bool isChecked = (SendMessageW(self->m_chkInstallPlugin, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (isChecked) {
                        SendMessageW(self->m_chkSoftwareOnly, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                    self->UpdatePathValidityUI();
                } else if (id == 3002) { // Checkbox Software Only
                    bool isChecked = (SendMessageW(self->m_chkSoftwareOnly, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (isChecked) {
                        SendMessageW(self->m_chkInstallPlugin, BM_SETCHECK, BST_UNCHECKED, 0);
                    } else {
                        SendMessageW(self->m_chkInstallPlugin, BM_SETCHECK, BST_CHECKED, 0);
                    }
                    self->UpdatePathValidityUI();
                } else if (id == 3011) { // Browse OBS Path
                    std::wstring picked = BrowseForFolder(hwnd, L"Pilih Folder Instalasi OBS Studio Anda (misal: C:\\Program Files\\obs-studio):");
                    if (!picked.empty()) {
                        SetWindowTextW(self->m_editObsPath, picked.c_str());
                        self->UpdatePathValidityUI();
                    }
                } else if (id == 3021) { // Browse App Path
                    std::wstring picked = BrowseForFolder(hwnd, L"Pilih Folder Tujuan Aplikasi Lala Live Chat Overlay:");
                    if (!picked.empty()) {
                        SetWindowTextW(self->m_editAppPath, picked.c_str());
                    }
                } else if (id == 3030) { // Donasi Trakteer
                    ShellExecuteW(nullptr, L"open", L"https://trakteer.id/nopauwxp/gift", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (id == 3031) { // GitHub Updates
                    ShellExecuteW(nullptr, L"open", L"https://github.com/Coding-No", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (id == 3032) { // INSTALL SEKARANG
                    self->ExecuteInstallation();
                } else if (id == 3033) { // Batal / Keluar
                    DestroyWindow(hwnd);
                }
                return 0;
            }
            case WM_PAINT: {
                if (!self) break;
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);
                self->OnPaint(hdc);
                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_CTLCOLORSTATIC: {
                if (!self) break;
                HDC hdc = (HDC)wParam;
                HWND hCtrl = (HWND)lParam;
                SetBkMode(hdc, TRANSPARENT);

                if (hCtrl == self->m_lblStatusObs) {
                    wchar_t txt[128] = {0};
                    GetWindowTextW(hCtrl, txt, 128);
                    if (wcsstr(txt, L"[OK]") != nullptr) {
                        SetTextColor(hdc, RGB(166, 227, 161)); // Green
                    } else {
                        SetTextColor(hdc, RGB(250, 179, 135)); // Amber / Peach
                    }
                    return (LRESULT)self->m_hCardBrush;
                }

                SetTextColor(hdc, RGB(226, 232, 240));
                return (LRESULT)self->m_hBgBrush;
            }
            case WM_DESTROY: {
                PostQuitMessage(0);
                return 0;
            }
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }

    void OnPaint(HDC hdc) {
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

        // 1. Draw Header Background Box
        SolidBrush headerBrush(Color(255, 30, 30, 46));
        g.FillRectangle(&headerBrush, 0, 0, 680, 95);

        Pen borderPen(Color(255, 49, 50, 68), 1.0f);
        g.DrawLine(&borderPen, 0, 95, 680, 95);

        // 2. Draw Banner Logo if available
        if (m_pLogoBmp) {
            int targetH = 54;
            int targetW = (int)((float)m_pLogoBmp->GetWidth() * ((float)targetH / (float)m_pLogoBmp->GetHeight()));
            if (targetW > 180) targetW = 180;
            g.DrawImage(m_pLogoBmp, 25, 20, targetW, targetH);
        }

        // 3. Card Backgrounds for sections
        SolidBrush cardBrush(Color(255, 30, 30, 46));
        // Options Card
        g.FillRectangle(&cardBrush, 25, 110, 615, 120);
        g.DrawRectangle(&borderPen, 25, 110, 615, 120);

        // OBS Path Card
        g.FillRectangle(&cardBrush, 25, 242, 615, 110);
        g.DrawRectangle(&borderPen, 25, 242, 615, 110);

        // Standalone Path Card
        g.FillRectangle(&cardBrush, 25, 364, 615, 95);
        g.DrawRectangle(&borderPen, 25, 364, 615, 95);

        // Status / Log Card
        g.FillRectangle(&cardBrush, 25, 470, 615, 115);
        g.DrawRectangle(&borderPen, 25, 470, 615, 115);
    }

    void CreateControls(HWND hwnd) {
        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

        // Header Titles
        int textX = m_pLogoBmp ? 220 : 30;
        HWND hTitle = CreateWindowW(L"STATIC", L"Lala Live Chat Overlay", WS_CHILD | WS_VISIBLE, textX, 16, 420, 28, hwnd, nullptr, hInst, nullptr);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)m_hFontTitle, TRUE);

        HWND hSub = CreateWindowW(L"STATIC", L"Pemasang Resmi - Chat Game Transparan oleh Coding-No", WS_CHILD | WS_VISIBLE, textX, 48, 420, 20, hwnd, nullptr, hInst, nullptr);
        SendMessageW(hSub, WM_SETFONT, (WPARAM)m_hFontSubtitle, TRUE);

        // CARD 1: OPTIONS
        HWND lblOptHeader = CreateWindowW(L"STATIC", L"1. PILIH KOMPONEN INSTALASI", WS_CHILD | WS_VISIBLE, 40, 118, 400, 20, hwnd, nullptr, hInst, nullptr);
        SendMessageW(lblOptHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

        m_chkInstallPlugin = CreateWindowW(L"BUTTON", L"Pasang Plugin OBS Studio (Integrasi Menu Docks dan Tools OBS)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 45, 142, 570, 22, hwnd, (HMENU)3001, hInst, nullptr);
        m_chkSoftwareOnly = CreateWindowW(L"BUTTON", L"Aplikasi Saja Tanpa Plugin OBS (Jalankan secara Mandiri)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 45, 168, 570, 22, hwnd, (HMENU)3002, hInst, nullptr);

        m_chkDesktopShortcut = CreateWindowW(L"BUTTON", L"Buat Pintasan di Desktop", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 45, 196, 250, 22, hwnd, (HMENU)3003, hInst, nullptr);
        m_chkStartShortcut = CreateWindowW(L"BUTTON", L"Buat Pintasan di Start Menu", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 320, 196, 280, 22, hwnd, (HMENU)3004, hInst, nullptr);

        SendMessageW(m_chkInstallPlugin, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(m_chkSoftwareOnly, BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessageW(m_chkDesktopShortcut, BM_SETCHECK, BST_CHECKED, 0);
        SendMessageW(m_chkStartShortcut, BM_SETCHECK, BST_CHECKED, 0);

        // CARD 2: OBS LOCATION
        HWND lblObsHeader = CreateWindowW(L"STATIC", L"2. FOLDER INSTALASI OBS STUDIO (TERDETEKSI OTOMATIS)", WS_CHILD | WS_VISIBLE, 40, 250, 450, 20, hwnd, nullptr, hInst, nullptr);
        SendMessageW(lblObsHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

        std::wstring defObs = DetectObsPath();
        m_editObsPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defObs.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 45, 276, 450, 26, hwnd, (HMENU)3010, hInst, nullptr);
        m_btnBrowseObs = CreateWindowW(L"BUTTON", L"Pilih Folder...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 505, 275, 120, 28, hwnd, (HMENU)3011, hInst, nullptr);

        m_lblStatusObs = CreateWindowW(L"STATIC", L"Memeriksa lokasi OBS...", WS_CHILD | WS_VISIBLE, 45, 312, 575, 20, hwnd, (HMENU)3012, hInst, nullptr);

        // CARD 3: STANDALONE PATH
        HWND lblAppHeader = CreateWindowW(L"STATIC", L"3. FOLDER APLIKASI UTAMA (SOFTWARE MANDIRI)", WS_CHILD | WS_VISIBLE, 40, 372, 450, 20, hwnd, nullptr, hInst, nullptr);
        SendMessageW(lblAppHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

        std::wstring defApp = GetDefaultAppPath();
        m_editAppPath = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", defApp.c_str(), WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 45, 398, 450, 26, hwnd, (HMENU)3020, hInst, nullptr);
        m_btnBrowseApp = CreateWindowW(L"BUTTON", L"Pilih Folder...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 505, 397, 120, 28, hwnd, (HMENU)3021, hInst, nullptr);

        HWND lblAppHint = CreateWindowW(L"STATIC", L"File LalaLiveChatOverlay.exe akan dipasang ke lokasi ini.", WS_CHILD | WS_VISIBLE, 45, 430, 575, 18, hwnd, nullptr, hInst, nullptr);
        SendMessageW(lblAppHint, WM_SETFONT, (WPARAM)m_hFontSmall, TRUE);

        // CARD 4: LOG STATUS
        HWND lblLogHeader = CreateWindowW(L"STATIC", L"STATUS INSTALASI", WS_CHILD | WS_VISIBLE, 40, 478, 400, 18, hwnd, nullptr, hInst, nullptr);
        SendMessageW(lblLogHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

        m_editLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Siap melakukan instalasi. Klik tombol 'Install Sekarang' untuk memulai.\r\n", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 40, 502, 585, 72, hwnd, nullptr, hInst, nullptr);

        // BOTTOM ACTION BUTTONS
        m_btnDonate = CreateWindowW(L"BUTTON", L"Donasi (Trakteer)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 25, 598, 145, 36, hwnd, (HMENU)3030, hInst, nullptr);
        m_btnGitHub = CreateWindowW(L"BUTTON", L"Update GitHub", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 180, 598, 145, 36, hwnd, (HMENU)3031, hInst, nullptr);

        m_btnInstall = CreateWindowW(L"BUTTON", L"Install Sekarang", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 375, 598, 160, 36, hwnd, (HMENU)3032, hInst, nullptr);
        m_btnCancel = CreateWindowW(L"BUTTON", L"Keluar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 545, 598, 95, 36, hwnd, (HMENU)3033, hInst, nullptr);

        // Apply Fonts
        EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
            InstallerWizard* wizard = (InstallerWizard*)lParam;
            HFONT hFont = wizard->m_hFontNormal;
            SendMessageW(child, WM_SETFONT, (WPARAM)hFont, TRUE);
            return TRUE;
        }, (LPARAM)this);

        SendMessageW(hTitle, WM_SETFONT, (WPARAM)m_hFontTitle, TRUE);
        SendMessageW(hSub, WM_SETFONT, (WPARAM)m_hFontSubtitle, TRUE);
        SendMessageW(lblOptHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
        SendMessageW(lblObsHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
        SendMessageW(lblAppHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
        SendMessageW(lblLogHeader, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
        SendMessageW(m_btnInstall, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
        SendMessageW(lblAppHint, WM_SETFONT, (WPARAM)m_hFontSmall, TRUE);

        UpdatePathValidityUI();
    }

    void UpdatePathValidityUI() {
        wchar_t buf[MAX_PATH] = {0};
        GetWindowTextW(m_editObsPath, buf, MAX_PATH);
        std::wstring path = buf;

        bool isPlugin = (SendMessageW(m_chkInstallPlugin, BM_GETCHECK, 0, 0) == BST_CHECKED);
        if (!isPlugin) {
            SetWindowTextW(m_lblStatusObs, L"[Info] Mode Aplikasi Saja dipilih (Plugin OBS dilewati).");
            EnableWindow(m_editObsPath, FALSE);
            EnableWindow(m_btnBrowseObs, FALSE);
            return;
        }

        EnableWindow(m_editObsPath, TRUE);
        EnableWindow(m_btnBrowseObs, TRUE);

        if (IsObsDirValid(path)) {
            SetWindowTextW(m_lblStatusObs, L"[OK] OBS Studio 64-bit terdeteksi dan siap dipasang.");
        } else {
            SetWindowTextW(m_lblStatusObs, L"[Info] Folder OBS kustom dipilih. File juga akan disalin ke AppData.");
        }
        InvalidateRect(m_lblStatusObs, nullptr, TRUE);
    }

    void AppendLog(const std::wstring& msg) {
        int len = GetWindowTextLengthW(m_editLog);
        SendMessageW(m_editLog, EM_SETSEL, (WPARAM)len, (LPARAM)len);
        std::wstring line = msg + L"\r\n";
        SendMessageW(m_editLog, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    }

    bool SourceFileExists(const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
    }

    std::wstring GetLocalSource(const std::wstring& filename) {
        wchar_t exePath[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring exeDir(exePath);
        size_t lastSlash = exeDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) exeDir = exeDir.substr(0, lastSlash);

        std::wstring p1 = exeDir + L"\\" + filename;
        if (SourceFileExists(p1)) return p1;

        std::wstring p2 = filename;
        if (SourceFileExists(p2)) return p2;

        std::wstring p3 = exeDir + L"\\build\\" + filename;
        if (SourceFileExists(p3)) return p3;

        std::wstring p4 = L"build\\" + filename;
        if (SourceFileExists(p4)) return p4;

        return L"";
    }

    bool DeployFile(int resId, const std::wstring& localName, const std::wstring& destPath) {
        HMODULE hMod = GetModuleHandleW(nullptr);
        // Try extracting embedded resource first (Single-file self-contained mode)
        if (ExtractResourceToFile(hMod, resId, destPath)) {
            return true;
        }

        // Fallback to local files alongside installer (Dev mode)
        std::wstring src = GetLocalSource(localName);
        if (!src.empty()) {
            size_t lastSlash = destPath.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) {
                SHCreateDirectoryExW(nullptr, destPath.substr(0, lastSlash).c_str(), nullptr);
            }
            return (CopyFileW(src.c_str(), destPath.c_str(), FALSE) == TRUE);
        }
        return false;
    }

    void ExecuteInstallation() {
        bool installPlugin = (SendMessageW(m_chkInstallPlugin, BM_GETCHECK, 0, 0) == BST_CHECKED);
        bool installSoftware = (SendMessageW(m_chkSoftwareOnly, BM_GETCHECK, 0, 0) == BST_CHECKED) ||
                               (SendMessageW(m_chkDesktopShortcut, BM_GETCHECK, 0, 0) == BST_CHECKED) ||
                               (SendMessageW(m_chkStartShortcut, BM_GETCHECK, 0, 0) == BST_CHECKED);

        if (!installPlugin && !installSoftware) {
            MessageBoxW(m_hwnd, L"Silakan centang setidaknya salah satu opsi instalasi (Plugin OBS atau Software Standalone).", L"Perhatian", MB_OK | MB_ICONWARNING);
            return;
        }

        EnableWindow(m_btnInstall, FALSE);
        AppendLog(L"=== Memulai Proses Instalasi Lala Live Chat Overlay ===");

        // 1. Close OBS if running
        HWND hObs = FindWindowW(L"OBSWindowClass", nullptr);
        if (hObs) {
            AppendLog(L"[INFO] Menemukan OBS Studio yang sedang aktif...");
            int res = MessageBoxW(m_hwnd, L"OBS Studio terdeteksi sedang berjalan.\nApakah Anda ingin menutup OBS Studio sekarang agar plugin dapat dipasang?", L"Tutup OBS Studio?", MB_YESNO | MB_ICONQUESTION);
            if (res == IDYES) {
                system("taskkill /F /IM obs64.exe >nul 2>&1");
                system("taskkill /F /IM obs.exe >nul 2>&1");
                Sleep(1500);
            }
        }

        wchar_t obsBuf[MAX_PATH] = {0};
        GetWindowTextW(m_editObsPath, obsBuf, MAX_PATH);
        std::wstring obsPath = obsBuf;

        wchar_t appBuf[MAX_PATH] = {0};
        GetWindowTextW(m_editAppPath, appBuf, MAX_PATH);
        std::wstring appPath = appBuf;

        bool pluginSuccess = false;
        bool softwareSuccess = false;

        // 2. Install Plugin
        if (installPlugin) {
            AppendLog(L"[1/2] Memasang OBS Plugin (yt-chat-overlay.dll)...");
            int copies = 0;

            // Target 1: Selected OBS directory
            std::wstring obsPluginsDir = obsPath + L"\\obs-plugins\\64bit";
            if (IsObsDirValid(obsPath)) {
                if (DeployFile(IDR_PLUGIN_DLL, L"yt-chat-overlay.dll", obsPluginsDir + L"\\yt-chat-overlay.dll")) {
                    copies++;
                    AppendLog(L"  -> Berhasil disalin ke OBS Plugins: " + obsPluginsDir);
                }
            }

            // Target 2: ProgramData Plugins (Universal OBS plugin discovery)
            std::wstring progDataDir = L"C:\\ProgramData\\obs-studio\\plugins\\yt-chat-overlay\\bin\\64bit";
            if (DeployFile(IDR_PLUGIN_DLL, L"yt-chat-overlay.dll", progDataDir + L"\\yt-chat-overlay.dll")) {
                copies++;
                AppendLog(L"  -> Berhasil disalin ke ProgramData Plugins: " + progDataDir);
            }

            // Target 3: User AppData Plugins
            wchar_t appData[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
                std::wstring userPluginDir = std::wstring(appData) + L"\\obs-studio\\plugins\\yt-chat-overlay\\bin\\64bit";
                if (DeployFile(IDR_PLUGIN_DLL, L"yt-chat-overlay.dll", userPluginDir + L"\\yt-chat-overlay.dll")) {
                    copies++;
                    AppendLog(L"  -> Berhasil disalin ke User AppData Plugins: " + userPluginDir);
                }
            }

            pluginSuccess = (copies > 0);
            if (pluginSuccess) {
                AppendLog(L"[OK] Plugin OBS berhasil dipasang ke " + std::to_wstring(copies) + L" lokasi.");
            } else {
                AppendLog(L"[ERROR] Gagal memasang plugin. Coba jalankan installer dengan Run as Administrator.");
            }
        }

        // 3. Install Standalone Software
        if (installSoftware) {
            AppendLog(L"[2/2] Memasang Aplikasi Standalone (LalaLiveChatOverlay.exe)...");
            std::wstring exeDest = appPath + L"\\LalaLiveChatOverlay.exe";
            std::wstring dllDest = appPath + L"\\yt-chat-overlay.dll";
            std::wstring icoDest = appPath + L"\\lala_icon.ico";

            SHCreateDirectoryExW(nullptr, appPath.c_str(), nullptr);

            bool exeOk = DeployFile(IDR_APP_EXE, L"YouTubeChatOverlay.exe", exeDest);
            if (!exeOk) {
                exeOk = DeployFile(IDR_APP_EXE, L"LalaLiveChatOverlay.exe", exeDest);
            }
            DeployFile(IDR_PLUGIN_DLL, L"yt-chat-overlay.dll", dllDest);

            // Extract icon file if available
            HMODULE hMod = GetModuleHandleW(nullptr);
            ExtractResourceToFile(hMod, IDI_APP_ICON, icoDest);

            if (exeOk) {
                softwareSuccess = true;
                AppendLog(L"[OK] Aplikasi berhasil dipasang di: " + appPath);

                // Create Shortcuts
                if (SendMessageW(m_chkDesktopShortcut, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    wchar_t desktop[MAX_PATH];
                    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, 0, desktop))) {
                        std::wstring link = std::wstring(desktop) + L"\\Lala Live Chat Overlay.lnk";
                        if (CreateShortcut(exeDest, link, L"Lala Live Chat Overlay - In-Game Live Chat", exeDest)) {
                            AppendLog(L"  -> Pintasan Desktop berhasil dibuat.");
                        }
                    }
                }

                if (SendMessageW(m_chkStartShortcut, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                    wchar_t startMenu[MAX_PATH];
                    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAMS, nullptr, 0, startMenu))) {
                        std::wstring link = std::wstring(startMenu) + L"\\Lala Live Chat Overlay.lnk";
                        if (CreateShortcut(exeDest, link, L"Lala Live Chat Overlay - In-Game Live Chat", exeDest)) {
                            AppendLog(L"  -> Pintasan Start Menu berhasil dibuat.");
                        }
                    }
                }
            } else {
                AppendLog(L"[ERROR] Gagal memasang aplikasi standalone ke " + appPath);
            }
        }

        EnableWindow(m_btnInstall, TRUE);

        bool overallSuccess = (installPlugin ? pluginSuccess : true) && (installSoftware ? softwareSuccess : true);
        if (overallSuccess) {
            AppendLog(L"=== INSTALASI SELESAI DENGAN SUKSES! ===");
            std::wstring successMsg = L"Selamat! Instalasi Lala Live Chat Overlay berhasil diselesaikan.\n\n";
            if (installPlugin) {
                successMsg += L"Untuk OBS Studio:\n"
                              L"1. Buka OBS Studio\n"
                              L"2. Buka menu Docks -> Lala Live Chat Overlay\n"
                              L"3. Atau buka menu Tools -> Lala Live Chat Overlay\n\n";
            }
            if (installSoftware) {
                successMsg += L"Aplikasi Mandiri siap digunakan melalui Pintasan Desktop / Start Menu.\n\n";
            }
            successMsg += L"Apakah Anda ingin membuka OBS Studio sekarang?";

            int choice = MessageBoxW(m_hwnd, successMsg.c_str(), L"Instalasi Berhasil - Lala Live Chat Overlay", MB_YESNO | MB_ICONINFORMATION);
            if (choice == IDYES) {
                std::wstring obsExe = obsPath + L"\\bin\\64bit\\obs64.exe";
                if (SourceFileExists(obsExe)) {
                    ShellExecuteW(nullptr, L"open", obsExe.c_str(), nullptr, (obsPath + L"\\bin\\64bit").c_str(), SW_SHOWNORMAL);
                } else {
                    ShellExecuteW(nullptr, L"open", L"obs64.exe", nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            DestroyWindow(m_hwnd);
        } else {
            MessageBoxW(m_hwnd, L"Instalasi mengalami kendala pada beberapa file.\nPastikan Anda memiliki hak akses administrator dan OBS Studio sedang ditutup.", L"Status Instalasi", MB_OK | MB_ICONWARNING);
        }
    }

    HWND m_hwnd{nullptr};
    ULONG_PTR m_gdiToken{0};
    Bitmap* m_pLogoBmp{nullptr};

    HBRUSH m_hBgBrush{nullptr};
    HBRUSH m_hCardBrush{nullptr};
    HBRUSH m_hBtnBrush{nullptr};
    HBRUSH m_hAccentBrush{nullptr};

    HFONT m_hFontTitle{nullptr};
    HFONT m_hFontSubtitle{nullptr};
    HFONT m_hFontBold{nullptr};
    HFONT m_hFontNormal{nullptr};
    HFONT m_hFontSmall{nullptr};

    HWND m_chkInstallPlugin{nullptr};
    HWND m_chkSoftwareOnly{nullptr};
    HWND m_chkDesktopShortcut{nullptr};
    HWND m_chkStartShortcut{nullptr};

    HWND m_editObsPath{nullptr};
    HWND m_btnBrowseObs{nullptr};
    HWND m_lblStatusObs{nullptr};

    HWND m_editAppPath{nullptr};
    HWND m_btnBrowseApp{nullptr};

    HWND m_editLog{nullptr};

    HWND m_btnDonate{nullptr};
    HWND m_btnGitHub{nullptr};
    HWND m_btnInstall{nullptr};
    HWND m_btnCancel{nullptr};
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    InstallerWizard wizard;
    wizard.ShowWizard();
    return 0;
}
