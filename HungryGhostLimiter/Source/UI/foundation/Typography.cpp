#include "Typography.h"
#include "ResourceResolver.h"

namespace ui { namespace foundation {

static juce::Font fontForStyle(Typography::Style s)
{
    switch (s)
    {
        case Typography::Style::Title:    return juce::Font(juce::FontOptions(14.0f, juce::Font::bold));
        case Typography::Style::Subtitle: return juce::Font(juce::FontOptions(12.0f, juce::Font::bold));
        case Typography::Style::Body:     return juce::Font(juce::FontOptions(12.0f, juce::Font::plain));
        case Typography::Style::Caption:  return juce::Font(juce::FontOptions(11.0f, juce::Font::plain));
    }
    return juce::Font(juce::FontOptions(12.0f));
}

void Typography::apply(juce::Label& label,
                       Style style,
                       juce::Colour colour,
                       juce::Justification just)
{
    // Colour default from theme if not provided
    auto& th = ::Style::theme();
    if (! colour.isTransparent())
        label.setColour(juce::Label::textColourId, colour);
    else
        label.setColour(juce::Label::textColourId, th.text);

    label.setJustificationType(just);
    label.setInterceptsMouseClicks(false, false);
    label.setEditable(false, false, false);

    // Apply font; if default LNF has a custom typeface, JUCE will substitute it.
    label.setFont(fontForStyle(style));
}

} } // namespace ui::foundation
