#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../../styling/Theme.h"

namespace ui { namespace foundation {

class Card : public juce::Component {
public:
    void setCornerRadius(float r) { corner = r; repaint(); }
    void setBorder(juce::Colour c, float thickness) { borderColour = c; borderWidth = thickness; repaint(); }
    void setBackground(juce::Colour c) { bgColour = c; repaint(); }
    void setDropShadow(bool enabled) { shadowEnabled = enabled; repaint(); }

    void paint(juce::Graphics& g) override;

private:
    float corner { Style::theme().borderRadius };
    float borderWidth { Style::theme().borderWidth };
    juce::Colour borderColour { juce::Colours::white.withAlpha(0.12f) };
    juce::Colour bgColour { juce::Colour(0xFF301935) }; // matches existing card fill in editor fallback
    bool shadowEnabled { true };
};

} } // namespace ui::foundation
