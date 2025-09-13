#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>

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

        auto tryNamed = [](const char* name) -> juce::Image
        {
            int sz = 0; if (const void* data = BinaryData::getNamedResource(name, sz))
                return juce::ImageFileFormat::loadFrom(data, (size_t)sz);
            return {};
        };
        
        strip = tryNamed("slfinal_png");
        if (!strip.isValid()) strip = tryNamed("sl-final.png");

        if (strip.isValid())
        {
            const int w = strip.getWidth();
            const int h = strip.getHeight();
            vertical = (h > w);
            frameSize = vertical ? w : h;
            if (vertical && w > 0 && h % w == 0)      frames = h / w;
            else if (!vertical && h > 0 && w % h == 0) frames = w / h;
            else frames = 128;
        }
    }

    ~KitStripSlider() override { slider.setLookAndFeel(nullptr); }

    void resized() override
    {
        slider.setBounds(getLocalBounds());
    }

    void paint(juce::Graphics& g) override
    {
        if (!(strip.isValid() && frameSize > 0 && frames > 1)) return;

        const auto& s = slider;
        const auto r  = getLocalBounds().toFloat();

        const auto range = s.getRange();
        const double prop = range.getLength() > 0.0 ? (s.getValue() - range.getStart()) / range.getLength() : 0.0;
        const int idx = juce::jlimit(0, frames - 1, (int)std::round(prop * (frames - 1)));
        const int sx  = vertical ? 0 : idx * frameSize;
        const int sy  = vertical ? idx * frameSize : 0;

        // Contain: fit inside both width and height (no cropping), centered
        const float scale = juce::jmin(r.getWidth() / (float)frameSize,
                                       r.getHeight() / (float)frameSize);
        const float dw = frameSize * scale;
        const float dh = frameSize * scale;
        const float dx = r.getX() + (r.getWidth()  - dw) * 0.5f;
        const float dy = r.getY() + (r.getHeight() - dh) * 0.5f;
        g.drawImage(strip, (int)dx, (int)dy, (int)dw, (int)dh, sx, sy, frameSize, frameSize);
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
    int frameSize { 0 };
    int frames { 0 };
};
