#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>

namespace ui { namespace controls {

// LogoHeader: centred logo with optional right-aligned brand image
class LogoHeader : public juce::Component {
public:
    LogoHeader()
    {
        // Try to load a default brand image if present
        setRightImageByNames({ "logo_img_png", "logoimg_png", "logo-img_png", "logo-img.png" });
    }

    bool setRightImageByNames(std::initializer_list<const char*> candidates)
    {
        for (auto* c : candidates)
        {
            int sz = 0; if (const void* data = BinaryData::getNamedResource(c, sz))
            {
                right.setImage(juce::ImageFileFormat::loadFrom(data, (size_t)sz), juce::RectanglePlacement::centred);
                right.setInterceptsMouseClicks(false, false);
                addAndMakeVisible(right);
                return true;
            }
        }
        return false;
    }

    void setMainImage(juce::Image img) { main.setImage(std::move(img), juce::RectanglePlacement::centred); addAndMakeVisible(main); }

    void resized() override
    {
        auto bounds = getLocalBounds();
        const int h = bounds.getHeight();
        auto rightArea = bounds.removeFromRight(h).reduced(6);
        int logoH = bounds.getHeight() - 20;
        int logoW = 320;
        if (auto img = main.getImage(); img.isValid())
            logoW = (int) std::round(img.getWidth() * (logoH / (double) img.getHeight()));
        auto logoBounds = bounds.withSizeKeepingCentre(logoW, logoH);
        main.setBounds(logoBounds);
        if (right.getImage().isValid()) right.setBounds(rightArea);
    }

private:
    juce::ImageComponent main, right;
};

} } // namespace ui::controls

