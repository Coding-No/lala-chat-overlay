#pragma once
#include <windows.h>
#include <string>
#include <atomic>
#include <thread>
#include <functional>

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif

class WindowsOverlayWindow {
public:
    using PaintCallback = std::function<void(HDC hdc, int width, int height)>;

    WindowsOverlayWindow();
    ~WindowsOverlayWindow();

    bool Create(int x, int y, int width, int height);
    void Destroy();

    void Show(bool show = true);
    void Hide();
    void SetBounds(int x, int y, int width, int height);
    void SetOpacity(float opacity);
    void Redraw();

    HWND GetHwnd() const { return m_hwnd; }
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetX() const { return m_x; }
    int GetY() const { return m_y; }
    float GetDpiScale() const { return m_dpiScale; }

    void SetPaintCallback(PaintCallback cb) { m_paintCb = cb; }

    // Verification methods for Phase 2 PoC
    bool VerifyStyles() const;
    bool VerifyCaptureExclusion() const;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void MessageLoop();
    void RenderLayeredSurface();
    void UpdateDpi();

    HWND m_hwnd{nullptr};
    int m_x{0};
    int m_y{0};
    int m_width{500};
    int m_height{600};
    float m_opacity{1.0f};
    float m_dpiScale{1.0f};
    
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_ready{false};
    PaintCallback m_paintCb;
};
