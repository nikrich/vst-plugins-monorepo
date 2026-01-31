#include "PluginEditor.h"
#include <BinaryData.h>
#include <Foundation/ResourceResolver.h>

MusicGPTExtractorAudioProcessorEditor::MusicGPTExtractorAudioProcessorEditor(MusicGPTExtractorAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
{
    setResizable(false, false);
    setOpaque(true);
    setSize(Layout::kWindowWidth, Layout::kWindowHeight);

    addAndMakeVisible(logoHeader);
    addAndMakeVisible(transportBar);
    addAndMakeVisible(waveformArea);
    addAndMakeVisible(stemControlsArea);

    // Connect transport callbacks
    transportBar.onPlayPauseChanged = [this](bool isPlaying) {
        proc.setPlaying(isPlaying);
    };

    transportBar.onSeekChanged = [this](double position) {
        proc.getStemPlayer().setPosition(position);
    };

    startTimerHz(30);
}

MusicGPTExtractorAudioProcessorEditor::~MusicGPTExtractorAudioProcessorEditor()
{
}

void MusicGPTExtractorAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto& th = Style::theme();
    g.fillAll(th.bg);

    // Waveform area placeholder
    auto waveformBounds = waveformArea.getBounds().toFloat();
    g.setColour(th.panel);
    g.fillRoundedRectangle(waveformBounds, th.borderRadius);
    g.setColour(th.textMuted);
    g.drawText("Drop audio file here", waveformBounds.toNearestInt(), juce::Justification::centred);

    // Stem controls area placeholder
    auto stemBounds = stemControlsArea.getBounds().toFloat();
    g.setColour(th.panel);
    g.fillRoundedRectangle(stemBounds, th.borderRadius);
    g.setColour(th.textMuted);
    g.drawText("Stem Controls", stemBounds.toNearestInt(), juce::Justification::centred);
}

void MusicGPTExtractorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(Layout::kPaddingPx);

    // Header
    auto header = bounds.removeFromTop(Layout::kHeaderHeightPx);
    logoHeader.setBounds(header);

    bounds.removeFromTop(Layout::kRowGapPx);

    // Transport bar at bottom
    auto transport = bounds.removeFromBottom(ui::controls::TransportBar::kHeight);
    transportBar.setBounds(transport);

    bounds.removeFromBottom(Layout::kRowGapPx);

    // Stem controls on the right
    auto stemArea = bounds.removeFromRight(Layout::kStemControlsWidthPx);
    stemControlsArea.setBounds(stemArea);

    bounds.removeFromRight(Layout::kColGapPx);

    // Remaining area for waveform display
    waveformArea.setBounds(bounds);
}

void MusicGPTExtractorAudioProcessorEditor::timerCallback()
{
    // Update transport position from processor
    if (proc.isPlaying())
    {
        double totalDuration = proc.getStemPlayer().getTotalDuration();
        if (totalDuration > 0.0)
        {
            transportBar.setTotalDuration(totalDuration);
            transportBar.setPosition(proc.getPlaybackPosition() / totalDuration);
        }
    }

    // Sync play button state
    transportBar.setPlaying(proc.isPlaying());
}
