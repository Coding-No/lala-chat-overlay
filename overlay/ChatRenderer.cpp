#include "ChatRenderer.hpp"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

ChatRenderer::ChatRenderer() {
    InitGdiplus();
}

ChatRenderer::~ChatRenderer() {
    ShutdownGdiplus();
}

void ChatRenderer::InitGdiplus() {
    if (!m_gdiplusInitialized) {
        GdiplusStartupInput input;
        GdiplusStartup(&m_gdiplusToken, &input, nullptr);
        m_gdiplusInitialized = true;
    }
}

void ChatRenderer::ShutdownGdiplus() {
    if (m_gdiplusInitialized) {
        m_avatarCache.clear();
        GdiplusShutdown(m_gdiplusToken);
        m_gdiplusInitialized = false;
    }
}

void ChatRenderer::UpdateConfig(const ChatConfig& config) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_config = config;
}

void ChatRenderer::SetMessages(const std::vector<ChatMessage>& messages) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_messages = messages;
}

void ChatRenderer::CacheAvatar(const std::string& url, const std::vector<uint8_t>& imageData) {
    if (url.empty() || imageData.empty()) return;

    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, imageData.size());
    if (!hMem) return;

    void* pMem = GlobalLock(hMem);
    if (!pMem) {
        GlobalFree(hMem);
        return;
    }
    memcpy(pMem, imageData.data(), imageData.size());
    GlobalUnlock(hMem);

    IStream* pStream = nullptr;
    if (CreateStreamOnHGlobal(hMem, TRUE, &pStream) == S_OK) {
        auto bmp = std::shared_ptr<Bitmap>(Bitmap::FromStream(pStream));
        pStream->Release();

        if (bmp && bmp->GetLastStatus() == Ok) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_avatarCache[url] = bmp;
        }
    }
}

bool ChatRenderer::HasAvatar(const std::string& url) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_avatarCache.find(url) != m_avatarCache.end();
}

static Color ArgbToGdiplusColor(uint32_t argb, float alphaScale = 1.0f) {
    BYTE a = static_cast<BYTE>(((argb >> 24) & 0xFF) * alphaScale);
    BYTE r = (argb >> 16) & 0xFF;
    BYTE g = (argb >> 8) & 0xFF;
    BYTE b = argb & 0xFF;
    return Color(a, r, g, b);
}

void ChatRenderer::DrawCircularAvatar(
    Graphics& g, 
    const std::string& photoUrl, 
    const std::string& authorName, 
    UserRole role,
    int x, int y, int size,
    float alpha
) {
    std::shared_ptr<Bitmap> avatarBmp;
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        auto it = m_avatarCache.find(photoUrl);
        if (it != m_avatarCache.end()) {
            avatarBmp = it->second;
        }
    }

    GraphicsContainer container = g.BeginContainer();
    GraphicsPath clipPath;
    clipPath.AddEllipse(x, y, size, size);
    g.SetClip(&clipPath);

    if (avatarBmp) {
        g.DrawImage(avatarBmp.get(), x, y, size, size);
    } else {
        // High quality fallback avatar: role color circle + initial
        Color bgCol = ArgbToGdiplusColor(0xFF334155, alpha); // Default dark slate
        if (role == UserRole::Owner) bgCol = ArgbToGdiplusColor(0xFFDC2626, alpha);
        else if (role == UserRole::Moderator) bgCol = ArgbToGdiplusColor(0xFF2563EB, alpha);
        else if (role == UserRole::Member) bgCol = ArgbToGdiplusColor(0xFF16A34A, alpha);
        else if (role == UserRole::Verified) bgCol = ArgbToGdiplusColor(0xFFD97706, alpha);

        SolidBrush brush(bgCol);
        g.FillEllipse(&brush, x, y, size, size);

        // Draw initial letter
        wchar_t initial = L'?';
        if (!authorName.empty()) {
            initial = static_cast<wchar_t>(toupper(authorName[authorName[0] == '@' ? 1 : 0]));
        }
        wchar_t str[2] = {initial, 0};

        FontFamily fontFamily(L"Segoe UI");
        Font font(&fontFamily, size * 0.45f, FontStyleBold, UnitPixel);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);

        SolidBrush textBrush(Color(static_cast<BYTE>(255 * alpha), 255, 255, 255));
        RectF r((REAL)x, (REAL)y, (REAL)size, (REAL)size);
        g.DrawString(str, 1, &font, r, &sf, &textBrush);
    }

    g.EndContainer(container);

    // Subtle outline ring around avatar
    Pen ringPen(Color(static_cast<BYTE>(100 * alpha), 255, 255, 255), 1.0f);
    g.DrawEllipse(&ringPen, x, y, size, size);
}

