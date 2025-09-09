#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

class AttenMeter : public juce::Component
{
public:
    explicit AttenMeter(const juce::String& title); // title currently unused

    void setDb(float db);

    // NEW: styling hooks
    void setBarWidth(int px) { barWidth = juce::jmax(4, px); repaint(); }
    void setTopDown(bool b) { topDown = b; repaint(); }              // true = fill from top downward
    void setShowTicks(bool b) { showTicks = b; repaint(); }
    void setTrackColours(juce::Colour top, juce::Colour bot) { trackTop = top; trackBot = bot; repaint(); }
    void setFillColours(juce::Colour top, juce::Colour bot) { fillTop = top; fillBot = bot; repaint(); }

    void paint(juce::Graphics& g) override;

private:
    float dbAtten = 0.0f;       // 0..12 dB
    int   barWidth = 14;        // px, visual thickness of the pill
    bool  topDown = true;      // reversed direction (top -> bottom)
    bool  showTicks = true;

    // colours (defaults match your vibe)
    juce::Colour trackTop{ 0xFF1B1F27 }, trackBot{ 0xFF141821 };
    juce::Colour fillTop{ 0xFFFF3B3B }, fillBot{ 0xFFFFB34D };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AttenMeter)
};
