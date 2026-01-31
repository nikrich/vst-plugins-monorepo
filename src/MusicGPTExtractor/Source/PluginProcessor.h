/*
  ==============================================================================
    MusicGPTExtractor - AI-powered stem extraction plugin
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <atomic>
#include "audio/StemPlayer.h"

class MusicGPTExtractorAudioProcessor : public juce::AudioProcessor
{
public:
    MusicGPTExtractorAudioProcessor();
    ~MusicGPTExtractorAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MusicGPTExtractor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS apvts{ *this, nullptr, "params", createParameterLayout() };
    static APVTS::ParameterLayout createParameterLayout();

    // Stem player for extracted stems
    StemPlayer& getStemPlayer() { return stemPlayer; }

private:
    double currentSampleRate = 44100.0;
    StemPlayer stemPlayer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicGPTExtractorAudioProcessor)
};
