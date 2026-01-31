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

    formatManager.registerBasicFormats();

    // Logo header
    addAndMakeVisible(logoHeader);

    // Transport bar
    addAndMakeVisible(transportBar);
    transportBar.onPlayPauseChanged = [this](bool isPlaying) {
        proc.setPlaying(isPlaying);
    };
    transportBar.onSeekChanged = [this](double position) {
        proc.getStemPlayer().setPositionNormalized(position);
    };

    // Drop zone for file input
    dropZone.setLabel("Drop audio file here to extract stems");
    dropZone.setAcceptedExtensions({ ".wav", ".mp3", ".aiff", ".flac", ".ogg", ".m4a" });
    dropZone.setInterceptsMouseClicks(true, true);
    dropZone.onFilesDropped = [this](const juce::StringArray& files) {
        handleFilesDropped(files);
    };
    dropZone.setInterceptsMouseClicks(true, true);
    addAndMakeVisible(dropZone);

    // Stem selector for choosing which stems to extract
    addAndMakeVisible(stemSelector);
    stemSelector.setSelected(ui::controls::StemSelector::Stem::Vocals, true, juce::dontSendNotification);
    stemSelector.setSelected(ui::controls::StemSelector::Stem::Drums, true, juce::dontSendNotification);
    stemSelector.setSelected(ui::controls::StemSelector::Stem::Bass, true, juce::dontSendNotification);
    stemSelector.setSelected(ui::controls::StemSelector::Stem::Instrumental, true, juce::dontSendNotification);

    // Stem track list (initially hidden)
    stemTrackList.setVisible(false);
    addAndMakeVisible(stemTrackList);

    // Settings panel (modal overlay)
    // CRITICAL: When hidden, must not intercept mouse clicks or it blocks drag-drop
    settingsPanel.setVisible(false);
    settingsPanel.setInterceptsMouseClicks(false, false);
    settingsPanel.onSettingsSaved = [this]() {
        juce::String apiKey = SettingsPanel::loadStoredApiKey();
        juce::String endpoint = SettingsPanel::loadStoredEndpoint();
        proc.setApiKey(apiKey);
        proc.setApiEndpoint(endpoint);
    };
    addChildComponent(settingsPanel);

    // Credit confirmation dialog (modal overlay)
    creditDialog.setVisible(false);
    creditDialog.onAccept = [this]() {
        if (pendingAudioFile.existsAsFile())
            startExtraction(pendingAudioFile);
        pendingAudioFile = juce::File();
    };
    creditDialog.onCancel = [this]() {
        pendingAudioFile = juce::File();
    };
    addChildComponent(creditDialog);

    // Settings button
    settingsButton.setButtonText("Settings");
    settingsButton.setColour(juce::TextButton::buttonColourId, Style::theme().panel);
    settingsButton.setColour(juce::TextButton::textColourOnId, Style::theme().text);
    settingsButton.setColour(juce::TextButton::textColourOffId, Style::theme().text);
    settingsButton.onClick = [this]() { showSettings(); };
    addAndMakeVisible(settingsButton);

    // Load stored API key into processor
    juce::String storedKey = SettingsPanel::loadStoredApiKey();
    juce::String storedEndpoint = SettingsPanel::loadStoredEndpoint();
    if (storedKey.isNotEmpty())
        proc.setApiKey(storedKey);
    if (storedEndpoint.isNotEmpty())
        proc.setApiEndpoint(storedEndpoint);

    // Restore stems from processor state (if any were saved)
    const auto& savedPaths = proc.getLoadedStemPaths();
    if (!savedPaths.isEmpty() && proc.getStemPlayer().getNumStems() > 0)
    {
        currentState = State::Ready;
        double duration = proc.getTotalDuration();
        transportBar.setTotalDuration(duration);
        transportBar.setPosition(proc.getPlaybackPosition());

        for (const auto& path : savedPaths)
        {
            juce::File stemFile(path);
            if (stemFile.existsAsFile())
                stemTrackList.addStem(stemFile.getFileNameWithoutExtension(), stemFile, formatManager);
        }
        updateUIState();
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

    // Show progress overlay during extraction
    if (currentState == State::Extracting)
    {
        auto contentArea = dropZone.getBounds().toFloat();
        g.setColour(th.panel);
        g.fillRoundedRectangle(contentArea, th.borderRadius);

        // Progress bar
        auto progressRect = contentArea.reduced(40.0f, 0.0f)
                                       .withHeight(8.0f)
                                       .withCentre(contentArea.getCentre());
        g.setColour(th.trackBot);
        g.fillRoundedRectangle(progressRect, 4.0f);

        auto filledRect = progressRect.withWidth(progressRect.getWidth() * extractionProgress);
        g.setColour(th.accent1);
        g.fillRoundedRectangle(filledRect, 4.0f);

        // Phase name (e.g., "Uploading...", "Processing...", "Downloading...")
        g.setColour(th.text);
        g.setFont(14.0f);
        auto textRect = progressRect.translated(0.0f, -30.0f).withHeight(24.0f);
        g.drawText(progressMessage, textRect, juce::Justification::centred);

        // Percentage
        g.setColour(th.textMuted);
        auto pctRect = progressRect.translated(0.0f, 20.0f).withHeight(20.0f);
        g.drawText(juce::String(static_cast<int>(extractionProgress * 100.0f)) + "%",
                   pctRect, juce::Justification::centred);

        // ETA display (format as mm:ss)
        if (extractionEta > 0)
        {
            int minutes = extractionEta / 60;
            int seconds = extractionEta % 60;
            juce::String etaText = "~" + juce::String(minutes) + ":"
                                   + juce::String(seconds).paddedLeft('0', 2) + " remaining";
            auto etaRect = pctRect.translated(0.0f, 18.0f).withHeight(20.0f);
            g.drawText(etaText, etaRect, juce::Justification::centred);
        }
    }
}

void MusicGPTExtractorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(Layout::kPaddingPx);

    // Header with settings button
    auto header = bounds.removeFromTop(Layout::kHeaderHeightPx);
    auto settingsBtnArea = header.removeFromRight(80);
    settingsButton.setBounds(settingsBtnArea.withSizeKeepingCentre(70, 28));
    logoHeader.setBounds(header);

    bounds.removeFromTop(Layout::kRowGapPx);

    // Transport bar at bottom
    auto transport = bounds.removeFromBottom(ui::controls::TransportBar::kHeight);
    transportBar.setBounds(transport);

    bounds.removeFromBottom(Layout::kRowGapPx);

    // Stem selector between drop zone and transport
    auto stemSelectorArea = bounds.removeFromBottom(Layout::kStemSelectorHeightPx);
    stemSelector.setBounds(stemSelectorArea);

    bounds.removeFromBottom(Layout::kRowGapPx);

    // Main content area (drop zone or stem list)
    dropZone.setBounds(bounds);
    stemTrackList.setBounds(bounds);

    // Settings panel covers entire editor
    settingsPanel.setBounds(getLocalBounds());

    // Credit dialog covers entire editor
    creditDialog.setBounds(getLocalBounds());
}

void MusicGPTExtractorAudioProcessorEditor::timerCallback()
{
    // Update transport position from processor
    double totalDuration = proc.getTotalDuration();
    if (totalDuration > 0.0)
    {
        transportBar.setTotalDuration(totalDuration);
        double pos = proc.getPlaybackPosition();
        transportBar.setPosition(pos);
        stemTrackList.setPlayheadPosition(pos);
    }

    // Sync play button state
    transportBar.setPlaying(proc.isPlaying());

    // Update stem track list playhead
    if (currentState == State::Ready && proc.isPlaying())
    {
        stemTrackList.setPlayheadPosition(proc.getPlaybackPosition());
    }

    // Repaint progress during extraction
    if (currentState == State::Extracting)
        repaint();
}

void MusicGPTExtractorAudioProcessorEditor::handleFilesDropped(const juce::StringArray& files)
{
    if (files.isEmpty())
        return;

    juce::File audioFile(files[0]);
    if (!audioFile.existsAsFile())
        return;

    // Check if API key is configured
    if (proc.getApiKey().isEmpty())
    {
        showSettings();
        return;
    }

    // Store file for pending extraction
    pendingAudioFile = audioFile;
    currentAudioFile = audioFile;

    // Set up dialog with stem summary and estimated credits
    // Default stems for extraction
    juce::StringArray defaultStems { "Vocals", "Drums", "Bass", "Other" };
    creditDialog.setStems(defaultStems);

    // Estimate credits based on file (placeholder: 1 credit per MB, minimum 1)
    float fileSizeMB = static_cast<float>(audioFile.getSize()) / (1024.0f * 1024.0f);
    float estimatedCredits = std::max(1.0f, std::ceil(fileSizeMB));
    creditDialog.setCredits(estimatedCredits);

    // Show the confirmation dialog
    creditDialog.setVisible(true);
    creditDialog.toFront(true);
}

void MusicGPTExtractorAudioProcessorEditor::startExtraction(const juce::File& audioFile)
{
    currentState = State::Extracting;
    progressMessage = "Uploading...";
    extractionProgress = 0.0f;
    extractionEta = 0;
    updateUIState();

    proc.startExtraction(
        audioFile,
        [this](const musicgpt::ProgressInfo& info) {
            juce::MessageManager::callAsync([this, info]() {
                onExtractionProgress(info);
            });
        },
        [this](const musicgpt::ExtractionResult& result) {
            juce::MessageManager::callAsync([this, result]() {
                onExtractionComplete(result);
            });
        }
    );
}

