#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Controls/StereoLinkedBars.h>
#include "../PluginProcessor.h"

class StereoCeiling : public juce::Component
{
public:
    explicit StereoCeiling(HungryGhostLimiterAudioProcessor::APVTS& apvts)
        : bars(apvts, "OUT CEILING", "outCeilingL", "outCeilingR", "outCeilingLink")
    {
        addAndMakeVisible(bars);
    }

    void resized() override { bars.setBounds(getLocalBounds()); }
    void setSliderLookAndFeel(juce::LookAndFeel* lnf) { bars.setSliderLookAndFeel(lnf); }
    void setLinkLookAndFeel(juce::LookAndFeel* lnf) { bars.setLinkLookAndFeel(lnf); }

private:
    StereoLinkedBars bars;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoCeiling)
};

