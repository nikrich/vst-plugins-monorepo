#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <BinaryData.h>
#include <Styling/Theme.h>
#include <Foundation/ResourceResolver.h>
#include <Foundation/Filmstrip.h>

// A reusable stereo linked vertical bars control with an optional middle filmstrip slider.
// It attaches to a juce::AudioProcessorValueTreeState for L, R, and Link parameters.
class StereoLinkedBars : public juce::Component {
public:
    using APVTS = juce::AudioProcessorValueTreeState;

    // Defaults for internal layout; independent from any external Layout.h
    struct Defaults {
        static constexpr int kBarGapPx                = 6;
        static constexpr int kCellMarginPx           = 4;
        static constexpr int kTitleRowHeightPx       = 28;
        static constexpr int kLargeSliderRowHeightPx = 252;
        static constexpr int kChannelLabelRowHeightPx= 36;
    };

    // A simple internal filmstrip slider that paints a filmstrip based on slider value
    class FilmBar : public juce::Component {
    public:
        FilmBar()
        {
            s.setSliderStyle(juce::Slider::LinearVertical);
            s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            addAndMakeVisible(s);
            s.onValueChange = [this]{ repaint(); };

            // Default asset name; caller may override via setFilmstrip
            auto img = ui::foundation::ResourceResolver::loadImageByNames({
                "slfinal_png",
                "sl-final.png",
                "assets/ui/kit-03/slider/sl-final.png"
            });
            setFilmstrip(img, 128, ui::foundation::Filmstrip::Orientation::Vertical);
        }

        void setFilmstrip(juce::Image img, int frames, ui::foundation::Filmstrip::Orientation orient)
        {
            film = ui::foundation::Filmstrip(std::move(img), frames, orient);
        }

        juce::Slider& getSlider() noexcept { return s; }

        void resized() override { s.setBounds(getLocalBounds()); }

        void paint(juce::Graphics& g) override
        {
            if (!film.isValid()) return;
            const auto range = s.getRange();
            const double prop = range.getLength() > 0.0 ? (s.getValue() - range.getStart()) / range.getLength() : 0.0;
            film.drawNormalized(g, getLocalBounds().toFloat(), (float)prop, true);
        }
    private:
        ui::foundation::Filmstrip film;
        juce::Slider s;
    };

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
        title.setColour(juce::Label::textColourId, ::Style::theme().text);
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
        for (auto* l : { &labelL, &labelM, &labelR })
        {
            l->setInterceptsMouseClicks(false, false);
            l->setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        }
        addAndMakeVisible(labelL);
        addAndMakeVisible(labelM);
        addAndMakeVisible(labelR);

        // Numeric value labels for L and R
        for (auto* v : { &valL, &valR })
        {
            v->setJustificationType(juce::Justification::centred);
            v->setInterceptsMouseClicks(false, false);
            v->setColour(juce::Label::textColourId, ::Style::theme().text);
            v->setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
            addAndMakeVisible(*v);
        }

        // Link toggle (hidden by default)
        addAndMakeVisible(linkButton);
        linkButton.setVisible(false);

        // Attachments
        attL = std::make_unique<APVTS::SliderAttachment>(apvts, paramLId, sliderL);
        attR = std::make_unique<APVTS::SliderAttachment>(apvts, paramRId, sliderR);
        attLink = std::make_unique<APVTS::ButtonAttachment>(apvts, linkId, linkButton);

        // Update numeric value labels when sliders move
        sliderL.onValueChange = [this]{ updateValueLabels(); };
        sliderR.onValueChange = [this]{ updateValueLabels(); };
        updateValueLabels();

        // Middle filmstrip slider acts as a master
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
    void setLinkLookAndFeel(juce::LookAndFeel* lnf) { linkButton.setLookAndFeel(lnf); }

    void setBottomTexts(const juce::String& left, const juce::String& right) {
        labelL.setText(left, juce::dontSendNotification);
        labelR.setText(right, juce::dontSendNotification);
    }

