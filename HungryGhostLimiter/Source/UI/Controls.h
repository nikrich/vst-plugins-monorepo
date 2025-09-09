#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

class LabelledVSlider : public juce::Component
{
public:
    explicit LabelledVSlider(const juce::String& title);

    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    juce::Label  label;
    juce::Slider slider;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabelledVSlider)
};
