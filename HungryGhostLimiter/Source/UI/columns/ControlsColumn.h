#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Controls.h"
#include "../Layout.h"
#include "../../PluginProcessor.h"

// A column that vertically arranges:
//  1) Release (rotary)
//  2) LookAhead (vertical bar)
//  3) Two toggles (SC HPF, SAFETY)
class ControlsColumn : public juce::Component {
public:
    ControlsColumn(HungryGhostLimiterAudioProcessor::APVTS& apvts,
                   juce::LookAndFeel* donutKnobLNF,
                   juce::LookAndFeel* pillVSliderLNF,
                   juce::LookAndFeel* neonToggleLNF)
        : apvts(apvts),
          release("RELEASE"),
          lookAhead("LOOK-AHEAD")
    {
        // Look & Feel
        if (donutKnobLNF) release.setSliderLookAndFeel(donutKnobLNF);
        if (pillVSliderLNF) lookAhead.setSliderLookAndFeel(pillVSliderLNF);
        if (neonToggleLNF) {
            scHpfToggle.setLookAndFeel(neonToggleLNF);
            safetyToggle.setLookAndFeel(neonToggleLNF);
        }

        addAndMakeVisible(release);
        addAndMakeVisible(lookAhead);

        scHpfToggle.setButtonText("SC HPF");
        safetyToggle.setButtonText("SAFETY");
        addAndMakeVisible(toggleRow);
        toggleRow.addAndMakeVisible(scHpfToggle);
        toggleRow.addAndMakeVisible(safetyToggle);

        // Attachments
        reAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", release.slider);
        laAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lookAheadMs", lookAhead.slider);
        hpfAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "scHpf", scHpfToggle);
        safAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "safetyClip", safetyToggle);
    }

    ~ControlsColumn() override = default;

    void resized() override
    {
        auto r = getLocalBounds();

        juce::Grid g;
        using Track = juce::Grid::TrackInfo;
        g.templateColumns = { Track(juce::Grid::Fr(1)) };
        g.templateRows = {
            Track(juce::Grid::Px(Layout::kReleaseRowHeightPx)),
            Track(juce::Grid::Px(Layout::kLookAheadRowHeightPx)),
            Track(juce::Grid::Px(Layout::kTogglesRowHeightPx))
        };
        g.rowGap = juce::Grid::Px(Layout::kRowGapPx);
        g.autoFlow = juce::Grid::AutoFlow::row;
        g.items = {
            juce::GridItem(release).withMargin(Layout::kCellMarginPx),
            juce::GridItem(lookAhead).withMargin(Layout::kCellMarginPx),
            juce::GridItem(toggleRow).withMargin(Layout::kCellMarginPx)
        };
        g.performLayout(r);

        // Layout toggles inside toggleRow: two equal halves with a small gap
        auto tr = toggleRow.getLocalBounds();
        const int gap = Layout::kCellMarginPx;
        auto left = tr.removeFromLeft(tr.getWidth() / 2 - gap / 2);
        tr.removeFromLeft(gap);
        auto right = tr;
        scHpfToggle.setBounds(left.reduced(2));
        safetyToggle.setBounds(right.reduced(2));
    }

    // Expose components if needed elsewhere
    LabelledVSlider& getRelease() { return release; }
    LabelledVSlider& getLookAhead() { return lookAhead; }
    juce::ToggleButton& getScHpfToggle() { return scHpfToggle; }
    juce::ToggleButton& getSafetyToggle() { return safetyToggle; }

private:
    HungryGhostLimiterAudioProcessor::APVTS& apvts;

    LabelledVSlider release;
    LabelledVSlider lookAhead;

    juce::Component toggleRow;
    juce::ToggleButton scHpfToggle{ "SC HPF" };
    juce::ToggleButton safetyToggle{ "SAFETY" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> laAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpfAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> safAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsColumn)
};

