#pragma once

#include <juce_core/juce_core.h>
#include "../include/ExtractionJob.h"
#include <optional>

namespace musicgpt {

class JsonParser {
public:
    struct UploadResponse {
        juce::String jobId;
        bool success = false;
        juce::String errorMessage;
    };

    struct StatusResponse {
        juce::String jobId;
        JobStatus status = JobStatus::Pending;
        float progress = 0.0f;
        std::vector<StemResult> stems;
        bool success = false;
        juce::String errorMessage;
        ErrorType errorType = ErrorType::None;
    };

    static UploadResponse parseUploadResponse(const juce::String& json);
    static StatusResponse parseStatusResponse(const juce::String& json);
    static ErrorType classifyError(int httpStatus, const juce::String& responseBody);

private:
    static JobStatus parseJobStatus(const juce::String& statusStr);
    static StemType parseStemType(const juce::String& stemStr);
};

} // namespace musicgpt
