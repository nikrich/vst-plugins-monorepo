#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>
#include "Styling/Theme.h"

// This header combines the shared LookAndFeel classes to be reused across plugins.

// Simple app-wide tweaks
struct VibeLNF : juce::LookAndFeel_V4
{
    VibeLNF()
    {
        // Try bundled Montserrat as default
        int sz = 0;
        const void* data = BinaryData::getNamedResource("Montserrat-VariableFont_wght.ttf", sz);
        if (!data)
            data = BinaryData::getNamedResource("Montserrat-Italic-VariableFont_wght.ttf", sz);
        if (data && sz > 0)
            setDefaultSansSerifTypeface(juce::Typeface::createSystemTypefaceFor(data, (size_t) sz));

        auto& th = ::Style::theme();
        setColour(juce::Slider::textBoxTextColourId, th.text);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::textColourId, th.text);
    }
};

// Pill-shaped vertical slider skin (for Slider::LinearBarVertical)
struct PillVSliderLNF : juce::LookAndFeel_V4
{
    float outlineAlpha{ 0.20f };

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float minPos, float maxPos,
        const juce::Slider::SliderStyle style,
        juce::Slider& s) override
    {
        juce::ignoreUnused(sliderPos, minPos, maxPos, style);
        const float sidePad = 8.0f;
        auto bounds = juce::Rectangle<float>(x, y, (float)w, (float)h).reduced(sidePad, 6.0f);
        bounds.removeFromBottom(4.0f);
        const float radius = bounds.getWidth() * 0.5f;

        juce::ColourGradient trackGrad(::Style::theme().trackTop, bounds.getX(), bounds.getY(),
                                       ::Style::theme().trackBot, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(bounds, radius);

        const auto range = s.getRange();
        const double prop = range.getLength() > 0.0 ? (s.getValue() - range.getStart()) / range.getLength() : 0.0;
        auto fill = bounds;
        fill.removeFromTop(fill.getHeight() * (float)(1.0 - juce::jlimit(0.0, 1.0, prop)));

        juce::ColourGradient fillGrad(::Style::theme().fillBot, fill.getX(), fill.getBottom(),
                                      ::Style::theme().fillTop, fill.getX(), fill.getY(), false);
        g.setGradientFill(fillGrad);
        g.fillRoundedRectangle(fill, radius);

        if (outlineAlpha > 0.0f)
        {
            g.setColour(juce::Colours::white.withAlpha(outlineAlpha));
            g.drawRoundedRectangle(bounds, radius, 1.0f);
        }
    }
};

// Neon toggle with optional image skin
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
        btnOff = tryLoadExact("_001_png");
        btnOn  = tryLoadExact("_002_png");
        if (!(btnOff.isValid() && btnOn.isValid())) { btnOff = tryLoadExact("001_png"); btnOn = tryLoadExact("002_png"); }

        if (!(btnOff.isValid() && btnOn.isValid()))
        {
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                if (auto* resName = BinaryData::namedResourceList[i])
                    if (auto* orig = BinaryData::getNamedResourceOriginalFilename(resName))
                    {
                        juce::String sOrig(orig);
                        auto loadRes = [&](juce::Image& dst)
                        {
                            int sz = 0;
                            if (const void* data = BinaryData::getNamedResource(resName, sz))
                                dst = juce::ImageFileFormat::loadFrom(data, (size_t)sz);
                        };
                        if (!btnOff.isValid())
                        {
                            if (sOrig.endsWithIgnoreCase("assets/ui/kit-06/button/001.png") ||
                                (sOrig.endsWithIgnoreCase("001.png") && sOrig.containsIgnoreCase("kit-06/button")))
                            {
                                loadRes(btnOff);
                                continue;
                            }
                        }
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

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                          bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        if (btnOff.isValid() && btnOn.isValid())
        {
            const juce::Image& img = b.getToggleState() ? btnOn : btnOff;
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
            return;
        }
        auto rr = r;
        const float pillH = juce::jlimit(20.0f, 28.0f, rr.getHeight());
        auto pill = rr.removeFromTop(pillH).reduced(2.0f);
        const float rad = pill.getHeight() * 0.5f;
        g.setColour(::Style::theme().panel);
        g.fillRoundedRectangle(pill, rad);
        if (b.getToggleState())
        {
            juce::ColourGradient grad(::Style::theme().accent2, pill.getX(), pill.getCentreY(),
                                      ::Style::theme().accent1, pill.getRight(), pill.getCentreY(), false);
            grad.addColour(0.5, ::Style::theme().accent2);
            g.setGradientFill(grad);
            auto onRect = pill.removeFromLeft(pill.getWidth() * 0.58f);
            g.fillRoundedRectangle(onRect, rad);
        }
        auto knob = juce::Rectangle<float>(pill.getHeight(), pill.getHeight())
                        .withCentre({ b.getToggleState() ? pill.getRight() - rad : pill.getX() + rad, pill.getCentreY() });
        g.setColour(juce::Colours::white.withAlpha(0.95f));
        g.fillEllipse(knob);
        auto labelArea = rr.reduced(2.0f);
        g.setColour(::Style::theme().text);
        g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::plain)));
        g.drawFittedText(b.getButtonText(), labelArea.toNearestInt(), juce::Justification::centred, 1);
    }
};

