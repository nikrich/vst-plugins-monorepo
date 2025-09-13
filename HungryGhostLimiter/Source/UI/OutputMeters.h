#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "../styling/Theme.h"
#include "Layout.h"

// Read-only stereo output meters (post-processing dBFS), with title and numeric readouts.
class OutputMeters : public juce::Component, private juce::Timer {
public:
    OutputMeters()
    {
        title.setText("OUTPUT", juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(title);

        for (auto* l : { &numL, &numR })
        {
            l->setJustificationType(juce::Justification::centred);
            l->setInterceptsMouseClicks(false, false);
            addAndMakeVisible(*l);
        }

        startTimerHz(30);
        updateLabels();
    }

    // Feed raw per-block dBFS values. Smoothing is applied internally.
    void setLevelsDbFs(float leftDb, float rightDb)
    {
        targetL = juce::jlimit(-60.0f, 0.0f, leftDb);
        targetR = juce::jlimit(-60.0f, 0.0f, rightDb);
    }

    void setSmoothing(float attackMs, float releaseMs)
    {
        atkMs = juce::jmax(1.0f, attackMs);
        relMs = juce::jmax(1.0f, releaseMs);
    }

    void resized() override
    {
        auto r = getLocalBounds();

        // Grid: Title, Bars, Numeric Labels
        juce::Grid g; using Track = juce::Grid::TrackInfo;
        g.templateColumns = { Track(juce::Grid::Fr(1)), Track(juce::Grid::Fr(1)) };
        g.templateRows = {
            Track(juce::Grid::Px(Layout::kTitleRowHeightPx)),
            Track(juce::Grid::Px(Layout::kLargeSliderRowHeightPx)),
            Track(juce::Grid::Px(Layout::kChannelLabelRowHeightPx))
        };
        g.rowGap = juce::Grid::Px(Layout::kRowGapPx);
        g.columnGap = juce::Grid::Px(Layout::kBarGapPx);

        auto tItem = juce::GridItem(title).withMargin(Layout::kCellMarginPx).withArea(1, 1, 2, 3);

        // record bar areas in second row across both columns
        // We'll compute from the grid later by performing a provisional layout.
        g.items = { tItem,
                    juce::GridItem(dummyL).withArea(2, 1),
                    juce::GridItem(dummyR).withArea(2, 2),
                    juce::GridItem(numL).withMargin(Layout::kCellMarginPx).withArea(3, 1),
                    juce::GridItem(numR).withMargin(Layout::kCellMarginPx).withArea(3, 2) };

        // Perform layout on a temp copy to extract dummy bounds
        auto area = r;
        g.performLayout(area);
        barAreaL = dummyL.getBounds();
        barAreaR = dummyR.getBounds();

        // Now remove dummies from hierarchy (they'll never be shown)
        dummyL.setBounds(0,0,0,0); dummyR.setBounds(0,0,0,0);

        // Position numeric labels now (title already placed by grid)
        // Re-run to ensure children land where expected
        g.performLayout(r);
    }

    void paint(juce::Graphics& g) override
    {
        auto& th = Style::theme();

        auto drawBar = [&](juce::Rectangle<int> bar, float db)
        {
            if (bar.isEmpty()) return;
            auto bf = bar.reduced(6).toFloat();
            const float radius = Style::theme().borderRadius;

            // Track
            juce::ColourGradient trackGrad(th.trackTop, bf.getX(), bf.getY(),
                                           th.trackBot, bf.getX(), bf.getBottom(), false);
            g.setGradientFill(trackGrad);
            g.fillRoundedRectangle(bf, radius);

            // Fill based on dBFS (map -60..0 -> 0..1)
            const float norm = juce::jlimit(0.0f, 1.0f, (db + 60.0f) / 60.0f);
            if (norm > 0.001f)
            {
                auto fill = bf.withY(bf.getBottom() - bf.getHeight() * norm)
                              .withHeight(bf.getHeight() * norm);
                // Typical output meter colours: green -> yellow -> red (bottom -> top)
                juce::Colour bottomCol = juce::Colours::limegreen;
                juce::Colour topCol    = juce::Colours::red;
                // Single gradient will blend through yellow naturally
                juce::ColourGradient fillGrad(bottomCol, fill.getX(), fill.getBottom(),
                                              topCol,    fill.getX(), fill.getY(), false);
                g.setGradientFill(fillGrad);
                g.fillRect(fill);
            }
        };

        drawBar(barAreaL, dispL);
        drawBar(barAreaR, dispR);
    }

private:
    void updateLabels()
    {
        auto fmt = [](float db){ return db <= -59.5f ? juce::String("-inf dBFS")
                                                     : juce::String(db, 1) + " dBFS"; };
        numL.setText(fmt(dispL), juce::dontSendNotification);
        numR.setText(fmt(dispR), juce::dontSendNotification);
    }

    void timerCallback() override
    {
        constexpr float dtMs = 1000.0f / 30.0f;
        auto smooth = [&](float target, float& disp){
            const bool rising = (target > disp);
            const float tau = rising ? atkMs : relMs;
            const float alpha = 1.0f - std::exp(-dtMs / juce::jmax(1.0f, tau));
            disp += alpha * (target - disp);
        };
        smooth(targetL, dispL);
        smooth(targetR, dispR);
        updateLabels();
        repaint();
    }

    // UI children
    juce::Label title, numL, numR;
    juce::Component dummyL, dummyR; // invisible placeholders to compute bar rectangles via Grid

    // Geometry
    juce::Rectangle<int> barAreaL, barAreaR;

    // Smoothing state
    float targetL { -60.0f }, targetR { -60.0f };
    float dispL   { -60.0f }, dispR   { -60.0f };
    float atkMs { 40.0f }, relMs { 160.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OutputMeters)
};

