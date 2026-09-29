#include "OverlayController.hpp"
#include <algorithm>
#include <iostream>

OverlayController::OverlayController() {
}

OverlayController::~OverlayController() {
    Shutdown();
}

bool OverlayController::Initialize(const ChatConfig& config) {
    Shutdown();

    m_config = config;
    m_renderer.UpdateConfig(m_config);

    // Compute initial window placement
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int windowW = (std::min)(m_config.maxWidth, screenW);
    int windowH = (std::min)(800, screenH - 100);

    if (!m_overlay.Create(m_config.offsetX, m_config.offsetY, windowW, windowH)) {
        return false;
    }

    m_overlay.SetOpacity(m_config.opacity);
    RecalculateWindowPosition();

    m_overlay.SetPaintCallback([this](HDC hdc, int w, int h) {
        m_renderer.Render(hdc, w, h);
    });

    // Wire up provider callbacks
    m_provider.SetStatusCallback([this](ChatProviderStatus status, const std::string& info) {
        if (m_externalStatusCb) {
            m_externalStatusCb(status, info);
        }
    });

    m_provider.SetMessageCallback([this](const ChatMessage& msg) {
        OnMessageReceived(msg);
    });

    m_provider.SetAvatarCallback([this](const std::string& url, const std::vector<uint8_t>& data) {
        OnAvatarDownloaded(url, data);
    });

    m_running = true;
    m_lastMessageTime = std::chrono::steady_clock::now();
    m_animThread = std::thread(&OverlayController::AnimationLoop, this);

    m_overlay.Show(true);
    return true;
}

void OverlayController::Shutdown() {
    StopChat();

    if (m_running) {
        m_running = false;
        if (m_animThread.joinable()) {
            m_animThread.join();
        }
    }

    m_overlay.Destroy();
}

void OverlayController::StartChat(const std::string& youtubeUrl) {
    m_config.youtubeUrl = youtubeUrl;
    SetPreviewMode(false);

    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_activeMessages.clear();
    }
    m_renderer.SetMessages({});
    m_overlay.Redraw();

    m_provider.Start(youtubeUrl);
}

void OverlayController::StopChat() {
    m_provider.Stop();
}

void OverlayController::UpdateConfig(const ChatConfig& config) {
    m_config = config;
    m_renderer.UpdateConfig(config);
    m_overlay.SetOpacity(config.opacity);
    RecalculateWindowPosition();

    if (m_previewMode) {
        InjectPreviewMessages();
    } else {
        m_needsRedraw = true;
    }
}

void OverlayController::RecalculateWindowPosition() {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    int overlayW = m_config.maxWidth;
    int overlayH = 800; // Room for max messages

    int targetX = m_config.offsetX;
    int targetY = m_config.offsetY;

    switch (m_config.position) {
        case OverlayPosition::TopLeft:
            targetX = m_config.offsetX;
            targetY = m_config.offsetY;
            break;
        case OverlayPosition::TopCenter:
            targetX = (screenW - overlayW) / 2 + m_config.offsetX;
            targetY = m_config.offsetY;
            break;
        case OverlayPosition::TopRight:
            targetX = screenW - overlayW - m_config.offsetX;
            targetY = m_config.offsetY;
            break;
        case OverlayPosition::MiddleLeft:
            targetX = m_config.offsetX;
            targetY = (screenH - overlayH) / 2 + m_config.offsetY;
            break;
        case OverlayPosition::MiddleCenter:
            targetX = (screenW - overlayW) / 2 + m_config.offsetX;
            targetY = (screenH - overlayH) / 2 + m_config.offsetY;
            break;
        case OverlayPosition::MiddleRight:
            targetX = screenW - overlayW - m_config.offsetX;
            targetY = (screenH - overlayH) / 2 + m_config.offsetY;
            break;
        case OverlayPosition::BottomLeft:
            targetX = m_config.offsetX;
            targetY = screenH - overlayH - m_config.offsetY;
            break;
        case OverlayPosition::BottomCenter:
            targetX = (screenW - overlayW) / 2 + m_config.offsetX;
            targetY = screenH - overlayH - m_config.offsetY;
            break;
        case OverlayPosition::BottomRight:
            targetX = screenW - overlayW - m_config.offsetX;
            targetY = screenH - overlayH - m_config.offsetY;
            break;
    }

    m_overlay.SetBounds(targetX, targetY, overlayW, overlayH);
}

