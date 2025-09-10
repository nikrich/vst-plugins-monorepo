#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace Style {

enum class Variant { Dark, Light };

struct Theme {
    // Base surfaces
    juce::Colour bg        { 0xFF192033 }; // window background
    juce::Colour panel     { 0xFF141821 }; // panels/cards

    // Text
    juce::Colour text      { juce::Colours::white.withAlpha(0.92f) };
    juce::Colour textMuted { juce::Colours::white.withAlpha(0.65f) };

    // Tracks / fills (vertical sliders/meters)
    juce::Colour trackTop  { 0xFF1B1F27 };
    juce::Colour trackBot  { 0xFF141821 };
    juce::Colour fillTop   { 0xFFFF3B3B }; // red
    juce::Colour fillBot   { 0xFFFFB34D }; // orange

    // Accents / brand
    juce::Colour accent1   { 0xFFFF3B3B };
    juce::Colour accent2   { 0xFFFFB34D };

    float outlineAlpha = 0.20f; // subtle outlines
};

// Global theme accessors
Theme& theme();
Variant currentVariant();
void setVariant(Variant v);
void setAccent(const juce::Colour primary, const juce::Colour secondary);

} // namespace Style

