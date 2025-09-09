#include "LookAndFeels.h"

// ----- VibeLNF -----
VibeLNF::VibeLNF()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
}

// ----- PillVSliderLNF -----
void PillVSliderLNF::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
    float sliderPos, float minPos, float maxPos,
    const juce::Slider::SliderStyle style,
    juce::Slider& s)
{
    juce::ignoreUnused(sliderPos, minPos, maxPos, style);

    auto bounds = juce::Rectangle<float>(x, y, (float)w, (float)h)
        .reduced(juce::jmax(w * 0.35f, 6.0f), 6.0f);
    bounds.removeFromBottom(4.0f);
    const float radius = bounds.getWidth() * 0.5f;

    // track
    juce::ColourGradient trackGrad(trackBgTop, bounds.getX(), bounds.getY(),
        trackBgBot, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(trackGrad);
    g.fillRoundedRectangle(bounds, radius);

    // fill amount (bottom up)
    const auto range = s.getRange();
    const double prop = range.getLength() > 0.0
        ? (s.getValue() - range.getStart()) / range.getLength()
        : 0.0;

    auto fill = bounds;
    fill.removeFromTop(fill.getHeight() * (float)(1.0 - juce::jlimit(0.0, 1.0, prop)));

    juce::ColourGradient fillGrad(fillBottom, fill.getX(), fill.getBottom(),
        fillTop, fill.getX(), fill.getY(), false);
    g.setGradientFill(fillGrad);
    g.fillRoundedRectangle(fill, radius);

    // outline + subtle top gloss
    g.setColour(juce::Colours::white.withAlpha(outlineAlpha));
    g.drawRoundedRectangle(bounds, radius, 1.0f);

    auto gloss = bounds.withHeight(bounds.getHeight() * 0.20f);
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillRoundedRectangle(gloss, radius);
}
