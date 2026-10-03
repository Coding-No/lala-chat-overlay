#pragma once
#include <string>
#include "HttpClient.hpp"

struct DiscoveryResult {
    bool success{false};
    std::string videoId;
    std::string apiKey;
    std::string clientVersion;
    std::string initialContinuation;
    int timeoutMs{3000};
    std::string errorMessage;
};

class ChatDiscovery {
public:
    static std::string ExtractVideoId(const std::string& inputUrl);
    static DiscoveryResult Discover(HttpClient& client, const std::string& inputUrl);

private:
    static std::string ExtractRegex(const std::string& text, const std::string& pattern);
};
