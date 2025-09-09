#pragma once
#include <juce_gui_extra/juce_gui_extra.h>

class AttenMeter : public juce::Component
{
public:
    explicit AttenMeter(const juce::String& title); // we ignore title for now

    void setDb(float db);
    void paint(juce::Graphics& g) override;

private:
    float dbAtten = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AttenMeter)
};
