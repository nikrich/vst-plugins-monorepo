#include "PluginEditor.h"
#include "BinaryData.h"

HungryGhostLimiterAudioProcessorEditor::HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
    , inputsCol(proc.apvts)
    , ceiling(proc.apvts)
    , controlsCol(proc.apvts, &donutLNF, &pillLNF, &neonToggleLNF)
    , meterCol()
{
    setLookAndFeel(&lnf);
    setResizable(false, false);
    setOpaque(true);
    setSize(760, 640); // increased height to ensure Threshold is visible

    int logoSize = 0;
    if (const auto* logoData = BinaryData::getNamedResource("logo_png", logoSize))
    {
        auto logoImage = juce::ImageFileFormat::loadFrom(logoData, (size_t) logoSize);
        logoComp.setImage(logoImage, juce::RectanglePlacement::centred);
    }
    logoComp.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(logoComp);

    // optional text under the logo
    logoSub.setText("LIMITER", juce::dontSendNotification);
    logoSub.setJustificationType(juce::Justification::centred);
    logoSub.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.9f));
    logoSub.setFont(juce::Font(juce::FontOptions(13.0f)));
    logoSub.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(logoSub);

    // hide the old title if you had one
    title.setVisible(false);

    // Skin Input & Threshold L/R with the pill L&F
    inputsCol.setSliderLookAndFeel(&pillLNF);
    ceiling.setSliderLookAndFeel(&pillLNF);

    addAndMakeVisible(inputsCol);
    addAndMakeVisible(ceiling);
    addAndMakeVisible(controlsCol);
    addAndMakeVisible(meterCol);

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
    const auto content = padded.withTrimmedTop(Layout::kHeaderHeightPx);

    const int required = Layout::kTotalColsWidthPx;
    auto rowBounds = content.withWidth(juce::jmin(required, content.getWidth()));
    rowBounds.setX(content.getX() + (content.getWidth() - rowBounds.getWidth()) / 2);

    // meter area is the rightmost column
    auto meterArea = rowBounds.removeFromRight(Layout::kColWidthMeterPx);

    juce::DropShadow ds(juce::Colours::black.withAlpha(0.45f), 18, {});
    ds.drawForRectangle(g, meterArea.reduced(4));
    g.setColour(th.panel);
    g.fillRoundedRectangle(meterArea.reduced(4).toFloat(), 12.0f);
}

void HungryGhostLimiterAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(Layout::kPaddingPx);

    // --- Header (logo + subtext) ---
    auto header = bounds.removeFromTop(Layout::kHeaderHeightPx);
    int logoH = header.getHeight() - 20;
    int logoW = 320;
    if (auto img = logoComp.getImage(); img.isValid())
        logoW = (int)std::round(img.getWidth() * (logoH / (double)img.getHeight()));

    auto logoBounds = header.withSizeKeepingCentre(logoW, logoH);
    logoComp.setBounds(logoBounds);

    logoSub.setBounds(logoBounds.withY(logoBounds.getBottom() - 16).withHeight(16));

    // --- Content: 4 fixed-width columns ---
    const int required = Layout::kTotalColsWidthPx;
    auto rowBounds = bounds.withWidth(juce::jmin(required, bounds.getWidth()));
    rowBounds.setX(bounds.getX() + (bounds.getWidth() - rowBounds.getWidth()) / 2);

    juce::Grid grid;
    using Track = juce::Grid::TrackInfo;
    grid.templateColumns = {
        Track(juce::Grid::Px(Layout::kColWidthThresholdPx)),
        Track(juce::Grid::Px(Layout::kColWidthCeilingPx)),
        Track(juce::Grid::Px(Layout::kColWidthControlPx)),
        Track(juce::Grid::Px(Layout::kColWidthMeterPx))
    };
    grid.templateRows = { Track(juce::Grid::Fr(1)) };
    grid.columnGap = juce::Grid::Px(Layout::kColGapPx);
    grid.items = {
        juce::GridItem(inputsCol).withMargin(Layout::kCellMarginPx),
        juce::GridItem(ceiling).withMargin(Layout::kCellMarginPx),
        juce::GridItem(controlsCol).withMargin(Layout::kCellMarginPx),
        juce::GridItem(meterCol).withMargin(Layout::kCellMarginPx)
    };
    grid.performLayout(rowBounds);
}

void HungryGhostLimiterAudioProcessorEditor::timerCallback()
{
    meterCol.setDb(proc.getSmoothedAttenDb()); // feed raw or lightly-smoothed dB
}
