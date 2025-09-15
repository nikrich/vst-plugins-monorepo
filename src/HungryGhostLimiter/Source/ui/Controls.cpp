#include "Controls.h"

LabelledVSlider::LabelledVSlider(const juce::String& title)
{
    label.setText(title, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setInterceptsMouseClicks(false, false);
    label.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));

    slider.setSliderStyle(juce::Slider::LinearBarVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 22);
    slider.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(210, 210, 210));

    addAndMakeVisible(label);
    addAndMakeVisible(slider);
}

void LabelledVSlider::resized()
{
    auto a = getLocalBounds();
    label.setBounds(a.removeFromTop(20));
    slider.setBounds(a);
}

void LabelledVSlider::setSliderLookAndFeel(juce::LookAndFeel* lnf)
{
	slider.setLookAndFeel(lnf);
}
