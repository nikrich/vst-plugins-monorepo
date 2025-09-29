#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include <Styling/LookAndFeels.h>

namespace CommonUI { namespace Charts { class MBCLineChart; } }

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

    // Band selector for controlling which band's curve is shown/edited
    juce::ComboBox bandSel;
    juce::Label    bandLabel;

    // Bottom knobs using CommonUI DonutKnobLNF
    DonutKnobLNF donutLNF;
    juce::Slider knobThresh, knobAttack, knobRelease, knobKnee, knobRatio, knobMix, knobOutput;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attThresh, attAttack, attRelease, attKnee, attRatio, attMix, attOutput;

    // Selected-node knobs (like Pro-Q bottom strip)
    juce::Slider selFreq, selThresh, selRatio, selKnee;
    juce::ComboBox selType;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostMultibandCompressorAudioProcessorEditor)
};
