#pragma once

#include <JuceHeader.h>
#include <cmath>
#include "ParameterTypes.h"
#include "Allpass.h"
#include "FDN.h"
#include "DampingFilter.h"
#include "DelayLine.h"

namespace hgr::dsp {

class ReverbEngine {
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels)
    {
        fs = sampleRate;
        maxBlock = maxBlockSize;
        channels = juce::jlimit(1, 2, numChannels);

        // Predelay lines per channel (up to 200 ms)
        const int maxPredelaySamples = (int) std::ceil(0.2 * fs);
        for (int ch = 0; ch < 2; ++ch)
        {
            predelay[ch].prepare(fs, maxPredelaySamples);
            // Input diffusion: 4 stages per channel, short delays
            for (int i = 0; i < 4; ++i) {
                diffuser[ch][i].prepare(fs, (int) std::ceil(0.02 * fs)); // up to 20 ms
                diffuser[ch][i].setGain(0.7f);
                diffuser[ch][i].setDelaySamples((int) std::round((0.005f + 0.002f * i) * fs)); // 5..11 ms
            }
        }

        fdn.prepare(fs, maxBlock);
        for (int ch = 0; ch < 2; ++ch) {
            postLowCut[ch].reset();
            postHighCut[ch].reset();
            postLowCut[ch].prepare(sampleRate);
            postHighCut[ch].prepare(sampleRate);
            postHighCut[ch].setCutoffHz(18000.0f);
        }
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch) {
            predelay[ch].reset();
            for (auto& ap : diffuser[ch]) ap.reset();
            postLowCut[ch].reset();
            postHighCut[ch].reset();
        }
        fdn.reset();
        mixSmoothed.reset(fs, 0.05);
        widthSmoothed.reset(fs, 0.05);
    }

    void setParameters(const ReverbParameters& p)
    {
        params = p;

        // Mode mapping (clamp underlying int into enum range)
        const int mi = juce::jlimit(0, 3, static_cast<int>(params.mode));
        mode = static_cast<ReverbMode>(mi);

        // Defaults (Hall-like)
        numDiffusionStages = 4;
        erBlend = 0.15f;
        float sizeMul = 1.0f;
        float hfDampOverrideHz = params.hfDampingHz;
        float gDiffuserBase = 0.70f;
        float rateMul = 1.0f, depthMul = 1.2f;
        int   modMaskVariant = 0; // 0=longest half, 1=more lines
        predelayMul = 1.0f;

        switch (mode)
        {
            case ReverbMode::Room:
                numDiffusionStages = 3;
                erBlend = 0.12f;
                sizeMul = 0.90f;
                hfDampOverrideHz = 8000.0f;
                gDiffuserBase = 0.62f;
                rateMul = 1.2f;
                depthMul = 0.9f;
                modMaskVariant = 0;
                predelayMul = 0.6f;
                break;
            case ReverbMode::Plate:
                numDiffusionStages = 3;
                erBlend = 0.10f;
                sizeMul = 1.00f;
                hfDampOverrideHz = 14000.0f;
                gDiffuserBase = 0.78f;
                rateMul = 1.6f;
                depthMul = 0.7f;
                modMaskVariant = 1; // mod more lines
                predelayMul = 0.3f;
                break;
            case ReverbMode::Ambience:
                numDiffusionStages = 2;
                erBlend = 0.30f;
                sizeMul = 0.75f;
                hfDampOverrideHz = 11000.0f;
                gDiffuserBase = 0.58f;
                rateMul = 0.7f;
                depthMul = 0.5f;
                modMaskVariant = 0;
                predelayMul = 0.2f;
                break;
            case ReverbMode::Hall:
            default:
                numDiffusionStages = 4;
                erBlend = 0.15f;
                sizeMul = 1.20f;
                hfDampOverrideHz = 12000.0f;
                gDiffuserBase = 0.72f;
                rateMul = 0.9f;
                depthMul = 1.3f;
                modMaskVariant = 0;
                predelayMul = 1.2f;
                break;
        }
        numDiffusionStages = juce::jlimit(1, 4, numDiffusionStages);

        // Map to internal modules with mode shaping
        fdn.setSeed(params.seed);
        fdn.setSize(params.size * sizeMul);
        fdn.setRT60(params.decaySeconds);
        fdn.setHFDampingHz(hfDampOverrideHz);
        fdn.setModulation(params.modRateHz * rateMul, params.modDepthMs * depthMul);
        fdn.setModulationMaskVariant(modMaskVariant);

        const float lowCut = juce::jlimit(20.0f, 300.0f, params.lowCutHz);
        const float highCut = juce::jlimit(1000.0f, 20000.0f, params.highCutHz);
        for (int ch = 0; ch < 2; ++ch) {
            postLowCut[ch].setCutoffHz(lowCut);
            postHighCut[ch].setCutoffHz(highCut);
        }

        mixSmoothed.setTargetValue(juce::jlimit(0.0f, 100.0f, params.mixPercent) * 0.01f);
        widthSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, params.width));

        // Diffusion coefficient per mode, scaled by user diffusion (0.6..1.0 factor)
        const float g = juce::jlimit(0.0f, 0.99f, gDiffuserBase * (0.6f + 0.4f * params.diffusion));
        for (int ch = 0; ch < 2; ++ch)
            for (auto& ap : diffuser[ch]) ap.setGain(g);
    }

    void process(juce::dsp::AudioBlock<float>& block)
    {
        juce::ScopedNoDenormals noDenorm;
        const int numCh = (int) block.getNumChannels();
        const int numSamp = (int) block.getNumSamples();
        if (numCh <= 0 || numSamp <= 0)
            return;

        for (int n = 0; n < numSamp; ++n)
        {
            // 1) Read dry input for this sample (cache before writing)
            const float inL = block.getChannelPointer(0)[n];
            const float inR = (numCh > 1 ? block.getChannelPointer(1)[n] : inL);

            // 2) Predelay on each channel's feed, then input diffusion per channel
            const float preL = predelay[0].processSample(inL, predelaySamples());
            const float preR = predelay[1].processSample(inR, predelaySamples());

            float difL = preL;
            float difR = preR;
            for (int i = 0; i < numDiffusionStages; ++i) {
                difL = diffuser[0][i].processSample(difL);
                difR = diffuser[1][i].processSample(difR);
            }

            // 3) FDN tick using mono feed from diffused mid (average L/R)
            float v[FDN8::NumLines];
            const float x = 0.5f * (difL + difR);
            fdn.tick(x, v);

            // 4) Mix to stereo and post-EQ
            float wetL = 0.0f, wetR = 0.0f;
            const float widthNow = widthSmoothed.getNextValue();
            fdn.mixStereo(v, widthNow, wetL, wetR);
            wetL = postEQ(wetL, 0);
            wetR = postEQ(wetR, 1);

            // Blend in a small amount of diffused early reflections to guarantee audible wet
            wetL = (1.0f - erBlend) * wetL + erBlend * difL;
            wetR = (1.0f - erBlend) * wetR + erBlend * difR;

            // 5) Sanity: avoid propagating NaNs/Infs to host
            if (! std::isfinite(wetL)) wetL = 0.0f;
            if (! std::isfinite(wetR)) wetR = 0.0f;

            // 6) Wet/dry mix (clamped) with simple gain compensation so wet-only isn't too quiet
            const float mix = mixSmoothed.getNextValue();
            const float dryGain = 1.0f - mix;
            const float wetGain = mix * 1.2f; // small boost to ensure audibility at 100%
            block.getChannelPointer(0)[n] = dryGain * inL + wetGain * wetL;
            if (numCh > 1)
                block.getChannelPointer(1)[n] = dryGain * inR + wetGain * wetR;
        }
    }

private:
    inline float predelaySamples() const noexcept { return (float) ((params.predelayMs * predelayMul) * 1e-3 * fs); }

    inline float postEQ(float x, int ch) noexcept
    {
        // Simple tone shaping: low-cut by subtracting LP; high-cut is LP itself
        // For now, apply only high-cut (LP) for safety; low-cut reserved for later
        return postHighCut[ch].processSample(x);
    }

    double fs = 48000.0;
    int maxBlock = 512;
    int channels = 2;

    ReverbParameters params;

    DelayLine predelay[2];
    Allpass   diffuser[2][4];
    FDN8      fdn;

    OnePoleLP postLowCut[2], postHighCut[2];

    // Mode profile
    ReverbMode mode = ReverbMode::Hall;
    int numDiffusionStages = 4;
    float erBlend = 0.15f;
    float predelayMul = 1.0f;

    juce::SmoothedValue<float> mixSmoothed { 0.25f }, widthSmoothed { 1.0f };
};

} // namespace hgr::dsp

