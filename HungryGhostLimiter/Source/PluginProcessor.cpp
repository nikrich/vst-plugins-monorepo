#include "PluginProcessor.h"
#include "PluginEditor.h"

//=====================================================================

namespace
{
    inline float dbToLin(float dB) noexcept { return juce::Decibels::decibelsToGain(dB); }
    inline float linToDb(float g)  noexcept { return juce::Decibels::gainToDecibels(juce::jmax(g, 1.0e-12f)); }

    inline float softClipTanhTo(float x, float limit, float k = 2.0f) noexcept
    {
        // Normalise to ±1, tanh-soft-clip, rescale to 'limit'
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
    // Choose factor: 8× for 44.1/48k, 4× for 88.2/96k or higher
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

void HungryGhostLimiterAudioProcessor::updateSidechainFilter()
{
    // 2nd order HPF ~30 Hz at the OS rate (only in the detector path)
    // 2nd order HPF ~30 Hz at the OS rate (only in the detector path)  
    scHPFCoefs = juce::dsp::IIR::Coefficients<float>::makeHighPass(osSampleRate, 30.0);
    scHPF_L.coefficients = scHPFCoefs;
    scHPF_R.coefficients = scHPFCoefs;
}

//=====================================================================

void HungryGhostLimiterAudioProcessor::updateLatencyReport()
{
    const auto* lookParam = apvts.getRawParameterValue("lookAheadMs");
    const float lookMs = lookParam ? lookParam->load() : 1.0f;
    const int lookNative = (int)std::ceil(lookMs * 0.001f * sampleRateHz);

    // Report OS filter latency + look-ahead (both expressed at native rate)
    setLatencySamples(oversamplingLatencyNative + lookNative);
}

//=====================================================================

void HungryGhostLimiterAudioProcessor::prepareToPlay(double sr, int samplesPerBlockExpected)
{
    sampleRateHz = (float)sr;

    buildOversampler(sr, samplesPerBlockExpected);
    updateSidechainFilter();

    // Pre-allocate look-ahead delay lines and sliding max at OS rate (up to kMaxLookAheadMs)
    const int maxLASamplesOS = (int)std::ceil(kMaxLookAheadMs * 0.001f * osSampleRate) + 64;
    delayL.reset(maxLASamplesOS);
    delayR.reset(maxLASamplesOS);
    slidingMax.reset(maxLASamplesOS);

    // Meter smoothing uses block-rate seconds as time constant
    attenDbSmoothed.reset(sampleRateHz, 0.08);

    currentGainDb = 0.0f;

    updateLatencyReport();
}

//=====================================================================

void HungryGhostLimiterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
    juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    const int numSmps = buffer.getNumSamples();
    if (numSmps == 0) return;

    // --- fetch params (atomic, no locks) ---
    const float thL = apvts.getRawParameterValue("thresholdL")->load();
    float       thR = apvts.getRawParameterValue("thresholdR")->load();
    const bool  thLink = apvts.getRawParameterValue("thresholdLink")->load() > 0.5f;
    if (thLink) { /* mirror left into right for pre-gain */ thR = thL; }

    const float ceilingDb = apvts.getRawParameterValue("outCeiling")->load();
    const float releaseMs = apvts.getRawParameterValue("release")->load();
    const float lookMs = apvts.getRawParameterValue("lookAheadMs")->load();
    const bool  scHPFOn = apvts.getRawParameterValue("scHpf")->load() > 0.5f;
    const bool  safetyOn = apvts.getRawParameterValue("safetyClip")->load() > 0.5f;

    const float ceilLin = dbToLin(ceilingDb);
    const float preGainL = dbToLin(-thL); // your original "threshold as input gain" concept, preserved
    const float preGainR = dbToLin(-thR);

    // --- derive OS-time constants ---
    lookAheadSamplesOS = juce::jlimit(1, (int)delayL.buf.size() - 1,
        (int)std::round(lookMs * 0.001f * osSampleRate));

    const float relSec = juce::jlimit(0.001f, 2.0f, releaseMs * 0.001f);
    releaseAlphaOS = std::exp(-1.0f / (relSec * osSampleRate)); // one-pole toward target (release only)

    // --- latency report (cheap) if look-ahead changed noticeably ---
    updateLatencyReport();

    // --- oversample up ---
    juce::dsp::AudioBlock<float> inBlock(buffer);
    auto upBlock = oversampler->processSamplesUp(inBlock);

    auto* upL = upBlock.getChannelPointer(0);
    auto* upR = upBlock.getChannelPointer(1);
    const int N = (int)upBlock.getNumSamples();

    float meterMaxAttenDb = 0.0f;

    for (int i = 0; i < N; ++i)
    {
        // 1) pre-gain at OS rate
        float xl = upL[i] * preGainL;
        float xr = upR[i] * preGainR;

        // 2) sidechain detection (optional HPF)
        float dl = scHPFOn ? scHPF_L.processSample(xl) : xl;
        float dr = scHPFOn ? scHPF_R.processSample(xr) : xr;

        const float a = juce::jmax(std::abs(dl), std::abs(dr)); // stereo max-link
        slidingMax.push(a, lookAheadSamplesOS);
        const float aMax = slidingMax.getMax();

        // 3) required gain to hit ceiling (true-peak @ OS): <= 1
        float gReq = 1.0f;
        if (aMax > ceilLin)
            gReq = ceilLin / (aMax + 1.0e-12f);

        const float gReqDb = linToDb(gReq); // <= 0 dB

        // 4) Attack (instant due to look-ahead) + program release
        if (gReqDb < currentGainDb)               // need more attenuation (more negative dB)
            currentGainDb = gReqDb;              // jump instantly
        else                                      // release toward target (usually 0 dB)
            currentGainDb = currentGainDb * releaseAlphaOS + gReqDb * (1.0f - releaseAlphaOS);

        const float gLin = dbToLin(currentGainDb);

        // 5) delay main (look-ahead), then apply same gain to both channels
        float yl = delayL.processSample(xl, lookAheadSamplesOS) * gLin;
        float yr = delayR.processSample(xr, lookAheadSamplesOS) * gLin;

        // 6) optional ultra-gentle soft clip as a safety (still at OS rate)
        if (safetyOn)
        {
            constexpr float safetyBelowCeilDb = -0.1f; // 0.1 dB beneath ceiling
            const float safetyLimit = ceilLin * dbToLin(safetyBelowCeilDb);

            if (std::abs(yl) > safetyLimit) yl = softClipTanhTo(yl, safetyLimit);
            if (std::abs(yr) > safetyLimit) yr = softClipTanhTo(yr, safetyLimit);
        }

        // write back to OS buffer
        upL[i] = yl;
        upR[i] = yr;

        const float attenDb = -currentGainDb; // positive dB of reduction
        if (attenDb > meterMaxAttenDb) meterMaxAttenDb = attenDb;
    }

    // --- downsample back to host rate ---
    oversampler->processSamplesDown(inBlock);

    // --- meter smoothing (host rate) ---
    const double releaseSeconds = juce::jlimit(0.005, 2.0, (double)releaseMs * 0.001);
    attenDbSmoothed.reset(sampleRateHz, releaseSeconds);
    attenDbSmoothed.setTargetValue(juce::jlimit(0.0f, 24.0f, meterMaxAttenDb));
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

    // --- True-peak ceiling (recommend -1.0 dBTP by default) ---
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "outCeiling", 1 }, "Out Ceiling",
        NormalisableRange<float>(-24.0f, 0.0f, 0.01f, 0.8f), -1.0f));

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

    return { params.begin(), params.end() };
}

//=====================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostLimiterAudioProcessor();
}
