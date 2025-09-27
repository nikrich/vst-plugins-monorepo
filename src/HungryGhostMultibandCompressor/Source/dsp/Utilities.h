#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

namespace hgmbc {

inline float dbToLin(float dB) noexcept { return juce::Decibels::decibelsToGain(dB); }
inline float linToDb(float g)  noexcept { return juce::Decibels::gainToDecibels(juce::jmax(g, 1.0e-12f)); }

inline float coefFromMs(float ms, double sr) noexcept
{
    const float sec = juce::jmax(1.0e-6f, (float)ms * 0.001f);
    return std::exp(-1.0f / (sec * (float)sr));
}

inline float softKneeGainDb(float inLevelDb, float T, float R, float kneeDb)
{
    // Compute static gain in dB for downward compression with soft knee.
    // Return negative or zero dB (attenuation or unity).
    const float half = 0.5f * juce::jmax(0.0f, kneeDb);
    const float x = inLevelDb;

    if (kneeDb <= 1.0e-6f)
    {
        // Hard knee
        if (x <= T) return 0.0f;
        const float y = T + (x - T) / juce::jmax(1.0f, R);
        return y - x; // negative
    }

    if (x <= T - half) return 0.0f; // below knee: unity
    if (x >= T + half)
    {
        const float y = T + (x - T) / juce::jmax(1.0f, R);
        return y - x;
    }

    // Quadratic soft knee interpolation across [T - half, T + half]
    const float a = x - (T - half);
    const float b = 2.0f * half;
    const float t = juce::jlimit(0.0f, 1.0f, a / b);

    const float y1 = x; // unity at lower edge
    const float y2 = T + (x - T) / juce::jmax(1.0f, R); // compressed at upper edge
    const float y = y1 + (y2 - y1) * t * t * (3.0f - 2.0f * t); // smoothstep
    return y - x;
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

} // namespace hgmbc
