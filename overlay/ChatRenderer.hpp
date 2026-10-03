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

    // Image cache management (avatars, member badges, custom emotes)
    void CacheImage(const std::string& url, const std::vector<uint8_t>& imageData);
    bool HasImage(const std::string& url);
    std::shared_ptr<Gdiplus::Bitmap> GetImage(const std::string& url);

    void CacheAvatar(const std::string& url, const std::vector<uint8_t>& imageData) {
        CacheImage(url, imageData);
    }
    bool HasAvatar(const std::string& url) {
        return HasImage(url);
    }
    void CacheBitmap(const std::string& url, std::shared_ptr<Gdiplus::Bitmap> bmp) {
        if (url.empty() || !bmp) return;
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_imageCache[url] = bmp;
    }

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

    void DrawSingleMessagePass(
        Gdiplus::Graphics& g, 
        const ChatMessage& msg, 
        int x, int y, int width, 
        int& outItemHeight,
        float effectiveAlpha
    );

    void DrawMessageContentWithEmotes(
        Gdiplus::Graphics& g,
        const ChatMessage& msg,
        Gdiplus::Font& msgFont,
        int startX, int startY,
        int availableWidth,
        float effectiveAlpha,
        int& outContentH
    );

    int MeasureMessageContentWithEmotes(
        Gdiplus::Graphics& g,
        const ChatMessage& msg,
        Gdiplus::Font& msgFont,
        int availableWidth
    );

    int MeasureMessageItem(
        Gdiplus::Graphics& g,
        const ChatMessage& msg,
        int width
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
        const ChatMessage& msg,
        int x, int y, int height,
        int& outBadgeWidth,
        float alpha
    );

    ChatConfig m_config;
    std::vector<ChatMessage> m_messages;
    std::mutex m_mutex;

    ULONG_PTR m_gdiplusToken{0};
    bool m_gdiplusInitialized{false};

    // Cached GDI+ Bitmaps for avatars, badges, and custom emotes
    std::unordered_map<std::string, std::shared_ptr<Gdiplus::Bitmap>> m_imageCache;
    std::mutex m_cacheMutex;
};
