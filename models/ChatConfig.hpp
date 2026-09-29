#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include "../deps/nlohmann/json.hpp"

enum class OverlayPosition {
    TopLeft,
    TopCenter,
    TopRight,
    MiddleLeft,
    MiddleCenter,
    MiddleRight,
    BottomLeft,
    BottomCenter,
    BottomRight
};

enum class OverlayAnimation {
    None,
    Fade,
    SlideUp,
    SlideDown,
    SlideLeft,
    SlideRight
};

enum class RoleFilter {
    Everyone,
    ModeratorOnly,
    MemberOnly,
    VerifiedOnly,
    RegularOnly
};

struct ChatConfig {
    std::string youtubeUrl{""};
    int chatDurationSeconds{10}; // 3, 5, 10, 15, 30, 60, -1 (Never)
    int maxMessages{8};          // 3, 5, 8, 10, 15, 20
    
    // Typography & Styling
    std::string fontFamily{"Segoe UI"};
    int fontSize{16};
    bool usernameBold{true};
    bool messageBold{true}; // Bold by default for crisp legibility over games
    
    // Background Card
    bool showBackground{true};
    uint32_t backgroundColor{0xFF181825}; // Catppuccin Mantle dark card
    float backgroundOpacity{0.65f};       // 65% opacity
    
    // Colors (32-bit ARGB hex: 0xAARRGGBB)
    uint32_t usernameColor{0xFFE2E8F0};   // Light slate
    uint32_t messageColor{0xFFFFFFFF};    // Pure white
    uint32_t moderatorColor{0xFF38BDF8};  // Bright sky blue
    uint32_t memberColor{0xFF4ADE80};     // Vibrant green
    uint32_t verifiedColor{0xFFFBBF24};   // Warm amber
    
    float opacity{0.95f}; // 0.1 to 1.0
    
    // Layout
    OverlayPosition position{OverlayPosition::TopLeft};
    int offsetX{30};
    int offsetY{30};
    int avatarSize{32};
    int spacing{10};
    int maxWidth{480};
    
    // Behavior & Effects
    OverlayAnimation animation{OverlayAnimation::Fade};
    int autoHideSeconds{0}; // 0 = disabled
    RoleFilter filter{RoleFilter::Everyone};
    bool autoFollowGame{false};

    // Helper functions for position name conversion
    static const char* PositionToString(OverlayPosition pos) {
        switch (pos) {
            case OverlayPosition::TopLeft: return "Top Left";
            case OverlayPosition::TopCenter: return "Top Center";
            case OverlayPosition::TopRight: return "Top Right";
            case OverlayPosition::MiddleLeft: return "Middle Left";
            case OverlayPosition::MiddleCenter: return "Middle Center";
            case OverlayPosition::MiddleRight: return "Middle Right";
            case OverlayPosition::BottomLeft: return "Bottom Left";
            case OverlayPosition::BottomCenter: return "Bottom Center";
            case OverlayPosition::BottomRight: return "Bottom Right";
            default: return "Top Left";
        }
    }

    static OverlayPosition StringToPosition(const std::string& str) {
        if (str == "Top Center") return OverlayPosition::TopCenter;
        if (str == "Top Right") return OverlayPosition::TopRight;
        if (str == "Middle Left") return OverlayPosition::MiddleLeft;
        if (str == "Middle Center") return OverlayPosition::MiddleCenter;
        if (str == "Middle Right") return OverlayPosition::MiddleRight;
        if (str == "Bottom Left") return OverlayPosition::BottomLeft;
        if (str == "Bottom Center") return OverlayPosition::BottomCenter;
        if (str == "Bottom Right") return OverlayPosition::BottomRight;
        return OverlayPosition::TopLeft;
    }

    static const char* AnimationToString(OverlayAnimation anim) {
        switch (anim) {
            case OverlayAnimation::None: return "None";
            case OverlayAnimation::Fade: return "Fade";
            case OverlayAnimation::SlideUp: return "Slide Up";
            case OverlayAnimation::SlideDown: return "Slide Down";
            case OverlayAnimation::SlideLeft: return "Slide Left";
            case OverlayAnimation::SlideRight: return "Slide Right";
            default: return "Fade";
        }
    }

