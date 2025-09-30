#include "PluginProcessor.h"
#ifndef HG_MBC_HEADLESS_TEST
#include "PluginEditor.h"
#endif

// DSP includes
#include "dsp/BandSplitterIIR.h"
#include "dsp/CompressorBand.h"
#include "dsp/Utilities.h"

namespace {
    inline int msToSamples(float ms, double sr) { return (int) std::round(ms * 0.001 * sr); }
}

HungryGhostMultibandCompressorAudioProcessor::HungryGhostMultibandCompressorAudioProcessor()
: juce::AudioProcessor(
    BusesProperties()
      .withInput ("Input",     juce::AudioChannelSet::stereo(), true)
      .withOutput("Output",    juce::AudioChannelSet::stereo(), true)
      // Keep a sidechain bus available (not yet wired per-band; future milestone)
      .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false))
{
}

HungryGhostMultibandCompressorAudioProcessor::~HungryGhostMultibandCompressorAudioProcessor() = default;

bool HungryGhostMultibandCompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    // Require stereo main input/output; sidechain optional (mono or stereo)
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();

    if (mainIn  != juce::AudioChannelSet::stereo())  return false;
    if (mainOut != juce::AudioChannelSet::stereo())  return false;

    // If a sidechain bus exists, allow mono or stereo
    if (getBusCount(true) > 1)
    {
        const auto sc = layouts.getChannelSet(true, 1);
        if (! (sc == juce::AudioChannelSet::mono() || sc == juce::AudioChannelSet::stereo() || sc == juce::AudioChannelSet()))
            return false;
    }
    return true;
}

void HungryGhostMultibandCompressorAudioProcessor::ensureBandBuffers(int numChannels, int numSamples)
{
    const int C = juce::jmax(2, numChannels);
    const int N = juce::jmax(1, numSamples);
    auto ensure = [&](juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() != C || b.getNumSamples() < N)
            b.setSize(C, N, false, true, true);
    };
    ensure(bandLowDry); ensure(bandHighDry);
    ensure(bandLowProc); ensure(bandHighProc);
}

void HungryGhostMultibandCompressorAudioProcessor::prepareToPlay(double sr, int samplesPerBlockExpected)
{
    sampleRateHz = (float) sr;

    // Initial latency report: look-ahead only for now
    float la = 3.0f;
    if (auto* v = apvts.getRawParameterValue("global.lookAheadMs")) la = v->load();
    reportedLatency.store(msToSamples(la, sr));
    setLatencySamples(reportedLatency.load());

    // Prepare DSP components for 2-band M1
    splitter.reset(new hgmbc::BandSplitterIIR());
    splitter->prepare(sr, getTotalNumInputChannels());

    compLow.reset(new hgmbc::CompressorBand());
    compHigh.reset(new hgmbc::CompressorBand());

    const int maxLASamples = juce::jmax(1, msToSamples(20.0f, sr) + 64);
    compLow->prepare(sr, getTotalNumInputChannels(), maxLASamples);
    compHigh->prepare(sr, getTotalNumInputChannels(), maxLASamples);

    ensureBandBuffers(getTotalNumInputChannels(), samplesPerBlockExpected);

    // Reset EQ filters
    for (auto& b : eq) { b.reset(); }

    // Analyzer FIFO setup (mono rings ~2 seconds at 48k / decimate)
    const int ringSize = 48000 * 2 / analyzerDecimate;
    analyzerRingPre.assign((size_t) juce::jmax(4096, ringSize), 0.0f);
    analyzerRingPost.assign((size_t) juce::jmax(4096, ringSize), 0.0f);
    analyzerFifoPre.reset(new juce::AbstractFifo((int) analyzerRingPre.size()));
    analyzerFifoPost.reset(new juce::AbstractFifo((int) analyzerRingPost.size()));
}

void HungryGhostMultibandCompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    const int numSmps = buffer.getNumSamples();
    if (numSmps == 0) return;

    const int numCh = juce::jmin(2, buffer.getNumChannels());

    // Snapshot globals
    bandCount = (int) juce::jlimit(1.0f, 6.0f, apvts.getRawParameterValue("global.bandCount")->load());
    lookAheadMs = apvts.getRawParameterValue("global.lookAheadMs")->load();
    splitConfig.fcHz = apvts.getRawParameterValue("xover.1.Hz")->load();

    // Update latency
    const int laSamples = msToSamples(lookAheadMs, sampleRateHz);
    if (laSamples != reportedLatency.load())
    {
        reportedLatency.store(laSamples);
        setLatencySamples(laSamples);
    }

    // Ensure working buffers and update splitter cutoff
    ensureBandBuffers(numCh, numSmps);
    splitter->setCrossoverHz(splitConfig.fcHz);

    // Split into bands (copy input to dry bands for delta before processing)
    bandLowDry.makeCopyOf(buffer, true);
    bandHighDry.makeCopyOf(buffer, true);

    // Push PRE analyzer mono samples (decimated) from input
    if (analyzerFifoPre)
    {
        int write = analyzerFifoPre->getFreeSpace();
        int pushed = 0;
        for (int n = 0; n < numSmps && pushed < write; ++n)
        {
            if ((n % analyzerDecimate) == 0)
            {
                const float m = 0.5f * (bandLowDry.getSample(0, n) + bandLowDry.getSample(juce::jmin(1, numCh-1), n));
                int start1, size1, start2, size2;
                analyzerFifoPre->prepareToWrite(1, start1, size1, start2, size2);
                if (size1 > 0) { analyzerRingPre[(size_t) start1] = m; analyzerFifoPre->finishedWrite(1); ++pushed; }
                else break;
            }
        }
    }

    splitter->process(buffer, bandLowDry, bandHighDry);

    // Configure per-band params from APVTS (bands 1 & 2)
    hgmbc::CompressorBandParams p1, p2;
    auto rp = [&](const juce::String& id){ return apvts.getRawParameterValue(id)->load(); };
    p1.threshold_dB = rp("band.1.threshold_dB");
    p1.ratio        = rp("band.1.ratio");
    p1.knee_dB      = rp("band.1.knee_dB");
    p1.attack_ms    = rp("band.1.attack_ms");
    p1.release_ms   = rp("band.1.release_ms");
    p1.mix_pct      = rp("band.1.mix_pct");

    p2.threshold_dB = rp("band.2.threshold_dB");
    p2.ratio        = rp("band.2.ratio");
    p2.knee_dB      = rp("band.2.knee_dB");
    p2.attack_ms    = rp("band.2.attack_ms");
    p2.release_ms   = rp("band.2.release_ms");
    p2.mix_pct      = rp("band.2.mix_pct");

    compLow->setParams(p1);
    compHigh->setParams(p2);
    compLow->setLookaheadSamples(laSamples);
    compHigh->setLookaheadSamples(laSamples);

    // Process bands
    bandLowProc.makeCopyOf(bandLowDry, true);
    bandHighProc.makeCopyOf(bandHighDry, true);

    if (!(bool) (apvts.getRawParameterValue("band.1.bypass")->load() > 0.5f))
        compLow->process(bandLowProc);
    if (!(bool) (apvts.getRawParameterValue("band.2.bypass")->load() > 0.5f))
        compHigh->process(bandHighProc);

    // Delta listen per band: replace processed with (dry - processed)
    const bool delta1 = apvts.getRawParameterValue("band.1.delta")->load() > 0.5f;
    const bool delta2 = apvts.getRawParameterValue("band.2.delta")->load() > 0.5f;
    if (delta1)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* dry = bandLowDry.getReadPointer(ch);
            auto* wet = bandLowProc.getWritePointer(ch);
            for (int n = 0; n < numSmps; ++n) wet[n] = dry[n] - wet[n];
        }
    }
    if (delta2)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* dry = bandHighDry.getReadPointer(ch);
            auto* wet = bandHighProc.getWritePointer(ch);
            for (int n = 0; n < numSmps; ++n) wet[n] = dry[n] - wet[n];
        }
    }

    // Update GR meters (positive dB reduction)
    grBandDb[0].store(-compLow->getCurrentGainDb());
    grBandDb[1].store(-compHigh->getCurrentGainDb());

    // Solo logic
    const bool solo1 = apvts.getRawParameterValue("band.1.solo")->load() > 0.5f;
    const bool solo2 = apvts.getRawParameterValue("band.2.solo")->load() > 0.5f;

    if (solo1 && !solo2)
    {
        buffer.makeCopyOf(bandLowProc, true);
    }
    else if (solo2 && !solo1)
    {
        buffer.makeCopyOf(bandHighProc, true);
    }
    else
    {
        // Sum processed bands into output
        buffer.clear();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* out = buffer.getWritePointer(ch);
            const float* l = bandLowProc.getReadPointer(ch);
            const float* h = bandHighProc.getReadPointer(ch);
            for (int n = 0; n < numSmps; ++n)
                out[n] = juce::jlimit(-2.0f, 2.0f, l[n] + h[n]);
        }
    }

    // ===== Parallel EQ stage (after compressor) =====
    {
        auto coeffFor = [&](int type, float freq, float q, float gainDb)
        {
            using Coeff = juce::dsp::IIR::Coefficients<float>;
            const double fs = (double) sampleRateHz;
            switch (type)
            {
                case 0: return Coeff::makePeakFilter(fs, freq, q, juce::Decibels::decibelsToGain(gainDb));
                case 1: return Coeff::makeLowShelf(fs, freq, q, juce::Decibels::decibelsToGain(gainDb));
                case 2: return Coeff::makeHighShelf(fs, freq, q, juce::Decibels::decibelsToGain(gainDb));
                case 3: return Coeff::makeLowPass(fs, freq, q);
                case 4: return Coeff::makeHighPass(fs, freq, q);
                case 5: return Coeff::makeNotch(fs, freq, q);
                default: return Coeff::makePeakFilter(fs, freq, q, 1.0f);
            }
        };

        for (int bi = 1; bi <= kMaxEqBands; ++bi)
        {
            const juce::String pfx = "eq." + juce::String(bi) + ".";
            const bool enabled = apvts.getRawParameterValue(pfx + "enabled")->load() > 0.5f;
            if (!enabled) continue;
            const int type = (int) apvts.getRawParameterValue(pfx + "type")->load();
            const float freq = apvts.getRawParameterValue(pfx + "freq_hz")->load();
            const float gainDb = apvts.getRawParameterValue(pfx + "gain_db")->load();
            const float q = apvts.getRawParameterValue(pfx + "q")->load();

            auto coeff = coeffFor(type, juce::jlimit(20.0f, sampleRateHz * 0.45f, freq), juce::jlimit(0.1f, 10.0f, q), gainDb);
            for (int ch = 0; ch < numCh; ++ch)
                eq[bi-1].filt[ch].coefficients = coeff;

            juce::dsp::AudioBlock<float> blk(buffer);
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto cb = blk.getSingleChannelBlock((size_t) ch);
                juce::dsp::ProcessContextReplacing<float> ctx(cb);
                eq[bi-1].filt[ch].process(ctx);
            }
        }
    }

    // Global output trim
    const float outTrimDb = apvts.getRawParameterValue("global.outputTrim_dB")->load();
    const float g = juce::Decibels::decibelsToGain(outTrimDb);
    for (int ch = 0; ch < numCh; ++ch)
        buffer.applyGain(ch, 0, numSmps, g);

    // Now push POST analyzer after EQ and output trim so yellow line reflects final output
    if (analyzerFifoPost)
    {
        int write = analyzerFifoPost->getFreeSpace(); // in samples
        int pushed = 0;
        for (int n = 0; n < numSmps && pushed < write; ++n)
        {
            if ((n % analyzerDecimate) == 0)
            {
                const float m = 0.5f * (buffer.getSample(0, n) + buffer.getSample(juce::jmin(1, numCh-1), n));
                int start1, size1, start2, size2;
                analyzerFifoPost->prepareToWrite(1, start1, size1, start2, size2);
                if (size1 > 0) { analyzerRingPost[(size_t) start1] = m; analyzerFifoPost->finishedWrite(1); ++pushed; }
                else break;
            }
        }
    }

    // Clear any extra channels
    for (int ch = numCh; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSmps);
}

