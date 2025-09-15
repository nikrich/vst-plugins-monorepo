#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>
#include <Foundation/ResourceResolver.h>

// A self-contained vertical slider that renders a UI kit filmstrip (sl-final.png)
// as the entire bar (per frame), while delegating input/values to an inner Slider.
class KitStripSlider : public juce::Component
{
public:
    KitStripSlider()
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setLookAndFeel(&emptyLNF);
        addAndMakeVisible(slider);

        slider.onValueChange = [this]{ repaint(); };

        // Use ResourceResolver to locate the filmstrip robustly
        strip = ui::foundation::ResourceResolver::loadImageByNames({
            "slfinal_png",
            "sl-final.png",
            "assets/ui/kit-03/slider/sl-final.png"
        });

        if (strip.isValid())
        {
            const int w = strip.getWidth();
            const int h = strip.getHeight();
            vertical = (h > w);
            frames = 128; // kit specifies 128 frames
            if (vertical)
            {
                frameW = w;
                frameH = (frames > 0) ? h / frames : h;
            }
            else
            {
                frameH = h;
                frameW = (frames > 0) ? w / frames : w;
            }
        }
    }

    ~KitStripSlider() override { slider.setLookAndFeel(nullptr); }

    void resized() override
    {
        slider.setBounds(getLocalBounds());
    }

    void paint(juce::Graphics& g) override
    {
        if (!(strip.isValid() && frameW > 0 && frameH > 0 && frames > 1)) return;

        const auto& s = slider;
        const auto r  = getLocalBounds().toFloat();

        const auto range = s.getRange();
        const double prop = range.getLength() > 0.0 ? (s.getValue() - range.getStart()) / range.getLength() : 0.0;
        const int idx = juce::jlimit(0, frames - 1, (int)std::round(prop * (frames - 1)));
        const int sx  = vertical ? 0 : idx * frameW;
        const int sy  = vertical ? idx * frameH : 0;

        // Fit full height to match side bars; center horizontally.
        const float scale = r.getHeight() / (float)frameH;
        const float dw = frameW * scale;
        const float dh = r.getHeight();
        const float dx = r.getX() + (r.getWidth()  - dw) * 0.5f;
        const float dy = r.getY();
        g.drawImage(strip, (int)dx, (int)dy, (int)dw, (int)dh, sx, sy, frameW, frameH);
    }

    juce::Slider& getSlider() noexcept { return slider; }

private:
    // LNF that draws nothing so our filmstrip is all you see
    struct EmptySliderLNF : juce::LookAndFeel_V4
    {
        void drawLinearSlider(juce::Graphics&, int, int, int, int,
                              float, float, float,
                              const juce::Slider::SliderStyle, juce::Slider&) override {}
    } emptyLNF;

    juce::Slider slider;
    juce::Image  strip;
    bool vertical { true };
    int frameW { 0 };
    int frameH { 0 };
    int frames { 0 };
};
