#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Controls/StereoLinkedBars.h>
#include "../PluginProcessor.h"

class StereoThreshold : public juce::Component
{
public:
    explicit StereoThreshold(HungryGhostLimiterAudioProcessor::APVTS& apvts)
        : bars(apvts, "THRESHOLD", "thresholdL", "thresholdR", "thresholdLink")
    {
        addAndMakeVisible(bars);
    }

    void paintOverChildren(juce::Graphics&) override {}
    void resized() override { bars.setBounds(getLocalBounds()); }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf) { bars.setSliderLookAndFeel(lnf); }
    void setLinkLookAndFeel(juce::LookAndFeel* lnf) { bars.setLinkLookAndFeel(lnf); }

private:
    StereoLinkedBars bars;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoThreshold)
};
