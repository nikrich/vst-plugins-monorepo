#include "PluginEditor.h"
#include "PluginProcessor.h"

HungryGhostSaturationAudioProcessorEditor::HungryGhostSaturationAudioProcessorEditor(HungryGhostSaturationAudioProcessor& p)
: juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 240);
}

void HungryGhostSaturationAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawFittedText("Hungry Ghost Saturation", getLocalBounds(), juce::Justification::centred, 1);
}

void HungryGhostSaturationAudioProcessorEditor::resized()
{
    // Layout UI here in future
}
