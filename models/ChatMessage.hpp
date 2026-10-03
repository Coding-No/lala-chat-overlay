#pragma once
#include <string>
#include <vector>
#include <chrono>
#include "UserRole.hpp"

struct BadgeInfo {
    std::string badgeName;
    std::string tooltip;
    std::string iconUrl;
};

struct CustomEmote {
    std::string shortcut; // e.g. ":member_cat:"
    std::string imageUrl; // e.g. "https://yt3.ggpht.com/..."
};

struct ChatMessage {
    std::string id;
    std::string authorName;
    std::string authorChannelId;
    std::string authorPhotoUrl;
    std::string messageText;
    UserRole role{UserRole::Regular};
    std::vector<BadgeInfo> badges;
    std::vector<CustomEmote> customEmotes;
    bool isMembershipEvent{false};
    uint64_t timestampUsec{0};

    // Animation & rendering runtime fields
    std::chrono::steady_clock::time_point receivedTime{std::chrono::steady_clock::now()};
    float currentAlpha{0.0f};
    float targetAlpha{1.0f};
    float animOffsetX{0.0f};
    float animOffsetY{0.0f};
    bool isExpired{false};
    bool isFadingOut{false};

    // Cached rendered layout height
    int renderedHeight{0};
};
