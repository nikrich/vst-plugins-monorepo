#include "CurlHttpClient.h"

namespace musicgpt {

CurlHttpClient::CurlHttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

CurlHttpClient::~CurlHttpClient() {
    curl_global_cleanup();
}

void CurlHttpClient::setApiKey(const juce::String& apiKey) {
    apiKey_ = apiKey;
}

void CurlHttpClient::setBaseUrl(const juce::String& baseUrl) {
    baseUrl_ = baseUrl;
}

void CurlHttpClient::setConnectionTimeout(int timeoutMs) {
    connectionTimeoutMs_ = timeoutMs;
}

void CurlHttpClient::setTransferTimeout(int timeoutMs) {
    transferTimeoutMs_ = timeoutMs;
}

void CurlHttpClient::setValidateCertificates(bool validate) {
    validateCertificates_ = validate;
}

void CurlHttpClient::cancel() {
    cancelled_.store(true);
}

bool CurlHttpClient::isCancelled() const {
    return cancelled_.load();
}

size_t CurlHttpClient::writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    auto* response = static_cast<juce::String*>(userp);
    response->append(juce::String::fromUTF8(static_cast<char*>(contents), static_cast<int>(totalSize)), totalSize);
    return totalSize;
}

size_t CurlHttpClient::writeFileCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* outStream = static_cast<juce::FileOutputStream*>(userp);
    if (outStream == nullptr || outStream->failedToOpen())
        return 0;

    size_t totalSize = size * nmemb;
    if (!outStream->write(contents, totalSize))
        return 0;

    return totalSize;
}

int CurlHttpClient::progressCallbackWrapper(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                                            curl_off_t ultotal, curl_off_t ulnow) {
    auto* data = static_cast<ProgressData*>(clientp);

    if (data->cancelled && data->cancelled->load())
        return 1;  // Non-zero return cancels the transfer

    if (data->callback) {
        float progress = 0.0f;
        if (data->isUpload && ultotal > 0) {
            progress = static_cast<float>(ulnow) / static_cast<float>(ultotal);
        } else if (!data->isUpload && dltotal > 0) {
            progress = static_cast<float>(dlnow) / static_cast<float>(dltotal);
        }
        data->callback(progress);
    }

    return 0;
}

void CurlHttpClient::setupCommonOptions(CURL* curl) {
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(connectionTimeoutMs_));
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(transferTimeoutMs_));

    if (!validateCertificates_) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    }

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
}

juce::String CurlHttpClient::buildUrl(const juce::String& endpoint) const {
    if (endpoint.startsWithIgnoreCase("http://") || endpoint.startsWithIgnoreCase("https://"))
        return endpoint;

    juce::String url = baseUrl_;
    if (!url.endsWithChar('/') && !endpoint.startsWithChar('/'))
        url += "/";
    url += endpoint;
    return url;
}

HttpResponse CurlHttpClient::postMultipart(
    const juce::String& endpoint,
    const juce::File& file,
    const juce::String& fieldName,
    const std::vector<std::pair<juce::String, juce::String>>& formFields,
    HttpProgressCallback progressCallback
) {
    HttpResponse response;
    cancelled_.store(false);

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.errorMessage = "Failed to initialize CURL";
        return response;
    }

    setupCommonOptions(curl);

    juce::String url = buildUrl(endpoint);
    curl_easy_setopt(curl, CURLOPT_URL, url.toRawUTF8());

    // Set up headers
    struct curl_slist* headers = nullptr;
    if (apiKey_.isNotEmpty()) {
        juce::String authHeader = "Authorization: Bearer " + apiKey_;
        headers = curl_slist_append(headers, authHeader.toRawUTF8());
    }
    if (headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set up multipart form
    curl_mime* mime = curl_mime_init(curl);

    // Add file
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_name(part, fieldName.toRawUTF8());
    curl_mime_filedata(part, file.getFullPathName().toRawUTF8());

    // Add other form fields
    for (const auto& field : formFields) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, field.first.toRawUTF8());
        curl_mime_data(part, field.second.toRawUTF8(), CURL_ZERO_TERMINATED);
    }

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    // Set up response handling
    juce::String responseBody;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    // Set up progress callback
    ProgressData progressData{progressCallback, &cancelled_, true};
    if (progressCallback) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallbackWrapper);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        response.body = responseBody;
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
    } else if (res == CURLE_ABORTED_BY_CALLBACK) {
        response.errorMessage = "Request cancelled";
    } else {
        response.errorMessage = curl_easy_strerror(res);
    }

    curl_mime_free(mime);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

HttpResponse CurlHttpClient::get(const juce::String& endpoint) {
    HttpResponse response;
    cancelled_.store(false);

    CURL* curl = curl_easy_init();
    if (!curl) {
        response.errorMessage = "Failed to initialize CURL";
        return response;
    }

    setupCommonOptions(curl);

    juce::String url = buildUrl(endpoint);
    curl_easy_setopt(curl, CURLOPT_URL, url.toRawUTF8());

    // Set up headers
    struct curl_slist* headers = nullptr;
    if (apiKey_.isNotEmpty()) {
        juce::String authHeader = "Authorization: Bearer " + apiKey_;
        headers = curl_slist_append(headers, authHeader.toRawUTF8());
    }
    if (headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set up response handling
    juce::String responseBody;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    // Perform request
    CURLcode res = curl_easy_perform(curl);

    if (res == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.statusCode);
        response.body = responseBody;
        response.success = (response.statusCode >= 200 && response.statusCode < 300);
    } else if (res == CURLE_ABORTED_BY_CALLBACK) {
        response.errorMessage = "Request cancelled";
    } else {
        response.errorMessage = curl_easy_strerror(res);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return response;
}

bool CurlHttpClient::downloadFile(
    const juce::String& url,
    const juce::File& destination,
    HttpProgressCallback progressCallback
) {
    cancelled_.store(false);

    CURL* curl = curl_easy_init();
    if (!curl)
        return false;

    setupCommonOptions(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.toRawUTF8());

    // Set up headers
    struct curl_slist* headers = nullptr;
    if (apiKey_.isNotEmpty()) {
        juce::String authHeader = "Authorization: Bearer " + apiKey_;
        headers = curl_slist_append(headers, authHeader.toRawUTF8());
    }
    if (headers)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Create output file
    destination.deleteFile();
    auto outStream = destination.createOutputStream();
    if (!outStream || outStream->failedToOpen()) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, outStream.get());

    // Set up progress callback
    ProgressData progressData{progressCallback, &cancelled_, false};
    if (progressCallback) {
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progressCallbackWrapper);
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    }

    CURLcode res = curl_easy_perform(curl);

    outStream->flush();
    outStream.reset();

    long statusCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    bool success = (res == CURLE_OK && statusCode >= 200 && statusCode < 300);
    if (!success)
        destination.deleteFile();

    return success;
}

} // namespace musicgpt
