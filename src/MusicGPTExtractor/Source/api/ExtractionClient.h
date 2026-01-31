#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <atomic>

struct ExtractionResult
{
    bool success { false };
    juce::String errorMessage;

    // Paths to extracted stem files (local temp files after download)
    juce::File vocalsPath;
    juce::File drumsPath;
    juce::File bassPath;
    juce::File otherPath;
};

// Async stem extraction client using libcurl
class ExtractionClient
{
public:
    using ProgressCallback = std::function<void(float progress)>;
    using CompletionCallback = std::function<void(const ExtractionResult& result)>;

    ExtractionClient() = default;
    ~ExtractionClient() { cancelCurrentJob(); }

    void setApiEndpoint(const juce::String& endpoint) { apiEndpoint = endpoint; }
    void setApiKey(const juce::String& key) { apiKey = key; }

    // Start async extraction job
    void extractStems(const juce::File& audioFile,
                      ProgressCallback onProgress,
                      CompletionCallback onComplete)
    {
        cancelCurrentJob();

        isRunning = true;
        extractionThread = std::make_unique<std::thread>([this, audioFile, onProgress, onComplete]() {
            runExtraction(audioFile, onProgress, onComplete);
        });
    }

    void cancelCurrentJob()
    {
        isRunning = false;
        if (extractionThread && extractionThread->joinable())
            extractionThread->join();
        extractionThread.reset();
    }

    bool isBusy() const { return isRunning.load(); }

private:
    void runExtraction(const juce::File& audioFile,
                       ProgressCallback onProgress,
                       CompletionCallback onComplete)
    {
        ExtractionResult result;

        // Phase 1: Upload file
        if (onProgress) onProgress(0.1f);

        juce::String jobId = uploadFile(audioFile);
        if (jobId.isEmpty())
        {
            result.success = false;
            result.errorMessage = "Failed to upload file";
            if (onComplete) onComplete(result);
            return;
        }

        // Phase 2: Poll for completion
        if (onProgress) onProgress(0.2f);

        while (isRunning.load())
        {
            auto status = pollJobStatus(jobId);

            if (status.first == "completed")
            {
                if (onProgress) onProgress(0.8f);
                break;
            }
            else if (status.first == "failed")
            {
                result.success = false;
                result.errorMessage = status.second;
                if (onComplete) onComplete(result);
                return;
            }
            else if (status.first == "processing")
            {
                float progress = 0.2f + status.second.getFloatValue() * 0.6f;
                if (onProgress) onProgress(progress);
            }

            std::this_thread::sleep_for(std::chrono::seconds(2));
        }

        if (!isRunning.load())
            return;

        // Phase 3: Download stems
        if (onProgress) onProgress(0.9f);

        auto tempDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getChildFile("musicgpt_" + juce::String(juce::Time::currentTimeMillis()));
        tempDir.createDirectory();

        result.vocalsPath = downloadStem(jobId, "vocals", tempDir);
        result.drumsPath = downloadStem(jobId, "drums", tempDir);
        result.bassPath = downloadStem(jobId, "bass", tempDir);
        result.otherPath = downloadStem(jobId, "other", tempDir);

        result.success = result.vocalsPath.existsAsFile() &&
                         result.drumsPath.existsAsFile() &&
                         result.bassPath.existsAsFile() &&
                         result.otherPath.existsAsFile();

        if (!result.success)
            result.errorMessage = "Failed to download one or more stems";

        if (onProgress) onProgress(1.0f);
        if (onComplete) onComplete(result);
    }

    juce::String uploadFile(const juce::File& file)
    {
        // TODO: Implement actual curl upload to apiEndpoint
        // POST /api/extract with multipart form data
        // Returns job ID on success

        // Placeholder - in real implementation use libcurl
        juce::ignoreUnused(file);
        return "job_" + juce::String(juce::Random::getSystemRandom().nextInt64());
    }

    std::pair<juce::String, juce::String> pollJobStatus(const juce::String& jobId)
    {
        // TODO: Implement actual curl GET to apiEndpoint/jobs/{jobId}
        // Returns {"status": "processing|completed|failed", "progress": 0.5, "error": "..."}

        // Placeholder - simulate processing
        juce::ignoreUnused(jobId);
        static int pollCount = 0;
        pollCount++;

        if (pollCount > 5)
        {
            pollCount = 0;
            return { "completed", "" };
        }
        return { "processing", juce::String(pollCount * 0.2f) };
    }

    juce::File downloadStem(const juce::String& jobId, const juce::String& stemType, const juce::File& destDir)
    {
        // TODO: Implement actual curl download from apiEndpoint/jobs/{jobId}/stems/{stemType}
        // Save to destDir/{stemType}.wav

        // Placeholder - create empty file for now
        juce::ignoreUnused(jobId);
        auto destFile = destDir.getChildFile(stemType + ".wav");
        destFile.create();
        return destFile;
    }

    juce::String apiEndpoint { "https://api.musicgpt.example/v1" };
    juce::String apiKey;

    std::atomic<bool> isRunning { false };
    std::unique_ptr<std::thread> extractionThread;
};
