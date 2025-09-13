#include "LookAndFeels.h"
#include <BinaryData.h>

// ----- VibeLNF -----
VibeLNF::VibeLNF()
{
    // Typography: set bundled Montserrat as default if available
    auto loadTypeface = []() -> juce::Typeface::Ptr
    {
        int sz = 0;
        const void* data = BinaryData::getNamedResource("Montserrat-VariableFont_wght.ttf", sz);
        if (!data)
            data = BinaryData::getNamedResource("Montserrat-Italic-VariableFont_wght.ttf", sz);
        if (data && sz > 0)
            return juce::Typeface::createSystemTypefaceFor(data, (size_t)sz);
        return {};
    };
    if (auto tf = loadTypeface())
        setDefaultSansSerifTypeface(tf);

    auto& th = Style::theme();
    setColour(juce::Slider::textBoxTextColourId, th.text);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, th.text);
}

// ----- PillVSliderLNF -----
void PillVSliderLNF::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float minPos, float maxPos,
    const juce::Slider::SliderStyle style,
    juce::Slider& s)
{
    juce::ignoreUnused(sliderPos, minPos, maxPos, style);

    auto bounds = juce::Rectangle<float>(x, y, (float)w, (float)h)
        .reduced(juce::jmax(w * 0.45f, 6.0f), 6.0f);
    bounds.removeFromBottom(4.0f);
    const float radius = bounds.getWidth() * 0.5f;

    // track
    juce::ColourGradient trackGrad(Style::theme().trackTop, bounds.getX(), bounds.getY(),
        Style::theme().trackBot, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(trackGrad);
    g.fillRoundedRectangle(bounds, radius);

    // fill amount (bottom up)
    const auto range = s.getRange();
    const double prop = range.getLength() > 0.0
        ? (s.getValue() - range.getStart()) / range.getLength()
        : 0.0;

    auto fill = bounds;
    fill.removeFromTop(fill.getHeight() * (float)(1.0 - juce::jlimit(0.0, 1.0, prop)));

    juce::ColourGradient fillGrad(Style::theme().fillBot, fill.getX(), fill.getBottom(),
        Style::theme().fillTop, fill.getX(), fill.getY(), false);
    g.setGradientFill(fillGrad);
    g.fillRoundedRectangle(fill, radius);

    // no outline or gloss (clean look)
    if (outlineAlpha > 0.0f)
    {
        g.setColour(juce::Colours::white.withAlpha(outlineAlpha));
        g.drawRoundedRectangle(bounds, radius, 1.0f);
    }

    // value text at bottom of the slider
    auto textArea = juce::Rectangle<int>(x, y, w, h).reduced(4).removeFromBottom(22);
    g.setColour(Style::theme().text);
    g.setFont(juce::Font(juce::FontOptions(12.0f)));
    g.drawFittedText(s.getTextFromValue(s.getValue()), textArea, juce::Justification::centred, 1);
}
