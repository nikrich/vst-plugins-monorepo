#include "Controls.h"

LabelledVSlider::LabelledVSlider(const juce::String& title)
{
    label.setText(title, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setInterceptsMouseClicks(false, false);

    slider.setSliderStyle(juce::Slider::LinearBarVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(210, 210, 210));

    addAndMakeVisible(label);
    addAndMakeVisible(slider);
}

void LabelledVSlider::paintOverChildren(juce::Graphics& g)
{
    // keep the warm fill vibe for Ceiling/Release; Threshold uses Pill L&F instead
    auto r = slider.getBounds().toFloat().reduced(8.0f, 6.0f);
    r.removeFromTop(18.0f);

    juce::ColourGradient grad(juce::Colour::fromRGB(250, 212, 65), r.getBottomLeft(),
        juce::Colour::fromRGB(220, 40, 30), r.getTopLeft(), false);
    juce::FillType ft(grad);
    ft.setOpacity(0.85f);
    g.setFillType(ft);
    g.fillRoundedRectangle(r, 3.0f);

    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);
}

void LabelledVSlider::resized()
{
    auto a = getLocalBounds();
    label.setBounds(a.removeFromTop(20));
    slider.setBounds(a);
}
