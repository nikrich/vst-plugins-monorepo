#pragma once
#include <JuceHeader.h>
#include <Styling/Theme.h>
#include "Layout.h"
#include <vector>
#include <memory>

namespace StemTrack {

// ============================================================================
// StemTrackRow: A single stem row with waveform visualization and drag-out
// ============================================================================
class StemTrackRow : public juce::Component
{
public:
    struct Style {
        juce::Colour bg        { 0xFF1A1D22 };
        juce::Colour waveform  { 0xFF66E1FF };
        juce::Colour waveformBg{ 0xFF0F1116 };
        juce::Colour text      { 0xFFE9EEF5 };
        juce::Colour textMuted { 0xFF9AA3AD };
        juce::Colour dragHandle{ 0xFF35FFDF };
        juce::Colour playhead  { 0xFFFFAD33 };
        float borderRadius = 4.0f;
        int labelWidth = 80;
        int handleWidth = 24;
        int rowPadding = 4;
    };

    StemTrackRow(const juce::String& stemName,
                 juce::AudioThumbnail& thumbnail,
                 const juce::File& sourceFile)
        : name(stemName)
        , thumb(thumbnail)
        , file(sourceFile)
    {
        nameLabel.setText(name, juce::dontSendNotification);
        nameLabel.setJustificationType(juce::Justification::centredLeft);
        nameLabel.setColour(juce::Label::textColourId, style.text);
        nameLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        nameLabel.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(nameLabel);
    }

    void setStyle(const Style& s) { style = s; repaint(); }

    void setPlayheadPosition(double normalizedPos)
    {
        playheadPos = juce::jlimit(0.0, 1.0, normalizedPos);
        repaint();
    }

    const juce::File& getFile() const { return file; }
    const juce::String& getName() const { return name; }

    void resized() override
    {
        auto r = getLocalBounds().reduced(style.rowPadding);

        // Label on left
        nameLabel.setBounds(r.removeFromLeft(style.labelWidth));

        // Drag handle on right
        handleArea = r.removeFromRight(style.handleWidth);

        // Waveform area in center
        waveformArea = r;
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Background
        g.setColour(style.bg);
        g.fillRoundedRectangle(r, style.borderRadius);

        // Waveform background
        auto wfBounds = waveformArea.toFloat();
        g.setColour(style.waveformBg);
        g.fillRoundedRectangle(wfBounds, style.borderRadius);

        // Draw waveform
        if (thumb.getTotalLength() > 0.0)
        {
            g.setColour(style.waveform);
            thumb.drawChannel(g,
                              waveformArea,
                              0.0,                      // start time
                              thumb.getTotalLength(),   // end time
                              0,                        // channel
                              1.0f);                    // vertical zoom

            // Draw second channel if stereo (slightly dimmer)
            if (thumb.getNumChannels() > 1)
            {
                g.setColour(style.waveform.withAlpha(0.5f));
                thumb.drawChannel(g, waveformArea, 0.0, thumb.getTotalLength(), 1, 1.0f);
            }
        }

        // Playhead
        if (playheadPos > 0.0 && playheadPos < 1.0)
        {
            const float xPos = wfBounds.getX() + static_cast<float>(playheadPos) * wfBounds.getWidth();
            g.setColour(style.playhead);
            g.drawLine(xPos, wfBounds.getY(), xPos, wfBounds.getBottom(), 2.0f);

            // Small triangle marker at top
            juce::Path tri;
            tri.addTriangle(xPos - 4.0f, wfBounds.getY(),
                            xPos + 4.0f, wfBounds.getY(),
                            xPos, wfBounds.getY() + 6.0f);
            g.fillPath(tri);
        }

        // Drag handle (grip lines)
        auto hBounds = handleArea.toFloat().reduced(4.0f, 8.0f);
        g.setColour(style.dragHandle.withAlpha(0.7f));
        const float lineSpacing = 4.0f;
        const float cx = hBounds.getCentreX();
        for (float y = hBounds.getY() + 4.0f; y < hBounds.getBottom() - 4.0f; y += lineSpacing)
        {
            g.fillEllipse(cx - 6.0f, y, 3.0f, 2.0f);
            g.fillEllipse(cx + 3.0f, y, 3.0f, 2.0f);
        }
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (handleArea.contains(e.getPosition()))
            dragStarted = true;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragStarted && file.existsAsFile())
        {
            // Find the DragAndDropContainer ancestor
            if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this))
            {
                // Create a drag image (small preview of waveform)
                juce::Image dragImage(juce::Image::ARGB, 120, 40, true);
                juce::Graphics ig(dragImage);
                ig.fillAll(style.bg.withAlpha(0.9f));
                ig.setColour(style.waveform);
                if (thumb.getTotalLength() > 0.0)
                    thumb.drawChannel(ig, juce::Rectangle<int>(0, 0, 120, 40),
                                      0.0, thumb.getTotalLength(), 0, 1.0f);
                ig.setColour(style.text);
                ig.setFont(10.0f);
                ig.drawText(name, 4, 2, 112, 12, juce::Justification::left);

                // Start the drag with the file
                juce::StringArray files;
                files.add(file.getFullPathName());
                container->performExternalDragDropOfFiles(files, false, this, nullptr);
            }
            dragStarted = false;
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragStarted = false;
    }

private:
    juce::String name;
    juce::AudioThumbnail& thumb;
    juce::File file;
    juce::Label nameLabel;

    juce::Rectangle<int> waveformArea;
    juce::Rectangle<int> handleArea;

    double playheadPos = 0.0;
    bool dragStarted = false;
    Style style;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemTrackRow)
};