int HungryGhostMultibandCompressorAudioProcessor::readAnalyzerPre(float* dst, int maxSamples)
{
    if (!analyzerFifoPre || analyzerRingPre.empty() || maxSamples <= 0) return 0;
    int start1, size1, start2, size2;
    analyzerFifoPre->prepareToRead(maxSamples, start1, size1, start2, size2);
    int read = 0;
    if (size1 > 0) { std::memcpy(dst, analyzerRingPre.data() + start1, (size_t) size1 * sizeof(float)); read += size1; }
    if (size2 > 0) { std::memcpy(dst + read, analyzerRingPre.data() + start2, (size_t) size2 * sizeof(float)); read += size2; }
    analyzerFifoPre->finishedRead(read);
    return read;
}

int HungryGhostMultibandCompressorAudioProcessor::readAnalyzerPost(float* dst, int maxSamples)
{
    if (!analyzerFifoPost || analyzerRingPost.empty() || maxSamples <= 0) return 0;
    int start1, size1, start2, size2;
    analyzerFifoPost->prepareToRead(maxSamples, start1, size1, start2, size2);
    int read = 0;
    if (size1 > 0) { std::memcpy(dst, analyzerRingPost.data() + start1, (size_t) size1 * sizeof(float)); read += size1; }
    if (size2 > 0) { std::memcpy(dst + read, analyzerRingPost.data() + start2, (size_t) size2 * sizeof(float)); read += size2; }
    analyzerFifoPost->finishedRead(read);
    return read;
}

void HungryGhostMultibandCompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
}

void HungryGhostMultibandCompressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorValueTreeState::ParameterLayout
HungryGhostMultibandCompressorAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> ps;

    // Global
    ps.push_back(std::make_unique<AudioParameterInt>(ParameterID{"global.bandCount", 1}, "Bands", 1, 6, 2));
    ps.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"global.crossoverMode", 1}, "Crossover Mode", StringArray{ "IIR-ZeroLatency", "FIR-LinearPhase" }, 0));
    ps.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"global.oversampling", 1}, "Oversampling", StringArray{ "1x", "2x", "4x" }, 0));
    ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"global.lookAheadMs", 1}, "Look-ahead (ms)", NormalisableRange<float>(0.0f, 20.0f, 0.01f, 0.35f), 3.0f));
    ps.push_back(std::make_unique<AudioParameterBool>(ParameterID{"global.latencyCompensate", 1}, "Latency Compensate", true));
    ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"global.outputTrim_dB", 1}, "Output Trim (dB)", NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 0.5f), 0.0f));

    // Crossovers (up to 5 for 6 bands) — start with one for M1 skeleton
    ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"xover.1.Hz", 1}, "Crossover 1 (Hz)", NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.3f), 120.0f));

    // Per-band (define for 2 bands for M1 skeleton)
    auto addBand = [&](int i)
    {
        const auto id = [i](const char* name){ return juce::String("band.") + juce::String(i) + "." + name; };
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("threshold_dB"), 1}, juce::String("Band ") + juce::String(i) + " Threshold (dB)", NormalisableRange<float>(-60.0f, 0.0f, 0.01f, 0.5f), -18.0f));
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("ratio"), 1}, juce::String("Band ") + juce::String(i) + " Ratio", NormalisableRange<float>(1.0f, 20.0f, 0.01f, 0.35f), 2.0f));
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("knee_dB"), 1}, juce::String("Band ") + juce::String(i) + " Knee (dB)", NormalisableRange<float>(0.0f, 24.0f, 0.01f, 0.5f), 6.0f));
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("attack_ms"), 1}, juce::String("Band ") + juce::String(i) + " Attack (ms)", NormalisableRange<float>(0.1f, 200.0f, 0.01f, 0.35f), 10.0f));
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("release_ms"), 1}, juce::String("Band ") + juce::String(i) + " Release (ms)", NormalisableRange<float>(10.0f, 1000.0f, 0.01f, 0.35f), 120.0f));
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("mix_pct"), 1}, juce::String("Band ") + juce::String(i) + " Mix (%)", NormalisableRange<float>(0.0f, 100.0f, 0.01f, 1.0f), 100.0f));
        ps.push_back(std::make_unique<AudioParameterBool>(ParameterID{id("bypass"), 1}, juce::String("Band ") + juce::String(i) + " Bypass", false));
        ps.push_back(std::make_unique<AudioParameterBool>(ParameterID{id("solo"), 1}, juce::String("Band ") + juce::String(i) + " Solo", false));
        ps.push_back(std::make_unique<AudioParameterBool>(ParameterID{id("delta"), 1}, juce::String("Band ") + juce::String(i) + " Delta", false));
    };

    addBand(1);
    addBand(2);

    // ===== EQ bands (up to 8) =====
    auto hzRange = [](float lo, float hi){ NormalisableRange<float> r(lo, hi); r.setSkewForCentre(1000.0f); return r; };
    for (int i = 1; i <= kMaxEqBands; ++i)
    {
        const auto id = [i](const char* name){ return juce::String("eq.") + juce::String(i) + "." + name; };
        ps.push_back(std::make_unique<AudioParameterBool>(ParameterID{id("enabled"), 1}, juce::String("EQ ") + juce::String(i) + " Enabled", i==1));
        ps.push_back(std::make_unique<AudioParameterChoice>(ParameterID{id("type"), 1}, juce::String("EQ ") + juce::String(i) + " Type",
            StringArray{ "Bell", "LowShelf", "HighShelf", "LowPass", "HighPass", "Notch" }, 0));
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("freq_hz"), 1}, juce::String("EQ ") + juce::String(i) + " Freq",
            hzRange(20.0f, 20000.0f), (i==1 ? 1000.0f : 200.0f * (float) i)));
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("gain_db"), 1}, juce::String("EQ ") + juce::String(i) + " Gain (dB)",
            NormalisableRange<float>(-24.0f, 24.0f, 0.01f, 0.5f), 0.0f));
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id("q"), 1}, juce::String("EQ ") + juce::String(i) + " Q",
            NormalisableRange<float>(0.1f, 10.0f, 0.0f, 0.5f), 1.0f));
    }

    return { ps.begin(), ps.end() };
}

juce::AudioProcessorEditor* HungryGhostMultibandCompressorAudioProcessor::createEditor()
{
#ifdef HG_MBC_HEADLESS_TEST
    return nullptr;
#else
    return new HungryGhostMultibandCompressorAudioProcessorEditor(*this);
#endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostMultibandCompressorAudioProcessor();
}
