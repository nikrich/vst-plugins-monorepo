#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/Theme.h>

namespace ui { namespace controls {

// VerticalMeter: simple vertical meter with attack/release smoothing, theme colours
class VerticalMeter : public juce::Component, private juce::Timer {
public:
    VerticalMeter()
    {
        setSmoothing(30.0f, 160.0f);
        startTimerHz(30);
    }

    void setSmoothing(float attackMs, float releaseMs)
    {
        atkMs = juce::jmax(1.0f, attackMs);
        relMs = juce::jmax(1.0f, releaseMs);
    }

    // Feed dB value (e.g., -60..0 dBFS). Will be clamped into [-60,0]
    void setDb(float db)
    {
        targetDb = juce::jlimit(-60.0f, 0.0f, db);
    }

    void resized() override { }

    void paint(juce::Graphics& g) override
    {
        auto& th = ::Style::theme();
        auto bf = getLocalBounds().reduced(6).toFloat();
        const float radius = th.borderRadius;

        // Track
        juce::ColourGradient trackGrad(th.trackTop, bf.getX(), bf.getY(), th.trackBot, bf.getX(), bf.getBottom(), false);
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(bf, radius);

        // Fill (-60..0 dBFS -> 0..1)
        const float norm = juce::jlimit(0.0f, 1.0f, (dispDb + 60.0f) / 60.0f);
        if (norm > 0.001f)
        {
            auto fill = bf.withY(bf.getBottom() - bf.getHeight() * norm)
                          .withHeight(bf.getHeight() * norm);
            // Simple green->red gradient for the meter
            juce::ColourGradient fg(juce::Colours::limegreen, fill.getX(), fill.getBottom(), juce::Colours::red, fill.getX(), fill.getY(), false);
            g.setGradientFill(fg);
            g.fillRect(fill);
        }
    }

private:
    void timerCallback() override
    {
        constexpr float dtMs = 1000.0f / 30.0f;
        const bool rising = (targetDb > dispDb);
        const float tau = rising ? atkMs : relMs;
        const float alpha = 1.0f - std::exp(-dtMs / juce::jmax(1.0f, tau));
        dispDb += alpha * (targetDb - dispDb);
        repaint();
    }

    float targetDb { -60.0f };
    float dispDb   { -60.0f };
    float atkMs { 40.0f }, relMs { 160.0f };
};

} } // namespace ui::controls

