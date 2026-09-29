#include "DockPanel.hpp"
#include <commctrl.h>
#include <iostream>

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

static const wchar_t* DOCK_WND_CLASS = L"LalaChatOverlayDockClass";

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

DockPanel::DockPanel(OverlayController& controller, SettingsDialog& dialog)
    : m_controller(controller), m_dialog(dialog) {
    m_hBgBrush   = CreateSolidBrush(RGB(28, 30, 46));   // #1c1e2e Slate Dark
    m_hEditBrush = CreateSolidBrush(RGB(20, 21, 33));   // Input background
    m_hFontNormal = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_hFontBold   = CreateFontW(14, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
}

DockPanel::~DockPanel() {
    if (m_hwnd && IsWindow(m_hwnd)) {
        KillTimer(m_hwnd, 1);
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    DeleteObject(m_hBgBrush);
    DeleteObject(m_hEditBrush);
    DeleteObject(m_hFontNormal);
    DeleteObject(m_hFontBold);
}

HWND DockPanel::Create(HWND parent) {
    HINSTANCE hInst = (HINSTANCE)&__ImageBase;

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = m_hBgBrush;
    wc.lpszClassName = DOCK_WND_CLASS;

    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_CONTROLPARENT,
        DOCK_WND_CLASS,
        L"Lala Live Chat Overlay Dock",
        WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        0, 0, 320, 260,
        parent ? parent : GetDesktopWindow(),
        nullptr, hInst, this
    );

    return m_hwnd;
}

void DockPanel::CreateControls(HWND hwnd) {
    HINSTANCE hInst = (HINSTANCE)&__ImageBase;

    m_lblTitle = CreateWindowW(L"STATIC", L"YouTube Live Stream URL:", WS_CHILD | WS_VISIBLE, 12, 10, 290, 18, hwnd, nullptr, hInst, nullptr);
    m_hEditUrl = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 12, 32, 290, 26, hwnd, nullptr, hInst, nullptr);

    // Initial URL from config
    std::string currentUrl = m_controller.GetConfig().youtubeUrl;
    if (!currentUrl.empty()) {
        std::wstring wUrl(currentUrl.begin(), currentUrl.end());
        SetWindowTextW(m_hEditUrl, wUrl.c_str());
    }

    m_btnStart = CreateWindowW(L"BUTTON", L"START", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 12, 66, 85, 30, hwnd, (HMENU)2001, hInst, nullptr);
    m_btnStop = CreateWindowW(L"BUTTON", L"STOP", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 102, 66, 85, 30, hwnd, (HMENU)2002, hInst, nullptr);
    m_btnPreview = CreateWindowW(L"BUTTON", L"PREVIEW", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 192, 66, 110, 30, hwnd, (HMENU)2003, hInst, nullptr);

    m_btnSettings = CreateWindowW(L"BUTTON", L"PENGATURAN LENGKAP...", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 12, 104, 290, 32, hwnd, (HMENU)2004, hInst, nullptr);

    m_chkDockBg = CreateWindowW(L"BUTTON", L"Latar Chat (Bg)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 12, 142, 135, 20, hwnd, (HMENU)2005, hInst, nullptr);
    m_chkDockBold = CreateWindowW(L"BUTTON", L"Teks Tebal (Bold)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX, 155, 142, 145, 20, hwnd, (HMENU)2006, hInst, nullptr);

    SendMessageW(m_chkDockBg, BM_SETCHECK, m_controller.GetConfig().showBackground ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_chkDockBold, BM_SETCHECK, m_controller.GetConfig().messageBold ? BST_CHECKED : BST_UNCHECKED, 0);

    m_lblStatus = CreateWindowW(L"STATIC", L"Status: Terputus", WS_CHILD | WS_VISIBLE, 12, 172, 290, 18, hwnd, nullptr, hInst, nullptr);
    m_lblCount = CreateWindowW(L"STATIC", L"Pesan: 0", WS_CHILD | WS_VISIBLE, 12, 192, 290, 18, hwnd, nullptr, hInst, nullptr);

    // Set fonts
    EnumChildWindows(hwnd, [](HWND child, LPARAM lParam) -> BOOL {
        HFONT hFont = (HFONT)lParam;
        SendMessageW(child, WM_SETFONT, (WPARAM)hFont, TRUE);
        return TRUE;
    }, (LPARAM)m_hFontNormal);

    SendMessageW(m_btnSettings, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    SendMessageW(m_btnStart, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    SendMessageW(m_btnStop, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);
    SendMessageW(m_btnPreview, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

    SetTimer(hwnd, 1, 1000, nullptr);
}

void DockPanel::OnResize(int width, int height) {
    if (width <= 0) width = 320;
    int margin = 12;
    int contentW = width - (margin * 2);
    if (contentW < 180) contentW = 180;

    SetWindowPos(m_lblTitle, nullptr, margin, 10, contentW, 18, SWP_NOZORDER);
    SetWindowPos(m_hEditUrl, nullptr, margin, 32, contentW, 26, SWP_NOZORDER);

    int btnW = (contentW - 12) / 3;
    SetWindowPos(m_btnStart, nullptr, margin, 66, btnW, 30, SWP_NOZORDER);
    SetWindowPos(m_btnStop, nullptr, margin + btnW + 6, 66, btnW, 30, SWP_NOZORDER);
    SetWindowPos(m_btnPreview, nullptr, margin + (btnW + 6) * 2, 66, contentW - ((btnW + 6) * 2), 30, SWP_NOZORDER);

    SetWindowPos(m_btnSettings, nullptr, margin, 104, contentW, 32, SWP_NOZORDER);

    int halfW = (contentW - 8) / 2;
    SetWindowPos(m_chkDockBg, nullptr, margin, 142, halfW, 20, SWP_NOZORDER);
    SetWindowPos(m_chkDockBold, nullptr, margin + halfW + 8, 142, halfW, 20, SWP_NOZORDER);

    SetWindowPos(m_lblStatus, nullptr, margin, 172, contentW, 18, SWP_NOZORDER);
    SetWindowPos(m_lblCount, nullptr, margin, 192, contentW, 18, SWP_NOZORDER);
}

void DockPanel::DrawButton(LPDRAWITEMSTRUCT dis) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    bool isPressed = (dis->itemState & ODS_SELECTED) != 0;

    if (dis->CtlID == 2001) { // START
        COLORREF bg = isPressed ? RGB(4, 120, 87) : RGB(16, 185, 129);
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, bg);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, L"START \x25B6", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 2002) { // STOP
        COLORREF bg = isPressed ? RGB(185, 28, 28) : RGB(239, 68, 68);
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, bg);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, L"STOP \x25A0", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 2003) { // PREVIEW
        bool isPreview = m_controller.IsPreviewMode();
        COLORREF bg = isPressed ? RGB(67, 56, 202) : (isPreview ? RGB(124, 58, 237) : RGB(99, 102, 241));
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, bg);
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        SelectObject(hdc, m_hFontBold);
        const wchar_t* pText = isPreview ? L"PREVIEW [ON]" : L"PREVIEW";
        DrawTextW(hdc, pText, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    } else if (dis->CtlID == 2004) { // DETAILED SETTINGS
        COLORREF bg = isPressed ? RGB(30, 41, 59) : RGB(49, 46, 129); // Deep Indigo
        HBRUSH br = CreateSolidBrush(bg);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(99, 102, 241));
        HGDIOBJ ob = SelectObject(hdc, br);
        HGDIOBJ op = SelectObject(hdc, pen);
        RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
        SelectObject(hdc, ob);
        SelectObject(hdc, op);
        DeleteObject(br);
        DeleteObject(pen);

        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(224, 231, 255));
        SelectObject(hdc, m_hFontBold);
        DrawTextW(hdc, L"\x2699 PENGATURAN LENGKAP...", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
}

void DockPanel::UpdateStatusText(ChatProviderStatus status, const std::string& info) {
    if (!m_lblStatus || !IsWindow(m_lblStatus)) return;
    std::wstring text = L"Status: " + std::wstring(StatusToIndonesian(status));
    if (!info.empty()) {
        std::wstring wInfo(info.begin(), info.end());
        text += L" (" + wInfo + L")";
    }
    SetWindowTextW(m_lblStatus, text.c_str());
}

void DockPanel::UpdateMessageCount(uint64_t count) {
    if (!m_lblCount || !IsWindow(m_lblCount)) return;
    std::wstring text = L"Pesan: " + std::to_wstring(count);
    SetWindowTextW(m_lblCount, text.c_str());
}

LRESULT CALLBACK DockPanel::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    DockPanel* self = (DockPanel*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        case WM_CREATE: {
            DockPanel* p = (DockPanel*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
            if (p) {
                p->m_hwnd = hwnd;
                p->CreateControls(hwnd);
            }
            return 0;
        }
        case WM_SIZE: {
            if (self) {
                int w = LOWORD(lParam);
                int h = HIWORD(lParam);
                self->OnResize(w, h);
            }
            return 0;
        }
        case WM_TIMER: {
            if (self && wParam == 1) {
                self->UpdateMessageCount(self->m_controller.GetMessageCount());
                if (self->m_chkDockBg && IsWindow(self->m_chkDockBg)) {
                    bool bgChecked = (SendMessageW(self->m_chkDockBg, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (bgChecked != self->m_controller.GetConfig().showBackground) {
                        SendMessageW(self->m_chkDockBg, BM_SETCHECK, self->m_controller.GetConfig().showBackground ? BST_CHECKED : BST_UNCHECKED, 0);
                    }
                }
                if (self->m_chkDockBold && IsWindow(self->m_chkDockBold)) {
                    bool boldChecked = (SendMessageW(self->m_chkDockBold, BM_GETCHECK, 0, 0) == BST_CHECKED);
                    if (boldChecked != self->m_controller.GetConfig().messageBold) {
                        SendMessageW(self->m_chkDockBold, BM_SETCHECK, self->m_controller.GetConfig().messageBold ? BST_CHECKED : BST_UNCHECKED, 0);
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

            if (id == 2001) { // START
                wchar_t buf[512] = {0};
                GetWindowTextW(self->m_hEditUrl, buf, 512);
                char u8[1024] = {0};
                WideCharToMultiByte(CP_UTF8, 0, buf, -1, u8, 1024, nullptr, nullptr);
                self->m_controller.StartChat(u8);
            } else if (id == 2002) { // STOP
                self->m_controller.StopChat();
            } else if (id == 2003) { // PREVIEW
                bool isPrev = self->m_controller.IsPreviewMode();
                self->m_controller.SetPreviewMode(!isPrev);
                if (self->m_btnPreview) InvalidateRect(self->m_btnPreview, nullptr, TRUE);
            } else if (id == 2004) { // DETAILED SETTINGS
                self->m_dialog.Show(nullptr);
            } else if (id == 2005) { // SHOW BG CHECKBOX
                ChatConfig cfg = self->m_controller.GetConfig();
                cfg.showBackground = (SendMessageW(self->m_chkDockBg, BM_GETCHECK, 0, 0) == BST_CHECKED);
                self->m_controller.UpdateConfig(cfg);
            } else if (id == 2006) { // BOLD CHECKBOX
                ChatConfig cfg = self->m_controller.GetConfig();
                cfg.messageBold = (SendMessageW(self->m_chkDockBold, BM_GETCHECK, 0, 0) == BST_CHECKED);
                self->m_controller.UpdateConfig(cfg);
            }
            return 0;
        }
        case WM_ERASEBKGND: {
            if (!self) break;
            HDC hdc = (HDC)wParam;
            RECT rc;
            GetClientRect(hwnd, &rc);
            FillRect(hdc, &rc, self->m_hBgBrush);
            return 1;
        }
        case WM_CTLCOLOREDIT: {
            if (!self) break;
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, RGB(20, 21, 33));
            SetTextColor(hdc, RGB(248, 250, 252));
            return (LRESULT)self->m_hEditBrush;
        }
        case WM_CTLCOLORSTATIC: {
            if (!self) break;
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(226, 232, 240));
            return (LRESULT)self->m_hBgBrush;
        }
        case WM_DESTROY: {
            if (self) {
                self->m_hwnd = nullptr;
            }
            KillTimer(hwnd, 1);
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}
