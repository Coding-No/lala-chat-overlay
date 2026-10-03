#include "OverlayController.hpp"
#include "../youtube/MessageParser.hpp"
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
    newMsg.targetAlpha = 1.0f;
    newMsg.isFadingOut = false;
    newMsg.isExpired = false;

    // Initialize animation properties based on configured effect
    switch (m_config.animation) {
        case OverlayAnimation::None:
            newMsg.currentAlpha = 1.0f;
            newMsg.animOffsetX = 0.0f;
            newMsg.animOffsetY = 0.0f;
            break;
        case OverlayAnimation::Fade:
            newMsg.currentAlpha = 0.0f;
            newMsg.animOffsetX = 0.0f;
            newMsg.animOffsetY = 0.0f;
            break;
        case OverlayAnimation::SlideUp:
            newMsg.currentAlpha = 0.0f;
            newMsg.animOffsetX = 0.0f;
            newMsg.animOffsetY = 45.0f;
            break;
        case OverlayAnimation::SlideDown:
            newMsg.currentAlpha = 0.0f;
            newMsg.animOffsetX = 0.0f;
            newMsg.animOffsetY = -45.0f;
            break;
        case OverlayAnimation::SlideLeft:
            newMsg.currentAlpha = 0.0f;
            newMsg.animOffsetX = 65.0f;
            newMsg.animOffsetY = 0.0f;
            break;
        case OverlayAnimation::SlideRight:
            newMsg.currentAlpha = 0.0f;
            newMsg.animOffsetX = -65.0f;
            newMsg.animOffsetY = 0.0f;
            break;
    }

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

    auto SetupMsgAnim = [this](ChatMessage& m, float initOffsetX, float initOffsetY) {
        m.receivedTime = std::chrono::steady_clock::now();
        m.targetAlpha = 1.0f;
        m.isFadingOut = false;
        m.isExpired = false;

        switch (m_config.animation) {
            case OverlayAnimation::None:
                m.currentAlpha = 1.0f;
                m.animOffsetX = 0.0f;
                m.animOffsetY = 0.0f;
                break;
            case OverlayAnimation::Fade:
                m.currentAlpha = 0.05f;
                m.animOffsetX = 0.0f;
                m.animOffsetY = 0.0f;
                break;
            case OverlayAnimation::SlideUp:
                m.currentAlpha = 0.05f;
                m.animOffsetX = 0.0f;
                m.animOffsetY = initOffsetY;
                break;
            case OverlayAnimation::SlideDown:
                m.currentAlpha = 0.05f;
                m.animOffsetX = 0.0f;
                m.animOffsetY = -initOffsetY;
                break;
            case OverlayAnimation::SlideLeft:
                m.currentAlpha = 0.05f;
                m.animOffsetX = initOffsetX;
                m.animOffsetY = 0.0f;
                break;
            case OverlayAnimation::SlideRight:
                m.currentAlpha = 0.05f;
                m.animOffsetX = -initOffsetX;
                m.animOffsetY = 0.0f;
                break;
        }
    };

    // Ensure preview custom badge and emote bitmaps are cached
    if (!m_renderer.HasImage("https://preview.local/member_badge.png")) {
        auto badgeBmp = std::make_shared<Gdiplus::Bitmap>(24, 24, PixelFormat32bppARGB);
        Gdiplus::Graphics bg(badgeBmp.get());
        bg.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush starBrush(Gdiplus::Color(255, 34, 197, 94)); // Emerald green star badge
        Gdiplus::PointF pts[5] = {
            { 12.0f, 2.0f }, { 15.0f, 8.5f }, { 22.0f, 9.5f }, { 17.0f, 14.5f }, { 19.0f, 21.0f }
        };
        bg.FillPolygon(&starBrush, pts, 5);
        m_renderer.CacheBitmap("https://preview.local/member_badge.png", badgeBmp);
    }

    if (!m_renderer.HasImage("https://preview.local/member_emote.png")) {
        auto emoteBmp = std::make_shared<Gdiplus::Bitmap>(32, 32, PixelFormat32bppARGB);
        Gdiplus::Graphics eg(emoteBmp.get());
        eg.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        Gdiplus::SolidBrush heartBrush(Gdiplus::Color(255, 239, 68, 68)); // Vivid heart emote
        eg.FillEllipse(&heartBrush, 4, 6, 12, 12);
        eg.FillEllipse(&heartBrush, 16, 6, 12, 12);
        Gdiplus::PointF triPts[3] = { { 4.0f, 13.0f }, { 28.0f, 13.0f }, { 16.0f, 26.0f } };
        eg.FillPolygon(&heartBrush, triPts, 3);
        m_renderer.CacheBitmap("https://preview.local/member_emote.png", emoteBmp);
    }

    ChatMessage m1;
    m1.id = "prev-1";
    m1.authorName = "Nopauw XP";
    m1.role = UserRole::Moderator;
    m1.messageText = MessageParser::FormatEmoticons("Halo semuanya! Selamat datang di live stream YouTube! <3 :fire:");
    SetupMsgAnim(m1, 70.0f, 50.0f);
    previewList.push_back(m1);

    ChatMessage m2;
    m2.id = "prev-2";
    m2.authorName = "Budi";
    m2.role = UserRole::Regular;
    m2.messageText = MessageParser::FormatEmoticons("Hadir bang! Gameplay mantap abis! :thumbsup: :sparkles:");
    SetupMsgAnim(m2, 70.0f, 50.0f);
    previewList.push_back(m2);

    ChatMessage m3;
    m3.id = "prev-3";
    m3.authorName = "Rina Gaming";
    m3.role = UserRole::Member;
    m3.badges.push_back({ "Member (6 months)", "Member", "https://preview.local/member_badge.png" });
    m3.customEmotes.push_back({ ":member_love:", "https://preview.local/member_emote.png" });
    m3.messageText = MessageParser::FormatEmoticons("Semangat streamingnya bre! :member_love: Mantap pol! :rocket:");
    SetupMsgAnim(m3, 70.0f, 50.0f);
    previewList.push_back(m3);

    ChatMessage m4;
    m4.id = "prev-4";
    m4.authorName = "SuperFan Indonesia";
    m4.role = UserRole::Member;
    m4.isMembershipEvent = true;
    m4.badges.push_back({ "New Member", "Member", "https://preview.local/member_badge.png" });
    m4.customEmotes.push_back({ ":member_love:", "https://preview.local/member_emote.png" });
    m4.messageText = "🎉 [Member Baru!] Baru saja bergabung menjadi Member Tier 1! \n :member_love: Halo semuanya salam kenal!";
    SetupMsgAnim(m4, 70.0f, 50.0f);
    previewList.push_back(m4);

    {
        std::lock_guard<std::mutex> lock(m_messagesMutex);
        m_activeMessages = previewList;
    }

    m_renderer.SetMessages(previewList);
    m_overlay.Show(true);
    m_overlay.Redraw();
}

