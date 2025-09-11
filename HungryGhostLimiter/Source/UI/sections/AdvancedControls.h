#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Layout.h"

// Bottom full-width advanced controls container (placeholder)
class AdvancedControls : public juce::Component {
public:
    AdvancedControls() {
        title.setText("Advanced Control", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setInterceptsMouseClicks(false, false);
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        title.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        addAndMakeVisible(title);
    }

    void resized() override {
        title.setBounds(getLocalBounds());
    }

private:
    juce::Label title;
};

