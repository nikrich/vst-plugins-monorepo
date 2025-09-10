#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Threshold.h"
#include "../Ceiling.h"
#include "../Layout.h"

// Column that stacks Threshold (L/R + link) above Out Ceiling (L/R + link)
class ThrCeilColumn : public juce::Component {
public:
    explicit ThrCeilColumn(HungryGhostLimiterAudioProcessor::APVTS& apvts)
        : threshold(apvts), ceiling(apvts) 
    {
        addAndMakeVisible(threshold);
        addAndMakeVisible(ceiling);
    }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf)
    {
        threshold.setSliderLookAndFeel(lnf);
        ceiling.setSliderLookAndFeel(lnf);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::row;
        fb.alignContent = juce::FlexBox::AlignContent::stretch;

        auto leftItem = juce::FlexItem(threshold)
            .withFlex(1.0f)
            .withMargin({ (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx });

        auto rightItem = juce::FlexItem(ceiling)
            .withFlex(1.0f)
            .withMargin({ (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kColGapPx });

        // Add a gap between the two groups
        rightItem.margin.left = (float)Layout::kColGapPx;

        fb.items.add(leftItem);
        fb.items.add(rightItem);

        fb.performLayout(bounds);
    }

private:
    StereoThreshold threshold;
    StereoCeiling   ceiling;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThrCeilColumn)
};

