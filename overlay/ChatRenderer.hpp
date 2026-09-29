#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "../models/ChatMessage.hpp"
#include "../models/ChatConfig.hpp"

class ChatRenderer {
public:
    ChatRenderer();
    ~ChatRenderer();

    void UpdateConfig(const ChatConfig& config);
    const ChatConfig& GetConfig() const { return m_config; }

    void SetMessages(const std::vector<ChatMessage>& messages);
    void Render(HDC hdc, int width, int height);

    // Image cache management
    void CacheAvatar(const std::string& url, const std::vector<uint8_t>& imageData);
    bool HasAvatar(const std::string& url);

private:
    void InitGdiplus();
    void ShutdownGdiplus();

    void DrawMessageItem(
        Gdiplus::Graphics& g, 
        const ChatMessage& msg, 
        int x, int y, int width, 
        int& outItemHeight,
        float alphaFactor
    );

    void DrawCircularAvatar(
        Gdiplus::Graphics& g, 
        const std::string& photoUrl, 
        const std::string& authorName, 
        UserRole role,
        int x, int y, int size,
        float alpha
    );

    void DrawBadge(
        Gdiplus::Graphics& g,
        UserRole role,
        int x, int y, int height,
        int& outBadgeWidth,
        float alpha
    );

    ChatConfig m_config;
    std::vector<ChatMessage> m_messages;
    std::mutex m_mutex;

    ULONG_PTR m_gdiplusToken{0};
    bool m_gdiplusInitialized{false};

    // Cached GDI+ Bitmaps for avatars
    std::unordered_map<std::string, std::shared_ptr<Gdiplus::Bitmap>> m_avatarCache;
    std::mutex m_cacheMutex;
};
