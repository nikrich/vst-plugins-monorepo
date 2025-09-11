#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "BinaryData.h"

// Top header that displays the logo and optional subtitle
class LogoHeader : public juce::Component {
public:
    LogoHeader()
    {
        int logoSize = 0;
        if (const auto* logoData = BinaryData::getNamedResource("logo_png", logoSize))
        {
            auto logoImage = juce::ImageFileFormat::loadFrom(logoData, (size_t) logoSize);
            logoComp.setImage(logoImage, juce::RectanglePlacement::centred);
        }
        logoComp.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(logoComp);

        logoSub.setText("LIMITER", juce::dontSendNotification);
        logoSub.setJustificationType(juce::Justification::centred);
        logoSub.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        logoSub.setFont(juce::Font(juce::FontOptions(13.0f)));
        logoSub.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(logoSub);
    }

    void resized() override
    {
        auto header = getLocalBounds();
        int logoH = header.getHeight() - 20;
        int logoW = 320;
        if (auto img = logoComp.getImage(); img.isValid())
            logoW = (int)std::round(img.getWidth() * (logoH / (double)img.getHeight()));

        auto logoBounds = header.withSizeKeepingCentre(logoW, logoH);
        logoComp.setBounds(logoBounds);
        logoSub.setBounds(logoBounds.withY(logoBounds.getBottom() - 16).withHeight(16));
    }

private:
    juce::ImageComponent logoComp;
    juce::Label          logoSub;
};

