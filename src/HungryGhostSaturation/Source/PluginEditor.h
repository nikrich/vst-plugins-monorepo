#pragma once

#include <JuceHeader.h>

class HungryGhostSaturationAudioProcessor;

class HungryGhostSaturationAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit HungryGhostSaturationAudioProcessorEditor(HungryGhostSaturationAudioProcessor&);
    ~HungryGhostSaturationAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    HungryGhostSaturationAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostSaturationAudioProcessorEditor)
};
