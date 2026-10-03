#include "ChatRenderer.hpp"
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

static std::wstring Utf8ToWide(const std::string& str) {
    if (str.empty()) return std::wstring();
    int count = MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), nullptr, 0);
    if (count <= 0) return std::wstring(str.begin(), str.end());
    std::wstring wstr(count, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), &wstr[0], count);
    return wstr;
}

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
        m_imageCache.clear();
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

void ChatRenderer::CacheImage(const std::string& url, const std::vector<uint8_t>& imageData) {
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
            m_imageCache[url] = bmp;
        }
    }
}

bool ChatRenderer::HasImage(const std::string& url) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    return m_imageCache.find(url) != m_imageCache.end();
}

std::shared_ptr<Bitmap> ChatRenderer::GetImage(const std::string& url) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    auto it = m_imageCache.find(url);
    if (it != m_imageCache.end()) {
        return it->second;
    }
    return nullptr;
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
    std::shared_ptr<Bitmap> avatarBmp = GetImage(photoUrl);

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
    const ChatMessage& msg,
    int x, int y, int height,
    int& outBadgeWidth,
    float alpha
) {
    UserRole role = msg.role;
    if (msg.isMembershipEvent && role == UserRole::Regular) {
        role = UserRole::Member;
    }

    if (role == UserRole::Regular && msg.badges.empty() && !msg.isMembershipEvent) {
        outBadgeWidth = 0;
        return;
    }

    // Check for cached custom badge image (e.g. member loyalty badge thumbnail)
    std::shared_ptr<Bitmap> customBadgeBmp = nullptr;
    for (const auto& b : msg.badges) {
        if (!b.iconUrl.empty()) {
            auto bmp = GetImage(b.iconUrl);
            if (bmp) {
                customBadgeBmp = bmp;
                break;
            }
        }
    }

    const wchar_t* badgeText = L"";
    Color badgeBg;
    Color badgeFg(static_cast<BYTE>(255 * alpha), 255, 255, 255);

    switch (role) {
        case UserRole::Owner:
            badgeText = L"OWNER";
            badgeBg = ArgbToGdiplusColor(0xFFDC2626, alpha);
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
            if (customBadgeBmp || msg.isMembershipEvent) {
                badgeText = L"MEMBER";
                badgeBg = ArgbToGdiplusColor(m_config.memberColor, alpha);
            } else {
                outBadgeWidth = 0;
                return;
            }
            break;
    }

    FontFamily fontFamily(L"Segoe UI");
    Font font(&fontFamily, height * 0.65f, FontStyleBold, UnitPixel);

    RectF boundingBox;
    StringFormat sf;
    g.MeasureString(badgeText, -1, &font, PointF(0, 0), &sf, &boundingBox);

    int padX = 6;
    int iconSize = (height > 4) ? (height - 4) : height;
    int iconSpacing = customBadgeBmp ? (iconSize + 4) : 0;
    int badgeW = static_cast<int>(boundingBox.Width) + iconSpacing + padX * 2;
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

    // Draw custom badge thumbnail if available
    int textStartX = x + padX;
    if (customBadgeBmp) {
        int iconY = y + (badgeH - iconSize) / 2;
        g.DrawImage(customBadgeBmp.get(), x + padX, iconY, iconSize, iconSize);
        textStartX += iconSpacing;
    }

    // Text
    SolidBrush fgBrush(badgeFg);
    sf.SetAlignment(StringAlignmentNear);
    sf.SetLineAlignment(StringAlignmentCenter);
    RectF textRect((REAL)textStartX, (REAL)y, (REAL)boundingBox.Width + 4, (REAL)badgeH);
    g.DrawString(badgeText, -1, &font, textRect, &sf, &fgBrush);

    outBadgeWidth = badgeW;
}

struct ContentToken {
    bool isEmote{false};
    std::shared_ptr<Bitmap> bmp{nullptr};
    std::wstring text;
    float width{0.0f};
    float height{0.0f};
};

