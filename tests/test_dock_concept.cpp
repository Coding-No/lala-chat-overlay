#include <windows.h>
#include <iostream>

typedef void* (*proc_QWindow_fromWinId)(unsigned __int64 winId);
typedef void* (*proc_QWidget_createWindowContainer)(void* window, void* parent, unsigned int flags);
typedef bool (*proc_obs_frontend_add_dock_by_id)(const char *id, const char *title, void *widget);

int main() {
    std::cout << "Testing OBS Dock & Qt6 symbol resolution..." << std::endl;

    SetDllDirectoryW(L"C:\\Program Files\\obs-studio\\bin\\64bit");

    HMODULE hGui = LoadLibraryW(L"C:\\Program Files\\obs-studio\\bin\\64bit\\Qt6Gui.dll");
    std::cout << "Load Qt6Gui: " << (hGui ? "OK" : "FAILED") << std::endl;

    HMODULE hWidgets = LoadLibraryW(L"C:\\Program Files\\obs-studio\\bin\\64bit\\Qt6Widgets.dll");
    std::cout << "Load Qt6Widgets: " << (hWidgets ? "OK" : "FAILED") << std::endl;

    HMODULE hFrontend = LoadLibraryW(L"C:\\Program Files\\obs-studio\\bin\\64bit\\obs-frontend-api.dll");
    std::cout << "Load obs-frontend-api: " << (hFrontend ? "OK" : "FAILED") << std::endl;

    if (!hGui || !hWidgets || !hFrontend) {
        return 1;
    }

    auto pFromWinId = (proc_QWindow_fromWinId)GetProcAddress(hGui, "?fromWinId@QWindow@@SAPEAV1@_K@Z");
    std::cout << "pFromWinId: " << (pFromWinId ? "FOUND" : "NULL") << std::endl;

    auto pCreateContainer = (proc_QWidget_createWindowContainer)GetProcAddress(hWidgets, "?createWindowContainer@QWidget@@SAPEAV1@PEAVQWindow@@PEAV1@V?$QFlags@W4WindowType@Qt@@@@@Z");
    std::cout << "pCreateContainer: " << (pCreateContainer ? "FOUND" : "NULL") << std::endl;

    auto pAddDock = (proc_obs_frontend_add_dock_by_id)GetProcAddress(hFrontend, "obs_frontend_add_dock_by_id");
    std::cout << "pAddDock: " << (pAddDock ? "FOUND" : "NULL") << std::endl;

    if (pFromWinId && pCreateContainer && pAddDock) {
        std::cout << "All symbols resolved successfully! Dock integration is 100% possible." << std::endl;
        return 0;
    }
    return 2;
}
