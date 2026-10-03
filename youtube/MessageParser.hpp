#pragma once
#include <string>
#include <optional>
#include "../deps/nlohmann/json.hpp"
#include "../models/ChatMessage.hpp"

class MessageParser {
public:
    static std::optional<ChatMessage> ParseItem(const nlohmann::json& actionItem);
    static std::string SanitizeText(const std::string& text);
    static std::string FormatEmoticons(const std::string& text);
    static std::string ResolveEmoji(const nlohmann::json& emojiObj);

private:
    static std::string ExtractRunsText(const nlohmann::json& messageObj, std::vector<CustomEmote>& outCustomEmotes);
};
