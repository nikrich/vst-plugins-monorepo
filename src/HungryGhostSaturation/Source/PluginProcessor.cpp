#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

HungryGhostSaturationAudioProcessor::HungryGhostSaturationAudioProcessor()
: AudioProcessor (BusesProperties()
                    .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                    .withOutput("Output", juce::AudioChannelSet::stereo(), true))
, apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
    mixSmoothed.reset(44100.0, 0.02);
    makeupSmoothedL.reset(44100.0, 0.06);
    makeupSmoothedR.reset(44100.0, 0.06);
}

void HungryGhostSaturationAudioProcessor::prepareToPlay(double newSampleRate, int samplesPerBlock)
{
    sampleRate = static_cast<float>(newSampleRate);
    maxBlock = samplesPerBlock;
    lastNumChannels = juce::jlimit(1, 2, getTotalNumOutputChannels());

    // RMS smoothing factor scaled for sample rate (approx 1e-3 at 48k)
    alphaRms = 1.0e-3f * (48000.0f / juce::jmax(1.0f, sampleRate));
    alphaRms = juce::jlimit(1.0e-5f, 5.0e-3f, alphaRms);

    // DC blocker coefficient for ~10 Hz
    const float fc = 10.0f;
    dcR = std::exp(-2.0f * juce::MathConstants<float>::pi * fc / sampleRate);
    dcStates.assign((size_t) lastNumChannels, {});

    // Prepare filters
    juce::dsp::ProcessSpec spec { sampleRate, static_cast<juce::uint32>(samplesPerBlock), static_cast<juce::uint32>(lastNumChannels) };
    preTilt.prepare(spec);
    postDeTilt.prepare(spec);
    postLP.prepare(spec);

    // Smoothers
    mixSmoothed.reset(sampleRate, 0.02);
    makeupSmoothedL.reset(sampleRate, 0.08);
    makeupSmoothedR.reset(sampleRate, 0.08);
    mixSmoothed.setCurrentAndTargetValue(1.0f);
    makeupSmoothedL.setCurrentAndTargetValue(1.0f);
    makeupSmoothedR.setCurrentAndTargetValue(1.0f);

    // Buffers
    dryBuffer.setSize(lastNumChannels, samplesPerBlock, false, true, true);
    monoScratch.setSize(1, samplesPerBlock, false, true, true);

    // Oversampling
    updateParameters();
    updateOversamplingIfNeeded(lastNumChannels);
    resetDSPState();
}

void HungryGhostSaturationAudioProcessor::releaseResources()
{
    dryBuffer.setSize(0, 0);
    monoScratch.setSize(0, 0);
}

bool HungryGhostSaturationAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    const bool monoOK   = in == juce::AudioChannelSet::mono()   && out == juce::AudioChannelSet::mono();
    const bool stereoOK = in == juce::AudioChannelSet::stereo() && out == juce::AudioChannelSet::stereo();
    return monoOK || stereoOK;
}

static inline float softClipCubic1(float u)
{
    // Saturates to +/-1 at u=+/-1; smooth cubic
    if (u >= 1.0f) return 1.0f;
    if (u <= -1.0f) return -1.0f;
    return 1.5f * u - 0.5f * u * u * u;
}

void HungryGhostSaturationAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals _;

    const int numSamples  = buffer.getNumSamples();
    const int busChannels = getTotalNumOutputChannels();
    lastNumChannels = juce::jlimit(1, 2, busChannels);

    // Ensure buffers sized
    if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() != lastNumChannels)
        dryBuffer.setSize(lastNumChannels, numSamples, false, false, true);
    if (monoScratch.getNumSamples() < numSamples)
        monoScratch.setSize(1, numSamples, false, false, true);

    // Update parameter-cached state and rebuild oversampling if needed
    updateParameters();
    updateOversamplingIfNeeded(lastNumChannels);

    // Determine processing buffer based on ChannelMode
    juce::AudioBuffer<float>* procBuf = &buffer;
    int procChans = lastNumChannels;

    if (channelMode == ChannelMode::MonoSum)
    {
        // Sum to mono: average L and R (or pass-through if mono)
        auto* mono = monoScratch.getWritePointer(0);
        for (int n = 0; n < numSamples; ++n)
        {
            float s = 0.0f;
            for (int ch = 0; ch < lastNumChannels; ++ch)
                s += buffer.getReadPointer(ch)[n];
            mono[n] = s * (1.0f / (float) juce::jmax(1, lastNumChannels));
        }
        procBuf = &monoScratch;
        procChans = 1;
    }

    // Input trim and dry tap (post trim, pre everything else)
    for (int ch = 0; ch < procChans; ++ch)
    {
        auto* d = procBuf->getWritePointer(ch);
        for (int n = 0; n < numSamples; ++n)
            d[n] *= inGain;
    }
    dryBuffer.makeCopyOf(*procBuf, true);

    // Pre-emphasis
    if (enablePreTilt)
    {
        juce::dsp::AudioBlock<float> blk (*procBuf);
        juce::dsp::ProcessContextReplacing<float> ctx (blk);
        preTilt.process(ctx);
    }

    // Oversample up
    juce::dsp::AudioBlock<float> blk (*procBuf);
    juce::dsp::ProcessContextReplacing<float> ctx (blk);
    juce::dsp::AudioBlock<float> upBlk = oversampling ? oversampling->processSamplesUp(blk)
                                                      : blk;

    // Shaper
    {
        for (size_t ch = 0; ch < upBlk.getNumChannels(); ++ch)
        {
            auto* d = upBlk.getChannelPointer(ch);
            for (size_t n = 0; n < upBlk.getNumSamples(); ++n)
            {
                float x = juce::jlimit(-1.0f, 1.0f, d[n]);
                float y = 0.0f;
                switch (model)
                {
                    case Model::TANH:
                        y = std::tanh(k * x) * invTanhK; break;
                    case Model::ATAN:
                        y = std::atan(k * x) * invAtanK; break;
                    case Model::SOFT:
                    {
                        float u = juce::jlimit(-1.0f, 1.0f, k * x);
                        y = softClipCubic1(u);
                    } break;
                    case Model::FEXP:
                    {
                        float xa = juce::jlimit(-1.0f, 1.0f, x + asym);
                        float denom = 1.0f - std::exp(-k);
                        if (denom <= 1.0e-6f) denom = 1.0e-6f;
                        y = (1.0f - std::exp(-k * xa)) / denom;
                    } break;
                }
                d[n] = juce::jlimit(-1.0f, 1.0f, y);
            }
        }
    }

    // Oversample down
    if (oversampling)
        oversampling->processSamplesDown(blk);

    // DC-block for asymmetric mode (post-down)
    if (model == Model::FEXP)
    {
        for (int ch = 0; ch < procChans; ++ch)
        {
            auto* d = procBuf->getWritePointer(ch);
            auto& st = dcStates[(size_t) juce::jmin(ch, (int)dcStates.size()-1)];
            for (int n = 0; n < numSamples; ++n)
            {
                float x = d[n];
                float y = x - st.x1 + dcR * st.y1;
                st.x1 = x; st.y1 = y;
                d[n] = y;
            }
        }
    }

    // Post de-emphasis and optional LP
    if (enablePreTilt)
    {
        juce::dsp::AudioBlock<float> b (*procBuf);
        juce::dsp::ProcessContextReplacing<float> c (b);
        postDeTilt.process(c);
    }
    if (enablePostLP)
    {
        juce::dsp::AudioBlock<float> b (*procBuf);
        juce::dsp::ProcessContextReplacing<float> c (b);
        postLP.process(c);
    }

    // Auto gain: compute from dryBuffer (input) and procBuf (wet, pre-output trim)
    float makeupL = 1.0f, makeupR = 1.0f;
    if (autoGain)
    {
        // Update mean-square trackers
        if (channelMode == ChannelMode::DualMono)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                float xinL = dryBuffer.getSample(0, n);
                float xoutL = procBuf->getSample(0, n);
                eIn[0]  += alphaRms * (xinL * xinL - eIn[0]);
                eOut[0] += alphaRms * (xoutL * xoutL - eOut[0]);
                if (procChans > 1)
                {
                    float xinR = dryBuffer.getSample(1, n);
                    float xoutR = procBuf->getSample(1, n);
                    eIn[1]  += alphaRms * (xinR * xinR - eIn[1]);
                    eOut[1] += alphaRms * (xoutR * xoutR - eOut[1]);
                }
            }
            float rmsInL = std::sqrt(juce::jmax(1.0e-8f, eIn[0]));
            float rmsOutL = std::sqrt(juce::jmax(1.0e-8f, eOut[0]));
            float targetL = juce::jlimit(0.25f, 4.0f, rmsInL / juce::jmax(1.0e-6f, rmsOutL));
            makeupSmoothedL.setTargetValue(targetL);
            makeupL = makeupSmoothedL.getNextValue();

            if (procChans > 1)
            {
                float rmsInR = std::sqrt(juce::jmax(1.0e-8f, eIn[1]));
                float rmsOutR = std::sqrt(juce::jmax(1.0e-8f, eOut[1]));
                float targetR = juce::jlimit(0.25f, 4.0f, rmsInR / juce::jmax(1.0e-6f, rmsOutR));
                makeupSmoothedR.setTargetValue(targetR);
                makeupR = makeupSmoothedR.getNextValue();
            }
        }
        else
        {
            // Combined stereo or mono
            for (int n = 0; n < numSamples; ++n)
            {
                float xin = 0.0f, xout = 0.0f;
                for (int ch = 0; ch < procChans; ++ch)
                {
                    xin  += dryBuffer.getSample(ch, n);
                    xout += procBuf->getSample(ch, n);
                }
                xin  *= (1.0f / (float) procChans);
                xout *= (1.0f / (float) procChans);
                eIn[0]  += alphaRms * (xin * xin - eIn[0]);
                eOut[0] += alphaRms * (xout * xout - eOut[0]);
            }
            float rmsIn  = std::sqrt(juce::jmax(1.0e-8f, eIn[0]));
            float rmsOut = std::sqrt(juce::jmax(1.0e-8f, eOut[0]));
            float target = juce::jlimit(0.25f, 4.0f, rmsIn / juce::jmax(1.0e-6f, rmsOut));
            makeupSmoothedL.setTargetValue(target);
            makeupL = makeupSmoothedL.getNextValue();
            makeupR = makeupL;
        }

        // Apply makeup to wet path only
        for (int ch = 0; ch < procChans; ++ch)
        {
            float mk = (ch == 0 ? makeupL : makeupR);
            procBuf->applyGain(ch, 0, numSamples, mk);
        }
    }

    // Output trim (wet path)
    for (int ch = 0; ch < procChans; ++ch)
        procBuf->applyGain(ch, 0, numSamples, outGain);

    // Equal-power dry/wet mix into final output buffer
    mixSmoothed.setTargetValue(mixTarget);

    // If we processed mono but output bus is stereo, we'll mix into both channels below
    if (channelMode == ChannelMode::MonoSum && lastNumChannels == 2)
        buffer.clear();

    for (int n = 0; n < numSamples; ++n)
    {
        float m = mixSmoothed.getNextValue();
        float wW = std::sqrt(juce::jlimit(0.0f, 1.0f, m));
        float dW = std::sqrt(juce::jlimit(0.0f, 1.0f, 1.0f - m));

        if (channelMode == ChannelMode::MonoSum)
        {
            float dry = dryBuffer.getSample(0, n);
            float wet = procBuf->getSample(0, n);
            float out = dW * dry + wW * wet;
            if (getTotalNumOutputChannels() == 1)
                buffer.setSample(0, n, out);
            else
            {
                buffer.setSample(0, n, out);
                buffer.setSample(1, n, out);
            }
        }
        else
        {
            for (int ch = 0; ch < lastNumChannels; ++ch)
            {
                float dry = dryBuffer.getSample(ch, n);
                float wet = buffer.getSample(ch, n); // procBuf == &buffer in stereo/dual-mono
                float out = dW * dry + wW * wet;
                buffer.setSample(ch, n, out);
            }
        }
    }

    // Clear any output channels above our processing channels
    for (int ch = lastNumChannels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}

void HungryGhostSaturationAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto s = apvts.copyState();
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
    using P = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.emplace_back(std::make_unique<juce::AudioParameterFloat>("in",   "Input",  juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));
    params.emplace_back(std::make_unique<juce::AudioParameterFloat>("drive","Drive",  juce::NormalisableRange<float>(0.f, 36.f, 0.01f), 12.f));
    params.emplace_back(std::make_unique<juce::AudioParameterChoice>("model","Model", juce::StringArray{"TANH","ATAN","SOFT","FEXP"}, 0));
    params.emplace_back(std::make_unique<juce::AudioParameterFloat>("asym", "Asymmetry", juce::NormalisableRange<float>(-0.5f, 0.5f, 0.001f), 0.f));
    params.emplace_back(std::make_unique<juce::AudioParameterFloat>("pretilt","PreTilt dB/oct", juce::NormalisableRange<float>(0.f, 6.f, 0.01f), 0.f));
    params.emplace_back(std::make_unique<juce::AudioParameterChoice>("postlp","Post LP", juce::StringArray{"Off","22k","16k","12k","8k"}, 0));
    params.emplace_back(std::make_unique<juce::AudioParameterChoice>("os",   "Oversampling", juce::StringArray{"1x","2x","4x"}, 1));
    params.emplace_back(std::make_unique<juce::AudioParameterFloat>("mix",  "Mix",    juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 1.f));
    params.emplace_back(std::make_unique<juce::AudioParameterBool>("autoGain","Auto Gain", true));
    params.emplace_back(std::make_unique<juce::AudioParameterFloat>("out",  "Output", juce::NormalisableRange<float>(-24.f, 24.f, 0.01f), 0.f));
    params.emplace_back(std::make_unique<juce::AudioParameterChoice>("channelMode", "Channel Mode", juce::StringArray{"Stereo","DualMono","MonoSum"}, 0));

    return { params.begin(), params.end() };
}

