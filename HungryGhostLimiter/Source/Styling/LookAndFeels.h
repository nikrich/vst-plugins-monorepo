#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

// Simple app-wide tweaks
struct VibeLNF : juce::LookAndFeel_V4
{
    VibeLNF();
};

// Pill-shaped vertical slider skin (for Slider::LinearBarVertical)
struct PillVSliderLNF : juce::LookAndFeel_V4
{
    juce::Colour trackBgTop{ 0xFF1B1F27 }, trackBgBot{ 0xFF141821 };
    juce::Colour fillBottom{ 0xFFFFB34D }, fillTop{ 0xFFFF3B3B };
    float outlineAlpha{ 0.20f };

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float minPos, float maxPos,
        const juce::Slider::SliderStyle style,
        juce::Slider& s) override;
};

// Neon / pill toggle for dark UI
struct NeonToggleLNF : juce::LookAndFeel_V4
{
    juce::Colour offBg{ 0xFF141821 };
    juce::Colour onGradInner{ 0xFFFFB34D }; // orange
    juce::Colour onGradOuter{ 0xFFFF3B3B }; // red
    float radius = 10.0f;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
        bool shouldHighlight, bool shouldDown) override
    {
        auto r = b.getLocalBounds().toFloat();
        auto bg = r.reduced(1.0f);
        const float h = bg.getHeight();
        const float w = juce::jlimit(28.0f, 120.0f, bg.getWidth());
        const float rad = h * 0.5f;

        // pill body
        g.setColour(offBg);
        g.fillRoundedRectangle(bg, rad);

        // on fill (glow gradient)
        if (b.getToggleState())
        {
            juce::ColourGradient grad(onGradInner, bg.getX(), bg.getCentreY(),
                onGradOuter, bg.getRight(), bg.getCentreY(), false);
            grad.addColour(0.5, onGradInner);
            g.setGradientFill(grad);
            auto onRect = bg.removeFromLeft(w * 0.58f);
            g.fillRoundedRectangle(onRect, rad);

            // subtle outer glow
            juce::DropShadow ds(juce::Colours::orange.withAlpha(0.3f), 12, {});
            juce::Path path;
            path.addRoundedRectangle(onRect, rad);
            ds.drawForPath(g, path);
        }

        // circular knob
        auto knob = r.withTrimmedLeft(4).withTrimmedRight(4);
        knob.setWidth(knob.getHeight());
        if (b.getToggleState())
            knob.setX(bg.getRight() - knob.getWidth() - 4.0f);
        else
            knob.setX(bg.getX() + 4.0f);

        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillEllipse(knob.toFloat());

        // label
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        g.drawFittedText(b.getButtonText(), r.withTrimmedLeft((int)(r.getHeight() * 1.2f)).toNearestInt(), juce::Justification::centredLeft, 1);

        if (shouldHighlight || shouldDown)
            g.fillAll(juce::Colours::white.withAlpha(0.03f));
    }
};
