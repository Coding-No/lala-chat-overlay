#include "obs-module-compat.h"
#include "../overlay/OverlayController.hpp"
#include "../ui/SettingsDialog.hpp"
#include "../ui/DockPanel.hpp"
#include "../models/ChatConfig.hpp"
#include <memory>
#include <fstream>
#include <shlobj.h>

// ============================================================================
// Qt function pointer types (used for dock registration via runtime linking)
// These work identically for Qt5 and Qt6 — only the DLL names differ.
// ============================================================================
typedef void* (*proc_QWindow_fromWinId)(unsigned __int64 winId);
typedef void* (*proc_QWidget_createWindowContainer)(void* window, void* parent, unsigned int flags);
typedef bool (*proc_obs_frontend_add_dock_by_id)(const char *id, const char *title, void *widget);

// ============================================================================
// Mangled symbol names for QWindow::fromWinId and QWidget::createWindowContainer
// Qt5 and Qt6 have different mangling due to ABI changes.
// We try multiple known manglings per Qt version for maximum compatibility.
// ============================================================================

// Qt6 MSVC x64 mangled names
static const char* QT6_FROM_WIN_ID_SYMBOLS[] = {
    "?fromWinId@QWindow@@SAPEAV1@_K@Z",
    nullptr
};

static const char* QT6_CREATE_CONTAINER_SYMBOLS[] = {
    "?createWindowContainer@QWidget@@SAPEAV1@PEAVQWindow@@PEAV1@V?$QFlags@W4WindowType@Qt@@@@@Z",
    nullptr
};

// Qt5 MSVC x64 mangled names
static const char* QT5_FROM_WIN_ID_SYMBOLS[] = {
    "?fromWinId@QWindow@@SAPEAV1@_K@Z",             // Qt 5.12+
    "?fromWinId@QWindow@@SAPEAV1@_K@Z",             // Most Qt5 versions use same signature
    nullptr
};

static const char* QT5_CREATE_CONTAINER_SYMBOLS[] = {
    "?createWindowContainer@QWidget@@SAPEAV1@PEAVQWindow@@PEAV1@V?$QFlags@W4WindowType@Qt@@@@@Z",
    nullptr
};

// Try multiple symbol names until one resolves
static FARPROC TryGetProcAddress(HMODULE hModule, const char** symbolList) {
    if (!hModule || !symbolList) return nullptr;
    for (int i = 0; symbolList[i] != nullptr; i++) {
        FARPROC proc = GetProcAddress(hModule, symbolList[i]);
        if (proc) return proc;
    }
    return nullptr;
}

// ============================================================================
// Module globals
// ============================================================================
static obs_module_t* g_obsModule = nullptr;
static std::unique_ptr<OverlayController> g_controller;
static std::unique_ptr<SettingsDialog> g_dialog;
static std::unique_ptr<DockPanel> g_dockPanel;
static void* g_qWidgetDock = nullptr;
static ChatConfig g_config;
static std::string g_configPath;

// Track which Qt DLLs we loaded (so we don't FreeLibrary DLLs we didn't load)
static HMODULE g_loadedGui = nullptr;
static HMODULE g_loadedWidgets = nullptr;
static HMODULE g_loadedFrontend = nullptr;

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
        if (g_dockPanel && g_dockPanel->GetHwnd() && IsWindow(g_dockPanel->GetHwnd())) {
            g_dockPanel->SyncToController();
        }
        LogMessage("Calling g_dialog->Show(nullptr)...");
        bool ok = g_dialog->Show(nullptr);
        LogMessage(std::string("Dialog Show() result: ") + (ok ? "SUCCESS" : "FAILED"));
    }
}

// ============================================================================
// Attempt to resolve Qt + OBS Frontend modules and register the dock panel.
// Tries Qt6 first, then Qt5. Returns true if dock was registered.
// ============================================================================
struct QtModules {
    HMODULE hGui = nullptr;
    HMODULE hWidgets = nullptr;
    HMODULE hFrontend = nullptr;
    bool isQt6 = false;
    bool weLoadedGui = false;
    bool weLoadedWidgets = false;
    bool weLoadedFrontend = false;
};

