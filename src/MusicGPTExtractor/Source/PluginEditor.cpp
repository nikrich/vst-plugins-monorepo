#include "PluginEditor.h"
#include <BinaryData.h>
#include <Foundation/ResourceResolver.h>

MusicGPTExtractorAudioProcessorEditor::MusicGPTExtractorAudioProcessorEditor(MusicGPTExtractorAudioProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , proc(p)
{
    setResizable(false, false);
    setOpaque(true);
    setSize(Layout::kWindowWidth, Layout::kWindowHeight);

    // Register audio formats for thumbnail loading
    formatManager.registerBasicFormats();

    // Header and settings button
    addAndMakeVisible(logoHeader);

    settingsButton.setButtonText("Settings");
    settingsButton.setColour(juce::TextButton::buttonColourId, Style::theme().panel);
    settingsButton.setColour(juce::TextButton::textColourOnId, Style::theme().text);
    settingsButton.setColour(juce::TextButton::textColourOffId, Style::theme().text);
    settingsButton.onClick = [this]() { showSettingsPanel(); };
    addAndMakeVisible(settingsButton);

    // Transport bar
    addAndMakeVisible(transportBar);

    // Drop zone for file input
    dropZone.setLabel("Drop audio file to extract stems");
    dropZone.setAcceptedExtensions({ "wav", "mp3", "flac", "aiff", "aif", "ogg", "m4a" });
    dropZone.onFilesDropped = [this](const juce::StringArray& files) {
        handleFilesDropped(files);
    };
    addAndMakeVisible(dropZone);

    // Stem track list (hidden initially)
    stemTrackList.setVisible(false);
    addAndMakeVisible(stemTrackList);

    // Connect transport callbacks
    transportBar.onPlayPauseChanged = [this](bool isPlaying) {
        proc.setPlaying(isPlaying);
    };

    transportBar.onSeekChanged = [this](double position) {
        proc.getStemPlayer().setPosition(position);
        stemTrackList.setPlayheadPosition(position);
    };

    // Load API settings into extraction client
    juce::String apiKey = SettingsPanel::loadStoredApiKey();
    juce::String endpoint = SettingsPanel::loadStoredEndpoint();
    proc.getExtractionClient().setApiKey(apiKey);
    proc.getExtractionClient().setEndpoint(endpoint);

    // Restore stems from processor state (if any were saved)
    const auto& savedPaths = proc.getLoadedStemPaths();
    if (!savedPaths.isEmpty() && proc.getStemPlayer().getNumStems() > 0)
    {
        // Stems were already loaded by setStateInformation, just update UI
        double duration = proc.getStemPlayer().getTotalDuration();
        transportBar.setTotalDuration(duration);
        transportBar.setPosition(proc.getPlaybackPosition());

        for (const auto& path : savedPaths)
        {
            juce::File stemFile(path);
            if (stemFile.existsAsFile())
                stemTrackList.addStem(stemFile.getFileNameWithoutExtension(), stemFile, formatManager);
        }
    }

    startTimerHz(30);
}

MusicGPTExtractorAudioProcessorEditor::~MusicGPTExtractorAudioProcessorEditor()
{
    stopTimer();
}

void MusicGPTExtractorAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto& th = Style::theme();
    g.fillAll(th.bg);

    // If extraction is in progress, show status over the drop zone area
    if (extractionInProgress)
    {
        auto statusBounds = dropZone.getBounds().toFloat();
        g.setColour(th.panel);
        g.fillRoundedRectangle(statusBounds, th.borderRadius);

        // Progress bar background
        auto progressBounds = statusBounds.reduced(40.0f, 0.0f);
        progressBounds = progressBounds.withHeight(8.0f).withCentre(statusBounds.getCentre());
        g.setColour(th.trackTop.withAlpha(0.3f));
        g.fillRoundedRectangle(progressBounds, 4.0f);

        // Progress bar fill
        auto fillBounds = progressBounds;
        fillBounds.setWidth(progressBounds.getWidth() * extractionProgress);
        g.setColour(th.accent1);
        g.fillRoundedRectangle(fillBounds, 4.0f);

        // Status text
        g.setColour(th.text);
        g.setFont(14.0f);

        juce::String statusText;
        switch (extractionStatus)
        {
            case api::ExtractionStatus::Uploading:
                statusText = "Uploading audio file...";
                break;
            case api::ExtractionStatus::Processing:
                statusText = "Processing stems...";
                break;
            case api::ExtractionStatus::Downloading:
                statusText = "Downloading stems...";
                break;
            default:
                statusText = statusMessage.isEmpty() ? "Processing..." : statusMessage;
                break;
        }

        auto textBounds = statusBounds;
        textBounds.setY(progressBounds.getBottom() + 20.0f);
        textBounds.setHeight(30.0f);
        g.drawText(statusText, textBounds.toNearestInt(), juce::Justification::centred);

        // Progress percentage
        g.setColour(th.textMuted);
        g.setFont(12.0f);
        auto percentBounds = statusBounds;
        percentBounds.setY(progressBounds.getY() - 25.0f);
        percentBounds.setHeight(20.0f);
        g.drawText(juce::String(static_cast<int>(extractionProgress * 100.0f)) + "%",
                   percentBounds.toNearestInt(), juce::Justification::centred);
    }
}

void MusicGPTExtractorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(Layout::kPaddingPx);

    // Header with settings button
    auto header = bounds.removeFromTop(Layout::kHeaderHeightPx);
    auto settingsArea = header.removeFromRight(80);
    logoHeader.setBounds(header);
    settingsButton.setBounds(settingsArea.reduced(0, 15));

    bounds.removeFromTop(Layout::kRowGapPx);

    // Transport bar at bottom
    auto transport = bounds.removeFromBottom(ui::controls::TransportBar::kHeight);
    transportBar.setBounds(transport);

    bounds.removeFromBottom(Layout::kRowGapPx);

    // Main content area - either drop zone or stem track list
    if (stemTrackList.getNumStems() > 0)
    {
        dropZone.setVisible(false);
        stemTrackList.setVisible(true);
        stemTrackList.setBounds(bounds);
    }
    else
    {
        stemTrackList.setVisible(false);
        dropZone.setVisible(!extractionInProgress);
        dropZone.setBounds(bounds);
    }

    // Settings panel overlay (full size)
    if (settingsPanel != nullptr)
        settingsPanel->setBounds(getLocalBounds());
}

void MusicGPTExtractorAudioProcessorEditor::timerCallback()
{
    // Update transport position from processor
    double totalDuration = proc.getStemPlayer().getTotalDuration();
    if (totalDuration > 0.0)
    {
        transportBar.setTotalDuration(totalDuration);
        double pos = proc.getPlaybackPosition();
        transportBar.setPosition(pos);
        stemTrackList.setPlayheadPosition(pos);
    }

    // Sync play button state
    transportBar.setPlaying(proc.isPlaying());

    // Update stem track list playback state
    if (proc.isPlaying())
    {
        stemTrackList.setPlaying(true,
                                  proc.getPlaybackPosition() * totalDuration,
                                  totalDuration);
    }
    else
    {
        stemTrackList.setPlaying(false);
    }

    // Check extraction progress
    if (extractionInProgress)
    {
        extractionProgress = proc.getExtractionClient().getProgress();
        extractionStatus = proc.getExtractionClient().getStatus();
        repaint();
    }
}

void MusicGPTExtractorAudioProcessorEditor::handleFilesDropped(const juce::StringArray& files)
{
    if (files.isEmpty())
        return;

    // Take the first audio file
    juce::File audioFile(files[0]);
    if (!audioFile.existsAsFile())
        return;

    // Check if API key is configured
    if (!SettingsPanel::checkApiKeyConfigured())
    {
        showSettingsPanel();
        return;
    }

    startExtraction(audioFile);
}

void MusicGPTExtractorAudioProcessorEditor::startExtraction(const juce::File& audioFile)
{
    currentAudioFile = audioFile;
    extractionInProgress = true;
    extractionProgress = 0.0f;
    extractionStatus = api::ExtractionStatus::Uploading;
    statusMessage.clear();

    // Clear any existing stems
    stemTrackList.clearStems();
    proc.getStemPlayer().clearStems();

    // Update visibility
    dropZone.setVisible(false);
    resized();
    repaint();

    // Start extraction
    proc.getExtractionClient().extractStems(
        audioFile,
        [this](api::ExtractionStatus status, float progress) {
            onExtractionStatus(status, progress);
        },
        [this](const api::ExtractionResult& result) {
            onExtractionComplete(result);
        }
    );
}

void MusicGPTExtractorAudioProcessorEditor::onExtractionStatus(api::ExtractionStatus status, float progress)
{
    extractionStatus = status;
    extractionProgress = progress;

    // Update on message thread
    juce::MessageManager::callAsync([this]() {
        repaint();
    });
}

void MusicGPTExtractorAudioProcessorEditor::onExtractionComplete(const api::ExtractionResult& result)
{
    extractionInProgress = false;

    if (result.success)
    {
        loadStemsIntoPlayer(result.stemPaths);
    }
    else
    {
        // Show error
        statusMessage = result.errorMessage;
        extractionStatus = api::ExtractionStatus::Error;

        juce::MessageManager::callAsync([this, errorMsg = result.errorMessage]() {
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon,
                "Extraction Failed",
                errorMsg,
                "OK"
            );
            dropZone.setVisible(true);
            resized();
            repaint();
        });
    }
}

void MusicGPTExtractorAudioProcessorEditor::loadStemsIntoPlayer(const juce::StringArray& stemPaths)
{
    // Load stems into the audio player
    if (proc.getStemPlayer().loadStems(stemPaths))
    {
        // Save paths for state persistence
        proc.setLoadedStemPaths(stemPaths);

        // Update transport with total duration
        double duration = proc.getStemPlayer().getTotalDuration();
        transportBar.setTotalDuration(duration);
        transportBar.setPosition(0.0);

        // Add stems to the visual track list
        stemTrackList.clearStems();
        for (const auto& path : stemPaths)
        {
            juce::File stemFile(path);
            stemTrackList.addStem(stemFile.getFileNameWithoutExtension(), stemFile, formatManager);
        }

        // Update layout
        juce::MessageManager::callAsync([this]() {
            resized();
            repaint();
        });
    }
}

void MusicGPTExtractorAudioProcessorEditor::showSettingsPanel()
{
    if (settingsPanel == nullptr)
    {
        settingsPanel = std::make_unique<SettingsPanel>();
        settingsPanel->onSettingsSaved = [this]() {
            // Update extraction client with new settings
            juce::String apiKey = SettingsPanel::loadStoredApiKey();
            juce::String endpoint = SettingsPanel::loadStoredEndpoint();
            proc.getExtractionClient().setApiKey(apiKey);
            proc.getExtractionClient().setEndpoint(endpoint);
        };
        addAndMakeVisible(*settingsPanel);
        resized();
    }

    settingsPanel->setVisible(true);
}
