#include "Threshold.h"
#include "Layout.h"

StereoThreshold::StereoThreshold(HungryGhostLimiterAudioProcessor::APVTS& apvts)
{
    title.setJustificationType(juce::Justification::centred);
    title.setInterceptsMouseClicks(false, false);
    title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
    title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    addAndMakeVisible(title);

    auto initSlider = [](juce::Slider& s)
        {
            s.setSliderStyle(juce::Slider::LinearBarVertical);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); // draw value via LNF at bottom
            s.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
            s.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(210, 210, 210));
        };
        
    initSlider(sliderL);
    initSlider(sliderR);

    addAndMakeVisible(sliderL);
    addAndMakeVisible(sliderR);

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
    attL = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "thresholdL", sliderL);
    attR = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "thresholdR", sliderR);
    attLink = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "thresholdLink", linkButton);

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

    // When enabling link, immediately sync R to L so UI reflects linkage right away
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

void StereoThreshold::resized()
{
    // Grid with 2 columns (L/R) and 4 rows: Title, Labels, Sliders, Link
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
    g.columnGap = juce::Grid::Px(Layout::kBarGapPx);

    auto titleItem = juce::GridItem(title).withMargin(Layout::kCellMarginPx);
    titleItem.column = { 1, 2 }; // span both columns

    auto linkItem = juce::GridItem(linkButton).withMargin(Layout::kCellMarginPx);
    linkItem.column = { 1, 2 }; // span both; align to the right
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

void StereoThreshold::paintOverChildren(juce::Graphics&) {}

void StereoThreshold::setSliderLookAndFeel(juce::LookAndFeel* lnf)
{
    sliderL.setLookAndFeel(lnf);
    sliderR.setLookAndFeel(lnf);
}
