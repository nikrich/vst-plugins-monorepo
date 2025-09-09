#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../PluginProcessor.h"

class StereoThreshold : public juce::Component
{
public:
    explicit StereoThreshold(HungryGhostLimiterAudioProcessor::APVTS& apvts);

    void paintOverChildren(juce::Graphics& g) override; // empty (Pill L&F paints)
    void resized() override;

    void setSliderLookAndFeel(juce::LookAndFeel* lnf);

private:
    juce::Label  title{ {}, "THRESHOLD" };
    juce::Slider sliderL, sliderR;
    juce::Label  labelL{ {}, "L" }, labelR{ {}, "R" };
    juce::ToggleButton linkButton{ "Link" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attL, attR;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attLink;

    bool syncing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoThreshold)
};
