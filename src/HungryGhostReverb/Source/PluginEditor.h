#pragma once

#include <JuceHeader.h>

class HungryGhostReverbAudioProcessor;

class HungryGhostReverbAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit HungryGhostReverbAudioProcessorEditor(HungryGhostReverbAudioProcessor&);
    ~HungryGhostReverbAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    HungryGhostReverbAudioProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostReverbAudioProcessorEditor)
};

