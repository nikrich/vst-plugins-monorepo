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

void HungryGhostLimiterAudioProcessor::updateLatencyReport(float lookMs, bool oversamplingActive)
{
    const int lookNative = (int)std::ceil(lookMs * 0.001f * sampleRateHz);
    const int osFilt = oversamplingActive ? oversamplingLatencyNative : 0;
    // Report OS filter latency + look-ahead (both expressed at native rate)
    setLatencySamples(osFilt + lookNative);
}

//=====================================================================

void HungryGhostLimiterAudioProcessor::prepareToPlay(double sr, int samplesPerBlockExpected)
{
    sampleRateHz = (float)sr;

    buildOversampler(sr, samplesPerBlockExpected);
    // Prepare core DSP (default to TruePeak domain)
    const int maxLASamplesOS = (int)std::ceil(kMaxLookAheadMs * 0.001f * osSampleRate) + 64;
    limiter.prepare(osSampleRate, maxLASamplesOS);

    currentDomain = Domain::TruePeak;
    lastDomain    = currentDomain;

    currentGainDb = 0.0f;

    // init input trim smoothing (20 ms at native rate)
    for (auto& s : inTrimLin)
    {
        s.reset(sampleRateHz, 0.02);
        s.setCurrentAndTargetValue(1.0f);
    }

    // initial latency report (respect domain parameter if set)
    const auto* lookParam = apvts.getRawParameterValue("lookAheadMs");
    lastReportedLookMs = lookParam ? lookParam->load() : 1.0f;
    const bool domDigInit = (apvts.getRawParameterValue("domDigital") != nullptr) && (apvts.getRawParameterValue("domDigital")->load() > 0.5f);
    updateLatencyReport(lastReportedLookMs, !domDigInit);
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

    // --- Domain selection ---
    const bool domDig  = (apvts.getRawParameterValue("domDigital") != nullptr) && (apvts.getRawParameterValue("domDigital")->load() > 0.5f);
    const bool domAna  = (apvts.getRawParameterValue("domAnalog")  != nullptr) && (apvts.getRawParameterValue("domAnalog")->load()  > 0.5f);
    currentDomain = domDig ? Domain::Digital : (domAna ? Domain::Analog : Domain::TruePeak);

    // --- derive time constants based on domain sample-rate ---
    const float compSR = (currentDomain == Domain::Digital) ? sampleRateHz : osSampleRate;
    lookAheadSamplesOS = juce::jlimit(1, (int)std::round(lookMs * 0.001f * compSR), (int)std::round(lookMs * 0.001f * compSR));

    const float relSec = juce::jlimit(0.001f, 2.0f, releaseMs * 0.001f);
    releaseAlphaOS = std::exp(-1.0f / (relSec * compSR));

    // Re-prepare limiter if domain switched (updates SR and delay capacities)
    if (currentDomain != lastDomain)
    {
        const int maxLASamples = (int)std::ceil(kMaxLookAheadMs * 0.001f * compSR) + 64;
        limiter.prepare(compSR, maxLASamples);
        lastDomain = currentDomain;
    }

    // Adjust sidechain HPF cutoff for analog flavour
    if (currentDomain == Domain::Analog)
        limiter.setSidechainHPFCutoff(60.0f);
    else
        limiter.setSidechainHPFCutoff(30.0f);

    // --- latency report only if look-ahead changed noticeably ---
    if (!std::isfinite(lastReportedLookMs) || std::abs(lookMs - lastReportedLookMs) > 1.0e-3f)
    {
        updateLatencyReport(lookMs, currentDomain != Domain::Digital);
        lastReportedLookMs = lookMs;
    }

    juce::dsp::AudioBlock<float> inBlock(buffer);

    float* upL = nullptr; float* upR = nullptr; int N = 0;

    if (currentDomain == Domain::Digital)
    {
        upL = inBlock.getChannelPointer(0);
        upR = inBlock.getChannelPointer(1);
        N = (int)inBlock.getNumSamples();
    }
    else
    {
        // --- oversample up ---
        auto upBlock = oversampler->processSamplesUp(inBlock);
        upL = upBlock.getChannelPointer(0);
        upR = upBlock.getChannelPointer(1);
        N = (int)upBlock.getNumSamples();
    }

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

    // --- downsample back to host rate if needed ---
    if (currentDomain != Domain::Digital)
        oversampler->processSamplesDown(inBlock);

    // --- ADVANCED: optional quantize + dither + shaping (host rate) ---
    int bits = 0; // 0 = bypass
    const auto on = [](const juce::AudioProcessorValueTreeState& s, const char* id){ if (auto* v = s.getRawParameterValue(id)) return v->load() > 0.5f; return false; };
    if (on(apvts, "q24")) bits = 24; else if (on(apvts, "q20")) bits = 20; else if (on(apvts, "q16")) bits = 16; else if (on(apvts, "q12")) bits = 12; else if (on(apvts, "q8")) bits = 8;
    const bool ditherT2 = on(apvts, "dT2"); // default to T2 in layout
    const bool shapeArc = on(apvts, "sArc");

    if (bits > 0 && bits < 32)
    {
        const float step = std::ldexp(1.0f, 1 - bits); // 2^(1-bits)
        const float amp  = ditherT2 ? 2.0f : 1.0f;     // scale for T2
        const int numCh = juce::jmin(buffer.getNumChannels(), 2);
        const int N = buffer.getNumSamples();
        for (int ch = 0; ch < numCh; ++ch)
        {
            float* x = buffer.getWritePointer(ch);
            float ePrev = nsErrPrev[ch];
            for (int n = 0; n < N; ++n)
            {
                float y = x[n];
                if (shapeArc) y += 0.8f * ePrev; // simple error-feedback shaping
                // TPDF dither in [-amp*step, amp*step]
                const float u1 = rng.nextFloat();
                const float u2 = rng.nextFloat();
                const float tpdf = ((u1 + u2) - 1.0f) * (amp * step);
                y += tpdf;
                // quantize to nearest step
                const float q = std::round(y / step) * step;
                const float err = y - q; ePrev = err;
                x[n] = juce::jlimit(-1.0f, 1.0f, q);
            }
            nsErrPrev[ch] = ePrev;
        }
    }

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

    // --- ADVANCED: Quantize/Dither/Shape/Domain ---
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "q24", 1 }, "Q 24-bit", true));
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "q20", 1 }, "Q 20-bit", false));
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "q16", 1 }, "Q 16-bit", false));
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "q12", 1 }, "Q 12-bit", false));
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "q8",  1 }, "Q 8-bit",  false));

    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "dT1", 1 }, "Dither T1", false));
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "dT2", 1 }, "Dither T2", true));

    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "sNone", 1 }, "Shape None", false));
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "sArc",  1 }, "Shape Arc",  true));

    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "domDigital", 1 }, "Domain Digital", false));
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "domAnalog",  1 }, "Domain Analog",  false));
    params.push_back(std::make_unique<AudioParameterBool>(ParameterID{ "domTruePeak",1 }, "Domain TruePeak", true));

    return { params.begin(), params.end() };
}

//=====================================================================

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostLimiterAudioProcessor();
}
