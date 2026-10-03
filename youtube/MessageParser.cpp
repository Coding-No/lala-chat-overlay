#include "MessageParser.hpp"
#include "UserMetadataParser.hpp"
#include <sstream>
#include <unordered_map>
#include <algorithm>

static const std::unordered_map<std::string, std::string> s_emojiMap = {
    // Hearts & Affection
    {":heart:", "❤️"}, {"<3", "❤️"}, {":love:", "❤️"}, {":heart_eyes:", "😍"},
    {":kissing_heart:", "😘"}, {":sparkling_heart:", "💖"}, {":broken_heart:", "💔"},
    {":orange_heart:", "🧡"}, {":yellow_heart:", "💛"}, {":green_heart:", "💚"},
    {":blue_heart:", "💙"}, {":purple_heart:", "💜"}, {":black_heart:", "🖤"},
    {":white_heart:", "🤍"},

    // Faces & Expressions
    {":smile:", "😊"}, {":)", "😊"}, {":-)", "😊"}, {"(:", "😊"},
    {":grin:", "😁"}, {":D", "😁"}, {":-D", "😁"},
    {":joy:", "😂"}, {":face-with-tears-of-joy:", "😂"}, {"XD", "😂"}, {"xd", "😂"},
    {":rofl:", "🤣"}, {":sweat_smile:", "😅"}, {":innocent:", "😇"},
    {":wink:", "😉"}, {";)", "😉"}, {";-)", "😉"},
    {":blush:", "😊"}, {":relaxed:", "☺️"}, {":yum:", "😋"},
    {":stuck_out_tongue:", "😛"}, {":P", "😛"}, {":-P", "😛"}, {":p", "😛"}, {":-p", "😛"},
    {":sunglasses:", "😎"}, {":cool:", "😎"},
    {":thinking:", "🤔"}, {":thinking_face:", "🤔"},
    {":neutral_face:", "😐"}, {":|", "😐"}, {":-|", "😐"},
    {":expressionless:", "😑"}, {"-_-", "😑"},
    {":unamused:", "😒"}, {":rolling_eyes:", "🙄"}, {":grimacing:", "😬"},
    {":relieved:", "😌"}, {":pensive:", "😔"}, {":sleepy:", "😪"}, {":sleeping:", "😴"},
    {":mask:", "😷"}, {":face_vomiting:", "🤮"}, {":hot_face:", "🥵"}, {":cold_face:", "🥶"},
    {":exploding_head:", "🤯"}, {":partying_face:", "🥳"}, {":pleading_face:", "🥺"},
    {":confused:", "😕"}, {":worried:", "😟"}, {":frowning:", "☹️"}, {":(", "☹️"}, {":-(", "☹️"},
    {":open_mouth:", "😮"}, {":O", "😮"}, {":-O", "😮"}, {":o", "😮"}, {":-o", "😮"},
    {":astonished:", "😲"}, {":flushed:", "😳"}, {":scream:", "😱"},
    {":cry:", "😢"}, {":'(", "😢"}, {":sob:", "😭"}, {"T_T", "😭"}, {";-;", "😭"},
    {":rage:", "😡"}, {":angry:", "😠"}, {":skull:", "💀"}, {":ghost:", "👻"},
    {":alien:", "👽"}, {":robot:", "🤖"}, {":cat:", "🐱"}, {":3", "🐱"}, {":dog:", "🐶"},

    // Gestures & Body
    {":thumbsup:", "👍"}, {":+1:", "👍"}, {":like:", "👍"},
    {":thumbsdown:", "👎"}, {":-1:", "👎"}, {":dislike:", "👎"},
    {":clap:", "👏"}, {":clapping_hands:", "👏"},
    {":wave:", "👋"}, {":ok_hand:", "👌"}, {":pinched_fingers:", "🤌"},
    {":v:", "✌️"}, {":peace:", "✌️"}, {":punch:", "👊"}, {":fist:", "👊"},
    {":raised_hands:", "🙌"}, {"\\o/", "🙌"},
    {":pray:", "🙏"}, {":folded_hands:", "🙏"},
    {":muscle:", "💪"}, {":biceps:", "💪"}, {":eyes:", "👀"},

    // Excitement, Celebration & Gaming
    {":fire:", "🔥"}, {":flame:", "🔥"}, {":lit:", "🔥"},
    {":100:", "💯"},
    {":sparkles:", "✨"}, {":star:", "⭐"}, {":star2:", "🌟"},
    {":party_popper:", "🎉"}, {":tada:", "🎉"},
    {":rocket:", "🚀"}, {":crown:", "👑"}, {":gem:", "💎"}, {":diamond:", "💎"},
    {":trophy:", "🏆"}, {":medal:", "🏅"}, {":check:", "✅"}, {":white_check_mark:", "✅"},
    {":x:", "❌"}, {":cross_mark:", "❌"}, {":warning:", "⚠️"}, {":bomb:", "💣"},
    {":zap:", "⚡"}, {":lightning:", "⚡"}, {":moneybag:", "💰"}, {":sweat_drops:", "💦"},
    {":dash:", "💨"}, {":zzz:", "💤"},

    // YouTube Native Live Stream Emotes
    {":yt:", "▶️"}, {":youtube:", "▶️"},
    {":oops:", "🤭"}, {":buffering:", "⏳"},
    {":stayhome:", "🏠"}, {":dothefive:", "🖐️"},
    {":elbowbump:", "🤜🤛"}, {":goodvibes:", "✨"},
    {":thanks:", "🙏"}, {":handwash:", "🧼"},
    {":hydrated:", "💧"}, {":chillout:", "🧊"},
    {":yougotthis:", "💪"}
};

