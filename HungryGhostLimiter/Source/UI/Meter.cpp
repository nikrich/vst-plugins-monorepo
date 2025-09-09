#include "Meter.h"

AttenMeter::AttenMeter(const juce::String&) {}

void AttenMeter::setDb(float db)
{
    dbAtten = juce::jlimit(0.0f, 12.0f, db);
}

void AttenMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Background (no hard border)
    g.setColour(juce::Colour(0xFF192033)); // or your editor background; optional.
    g.fillRect(bounds);

    // Build a thin, centered pill area
    auto bar = bounds.reduced(6.0f);                // overall padding inside the component
    const float w = (float)juce::jmin(barWidth, (int)bar.getWidth());
    bar = juce::Rectangle<float>(bar.getCentreX() - w * 0.5f, bar.getY(), w, bar.getHeight());
    const float radius = w * 0.5f;

    // Track (dark gradient)
    juce::ColourGradient trackGrad(trackTop, bar.getX(), bar.getY(),
        trackBot, bar.getX(), bar.getBottom(), false);
    g.setGradientFill(trackGrad);
    g.fillRoundedRectangle(bar, radius);

    // Fill amount (0..12 dB -> 0..100%)
    const float frac = dbAtten / 12.0f;
    juce::Rectangle<float> fill = bar;

    if (topDown)
    {
        fill.setHeight(bar.getHeight() * frac);  // grow downward from the top
    }
    else
    {
        // traditional bottom-up
        fill.removeFromTop(bar.getHeight() * (1.0f - frac));
    }

    juce::ColourGradient fillGrad(fillBot, fill.getX(), fill.getBottom(),
        fillTop, fill.getX(), fill.getY(), false);
    g.setGradientFill(fillGrad);
    g.fillRoundedRectangle(fill, radius);

    // Optional: tick grid on the right
    if (showTicks)
    {
        g.setFont(12.0f);
        g.setColour(juce::Colours::white.withAlpha(0.65f));

        const int gap = 6;    // distance from bar to ticks
        const int tickLen = 10;   // length of the horizontal tick
        const int labelW = 18;   // width reserved for the number

        // anchor the rail to the bar, not the component edge
        const int x0 = (int)std::round(bar.getRight()) + gap;      // start of tick lines
        const int x1 = x0 + tickLen;                                  // end of tick lines
        juce::Rectangle<int> labelArea(x1, (int)bounds.getY(),      // labels sit just right of ticks
            labelW, (int)bounds.getHeight());

        auto drawTick = [&](int dB)
            {
                const float y = juce::jmap((float)dB, 0.0f, 12.0f,
                    (float)bounds.getBottom(), (float)bounds.getY());

                // line stays between x0..x1 so it never crosses the numbers
                g.drawHorizontalLine((int)std::round(y), (float)x0, (float)x1);

                g.drawFittedText(juce::String(dB),
                    labelArea.withY((int)(y - 8)).withHeight(16),
                    juce::Justification::centredLeft, 1);
            };

        for (int d : { 0, 1, 2, 3, 6, 9, 12 }) drawTick(d);
    }
}
