#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "BinaryData.h"

// Top header that displays the main logo (centred) and a brand image on the right
class LogoHeader : public juce::Component {
public:
    LogoHeader()
    {
        // Attempt to load new right-aligned brand image from assets/logo-img.png
        auto tryNamed = [](const char* name) -> juce::Image
        {
            int sz = 0; if (const void* data = BinaryData::getNamedResource(name, sz))
                return juce::ImageFileFormat::loadFrom(data, (size_t) sz);
            return {};
        };
        juce::Image rightImg;
        rightImg = tryNamed("logo_img_png");
        if (!rightImg.isValid()) rightImg = tryNamed("logoimg_png");
        if (!rightImg.isValid()) rightImg = tryNamed("logo-img_png");
        if (!rightImg.isValid()) rightImg = tryNamed("logo-img.png");

        if (rightImg.isValid())
        {
            rightLogo.setImage(rightImg, juce::RectanglePlacement::centred);
            rightLogo.setInterceptsMouseClicks(false, false);
            addAndMakeVisible(rightLogo);
        }

        // logoSub.setText("LIMITER", juce::dontSendNotification);
        // logoSub.setJustificationType(juce::Justification::centred);
        // logoSub.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        // logoSub.setFont(juce::Font(juce::FontOptions(13.0f)));
        // logoSub.setInterceptsMouseClicks(false, false);
        // addAndMakeVisible(logoSub);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        const int h = bounds.getHeight();

        // Reserve a square area on the right for the new brand image
        auto rightArea = bounds.removeFromRight(h).reduced(6);

        // Centre the main logo in the remaining bounds
        int logoH = bounds.getHeight() - 20;
        int logoW = 320;
        if (auto img = logoComp.getImage(); img.isValid())
            logoW = (int) std::round(img.getWidth() * (logoH / (double) img.getHeight()));

        auto logoBounds = bounds.withSizeKeepingCentre(logoW, logoH);
        logoComp.setBounds(logoBounds);
        logoSub.setBounds(logoBounds.withY(logoBounds.getBottom() - 16).withHeight(16));

        // Layout the right brand image (if present)
        if (rightLogo.getImage().isValid())
            rightLogo.setBounds(rightArea);
    }

private:
    juce::ImageComponent logoComp;     // centred logo
    juce::ImageComponent rightLogo;    // right-aligned brand image (logo-img.png)
    juce::Label          logoSub;
};