std::string MessageParser::ResolveEmoji(const nlohmann::json& emojiObj) {
    // 1. Direct emojiId if it contains Unicode characters
    if (emojiObj.contains("emojiId") && emojiObj["emojiId"].is_string()) {
        std::string id = emojiObj["emojiId"].get<std::string>();
        bool hasNonAscii = false;
        for (unsigned char c : id) {
            if (c >= 0x80) { hasNonAscii = true; break; }
        }
        if (hasNonAscii && id.find('/') == std::string::npos) {
            return id;
        }
    }

    // 2. Direct accessibility label if it contains Unicode characters (e.g. "❤️", "🔥")
    if (emojiObj.contains("image") && emojiObj["image"].contains("accessibility")) {
        const auto& acc = emojiObj["image"]["accessibility"];
        if (acc.contains("accessibilityData")) {
            std::string label = acc["accessibilityData"].value("label", "");
            bool hasNonAscii = false;
            for (unsigned char c : label) {
                if (c >= 0x80) { hasNonAscii = true; break; }
            }
            if (hasNonAscii && label.length() <= 8) {
                return label;
            }
            std::string lowerLabel = label;
            for (char& c : lowerLabel) c = static_cast<char>(tolower(c));
            if (lowerLabel == "red heart" || lowerLabel == "heart") return "❤️";
            if (lowerLabel == "fire") return "🔥";
            if (lowerLabel == "thumbs up") return "👍";
            if (lowerLabel == "clapping hands") return "👏";
            if (lowerLabel == "sparkles") return "✨";
        }
    }

    // 3. Match from shortcuts
    if (emojiObj.contains("shortcuts") && emojiObj["shortcuts"].is_array() && !emojiObj["shortcuts"].empty()) {
        for (const auto& sc : emojiObj["shortcuts"]) {
            if (!sc.is_string()) continue;
            std::string shortcut = sc.get<std::string>();
            std::string lowerSc = shortcut;
            for (char& c : lowerSc) c = static_cast<char>(tolower(c));
            
            auto it = s_emojiMap.find(lowerSc);
            if (it != s_emojiMap.end()) {
                return it->second;
            }
        }
    }

    // 4. Custom channel emoji: return first shortcut or label
    if (emojiObj.contains("shortcuts") && emojiObj["shortcuts"].is_array() && !emojiObj["shortcuts"].empty()) {
        return emojiObj["shortcuts"][0].get<std::string>();
    }

    return "";
}

