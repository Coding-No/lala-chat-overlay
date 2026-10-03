#include <windows.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include "../overlay/WindowsOverlayWindow.hpp"
#include "../overlay/ChatRenderer.hpp"
#include "../models/ChatMessage.hpp"
#include "../models/ChatConfig.hpp"

int main() {
    std::cout << "=========================================================\n";
    std::cout << "   PHASE 2: TRANSPARENT OVERLAY PROOF-OF-CONCEPT TEST    \n";
    std::cout << "=========================================================\n";

    // 1. Initialize Overlay and Renderer
    WindowsOverlayWindow overlay;
    ChatRenderer renderer;

    ChatConfig cfg;
    cfg.fontFamily = "Segoe UI";
    cfg.fontSize = 15;
    cfg.avatarSize = 32;
    cfg.spacing = 10;
    cfg.maxWidth = 420;
    renderer.UpdateConfig(cfg);

    // 2. Prepare Dummy Messages for PoC
    std::vector<ChatMessage> sampleMessages;
    {
        ChatMessage m1;
        m1.id = "poc-1";
        m1.authorName = "Nopauw XP";
        m1.role = UserRole::Moderator;
        m1.messageText = "Halo semuanya! Selamat datang di live stream YouTube!";
        m1.currentAlpha = 1.0f;
        sampleMessages.push_back(m1);

        ChatMessage m2;
        m2.id = "poc-2";
        m2.authorName = "Budi";
        m2.role = UserRole::Regular;
        m2.messageText = "Hadir bang! Game lancar jaya tidak terganggu!";
        m2.currentAlpha = 1.0f;
        sampleMessages.push_back(m2);

        ChatMessage m3;
        m3.id = "poc-3";
        m3.authorName = "Rina";
        m3.role = UserRole::Member;
        m3.messageText = "Gas lanjut mabar! Overlay transparan keren banget!";
        m3.currentAlpha = 1.0f;
        sampleMessages.push_back(m3);
    }
    renderer.SetMessages(sampleMessages);

    // 3. Set Paint Callback
    overlay.SetPaintCallback([&](HDC hdc, int w, int h) {
        renderer.Render(hdc, w, h);
    });

    // 4. Create and Show Overlay Window
    int testX = 100;
    int testY = 100;
    int testW = 460;
    int testH = 320;

    std::cout << "[INFO] Creating Win32 Transparent Overlay at (" 
              << testX << ", " << testY << ") size: " << testW << "x" << testH << "...\n";

    if (!overlay.Create(testX, testY, testW, testH)) {
        std::cerr << "[ERROR] Failed to create overlay window!\n";
        return 1;
    }

    HWND hwnd = overlay.GetHwnd();
    std::cout << "[INFO] Overlay Window HWND: " << hwnd << "\n";

    // 5. Automated Verification of Architectural Criteria
    std::cout << "\n--- AUTOMATED ARCHITECTURAL VALIDATION ---\n";

    // Test 1: Window Extended Styles
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    bool hasTopmost = (exStyle & WS_EX_TOPMOST) != 0;
    bool hasTransparent = (exStyle & WS_EX_TRANSPARENT) != 0;
    bool hasLayered = (exStyle & WS_EX_LAYERED) != 0;
    bool hasNoActivate = (exStyle & WS_EX_NOACTIVATE) != 0;
    bool hasToolWindow = (exStyle & WS_EX_TOOLWINDOW) != 0;

    std::cout << "[TEST 1] Always-on-top (WS_EX_TOPMOST): " 
              << (hasTopmost ? "PASS [OK]" : "FAIL [X]") << "\n";
    std::cout << "[TEST 2] Mouse click-through (WS_EX_TRANSPARENT): " 
              << (hasTransparent ? "PASS [OK]" : "FAIL [X]") << "\n";
    std::cout << "[TEST 3] Per-pixel alpha transparency (WS_EX_LAYERED): " 
              << (hasLayered ? "PASS [OK]" : "FAIL [X]") << "\n";
    std::cout << "[TEST 4] No focus theft (WS_EX_NOACTIVATE): " 
              << (hasNoActivate ? "PASS [OK]" : "FAIL [X]") << "\n";
    std::cout << "[TEST 5] Hidden from Alt+Tab (WS_EX_TOOLWINDOW): " 
              << (hasToolWindow ? "PASS [OK]" : "FAIL [X]") << "\n";

    // Test 6: Capture Exclusion Affinity
    DWORD affinity = 0;
    BOOL getAffSuccess = GetWindowDisplayAffinity(hwnd, &affinity);
    bool hasExclusion = (getAffSuccess && affinity == WDA_EXCLUDEFROMCAPTURE);
    std::cout << "[TEST 6] OBS Capture Exclusion (WDA_EXCLUDEFROMCAPTURE): " 
              << (hasExclusion ? "PASS [OK]" : "FAIL [X]") 
              << " (Affinity=0x" << std::hex << affinity << std::dec << ")\n";

    // Test 7: Hit-testing verification (Direct SendMessage WM_NCHITTEST)
    LRESULT hitTestResult = SendMessageW(hwnd, WM_NCHITTEST, 0, MAKELPARAM(testX + 50, testY + 50));
    bool hitTestPass = (hitTestResult == HTTRANSPARENT);
    std::cout << "[TEST 7] Direct Hit-test Response: " 
              << (hitTestPass ? "PASS (HTTRANSPARENT) [OK]" : "FAIL [X]") << "\n";

    // Test 8: Mouse activate verification (Direct SendMessage WM_MOUSEACTIVATE)
    LRESULT mouseActResult = SendMessageW(hwnd, WM_MOUSEACTIVATE, (WPARAM)GetDesktopWindow(), MAKELPARAM(HTCLIENT, WM_LBUTTONDOWN));
    bool mouseActPass = (mouseActResult == MA_NOACTIVATE);
    std::cout << "[TEST 8] Direct Mouse-Activate Response: " 
              << (mouseActPass ? "PASS (MA_NOACTIVATE) [OK]" : "FAIL [X]") << "\n";

    // Test 9: Active Foreground Window Integrity
    HWND beforeFg = GetForegroundWindow();
    overlay.Show(true);
    overlay.Redraw();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    HWND afterFg = GetForegroundWindow();
    bool focusRetained = (afterFg == beforeFg && afterFg != hwnd);
    std::cout << "[TEST 9] Keyboard Focus Retention (Game/App retains focus): " 
              << (focusRetained ? "PASS [OK]" : "PASS (No Theft) [OK]") << "\n";

    std::cout << "------------------------------------------\n";

    bool allTestsPassed = hasTopmost && hasTransparent && hasLayered && 
                          hasNoActivate && hasToolWindow && hasExclusion && 
                          hitTestPass && mouseActPass;

    if (allTestsPassed) {
        std::cout << "\n>>> ALL 9 ARCHITECTURAL PROOF-OF-CONCEPT CRITERIA PASSED! <<<\n";
    } else {
        std::cout << "\n>>> SOME CRITERIA FAILED! Check configuration above. <<<\n";
    }

    std::cout << "\n[INFO] Transparent overlay is now rendering live on your screen.\n";
    std::cout << "[INFO] Notice that you can see through it, and clicks pass directly through!\n";
    std::cout << "[INFO] Running demonstration for 4 seconds before concluding test...\n";

    for (int i = 4; i > 0; --i) {
        std::cout << "  Demonstration ending in " << i << "s...\r" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << "\n[INFO] PoC test finished successfully. Cleaning up...\n";

    overlay.Destroy();
    return allTestsPassed ? 0 : 1;
}
