#pragma once

#include <JuceHeader.h>
#include "DSP/ReverbEngine.h"
#include "DSP/ParameterTypes.h"

class HungryGhostReverbAudioProcessor : public juce::AudioProcessor {
public:
    HungryGhostReverbAudioProcessor();
    ~HungryGhostReverbAudioProcessor() override = default;

    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HungryGhostReverb"; }

    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    hgr::dsp::ReverbEngine reverb;
    hgr::dsp::ReverbParameters currentParams;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
};

