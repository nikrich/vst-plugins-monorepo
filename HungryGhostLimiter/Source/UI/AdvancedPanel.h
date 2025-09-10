#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"

class AdvancedPanel : public juce::Component {
public:
    AdvancedPanel()
    {
        setInterceptsMouseClicks(true, true);
        title.setText("ADVANCED", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
        addAndMakeVisible(title);

        // Quantize (UI-only)
        qLabel.setText("QUANTIZE (Bits)", juce::dontSendNotification);
        addAndMakeVisible(qLabel);
        for (auto* b : { &q24, &q20, &q16, &q12, &q8 }) { addAndMakeVisible(*b); }
        q24.setToggleState(true, juce::dontSendNotification);
        q24.setButtonText("24"); q20.setButtonText("20"); q16.setButtonText("16"); q12.setButtonText("12"); q8.setButtonText("8");

        // Dither (UI-only)
        dLabel.setText("DITHER", juce::dontSendNotification);
        addAndMakeVisible(dLabel);
        for (auto* b : { &dT1, &dT2 }) { addAndMakeVisible(*b); }
        dT2.setToggleState(true, juce::dontSendNotification);
        dT1.setButtonText("T1"); dT2.setButtonText("T2");

        // Shaping (UI-only)
        sLabel.setText("SHAPING", juce::dontSendNotification);
        addAndMakeVisible(sLabel);
        for (auto* b : { &sNone, &sArc }) { addAndMakeVisible(*b); }
        sArc.setToggleState(true, juce::dontSendNotification);
        sNone.setButtonText("—"); sArc.setButtonText("◠");

        // Domain (UI-only)
        domLabel.setText("DOMAIN", juce::dontSendNotification);
        addAndMakeVisible(domLabel);
        for (auto* b : { &domDigital, &domAnalog, &domTruePeak }) { addAndMakeVisible(*b); }
        domTruePeak.setToggleState(true, juce::dontSendNotification);
        domDigital.setButtonText("Digital"); domAnalog.setButtonText("Analog"); domTruePeak.setButtonText("TruePeak");

        // Disable to indicate placeholder nature
        for (auto* b : { &q24, &q20, &q16, &q12, &q8, &dT1, &dT2, &sNone, &sArc, &domDigital, &domAnalog, &domTruePeak })
            b->setEnabled(false);
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdvancedPanel)
};

