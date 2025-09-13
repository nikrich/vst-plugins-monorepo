#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include "Theme.h"
#include <BinaryData.h>

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
    juce::Colour face{ 0xFF0B1B1E }; // fallback face if image missing

    // Optional image knob (filmstrip or single). If present, it is drawn with frame selection.
    juce::Image knobImage;
    int filmstripFrames { 0 };     // >0 if knobImage is a filmstrip
    bool filmstripVertical { true }; // true if frames stacked vertically
    int filmFrameSize { 0 };       // width/height of a single frame (square)

    DonutKnobLNF()
    {
        // Try to load embedded image first (BinaryData from Projucer or CMake)
        auto tryNamed = [&](const char* name)
        {
            int sz = 0; if (const void* data = BinaryData::getNamedResource(name, sz))
                return juce::ImageFileFormat::loadFrom(data, (size_t)sz);
            return juce::Image();
        };
        // Prefer kit-03 middle knob filmstrip if present
        knobImage = tryNamed("mkfinal_png");           // likely symbol from mk-final.png
        if (!knobImage.isValid()) knobImage = tryNamed("mk_final_png");
        if (!knobImage.isValid()) knobImage = tryNamed("mk-final.png");
      
        // As a robust fallback, scan all embedded resources for one containing "knob" and ending with .png
        if (!knobImage.isValid())
        {
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                if (auto* resName = BinaryData::namedResourceList[i])
                {
                    if (auto* orig = BinaryData::getNamedResourceOriginalFilename(resName))
                    {
                        juce::String sOrig(orig);
                        if (sOrig.containsIgnoreCase("knob") && sOrig.endsWithIgnoreCase(".png"))
                        {
                            int sz = 0;
                            if (const void* data = BinaryData::getNamedResource(resName, sz))
                            {
                                knobImage = juce::ImageFileFormat::loadFrom(data, (size_t)sz);
                                if (knobImage.isValid()) break;
                            }
                        }
                    }
                }
            }
        }
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
        float pos, const float startAngle, const float endAngle,
        juce::Slider& slider) override
    {
        auto r = juce::Rectangle<float>(x, y, (float)w, (float)h).reduced(6.0f);
        auto diam = juce::jmin(r.getWidth(), r.getHeight());
        auto centre = r.getCentre();
        auto radius = diam * 0.5f;

        // Thinner ring: compute thickness as a fraction of radius
        const float ringThickness = juce::jlimit(2.0f, radius * 0.18f, radius * 0.22f); // ~18–22% of radius
        const float inner = radius - ringThickness;

        const float angle = startAngle + pos * (endAngle - startAngle);

        // background ring (track-like)
        juce::Path bg; bg.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        juce::PathStrokeType stroke(ringThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::ColourGradient trackGrad(Style::theme().trackTop, centre.x, centre.y - radius,
                                       Style::theme().trackBot, centre.x, centre.y + radius, false);
        g.setGradientFill(trackGrad);
        g.strokePath(bg, stroke);

        // Purple glow behind the value ring
        {
            juce::Path glow; glow.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
            juce::Colour glowCol = juce::Colour(0xFFC084FC).withAlpha(0.35f); // soft purple glow
            juce::PathStrokeType glowStroke(ringThickness * 2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour(glowCol);
            g.strokePath(glow, glowStroke);
        }

        // value ring (purple accent)
        juce::Path val; val.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        juce::ColourGradient grad(juce::Colour(0xFFC084FC), centre.x - radius, centre.y + radius,
                                  juce::Colour(0xFF7C3AED), centre.x + radius, centre.y - radius, false);
        g.setGradientFill(grad);
        g.strokePath(val, stroke);

        // Inner face: if we have a PNG knob, draw a filmstrip frame if available; otherwise fallback gradient face
        auto innerBounds = juce::Rectangle<float>(2.0f * inner, 2.0f * inner).withCentre(centre);
        if (knobImage.isValid())
        {
            const float pad = juce::jmax(2.0f, ringThickness * 0.10f); // minimal padding inside ring
            auto target = innerBounds.reduced(pad);

            // Detect filmstrip layout once
            if (filmstripFrames == 0)
            {
                const int w = knobImage.getWidth();
                const int h = knobImage.getHeight();
                // Assume square frames; detect orientation
                if (h > w) { filmstripVertical = true; filmFrameSize = w; filmstripFrames = juce::jmax(1, h / juce::jmax(1, w)); }
                else if (w > h) { filmstripVertical = false; filmFrameSize = h; filmstripFrames = juce::jmax(1, w / juce::jmax(1, h)); }
                else { filmstripFrames = 1; filmFrameSize = juce::jmin(w, h); }
                // Clamp to 128 if the kit declares that many
                filmstripFrames = juce::jmin(128, filmstripFrames);
            }

            if (filmstripFrames > 1 && filmFrameSize > 0)
            {
                const int idx = juce::jlimit(0, filmstripFrames - 1, (int)std::round(pos * (filmstripFrames - 1)));
                int sx = 0, sy = 0, sw = filmFrameSize, sh = filmFrameSize;
                if (filmstripVertical) sy = idx * filmFrameSize; else sx = idx * filmFrameSize;
                g.drawImage(knobImage,
                            (int)target.getX(), (int)target.getY(), (int)target.getWidth(), (int)target.getHeight(),
                            sx, sy, sw, sh);
            }
            else
            {
                // Single image fallback
                float scale = target.getWidth() / (float)knobImage.getWidth();
                auto t = juce::AffineTransform::translation(-(float)knobImage.getWidth() * 0.5f,
                                                             -(float)knobImage.getHeight() * 0.5f)
                            .scaled(scale, scale)
                            .translated(target.getCentreX(), target.getCentreY());
                g.drawImageTransformed(knobImage, t, false);
            }
        }
        else
        {
            juce::ColourGradient innerGrad(juce::Colour(0xFFB794F6), innerBounds.getX(), innerBounds.getY(),
                                           juce::Colour(0xFF5B21B6), innerBounds.getRight(), innerBounds.getBottom(), false);
            innerGrad.isRadial = true;
            innerGrad.point1 = { centre.x - inner * 0.3f, centre.y - inner * 0.3f };
            innerGrad.point2 = { centre.x + inner * 0.7f, centre.y + inner * 0.7f };
            g.setGradientFill(innerGrad);
            g.fillEllipse(innerBounds);
        }

        // value text
        g.setColour(Style::theme().text);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        auto text = slider.getTextFromValue(slider.getValue());
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
};
