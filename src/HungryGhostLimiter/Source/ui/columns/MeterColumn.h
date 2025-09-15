#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Layout.h"
#include <Controls/VerticalMeter.h>
#include <Foundation/Typography.h>

// A column to show a label and the attenuation meter under it.
class MeterColumn : public juce::Component {
public:
    MeterColumn()
        : meter()
    {
        // Title styling to match other columns
        label.setText("ATTEN", juce::dontSendNotification);
        // Attenuation should fill top-down (more reduction -> more fill)
        meter.setTopDown(true);
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);
        // Standardize typography via Typography helper
        ui::foundation::Typography::apply(label,
                                          ui::foundation::Typography::Style::Title,
                                          juce::Colours::white.withAlpha(0.95f),
                                          juce::Justification::centred);
        addAndMakeVisible(label);

        addAndMakeVisible(meter);
        addAndMakeVisible(spacer); // invisible spacer to align baseline with other columns

        // default smoothing, can be overridden from the editor
        meter.setSmoothing(30.0f, 180.0f);
    }

    void setDb(float db) { meter.setDb(db); }
    void setSmoothing(float attackMs, float releaseMs) { meter.setSmoothing(attackMs, releaseMs); }

    void resized() override
    {
        auto r = getLocalBounds();

        juce::Grid g;
        using Track = juce::Grid::TrackInfo;
        g.templateColumns = { Track(juce::Grid::Fr(1)) };
        // Match other columns: Title, main content height, bottom labels row (even if empty)
        g.templateRows = {
            Track(juce::Grid::Px(Layout::kTitleRowHeightPx)),
            Track(juce::Grid::Px(Layout::kLargeSliderRowHeightPx)),
            Track(juce::Grid::Px(Layout::kChannelLabelRowHeightPx))
        };
        g.rowGap = juce::Grid::Px(Layout::kRowGapPx);
        g.items = {
            juce::GridItem(label).withMargin(Layout::kCellMarginPx).withArea(1,1),
            juce::GridItem(meter).withMargin(Layout::kCellMarginPx).withArea(2,1),
            juce::GridItem(spacer).withMargin(Layout::kCellMarginPx).withArea(3,1)
        };
        g.performLayout(r);
    }

private:
    juce::Label label;
    ui::controls::VerticalMeter  meter;
    juce::Component spacer; // empty alignment spacer matching labels row height

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterColumn)
};

