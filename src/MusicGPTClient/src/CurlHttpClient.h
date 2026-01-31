#pragma once

#include <juce_core/juce_core.h>
#include <curl/curl.h>
#include <functional>
#include <memory>

namespace musicgpt {

struct HttpResponse {
    long statusCode = 0;
    juce::String body;
    juce::String errorMessage;
    bool success = false;
};

using ProgressCallback = std::function<void(float progress)>;

class CurlHttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient();

    CurlHttpClient(const CurlHttpClient&) = delete;
    CurlHttpClient& operator=(const CurlHttpClient&) = delete;

    void setApiKey(const juce::String& apiKey);
    void setBaseUrl(const juce::String& baseUrl);
    void setConnectionTimeout(int timeoutMs);
    void setTransferTimeout(int timeoutMs);
    void setValidateCertificates(bool validate);

    HttpResponse postMultipart(
        const juce::String& endpoint,
        const juce::File& file,
        const juce::String& fieldName,
        const std::vector<std::pair<juce::String, juce::String>>& formFields,
        ProgressCallback progressCallback = nullptr
    );

    HttpResponse get(const juce::String& endpoint);

    bool downloadFile(
        const juce::String& url,
        const juce::File& destination,
        ProgressCallback progressCallback = nullptr
    );

    void cancel();
    bool isCancelled() const;

private:
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t writeFileCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static int progressCallbackWrapper(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                       curl_off_t ultotal, curl_off_t ulnow);

    void setupCommonOptions(CURL* curl);
    juce::String buildUrl(const juce::String& endpoint) const;

    juce::String apiKey_;
    juce::String baseUrl_;
    int connectionTimeoutMs_ = 30000;
    int transferTimeoutMs_ = 300000;
    bool validateCertificates_ = true;
    std::atomic<bool> cancelled_{false};

    struct ProgressData {
        ProgressCallback callback;
        std::atomic<bool>* cancelled;
        bool isUpload;
    };
};

} // namespace musicgpt
