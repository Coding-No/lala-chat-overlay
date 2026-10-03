#include "WindowsOverlayWindow.hpp"
#include <iostream>

static const wchar_t* WINDOW_CLASS_NAME = L"YouTubeLiveChatOverlayWindowClass";

WindowsOverlayWindow::WindowsOverlayWindow() {
}

WindowsOverlayWindow::~WindowsOverlayWindow() {
    Destroy();
}

bool WindowsOverlayWindow::Create(int x, int y, int width, int height) {
    if (m_running) return true;

    m_x = x;
    m_y = y;
    m_width = (width > 0) ? width : 500;
    m_height = (height > 0) ? height : 600;
    m_running = true;
    m_ready = false;

    m_thread = std::thread(&WindowsOverlayWindow::MessageLoop, this);

    // Wait until window is created
    while (!m_ready && m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    return m_hwnd != nullptr;
}

void WindowsOverlayWindow::Destroy() {
    if (!m_running) return;

    m_running = false;
    if (m_hwnd) {
        PostMessageW(m_hwnd, WM_CLOSE, 0, 0);
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_hwnd = nullptr;
}

void WindowsOverlayWindow::Show(bool show) {
    if (!m_hwnd) return;
    if (show) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        Redraw();
    } else {
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

void WindowsOverlayWindow::Hide() {
    Show(false);
}

void WindowsOverlayWindow::SetBounds(int x, int y, int width, int height) {
    m_x = x;
    m_y = y;
    m_width = width;
    m_height = height;

    if (m_hwnd) {
        SetWindowPos(m_hwnd, HWND_TOPMOST, m_x, m_y, m_width, m_height, 
            SWP_NOACTIVATE | SWP_NOZORDER);
        Redraw();
    }
}

void WindowsOverlayWindow::SetOpacity(float opacity) {
    m_opacity = (opacity < 0.0f) ? 0.0f : (opacity > 1.0f ? 1.0f : opacity);
    Redraw();
}

void WindowsOverlayWindow::Redraw() {
    if (!m_hwnd) return;
    PostMessageW(m_hwnd, WM_USER + 1, 0, 0);
}

bool WindowsOverlayWindow::VerifyStyles() const {
    if (!m_hwnd) return false;
    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    bool isTopmost = (exStyle & WS_EX_TOPMOST) != 0;
    bool isTransparent = (exStyle & WS_EX_TRANSPARENT) != 0;
    bool isLayered = (exStyle & WS_EX_LAYERED) != 0;
    bool isNoActivate = (exStyle & WS_EX_NOACTIVATE) != 0;
    bool isToolWindow = (exStyle & WS_EX_TOOLWINDOW) != 0;
    return isTopmost && isTransparent && isLayered && isNoActivate && isToolWindow;
}

bool WindowsOverlayWindow::VerifyCaptureExclusion() const {
    if (!m_hwnd) return false;
    DWORD affinity = 0;
    if (GetWindowDisplayAffinity(m_hwnd, &affinity)) {
        return affinity == WDA_EXCLUDEFROMCAPTURE;
    }
    return false;
}

void WindowsOverlayWindow::UpdateDpi() {
    if (!m_hwnd) return;
    typedef UINT(WINAPI* GetDpiForWindowProc)(HWND);
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        auto pGetDpiForWindow = (GetDpiForWindowProc)GetProcAddress(hUser32, "GetDpiForWindow");
        if (pGetDpiForWindow) {
            UINT dpi = pGetDpiForWindow(m_hwnd);
            m_dpiScale = static_cast<float>(dpi) / 96.0f;
            return;
        }
    }
    HDC hdc = GetDC(m_hwnd);
    if (hdc) {
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        m_dpiScale = static_cast<float>(dpiX) / 96.0f;
        ReleaseDC(m_hwnd, hdc);
    }
}

void WindowsOverlayWindow::RenderLayeredSurface() {
    if (!m_hwnd || m_width <= 0 || m_height <= 0) return;

    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = m_width;
    bmi.bmiHeader.biHeight = -m_height; // Top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    // Clear background to fully transparent (ARGB = 0x00000000)
    memset(pBits, 0, m_width * m_height * 4);

    // Call paint callback to render text and visuals
    if (m_paintCb) {
        m_paintCb(hdcMem, m_width, m_height);
    }

    // Apply UpdateLayeredWindow
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {m_width, m_height};
    POINT ptDst = {m_x, m_y};

    BLENDFUNCTION blend = {0};
    blend.BlendOp = AC_SRC_OVER;
    blend.BlendFlags = 0;
    blend.SourceConstantAlpha = static_cast<BYTE>(m_opacity * 255.0f);
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(m_hwnd, hdcScreen, &ptDst, &sizeWnd, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

LRESULT CALLBACK WindowsOverlayWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    WindowsOverlayWindow* self = (WindowsOverlayWindow*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_NCCREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }
        case WM_NCHITTEST:
            // Crucial: HTTRANSPARENT instructs Windows to pass all mouse input through to windows underneath
            return HTTRANSPARENT;

        case WM_MOUSEACTIVATE:
            // Crucial: MA_NOACTIVATE prevents clicking near or on the window from stealing focus
            return MA_NOACTIVATE;

        case WM_USER + 1:
            // Custom redraw message
            if (self) {
                self->RenderLayeredSurface();
            }
            return 0;

        case WM_TIMER:
            // Keep window strictly topmost without stealing focus
            if (wParam == 1 && self) {
                SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, 
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_ASYNCWINDOWPOS);
            }
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void WindowsOverlayWindow::MessageLoop() {
    HINSTANCE hInstance = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    RegisterClassExW(&wc);

    DWORD exStyle = WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | 
                    WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW;
    DWORD style = WS_POPUP;

    m_hwnd = CreateWindowExW(
        exStyle,
        WINDOW_CLASS_NAME,
        L"YouTubeLiveChatOverlay",
        style,
        m_x, m_y, m_width, m_height,
        nullptr, nullptr, hInstance, this
    );

    if (m_hwnd) {
        // Apply capture exclusion: DWM will exclude this window from screen capture/streaming
        SetWindowDisplayAffinity(m_hwnd, WDA_EXCLUDEFROMCAPTURE);
        
        UpdateDpi();

        // 1-second watchdog timer to ensure topmost Z-order over games
        SetTimer(m_hwnd, 1, 1000, nullptr);

        m_ready = true;

        MSG msg;
        while (m_running && GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        KillTimer(m_hwnd, 1);
        DestroyWindow(m_hwnd);
    } else {
        m_ready = true;
    }

    UnregisterClassW(WINDOW_CLASS_NAME, hInstance);
}
