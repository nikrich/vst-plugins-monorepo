#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"
#include "../PluginProcessor.h"
#include "../styling/Theme.h"
#include <BinaryData.h>

// Reusable stereo linked vertical bars (L/R) with a title and Link toggle.
// Parameter IDs are provided at construction.
class StereoLinkedBars : public juce::Component {
public:
    // Small square handle that can be dragged vertically
    class DraggableHandle : public juce::Component {
    public:
        enum class Mode { Left, Right, Both };
        DraggableHandle(StereoLinkedBars& owner, Mode m) : parent(owner), mode(m) { setRepaintsOnMouseActivity(true); setMouseCursor(juce::MouseCursor::UpDownResizeCursor); }
        void paint(juce::Graphics& g) override {
            auto r = getLocalBounds().toFloat();
            // load slider knob image once
            static juce::Image knobImg;
            if (!knobImg.isValid())
            {
                auto tryNamed = [](const char* name) -> juce::Image
                {
                    int sz = 0; if (const void* data = BinaryData::getNamedResource(name, sz))
                        return juce::ImageFileFormat::loadFrom(data, (size_t)sz);
                    return {};
                };
                knobImg = tryNamed("sliderknob_png");
                if (!knobImg.isValid()) knobImg = tryNamed("slider_knob_png");
                if (!knobImg.isValid()) knobImg = tryNamed("slider-knob.png");
            }

            if (knobImg.isValid())
            {
                g.drawImageWithin(knobImg,
                                  (int)r.getX(), (int)r.getY(), (int)r.getWidth(), (int)r.getHeight(),
                                  juce::RectanglePlacement::centred);
            }
            else
            {
                // fallback outline if image missing
                g.setColour(juce::Colours::black.withAlpha(isMouseOver() ? 0.9f : 0.8f));
                g.drawRoundedRectangle(r.reduced(1.0f), 4.0f, 3.0f);
            }
        }
        void mouseDrag(const juce::MouseEvent& e) override { parent.onHandleDrag(mode, (float) (getY() + e.getPosition().y)); }
        void mouseDown(const juce::MouseEvent& e) override { startY = (float)e.getPosition().y; }
    private:
        float startY { 0.0f };
        Mode getMode() const { return mode; }
    private:
        StereoLinkedBars& parent;
        Mode mode;
    };
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

        // Handles
        addChildComponent(handleBoth);
        addChildComponent(handleL);
        addChildComponent(handleR);

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
            updateHandlesVisibility();
            updateHandlePositions();
        };
    }

    void setSliderLookAndFeel(juce::LookAndFeel* lnf) {
        sliderL.setLookAndFeel(lnf);
        sliderR.setLookAndFeel(lnf);
    }

    void setLinkLookAndFeel(juce::LookAndFeel* lnf) {
        linkButton.setLookAndFeel(lnf);
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

        // Update drag track and handles
        dragTrack = sliderL.getBounds().withRight(sliderR.getRight());
        dragTrack = dragTrack.reduced(juce::jmax(sliderL.getWidth() / 4, 4), 4);
        updateHandlesVisibility();
        updateHandlePositions();
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
            const float radius = Style::theme().borderRadius;
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
    void updateHandlesVisibility()
    {
        const bool linked = linkButton.getToggleState();
        handleBoth.setVisible(linked);
        handleL.setVisible(!linked);
        handleR.setVisible(!linked);
    }

    float valueToY(const juce::Slider& s) const
    {
        auto r = dragTrack;
        auto range = s.getRange();
        const double prop = range.getLength() > 0.0 ? (s.getValue() - range.getStart()) / range.getLength() : 0.0;
        // top = 0 dB (prop 1), bottom = 0 (prop 0)
        const int yPix = r.getBottom() - (int)std::round(prop * r.getHeight());
        return (float) juce::jlimit(r.getY(), r.getBottom(), yPix);
    }

    double yToValue(const juce::Slider& s, float y) const
    {
        auto r = dragTrack;
        y = juce::jlimit((float)r.getY(), (float)r.getBottom(), y);
        auto range = s.getRange();
        const double prop = juce::jlimit(0.0, 1.0, (double)(r.getBottom() - y) / (double)r.getHeight());
        return range.getStart() + prop * range.getLength();
    }

    void updateHandlePositions()
    {
        // Square handle sized relative to slider column
        const int dia = juce::jlimit(20, 48, sliderL.getWidth() * 2 + 8);
        const int hw = dia, hh = dia;
        const int cx = dragTrack.getCentreX();
        auto yl = (int)std::round(valueToY(sliderL)) - hh / 2;
        auto yr = (int)std::round(valueToY(sliderR)) - hh / 2;
        handleBoth.setBounds(cx - hw / 2, yl, hw, hh);
        // individual handles centered over each slider column
        handleL.setBounds(sliderL.getX() + sliderL.getWidth()/2 - hw/2, yl, hw, hh);
        handleR.setBounds(sliderR.getX() + sliderR.getWidth()/2 - hw/2, yr, hw, hh);
        repaint();
    }

    void onHandleDrag(DraggableHandle::Mode mode, float y)
    {
        if (mode == DraggableHandle::Mode::Both)
        {
            auto v = yToValue(sliderL, y);
            sliderL.setValue(v, juce::sendNotificationSync);
            sliderR.setValue(v, juce::sendNotificationSync);
        }
        else if (mode == DraggableHandle::Mode::Left)
        {
            sliderL.setValue(yToValue(sliderL, y), juce::sendNotificationSync);
        }
        else
        {
            sliderR.setValue(yToValue(sliderR, y), juce::sendNotificationSync);
        }
        updateHandlePositions();
    }

    APVTS& apvts;

    // live meter state (normalized 0..1; 1 = 0 dBFS)
    float meterL { 0.0f };
    float meterR { 0.0f };

    juce::Label  title;
    juce::Slider sliderL, sliderR;
    juce::Label  labelL, labelR;
    juce::ToggleButton linkButton { "Link" };

    // Drag handles and track area
    juce::Rectangle<int> dragTrack;
    DraggableHandle handleBoth { *this, DraggableHandle::Mode::Both };
    DraggableHandle handleL    { *this, DraggableHandle::Mode::Left };
    DraggableHandle handleR    { *this, DraggableHandle::Mode::Right };

    friend class DraggableHandle;

    std::unique_ptr<APVTS::SliderAttachment>  attL, attR;
    std::unique_ptr<APVTS::ButtonAttachment>  attLink;

    juce::String paramLId, paramRId, linkId;
    bool syncing { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoLinkedBars)
};

