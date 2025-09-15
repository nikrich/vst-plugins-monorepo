#include "ResourceResolver.h"

namespace ui { namespace foundation {

static bool binarySymbolLookup(const juce::String& symbol, const void*& dataOut, int& sizeOut)
{
    sizeOut = 0; dataOut = BinaryData::getNamedResource(symbol.toRawUTF8(), sizeOut);
    return dataOut != nullptr && sizeOut > 0;
}

static juce::String normalizeSingle(const juce::String& name)
{
    // Accept common variants: "foo.png" -> "foo_png"; strip directory parts if provided
    auto base = name;
    if (base.containsChar('/'))
        base = base.fromLastOccurrenceOf("/", false, false);
    if (base.containsChar('\\'))
        base = base.fromLastOccurrenceOf("\\", false, false);

    if (base.endsWithIgnoreCase(".png"))
        return base.upToLastOccurrenceOf(".png", false, false) + "_png";
    if (base.endsWithIgnoreCase(".jpg"))
        return base.upToLastOccurrenceOf(".jpg", false, false) + "_jpg";
    if (base.endsWithIgnoreCase(".ttf"))
        return base.upToLastOccurrenceOf(".ttf", false, false) + ".ttf"; // typefaces typically not underscore-mangled
    return base;
}

juce::Image ResourceResolver::loadImageByNames(std::initializer_list<const char*> candidates)
{
    // Try direct symbol names then normalized variants, then scan by original filename suffix
    for (auto* c : candidates)
    {
        if (c == nullptr) continue;
        juce::String s(c);
        if (auto img = loadFromBinaryDataName(s); img.isValid()) return img;
        if (auto img2 = loadFromBinaryDataName(normalizeSingle(s)); img2.isValid()) return img2;
    }

    for (auto* c : candidates)
    {
        if (c == nullptr) continue;
        juce::String s(c);
        if (auto img = loadByOriginalFilenameSuffix(s); img.isValid()) return img;
    }

    return {};
}

juce::Image ResourceResolver::loadImage(const juce::String& name)
{
    if (auto img = loadFromBinaryDataName(name); img.isValid()) return img;
    if (auto img2 = loadFromBinaryDataName(normalizeSingle(name)); img2.isValid()) return img2;
    if (auto img3 = loadByOriginalFilenameSuffix(name); img3.isValid()) return img3;
    return {};
}

juce::Typeface::Ptr ResourceResolver::loadTypefaceByNames(std::initializer_list<const char*> candidates)
{
    for (auto* c : candidates)
    {
        if (c == nullptr) continue;
        juce::String s(c);
        if (auto tf = loadTypefaceFromBinaryDataName(s)) return tf;
        if (auto tf2 = loadTypefaceFromBinaryDataName(normalizeSingle(s))) return tf2;
    }
    for (auto* c : candidates)
    {
        if (c == nullptr) continue;
        juce::String s(c);
        if (auto tf = loadTypefaceByOriginalFilenameSuffix(s)) return tf;
    }
    return nullptr;
}

juce::Typeface::Ptr ResourceResolver::loadTypeface(const juce::String& name)
{
    if (auto tf = loadTypefaceFromBinaryDataName(name)) return tf;
    if (auto tf2 = loadTypefaceFromBinaryDataName(normalizeSingle(name))) return tf2;
    if (auto tf3 = loadTypefaceByOriginalFilenameSuffix(name)) return tf3;
    return nullptr;
}

juce::MemoryBlock ResourceResolver::getResource(const juce::String& name)
{
    const void* data = nullptr; int size = 0;
    if (binarySymbolLookup(name, data, size))
        return juce::MemoryBlock(data, (size_t) size);
    auto norm = normalizeSingle(name);
    if (binarySymbolLookup(norm, data, size))
        return juce::MemoryBlock(data, (size_t) size);

    // Scan by original filename suffix
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (auto* resName = BinaryData::namedResourceList[i])
        {
            if (auto* orig = BinaryData::getNamedResourceOriginalFilename(resName))
            {
                juce::String sOrig(orig);
                if (sOrig.endsWithIgnoreCase(name) || sOrig.endsWithIgnoreCase(norm))
                {
                    int sz = 0; if (const void* d = BinaryData::getNamedResource(resName, sz))
                        return juce::MemoryBlock(d, (size_t) sz);
                }
            }
        }
    }
    return {};
}

juce::Image ResourceResolver::loadFromBinaryDataName(const juce::String& symbolName)
{
    const void* data = nullptr; int size = 0;
    if (!binarySymbolLookup(symbolName, data, size))
        return {};
    return juce::ImageFileFormat::loadFrom(data, (size_t) size);
}

juce::Image ResourceResolver::loadByOriginalFilenameSuffix(const juce::String& filenameSuffix)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (auto* resName = BinaryData::namedResourceList[i])
        {
            if (auto* orig = BinaryData::getNamedResourceOriginalFilename(resName))
            {
                juce::String sOrig(orig);
                if (sOrig.endsWithIgnoreCase(filenameSuffix) || sOrig.containsIgnoreCase(filenameSuffix))
                {
                    int sz = 0; if (const void* data = BinaryData::getNamedResource(resName, sz))
                        return juce::ImageFileFormat::loadFrom(data, (size_t) sz);
                }
            }
        }
    }
    return {};
}

juce::Typeface::Ptr ResourceResolver::loadTypefaceFromBinaryDataName(const juce::String& symbolName)
{
    const void* data = nullptr; int size = 0;
    if (!binarySymbolLookup(symbolName, data, size))
        return nullptr;
    return juce::Typeface::createSystemTypefaceFor(data, (size_t) size);
}

juce::Typeface::Ptr ResourceResolver::loadTypefaceByOriginalFilenameSuffix(const juce::String& filenameSuffix)
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        if (auto* resName = BinaryData::namedResourceList[i])
        {
            if (auto* orig = BinaryData::getNamedResourceOriginalFilename(resName))
            {
                juce::String sOrig(orig);
                if (sOrig.endsWithIgnoreCase(filenameSuffix) || sOrig.containsIgnoreCase(filenameSuffix))
                {
                    int sz = 0; if (const void* data = BinaryData::getNamedResource(resName, sz))
                        return juce::Typeface::createSystemTypefaceFor(data, (size_t) sz);
                }
            }
        }
    }
    return nullptr;
}

} } // namespace ui::foundation
