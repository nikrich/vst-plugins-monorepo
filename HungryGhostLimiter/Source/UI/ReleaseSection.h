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

        autoBtn.setButtonText("Auto Release");
        addAndMakeVisible(autoBtn);

        attRel  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "release", release);
        attAuto = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "autoRelease", autoBtn);
    }

    void setKnobLookAndFeel(juce::LookAndFeel* lnf) { release.setLookAndFeel(lnf); }
    void setAutoToggleLookAndFeel(juce::LookAndFeel* lnf) { autoBtn.setLookAndFeel(lnf); }

    void resized() override
    {
        // Title at top, knob in middle, auto toggle below
        auto r = getLocalBounds();
        auto titleArea = r.removeFromTop(Layout::kTitleRowHeightPx);
        title.setBounds(titleArea.reduced(4));

        auto knobArea = r.removeFromTop(Layout::kReleaseRowHeightPx - Layout::kTitleRowHeightPx - 24);
        release.setBounds(knobArea.reduced(6));

        auto autoArea = r.removeFromTop(24);
        autoBtn.setBounds(autoArea.reduced(4));
    }

private:
    juce::Label        title;
    juce::Slider       release;
    juce::ToggleButton autoBtn { "Auto Release" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attRel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attAuto;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReleaseSection)
};