static bool TryLoadQtModules(QtModules& qt) {
    // ---- Try Qt6 first (OBS 28+) ----
    qt.hGui = GetModuleHandleW(L"Qt6Gui.dll");
    qt.hWidgets = GetModuleHandleW(L"Qt6Widgets.dll");

    if (qt.hGui && qt.hWidgets) {
        qt.isQt6 = true;
        LogMessage("Detected Qt6 (already loaded by OBS)");
    } else {
        // ---- Try Qt5 (OBS 25-27) ----
        qt.hGui = GetModuleHandleW(L"Qt5Gui.dll");
        qt.hWidgets = GetModuleHandleW(L"Qt5Widgets.dll");

        if (qt.hGui && qt.hWidgets) {
            qt.isQt6 = false;
            LogMessage("Detected Qt5 (already loaded by OBS)");
        } else {
            // Last resort: try loading Qt6 explicitly
            qt.hGui = LoadLibraryW(L"Qt6Gui.dll");
            qt.hWidgets = LoadLibraryW(L"Qt6Widgets.dll");
            if (qt.hGui && qt.hWidgets) {
                qt.isQt6 = true;
                qt.weLoadedGui = true;
                qt.weLoadedWidgets = true;
                LogMessage("Loaded Qt6 DLLs explicitly");
            } else {
                // Try loading Qt5 explicitly
                if (qt.hGui) { FreeLibrary(qt.hGui); qt.hGui = nullptr; }
                if (qt.hWidgets) { FreeLibrary(qt.hWidgets); qt.hWidgets = nullptr; }

                qt.hGui = LoadLibraryW(L"Qt5Gui.dll");
                qt.hWidgets = LoadLibraryW(L"Qt5Widgets.dll");
                if (qt.hGui && qt.hWidgets) {
                    qt.isQt6 = false;
                    qt.weLoadedGui = true;
                    qt.weLoadedWidgets = true;
                    LogMessage("Loaded Qt5 DLLs explicitly");
                } else {
                    if (qt.hGui) { FreeLibrary(qt.hGui); qt.hGui = nullptr; }
                    if (qt.hWidgets) { FreeLibrary(qt.hWidgets); qt.hWidgets = nullptr; }
                    LogMessage("Warning: No Qt modules found (Qt5 or Qt6). Dock panel will not be available.");
                    return false;
                }
            }
        }
    }

    // ---- Frontend API ----
    qt.hFrontend = GetModuleHandleW(L"obs-frontend-api.dll");
    if (!qt.hFrontend) {
        qt.hFrontend = LoadLibraryW(L"obs-frontend-api.dll");
        if (qt.hFrontend) {
            qt.weLoadedFrontend = true;
        }
    }
    if (!qt.hFrontend) {
        LogMessage("Warning: obs-frontend-api.dll not found. Tools menu and dock will not be available.");
        return false;
    }

    return true;
}

