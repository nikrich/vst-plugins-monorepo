#include "PluginProcessor.h"
#ifndef HG_MBL_HEADLESS_TEST
#include "PluginEditor.h"
#endif

//==============================================================================

HungryGhostMultibandLimiterAudioProcessor::HungryGhostMultibandLimiterAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

//==============================================================================

bool HungryGhostMultibandLimiterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================

void HungryGhostMultibandLimiterAudioProcessor::prepareToPlay(double sr, int samplesPerBlockExpected)
{
    sampleRateHz = sr;
    this->samplesPerBlockExpected = samplesPerBlockExpected;

    // ===== STORY-MBL-002: Initialize band splitter =====
    splitter = std::make_unique<hgml::BandSplitterIIR>();
    splitter->prepare(sr, 2);  // Stereo input

    // Configure with initial crossover frequency from parameters
    cachedCrossoverHz = apvts.getRawParameterValue("xover.1.Hz")->load();
    splitter->setCrossoverHz(cachedCrossoverHz);
}

//==============================================================================

void HungryGhostMultibandLimiterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Update cached parameter values
    cachedBandCount = apvts.getRawParameterValue("global.bandCount")->load();
    cachedCrossoverHz = apvts.getRawParameterValue("xover.1.Hz")->load();
    cachedOversamplingFactor = 1 << apvts.getRawParameterValue("global.oversampling")->load(); // 1x, 2x, 4x
    cachedLookAheadMs = apvts.getRawParameterValue("global.lookAheadMs")->load();

    // ===== STORY-MBL-002: Band splitting =====
    // Ensure band buffers are properly sized for this block
    ensureBandBuffers(buffer.getNumChannels(), buffer.getNumSamples());

    // Update crossover frequency if changed
    if (juce::jabs(cachedCrossoverHz - splitter->getCrossoverHz()) > 0.01f)
    {
        splitter->setCrossoverHz(cachedCrossoverHz);
    }

    // Split input into bands
    splitter->process(buffer, bandBuffers);

    // ===== STORY-MBL-003/004 TODO: Per-band limiting and recombination =====
    // For now, pass the split bands back to output as pass-through
    // This demonstrates the band splitting infrastructure is working
    if (bandBuffers.size() >= 2)
    {
        buffer.makeCopyOf(bandBuffers[0], true);  // Low band to output
        // High band will be summed by future limiting stage
    }

    // Get per-band parameters (prepared for DSP integration)
    // These will be used by STORY-MBL-003/004 when DSP components are implemented
    for (int b = 0; b < juce::jmin(cachedBandCount, 8); ++b)
    {
        const auto id = [b](const char* name){ return juce::String("band.") + juce::String(b + 1) + "." + name; };

        float thresholdDb = apvts.getRawParameterValue(id("threshold_dB"))->load();
        float attackMs = apvts.getRawParameterValue(id("attack_ms"))->load();
        float releaseMs = apvts.getRawParameterValue(id("release_ms"))->load();
        float mixPct = apvts.getRawParameterValue(id("mix_pct"))->load();
        bool bypass = *apvts.getRawParameterValue(id("bypass")) > 0.5f;
        bool solo = *apvts.getRawParameterValue(id("solo")) > 0.5f;
        bool delta = *apvts.getRawParameterValue(id("delta")) > 0.5f;

        // Store parameters for use by DSP components (to be implemented)
        (void)thresholdDb;
        (void)attackMs;
        (void)releaseMs;
        (void)mixPct;
        (void)bypass;
        (void)solo;
        (void)delta;
    }

    // Get global parameters
    float outputTrimDb = apvts.getRawParameterValue("global.outputTrim_dB")->load();
    bool latencyCompensate = *apvts.getRawParameterValue("global.latencyCompensate") > 0.5f;
    int crossoverMode = (int)*apvts.getRawParameterValue("global.crossoverMode");

    // TODO: Output trim and latency compensation (STORY-MBL-004)
    (void)outputTrimDb;
    (void)latencyCompensate;
    (void)crossoverMode;
}

//==============================================================================

juce::AudioProcessorEditor* HungryGhostMultibandLimiterAudioProcessor::createEditor()
{
    #ifndef HG_MBL_HEADLESS_TEST
    return new HungryGhostMultibandLimiterAudioProcessorEditor(*this);
    #else
    return nullptr;
    #endif
}

//==============================================================================

