#pragma once
#include <string>
#include <vector>
#include "../deps/nlohmann/json.hpp"
#include "../models/UserRole.hpp"
#include "../models/ChatMessage.hpp"

class UserMetadataParser {
public:
    static UserRole ParseRole(const nlohmann::json& renderer);
    static std::string ParseAvatarUrl(const nlohmann::json& renderer);
    static std::string ParseAuthorName(const nlohmann::json& renderer);
    static std::string ParseAuthorChannelId(const nlohmann::json& renderer);
    static std::vector<BadgeInfo> ParseBadges(const nlohmann::json& renderer);
};
