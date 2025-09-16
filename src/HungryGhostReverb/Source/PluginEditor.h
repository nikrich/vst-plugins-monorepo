#pragma once

#include <JuceHeader.h>
#include <Styling/LookAndFeels.h>
#include <Layout/Defaults.h>
#include <Controls/LogoHeader.h>
#include <Foundation/Card.h>

class HungryGhostReverbAudioProcessor;

class HungryGhostReverbAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit HungryGhostReverbAudioProcessorEditor(HungryGhostReverbAudioProcessor&);
    ~HungryGhostReverbAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void addKnob(juce::Slider& s, const juce::String& name, juce::LookAndFeel* lnf);
    void addBar(juce::Slider& s, const juce::String& name, juce::LookAndFeel* lnf);

    HungryGhostReverbAudioProcessor& processor;

    // Look & Feel
    VibeLNF appLNF;
    DonutKnobLNF knobLNF;
    PillVSliderLNF barLNF;
    NeonToggleLNF toggleLNF;

    // Header and main card
    ui::controls::LogoHeader header;
    ui::foundation::Card panel;

    // Controls
    juce::ComboBox modeBox;
    juce::Label     modeLabel;

    juce::Slider mixKnob, decayKnob, sizeKnob, widthKnob;
    juce::Slider predelayBar, diffusionBar, modRateBar, modDepthBar, hfDampBar, lowCutBar, highCutBar;
    juce::ToggleButton freezeBtn { "Freeze" };

    // Attachments
    std::unique_ptr<APVTS::ComboBoxAttachment> modeAtt;
    std::unique_ptr<APVTS::SliderAttachment> mixAtt, decayAtt, sizeAtt, widthAtt;
    std::unique_ptr<APVTS::SliderAttachment> predelayAtt, diffusionAtt, modRateAtt, modDepthAtt, hfDampAtt, lowCutAtt, highCutAtt;
    std::unique_ptr<APVTS::ButtonAttachment> freezeAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostReverbAudioProcessorEditor)
};

