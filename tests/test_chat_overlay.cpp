#include <windows.h>
#include <iostream>
#include <thread>
#include <chrono>
#include "../overlay/OverlayController.hpp"
#include "../models/ChatConfig.hpp"

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "=========================================================\n";
    std::cout << "   PHASE 4: LIVE CHAT READER -> OVERLAY INTEGRATION      \n";
    std::cout << "=========================================================\n";

    std::string streamUrl = "https://www.youtube.com/watch?v=JLCFuHgIoKY";
    if (argc > 1) {
        streamUrl = argv[1];
    }

    ChatConfig config;
    config.youtubeUrl = streamUrl;
    config.position = OverlayPosition::TopLeft;
    config.offsetX = 40;
    config.offsetY = 40;
    config.maxWidth = 460;
    config.chatDurationSeconds = 15;
    config.maxMessages = 8;
    config.fontSize = 16;
    config.opacity = 0.95f;
    config.animation = OverlayAnimation::Fade;

    OverlayController controller;

    controller.SetStatusCallback([](ChatProviderStatus s, const std::string& info) {
        std::cout << "[OVERLAY STATUS] " << StatusToString(s) << " - " << info << "\n";
    });

    std::cout << "[INFO] Initializing Overlay Controller...\n";
    if (!controller.Initialize(config)) {
        std::cerr << "[ERROR] Failed to initialize OverlayController!\n";
        return 1;
    }

    std::cout << "[INFO] Starting YouTube Live Chat Stream: " << streamUrl << "...\n";
    controller.StartChat(streamUrl);

    std::cout << "[INFO] Live chat is now streaming directly onto your transparent overlay!\n";
    std::cout << "[INFO] Running live integration for 15 seconds...\n";

    for (int i = 15; i > 0; --i) {
        std::cout << "  Live overlay active: " << controller.GetMessageCount() 
                  << " messages received (" << i << "s remaining)...\r" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n[INFO] Total messages received and rendered: " 
              << controller.GetMessageCount() << "\n";
    std::cout << "[INFO] Shutting down controller...\n";
    controller.Shutdown();
    std::cout << "[INFO] Phase 4 Integration Test Complete!\n";

    return 0;
}
