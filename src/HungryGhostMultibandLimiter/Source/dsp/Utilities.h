#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace hgml {

inline float dbToLin(float dB) noexcept { return juce::Decibels::decibelsToGain(dB); }
inline float linToDb(float g)  noexcept { return juce::Decibels::gainToDecibels(juce::jmax(g, 1.0e-12f)); }

inline float coefFromMs(float ms, double sr) noexcept
{
    const float sec = juce::jmax(1.0e-6f, (float)ms * 0.001f);
    return std::exp(-1.0f / (sec * (float)sr));
}

// Simple look-ahead delay buffer per channel
struct LookaheadDelay
{
    void reset(int capacitySamples)
    {
        buf.assign((size_t)juce::jmax(capacitySamples, 1), 0.0f);
        w = 0;
    }
    inline float process(float x, int delaySamples) noexcept
    {
        const int cap = (int)buf.size();
        int r = w - delaySamples;
        if (r < 0) r += cap;
        const float y = buf[(size_t)r];
        buf[(size_t)w] = x;
        if (++w == cap) w = 0;
        return y;
    }
    std::vector<float> buf; int w = 0;
};

} // namespace hgml
