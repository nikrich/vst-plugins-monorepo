#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/LookAndFeels.h>

class StyledKnob : public juce::Component
{
public:
    StyledKnob(double min, double max, double step, double defaultValue,
               const juce::String& suffix = {})
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
        slider.setRange(min, max, step);
        slider.setValue(defaultValue, juce::dontSendNotification);
        slider.setDoubleClickReturnValue(true, defaultValue);
        slider.setNumDecimalPlacesToDisplay(step >= 1.0 ? 0 : 2);
        slider.setLookAndFeel(&donutLNF);
        if (suffix.isNotEmpty())
            slider.setTextValueSuffix(suffix);
        addAndMakeVisible(slider);
    }

    ~StyledKnob() override { slider.setLookAndFeel(nullptr); }

    juce::Slider& getSlider() noexcept { return slider; }
    void resized() override { slider.setBounds(getLocalBounds()); }

private:
    DonutKnobLNF donutLNF;
    juce::Slider slider;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StyledKnob)
};
