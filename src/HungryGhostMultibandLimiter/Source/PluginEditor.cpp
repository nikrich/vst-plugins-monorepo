#include "PluginProcessor.h"
#include "PluginEditor.h"

HungryGhostMultibandLimiterAudioProcessorEditor::HungryGhostMultibandLimiterAudioProcessorEditor(HungryGhostMultibandLimiterAudioProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setSize(800, 600);
}

HungryGhostMultibandLimiterAudioProcessorEditor::~HungryGhostMultibandLimiterAudioProcessorEditor() {}

void HungryGhostMultibandLimiterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(15.0f);
    g.drawFittedText("HungryGhostMultibandLimiter - STORY-MBL-002: Crossover Filter", getLocalBounds(), juce::Justification::centred, 1);
}

void HungryGhostMultibandLimiterAudioProcessorEditor::resized() {}
