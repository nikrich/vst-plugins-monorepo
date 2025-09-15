#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Foundation/Filmstrip.h>
#include <Foundation/ResourceResolver.h>

namespace ui { namespace controls {

// FilmstripSlider: a lightweight component that renders a filmstrip sprite sheet
// while delegating value handling to an inner juce::Slider.
//
// Usage:
//   FilmstripSlider fs;
//   fs.setFilmstrip(img, 128, ui::foundation::Filmstrip::Orientation::Vertical);
//   fs.getSlider().setRange(min, max, interval);
//   // Attach APVTS::SliderAttachment to fs.getSlider() as needed.
class FilmstripSlider : public juce::Component {
public:
    FilmstripSlider()
    {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        slider.onValueChange = [this]{ repaint(); };
    }

    void setFilmstrip(juce::Image img, int frames, ui::foundation::Filmstrip::Orientation orient)
    {
        film = ui::foundation::Filmstrip(std::move(img), frames, orient);
        repaint();
    }

    // Convenience: resolve a filmstrip by candidate names (BinaryData symbol or original filename suffix)
    bool setFilmstripByNames(std::initializer_list<const char*> candidates, int frames, ui::foundation::Filmstrip::Orientation orient)
    {
        auto img = ui::foundation::ResourceResolver::loadImageByNames(candidates);
        if (img.isValid()) { setFilmstrip(std::move(img), frames, orient); return true; }
        return false;
    }

    juce::Slider& getSlider() noexcept { return slider; }

    void resized() override { slider.setBounds(getLocalBounds()); }

    void paint(juce::Graphics& g) override
    {
        if (!film.isValid()) return;
        const auto& s = slider;
        const auto range = s.getRange();
        const double prop = range.getLength() > 0.0 ? (s.getValue() - range.getStart()) / range.getLength() : 0.0;
        film.drawNormalized(g, getLocalBounds().toFloat(), (float)prop, true);
    }

private:
    ui::foundation::Filmstrip film;
    juce::Slider slider;
};

} } // namespace ui::controls

