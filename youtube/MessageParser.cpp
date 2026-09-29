#include "MessageParser.hpp"
#include "UserMetadataParser.hpp"
#include <sstream>

std::string MessageParser::SanitizeText(const std::string& text) {
    std::string clean;
    clean.reserve(text.size());

    for (unsigned char c : text) {
        // Strip non-printable ASCII control characters except space and standard punctuation
        if (c < 32 && c != '\t' && c != '\n') {
            continue;
        }
        clean.push_back(c);
    }

    // Limit maximum message length to avoid memory or rendering abuse
    if (clean.length() > 500) {
        clean = clean.substr(0, 497) + "...";
    }

    return clean;
}

std::string MessageParser::ExtractRunsText(const nlohmann::json& messageObj) {
    if (!messageObj.contains("runs") || !messageObj["runs"].is_array()) {
        if (messageObj.contains("simpleText") && messageObj["simpleText"].is_string()) {
            return messageObj["simpleText"].get<std::string>();
        }
        return "";
    }

    std::stringstream ss;
    for (const auto& run : messageObj["runs"]) {
        if (run.contains("text") && run["text"].is_string()) {
            ss << run["text"].get<std::string>();
        } else if (run.contains("emoji")) {
            const auto& emoji = run["emoji"];
            if (emoji.contains("shortcuts") && emoji["shortcuts"].is_array() && !emoji["shortcuts"].empty()) {
                ss << emoji["shortcuts"][0].get<std::string>();
            } else if (emoji.contains("image") && emoji["image"].contains("accessibility")) {
                ss << emoji["image"]["accessibility"]["accessibilityData"].value("label", "");
            }
        }
    }
    return ss.str();
}

std::optional<ChatMessage> MessageParser::ParseItem(const nlohmann::json& actionItem) {
    const nlohmann::json* renderer = nullptr;

    if (actionItem.contains("liveChatTextMessageRenderer")) {
        renderer = &actionItem["liveChatTextMessageRenderer"];
    } else if (actionItem.contains("liveChatPaidMessageRenderer")) {
        renderer = &actionItem["liveChatPaidMessageRenderer"];
    } else if (actionItem.contains("liveChatMembershipItemRenderer")) {
        renderer = &actionItem["liveChatMembershipItemRenderer"];
    }

    if (!renderer) return std::nullopt;

    ChatMessage msg;
    msg.id = renderer->value("id", "");
    if (msg.id.empty()) {
        return std::nullopt;
    }

    // Parse timestamp
    std::string timestampStr = renderer->value("timestampUsec", "0");
    try {
        msg.timestampUsec = std::stoull(timestampStr);
    } catch (...) {
        msg.timestampUsec = 0;
    }

    // Parse author metadata
    msg.authorName = UserMetadataParser::ParseAuthorName(*renderer);
    msg.authorChannelId = UserMetadataParser::ParseAuthorChannelId(*renderer);
    msg.authorPhotoUrl = UserMetadataParser::ParseAvatarUrl(*renderer);
    msg.role = UserMetadataParser::ParseRole(*renderer);
    msg.badges = UserMetadataParser::ParseBadges(*renderer);

    // Parse message body
    std::string rawText;
    if (renderer->contains("message")) {
        rawText = ExtractRunsText((*renderer)["message"]);
    } else if (renderer->contains("headerSubtext")) {
        // Membership announcement
        rawText = ExtractRunsText((*renderer)["headerSubtext"]);
    }

    // If it's a paid message (Super Chat), prepend purchase amount
    if (renderer->contains("purchaseAmountText") && (*renderer)["purchaseAmountText"].contains("simpleText")) {
        std::string amount = (*renderer)["purchaseAmountText"]["simpleText"].get<std::string>();
        rawText = "[" + amount + "] " + rawText;
    }

    msg.messageText = SanitizeText(rawText);
    msg.currentAlpha = 1.0f;
    msg.targetAlpha = 1.0f;

    return msg;
}
