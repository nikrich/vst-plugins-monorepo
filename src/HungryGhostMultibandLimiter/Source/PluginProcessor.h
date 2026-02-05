/*
  ==============================================================================
    HungryGhostMultibandLimiter – multiband limiting for transparent loudness
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <vector>
#include <array>
#include <juce_dsp/juce_dsp.h>
#include "dsp/BandSplitterIIR.h"

//==============================================================================

class HungryGhostMultibandLimiterAudioProcessor : public juce::AudioProcessor
{
public:
    HungryGhostMultibandLimiterAudioProcessor();
    ~HungryGhostMultibandLimiterAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlockExpected) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "HungryGhostMultibandLimiter"; }
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
    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS apvts{ *this, nullptr, "params", createParameterLayout() };
    static APVTS::ParameterLayout createParameterLayout();

    //==============================================================================
    // DSP State Access (for future DSP component implementation)
    double getSampleRateHz() const { return sampleRateHz; }
    int getSamplesPerBlock() const { return samplesPerBlockExpected; }

private:
    double sampleRateHz = 44100.0;
    int samplesPerBlockExpected = 512;

    // Parameter cache (for efficient lookup in processBlock)
    int cachedBandCount = 2;
    float cachedCrossoverHz = 120.0f;
    int cachedOversamplingFactor = 1;
    float cachedLookAheadMs = 3.0f;

    // Helper method to ensure band buffers are properly sized
    void ensureBandBuffers(int numChannels, int numSamples);

    // ===== STORY-MBL-002: Multiband Crossover System =====
    std::unique_ptr<hgml::BandSplitterIIR> splitter;
    std::vector<juce::AudioBuffer<float>> bandBuffers;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostMultibandLimiterAudioProcessor)
};
