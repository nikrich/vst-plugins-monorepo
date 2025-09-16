#pragma once

#include "DelayLine.h"

namespace hgr::dsp {

// Unity-gain Schroeder allpass diffuser (2-multiply form)
class Allpass {
public:
    void prepare(double sampleRate, int maxDelaySamples)
    {
        dl.prepare(sampleRate, maxDelaySamples);
        setDelaySamples(maxDelaySamples / 2);
        setGain(0.7f);
    }

    void reset()
    {
        dl.reset();
    }

    void setDelaySamples(int d) { dl.setBaseDelaySamples(d); }
    void setGain(float gIn) { g = std::clamp(gIn, 0.0f, 0.999f); }

    inline float processSample(float x) noexcept
    {
        // Two-multiply allpass implementation:
        // y = -g*x + d[n];  d[n+1] = x + g*y
        const float delayed = dl.getDelayedSample((float) dl.getBaseDelaySamples());
        const float y = (-g * x) + delayed;
        dl.pushSample(x + (g * y));
        return y;
    }

private:
    DelayLine dl;
    float g = 0.7f;
};

} // namespace hgr::dsp

