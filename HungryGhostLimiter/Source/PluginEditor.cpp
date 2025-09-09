#include "PluginEditor.h"

HungryGhostLimiterAudioProcessorEditor::HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
    , threshold(proc.apvts)
    , ceiling("OUT CEILING")
    , release("RELEASE")
    , attenMeter("ATTENUATION")
{
    setLookAndFeel(&lnf);
    setResizable(false, false);
    setOpaque(true);
    setSize(503, 284);

    title.setText("HUNGRY GHOST LIMITER", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centred);
    title.setFont(juce::Font(juce::FontOptions(16.0f)));
    addAndMakeVisible(title);

    // Skin Threshold L/R with the pill L&F
    threshold.setSliderLookAndFeel(&pillLNF);
	ceiling.setSliderLookAndFeel(&pillLNF);
	release.setSliderLookAndFeel(&pillLNF);

    addAndMakeVisible(threshold);
    addAndMakeVisible(ceiling);
    addAndMakeVisible(release);

    attenLabel.setText("ATTEN", juce::dontSendNotification);
    attenLabel.setJustificationType(juce::Justification::centred);
    attenLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(attenLabel);
    addAndMakeVisible(attenMeter);

    // APVTS attachments for the remaining sliders
    clAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "outCeiling", ceiling.slider);
    reAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "release", release.slider);

    startTimerHz(30);
}

HungryGhostLimiterAudioProcessorEditor::~HungryGhostLimiterAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void HungryGhostLimiterAudioProcessorEditor::paint(juce::Graphics& g)
{
    // background #192033
    g.fillAll(juce::Colour(0xFF192033));
}

void HungryGhostLimiterAudioProcessorEditor::resized()
{
    auto a = getLocalBounds().reduced(10);
    auto header = a.removeFromTop(26);
    title.setBounds(header);

    auto left = a.removeFromLeft(a.proportionOfWidth(0.65f)).reduced(10);
    auto right = a.reduced(10);

    auto colW = left.getWidth() / 3;
    threshold.setBounds(left.removeFromLeft(colW).reduced(8));
    ceiling.setBounds(left.removeFromLeft(colW).reduced(8));
    release.setBounds(left.removeFromLeft(colW).reduced(8));

    auto rTop = right.removeFromTop(20);
    attenLabel.setBounds(rTop);
    attenMeter.setBounds(right.withTrimmedTop(4));
}

void HungryGhostLimiterAudioProcessorEditor::timerCallback()
{
    attenMeter.setDb(proc.getSmoothedAttenDb());
    attenMeter.repaint();
}
