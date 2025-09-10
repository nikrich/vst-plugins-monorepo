#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PluginProcessor.h"

class StereoCeiling : public juce::Component
{
public:
    explicit StereoCeiling(HungryGhostLimiterAudioProcessor::APVTS& apvts)
    {
        title.setText("OUT CEILING", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setInterceptsMouseClicks(false, false);
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
        title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        addAndMakeVisible(title);

        auto initSlider = [](juce::Slider& s)
        {
            s.setSliderStyle(juce::Slider::LinearBarVertical);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // value drawn by LNF at bottom
            s.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
            s.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(210, 210, 210));
        };
        initSlider(sliderL);
        initSlider(sliderR);
        addAndMakeVisible(sliderL);
        addAndMakeVisible(sliderR);

        labelL.setText("L", juce::dontSendNotification);
        labelR.setText("R", juce::dontSendNotification);
        labelL.setJustificationType(juce::Justification::centred);
        labelR.setJustificationType(juce::Justification::centred);
        labelL.setInterceptsMouseClicks(false, false);
        labelR.setInterceptsMouseClicks(false, false);
        labelL.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        labelR.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        addAndMakeVisible(labelL);
        addAndMakeVisible(labelR);

        addAndMakeVisible(linkButton);

        // APVTS attachments
        attL = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outCeilingL", sliderL);
        attR = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outCeilingR", sliderR);
        attLink = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "outCeilingLink", linkButton);

        // mirror behavior when linked
        auto mirror = [this](juce::Slider* src, juce::Slider* dst)
        {
            if (syncing || !linkButton.getToggleState()) return;
            syncing = true;
            dst->setValue(src->getValue(), juce::sendNotificationSync);
            syncing = false;
        };
        sliderL.onValueChange = [=] { mirror(&sliderL, &sliderR); };
        sliderR.onValueChange = [=] { mirror(&sliderR, &sliderL); };
    }

    void resized() override
    {
        auto a = getLocalBounds();
        title.setBounds(a.removeFromTop(20));

        auto area = a.reduced(6);
        int gap = 8;
        auto left = area.removeFromLeft(area.getWidth() / 2 - gap / 2);
        area.removeFromLeft(gap);
        auto right = area;

        labelL.setBounds(left.removeFromTop(16));
        sliderL.setBounds(left);

        labelR.setBounds(right.removeFromTop(16));
        sliderR.setBounds(right);

        auto bottom = getLocalBounds().removeFromBottom(20);
        linkButton.setBounds(bottom.removeFromRight(70));
    }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf)
    {
        sliderL.setLookAndFeel(lnf);
        sliderR.setLookAndFeel(lnf);
    }

private:
    juce::Label  title;
    juce::Slider sliderL, sliderR;
    juce::Label  labelL, labelR;
    juce::ToggleButton linkButton { "Link" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attL, attR;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attLink;

    bool syncing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoCeiling)
};