static bool TryRegisterDock(const QtModules& qt) {
    if (!qt.hGui || !qt.hWidgets || !qt.hFrontend) return false;

    // Select correct mangled symbols based on Qt version
    const char** fromWinIdSymbols = qt.isQt6 ? QT6_FROM_WIN_ID_SYMBOLS : QT5_FROM_WIN_ID_SYMBOLS;
    const char** createContainerSymbols = qt.isQt6 ? QT6_CREATE_CONTAINER_SYMBOLS : QT5_CREATE_CONTAINER_SYMBOLS;

    auto pFromWinId = (proc_QWindow_fromWinId)TryGetProcAddress(qt.hGui, fromWinIdSymbols);
    auto pCreateContainer = (proc_QWidget_createWindowContainer)TryGetProcAddress(qt.hWidgets, createContainerSymbols);
    auto pAddDock = (proc_obs_frontend_add_dock_by_id)GetProcAddress(qt.hFrontend, "obs_frontend_add_dock_by_id");

    if (!pFromWinId) {
        LogMessage("Warning: QWindow::fromWinId not resolved from " + std::string(qt.isQt6 ? "Qt6" : "Qt5"));
        return false;
    }
    if (!pCreateContainer) {
        LogMessage("Warning: QWidget::createWindowContainer not resolved from " + std::string(qt.isQt6 ? "Qt6" : "Qt5"));
        return false;
    }
    if (!pAddDock) {
        LogMessage("Warning: obs_frontend_add_dock_by_id not found (this OBS version may not support custom docks)");
        return false;
    }

    HWND dockHwnd = g_dockPanel->Create(nullptr);
    if (!dockHwnd) {
        LogMessage("Warning: DockPanel::Create failed");
        return false;
    }

    void* qWindow = pFromWinId((unsigned __int64)dockHwnd);
    if (!qWindow) {
        LogMessage("Warning: QWindow::fromWinId returned null");
        return false;
    }

    g_qWidgetDock = pCreateContainer(qWindow, nullptr, 0);
    if (!g_qWidgetDock) {
        LogMessage("Warning: QWidget::createWindowContainer returned null");
        return false;
    }

    bool dockOk = pAddDock("lala_chat_overlay_dock", "Lala Live Chat Overlay", g_qWidgetDock);
    LogMessage(std::string("Registered 'Docks -> Lala Live Chat Overlay': ") + (dockOk ? "SUCCESS" : "FAILED"));
    return dockOk;
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

    // ======================================================================
    // 1. Try to register OBS Dock (Menu: Docks -> Lala Live Chat Overlay)
    //    Supports both Qt5 (OBS 25-27) and Qt6 (OBS 28+) automatically.
    //    If dock registration fails, the plugin still works via Tools menu.
    // ======================================================================
    QtModules qt = {};
    bool dockRegistered = false;

    if (TryLoadQtModules(qt)) {
        dockRegistered = TryRegisterDock(qt);

        // Track modules we loaded ourselves (for cleanup)
        if (qt.weLoadedGui) g_loadedGui = qt.hGui;
        if (qt.weLoadedWidgets) g_loadedWidgets = qt.hWidgets;
        if (qt.weLoadedFrontend) g_loadedFrontend = qt.hFrontend;
    }

    if (!dockRegistered) {
        LogMessage("Dock panel not available for this OBS version. Plugin accessible via Tools menu.");
    }

    // ======================================================================
    // 2. Register Tools menu item (Menu: Tools -> Lala Live Chat Overlay)
    //    This works on ALL OBS versions and is the primary fallback.
    // ======================================================================
    HMODULE hFrontend = qt.hFrontend;
    if (!hFrontend) hFrontend = GetModuleHandleW(L"obs-frontend-api.dll");

    if (hFrontend) {
        auto pAddMenuItem = (proc_obs_frontend_add_tools_menu_item)GetProcAddress(hFrontend, "obs_frontend_add_tools_menu_item");
        if (pAddMenuItem) {
            pAddMenuItem("Lala Live Chat Overlay", OnToolsMenuClicked, nullptr);
            LogMessage("Successfully registered 'Tools -> Lala Live Chat Overlay' menu item");
        } else {
            LogMessage("Warning: obs_frontend_add_tools_menu_item not found");
        }
    } else {
        LogMessage("Warning: obs-frontend-api.dll not available for Tools menu registration");
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

    g_qWidgetDock = nullptr;

    // Free DLLs we explicitly loaded
    if (g_loadedFrontend) { FreeLibrary(g_loadedFrontend); g_loadedFrontend = nullptr; }
    if (g_loadedWidgets)  { FreeLibrary(g_loadedWidgets);  g_loadedWidgets = nullptr; }
    if (g_loadedGui)      { FreeLibrary(g_loadedGui);      g_loadedGui = nullptr; }
}

} // extern "C"
