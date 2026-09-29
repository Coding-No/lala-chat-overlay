#pragma once
#include <windows.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include "../models/ChatConfig.hpp"
#include "../overlay/OverlayController.hpp"

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "comdlg32.lib")

class SettingsDialog {
public:
    SettingsDialog(OverlayController& controller);
    ~SettingsDialog();

    bool Show(HWND parentHwnd = nullptr);
    void Hide();
    bool IsOpen() const { return m_hwnd != nullptr && IsWindow(m_hwnd) && IsWindowVisible(m_hwnd); }

    void UpdateStatusText(ChatProviderStatus status, const std::string& info);
    void UpdateMessageCount(uint64_t count);

private:
    static LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void MessageLoop();
    void CreateControls(HWND hwnd);
    void PopulateControlsFromConfig();
    void SaveConfigFromControls();
    void OnColorPick(uint32_t& targetColor, HWND buttonHwnd);
    void CheckUpdate();
    void OpenDonate();
    void DrawButton(LPDRAWITEMSTRUCT dis);

    OverlayController& m_controller;
    ChatConfig m_config;
    HWND m_hwnd{nullptr};
    HWND m_parentHwnd{nullptr};

    std::thread m_uiThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_ready{false};

    // Header & Section Label handles
    HWND m_hHeaderTitle{nullptr};
    HWND m_hHeaderSub{nullptr};
    HWND m_lblSig{nullptr};
    HWND m_lblSec1{nullptr};
    HWND m_lblSec2{nullptr};
    HWND m_lblSec3{nullptr};
    HWND m_lblSec4{nullptr};
    HWND m_lblSec5{nullptr};

    // Control handles
    HWND m_hEditUrl{nullptr};
    HWND m_btnStart{nullptr};
    HWND m_btnStop{nullptr};
    HWND m_btnPreview{nullptr};
    HWND m_lblStatus{nullptr};
    HWND m_lblMessages{nullptr};

    HWND m_cbFont{nullptr};
    HWND m_sliderFontSize{nullptr};
    HWND m_lblFontSizeVal{nullptr};
    HWND m_sliderOpacity{nullptr};
    HWND m_lblOpacityVal{nullptr};
    HWND m_cbDuration{nullptr};
    HWND m_cbMaxMessages{nullptr};
    HWND m_cbPosition{nullptr};
    HWND m_sliderOffsetX{nullptr};
    HWND m_lblOffsetXVal{nullptr};
    HWND m_sliderOffsetY{nullptr};
    HWND m_lblOffsetYVal{nullptr};
    HWND m_sliderAvatarSize{nullptr};
    HWND m_lblAvatarSizeVal{nullptr};
    HWND m_sliderSpacing{nullptr};
    HWND m_lblSpacingVal{nullptr};
    HWND m_cbAnimation{nullptr};
    HWND m_sliderMaxWidth{nullptr};
    HWND m_lblMaxWidthVal{nullptr};
    HWND m_cbAutoHide{nullptr};
    HWND m_chkAutoFollow{nullptr};
    HWND m_cbFilter{nullptr};

    HWND m_chkMessageBold{nullptr};
    HWND m_chkShowBg{nullptr};
    HWND m_btnColBg{nullptr};
    HWND m_sliderBgOpacity{nullptr};
    HWND m_lblBgOpacityVal{nullptr};

    HWND m_btnColUser{nullptr};
    HWND m_btnColMsg{nullptr};
    HWND m_btnColMod{nullptr};
    HWND m_btnColMember{nullptr};
    HWND m_btnColVerified{nullptr};

    HWND m_btnCheckUpdate{nullptr};
    HWND m_btnDonate{nullptr};

    HBRUSH m_hBgBrush{nullptr};
    HBRUSH m_hCardBrush{nullptr};
    HBRUSH m_hEditBrush{nullptr};
    HFONT m_hFontNormal{nullptr};
    HFONT m_hFontBold{nullptr};
    HFONT m_hFontTitle{nullptr};
    HFONT m_hFontSmall{nullptr};
    HFONT m_hFontSection{nullptr};

    ULONG_PTR m_gdiToken{0};
    Gdiplus::Bitmap* m_pLogoBmp{nullptr};
};
