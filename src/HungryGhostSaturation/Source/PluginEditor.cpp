#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <Foundation/Typography.h>

HungryGhostSaturationAudioProcessorEditor::HungryGhostSaturationAudioProcessorEditor(HungryGhostSaturationAudioProcessor& p)
: juce::AudioProcessorEditor(&p), processor(p)
{
    setSize(640, 360);

    // Title label
    titleLabel.setText("Hungry Ghost Saturation", juce::dontSendNotification);
    ui::foundation::Typography::apply(titleLabel, ui::foundation::Typography::Style::Title);
    addAndMakeVisible(titleLabel);

    // Add components
    for (auto* s : { &inKnob, &driveKnob, &preTiltKnob, &mixKnob, &outKnob, &asymKnob }) addAndMakeVisible(s);
    for (auto* c : { &modelBox, &osBox, &postLPBox, &channelModeBox }) addAndMakeVisible(c);
    addAndMakeVisible(autoGainToggle);
    addAndMakeVisible(vocalToggle);
    addAndMakeVisible(vocalAmt);
    addAndMakeVisible(vocalStyleBox);

    // Populate combo choices to match parameter layout indices
    modelBox.getCombo().addItemList({ "TANH", "ATAN", "SOFT", "FEXP" }, 1);
    osBox.getCombo().addItemList({ "1x", "2x", "4x" }, 1);
    postLPBox.getCombo().addItemList({ "Off", "22k", "16k", "12k", "8k" }, 1);
    channelModeBox.getCombo().addItemList({ "Stereo", "DualMono", "MonoSum" }, 1);
    vocalStyleBox.getCombo().addItemList({"Normal","Telephone"}, 1);

    // Attach to APVTS
    auto& apvts = processor.getAPVTS();
    inAtt       = std::make_unique<APVTS::SliderAttachment>(apvts, "in", inKnob.getSlider());
    driveAtt    = std::make_unique<APVTS::SliderAttachment>(apvts, "drive", driveKnob.getSlider());
    preTiltAtt  = std::make_unique<APVTS::SliderAttachment>(apvts, "pretilt", preTiltKnob.getSlider());
    mixAtt      = std::make_unique<APVTS::SliderAttachment>(apvts, "mix", mixKnob.getSlider());
    outAtt      = std::make_unique<APVTS::SliderAttachment>(apvts, "out", outKnob.getSlider());
    asymAtt     = std::make_unique<APVTS::SliderAttachment>(apvts, "asym", asymKnob.getSlider());

    modelAtt      = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "model", modelBox.getCombo());
    osAtt         = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "os", osBox.getCombo());
    postLPAtt     = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "postlp", postLPBox.getCombo());
    channelModeAtt= std::make_unique<APVTS::ComboBoxAttachment>(apvts, "channelMode", channelModeBox.getCombo());
    autoGainAtt   = std::make_unique<APVTS::ButtonAttachment>(apvts, "autoGain", autoGainToggle);
    vocalAtt      = std::make_unique<APVTS::ButtonAttachment>(apvts, "vocal", vocalToggle);
    vocalAmtAtt   = std::make_unique<APVTS::SliderAttachment>(apvts, "vocalAmt", vocalAmt.getSlider());
    vocalStyleAtt = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "vocalStyle", vocalStyleBox.getCombo());

    // Enable asym only for FEXP model
    auto onModelChange = [this]()
    {
        const int idx = modelBox.getCombo().getSelectedItemIndex();
        const bool fexp = (idx == 3);
        asymKnob.getSlider().setEnabled(fexp);
        asymKnob.getSlider().setAlpha(fexp ? 1.0f : 0.5f);
    };
    modelBox.getCombo().onChange = onModelChange;
    modelBox.getCombo().setSelectedItemIndex((int) apvts.getRawParameterValue("model")->load(), juce::dontSendNotification);
    onModelChange();
}

void HungryGhostSaturationAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF0B0F14));
}

void HungryGhostSaturationAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(12);
    auto header = r.removeFromTop(36);
    titleLabel.setBounds(header);

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

