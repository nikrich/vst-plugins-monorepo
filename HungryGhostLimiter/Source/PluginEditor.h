#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include "PluginProcessor.h"
#include "Styling/LookAndFeels.h"
#include "UI/Controls.h"
#include "UI/Meter.h"
#include "UI/Threshold.h"
#include "BinaryData.h"
#include "Styling/Theme.h"
class HungryGhostLimiterAudioProcessorEditor
    : public juce::AudioProcessorEditor
    , private juce::Timer
{
public:
    explicit HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p);
    ~HungryGhostLimiterAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    HungryGhostLimiterAudioProcessor& proc;
    VibeLNF        lnf;
    PillVSliderLNF pillLNF;
    DonutKnobLNF   donutLNF;

    juce::Label title;
    juce::ImageComponent logoComp;   // centered logo at the top
    juce::Label          logoSub;    // optional "LIMITER" text under the logo

    StereoThreshold threshold;
    LabelledVSlider ceiling, release;

    AttenMeter  attenMeter;
    juce::Label attenLabel;

    // after 'LabelledVSlider ceiling, release;'
    LabelledVSlider lookAhead;          // "LOOK-AHEAD"

    juce::ToggleButton scHpfToggle{ "SC HPF" };
    juce::ToggleButton safetyToggle{ "SAFETY" };

    NeonToggleLNF neonToggleLNF;

    // attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> laAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> hpfAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> safAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostLimiterAudioProcessorEditor)
};
