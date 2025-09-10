#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"
#include "../PluginProcessor.h"

class AdvancedPanel : public juce::Component {
public:
    explicit AdvancedPanel(HungryGhostLimiterAudioProcessor::APVTS& apvts)
    {
        setInterceptsMouseClicks(true, true);
        title.setText("ADVANCED", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(title);

        // Quantize
        qLabel.setText("QUANTIZE (Bits)", juce::dontSendNotification);
        addAndMakeVisible(qLabel);
        for (auto* b : { &q24, &q20, &q16, &q12, &q8 }) { addAndMakeVisible(*b); }
        q24.setButtonText("24"); q20.setButtonText("20"); q16.setButtonText("16"); q12.setButtonText("12"); q8.setButtonText("8");

        // Dither
        dLabel.setText("DITHER", juce::dontSendNotification);
        addAndMakeVisible(dLabel);
        for (auto* b : { &dT1, &dT2 }) { addAndMakeVisible(*b); }
        dT1.setButtonText("T1"); dT2.setButtonText("T2");

        // Shaping
        sLabel.setText("SHAPING", juce::dontSendNotification);
        addAndMakeVisible(sLabel);
        for (auto* b : { &sNone, &sArc }) { addAndMakeVisible(*b); }
        sNone.setButtonText("—"); sArc.setButtonText("◠");

        // Domain
        domLabel.setText("DOMAIN", juce::dontSendNotification);
        addAndMakeVisible(domLabel);
        for (auto* b : { &domDigital, &domAnalog, &domTruePeak }) { addAndMakeVisible(*b); }
        domDigital.setButtonText("Digital"); domAnalog.setButtonText("Analog"); domTruePeak.setButtonText("TruePeak");

        // Attachments
        attQ24 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "q24", q24);
        attQ20 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "q20", q20);
        attQ16 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "q16", q16);
        attQ12 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "q12", q12);
        attQ8  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "q8", q8);

        attDT1 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "dT1", dT1);
        attDT2 = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "dT2", dT2);

        attSNone = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "sNone", sNone);
        attSArc  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "sArc", sArc);

        attDomDig  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "domDigital", domDigital);
        attDomAna  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "domAnalog", domAnalog);
        attDomTP   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(apvts, "domTruePeak", domTruePeak);

        // Mutual exclusivity (per group)
        auto exclusive = [](juce::ToggleButton& self, std::initializer_list<juce::ToggleButton*> others)
        {
            self.onClick = [&]() {
                if (self.getToggleState())
                    for (auto* b : others) b->setToggleState(false, juce::sendNotificationSync);
            };
        };

        exclusive(q24, { &q20, &q16, &q12, &q8 });
        exclusive(q20, { &q24, &q16, &q12, &q8 });
        exclusive(q16, { &q24, &q20, &q12, &q8 });
        exclusive(q12, { &q24, &q20, &q16, &q8 });
        exclusive(q8,  { &q24, &q20, &q16, &q12 });

        exclusive(dT1, { &dT2 });
        exclusive(dT2, { &dT1 });

        exclusive(sNone, { &sArc });
        exclusive(sArc,  { &sNone });

        exclusive(domDigital, { &domAnalog, &domTruePeak });
        exclusive(domAnalog, { &domDigital, &domTruePeak });
        exclusive(domTruePeak, { &domDigital, &domAnalog });
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced(6);
        title.setBounds(r.removeFromTop(24));

        auto layoutRow = [&](juce::Label& lbl, std::initializer_list<juce::ToggleButton*> btns) {
            auto row = r.removeFromTop(28);
            auto lab = row.removeFromLeft(140);
            lbl.setBounds(lab);
            int bw = 64, gap = 6;
            for (auto* b : btns) {
                b->setBounds(row.removeFromLeft(bw).reduced(2));
                row.removeFromLeft(gap);
            }
            r.removeFromTop(4);
        };

        layoutRow(qLabel, { &q24, &q20, &q16, &q12, &q8 });
        layoutRow(dLabel, { &dT1, &dT2 });
        layoutRow(sLabel, { &sNone, &sArc });
        layoutRow(domLabel, { &domDigital, &domAnalog, &domTruePeak });
    }

private:
    juce::Label title;

    juce::Label qLabel, dLabel, sLabel, domLabel;
    juce::ToggleButton q24, q20, q16, q12, q8;
    juce::ToggleButton dT1, dT2;
    juce::ToggleButton sNone, sArc;
    juce::ToggleButton domDigital, domAnalog, domTruePeak;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attQ24, attQ20, attQ16, attQ12, attQ8;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attDT1, attDT2;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attSNone, attSArc;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attDomDig, attDomAna, attDomTP;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedPanel)
};

