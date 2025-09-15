#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace ui { namespace foundation {

class Filmstrip {
public:
    enum class Orientation { Vertical, Horizontal };

    Filmstrip() = default;
    Filmstrip(juce::Image stripImage, int frames, Orientation orient);

    bool isValid() const noexcept { return image.isValid() && frameCount > 0 && frameW > 0 && frameH > 0; }
    int getFrameCount() const noexcept { return frameCount; }

    int clampIndex(int idx) const noexcept { return juce::jlimit(0, frameCount - 1, idx); }

    int indexFromNormalized(float norm) const noexcept;

    juce::Rectangle<int> getFrameBounds(int idx) const noexcept;

    void drawFrame(juce::Graphics& g, juce::Rectangle<float> dest, int idx, bool highQuality = true) const;
    void drawNormalized(juce::Graphics& g, juce::Rectangle<float> dest, float norm, bool highQuality = true) const;

private:
    void inferFrameSize();

    juce::Image image;
    int frameCount { 0 };
    Orientation orientation { Orientation::Vertical };
    int frameW { 0 };
    int frameH { 0 };
};

} } // namespace ui::foundation
