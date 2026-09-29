#pragma once
#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")

struct HttpResponse {
    int statusCode{0};
    std::string body;
    std::string contentType;
    bool success{false};
    std::string errorMessage;
};

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    HttpResponse Get(const std::string& url, const std::string& extraHeaders = "");
    HttpResponse Post(const std::string& url, const std::string& jsonBody, const std::string& extraHeaders = "");

    std::vector<uint8_t> DownloadBinary(const std::string& url);

private:
    bool ParseUrl(const std::string& url, std::wstring& host, std::wstring& path, INTERNET_PORT& port, bool& isHttps);

    HINTERNET m_hSession{nullptr};
};
