#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

// ----- Look & Feel -----
struct VibeLNF : juce::LookAndFeel_V4
{
    VibeLNF();
};

// ----- Label + Vertical Slider -----
class LabelledVSlider : public juce::Component
{
public:
    explicit LabelledVSlider(const juce::String& title);

    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    juce::Label  label;
    juce::Slider slider;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabelledVSlider)
};

// ----- Simple attenuation meter (0..12 dB) -----
class AttenMeter : public juce::Component
{
public:
    AttenMeter();
    ~AttenMeter() override = default;

    void setDb(float db);
    void paint(juce::Graphics& g) override;

private:
    float dbAtten = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AttenMeter)
};

// ----- Main Editor -----
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
    VibeLNF lnf;

    juce::Label title;
    LabelledVSlider threshold, ceiling, release;

    AttenMeter attenMeter;
    juce::Label attenLabel;

    juce::AudioProcessorValueTreeState::SliderAttachment thAttach, clAttach, reAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostLimiterAudioProcessorEditor)
};
