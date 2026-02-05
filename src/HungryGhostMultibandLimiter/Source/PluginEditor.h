#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class HungryGhostMultibandLimiterAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit HungryGhostMultibandLimiterAudioProcessorEditor(HungryGhostMultibandLimiterAudioProcessor&);
    ~HungryGhostMultibandLimiterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    HungryGhostMultibandLimiterAudioProcessor& processorRef;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostMultibandLimiterAudioProcessorEditor)
};
