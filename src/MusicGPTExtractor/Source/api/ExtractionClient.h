#pragma once
#include <juce_core/juce_core.h>
#include <functional>
#include <memory>
#include <atomic>

namespace api {

// Extraction job status
enum class ExtractionStatus
{
    Idle,
    Uploading,
    Processing,
    Downloading,
    Complete,
    Error
};

// Stem types returned by extraction
enum class StemType
{
    Vocals,
    Drums,
    Bass,
    Other,
    Instrumental
};

// Result of an extraction job
struct ExtractionResult
{
    bool success = false;
    juce::String errorMessage;
    juce::StringArray stemPaths; // Local paths to downloaded stems
};

// Callback types
using StatusCallback = std::function<void(ExtractionStatus status, float progress)>;
using CompletionCallback = std::function<void(const ExtractionResult& result)>;

// Client for MusicGPT stem extraction API
class ExtractionClient
{
public:
    ExtractionClient() = default;
    ~ExtractionClient() { cancelExtraction(); }

    // Set API endpoint and credentials
    void setEndpoint(const juce::String& url) { apiEndpoint = url; }
    void setApiKey(const juce::String& key) { apiKey = key; }

    // Start extraction job for an audio file
    // Returns immediately; progress reported via callbacks
    void extractStems(const juce::File& audioFile,
                      StatusCallback onStatus,
                      CompletionCallback onComplete);

    // Cancel any in-progress extraction
    void cancelExtraction();

    // Check if extraction is in progress
    bool isExtracting() const { return extracting.load(); }

    // Get current status
    ExtractionStatus getStatus() const { return currentStatus.load(); }
    float getProgress() const { return progress.load(); }

private:
    juce::String apiEndpoint { "https://api.musicgpt.example/v1/extract" };
    juce::String apiKey;

    std::atomic<bool> extracting { false };
    std::atomic<ExtractionStatus> currentStatus { ExtractionStatus::Idle };
    std::atomic<float> progress { 0.0f };

    std::unique_ptr<juce::Thread> workerThread;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ExtractionClient)
};

} // namespace api
