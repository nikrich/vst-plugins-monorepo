#include "ExtractionClient.h"
#include <juce_audio_formats/juce_audio_formats.h>

namespace api {

//==============================================================================
// Worker thread for async extraction
//==============================================================================
class ExtractionWorker : public juce::Thread
{
public:
    ExtractionWorker(ExtractionClient& owner,
                     const juce::File& file,
                     StatusCallback status,
                     CompletionCallback complete)
        : Thread("ExtractionWorker")
        , client(owner)
        , audioFile(file)
        , onStatus(std::move(status))
        , onComplete(std::move(complete))
    {
    }

    void run() override
    {
        ExtractionResult result;

        // Phase 1: Upload
        updateStatus(ExtractionStatus::Uploading, 0.0f);

        auto uploadResult = uploadFile();
        if (threadShouldExit())
        {
            result.success = false;
            result.errorMessage = "Cancelled";
            callComplete(result);
            return;
        }

        if (!uploadResult.first)
        {
            result.success = false;
            result.errorMessage = uploadResult.second;
            updateStatus(ExtractionStatus::Error, 0.0f);
            callComplete(result);
            return;
        }

        juce::String jobId = uploadResult.second;

        // Phase 2: Poll for completion
        updateStatus(ExtractionStatus::Processing, 0.0f);

        auto pollResult = pollForCompletion(jobId);
        if (threadShouldExit())
        {
            result.success = false;
            result.errorMessage = "Cancelled";
            callComplete(result);
            return;
        }

        if (!pollResult.first)
        {
            result.success = false;
            result.errorMessage = pollResult.second;
            updateStatus(ExtractionStatus::Error, 0.0f);
            callComplete(result);
            return;
        }

        // Phase 3: Download stems
        updateStatus(ExtractionStatus::Downloading, 0.0f);

        auto downloadResult = downloadStems(jobId);
        if (threadShouldExit())
        {
            result.success = false;
            result.errorMessage = "Cancelled";
            callComplete(result);
            return;
        }

        if (!downloadResult.first)
        {
            result.success = false;
            result.errorMessage = downloadResult.second;
            updateStatus(ExtractionStatus::Error, 0.0f);
            callComplete(result);
            return;
        }

        // Success
        result.success = true;
        result.stemPaths = downloadResult.stemPaths;
        updateStatus(ExtractionStatus::Complete, 1.0f);
        callComplete(result);
    }

private:
    struct DownloadResult
    {
        bool first = false;
        juce::String second;
        juce::StringArray stemPaths;
    };

    void updateStatus(ExtractionStatus status, float prog)
    {
        client.currentStatus.store(status);
        client.progress.store(prog);
        if (onStatus)
        {
            auto statusCopy = status;
            auto progCopy = prog;
            auto callback = onStatus;
            juce::MessageManager::callAsync([callback, statusCopy, progCopy]() {
                callback(statusCopy, progCopy);
            });
        }
    }

    void callComplete(const ExtractionResult& result)
    {
        if (onComplete)
        {
            auto resultCopy = result;
            auto callback = onComplete;
            juce::MessageManager::callAsync([callback, resultCopy]() {
                callback(resultCopy);
            });
        }
    }

    std::pair<bool, juce::String> uploadFile()
    {
        // Create URL for upload endpoint
        juce::URL uploadUrl(client.apiEndpoint + "/upload");

        // Read audio file
        juce::MemoryBlock fileData;
        if (!audioFile.loadFileAsData(fileData))
            return { false, "Failed to read audio file" };

        // Create multipart form data
        uploadUrl = uploadUrl.withDataToUpload(
            "file",
            audioFile.getFileName(),
            fileData,
            "audio/mpeg"
        );

        // Build headers string
        juce::String headers = "Authorization: Bearer " + client.apiKey + "\r\n"
                              "Accept: application/json";

        // Make POST request with options built in chain
        auto stream = uploadUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                .withExtraHeaders(headers)
                .withConnectionTimeoutMs(30000)
                .withProgressCallback([this](int bytesUploaded, int totalBytes) {
                    if (totalBytes > 0)
                    {
                        float p = static_cast<float>(bytesUploaded) / static_cast<float>(totalBytes);
                        client.progress.store(p);
                    }
                    return !threadShouldExit();
                })
        );

        if (stream == nullptr)
            return { false, "Failed to connect to server" };

        juce::String response = stream->readEntireStreamAsString();

        // Get status code
        int statusCode = 0;
        if (auto* webStream = dynamic_cast<juce::WebInputStream*>(stream.get()))
            statusCode = webStream->getStatusCode();

        if (statusCode != 200 && statusCode != 201)
            return { false, "Upload failed: HTTP " + juce::String(statusCode) };

        // Parse JSON response for job ID
        auto json = juce::JSON::parse(response);
        if (json.isVoid())
            return { false, "Invalid server response" };

        auto* obj = json.getDynamicObject();
        if (obj == nullptr)
            return { false, "Invalid JSON response" };

        juce::String jobId = obj->getProperty("job_id").toString();
        if (jobId.isEmpty())
            return { false, "No job ID in response" };

        return { true, jobId };
    }

