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

        // Smoothers for EQ and tank decay
        hfSm.reset(fs, 0.02);
        lowSm.reset(fs, 0.02);
        highSm.reset(fs, 0.02);
        rt60Sm.reset(fs, 0.02);

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

        fdnA.prepare(fs, maxBlock);
        fdnB.prepare(fs, maxBlock);
        useA = true;
        xfActive = false;
        xf = 0.0f;
        const float tauXf = 0.10f; // 100 ms crossfade
        xfAlpha = 1.0f - std::exp(-1.0f / (float) (tauXf * fs));
        lastSizeApplied = 1.0f;
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
        fdnA.reset();
        fdnB.reset();
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
        // Apply to both tanks; crossfade will decide which is audible
        const float sizeEff = params.size * sizeMul;
        const float rt60Eff = params.decaySeconds;
        const float hfEff   = hfDampOverrideHz;

        // Clamp modulation depth for non-Plate modes to avoid chorus; allow deeper in Plate
        float modDepthMsPolicy = params.modDepthMs * depthMul;
        if (mode != ReverbMode::Plate)
        {
            const float depthSamples = (float) (modDepthMsPolicy * 1e-3 * fs);
            const float cappedSamples = juce::jlimit(0.0f, 8.0f * (float) (fs / 48000.0), depthSamples);
            modDepthMsPolicy = cappedSamples * 1e3f / (float) fs;
        }

        auto applyCommon = [&](FDN8& f){
            f.setSeed(params.seed);
            f.setRT60(rt60Eff);
            f.setHFDampingHz(hfEff);
            f.setModulation(params.modRateHz * rateMul, modDepthMsPolicy);
            f.setModulationMaskVariant(modMaskVariant);
        };

        // Decide whether to start a size crossfade
        const float sizeDelta = std::abs(sizeEff - lastSizeApplied);
        const float sizeThresh = 0.02f; // 2% change triggers crossfade
        if (!xfActive && sizeDelta > sizeThresh)
        {
            // Configure idle tank with new size and common params
            idleFdn().setSize(sizeEff);
            applyCommon(idleFdn());
            // Ensure active tank keeps old size until crossfade completes
            applyCommon(activeFdn());
            // Start crossfade
            xfActive = true;
            xf = 0.0f;
            pendingSize = sizeEff;
        }
        else
        {
            // No crossfade: apply size immediately to both (safe small change)
            activeFdn().setSize(sizeEff);
            idleFdn().setSize(sizeEff);
            applyCommon(activeFdn());
            applyCommon(idleFdn());
            lastSizeApplied = sizeEff;
        }

        const float lowCut = juce::jlimit(20.0f, 300.0f, params.lowCutHz);
        const float highCut = juce::jlimit(1000.0f, 20000.0f, params.highCutHz);
        for (int ch = 0; ch < 2; ++ch) {
            postLowCut[ch].setCutoffHz(lowCut);
            postHighCut[ch].setCutoffHz(highCut);
        }

        // Smooth targets for EQ and damping
        hfSm.setTargetValue(hfEff);
        lowSm.setTargetValue(lowCut);
        highSm.setTargetValue(highCut);
        rt60Sm.setTargetValue(rt60Eff);

        // Slight channel decorrelation: jitter diffuser delays deterministically by +/- ~0.15 ms per channel
        auto jitterSign = [&](int stage, int ch){
            uint32_t v = (uint32_t) params.seed;
            v ^= (uint32_t) (stage * 97);
            v ^= (uint32_t) (ch * 131);
            return (v & 1u) ? +1.0f : -1.0f;
        };
        for (int ch = 0; ch < 2; ++ch) {
            for (int i = 0; i < 4; ++i) {
                const float baseMs = 5.0f + 2.0f * (float) i;
                const float jitterMs = 0.15f * jitterSign(i, ch);
                const int dSamp = (int) std::round((baseMs + jitterMs) * 1e-3f * (float) fs);
                diffuser[ch][i].setDelaySamples(juce::jmax(1, dSamp));
            }
        }

        mixSmoothed.setTargetValue(juce::jlimit(0.0f, 100.0f, params.mixPercent) * 0.01f);
        widthSmoothed.setTargetValue(juce::jlimit(0.0f, 1.0f, params.width));

        // Freeze handling after params applied so it can override motion/EQ
        activeFdn().setFreeze(params.freeze);
        idleFdn().setFreeze(params.freeze);

        // Diffusion coefficient with perceptual taper; mode base steers range
        const float t = std::pow(juce::jlimit(0.0f, 1.0f, params.diffusion), 0.65f);
        float minG = juce::jlimit(0.6f, 0.85f, gDiffuserBase - 0.10f);
        float maxG = juce::jlimit(0.6f, 0.85f, gDiffuserBase + 0.10f);
        if (minG > maxG) std::swap(minG, maxG);
        const float g = juce::jlimit(0.0f, 0.99f, juce::jmap(t, minG, maxG));
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

            // 3) Prepare mono feed from diffused mid (average L/R)
            const float x = 0.5f * (difL + difR);

            // 4) Mix to stereo and post-EQ
            float wetL = 0.0f, wetR = 0.0f;
            const float widthNow = widthSmoothed.getNextValue();

            // Smoothly update EQ and tank damping/decay a few times per block
            const int updStep = juce::jmax(1, numSamp / 8);
            if ((n % updStep) == 0)
            {
                const float hfNow = hfSm.getNextValue();
                const float lcNow = lowSm.getNextValue();
                const float hcNow = highSm.getNextValue();
                const float rtNow = rt60Sm.getNextValue();
                activeFdn().setHFDampingHz(hfNow);
                idleFdn().setHFDampingHz(hfNow);
                activeFdn().setRT60(rtNow);
                idleFdn().setRT60(rtNow);
                postLowCut[0].setCutoffHz(lcNow); postLowCut[1].setCutoffHz(lcNow);
                postHighCut[0].setCutoffHz(hcNow); postHighCut[1].setCutoffHz(hcNow);
            }

            float wetL_A = 0.0f, wetR_A = 0.0f;
            float wetL_B = 0.0f, wetR_B = 0.0f;

            if (xfActive)
            {
                float vA[FDN8::NumLines];
                float vB[FDN8::NumLines];
                // Tick both tanks
                activeFdn().tick(x, vA);
                idleFdn().tick(x, vB);
                activeFdn().mixStereo(vA, widthNow, wetL_A, wetR_A);
                idleFdn().mixStereo(vB, widthNow, wetL_B, wetR_B);
                // Advance crossfade
                xf += (1.0f - xf) * xfAlpha;
                if (xf > 0.999f)
                {
                    // Finish crossfade: swap tanks
                    std::swap(fdnA, fdnB);
                    useA = true; // after swap, A is active again
                    xfActive = false;
                    xf = 0.0f;
                    lastSizeApplied = pendingSize;
                }
            }
            else
            {
                float vA[FDN8::NumLines];
                activeFdn().tick(x, vA);
                activeFdn().mixStereo(vA, widthNow, wetL_A, wetR_A);
            }

            float wetLmix = xfActive ? ((1.0f - xf) * wetL_A + xf * wetL_B) : wetL_A;
            float wetRmix = xfActive ? ((1.0f - xf) * wetR_A + xf * wetR_B) : wetR_A;
            
            wetL = wetLmix;
            wetR = wetRmix;
            wetL = postEQ(wetL, 0);
            wetR = postEQ(wetR, 1);

            // Blend in a small amount of diffused early reflections to guarantee audible wet
            wetL = (1.0f - erBlend) * wetL + erBlend * difL;
            wetR = (1.0f - erBlend) * wetR + erBlend * difR;

            // 5) Sanity: avoid propagating NaNs/Infs to host
            if (! std::isfinite(wetL)) wetL = 0.0f;
            if (! std::isfinite(wetR)) wetR = 0.0f;

            // 6) Equal-power wet/dry mix to preserve perceived loudness
            const float mix = mixSmoothed.getNextValue();
            const float dryGain = std::sqrt(1.0f - mix);
            const float wetGain = std::sqrt(mix);
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
        // High-pass via subtraction (HP = x - LP), then high-cut LP
        const float lp = postLowCut[ch].processSample(x);
        const float hp = x - lp;
        return postHighCut[ch].processSample(hp);
    }

    double fs = 48000.0;
    int maxBlock = 512;
    int channels = 2;

    ReverbParameters params;

    DelayLine predelay[2];
    Allpass   diffuser[2][4];
    FDN8      fdnA, fdnB;

    // Crossfade state for size changes
    bool  useA = true;
    bool  xfActive = false;
    float xf = 0.0f;
    float xfAlpha = 0.02f;
    float lastSizeApplied = 1.0f;
    float pendingSize = 1.0f;

    OnePoleLP postLowCut[2], postHighCut[2];

    // Mode profile
    ReverbMode mode = ReverbMode::Hall;
    int numDiffusionStages = 4;
    float erBlend = 0.15f;
    float predelayMul = 1.0f;

    juce::SmoothedValue<float> mixSmoothed { 0.25f }, widthSmoothed { 1.0f };
    juce::SmoothedValue<float> hfSm { 6000.0f }, lowSm { 100.0f }, highSm { 18000.0f }, rt60Sm { 3.0f };

    inline FDN8& activeFdn() noexcept { return useA ? fdnA : fdnB; }
    inline FDN8& idleFdn()   noexcept { return useA ? fdnB : fdnA; }
};

} // namespace hgr::dsp

