#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include "PluginProcessor.h"
#include "styling/LookAndFeels.h"
#include "ui/Controls.h"
#include "ui/Threshold.h"
#include "ui/Ceiling.h"
#include "ui/Layout.h"
#include "ui/columns/InputsColumn.h"
#include "ui/columns/ControlsColumn.h"
#include "ui/columns/MeterColumn.h"
#include "BinaryData.h"
#include "styling/Theme.h"
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
    NeonToggleLNF  neonToggleLNF;

    juce::Label title;
    juce::ImageComponent logoComp;   // centered logo at the top
    juce::Label          logoSub;    // optional "LIMITER" text under the logo

    // Columns
    InputsColumn    inputsCol;
    StereoCeiling   ceiling;
    ControlsColumn  controlsCol;
    MeterColumn     meterCol;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostLimiterAudioProcessorEditor)
};
