#include "Theme.h"

namespace Style {

static Theme current;
static Variant variant = Variant::Dark;

static void applyDark()
{
    // Map from CSS:
    // --bg:#121315; --panel:#1c1d20; --text:#e9eef5; --muted:#9aa3ad;
    // --track: top #2b2e35 -> bottom #202228
    // --grad-orange: bottom #ff4d1f -> top #ffad33
    // --aqua ring: #35ffdf -> #0097a7
    current.bg        = juce::Colour(0xFF121315);
    current.panel     = juce::Colour(0xFF1C1D20);
    current.text      = juce::Colour(0xFFE9EEF5);
    current.textMuted = juce::Colour(0xFF9AA3AD);
    current.trackTop  = juce::Colour(0xFF2B2E35);
    current.trackBot  = juce::Colour(0xFF202228);
    current.fillTop   = juce::Colour(0xFFFFAD33);
    current.fillBot   = juce::Colour(0xFFFF4D1F);
    current.accent1   = juce::Colour(0xFF35FFDF);
    current.accent2   = juce::Colour(0xFF0097A7);
    current.outlineAlpha = 0.16f;
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

