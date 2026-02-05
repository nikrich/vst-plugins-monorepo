#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

HungryGhostMultibandLimiterAudioProcessorEditor::HungryGhostMultibandLimiterAudioProcessorEditor(HungryGhostMultibandLimiterAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setSize(800, 600);
}

//==============================================================================

void HungryGhostMultibandLimiterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

//==============================================================================

void HungryGhostMultibandLimiterAudioProcessorEditor::resized()
{
    // Placeholder: layout will be implemented in STORY-MBL-006 (UI layout)
}
