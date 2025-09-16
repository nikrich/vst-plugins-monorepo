#include "PluginEditor.h"
#include "PluginProcessor.h"

HungryGhostReverbAudioProcessorEditor::HungryGhostReverbAudioProcessorEditor(HungryGhostReverbAudioProcessor& p)
: juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(400, 240);
}

void HungryGhostReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setColour(juce::Colours::white);
    g.setFont(18.0f);
    g.drawFittedText("Hungry Ghost Reverb", getLocalBounds(), juce::Justification::centred, 1);
}

void HungryGhostReverbAudioProcessorEditor::resized()
{
}

