#pragma once

#include <JuceHeader.h>
#include "ui/StyledKnob.h"
#include "ui/StyledCombo.h"

class HungryGhostSaturationAudioProcessor;

class HungryGhostSaturationAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit HungryGhostSaturationAudioProcessorEditor(HungryGhostSaturationAudioProcessor&);
    ~HungryGhostSaturationAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    HungryGhostSaturationAudioProcessor& processor;

    // Title
    juce::Label titleLabel;

    // Controls
    StyledKnob inKnob { -24.0, 24.0, 0.01, 0.0, " dB" };
    StyledKnob driveKnob { 0.0, 36.0, 0.01, 12.0, " dB" };
    StyledKnob preTiltKnob { 0.0, 6.0, 0.01, 0.0, " dB/oct" };
    StyledKnob mixKnob { 0.0, 1.0, 0.001, 1.0, "" };
    StyledKnob outKnob { -24.0, 24.0, 0.01, 0.0, " dB" };
    StyledKnob asymKnob { -0.5, 0.5, 0.001, 0.0, "" };
    StyledCombo modelBox;
    StyledCombo osBox;
    StyledCombo postLPBox;
    StyledCombo channelModeBox;
    juce::ToggleButton autoGainToggle { "Auto Gain" };
    juce::ToggleButton vocalToggle { "Vocal Lo-Fi" };
    StyledKnob vocalAmt { 0.0, 1.0, 0.001, 1.0, "" };
    StyledCombo vocalStyleBox;

    // Attachments
    std::unique_ptr<APVTS::SliderAttachment>  inAtt, driveAtt, preTiltAtt, mixAtt, outAtt, asymAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> modelAtt, osAtt, postLPAtt, channelModeAtt;
    std::unique_ptr<APVTS::ButtonAttachment>   autoGainAtt, vocalAtt;
    std::unique_ptr<APVTS::SliderAttachment>   vocalAmtAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> vocalStyleAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostSaturationAudioProcessorEditor)
};
