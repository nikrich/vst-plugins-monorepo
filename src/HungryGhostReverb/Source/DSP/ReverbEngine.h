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
        postLowCut.reset(); postHighCut.reset();
        postLowCut.prepare(sampleRate);
        postHighCut.prepare(sampleRate);
        postHighCut.setCutoffHz(18000.0f);
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch) {
            predelay[ch].reset();
            for (auto& ap : diffuser[ch]) ap.reset();
        }
        fdn.reset();
        postLowCut.reset();
        postHighCut.reset();
        mixSmoothed.reset(fs, 0.05);
        widthSmoothed.reset(fs, 0.05);
    }

    void setParameters(const ReverbParameters& p)
    {
        params = p;
        // Map to internal modules
        fdn.setSeed(params.seed);
        fdn.setSize(params.size);
        fdn.setRT60(params.decaySeconds);
        fdn.setHFDampingHz(params.hfDampingHz);
        fdn.setModulation(params.modRateHz, params.modDepthMs);
        postLowCut.setCutoffHz(juce::jlimit(20.0f, 300.0f, params.lowCutHz));
        postHighCut.setCutoffHz(juce::jlimit(1000.0f, 20000.0f, params.highCutHz));

        mixSmoothed.setTargetValue(juce::jlimit(0.0f, 100.0f, params.mixPercent) * 0.01f);
        widthSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, params.width));

        // Update diffusion coefficient across stages
        const float g = juce::jlimit(0.0f, 0.99f, 0.6f + 0.35f * params.diffusion);
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
            for (int i = 0; i < 4; ++i) {
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
            wetL = postEQ(wetL);
            wetR = postEQ(wetR);

            // Blend in a small amount of diffused early reflections to guarantee audible wet
            const float erBlend = 0.15f;
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
    inline float predelaySamples() const noexcept { return (float) (params.predelayMs * 1e-3 * fs); }

    inline float postEQ(float x) noexcept
    {
        // Simple tone shaping: low-cut by subtracting LP; high-cut is LP itself
        // For now, apply only high-cut (LP) for safety; low-cut reserved for later
        return postHighCut.processSample(x);
    }

    double fs = 48000.0;
    int maxBlock = 512;
    int channels = 2;

    ReverbParameters params;

    DelayLine predelay[2];
    Allpass   diffuser[2][4];
    FDN8      fdn;

    OnePoleLP postLowCut, postHighCut;

    juce::SmoothedValue<float> mixSmoothed { 0.25f }, widthSmoothed { 1.0f };
};

} // namespace hgr::dsp

