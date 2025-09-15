#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Styling/Theme.h"
#include <BinaryData.h>
#include <Foundation/ResourceResolver.h>
#include <Foundation/Filmstrip.h>
#include "KitStripSlider.h"

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

            // Load UI kit 03 slider filmstrip (sl-final.png) once using ResourceResolver + Filmstrip
            static ui::foundation::Filmstrip film;
            if (!film.isValid())
            {
                auto img = ui::foundation::ResourceResolver::loadImageByNames({
                    "slfinal_png",
                    "sl-final.png",
                    "assets/ui/kit-03/slider/sl-final.png"
                });
                film = ui::foundation::Filmstrip(img, 128, ui::foundation::Filmstrip::Orientation::Vertical);
            }

            if (film.isValid())
            {
                // Choose source slider based on handle mode (Both uses left)
                const juce::Slider& s = (mode == Mode::Right ? parent.sliderR : parent.sliderL);
                const auto range = s.getRange();
                const double prop = range.getLength() > 0.0 ? (s.getValue() - range.getStart()) / range.getLength() : 0.0;
                // Preserve frame aspect inside handle bounds (contain)
                film.drawNormalized(g, r, (float)prop, true);
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
    using APVTS = juce::AudioProcessorValueTreeState;

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
        addAndMakeVisible(sliderM);
        addAndMakeVisible(sliderR);

        // Labels
        labelL.setText("L", juce::dontSendNotification);
        labelM.setText("M", juce::dontSendNotification);
        labelR.setText("R", juce::dontSendNotification);
        labelL.setJustificationType(juce::Justification::centred);
        labelM.setJustificationType(juce::Justification::centred);
        labelR.setJustificationType(juce::Justification::centred);
        labelL.setInterceptsMouseClicks(false, false);
        labelM.setInterceptsMouseClicks(false, false);
        labelR.setInterceptsMouseClicks(false, false);
        labelL.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        labelM.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        labelR.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        addAndMakeVisible(labelL);
        addAndMakeVisible(labelM);
        addAndMakeVisible(labelR);

        // Numeric value labels for L and R (shown below sliders, above letters)
        for (auto* v : { &valL, &valR })
        {
            v->setJustificationType(juce::Justification::centred);
            v->setInterceptsMouseClicks(false, false);
            v->setColour(juce::Label::textColourId, Style::theme().text);
            v->setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
            addAndMakeVisible(*v);
        }

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

        // Update numeric value labels when sliders move
        sliderL.onValueChange = [this]{ updateValueLabels(); };
        sliderR.onValueChange = [this]{ updateValueLabels(); };
        // Linked functionality disabled for now (link button hidden)
        linkButton.setVisible(false);

        updateValueLabels();

        // Middle filmstrip slider acts as a master: dragging it updates L and R
        // Match the range to L (both L and R share the same parameter range)
        {
            auto r = sliderL.getRange();
            sliderM.getSlider().setRange(r, sliderL.getInterval());
            sliderM.getSlider().setSkewFactor(sliderL.getSkewFactor());
            sliderM.getSlider().setValue((sliderL.getValue() + sliderR.getValue()) * 0.5, juce::dontSendNotification);
        }
        sliderM.getSlider().onValueChange = [this]
        {
            const auto v = sliderM.getSlider().getValue();
            sliderL.setValue(v, juce::sendNotificationSync);
            sliderR.setValue(v, juce::sendNotificationSync);
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
        // Enforce identical internal layout regardless of column width
        // Use fixed internal widths so all columns (Input/Threshold/Ceiling) look identical
        constexpr int idealSliderW = 46;                   // target width for each slider
        constexpr int gapX         = Layout::kBarGapPx;    // horizontal gap between columns
        constexpr int outerPadX    = Layout::kCellMarginPx;// side padding inside this component

        auto bounds = getLocalBounds();

        // Compute middle slider width that fits the available width, capped at the ideal
        const int avail = juce::jmax(0, bounds.getWidth() - outerPadX * 2 - gapX * 2);
        const int midW = juce::jmax(24, juce::jmin(idealSliderW, avail / 3));
        // Make side bars slightly narrower than the middle to get the requested look
        const int sideW = juce::jmax(18, midW - 8);

        const int contentW = outerPadX * 2 + sideW + gapX + midW + gapX + sideW;
        auto content = bounds.withWidth(contentW)
                              .withX(bounds.getX() + (bounds.getWidth() - contentW) / 2);
        // Rows
        auto r = content;
        auto rowTitle   = r.removeFromTop(Layout::kTitleRowHeightPx);
        r.removeFromTop(Layout::kRowGapPx);
        auto rowSliders = r.removeFromTop(Layout::kLargeSliderRowHeightPx);
        r.removeFromTop(0); // no extra gap before labels
        auto rowLabels  = r.removeFromTop(Layout::kChannelLabelRowHeightPx);

        // Title spans full content width
        title.setBounds(rowTitle.reduced(outerPadX));

        // Columns
        auto slidersInner = rowSliders.reduced(outerPadX, Layout::kCellMarginPx);
        auto labelsInner  = rowLabels.reduced(outerPadX, 0);

        auto c1 = slidersInner.removeFromLeft(sideW); slidersInner.removeFromLeft(gapX);
        auto c2 = slidersInner.removeFromLeft(midW);  slidersInner.removeFromLeft(gapX);
        auto c3 = slidersInner.removeFromLeft(sideW);

        sliderL.setBounds(c1);
        static_cast<juce::Component&>(sliderM).setBounds(c2);
        sliderR.setBounds(c3);

        // Split the labels area into two rows: values (top) and channel letters (bottom)
        const int valueRowH  = 16;
        const int letterRowH = 16;
        const int midGap     = 4; // small gap between value and letter rows
        auto valueRow  = labelsInner.removeFromTop(valueRowH);
        labelsInner.removeFromTop(midGap);
        auto letterRow = labelsInner.removeFromTop(letterRowH);

        // Value row: L and R only (centre under side bars)
        auto v1 = valueRow.removeFromLeft(sideW); valueRow.removeFromLeft(gapX);
        valueRow.removeFromLeft(midW);            valueRow.removeFromLeft(gapX); // skip middle
        auto v3 = valueRow.removeFromLeft(sideW);
        const int expand = 8; // allow a little extra width to avoid ellipses
        valL.setBounds(v1.expanded(expand, 0));
        valR.setBounds(v3.expanded(expand, 0));

        // Letter row: L / M / R
        auto l1 = letterRow.removeFromLeft(sideW); letterRow.removeFromLeft(gapX);
        auto l2 = letterRow.removeFromLeft(midW);  letterRow.removeFromLeft(gapX);
        auto l3 = letterRow.removeFromLeft(sideW);
        labelL.setBounds(l1);
        labelM.setBounds(l2);
        labelR.setBounds(l3);

        // Update drag track and handles (handles hidden for now)
        dragTrack = sliderL.getBounds().withRight(sliderR.getRight());
        dragTrack = dragTrack.reduced(juce::jmax(sliderL.getWidth() / 4, 4), 4);
        handleBoth.setVisible(false);
        handleL.setVisible(false);
        handleR.setVisible(false);
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
    void updateValueLabels()
    {
        auto fmt = [](juce::Slider& s){
            const double v = s.getValue();
            // Use 2 decimals for |v| < 10, otherwise 1 decimal to keep text short
            return std::abs(v) < 10.0 ? juce::String(v, 2) : juce::String(v, 1);
        };
        valL.setText(fmt(sliderL), juce::dontSendNotification);
        valR.setText(fmt(sliderR), juce::dontSendNotification);
        // Avoid ellipsis by allowing slight horizontal scaling if needed
        valL.setMinimumHorizontalScale(0.7f);
        valR.setMinimumHorizontalScale(0.7f);
    }

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
        // Handle size relative to slider width and filmstrip aspect ratio (height may differ from width)
        static float stripAspect = 0.0f; // height / width per frame
        if (stripAspect <= 0.0f)
        {
            auto img = ui::foundation::ResourceResolver::loadImageByNames({
                "slfinal_png",
                "sl-final.png",
                "assets/ui/kit-03/slider/sl-final.png"
            });
            if (img.isValid())
            {
                const int w = img.getWidth();
                const int h = img.getHeight();
                bool vertical = (h > w);
                int frameW = vertical ? w : h;
                int frames = vertical && w > 0 && h % w == 0 ? h / w : (!vertical && h > 0 && w % h == 0 ? w / h : 128);
                int frameH = vertical ? (h / juce::jmax(1, frames)) : h;
                stripAspect = (frameW > 0 && frameH > 0) ? (float)frameH / (float)frameW : 1.0f;
            }
            else stripAspect = 1.0f;
        }

        const int baseW = sliderL.getWidth() * 2 + 8;
        const int hw = juce::jlimit(20, 96, baseW);
        const int hh = (int)juce::jlimit(20.0f, 160.0f, (float)hw * (stripAspect > 0.0f ? stripAspect : 1.0f));

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

    // UI elements
    juce::Label valL, valR; // numeric value labels under side bars

    // live meter state (normalized 0..1; 1 = 0 dBFS)
    float meterL { 0.0f };
    float meterR { 0.0f };

    juce::Label  title;
    juce::Slider     sliderL, sliderR;
    KitStripSlider   sliderM;
    juce::Label      labelL, labelM, labelR;
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

