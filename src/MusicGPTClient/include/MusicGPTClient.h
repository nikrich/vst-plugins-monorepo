#pragma once

/**
 * MusicGPT Extraction Client Library
 *
 * A JUCE-compatible client for the MusicGPT stem extraction API.
 *
 * Usage:
 *   musicgpt::ExtractionConfig config;
 *   config.apiKey = "your-api-key";
 *   config.outputDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
 *
 *   musicgpt::ExtractionClient client(config);
 *
 *   client.extractStems(
 *       audioFile,
 *       musicgpt::StemType::All,
 *       [](const musicgpt::ProgressInfo& progress) {
 *           // Update UI with progress
 *       },
 *       [](const musicgpt::ExtractionResult& result) {
 *           if (result.status == musicgpt::JobStatus::Succeeded) {
 *               // Use result.stems
 *           }
 *       }
 *   );
 */

#include "ExtractionConfig.h"
#include "ExtractionJob.h"
#include "ExtractionClient.h"
