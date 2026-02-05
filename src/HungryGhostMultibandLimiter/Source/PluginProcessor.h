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
#include "dsp/LimiterBand.h"
#include "dsp/Utilities.h"

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

    //==============================================================================
    // STORY-MBL-007: Per-band and master metering for visual feedback
    float getBandGainReductionDb(int i) const { return i >= 0 && i < 6 ? bandGainReductionDb[i].load() : 0.0f; }
    float getBandInputDb(int i) const { return i >= 0 && i < 6 ? bandInputDb[i].load() : -60.0f; }
    float getBandOutputDb(int i) const { return i >= 0 && i < 6 ? bandOutputDb[i].load() : -60.0f; }
    float getMasterInputDb() const { return masterInputDb.load(); }
    float getMasterOutputDb() const { return masterOutputDb.load(); }

private:
    double sampleRateHz = 44100.0;
    int samplesPerBlockExpected = 512;

    // STORY-MBL-007: Atomic meter storage for thread-safe UI access
    std::atomic<float> bandGainReductionDb[6] { 0,0,0,0,0,0 };
    std::atomic<float> bandInputDb[6] { -60.0f, -60.0f, -60.0f, -60.0f, -60.0f, -60.0f };
    std::atomic<float> bandOutputDb[6] { -60.0f, -60.0f, -60.0f, -60.0f, -60.0f, -60.0f };
    std::atomic<float> masterInputDb { -60.0f };
    std::atomic<float> masterOutputDb { -60.0f };

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

    // ===== STORY-MBL-003: Per-band Limiting =====
    std::array<hgml::LimiterBand, 2> limiters;  // Up to 2 bands for M1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostMultibandLimiterAudioProcessor)
};
