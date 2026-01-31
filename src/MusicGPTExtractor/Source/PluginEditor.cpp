#include "PluginEditor.h"
#include <Foundation/ResourceResolver.h>

MusicGPTExtractorAudioProcessorEditor::MusicGPTExtractorAudioProcessorEditor(MusicGPTExtractorAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
{
    setLookAndFeel(&lnf);
    setResizable(false, false);
    setOpaque(true);
    setSize(Layout::kWindowWidth, Layout::kWindowHeight);

    // Logo header
    addAndMakeVisible(logoHeader);

    // Drop zone for audio files
    dropZone.setLabel("Drop audio file here to extract stems");
    dropZone.setAcceptedExtensions({ ".wav", ".mp3", ".aiff", ".flac", ".m4a" });
    dropZone.onFilesDropped = [this](const juce::StringArray& files) {
        onFilesDropped(files);
    };
    addAndMakeVisible(dropZone);

    // Status label
    statusLabel.setText("Ready", juce::dontSendNotification);
    statusLabel.setJustificationType(juce::Justification::centred);
    statusLabel.setColour(juce::Label::textColourId, Style::theme().text);
    addAndMakeVisible(statusLabel);

    // Configure sliders
    auto setupSlider = [this](juce::Slider& slider, juce::Label& label, const juce::String& text) {
        slider.setSliderStyle(juce::Slider::LinearVertical);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 50, 20);
        slider.setRange(0.0, 1.0, 0.01);
        addAndMakeVisible(slider);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        label.setColour(juce::Label::textColourId, Style::theme().text);
        addAndMakeVisible(label);
    };

    setupSlider(vocalsSlider, vocalsLabel, "Vocals");
    setupSlider(drumsSlider, drumsLabel, "Drums");
    setupSlider(bassSlider, bassLabel, "Bass");
    setupSlider(otherSlider, otherLabel, "Other");

    // APVTS attachments
    vocalsAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "vocalsLevel", vocalsSlider);
    drumsAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "drumsLevel", drumsSlider);
    bassAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "bassLevel", bassSlider);
    otherAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        proc.apvts, "otherLevel", otherSlider);

    // Transport buttons
    playButton.onClick = [this]() {
        proc.getStemPlayer().play();
    };
    stopButton.onClick = [this]() {
        proc.getStemPlayer().stop();
    };
    addAndMakeVisible(playButton);
    addAndMakeVisible(stopButton);

    // Load background image
    {
        using ui::foundation::ResourceResolver;
        bgImage = ResourceResolver::loadImageByNames({
            "background03_png",
            "background-03.png"
        });
    }

    startTimerHz(30);
}

MusicGPTExtractorAudioProcessorEditor::~MusicGPTExtractorAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void MusicGPTExtractorAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto& th = Style::theme();

    if (bgImage.isValid())
    {
        auto bounds = getLocalBounds().toFloat();
        const float scale = juce::jmax(bounds.getWidth() / bgImage.getWidth(),
                                        bounds.getHeight() / bgImage.getHeight());
        const float w = bgImage.getWidth() * scale;
        const float h = bgImage.getHeight() * scale;
        const float x = (bounds.getWidth() - w) * 0.5f;
        const float y = (bounds.getHeight() - h) * 0.5f;
        g.drawImage(bgImage, juce::Rectangle<float>(x, y, w, h));
    }
    else
    {
        g.fillAll(th.bg);
    }
}

void MusicGPTExtractorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(Layout::kPadding);

    // Header
    auto header = bounds.removeFromTop(Layout::kHeaderHeight);
    logoHeader.setBounds(header);

    bounds.removeFromTop(Layout::kGap);

    // Drop zone
    auto dropArea = bounds.removeFromTop(Layout::kDropZoneHeight);
    dropZone.setBounds(dropArea);

    bounds.removeFromTop(Layout::kGap);

    // Status
    auto statusArea = bounds.removeFromTop(Layout::kStatusHeight);
    statusLabel.setBounds(statusArea);

    bounds.removeFromTop(Layout::kGap);

    // Transport buttons
    auto transportArea = bounds.removeFromTop(Layout::kTransportHeight);
    auto buttonWidth = (transportArea.getWidth() - Layout::kGap) / 2;
    playButton.setBounds(transportArea.removeFromLeft(buttonWidth));
    transportArea.removeFromLeft(Layout::kGap);
    stopButton.setBounds(transportArea);

    bounds.removeFromTop(Layout::kGap);

    // Stem mixer sliders
    auto mixerArea = bounds;
    const int sliderWidth = (mixerArea.getWidth() - 3 * Layout::kGap) / 4;
    const int labelHeight = 24;

    auto setupSliderBounds = [&](juce::Slider& slider, juce::Label& label) {
        auto col = mixerArea.removeFromLeft(sliderWidth);
        label.setBounds(col.removeFromTop(labelHeight));
        slider.setBounds(col);
        mixerArea.removeFromLeft(Layout::kGap);
    };

    setupSliderBounds(vocalsSlider, vocalsLabel);
    setupSliderBounds(drumsSlider, drumsLabel);
    setupSliderBounds(bassSlider, bassLabel);
    setupSliderBounds(otherSlider, otherLabel);
}

void MusicGPTExtractorAudioProcessorEditor::timerCallback()
{
    updateExtractionProgress();
}

void MusicGPTExtractorAudioProcessorEditor::onFilesDropped(const juce::StringArray& files)
{
    if (files.isEmpty())
        return;

    juce::File audioFile(files[0]);
    if (!audioFile.existsAsFile())
        return;

    statusLabel.setText("Uploading: " + audioFile.getFileName(), juce::dontSendNotification);

    // Start extraction via API client
    extractionClient.extractStems(audioFile,
        [this](float progress) {
            // Progress callback (called from background thread)
            juce::MessageManager::callAsync([this, progress]() {
                extractionProgress = progress;
                statusLabel.setText("Extracting: " + juce::String(int(progress * 100)) + "%",
                    juce::dontSendNotification);
            });
        },
        [this](const ExtractionResult& result) {
            // Completion callback
            juce::MessageManager::callAsync([this, result]() {
                if (result.success)
                {
                    statusLabel.setText("Extraction complete!", juce::dontSendNotification);
                    proc.getStemPlayer().loadStems(result.vocalsPath, result.drumsPath,
                        result.bassPath, result.otherPath);
                }
                else
                {
                    statusLabel.setText("Error: " + result.errorMessage, juce::dontSendNotification);
                }
            });
        });
}

void MusicGPTExtractorAudioProcessorEditor::updateExtractionProgress()
{
    // UI updates driven by timer if needed
}
