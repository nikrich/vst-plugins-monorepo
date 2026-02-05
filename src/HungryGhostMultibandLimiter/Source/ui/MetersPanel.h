#pragma once
#include <JuceHeader.h>
#include <Charts/VerticalMeter.h>
#include "GRMeter.h"

/** MetersPanel: Organize per-band and master level meters with gain reduction display
    Layout: Per-band input | GR | output meters, followed by master input | output
    Designed for 2-band limiter (M1), ready to scale to N bands in M2 */
class MetersPanel : public juce::Component
{
public:
    MetersPanel()
    {
        // Create per-band meters (2 bands for M1)
        for (int b = 0; b < 2; ++b)
        {
            // Input level meter
            bandInputMeters.push_back(std::make_unique<VerticalMeter>());
            bandInputMeters[b]->setName(juce::String("Band ") + juce::String(b + 1) + " In");
            addAndMakeVisible(*bandInputMeters[b]);

            // GR meter
            bandGrMeters.push_back(std::make_unique<GRMeter>());
            bandGrMeters[b]->setName(juce::String("Band ") + juce::String(b + 1) + " GR");
            addAndMakeVisible(*bandGrMeters[b]);

            // Output level meter
            bandOutputMeters.push_back(std::make_unique<VerticalMeter>());
            bandOutputMeters[b]->setName(juce::String("Band ") + juce::String(b + 1) + " Out");
            addAndMakeVisible(*bandOutputMeters[b]);
        }

        // Master meters
        masterInputMeter = std::make_unique<VerticalMeter>();
        masterInputMeter->setName("Master In");
        addAndMakeVisible(*masterInputMeter);

        masterOutputMeter = std::make_unique<VerticalMeter>();
        masterOutputMeter->setName("Master Out");
        addAndMakeVisible(*masterOutputMeter);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xFF1a1a1a));  // Dark background
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        const int bandCount = 2;  // M1 constraint
        const int meterWidth = 20;
        const int spacing = 2;
        const int grMeterWidth = 24;

        // Calculate widths: (input + spacing + gr + spacing + output) per band + spacing + master
        int x = 4;

        // Per-band meters
        for (int b = 0; b < bandCount; ++b)
        {
            // Input meter
            bandInputMeters[b]->setBounds(x, 4, meterWidth, bounds.getHeight() - 8);
            x += meterWidth + spacing;

            // GR meter
            bandGrMeters[b]->setBounds(x, 4, grMeterWidth, bounds.getHeight() - 8);
            x += grMeterWidth + spacing;

            // Output meter
            bandOutputMeters[b]->setBounds(x, 4, meterWidth, bounds.getHeight() - 8);
            x += meterWidth + spacing * 2;  // Extra spacing between bands
        }

        // Master meters (right side)
        int masterX = bounds.getRight() - (meterWidth * 2) - spacing - 4;
        masterInputMeter->setBounds(masterX, 4, meterWidth, bounds.getHeight() - 8);
        masterOutputMeter->setBounds(masterX + meterWidth + spacing, 4, meterWidth, bounds.getHeight() - 8);
    }

    /** Feed per-band input levels (dBFS) */
    void setBandInputDb(int bandIndex, float db)
    {
        if (bandIndex >= 0 && bandIndex < (int)bandInputMeters.size())
            bandInputMeters[bandIndex]->setDb(db);
    }

    /** Feed per-band gain reduction levels (dB) */
    void setBandGrDb(int bandIndex, float db)
    {
        if (bandIndex >= 0 && bandIndex < (int)bandGrMeters.size())
            bandGrMeters[bandIndex]->setGRdB(db);
    }

    /** Feed per-band output levels (dBFS) */
    void setBandOutputDb(int bandIndex, float db)
    {
        if (bandIndex >= 0 && bandIndex < (int)bandOutputMeters.size())
            bandOutputMeters[bandIndex]->setDb(db);
    }

    /** Feed master input level (dBFS) */
    void setMasterInputDb(float db)
    {
        if (masterInputMeter)
            masterInputMeter->setDb(db);
    }

    /** Feed master output level (dBFS) */
    void setMasterOutputDb(float db)
    {
        if (masterOutputMeter)
            masterOutputMeter->setDb(db);
    }

private:
    // Per-band meters (up to 6 for future expansion)
    std::vector<std::unique_ptr<VerticalMeter>> bandInputMeters;
    std::vector<std::unique_ptr<GRMeter>> bandGrMeters;
    std::vector<std::unique_ptr<VerticalMeter>> bandOutputMeters;

    // Master meters
    std::unique_ptr<VerticalMeter> masterInputMeter;
    std::unique_ptr<VerticalMeter> masterOutputMeter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MetersPanel)
};
