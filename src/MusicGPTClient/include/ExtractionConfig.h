#pragma once

#include <juce_core/juce_core.h>

namespace musicgpt {

struct ExtractionConfig {
    juce::String apiEndpoint = "https://api.musicgpt.com/api/public/v1";
    juce::String apiKey;

    juce::File outputDirectory;

    int connectionTimeoutMs = 30000;
    int transferTimeoutMs = 300000;  // 5 minutes for large files
    int pollIntervalMs = 2000;
    int maxRetries = 3;
    int retryDelayMs = 1000;

    bool validateCertificates = true;
};

} // namespace musicgpt
