#include "PluginEditor.h"
#include "PluginProcessor.h"

HungryGhostSaturationAudioProcessorEditor::HungryGhostSaturationAudioProcessorEditor(HungryGhostSaturationAudioProcessor& p)
: juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(640, 360);

    // Style knobs
    styleKnob(inKnob,   -24.0, 24.0, 0.01, 0.0,  " dB");
    styleKnob(driveKnob,  0.0, 36.0, 0.01, 12.0, " dB");
    styleKnob(preTiltKnob,0.0,  6.0, 0.01, 0.0,  " dB/oct");
    styleKnob(mixKnob,    0.0,  1.0, 0.001,1.0,  "");
    styleKnob(outKnob,  -24.0, 24.0, 0.01, 0.0,  " dB");
    styleKnob(asymKnob,  -0.5,  0.5, 0.001,0.0,  "");

    // Model / OS / PostLP / ChannelMode combos
    styleCombo(modelBox); styleCombo(osBox); styleCombo(postLPBox); styleCombo(channelModeBox);

    // Add components
    for (auto* s : { &inKnob, &driveKnob, &preTiltKnob, &mixKnob, &outKnob, &asymKnob }) addAndMakeVisible(s);
    for (auto* c : { &modelBox, &osBox, &postLPBox, &channelModeBox }) addAndMakeVisible(c);
    addAndMakeVisible(autoGainToggle);
    addAndMakeVisible(vocalToggle);
    styleKnob(vocalAmt, 0.0, 1.0, 0.001, 1.0, "");
    addAndMakeVisible(vocalAmt);
    styleCombo(vocalStyleBox);
    vocalStyleBox.addItemList({"Normal","Telephone"}, 1);
    addAndMakeVisible(vocalStyleBox);

    // Populate combo choices to match parameter layout indices
    modelBox.addItemList({ "TANH", "ATAN", "SOFT", "FEXP" }, 1);
    osBox.addItemList({ "1x", "2x", "4x" }, 1);
    postLPBox.addItemList({ "Off", "22k", "16k", "12k", "8k" }, 1);
    channelModeBox.addItemList({ "Stereo", "DualMono", "MonoSum" }, 1);

    // Attach to APVTS
    auto& apvts = processor.getAPVTS();
    inAtt       = std::make_unique<APVTS::SliderAttachment>(apvts, "in", inKnob);
    driveAtt    = std::make_unique<APVTS::SliderAttachment>(apvts, "drive", driveKnob);
    preTiltAtt  = std::make_unique<APVTS::SliderAttachment>(apvts, "pretilt", preTiltKnob);
    mixAtt      = std::make_unique<APVTS::SliderAttachment>(apvts, "mix", mixKnob);
    outAtt      = std::make_unique<APVTS::SliderAttachment>(apvts, "out", outKnob);
    asymAtt     = std::make_unique<APVTS::SliderAttachment>(apvts, "asym", asymKnob);

    modelAtt      = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "model", modelBox);
    osAtt         = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "os", osBox);
    postLPAtt     = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "postlp", postLPBox);
    channelModeAtt= std::make_unique<APVTS::ComboBoxAttachment>(apvts, "channelMode", channelModeBox);
    autoGainAtt   = std::make_unique<APVTS::ButtonAttachment>(apvts, "autoGain", autoGainToggle);
    vocalAtt      = std::make_unique<APVTS::ButtonAttachment>(apvts, "vocal", vocalToggle);
    vocalAmtAtt   = std::make_unique<APVTS::SliderAttachment>(apvts, "vocalAmt", vocalAmt);
    vocalStyleAtt = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "vocalStyle", vocalStyleBox);

    // Enable asym only for FEXP model
    auto onModelChange = [this]()
    {
        const int idx = modelBox.getSelectedItemIndex();
        const bool fexp = (idx == 3);
        asymKnob.setEnabled(fexp);
        asymKnob.setAlpha(fexp ? 1.0f : 0.5f);
    };
    modelBox.onChange = onModelChange;
    modelBox.setSelectedItemIndex((int) apvts.getRawParameterValue("model")->load(), juce::dontSendNotification);
    onModelChange();
}

void HungryGhostSaturationAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0B0F14));
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(juce::FontOptions(16.0f, juce::Font::bold)));
    g.drawFittedText("Hungry Ghost Saturation", getLocalBounds().removeFromTop(28), juce::Justification::centred, 1);
}

void HungryGhostSaturationAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(12);
    auto header = r.removeFromTop(36);
    juce::ignoreUnused(header);

    auto row1 = r.removeFromTop(160);
    auto row2 = r.removeFromTop(64);

    auto knobW = juce::jmax(72, row1.getWidth() / 6 - 8);
    auto gap = 8;

    auto placeKnob = [&](juce::Rectangle<int>& rr, juce::Component& c)
    {
        auto cell = rr.removeFromLeft(knobW);
        rr.removeFromLeft(gap);
        c.setBounds(cell.withSizeKeepingCentre(knobW, juce::jmin(cell.getHeight(), 120)));
    };

    // Row1: Input, Drive, PreTilt, Mix, Output, Asym
    {
        auto rr = row1.reduced(8);
        placeKnob(rr, inKnob);
        placeKnob(rr, driveKnob);
        placeKnob(rr, preTiltKnob);
        placeKnob(rr, mixKnob);
        placeKnob(rr, outKnob);
        placeKnob(rr, asymKnob);
    }

    // Row2: Model, OS, PostLP, ChannelMode, AutoGain, Vocal, Vocal Amt, Vocal Style
    {
        auto rr = row2.reduced(8);
        const int boxW = juce::jmax(80, rr.getWidth() / 8 - gap);
        modelBox.setBounds(rr.removeFromLeft(boxW)); rr.removeFromLeft(gap);
        osBox.setBounds(rr.removeFromLeft(boxW)); rr.removeFromLeft(gap);
        postLPBox.setBounds(rr.removeFromLeft(boxW)); rr.removeFromLeft(gap);
        channelModeBox.setBounds(rr.removeFromLeft(boxW)); rr.removeFromLeft(gap);
        autoGainToggle.setBounds(rr.removeFromLeft(100)); rr.removeFromLeft(gap);
        vocalToggle.setBounds(rr.removeFromLeft(110)); rr.removeFromLeft(gap);
        vocalAmt.setBounds(rr.removeFromLeft(80)); rr.removeFromLeft(gap);
        vocalStyleBox.setBounds(rr.removeFromLeft(120));
    }
}

void HungryGhostSaturationAudioProcessorEditor::styleKnob(juce::Slider& s, double min, double max, double step, double def, const juce::String& suffix)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    s.setRange(min, max, step);
    s.setValue(def, juce::dontSendNotification);
    s.setDoubleClickReturnValue(true, def);
    s.setNumDecimalPlacesToDisplay(step >= 1.0 ? 0 : 2);
    s.setLookAndFeel(&donutLNF);
    if (suffix.isNotEmpty()) s.setTextValueSuffix(suffix);
}

void HungryGhostSaturationAudioProcessorEditor::styleCombo(juce::ComboBox& c)
{
    c.setJustificationType(juce::Justification::centred);
}