struct SquareToggleLNF : juce::LookAndFeel_V4
{
    float border = 3.0f;
    float corner = 8.0f;
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                          bool, bool) override
    {
        auto r = b.getLocalBounds().reduced(4).toFloat();
        auto side = juce::jmin(r.getWidth(), r.getHeight());
        r = juce::Rectangle<float>(side, side).withCentre(r.getCentre());
        auto bg = b.getToggleState() ? juce::Colours::white.withAlpha(0.20f) : juce::Colours::transparentBlack;
        g.setColour(bg);
        g.fillRoundedRectangle(r, corner);
        g.setColour(juce::Colours::black);
        g.drawRoundedRectangle(r, corner, border);
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(juce::FontOptions(18.0f)));
        g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
    }
};

// Donut gradient rotary knob with optional filmstrip inside
struct DonutKnobLNF : juce::LookAndFeel_V4
{
    juce::Colour face{ 0xFF0B1B1E };
    juce::Image knobImage;
    int filmstripFrames { 0 };
    bool filmstripVertical { true };
    int filmFrameSize { 0 };

    // Allow external code to set a filmstrip image programmatically.
    void setKnobImage(juce::Image img, int frames = 0, bool vertical = true)
    {
        knobImage = std::move(img);
        filmstripFrames = frames;
        filmstripVertical = vertical;
        // If frames provided and image is valid, infer frame size now to avoid lazy zero size
        if (knobImage.isValid() && filmstripFrames > 1)
        {
            filmFrameSize = filmstripVertical ? knobImage.getWidth() : knobImage.getHeight();
        }
        else
        {
            filmFrameSize = 0; // will be inferred on first draw if frames == 0 or image invalid
        }
    }

