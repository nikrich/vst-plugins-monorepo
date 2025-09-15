#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/Theme.h>

namespace ui { namespace foundation {

// Header-only themed card component
class Card : public juce::Component {
public:
    void setCornerRadius(float r) { corner = r; repaint(); }
    void setBorder(juce::Colour c, float thickness) { borderColour = c; borderWidth = thickness; repaint(); }
    void setBackground(juce::Colour c) { bgColour = c; repaint(); }
    void setDropShadow(bool enabled) { shadowEnabled = enabled; repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (shadowEnabled)
        {
            juce::DropShadow ds(juce::Colours::black.withAlpha(0.55f), 22, {});
            ds.drawForRectangle(g, r.toNearestInt());
        }
        g.setColour(bgColour);
        g.fillRoundedRectangle(r, corner);
        if (borderWidth > 0.0f)
        {
            g.setColour(borderColour);
            g.drawRoundedRectangle(r, corner, borderWidth);
        }
    }

private:
    float corner { ::Style::theme().borderRadius };
    float borderWidth { ::Style::theme().borderWidth };
    juce::Colour borderColour { juce::Colours::white.withAlpha(0.12f) };
    juce::Colour bgColour { juce::Colour(0xFF301935) };
    bool shadowEnabled { true };
};

} } // namespace ui::foundation

