#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Controls.h"
#include "../ReleaseSection.h"
#include "../Layout.h"
#include "../../PluginProcessor.h"

// A column that vertically arranges:
//  1) Release (rotary + Auto toggle)
//  2) LookAhead (vertical bar)
//  3) Two toggles (SC HPF, SAFETY)
class ControlsColumn : public juce::Component {
public:
    ControlsColumn(HungryGhostLimiterAudioProcessor::APVTS& apvts,
                   juce::LookAndFeel* donutKnobLNF,
                   juce::LookAndFeel* pillVSliderLNF,
                   juce::LookAndFeel* neonToggleLNF)
        : apvts(apvts),
          releaseSec(apvts),
          lookAhead("LOOK-AHEAD")
    {
        // Look & Feel
        if (donutKnobLNF) releaseSec.setKnobLookAndFeel(donutKnobLNF);
        if (pillVSliderLNF) lookAhead.setSliderLookAndFeel(pillVSliderLNF);
        if (neonToggleLNF) {
            scHpfToggle.setLookAndFeel(neonToggleLNF);
            safetyToggle.setLookAndFeel(neonToggleLNF);
        }

        // Basic rows
        addAndMakeVisible(releaseSec);
        addAndMakeVisible(lookAhead);

        // Toggles row
        scHpfToggle.setButtonText("SC HPF");
        safetyToggle.setButtonText("SAFETY");
        addAndMakeVisible(toggleRow);
        toggleRow.addAndMakeVisible(scHpfToggle);
        toggleRow.addAndMakeVisible(safetyToggle);

        // Attachments
        laAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "lookAheadMs", lookAhead.slider);
        hpfAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "scHpf", scHpfToggle);
        safAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "safetyClip", safetyToggle);
    }

    ~ControlsColumn() override = default;

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Layout toggles inside toggleRow: two equal halves with a small gap
        auto layoutToggleRow = [&]()
        {
            auto tr = toggleRow.getLocalBounds();
            const int gap = Layout::kCellMarginPx;
            auto left = tr.removeFromLeft(tr.getWidth() / 2 - gap / 2);
            tr.removeFromLeft(gap);
            auto right = tr;
            scHpfToggle.setBounds(left.reduced(2));
            safetyToggle.setBounds(right.reduced(2));
        };

        juce::FlexBox fb;
        fb.flexDirection = juce::FlexBox::Direction::column;
        fb.alignContent = juce::FlexBox::AlignContent::stretch;
        fb.items.clear();

        auto row = [&](juce::Component& c, int h)
        {
            fb.items.add(juce::FlexItem(c)
                .withHeight((float)h)
                .withMargin({ (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kRowGapPx, (float)Layout::kCellMarginPx }));
        };

        row(releaseSec, Layout::kReleaseRowHeightPx);
        row(lookAhead,  Layout::kLookAheadRowHeightPx);
        row(toggleRow,  Layout::kTogglesRowHeightPx);
        // spacer to push content to top
        fb.items.add(juce::FlexItem().withFlex(1.0f));

        fb.performLayout(bounds);

        layoutToggleRow();
    }

    // Expose components if needed elsewhere
    ReleaseSection& getReleaseSection() { return releaseSec; }
    LabelledVSlider& getLookAhead() { return lookAhead; }
    juce::ToggleButton& getScHpfToggle() { return scHpfToggle; }
    juce::ToggleButton& getSafetyToggle() { return safetyToggle; }

private:
    HungryGhostLimiterAudioProcessor::APVTS& apvts;

    ReleaseSection releaseSec;
    LabelledVSlider lookAhead;

    juce::Component toggleRow;
    juce::ToggleButton scHpfToggle{ "SC HPF" };
    juce::ToggleButton safetyToggle{ "SAFETY" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> laAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpfAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> safAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlsColumn)
};

