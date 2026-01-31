#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <Styling/Theme.h>

namespace ui { namespace controls {

// DropZone: file drag-and-drop target with visual feedback, theme colours
// Supports both OS file drags (FileDragAndDropTarget) and DAW drags (DragAndDropTarget)
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

    // FileDragAndDropTarget
    bool isInterestedInFileDrag(const juce::StringArray& files) override
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

    // DragAndDropTarget - handles DAW drags (internal JUCE drag-and-drop)
    bool isInterestedInDragSource(const SourceDetails& details) override
    {
        auto files = extractFilesFromDragSource(details);
        if (files.isEmpty())
            return false;

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

    void itemDragEnter(const SourceDetails&) override
    {
        dragging = true;
        repaint();
    }

    void itemDragExit(const SourceDetails&) override
    {
        dragging = false;
        repaint();
    }

    void itemDropped(const SourceDetails& details) override
    {
        dragging = false;
        repaint();

        auto files = extractFilesFromDragSource(details);
        if (files.isNotEmpty() && onFilesDropped)
            onFilesDropped(files);
    }

private:
    // Extract file paths from DAW drag source
    // DAWs may send: String path, StringArray, Array of paths, or file:// URLs
    juce::StringArray extractFilesFromDragSource(const SourceDetails& details)
    {
        juce::StringArray files;
        const auto& desc = details.description;

        if (desc.isString())
        {
            juce::String path = desc.toString();
            if (path.startsWith("file://"))
                path = juce::URL(path).getLocalFile().getFullPathName();
            if (juce::File(path).existsAsFile())
                files.add(path);
        }
        else if (desc.isArray())
        {
            for (int i = 0; i < desc.size(); ++i)
            {
                juce::String path = desc[i].toString();
                if (path.startsWith("file://"))
                    path = juce::URL(path).getLocalFile().getFullPathName();
                if (juce::File(path).existsAsFile())
                    files.add(path);
            }
        }

        return files;
    }

    bool dragging { false };
    juce::String label { "Drop files here" };
    juce::StringArray acceptedExtensions;
};

} } // namespace ui::controls
