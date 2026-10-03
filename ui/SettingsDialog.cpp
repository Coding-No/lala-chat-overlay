#include "SettingsDialog.hpp"
#include <shellapi.h>
#include <commctrl.h>
#include <urlmon.h>
#include <wininet.h>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "wininet.lib")

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

static const wchar_t* SETTINGS_WND_CLASS = L"LalaChatOverlaySettingsClass";
static COLORREF g_customColors[16] = {0};

static Gdiplus::Bitmap* LoadBitmapFromResource(HMODULE hMod, int resId) {
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
    Gdiplus::Bitmap* bmp = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &pStream) == S_OK) {
        bmp = Gdiplus::Bitmap::FromStream(pStream);
        pStream->Release();
    }
    return bmp;
}

static const wchar_t* StatusToIndonesian(ChatProviderStatus status) {
    switch (status) {
        case ChatProviderStatus::Disconnected: return L"Terputus";
        case ChatProviderStatus::Connecting:   return L"Menghubungkan...";
        case ChatProviderStatus::Connected:    return L"Terhubung (Aktif)";
        case ChatProviderStatus::Reconnecting: return L"Menghubungkan Ulang...";
        case ChatProviderStatus::Error:        return L"Gangguan Jaringan";
        default:                               return L"Tidak Diketahui";
    }
}