void ChatRenderer::DrawBadge(
    Graphics& g,
    UserRole role,
    int x, int y, int height,
    int& outBadgeWidth,
    float alpha
) {
    if (role == UserRole::Regular) {
        outBadgeWidth = 0;
        return;
    }

    const wchar_t* badgeText = L"";
    Color badgeBg;
    Color badgeFg(static_cast<BYTE>(255 * alpha), 255, 255, 255);

    switch (role) {
        case UserRole::Owner:
            badgeText = L"OWNER";
            badgeBg = ArgbToGdiplusColor(m_config.moderatorColor, alpha);
            break;
        case UserRole::Moderator:
            badgeText = L"MOD";
            badgeBg = ArgbToGdiplusColor(m_config.moderatorColor, alpha);
            break;
        case UserRole::Member:
            badgeText = L"MEMBER";
            badgeBg = ArgbToGdiplusColor(m_config.memberColor, alpha);
            break;
        case UserRole::Verified:
            badgeText = L"VERIFIED";
            badgeBg = ArgbToGdiplusColor(m_config.verifiedColor, alpha);
            break;
        default:
            outBadgeWidth = 0;
            return;
    }

    FontFamily fontFamily(L"Segoe UI");
    Font font(&fontFamily, height * 0.65f, FontStyleBold, UnitPixel);

    RectF boundingBox;
    StringFormat sf;
    g.MeasureString(badgeText, -1, &font, PointF(0, 0), &sf, &boundingBox);

    int padX = 6;
    int badgeW = static_cast<int>(boundingBox.Width) + padX * 2;
    int badgeH = height;

    // Draw rounded badge background
    GraphicsPath path;
    int radius = 4;
    path.AddArc(x, y, radius, radius, 180, 90);
    path.AddArc(x + badgeW - radius, y, radius, radius, 270, 90);
    path.AddArc(x + badgeW - radius, y + badgeH - radius, radius, radius, 0, 90);
    path.AddArc(x, y + badgeH - radius, radius, radius, 90, 90);
    path.CloseFigure();

    SolidBrush bgBrush(badgeBg);
    g.FillPath(&bgBrush, &path);

    // Text
    SolidBrush fgBrush(badgeFg);
    sf.SetAlignment(StringAlignmentCenter);
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF textRect((REAL)x, (REAL)y, (REAL)badgeW, (REAL)badgeH);
    g.DrawString(badgeText, -1, &font, textRect, &sf, &fgBrush);

    outBadgeWidth = badgeW;
}

