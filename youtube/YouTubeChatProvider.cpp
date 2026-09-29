#include "YouTubeChatProvider.hpp"
#include "ChatDiscovery.hpp"
#include "ChatParser.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>

static const int RECONNECT_DELAYS[] = {2, 5, 10, 20, 30};
static const int NUM_RECONNECT_DELAYS = sizeof(RECONNECT_DELAYS) / sizeof(RECONNECT_DELAYS[0]);

YouTubeChatProvider::YouTubeChatProvider() {
}

YouTubeChatProvider::~YouTubeChatProvider() {
    Stop();
}

bool YouTubeChatProvider::Start(const std::string& youtubeUrl) {
    Stop();

    m_targetUrl = youtubeUrl;
    m_running = true;
    m_totalMessages = 0;

    {
        std::lock_guard<std::mutex> lock(m_dedupMutex);
        m_seenMessageIds.clear();
        m_recentMessageIdQueue.clear();
    }

    SetStatus(ChatProviderStatus::Connecting, "Resolving YouTube livestream...");
    m_workerThread = std::thread(&YouTubeChatProvider::WorkerLoop, this);
    return true;
}

void YouTubeChatProvider::Stop() {
    if (!m_running) return;

    m_running = false;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    SetStatus(ChatProviderStatus::Disconnected, "Stopped by user");
}

void YouTubeChatProvider::SetStatus(ChatProviderStatus status, const std::string& info) {
    m_status = status;
    if (m_statusCb) {
        m_statusCb(status, info);
    }
}

void YouTubeChatProvider::FetchAvatarAsync(const std::string& url) {
    if (url.empty() || !m_avatarCb) return;

    // Run detached thread to fetch avatar binary data without blocking chat loop
    std::thread([this, url]() {
        HttpClient client;
        auto data = client.DownloadBinary(url);
        if (!data.empty() && m_avatarCb) {
            m_avatarCb(url, data);
        }
    }).detach();
}

void YouTubeChatProvider::WorkerLoop() {
    int reconnectIndex = 0;

    while (m_running) {
        // Step 1: Discover Chat Continuation
        SetStatus(ChatProviderStatus::Connecting, "Connecting to stream...");
        DiscoveryResult discovery = ChatDiscovery::Discover(m_httpClient, m_targetUrl);

        if (!discovery.success) {
            // Fatal validation errors do not endlessly retry
            if (discovery.errorMessage.find("Invalid YouTube URL") != std::string::npos ||
                discovery.errorMessage.find("Live chat is disabled") != std::string::npos) {
                SetStatus(ChatProviderStatus::Error, discovery.errorMessage);
                m_running = false;
                break;
            }

            int delaySec = RECONNECT_DELAYS[reconnectIndex];
            SetStatus(ChatProviderStatus::Reconnecting, 
                discovery.errorMessage + " (Retrying in " + std::to_string(delaySec) + "s)");

            reconnectIndex = (std::min)(reconnectIndex + 1, NUM_RECONNECT_DELAYS - 1);

            // Responsive sleep
            for (int i = 0; i < delaySec * 20 && m_running; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            continue;
        }

        m_videoId = discovery.videoId;
        std::string continuation = discovery.initialContinuation;
        std::string apiKey = discovery.apiKey;
        std::string clientVersion = discovery.clientVersion;

        SetStatus(ChatProviderStatus::Connected, "Connected to YouTube live chat");
        reconnectIndex = 0;

        // Step 2: Continuous Chat Polling Loop
        while (m_running && !continuation.empty()) {
            ChatBatchResult batch = ChatParser::FetchBatch(m_httpClient, apiKey, clientVersion, continuation);

            if (!batch.success) {
                SetStatus(ChatProviderStatus::Reconnecting, "Connection lost: " + batch.errorMessage);
                break; // Break inner loop to trigger reconnect / re-discovery
            }

            // Successfully received batch
            if (m_status != ChatProviderStatus::Connected) {
                SetStatus(ChatProviderStatus::Connected, "Connected to YouTube live chat");
            }

            // Process received messages with deduplication
            for (const auto& msg : batch.messages) {
                bool isNew = false;
                {
                    std::lock_guard<std::mutex> lock(m_dedupMutex);
                    if (m_seenMessageIds.find(msg.id) == m_seenMessageIds.end()) {
                        m_seenMessageIds.insert(msg.id);
                        m_recentMessageIdQueue.push_back(msg.id);

                        // Bound history to 2000 message IDs to prevent unbounded memory growth
                        if (m_recentMessageIdQueue.size() > 2000) {
                            m_seenMessageIds.erase(m_recentMessageIdQueue.front());
                            m_recentMessageIdQueue.pop_front();
                        }
                        isNew = true;
                    }
                }

                if (isNew) {
                    m_totalMessages++;
                    if (m_messageCb) {
                        m_messageCb(msg);
                    }
                    if (!msg.authorPhotoUrl.empty()) {
                        FetchAvatarAsync(msg.authorPhotoUrl);
                    }
                }
            }

            continuation = batch.nextContinuation;

            // Wait before next poll based on server requested timeout (clamped to 1000ms - 8000ms)
            int sleepMs = (std::clamp)(batch.nextTimeoutMs, 1000, 8000);
            for (int i = 0; i < sleepMs / 50 && m_running; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }

        // If we reached here while still running, apply backoff and re-discover
        if (m_running) {
            int delaySec = RECONNECT_DELAYS[reconnectIndex];
            SetStatus(ChatProviderStatus::Reconnecting, "Reconnecting in " + std::to_string(delaySec) + "s...");
            reconnectIndex = (std::min)(reconnectIndex + 1, NUM_RECONNECT_DELAYS - 1);

            for (int i = 0; i < delaySec * 20 && m_running; ++i) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }
}
