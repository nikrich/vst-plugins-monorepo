#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include "PluginProcessor.h"
#include "ui/Layout.h"
#include <Controls/LogoHeader.h>
#include <Controls/TransportBar.h>
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

    MusicGPTExtractorAudioProcessor& proc;

    // Header
    ui::controls::LogoHeader logoHeader;

    // Transport controls
    ui::controls::TransportBar transportBar;

    // File drop zone / waveform display area
    juce::Component waveformArea;

    // Stem controls (will be expanded later)
    juce::Component stemControlsArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicGPTExtractorAudioProcessorEditor)
};
