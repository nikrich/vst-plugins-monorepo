#include "Meter.h"
#include <cmath>

AttenMeter::AttenMeter(const juce::String&)
{
    // default colours from theme
    auto& th = Style::theme();
    setTrackColours(th.trackTop, th.trackBot);
    setFillColours(th.fillTop, th.fillBot);
    startTimerHz(60); // animate smoothing ~60 FPS
}

void AttenMeter::setDb(float db)
{
    targetDb = juce::jlimit(0.0f, 12.0f, db);
}

void AttenMeter::timerCallback()
{
    constexpr float dtMs = 1000.0f / 60.0f;
    const bool rising = (targetDb > displayDb);
    const float tauMs = rising ? attackTimeMs : releaseTimeMs;

    const float alpha = 1.0f - std::exp(-dtMs / juce::jmax(1.0f, tauMs));
    const float old = displayDb;
    displayDb += alpha * (targetDb - displayDb);
    displayDb = juce::jlimit(0.0f, 12.0f, displayDb);

    if (std::abs(displayDb - old) > 0.01f)
        repaint();
}

void AttenMeter::paint(juce::Graphics& g)
{
    auto outer = getLocalBounds();

    // optional background; comment out if parent paints it
    // g.fillAll (juce::Colour (0xFF192033));

    // ---------- layout ----------
    const int padLR = 6;
    auto content = outer.reduced(padLR);

    // bar centred in content
    const int wPx = juce::jlimit(4, content.getWidth(), barWidth);
    juce::Rectangle<int> bar(content.getCentreX() - wPx / 2, content.getY(), wPx, content.getHeight());

    // keep rounded caps fully visible (avoid clip)
    const int capPad = (int)std::ceil(wPx * 0.5f) + 1;
    bar = bar.reduced(0, capPad);

    // this is the Y-range used for ticks/labels too
    auto tickMapArea = bar;

    // ---------- track ----------
    auto barF = bar.toFloat();
    const float radius = barF.getWidth() * 0.5f;

    juce::ColourGradient trackGrad(trackTop, barF.getX(), barF.getY(),
        trackBot, barF.getX(), barF.getBottom(), false);
    g.setGradientFill(trackGrad);
    g.fillRoundedRectangle(barF, radius);

    // ---------- fill (smoothed, clipped to capsule) ----------
    const float frac = juce::jlimit(0.0f, 1.0f, displayDb / 12.0f);

     juce::Graphics::ScopedSaveState scoped (g);        // <— limited to this block
    juce::Path clipCapsule; 
    clipCapsule.addRoundedRectangle (barF, radius);
    g.reduceClipRegion (clipCapsule);

    juce::Rectangle<float> fillRect = barF;
    if (topDown)  fillRect.setHeight (barF.getHeight() * frac);
    else          fillRect.removeFromTop (barF.getHeight() * (1.0f - frac));

    juce::ColourGradient fillGrad (fillBot, fillRect.getX(), fillRect.getBottom(),
                                   fillTop, fillRect.getX(), fillRect.getY(), false);
    g.setGradientFill (fillGrad);
    g.fillRect (fillRect);

    // ---------- ticks + labels snug to bar ----------
    if (showTicks)
    {
        g.setFont(12.0f);
        g.setColour(juce::Colours::white.withAlpha(0.65f));

        const int gap = 6;
        const int tickLen = 10;
        const int labelW = 18;

        const int rightSpace = content.getRight() - bar.getRight();
        const bool placeRight = rightSpace >= (gap + tickLen + labelW);

        int x0, x1;
        juce::Rectangle<int> labelArea;

        if (placeRight)
        {
            x0 = bar.getRight() + gap;
            x1 = x0 + tickLen;
            labelArea = { x1, tickMapArea.getY(), labelW, tickMapArea.getHeight() };
        }
        else
        {
            x1 = bar.getX() - gap;           // end of tick (near bar)
            x0 = x1 - tickLen;               // start of tick
            labelArea = { x0 - labelW, tickMapArea.getY(), labelW, tickMapArea.getHeight() };
        }

        auto drawTick = [&](int dB)
            {
                const float y = juce::jmap((float)dB, 0.0f, 12.0f,
                    (float)tickMapArea.getBottom(), (float)tickMapArea.getY());

                g.drawHorizontalLine((int)std::round(y), (float)x0, (float)x1);

                g.drawText(juce::String(dB),
                    labelArea.withY((int)(y - 8)).withHeight(16),
                    placeRight ? juce::Justification::centredLeft
                    : juce::Justification::centredRight,
                    false);
            };

        for (int d : { 0, 1, 2, 3, 6, 9, 12 }) drawTick(d);
    }
}
