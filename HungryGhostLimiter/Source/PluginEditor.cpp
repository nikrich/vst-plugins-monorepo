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

    // reserve a header row for the logo
    auto header = a.removeFromTop(50); // tweak height to taste

    // size the logo by image aspect ratio, centered in header
    int logoH = 135;   // desired pixel height
    int logoW = 335;   // fallback width

    if (auto img = logoComp.getImage(); img.isValid())
        logoW = (int)std::round(img.getWidth() * (logoH / (double)img.getHeight()));

    // make sure it fits the header
    auto logoBounds = header.withSizeKeepingCentre(logoW, logoH);
    logoComp.setBounds(logoBounds);
	logoComp.setTopLeftPosition(logoBounds.getX(), logoBounds.getY() - 10);

    // subtext sits right under the logo, same width
    logoSub.setBounds(logoBounds.withY(logoBounds.getBottom() - 55).withHeight(16));

    // ---- your existing layout below ----
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
    attenMeter.setDb(proc.getSmoothedAttenDb()); // feed raw or lightly-smoothed dB
    // no repaint() — meter repaints itself at 60 Hz
    // (optional) once during setup:
    attenMeter.setSmoothing(30.0f, 180.0f); // attackMs, releaseMs
}
