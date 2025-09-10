#include "PluginProcessor.h"
#include "PluginEditor.h"

//=====================================================================

namespace
{
    inline float dbToLin(float dB) noexcept { return juce::Decibels::decibelsToGain(dB); }
    inline float linToDb(float g)  noexcept { return juce::Decibels::gainToDecibels(juce::jmax(g, 1.0e-12f)); }

    inline float softClipTanhTo(float x, float limit, float k = 2.0f) noexcept
    {
        // Normalise to �1, tanh-soft-clip, rescale to 'limit'
        const float xn = x / juce::jmax(limit, 1.0e-9f);
        const float yn = std::tanh(k * xn) / std::tanh(k);
        return yn * limit;
    }

    constexpr float kMaxLookAheadMs = 5.0f; // pre-allocate up to 5 ms to avoid reallocations in audio thread
}

//=====================================================================

HungryGhostLimiterAudioProcessor::HungryGhostLimiterAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

//=====================================================================

bool HungryGhostLimiterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//=====================================================================

void HungryGhostLimiterAudioProcessor::buildOversampler(double sr, int samplesPerBlockExpected)
{
    // Choose factor: 8� for 44.1/48k, 4� for 88.2/96k or higher
    osFactor = (sr <= 48000.0 ? 8 : 4);
    osSampleRate = (float)(sr * osFactor);

    const int stages = (int)std::log2((double)osFactor);
    oversampler.reset(new juce::dsp::Oversampling<float>(
        /*channels*/ 2,
        /*numStages*/ stages,
        /*filter*/ juce::dsp::Oversampling<float>::FilterType::filterHalfBandFIREquiripple,
        /*isMaxQuality*/ true));

    oversampler->reset();
    oversampler->initProcessing((size_t)samplesPerBlockExpected);

    oversamplingLatencyNative = (int)oversampler->getLatencyInSamples();
}

//=====================================================================


//=====================================================================

void HungryGhostLimiterAudioProcessor::updateLatencyReport(float lookMs)
{
    const int lookNative = (int)std::ceil(lookMs * 0.001f * sampleRateHz);
    // Report OS filter latency + look-ahead (both expressed at native rate)
    setLatencySamples(oversamplingLatencyNative + lookNative);
}

//=====================================================================

void HungryGhostLimiterAudioProcessor::prepareToPlay(double sr, int samplesPerBlockExpected)
{
    sampleRateHz = (float)sr;

    buildOversampler(sr, samplesPerBlockExpected);
    // Prepare core DSP (OS rate)
    const int maxLASamplesOS = (int)std::ceil(kMaxLookAheadMs * 0.001f * osSampleRate) + 64;
    limiter.prepare(osSampleRate, maxLASamplesOS);

    currentGainDb = 0.0f;

    // init input trim smoothing (20 ms at native rate)
    for (auto& s : inTrimLin)
    {
        s.reset(sampleRateHz, 0.02);
        s.setCurrentAndTargetValue(1.0f);
    }

    // initial latency report
    const auto* lookParam = apvts.getRawParameterValue("lookAheadMs");
    lastReportedLookMs = lookParam ? lookParam->load() : 1.0f;
    updateLatencyReport(lastReportedLookMs);
}

//=====================================================================

void HungryGhostLimiterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    const int numSmps = buffer.getNumSamples();
    if (numSmps == 0) return;

    // --- INPUT TRIM (pre) ---
    float inTrimLdB = 0.0f;
    float inTrimRdB = 0.0f;
    if (auto* v = apvts.getRawParameterValue("inTrimL")) inTrimLdB = v->load();
    if (auto* v = apvts.getRawParameterValue("inTrimR")) inTrimRdB = v->load();
    const bool inLink = (apvts.getRawParameterValue("inTrimLink") != nullptr)
                        && (apvts.getRawParameterValue("inTrimLink")->load() > 0.5f);
    if (inLink) inTrimRdB = inTrimLdB;

    const float inGL = dbToLin(inTrimLdB);
    const float inGR = dbToLin(inTrimRdB);

    inTrimLin[0].setTargetValue(inGL);
    inTrimLin[1].setTargetValue(inGR);

    const int numCh = juce::jmin(buffer.getNumChannels(), 2);
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* x = buffer.getWritePointer(ch);
        auto& s = inTrimLin[ch];
        for (int n = 0; n < numSmps; ++n)
            x[n] *= s.getNextValue();
    }

    // --- fetch params (atomic, no locks) ---
    const float thL = apvts.getRawParameterValue("thresholdL")->load();
    float       thR = apvts.getRawParameterValue("thresholdR")->load();
    const bool  thLink = apvts.getRawParameterValue("thresholdLink")->load() > 0.5f;
    if (thLink) { /* mirror left into right for pre-gain */ thR = thL; }

    float ceilLDb = apvts.getRawParameterValue("outCeilingL")->load();
    float ceilRDb = apvts.getRawParameterValue("outCeilingR")->load();
    const bool  ceilLink = apvts.getRawParameterValue("outCeilingLink")->load() > 0.5f;
    if (ceilLink) ceilRDb = ceilLDb;

    const float releaseMs = apvts.getRawParameterValue("release")->load();
    const float lookMs = apvts.getRawParameterValue("lookAheadMs")->load();
    const bool  scHPFOn = apvts.getRawParameterValue("scHpf")->load() > 0.5f;
    const bool  safetyOn = apvts.getRawParameterValue("safetyClip")->load() > 0.5f;
    const bool  autoRel = (apvts.getRawParameterValue("autoRelease") != nullptr)
                          && (apvts.getRawParameterValue("autoRelease")->load() > 0.5f);

    const float ceilLin = dbToLin(juce::jmin(ceilLDb, ceilRDb));
    const float preGainL = dbToLin(-thL);
    const float preGainR = dbToLin(-thR);

    // --- derive OS-time constants ---
    lookAheadSamplesOS = juce::jlimit(1, (int)std::round(lookMs * 0.001f * osSampleRate) - 0 + 0,
        (int)std::round(lookMs * 0.001f * osSampleRate)); // value used only for limiter params

    const float relSec = juce::jlimit(0.001f, 2.0f, releaseMs * 0.001f);
    releaseAlphaOS = std::exp(-1.0f / (relSec * osSampleRate));

    // --- latency report (cheap) only if look-ahead changed noticeably ---
    if (!std::isfinite(lastReportedLookMs) || std::abs(lookMs - lastReportedLookMs) > 1.0e-3f)
    {
        updateLatencyReport(lookMs);
        lastReportedLookMs = lookMs;
    }

    // --- oversample up ---
    juce::dsp::AudioBlock<float> inBlock(buffer);
    auto upBlock = oversampler->processSamplesUp(inBlock);

    auto* upL = upBlock.getChannelPointer(0);
    auto* upR = upBlock.getChannelPointer(1);
    const int N = (int)upBlock.getNumSamples();

    // Prepare params for core DSP
    hgl::LimiterParams p;
    p.preGainL = preGainL;
    p.preGainR = preGainR;
    p.ceilLin  = ceilLin;
    p.releaseAlphaOS = releaseAlphaOS;
    p.lookAheadSamplesOS = lookAheadSamplesOS;
    p.scHpfOn = scHPFOn;
    p.safetyOn = safetyOn;
    p.autoReleaseOn = autoRel;

    limiter.setParams(p);
    const float meterMaxAttenDb = limiter.processBlockOS(upL, upR, N);

    // --- downsample back to host rate ---
    oversampler->processSamplesDown(inBlock);

    // --- meter (host rate): store raw dB and let UI smooth
    attenDbRaw.store(juce::jlimit(0.0f, 24.0f, meterMaxAttenDb), std::memory_order_relaxed);
}

//=====================================================================

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

//=====================================================================

juce::AudioProcessorEditor* HungryGhostLimiterAudioProcessor::createEditor()
{
    return new HungryGhostLimiterAudioProcessorEditor(*this);
}

//=====================================================================

juce::AudioProcessorValueTreeState::ParameterLayout
HungryGhostLimiterAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    // --- Stereo "threshold" as pre-gain (kept for UI compatibility) ---
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "thresholdL", 1 }, "Threshold L",
        NormalisableRange<float>(-24.0f, 0.0f, 0.01f, 0.6f), -10.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "thresholdR", 1 }, "Threshold R",
        NormalisableRange<float>(-24.0f, 0.0f, 0.01f, 0.6f), -10.0f));

    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "thresholdLink", 1 }, "Link Threshold", true));

    // --- True-peak ceiling (stereo, linkable), recommend -1.0 dBTP by default ---
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "outCeilingL", 1 }, "Out Ceiling L",
        NormalisableRange<float>(-24.0f, 0.0f, 0.01f, 0.8f), -1.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "outCeilingR", 1 }, "Out Ceiling R",
        NormalisableRange<float>(-24.0f, 0.0f, 0.01f, 0.8f), -1.0f));

    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "outCeilingLink", 1 }, "Link Ceiling", true));

    // --- Release time (ms) ---
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "release", 1 }, "Release (ms)",
        NormalisableRange<float>(1.0f, 1000.0f, 0.1f, 0.35f), 120.0f));

    // --- NEW: Look-ahead (ms), OS-rate detector & delay ---
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "lookAheadMs", 1 }, "Look-ahead (ms)",
        NormalisableRange<float>(0.25f, 3.0f, 0.01f, 0.35f), 1.0f));

    // --- NEW: Sidechain HPF enable (30 Hz) ---
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "scHpf", 1 }, "Sidechain HPF", true));

    // --- NEW: Safety soft clipper just beneath ceiling (oversampled) ---
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "safetyClip", 1 }, "Safety Clip", false));

    // --- NEW: Stereo Input Trim (dB) + link ---
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "inTrimL", 1 }, "Input Trim L",
        NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 0.5f), 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "inTrimR", 1 }, "Input Trim R",
        NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 0.5f), 0.0f));

    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "inTrimLink", 1 }, "Link Input Trim", true));

    // --- NEW: Auto Release toggle ---
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "autoRelease", 1 }, "Auto Release", false));

    return { params.begin(), params.end() };
}

//=====================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostLimiterAudioProcessor();
}
