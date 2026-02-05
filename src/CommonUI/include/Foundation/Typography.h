#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/Theme.h>

namespace ui { namespace foundation {

struct Typography {
    enum class Style { Title, Subtitle, Body, Caption };

    static void apply(juce::Label& label,
                      Style style,
                      juce::Colour colour = juce::Colour(),
                      juce::Justification just = juce::Justification::centred)
    {
        auto& th = ::Style::theme();
        if (! colour.isTransparent())
            label.setColour(juce::Label::textColourId, colour);
        else
            label.setColour(juce::Label::textColourId, th.text);

        label.setJustificationType(just);
        label.setInterceptsMouseClicks(false, false);
        label.setEditable(false, false, false);

        auto fontFor = [](Style s){
            switch (s)
            {
                case Style::Title:    return juce::Font(juce::FontOptions(14.0f, juce::Font::bold));
                case Style::Subtitle: return juce::Font(juce::FontOptions(12.0f, juce::Font::bold));
                case Style::Body:     return juce::Font(juce::FontOptions(12.0f, juce::Font::plain));
                case Style::Caption:  return juce::Font(juce::FontOptions(11.0f, juce::Font::plain));
            }
            return juce::Font(juce::FontOptions(12.0f));
        };
        label.setFont(fontFor(style));
    }
};

} } // namespace ui::foundation

