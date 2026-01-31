#include "PluginProcessor.h"
#include "PluginEditor.h"

//=====================================================================

MusicGPTExtractorAudioProcessor::MusicGPTExtractorAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
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
    stemPlayer.prepare(sampleRate, samplesPerBlockExpected);
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    buffer.clear();

    if (playing.load())
    {
        stemPlayer.processBlock(buffer);
        playbackPosition.store(stemPlayer.getPosition());
    }
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::setPlaybackPosition(double pos)
{
    playbackPosition.store(pos);
    stemPlayer.setPosition(pos);
}

//=====================================================================

void MusicGPTExtractorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Create XML state
    auto state = std::make_unique<juce::XmlElement>("MusicGPTExtractorState");
    state->setAttribute("version", 1);

    // Save playback position
    state->setAttribute("playbackPosition", playbackPosition.load());

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
        auto* stemSetting = settingsElement->createNewChildElement("Setting");
        stemSetting->setAttribute("index", i);
        stemSetting->setAttribute("gain", static_cast<double>(stemPlayer.getStemGain(i)));
        stemSetting->setAttribute("muted", stemPlayer.isStemMuted(i));
        stemSetting->setAttribute("solo", stemPlayer.isStemSolo(i));
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
        stemPlayer.loadStems(validPaths);

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
                        float gain = static_cast<float>(stemSetting->getDoubleAttribute("gain", 1.0));
                        bool muted = stemSetting->getBoolAttribute("muted", false);
                        bool solo = stemSetting->getBoolAttribute("solo", false);

                        stemPlayer.setStemGain(index, gain);
                        stemPlayer.setStemMuted(index, muted);
                        stemPlayer.setStemSolo(index, solo);
                    }
                }
            }
        }

        // Restore playback position
        setPlaybackPosition(savedPosition);
    }
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
