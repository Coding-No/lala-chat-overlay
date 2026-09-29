#include <windows.h>
#include <iostream>
#include "overlay/OverlayController.hpp"
#include "ui/SettingsDialog.hpp"
#include "models/ChatConfig.hpp"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Enable DPI awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    ChatConfig config;
    config.loadFromFile("config/overlay_config.json");

    OverlayController controller;
    if (!controller.Initialize(config)) {
        MessageBoxW(nullptr, L"Failed to initialize Lala Live Chat Overlay window!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    SettingsDialog dialog(controller);

    controller.SetStatusCallback([&dialog](ChatProviderStatus status, const std::string& info) {
        dialog.UpdateStatusText(status, info);
    });

    if (!dialog.Show()) {
        MessageBoxW(nullptr, L"Failed to display Settings Dialog!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    while (dialog.IsOpen()) {
        Sleep(100);
    }

    // Save configuration before exit
    CreateDirectoryW(L"config", nullptr);
    controller.GetConfig().saveToFile("config/overlay_config.json");

    controller.Shutdown();
    return 0;
}
