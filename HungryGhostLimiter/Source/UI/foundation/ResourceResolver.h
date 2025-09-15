#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>

namespace ui { namespace foundation {

// Centralized resource loading from embedded BinaryData, with convenience overloads.
// Goals:
// - Deduplicate scattered BinaryData lookups and decoding
// - Provide robust loading via multiple candidate names (e.g., "mkfinal_png", "mk-final.png")
// - Preserve existing visuals by returning empty images if not found (callers handle fallbacks)
class ResourceResolver {
public:
    // Load first matching image from a list of candidate resource names (BinaryData symbol or original filename)
    static juce::Image loadImageByNames(std::initializer_list<const char*> candidates);

    // Load image by a single name (BinaryData symbol or original filename)
    static juce::Image loadImage(const juce::String& name);

    // Typeface/font helpers
    static juce::Typeface::Ptr loadTypefaceByNames(std::initializer_list<const char*> candidates);
    static juce::Typeface::Ptr loadTypeface(const juce::String& name);

    // Return a MemoryBlock for a named BinaryData resource; empty if not found.
    static juce::MemoryBlock getResource(const juce::String& name);

private:
    static juce::Image loadFromBinaryDataName(const juce::String& symbolName);
    static juce::Image loadByOriginalFilenameSuffix(const juce::String& filenameSuffix);
    static juce::Typeface::Ptr loadTypefaceFromBinaryDataName(const juce::String& symbolName);
    static juce::Typeface::Ptr loadTypefaceByOriginalFilenameSuffix(const juce::String& filenameSuffix);
};

} } // namespace ui::foundation
