#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Controls/StereoLinkedBars.h>
#include "../PluginProcessor.h"

// Input section, using the same shared UI as Threshold and Ceiling
class InputTrimSection : public juce::Component {
public:
    explicit InputTrimSection(HungryGhostLimiterAudioProcessor::APVTS& apvts)
        : bars(apvts, "INPUT", "inTrimL", "inTrimR", "inTrimLink")
    {
        addAndMakeVisible(bars);
    }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf)
    {
        bars.setSliderLookAndFeel(lnf);
    }

    void setLinkLookAndFeel(juce::LookAndFeel* lnf)
    {
        bars.setLinkLookAndFeel(lnf);
    }

    void resized() override
    {
        bars.setBounds(getLocalBounds());
    }

private:
    StereoLinkedBars bars;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputTrimSection)
};

