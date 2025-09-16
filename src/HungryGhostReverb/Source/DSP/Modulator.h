#pragma once

#include <cmath>
#include <cstdint>
#include <random>

namespace hgr::dsp {

// Simple sine LFO + optional jitter
class LFO {
public:
    void prepare(double sampleRate, int seed = 1337)
    {
        fs = sampleRate;
        phase = 0.0f;
        rng.seed((uint32_t) seed);
    }

    void setRateHz(float hz) { rateHz = hz; updateInc(); }
    void setDepthSamples(float samples) { depthSamples = samples; }

    void setPhase(float ph) { phase = wrap01(ph); }
    void randomisePhase() { std::uniform_real_distribution<float> d(0.0f, 1.0f); setPhase(d(rng)); }

    inline float nextOffsetSamples()
    {
        // sine LFO; jitter is tiny filtered noise
        const float twoPi = 6.283185307179586f;
        const float off = std::sin(twoPi * phase) * depthSamples + smallJitter();
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        return off;
    }

private:
    void updateInc() { inc = (float) (rateHz / fs); }
    static float wrap01(float x) { x -= std::floor(x); return x; }

    float smallJitter()
    {
        // TPDF-like small noise (~1e-4 samples)
        const float a = uni(rng) - 0.5f;
        const float b = uni(rng) - 0.5f;
        return (a + b) * 1e-4f;
    }

    double fs = 48000.0;
    float rateHz = 0.3f;
    float depthSamples = 0.0f;
    float phase = 0.0f;
    float inc = 0.0f;
    std::mt19937 rng { 1337u };
    std::uniform_real_distribution<float> uni { 0.0f, 1.0f };
};

} // namespace hgr::dsp

