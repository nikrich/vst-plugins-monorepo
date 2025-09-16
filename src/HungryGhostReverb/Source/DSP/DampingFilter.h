#pragma once

#include <algorithm>
#include <cmath>

namespace hgr::dsp {

// One-pole LPF (TPT form). Optionally usable as HP by subtraction.
class OnePoleLP {
public:
    void prepare(double sampleRate) { fs = sampleRate; setCutoffHz(cutoffHz); reset(); }
    void reset() { z = 0.0f; }

    void setCutoffHz(float hz)
    {
        // Clamp cutoff to a safe, finite range and guard the tan() mapping
        cutoffHz = std::clamp(hz, 20.0f, 0.49f * (float) fs);
        constexpr float pi = 3.14159265358979323846f;
        const float x  = pi * (cutoffHz / (float) fs);
        const float wc = std::tan(x);
        if (!std::isfinite(wc)) { a = 1.0f; return; } // transparent if bad
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

