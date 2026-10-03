#include <windows.h>
#include <shlobj.h>
#include <gdiplus.h>
#include <commctrl.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <functional>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "advapi32.lib")

// Registry key for Add/Remove Programs
static const wchar_t* UNINSTALL_REG_KEY = L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LalaLiveChatOverlay";
static const wchar_t* APP_REG_KEY = L"SOFTWARE\\LalaLiveChatOverlay";

// Forward declaration for uninstall
static void RemoveDirectoryRecursive(const std::wstring& path);

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

    // Helper: Try reading a registry string value
    static std::wstring ReadRegistryString(HKEY root, const wchar_t* subKey, const wchar_t* valueName = nullptr) {
        HKEY hKey;
        if (RegOpenKeyExW(root, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS) return L"";
        wchar_t buf[MAX_PATH] = {0};
        DWORD bufSize = sizeof(buf);
        DWORD type = 0;
        bool ok = (RegQueryValueExW(hKey, valueName, nullptr, &type, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS);
        RegCloseKey(hKey);
        if (ok && (type == REG_SZ || type == REG_EXPAND_SZ)) return std::wstring(buf);
        return L"";
    }

    std::wstring DetectObsPath() {
        // 1. Program Files (most common)
        std::wstring p1 = L"C:\\Program Files\\obs-studio";
        if (IsObsDirValid(p1)) return p1;

        // 2. Program Files (x86)
        std::wstring p2 = L"C:\\Program Files (x86)\\obs-studio";
        if (IsObsDirValid(p2)) return p2;

        // 3. Steam OBS (default library)
        std::wstring pSteam = L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\OBS Studio";
        if (IsObsDirValid(pSteam)) return pSteam;

        // 4. Steam OBS on other common drives (D:, E:, F:)
        const wchar_t* drives[] = { L"D", L"E", L"F", L"G" };
        for (auto drv : drives) {
            std::wstring steamPath = std::wstring(drv) + L":\\SteamLibrary\\steamapps\\common\\OBS Studio";
            if (IsObsDirValid(steamPath)) return steamPath;
        }

        // 5. Registry HKLM (official OBS installer)
        std::wstring regHKLM = ReadRegistryString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\OBS Studio");
        if (!regHKLM.empty() && IsObsDirValid(regHKLM)) return regHKLM;

        // 6. Registry HKCU (per-user OBS install)
        std::wstring regHKCU = ReadRegistryString(HKEY_CURRENT_USER, L"SOFTWARE\\OBS Studio");
        if (!regHKCU.empty() && IsObsDirValid(regHKCU)) return regHKCU;

        // 7. Scoop install
        wchar_t userProfile[MAX_PATH] = {0};
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            std::wstring scoopPath = std::wstring(userProfile) + L"\\scoop\\apps\\obs-studio\\current";
            if (IsObsDirValid(scoopPath)) return scoopPath;
        }

        // 8. Chocolatey install
        std::wstring chocoPath = L"C:\\ProgramData\\chocolatey\\lib\\obs-studio\\tools\\obs-studio";
        if (IsObsDirValid(chocoPath)) return chocoPath;

        // 9. Portable OBS on other drives
        for (auto drv : drives) {
            std::wstring portPath = std::wstring(drv) + L":\\obs-studio";
            if (IsObsDirValid(portPath)) return portPath;
        }

        // 10. Default fallback
        return L"C:\\Program Files\\obs-studio";
    }

    static bool IsObsDirValid(const std::wstring& path) {
        if (path.empty()) return false;
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) return false;

        // OBS 27 and older: bin/64bit/obs64.exe
        std::wstring exe1 = path + L"\\bin\\64bit\\obs64.exe";
        if (GetFileAttributesW(exe1.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

        // OBS 30+: bin/obs64.exe (new flat layout)
        std::wstring exe2 = path + L"\\bin\\obs64.exe";
        if (GetFileAttributesW(exe2.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

        // obs-plugins directory exists
        std::wstring plugins = path + L"\\obs-plugins";
        if (GetFileAttributesW(plugins.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

        // data/obs-plugins directory (OBS 30+ new layout)
        std::wstring dataPlugins = path + L"\\data\\obs-plugins";
        if (GetFileAttributesW(dataPlugins.c_str()) != INVALID_FILE_ATTRIBUTES) return true;

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
                    ShellExecuteW(nullptr, L"open", L"https://github.com/Coding-No/lala-chat-overlay/releases", nullptr, nullptr, SW_SHOWNORMAL);
                } else if (id == 3032) { // INSTALL SEKARANG
                    self->ExecuteInstallation();
                } else if (id == 3033) { // Batal / Keluar
                    DestroyWindow(hwnd);
                } else if (id == 3034) { // COPOT PEMASANGAN (UNINSTALL)
                    self->ExecuteUninstallInteractive();
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

        if (IsAlreadyInstalled()) {
            m_editLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", 
                L"[INFO] Terdeteksi Lala Live Chat Overlay sebelumnya sudah terpasang.\r\n"
                L"  \x2022 Klik 'Install / Update Sekarang' untuk memperbarui file.\r\n"
                L"  \x2022 Klik 'Copot (Uninstall)' untuk menghapus bersih total dari sistem & OBS.\r\n",
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 40, 502, 585, 72, hwnd, nullptr, hInst, nullptr);
        } else {
            m_editLog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", 
                L"Siap melakukan instalasi. Klik tombol 'Install / Update Sekarang' untuk memulai.\r\n", 
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 40, 502, 585, 72, hwnd, nullptr, hInst, nullptr);
        }

        // BOTTOM ACTION BUTTONS
        m_btnDonate = CreateWindowW(L"BUTTON", L"Donasi (Trakteer)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 25, 598, 120, 36, hwnd, (HMENU)3030, hInst, nullptr);
        m_btnUninstall = CreateWindowW(L"BUTTON", L"Copot (Uninstall)", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 155, 598, 150, 36, hwnd, (HMENU)3034, hInst, nullptr);
        m_btnInstall = CreateWindowW(L"BUTTON", L"Install / Update Sekarang", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 315, 598, 205, 36, hwnd, (HMENU)3032, hInst, nullptr);
        m_btnCancel = CreateWindowW(L"BUTTON", L"Keluar", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 530, 598, 95, 36, hwnd, (HMENU)3033, hInst, nullptr);

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
        SendMessageW(m_btnUninstall, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
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

            // Target 1: Selected OBS directory — old-style layout (obs-plugins/64bit/)
            if (IsObsDirValid(obsPath)) {
                std::wstring obsPluginsDir = obsPath + L"\\obs-plugins\\64bit";
                if (DeployFile(IDR_PLUGIN_DLL, L"yt-chat-overlay.dll", obsPluginsDir + L"\\yt-chat-overlay.dll")) {
                    copies++;
                    AppendLog(L"  -> Berhasil disalin ke OBS Plugins: " + obsPluginsDir);
                }

                // Target 1b: OBS 30+ new flat layout (obs-plugins/yt-chat-overlay.dll)
                std::wstring obsFlatDir = obsPath + L"\\obs-plugins";
                DWORD flatAttr = GetFileAttributesW((obsPath + L"\\bin\\obs64.exe").c_str());
                if (flatAttr != INVALID_FILE_ATTRIBUTES) {
                    // This looks like a new-layout OBS, also place in flat directory
                    if (DeployFile(IDR_PLUGIN_DLL, L"yt-chat-overlay.dll", obsFlatDir + L"\\yt-chat-overlay.dll")) {
                        copies++;
                        AppendLog(L"  -> Berhasil disalin ke OBS 30+ flat layout: " + obsFlatDir);
                    }
                }
            }

            // Target 2: ProgramData Plugins (Universal OBS plugin discovery)
            std::wstring progDataDir = L"C:\\ProgramData\\obs-studio\\plugins\\yt-chat-overlay\\bin\\64bit";
            if (DeployFile(IDR_PLUGIN_DLL, L"yt-chat-overlay.dll", progDataDir + L"\\yt-chat-overlay.dll")) {
                copies++;
                AppendLog(L"  -> Berhasil disalin ke ProgramData Plugins: " + progDataDir);
            }

            // Target 3: User AppData Plugins (works without admin, all OBS versions)
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
                AppendLog(L"[INFO] Plugin mendukung OBS 25+ (Qt5), OBS 28+ (Qt6), dan OBS 30+ (layout baru).");
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

        // 4. Copy Installer as Uninstaller & Register in Add/Remove Programs
        bool overallSuccess = (installPlugin ? pluginSuccess : true) && (installSoftware ? softwareSuccess : true);
        if (overallSuccess) {
            RegisterUninstaller(appPath, obsPath, installPlugin);
        }

        EnableWindow(m_btnInstall, TRUE);

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
            successMsg += L"Untuk menghapus: buka Settings -> Apps -> Lala Live Chat Overlay -> Uninstall\n\n";
            successMsg += L"Apakah Anda ingin membuka OBS Studio sekarang?";

            int choice = MessageBoxW(m_hwnd, successMsg.c_str(), L"Instalasi Berhasil - Lala Live Chat Overlay", MB_YESNO | MB_ICONINFORMATION);
            if (choice == IDYES) {
                std::wstring obsExeOld = obsPath + L"\\bin\\64bit\\obs64.exe";
                std::wstring obsExeNew = obsPath + L"\\bin\\obs64.exe";

                if (SourceFileExists(obsExeNew)) {
                    ShellExecuteW(nullptr, L"open", obsExeNew.c_str(), nullptr, (obsPath + L"\\bin").c_str(), SW_SHOWNORMAL);
                } else if (SourceFileExists(obsExeOld)) {
                    ShellExecuteW(nullptr, L"open", obsExeOld.c_str(), nullptr, (obsPath + L"\\bin\\64bit").c_str(), SW_SHOWNORMAL);
                } else {
                    ShellExecuteW(nullptr, L"open", L"obs64.exe", nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
            DestroyWindow(m_hwnd);
        } else {
            MessageBoxW(m_hwnd, L"Instalasi mengalami kendala pada beberapa file.\nPastikan Anda memiliki hak akses administrator dan OBS Studio sedang ditutup.", L"Status Instalasi", MB_OK | MB_ICONWARNING);
        }
    }

    // ================================================================
    // Register in Windows Add/Remove Programs + copy uninstaller
    // ================================================================
    void RegisterUninstaller(const std::wstring& appPath, const std::wstring& obsPath, bool installedPlugin) {
        AppendLog(L"[INFO] Mendaftarkan uninstaller di Windows...");

        // Copy this installer EXE to app folder as uninstaller
        wchar_t selfPath[MAX_PATH] = {0};
        GetModuleFileNameW(nullptr, selfPath, MAX_PATH);
        std::wstring uninstallerPath = appPath + L"\\Uninstall.exe";
        SHCreateDirectoryExW(nullptr, appPath.c_str(), nullptr);
        CopyFileW(selfPath, uninstallerPath.c_str(), FALSE);

        // Save installation info to HKCU registry (no admin needed)
        HKEY hKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, UNINSTALL_REG_KEY, 0, nullptr,
                           REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {

            std::wstring displayName = L"Lala Live Chat Overlay";
            std::wstring publisher = L"Coding-No";
            std::wstring version = L"1.0.0";
            std::wstring uninstallCmd = L"\"" + uninstallerPath + L"\" /uninstall";
            std::wstring iconPath = appPath + L"\\lala_icon.ico";

            RegSetValueExW(hKey, L"DisplayName", 0, REG_SZ, (BYTE*)displayName.c_str(), (DWORD)(displayName.size() + 1) * 2);
            RegSetValueExW(hKey, L"Publisher", 0, REG_SZ, (BYTE*)publisher.c_str(), (DWORD)(publisher.size() + 1) * 2);
            RegSetValueExW(hKey, L"DisplayVersion", 0, REG_SZ, (BYTE*)version.c_str(), (DWORD)(version.size() + 1) * 2);
            RegSetValueExW(hKey, L"UninstallString", 0, REG_SZ, (BYTE*)uninstallCmd.c_str(), (DWORD)(uninstallCmd.size() + 1) * 2);
            RegSetValueExW(hKey, L"DisplayIcon", 0, REG_SZ, (BYTE*)iconPath.c_str(), (DWORD)(iconPath.size() + 1) * 2);
            RegSetValueExW(hKey, L"InstallLocation", 0, REG_SZ, (BYTE*)appPath.c_str(), (DWORD)(appPath.size() + 1) * 2);
            DWORD noModify = 1;
            RegSetValueExW(hKey, L"NoModify", 0, REG_DWORD, (BYTE*)&noModify, sizeof(DWORD));
            RegSetValueExW(hKey, L"NoRepair", 0, REG_DWORD, (BYTE*)&noModify, sizeof(DWORD));

            // Estimate installed size in KB
            DWORD sizeKB = 4096; // ~4MB estimate
            RegSetValueExW(hKey, L"EstimatedSize", 0, REG_DWORD, (BYTE*)&sizeKB, sizeof(DWORD));

            RegCloseKey(hKey);
            AppendLog(L"  -> Terdaftar di Windows 'Apps & Features' (Add/Remove Programs).");
        }

        // Save install paths so uninstaller knows where to clean
        HKEY hAppKey;
        if (RegCreateKeyExW(HKEY_CURRENT_USER, APP_REG_KEY, 0, nullptr,
                           REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hAppKey, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(hAppKey, L"AppPath", 0, REG_SZ, (BYTE*)appPath.c_str(), (DWORD)(appPath.size() + 1) * 2);
            RegSetValueExW(hAppKey, L"ObsPath", 0, REG_SZ, (BYTE*)obsPath.c_str(), (DWORD)(obsPath.size() + 1) * 2);
            DWORD pluginVal = installedPlugin ? 1 : 0;
            RegSetValueExW(hAppKey, L"InstalledPlugin", 0, REG_DWORD, (BYTE*)&pluginVal, sizeof(DWORD));
            RegCloseKey(hAppKey);
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
    HWND m_btnUninstall{nullptr};
    HWND m_btnGitHub{nullptr};
    HWND m_btnInstall{nullptr};
    HWND m_btnCancel{nullptr};

    bool IsAlreadyInstalled();
    void ExecuteUninstallInteractive();
};

// Helper: Recursively remove a directory
static void RemoveDirectoryRecursive(const std::wstring& path) {
    std::wstring searchPath = path + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (wcscmp(fd.cFileName, L".") == 0 || wcscmp(fd.cFileName, L"..") == 0) continue;
        std::wstring fullPath = path + L"\\" + fd.cFileName;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            RemoveDirectoryRecursive(fullPath);
        } else {
            SetFileAttributesW(fullPath.c_str(), FILE_ATTRIBUTE_NORMAL);
            if (!DeleteFileW(fullPath.c_str())) {
                MoveFileExW(fullPath.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    RemoveDirectoryW(path.c_str());
}

static bool PerformCleanUninstall(HWND hwndOwner, bool wipeConfigs, std::function<void(const std::wstring&)> logger) {
    if (logger) logger(L"=== Memulai Proses Pencopotan (Uninstall) Bersih Total ===");

    // 1. Force kill all running processes (app and OBS)
    if (logger) logger(L"[1/6] Menghentikan proses aplikasi dan OBS yang sedang aktif...");
    system("taskkill /F /T /IM LalaLiveChatOverlay.exe >nul 2>&1");
    system("taskkill /F /T /IM YouTubeChatOverlay.exe >nul 2>&1");
    system("taskkill /F /T /IM obs64.exe >nul 2>&1");
    system("taskkill /F /T /IM obs.exe >nul 2>&1");
    system("taskkill /F /T /IM obs-browser-page.exe >nul 2>&1");
    Sleep(1200);

    // 2. Read saved install paths from registry
    std::wstring regAppPath, regObsPath;
    HKEY hAppKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, APP_REG_KEY, 0, KEY_READ, &hAppKey) == ERROR_SUCCESS) {
        wchar_t buf[MAX_PATH] = {0};
        DWORD bufSize = sizeof(buf);
        if (RegQueryValueExW(hAppKey, L"AppPath", nullptr, nullptr, (BYTE*)buf, &bufSize) == ERROR_SUCCESS)
            regAppPath = buf;
        memset(buf, 0, sizeof(buf));
        bufSize = sizeof(buf);
        if (RegQueryValueExW(hAppKey, L"ObsPath", nullptr, nullptr, (BYTE*)buf, &bufSize) == ERROR_SUCCESS)
            regObsPath = buf;
        RegCloseKey(hAppKey);
    }

    // 3. Remove plugin DLL from ALL known and detected OBS locations
    if (logger) logger(L"[2/6] Menghapus plugin OBS dari seluruh lokasi instalasi...");

    auto SafeDeleteFile = [](const std::wstring& path) {
        DWORD attr = GetFileAttributesW(path.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
            SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
            if (!DeleteFileW(path.c_str())) {
                MoveFileExW(path.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
            }
            return true;
        }
        return false;
    };

    // Paths from registry
    if (!regObsPath.empty()) {
        SafeDeleteFile(regObsPath + L"\\obs-plugins\\64bit\\yt-chat-overlay.dll");
        SafeDeleteFile(regObsPath + L"\\obs-plugins\\yt-chat-overlay.dll");
        SafeDeleteFile(regObsPath + L"\\obs-plugins\\64bit\\lala-chat-overlay.dll");
        SafeDeleteFile(regObsPath + L"\\obs-plugins\\lala-chat-overlay.dll");
    }

    // Standard Program Files paths
    SafeDeleteFile(L"C:\\Program Files\\obs-studio\\obs-plugins\\64bit\\yt-chat-overlay.dll");
    SafeDeleteFile(L"C:\\Program Files\\obs-studio\\obs-plugins\\yt-chat-overlay.dll");
    SafeDeleteFile(L"C:\\Program Files\\obs-studio\\obs-plugins\\64bit\\lala-chat-overlay.dll");
    SafeDeleteFile(L"C:\\Program Files (x86)\\obs-studio\\obs-plugins\\64bit\\yt-chat-overlay.dll");
    SafeDeleteFile(L"C:\\Program Files (x86)\\obs-studio\\obs-plugins\\yt-chat-overlay.dll");
    SafeDeleteFile(L"C:\\Program Files (x86)\\obs-studio\\obs-plugins\\64bit\\lala-chat-overlay.dll");

    // Steam library paths across all drives
    const wchar_t* drives[] = { L"C", L"D", L"E", L"F", L"G" };
    for (auto drv : drives) {
        std::wstring s1 = std::wstring(drv) + L":\\SteamLibrary\\steamapps\\common\\OBS Studio\\obs-plugins\\64bit\\yt-chat-overlay.dll";
        std::wstring s2 = std::wstring(drv) + L":\\SteamLibrary\\steamapps\\common\\OBS Studio\\obs-plugins\\yt-chat-overlay.dll";
        std::wstring s3 = std::wstring(drv) + L":\\SteamLibrary\\steamapps\\common\\OBS Studio\\obs-plugins\\64bit\\lala-chat-overlay.dll";
        SafeDeleteFile(s1); SafeDeleteFile(s2); SafeDeleteFile(s3);
    }
    SafeDeleteFile(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\OBS Studio\\obs-plugins\\64bit\\yt-chat-overlay.dll");
    SafeDeleteFile(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\OBS Studio\\obs-plugins\\yt-chat-overlay.dll");

    // Scoop path
    wchar_t userProfile[MAX_PATH] = {0};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        std::wstring scoopPath = std::wstring(userProfile) + L"\\scoop\\apps\\obs-studio\\current\\obs-plugins\\64bit\\yt-chat-overlay.dll";
        SafeDeleteFile(scoopPath);
    }

    // ProgramData plugins
    RemoveDirectoryRecursive(L"C:\\ProgramData\\obs-studio\\plugins\\yt-chat-overlay");
    RemoveDirectoryRecursive(L"C:\\ProgramData\\obs-studio\\plugins\\lala-chat-overlay");

    // AppData user plugins
    wchar_t appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        RemoveDirectoryRecursive(std::wstring(appData) + L"\\obs-studio\\plugins\\yt-chat-overlay");
        RemoveDirectoryRecursive(std::wstring(appData) + L"\\obs-studio\\plugins\\lala-chat-overlay");
        RemoveDirectoryRecursive(std::wstring(appData) + L"\\obs-studio\\plugins\\lala-chatstream");
        RemoveDirectoryRecursive(std::wstring(appData) + L"\\obs-studio\\plugins\\chatstream-obs");
    }
    if (logger) logger(L"  -> Plugin OBS selesai dibersihkan dari seluruh lokasi.");

    // 4. Remove standalone app files
    if (logger) logger(L"[3/6] Menghapus file aplikasi standalone...");
    std::vector<std::wstring> possibleAppPaths;
    if (!regAppPath.empty()) possibleAppPaths.push_back(regAppPath);
    possibleAppPaths.push_back(InstallerWizard::GetDefaultAppPath());
    wchar_t progFiles[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, progFiles))) {
        possibleAppPaths.push_back(std::wstring(progFiles) + L"\\LalaLiveChatOverlay");
    }
    wchar_t localApp[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localApp))) {
        possibleAppPaths.push_back(std::wstring(localApp) + L"\\Programs\\LalaLiveChatOverlay");
    }

    wchar_t selfExe[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, selfExe, MAX_PATH);

    for (const auto& ap : possibleAppPaths) {
        if (ap.empty()) continue;
        DWORD attr = GetFileAttributesW(ap.c_str());
        if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY)) continue;

        SafeDeleteFile(ap + L"\\LalaLiveChatOverlay.exe");
        SafeDeleteFile(ap + L"\\YouTubeChatOverlay.exe");
        SafeDeleteFile(ap + L"\\yt-chat-overlay.dll");
        SafeDeleteFile(ap + L"\\lala_icon.ico");
        SafeDeleteFile(ap + L"\\config\\overlay_config.json");
        RemoveDirectoryW((ap + L"\\config").c_str());

        // If self is NOT in this folder, delete Uninstall.exe and folder directly
        std::wstring uninst = ap + L"\\Uninstall.exe";
        if (_wcsicmp(selfExe, uninst.c_str()) != 0) {
            SafeDeleteFile(uninst);
            RemoveDirectoryRecursive(ap);
        }
    }
    if (logger) logger(L"  -> File aplikasi standalone selesai dibersihkan.");

    // 5. Remove all shortcuts (Desktop & Start Menu, both user & public)
    if (logger) logger(L"[4/6] Menghapus pintasan di Desktop & Start Menu...");
    wchar_t folderPath[MAX_PATH];
    const int folders[] = {
        CSIDL_DESKTOPDIRECTORY,
        CSIDL_COMMON_DESKTOPDIRECTORY,
        CSIDL_PROGRAMS,
        CSIDL_COMMON_PROGRAMS
    };
    for (int f : folders) {
        if (SUCCEEDED(SHGetFolderPathW(nullptr, f, nullptr, 0, folderPath))) {
            DeleteFileW((std::wstring(folderPath) + L"\\Lala Live Chat Overlay.lnk").c_str());
            DeleteFileW((std::wstring(folderPath) + L"\\YouTube Chat Overlay.lnk").c_str());
        }
    }
    if (logger) logger(L"  -> Seluruh pintasan berhasil dihapus.");

    // 6. Clean Windows Registry
    if (logger) logger(L"[5/6] Membersihkan entri registri Windows...");
    RegDeleteKeyW(HKEY_CURRENT_USER, UNINSTALL_REG_KEY);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\YouTubeChatOverlay");
    RegDeleteKeyW(HKEY_CURRENT_USER, APP_REG_KEY);
    RegDeleteKeyW(HKEY_CURRENT_USER, L"SOFTWARE\\YouTubeChatOverlay");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, UNINSTALL_REG_KEY);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\YouTubeChatOverlay");
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, APP_REG_KEY);
    RegDeleteKeyW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\YouTubeChatOverlay");
    if (logger) logger(L"  -> Registri Windows berhasil dibersihkan.");

    // 7. Wipe config and logs if requested
    if (wipeConfigs) {
        if (logger) logger(L"[6/6] Membersihkan seluruh data konfigurasi dan riwayat log...");
        if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
            RemoveDirectoryRecursive(std::wstring(appData) + L"\\LalaLiveChatOverlay");
            RemoveDirectoryRecursive(std::wstring(appData) + L"\\obs-studio\\plugin_config\\yt-chat-overlay");
        }
        if (logger) logger(L"  -> Konfigurasi dan riwayat log berhasil dibersihkan total.");
    } else {
        if (logger) logger(L"[6/6] Konfigurasi pengguna dipertahankan (untuk kebutuhan instalasi ulang).");
    }

    // 8. If running as Uninstall.exe inside app folder, schedule self-delete
    for (const auto& ap : possibleAppPaths) {
        std::wstring uninst = ap + L"\\Uninstall.exe";
        if (_wcsicmp(selfExe, uninst.c_str()) == 0) {
            std::wstring cmd = L"cmd /c timeout /t 2 /nobreak >nul & del /f /q \"" + uninst + L"\" & rmdir /s /q \"" + ap + L"\"";
            STARTUPINFOW si = { sizeof(si) };
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            PROCESS_INFORMATION pi = {0};
            CreateProcessW(nullptr, (LPWSTR)cmd.c_str(), nullptr, nullptr, FALSE,
                          CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
            if (pi.hProcess) CloseHandle(pi.hProcess);
            if (pi.hThread) CloseHandle(pi.hThread);
            break;
        }
    }

    if (logger) logger(L"=== PENCAPOTAN (UNINSTALL) SELESAI SECARA BERSIH TOTAL! ===");
    return true;
}

bool InstallerWizard::IsAlreadyInstalled() {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, UNINSTALL_REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    std::wstring appPath = GetDefaultAppPath();
    if (SourceFileExists(appPath + L"\\LalaLiveChatOverlay.exe") ||
        SourceFileExists(appPath + L"\\YouTubeChatOverlay.exe")) {
        return true;
    }
    wchar_t appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        if (SourceFileExists(std::wstring(appData) + L"\\obs-studio\\plugins\\yt-chat-overlay\\bin\\64bit\\yt-chat-overlay.dll"))
            return true;
    }
    if (SourceFileExists(L"C:\\ProgramData\\obs-studio\\plugins\\yt-chat-overlay\\bin\\64bit\\yt-chat-overlay.dll"))
        return true;
    if (SourceFileExists(L"C:\\Program Files\\obs-studio\\obs-plugins\\64bit\\yt-chat-overlay.dll"))
        return true;
    return false;
}

void InstallerWizard::ExecuteUninstallInteractive() {
    int res = MessageBoxW(m_hwnd,
        L"Apakah Anda yakin ingin mencopot (uninstall) Lala Live Chat Overlay\n"
        L"dan membersihkan seluruh plugin dari OBS Studio?\n\n"
        L"Semua file aplikasi, plugin OBS dari seluruh lokasi, pintasan, dan registri akan dihapus bersih.",
        L"Konfirmasi Uninstall - Lala Live Chat Overlay",
        MB_YESNO | MB_ICONQUESTION);
    if (res != IDYES) return;

    int wipeRes = MessageBoxW(m_hwnd,
        L"Apakah Anda juga ingin menghapus seluruh data konfigurasi dan riwayat log\n"
        L"agar komputer Anda bersih total 100% tanpa sisa?\n\n"
        L"[YES] Hapus Bersih Total (termasuk config & log)\n"
        L"[NO] Tetap simpan konfigurasi untuk jika nanti install lagi",
        L"Hapus Konfigurasi Pengguna?",
        MB_YESNO | MB_ICONQUESTION);
    bool wipeConfigs = (wipeRes == IDYES);

    EnableWindow(m_btnInstall, FALSE);
    EnableWindow(m_btnUninstall, FALSE);

    PerformCleanUninstall(m_hwnd, wipeConfigs, [this](const std::wstring& msg) {
        AppendLog(msg);
    });

    EnableWindow(m_btnInstall, TRUE);
    EnableWindow(m_btnUninstall, TRUE);

    MessageBoxW(m_hwnd,
        L"Lala Live Chat Overlay dan Plugin OBS telah berhasil dicopot secara bersih total!\n"
        L"Semua proses dan file yang terkait telah dibersihkan tanpa ada yang tersisa atau nyangkut.",
        L"Uninstall Selesai", MB_OK | MB_ICONINFORMATION);
}

static void ExecuteUninstall() {
    int res = MessageBoxW(nullptr,
        L"Apakah Anda yakin ingin mencopot (uninstall) Lala Live Chat Overlay?\n\n"
        L"Semua file aplikasi, plugin OBS dari seluruh folder, pintasan, dan registri akan dihapus bersih.",
        L"Uninstall Lala Live Chat Overlay", MB_YESNO | MB_ICONQUESTION);
    if (res != IDYES) return;

    int wipeRes = MessageBoxW(nullptr,
        L"Apakah Anda juga ingin menghapus seluruh data konfigurasi dan riwayat log\n"
        L"agar komputer Anda bersih total 100% tanpa sisa?\n\n"
        L"[YES] Hapus Bersih Total (termasuk config & log)\n"
        L"[NO] Tetap simpan konfigurasi untuk jika nanti install lagi",
        L"Hapus Konfigurasi Pengguna?",
        MB_YESNO | MB_ICONQUESTION);
    bool wipeConfigs = (wipeRes == IDYES);

    PerformCleanUninstall(nullptr, wipeConfigs, [](const std::wstring& msg) {
        OutputDebugStringW((msg + L"\n").c_str());
    });

    MessageBoxW(nullptr,
        L"Lala Live Chat Overlay berhasil dihapus secara bersih total!",
        L"Uninstall Selesai", MB_OK | MB_ICONINFORMATION);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Check if launched in uninstall mode
    if (lpCmdLine && (strstr(lpCmdLine, "/uninstall") || strstr(lpCmdLine, "-uninstall"))) {
        ExecuteUninstall();
        return 0;
    }

    InstallerWizard wizard;
    wizard.ShowWizard();
    return 0;
}
