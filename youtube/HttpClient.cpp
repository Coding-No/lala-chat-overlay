#include "HttpClient.hpp"
#include <iostream>

HttpClient::HttpClient() {
    m_hSession = WinHttpOpen(
        L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (m_hSession) {
        // Set standard network timeouts (in milliseconds)
        WinHttpSetTimeouts(m_hSession, 5000, 10000, 10000, 15000);
    }
}

HttpClient::~HttpClient() {
    if (m_hSession) {
        WinHttpCloseHandle(m_hSession);
        m_hSession = nullptr;
    }
}

bool HttpClient::ParseUrl(const std::string& url, std::wstring& host, std::wstring& path, INTERNET_PORT& port, bool& isHttps) {
    std::wstring wUrl(url.begin(), url.end());

    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.dwExtraInfoLength = (DWORD)-1;

    if (!WinHttpCrackUrl(wUrl.c_str(), (DWORD)wUrl.length(), 0, &urlComp)) {
        return false;
    }

    host = std::wstring(urlComp.lpszHostName, urlComp.dwHostNameLength);
    path = std::wstring(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);
    if (urlComp.dwExtraInfoLength > 0) {
        path += std::wstring(urlComp.lpszExtraInfo, urlComp.dwExtraInfoLength);
    }

    port = urlComp.nPort;
    isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    return true;
}

HttpResponse HttpClient::Get(const std::string& url, const std::string& extraHeaders) {
    HttpResponse resp;
    if (!m_hSession) {
        resp.errorMessage = "WinHttp session not initialized";
        return resp;
    }

    std::wstring host, path;
    INTERNET_PORT port = 0;
    bool isHttps = true;
    if (!ParseUrl(url, host, path, port, isHttps)) {
        resp.errorMessage = "Failed to parse URL: " + url;
        return resp;
    }

    HINTERNET hConnect = WinHttpConnect(m_hSession, host.c_str(), port, 0);
    if (!hConnect) {
        resp.errorMessage = "WinHttpConnect failed";
        return resp;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!hRequest) {
        resp.errorMessage = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        return resp;
    }

    std::wstring headers = L"Accept-Language: en-US,en;q=0.9\r\n";
    if (!extraHeaders.empty()) {
        headers += std::wstring(extraHeaders.begin(), extraHeaders.end()) + L"\r\n";
    }

    BOOL sendOk = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        (DWORD)headers.length(),
        WINHTTP_NO_REQUEST_DATA,
        0, 0, 0
    );

    if (!sendOk || !WinHttpReceiveResponse(hRequest, nullptr)) {
        resp.errorMessage = "WinHttpSendRequest/ReceiveResponse failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return resp;
    }

    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &size,
        WINHTTP_NO_HEADER_INDEX
    );
    resp.statusCode = static_cast<int>(statusCode);

    std::vector<char> buffer;
    DWORD bytesRead = 0;
    do {
        DWORD bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable) || bytesAvailable == 0) break;

        size_t curSize = buffer.size();
        buffer.resize(curSize + bytesAvailable);
        if (!WinHttpReadData(hRequest, buffer.data() + curSize, bytesAvailable, &bytesRead) || bytesRead == 0) {
            buffer.resize(curSize);
            break;
        }
        buffer.resize(curSize + bytesRead);
    } while (bytesRead > 0);

    resp.body = std::string(buffer.begin(), buffer.end());
    resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return resp;
}

HttpResponse HttpClient::Post(const std::string& url, const std::string& jsonBody, const std::string& extraHeaders) {
    HttpResponse resp;
    if (!m_hSession) {
        resp.errorMessage = "WinHttp session not initialized";
        return resp;
    }

    std::wstring host, path;
    INTERNET_PORT port = 0;
    bool isHttps = true;
    if (!ParseUrl(url, host, path, port, isHttps)) {
        resp.errorMessage = "Failed to parse URL: " + url;
        return resp;
    }

    HINTERNET hConnect = WinHttpConnect(m_hSession, host.c_str(), port, 0);
    if (!hConnect) {
        resp.errorMessage = "WinHttpConnect failed";
        return resp;
    }

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!hRequest) {
        resp.errorMessage = "WinHttpOpenRequest failed";
        WinHttpCloseHandle(hConnect);
        return resp;
    }

    std::wstring headers = L"Content-Type: application/json\r\nAccept-Language: en-US,en;q=0.9\r\n";
    if (!extraHeaders.empty()) {
        headers += std::wstring(extraHeaders.begin(), extraHeaders.end()) + L"\r\n";
    }

    BOOL sendOk = WinHttpSendRequest(
        hRequest,
        headers.c_str(),
        (DWORD)headers.length(),
        (LPVOID)jsonBody.c_str(),
        (DWORD)jsonBody.length(),
        (DWORD)jsonBody.length(),
        0
    );

    if (!sendOk || !WinHttpReceiveResponse(hRequest, nullptr)) {
        resp.errorMessage = "WinHttpSendRequest/ReceiveResponse failed";
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return resp;
    }

    DWORD statusCode = 0;
    DWORD size = sizeof(statusCode);
    WinHttpQueryHeaders(
        hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &size,
        WINHTTP_NO_HEADER_INDEX
    );
    resp.statusCode = static_cast<int>(statusCode);

    std::vector<char> buffer;
    DWORD bytesRead = 0;
    do {
        DWORD bytesAvailable = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable) || bytesAvailable == 0) break;

        size_t curSize = buffer.size();
        buffer.resize(curSize + bytesAvailable);
        if (!WinHttpReadData(hRequest, buffer.data() + curSize, bytesAvailable, &bytesRead) || bytesRead == 0) {
            buffer.resize(curSize);
            break;
        }
        buffer.resize(curSize + bytesRead);
    } while (bytesRead > 0);

    resp.body = std::string(buffer.begin(), buffer.end());
    resp.success = (resp.statusCode >= 200 && resp.statusCode < 300);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    return resp;
}

std::vector<uint8_t> HttpClient::DownloadBinary(const std::string& url) {
    std::vector<uint8_t> result;
    if (!m_hSession) return result;

    std::wstring host, path;
    INTERNET_PORT port = 0;
    bool isHttps = true;
    if (!ParseUrl(url, host, path, port, isHttps)) return result;

    HINTERNET hConnect = WinHttpConnect(m_hSession, host.c_str(), port, 0);
    if (!hConnect) return result;

    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect, L"GET", path.c_str(), nullptr,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags
    );

    if (hRequest) {
        if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(hRequest, nullptr)) {
            
            DWORD bytesRead = 0;
            do {
                DWORD bytesAvailable = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &bytesAvailable) || bytesAvailable == 0) break;

                size_t curSize = result.size();
                result.resize(curSize + bytesAvailable);
                if (!WinHttpReadData(hRequest, result.data() + curSize, bytesAvailable, &bytesRead) || bytesRead == 0) {
                    result.resize(curSize);
                    break;
                }
                result.resize(curSize + bytesRead);
            } while (bytesRead > 0);
        }
        WinHttpCloseHandle(hRequest);
    }
    WinHttpCloseHandle(hConnect);
    return result;
}
