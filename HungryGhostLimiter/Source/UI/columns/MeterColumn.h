#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Layout.h"
#include "../Meter.h"

// A column to show a label and the attenuation meter under it.
class MeterColumn : public juce::Component {
public:
    MeterColumn()
        : meter("ATTENUATION")
    {
        label.setText("ATTENUATION", juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(label);
        addAndMakeVisible(meter);

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
        g.templateRows = {
            Track(juce::Grid::Px(Layout::kMeterLabelHeightPx)),
            Track(juce::Grid::Px(Layout::kMeterHeightPx))
        };
        g.rowGap = juce::Grid::Px(Layout::kRowGapPx);
        g.items = {
            juce::GridItem(label).withMargin(Layout::kCellMarginPx),
            juce::GridItem(meter).withMargin(Layout::kCellMarginPx)
        };
        g.performLayout(r);
    }

private:
    juce::Label label;
    AttenMeter  meter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterColumn)
};

