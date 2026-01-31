#pragma once

#include <juce_core/juce_core.h>
#include <functional>
#include <vector>

namespace musicgpt {

enum class StemType {
    Vocals = 1 << 0,
    Drums = 1 << 1,
    Bass = 1 << 2,
    Other = 1 << 3,
    Instrumental = 1 << 4,
    All = Vocals | Drums | Bass | Other
};

inline StemType operator|(StemType a, StemType b) {
    return static_cast<StemType>(static_cast<int>(a) | static_cast<int>(b));
}

inline StemType operator&(StemType a, StemType b) {
    return static_cast<StemType>(static_cast<int>(a) & static_cast<int>(b));
}

inline bool hasStem(StemType set, StemType stem) {
    return (static_cast<int>(set) & static_cast<int>(stem)) != 0;
}

enum class JobStatus {
    Pending,
    Processing,
    Succeeded,
    Failed,
    Cancelled
};

enum class ErrorType {
    None,
    NetworkError,
    AuthError,
    ValidationError,
    QuotaExceeded,
    ServerError,
    ParseError,
    FileIOError,
    Cancelled
};

struct StemResult {
    StemType type;
    juce::File file;
    juce::String url;
};

struct ExtractionResult {
    juce::String jobId;
    JobStatus status = JobStatus::Pending;
    ErrorType error = ErrorType::None;
    juce::String errorMessage;
    std::vector<StemResult> stems;
};

struct ProgressInfo {
    enum class Phase {
        Uploading,
        Processing,
        Downloading
    };

    Phase phase = Phase::Uploading;
    float progress = 0.0f;  // 0.0 - 1.0
    juce::String message;
};

using ProgressCallback = std::function<void(const ProgressInfo&)>;
using CompletionCallback = std::function<void(const ExtractionResult&)>;

} // namespace musicgpt
