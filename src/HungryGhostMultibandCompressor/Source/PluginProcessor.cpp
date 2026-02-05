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
    createFactoryPresets();
}

HungryGhostMultibandCompressorAudioProcessor::~HungryGhostMultibandCompressorAudioProcessor() = default;

void HungryGhostMultibandCompressorAudioProcessor::createFactoryPresets()
{
    factoryPresets.clear();

    // Preset 1: Bus Glue (gentle 2-band compression)
    factoryPresets.push_back(PresetConfig{
        "Bus Glue",
        {
            {"xover.1.Hz", 120.0f},
            {"band.1.threshold_dB", -24.0f},
            {"band.1.ratio", 1.5f},
            {"band.1.knee_dB", 8.0f},
            {"band.1.attack_ms", 15.0f},
            {"band.1.release_ms", 150.0f},
            {"band.1.mix_pct", 100.0f},
            {"band.2.threshold_dB", -20.0f},
            {"band.2.ratio", 1.3f},
            {"band.2.knee_dB", 6.0f},
            {"band.2.attack_ms", 10.0f},
            {"band.2.release_ms", 120.0f},
            {"band.2.mix_pct", 100.0f},
        }
    });

    // Preset 2: Drum Split (60/250 Hz for drums)
    factoryPresets.push_back(PresetConfig{
        "Drum Split",
        {
            {"xover.1.Hz", 60.0f},
            {"band.1.threshold_dB", -18.0f},
            {"band.1.ratio", 4.0f},
            {"band.1.knee_dB", 3.0f},
            {"band.1.attack_ms", 5.0f},
            {"band.1.release_ms", 100.0f},
            {"band.1.mix_pct", 100.0f},
            {"band.2.threshold_dB", -15.0f},
            {"band.2.ratio", 2.5f},
            {"band.2.knee_dB", 6.0f},
            {"band.2.attack_ms", 8.0f},
            {"band.2.release_ms", 80.0f},
            {"band.2.mix_pct", 100.0f},
        }
    });

    // Preset 3: Vocal (150/1.5k Hz for vocals)
    factoryPresets.push_back(PresetConfig{
        "Vocal",
        {
            {"xover.1.Hz", 150.0f},
            {"band.1.threshold_dB", -22.0f},
            {"band.1.ratio", 2.0f},
            {"band.1.knee_dB", 8.0f},
            {"band.1.attack_ms", 20.0f},
            {"band.1.release_ms", 140.0f},
            {"band.1.mix_pct", 100.0f},
            {"band.2.threshold_dB", -18.0f},
            {"band.2.ratio", 3.0f},
            {"band.2.knee_dB", 6.0f},
            {"band.2.attack_ms", 10.0f},
            {"band.2.release_ms", 110.0f},
            {"band.2.mix_pct", 100.0f},
        }
    });

    // Preset 4: Mastering Gentle (conservative settings)
    factoryPresets.push_back(PresetConfig{
        "Mastering Gentle",
        {
            {"xover.1.Hz", 200.0f},
            {"band.1.threshold_dB", -20.0f},
            {"band.1.ratio", 1.2f},
            {"band.1.knee_dB", 10.0f},
            {"band.1.attack_ms", 50.0f},
            {"band.1.release_ms", 200.0f},
            {"band.1.mix_pct", 100.0f},
            {"band.2.threshold_dB", -18.0f},
            {"band.2.ratio", 1.1f},
            {"band.2.knee_dB", 12.0f},
            {"band.2.attack_ms", 40.0f},
            {"band.2.release_ms", 180.0f},
            {"band.2.mix_pct", 100.0f},
        }
    });
}

