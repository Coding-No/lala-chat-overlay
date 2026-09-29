#include <windows.h>
#include <iostream>
#include <cassert>
#include "../overlay/OverlayController.hpp"
#include "../ui/SettingsDialog.hpp"
#include "../models/ChatConfig.hpp"

int main() {
    std::cout << "=========================================================" << std::endl;
    std::cout << "         TEST: SETTINGS DIALOG UI THREAD TEST            " << std::endl;
    std::cout << "=========================================================" << std::endl;

    ChatConfig config;
    OverlayController controller;
    bool initOk = controller.Initialize(config);
    std::cout << "[INFO] Controller Initialize: " << (initOk ? "OK" : "FAILED") << std::endl;
    assert(initOk);

    SettingsDialog dialog(controller);

    std::cout << "[INFO] Calling dialog.Show(nullptr)..." << std::endl;
    bool showOk = dialog.Show(nullptr);
    std::cout << "[INFO] dialog.Show(nullptr) returned: " << (showOk ? "TRUE" : "FALSE") << std::endl;
    assert(showOk);
    assert(dialog.IsOpen());

    std::cout << "[INFO] Dialog is open and responsive! Testing status update..." << std::endl;
    dialog.UpdateStatusText(ChatProviderStatus::Connected, "Test YouTube Stream");
    dialog.UpdateMessageCount(42);

    std::cout << "[INFO] Waiting 2 seconds to simulate user interaction..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "[INFO] Testing Hide()..." << std::endl;
    dialog.Hide();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(!dialog.IsOpen());
    std::cout << "[INFO] Dialog is hidden successfully." << std::endl;

    std::cout << "[INFO] Testing Show() again (re-opening from Tools menu)..." << std::endl;
    showOk = dialog.Show(nullptr);
    assert(showOk);
    assert(dialog.IsOpen());
    std::cout << "[INFO] Dialog re-opened successfully!" << std::endl;

    std::cout << "[INFO] Shutting down controller and dialog..." << std::endl;
    controller.Shutdown();

    std::cout << "[SUCCESS] SettingsDialog verification passed completely!" << std::endl;
    return 0;
}