    static OverlayAnimation StringToAnimation(const std::string& str) {
        if (str == "None") return OverlayAnimation::None;
        if (str == "Slide Up") return OverlayAnimation::SlideUp;
        if (str == "Slide Down") return OverlayAnimation::SlideDown;
        if (str == "Slide Left") return OverlayAnimation::SlideLeft;
        if (str == "Slide Right") return OverlayAnimation::SlideRight;
        return OverlayAnimation::Fade;
    }

    std::string toJsonString() const {
        nlohmann::json j;
        j["youtube_url"] = youtubeUrl;
        j["chat_duration"] = chatDurationSeconds;
        j["max_messages"] = maxMessages;
        j["font_family"] = fontFamily;
        j["font_size"] = fontSize;
        j["username_bold"] = usernameBold;
        j["message_bold"] = messageBold;
        j["show_background"] = showBackground;
        j["background_color"] = backgroundColor;
        j["background_opacity"] = backgroundOpacity;
        j["username_color"] = usernameColor;
        j["message_color"] = messageColor;
        j["moderator_color"] = moderatorColor;
        j["member_color"] = memberColor;
        j["verified_color"] = verifiedColor;
        j["opacity"] = opacity;
        j["position"] = PositionToString(position);
        j["offset_x"] = offsetX;
        j["offset_y"] = offsetY;
        j["avatar_size"] = avatarSize;
        j["spacing"] = spacing;
        j["max_width"] = maxWidth;
        j["animation"] = AnimationToString(animation);
        j["auto_hide"] = autoHideSeconds;
        j["filter"] = static_cast<int>(filter);
        j["auto_follow_game"] = autoFollowGame;
        return j.dump(4);
    }

    bool fromJsonString(const std::string& str) {
        try {
            auto j = nlohmann::json::parse(str);
            if (j.contains("youtube_url")) youtubeUrl = j["youtube_url"].get<std::string>();
            if (j.contains("chat_duration")) chatDurationSeconds = j["chat_duration"].get<int>();
            if (j.contains("max_messages")) maxMessages = j["max_messages"].get<int>();
            if (j.contains("font_family")) fontFamily = j["font_family"].get<std::string>();
            if (j.contains("font_size")) fontSize = j["font_size"].get<int>();
            if (j.contains("username_bold")) usernameBold = j["username_bold"].get<bool>();
            if (j.contains("message_bold")) messageBold = j["message_bold"].get<bool>();
            if (j.contains("show_background")) showBackground = j["show_background"].get<bool>();
            if (j.contains("background_color")) backgroundColor = j["background_color"].get<uint32_t>();
            if (j.contains("background_opacity")) backgroundOpacity = j["background_opacity"].get<float>();
            if (j.contains("username_color")) usernameColor = j["username_color"].get<uint32_t>();
            if (j.contains("message_color")) messageColor = j["message_color"].get<uint32_t>();
            if (j.contains("moderator_color")) moderatorColor = j["moderator_color"].get<uint32_t>();
            if (j.contains("member_color")) memberColor = j["member_color"].get<uint32_t>();
            if (j.contains("verified_color")) verifiedColor = j["verified_color"].get<uint32_t>();
            if (j.contains("opacity")) opacity = j["opacity"].get<float>();
            if (j.contains("position")) position = StringToPosition(j["position"].get<std::string>());
            if (j.contains("offset_x")) offsetX = j["offset_x"].get<int>();
            if (j.contains("offset_y")) offsetY = j["offset_y"].get<int>();
            if (j.contains("avatar_size")) avatarSize = j["avatar_size"].get<int>();
            if (j.contains("spacing")) spacing = j["spacing"].get<int>();
            if (j.contains("max_width")) maxWidth = j["max_width"].get<int>();
            if (j.contains("animation")) animation = StringToAnimation(j["animation"].get<std::string>());
            if (j.contains("auto_hide")) autoHideSeconds = j["auto_hide"].get<int>();
            if (j.contains("filter")) filter = static_cast<RoleFilter>(j["filter"].get<int>());
            if (j.contains("auto_follow_game")) autoFollowGame = j["auto_follow_game"].get<bool>();
            return true;
        } catch (...) {
            return false;
        }
    }

    bool saveToFile(const std::string& filePath) const {
        std::ofstream f(filePath);
        if (!f.is_open()) return false;
        f << toJsonString();
        return true;
    }

    bool loadFromFile(const std::string& filePath) {
        std::ifstream f(filePath);
        if (!f.is_open()) return false;
        std::stringstream buf;
        buf << f.rdbuf();
        return fromJsonString(buf.str());
    }
};
