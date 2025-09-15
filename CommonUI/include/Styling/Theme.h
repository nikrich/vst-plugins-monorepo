#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace Style {

// Re-export existing API so other plugins can include a stable header path.
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

Theme& theme();
Variant currentVariant();
void setVariant(Variant v);
void setAccent(const juce::Colour primary, const juce::Colour secondary);

} // namespace Style

