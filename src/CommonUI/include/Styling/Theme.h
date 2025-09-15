#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace Style {

enum class Variant { Dark, Light };

struct Theme {
    float borderRadius = 3.0f;
    float borderWidth  = 1.0f;
    juce::Colour bg        { 0xFF121315 };
    juce::Colour panel     { 0xFF1c1d20 };
    juce::Colour text      { 0xFFE9EEF5 };
    juce::Colour textMuted { 0xFF9AA3AD };
    juce::Colour trackTop  { 0xFF2B2E35 };
    juce::Colour trackBot  { 0xFF202228 };
    juce::Colour fillTop   { 0xFFFFAD33 };
    juce::Colour fillBot   { 0xFFFF4D1F };
    juce::Colour accent1   { 0xFF35FFDF };
    juce::Colour accent2   { 0xFF0097A7 };
    float outlineAlpha = 0.16f;
};

inline Theme& theme()
{
    static Theme current{};
    return current;
}

inline Variant& variantRef()
{
    static Variant v = Variant::Dark;
    return v;
}

inline Variant currentVariant() { return variantRef(); }

inline void setVariant(Variant v)
{
    variantRef() = v;
    auto& cur = theme();
    if (v == Variant::Dark)
    {
        cur.bg        = juce::Colour(0xFF121315);
        cur.panel     = juce::Colour(0xFF1C1D20);
        cur.text      = juce::Colour(0xFFE9EEF5);
        cur.textMuted = juce::Colour(0xFF9AA3AD);
        cur.trackTop  = juce::Colour(0xFF2B2E35);
        cur.trackBot  = juce::Colour(0xFF202228);
        cur.fillTop   = juce::Colour(0xFFFFAD33);
        cur.fillBot   = juce::Colour(0xFFFF4D1F);
        cur.accent1   = juce::Colour(0xFF35FFDF);
        cur.accent2   = juce::Colour(0xFF0097A7);
        cur.outlineAlpha = 0.16f;
    }
    else
    {
        cur.bg        = juce::Colour(0xFFF3F5F8);
        cur.panel     = juce::Colour(0xFFE7EBF2);
        cur.text      = juce::Colour(0xFF1A1E26);
        cur.textMuted = juce::Colour(0x991A1E26);
        cur.trackTop  = juce::Colour(0xFFE0E6EF);
        cur.trackBot  = juce::Colour(0xFFD3DAE6);
        cur.fillTop   = juce::Colour(0xFF1A73E8);
        cur.fillBot   = juce::Colour(0xFF66A6FF);
        cur.accent1   = cur.fillTop;
        cur.accent2   = cur.fillBot;
        cur.outlineAlpha = 0.18f;
    }
}

inline void setAccent(const juce::Colour primary, const juce::Colour secondary)
{
    auto& cur = theme();
    cur.accent1 = primary;
    cur.accent2 = secondary;
    cur.fillTop = primary;
    cur.fillBot = secondary;
}

} // namespace Style

