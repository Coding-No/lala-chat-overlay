#include "obs-module-compat.h"
#include "../overlay/OverlayController.hpp"
#include "../ui/SettingsDialog.hpp"
#include "../ui/DockPanel.hpp"
#include "../models/ChatConfig.hpp"
#include <memory>
#include <fstream>
#include <shlobj.h>

typedef void* (*proc_QWindow_fromWinId)(unsigned __int64 winId);
typedef void* (*proc_QWidget_createWindowContainer)(void* window, void* parent, unsigned int flags);
typedef bool (*proc_obs_frontend_add_dock_by_id)(const char *id, const char *title, void *widget);

static obs_module_t* g_obsModule = nullptr;
static std::unique_ptr<OverlayController> g_controller;
static std::unique_ptr<SettingsDialog> g_dialog;
static std::unique_ptr<DockPanel> g_dockPanel;
static void* g_qWidgetDock = nullptr;
static ChatConfig g_config;
static std::string g_configPath;

static void LogMessage(const std::string& msg) {
    wchar_t appData[MAX_PATH];
    std::wstring logDir = L"logs";
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        logDir = std::wstring(appData) + L"\\obs-studio\\plugin_config\\yt-chat-overlay";
        CreateDirectoryW((std::wstring(appData) + L"\\obs-studio").c_str(), nullptr);
        CreateDirectoryW((std::wstring(appData) + L"\\obs-studio\\plugin_config").c_str(), nullptr);
        CreateDirectoryW(logDir.c_str(), nullptr);
    } else {
        CreateDirectoryW(L"logs", nullptr);
    }
    std::wstring fullPath = logDir + L"\\overlay.log";
    std::ofstream logFile(fullPath, std::ios::app);
    if (logFile.is_open()) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        logFile << "[" << now << "] " << msg << std::endl;
    }
    OutputDebugStringA(("[yt-chat-overlay] " + msg + "\n").c_str());
}

static std::string GetConfigFilePath() {
    wchar_t appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        std::wstring dir = std::wstring(appData) + L"\\obs-studio\\plugin_config\\yt-chat-overlay";
        CreateDirectoryW((std::wstring(appData) + L"\\obs-studio").c_str(), nullptr);
        CreateDirectoryW((std::wstring(appData) + L"\\obs-studio\\plugin_config").c_str(), nullptr);
        CreateDirectoryW(dir.c_str(), nullptr);

        char u8[MAX_PATH * 2] = {0};
        WideCharToMultiByte(CP_UTF8, 0, (dir + L"\\overlay_config.json").c_str(), -1, u8, sizeof(u8), nullptr, nullptr);
        return std::string(u8);
    }
    return "config/overlay_config.json";
}

static void OnToolsMenuClicked(void* privateData) {
    (void)privateData;
    LogMessage("Tools menu clicked: Opening Settings & Control Dialog");

    if (!g_controller) {
        LogMessage("Error: g_controller is null!");
        return;
    }

    if (!g_dialog) {
        LogMessage("Instantiating SettingsDialog...");
        g_dialog = std::make_unique<SettingsDialog>(*g_controller);
        g_controller->SetStatusCallback([](ChatProviderStatus s, const std::string& info) {
            if (g_dialog) g_dialog->UpdateStatusText(s, info);
            if (g_dockPanel) g_dockPanel->UpdateStatusText(s, info);
        });
    }

    if (g_dialog) {
        LogMessage("Calling g_dialog->Show(nullptr)...");
        bool ok = g_dialog->Show(nullptr);
        LogMessage(std::string("Dialog Show() result: ") + (ok ? "SUCCESS" : "FAILED"));
    }
}

