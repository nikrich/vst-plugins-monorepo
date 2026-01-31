/*
  ==============================================================================
    MusicGPTExtractor - Stem extraction plugin using MusicGPT API
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include <ExtractionClient.h>
#include <ExtractionConfig.h>
#include <ExtractionJob.h>
#include <audio/StemPlayer.h>

//==============================================================================

class MusicGPTExtractorAudioProcessor : public juce::AudioProcessor
{
public:
    MusicGPTExtractorAudioProcessor();
    ~MusicGPTExtractorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlockExpected) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "MusicGPTExtractor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    // API Configuration
    void setApiKey(const juce::String& key);
    void setApiEndpoint(const juce::String& endpoint);
    juce::String getApiKey() const { return extractionConfig.apiKey; }

    //==============================================================================
    // Extraction workflow
    using ExtractionProgressCallback = std::function<void(const musicgpt::ProgressInfo&)>;
    using ExtractionCompleteCallback = std::function<void(const musicgpt::ExtractionResult&)>;

    void startExtraction(const juce::File& audioFile,
                         ExtractionProgressCallback onProgress,
                         ExtractionCompleteCallback onComplete);
    void cancelExtraction();
    bool isExtracting() const;

    //==============================================================================
    // Stem player for audio playback
    audio::StemPlayer& getStemPlayer() { return stemPlayer; }

    // Load stems from extraction result
    void loadExtractedStems(const std::vector<musicgpt::StemResult>& stems);

    // Transport state
    bool isPlaying() const { return playing.load(); }
    void setPlaying(bool shouldPlay);
    double getPlaybackPosition() const;
    double getTotalDuration() const;
    void setPlaybackPosition(double pos);

    // Stem paths for state persistence
    const juce::StringArray& getLoadedStemPaths() const { return loadedStemPaths; }
    void setLoadedStemPaths(const juce::StringArray& paths) { loadedStemPaths = paths; }

private:
    void ensureExtractionClient();

    double sampleRateHz = 44100.0;
    int blockSizeExpected = 512;

    musicgpt::ExtractionConfig extractionConfig;
    std::unique_ptr<musicgpt::ExtractionClient> extractionClient;
    juce::String currentJobId;

    audio::StemPlayer stemPlayer;

    std::atomic<bool> playing { false };

    // State persistence
    juce::StringArray loadedStemPaths;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicGPTExtractorAudioProcessor)
};
