#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Styling/Theme.h"

namespace ui { namespace foundation {

class CardImpl;
class Card : public juce::Component {
public:
    void setCornerRadius(float r);
    void setBorder(juce::Colour c, float thickness);
    void setBackground(juce::Colour c);
    void setDropShadow(bool enabled);

    void paint(juce::Graphics& g) override;
private:
    std::shared_ptr<CardImpl> p;
};

} } // namespace ui::foundation

