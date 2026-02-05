#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../Layout.h"
#include <Foundation/Typography.h>

// Bottom full-width advanced controls container (placeholder)
class AdvancedControls : public juce::Component {
public:
    AdvancedControls() {
        title.setText("Advanced Control", juce::dontSendNotification);
        ui::foundation::Typography::apply(title,
                                         ui::foundation::Typography::Style::Subtitle,
                                         juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(title);
    }

    void resized() override {
        title.setBounds(getLocalBounds());
    }

private:
    juce::Label title;
};

