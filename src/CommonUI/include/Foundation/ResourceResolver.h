#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <BinaryData.h>

namespace ui { namespace foundation {

class ResourceResolver {
public:
    static juce::Image loadImageByNames(std::initializer_list<const char*> candidates)
    {
        for (auto* c : candidates)
        {
            if (c == nullptr) continue; juce::String s(c);
            if (auto img = loadImage(s); img.isValid()) return img;
        }
        return {};
    }

    static juce::Image loadImage(const juce::String& name)
    {
        auto norm = normalizeSingle(name);
        if (auto img = loadFromBinaryDataName(name); img.isValid()) return img;
        if (auto img2 = loadFromBinaryDataName(norm); img2.isValid()) return img2;
        if (auto img3 = loadByOriginalFilenameSuffix(name); img3.isValid()) return img3;
        if (auto img4 = loadByOriginalFilenameSuffix(norm); img4.isValid()) return img4;
        return {};
    }

    static juce::Typeface::Ptr loadTypefaceByNames(std::initializer_list<const char*> candidates)
    {
        for (auto* c : candidates)
        {
            if (c == nullptr) continue; juce::String s(c);
            if (auto tf = loadTypeface(s)) return tf;
        }
        return nullptr;
    }

    static juce::Typeface::Ptr loadTypeface(const juce::String& name)
    {
        auto norm = normalizeSingle(name);
        if (auto tf = loadTypefaceFromBinaryDataName(name)) return tf;
        if (auto tf2 = loadTypefaceFromBinaryDataName(norm)) return tf2;
        if (auto tf3 = loadTypefaceByOriginalFilenameSuffix(name)) return tf3;
        if (auto tf4 = loadTypefaceByOriginalFilenameSuffix(norm)) return tf4;
        return nullptr;
    }

    static juce::MemoryBlock getResource(const juce::String& name)
    {
        const void* data = nullptr; int size = 0;
        if (binarySymbolLookup(name, data, size)) return juce::MemoryBlock(data, (size_t) size);
        auto norm = normalizeSingle(name);
        if (binarySymbolLookup(norm, data, size)) return juce::MemoryBlock(data, (size_t) size);

        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (auto* resName = BinaryData::namedResourceList[i])
                if (auto* orig = BinaryData::getNamedResourceOriginalFilename(resName))
                {
                    juce::String sOrig(orig);
                    if (sOrig.endsWithIgnoreCase(name) || sOrig.containsIgnoreCase(name) || sOrig.endsWithIgnoreCase(norm))
                    {
                        int sz = 0; if (const void* d = BinaryData::getNamedResource(resName, sz))
                            return juce::MemoryBlock(d, (size_t) sz);
                    }
                }
        }
        return {};
    }

private:
    static bool binarySymbolLookup(const juce::String& symbol, const void*& dataOut, int& sizeOut)
    {
        sizeOut = 0; dataOut = BinaryData::getNamedResource(symbol.toRawUTF8(), sizeOut);
        return dataOut != nullptr && sizeOut > 0;
    }

    static juce::String normalizeSingle(const juce::String& name)
    {
        auto base = name;
        if (base.containsChar('/')) base = base.fromLastOccurrenceOf("/", false, false);
        if (base.containsChar('\\')) base = base.fromLastOccurrenceOf("\\", false, false);
        if (base.endsWithIgnoreCase(".png")) return base.upToLastOccurrenceOf(".png", false, false) + "_png";
        if (base.endsWithIgnoreCase(".jpg")) return base.upToLastOccurrenceOf(".jpg", false, false) + "_jpg";
        if (base.endsWithIgnoreCase(".ttf")) return base.upToLastOccurrenceOf(".ttf", false, false) + ".ttf";
        return base;
    }

    static juce::Image loadFromBinaryDataName(const juce::String& symbolName)
    {
        const void* data = nullptr; int size = 0;
        if (!binarySymbolLookup(symbolName, data, size)) return {};
        return juce::ImageFileFormat::loadFrom(data, (size_t) size);
    }

    static juce::Image loadByOriginalFilenameSuffix(const juce::String& filenameSuffix)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (auto* resName = BinaryData::namedResourceList[i])
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
        return {};
    }

    static juce::Typeface::Ptr loadTypefaceFromBinaryDataName(const juce::String& symbolName)
    {
        const void* data = nullptr; int size = 0;
        if (!binarySymbolLookup(symbolName, data, size)) return nullptr;
        return juce::Typeface::createSystemTypefaceFor(data, (size_t) size);
    }

    static juce::Typeface::Ptr loadTypefaceByOriginalFilenameSuffix(const juce::String& filenameSuffix)
    {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        {
            if (auto* resName = BinaryData::namedResourceList[i])
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
        return nullptr;
    }
};

} } // namespace ui::foundation

