#pragma once
#include <JuceHeader.h>
#include <Styling/Theme.h>

/** GRMeter: Gain Reduction meter with red-orange gradient visualization
    Displays dB of reduction (0-12 dB range typical for limiters)
    Features attack/release smoothing for visual feedback */
class GRMeter : public juce::Component, private juce::Timer
{
public:
    GRMeter() { setSmoothing(40.0f, 140.0f); }

    /** Feed dB value (0-12 dB range, will be clamped) */
    void setGRdB(float db)
    {
        targetDb = juce::jlimit(0.0f, 12.0f, db);
        if (!isTimerRunning()) startTimerHz(30);
    }

    /** Configure attack/release smoothing times */
    void setSmoothing(float attackMs, float releaseMs)
    {
        atkMs = juce::jmax(1.0f, attackMs);
        relMs = juce::jmax(1.0f, releaseMs);
    }

    void paint(juce::Graphics& g) override
    {
        auto& th = ::Style::theme();
        auto bounds = getLocalBounds().toFloat();

        // Track background (dark gradient)
        juce::ColourGradient trackGrad(th.trackTop, bounds.getX(), bounds.getY(),
                                       th.trackBot, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(bounds, 3.0f);

        // GR fill (red-orange gradient, bottom-up)
        const float normalizedGR = juce::jlimit(0.0f, 1.0f, dispDb / 12.0f);
        if (normalizedGR > 0.001f)
        {
            juce::Graphics::ScopedSaveState scoped(g);
            juce::Path clipPath;
            clipPath.addRoundedRectangle(bounds, 3.0f);
            g.reduceClipRegion(clipPath);

            juce::ColourGradient fillGrad(juce::Colour(0xFFFF6B35),  // Orange
                                          bounds.getX(), bounds.getY(),
                                          juce::Colour(0xFFCC3300),   // Red
                                          bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill(fillGrad);

            juce::Rectangle<float> fill = bounds;
            fill.removeFromTop(bounds.getHeight() * (1.0f - normalizedGR));
            g.fillRect(fill);
        }

        // Reference line at 0 dB (top)
        g.setColour(juce::Colour(0xFF666666).withAlpha(0.3f));
        g.drawHorizontalLine((int)bounds.getY(), bounds.getX(), bounds.getRight());
    }

    void resized() override {}

private:
    void timerCallback() override
    {
        constexpr float dtMs = 1000.0f / 30.0f;
        const bool rising = (targetDb > dispDb);
        const float tau = rising ? atkMs : relMs;
        const float alpha = 1.0f - std::exp(-dtMs / juce::jmax(1.0f, tau));
        dispDb += alpha * (targetDb - dispDb);

        if (std::abs(targetDb - dispDb) < 0.001f) stopTimer();
        repaint();
    }

    float targetDb{0.0f}, dispDb{0.0f};
    float atkMs{40.0f}, relMs{140.0f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GRMeter)
};