void HungryGhostSaturationAudioProcessor::updateParameters()
{
    inGain  = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("in")->load());
    outGain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue("out")->load());
    mixTarget = apvts.getRawParameterValue("mix")->load();

    driveDb = apvts.getRawParameterValue("drive")->load();
    k = mapDriveDbToK(driveDb);
    invTanhK = 1.0f / (std::tanh(k) + 1.0e-6f);
    invAtanK = 1.0f / (std::atan(k) + 1.0e-6f);

    asym = apvts.getRawParameterValue("asym")->load();

    const int mdl = (int) apvts.getRawParameterValue("model")->load();
    model = (Model) juce::jlimit(0, 3, mdl);

    const int chm = (int) apvts.getRawParameterValue("channelMode")->load();
    channelMode = (ChannelMode) juce::jlimit(0, 2, chm);

    // Pre-tilt shelves
    const float preTiltDbPerOct = apvts.getRawParameterValue("pretilt")->load();
    const float nyq = 0.5f * sampleRate;
    const float octaves = juce::jmax(0.0f, std::log2(juce::jmax(1.0f, nyq) / 200.0f));
    const float totalDb = preTiltDbPerOct * octaves;
    const float gainPre = juce::Decibels::decibelsToGain(totalDb);
    const float gainPost = 1.0f / juce::jmax(1.0e-6f, gainPre);
    enablePreTilt = std::abs(preTiltDbPerOct) > 1.0e-3f;
    if (enablePreTilt)
    {
        preTilt.coefficients    = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 200.0f, 0.707f, gainPre);
        postDeTilt.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf(sampleRate, 200.0f, 0.707f, gainPost);
    }

    // Post LP
    const int lpSel = (int) apvts.getRawParameterValue("postlp")->load();
    const float lpTable[5] = { 0.0f, 22000.0f, 16000.0f, 12000.0f, 8000.0f };
    const float cutoff = lpTable[juce::jlimit(0, 4, lpSel)];
    enablePostLP = cutoff > 0.0f && cutoff < 0.49f * sampleRate;
    if (enablePostLP)
        postLP.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, cutoff, 0.707f);

    // Oversampling selection
    const int osSel = (int) apvts.getRawParameterValue("os")->load();
    const int newFactor = (osSel == 0 ? 1 : (osSel == 1 ? 2 : 4));
    if (newFactor != osFactor)
    {
        osFactor = newFactor;
        osStages = (osFactor == 1 ? 0 : (osFactor == 2 ? 1 : 2));
        updateOversamplingIfNeeded(lastNumChannels);
    }

    autoGain = apvts.getRawParameterValue("autoGain")->load() > 0.5f;
}

void HungryGhostSaturationAudioProcessor::updateOversamplingIfNeeded(int numChannels)
{
    const int desiredStages = (osFactor == 1 ? 0 : (osFactor == 2 ? 1 : 2));
    bool needRebuild = false;
    if (!oversampling) needRebuild = true;
    if (desiredStages != osStages) needRebuild = true;

    if (needRebuild)
    {
        oversampling.reset();
        if (desiredStages > 0)
        {
            oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
                (size_t) juce::jlimit(1, 2, numChannels), desiredStages,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        }
        osStages = desiredStages;
        // Prepare not required explicitly; Oversampling works directly with processSamplesUp/Down
    }
}

void HungryGhostSaturationAudioProcessor::resetDSPState()
{
    preTilt.reset();
    postDeTilt.reset();
    postLP.reset();
    eIn[0] = eIn[1] = 1.0e-4f;
    eOut[0] = eOut[1] = 1.0e-4f;
    for (auto& s : dcStates) { s.x1 = 0.0f; s.y1 = 0.0f; }
}

float HungryGhostSaturationAudioProcessor::mapDriveDbToK(float dB)
{
    // Map 0..36 dB to k in [1..8]
    dB = juce::jlimit(0.0f, 36.0f, dB);
    return juce::jmap(dB, 0.0f, 36.0f, 1.0f, 8.0f);
}

juce::AudioProcessorEditor* HungryGhostSaturationAudioProcessor::createEditor()
{
    return new HungryGhostSaturationAudioProcessorEditor(*this);
}

// JUCE plugin entry point
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HungryGhostSaturationAudioProcessor();
}
