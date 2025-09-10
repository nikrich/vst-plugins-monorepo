#include "PluginEditor.h"
#include "BinaryData.h"

HungryGhostLimiterAudioProcessorEditor::HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
    , threshold(proc.apvts)
    , ceiling(proc.apvts)
    , release("RELEASE")
    , attenMeter("ATTENUATION")
    , lookAhead("LOOK-AHEAD")
{
    setLookAndFeel(&lnf);
    setResizable(false, false);
    setOpaque(true);
    setSize(760, 460); // was 503x400 → give the UI room

    int logoSize = 0;
    if (const auto* logoData = BinaryData::getNamedResource("logo_png", logoSize))
    {
        auto logoImage = juce::ImageFileFormat::loadFrom(logoData, (size_t) logoSize);
        logoComp.setImage(logoImage, juce::RectanglePlacement::centred);
    }
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

    release.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    release.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    release.setSliderLookAndFeel(&donutLNF);

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

    // container for third column (grid layout convenience)
    addAndMakeVisible(col3Container);

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
    auto& th = Style::theme();
    g.fillAll(th.bg);

    // subtle panel behind right meter
    auto bounds = getLocalBounds().reduced(10);
    auto rightPanel = bounds.removeFromRight((int)std::round(bounds.getWidth() * 0.30)).reduced(12);
    juce::DropShadow ds(juce::Colours::black.withAlpha(0.45f), 18, {});
    ds.drawForRectangle(g, rightPanel);
    g.setColour(th.panel);
    g.fillRoundedRectangle(rightPanel.toFloat(), 12.0f);
}

void HungryGhostLimiterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(12);

    // --- Header (logo + subtext) ---
    auto header = bounds.removeFromTop(88);
    int logoH = header.getHeight() - 20;
    int logoW = 320;
    if (auto img = logoComp.getImage(); img.isValid())
        logoW = (int)std::round(img.getWidth() * (logoH / (double)img.getHeight()));

    auto logoBounds = header.withSizeKeepingCentre(logoW, logoH);
    logoComp.setBounds(logoBounds);

    logoSub.setBounds(logoBounds.withY(logoBounds.getBottom() - 16).withHeight(16));

    // --- Content area split: left controls / right meter ---
    auto content = bounds;
    auto left = content.removeFromLeft(juce::roundToInt(content.getWidth() * 0.66f)).reduced(8);
    auto right = content.reduced(8);

    // Left: 3 columns (Threshold, Ceiling, Release) via juce::Grid
    {
        juce::Grid grid;
        using Track = juce::Grid::TrackInfo;
        grid.templateColumns = { Track(juce::Grid::Fr(1)), Track(juce::Grid::Fr(1)), Track(juce::Grid::Fr(1)) };
        grid.templateRows = { Track(juce::Grid::Fr(1)) };
        grid.items = {
            juce::GridItem(threshold).withMargin(6),
            juce::GridItem(ceiling).withMargin(6),
            juce::GridItem(col3Container).withMargin(6)
        };
        grid.performLayout(left);
    }

    auto col3 = col3Container.getBounds();

    // Release knob: square area near top, label already inside your LabelledVSlider
    auto knobSize = juce::jmin(col3.getWidth(), juce::roundToInt(col3.getHeight() * 0.48f));
    auto knobArea = juce::Rectangle<int>(0, 0, knobSize, knobSize)
        .withCentre({ col3.getCentreX(), col3.getY() + knobSize / 2 + 6 });
    release.setBounds(knobArea);

    // Look-ahead slider under the knob
    {
        const int laW = juce::jmin(120, col3.getWidth());
        const int laH = juce::jmax(90, col3.getHeight() - knobArea.getBottom() - 60);
        auto laArea = juce::Rectangle<int>(laW, laH)
            .withCentre({ col3.getCentreX(), knobArea.getBottom() + 12 + laH / 2 });
        lookAhead.setBounds(laArea);
    }

    // Toggles at the bottom of column 3 (make space for label-under style)
    {
        auto bottomRow = col3;
        bottomRow.removeFromTop(col3.getHeight() - 48);
        const int gap = 12;
        auto leftHalf = bottomRow.removeFromLeft(bottomRow.getWidth() / 2 - gap / 2);
        bottomRow.removeFromLeft(gap);
        auto rightHalf = bottomRow;
        scHpfToggle.setBounds(leftHalf.reduced(2));
        safetyToggle.setBounds(rightHalf.reduced(2));
    }

    // Right: Attenuation label + meter
    auto meterLabel = right.removeFromTop(24);
    attenLabel.setBounds(meterLabel);
    attenLabel.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
    attenMeter.setBounds(right.reduced(18, 6));
}

void HungryGhostLimiterAudioProcessorEditor::timerCallback()
{
    attenMeter.setDb(proc.getSmoothedAttenDb()); // feed raw or lightly-smoothed dB
    // no repaint() — meter repaints itself at 60 Hz
    // (optional) once during setup:
    attenMeter.setSmoothing(30.0f, 180.0f); // attackMs, releaseMs
}
