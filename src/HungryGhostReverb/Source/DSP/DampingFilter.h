#pragma once

#include <cmath>

namespace hgr::dsp {

// One-pole LPF (TPT form). Optionally usable as HP by subtraction.
class OnePoleLP {
public:
    void prepare(double sampleRate) { fs = sampleRate; setCutoffHz(cutoffHz); reset(); }
    void reset() { z = 0.0f; }

    void setCutoffHz(float hz)
    {
        cutoffHz = hz;
        constexpr float pi = 3.14159265358979323846f;
        const float wc = std::tan(pi * (cutoffHz / (float) fs));
        // TPT integrator coefficient
        a = wc / (1.0f + wc);
    }

    inline float processSample(float x) noexcept
    {
        // TPT 1-pole lowpass
        const float v = (x - z) * a;
        const float y = v + z;
        z = y + v;
        return y;
    }

private:
    double fs = 48000.0;
    float cutoffHz = 6000.0f;
    float a = 0.5f;
    float z = 0.0f;
};

} // namespace hgr::dsp

