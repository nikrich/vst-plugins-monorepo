#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class StyledCombo : public juce::Component
{
public:
    StyledCombo()
    {
        combo.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(combo);
    }

    juce::ComboBox& getCombo() noexcept { return combo; }
    void resized() override { combo.setBounds(getLocalBounds()); }

private:
    juce::ComboBox combo;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StyledCombo)
};
