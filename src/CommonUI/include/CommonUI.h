#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>
#include "Styling/Theme.h"

namespace ui { namespace foundation {

class ResourceResolver {
public:
    static juce::Image loadImageByNames(std::initializer_list<const char*> candidates);
    static juce::Image loadImage(const juce::String& name);
    static juce::Typeface::Ptr loadTypefaceByNames(std::initializer_list<const char*> candidates);
    static juce::Typeface::Ptr loadTypeface(const juce::String& name);
    static juce::MemoryBlock getResource(const juce::String& name);
};

class Filmstrip {
public:
    enum class Orientation { Vertical, Horizontal };

    Filmstrip() = default;
    Filmstrip(juce::Image stripImage, int frames, Orientation orient);

    bool isValid() const noexcept;
    int getFrameCount() const noexcept;
    int clampIndex(int idx) const noexcept;
    int indexFromNormalized(float norm) const noexcept;
    juce::Rectangle<int> getFrameBounds(int idx) const noexcept;
    void drawFrame(juce::Graphics& g, juce::Rectangle<float> dest, int idx, bool highQuality = true) const;
    void drawNormalized(juce::Graphics& g, juce::Rectangle<float> dest, float norm, bool highQuality = true) const;
};

struct Typography {
    enum class Style { Title, Subtitle, Body, Caption };

    static void apply(juce::Label& label,
                      Style style,
                      juce::Colour colour = juce::Colour(),
                      juce::Justification just = juce::Justification::centred);
};

class Card : public juce::Component {
public:
    void setCornerRadius(float r);
    void setBorder(juce::Colour c, float thickness);
    void setBackground(juce::Colour c);
    void setDropShadow(bool enabled);

    void paint(juce::Graphics& g) override;
};

} } // namespace ui::foundation

