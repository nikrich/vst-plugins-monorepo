#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PluginProcessor.h"
#include "Layout.h"

class ReleaseSection : public juce::Component {
public:
    explicit ReleaseSection(HungryGhostLimiterAudioProcessor::APVTS& apvts)
    {
        title.setText("RELEASE", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setInterceptsMouseClicks(false, false);
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
        title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        addAndMakeVisible(title);

        release.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        release.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 20);
        release.setTextValueSuffix(" ms");
        addAndMakeVisible(release);

        // Auto Release header + toggle
        autoHeader.setText("AUTO RELEASE", juce::dontSendNotification);
        autoHeader.setJustificationType(juce::Justification::centred);
        autoHeader.setInterceptsMouseClicks(false, false);
        autoHeader.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.90f));
        autoHeader.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
        addAndMakeVisible(autoHeader);

        autoBtn.setButtonText("Auto Release");
        addAndMakeVisible(autoBtn);

        attRel  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", release);
        attAuto = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "autoRelease", autoBtn);

        // Ensure knob enablement follows auto toggle state (user and programmatic changes)
        auto updateEnabled = [this]{
            const bool enabled = !autoBtn.getToggleState();
            release.setEnabled(enabled);
            repaint();
        };
        autoBtn.onClick = updateEnabled;
        autoBtn.onStateChange = updateEnabled; // covers programmatic changes
        updateEnabled();
    }

    void setKnobLookAndFeel(juce::LookAndFeel* lnf) { release.setLookAndFeel(lnf); }
    void setAutoToggleLookAndFeel(juce::LookAndFeel* lnf) { autoBtn.setLookAndFeel(lnf); }

    void resized() override
    {
        // Title at top, knob in middle, auto header + big toggle at bottom
        auto r = getLocalBounds();
        auto titleArea = r.removeFromTop(Layout::kTitleRowHeightPx);
        title.setBounds(titleArea.reduced(2));

        // Reserve space for auto header + toggle at the bottom
        const int autoHeaderH = 20;
        const int autoToggleH = 44; // slightly smaller toggle height
        auto autoBlock = r.removeFromBottom(autoHeaderH + autoToggleH);
        auto autoToggleArea = autoBlock.removeFromBottom(autoToggleH);
        auto autoHeaderArea = autoBlock;

        // Add spacing between the auto toggle block and the release knob (increase gap)
        r.removeFromBottom(Layout::kRowGapPx * 2);

        // Knob uses remaining space
        auto knobArea = r;
        release.setBounds(knobArea.reduced(2));

        // Place header + toggle
        autoHeader.setBounds(autoHeaderArea.reduced(2));
        autoBtn.setBounds(autoToggleArea.reduced(2));
    }

private:
    juce::Label        title;
    juce::Slider       release;
    juce::Label        autoHeader;
    juce::ToggleButton autoBtn { "Auto Release" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attAuto;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReleaseSection)
};

