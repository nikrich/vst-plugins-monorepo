/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class HungryGhostLimiterAudioProcessor : public juce::AudioProcessor
{
public:
    HungryGhostLimiterAudioProcessor();
    ~HungryGhostLimiterAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlockExpected) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "HungryGhostLimiter"; }
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

    // exposed to editor (smoothed gain-reduction, dB, >= 0)
    float getSmoothedAttenDb() const { return attenDbSmoothed.getCurrentValue(); }

private:
    float sampleRateHz = 44100.0f;
    juce::LinearSmoothedValue<float> attenDbSmoothed{ 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostLimiterAudioProcessor)
};
