#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "Layout.h"
#include "../PluginProcessor.h"

class AdvancedPanel : public juce::Component {
public:
    explicit AdvancedPanel(HungryGhostLimiterAudioProcessor::APVTS& apvts)
    {
        setInterceptsMouseClicks(true, true);

        // Labels: centered as card headers
        qLabel.setText("QUANTIZE", juce::dontSendNotification);
        dLabel.setText("DITHER", juce::dontSendNotification);
        sLabel.setText("SHAPING", juce::dontSendNotification);
        domLabel.setText("Domain", juce::dontSendNotification);
        for (auto* l : { &qLabel, &dLabel, &sLabel, &domLabel })
        {
            l->setJustificationType(juce::Justification::centred);
            l->setInterceptsMouseClicks(false, false);
            l->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
            l->setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
            addAndMakeVisible(*l);
        }

        // Quantize buttons
        for (auto* b : { &q24, &q20, &q16, &q12, &q8 }) { addAndMakeVisible(*b); }
        q24.setButtonText("24"); q20.setButtonText("20"); q16.setButtonText("16"); q12.setButtonText("12"); q8.setButtonText("8");

        // Dither
        for (auto* b : { &dT1, &dT2 }) { addAndMakeVisible(*b); }
        dT1.setButtonText("T1"); dT2.setButtonText("T2");

        // Shaping
        for (auto* b : { &sNone, &sArc }) { addAndMakeVisible(*b); }
        sNone.setButtonText("—"); sArc.setButtonText("◠");

        // Domain
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
            auto selfPtr = &self; // capture a stable pointer to the button itself
            std::vector<juce::ToggleButton*> peers(others.begin(), others.end()); // copy peers to avoid dangling refs
            self.onClick = [selfPtr, peers]() {
                if (selfPtr->getToggleState())
                    for (auto* b : peers)
                        if (b != nullptr)
                            b->setToggleState(false, juce::sendNotificationSync);
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

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().reduced(6);
        const int gap = Layout::kColGapPx;
        int cardW = (area.getWidth() - 3 * gap) / 4;

        auto r = area;
        auto q = r.removeFromLeft(cardW); r.removeFromLeft(gap);
        auto d = r.removeFromLeft(cardW); r.removeFromLeft(gap);
        auto s = r.removeFromLeft(cardW); r.removeFromLeft(gap);
        auto m = r.removeFromLeft(cardW);

        auto drawCard = [&](juce::Rectangle<int> box) {
            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.fillRoundedRectangle(box.toFloat(), 8.0f);
            g.setColour(juce::Colours::white.withAlpha(0.7f));
            g.drawRoundedRectangle(box.toFloat(), 8.0f, 2.0f);
        };

        drawCard(qCard = q);
        drawCard(dCard = d);
        drawCard(sCard = s);
        drawCard(mCard = m);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(6);
        const int gap = Layout::kColGapPx;
        int cardW = (area.getWidth() - 3 * gap) / 4;

        auto r = area;
        auto q = r.removeFromLeft(cardW); r.removeFromLeft(gap);
        auto d = r.removeFromLeft(cardW); r.removeFromLeft(gap);
        auto s = r.removeFromLeft(cardW); r.removeFromLeft(gap);
        auto m = r.removeFromLeft(cardW);

        auto layoutGroup = [&](juce::Rectangle<int> box,
                               juce::Label& header,
                               std::initializer_list<juce::ToggleButton*> btns)
        {
            auto inner = box.reduced(12);
            header.setBounds(inner.removeFromTop(24));
            inner.removeFromTop(6);
            // Layout buttons centered in a row
            int totalBtn = (int)btns.size();
            int bw = 60;
            int gapB = 8;
            int rowW = totalBtn * bw + (totalBtn - 1) * gapB;
            auto row = inner.withWidth(rowW).withX(inner.getX() + (inner.getWidth() - rowW) / 2)
                             .removeFromTop(28);
            for (auto* b : btns) {
                b->setBounds(row.removeFromLeft(bw));
                row.removeFromLeft(gapB);
            }
        };

        layoutGroup(qCard = q, qLabel, { &q24, &q20, &q16, &q12, &q8 });
        layoutGroup(dCard = d, dLabel, { &dT1, &dT2 });
        layoutGroup(sCard = s, sLabel, { &sNone, &sArc });
        layoutGroup(mCard = m, domLabel, { &domDigital, &domAnalog, &domTruePeak });
    }

private:
    // Card rectangles for painting
    juce::Rectangle<int> qCard, dCard, sCard, mCard;

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

