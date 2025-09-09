#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

HungryGhostLimiterAudioProcessor::HungryGhostLimiterAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

bool HungryGhostLimiterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void HungryGhostLimiterAudioProcessor::prepareToPlay(double sr, int)
{
    sampleRateHz = (float)sr;
    attenDbSmoothed.reset(sampleRateHz, 0.08);
}

void HungryGhostLimiterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    auto* threshParam = apvts.getRawParameterValue("threshold");
    auto* ceilParam = apvts.getRawParameterValue("outCeiling");
    auto* relParam = apvts.getRawParameterValue("release");

    const float thresholdDb = threshParam->load();
    const float ceilingDb = ceilParam->load();
    const float preGain = juce::Decibels::decibelsToGain(-thresholdDb);
    const float ceilingLin = juce::Decibels::decibelsToGain(ceilingDb);

    float maxAttenDbThisBlock = 0.0f;
    const int numCh = buffer.getNumChannels();
    const int numSmps = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* x = buffer.getWritePointer(ch);
        for (int n = 0; n < numSmps; ++n)
        {
            float s = x[n] * preGain;
            const float a = std::abs(s);

            if (a > ceilingLin)
            {
                const float gr = ceilingLin / (a + 1.0e-12f);
                s *= gr;
                const float attenDb = -juce::Decibels::gainToDecibels(gr);
                if (attenDb > maxAttenDbThisBlock) maxAttenDbThisBlock = attenDb;
            }

            x[n] = s;
        }
    }

    const float releaseMs = relParam->load();
    const double releaseSeconds = juce::jlimit(0.005, 2.0, (double)releaseMs * 0.001);
    attenDbSmoothed.reset(sampleRateHz, releaseSeconds);
    attenDbSmoothed.setTargetValue(juce::jlimit(0.0f, 12.0f, maxAttenDbThisBlock));
}

void HungryGhostLimiterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void HungryGhostLimiterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* HungryGhostLimiterAudioProcessor::createEditor()
{
    return new HungryGhostLimiterAudioProcessorEditor(*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout HungryGhostLimiterAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "threshold", 1 }, "Threshold",
        NormalisableRange<float>(-24.0f, 0.0f, 0.01f, 0.6f), -10.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "outCeiling", 1 }, "Out Ceiling",
        NormalisableRange<float>(-24.0f, 0.0f, 0.01f, 0.8f), -0.2f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "release", 1 }, "Release (ms)",
        NormalisableRange<float>(1.0f, 1000.0f, 0.1f, 0.35f), 100.0f));

    return { params.begin(), params.end() };
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostLimiterAudioProcessor();
}
