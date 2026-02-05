#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

HungryGhostMultibandLimiterAudioProcessorEditor::HungryGhostMultibandLimiterAudioProcessorEditor(HungryGhostMultibandLimiterAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    // STORY-MBL-007: Create and display metering panel
    metersPanel = std::make_unique<MetersPanel>();
    addAndMakeVisible(*metersPanel);

    // Start timer for meter updates at 60 Hz
    startTimerHz(60);

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
    // STORY-MBL-007: Layout meters panel at top of editor
    if (metersPanel)
        metersPanel->setBounds(0, 0, getWidth(), 60);
}

//==============================================================================

void HungryGhostMultibandLimiterAudioProcessorEditor::timerCallback()
{
    // STORY-MBL-007: Feed processor metering data to UI at 60 Hz
    if (!metersPanel)
        return;

    // Per-band data (2 bands for M1)
    for (int b = 0; b < 2; ++b)
    {
        metersPanel->setBandInputDb(b, processor.getBandInputDb(b));
        metersPanel->setBandGrDb(b, processor.getBandGainReductionDb(b));
        metersPanel->setBandOutputDb(b, processor.getBandOutputDb(b));
    }

    // Master levels
    metersPanel->setMasterInputDb(processor.getMasterInputDb());
    metersPanel->setMasterOutputDb(processor.getMasterOutputDb());
}
