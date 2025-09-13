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

// Neon / pill toggle for dark UI (now supports image skin from kit-06/button)
struct NeonToggleLNF : juce::LookAndFeel_V4
{
    float radius = 10.0f;

    juce::Image btnOff;
    juce::Image btnOn;

    NeonToggleLNF()
    {
        auto tryLoadExact = [](const char* name) -> juce::Image
        {
            int sz = 0; if (const void* data = BinaryData::getNamedResource(name, sz))
                return juce::ImageFileFormat::loadFrom(data, (size_t)sz);
            return {};
        };

        // Attempt common symbolizations first (JUCE prefixes identifiers that start with digits with an underscore)
        btnOff = tryLoadExact("_001_png");
        btnOn  = tryLoadExact("_002_png");
        if (!(btnOff.isValid() && btnOn.isValid())) { btnOff = tryLoadExact("001_png"); btnOn = tryLoadExact("002_png"); }

        // If not found, scan BinaryData for original filenames matching kit-06/button/001.png and 002.png
        if (!(btnOff.isValid() && btnOn.isValid()))
        {
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                if (auto* resName = BinaryData::namedResourceList[i])
                {
                    if (auto* orig = BinaryData::getNamedResourceOriginalFilename(resName))
                    {
                        juce::String sOrig(orig);
                        auto loadRes = [&](juce::Image& dst)
                        {
                            int sz = 0;
                            if (const void* data = BinaryData::getNamedResource(resName, sz))
                                dst = juce::ImageFileFormat::loadFrom(data, (size_t)sz);
                        };

                        // Match off state
                        if (!btnOff.isValid())
                        {
                            if (sOrig.endsWithIgnoreCase("assets/ui/kit-06/button/001.png") ||
                                (sOrig.endsWithIgnoreCase("001.png") && sOrig.containsIgnoreCase("kit-06/button")))
                            {
                                loadRes(btnOff);
                                continue;
                            }
                        }
                        // Match on state
                        if (!btnOn.isValid())
                        {
                            if (sOrig.endsWithIgnoreCase("assets/ui/kit-06/button/002.png") ||
                                (sOrig.endsWithIgnoreCase("002.png") && sOrig.containsIgnoreCase("kit-06/button")))
                            {
                                loadRes(btnOn);
                                continue;
                            }
                        }

                        if (btnOff.isValid() && btnOn.isValid()) break;
                    }
                }
            }
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
        bool shouldHighlight, bool shouldDown) override
    {
        auto r = b.getLocalBounds().toFloat();

        // If we have skinned images, draw those; else fallback to pill style
        if (btnOff.isValid() && btnOn.isValid())
        {
            const juce::Image& img = b.getToggleState() ? btnOn : btnOff;

            // Maintain aspect ratio and fit within bounds
            const float iw = (float) img.getWidth();
            const float ih = (float) img.getHeight();
            const float sx = r.getWidth()  / iw;
            const float sy = r.getHeight() / ih;
            const float scale = juce::jmin(sx, sy);
            const float dw = iw * scale;
            const float dh = ih * scale;
            const float dx = r.getX() + (r.getWidth()  - dw) * 0.5f;
            const float dy = r.getY() + (r.getHeight() - dh) * 0.5f;

            g.drawImage(img, (int)dx, (int)dy, (int)dw, (int)dh, 0, 0, (int)iw, (int)ih);

            // Skip drawing default text when skinned
            return;
        }

        // Fallback: original neon pill style
        // Layout: pill switch at top, label below
        auto rr = r;
        const float pillH = juce::jlimit(20.0f, 28.0f, rr.getHeight());
        auto pill = rr.removeFromTop(pillH);
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
        auto labelArea = rr.reduced(2.0f);
        g.setColour(Style::theme().text);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        g.drawFittedText(b.getButtonText(), labelArea.toNearestInt(), juce::Justification::centred, 1);

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

        // Fixed ultra-thin ring width (2 px)
        const float ringThickness = 2.0f;
        const float inner = radius - ringThickness;

        const float angle = startAngle + pos * (endAngle - startAngle);

        // background ring (track-like)
        juce::Path bg; bg.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        juce::PathStrokeType stroke(ringThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::ColourGradient trackGrad(Style::theme().trackTop, centre.x, centre.y - radius,
                                       Style::theme().trackBot, centre.x, centre.y + radius, false);
        g.setGradientFill(trackGrad);
        g.strokePath(bg, stroke);

        // Stronger outward halo behind the value ring with smooth fade (Option A)
        if (slider.isEnabled())
        {
            juce::Path valueArc; valueArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);

            const float ringOuterRadius = radius + ringThickness * 0.5f; // outer edge of stroked ring
            const float haloExtent = juce::jmax(12.0f, ringThickness * 10.0f);

            juce::Path outwardOnlyClip;
            outwardOnlyClip.addEllipse(centre.x - (ringOuterRadius + haloExtent), centre.y - (ringOuterRadius + haloExtent),
                                       2.0f * (ringOuterRadius + haloExtent), 2.0f * (ringOuterRadius + haloExtent));
            outwardOnlyClip.addEllipse(centre.x - ringOuterRadius, centre.y - ringOuterRadius,
                                       2.0f * ringOuterRadius, 2.0f * ringOuterRadius);
            outwardOnlyClip.setUsingNonZeroWinding(false); // even-odd -> donut region

            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(outwardOnlyClip);

            const float haloThicknesses[] = { ringThickness * 3.0f, ringThickness * 6.0f, ringThickness * 10.0f };
            const float haloAlphas[]      = { 0.40f,                 0.20f,                 0.08f };

            for (int i = 0; i < 3; ++i)
            {
                juce::PathStrokeType haloStroke(haloThicknesses[i], juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
                g.setColour(juce::Colour(0xFFC084FC).withAlpha(haloAlphas[i])); // same hue as ring, stronger glow with fade
                g.strokePath(valueArc, haloStroke);
            }
        }

        // value ring: purple when enabled, grey when disabled
        juce::Path val; val.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
        if (slider.isEnabled())
        {
            juce::ColourGradient grad(juce::Colour(0xFFC084FC), centre.x - radius, centre.y + radius,
                                      juce::Colour(0xFF7C3AED), centre.x + radius, centre.y - radius, false);
            g.setGradientFill(grad);
        }
        else
        {
            juce::ColourGradient grad(juce::Colour(0xFFBBBBBB), centre.x - radius, centre.y + radius,
                                      juce::Colour(0xFF888888), centre.x + radius, centre.y - radius, false);
            g.setGradientFill(grad);
        }
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
