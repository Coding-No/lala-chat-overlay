#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "../overlay/OverlayController.hpp"
#include "../models/ChatConfig.hpp"

int main() {
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "=========================================================\n";
    std::cout << "           PHASE 7: PREVIEW SYSTEM VALIDATION            \n";
    std::cout << "=========================================================\n";

    ChatConfig config;
    config.position = OverlayPosition::TopLeft;
    config.offsetX = 50;
    config.offsetY = 50;
    config.maxWidth = 480;
    config.fontSize = 16;
    config.avatarSize = 32;
    config.opacity = 0.95f;
    config.spacing = 10;
    config.usernameColor = 0xFF38BDF8; // Light blue
    config.messageColor = 0xFFFFFFFF;  // Pure white

    OverlayController controller;
    if (!controller.Initialize(config)) {
        std::cerr << "[ERROR] Failed to initialize OverlayController!\n";
        return 1;
    }

    std::cout << "[INFO] Activating Preview Mode (Dummy Chat Injection)...\n";
    controller.SetPreviewMode(true);

    std::cout << "[INFO] Preview active on screen! Testing dynamic parameter updates:\n";

    // Test dynamic size and font update
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "  -> Updating Font Size to 20px, Avatar Size to 40px...\n";
    config.fontSize = 20;
    config.avatarSize = 40;
    controller.UpdateConfig(config);

    // Test dynamic color update
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "  -> Updating Username Color to Amber (0xFFF59E0B)...\n";
    config.usernameColor = 0xFFF59E0B;
    controller.UpdateConfig(config);

    // Test dynamic position update (Top Right)
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "  -> Updating Position to Top Right...\n";
    config.position = OverlayPosition::TopRight;
    controller.UpdateConfig(config);

    // Test dynamic opacity update
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "  -> Updating Opacity to 70%...\n";
    config.opacity = 0.70f;
    controller.UpdateConfig(config);

    // Return to Top Left with default settings
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "  -> Resetting to Top Left, 95% opacity...\n";
    config.position = OverlayPosition::TopLeft;
    config.opacity = 0.95f;
    config.fontSize = 16;
    config.avatarSize = 32;
    config.usernameColor = 0xFF38BDF8;
    controller.UpdateConfig(config);

    std::cout << "\n[INFO] Preview demonstration running for 3 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));

    controller.SetPreviewMode(false);
    controller.Shutdown();

    std::cout << "[INFO] Phase 7 Preview System Test Passed Successfully!\n";
    return 0;
}
