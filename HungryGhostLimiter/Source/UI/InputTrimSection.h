#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PluginProcessor.h"
#include "Layout.h"

class InputTrimSection : public juce::Component {
public:
    explicit InputTrimSection(HungryGhostLimiterAudioProcessor::APVTS& apvts)
    {
        title.setText("INPUT", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setInterceptsMouseClicks(false, false);
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
        title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        addAndMakeVisible(title);

        auto initSlider = [](juce::Slider& s)
        {
            s.setSliderStyle(juce::Slider::LinearBarVertical);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setRange(-24.0, 24.0, 0.01);
            s.setDoubleClickReturnValue(true, 0.0);
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

        linkButton.setButtonText("Link");
        addAndMakeVisible(linkButton);

        // APVTS attachments
        attL = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "inTrimL", sliderL);
        attR = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "inTrimR", sliderR);
        attLink = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "inTrimLink", linkButton);

        // mirror when linked
        auto mirror = [this](juce::Slider* src, juce::Slider* dst)
        {
            if (syncing || !linkButton.getToggleState()) return;
            syncing = true;
            dst->setValue(src->getValue(), juce::sendNotificationSync);
            syncing = false;
        };
        sliderL.onValueChange = [=] { mirror(&sliderL, &sliderR); };
        sliderR.onValueChange = [=] { mirror(&sliderR, &sliderL); };

        linkButton.onClick = [this]
        {
            if (linkButton.getToggleState())
            {
                syncing = true;
                sliderR.setValue(sliderL.getValue(), juce::sendNotificationSync);
                syncing = false;
            }
        };
    }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf)
    {
        sliderL.setLookAndFeel(lnf);
        sliderR.setLookAndFeel(lnf);
    }

    void resized() override
    {
        juce::Grid g;
        using Track = juce::Grid::TrackInfo;
        g.templateColumns = { Track(juce::Grid::Fr(1)), Track(juce::Grid::Fr(1)) };
        g.templateRows = {
            Track(juce::Grid::Px(Layout::kTitleRowHeightPx)),
            Track(juce::Grid::Px(Layout::kChannelLabelRowHeightPx)),
            Track(juce::Grid::Px(Layout::kLargeSliderRowHeightPx)),
            Track(juce::Grid::Px(Layout::kLinkRowHeightPx))
        };
        g.rowGap = juce::Grid::Px(Layout::kRowGapPx);
        g.columnGap = juce::Grid::Px(Layout::kColGapPx);

        auto titleItem = juce::GridItem(title).withMargin(Layout::kCellMarginPx);
        titleItem.column = { 1, 2 };

        auto linkItem = juce::GridItem(linkButton).withMargin(Layout::kCellMarginPx);
        linkItem.column = { 1, 2 };
        linkItem.justifySelf = juce::GridItem::JustifySelf::end;

        g.items = {
            titleItem,
            juce::GridItem(labelL).withMargin(Layout::kCellMarginPx),
            juce::GridItem(labelR).withMargin(Layout::kCellMarginPx),
            juce::GridItem(sliderL).withMargin(Layout::kCellMarginPx),
            juce::GridItem(sliderR).withMargin(Layout::kCellMarginPx),
            linkItem
        };

        g.performLayout(getLocalBounds());
    }

private:
    juce::Label  title;
    juce::Slider sliderL, sliderR;
    juce::Label  labelL, labelR;
    juce::ToggleButton linkButton { "Link" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attL, attR;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attLink;

    bool syncing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InputTrimSection)
};

