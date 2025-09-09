#include "PluginEditor.h"

// ---------- VibeLNF ----------
VibeLNF::VibeLNF()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
}

// ---------- LabelledVSlider ----------
LabelledVSlider::LabelledVSlider(const juce::String& title)
{
    label.setText(title, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setInterceptsMouseClicks(false, false);

    slider.setSliderStyle(juce::Slider::LinearBarVertical);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(210, 210, 210));

    addAndMakeVisible(label);
    addAndMakeVisible(slider);
}

void LabelledVSlider::paintOverChildren(juce::Graphics& g)
{
    auto r = slider.getBounds().toFloat().reduced(8.0f, 6.0f);
    r.removeFromTop(18.0f); // space around thumb/text
    juce::ColourGradient grad(juce::Colour::fromRGB(250, 212, 65), r.getBottomLeft(),
        juce::Colour::fromRGB(220, 40, 30), r.getTopLeft(), false);
    juce::FillType ft(grad);
    ft.setOpacity(0.85f);
    g.setFillType(ft);
    g.fillRoundedRectangle(r, 3.0f);

    g.setColour(juce::Colours::white.withAlpha(0.15f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);
}

void LabelledVSlider::resized()
{
    auto a = getLocalBounds();
    label.setBounds(a.removeFromTop(20));
    slider.setBounds(a);
}

AttenMeter::AttenMeter(const juce::String& title)
{
    // TODO Implement Header
}

// ---------- AttenMeter ----------
void AttenMeter::setDb(float db)
{
    dbAtten = juce::jlimit(0.0f, 12.0f, db);
}

void AttenMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(juce::Colour::fromRGB(30, 30, 30));
    g.fillRoundedRectangle(r, 4.0f);

    const float frac = dbAtten / 12.0f;
    auto fill = r.removeFromBottom(r.getHeight() * frac);

    juce::ColourGradient grad(juce::Colour::fromRGB(245, 210, 60), fill.getBottomLeft(),
        juce::Colour::fromRGB(220, 40, 30), fill.getTopLeft(), false);
    g.setGradientFill(grad);
    g.fillRoundedRectangle(fill, 4.0f);

    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 4.0f, 1.0f);

    // ticks & labels: 0,1,2,3,6,9,12
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    auto b = getLocalBounds();
    auto labelArea = b.withX(b.getRight() - 22).withWidth(22);

    auto drawTick = [&](int dB)
        {
            const float y = juce::jmap((float)dB, 0.0f, 12.0f,
                (float)b.getBottom(), (float)b.getY());
            g.drawHorizontalLine((int)std::round(y), (float)b.getX(), (float)b.getRight());
            g.drawFittedText(juce::String(dB),
                labelArea.withY((int)(y - 8)).withHeight(16),
                juce::Justification::centredRight, 1);
        };
    for (int d : { 0, 1, 2, 3, 6, 9, 12 }) drawTick(d);
}

// ---------- StereoThreshold ----------
StereoThreshold::StereoThreshold(HungryGhostLimiterAudioProcessor::APVTS& apvts)
{
    title.setJustificationType(juce::Justification::centred);
    title.setInterceptsMouseClicks(false, false);
    title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    addAndMakeVisible(title);

    auto initSlider = [](juce::Slider& s)
        {
            s.setSliderStyle(juce::Slider::LinearBarVertical);
            s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
            s.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);
            s.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(210, 210, 210));
        };
    initSlider(sliderL);
    initSlider(sliderR);

    addAndMakeVisible(sliderL);
    addAndMakeVisible(sliderR);

    labelL.setJustificationType(juce::Justification::centred);
    labelR.setJustificationType(juce::Justification::centred);
    labelL.setInterceptsMouseClicks(false, false);
    labelR.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(labelL);
    addAndMakeVisible(labelR);

    addAndMakeVisible(linkButton);

    // APVTS attachments
    attL = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "thresholdL", sliderL);
    attR = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "thresholdR", sliderR);
    attLink = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "thresholdLink", linkButton);

    // mirror behavior when linked
    auto mirror = [this](juce::Slider* src, juce::Slider* dst)
        {
            if (syncing || !linkButton.getToggleState()) return;
            syncing = true;
            dst->setValue(src->getValue(), juce::sendNotificationSync);
            syncing = false;
        };
    sliderL.onValueChange = [=] { mirror(&sliderL, &sliderR); };
    sliderR.onValueChange = [=] { mirror(&sliderR, &sliderL); };
}

void StereoThreshold::resized()
{
    auto a = getLocalBounds();
    title.setBounds(a.removeFromTop(20));

    auto area = a.reduced(6);
    int gap = 8;
    auto left = area.removeFromLeft(area.getWidth() / 2 - gap / 2);
    area.removeFromLeft(gap);
    auto right = area;

    labelL.setBounds(left.removeFromTop(16));
    sliderL.setBounds(left);

    labelR.setBounds(right.removeFromTop(16));
    sliderR.setBounds(right);

    auto bottom = getLocalBounds().removeFromBottom(20);
    linkButton.setBounds(bottom.removeFromRight(70));
}

// Handled by LookAndFeel now
void StereoThreshold::paintOverChildren(juce::Graphics&) {}

void StereoThreshold::setSliderLookAndFeel(juce::LookAndFeel* lnf)
{
    sliderL.setLookAndFeel(lnf);
    sliderR.setLookAndFeel(lnf);
}

// ---------- HungryGhostLimiterAudioProcessorEditor ----------
HungryGhostLimiterAudioProcessorEditor::HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
    , threshold(proc.apvts)              // StereoThreshold member
    , ceiling("OUT CEILING")
    , attenMeter("ATTENUATION")
    , release("RELEASE")
{
    setLookAndFeel(&lnf);
    setResizable(false, false);
    setSize(503, 284);

    title.setText("HUNGRY GHOST LIMITER", juce::dontSendNotification);
    title.setJustificationType(juce::Justification::centred);
    title.setFont(juce::Font(juce::FontOptions(16.0f)));
    addAndMakeVisible(title);

    threshold.setSliderLookAndFeel(&pillLNF);

    addAndMakeVisible(threshold);
    addAndMakeVisible(ceiling);
    addAndMakeVisible(release);

    attenLabel.setText("ATTEN", juce::dontSendNotification);
    attenLabel.setJustificationType(juce::Justification::centred);
    attenLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(attenLabel);
    addAndMakeVisible(attenMeter);

    // attachments for the remaining sliders
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
    auto b = getLocalBounds().toFloat();
    juce::Colour top = juce::Colour::fromRGB(27, 34, 51);
    juce::Colour bot = juce::Colour::fromRGB(19, 20, 33);
    g.setGradientFill(juce::ColourGradient(top, b.getX(), b.getY(),
        bot, b.getX(), b.getBottom(), false));
    g.fillRect(b);

    g.setColour(juce::Colours::black.withAlpha(0.25f));
    g.drawRect(getLocalBounds(), 1);
}

void HungryGhostLimiterAudioProcessorEditor::resized()
{
    auto a = getLocalBounds().reduced(10);
    auto header = a.removeFromTop(26);
    title.setBounds(header);

    auto left = a.removeFromLeft(a.proportionOfWidth(0.65f)).reduced(10);
    auto right = a.reduced(10);

    auto colW = left.getWidth() / 3;
    threshold.setBounds(left.removeFromLeft(colW).reduced(8)); // Stereo version
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
