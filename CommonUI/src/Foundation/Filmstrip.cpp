#include "Filmstrip.h"

namespace ui { namespace foundation {

Filmstrip::Filmstrip(juce::Image stripImage, int frames, Orientation orient)
    : image(std::move(stripImage)), frameCount(frames), orientation(orient)
{
    inferFrameSize();
}

void Filmstrip::inferFrameSize()
{
    if (!image.isValid() || frameCount <= 0) { frameW = frameH = 0; return; }
    const int w = image.getWidth();
    const int h = image.getHeight();

    if (orientation == Orientation::Vertical)
    {
        if (h % frameCount == 0) { frameW = w; frameH = h / juce::jmax(1, frameCount); }
        else { frameW = w; frameH = h / juce::jmax(1, frameCount); }
    }
    else
    {
        if (w % frameCount == 0) { frameH = h; frameW = w / juce::jmax(1, frameCount); }
        else { frameH = h; frameW = w / juce::jmax(1, frameCount); }
    }
}

int Filmstrip::indexFromNormalized(float norm) const noexcept
{
    if (frameCount <= 1) return 0;
    const float x = juce::jlimit(0.0f, 1.0f, norm);
    const int idx = (int) std::round(x * (float) (frameCount - 1));
    return juce::jlimit(0, frameCount - 1, idx);
}

juce::Rectangle<int> Filmstrip::getFrameBounds(int idx) const noexcept
{
    if (!isValid()) return {};
    idx = clampIndex(idx);
    if (orientation == Orientation::Vertical)
        return { 0, idx * frameH, frameW, frameH };
    return { idx * frameW, 0, frameW, frameH };
}

void Filmstrip::drawFrame(juce::Graphics& g, juce::Rectangle<float> dest, int idx, bool highQuality) const
{
    if (!isValid()) return;
    auto src = getFrameBounds(idx);
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform());
    g.setImageResamplingQuality(highQuality ? juce::Graphics::highResamplingQuality
                                            : juce::Graphics::lowResamplingQuality);
    g.drawImage(image,
                (int) std::round(dest.getX()), (int) std::round(dest.getY()),
                (int) std::round(dest.getWidth()), (int) std::round(dest.getHeight()),
                src.getX(), src.getY(), src.getWidth(), src.getHeight());
}

void Filmstrip::drawNormalized(juce::Graphics& g, juce::Rectangle<float> dest, float norm, bool highQuality) const
{
    drawFrame(g, dest, indexFromNormalized(norm), highQuality);
}

} } // namespace ui::foundation
