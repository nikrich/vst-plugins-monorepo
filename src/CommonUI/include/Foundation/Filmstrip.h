#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

namespace ui { namespace foundation {

class Filmstrip {
public:
    enum class Orientation { Vertical, Horizontal };

    Filmstrip() = default;
    Filmstrip(juce::Image stripImage, int frames, Orientation orient)
        : image(std::move(stripImage)), frameCount(frames), orientation(orient)
    {
        inferFrameSize();
    }

    bool isValid() const noexcept { return image.isValid() && frameCount > 0 && frameW > 0 && frameH > 0; }
    int getFrameCount() const noexcept { return frameCount; }
    int clampIndex(int idx) const noexcept { return juce::jlimit(0, frameCount - 1, idx); }
    int indexFromNormalized(float norm) const noexcept
    {
        if (frameCount <= 1) return 0;
        const float x = juce::jlimit(0.0f, 1.0f, norm);
        const int idx = (int) std::round(x * (float) (frameCount - 1));
        return juce::jlimit(0, frameCount - 1, idx);
    }
    juce::Rectangle<int> getFrameBounds(int idx) const noexcept
    {
        if (!isValid()) return {};
        idx = clampIndex(idx);
        if (orientation == Orientation::Vertical)
            return { 0, idx * frameH, frameW, frameH };
        return { idx * frameW, 0, frameW, frameH };
    }
    void drawFrame(juce::Graphics& g, juce::Rectangle<float> dest, int idx, bool highQuality = true) const
    {
        if (!isValid()) return;
        auto src = getFrameBounds(idx);
        juce::Graphics::ScopedSaveState save(g);
        g.setImageResamplingQuality(highQuality ? juce::Graphics::highResamplingQuality
                                                : juce::Graphics::lowResamplingQuality);
        g.drawImage(image,
                    (int) std::round(dest.getX()), (int) std::round(dest.getY()),
                    (int) std::round(dest.getWidth()), (int) std::round(dest.getHeight()),
                    src.getX(), src.getY(), src.getWidth(), src.getHeight());
    }
    void drawNormalized(juce::Graphics& g, juce::Rectangle<float> dest, float norm, bool highQuality = true) const
    {
        drawFrame(g, dest, indexFromNormalized(norm), highQuality);
    }

private:
    void inferFrameSize()
    {
        if (!image.isValid() || frameCount <= 0) { frameW = frameH = 0; return; }
        const int w = image.getWidth();
        const int h = image.getHeight();
        if (orientation == Orientation::Vertical)
        {
            frameW = w;
            frameH = h / juce::jmax(1, frameCount);
        }
        else
        {
            frameH = h;
            frameW = w / juce::jmax(1, frameCount);
        }
    }

    juce::Image image;
    int frameCount { 0 };
    Orientation orientation { Orientation::Vertical };
    int frameW { 0 };
    int frameH { 0 };
};

} } // namespace ui::foundation

