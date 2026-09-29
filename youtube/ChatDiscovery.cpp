#include "ChatDiscovery.hpp"
#include <regex>
#include <iostream>
#include "../deps/nlohmann/json.hpp"

std::string ChatDiscovery::ExtractRegex(const std::string& text, const std::string& pattern) {
    try {
        std::regex re(pattern);
        std::smatch match;
        if (std::regex_search(text, match, re) && match.size() > 1) {
            return match.str(1);
        }
    } catch (...) {
    }
    return "";
}

std::string ChatDiscovery::ExtractVideoId(const std::string& inputUrl) {
    if (inputUrl.empty()) return "";

    // 1. Check if directly 11-char video ID (alphanumeric, -, _)
    if (inputUrl.length() == 11) {
        bool valid = true;
        for (char c : inputUrl) {
            if (!isalnum(c) && c != '-' && c != '_') {
                valid = false;
                break;
            }
        }
        if (valid) return inputUrl;
    }

    // 2. Pattern for watch?v=...
    std::string id = ExtractRegex(inputUrl, R"((?:v=|\/live\/|\/shorts\/|youtu\.be\/)([a-zA-Z0-9_-]{11}))");
    if (!id.empty()) return id;

    return "";
}

DiscoveryResult ChatDiscovery::Discover(HttpClient& client, const std::string& inputUrl) {
    DiscoveryResult result;

    result.videoId = ExtractVideoId(inputUrl);
    if (result.videoId.empty()) {
        result.errorMessage = "Invalid YouTube URL or Video ID";
        return result;
    }

    // Step 1: Fetch watch page to retrieve API key & Client Version
    std::string watchUrl = "https://www.youtube.com/watch?v=" + result.videoId;
    HttpResponse watchResp = client.Get(watchUrl);
    if (!watchResp.success) {
        result.errorMessage = "Failed to fetch YouTube watch page: " + watchResp.errorMessage;
        return result;
    }

    // Check for obvious stream offline or chat disabled markers
    if (watchResp.body.find("Chat is disabled for this live stream") != std::string::npos) {
        result.errorMessage = "Live chat is disabled for this stream";
        return result;
    }

    // Extract INNERTUBE_API_KEY
    result.apiKey = ExtractRegex(watchResp.body, "\"INNERTUBE_API_KEY\":\"([^\"]+)\"");
    if (result.apiKey.empty()) {
        result.apiKey = "AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8"; // Default fallback InnerTube key
    }

    // Extract INNERTUBE_CLIENT_VERSION
    result.clientVersion = ExtractRegex(watchResp.body, "\"INNERTUBE_CLIENT_VERSION\":\"([^\"]+)\"");
    if (result.clientVersion.empty()) {
        result.clientVersion = "2.20260925.08.00";
    }

    // Step 2: Call /youtubei/v1/next to obtain active liveChatRenderer
    std::string nextUrl = "https://www.youtube.com/youtubei/v1/next?key=" + result.apiKey;

    nlohmann::json nextPayload;
    nextPayload["context"]["client"]["clientName"] = "WEB";
    nextPayload["context"]["client"]["clientVersion"] = result.clientVersion;
    nextPayload["videoId"] = result.videoId;

    HttpResponse nextResp = client.Post(nextUrl, nextPayload.dump());
    if (!nextResp.success) {
        result.errorMessage = "InnerTube next call failed (HTTP " + std::to_string(nextResp.statusCode) + ")";
        return result;
    }

    try {
        auto nextJson = nlohmann::json::parse(nextResp.body);

        // Recursive search for liveChatRenderer in response tree
        std::function<const nlohmann::json*(const nlohmann::json&)> findChatRenderer;
        findChatRenderer = [&](const nlohmann::json& node) -> const nlohmann::json* {
            if (node.is_object()) {
                if (node.contains("liveChatRenderer")) {
                    return &node["liveChatRenderer"];
                }
                for (auto it = node.begin(); it != node.end(); ++it) {
                    const auto* res = findChatRenderer(it.value());
                    if (res) return res;
                }
            } else if (node.is_array()) {
                for (const auto& item : node) {
                    const auto* res = findChatRenderer(item);
                    if (res) return res;
                }
            }
            return nullptr;
        };

        const auto* chatRenderer = findChatRenderer(nextJson);
        if (!chatRenderer) {
            result.errorMessage = "No active live chat found (Stream may be offline or chat disabled)";
            return result;
        }

        if (chatRenderer->contains("continuations") && (*chatRenderer)["continuations"].is_array() && !(*chatRenderer)["continuations"].empty()) {
            const auto& firstCont = (*chatRenderer)["continuations"][0];
            for (auto it = firstCont.begin(); it != firstCont.end(); ++it) {
                if (it.value().contains("continuation")) {
                    result.initialContinuation = it.value()["continuation"].get<std::string>();
                    result.timeoutMs = it.value().value("timeoutMs", 3000);
                    result.success = true;
                    return result;
                }
            }
        }

        result.errorMessage = "Live chat found but continuation token missing";
    } catch (const std::exception& e) {
        result.errorMessage = std::string("JSON parsing error: ") + e.what();
    }

    return result;
}
