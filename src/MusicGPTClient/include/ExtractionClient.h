#pragma once

#include "ExtractionJob.h"
#include "ExtractionConfig.h"
#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include <memory>

namespace musicgpt {

class ExtractionClient : private juce::AsyncUpdater {
public:
    explicit ExtractionClient(const ExtractionConfig& config);
    ~ExtractionClient() override;

    ExtractionClient(const ExtractionClient&) = delete;
    ExtractionClient& operator=(const ExtractionClient&) = delete;

    /**
     * Start an asynchronous stem extraction.
     *
     * @param audioFile The audio file to process
     * @param stems Which stems to extract (can be combined with |)
     * @param onProgress Called periodically with progress updates (on message thread)
     * @param onComplete Called when extraction completes or fails (on message thread)
     * @return A job ID that can be used to cancel the extraction
     */
    juce::String extractStems(
        const juce::File& audioFile,
        StemType stems,
        ProgressCallback onProgress,
        CompletionCallback onComplete
    );

    /**
     * Cancel a specific extraction job.
     */
    void cancelJob(const juce::String& jobId);

    /**
     * Cancel all pending and in-progress extractions.
     */
    void cancelAll();

    /**
     * Check if any extraction is currently in progress.
     */
    bool isBusy() const;

    /**
     * Get the number of jobs currently queued or in progress.
     */
    int getActiveJobCount() const;

private:
    void handleAsyncUpdate() override;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace musicgpt
