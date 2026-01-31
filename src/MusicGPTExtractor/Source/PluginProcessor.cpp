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

void MusicGPTExtractorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::ignoreUnused(destData);
}

void MusicGPTExtractorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::ignoreUnused(data, sizeInBytes);
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
