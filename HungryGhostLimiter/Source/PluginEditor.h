#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include "PluginProcessor.h"


// ================= Look & Feel =================
struct VibeLNF : juce::LookAndFeel_V4
{
    VibeLNF();
};



// A pill-shaped vertical slider with warm gradient fill (for LinearBarVertical)
struct PillVSliderLNF : juce::LookAndFeel_V4
{
    // tweak these if you want a different vibe
    juce::Colour trackBgTop{ 0xFF1B1F27 }; // dark navy-ish
    juce::Colour trackBgBot{ 0xFF141821 };
    juce::Colour fillBottom{ 0xFFFFB34D }; // warm yellow
    juce::Colour fillTop{ 0xFFFF3B3B }; // hot red
    float        outlineAlpha{ 0.20f };      // subtle outline/gloss

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h,
        float sliderPos, float minPos, float maxPos,
        const juce::Slider::SliderStyle style,
        juce::Slider& s) override
    {
        juce::ignoreUnused(sliderPos, minPos, maxPos, style);

        // inner rect for the capsule; leave a little breathing room
        auto bounds = juce::Rectangle<float>(x, y, (float)w, (float)h).reduced(juce::jmax(w * 0.35f, 6.0f), 6.0f);

        // In TextBoxBelow mode JUCE already shrinks the slider area,
        // but if you see overlap, trim a bit more at the bottom:
        bounds.removeFromBottom(4.0f);

        const float radius = bounds.getWidth() * 0.5f;

        // === Track background (subtle vertical gradient) ===
        juce::ColourGradient trackGrad(trackBgTop, bounds.getX(), bounds.getY(),
            trackBgBot, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill(trackGrad);
        g.fillRoundedRectangle(bounds, radius);

        // === Fill amount (bottom up) ===
        const auto range = s.getRange();
        const double proportion = range.getLength() > 0.0 ? (s.getValue() - range.getStart()) / range.getLength() : 0.0;
        auto fill = bounds;
        fill.removeFromTop(fill.getHeight() * (float)(1.0 - juce::jlimit(0.0, 1.0, proportion)));

        juce::ColourGradient fillGrad(fillBottom, fill.getX(), fill.getBottom(),
            fillTop, fill.getX(), fill.getY(), false);
        g.setGradientFill(fillGrad);
        g.fillRoundedRectangle(fill, radius);

        // === Soft outline & gloss lines ===
        g.setColour(juce::Colours::white.withAlpha(outlineAlpha));
        g.drawRoundedRectangle(bounds, radius, 1.0f);

        // optional: top inner gloss
        auto gloss = bounds.withHeight(bounds.getHeight() * 0.20f);
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.fillRoundedRectangle(gloss, radius);
    }
};


// ================= Label + Vertical Slider =================
class LabelledVSlider : public juce::Component
{
public:
    explicit LabelledVSlider(const juce::String& title);

    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    juce::Label  label;
    juce::Slider slider;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LabelledVSlider)
};

// ================= Simple attenuation meter (0..12 dB) =================
class AttenMeter : public juce::Component
{
public:
    explicit AttenMeter(const juce::String& title);

    void setDb(float db);
    void paint(juce::Graphics& g) override;

private:
    float dbAtten = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AttenMeter)
};

// ================= Stereo Threshold (L/R with Link) =================
class StereoThreshold : public juce::Component
{
public:
    explicit StereoThreshold(HungryGhostLimiterAudioProcessor::APVTS& apvts);
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    void setSliderLookAndFeel(juce::LookAndFeel* lnf);

private:
    juce::Label  title{ {}, "THRESHOLD" };
    juce::Slider sliderL, sliderR;              // vertical bar sliders
    juce::Label  labelL{ {}, "L" }, labelR{ {}, "R" };
    juce::ToggleButton linkButton{ "Link" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>  attL, attR;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>  attLink;

    bool syncing = false; // prevent feedback loops when mirroring

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StereoThreshold)
};

// ================= Main Editor =================
class HungryGhostLimiterAudioProcessorEditor
    : public juce::AudioProcessorEditor
    , private juce::Timer
{
public:
    explicit HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p);
    ~HungryGhostLimiterAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    HungryGhostLimiterAudioProcessor& proc;
    VibeLNF lnf;

    juce::Label title;

    // NOTE: threshold is a StereoThreshold now
    StereoThreshold threshold;
    LabelledVSlider ceiling, release;

    AttenMeter attenMeter;
    juce::Label attenLabel;

    PillVSliderLNF pillLNF;

    // use pointers for simpler lifetime
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> clAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostLimiterAudioProcessorEditor)
};