std::string MessageParser::FormatEmoticons(const std::string& text) {
    if (text.empty()) return "";

    std::string result;
    result.reserve(text.size() + 16);

    size_t i = 0;
    while (i < text.size()) {
        // Quick check for colon shortcut :name:
        if (text[i] == ':') {
            size_t endColon = text.find(':', i + 1);
            if (endColon != std::string::npos && (endColon - i) <= 24) {
                std::string token = text.substr(i, endColon - i + 1);
                std::string lowerToken = token;
                for (char& c : lowerToken) c = static_cast<char>(tolower(c));

                auto it = s_emojiMap.find(lowerToken);
                if (it != s_emojiMap.end()) {
                    result += it->second;
                    i = endColon + 1;
                    continue;
                }
            }
        }

        // Check for standalone text emoticons like <3, :), :D, XD, etc.
        bool matched = false;
        static const std::pair<const char*, const char*> s_textEmotes[] = {
            {"<3", "❤️"},
            {"</3", "💔"},
            {":)", "😊"},
            {":-)", "😊"},
            {"(:", "😊"},
            {":D", "😁"},
            {":-D", "😁"},
            {"XD", "😂"},
            {"xd", "😂"},
            {";)", "😉"},
            {";-)", "😉"},
            {":P", "😛"},
            {":-P", "😛"},
            {":p", "😛"},
            {":-p", "😛"},
            {":(", "☹️"},
            {":-(", "☹️"},
            {":'(", "😢"},
            {"T_T", "😭"},
            {";-;", "😭"},
            {"-_-", "😑"},
            {"^_^", "😊"},
            {"\\o/", "🙌"},
            {":3", "🐱"}
        };

        for (const auto& emote : s_textEmotes) {
            size_t elen = strlen(emote.first);
            if (i + elen <= text.size() && text.compare(i, elen, emote.first) == 0) {
                // Verify word boundaries
                bool leftBoundary = (i == 0 || isspace(static_cast<unsigned char>(text[i - 1])));
                bool rightBoundary = (i + elen == text.size() || isspace(static_cast<unsigned char>(text[i + elen])) || ispunct(static_cast<unsigned char>(text[i + elen])));
                if (leftBoundary && rightBoundary) {
                    result += emote.second;
                    i += elen;
                    matched = true;
                    break;
                }
            }
        }

        if (!matched) {
            result.push_back(text[i]);
            i++;
        }
    }

    return result;
}

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

std::string MessageParser::ExtractRunsText(const nlohmann::json& messageObj, std::vector<CustomEmote>& outCustomEmotes) {
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
            bool isCustom = emoji.value("isCustomEmoji", false);
            std::string imgUrl = "";
            if (emoji.contains("image") && emoji["image"].contains("thumbnails")) {
                const auto& thumbs = emoji["image"]["thumbnails"];
                if (thumbs.is_array() && !thumbs.empty()) {
                    imgUrl = thumbs.back().value("url", "");
                }
            }
            if (!imgUrl.empty() && imgUrl.rfind("//", 0) == 0) {
                imgUrl = "https:" + imgUrl;
            }

            // Check if this is a custom member emote
            if (isCustom || (!imgUrl.empty() && imgUrl.find("yt3.ggpht.com") != std::string::npos)) {
                std::string shortcut = "";
                if (emoji.contains("shortcuts") && emoji["shortcuts"].is_array() && !emoji["shortcuts"].empty()) {
                    shortcut = emoji["shortcuts"][0].get<std::string>();
                } else if (emoji.contains("image") && emoji["image"].contains("accessibility")) {
                    shortcut = ":" + emoji["image"]["accessibility"]["accessibilityData"].value("label", "emote") + ":";
                } else {
                    shortcut = ":emote_" + std::to_string(outCustomEmotes.size()) + ":";
                }

                bool exists = false;
                for (const auto& ce : outCustomEmotes) {
                    if (ce.shortcut == shortcut) { exists = true; break; }
                }
                if (!exists && !imgUrl.empty()) {
                    outCustomEmotes.push_back({ shortcut, imgUrl });
                }

                ss << " " << shortcut << " ";
            } else {
                std::string resolved = ResolveEmoji(emoji);
                if (!resolved.empty()) {
                    ss << " " << resolved << " ";
                }
            }
        }
    }
    return ss.str();
}

