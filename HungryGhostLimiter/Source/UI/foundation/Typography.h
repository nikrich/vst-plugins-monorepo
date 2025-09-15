#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../../styling/Theme.h"

namespace ui { namespace foundation {

struct Typography {
    enum class Style { Title, Subtitle, Body, Caption };

    static void apply(juce::Label& label,
                      Style style,
                      juce::Colour colour = juce::Colour(),
                      juce::Justification just = juce::Justification::centred);
};

} } // namespace ui::foundation