void MusicGPTExtractorAudioProcessorEditor::onExtractionProgress(const musicgpt::ProgressInfo& info)
{
    extractionProgress = info.progress;
    extractionEta = info.eta;

    switch (info.phase)
    {
        case musicgpt::ProgressInfo::Phase::Uploading:
            progressMessage = "Uploading...";
            break;
        case musicgpt::ProgressInfo::Phase::Processing:
            progressMessage = "Processing...";
            break;
        case musicgpt::ProgressInfo::Phase::Downloading:
            progressMessage = "Downloading...";
            break;
    }

    if (info.message.isNotEmpty())
        progressMessage = info.message;

    repaint();
}

void MusicGPTExtractorAudioProcessorEditor::onExtractionComplete(const musicgpt::ExtractionResult& result)
{
    if (result.status == musicgpt::JobStatus::Succeeded && !result.stems.empty())
    {
        // Load stems into the processor for playback
        proc.loadExtractedStems(result.stems);

        // Load stems into the UI for visualization
        loadStemsIntoUI(result.stems);

        currentState = State::Ready;
    }
    else
    {
        // Show error
        currentState = State::Idle;
        progressMessage = result.errorMessage.isNotEmpty()
                         ? result.errorMessage
                         : "Extraction failed";
    }

    updateUIState();
}

void MusicGPTExtractorAudioProcessorEditor::loadStemsIntoUI(const std::vector<musicgpt::StemResult>& stems)
{
    stemTrackList.clearStems();

    for (const auto& stem : stems)
    {
        if (!stem.file.existsAsFile())
            continue;

        // Determine stem name from type
        juce::String name;
        switch (stem.type)
        {
            case musicgpt::StemType::Vocals:       name = "Vocals"; break;
            case musicgpt::StemType::Drums:        name = "Drums"; break;
            case musicgpt::StemType::Bass:         name = "Bass"; break;
            case musicgpt::StemType::Other:        name = "Other"; break;
            case musicgpt::StemType::Instrumental: name = "Instrumental"; break;
            default:                               name = stem.file.getFileNameWithoutExtension(); break;
        }

        stemTrackList.addStem(name, stem.file, formatManager);
    }
}

void MusicGPTExtractorAudioProcessorEditor::showSettings()
{
    // Pre-populate with current API key if available
    if (proc.getApiKey().isNotEmpty())
        settingsPanel.setApiKey(proc.getApiKey());

    // Enable mouse clicks when showing settings panel
    settingsPanel.setInterceptsMouseClicks(true, true);
    settingsPanel.setVisible(true);
    settingsPanel.toFront(true);
}

void MusicGPTExtractorAudioProcessorEditor::updateUIState()
{
    switch (currentState)
    {
        case State::Idle:
        case State::Extracting:
            dropZone.setVisible(true);
            stemTrackList.setVisible(false);
            break;

        case State::Ready:
            dropZone.setVisible(false);
            stemTrackList.setVisible(true);
            break;
    }

    // Ensure settings panel doesn't block drag-drop when hidden
    if (!settingsPanel.isVisible())
        settingsPanel.setInterceptsMouseClicks(false, false);

    repaint();
}

musicgpt::StemType MusicGPTExtractorAudioProcessorEditor::buildStemTypeMask() const
{
    using Stem = ui::controls::StemSelector::Stem;
    int mask = 0;

    if (stemSelector.isSelected(Stem::Vocals))
        mask |= static_cast<int>(musicgpt::StemType::Vocals);
    if (stemSelector.isSelected(Stem::Drums))
        mask |= static_cast<int>(musicgpt::StemType::Drums);
    if (stemSelector.isSelected(Stem::Bass))
        mask |= static_cast<int>(musicgpt::StemType::Bass);
    if (stemSelector.isSelected(Stem::Instrumental))
        mask |= static_cast<int>(musicgpt::StemType::Instrumental);

    // Map "Other" from StemSelector if there's a suitable enum
    // For now, if none are selected, default to All
    if (mask == 0)
        return musicgpt::StemType::All;

    return static_cast<musicgpt::StemType>(mask);
}

//=====================================================================
// FileDragAndDropTarget - fallback for OS file drags directly on editor
//=====================================================================

bool MusicGPTExtractorAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    // Accept common audio file extensions
    static const juce::StringArray audioExtensions { ".wav", ".mp3", ".aiff", ".aif", ".flac", ".ogg", ".m4a" };

    for (const auto& file : files)
    {
        juce::String ext = juce::File(file).getFileExtension().toLowerCase();
        if (audioExtensions.contains(ext))
            return true;
    }
    return false;
}

void MusicGPTExtractorAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int)
{
    // Delegate to the same handler used by DropZone
    handleFilesDropped(files);
}
