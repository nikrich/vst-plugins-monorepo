#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Layout.h"
#include "../OutputMeters.h"
#include "../../PluginProcessor.h"

// Output column: read-only live output meters (post-processor)
class OutputColumn : public juce::Component {
public:
    explicit OutputColumn(HungryGhostLimiterAudioProcessor::APVTS&)
    {
        addAndMakeVisible(meters);
    }

    void setLevelsDbFs(float lDb, float rDb) { meters.setLevelsDbFs(lDb, rDb); }
    void setSmoothing(float attackMs, float releaseMs) { meters.setSmoothing(attackMs, releaseMs); }
    void setSliderLookAndFeel(juce::LookAndFeel*) {}

    void resized() override { meters.setBounds(getLocalBounds()); }

private:
    OutputMeters meters;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputColumn)
};