std::optional<ChatMessage> MessageParser::ParseItem(const nlohmann::json& actionItem) {
    const nlohmann::json* renderer = nullptr;
    bool isMembership = false;

    if (actionItem.contains("liveChatTextMessageRenderer")) {
        renderer = &actionItem["liveChatTextMessageRenderer"];
    } else if (actionItem.contains("liveChatPaidMessageRenderer")) {
        renderer = &actionItem["liveChatPaidMessageRenderer"];
    } else if (actionItem.contains("liveChatMembershipItemRenderer")) {
        renderer = &actionItem["liveChatMembershipItemRenderer"];
        isMembership = true;
    } else if (actionItem.contains("liveChatSponsorshipsGiftPurchaseAnnouncementRenderer")) {
        renderer = &actionItem["liveChatSponsorshipsGiftPurchaseAnnouncementRenderer"];
        isMembership = true;
    } else if (actionItem.contains("liveChatSponsorshipsGiftRedemptionAnnouncementRenderer")) {
        renderer = &actionItem["liveChatSponsorshipsGiftRedemptionAnnouncementRenderer"];
        isMembership = true;
    }

    if (!renderer) return std::nullopt;

    ChatMessage msg;
    msg.id = renderer->value("id", "");
    if (msg.id.empty()) {
        return std::nullopt;
    }
    msg.isMembershipEvent = isMembership;

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

    if (isMembership && msg.role == UserRole::Regular) {
        msg.role = UserRole::Member;
    }

    // Parse message body
    std::string rawText;
    if (isMembership) {
        std::string primaryText = "";
        std::string subText = "";
        std::string userMsg = "";

        if (renderer->contains("headerPrimaryText")) {
            primaryText = ExtractRunsText((*renderer)["headerPrimaryText"], msg.customEmotes);
        }
        if (renderer->contains("headerSubtext")) {
            subText = ExtractRunsText((*renderer)["headerSubtext"], msg.customEmotes);
        }
        if (renderer->contains("message")) {
            userMsg = ExtractRunsText((*renderer)["message"], msg.customEmotes);
        }

        // Support liveChatSponsorshipsGiftPurchaseAnnouncementRenderer
        if (renderer->contains("header") && (*renderer)["header"].contains("liveChatSponsorshipsHeaderRenderer")) {
            const auto& hdr = (*renderer)["header"]["liveChatSponsorshipsHeaderRenderer"];
            if (hdr.contains("primaryText")) {
                primaryText = ExtractRunsText(hdr["primaryText"], msg.customEmotes);
            }
            if (msg.authorName.empty() && hdr.contains("authorName")) {
                msg.authorName = UserMetadataParser::ParseAuthorName(hdr);
            }
            if (msg.authorPhotoUrl.empty() && hdr.contains("authorPhoto")) {
                msg.authorPhotoUrl = UserMetadataParser::ParseAvatarUrl(hdr);
            }
            if (msg.badges.empty() && hdr.contains("authorBadges")) {
                msg.badges = UserMetadataParser::ParseBadges(hdr);
            }
        }

        if (!primaryText.empty() && !subText.empty()) {
            rawText = "🎉 [" + primaryText + "] " + subText;
        } else if (!subText.empty()) {
            rawText = "🎉 [Member Baru!] " + subText;
        } else if (!primaryText.empty()) {
            rawText = "🎉 [Member!] " + primaryText;
        } else {
            rawText = "🎉 [Baru Bergabung Menjadi Member!]";
        }

        if (!userMsg.empty()) {
            rawText += " \n" + userMsg;
        }
    } else {
        if (renderer->contains("message")) {
            rawText = ExtractRunsText((*renderer)["message"], msg.customEmotes);
        } else if (renderer->contains("headerSubtext")) {
            rawText = ExtractRunsText((*renderer)["headerSubtext"], msg.customEmotes);
        }
    }

    // If it's a paid message (Super Chat), prepend purchase amount
    if (renderer->contains("purchaseAmountText") && (*renderer)["purchaseAmountText"].contains("simpleText")) {
        std::string amount = (*renderer)["purchaseAmountText"]["simpleText"].get<std::string>();
        rawText = "[" + amount + "] " + rawText;
    }

    msg.messageText = FormatEmoticons(SanitizeText(rawText));
    msg.currentAlpha = 1.0f;
    msg.targetAlpha = 1.0f;

    return msg;
}
