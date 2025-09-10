#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Threshold.h"
#include "../Layout.h"
#include "../InputTrimSection.h"

// InputsColumn stacks the Input Trim section above the Threshold section,
// matching the HTML concept order while keeping them in the same column.
class InputsColumn : public juce::Component {
public:
    explicit InputsColumn(HungryGhostLimiterAudioProcessor::APVTS& apvts)
        : input(apvts), threshold(apvts)
    {
        addAndMakeVisible(input);
        addAndMakeVisible(threshold);
    }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf)
    {
        input.setSliderLookAndFeel(lnf);
        threshold.setSliderLookAndFeel(lnf);
    }

    InputTrimSection& getInput() { return input; }
    StereoThreshold&  getThreshold() { return threshold; }

    void resized() override
    {
        juce::Grid g;
        using Track = juce::Grid::TrackInfo;
        g.templateColumns = { Track(juce::Grid::Fr(1)) };
        g.templateRows = {
            Track(juce::Grid::Px(Layout::kLargeSliderRowHeightPx + Layout::kTitleRowHeightPx + Layout::kChannelLabelRowHeightPx + Layout::kLinkRowHeightPx + 12)),
            Track(juce::Grid::Fr(1))
        };
        g.rowGap = juce::Grid::Px(Layout::kRowGapPx);
        g.items = {
            juce::GridItem(input).withMargin(Layout::kCellMarginPx),
            juce::GridItem(threshold).withMargin(Layout::kCellMarginPx)
        };
        g.performLayout(getLocalBounds());
    }

private:
    InputTrimSection input;
    StereoThreshold  threshold;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputsColumn)
};

