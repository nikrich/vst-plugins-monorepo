#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"
#include "../PluginProcessor.h"

// Reusable stereo linked vertical bars (L/R) with a title and Link toggle.
// Parameter IDs are provided at construction.
class StereoLinkedBars : public juce::Component {
public:
    using APVTS = HungryGhostLimiterAudioProcessor::APVTS;

    StereoLinkedBars(APVTS& apvts,
                     juce::String titleText,
                     juce::String paramIdL,
                     juce::String paramIdR,
                     juce::String linkParamId)
        : apvts(apvts),
          title({}, std::move(titleText)),
          paramLId(std::move(paramIdL)),
          paramRId(std::move(paramIdR)),
          linkId(std::move(linkParamId))
    {
        // Title
        title.setJustificationType(juce::Justification::centred);
        title.setInterceptsMouseClicks(false, false);
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
        title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        addAndMakeVisible(title);

        // Sliders
        auto initSlider = [](juce::Slider& s) {
            s.setSliderStyle(juce::Slider::LinearBarVertical);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            s.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
            s.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(210, 210, 210));
        };
        initSlider(sliderL);
        initSlider(sliderR);
        addAndMakeVisible(sliderL);
        addAndMakeVisible(sliderR);

        // Labels
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

        // Link toggle
        addAndMakeVisible(linkButton);

        // Attachments
        attL = std::make_unique<APVTS::SliderAttachment>(apvts, paramLId, sliderL);
        attR = std::make_unique<APVTS::SliderAttachment>(apvts, paramRId, sliderR);
        attLink = std::make_unique<APVTS::ButtonAttachment>(apvts, linkId, linkButton);

        // Link mirroring
        auto mirror = [this](juce::Slider* src, juce::Slider* dst) {
            if (syncing || !linkButton.getToggleState()) return;
            syncing = true;
            dst->setValue(src->getValue(), juce::sendNotificationSync);
            syncing = false;
        };
        sliderL.onValueChange = [=] { mirror(&sliderL, &sliderR); };
        sliderR.onValueChange = [=] { mirror(&sliderR, &sliderL); };
        linkButton.onClick = [this] {
            if (linkButton.getToggleState()) {
                syncing = true;
                sliderR.setValue(sliderL.getValue(), juce::sendNotificationSync);
                syncing = false;
            }
        };
    }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf) {
        sliderL.setLookAndFeel(lnf);
        sliderR.setLookAndFeel(lnf);
    }

    // Update the text below sliders (e.g., real-time level readouts)
    void setBottomTexts(const juce::String& left, const juce::String& right) {
        labelL.setText(left, juce::dontSendNotification);
        labelR.setText(right, juce::dontSendNotification);
    }

    void resized() override {
        // 2 columns (L/R), 4 rows: Title, Labels, Sliders, Link
        juce::Grid g; using Track = juce::Grid::TrackInfo;
        g.templateColumns = { Track(juce::Grid::Fr(1)), Track(juce::Grid::Fr(1)) };
        g.templateRows = {
            Track(juce::Grid::Px(Layout::kTitleRowHeightPx)),
            Track(juce::Grid::Px(Layout::kLargeSliderRowHeightPx)),
            Track(juce::Grid::Px(Layout::kChannelLabelRowHeightPx)),
            Track(juce::Grid::Px(Layout::kLinkRowHeightPx))
        };
        g.rowGap    = juce::Grid::Px(Layout::kRowGapPx);
        g.columnGap = juce::Grid::Px(0); // eliminate inter-column gap; control spacing via per-item margins
        g.justifyItems = juce::Grid::JustifyItems::stretch;
        g.alignItems   = juce::Grid::AlignItems::stretch;

        auto titleItem = juce::GridItem(title).withMargin(Layout::kCellMarginPx)
                                                .withArea(1, 1, 2, 3);

        // Sliders: minimal center gap using asymmetric margins
        auto sl = juce::GridItem(sliderL)
                        .withMargin(juce::GridItem::Margin(
                            (float)Layout::kCellMarginPx, /*right*/ 2.0f,
                            (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx))
                        .withArea(2, 1);
        auto sr = juce::GridItem(sliderR)
                        .withMargin(juce::GridItem::Margin(
                            (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx,
                            (float)Layout::kCellMarginPx, /*left*/ 2.0f))
                        .withArea(2, 2);

        // Labels below sliders: small top + center gap
        auto ll = juce::GridItem(labelL)
                        .withMargin(juce::GridItem::Margin(
                            2.0f, /*right*/ 2.0f, 0.0f, (float)Layout::kCellMarginPx))
                        .withArea(3, 1);
        auto lr = juce::GridItem(labelR)
                        .withMargin(juce::GridItem::Margin(
                            2.0f, (float)Layout::kCellMarginPx, 0.0f, /*left*/ 2.0f))
                        .withArea(3, 2);

        auto linkItem = juce::GridItem(linkButton).withMargin(Layout::kCellMarginPx)
                                                  .withArea(4, 2)
                                                  .withWidth(70.0f)
                                                  .withJustifySelf(juce::GridItem::JustifySelf::end)
                                                  .withAlignSelf(juce::GridItem::AlignSelf::center);

        g.items = { titleItem, sl, sr, ll, lr, linkItem };
        g.performLayout(getLocalBounds());
    }

    // Live meter overlay (normalized 0..1, where 1 = 0 dBFS).
    void setMeterDbFs(float leftDb, float rightDb)
    {
        auto norm = [](float db){ return juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f); };
        meterL = norm(leftDb);
        meterR = norm(rightDb);
        repaint();
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        // Draw translucent fills inside each slider's bounds to indicate live level
        auto drawIn = [&](const juce::Rectangle<int>& r, float norm)
        {
            if (r.isEmpty() || norm <= 0.001f) return;
            auto rf = r.toFloat().reduced(2.0f);
            const float radius = rf.getWidth() * 0.5f;
            const float h = rf.getHeight() * juce::jlimit(0.0f, 1.0f, norm);
            juce::Rectangle<float> fill = rf.withY(rf.getBottom() - h).withHeight(h);
            juce::Colour c = juce::Colours::aqua.withAlpha(0.22f);
            g.setColour(c);
            g.fillRoundedRectangle(fill, radius);
        };

        drawIn(sliderL.getBounds(), meterL);
        drawIn(sliderR.getBounds(), meterR);
    }

private:
    APVTS& apvts;

    // live meter state (normalized 0..1; 1 = 0 dBFS)
    float meterL { 0.0f };
    float meterR { 0.0f };

    juce::Label  title;
    juce::Slider sliderL, sliderR;
    juce::Label  labelL, labelR;
    juce::ToggleButton linkButton { "Link" };

    std::unique_ptr<APVTS::SliderAttachment>  attL, attR;
    std::unique_ptr<APVTS::ButtonAttachment>  attLink;

    juce::String paramLId, paramRId, linkId;
    bool syncing { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoLinkedBars)
};

