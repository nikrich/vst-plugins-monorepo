#include "PluginEditor.h"
#include "BinaryData.h"

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
    setSize(Layout::kTotalColsWidthPx + 2 * Layout::kPaddingPx, 640);

    // Sections visible
    addAndMakeVisible(logoHeader);
    addAndMakeVisible(advanced);

    // Skin Input, Threshold & Ceiling with the pill L&F
    inputsCol.setSliderLookAndFeel(&pillLNF);
    threshold.setSliderLookAndFeel(&pillLNF);
    ceiling.setSliderLookAndFeel(&pillLNF);
    outputCol.setSliderLookAndFeel(&pillLNF);
    addAndMakeVisible(inputsCol);
    addAndMakeVisible(threshold);
    addAndMakeVisible(ceiling);
    addAndMakeVisible(controlsCol);
    addAndMakeVisible(meterCol);
    addAndMakeVisible(outputCol);

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

    // subtle panel behind meter column (exact match with layout)
    const auto padded = getLocalBounds().reduced(Layout::kPaddingPx);
    const auto content = padded.withTrimmedTop(Layout::kHeaderHeightPx).withTrimmedBottom(Layout::kFooterHeightPx);

    const int required = Layout::kTotalColsWidthPx;
    auto rowBounds = content.withWidth(juce::jmin(required, content.getWidth()));
    rowBounds.setX(content.getX() + (content.getWidth() - rowBounds.getWidth()) / 2);

    // meter area is column 5 from right side when Output exists; easier: compute by subtracting right output width + gaps
    auto tmp = rowBounds;
    tmp.removeFromRight(Layout::kColWidthOutputPx); // output col
    tmp.removeFromRight(Layout::kColGapPx);         // gap between meter and output
    auto meterArea = tmp.removeFromRight(Layout::kColWidthMeterPx);

    juce::DropShadow ds(juce::Colours::black.withAlpha(0.45f), 18, {});
    ds.drawForRectangle(g, meterArea.reduced(4));
    g.setColour(th.panel);
    g.fillRoundedRectangle(meterArea.reduced(4).toFloat(), 12.0f);
}

void HungryGhostLimiterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(Layout::kPaddingPx);

    // --- Header ---
    auto header = bounds.removeFromTop(Layout::kHeaderHeightPx);
    logoHeader.setBounds(header);

    // --- Footer (Advanced Controls) ---
    auto footer = bounds.removeFromBottom(Layout::kFooterHeightPx);
    advanced.setBounds(footer);

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
