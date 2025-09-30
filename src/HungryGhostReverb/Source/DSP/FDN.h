#pragma once

#include <array>
#include <vector>
#include <cmath>
#include <algorithm>

#include "DelayLine.h"
#include "DampingFilter.h"
#include "Modulator.h"

namespace hgr::dsp {

// 8x8 Feedback Delay Network with Hadamard feedback matrix
class FDN8 {
public:
    static constexpr int NumLines = 8;

    void prepare(double sampleRate, int maxBlockSize)
    {
        fs = sampleRate;
        // Freeze ramp coefficient for ~75 ms time constant
        const float tauSec = 0.075f;
        freezeAlpha = 1.0f - std::exp(-1.0f / (float) (tauSec * fs));
        freezeXf = 0.0f;
        freezeTarget = 0.0f;

        // Compute a worst-case max delay capacity to cover SR/Size/Mod ranges
        // baseMax48k = longest base delay at 48k; sizeMax = 1.5; modMaxS = 10 ms in samples
        const int baseMax48k = 6229;
        const float sizeMax  = 1.5f;
        const float modMaxS  = (10e-3f) * (float) fs;
        const double scale   = fs / 48000.0;
        const int maxDelaySamples = DelayLine::nextPow2((int) std::ceil(baseMax48k * scale * sizeMax + modMaxS + 4.0));
        
        for (int i = 0; i < NumLines; ++i)
        {
            lines[i].prepare(fs, maxDelaySamples);
            dampers[i].prepare(fs);
            dampers[i].setCutoffHz(hfHz);
            lfos[i].prepare(fs, seed + i * 17);
        }
        setSize(1.0f);
        setRT60(3.0f);
        setHFDampingHz(6000.0f);
        setModulation(0.3f, 1.5f);
        computeHadamardScale();
    }

    void reset()
    {
        for (int i = 0; i < NumLines; ++i) { lines[i].reset(); dampers[i].reset(); }
        std::fill(prevOut.begin(), prevOut.end(), 0.0f);
    }

    void setSeed(int s)
    {
        seed = s;
        for (int i = 0; i < NumLines; ++i) lfos[i].prepare(fs, seed + i * 17);
    }

    void setSize(float size)
    {
        sizeScale = std::clamp(size, 0.5f, 1.5f);
        // Base delays (samples at 48k reference). Mutually inharmonic-ish lengths.
        const int base48k[NumLines] = { 1421, 1877, 2269, 2791, 3359, 4217, 5183, 6229 };
        for (int i = 0; i < NumLines; ++i)
        {
            const float scaled = base48k[i] * (float) (fs / 48000.0) * sizeScale;
            lines[i].setBaseDelaySamples((int) std::round(scaled));
        }
        updateGi();
    }

    void setRT60(float seconds)
    {
        rt60 = std::clamp(seconds, 0.1f, 60.0f);
        updateGi();
    }

    void setHFDampingHz(float hz)
    {
        hfHz = std::clamp(hz, 1000.0f, 20000.0f);
        for (auto& d : dampers) d.setCutoffHz(hfHz);
    }

    void setFreeze(bool on)
    {
        freezeTarget = on ? 1.0f : 0.0f;
    }

    void setModulation(float rateHz, float depthMs)
    {
        modRateHz = std::clamp(rateHz, 0.01f, 8.0f);
        modDepthMs = std::clamp(depthMs, 0.0f, 10.0f);
        const float depthSamples = (float) (modDepthMs * 1e-3 * fs);
        const float lagrThresh = 8.0f * (float) (fs / 48000.0); // ~8 samples at 48k
        for (int i = 0; i < NumLines; ++i)
        {
            lfos[i].setRateHz(modRateHz * (1.0f + 0.03f * (float) i)); // slight offsets
            const float dS = depthSamples * depthMask(i);
            lfos[i].setDepthSamples(dS);
            interpMode[i] = (dS >= lagrThresh) ? DelayLine::InterpMode::Lagrange3 : DelayLine::InterpMode::Linear;
        }
    }

    void setModulationMaskVariant(int variant) noexcept { modMaskVariant = (variant != 0 ? 1 : 0); }

    // Inject mono input x into the network and produce N raw line outputs into out[]
    inline void tick(float xIn, float out[NumLines]) noexcept
    {
        // Update freeze ramp
        freezeXf += (freezeTarget - freezeXf) * freezeAlpha;
        if (freezeXf < 0.0f) freezeXf = 0.0f;
        if (freezeXf > 1.0f) freezeXf = 1.0f;
        const float motionScale = 1.0f - freezeXf; // 1 → normal, 0 → frozen

        // 1) Read current delay outputs with modulation (apply on longest lines only)
        float v[NumLines];
        for (int i = 0; i < NumLines; ++i)
        {
            const float lfo = lfos[i].nextOffsetSamples() * motionScale;
            const float baseD = (float) lines[i].getBaseDelaySamples();
            v[i] = lines[i].readInterpolated(baseD, lfo, interpMode[i]);
        }

        // 2) Hadamard mix (unitary up to scale); produce feedback vector fb
        float fb[NumLines];
        hadamard(v, fb);

        // 3) Apply per-line feedback gain and damping (feedback-only), then write next state with input injection
        const float xinGain = motionScale; // mute input when frozen
        for (int i = 0; i < NumLines; ++i)
        {
            const float fbGain = gi[i] * motionScale + 0.99995f * (1.0f - motionScale);
            const float fbSig  = fb[i] * fbGain;
            const float damped = dampers[i].processSample(fbSig); // LP only on recirculating path
            const float in     = damped + inputTap(i) * (xinGain * xIn); // undamped input injection keeps ER brighter
            
            lines[i].pushSample(in);
            out[i] = v[i];
        }
    }