    void resized() override
    {
        constexpr int gapX      = Defaults::kBarGapPx;
        constexpr int outerPadX = Defaults::kCellMarginPx;

        auto bounds = getLocalBounds();
        // Compute middle/side widths
        const int avail = juce::jmax(0, bounds.getWidth() - outerPadX * 2 - gapX * 2);
        const int midW  = juce::jmax(24, juce::jmin(46, avail / 3));
        const int sideW = juce::jmax(18, midW - 8);
        const int contentW = outerPadX * 2 + sideW + gapX + midW + gapX + sideW;
        auto content = bounds.withWidth(contentW)
                              .withX(bounds.getX() + (bounds.getWidth() - contentW) / 2);

        auto r = content;
        auto rowTitle   = r.removeFromTop(Defaults::kTitleRowHeightPx);
        r.removeFromTop(Defaults::kCellMarginPx * 2);
        auto rowSliders = r.removeFromTop(Defaults::kLargeSliderRowHeightPx);
        auto rowLabels  = r.removeFromTop(Defaults::kChannelLabelRowHeightPx);

        title.setBounds(rowTitle.reduced(outerPadX));

        auto slidersInner = rowSliders.reduced(outerPadX, Defaults::kCellMarginPx);
        auto labelsInner  = rowLabels.reduced(outerPadX, 0);

        auto c1 = slidersInner.removeFromLeft(sideW); slidersInner.removeFromLeft(gapX);
        auto c2 = slidersInner.removeFromLeft(midW);  slidersInner.removeFromLeft(gapX);
        auto c3 = slidersInner.removeFromLeft(sideW);

        sliderL.setBounds(c1);
        sliderM.setBounds(c2);
        sliderR.setBounds(c3);

        const int valueRowH  = 16;
        const int letterRowH = 16;
        const int midGap     = 4;
        auto valueRow  = labelsInner.removeFromTop(valueRowH);
        labelsInner.removeFromTop(midGap);
        auto letterRow = labelsInner.removeFromTop(letterRowH);

        auto v1 = valueRow.removeFromLeft(sideW); valueRow.removeFromLeft(gapX);
        valueRow.removeFromLeft(midW);            valueRow.removeFromLeft(gapX);
        auto v3 = valueRow.removeFromLeft(sideW);
        const int expand = 8;
        valL.setBounds(v1.expanded(expand, 0));
        valR.setBounds(v3.expanded(expand, 0));

        auto l1 = letterRow.removeFromLeft(sideW); letterRow.removeFromLeft(gapX);
        auto l2 = letterRow.removeFromLeft(midW);  letterRow.removeFromLeft(gapX);
        auto l3 = letterRow.removeFromLeft(sideW);
        labelL.setBounds(l1);
        labelM.setBounds(l2);
        labelR.setBounds(l3);

        // Prepare drag track (if handles are re-enabled later)
        dragTrack = sliderL.getBounds().withRight(sliderR.getRight());
        dragTrack = dragTrack.reduced(juce::jmax(sliderL.getWidth() / 4, 4), 4);
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
        auto drawIn = [&](const juce::Rectangle<int>& r, float norm)
        {
            if (r.isEmpty() || norm <= 0.001f) return;
            auto rf = r.toFloat().reduced(2.0f);
            const float radius = ::Style::theme().borderRadius;
            const float h = rf.getHeight() * juce::jlimit(0.0f, 1.0f, norm);
            juce::Rectangle<float> fill = rf.withY(rf.getBottom() - h).withHeight(h);
            g.setColour(juce::Colours::aqua.withAlpha(0.22f));
            g.fillRoundedRectangle(fill, radius);
        };
        drawIn(sliderL.getBounds(), meterL);
        drawIn(sliderR.getBounds(), meterR);
    }

private:
    void updateValueLabels()
    {
        auto fmt = [](juce::Slider& s){ const double v = s.getValue(); return std::abs(v) < 10.0 ? juce::String(v, 2) : juce::String(v, 1); };
        valL.setText(fmt(sliderL), juce::dontSendNotification);
        valR.setText(fmt(sliderR), juce::dontSendNotification);
        valL.setMinimumHorizontalScale(0.7f);
        valR.setMinimumHorizontalScale(0.7f);
    }

    APVTS& apvts;

    // UI elements
    juce::Label  title;
    juce::Slider sliderL, sliderR;
    FilmBar      sliderM;
    juce::Label  labelL, labelM, labelR;
    juce::Label  valL, valR; // numeric value labels under side bars
    juce::ToggleButton linkButton { "Link" };

    // live meter state
    float meterL { 0.0f };
    float meterR { 0.0f };

    // Drag handles (currently not visible)
    juce::Rectangle<int> dragTrack;

    std::unique_ptr<APVTS::SliderAttachment>  attL, attR;
    std::unique_ptr<APVTS::ButtonAttachment>  attLink;

    juce::String paramLId, paramRId, linkId;
};
