#pragma once
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include "WindowsOverlayWindow.hpp"
#include "ChatRenderer.hpp"
#include "../youtube/YouTubeChatProvider.hpp"
#include "../models/ChatConfig.hpp"
#include "../models/ChatMessage.hpp"

class OverlayController {
public:
    OverlayController();
    ~OverlayController();

    bool Initialize(const ChatConfig& config);
    void Shutdown();

    void StartChat(const std::string& youtubeUrl);
    void StopChat();

    void UpdateConfig(const ChatConfig& config);
    const ChatConfig& GetConfig() const { return m_config; }

    void SetPreviewMode(bool preview);
    bool IsPreviewMode() const { return m_previewMode; }

    ChatProviderStatus GetStatus() const { return m_provider.GetStatus(); }
    uint64_t GetMessageCount() const { return m_provider.GetTotalMessagesReceived(); }

    void SetStatusCallback(std::function<void(ChatProviderStatus, const std::string&)> cb) {
        m_externalStatusCb = cb;
    }

private:
    void AnimationLoop();
    void RecalculateWindowPosition();
    void OnMessageReceived(const ChatMessage& msg);
    void OnAvatarDownloaded(const std::string& url, const std::vector<uint8_t>& data);
    void InjectPreviewMessages();
    void CyclePreviewMessage();
    void TrackGameWindow();

    ChatConfig m_config;
    WindowsOverlayWindow m_overlay;
    ChatRenderer m_renderer;
    YouTubeChatProvider m_provider;

    std::vector<ChatMessage> m_activeMessages;
    std::mutex m_messagesMutex;

    std::thread m_animThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_previewMode{false};
    std::atomic<bool> m_needsRedraw{false};

    std::chrono::steady_clock::time_point m_lastMessageTime;
    std::function<void(ChatProviderStatus, const std::string&)> m_externalStatusCb;
};
