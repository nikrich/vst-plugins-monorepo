/*
  ==============================================================================
    HungryGhostMultibandLimiter Plugin Editor
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/MetersPanel.h"

//==============================================================================

class HungryGhostMultibandLimiterAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HungryGhostMultibandLimiterAudioProcessorEditor(HungryGhostMultibandLimiterAudioProcessor&);
    ~HungryGhostMultibandLimiterAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    HungryGhostMultibandLimiterAudioProcessor& processor;

    // STORY-MBL-007: Metering panel with per-band and master levels
    std::unique_ptr<MetersPanel> metersPanel;

    // Timer for updating meter data from processor
    void timerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostMultibandLimiterAudioProcessorEditor)
};