extern "C" {

MODULE_EXPORT uint32_t obs_module_ver(void) {
    return LIBOBS_API_VER;
}

MODULE_EXPORT void obs_module_set_pointer(obs_module_t* module) {
    g_obsModule = module;
}

MODULE_EXPORT const char* obs_module_name(void) {
    return "Lala Live Chat Overlay";
}

MODULE_EXPORT const char* obs_module_description(void) {
    return "Lala Live Chat Overlay - Transparent In-Game Live Chat for OBS by Coding-No";
}

MODULE_EXPORT bool obs_module_load(void) {
    LogMessage("Loading Lala Live Chat Overlay plugin for OBS");

    INITCOMMONCONTROLSEX icce = { sizeof(INITCOMMONCONTROLSEX), ICC_BAR_CLASSES | ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icce);

    g_configPath = GetConfigFilePath();
    g_config.loadFromFile(g_configPath);

    g_controller = std::make_unique<OverlayController>();
    if (!g_controller->Initialize(g_config)) {
        LogMessage("Error: Failed to initialize OverlayController");
        return false;
    }

    // Prepare Dialog and DockPanel
    g_dialog = std::make_unique<SettingsDialog>(*g_controller);
    g_dockPanel = std::make_unique<DockPanel>(*g_controller, *g_dialog);

    g_controller->SetStatusCallback([](ChatProviderStatus s, const std::string& info) {
        if (g_dialog) g_dialog->UpdateStatusText(s, info);
        if (g_dockPanel) g_dockPanel->UpdateStatusText(s, info);
    });

    // 1. Register OBS Dock (Menu: Docks -> Lala Live Chat Overlay)
    HMODULE hGui = GetModuleHandleW(L"Qt6Gui.dll");
    HMODULE hWidgets = GetModuleHandleW(L"Qt6Widgets.dll");
    HMODULE hFrontend = GetModuleHandleW(L"obs-frontend-api.dll");

    if (!hGui) hGui = LoadLibraryW(L"Qt6Gui.dll");
    if (!hWidgets) hWidgets = LoadLibraryW(L"Qt6Widgets.dll");
    if (!hFrontend) hFrontend = LoadLibraryW(L"obs-frontend-api.dll");

    if (hGui && hWidgets && hFrontend) {
        auto pFromWinId = (proc_QWindow_fromWinId)GetProcAddress(hGui, "?fromWinId@QWindow@@SAPEAV1@_K@Z");
        auto pCreateContainer = (proc_QWidget_createWindowContainer)GetProcAddress(hWidgets, "?createWindowContainer@QWidget@@SAPEAV1@PEAVQWindow@@PEAV1@V?$QFlags@W4WindowType@Qt@@@@@Z");
        auto pAddDock = (proc_obs_frontend_add_dock_by_id)GetProcAddress(hFrontend, "obs_frontend_add_dock_by_id");

        if (pFromWinId && pCreateContainer && pAddDock) {
            HWND dockHwnd = g_dockPanel->Create(nullptr);
            if (dockHwnd) {
                void* qWindow = pFromWinId((unsigned __int64)dockHwnd);
                if (qWindow) {
                    g_qWidgetDock = pCreateContainer(qWindow, nullptr, 0);
                    if (g_qWidgetDock) {
                        bool dockOk = pAddDock("lala_chat_overlay_dock", "Lala Live Chat Overlay", g_qWidgetDock);
                        LogMessage(std::string("Registered 'Docks -> Lala Live Chat Overlay': ") + (dockOk ? "SUCCESS" : "FAILED"));
                    }
                }
            }
        } else {
            LogMessage("Warning: Qt6 / Frontend dock function addresses not found.");
        }
    } else {
        LogMessage("Warning: Qt6 or Frontend modules not found for dock registration.");
    }

    // 2. Register Tools menu item (Menu: Tools -> Lala Live Chat Overlay)
    if (hFrontend) {
        auto pAddMenuItem = (proc_obs_frontend_add_tools_menu_item)GetProcAddress(hFrontend, "obs_frontend_add_tools_menu_item");
        if (pAddMenuItem) {
            pAddMenuItem("Lala Live Chat Overlay", OnToolsMenuClicked, nullptr);
            LogMessage("Successfully registered 'Tools -> Lala Live Chat Overlay' menu item");
        }
    }

    return true;
}

MODULE_EXPORT void obs_module_unload(void) {
    LogMessage("Unloading Lala Live Chat Overlay plugin");

    if (g_dockPanel) {
        g_dockPanel.reset();
    }

    if (g_dialog) {
        g_dialog->Hide();
        g_dialog.reset();
    }

    if (g_controller) {
        g_config = g_controller->GetConfig();
        g_config.saveToFile(g_configPath);
        g_controller->Shutdown();
        g_controller.reset();
    }
}

} // extern "C"
