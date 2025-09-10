#include "Theme.h"

namespace Style {

static Theme current;
static Variant variant = Variant::Dark;

static void applyDark()
{
    current.bg        = juce::Colour(0xFF192033);
    current.panel     = juce::Colour(0xFF141821);
    current.text      = juce::Colours::white.withAlpha(0.92f);
    current.textMuted = juce::Colours::white.withAlpha(0.65f);
    current.trackTop  = juce::Colour(0xFF1B1F27);
    current.trackBot  = juce::Colour(0xFF141821);
    current.fillTop   = juce::Colour(0xFFFF3B3B);
    current.fillBot   = juce::Colour(0xFFFFB34D);
    current.accent1   = current.fillTop;
    current.accent2   = current.fillBot;
    current.outlineAlpha = 0.20f;
}

static void applyLight()
{
    current.bg        = juce::Colour(0xFFF3F5F8);
    current.panel     = juce::Colour(0xFFE7EBF2);
    current.text      = juce::Colour(0xFF1A1E26);
    current.textMuted = juce::Colour(0x991A1E26);
    current.trackTop  = juce::Colour(0xFFE0E6EF);
    current.trackBot  = juce::Colour(0xFFD3DAE6);
    current.fillTop   = juce::Colour(0xFF1A73E8); // blue
    current.fillBot   = juce::Colour(0xFF66A6FF);
    current.accent1   = current.fillTop;
    current.accent2   = current.fillBot;
    current.outlineAlpha = 0.18f;
}

Theme& theme() { return current; }

Variant currentVariant() { return variant; }

void setVariant(Variant v)
{
    variant = v;
    if (variant == Variant::Dark) applyDark(); else applyLight();
}

void setAccent(const juce::Colour primary, const juce::Colour secondary)
{
    current.accent1 = primary;
    current.accent2 = secondary;
    current.fillTop = primary;
    current.fillBot = secondary;
}

} // namespace Style

