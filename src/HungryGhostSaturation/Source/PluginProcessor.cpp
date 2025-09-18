#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

HungryGhostSaturationAudioProcessor::HungryGhostSaturationAudioProcessor()
: AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                 .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

void HungryGhostSaturationAudioProcessor::prepareToPlay(double sampleRate, int)
{
    lastSampleRate = static_cast<float>(sampleRate);
}

void HungryGhostSaturationAudioProcessor::releaseResources() {}

bool HungryGhostSaturationAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Only support stereo in/out
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    return in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::stereo();
}

void HungryGhostSaturationAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    const float drive = apvts.getRawParameterValue("drive")->load();
    const float mix   = apvts.getRawParameterValue("mix")->load();

    // Simple arctangent waveshaper
    auto processSample = [drive](float x) {
        const float g = juce::jlimit(0.0f, 50.0f, drive);
        const float y = std::atan(g * x) / std::atan(g == 0.0f ? 1.0f : g);
        return y;
    };

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        for (int n = 0; n < numSamples; ++n)
        {
            const float dry = data[n];
            const float wet = processSample(dry);
            data[n] = dry + (wet - dry) * mix; // linear mix
        }
    }
}

void HungryGhostSaturationAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto s = apvts.copyState())
        if (auto xml = s.createXml())
            copyXmlToBinary(*xml, destData);
}

void HungryGhostSaturationAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout HungryGhostSaturationAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"drive", 1}, "Drive", juce::NormalisableRange<float>(0.0f, 20.0f, 0.01f, 0.5f), 4.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1}, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));

    return { params.begin(), params.end() };
}

juce::AudioProcessorEditor* HungryGhostSaturationAudioProcessor::createEditor()
{
    return new HungryGhostSaturationAudioProcessorEditor(*this);
}