SettingsDialog::SettingsDialog(OverlayController& controller) 
    : m_controller(controller) {
    m_config = controller.GetConfig();

    Gdiplus::GdiplusStartupInput gsi;
    Gdiplus::GdiplusStartup(&m_gdiToken, &gsi, nullptr);

    m_hBgBrush   = CreateSolidBrush(RGB(19, 20, 31));       // Deep obsidian dark base
    m_hCardBrush = CreateSolidBrush(RGB(28, 30, 46));      // Card surface
    m_hEditBrush = CreateSolidBrush(RGB(22, 23, 36));      // Dark edit input box

    m_hFontNormal  = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_hFontBold    = CreateFontW(14, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_hFontTitle   = CreateFontW(19, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_hFontSmall   = CreateFontW(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_hFontSection = CreateFontW(13, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    HINSTANCE hInst = (HINSTANCE)&__ImageBase;
    m_pLogoBmp = LoadBitmapFromResource(hInst, 102);
    if (!m_pLogoBmp) {
        m_pLogoBmp = LoadBitmapFromResource(hInst, 103);
    }
    if (!m_pLogoBmp) {
        m_pLogoBmp = Gdiplus::Bitmap::FromFile(L"assets\\png\\lala_logo_horizontal_transparent_1200x400.png");
    }
    if (!m_pLogoBmp || m_pLogoBmp->GetLastStatus() != Gdiplus::Ok) {
        if (m_pLogoBmp) { delete m_pLogoBmp; m_pLogoBmp = nullptr; }
        m_pLogoBmp = Gdiplus::Bitmap::FromFile(L"assets\\png\\lala_icon_app_64.png");
    }
}

SettingsDialog::~SettingsDialog() {
    Hide();
    if (m_running) {
        m_running = false;
        if (m_hwnd && IsWindow(m_hwnd)) {
            PostMessageW(m_hwnd, WM_USER + 999, 0, 0);
        }
        if (m_uiThread.joinable()) {
            PostThreadMessageW(GetThreadId(m_uiThread.native_handle()), WM_QUIT, 0, 0);
            m_uiThread.join();
        }
    }
    if (m_pLogoBmp) {
        delete m_pLogoBmp;
        m_pLogoBmp = nullptr;
    }
    DeleteObject(m_hBgBrush);
    DeleteObject(m_hCardBrush);
    DeleteObject(m_hEditBrush);
    DeleteObject(m_hFontNormal);
    DeleteObject(m_hFontBold);
    DeleteObject(m_hFontTitle);
    DeleteObject(m_hFontSmall);
    DeleteObject(m_hFontSection);
    if (m_gdiToken) {
        Gdiplus::GdiplusShutdown(m_gdiToken);
        m_gdiToken = 0;
    }
}

bool SettingsDialog::Show(HWND parentHwnd) {
    m_config = m_controller.GetConfig();

    if (m_running && m_hwnd && IsWindow(m_hwnd)) {
        SendMessageW(m_hwnd, WM_SYNC_CONFIG, 0, 0);
        ShowWindow(m_hwnd, SW_SHOW);
        SetForegroundWindow(m_hwnd);
        return true;
    }

    if (m_uiThread.joinable()) {
        m_running = false;
        if (m_hwnd && IsWindow(m_hwnd)) {
            PostMessageW(m_hwnd, WM_USER + 999, 0, 0);
        }
        PostThreadMessageW(GetThreadId(m_uiThread.native_handle()), WM_QUIT, 0, 0);
        m_uiThread.join();
    }

    m_running = true;
    m_ready = false;
    m_parentHwnd = (parentHwnd && IsWindow(parentHwnd)) ? parentHwnd : nullptr;

    m_uiThread = std::thread(&SettingsDialog::MessageLoop, this);

    auto startTime = std::chrono::steady_clock::now();
    while (!m_ready && m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
        if (elapsed > 3000) break;
    }

    return (m_hwnd != nullptr && IsWindow(m_hwnd));
}

void SettingsDialog::Hide() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void SettingsDialog::MessageLoop() {
    INITCOMMONCONTROLSEX icce = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icce);

    HINSTANCE hInst = (HINSTANCE)&__ImageBase;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = DialogProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_hBgBrush;
    wc.lpszClassName = SETTINGS_WND_CLASS;
    wc.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(101));

    RegisterClassExW(&wc);

    int dlgW = 600;
    int dlgH = 840;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int dlgX = (screenW - dlgW) / 2;
    int dlgY = (screenH - dlgH) / 2;

    m_hwnd = CreateWindowExW(
        WS_EX_APPWINDOW | WS_EX_WINDOWEDGE,
        SETTINGS_WND_CLASS,
        L"Lala Live Chat Overlay - Pengaturan & Kontrol",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        dlgX, dlgY, dlgW, dlgH,
        m_parentHwnd, nullptr, hInst, this
    );

    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
        SetForegroundWindow(m_hwnd);
    }

    m_ready = true;

    MSG msg;
    while (m_running && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (m_hwnd && IsWindow(m_hwnd)) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_running = false;
}

void SettingsDialog::UpdateStatusText(ChatProviderStatus status, const std::string& info) {
    if (!m_lblStatus || !IsWindow(m_lblStatus)) return;
    std::wstring text = L"Status: " + std::wstring(StatusToIndonesian(status));
    if (!info.empty()) {
        std::wstring wInfo(info.begin(), info.end());
        text += L" (" + wInfo + L")";
    }
    SetWindowTextW(m_lblStatus, text.c_str());
}

void SettingsDialog::UpdateMessageCount(uint64_t count) {
    if (!m_lblMessages || !IsWindow(m_lblMessages)) return;
    std::wstring text = L"Pesan: " + std::to_wstring(count);
    SetWindowTextW(m_lblMessages, text.c_str());
}

void SettingsDialog::CreateControls(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);

    // Top Header Banner
    m_hHeaderTitle = CreateWindowW(L"STATIC", L"Lala Live Chat Overlay", WS_CHILD | WS_VISIBLE, 185, 18, 380, 26, hwnd, nullptr, hInst, nullptr);
    m_hHeaderSub = CreateWindowW(L"STATIC", L"v1.0.0 Resmi \x2022 Dikembangkan oleh Coding-No", WS_CHILD | WS_VISIBLE, 185, 45, 380, 20, hwnd, nullptr, hInst, nullptr);

    // Section 1: Stream Connection (y=84 to 206)
    m_lblSec1 = CreateWindowW(L"STATIC", L"\x25CF KONEKSI LIVE STREAM", WS_CHILD | WS_VISIBLE, 26, 92, 300, 18, hwnd, nullptr, hInst, nullptr);
    CreateWindowW(L"STATIC", L"YouTube Live Stream URL:", WS_CHILD | WS_VISIBLE, 26, 114, 250, 18, hwnd, nullptr, hInst, nullptr);
    m_hEditUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 26, 134, 532, 26, hwnd, nullptr, hInst, nullptr);

    m_btnStart = CreateWindowW(L"BUTTON", L"START", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 26, 168, 70, 30, hwnd, (HMENU)1001, hInst, nullptr);
    m_btnStop = CreateWindowW(L"BUTTON", L"STOP", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 100, 168, 68, 30, hwnd, (HMENU)1002, hInst, nullptr);
    m_btnPreview = CreateWindowW(L"BUTTON", L"PREVIEW", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 172, 168, 86, 30, hwnd, (HMENU)1003, hInst, nullptr);
    m_btnSaveConfig = CreateWindowW(L"BUTTON", L"SIMPAN", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 262, 168, 84, 30, hwnd, (HMENU)1004, hInst, nullptr);
    m_btnLoadConfig = CreateWindowW(L"BUTTON", L"LOAD", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 350, 168, 80, 30, hwnd, (HMENU)1005, hInst, nullptr);

    m_lblStatus = CreateWindowW(L"STATIC", L"Status: Terputus", WS_CHILD | WS_VISIBLE, 436, 166, 122, 16, hwnd, nullptr, hInst, nullptr);
    m_lblMessages = CreateWindowW(L"STATIC", L"Pesan: 0", WS_CHILD | WS_VISIBLE, 436, 184, 122, 16, hwnd, nullptr, hInst, nullptr);

    // Section 2: Appearance (y=216 to 484)
    m_lblSec2 = CreateWindowW(L"STATIC", L"\x25CF PENGATURAN TAMPILAN (APPEARANCE)", WS_CHILD | WS_VISIBLE, 26, 222, 400, 18, hwnd, nullptr, hInst, nullptr);

    // Font Family
    CreateWindowW(L"STATIC", L"Font:", WS_CHILD | WS_VISIBLE, 26, 246, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_cbFont = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 95, 242, 165, 200, hwnd, (HMENU)1010, hInst, nullptr);
    SendMessageW(m_cbFont, CB_ADDSTRING, 0, (LPARAM)L"Segoe UI");
    SendMessageW(m_cbFont, CB_ADDSTRING, 0, (LPARAM)L"Segoe UI Emoji");
    SendMessageW(m_cbFont, CB_ADDSTRING, 0, (LPARAM)L"Arial");
    SendMessageW(m_cbFont, CB_ADDSTRING, 0, (LPARAM)L"Roboto");
    SendMessageW(m_cbFont, CB_ADDSTRING, 0, (LPARAM)L"Consolas");
    SendMessageW(m_cbFont, CB_ADDSTRING, 0, (LPARAM)L"Verdana");
    SendMessageW(m_cbFont, CB_ADDSTRING, 0, (LPARAM)L"Tahoma");

    // Font Size
    CreateWindowW(L"STATIC", L"Font Size:", WS_CHILD | WS_VISIBLE, 285, 246, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_sliderFontSize = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 365, 241, 140, 26, hwnd, (HMENU)1011, hInst, nullptr);
    SendMessageW(m_sliderFontSize, TBM_SETRANGE, TRUE, MAKELPARAM(10, 36));
    m_lblFontSizeVal = CreateWindowW(L"STATIC", L"16px", WS_CHILD | WS_VISIBLE, 515, 246, 45, 20, hwnd, nullptr, hInst, nullptr);

    // Position
    CreateWindowW(L"STATIC", L"Position:", WS_CHILD | WS_VISIBLE, 26, 278, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_cbPosition = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 95, 274, 165, 250, hwnd, (HMENU)1012, hInst, nullptr);
    const wchar_t* positions[] = {
        L"Top Left", L"Top Center", L"Top Right",
        L"Middle Left", L"Middle Center", L"Middle Right",
        L"Bottom Left", L"Bottom Center", L"Bottom Right"
    };
    for (const auto* p : positions) SendMessageW(m_cbPosition, CB_ADDSTRING, 0, (LPARAM)p);

    // Opacity
    CreateWindowW(L"STATIC", L"Opacity:", WS_CHILD | WS_VISIBLE, 285, 278, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_sliderOpacity = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 365, 273, 140, 26, hwnd, (HMENU)1013, hInst, nullptr);
    SendMessageW(m_sliderOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(20, 100));
    m_lblOpacityVal = CreateWindowW(L"STATIC", L"95%", WS_CHILD | WS_VISIBLE, 515, 278, 45, 20, hwnd, nullptr, hInst, nullptr);

    // Duration
    CreateWindowW(L"STATIC", L"Duration:", WS_CHILD | WS_VISIBLE, 26, 310, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_cbDuration = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 95, 306, 165, 200, hwnd, (HMENU)1014, hInst, nullptr);
    const wchar_t* durations[] = { L"3 sec", L"5 sec", L"10 sec", L"15 sec", L"30 sec", L"60 sec", L"Selamanya" };
    for (const auto* d : durations) SendMessageW(m_cbDuration, CB_ADDSTRING, 0, (LPARAM)d);

    // Max Chat
    CreateWindowW(L"STATIC", L"Max Chat:", WS_CHILD | WS_VISIBLE, 285, 310, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_cbMaxMessages = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 365, 306, 140, 200, hwnd, (HMENU)1015, hInst, nullptr);
    const wchar_t* maxCounts[] = { L"3", L"5", L"8", L"10", L"15", L"20" };
    for (const auto* c : maxCounts) SendMessageW(m_cbMaxMessages, CB_ADDSTRING, 0, (LPARAM)c);

    // X Offset
    CreateWindowW(L"STATIC", L"X Offset:", WS_CHILD | WS_VISIBLE, 26, 342, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_sliderOffsetX = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 95, 337, 120, 26, hwnd, (HMENU)1016, hInst, nullptr);
    SendMessageW(m_sliderOffsetX, TBM_SETRANGE, TRUE, MAKELPARAM(0, 400));
    m_lblOffsetXVal = CreateWindowW(L"STATIC", L"30px", WS_CHILD | WS_VISIBLE, 222, 342, 45, 20, hwnd, nullptr, hInst, nullptr);

    // Y Offset
    CreateWindowW(L"STATIC", L"Y Offset:", WS_CHILD | WS_VISIBLE, 285, 342, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_sliderOffsetY = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 365, 337, 140, 26, hwnd, (HMENU)1017, hInst, nullptr);
    SendMessageW(m_sliderOffsetY, TBM_SETRANGE, TRUE, MAKELPARAM(0, 400));
    m_lblOffsetYVal = CreateWindowW(L"STATIC", L"30px", WS_CHILD | WS_VISIBLE, 515, 342, 45, 20, hwnd, nullptr, hInst, nullptr);

    // Avatar Size
    CreateWindowW(L"STATIC", L"Avatar Size:", WS_CHILD | WS_VISIBLE, 26, 374, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_sliderAvatarSize = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 105, 369, 110, 26, hwnd, (HMENU)1018, hInst, nullptr);
    SendMessageW(m_sliderAvatarSize, TBM_SETRANGE, TRUE, MAKELPARAM(16, 64));
    m_lblAvatarSizeVal = CreateWindowW(L"STATIC", L"32px", WS_CHILD | WS_VISIBLE, 222, 374, 45, 20, hwnd, nullptr, hInst, nullptr);

    // Spacing
    CreateWindowW(L"STATIC", L"Spacing:", WS_CHILD | WS_VISIBLE, 285, 374, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_sliderSpacing = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 365, 369, 140, 26, hwnd, (HMENU)1019, hInst, nullptr);
    SendMessageW(m_sliderSpacing, TBM_SETRANGE, TRUE, MAKELPARAM(2, 30));
    m_lblSpacingVal = CreateWindowW(L"STATIC", L"10px", WS_CHILD | WS_VISIBLE, 515, 374, 45, 20, hwnd, nullptr, hInst, nullptr);

    // Animation
    CreateWindowW(L"STATIC", L"Animation:", WS_CHILD | WS_VISIBLE, 26, 406, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_cbAnimation = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 95, 402, 165, 200, hwnd, (HMENU)1020, hInst, nullptr);
    const wchar_t* anims[] = { L"None", L"Fade", L"Slide Up", L"Slide Down", L"Slide Left", L"Slide Right" };
    for (const auto* a : anims) SendMessageW(m_cbAnimation, CB_ADDSTRING, 0, (LPARAM)a);

    // Max Width
    CreateWindowW(L"STATIC", L"Max Width:", WS_CHILD | WS_VISIBLE, 285, 406, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_sliderMaxWidth = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 365, 401, 140, 26, hwnd, (HMENU)1021, hInst, nullptr);
    SendMessageW(m_sliderMaxWidth, TBM_SETRANGE, TRUE, MAKELPARAM(200, 900));
    m_lblMaxWidthVal = CreateWindowW(L"STATIC", L"480px", WS_CHILD | WS_VISIBLE, 515, 406, 50, 20, hwnd, nullptr, hInst, nullptr);

    // Auto Hide
    CreateWindowW(L"STATIC", L"Auto Hide:", WS_CHILD | WS_VISIBLE, 26, 438, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_cbAutoHide = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 95, 434, 165, 200, hwnd, (HMENU)1022, hInst, nullptr);
    const wchar_t* autoHides[] = { L"Never", L"5 sec", L"10 sec", L"15 sec", L"30 sec", L"60 sec" };
    for (const auto* h : autoHides) SendMessageW(m_cbAutoHide, CB_ADDSTRING, 0, (LPARAM)h);

    // Game Tracking & Bold
    m_chkAutoFollow = CreateWindowW(L"BUTTON", L"Auto Follow Game Window", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 285, 436, 270, 22, hwnd, (HMENU)1023, hInst, nullptr);
    m_chkMessageBold = CreateWindowW(L"BUTTON", L"Bold Font (Tebal)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 26, 464, 250, 22, hwnd, (HMENU)1024, hInst, nullptr);
    m_chkExtraBold = CreateWindowW(L"BUTTON", L"Extra Bold (Super Tebal)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 285, 464, 270, 22, hwnd, (HMENU)1027, hInst, nullptr);

    // Section 3: Chat Background (y=494 to 574)
    m_lblSec3 = CreateWindowW(L"STATIC", L"\x25CF LATAR BELAKANG CHAT (BACKGROUND)", WS_CHILD | WS_VISIBLE, 26, 498, 400, 18, hwnd, nullptr, hInst, nullptr);

    m_chkShowBg = CreateWindowW(L"BUTTON", L"Enable Background (Latar Chat)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 26, 520, 240, 22, hwnd, (HMENU)1025, hInst, nullptr);

    CreateWindowW(L"STATIC", L"Bg Color:", WS_CHILD | WS_VISIBLE, 285, 522, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_btnColBg = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 355, 517, 105, 28, hwnd, (HMENU)1035, hInst, nullptr);

    CreateWindowW(L"STATIC", L"Bg Opacity:", WS_CHILD | WS_VISIBLE, 26, 550, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_sliderBgOpacity = CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_HORZ, 105, 545, 135, 26, hwnd, (HMENU)1026, hInst, nullptr);
    SendMessageW(m_sliderBgOpacity, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    m_lblBgOpacityVal = CreateWindowW(L"STATIC", L"65%", WS_CHILD | WS_VISIBLE, 248, 550, 45, 20, hwnd, nullptr, hInst, nullptr);

    m_chkMotionBlur = CreateWindowW(L"BUTTON", L"Motion Blur (Animasi Halus & Dinamis)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 305, 548, 255, 22, hwnd, (HMENU)1028, hInst, nullptr);

    // Section 4: Text Colors (y=584 to 668)
    m_lblSec4 = CreateWindowW(L"STATIC", L"\x25CF WARNA TEKS & IDENTITAS PENGGUNA", WS_CHILD | WS_VISIBLE, 26, 588, 400, 18, hwnd, nullptr, hInst, nullptr);

    // Row 1
    CreateWindowW(L"STATIC", L"Username:", WS_CHILD | WS_VISIBLE, 26, 612, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_btnColUser = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 105, 607, 100, 28, hwnd, (HMENU)1030, hInst, nullptr);

    CreateWindowW(L"STATIC", L"Message:", WS_CHILD | WS_VISIBLE, 218, 612, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_btnColMsg = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 288, 607, 100, 28, hwnd, (HMENU)1031, hInst, nullptr);

    CreateWindowW(L"STATIC", L"Moderator:", WS_CHILD | WS_VISIBLE, 400, 612, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_btnColMod = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 478, 607, 80, 28, hwnd, (HMENU)1032, hInst, nullptr);

    // Row 2
    CreateWindowW(L"STATIC", L"Member:", WS_CHILD | WS_VISIBLE, 26, 642, 75, 20, hwnd, nullptr, hInst, nullptr);
    m_btnColMember = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 105, 638, 100, 28, hwnd, (HMENU)1033, hInst, nullptr);

    CreateWindowW(L"STATIC", L"Verified:", WS_CHILD | WS_VISIBLE, 218, 642, 65, 20, hwnd, nullptr, hInst, nullptr);
    m_btnColVerified = CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 288, 638, 100, 28, hwnd, (HMENU)1034, hInst, nullptr);

    // Section 5: Filter (y=676 to 730)
    m_lblSec5 = CreateWindowW(L"STATIC", L"\x25CF FILTER PESAN CHAT", WS_CHILD | WS_VISIBLE, 26, 680, 400, 18, hwnd, nullptr, hInst, nullptr);

    CreateWindowW(L"STATIC", L"Tampilkan Chat:", WS_CHILD | WS_VISIBLE, 26, 703, 115, 20, hwnd, nullptr, hInst, nullptr);
    m_cbFilter = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 145, 699, 240, 200, hwnd, (HMENU)1040, hInst, nullptr);
    const wchar_t* filters[] = { L"Semua Penonton (Everyone)", L"Hanya Moderator", L"Hanya Member / Langganan", L"Hanya Terverifikasi", L"Hanya Penonton Biasa" };
    for (const auto* f : filters) SendMessageW(m_cbFilter, CB_ADDSTRING, 0, (LPARAM)f);

    // Section 6: Bottom Actions (y=744)
    m_btnCheckUpdate = CreateWindowW(L"BUTTON", L"UPDATE GITHUB", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 26, 746, 145, 38, hwnd, (HMENU)1050, hInst, nullptr);
    m_btnDonate = CreateWindowW(L"BUTTON", L"DONASI (TRAKTEER)", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 182, 746, 175, 38, hwnd, (HMENU)1051, hInst, nullptr);
    m_lblSig = CreateWindowW(L"STATIC", L"Lala Live Chat \x2022 Coding-No", WS_CHILD | WS_VISIBLE, 375, 755, 195, 20, hwnd, nullptr, hInst, nullptr);

    // Set fonts for all controls
    EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
        HFONT hFont = (HFONT)lParam;
        SendMessageW(child, WM_SETFONT, (WPARAM)hFont, TRUE);
        return TRUE;
    }, (LPARAM)m_hFontNormal);

    SendMessageW(m_hHeaderTitle, WM_SETFONT, (WPARAM)m_hFontTitle, TRUE);
    SendMessageW(m_hHeaderSub, WM_SETFONT, (WPARAM)m_hFontSmall, TRUE);
    SendMessageW(m_lblSec1, WM_SETFONT, (WPARAM)m_hFontSection, TRUE);
    SendMessageW(m_lblSec2, WM_SETFONT, (WPARAM)m_hFontSection, TRUE);
    SendMessageW(m_lblSec3, WM_SETFONT, (WPARAM)m_hFontSection, TRUE);
    SendMessageW(m_lblSec4, WM_SETFONT, (WPARAM)m_hFontSection, TRUE);
    SendMessageW(m_lblSec5, WM_SETFONT, (WPARAM)m_hFontSection, TRUE);
    SendMessageW(m_lblSig, WM_SETFONT, (WPARAM)m_hFontSmall, TRUE);
    if (m_btnLoadConfig) {
        SendMessageW(m_btnLoadConfig, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    }

    PopulateControlsFromConfig();
}

void SettingsDialog::PopulateControlsFromConfig() {
    m_populatingControls = true;

    // Populate URL
    std::wstring wUrl(m_config.youtubeUrl.begin(), m_config.youtubeUrl.end());
    SetWindowTextW(m_hEditUrl, wUrl.c_str());

    // Font Family
    std::wstring wFont(m_config.fontFamily.begin(), m_config.fontFamily.end());
    int fontIdx = (int)SendMessageW(m_cbFont, CB_FINDSTRINGEXACT, -1, (LPARAM)wFont.c_str());
    if (fontIdx >= 0) SendMessageW(m_cbFont, CB_SETCURSEL, fontIdx, 0);
    else SendMessageW(m_cbFont, CB_SETCURSEL, 0, 0);

    // Font Size
    SendMessageW(m_sliderFontSize, TBM_SETPOS, TRUE, m_config.fontSize);
    SetWindowTextW(m_lblFontSizeVal, (std::to_wstring(m_config.fontSize) + L"px").c_str());

    // Opacity
    int opVal = (int)(m_config.opacity * 100.0f);
    SendMessageW(m_sliderOpacity, TBM_SETPOS, TRUE, opVal);
    SetWindowTextW(m_lblOpacityVal, (std::to_wstring(opVal) + L"%").c_str());

    // Position
    SendMessageW(m_cbPosition, CB_SETCURSEL, (int)m_config.position, 0);

    // Offsets
    SendMessageW(m_sliderOffsetX, TBM_SETPOS, TRUE, m_config.offsetX);
    SetWindowTextW(m_lblOffsetXVal, (std::to_wstring(m_config.offsetX) + L"px").c_str());

    SendMessageW(m_sliderOffsetY, TBM_SETPOS, TRUE, m_config.offsetY);
    SetWindowTextW(m_lblOffsetYVal, (std::to_wstring(m_config.offsetY) + L"px").c_str());

    // Avatar Size & Spacing
    SendMessageW(m_sliderAvatarSize, TBM_SETPOS, TRUE, m_config.avatarSize);
    SetWindowTextW(m_lblAvatarSizeVal, (std::to_wstring(m_config.avatarSize) + L"px").c_str());

    SendMessageW(m_sliderSpacing, TBM_SETPOS, TRUE, m_config.spacing);
    SetWindowTextW(m_lblSpacingVal, (std::to_wstring(m_config.spacing) + L"px").c_str());

    // Max Width
    SendMessageW(m_sliderMaxWidth, TBM_SETPOS, TRUE, m_config.maxWidth);
    SetWindowTextW(m_lblMaxWidthVal, (std::to_wstring(m_config.maxWidth) + L"px").c_str());

    // Animation
    SendMessageW(m_cbAnimation, CB_SETCURSEL, (int)m_config.animation, 0);

    // Duration
    int durIdx = 2; // Default 10 sec
    if (m_config.chatDurationSeconds == 3) durIdx = 0;
    else if (m_config.chatDurationSeconds == 5) durIdx = 1;
    else if (m_config.chatDurationSeconds == 10) durIdx = 2;
    else if (m_config.chatDurationSeconds == 15) durIdx = 3;
    else if (m_config.chatDurationSeconds == 30) durIdx = 4;
    else if (m_config.chatDurationSeconds == 60) durIdx = 5;
    else if (m_config.chatDurationSeconds < 0) durIdx = 6;
    SendMessageW(m_cbDuration, CB_SETCURSEL, durIdx, 0);

    // Max Chat
    int maxIdx = 2; // Default 8
    if (m_config.maxMessages == 3) maxIdx = 0;
    else if (m_config.maxMessages == 5) maxIdx = 1;
    else if (m_config.maxMessages == 8) maxIdx = 2;
    else if (m_config.maxMessages == 10) maxIdx = 3;
    else if (m_config.maxMessages == 15) maxIdx = 4;
    else if (m_config.maxMessages == 20) maxIdx = 5;
    SendMessageW(m_cbMaxMessages, CB_SETCURSEL, maxIdx, 0);

    // Auto Follow Checkbox
    SendMessageW(m_chkAutoFollow, BM_SETCHECK, m_config.autoFollowGame ? BST_CHECKED : BST_UNCHECKED, 0);

    // Bold Font Checkbox
    SendMessageW(m_chkMessageBold, BM_SETCHECK, m_config.messageBold ? BST_CHECKED : BST_UNCHECKED, 0);

    // Extra Bold Font Checkbox
    SendMessageW(m_chkExtraBold, BM_SETCHECK, m_config.extraBold ? BST_CHECKED : BST_UNCHECKED, 0);

    // Motion Blur Checkbox
    SendMessageW(m_chkMotionBlur, BM_SETCHECK, m_config.motionBlur ? BST_CHECKED : BST_UNCHECKED, 0);

    // Chat Background Checkbox & Slider
    SendMessageW(m_chkShowBg, BM_SETCHECK, m_config.showBackground ? BST_CHECKED : BST_UNCHECKED, 0);
    int bgOpVal = (int)(m_config.backgroundOpacity * 100.0f);
    SendMessageW(m_sliderBgOpacity, TBM_SETPOS, TRUE, bgOpVal);
    SetWindowTextW(m_lblBgOpacityVal, (std::to_wstring(bgOpVal) + L"%").c_str());

    // Filter
    SendMessageW(m_cbFilter, CB_SETCURSEL, (int)m_config.filter, 0);

    m_populatingControls = false;
}

void SettingsDialog::SaveConfigFromControls() {
    // 1. URL
    wchar_t buf[512] = {0};
    GetWindowTextW(m_hEditUrl, buf, 512);
    char u8Buf[1024] = {0};
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, u8Buf, 1024, nullptr, nullptr);
    m_config.youtubeUrl = u8Buf;

    // 2. Font Family
    int fontSel = (int)SendMessageW(m_cbFont, CB_GETCURSEL, 0, 0);
    if (fontSel >= 0) {
        wchar_t fontName[64] = {0};
        SendMessageW(m_cbFont, CB_GETLBTEXT, fontSel, (LPARAM)fontName);
        char u8Font[128] = {0};
        WideCharToMultiByte(CP_UTF8, 0, fontName, -1, u8Font, 128, nullptr, nullptr);
        m_config.fontFamily = u8Font;
    }

    // 3. Font Size
    m_config.fontSize = (int)SendMessageW(m_sliderFontSize, TBM_GETPOS, 0, 0);
    SetWindowTextW(m_lblFontSizeVal, (std::to_wstring(m_config.fontSize) + L"px").c_str());

    // 4. Opacity
    int opPos = (int)SendMessageW(m_sliderOpacity, TBM_GETPOS, 0, 0);
    m_config.opacity = (float)opPos / 100.0f;
    SetWindowTextW(m_lblOpacityVal, (std::to_wstring(opPos) + L"%").c_str());

    // 5. Position
    int posSel = (int)SendMessageW(m_cbPosition, CB_GETCURSEL, 0, 0);
    if (posSel >= 0) m_config.position = (OverlayPosition)posSel;

    // 6. Offsets
    m_config.offsetX = (int)SendMessageW(m_sliderOffsetX, TBM_GETPOS, 0, 0);
    SetWindowTextW(m_lblOffsetXVal, (std::to_wstring(m_config.offsetX) + L"px").c_str());

    m_config.offsetY = (int)SendMessageW(m_sliderOffsetY, TBM_GETPOS, 0, 0);
    SetWindowTextW(m_lblOffsetYVal, (std::to_wstring(m_config.offsetY) + L"px").c_str());

    // 7. Avatar Size & Spacing
    m_config.avatarSize = (int)SendMessageW(m_sliderAvatarSize, TBM_GETPOS, 0, 0);
    SetWindowTextW(m_lblAvatarSizeVal, (std::to_wstring(m_config.avatarSize) + L"px").c_str());

    m_config.spacing = (int)SendMessageW(m_sliderSpacing, TBM_GETPOS, 0, 0);
    SetWindowTextW(m_lblSpacingVal, (std::to_wstring(m_config.spacing) + L"px").c_str());

    // 8. Max Width
    m_config.maxWidth = (int)SendMessageW(m_sliderMaxWidth, TBM_GETPOS, 0, 0);
    SetWindowTextW(m_lblMaxWidthVal, (std::to_wstring(m_config.maxWidth) + L"px").c_str());

    // 9. Animation
    int animSel = (int)SendMessageW(m_cbAnimation, CB_GETCURSEL, 0, 0);
    if (animSel >= 0) m_config.animation = (OverlayAnimation)animSel;

    // 10. Duration
    int durSel = (int)SendMessageW(m_cbDuration, CB_GETCURSEL, 0, 0);
    const int durValues[] = { 3, 5, 10, 15, 30, 60, -1 };
    if (durSel >= 0 && durSel < 7) m_config.chatDurationSeconds = durValues[durSel];

    // 11. Max Messages
    int maxSel = (int)SendMessageW(m_cbMaxMessages, CB_GETCURSEL, 0, 0);
    const int maxValues[] = { 3, 5, 8, 10, 15, 20 };
    if (maxSel >= 0 && maxSel < 6) m_config.maxMessages = maxValues[maxSel];

    // 12. Auto Follow
    m_config.autoFollowGame = (SendMessageW(m_chkAutoFollow, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // 13. Bold & Extra Bold Message Font & Motion Blur
    m_config.messageBold = (SendMessageW(m_chkMessageBold, BM_GETCHECK, 0, 0) == BST_CHECKED);
    m_config.extraBold = (SendMessageW(m_chkExtraBold, BM_GETCHECK, 0, 0) == BST_CHECKED);
    m_config.motionBlur = (SendMessageW(m_chkMotionBlur, BM_GETCHECK, 0, 0) == BST_CHECKED);

    // 14. Chat Background
    m_config.showBackground = (SendMessageW(m_chkShowBg, BM_GETCHECK, 0, 0) == BST_CHECKED);
    int bgOp = (int)SendMessageW(m_sliderBgOpacity, TBM_GETPOS, 0, 0);
    m_config.backgroundOpacity = (float)bgOp / 100.0f;
    SetWindowTextW(m_lblBgOpacityVal, (std::to_wstring(bgOp) + L"%").c_str());

    // 15. Filter
    int filterSel = (int)SendMessageW(m_cbFilter, CB_GETCURSEL, 0, 0);
    if (filterSel >= 0) m_config.filter = (RoleFilter)filterSel;

    // Apply live updates directly to controller
    m_controller.UpdateConfig(m_config);
}

void SettingsDialog::SaveConfigToDisk() {
    SaveConfigFromControls();
    m_config.saveStandardConfig();
    if (m_btnSaveConfig && IsWindow(m_btnSaveConfig)) {
        SetWindowTextW(m_btnSaveConfig, L"TERSIMPAN! \x2714");
        InvalidateRect(m_btnSaveConfig, nullptr, TRUE);
        SetTimer(m_hwnd, 2, 2000, nullptr);
    }
    UpdateStatusText(ChatProviderStatus::Connected, "Pengaturan tersimpan");
}

void SettingsDialog::LoadConfigFromDisk() {
    if (m_config.loadStandardConfig()) {
        m_controller.UpdateConfig(m_config);
        PopulateControlsFromConfig();
        if (m_btnLoadConfig && IsWindow(m_btnLoadConfig)) {
            SetWindowTextW(m_btnLoadConfig, L"TER-LOAD! \x2714");
            InvalidateRect(m_btnLoadConfig, nullptr, TRUE);
            SetTimer(m_hwnd, 3, 2000, nullptr);
        }
        UpdateStatusText(ChatProviderStatus::Connected, "Pengaturan berhasil di-load");
    } else {
        UpdateStatusText(ChatProviderStatus::Error, "File setting belum ada/gagal di-load");
    }
}

void SettingsDialog::OnColorPick(uint32_t& targetColor, HWND buttonHwnd) {
    CHOOSECOLORW cc = {0};
    cc.lStructSize = sizeof(CHOOSECOLORW);
    cc.hwndOwner = m_hwnd;
    COLORREF initialRgb = RGB((targetColor >> 16) & 0xFF, (targetColor >> 8) & 0xFF, targetColor & 0xFF);
    cc.rgbResult = initialRgb;
    cc.lpCustColors = g_customColors;
    cc.Flags = CC_RGBINIT | CC_FULLOPEN;

    if (ChooseColorW(&cc)) {
        BYTE r = GetRValue(cc.rgbResult);
        BYTE g = GetGValue(cc.rgbResult);
        BYTE b = GetBValue(cc.rgbResult);
        targetColor = (0xFF << 24) | (r << 16) | (g << 8) | b;
        m_controller.UpdateConfig(m_config);
        if (buttonHwnd && IsWindow(buttonHwnd)) {
            InvalidateRect(buttonHwnd, nullptr, TRUE);
        }
    }
}

void SettingsDialog::CheckUpdate() {
    if (m_isUpdating) {
        MessageBoxW(
            m_hwnd,
            L"Pengunduhan pembaruan installer sedang berlangsung di latar belakang.\nMohon tunggu sejenak hingga proses selesai.",
            L"Sedang Mengunduh - Lala Live Chat Overlay",
            MB_OK | MB_ICONINFORMATION
        );
        return;
    }

    int choice = MessageBoxW(
        m_hwnd,
        L"Pembaruan Resmi Lala Live Chat Overlay (v1.0.0)\n"
        L"Developer: Coding-No\n\n"
        L"Repositori: https://github.com/Coding-No/lala-chat-overlay/releases\n\n"
        L"Apakah Anda ingin mengunduh otomatis installer versi terbaru (LalaLiveChatOverlaySetup.exe)?\n\n"
        L"[Ya]     : Unduh otomatis installer versi terbaru ke komputer sekarang.\n"
        L"[Tidak]  : Buka halaman rilis GitHub di browser web.\n"
        L"[Batal]  : Kembali ke pengaturan.",
        L"Cek Pembaruan - Lala Live Chat Overlay",
        MB_YESNOCANCEL | MB_ICONQUESTION
    );

    if (choice == IDCANCEL) {
        return;
    }

    if (choice == IDNO) {
        ShellExecuteW(nullptr, L"open", L"https://github.com/Coding-No/lala-chat-overlay/releases", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    // IDYES: Unduh otomatis di thread background
    m_isUpdating = true;
    if (m_btnCheckUpdate && IsWindow(m_btnCheckUpdate)) {
        SetWindowTextW(m_btnCheckUpdate, L"MENGUNDUH...");
        InvalidateRect(m_btnCheckUpdate, nullptr, TRUE);
    }

    std::thread([this]() {
        // Link aset rilis langsung dari GitHub
        const wchar_t* downloadUrl = L"https://github.com/Coding-No/lala-chat-overlay/releases/latest/download/LalaLiveChatOverlaySetup.exe";

        wchar_t tempDir[MAX_PATH];
        GetTempPathW(MAX_PATH, tempDir);
        std::wstring targetExe = std::wstring(tempDir) + L"LalaLiveChatOverlaySetup.exe";

        // Bersihkan cache agar selalu mengunduh file rilis yang paling segar
        DeleteUrlCacheEntryW(downloadUrl);

        HRESULT hr = URLDownloadToFileW(nullptr, downloadUrl, targetExe.c_str(), 0, nullptr);

        DWORD fileSize = 0;
        HANDLE hFile = CreateFileW(targetExe.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            fileSize = GetFileSize(hFile, nullptr);
            CloseHandle(hFile);
        }

        m_isUpdating = false;
        if (m_btnCheckUpdate && IsWindow(m_btnCheckUpdate)) {
            SetWindowTextW(m_btnCheckUpdate, L"UPDATE GITHUB");
            InvalidateRect(m_btnCheckUpdate, nullptr, TRUE);
        }

        // File installer setup biasanya beberapa MB (> 100 KB).
        // Bila file < 100 KB berarti response error 404 dari GitHub.
        if (SUCCEEDED(hr) && fileSize > 100000) {
            int installChoice = MessageBoxW(
                m_hwnd,
                (L"Installer versi terbaru (LalaLiveChatOverlaySetup.exe) berhasil diunduh!\n\n"
                 L"Lokasi: " + targetExe + L"\n\n"
                 L"Apakah Anda ingin langsung menjalankan installer sekarang untuk memperbarui aplikasi & OBS plugin?").c_str(),
                L"Unduhan Selesai - Lala Live Chat Overlay",
                MB_YESNO | MB_ICONINFORMATION
            );

            if (installChoice == IDYES) {
                ShellExecuteW(nullptr, L"open", targetExe.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
        } else {
            if (fileSize <= 100000) {
                DeleteFileW(targetExe.c_str());
            }

            int viewWeb = MessageBoxW(
                m_hwnd,
                L"Belum dapat mengunduh installer otomatis langsung dari server GitHub.\n"
                L"(Kemungkinan file 'LalaLiveChatOverlaySetup.exe' belum diunggah ke rilis terbaru GitHub, atau koneksi terputus).\n\n"
                L"Apakah Anda ingin membuka halaman rilis GitHub di browser?",
                L"Info Pembaruan GitHub",
                MB_YESNO | MB_ICONWARNING
            );

            if (viewWeb == IDYES) {
                ShellExecuteW(nullptr, L"open", L"https://github.com/Coding-No/lala-chat-overlay/releases", nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
    }).detach();
}

void SettingsDialog::OpenDonate() {
    ShellExecuteW(nullptr, L"open", L"https://trakteer.id/nopauwxp/gift", nullptr, nullptr, SW_SHOWNORMAL);
}

void SettingsDialog::DrawButton(LPDRAWITEMSTRUCT dis) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED) != 0;

    if (dis->CtlID == 1001) { // START
        COLORREF bg = isPressed ? RGB(4, 120, 87) : RGB(16, 185, 129);
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, bg);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, L"START \x25B6", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 1002) { // STOP
        COLORREF bg = isPressed ? RGB(185, 28, 28) : RGB(239, 68, 68);
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, bg);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, L"STOP \x25A0", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 1003) { // PREVIEW
        bool isPreview = m_controller.IsPreviewMode();
        COLORREF bg = isPressed ? RGB(67, 56, 202) : (isPreview ? RGB(124, 58, 237) : RGB(99, 102, 241));
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, bg);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        const wchar_t* pText = isPreview ? L"PREVIEW [ON]" : L"PREVIEW";
        DrawTextW(hdc, pText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 1004) { // SIMPAN SETTING
        wchar_t btnText[64] = {0};
        GetWindowTextW(m_btnSaveConfig, btnText, 64);
        bool isSaved = (wcsstr(btnText, L"TERSIMPAN") != nullptr);
        COLORREF bg = isSaved ? (isPressed ? RGB(4, 120, 87) : RGB(16, 185, 129))
                              : (isPressed ? RGB(30, 41, 59) : RGB(51, 65, 85));
        COLORREF borderCol = isSaved ? RGB(16, 185, 129) : RGB(99, 102, 241);
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, borderCol);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, btnText[0] ? btnText : L"SIMPAN \x2714", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 1005) { // LOAD SETTING
        wchar_t btnText[64] = {0};
        GetWindowTextW(m_btnLoadConfig, btnText, 64);
        bool isLoaded = (wcsstr(btnText, L"TER-LOAD") != nullptr);
        COLORREF bg = isLoaded ? (isPressed ? RGB(4, 120, 87) : RGB(16, 185, 129))
                               : (isPressed ? RGB(30, 41, 59) : RGB(47, 51, 78));
        COLORREF borderCol = isLoaded ? RGB(16, 185, 129) : RGB(139, 92, 246);
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, borderCol);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, btnText[0] ? btnText : L"LOAD \x21BB", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 1050) { // UPDATE GITHUB
        wchar_t btnText[64] = {0};
        GetWindowTextW(m_btnCheckUpdate, btnText, 64);
        COLORREF bg = isPressed ? RGB(15, 23, 42) : (m_isUpdating ? RGB(180, 83, 9) : RGB(30, 41, 59));
        HBRUSH br = CreateSolidBrush(bg);
        COLORREF borderCol = m_isUpdating ? RGB(245, 158, 11) : RGB(71, 85, 105);
        HPEN pen = CreatePen(PS_SOLID, 1, borderCol);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, m_isUpdating ? RGB(254, 240, 138) : RGB(56, 189, 248)); // Amber if updating, Cyan if idle
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, btnText[0] ? btnText : L"UPDATE GITHUB", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 1051) { // DONASI TRAKTEER
        COLORREF bg = isPressed ? RGB(190, 18, 60) : RGB(225, 29, 72);
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, bg);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 8, 8);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, L"\x2665 DONASI TRAKTEER", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID >= 1030 && dis->CtlID <= 1035) { // Color Buttons with live swatch
        uint32_t col = 0xFFFFFFFF;
        if (dis->CtlID == 1030) col = m_config.usernameColor;
        else if (dis->CtlID == 1031) col = m_config.messageColor;
        else if (dis->CtlID == 1032) col = m_config.moderatorColor;
        else if (dis->CtlID == 1033) col = m_config.memberColor;
        else if (dis->CtlID == 1034) col = m_config.verifiedColor;
        else if (dis->CtlID == 1035) col = m_config.backgroundColor;

        COLORREF bg = isPressed ? RGB(45, 47, 70) : RGB(37, 38, 56);
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(60, 63, 92));
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        // Draw Live Color Swatch Box on Left
        RECT swatchRc = { rc.left + 5, rc.top + 4, rc.left + 23, rc.bottom - 4 };
        COLORREF swatchColor = RGB((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF);
        HBRUSH sBr = CreateSolidBrush(swatchColor);
        HPEN sPen = CreatePen(PS_SOLID, 1, RGB(180, 180, 180));
        HGDIOBJ sob = SelectObject(hdc, sBr);
        HGDIOBJ sop = SelectObject(hdc, sPen);
        Rectangle(hdc, swatchRc.left, swatchRc.top, swatchRc.right, swatchRc.bottom);
        SelectObject(hdc, sob);
        SelectObject(hdc, sop);
        DeleteObject(sBr);
        DeleteObject(sPen);

        // Draw Text
        RECT textRc = { rc.left + 26, rc.top, rc.right - 2, rc.bottom };
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(226, 232, 240));
        SelectObject(hdc, m_hFontNormal);
        DrawTextW(hdc, L"Pilih Warna", -1, &textRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}

LRESULT CALLBACK SettingsDialog::DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    SettingsDialog* self = (SettingsDialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        case WM_CREATE: {
            SettingsDialog* dlg = (SettingsDialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (dlg) {
                dlg->m_hwnd = hwnd;
                dlg->m_config = dlg->m_controller.GetConfig();
                dlg->CreateControls(hwnd);
                SetTimer(hwnd, 1, 1000, nullptr);
            }
            return 0;
        }
        case WM_SYNC_CONFIG: {
            SettingsDialog* dlg = (SettingsDialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (dlg) {
                dlg->m_config = dlg->m_controller.GetConfig();
                dlg->PopulateControlsFromConfig();
            }
            return 0;
        }
        case WM_TIMER: {
            if (self) {
                if (wParam == 1) {
                    self->UpdateMessageCount(self->m_controller.GetMessageCount());
                } else if (wParam == 2) {
                    KillTimer(hwnd, 2);
                    if (self->m_btnSaveConfig && IsWindow(self->m_btnSaveConfig)) {
                        SetWindowTextW(self->m_btnSaveConfig, L"SIMPAN");
                        InvalidateRect(self->m_btnSaveConfig, nullptr, TRUE);
                    }
                } else if (wParam == 3) {
                    KillTimer(hwnd, 3);
                    if (self->m_btnLoadConfig && IsWindow(self->m_btnLoadConfig)) {
                        SetWindowTextW(self->m_btnLoadConfig, L"LOAD");
                        InvalidateRect(self->m_btnLoadConfig, nullptr, TRUE);
                    }
                }
            }
            return 0;
        }
        case WM_DRAWITEM: {
            if (self) {
                self->DrawButton((LPDRAWITEMSTRUCT)lParam);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND: {
            if (!self) break;
            int id = LOWORD(wParam);
            int code = HIWORD(wParam);

            if (id == 1001) { // START
                self->SaveConfigToDisk();
                wchar_t urlBuf[512] = {0};
                GetWindowTextW(self->m_hEditUrl, urlBuf, 512);
                char u8[1024] = {0};
                WideCharToMultiByte(CP_UTF8, 0, urlBuf, -1, u8, 1024, nullptr, nullptr);
                self->m_controller.StartChat(u8);
            } else if (id == 1002) { // STOP
                self->m_controller.StopChat();
            } else if (id == 1003) { // PREVIEW
                bool isPrev = self->m_controller.IsPreviewMode();
                self->m_controller.SetPreviewMode(!isPrev);
                if (self->m_btnPreview) InvalidateRect(self->m_btnPreview, nullptr, TRUE);
            } else if (id == 1004) { // SIMPAN SETTING
                self->SaveConfigToDisk();
            } else if (id == 1005) { // LOAD SETTING
                self->LoadConfigFromDisk();
            } else if (id == 1030) { // Color User
                self->OnColorPick(self->m_config.usernameColor, (HWND)lParam);
            } else if (id == 1031) { // Color Msg
                self->OnColorPick(self->m_config.messageColor, (HWND)lParam);
            } else if (id == 1032) { // Color Mod
                self->OnColorPick(self->m_config.moderatorColor, (HWND)lParam);
            } else if (id == 1033) { // Color Member
                self->OnColorPick(self->m_config.memberColor, (HWND)lParam);
            } else if (id == 1034) { // Color Verified
                self->OnColorPick(self->m_config.verifiedColor, (HWND)lParam);
            } else if (id == 1035) { // Color Background
                self->OnColorPick(self->m_config.backgroundColor, (HWND)lParam);
            } else if (id == 1050) { // CHECK UPDATE
                self->CheckUpdate();
            } else if (id == 1051) { // DONATE
                self->OpenDonate();
            } else if (code == CBN_SELCHANGE || code == EN_CHANGE || id == 1023 || id == 1024 || id == 1025 || id == 1027 || id == 1028) {
                if (!self->m_populatingControls) {
                    self->SaveConfigFromControls();
                }
            }
            return 0;
        }
        case WM_HSCROLL: {
            if (self && !self->m_populatingControls) {
                self->SaveConfigFromControls();
            }
            return 0;
        }
        case WM_PAINT: {
            if (!self) break;
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            // 1. Background
            FillRect(hdc, &rc, self->m_hBgBrush);

            // Helper to draw clean rounded card container
            auto DrawCard = [&](int left, int top, int right, int bottom, COLORREF borderCol) {
                HPEN pen = CreatePen(PS_SOLID, 1, borderCol);
                HGDIOBJ ob = SelectObject(hdc, self->m_hCardBrush);
                HGDIOBJ op = SelectObject(hdc, pen);
                RoundRect(hdc, left, top, right, bottom, 12, 12);
                SelectObject(hdc, ob);
                SelectObject(hdc, op);
                DeleteObject(pen);
            };

            // 2. Top Header Card Banner
            DrawCard(16, 12, rc.right - 16, 76, RGB(55, 60, 90));

            // 3. Draw Official Brand Logo
            if (self->m_pLogoBmp) {
                Gdiplus::Graphics g(hdc);
                g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                g.DrawImage(self->m_pLogoBmp, 26, 18, 145, 48);
            }

            // 4. Section Card Containers
            DrawCard(16, 84,  rc.right - 16, 208, RGB(40, 44, 66)); // Stream
            DrawCard(16, 216, rc.right - 16, 490, RGB(40, 44, 66)); // Appearance
            DrawCard(16, 494, rc.right - 16, 580, RGB(40, 44, 66)); // Background
            DrawCard(16, 584, rc.right - 16, 672, RGB(40, 44, 66)); // Text Colors
            DrawCard(16, 676, rc.right - 16, 734, RGB(40, 44, 66)); // Filter

            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND: {
            return 1;
        }
        case WM_CTLCOLOREDIT: {
            if (!self) break;
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(22, 23, 36));
            SetTextColor(hdc, RGB(248, 250, 252));
            return (LRESULT)self->m_hEditBrush;
        }
        case WM_CTLCOLORSTATIC: {
            if (!self) break;
            HDC hdc = (HDC)wParam;
            HWND hCtrl = (HWND)lParam;
            SetBkMode(hdc, TRANSPARENT);

            if (hCtrl == self->m_lblSec1 || hCtrl == self->m_lblSec2 || 
                hCtrl == self->m_lblSec3 || hCtrl == self->m_lblSec4 || 
                hCtrl == self->m_lblSec5) {
                SetTextColor(hdc, RGB(129, 140, 248)); // #818cf8 Vibrant Indigo
                return (LRESULT)self->m_hCardBrush;
            }
            if (hCtrl == self->m_hHeaderTitle) {
                SetTextColor(hdc, RGB(255, 255, 255));
                return (LRESULT)self->m_hCardBrush;
            }
            if (hCtrl == self->m_hHeaderSub) {
                SetTextColor(hdc, RGB(165, 180, 252));
                return (LRESULT)self->m_hCardBrush;
            }
            if (hCtrl == self->m_lblSig) {
                SetTextColor(hdc, RGB(148, 163, 184));
                return (LRESULT)self->m_hBgBrush;
            }
            if (hCtrl == self->m_lblFontSizeVal || hCtrl == self->m_lblOpacityVal ||
                hCtrl == self->m_lblOffsetXVal || hCtrl == self->m_lblOffsetYVal ||
                hCtrl == self->m_lblAvatarSizeVal || hCtrl == self->m_lblSpacingVal ||
                hCtrl == self->m_lblMaxWidthVal || hCtrl == self->m_lblBgOpacityVal) {
                SetTextColor(hdc, RGB(56, 189, 248)); // Sky 400
                return (LRESULT)self->m_hCardBrush;
            }

            SetTextColor(hdc, RGB(226, 232, 240));
            return (LRESULT)self->m_hCardBrush;
        }
        case WM_CTLCOLORDLG: {
            if (!self) break;
            return (LRESULT)self->m_hBgBrush;
        }
        case WM_CLOSE: {
            if (self) {
                self->SaveConfigToDisk();
            }
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        }
        case WM_USER + 999: {
            DestroyWindow(hwnd);
            return 0;
        }
        case WM_DESTROY: {
            if (self) {
                self->SaveConfigToDisk();
                self->m_hwnd = nullptr;
            }
            KillTimer(hwnd, 1);
            KillTimer(hwnd, 2);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