    // Stereo mix from line outputs using fixed tap weights and width
    inline void mixStereo(const float v[NumLines], float width, float& outL, float& outR) const noexcept
    {
        // Mix with simple orthogonal patterns and normalize by sqrt(N)
        float sumL = 0.0f, sumR = 0.0f;
        for (int i = 0; i < NumLines; ++i)
        {
            const float s = (i & 1) ? -1.0f : 1.0f;
            sumL += v[i] * s;
            sumR += v[(i + 3) & (NumLines - 1)] * -s; // permuted index for decorrelation
        }
        const float norm  = 1.0f / std::sqrt((float) NumLines);
        const float mid0  = (sumL + sumR) * 0.5f * norm;
        const float side0 = (sumL - sumR) * 0.5f * norm;

        // Equal-power width law: map width in [0..1] to energy-preserving M/S scales
        const float w = std::clamp(width, 0.0f, 1.0f);
        constexpr float pi = 3.14159265358979323846f;
        const float sScale = std::sin(0.5f * pi * w);
        const float mScale = std::sqrt(1.0f - sScale * sScale);

        outL = mScale * mid0 + sScale * side0;
        outR = mScale * mid0 - sScale * side0;
    }

private:
    void computeHadamardScale() { hadamardScale = 1.0f / std::sqrt((float) NumLines); }

    void hadamard(const float in[NumLines], float out[NumLines]) const noexcept
    {
        // unrolled 8-point hadamard butterflies
        float s0 = in[0] + in[1]; float d0 = in[0] - in[1];
        float s1 = in[2] + in[3]; float d1 = in[2] - in[3];
        float s2 = in[4] + in[5]; float d2 = in[4] - in[5];
        float s3 = in[6] + in[7]; float d3 = in[6] - in[7];
        float s4 = s0 + s1;      float d4 = s0 - s1;
        float s5 = d0 + d1;      float d5 = d0 - d1;
        float s6 = s2 + s3;      float d6 = s2 - s3;
        float s7 = d2 + d3;      float d7 = d2 - d3;
        out[0] = (s4 + s6) * hadamardScale;
        out[1] = (s5 + d7) * hadamardScale;
        out[2] = (d4 + s7) * hadamardScale;
        out[3] = (d5 + d6) * hadamardScale;
        out[4] = (s4 - s6) * hadamardScale;
        out[5] = (s5 - d7) * hadamardScale;
        out[6] = (d4 - s7) * hadamardScale;
        out[7] = (d5 - d6) * hadamardScale;
    }

    void updateGi()
    {
        // RT60 mapping: per-line gain so each delay line achieves ≈60 dB decay over rt60
        for (int i = 0; i < NumLines; ++i)
        {
            const int Li = std::max(1, lines[i].getBaseDelaySamples());
            const float g = std::pow(10.0f, (float) (-3.0 * (double) Li / (rt60 * fs)));
            gi[i] = std::clamp(g, 0.0f, 0.99f);
        }
    }

    static float inputTap(int i)
    {
        // alternating signs for width; slightly stronger feed to ensure audible wet
        return (i & 1) ? -0.5f : 0.5f;
    }

    float depthMask(int i) const noexcept
    {
        // Variant 0: longest half only (default)
        // Variant 1: mod more lines (all except the two shortest) for livelier modes like Plate
        if (modMaskVariant == 0)
            return (i >= NumLines / 2) ? 1.0f : 0.0f;
        else
            return (i >= 2) ? 1.0f : 0.0f; // mod 6 longest of 8
    }

    double fs = 48000.0;
    float sizeScale = 1.0f;
    float rt60 = 3.0f;
    float hfHz = 6000.0f;
    float modRateHz = 0.3f;
    float modDepthMs = 1.5f;
    int seed = 1337;
    float hadamardScale = 1.0f;

    std::array<DelayLine, NumLines> lines;
    std::array<OnePoleLP, NumLines> dampers;
    std::array<LFO, NumLines> lfos;
    std::array<float, NumLines> gi { };
    std::array<float, NumLines> prevOut { };
    std::array<DelayLine::InterpMode, NumLines> interpMode { DelayLine::InterpMode::Linear, DelayLine::InterpMode::Linear, DelayLine::InterpMode::Linear, DelayLine::InterpMode::Linear, DelayLine::InterpMode::Linear, DelayLine::InterpMode::Linear, DelayLine::InterpMode::Linear, DelayLine::InterpMode::Linear };

    int modMaskVariant = 0; // 0: longest half, 1: all but two shortest

    // Freeze ramp state
    float freezeXf = 0.0f;     // 0 = normal, 1 = fully frozen
    float freezeTarget = 0.0f; // target state
    float freezeAlpha = 0.02f; // per-sample smoothing coefficient
};

} // namespace hgr::dsp

