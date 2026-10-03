#include "ChatParser.hpp"
#include "MessageParser.hpp"
#include "../deps/nlohmann/json.hpp"

ChatBatchResult ChatParser::FetchBatch(
    HttpClient& client,
    const std::string& apiKey,
    const std::string& clientVersion,
    const std::string& continuation
) {
    ChatBatchResult result;

    if (continuation.empty()) {
        result.errorMessage = "Empty continuation token";
        return result;
    }

    std::string postUrl = "https://www.youtube.com/youtubei/v1/live_chat/get_live_chat?key=" + apiKey;

    nlohmann::json payload;
    payload["context"]["client"]["clientName"] = "WEB";
    payload["context"]["client"]["clientVersion"] = clientVersion;
    payload["continuation"] = continuation;

    HttpResponse resp = client.Post(postUrl, payload.dump());
    if (!resp.success) {
        result.errorMessage = "HTTP error: " + std::to_string(resp.statusCode) + " " + resp.errorMessage;
        return result;
    }

    try {
        auto root = nlohmann::json::parse(resp.body);

        if (!root.contains("continuationContents") || !root["continuationContents"].contains("liveChatContinuation")) {
            result.errorMessage = "Invalid JSON structure (missing liveChatContinuation)";
            return result;
        }

        const auto& chatCont = root["continuationContents"]["liveChatContinuation"];

        // 1. Parse actions (messages)
        if (chatCont.contains("actions") && chatCont["actions"].is_array()) {
            for (const auto& act : chatCont["actions"]) {
                if (!act.contains("addChatItemAction") || !act["addChatItemAction"].contains("item")) {
                    continue;
                }
                const auto& item = act["addChatItemAction"]["item"];
                auto msgOpt = MessageParser::ParseItem(item);
                if (msgOpt.has_value()) {
                    result.messages.push_back(std::move(msgOpt.value()));
                }
            }
        }

        // 2. Parse next continuation token and timeout
        if (chatCont.contains("continuations") && chatCont["continuations"].is_array() && !chatCont["continuations"].empty()) {
            const auto& nextC = chatCont["continuations"][0];
            for (auto it = nextC.begin(); it != nextC.end(); ++it) {
                if (it.value().contains("continuation")) {
                    result.nextContinuation = it.value()["continuation"].get<std::string>();
                    result.nextTimeoutMs = it.value().value("timeoutMs", 3000);
                    break;
                }
            }
        }

        result.success = true;
    } catch (const std::exception& e) {
        result.errorMessage = std::string("JSON parsing error: ") + e.what();
    }

    return result;
}
