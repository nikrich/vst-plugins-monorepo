#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/Theme.h>

namespace ui { namespace controls {

// DropZone: file drag-and-drop target with visual feedback, theme colours
// Supports both OS file drags (FileDragAndDropTarget) and DAW/JUCE internal drags (DragAndDropTarget)
class DropZone : public juce::Component,
                 public juce::FileDragAndDropTarget,
                 public juce::DragAndDropTarget {
public:
    std::function<void(const juce::StringArray& files)> onFilesDropped;
    std::function<bool(const juce::StringArray& files)> onFilesInterested;

    void setLabel(const juce::String& text)
    {
        label = text;
        repaint();
    }

    void setAcceptedExtensions(const juce::StringArray& exts)
    {
        acceptedExtensions = exts;
    }

    void paint(juce::Graphics& g) override
    {
        auto& th = ::Style::theme();
        auto r = getLocalBounds().toFloat().reduced(2.0f);
        const float radius = th.borderRadius;

        // Background
        g.setColour(dragging ? th.accent1.withAlpha(0.15f) : th.trackTop.withAlpha(0.3f));
        g.fillRoundedRectangle(r, radius);

        // Dashed border
        juce::Path borderPath;
        borderPath.addRoundedRectangle(r, radius);

        juce::PathStrokeType stroke(2.0f);
        float dashLengths[] = { 6.0f, 4.0f };
        stroke.createDashedStroke(borderPath, borderPath, dashLengths, 2);

        g.setColour(dragging ? th.accent1 : th.trackBot);
        g.strokePath(borderPath, stroke);

        // Label
        g.setColour(dragging ? th.accent1 : th.text.withAlpha(0.6f));
        g.setFont(14.0f);
        g.drawText(label, r, juce::Justification::centred, true);
    }

    // =========================================================================
    // FileDragAndDropTarget - handles OS file system drags
    // =========================================================================
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        return checkFilesAccepted(files);
    }

    void fileDragEnter(const juce::StringArray&, int, int) override
    {
        dragging = true;
        repaint();
    }

    void fileDragExit(const juce::StringArray&) override
    {
        dragging = false;
        repaint();
    }

    void filesDropped(const juce::StringArray& files, int, int) override
    {
        dragging = false;
        repaint();
        if (onFilesDropped)
            onFilesDropped(files);
    }

    // =========================================================================
    // DragAndDropTarget - handles DAW/JUCE internal drags
    // DAWs send audio as SourceDetails with description containing file paths
    // or audio data references
    // =========================================================================
    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        // Extract file paths from the drag description
        juce::StringArray files = extractFilesFromDragSource(details);
        if (!files.isEmpty())
            return checkFilesAccepted(files);

        // Accept if description looks like it could contain audio data
        // (some DAWs send custom var types for audio regions)
        if (details.description.isArray() || details.description.isObject())
            return true;

        // Accept string descriptions that look like file paths
        if (details.description.isString())
        {
            juce::String desc = details.description.toString();
            if (desc.containsChar('/') || desc.containsChar('\\') ||
                desc.endsWithIgnoreCase(".wav") || desc.endsWithIgnoreCase(".aiff") ||
                desc.endsWithIgnoreCase(".mp3") || desc.endsWithIgnoreCase(".flac"))
                return true;
        }

        return false;
    }

    void itemDragEnter(const SourceDetails&) override
    {
        dragging = true;
        repaint();
    }

    void itemDragMove(const SourceDetails&) override {}

    void itemDragExit(const SourceDetails&) override
    {
        dragging = false;
        repaint();
    }

    void itemDropped(const SourceDetails& details) override
    {
        dragging = false;
        repaint();

        juce::StringArray files = extractFilesFromDragSource(details);
        if (!files.isEmpty() && onFilesDropped)
            onFilesDropped(files);
    }

private:
    bool dragging { false };
    juce::String label { "Drop files here" };
    juce::StringArray acceptedExtensions;

    // Check if files match accepted extensions
    bool checkFilesAccepted(const juce::StringArray& files)
    {
        if (onFilesInterested)
            return onFilesInterested(files);

        if (acceptedExtensions.isEmpty())
            return true;

        for (const auto& file : files)
        {
            juce::String ext = juce::File(file).getFileExtension().toLowerCase();
            if (acceptedExtensions.contains(ext) || acceptedExtensions.contains(ext.substring(1)))
                return true;
        }
        return false;
    }

    // Extract file paths from DAW drag source
    // DAWs may send paths as: String, StringArray, Array of strings, or in var properties
    juce::StringArray extractFilesFromDragSource(const SourceDetails& details)
    {
        juce::StringArray files;

        // Case 1: Description is a single file path string
        if (details.description.isString())
        {
            juce::String path = details.description.toString();
            if (juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile())
                files.add(path);
            return files;
        }

        // Case 2: Description is an array (could be file paths or audio data refs)
        if (details.description.isArray())
        {
            const auto* arr = details.description.getArray();
            if (arr != nullptr)
            {
                for (const auto& item : *arr)
                {
                    if (item.isString())
                    {
                        juce::String path = item.toString();
                        if (juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile())
                            files.add(path);
                    }
                }
            }
            return files;
        }

        // Case 3: Description is an object with file/path properties (common DAW format)
        if (details.description.isObject())
        {
            // Try common property names used by DAWs
            static const char* pathProps[] = { "file", "path", "filePath", "audioFile", "url", "uri" };
            for (const char* prop : pathProps)
            {
                if (details.description.hasProperty(prop))
                {
                    juce::String path = details.description.getProperty(prop, {}).toString();
                    if (juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile())
                        files.add(path);
                }
            }

            // Check for "files" array property
            if (details.description.hasProperty("files"))
            {
                auto filesVar = details.description.getProperty("files", {});
                if (filesVar.isArray())
                {
                    const auto* arr = filesVar.getArray();
                    if (arr != nullptr)
                    {
                        for (const auto& item : *arr)
                        {
                            juce::String path = item.toString();
                            if (juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile())
                                files.add(path);
                        }
                    }
                }
            }
        }

        return files;
    }
};

} } // namespace ui::controls
