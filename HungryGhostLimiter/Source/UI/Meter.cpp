#include "Meter.h"

AttenMeter::AttenMeter(const juce::String&) {}

void AttenMeter::setDb(float db)
{
    dbAtten = juce::jlimit(0.0f, 12.0f, db);
}

void AttenMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(30, 30, 30));
    g.fillRoundedRectangle(r, 4.0f);

    const float frac = dbAtten / 12.0f;
    auto fill = r.removeFromBottom(r.getHeight() * frac);

    juce::ColourGradient grad(juce::Colour::fromRGB(245, 210, 60), fill.getBottomLeft(),
        juce::Colour::fromRGB(220, 40, 30), fill.getTopLeft(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fill, 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 4.0f, 1.0f);

    // ticks: 0,1,2,3,6,9,12
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    auto b = getLocalBounds();
    auto labelArea = b.withX(b.getRight() - 22).withWidth(22);

    auto drawTick = [&](int dB)
        {
            const float y = juce::jmap((float)dB, 0.0f, 12.0f, (float)b.getBottom(), (float)b.getY());
            g.drawHorizontalLine((int)std::round(y), (float)b.getX(), (float)b.getRight());
            g.drawFittedText(juce::String(dB), labelArea.withY((int)(y - 8)).withHeight(16),
                juce::Justification::centredRight, 1);
        };
    for (int d : { 0, 1, 2, 3, 6, 9, 12 }) drawTick(d);
}