void OverlayController::OnMessageReceived(const ChatMessage& msg) {
    // Role Filtering
    if (m_config.filter == RoleFilter::ModeratorOnly) {
        if (msg.role != UserRole::Moderator && msg.role != UserRole::Owner) return;
    } else if (m_config.filter == RoleFilter::MemberOnly) {
        if (msg.role != UserRole::Member && msg.role != UserRole::Moderator && msg.role != UserRole::Owner) return;
    } else if (m_config.filter == RoleFilter::VerifiedOnly) {
        if (msg.role != UserRole::Verified && msg.role != UserRole::Moderator && msg.role != UserRole::Owner) return;
    } else if (m_config.filter == RoleFilter::RegularOnly) {
        if (msg.role != UserRole::Regular) return;
    }

    ChatMessage newMsg = msg;
    newMsg.receivedTime = std::chrono::steady_clock::now();
    newMsg.currentAlpha = (m_config.animation == OverlayAnimation::Fade) ? 0.05f : 1.0f;
    newMsg.targetAlpha = 1.0f;

    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_activeMessages.push_back(newMsg);

        // Limit active message count
        while (m_activeMessages.size() > static_cast<size_t>(m_config.maxMessages)) {
            m_activeMessages.erase(m_activeMessages.begin());
        }
    }

    m_lastMessageTime = std::chrono::steady_clock::now();
    m_overlay.Show(true);
    m_needsRedraw = true;
}

void OverlayController::OnAvatarDownloaded(const std::string& url, const std::vector<uint8_t>& data) {
    m_renderer.CacheAvatar(url, data);
    m_needsRedraw = true;
}

void OverlayController::InjectPreviewMessages() {
    std::vector<ChatMessage> previewList;

    ChatMessage m1;
    m1.id = "prev-1";
    m1.authorName = "Nopauw XP";
    m1.role = UserRole::Moderator;
    m1.messageText = "Halo semuanya! Selamat datang di live stream YouTube!";
    m1.currentAlpha = 1.0f;
    previewList.push_back(m1);

    ChatMessage m2;
    m2.id = "prev-2";
    m2.authorName = "Budi";
    m2.role = UserRole::Regular;
    m2.messageText = "Hadir bang! Gameplay mantap!";
    m2.currentAlpha = 1.0f;
    previewList.push_back(m2);

    ChatMessage m3;
    m3.id = "prev-3";
    m3.authorName = "Rina";
    m3.role = UserRole::Member;
    m3.messageText = "Gas lanjut mabar! Semangat streamingnya!";
    m3.currentAlpha = 1.0f;
    previewList.push_back(m3);

    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_activeMessages = previewList;
    }

    m_renderer.SetMessages(previewList);
    m_overlay.Show(true);
    m_overlay.Redraw();
}

void OverlayController::SetPreviewMode(bool preview) {
    m_previewMode = preview;
    if (preview) {
        InjectPreviewMessages();
    } else {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_activeMessages.clear();
        m_renderer.SetMessages({});
        m_overlay.Redraw();
    }
}

void OverlayController::TrackGameWindow() {
    if (!m_config.autoFollowGame) return;

    HWND fg = GetForegroundWindow();
    if (!fg || fg == m_overlay.GetHwnd()) return;

    RECT rect;
    if (GetWindowRect(fg, &rect)) {
        int w = rect.right - rect.left;
        int h = rect.bottom - rect.top;
        // If foreground window is reasonably large (game window)
        if (w >= 640 && h >= 480) {
            int newX = rect.left + m_config.offsetX;
            int newY = rect.top + m_config.offsetY;
            if (abs(newX - m_overlay.GetX()) > 5 || abs(newY - m_overlay.GetY()) > 5) {
                m_overlay.SetBounds(newX, newY, m_config.maxWidth, 800);
            }
        }
    }
}

void OverlayController::AnimationLoop() {
    auto lastTick = std::chrono::steady_clock::now();

    while (m_running) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;

        bool updated = false;

        if (!m_previewMode) {
            std::lock_guard<std::mutex> lock(m_messagesMutex);

            // 1. Process Fade-In and Fade-Out
            for (auto it = m_activeMessages.begin(); it != m_activeMessages.end();) {
                auto elapsedSec = std::chrono::duration<float>(now - it->receivedTime).count();

                // Check chat duration expiration
                if (m_config.chatDurationSeconds > 0 && elapsedSec >= m_config.chatDurationSeconds) {
                    it->isFadingOut = true;
                }

                if (it->isFadingOut) {
                    it->currentAlpha -= dt * 2.0f; // Fade out over 0.5s
                    updated = true;
                    if (it->currentAlpha <= 0.0f) {
                        it = m_activeMessages.erase(it);
                        continue;
                    }
                } else if (it->currentAlpha < it->targetAlpha) {
                    it->currentAlpha = (std::min)(it->targetAlpha, it->currentAlpha + dt * 4.0f);
                    updated = true;
                }
                ++it;
            }

            // 2. Process Auto-Hide
            if (m_config.autoHideSeconds > 0) {
                auto inactiveSec = std::chrono::duration<float>(now - m_lastMessageTime).count();
                if (inactiveSec >= m_config.autoHideSeconds && !m_activeMessages.empty()) {
                    m_overlay.Hide();
                }
            }
        }

        // 3. Process Game Tracking
        if (m_config.autoFollowGame) {
            TrackGameWindow();
        }

        if (updated || m_needsRedraw) {
            m_needsRedraw = false;
            {
                std::lock_guard<std::mutex> lock(m_messagesMutex);
                m_renderer.SetMessages(m_activeMessages);
            }
            m_overlay.Redraw();
        }

        // ~60 FPS rate limit
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}