// ============================================================================
// StemData: Holds all data for a single stem
// ============================================================================
struct StemData
{
    juce::String name;
    juce::File file;
    std::unique_ptr<juce::AudioThumbnail> thumbnail;
};

// ============================================================================
// StemTrackList: Container for stacked stem rows with playhead
// ============================================================================
class StemTrackList : public juce::Component,
                       public juce::DragAndDropContainer,
                       private juce::Timer
{
public:
    struct Style {
        juce::Colour bg        { 0xFF121315 };
        juce::Colour border    { 0xFF2B2E35 };
        juce::Colour playhead  { 0xFFFFAD33 };
        float borderRadius = 6.0f;
        int rowHeight = 64;
        int rowGap = 6;
        int padding = 8;
    };

    StemTrackList()
        : thumbnailCache(8)  // Cache up to 8 thumbnails
    {
        startTimerHz(30);
    }

    ~StemTrackList() override { stopTimer(); }

    void setStyle(const Style& s) { style = s; resized(); repaint(); }

    // Clear all stems
    void clearStems()
    {
        rows.clear();
        stems.clear();
        resized();
        repaint();
    }

    // Add a stem from an audio file
    void addStem(const juce::String& name, const juce::File& audioFile,
                 juce::AudioFormatManager& formatManager)
    {
        auto stem = std::make_unique<StemData>();
        stem->name = name;
        stem->file = audioFile;
        stem->thumbnail = std::make_unique<juce::AudioThumbnail>(
            512,              // samples per thumbnail point
            formatManager,
            thumbnailCache
        );

        // Load the thumbnail from file
        stem->thumbnail->setSource(new juce::FileInputSource(audioFile));

        // Create the row component
        auto row = std::make_unique<StemTrackRow>(name, *stem->thumbnail, audioFile);
        addAndMakeVisible(*row);
        rows.push_back(std::move(row));
        stems.push_back(std::move(stem));

        resized();
        repaint();
    }

    // Add a stem from an audio buffer (for real-time generated stems)
    void addStemFromBuffer(const juce::String& name,
                           const juce::File& outputFile,
                           juce::AudioFormatManager& formatManager,
                           const juce::AudioBuffer<float>& buffer,
                           double sampleRate)
    {
        auto stem = std::make_unique<StemData>();
        stem->name = name;
        stem->file = outputFile;
        stem->thumbnail = std::make_unique<juce::AudioThumbnail>(
            512, formatManager, thumbnailCache
        );

        // Set thumbnail directly from buffer
        stem->thumbnail->reset(buffer.getNumChannels(), sampleRate, buffer.getNumSamples());
        stem->thumbnail->addBlock(0, buffer, 0, buffer.getNumSamples());

        auto row = std::make_unique<StemTrackRow>(name, *stem->thumbnail, outputFile);
        addAndMakeVisible(*row);
        rows.push_back(std::move(row));
        stems.push_back(std::move(stem));

        resized();
        repaint();
    }

    // Set playhead position (0.0 to 1.0)
    void setPlayheadPosition(double normalizedPos)
    {
        playheadPos = juce::jlimit(0.0, 1.0, normalizedPos);
        for (auto& row : rows)
            row->setPlayheadPosition(playheadPos);
        repaint();
    }

    // Set playback state (for auto-advancing playhead)
    void setPlaying(bool isPlaying, double positionInSeconds = 0.0, double totalLengthSeconds = 1.0)
    {
        playing = isPlaying;
        currentPosSeconds = positionInSeconds;
        totalLength = juce::jmax(0.001, totalLengthSeconds);
    }

    // Get number of stems
    int getNumStems() const { return static_cast<int>(stems.size()); }

    // Get stem file by index
    juce::File getStemFile(int index) const
    {
        if (index >= 0 && index < static_cast<int>(stems.size()))
            return stems[static_cast<size_t>(index)]->file;
        return {};
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(style.padding);

        for (size_t i = 0; i < rows.size(); ++i)
        {
            auto rowBounds = r.removeFromTop(style.rowHeight);
            rows[i]->setBounds(rowBounds);
            r.removeFromTop(style.rowGap);
        }
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Background
        g.setColour(style.bg);
        g.fillRoundedRectangle(r, style.borderRadius);

        // Border
        g.setColour(style.border);
        g.drawRoundedRectangle(r.reduced(0.5f), style.borderRadius, 1.0f);

        // If no stems, show placeholder
        if (rows.empty())
        {
            g.setColour(::Style::theme().textMuted);
            g.setFont(14.0f);
            g.drawText("No stems loaded", r, juce::Justification::centred);
        }
    }

    // Calculate preferred height based on number of stems
    int getPreferredHeight() const
    {
        if (rows.empty())
            return style.padding * 2 + 60;  // Minimum height for placeholder
        return style.padding * 2
               + static_cast<int>(rows.size()) * style.rowHeight
               + static_cast<int>(rows.size() - 1) * style.rowGap;
    }

private:
    void timerCallback() override
    {
        // Auto-advance playhead if playing
        if (playing && totalLength > 0.0)
        {
            currentPosSeconds += 1.0 / 30.0;  // Advance by one frame
            if (currentPosSeconds > totalLength)
                currentPosSeconds = 0.0;  // Loop

            setPlayheadPosition(currentPosSeconds / totalLength);
        }
    }

    Style style;
    juce::AudioThumbnailCache thumbnailCache;

    std::vector<std::unique_ptr<StemData>> stems;
    std::vector<std::unique_ptr<StemTrackRow>> rows;

    double playheadPos = 0.0;
    bool playing = false;
    double currentPosSeconds = 0.0;
    double totalLength = 1.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemTrackList)
};

} // namespace StemTrack