    DonutKnobLNF()
    {
        auto tryNamed = [&](const char* name)
        {
            int sz = 0; if (const void* data = BinaryData::getNamedResource(name, sz))
                return juce::ImageFileFormat::loadFrom(data, (size_t)sz);
            return juce::Image();
        };
        knobImage = tryNamed("mkfinal_png");
        if (!knobImage.isValid()) knobImage = tryNamed("mk_final_png");
        if (!knobImage.isValid()) knobImage = tryNamed("mk-final.png");
        if (!knobImage.isValid())
        {
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
                if (auto* resName = BinaryData::namedResourceList[i])
                    if (auto* orig = BinaryData::getNamedResourceOriginalFilename(resName))
                    {
                        juce::String sOrig(orig);
                        if (sOrig.containsIgnoreCase("knob") && sOrig.endsWithIgnoreCase(".png"))
                        {
                            int sz = 0;
                            if (const void* data = BinaryData::getNamedResource(resName, sz))
                            { knobImage = juce::ImageFileFormat::loadFrom(data, (size_t)sz); if (knobImage.isValid()) break; }
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
        const float ringThickness = 2.0f;
        const float inner = radius - ringThickness;
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path bg; bg.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
        juce::PathStrokeType stroke(ringThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::ColourGradient trackGrad(::Style::theme().trackTop, centre.x, centre.y - radius,
                                       ::Style::theme().trackBot, centre.x, centre.y + radius, false);
        g.setGradientFill(trackGrad);
        g.strokePath(bg, stroke);

        if (slider.isEnabled())
        {
            juce::Path valueArc; valueArc.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, angle, true);
            const float ringOuterRadius = radius + ringThickness * 0.5f;
            const float haloExtent = juce::jmax(12.0f, ringThickness * 10.0f);
            juce::Path outwardOnlyClip; outwardOnlyClip.addEllipse(centre.x - (ringOuterRadius + haloExtent), centre.y - (ringOuterRadius + haloExtent), 2.0f * (ringOuterRadius + haloExtent), 2.0f * (ringOuterRadius + haloExtent));
            outwardOnlyClip.addEllipse(centre.x - ringOuterRadius, centre.y - ringOuterRadius, 2.0f * ringOuterRadius, 2.0f * ringOuterRadius);
            outwardOnlyClip.setUsingNonZeroWinding(false);
            juce::Graphics::ScopedSaveState save(g);
            g.reduceClipRegion(outwardOnlyClip);
            const float haloThicknesses[] = { ringThickness * 3.0f, ringThickness * 6.0f, ringThickness * 10.0f };
            const float haloAlphas[]      = { 0.40f,                 0.20f,                 0.08f };
            for (int i = 0; i < 3; ++i)
            {
                juce::PathStrokeType haloStroke(haloThicknesses[i], juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
                g.setColour(juce::Colour(0xFFC084FC).withAlpha(haloAlphas[i]));
                g.strokePath(valueArc, haloStroke);
            }
        }

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

        auto innerBounds = juce::Rectangle<float>(2.0f * inner, 2.0f * inner).withCentre(centre);
        if (knobImage.isValid())
        {
            const float pad = juce::jmax(2.0f, ringThickness * 0.10f);
            auto target = innerBounds.reduced(pad);
            if (filmstripFrames <= 1)
            {
                const int w = knobImage.getWidth();
                const int h = knobImage.getHeight();
                // Infer frames if a typical 128-frame strip is detected (height multiple of width or vice versa)
                if (h % w == 0 && w > 0) { filmstripVertical = true; filmFrameSize = w; filmstripFrames = juce::jmax(1, h / w); }
                else if (w % h == 0 && h > 0) { filmstripVertical = false; filmFrameSize = h; filmstripFrames = juce::jmax(1, w / h); }
                else { filmstripFrames = 1; filmFrameSize = juce::jmin(w, h); }
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
                float scale = target.getWidth() / (float)knobImage.getWidth();
                auto t = juce::AffineTransform::translation(-(float)knobImage.getWidth() * 0.5f, -(float)knobImage.getHeight() * 0.5f)
                           .scaled(scale, scale)
                           .translated(target.getCentreX(), target.getCentreY());
                g.drawImageTransformed(knobImage, t, false);
            }
        }
        else
        {
            juce::ColourGradient innerGrad(juce::Colour(0xFFB794F6), innerBounds.getX(), innerBounds.getY(),
                                           juce::Colour(0xFF5B21B6), innerBounds.getRight(), innerBounds.getBottom(), false);
            innerGrad.isRadial = true; innerGrad.point1 = { centre.x - inner * 0.3f, centre.y - inner * 0.3f };
            innerGrad.point2 = { centre.x + inner * 0.7f, centre.y + inner * 0.7f };
            g.setGradientFill(innerGrad);
            g.fillEllipse(innerBounds);
        }
        g.setColour(::Style::theme().text);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        auto text = slider.getTextFromValue(slider.getValue());
        g.drawFittedText(text, r.toNearestInt(), juce::Justification::centred, 1);
    }
};