void HungryGhostMultibandCompressorAudioProcessor::loadPreset(const PresetConfig& preset)
{
    for (const auto& [paramId, value] : preset.parameters)
    {
        if (auto* param = apvts.getRawParameterValue(paramId))
        {
            param->store(value);
        }
    }
}

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
    const int numBands = juce::jmax(1, bandCount);

    // Ensure dry and processed buffers for each band
    if ((int)bandDry.size() != numBands)
        bandDry.resize((size_t) numBands);
    if ((int)bandProc.size() != numBands)
        bandProc.resize((size_t) numBands);

    // Resize each band buffer
    auto ensure = [&](juce::AudioBuffer<float>& b)
    {
        if (b.getNumChannels() != C || b.getNumSamples() < N)
            b.setSize(C, N, false, true, true);
    };

    for (auto& b : bandDry) ensure(b);
    for (auto& b : bandProc) ensure(b);
}

void HungryGhostMultibandCompressorAudioProcessor::prepareToPlay(double sr, int samplesPerBlockExpected)
{
    sampleRateHz = (float) sr;

    // Initial latency report: look-ahead only for now
    float la = 3.0f;
    if (auto* v = apvts.getRawParameterValue("global.lookAheadMs")) la = v->load();
    reportedLatency.store(msToSamples(la, sr));
    setLatencySamples(reportedLatency.load());

    // Prepare DSP components for N-band support
    splitter.reset(new hgmbc::BandSplitterIIR());
    splitter->prepare(sr, getTotalNumInputChannels());

    // Initialize 6 compressor bands (will use 1-6 depending on bandCount parameter)
    compressors.clear();
    const int maxLASamples = juce::jmax(1, msToSamples(20.0f, sr) + 64);
    for (int i = 0; i < 6; ++i)
    {
        auto comp = std::make_unique<hgmbc::CompressorBand>();
        comp->prepare(sr, getTotalNumInputChannels(), maxLASamples);
        compressors.push_back(std::move(comp));
    }

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

    // Read N crossover frequencies
    crossoverHz.clear();
    for (int j = 1; j < bandCount; ++j)
    {
        const juce::String id = "xover." + juce::String(j) + ".Hz";
        if (auto* p = apvts.getRawParameterValue(id))
            crossoverHz.push_back(p->load());
    }


    // Update latency
    const int laSamples = msToSamples(lookAheadMs, sampleRateHz);
    if (laSamples != reportedLatency.load())
    {
        reportedLatency.store(laSamples);
        setLatencySamples(laSamples);
    }

    // Ensure working buffers and update splitter cutoff
    ensureBandBuffers(numCh, numSmps);

    // Extract first crossover frequency for 2-band split (band.1 is low, band.2 is high)
    // For M1: single crossover, bandDry[0]=low, bandDry[1]=high
    float fcHz = 120.0f;
    if (!crossoverHz.empty())
        fcHz = crossoverHz[0];
    splitter->setCrossoverHz(fcHz);

    // Push PRE analyzer mono samples (decimated) from input BEFORE splitting
    if (analyzerFifoPre)
    {
        int write = analyzerFifoPre->getFreeSpace();
        int pushed = 0;
        for (int n = 0; n < numSmps && pushed < write; ++n)
        {
            if ((n % analyzerDecimate) == 0)
            {
                const float m = 0.5f * (buffer.getSample(0, n) + buffer.getSample(juce::jmin(1, numCh-1), n));
                int start1, size1, start2, size2;
                analyzerFifoPre->prepareToWrite(1, start1, size1, start2, size2);
                if (size1 > 0) { analyzerRingPre[(size_t) start1] = m; analyzerFifoPre->finishedWrite(1); ++pushed; }
                else break;
            }
        }
    }

    // Split input into 2 bands using the 2-band splitter API
    // Ensure vectors have exactly 2 elements for M1
    if ((int)bandDry.size() != 2)
        bandDry.resize(2);
    if ((int)bandProc.size() != 2)
        bandProc.resize(2);

    splitter->process(buffer, bandDry[0], bandDry[1]);
    for (int b = 0; b < 2; ++b)
        bandProc[b].makeCopyOf(bandDry[b], true);


    // Configure and process per-band params
    auto rp = [&](const juce::String& id) {
        if (auto* p = apvts.getRawParameterValue(id)) return p->load();
        return 0.0f;
    };

    for (int b = 0; b < bandCount && b < (int)compressors.size() && b < (int)bandProc.size(); ++b)
    {
        const int bandNum = b + 1;
        const juce::String pfx = "band." + juce::String(bandNum) + ".";

        hgmbc::CompressorBandParams params;
        params.threshold_dB = rp(pfx + "threshold_dB");
        params.ratio = rp(pfx + "ratio");
        params.knee_dB = rp(pfx + "knee_dB");
        params.attack_ms = rp(pfx + "attack_ms");
        params.release_ms = rp(pfx + "release_ms");
        params.mix_pct = rp(pfx + "mix_pct");

        compressors[b]->setParams(params);
        compressors[b]->setLookaheadSamples(laSamples);

        if (!(bool) (rp(pfx + "bypass") > 0.5f))
            compressors[b]->process(bandProc[b]);

        if (rp(pfx + "delta") > 0.5f)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                const auto* dry = bandDry[b].getReadPointer(ch);
                auto* wet = bandProc[b].getWritePointer(ch);
                for (int n = 0; n < numSmps; ++n)
                    wet[n] = dry[n] - wet[n];
            }
        }

        grBandDb[b].store(-compressors[b]->getCurrentGainDb());
    }


    // Solo logic
    int soloedBand = -1;
    for (int b = 0; b < bandCount && b < 6; ++b)
    {
        const juce::String id = "band." + juce::String(b + 1) + ".solo";
        if (auto* p = apvts.getRawParameterValue(id))
        {
            if (p->load() > 0.5f)
            {
                soloedBand = b;
                break;
            }
        }
    }

    if (soloedBand >= 0)
    {
        buffer.makeCopyOf(bandProc[soloedBand], true);
    }
    else
    {
        // Sum all processed bands into output
        buffer.clear();
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* out = buffer.getWritePointer(ch);
            for (int n = 0; n < numSmps; ++n)
            {
                float sum = 0.0f;
                for (int b = 0; b < bandCount && b < (int)bandProc.size(); ++b)
                    sum += bandProc[b].getSample(ch, n);
                out[n] = juce::jlimit(-2.0f, 2.0f, sum);
            }
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

    // Crossovers (up to 5 for 6 bands)
    // Default crossover frequencies for 6-band configuration: Low/Mid/Mid2/High/Top
    const float defaultCrossovers[5] = { 120.0f, 400.0f, 1200.0f, 4000.0f, 10000.0f };
    for (int i = 1; i <= 5; ++i)
    {
        const juce::String id = "xover." + juce::String(i) + ".Hz";
        ps.push_back(std::make_unique<AudioParameterFloat>(ParameterID{id, 1},
            "Crossover " + juce::String(i) + " (Hz)",
            NormalisableRange<float>(20.0f, 20000.0f, 0.01f, 0.3f),
            defaultCrossovers[i - 1]));
    }

    // Per-band parameters (define for all 6 bands)
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

    // Add all 6 bands
    for (int i = 1; i <= 6; ++i)
        addBand(i);

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

int HungryGhostMultibandCompressorAudioProcessor::getNumPrograms()
{
    return (int)factoryPresets.size();
}

int HungryGhostMultibandCompressorAudioProcessor::getCurrentProgram()
{
    return currentProgramIndex;
}

void HungryGhostMultibandCompressorAudioProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < (int)factoryPresets.size())
    {
        currentProgramIndex = index;
        loadPreset(factoryPresets[index]);
    }
}

const juce::String HungryGhostMultibandCompressorAudioProcessor::getProgramName(int index)
{
    if (index >= 0 && index < (int)factoryPresets.size())
        return factoryPresets[index].name;
    return {};
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostMultibandCompressorAudioProcessor();
}
