#include "PluginProcessor.h"
#include "PluginEditor.h"

//=====================================================================

MusicGPTExtractorAudioProcessor::MusicGPTExtractorAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    // Set default output directory for extracted stems
    extractionConfig.outputDirectory = juce::File::getSpecialLocation(
        juce::File::userMusicDirectory).getChildFile("MusicGPT Stems");
}

MusicGPTExtractorAudioProcessor::~MusicGPTExtractorAudioProcessor()
{
    if (extractionClient)
        extractionClient->cancelAll();
}

//=====================================================================

bool MusicGPTExtractorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlockExpected)
{
    sampleRateHz = sampleRate;
    blockSizeExpected = samplesPerBlockExpected;
    stemPlayer.prepareToPlay(samplesPerBlockExpected, sampleRate);
}

void MusicGPTExtractorAudioProcessor::releaseResources()
{
    stemPlayer.releaseResources();
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    buffer.clear();

    if (playing.load())
    {
        juce::AudioSourceChannelInfo info(buffer);
        stemPlayer.getNextAudioBlock(info);
    }
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::setPlaybackPosition(double pos)
{
    stemPlayer.setPositionNormalized(pos);
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Create XML state
    auto state = std::make_unique<juce::XmlElement>("MusicGPTExtractorState");
    state->setAttribute("version", 1);

    // Save playback position
    state->setAttribute("playbackPosition", getPlaybackPosition());

    // Save loaded stem paths
    auto* stemsElement = state->createNewChildElement("Stems");
    for (const auto& path : loadedStemPaths)
    {
        auto* stemElement = stemsElement->createNewChildElement("Stem");
        stemElement->setAttribute("path", path);
    }

    // Save per-stem settings (gain, mute, solo)
    auto* settingsElement = state->createNewChildElement("StemSettings");
    int numStems = stemPlayer.getNumStems();
    for (int i = 0; i < numStems; ++i)
    {
        if (auto* stem = stemPlayer.getStem(i))
        {
            auto* stemSetting = settingsElement->createNewChildElement("Setting");
            stemSetting->setAttribute("index", i);
            stemSetting->setAttribute("gain", static_cast<double>(stem->getGain()));
            stemSetting->setAttribute("muted", stem->isMuted());
            stemSetting->setAttribute("solo", stem->isSolo());
        }
    }

    // Copy to memory block
    copyXmlToBinary(*state, destData);
}

void MusicGPTExtractorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Parse XML state
    auto state = getXmlFromBinary(data, sizeInBytes);
    if (state == nullptr)
        return;

    if (!state->hasTagName("MusicGPTExtractorState"))
        return;

    // Restore playback position
    double savedPosition = state->getDoubleAttribute("playbackPosition", 0.0);

    // Restore stem paths
    loadedStemPaths.clear();
    if (auto* stemsElement = state->getChildByName("Stems"))
    {
        for (auto* stemElement : stemsElement->getChildIterator())
        {
            if (stemElement->hasTagName("Stem"))
            {
                juce::String path = stemElement->getStringAttribute("path");
                if (path.isNotEmpty())
                    loadedStemPaths.add(path);
            }
        }
    }

    // Load stems if paths exist and files still exist
    juce::StringArray validPaths;
    for (const auto& path : loadedStemPaths)
    {
        if (juce::File(path).existsAsFile())
            validPaths.add(path);
    }

    if (!validPaths.isEmpty())
    {
        loadedStemPaths = validPaths;
        for (const auto& path : validPaths)
            stemPlayer.loadStem(juce::File(path));

        // Restore per-stem settings
        if (auto* settingsElement = state->getChildByName("StemSettings"))
        {
            for (auto* stemSetting : settingsElement->getChildIterator())
            {
                if (stemSetting->hasTagName("Setting"))
                {
                    int index = stemSetting->getIntAttribute("index", -1);
                    if (index >= 0 && index < stemPlayer.getNumStems())
                    {
                        if (auto* stem = stemPlayer.getStem(index))
                        {
                            float gain = static_cast<float>(stemSetting->getDoubleAttribute("gain", 1.0));
                            bool muted = stemSetting->getBoolAttribute("muted", false);
                            bool solo = stemSetting->getBoolAttribute("solo", false);

                            stem->setGain(gain);
                            stem->setMuted(muted);
                            stem->setSolo(solo);
                        }
                    }
                }
            }
        }

        // Restore playback position
        setPlaybackPosition(savedPosition);
    }
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::setApiKey(const juce::String& key)
{
    extractionConfig.apiKey = key;
    // Recreate client with new config on next extraction
    extractionClient.reset();
}

void MusicGPTExtractorAudioProcessor::setApiEndpoint(const juce::String& endpoint)
{
    extractionConfig.apiEndpoint = endpoint;
    extractionClient.reset();
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::ensureExtractionClient()
{
    if (!extractionClient)
    {
        // Ensure output directory exists
        extractionConfig.outputDirectory.createDirectory();
        extractionClient = std::make_unique<musicgpt::ExtractionClient>(extractionConfig);
    }
}

void MusicGPTExtractorAudioProcessor::startExtraction(
    const juce::File& audioFile,
    ExtractionProgressCallback onProgress,
    ExtractionCompleteCallback onComplete)
{
    ensureExtractionClient();

    // Extract all standard stems
    currentJobId = extractionClient->extractStems(
        audioFile,
        musicgpt::StemType::All,
        std::move(onProgress),
        std::move(onComplete)
    );
}

void MusicGPTExtractorAudioProcessor::cancelExtraction()
{
    if (extractionClient && currentJobId.isNotEmpty())
    {
        extractionClient->cancelJob(currentJobId);
        currentJobId.clear();
    }
}

bool MusicGPTExtractorAudioProcessor::isExtracting() const
{
    return extractionClient && extractionClient->isBusy();
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::loadExtractedStems(
    const std::vector<musicgpt::StemResult>& stems)
{
    stemPlayer.clearStems();
    loadedStemPaths.clear();

    for (const auto& stem : stems)
    {
        if (stem.file.existsAsFile())
        {
            stemPlayer.loadStem(stem.file);
            loadedStemPaths.add(stem.file.getFullPathName());
        }
    }
}

void MusicGPTExtractorAudioProcessor::setPlaying(bool shouldPlay)
{
    playing.store(shouldPlay);
    if (shouldPlay)
        stemPlayer.play();
    else
        stemPlayer.pause();
}

double MusicGPTExtractorAudioProcessor::getPlaybackPosition() const
{
    return stemPlayer.getPositionNormalized();
}

double MusicGPTExtractorAudioProcessor::getTotalDuration() const
{
    return stemPlayer.getLengthInSeconds();
}

//=====================================================================

juce::AudioProcessorEditor* MusicGPTExtractorAudioProcessor::createEditor()
{
    return new MusicGPTExtractorAudioProcessorEditor(*this);
}

//=====================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusicGPTExtractorAudioProcessor();
}
