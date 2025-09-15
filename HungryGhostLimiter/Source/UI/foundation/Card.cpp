#include "Card.h"

namespace ui { namespace foundation {

void Card::paint(juce::Graphics& g)
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

} } // namespace ui::foundation