HungryGhostMultibandLimiterAudioProcessor::APVTS::ParameterLayout HungryGhostMultibandLimiterAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // Global parameters
    // Number of bands: 1-4 for M1, ready to expand to 8 in M2
    params.push_back(std::make_unique<AudioParameterInt>(ParameterID{"global.bandCount", 1}, "Bands", 1, 8, 2));

    // Crossover mode: IIR (zero-latency) vs FIR (linear-phase)
    params.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"global.crossoverMode", 1}, "Crossover Mode", StringArray{ "IIR-ZeroLatency", "FIR-LinearPhase" }, 0));

    // Oversampling for improved limiter precision at transients
    params.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"global.oversampling", 1}, "Oversampling", StringArray{ "1x", "2x", "4x" }, 0));

    // Look-ahead time for transparent limiting (0-20ms)
    params.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"global.lookAheadMs", 1}, "Look-ahead (ms)", NormalisableRange<float>(0.0f, 20.0f, 0.01f, 0.35f), 3.0f));

    // Latency compensation flag
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{"global.latencyCompensate", 1}, "Latency Compensate", true));

    // Output trim for make-up gain
    params.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"global.outputTrim_dB", 1}, "Output Trim (dB)", NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 0.5f), 0.0f));

    // Crossover frequencies (up to 7 for 8 bands) — M1 uses just one
    params.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"xover.1.Hz", 1}, "Crossover 1 (Hz)", NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.3f), 120.0f));

    // Per-band limiter parameters (define for 2 bands for M1)
    auto addBand = [&](int i)
    {
        const auto id = [i](const char* name){ return juce::String("band.") + juce::String(i) + "." + name; };

        // Threshold: where the limiter engages (-60 to 0 dB)
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id("threshold_dB"), 1},
            juce::String("Band ") + juce::String(i) + " Threshold (dB)",
            NormalisableRange<float>(-60.0f, 0.0f, 0.01f, 0.5f),
            -6.0f));

        // Attack time: how fast limiting engages (0.1-200ms)
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id("attack_ms"), 1},
            juce::String("Band ") + juce::String(i) + " Attack (ms)",
            NormalisableRange<float>(0.1f, 200.0f, 0.01f, 0.35f),
            2.0f));

        // Release time: how fast limiting disengages (10-1000ms)
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id("release_ms"), 1},
            juce::String("Band ") + juce::String(i) + " Release (ms)",
            NormalisableRange<float>(10.0f, 1000.0f, 0.01f, 0.35f),
            100.0f));

        // Mix: dry/wet blend for transparent limiting (0-100%)
        params.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID{id("mix_pct"), 1},
            juce::String("Band ") + juce::String(i) + " Mix (%)",
            NormalisableRange<float>(0.0f, 100.0f, 0.01f, 1.0f),
            100.0f));

        // Bypass: disable limiting on this band
        params.push_back(std::make_unique<AudioParameterBool>(
            ParameterID{id("bypass"), 1},
            juce::String("Band ") + juce::String(i) + " Bypass",
            false));

        // Solo: hear only this band
        params.push_back(std::make_unique<AudioParameterBool>(
            ParameterID{id("solo"), 1},
            juce::String("Band ") + juce::String(i) + " Solo",
            false));

        // Delta: hear the difference (dry - wet) for analysis
        params.push_back(std::make_unique<AudioParameterBool>(
            ParameterID{id("delta"), 1},
            juce::String("Band ") + juce::String(i) + " Delta",
            false));
    };

    // Add 2 bands for M1 skeleton (ready to expand to up to 8 in M2)
    addBand(1);
    addBand(2);

    return { params.begin(), params.end() };
}

//==============================================================================

void HungryGhostMultibandLimiterAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyValueTreeToXml(true, nullptr);
    copyXmlToBinary(*state, destData);
}

//==============================================================================

void HungryGhostMultibandLimiterAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xmlState = getXmlFromBinary(data, sizeInBytes);
    if (xmlState && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================

void HungryGhostMultibandLimiterAudioProcessor::ensureBandBuffers(int numChannels, int numSamples)
{
    // Ensure we have enough band buffers (up to 8 bands max for future expansion)
    const int maxBands = 8;
    if ((int)bandBuffers.size() < maxBands)
        bandBuffers.resize(maxBands);

    // Resize each band buffer to match current block size
    for (int b = 0; b < maxBands; ++b)
        bandBuffers[b].setSize(numChannels, numSamples, false, true, true);
}

//==============================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostMultibandLimiterAudioProcessor();
}
