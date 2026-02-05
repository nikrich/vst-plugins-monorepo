#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/Theme.h>

namespace ui { namespace controls {

// VerticalMeter: vertical meter with attack/release smoothing, theme colours, and optional dB labels/markers
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

    // If true, the meter fills from top to bottom. If false, bottom to top.
    void setTopDown(bool b) { topDown = b; repaint(); }

    // Show/hide dB scale ticks and labels
    void setShowTicks(bool b) { showTicks = b; repaint(); }

    // Customize tick appearance (gap from bar, tick line length, label width)
    void setTickAppearance(int gapPx, int tickLenPx, int labelWidthPx)
    {
        gap = juce::jmax(2, gapPx);
        tickLen = juce::jmax(4, tickLenPx);
        labelWidth = juce::jmax(16, labelWidthPx);
        repaint();
    }

    void resized() override { }

    void paint(juce::Graphics& g) override
    {
        auto& th = ::Style::theme();
        auto outer = getLocalBounds();
        const int padLR = 6;
        auto content = outer.reduced(padLR);

        // Bar centered in content
        const int barW = juce::jlimit(4, content.getWidth(), 12);
        juce::Rectangle<int> bar(content.getCentreX() - barW / 2, content.getY(), barW, content.getHeight());

        // Keep rounded caps fully visible
        const int capPad = (int)std::ceil(barW * 0.5f) + 1;
        bar = bar.reduced(0, capPad);
        auto tickMapArea = bar;

        // --- Track ---
        auto bf = bar.toFloat();
        const float radius = th.borderRadius;

        juce::ColourGradient trackGrad(th.trackTop, bf.getX(), bf.getY(), th.trackBot, bf.getX(), bf.getBottom(), false);
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(bf, radius);

        // --- Fill (-60..0 dBFS -> 0..1) ---
        const float norm = juce::jlimit(0.0f, 1.0f, (dispDb + 60.0f) / 60.0f);
        if (norm > 0.001f)
        {
            juce::Graphics::ScopedSaveState scoped(g);
            juce::Path clipCapsule;
            clipCapsule.addRoundedRectangle(bf, radius);
            g.reduceClipRegion(clipCapsule);

            juce::Rectangle<float> fill = bf;
            if (topDown)
                fill.setHeight(bf.getHeight() * norm);
            else
                fill.removeFromTop(bf.getHeight() * (1.0f - norm));

            // Green->red gradient for the meter
            juce::ColourGradient fg(juce::Colours::limegreen, fill.getX(), fill.getBottom(),
                                   juce::Colours::red, fill.getX(), fill.getY(), false);
            g.setGradientFill(fg);
            g.fillRect(fill);
        }

        // --- Ticks + Labels ---
        if (showTicks)
        {
            g.setFont(11.0f);
            g.setColour(th.textMuted);

            const int rightSpace = content.getRight() - bar.getRight();
            const bool placeRight = rightSpace >= (gap + tickLen + labelWidth);

            int x0, x1;
            juce::Rectangle<int> labelArea;

            if (placeRight)
            {
                x0 = bar.getRight() + gap;
                x1 = x0 + tickLen;
                labelArea = { x1, tickMapArea.getY(), labelWidth, tickMapArea.getHeight() };
            }
            else
            {
                x1 = bar.getX() - gap;
                x0 = x1 - tickLen;
                labelArea = { x0 - labelWidth, tickMapArea.getY(), labelWidth, tickMapArea.getHeight() };
            }

            auto drawTick = [&](int dB) {
                const float y = juce::jmap((float)dB, -60.0f, 0.0f,
                    (float)tickMapArea.getBottom(), (float)tickMapArea.getY());

                g.drawHorizontalLine((int)std::round(y), (float)x0, (float)x1);

                g.drawText(juce::String(dB),
                    labelArea.withY((int)(y - 5)).withHeight(12),
                    placeRight ? juce::Justification::centredLeft : juce::Justification::centredRight,
                    false);
            };

            // Draw ticks at -60, -50, -40, -30, -20, -10, 0 dB
            for (int d : { -60, -50, -40, -30, -20, -10, 0 }) drawTick(d);
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

    bool  topDown { false };
    bool  showTicks { false };
    float targetDb { -60.0f };
    float dispDb   { -60.0f };
    float atkMs { 40.0f }, relMs { 160.0f };
    int gap { 6 };
    int tickLen { 10 };
    int labelWidth { 18 };
};

} } // namespace ui::controls
