#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

class AttenMeter : public juce::Component, private juce::Timer
{
public:
    explicit AttenMeter(const juce::String& title); // title unused

    // FEED values here (0..12 dB). We smooth internally.
    void setDb(float db);

    // Smoothing (ms): attack when value rises, release when it falls.
    void setSmoothing(float attackMs, float releaseMs)
    {
        attackTimeMs = juce::jmax(1.0f, attackMs);
        releaseTimeMs = juce::jmax(1.0f, releaseMs);
    }

    // Styling hooks you already had
    void setBarWidth(int px) { barWidth = juce::jmax(4, px); repaint(); }
    void setTopDown(bool b) { topDown = b; repaint(); }       // true = top→down
    void setShowTicks(bool b) { showTicks = b; repaint(); }
    void setTrackColours(juce::Colour top, juce::Colour bot) { trackTop = top; trackBot = bot; repaint(); }
    void setFillColours(juce::Colour top, juce::Colour bot) { fillTop = top; fillBot = bot; repaint(); }

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    // --- smoothing state ---
    float targetDb = 0.0f;     // last fed value
    float displayDb = 0.0f;     // smoothed value we draw
    float attackTimeMs = 40.0f;    // faster rise
    float releaseTimeMs = 140.0f;   // slower fall

    // --- style ---
    int   barWidth = 14;
    bool  topDown = true;
    bool  showTicks = true;
    juce::Colour trackTop{ 0xFF1B1F27 }, trackBot{ 0xFF141821 };
    juce::Colour fillTop{ 0xFFFF3B3B }, fillBot{ 0xFFFFB34D };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AttenMeter)
};
