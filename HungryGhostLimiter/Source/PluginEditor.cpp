#include "PluginEditor.h"

HungryGhostLimiterAudioProcessorEditor::HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
    , threshold(proc.apvts)
    , ceiling("OUT CEILING")
    , release("RELEASE")
    , attenMeter("ATTENUATION")
    , lookAhead("LOOK-AHEAD")
{
    setLookAndFeel(&lnf);
    setResizable(false, false);
    setOpaque(true);
    setSize(503, 400);

    auto logoImage = juce::ImageFileFormat::loadFrom(BinaryData::logo_png,
        BinaryData::logo_pngSize);
    logoComp.setImage(logoImage, juce::RectanglePlacement::centred);
    logoComp.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(logoComp);

    // optional text under the logo
    logoSub.setText("LIMITER", juce::dontSendNotification);
    logoSub.setJustificationType(juce::Justification::centred);
    logoSub.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    logoSub.setFont(juce::Font(juce::FontOptions(13.0f)));
    logoSub.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(logoSub);

    // hide the old title if you had one
    title.setVisible(false);

    // Skin Threshold L/R with the pill L&F
    threshold.setSliderLookAndFeel(&pillLNF);
	ceiling.setSliderLookAndFeel(&pillLNF);
	release.setSliderLookAndFeel(&pillLNF);

    addAndMakeVisible(threshold);
    addAndMakeVisible(ceiling);
    addAndMakeVisible(release);

    // inside constructor, after release etc. are made visible:
    lookAhead.setSliderLookAndFeel(&pillLNF);
    addAndMakeVisible(lookAhead);

    // toggles
    scHpfToggle.setLookAndFeel(&neonToggleLNF);
    safetyToggle.setLookAndFeel(&neonToggleLNF);
    addAndMakeVisible(scHpfToggle);
    addAndMakeVisible(safetyToggle);

	// Attenuation Meter
    attenMeter.setBarWidth(12);      // thinner
    attenMeter.setTopDown(true);     // reverse: top → bottom
    attenMeter.setShowTicks(true);   // or false if you want a super-clean bar

    attenLabel.setText("ATTEN", juce::dontSendNotification);
    attenLabel.setJustificationType(juce::Justification::centred);
    attenLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(attenLabel);
    addAndMakeVisible(attenMeter);

    // APVTS attachments for the remaining sliders
    clAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "outCeiling", ceiling.slider);
    reAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "release", release.slider);
    laAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.apvts, "lookAheadMs", lookAhead.slider);
    hpfAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "scHpf", scHpfToggle);
    safAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "safetyClip", safetyToggle);

    startTimerHz(30);
}

HungryGhostLimiterAudioProcessorEditor::~HungryGhostLimiterAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void HungryGhostLimiterAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF192033)); // deep navy

    // optional: subtle panel behind right meter
    auto bounds = getLocalBounds().reduced(10);
    auto rightPanel = bounds.removeFromRight((int)std::round(bounds.getWidth() * 0.30)).reduced(12);
    juce::DropShadow ds(juce::Colours::black.withAlpha(0.5f), 20, {});
    ds.drawForRectangle(g, rightPanel);
    g.setColour(juce::Colour(0xFF141821));
    g.fillRoundedRectangle(rightPanel.toFloat(), 12.0f);
}

void HungryGhostLimiterAudioProcessorEditor::resized()
{
    auto a = getLocalBounds().reduced(10);

    // header (logo) stays the same …
    auto header = a.removeFromTop(50);
    int logoH = 135, logoW = 335;
    if (auto img = logoComp.getImage(); img.isValid())
        logoW = (int)std::round(img.getWidth() * (logoH / (double)img.getHeight()));
    auto logoBounds = header.withSizeKeepingCentre(logoW, logoH);
    logoComp.setBounds(logoBounds);
    logoComp.setTopLeftPosition(logoBounds.getX(), logoBounds.getY() - 10);
    logoSub.setBounds(logoBounds.withY(logoBounds.getBottom() - 55).withHeight(16));

    // content
    auto left = a.removeFromLeft(a.proportionOfWidth(0.65f)).reduced(10);
    auto right = a.reduced(10);

    // three columns for Threshold / Ceiling / Release
    auto colW = left.getWidth() / 3;
    auto col1 = left.removeFromLeft(colW).reduced(8);
    auto col2 = left.removeFromLeft(colW).reduced(8);
    auto col3 = left.reduced(8);

    threshold.setBounds(col1);
    ceiling.setBounds(col2);
    release.setBounds(col3);

    // look-ahead spans beneath all three columns
    auto laH = 58;
    auto laArea = juce::Rectangle<int>(col1.getX(), col1.getBottom() + 10,
        col3.getRight() - col1.getX(), laH);
    lookAhead.setBounds(laArea);

    // meter + toggles on the right
    auto rTop = right.removeFromTop(20);
    attenLabel.setBounds(rTop);
    auto meterArea = right.withTrimmedTop(4);
    // leave space at bottom for toggles
    auto toggleHeight = 28;
    meterArea.removeFromBottom(toggleHeight + 10);
    attenMeter.setBounds(meterArea);

    // toggles arranged horizontally under the meter
    auto toggles = right.removeFromBottom(toggleHeight);
    auto half = toggles.removeFromLeft(toggles.getWidth() / 2).reduced(4);
    scHpfToggle.setBounds(half);
    safetyToggle.setBounds(toggles.reduced(4));
}

void HungryGhostLimiterAudioProcessorEditor::timerCallback()
{
    attenMeter.setDb(proc.getSmoothedAttenDb()); // feed raw or lightly-smoothed dB
    // no repaint() — meter repaints itself at 60 Hz
    // (optional) once during setup:
    attenMeter.setSmoothing(30.0f, 180.0f); // attackMs, releaseMs
}
