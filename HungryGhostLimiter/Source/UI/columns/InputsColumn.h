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
        auto bounds = getLocalBounds();
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::column;
        fb.alignContent = juce::FlexBox::AlignContent::stretch;

        fb.items.add(juce::FlexItem(input)
            .withHeight((float)(Layout::kTitleRowHeightPx + Layout::kChannelLabelRowHeightPx + Layout::kLargeSliderRowHeightPx + Layout::kLinkRowHeightPx + 12))
            .withMargin({ (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx }));

        // spacer to push content to top
        fb.items.add(juce::FlexItem().withFlex(1.0f));

        fb.performLayout(bounds);
    }

private:
    InputTrimSection input;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputsColumn)
};