void OverlayController::CyclePreviewMessage() {
    static int s_cycleIdx = 0;
    const char* names[] = { "Gamer_ID", "Siti Gaming", "SuperFan ID", "Aldo", "Nopauw XP" };
    UserRole roles[] = { UserRole::Regular, UserRole::Member, UserRole::Member, UserRole::Regular, UserRole::Moderator };
    const char* texts[] = {
        "Animasi teks & motion blur-nya bikin tampilan super smooth! :sparkles: :dash:",
        "Emoticon YouTube tetap hidup & font extra bold super jelas! :member_love: :fire:",
        "🎉 [Member Baru!] Baru saja bergabung ke Member Tier VIP! \n :member_love: Halo bang izin gabung!",
        "Tersimpan otomatis settingannya, ga perlu nyeting ulang lagi! :check: :thumbsup:",
        "Lala Live Chat Overlay mantap pol bre! Semangat streaming! :crown: :rocket:"
    };

    int idx = (s_cycleIdx++) % 5;
    ChatMessage cm;
    cm.id = "prev-cycle-" + std::to_string(s_cycleIdx);
    cm.authorName = names[idx];
    cm.role = roles[idx];
    if (idx == 1 || idx == 2) {
        cm.badges.push_back({ "Member", "Member", "https://preview.local/member_badge.png" });
        cm.customEmotes.push_back({ ":member_love:", "https://preview.local/member_emote.png" });
    }
    if (idx == 2) {
        cm.isMembershipEvent = true;
    }
    cm.messageText = MessageParser::FormatEmoticons(texts[idx]);
    OnMessageReceived(cm);
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
    auto lastPreviewCycle = std::chrono::steady_clock::now();

    while (m_running) {
        auto now = std::chrono::steady_clock::now();
        float dt = std::chrono::duration<float>(now - lastTick).count();
        lastTick = now;
        if (dt > 0.05f) dt = 0.05f; // Clamp delta time

        bool updated = false;

        // In Preview Mode, cycle a new animated preview message every 3.5 seconds
        if (m_previewMode) {
            auto previewElapsed = std::chrono::duration<float>(now - lastPreviewCycle).count();
            if (previewElapsed >= 3.5f) {
                lastPreviewCycle = now;
                CyclePreviewMessage();
                updated = true;
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_messagesMutex);

            for (auto it = m_activeMessages.begin(); it != m_activeMessages.end();) {
                auto elapsedSec = std::chrono::duration<float>(now - it->receivedTime).count();

                // Chat duration expiration (live mode only)
                if (!m_previewMode && m_config.chatDurationSeconds > 0 && elapsedSec >= m_config.chatDurationSeconds) {
                    it->isFadingOut = true;
                }

                // 1. Process fading out
                if (it->isFadingOut) {
                    it->currentAlpha -= dt * 2.5f;
                    if (m_config.animation == OverlayAnimation::SlideLeft) {
                        it->animOffsetX -= dt * 60.0f;
                    } else if (m_config.animation == OverlayAnimation::SlideRight) {
                        it->animOffsetX += dt * 60.0f;
                    } else if (m_config.animation == OverlayAnimation::SlideUp) {
                        it->animOffsetY -= dt * 40.0f;
                    } else if (m_config.animation == OverlayAnimation::SlideDown) {
                        it->animOffsetY += dt * 40.0f;
                    }
                    updated = true;
                    if (it->currentAlpha <= 0.01f) {
                        it = m_activeMessages.erase(it);
                        continue;
                    }
                } else {
                    // 2. Process fading in
                    if (it->currentAlpha < it->targetAlpha) {
                        float fadeSpeed = (m_config.animation == OverlayAnimation::None) ? 100.0f : 5.0f;
                        it->currentAlpha = (std::min)(it->targetAlpha, it->currentAlpha + dt * fadeSpeed);
                        updated = true;
                    }

                    // 3. Process slide easing (lerp towards 0, 0)
                    if (std::abs(it->animOffsetX) > 0.3f) {
                        it->animOffsetX += (0.0f - it->animOffsetX) * (std::min)(1.0f, dt * 10.0f);
                        if (std::abs(it->animOffsetX) <= 0.3f) it->animOffsetX = 0.0f;
                        updated = true;
                    }
                    if (std::abs(it->animOffsetY) > 0.3f) {
                        it->animOffsetY += (0.0f - it->animOffsetY) * (std::min)(1.0f, dt * 10.0f);
                        if (std::abs(it->animOffsetY) <= 0.3f) it->animOffsetY = 0.0f;
                        updated = true;
                    }
                }
                ++it;
            }

            // Auto-Hide (live mode only)
            if (!m_previewMode && m_config.autoHideSeconds > 0) {
                auto inactiveSec = std::chrono::duration<float>(now - m_lastMessageTime).count();
                if (inactiveSec >= m_config.autoHideSeconds && !m_activeMessages.empty()) {
                    m_overlay.Hide();
                }
            }
        }

        // Game Tracking
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
