#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <unordered_set>
#include <deque>
#include "HttpClient.hpp"
#include "../models/ChatMessage.hpp"

enum class ChatProviderStatus {
    Disconnected,
    Connecting,
    Connected,
    Reconnecting,
    Error
};

inline const char* StatusToString(ChatProviderStatus status) {
    switch (status) {
        case ChatProviderStatus::Disconnected: return "Disconnected";
        case ChatProviderStatus::Connecting: return "Connecting";
        case ChatProviderStatus::Connected: return "Connected";
        case ChatProviderStatus::Reconnecting: return "Reconnecting";
        case ChatProviderStatus::Error: return "Error";
        default: return "Unknown";
    }
}

class YouTubeChatProvider {
public:
    using StatusCallback = std::function<void(ChatProviderStatus status, const std::string& info)>;
    using MessageCallback = std::function<void(const ChatMessage& msg)>;
    using AvatarCallback = std::function<void(const std::string& url, const std::vector<uint8_t>& data)>;

    YouTubeChatProvider();
    ~YouTubeChatProvider();

    bool Start(const std::string& youtubeUrl);
    void Stop();

    ChatProviderStatus GetStatus() const { return m_status; }
    std::string GetCurrentVideoId() const { return m_videoId; }
    uint64_t GetTotalMessagesReceived() const { return m_totalMessages; }

    void SetStatusCallback(StatusCallback cb) { m_statusCb = cb; }
    void SetMessageCallback(MessageCallback cb) { m_messageCb = cb; }
    void SetAvatarCallback(AvatarCallback cb) { m_avatarCb = cb; }

private:
    void WorkerLoop();
    void SetStatus(ChatProviderStatus status, const std::string& info = "");
    void FetchAvatarAsync(const std::string& url);

    std::string m_targetUrl;
    std::string m_videoId;
    std::atomic<ChatProviderStatus> m_status{ChatProviderStatus::Disconnected};
    std::atomic<bool> m_running{false};
    std::atomic<uint64_t> m_totalMessages{0};

    std::thread m_workerThread;
    HttpClient m_httpClient;

    // Deduplication set to protect against duplicate messages and spam
    std::unordered_set<std::string> m_seenMessageIds;
    std::deque<std::string> m_recentMessageIdQueue;
    std::mutex m_dedupMutex;

    StatusCallback m_statusCb;
    MessageCallback m_messageCb;
    AvatarCallback m_avatarCb;
};
