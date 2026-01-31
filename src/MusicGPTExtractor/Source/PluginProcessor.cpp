#include "PluginProcessor.h"
#include "PluginEditor.h"

MusicGPTExtractorAudioProcessor::MusicGPTExtractorAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool MusicGPTExtractorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void MusicGPTExtractorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    stemPlayer.prepare(sampleRate, samplesPerBlock);
}

void MusicGPTExtractorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    // Clear input and mix in stem playback
    buffer.clear();

    // Get stem mix levels from parameters
    const float vocalsLevel = apvts.getRawParameterValue("vocalsLevel")->load();
    const float drumsLevel = apvts.getRawParameterValue("drumsLevel")->load();
    const float bassLevel = apvts.getRawParameterValue("bassLevel")->load();
    const float otherLevel = apvts.getRawParameterValue("otherLevel")->load();

    stemPlayer.setLevels(vocalsLevel, drumsLevel, bassLevel, otherLevel);
    stemPlayer.processBlock(buffer);
}

void MusicGPTExtractorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void MusicGPTExtractorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* MusicGPTExtractorAudioProcessor::createEditor()
{
    return new MusicGPTExtractorAudioProcessorEditor(*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout
MusicGPTExtractorAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Stem mix levels (0.0 = muted, 1.0 = full)
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "vocalsLevel", 1 }, "Vocals",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "drumsLevel", 1 }, "Drums",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "bassLevel", 1 }, "Bass",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "otherLevel", 1 }, "Other",
        NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusicGPTExtractorAudioProcessor();
}
