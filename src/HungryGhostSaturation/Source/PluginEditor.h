#pragma once

#include <JuceHeader.h>
#include <Styling/LookAndFeels.h>

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

    // Look and feel for knobs
    DonutKnobLNF donutLNF;

    // Controls
    juce::Slider inKnob, driveKnob, preTiltKnob, mixKnob, outKnob, asymKnob;
    juce::ComboBox modelBox, osBox, postLPBox, channelModeBox;
    juce::ToggleButton autoGainToggle { "Auto Gain" };

    // Attachments
    std::unique_ptr<APVTS::SliderAttachment>  inAtt, driveAtt, preTiltAtt, mixAtt, outAtt, asymAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> modelAtt, osAtt, postLPAtt, channelModeAtt;
    std::unique_ptr<APVTS::ButtonAttachment>   autoGainAtt;

    void styleKnob(juce::Slider& s, double min, double max, double step, double def, const juce::String& suffix = {});
    void styleCombo(juce::ComboBox& c);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostSaturationAudioProcessorEditor)
};
