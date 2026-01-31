#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <Styling/LookAndFeels.h>
#include <Styling/Theme.h>
#include <Controls/LogoHeader.h>
#include <Controls/DropZone.h>
#include "ui/Layout.h"
#include "api/ExtractionClient.h"
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
    void onFilesDropped(const juce::StringArray& files);
    void updateExtractionProgress();

    MusicGPTExtractorAudioProcessor& proc;
    VibeLNF lnf;

    // UI Components
    ui::controls::LogoHeader logoHeader;
    ui::controls::DropZone dropZone;

    // Stem level sliders
    juce::Slider vocalsSlider, drumsSlider, bassSlider, otherSlider;
    juce::Label vocalsLabel, drumsLabel, bassLabel, otherLabel;

    // APVTS attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> vocalsAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> drumsAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bassAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> otherAttach;

    // Transport controls
    juce::TextButton playButton { "Play" };
    juce::TextButton stopButton { "Stop" };

    // Status display
    juce::Label statusLabel;
    float extractionProgress { 0.0f };

    // API client
    ExtractionClient extractionClient;

    // Background image
    juce::Image bgImage;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicGPTExtractorAudioProcessorEditor)
};
