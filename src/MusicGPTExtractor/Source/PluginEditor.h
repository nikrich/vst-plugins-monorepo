#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include "PluginProcessor.h"
#include "ui/Layout.h"
#include "ui/StemTrackList.h"
#include "ui/SettingsPanel.h"
#include <Controls/LogoHeader.h>
#include <Controls/TransportBar.h>
#include <Controls/DropZone.h>
#include <Styling/Theme.h>
#include <BinaryData.h>

class MusicGPTExtractorAudioProcessorEditor
    : public juce::AudioProcessorEditor
    , private juce::Timer
{
public:
    explicit MusicGPTExtractorAudioProcessorEditor(MusicGPTExtractorAudioProcessor& p);
    ~MusicGPTExtractorAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    // Extraction workflow
    void handleFilesDropped(const juce::StringArray& files);
    void startExtraction(const juce::File& audioFile);
    void onExtractionStatus(api::ExtractionStatus status, float progress);
    void onExtractionComplete(const api::ExtractionResult& result);
    void loadStemsIntoPlayer(const juce::StringArray& stemPaths);
    void showSettingsPanel();

    MusicGPTExtractorAudioProcessor& proc;

    // Header with settings button
    ui::controls::LogoHeader logoHeader;
    juce::TextButton settingsButton;

    // Transport controls
    ui::controls::TransportBar transportBar;

    // Drop zone for file input (shown when no stems loaded)
    ui::controls::DropZone dropZone;

    // Stem track list (shown after extraction)
    StemTrack::StemTrackList stemTrackList;

    // Settings panel (modal overlay)
    std::unique_ptr<SettingsPanel> settingsPanel;

    // State
    juce::File currentAudioFile;
    bool extractionInProgress = false;
    api::ExtractionStatus extractionStatus = api::ExtractionStatus::Idle;
    float extractionProgress = 0.0f;
    juce::String statusMessage;

    // Format manager for loading thumbnails
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicGPTExtractorAudioProcessorEditor)
};