struct ContentLine {
    std::vector<ContentToken> tokens;
};

static std::vector<ContentLine> BuildContentTokens(
    ChatRenderer* renderer,
    Graphics& g,
    const ChatMessage& msg,
    Font& msgFont,
    float emoteSize,
    float& outSpaceW
) {
    StringFormat sfMeasure(StringFormat::GenericTypographic());
    sfMeasure.SetFormatFlags(StringFormatFlagsNoClip | StringFormatFlagsMeasureTrailingSpaces);

    RectF spaceBox;
    g.MeasureString(L" ", 1, &msgFont, PointF(0, 0), &sfMeasure, &spaceBox);
    outSpaceW = (std::max)(4.0f, spaceBox.Width);

    std::vector<std::string> rawLines;
    {
        std::string cur;
        for (char c : msg.messageText) {
            if (c == '\r') continue;
            if (c == '\n') {
                rawLines.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        rawLines.push_back(cur);
    }

    std::vector<ContentLine> lines;
    for (const auto& rawLine : rawLines) {
        ContentLine line;
        std::stringstream ss(rawLine);
        std::string word;
        while (ss >> word) {
            bool matchedEmote = false;
            for (const auto& ce : msg.customEmotes) {
                if (ce.shortcut.empty()) continue;
                if (word == ce.shortcut) {
                    ContentToken token;
                    token.isEmote = true;
                    token.bmp = renderer->GetImage(ce.imageUrl);
                    token.width = emoteSize;
                    token.height = emoteSize;
                    line.tokens.push_back(token);
                    matchedEmote = true;
                    break;
                } else if (word.rfind(ce.shortcut, 0) == 0) {
                    ContentToken token;
                    token.isEmote = true;
                    token.bmp = renderer->GetImage(ce.imageUrl);
                    token.width = emoteSize;
                    token.height = emoteSize;
                    line.tokens.push_back(token);

                    std::string trailing = word.substr(ce.shortcut.size());
                    if (!trailing.empty()) {
                        ContentToken trailToken;
                        trailToken.isEmote = false;
                        trailToken.text = Utf8ToWide(trailing);
                        RectF trailBox;
                        g.MeasureString(trailToken.text.c_str(), -1, &msgFont, PointF(0, 0), &sfMeasure, &trailBox);
                        trailToken.width = (std::max)(1.0f, trailBox.Width);
                        trailToken.height = (std::max)(1.0f, trailBox.Height);
                        line.tokens.push_back(trailToken);
                    }
                    matchedEmote = true;
                    break;
                }
            }

            if (!matchedEmote) {
                ContentToken token;
                token.isEmote = false;
                token.text = Utf8ToWide(word);
                RectF wordBox;
                g.MeasureString(token.text.c_str(), -1, &msgFont, PointF(0, 0), &sfMeasure, &wordBox);
                token.width = (std::max)(1.0f, wordBox.Width);
                token.height = (std::max)(1.0f, wordBox.Height);
                line.tokens.push_back(token);
            }
        }
        lines.push_back(line);
    }
    return lines;
}

int ChatRenderer::MeasureMessageContentWithEmotes(
    Graphics& g,
    const ChatMessage& msg,
    Font& msgFont,
    int availableWidth
) {
    float emoteSize = (std::max)(16.0f, std::round(static_cast<REAL>(m_config.fontSize) * 1.35f));
    float spaceW = 4.0f;
    auto lines = BuildContentTokens(this, g, msg, msgFont, emoteSize, spaceW);

    float fontH = static_cast<REAL>(m_config.fontSize);
    float lineHeight = (std::max)(fontH * 1.40f, emoteSize + 2.0f);
    float curY = 0.0f;

    for (const auto& line : lines) {
        float curX = 0.0f;
        for (const auto& token : line.tokens) {
            if (curX + token.width > availableWidth && curX > 0.0f) {
                curX = 0.0f;
                curY += lineHeight;
            }
            curX += token.width + spaceW;
        }
        curY += lineHeight;
    }

    int result = static_cast<int>(std::ceil(curY));
    return (std::max)(result, static_cast<int>(lineHeight));
}

void ChatRenderer::DrawMessageContentWithEmotes(
    Graphics& g,
    const ChatMessage& msg,
    Font& msgFont,
    int startX, int startY,
    int availableWidth,
    float effectiveAlpha,
    int& outContentH
) {
    float emoteSize = (std::max)(16.0f, std::round(static_cast<REAL>(m_config.fontSize) * 1.35f));
    float spaceW = 4.0f;
    auto lines = BuildContentTokens(this, g, msg, msgFont, emoteSize, spaceW);

    float fontH = static_cast<REAL>(m_config.fontSize);
    float lineHeight = (std::max)(fontH * 1.40f, emoteSize + 2.0f);
    float curY = 0.0f;

    SolidBrush msgBrush(ArgbToGdiplusColor(m_config.messageColor, effectiveAlpha));
    SolidBrush shadowBrush(Color(static_cast<BYTE>(200 * effectiveAlpha), 0, 0, 0));
    StringFormat sfMeasure(StringFormat::GenericTypographic());
    sfMeasure.SetFormatFlags(StringFormatFlagsNoClip | StringFormatFlagsMeasureTrailingSpaces);

    for (const auto& line : lines) {
        float curX = 0.0f;
        for (const auto& token : line.tokens) {
            if (curX + token.width > availableWidth && curX > 0.0f) {
                curX = 0.0f;
                curY += lineHeight;
            }

            if (token.isEmote) {
                float emX = static_cast<float>(startX) + curX;
                float emY = static_cast<float>(startY) + curY + (lineHeight - emoteSize) / 2.0f;
                if (token.bmp) {
                    g.DrawImage(token.bmp.get(), emX, emY, emoteSize, emoteSize);
                } else {
                    // Modern placeholder while custom emote finishes downloading
                    SolidBrush phBrush(Color(static_cast<BYTE>(100 * effectiveAlpha), 34, 197, 94));
                    g.FillRectangle(&phBrush, emX, emY, emoteSize, emoteSize);
                }
            } else {
                float txX = static_cast<float>(startX) + curX;
                float txY = static_cast<float>(startY) + curY + (lineHeight - token.height) / 2.0f;

                if (m_config.extraBold) {
                    g.DrawString(token.text.c_str(), -1, &msgFont, PointF(txX - 0.65f + 1.0f, txY + 1.0f), &sfMeasure, &shadowBrush);
                    g.DrawString(token.text.c_str(), -1, &msgFont, PointF(txX + 0.65f + 1.0f, txY + 1.0f), &sfMeasure, &shadowBrush);
                    g.DrawString(token.text.c_str(), -1, &msgFont, PointF(txX - 0.65f, txY), &sfMeasure, &msgBrush);
                    g.DrawString(token.text.c_str(), -1, &msgFont, PointF(txX + 0.65f, txY), &sfMeasure, &msgBrush);
                }
                g.DrawString(token.text.c_str(), -1, &msgFont, PointF(txX + 1.0f, txY + 1.0f), &sfMeasure, &shadowBrush);
                g.DrawString(token.text.c_str(), -1, &msgFont, PointF(txX, txY), &sfMeasure, &msgBrush);
            }

            curX += token.width + spaceW;
        }
        curY += lineHeight;
    }

    outContentH = static_cast<int>(std::ceil(curY));
    if (outContentH < static_cast<int>(lineHeight)) {
        outContentH = static_cast<int>(lineHeight);
    }
}

void ChatRenderer::DrawSingleMessagePass(
    Graphics& g, 
    const ChatMessage& msg, 
    int x, int y, int width, 
    int& outItemHeight,
    float effectiveAlpha
) {
    if (effectiveAlpha <= 0.005f) {
        outItemHeight = 0;
        return;
    }

    int avatarSize = m_config.avatarSize;
    int textStartX = x + avatarSize + 10;
    int availableTextWidth = width - (avatarSize + 10);
    if (availableTextWidth < 100) availableTextWidth = 100;

    // 1. Prepare typography & measure
    std::wstring wFontFamily = Utf8ToWide(m_config.fontFamily);
    if (m_config.extraBold) {
        std::wstring blackName = wFontFamily + L" Black";
        FontFamily testFam(blackName.c_str());
        if (testFam.GetLastStatus() == Ok) {
            wFontFamily = blackName;
        }
    }
    FontFamily fontFamily(wFontFamily.c_str());
    if (fontFamily.GetLastStatus() != Ok) {
        FontFamily emojiFam(L"Segoe UI Emoji");
        if (emojiFam.GetLastStatus() == Ok) {
            wFontFamily = L"Segoe UI Emoji";
        } else {
            fontFamily.GenericSansSerif();
        }
    }

    INT userStyle = (m_config.usernameBold || m_config.extraBold) ? FontStyleBold : FontStyleRegular;
    INT msgStyle = (m_config.messageBold || m_config.extraBold) ? FontStyleBold : FontStyleRegular;

    Font userFont(&fontFamily, static_cast<REAL>(m_config.fontSize), userStyle, UnitPixel);
    Font msgFont(&fontFamily, static_cast<REAL>(m_config.fontSize), msgStyle, UnitPixel);

    // Convert strings properly from UTF-8 to UTF-16 wide string to preserve emojis & emoticons
    std::wstring wAuthor = Utf8ToWide(msg.authorName);
    std::wstring wText = Utf8ToWide(msg.messageText);

    // Measure Username
    RectF userBounds;
    g.MeasureString(wAuthor.c_str(), -1, &userFont, PointF(0, 0), &userBounds);

    int badgeEstimatedW = (msg.role != UserRole::Regular || !msg.badges.empty() || msg.isMembershipEvent) ? 80 : 0;
    int msgY = y + static_cast<int>(userBounds.Height) + 3;
    int msgTextH = 0;
    int msgMeasuredW = 0;

    if (!msg.customEmotes.empty()) {
        msgTextH = MeasureMessageContentWithEmotes(g, msg, msgFont, availableTextWidth);
        msgMeasuredW = availableTextWidth;
    } else {
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
        msgTextH = static_cast<int>(msgMeasured.Height);
        msgMeasuredW = static_cast<int>(msgMeasured.Width);
    }

    int totalTextH = static_cast<int>(userBounds.Height) + 3 + msgTextH;
    int itemH = (std::max)(avatarSize, totalTextH);

    // 2. Draw Message Background Card if enabled or if membership event
    if (m_config.showBackground || msg.isMembershipEvent) {
        int padX = 8;
        int padY = 6;
        int cardX = x - padX;
        int cardY = y - padY;
        int maxTextRight = (std::max)(
            static_cast<int>(userBounds.Width) + badgeEstimatedW + 8,
            msgMeasuredW
        );
        int cardW = avatarSize + 10 + maxTextRight + padX * 2;
        if (cardW > width + padX * 2) cardW = width + padX * 2;
        if (cardW < 130) cardW = 130;
        int cardH = itemH + padY * 2;

        float bgAlpha = (m_config.showBackground ? m_config.backgroundOpacity : 0.85f) * effectiveAlpha;
        if (bgAlpha > 0.01f) {
            Color bgCol = ArgbToGdiplusColor(m_config.backgroundColor, bgAlpha);
            if (msg.isMembershipEvent) {
                // Dark emerald green tint for membership events
                bgCol = Color(static_cast<BYTE>(230 * effectiveAlpha), 10, 30, 20);
            }
            SolidBrush bgBrush(bgCol);

            GraphicsPath cardPath;
            int r = 8; // rounded radius
            cardPath.AddArc(cardX, cardY, r * 2, r * 2, 180, 90);
            cardPath.AddArc(cardX + cardW - r * 2, cardY, r * 2, r * 2, 270, 90);
            cardPath.AddArc(cardX + cardW - r * 2, cardY + cardH - r * 2, r * 2, r * 2, 0, 90);
            cardPath.AddArc(cardX, cardY + cardH - r * 2, r * 2, r * 2, 90, 90);
            cardPath.CloseFigure();

            g.FillPath(&bgBrush, &cardPath);

            if (msg.isMembershipEvent) {
                // Distinctive glowing green border for new member joins!
                Pen memberBorder(Color(static_cast<BYTE>(240 * effectiveAlpha), 34, 197, 94), 2.0f);
                g.DrawPath(&memberBorder, &cardPath);
            } else {
                Pen outlinePen(Color(static_cast<BYTE>(40 * bgAlpha), 255, 255, 255), 1.0f);
                g.DrawPath(&outlinePen, &cardPath);
            }
        }
    }

    // 3. Draw circular avatar
    DrawCircularAvatar(g, msg.authorPhotoUrl, msg.authorName, msg.role, x, y, avatarSize, effectiveAlpha);

    // Color determination for username based on role
    uint32_t userColArgb = m_config.usernameColor;
    if (msg.role == UserRole::Owner || msg.role == UserRole::Moderator) {
        userColArgb = m_config.moderatorColor;
    } else if (msg.role == UserRole::Member || msg.isMembershipEvent) {
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

    if (m_config.extraBold) {
        // Subpixel dilation for extra bold username
        g.DrawString(wAuthor.c_str(), -1, &userFont, PointF((REAL)textStartX - 0.6f + 1.0f, (REAL)y + 1.0f), &shadowBrush);
        g.DrawString(wAuthor.c_str(), -1, &userFont, PointF((REAL)textStartX + 0.6f + 1.0f, (REAL)y + 1.0f), &shadowBrush);
        g.DrawString(wAuthor.c_str(), -1, &userFont, PointF((REAL)textStartX - 0.6f, (REAL)y), &userBrush);
        g.DrawString(wAuthor.c_str(), -1, &userFont, PointF((REAL)textStartX + 0.6f, (REAL)y), &userBrush);
    }
    // Shadow offset 1px
    g.DrawString(wAuthor.c_str(), -1, &userFont, PointF((REAL)textStartX + 1.0f, (REAL)y + 1.0f), &shadowBrush);
    g.DrawString(wAuthor.c_str(), -1, &userFont, PointF((REAL)textStartX, (REAL)y), &userBrush);

    // 5. Draw Role Badge next to username
    int badgeW = 0;
    int badgeH = static_cast<int>(userBounds.Height * 0.85f);
    int badgeX = textStartX + static_cast<int>(userBounds.Width) + 8;
    int badgeY = y + static_cast<int>((userBounds.Height - badgeH) / 2.0f);
    DrawBadge(g, msg, badgeX, badgeY, badgeH, badgeW, effectiveAlpha);

    // 6. Draw Message Text below username
    if (!msg.customEmotes.empty()) {
        int dummyContentH = 0;
        DrawMessageContentWithEmotes(g, msg, msgFont, textStartX, msgY, availableTextWidth, effectiveAlpha, dummyContentH);
    } else {
        RectF msgLayoutRect(
            (REAL)textStartX, 
            (REAL)msgY, 
            (REAL)availableTextWidth, 
            2000.0f
        );

        StringFormat sf;
        sf.SetTrimming(StringTrimmingWord);

        RectF shadowRect = msgLayoutRect;
        shadowRect.X += 1.0f;
        shadowRect.Y += 1.0f;
        SolidBrush msgBrush(msgColor);

        if (m_config.extraBold) {
            // Subpixel dilation for extra bold message text
            RectF sLeft = shadowRect;  sLeft.X -= 0.65f;
            RectF sRight = shadowRect; sRight.X += 0.65f;
            g.DrawString(wText.c_str(), -1, &msgFont, sLeft, &sf, &shadowBrush);
            g.DrawString(wText.c_str(), -1, &msgFont, sRight, &sf, &shadowBrush);

            RectF mLeft = msgLayoutRect;  mLeft.X -= 0.65f;
            RectF mRight = msgLayoutRect; mRight.X += 0.65f;
            g.DrawString(wText.c_str(), -1, &msgFont, mLeft, &sf, &msgBrush);
            g.DrawString(wText.c_str(), -1, &msgFont, mRight, &sf, &msgBrush);
        }

        g.DrawString(wText.c_str(), -1, &msgFont, shadowRect, &sf, &shadowBrush);
        g.DrawString(wText.c_str(), -1, &msgFont, msgLayoutRect, &sf, &msgBrush);
    }

    // Calculate total height of this chat item
    outItemHeight = itemH + ((m_config.showBackground || msg.isMembershipEvent) ? 6 : 0);
}

void ChatRenderer::DrawMessageItem(
    Graphics& g, 
    const ChatMessage& msg, 
    int x, int y, int width, 
    int& outItemHeight,
    float alphaFactor
) {
    float effectiveAlpha = (std::min)(1.0f, (std::max)(0.0f, msg.currentAlpha * alphaFactor * m_config.opacity));
    if (effectiveAlpha <= 0.01f) {
        outItemHeight = 0;
        return;
    }

    int drawX = x + static_cast<int>(msg.animOffsetX);
    int drawY = y + static_cast<int>(msg.animOffsetY);

    float motionDist = sqrtf(msg.animOffsetX * msg.animOffsetX + msg.animOffsetY * msg.animOffsetY);

    // Motion Blur: Render motion trail along vector of movement
    if (m_config.motionBlur && motionDist > 1.8f) {
        float smearDist = (std::min)(motionDist * 0.40f, 16.0f);
        float normX = msg.animOffsetX / motionDist;
        float normY = msg.animOffsetY / motionDist;

        int dummyH = 0;
        // Trail pass 3 (furthest & faintest ghost)
        int tx3 = drawX + static_cast<int>(normX * smearDist * 1.0f);
        int ty3 = drawY + static_cast<int>(normY * smearDist * 1.0f);
        DrawSingleMessagePass(g, msg, tx3, ty3, width, dummyH, effectiveAlpha * 0.10f);

        // Trail pass 2 (mid trail ghost)
        int tx2 = drawX + static_cast<int>(normX * smearDist * 0.65f);
        int ty2 = drawY + static_cast<int>(normY * smearDist * 0.65f);
        DrawSingleMessagePass(g, msg, tx2, ty2, width, dummyH, effectiveAlpha * 0.20f);

        // Trail pass 1 (near trail ghost)
        int tx1 = drawX + static_cast<int>(normX * smearDist * 0.32f);
        int ty1 = drawY + static_cast<int>(normY * smearDist * 0.32f);
        DrawSingleMessagePass(g, msg, tx1, ty1, width, dummyH, effectiveAlpha * 0.32f);
    }

    // Primary crisp pass
    DrawSingleMessagePass(g, msg, drawX, drawY, width, outItemHeight, effectiveAlpha);
}

int ChatRenderer::MeasureMessageItem(Graphics& g, const ChatMessage& msg, int width) {
    if (msg.currentAlpha <= 0.01f) return 0;

    int avatarSize = m_config.avatarSize;
    int availableTextWidth = width - (avatarSize + 10);
    if (availableTextWidth < 100) availableTextWidth = 100;

    std::wstring wFontFamily = Utf8ToWide(m_config.fontFamily);
    if (m_config.extraBold) {
        std::wstring blackName = wFontFamily + L" Black";
        FontFamily testFam(blackName.c_str());
        if (testFam.GetLastStatus() == Ok) {
            wFontFamily = blackName;
        }
    }
    FontFamily fontFamily(wFontFamily.c_str());
    if (fontFamily.GetLastStatus() != Ok) {
        FontFamily emojiFam(L"Segoe UI Emoji");
        if (emojiFam.GetLastStatus() == Ok) {
            wFontFamily = L"Segoe UI Emoji";
        } else {
            fontFamily.GenericSansSerif();
        }
    }

    INT userStyle = (m_config.usernameBold || m_config.extraBold) ? FontStyleBold : FontStyleRegular;
    INT msgStyle = (m_config.messageBold || m_config.extraBold) ? FontStyleBold : FontStyleRegular;

    Font userFont(&fontFamily, static_cast<REAL>(m_config.fontSize), userStyle, UnitPixel);
    Font msgFont(&fontFamily, static_cast<REAL>(m_config.fontSize), msgStyle, UnitPixel);

    std::wstring wAuthor = Utf8ToWide(msg.authorName);
    std::wstring wText = Utf8ToWide(msg.messageText);

    RectF userBounds;
    g.MeasureString(wAuthor.c_str(), -1, &userFont, PointF(0, 0), &userBounds);

    int msgH = 0;
    if (!msg.customEmotes.empty()) {
        msgH = MeasureMessageContentWithEmotes(g, msg, msgFont, availableTextWidth);
    } else {
        RectF msgLayoutRect(0, 0, (REAL)availableTextWidth, 2000.0f);
        StringFormat sf;
        sf.SetTrimming(StringTrimmingWord);

        RectF msgMeasured;
        g.MeasureString(wText.c_str(), -1, &msgFont, msgLayoutRect, &sf, &msgMeasured);
        msgH = static_cast<int>(msgMeasured.Height);
    }

    int totalTextH = static_cast<int>(userBounds.Height) + 3 + msgH;
    int itemH = (std::max)(avatarSize, totalTextH);
    return itemH + ((m_config.showBackground || msg.isMembershipEvent) ? 6 : 0);
}

void ChatRenderer::Render(HDC hdc, int width, int height) {
    std::lock_guard<std::mutex> lock(m_mutex);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    int spacing = m_config.spacing;

    // 1. Measure all visible items to determine total content height
    std::vector<int> itemHeights;
    itemHeights.reserve(m_messages.size());
    int totalContentH = 0;

    for (const auto& msg : m_messages) {
        if (msg.isExpired || msg.currentAlpha <= 0.01f) {
            itemHeights.push_back(0);
            continue;
        }
        int h = MeasureMessageItem(g, msg, width);
        itemHeights.push_back(h);
        if (h > 0) {
            totalContentH += h + spacing;
        }
    }
    if (totalContentH > 0) totalContentH -= spacing;

    // 2. Calculate vertical starting anchor
    int curY = 0;
    bool isBottom = (m_config.position == OverlayPosition::BottomLeft ||
                     m_config.position == OverlayPosition::BottomCenter ||
                     m_config.position == OverlayPosition::BottomRight);

    bool isMiddle = (m_config.position == OverlayPosition::MiddleLeft ||
                     m_config.position == OverlayPosition::MiddleCenter ||
                     m_config.position == OverlayPosition::MiddleRight);

    if (isBottom) {
        // Anchor to bottom of overlay window so chat grows upwards cleanly
        curY = (std::max)(0, height - totalContentH - 8);
    } else if (isMiddle) {
        curY = (std::max)(0, (height - totalContentH) / 2);
    } else {
        curY = 0;
    }

    // 3. Render active messages from queue with animation offsets
    for (size_t i = 0; i < m_messages.size(); ++i) {
        const auto& msg = m_messages[i];
        if (msg.isExpired || msg.currentAlpha <= 0.01f) continue;

        int itemH = itemHeights[i];
        if (itemH <= 0) continue;

        int drawX = static_cast<int>(std::round(msg.animOffsetX));
        int drawY = curY + static_cast<int>(std::round(msg.animOffsetY));

        int actualH = 0;
        DrawMessageItem(g, msg, drawX, drawY, width, actualH, 1.0f);
        curY += (actualH > 0 ? actualH : itemH) + spacing;

        if (curY >= height + 100) break;
    }
}
