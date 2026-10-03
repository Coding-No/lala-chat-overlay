#include <iostream>
#include <chrono>
#include <thread>
#include <windows.h>
#include "../youtube/YouTubeChatProvider.hpp"
#include "../youtube/ChatDiscovery.hpp"

int main(int argc, char* argv[]) {
    // Enable UTF-8 console output
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "=========================================================\n";
    std::cout << "      PHASE 3: YOUTUBE CHAT READER STANDALONE TEST       \n";
    std::cout << "=========================================================\n";

    // Test 1: URL Parsing and Extraction
    std::cout << "\n[TEST 1] URL Validation and Extraction:\n";
    std::vector<std::string> testUrls = {
        "https://www.youtube.com/watch?v=JLCFuHgIoKY",
        "https://youtu.be/JLCFuHgIoKY",
        "https://www.youtube.com/live/JLCFuHgIoKY",
        "JLCFuHgIoKY",
        "https://invalid-url.com/something"
    };

    for (const auto& url : testUrls) {
        std::string vid = ChatDiscovery::ExtractVideoId(url);
        std::cout << "  URL: " << url << " -> Extracted Video ID: " 
                  << (vid.empty() ? "(INVALID)" : vid) << "\n";
    }

    // Test 2: Error Handling with Nonexistent Video
    std::cout << "\n[TEST 2] Graceful Error Handling (Nonexistent Video):\n";
    {
        YouTubeChatProvider invalidProvider;
        invalidProvider.SetStatusCallback([](ChatProviderStatus s, const std::string& info) {
            std::cout << "  Status Changed: [" << StatusToString(s) << "] " << info << "\n";
        });
        invalidProvider.Start("https://www.youtube.com/watch?v=00000000000");
        std::this_thread::sleep_for(std::chrono::seconds(3));
        invalidProvider.Stop();
        std::cout << "  Nonexistent video handled gracefully without crashing!\n";
    }

    // Test 3: Live Chat Streaming Test
    std::string targetUrl = "https://www.youtube.com/watch?v=JLCFuHgIoKY";
    if (argc > 1) {
        targetUrl = argv[1];
    }

    std::cout << "\n[TEST 3] Connecting to Live Chat: " << targetUrl << "\n";
    YouTubeChatProvider provider;

    provider.SetStatusCallback([](ChatProviderStatus status, const std::string& info) {
        std::cout << "[STATUS] " << StatusToString(status) << " - " << info << "\n";
    });

    provider.SetMessageCallback([](const ChatMessage& msg) {
        std::cout << ">>> [" << UserRoleToString(msg.role) << "] " 
                  << msg.authorName << ": " << msg.messageText << "\n";
        if (!msg.authorPhotoUrl.empty()) {
            std::cout << "    (Avatar: " << msg.authorPhotoUrl.substr(0, 45) << "...)\n";
        }
    });

    provider.SetAvatarCallback([](const std::string& url, const std::vector<uint8_t>& data) {
        // std::cout << "    [AVATAR DOWNLOADED] " << data.size() << " bytes\n";
    });

    provider.Start(targetUrl);

    std::cout << "[INFO] Listening for live chat messages for 12 seconds...\n";
    for (int i = 12; i > 0; --i) {
        std::cout << "  Monitoring live stream (" << i << "s remaining)...\r" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\n[INFO] Total messages received: " << provider.GetTotalMessagesReceived() << "\n";
    std::cout << "[INFO] Stopping provider...\n";
    provider.Stop();
    std::cout << "[INFO] Phase 3 Test Complete!\n";

    return 0;
}
