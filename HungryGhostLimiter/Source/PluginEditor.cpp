#include "PluginEditor.h"
#include <BinaryData.h>
#include <Foundation/ResourceResolver.h>

HungryGhostLimiterAudioProcessorEditor::HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
    , inputsCol(proc.apvts)
    , threshold(proc.apvts)
    , ceiling(proc.apvts)
    , controlsCol(proc.apvts, &donutLNF, &pillLNF, &neonToggleLNF)
    , meterCol()
    , outputCol(proc.apvts)
    , advanced(proc.apvts)
{
    setLookAndFeel(&lnf);
    setResizable(false, false);
    setOpaque(true);
    setSize(Layout::kTotalColsWidthPx + 2 * Layout::kPaddingPx, 520);

    // Sections visible
    addAndMakeVisible(logoHeader);
    // addAndMakeVisible(advanced);

    // Skin Input, Threshold & Ceiling with the pill L&F
    inputsCol.setSliderLookAndFeel(&pillLNF);
    threshold.setSliderLookAndFeel(&pillLNF);
    ceiling.setSliderLookAndFeel(&pillLNF);

    // Apply neon toggle style to link buttons
    inputsCol.getInput().setLinkLookAndFeel(&neonToggleLNF);
    threshold.setLinkLookAndFeel(&neonToggleLNF);
    ceiling.setLinkLookAndFeel(&neonToggleLNF);
    outputCol.setSliderLookAndFeel(&pillLNF);
    addAndMakeVisible(inputsCol);
    addAndMakeVisible(threshold);
    addAndMakeVisible(ceiling);
    addAndMakeVisible(controlsCol);
    addAndMakeVisible(meterCol);
    addAndMakeVisible(outputCol);

    // Load kit-03 background-03 into memory via ResourceResolver
    {
        using ui::foundation::ResourceResolver;
        bgCardImage = ResourceResolver::loadImageByNames({
            "background03_png",
            "background_03_png",
            "background-03.png",
            "assets/ui/kit-03/background/background-03.png"
        });
    }

    startTimerHz(30);
}

HungryGhostLimiterAudioProcessorEditor::~HungryGhostLimiterAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void HungryGhostLimiterAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto& th = Style::theme();
    g.fillAll(th.bg);

    auto padded = getLocalBounds().reduced(Layout::kPaddingPx);

    // Footer background (advanced section)
    auto footer = padded.removeFromBottom(Layout::kFooterHeightPx);
    g.setColour(juce::Colour(0xFF0C0C0C)); // #0C0C0C
    g.fillRect(footer);

    // Main card (logo + main columns)
    auto mainCard = padded; // remaining area after removing footer

    // If no background image, draw the card with drop shadow as before
    auto radius = Style::theme().borderRadius;
    auto bw     = Style::theme().borderWidth;
    if (!bgCardImage.isValid())
    {
        juce::DropShadow ds(juce::Colours::black.withAlpha(0.55f), 22, {});
        ds.drawForRectangle(g, mainCard); // draw shadow around card

        g.setColour(juce::Colour(0xFF301935));
        g.fillRoundedRectangle(mainCard.toFloat(), radius);

        // Border
        g.setColour(juce::Colours::white.withAlpha(0.12f));
        g.drawRoundedRectangle(mainCard.toFloat(), radius, bw);
    }
    else
    {
        // Fill the entire plugin area with the background image
        auto bgRect = getLocalBounds().toFloat();

        const float srcW = (float) bgCardImage.getWidth();
        const float srcH = (float) bgCardImage.getHeight();
        const float dstW = bgRect.getWidth();
        const float dstH = bgRect.getHeight();

        // Cover-fit so the image fills the rect completely (may crop on one axis)
        const float scale = juce::jmax(dstW / srcW, dstH / srcH);
        const float w = srcW * scale;
        const float h = srcH * scale;
        const float x = bgRect.getX() + (dstW - w) * 0.5f;
        const float y = bgRect.getY() + (dstH - h) * 0.5f;
        juce::Rectangle<float> dst(x, y, w, h);

        g.drawImage(bgCardImage, dst);
    }
}

void HungryGhostLimiterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(Layout::kPaddingPx);

    // --- Header ---
    auto header = bounds.removeFromTop(Layout::kHeaderHeightPx);
    logoHeader.setBounds(header);

    // --- Footer (Advanced Controls) ---
    // auto footer = bounds.removeFromBottom(Layout::kFooterHeightPx);
    // advanced.setBounds(footer);

    // --- Main content: 6 columns in a Grid ---
    const int required = Layout::kTotalColsWidthPx;
    auto main = bounds.withWidth(juce::jmin(required, bounds.getWidth()));
    main.setX(bounds.getX() + (bounds.getWidth() - main.getWidth()) / 2);

    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    grid.templateColumns = {
        Track(juce::Grid::Px(Layout::kColWidthInputsPx)),
        Track(juce::Grid::Px(Layout::kColWidthThresholdPx)),
        Track(juce::Grid::Px(Layout::kColWidthCeilingPx)),
        Track(juce::Grid::Px(Layout::kColWidthControlPx)),
        Track(juce::Grid::Px(Layout::kColWidthMeterPx)),
        Track(juce::Grid::Px(Layout::kColWidthOutputPx))
    };
    grid.templateRows = { Track(juce::Grid::Fr(1)) };
    grid.columnGap = juce::Grid::Px(Layout::kColGapPx);

    grid.items = {
        juce::GridItem(inputsCol).withMargin(Layout::kCellMarginPx),
        juce::GridItem(threshold).withMargin(Layout::kCellMarginPx),
        juce::GridItem(ceiling).withMargin(Layout::kCellMarginPx),
        juce::GridItem(controlsCol).withMargin(Layout::kCellMarginPx),
        juce::GridItem(meterCol).withMargin(Layout::kCellMarginPx),
        juce::GridItem(outputCol).withMargin(Layout::kCellMarginPx)
    };

    grid.performLayout(main);
}

void HungryGhostLimiterAudioProcessorEditor::timerCallback()
{
    meterCol.setDb(proc.getSmoothedAttenDb()); // feed raw or lightly-smoothed dB
    // Update output live levels (dBFS) -> Output column smooths and displays
    outputCol.setLevelsDbFs(proc.getOutDbL(), proc.getOutDbR());
}
