#pragma once
#include <string>
#include <vector>
#include "HttpClient.hpp"
#include "../models/ChatMessage.hpp"

struct ChatBatchResult {
    bool success{false};
    std::vector<ChatMessage> messages;
    std::string nextContinuation;
    int nextTimeoutMs{3000};
    std::string errorMessage;
};

class ChatParser {
public:
    static ChatBatchResult FetchBatch(
        HttpClient& client,
        const std::string& apiKey,
        const std::string& clientVersion,
        const std::string& continuation
    );
};
