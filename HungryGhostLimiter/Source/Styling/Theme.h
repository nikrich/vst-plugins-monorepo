#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace Style {

enum class Variant { Dark, Light };

struct Theme {
    // Base surfaces
    juce::Colour bg        { 0xFF121315 }; // window background
    juce::Colour panel     { 0xFF1c1d20 }; // panels/cards

    // Text
    juce::Colour text      { 0xFFE9EEF5 }; // primary text
    juce::Colour textMuted { 0xFF9AA3AD }; // muted text / ticks

    // Tracks / fills (vertical sliders/meters)
    juce::Colour trackTop  { 0xFF2B2E35 }; // top of track
    juce::Colour trackBot  { 0xFF202228 }; // bottom of track
    juce::Colour fillTop   { 0xFFFFAD33 }; // orange top
    juce::Colour fillBot   { 0xFFFF4D1F }; // orange bottom

    // Accents / brand (aqua ring for knobs)
    juce::Colour accent1   { 0xFF35FFDF }; // aqua
    juce::Colour accent2   { 0xFF0097A7 }; // teal

    float outlineAlpha = 0.16f; // subtle outlines
};

// Global theme accessors
Theme& theme();
Variant currentVariant();
void setVariant(Variant v);
void setAccent(const juce::Colour primary, const juce::Colour secondary);

} // namespace Style

