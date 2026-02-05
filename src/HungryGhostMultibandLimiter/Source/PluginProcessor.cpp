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

    // ===== STORY-MBL-003: Initialize per-band limiters =====
    for (int b = 0; b < limiters.size(); ++b)
    {
        limiters[b].prepare((float)sr);
    }
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

    // ===== STORY-MBL-007: Compute level metering data =====
    // Master input level (before splitting)
    float masterPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        masterPeak = juce::jmax(masterPeak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    masterInputDb.store(juce::Decibels::gainToDecibels(juce::jmax(masterPeak, 1.0e-6f)));

    // Per-band input levels
    auto computeDb = [](const juce::AudioBuffer<float>& buf) -> float {
        float peak = 0.0f;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            peak = juce::jmax(peak, buf.getMagnitude(ch, 0, buf.getNumSamples()));
        return juce::Decibels::gainToDecibels(juce::jmax(peak, 1.0e-6f));
    };
    for (size_t b = 0; b < bandBuffers.size() && b < 6; ++b)
        bandInputDb[b].store(computeDb(bandBuffers[b]));

    // ===== STORY-MBL-003: Per-band limiting and recombination =====
    // Apply limiting to each band
    for (int b = 0; b < juce::jmin((int)bandBuffers.size(), (int)limiters.size()); ++b)
    {
        // Read parameters for this band from APVTS
        const auto id = [b](const char* name){ return juce::String("band.") + juce::String(b + 1) + "." + name; };

        float thresholdDb = apvts.getRawParameterValue(id("threshold_dB"))->load();
        float attackMs = apvts.getRawParameterValue(id("attack_ms"))->load();
        float releaseMs = apvts.getRawParameterValue(id("release_ms"))->load();
        float mixPct = apvts.getRawParameterValue(id("mix_pct"))->load();
        bool bypass = *apvts.getRawParameterValue(id("bypass")) > 0.5f;

        // Configure limiter with current parameters
        hgml::LimiterBandParams params;
        params.thresholdDb = thresholdDb;
        params.attackMs = attackMs;
        params.releaseMs = releaseMs;
        params.mixPct = mixPct;
        params.bypass = bypass;
        limiters[b].setParams(params);

        // Process the band through the limiter
        float maxGrDb = limiters[b].processBlock(bandBuffers[b]);

        // Store gain reduction for metering
        if (b < 6)
            bandGainReductionDb[b].store(maxGrDb);
    }

    // Recombine limited bands back to output
    // Start with low band
    if (bandBuffers.size() >= 1)
        buffer.makeCopyOf(bandBuffers[0], true);

    // Add high band(s) to output
    for (size_t b = 1; b < bandBuffers.size(); ++b)
    {
        for (int ch = 0; ch < juce::jmin(buffer.getNumChannels(), bandBuffers[b].getNumChannels()); ++ch)
        {
            float* outData = buffer.getWritePointer(ch);
            const float* inData = bandBuffers[b].getReadPointer(ch);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                outData[n] += inData[n];
        }
    }

    // ===== STORY-MBL-004: Solo and Delta routing =====
    // Check if any band is soloed
    bool anyBandSoloed = false;
    for (int b = 0; b < juce::jmin((int)bandBuffers.size(), (int)limiters.size()); ++b)
    {
        const auto id = [b](const char* name){ return juce::String("band.") + juce::String(b + 1) + "." + name; };
        bool solo = *apvts.getRawParameterValue(id("solo")) > 0.5f;
        if (solo) { anyBandSoloed = true; break; }
    }

    // Apply solo routing: zero out non-soloed bands if any are soloed
    if (anyBandSoloed)
    {
        for (int b = 0; b < juce::jmin((int)bandBuffers.size(), (int)limiters.size()); ++b)
        {
            const auto id = [b](const char* name){ return juce::String("band.") + juce::String(b + 1) + "." + name; };
            bool solo = *apvts.getRawParameterValue(id("solo")) > 0.5f;
            if (!solo)
            {
                // Zero out this band since it's not soloed
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    float* data = buffer.getWritePointer(ch);
                    const float* bandData = bandBuffers[b].getReadPointer(ch);
                    for (int n = 0; n < buffer.getNumSamples(); ++n)
                        data[n] -= bandData[n];  // Remove non-soloed band from output
                }
            }
        }
    }

    // TODO: STORY-MBL-004 - Delta mode (output dry - wet difference for analysis)
    // Requires storing original split band before limiting

    // ===== STORY-MBL-004: Apply output trim (make-up gain) =====
    float outputTrimDb = apvts.getRawParameterValue("global.outputTrim_dB")->load();
    if (juce::jabs(outputTrimDb) > 0.01f)
    {
        float trimGain = hgml::dbToLin(outputTrimDb);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int n = 0; n < buffer.getNumSamples(); ++n)
                data[n] *= trimGain;
        }
    }

    // ===== STORY-MBL-007: Compute band output levels =====
    for (size_t b = 0; b < bandBuffers.size() && b < 6; ++b)
        bandOutputDb[b].store(computeDb(bandBuffers[b]));

    // ===== STORY-MBL-007: Compute master output level =====
    float masterOutPeak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        masterOutPeak = juce::jmax(masterOutPeak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
    masterOutputDb.store(juce::Decibels::gainToDecibels(juce::jmax(masterOutPeak, 1.0e-6f)));

    // ===== STORY-MBL-004: Latency reporting =====
    // TODO: Calculate and report total latency (band splitter + look-ahead)
    // Currently: BandSplitterIIR has ~1-2 sample latency (phase alignment)
    // Future: Add look-ahead delay buffer if needed for true peak limiting
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
