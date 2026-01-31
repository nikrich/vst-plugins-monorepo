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
    , public juce::DragAndDropContainer
    , public juce::FileDragAndDropTarget
    , private juce::Timer
{
public:
    explicit MusicGPTExtractorAudioProcessorEditor(MusicGPTExtractorAudioProcessor& p);
    ~MusicGPTExtractorAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // FileDragAndDropTarget - fallback for OS file drags directly on editor
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void timerCallback() override;
    void handleFilesDropped(const juce::StringArray& files);
    void startExtraction(const juce::File& audioFile);
    void onExtractionProgress(const musicgpt::ProgressInfo& info);
    void onExtractionComplete(const musicgpt::ExtractionResult& result);
    void loadStemsIntoUI(const std::vector<musicgpt::StemResult>& stems);
    void showSettings();
    void updateUIState();

    MusicGPTExtractorAudioProcessor& proc;

    // Current state
    enum class State { Idle, Extracting, Ready };
    State currentState { State::Idle };
    juce::File currentAudioFile;
    juce::String progressMessage;
    float extractionProgress { 0.0f };

    // Header
    ui::controls::LogoHeader logoHeader;

    // Transport controls
    ui::controls::TransportBar transportBar;

    // File drop zone (shown when idle)
    ui::controls::DropZone dropZone;

    // Stem track list (shown when stems are loaded)
    StemTrack::StemTrackList stemTrackList;

    // Settings panel (modal)
    SettingsPanel settingsPanel;

    // Settings button
    juce::TextButton settingsButton;

    // Audio format manager for waveform display
    juce::AudioFormatManager formatManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicGPTExtractorAudioProcessorEditor)
};
