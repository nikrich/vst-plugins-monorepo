#include "PluginProcessor.h"
#include "PluginEditor.h"

HungryGhostReverbAudioProcessor::HungryGhostReverbAudioProcessor()
: juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
  apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

void HungryGhostReverbAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverb.prepare(sampleRate, samplesPerBlock, getTotalNumOutputChannels());
    reverb.reset();
}

void HungryGhostReverbAudioProcessor::releaseResources() {}

void HungryGhostReverbAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ignoreUnused(midi);
    juce::ScopedNoDenormals noDenormals;

    // Map APVTS to engine parameters
    currentParams.mixPercent   = apvts.getRawParameterValue("mix")->load();
    currentParams.decaySeconds = apvts.getRawParameterValue("decaySeconds")->load();
    currentParams.size         = apvts.getRawParameterValue("size")->load();
    currentParams.predelayMs   = apvts.getRawParameterValue("predelayMs")->load();
    currentParams.diffusion    = apvts.getRawParameterValue("diffusion")->load();
    currentParams.modRateHz    = apvts.getRawParameterValue("modRateHz")->load();
    currentParams.modDepthMs   = apvts.getRawParameterValue("modDepthMs")->load();
    currentParams.hfDampingHz  = apvts.getRawParameterValue("hfDampingHz")->load();
    currentParams.lowCutHz     = apvts.getRawParameterValue("lowCutHz")->load();
    currentParams.highCutHz    = apvts.getRawParameterValue("highCutHz")->load();
    currentParams.width        = apvts.getRawParameterValue("width")->load();
    currentParams.seed         = (int) apvts.getRawParameterValue("seed")->load();
    currentParams.freeze       = apvts.getRawParameterValue("freeze")->load() > 0.5f;
    {
        const int mi = (int) apvts.getRawParameterValue("mode")->load();
        currentParams.mode = static_cast<hgr::dsp::ReverbMode>(juce::jlimit(0, 3, mi));
    }

    reverb.setParameters(currentParams);

    juce::dsp::AudioBlock<float> block(buffer);
    reverb.process(block);
}

juce::AudioProcessorEditor* HungryGhostReverbAudioProcessor::createEditor()
{
    return new HungryGhostReverbAudioProcessorEditor(*this);
}

void HungryGhostReverbAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void HungryGhostReverbAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout HungryGhostReverbAudioProcessor::createParameterLayout()
{
    using P = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto hzRange = [](float lo, float hi){ juce::NormalisableRange<float> r(lo, hi); r.setSkewForCentre(1000.0f); return r; };
    auto timeRange = [](){ juce::NormalisableRange<float> r(0.1f, 60.0f); r.setSkewForCentre(2.0f); return r; };

    params.push_back(std::make_unique<juce::AudioParameterChoice>("mode",         "Mode",         juce::StringArray{ "Hall", "Room", "Plate", "Ambience" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("mix",          "Mix",          juce::NormalisableRange<float>(0.f, 100.f), 25.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("decaySeconds", "Decay (s)",    timeRange(), 3.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("size",         "Size",         juce::NormalisableRange<float>(0.5f, 1.5f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("predelayMs",   "Pre-delay (ms)", juce::NormalisableRange<float>(0.f, 200.f), 20.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("diffusion",    "Diffusion",    juce::NormalisableRange<float>(0.f, 1.f), 0.75f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("modRateHz",    "Mod Rate (Hz)", juce::NormalisableRange<float>(0.05f, 3.0f), 0.30f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("modDepthMs",   "Mod Depth (ms)", juce::NormalisableRange<float>(0.0f, 10.0f), 1.5f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("hfDampingHz",  "HF Damping (Hz)", hzRange(1000.0f, 16000.0f), 6000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("lowCutHz",     "Low Cut (Hz)",   hzRange(20.0f, 300.0f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("highCutHz",    "High Cut (Hz)",  hzRange(6000.0f, 20000.0f), 18000.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>("width",        "Width",        juce::NormalisableRange<float>(0.f, 1.f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(  "seed",         "Seed",         0, 9999, 1337));
    params.push_back(std::make_unique<juce::AudioParameterBool>( "freeze",       "Freeze",       false));

    return { params.begin(), params.end() };
}

// This factory is required by JUCE plugin client code
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostReverbAudioProcessor();
}

