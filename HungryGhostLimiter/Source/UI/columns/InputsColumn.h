#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Layout.h"
#include "../InputTrimSection.h"

// InputsColumn shows input-related controls (trim/link), using FlexBox
class InputsColumn : public juce::Component {
public:
    explicit InputsColumn(HungryGhostLimiterAudioProcessor::APVTS& apvts)
        : input(apvts)
    {
        addAndMakeVisible(input);
    }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf)
    {
        input.setSliderLookAndFeel(lnf);
    }

    InputTrimSection& getInput() { return input; }

    void resized() override
    {
        // Match Threshold and Ceiling: fill the allotted cell, margins come from outer grid
        input.setBounds(getLocalBounds());
    }

private:
    InputTrimSection input;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputsColumn)
};

