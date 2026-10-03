#include "UserMetadataParser.hpp"
#include <algorithm>

UserRole UserMetadataParser::ParseRole(const nlohmann::json& renderer) {
    UserRole highestRole = UserRole::Regular;

    // Detect membership item renderer indicators
    if (renderer.contains("headerSubtext") || renderer.contains("headerPrimaryText") || renderer.contains("sponsorshipsHeader")) {
        highestRole = UserRole::Member;
    }

    if (!renderer.contains("authorBadges") || !renderer["authorBadges"].is_array()) {
        return highestRole;
    }

    for (const auto& badge : renderer["authorBadges"]) {
        if (!badge.contains("liveChatAuthorBadgeRenderer")) continue;
        const auto& badgeRenderer = badge["liveChatAuthorBadgeRenderer"];

        std::string tooltip = "";
        if (badgeRenderer.contains("tooltip") && badgeRenderer["tooltip"].is_string()) {
            tooltip = badgeRenderer["tooltip"].get<std::string>();
        }

        std::string lowerTooltip = tooltip;
        std::transform(lowerTooltip.begin(), lowerTooltip.end(), lowerTooltip.begin(), ::tolower);

        if (lowerTooltip.find("owner") != std::string::npos) {
            return UserRole::Owner; // Highest precedence
        } else if (lowerTooltip.find("moderator") != std::string::npos) {
            highestRole = UserRole::Moderator;
        } else if (lowerTooltip.find("member") != std::string::npos) {
            if (highestRole != UserRole::Moderator) highestRole = UserRole::Member;
        } else if (lowerTooltip.find("verified") != std::string::npos) {
            if (highestRole == UserRole::Regular) highestRole = UserRole::Verified;
        }
    }

    return highestRole;
}

std::vector<BadgeInfo> UserMetadataParser::ParseBadges(const nlohmann::json& renderer) {
    std::vector<BadgeInfo> badges;
    if (!renderer.contains("authorBadges") || !renderer["authorBadges"].is_array()) {
        return badges;
    }

    for (const auto& badge : renderer["authorBadges"]) {
        if (!badge.contains("liveChatAuthorBadgeRenderer")) continue;
        const auto& badgeRenderer = badge["liveChatAuthorBadgeRenderer"];

        BadgeInfo info;
        if (badgeRenderer.contains("tooltip") && badgeRenderer["tooltip"].is_string()) {
            info.tooltip = badgeRenderer["tooltip"].get<std::string>();
            info.badgeName = info.tooltip;
        }
        if (badgeRenderer.contains("customThumbnail") && badgeRenderer["customThumbnail"].contains("thumbnails")) {
            const auto& thumbs = badgeRenderer["customThumbnail"]["thumbnails"];
            if (thumbs.is_array() && !thumbs.empty()) {
                info.iconUrl = thumbs.back().value("url", "");
            }
        }
        badges.push_back(info);
    }
    return badges;
}

std::string UserMetadataParser::ParseAvatarUrl(const nlohmann::json& renderer) {
    if (!renderer.contains("authorPhoto") || !renderer["authorPhoto"].contains("thumbnails")) {
        return "";
    }

    const auto& thumbnails = renderer["authorPhoto"]["thumbnails"];
    if (!thumbnails.is_array() || thumbnails.empty()) {
        return "";
    }

    // Usually thumbnails are sorted ascending by resolution, take the highest resolution
    std::string url = thumbnails.back().value("url", "");
    if (!url.empty() && url.rfind("//", 0) == 0) {
        url = "https:" + url;
    }
    return url;
}

std::string UserMetadataParser::ParseAuthorName(const nlohmann::json& renderer) {
    if (renderer.contains("authorName") && renderer["authorName"].contains("simpleText")) {
        return renderer["authorName"]["simpleText"].get<std::string>();
    }
    return "User";
}

std::string UserMetadataParser::ParseAuthorChannelId(const nlohmann::json& renderer) {
    if (renderer.contains("authorExternalChannelId") && renderer["authorExternalChannelId"].is_string()) {
        return renderer["authorExternalChannelId"].get<std::string>();
    }
    return "";
}
