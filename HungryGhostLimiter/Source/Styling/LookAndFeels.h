#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

// Simple app-wide tweaks
struct VibeLNF : juce::LookAndFeel_V4
{
    VibeLNF();
};

// Pill-shaped vertical slider skin (for Slider::LinearBarVertical)
struct PillVSliderLNF : juce::LookAndFeel_V4
{
    juce::Colour trackBgTop{ 0xFF1B1F27 }, trackBgBot{ 0xFF141821 };
    juce::Colour fillBottom{ 0xFFFFB34D }, fillTop{ 0xFFFF3B3B };
    float outlineAlpha{ 0.20f };

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float minPos, float maxPos,
        const juce::Slider::SliderStyle style,
        juce::Slider& s) override;
};