void ChatRenderer::DrawMessageItem(
    Graphics& g, 
    const ChatMessage& msg, 
    int x, int y, int width, 
    int& outItemHeight,
    float alphaFactor
) {
    float effectiveAlpha = msg.currentAlpha * alphaFactor;
    if (effectiveAlpha <= 0.01f) {
        outItemHeight = 0;
        return;
    }

    int avatarSize = m_config.avatarSize;
    int textStartX = x + avatarSize + 10;
    int availableTextWidth = width - (avatarSize + 10);
    if (availableTextWidth < 100) availableTextWidth = 100;

    // 1. Prepare typography & measure
    std::wstring wFontFamily(m_config.fontFamily.begin(), m_config.fontFamily.end());
    FontFamily fontFamily(wFontFamily.c_str());
    if (fontFamily.GetLastStatus() != Ok) {
        fontFamily.GenericSansSerif();
    }

    INT userStyle = m_config.usernameBold ? FontStyleBold : FontStyleRegular;
    INT msgStyle = m_config.messageBold ? FontStyleBold : FontStyleRegular;

    Font userFont(&fontFamily, static_cast<REAL>(m_config.fontSize), userStyle, UnitPixel);
    Font msgFont(&fontFamily, static_cast<REAL>(m_config.fontSize), msgStyle, UnitPixel);

    // Convert strings to wide
    std::wstring wAuthor(msg.authorName.begin(), msg.authorName.end());
    std::wstring wText(msg.messageText.begin(), msg.messageText.end());

    // Measure Username
    RectF userBounds;
    g.MeasureString(wAuthor.c_str(), -1, &userFont, PointF(0, 0), &userBounds);

    int badgeEstimatedW = (msg.role != UserRole::Regular) ? 65 : 0;
    int msgY = y + static_cast<int>(userBounds.Height) + 3;

    RectF msgLayoutRect(
        (REAL)textStartX, 
        (REAL)msgY, 
        (REAL)availableTextWidth, 
        2000.0f
    );

    StringFormat sf;
    sf.SetTrimming(StringTrimmingWord);

    RectF msgMeasured;
    g.MeasureString(wText.c_str(), -1, &msgFont, msgLayoutRect, &sf, &msgMeasured);

    int totalTextH = static_cast<int>(userBounds.Height) + 3 + static_cast<int>(msgMeasured.Height);
    int itemH = (std::max)(avatarSize, totalTextH);

    // 2. Draw Message Background Card if enabled
    if (m_config.showBackground) {
        int padX = 8;
        int padY = 6;
        int cardX = x - padX;
        int cardY = y - padY;
        int maxTextRight = (std::max)(
            static_cast<int>(userBounds.Width) + badgeEstimatedW + 8,
            static_cast<int>(msgMeasured.Width)
        );
        int cardW = avatarSize + 10 + maxTextRight + padX * 2;
        if (cardW > width + padX * 2) cardW = width + padX * 2;
        if (cardW < 120) cardW = 120;
        int cardH = itemH + padY * 2;

        float bgAlpha = m_config.backgroundOpacity * effectiveAlpha;
        if (bgAlpha > 0.01f) {
            Color bgCol = ArgbToGdiplusColor(m_config.backgroundColor, bgAlpha);
            SolidBrush bgBrush(bgCol);

            GraphicsPath cardPath;
            int r = 8; // rounded radius
            cardPath.AddArc(cardX, cardY, r * 2, r * 2, 180, 90);
            cardPath.AddArc(cardX + cardW - r * 2, cardY, r * 2, r * 2, 270, 90);
            cardPath.AddArc(cardX + cardW - r * 2, cardY + cardH - r * 2, r * 2, r * 2, 0, 90);
            cardPath.AddArc(cardX, cardY + cardH - r * 2, r * 2, r * 2, 90, 90);
            cardPath.CloseFigure();

            g.FillPath(&bgBrush, &cardPath);

            Pen outlinePen(Color(static_cast<BYTE>(40 * bgAlpha), 255, 255, 255), 1.0f);
            g.DrawPath(&outlinePen, &cardPath);
        }
    }

    // 3. Draw circular avatar
    DrawCircularAvatar(g, msg.authorPhotoUrl, msg.authorName, msg.role, x, y, avatarSize, effectiveAlpha);

    // Color determination for username based on role
    uint32_t userColArgb = m_config.usernameColor;
    if (msg.role == UserRole::Owner || msg.role == UserRole::Moderator) {
        userColArgb = m_config.moderatorColor;
    } else if (msg.role == UserRole::Member) {
        userColArgb = m_config.memberColor;
    } else if (msg.role == UserRole::Verified) {
        userColArgb = m_config.verifiedColor;
    }

    Color userColor = ArgbToGdiplusColor(userColArgb, effectiveAlpha);
    Color msgColor = ArgbToGdiplusColor(m_config.messageColor, effectiveAlpha);
    Color shadowColor(static_cast<BYTE>(200 * effectiveAlpha), 0, 0, 0); // Drop shadow

    // 4. Draw Username with subtle shadow for crisp contrast over any game background
    SolidBrush shadowBrush(shadowColor);
    SolidBrush userBrush(userColor);

    // Shadow offset 1px
    g.DrawString(wAuthor.c_str(), -1, &userFont, PointF((REAL)textStartX + 1.0f, (REAL)y + 1.0f), &shadowBrush);
    g.DrawString(wAuthor.c_str(), -1, &userFont, PointF((REAL)textStartX, (REAL)y), &userBrush);

    // 5. Draw Role Badge next to username
    int badgeW = 0;
    int badgeH = static_cast<int>(userBounds.Height * 0.85f);
    int badgeX = textStartX + static_cast<int>(userBounds.Width) + 8;
    int badgeY = y + static_cast<int>((userBounds.Height - badgeH) / 2.0f);
    DrawBadge(g, msg.role, badgeX, badgeY, badgeH, badgeW, effectiveAlpha);

    // 6. Draw Message Text below username (word-wrapped)
    // Shadow offset 1px for message
    RectF shadowRect = msgLayoutRect;
    shadowRect.X += 1.0f;
    shadowRect.Y += 1.0f;
    SolidBrush msgBrush(msgColor);

    g.DrawString(wText.c_str(), -1, &msgFont, shadowRect, &sf, &shadowBrush);
    g.DrawString(wText.c_str(), -1, &msgFont, msgLayoutRect, &sf, &msgBrush);

    // Calculate total height of this chat item
    outItemHeight = itemH + (m_config.showBackground ? 6 : 0);
}

void ChatRenderer::Render(HDC hdc, int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    int curY = 0;
    int spacing = m_config.spacing;

    // Render active messages from queue
    for (const auto& msg : m_messages) {
        if (msg.isExpired) continue;

        int itemH = 0;
        DrawMessageItem(g, msg, 0, curY, width, itemH, 1.0f);
        if (itemH > 0) {
            curY += itemH + spacing;
        }

        if (curY >= height) break;
    }
}
