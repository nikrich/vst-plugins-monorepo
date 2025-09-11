#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Theme.h"

// Simple app-wide tweaks
struct VibeLNF : juce::LookAndFeel_V4
{
    VibeLNF();
};

// Pill-shaped vertical slider skin (for Slider::LinearBarVertical)
struct PillVSliderLNF : juce::LookAndFeel_V4
{
    float outlineAlpha{ 0.20f }; // remove white outline around sliders

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float minPos, float maxPos,
        const juce::Slider::SliderStyle style,
        juce::Slider& s) override;
};

// Neon / pill toggle for dark UI
struct NeonToggleLNF : juce::LookAndFeel_V4
{
    float radius = 10.0f;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
        bool shouldHighlight, bool shouldDown) override
    {
        auto r = b.getLocalBounds().toFloat();

        // Layout: pill switch at top, label below
        const float pillH = juce::jlimit(20.0f, 28.0f, r.getHeight());
        auto pill = r.removeFromTop(pillH);
        pill = pill.reduced(2.0f);
        const float rad = pill.getHeight() * 0.5f;

        // pill body
        g.setColour(Style::theme().panel);
        g.fillRoundedRectangle(pill, rad);

        // on fill (glow gradient)
        if (b.getToggleState())
        {
            juce::ColourGradient grad(Style::theme().accent2, pill.getX(), pill.getCentreY(),
                Style::theme().accent1, pill.getRight(), pill.getCentreY(), false);
            grad.addColour(0.5, Style::theme().accent2);
            g.setGradientFill(grad);
            auto onRect = pill.removeFromLeft(pill.getWidth() * 0.58f);
            g.fillRoundedRectangle(onRect, rad);
        }

        // circular knob
        auto knob = juce::Rectangle<float>(pill.getHeight(), pill.getHeight()).withCentre({ b.getToggleState() ? pill.getRight() - rad : pill.getX() + rad, pill.getCentreY() });
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillEllipse(knob);

        // label
        auto labelArea = r.reduced(2.0f);
        g.setColour(Style::theme().text);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        g.drawFittedText(b.getButtonText(), labelArea.toNearestInt(), juce::Justification::centred, 1);

        if (shouldHighlight || shouldDown)
            g.fillAll(juce::Colours::white.withAlpha(0.03f));
    }
};

// Global square checkbox / radio look used across advanced cards
struct SquareToggleLNF : juce::LookAndFeel_V4
{
    float border = 3.0f;
    float corner = 8.0f;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                          bool /*highlighted*/, bool /*down*/) override
    {
        auto r = b.getLocalBounds().reduced(4).toFloat();
        // Make the content area square if this is a bare checkbox (not list-style)
        auto side = juce::jmin(r.getWidth(), r.getHeight());
        r = juce::Rectangle<float>(side, side).withCentre(r.getCentre());

        auto bg = b.getToggleState() ? juce::Colours::white.withAlpha(0.20f)
                                     : juce::Colours::transparentBlack;
        g.setColour(bg);
        g.fillRoundedRectangle(r, corner);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(r, corner, border);

        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(18.0f)));
        g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }
};

// Donut gradient rotary knob (dark UI, orange→red progress)
struct DonutKnobLNF : juce::LookAndFeel_V4
{
    // colours via Style::theme() in draw
    juce::Colour face{ 0xFF0B1B1E }; // knob inner bg from CSS --knob-bg

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float pos, const float startAngle, const float endAngle,
        juce::Slider& slider) override
    {
        auto r = juce::Rectangle<float>(x, y, (float)w, (float)h).reduced(6.0f);
        auto diam = juce::jmin(r.getWidth(), r.getHeight());
        auto centre = r.getCentre();
        auto radius = diam * 0.5f;
        auto inner = radius * 0.64f;      // ring thickness

        const float angle = startAngle + pos * (endAngle - startAngle);

        // background ring (track-like)
        juce::Path bg; bg.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        juce::PathStrokeType stroke(radius - inner, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::ColourGradient trackGrad(Style::theme().trackTop, centre.x, centre.y - radius,
                                       Style::theme().trackBot, centre.x, centre.y + radius, false);
        g.setGradientFill(trackGrad);
        g.strokePath(bg, stroke);

        // value ring (aqua gradient accent)
        juce::Path val; val.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        juce::ColourGradient grad(Style::theme().accent1, centre.x - radius, centre.y + radius,
                                  Style::theme().accent2, centre.x + radius, centre.y - radius, false);
        g.setGradientFill(grad);
        g.strokePath(val, stroke);

        // inner face with radial gradient (aqua → deep teal)
        auto innerBounds = juce::Rectangle<float>(2.0f * inner, 2.0f * inner).withCentre(centre);
        juce::ColourGradient innerGrad(juce::Colour(0xFF1FE7D0), innerBounds.getX(), innerBounds.getY(),
                                       juce::Colour(0xFF004C59), innerBounds.getRight(), innerBounds.getBottom(), false);
        innerGrad.isRadial = true;
        innerGrad.point1 = { centre.x - inner * 0.3f, centre.y - inner * 0.3f };
        innerGrad.point2 = { centre.x + inner * 0.7f, centre.y + inner * 0.7f };
        g.setGradientFill(innerGrad);
        g.fillEllipse(innerBounds);

        // value text
        g.setColour(Style::theme().text);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        auto text = slider.getTextFromValue(slider.getValue());
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
};
