#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace CommonUI { namespace Charts { class MBCLineChart; class CompressorChart; } }

class HungryGhostMultibandCompressorAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit HungryGhostMultibandCompressorAudioProcessorEditor(HungryGhostMultibandCompressorAudioProcessor& p);
    ~HungryGhostMultibandCompressorAudioProcessorEditor() override;
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    HungryGhostMultibandCompressorAudioProcessor& proc;
    std::unique_ptr<CommonUI::Charts::MBCLineChart> chart;
    std::unique_ptr<CommonUI::Charts::CompressorChart> compChart;

    // Band selector for controlling which band's curve is shown/edited
    juce::ComboBox bandSel;
    juce::Label    bandLabel;

    // Simple per-band controls row (threshold/ratio) for two bands
    juce::Slider th1, th2, ra1, ra2;
    juce::Label  lth1, lth2, lra1, lra2;

    // Selected-node knobs (like Pro-Q bottom strip)
    juce::Slider selFreq, selThresh, selRatio, selKnee;
    juce::ComboBox selType;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostMultibandCompressorAudioProcessorEditor)
};