    std::pair<bool, juce::String> pollForCompletion(const juce::String& jobId)
    {
        const int maxPolls = 300;  // 10 minutes at 2s intervals
        const int pollIntervalMs = 2000;

        juce::String headers = "Authorization: Bearer " + client.apiKey + "\r\n"
                              "Accept: application/json";

        for (int i = 0; i < maxPolls && !threadShouldExit(); ++i)
        {
            juce::URL statusUrl(client.apiEndpoint + "/status/" + jobId);

            auto stream = statusUrl.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders(headers)
                    .withConnectionTimeoutMs(10000)
            );

            if (stream != nullptr)
            {
                juce::String response = stream->readEntireStreamAsString();
                auto json = juce::JSON::parse(response);

                if (!json.isVoid())
                {
                    auto* obj = json.getDynamicObject();
                    if (obj != nullptr)
                    {
                        juce::String status = obj->getProperty("status").toString();
                        float serverProgress = static_cast<float>(obj->getProperty("progress"));

                        client.progress.store(serverProgress);

                        if (status == "completed" || status == "complete")
                            return { true, "" };

                        if (status == "failed" || status == "error")
                        {
                            juce::String error = obj->getProperty("error").toString();
                            return { false, error.isEmpty() ? "Processing failed" : error };
                        }
                    }
                }
            }

            // Wait before next poll
            wait(pollIntervalMs);
        }

        if (threadShouldExit())
            return { false, "Cancelled" };

        return { false, "Processing timeout" };
    }

    DownloadResult downloadStems(const juce::String& jobId)
    {
        DownloadResult result;

        // Get stem URLs from API
        juce::URL stemsUrl(client.apiEndpoint + "/stems/" + jobId);

        juce::String headers = "Authorization: Bearer " + client.apiKey + "\r\n"
                              "Accept: application/json";

        auto stream = stemsUrl.createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders(headers)
        );

        if (stream == nullptr)
        {
            result.second = "Failed to fetch stem URLs";
            return result;
        }

        juce::String response = stream->readEntireStreamAsString();
        auto json = juce::JSON::parse(response);

        if (json.isVoid())
        {
            result.second = "Invalid stem response";
            return result;
        }

        auto* obj = json.getDynamicObject();
        if (obj == nullptr)
        {
            result.second = "Invalid JSON";
            return result;
        }

        // Get stems array
        auto* stemsArray = obj->getProperty("stems").getArray();
        if (stemsArray == nullptr)
        {
            result.second = "No stems in response";
            return result;
        }

        // Create output directory
        juce::File outputDir = audioFile.getParentDirectory()
            .getChildFile(audioFile.getFileNameWithoutExtension() + "_stems");
        outputDir.createDirectory();

        // Download each stem
        int stemIndex = 0;
        int numStems = stemsArray->size();
        for (const auto& stemVar : *stemsArray)
        {
            if (threadShouldExit())
            {
                result.second = "Cancelled";
                return result;
            }

            auto* stemObj = stemVar.getDynamicObject();
            if (stemObj == nullptr)
                continue;

            juce::String stemName = stemObj->getProperty("name").toString();
            juce::String stemUrl = stemObj->getProperty("url").toString();

            if (stemUrl.isEmpty())
                continue;

            // Download stem file
            juce::URL downloadUrl(stemUrl);

            auto dlStream = downloadUrl.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withExtraHeaders(headers)
                    .withProgressCallback([this, stemIndex, numStems](int bytesDownloaded, int totalBytes) {
                        if (totalBytes > 0 && numStems > 0)
                        {
                            float baseProgress = static_cast<float>(stemIndex) / static_cast<float>(numStems);
                            float stemProgress = static_cast<float>(bytesDownloaded) / static_cast<float>(totalBytes);
                            float totalProgress = baseProgress + stemProgress / static_cast<float>(numStems);
                            client.progress.store(totalProgress);
                        }
                        return !threadShouldExit();
                    })
            );

            if (dlStream == nullptr)
                continue;

            // Determine file extension from URL or default to .wav
            juce::String ext = ".wav";
            if (stemUrl.containsIgnoreCase(".mp3"))
                ext = ".mp3";
            else if (stemUrl.containsIgnoreCase(".flac"))
                ext = ".flac";

            juce::File stemFile = outputDir.getChildFile(stemName + ext);
            juce::FileOutputStream fileOut(stemFile);

            if (fileOut.openedOk())
            {
                fileOut.writeFromInputStream(*dlStream, -1);
                result.stemPaths.add(stemFile.getFullPathName());
            }

            ++stemIndex;
        }

        if (result.stemPaths.isEmpty())
        {
            result.second = "No stems downloaded";
            return result;
        }

        result.first = true;
        return result;
    }

    ExtractionClient& client;
    juce::File audioFile;
    StatusCallback onStatus;
    CompletionCallback onComplete;
};

//==============================================================================
// ExtractionClient implementation
//==============================================================================

void ExtractionClient::extractStems(const juce::File& audioFile,
                                     StatusCallback onStatus,
                                     CompletionCallback onComplete)
{
    // Cancel any existing extraction
    cancelExtraction();

    if (!audioFile.existsAsFile())
    {
        if (onComplete)
        {
            ExtractionResult result;
            result.success = false;
            result.errorMessage = "File does not exist";
            onComplete(result);
        }
        return;
    }

    if (apiKey.isEmpty())
    {
        if (onComplete)
        {
            ExtractionResult result;
            result.success = false;
            result.errorMessage = "API key not configured";
            onComplete(result);
        }
        return;
    }

    extracting.store(true);
    currentStatus.store(ExtractionStatus::Uploading);
    progress.store(0.0f);

    // Create and start worker thread
    workerThread = std::make_unique<ExtractionWorker>(
        *this, audioFile, std::move(onStatus),
        [this, onComplete](const ExtractionResult& result) {
            extracting.store(false);
            if (onComplete)
                onComplete(result);
        }
    );
    workerThread->startThread();
}

void ExtractionClient::cancelExtraction()
{
    if (workerThread != nullptr)
    {
        workerThread->signalThreadShouldExit();
        workerThread->waitForThreadToExit(5000);
        workerThread.reset();
    }

    extracting.store(false);
    currentStatus.store(ExtractionStatus::Idle);
    progress.store(0.0f);
}

} // namespace api
