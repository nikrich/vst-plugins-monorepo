#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Layout.h"

// Simple rightmost column placeholder for output-related controls (to be expanded)
class OutputColumn : public juce::Component {
public:
    OutputColumn() {
        title.setText("OUTPUT", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setInterceptsMouseClicks(false, false);
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
        title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        addAndMakeVisible(title);
    }

    void resized() override {
        title.setBounds(getLocalBounds());
    }

private:
    juce::Label title;
};

