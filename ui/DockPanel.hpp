#pragma once
#include <windows.h>
#include <string>
#include <memory>
#include "../models/ChatConfig.hpp"
#include "../overlay/OverlayController.hpp"
#include "SettingsDialog.hpp"

class DockPanel {
public:
    DockPanel(OverlayController& controller, SettingsDialog& dialog);
    ~DockPanel();

    HWND Create(HWND parent = nullptr);
    HWND GetHwnd() const { return m_hwnd; }

    void UpdateStatusText(ChatProviderStatus status, const std::string& info);
    void UpdateMessageCount(uint64_t count);

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void CreateControls(HWND hwnd);
    void OnResize(int width, int height);
    void DrawButton(LPDRAWITEMSTRUCT dis);

    OverlayController& m_controller;
    SettingsDialog& m_dialog;
    HWND m_hwnd{nullptr};

    HWND m_lblTitle{nullptr};
    HWND m_hEditUrl{nullptr};
    HWND m_btnStart{nullptr};
    HWND m_btnStop{nullptr};
    HWND m_btnPreview{nullptr};
    HWND m_btnSettings{nullptr};
    HWND m_chkDockBg{nullptr};
    HWND m_chkDockBold{nullptr};
    HWND m_lblStatus{nullptr};
    HWND m_lblCount{nullptr};

    HBRUSH m_hBgBrush{nullptr};
    HBRUSH m_hEditBrush{nullptr};
    HFONT m_hFontNormal{nullptr};
    HFONT m_hFontBold{nullptr};
};
