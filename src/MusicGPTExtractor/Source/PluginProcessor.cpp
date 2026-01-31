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

void MusicGPTExtractorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Save API key if set
    juce::ValueTree state("MusicGPTExtractor");
    if (extractionConfig.apiKey.isNotEmpty())
        state.setProperty("apiKey", extractionConfig.apiKey, nullptr);
    if (extractionConfig.apiEndpoint != "https://api.musicgpt.com/v1")
        state.setProperty("apiEndpoint", extractionConfig.apiEndpoint, nullptr);

    juce::MemoryOutputStream stream(destData, false);
    state.writeToStream(stream);
}

void MusicGPTExtractorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto state = juce::ValueTree::readFromData(data, static_cast<size_t>(sizeInBytes));
    if (state.isValid())
    {
        if (state.hasProperty("apiKey"))
            extractionConfig.apiKey = state["apiKey"].toString();
        if (state.hasProperty("apiEndpoint"))
            extractionConfig.apiEndpoint = state["apiEndpoint"].toString();
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

    for (const auto& stem : stems)
    {
        if (stem.file.existsAsFile())
            stemPlayer.loadStem(stem.file);
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
