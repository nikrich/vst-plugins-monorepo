#include "PluginEditor.h"
#include "BinaryData.h"

HungryGhostLimiterAudioProcessorEditor::HungryGhostLimiterAudioProcessorEditor(HungryGhostLimiterAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
    , inputsCol(proc.apvts)
    , thrCeilCol(proc.apvts)
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

    // Skin Input, Threshold & Ceiling with the pill L&F
    inputsCol.setSliderLookAndFeel(&pillLNF);
    thrCeilCol.setSliderLookAndFeel(&pillLNF);

    addAndMakeVisible(inputsCol);
    addAndMakeVisible(thrCeilCol);
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

    // --- Content: 4 columns laid out with FlexBox ---
    const int required = Layout::kTotalColsWidthPx;
    auto content = bounds.withWidth(juce::jmin(required, bounds.getWidth()));
    content.setX(bounds.getX() + (bounds.getWidth() - content.getWidth()) / 2);

    juce::FlexBox row;
    row.flexDirection = juce::FlexBox::Direction::row;
    row.alignContent  = juce::FlexBox::AlignContent::stretch;

    auto addCol = [&](juce::Component& c, int basisPx, float grow, bool first)
    {
        auto it = juce::FlexItem(c).withWidth((float)basisPx).withFlex(grow);
        it.margin = first ? juce::FlexItem::Margin{ (float)Layout::kCellMarginPx } : juce::FlexItem::Margin{ (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kCellMarginPx, (float)Layout::kColGapPx };
        row.items.add(it);
    };

    addCol(inputsCol,  Layout::kColWidthThresholdPx, 0.0f, true);
    addCol(thrCeilCol, Layout::kColWidthCeilingPx,   0.0f, false);
    addCol(controlsCol,Layout::kColWidthControlPx,   1.0f, false);
    addCol(meterCol,   Layout::kColWidthMeterPx,     0.0f, false);

    row.performLayout(content);
}

void HungryGhostLimiterAudioProcessorEditor::timerCallback()
{
    meterCol.setDb(proc.getSmoothedAttenDb()); // feed raw or lightly-smoothed dB
}
