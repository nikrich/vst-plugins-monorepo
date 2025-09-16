#include "PluginEditor.h"
#include "PluginProcessor.h"

using namespace ui::layout;

HungryGhostReverbAudioProcessorEditor::HungryGhostReverbAudioProcessorEditor(HungryGhostReverbAudioProcessor& p)
: juce::AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&appLNF);

    // Header and panel
    addAndMakeVisible(header);
    addAndMakeVisible(panel);

    // Mode selector
    modeLabel.setText("Mode", juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centred);
    modeLabel.setColour(juce::Label::textColourId, Style::theme().text);
    addAndMakeVisible(modeLabel);

    modeBox.addItemList({ "Hall", "Room", "Plate", "Ambience" }, 1);
    addAndMakeVisible(modeBox);

    // Knobs
    addKnob(mixKnob,   "Mix",   &knobLNF);
    addKnob(decayKnob, "Decay", &knobLNF);
    addKnob(sizeKnob,  "Size",  &knobLNF);
    addKnob(widthKnob, "Width", &knobLNF);

    // Bars (vertical)
    addBar(predelayBar,  "Pre",      &barLNF);
    addBar(diffusionBar, "Diff",     &barLNF);
    addBar(modRateBar,   "Rate",     &barLNF);
    addBar(modDepthBar,  "Depth",    &barLNF);
    addBar(hfDampBar,    "HF Damp",  &barLNF);
    addBar(lowCutBar,    "LowCut",   &barLNF);
    addBar(highCutBar,   "HighCut",  &barLNF);

    // Toggle
    freezeBtn.setLookAndFeel(&toggleLNF);
    addAndMakeVisible(freezeBtn);

    // Attachments
    auto& apvts = processor.apvts;
    modeAtt     = std::make_unique<APVTS::ComboBoxAttachment>(apvts, "mode", modeBox);

    mixAtt      = std::make_unique<APVTS::SliderAttachment>(apvts, "mix",          mixKnob);
    decayAtt    = std::make_unique<APVTS::SliderAttachment>(apvts, "decaySeconds", decayKnob);
    sizeAtt     = std::make_unique<APVTS::SliderAttachment>(apvts, "size",         sizeKnob);
    widthAtt    = std::make_unique<APVTS::SliderAttachment>(apvts, "width",        widthKnob);

    predelayAtt = std::make_unique<APVTS::SliderAttachment>(apvts, "predelayMs",   predelayBar);
    diffusionAtt= std::make_unique<APVTS::SliderAttachment>(apvts, "diffusion",    diffusionBar);
    modRateAtt  = std::make_unique<APVTS::SliderAttachment>(apvts, "modRateHz",    modRateBar);
    modDepthAtt = std::make_unique<APVTS::SliderAttachment>(apvts, "modDepthMs",   modDepthBar);
    hfDampAtt   = std::make_unique<APVTS::SliderAttachment>(apvts, "hfDampingHz",  hfDampBar);
    lowCutAtt   = std::make_unique<APVTS::SliderAttachment>(apvts, "lowCutHz",     lowCutBar);
    highCutAtt  = std::make_unique<APVTS::SliderAttachment>(apvts, "highCutHz",    highCutBar);

    freezeAtt   = std::make_unique<APVTS::ButtonAttachment>(apvts, "freeze",       freezeBtn);

    setSize(820, 460);
}

void HungryGhostReverbAudioProcessorEditor::addKnob(juce::Slider& s, const juce::String& name, juce::LookAndFeel* lnf)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setLookAndFeel(lnf);
    s.setName(name);
    addAndMakeVisible(s);
}

void HungryGhostReverbAudioProcessorEditor::addBar(juce::Slider& s, const juce::String& name, juce::LookAndFeel* lnf)
{
    s.setSliderStyle(juce::Slider::LinearBarVertical);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 18);
    s.setLookAndFeel(lnf);
    s.setName(name);
    addAndMakeVisible(s);
}

void HungryGhostReverbAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(Style::theme().bg);
}

void HungryGhostReverbAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced(Defaults::kPaddingPx);

    // Header at top
    auto headerArea = r.removeFromTop(Defaults::kHeaderHeightPx);
    header.setBounds(headerArea);

    // Panel below
    panel.setBounds(r);
    auto content = r.reduced(Defaults::kPaddingPx);

    // Mode dropdown at top-left
    auto topRow = content.removeFromTop(Defaults::kTitleRowHeightPx);
    auto modeArea = topRow.removeFromLeft(240);
    modeLabel.setBounds(modeArea.removeFromLeft(80));
    modeBox.setBounds(modeArea.reduced(6));

    content.removeFromTop(Defaults::kRowGapPx);

    // Main area split into two rows: knobs row then bars row
    auto knobsRow = content.removeFromTop(220);
    content.removeFromTop(Defaults::kRowGapPx);
    auto barsRow  = content;

    const int knobW = 160;
    const int knobH = knobsRow.getHeight();
    auto k1 = knobsRow.removeFromLeft(knobW); k1.reduce(8, 8);
    auto k2 = knobsRow.removeFromLeft(knobW); k2.reduce(8, 8);
    auto k3 = knobsRow.removeFromLeft(knobW); k3.reduce(8, 8);
    auto k4 = knobsRow.removeFromLeft(knobW); k4.reduce(8, 8);

    mixKnob.setBounds(k1);
    decayKnob.setBounds(k2);
    sizeKnob.setBounds(k3);
    widthKnob.setBounds(k4);

    // Bars grid: 7 bars in one row
    const int barGap = Defaults::kColGapPx;
    const int barCount = 7;
    int barW = juce::jmax(60, (barsRow.getWidth() - (barCount - 1) * barGap) / barCount);
    auto placeBar = [&](juce::Component& c)
    {
        auto b = barsRow.removeFromLeft(barW);
        c.setBounds(b.reduced(4));
        barsRow.removeFromLeft(barGap);
    };

    placeBar(predelayBar);
    placeBar(diffusionBar);
    placeBar(modRateBar);
    placeBar(modDepthBar);
    placeBar(hfDampBar);
    placeBar(lowCutBar);
    placeBar(highCutBar);

    // Freeze toggle bottom-right overlay near bars
    auto freezeArea = getLocalBounds().removeFromBottom(40).removeFromRight(120).reduced(8, 4);
    freezeBtn.setBounds(freezeArea);
}

