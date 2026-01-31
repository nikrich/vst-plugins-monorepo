/*
  ==============================================================================
    MusicGPTExtractor - Stem extraction plugin using MusicGPT API
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <memory>
#include "api/ExtractionClient.h"
#include "audio/StemPlayer.h"

//==============================================================================

class MusicGPTExtractorAudioProcessor : public juce::AudioProcessor
{
public:
    MusicGPTExtractorAudioProcessor();
    ~MusicGPTExtractorAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlockExpected) override;
    void releaseResources() override {}
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
    // Extraction API client
    api::ExtractionClient& getExtractionClient() { return extractionClient; }

    // Stem player for audio playback
    audio::StemPlayer& getStemPlayer() { return stemPlayer; }

    // Transport state
    bool isPlaying() const { return playing.load(); }
    void setPlaying(bool shouldPlay) { playing.store(shouldPlay); }
    double getPlaybackPosition() const { return playbackPosition.load(); }

private:
    double sampleRateHz = 44100.0;

    api::ExtractionClient extractionClient;
    audio::StemPlayer stemPlayer;

    std::atomic<bool> playing { false };
    std::atomic<double> playbackPosition { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicGPTExtractorAudioProcessor)
};
