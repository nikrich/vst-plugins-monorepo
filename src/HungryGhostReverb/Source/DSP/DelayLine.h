#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace hgr::dsp {

// Simple fractional delay line with linear interpolation.
// Preallocate in prepare(). No allocations in process.
class DelayLine {
public:
    void prepare(double sampleRate, int maxDelaySamples)
    {
        fs = sampleRate;
        const int pow2 = nextPow2(std::max(2, maxDelaySamples + 4));
        buffer.assign((size_t) pow2, 0.0f);
        mask = pow2 - 1;
        writeIdx = 0;
        baseDelaySamples = std::min(maxDelaySamples, pow2 - 4);
        fracDelay = 0.0f;
    }

    void reset()
    {
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIdx = 0;
    }

    inline void pushSample(float x) noexcept
    {
        buffer[(size_t) writeIdx] = x;
        writeIdx = (writeIdx + 1) & mask;
    }

    inline float readFractional(float totalDelaySamples, float lfoOffsetSamples = 0.0f) const noexcept
    {
        // Clamp total delay to a safe range to avoid reading past stale memory
        const float cap = (float) capacity();
        const float dRaw = totalDelaySamples + lfoOffsetSamples;
        const float d = std::clamp(dRaw, 1.0f, cap - 2.0f);
        
        const float rIdx = (float) writeIdx - d;
        const float kf = std::floor(rIdx);
        const int k = (int) kf;
        
        const int i0 = k & mask;
        const int i1 = (i0 + 1) & mask;
        const float frac = rIdx - kf;
        const float s0 = buffer[(size_t) i0];
        const float s1 = buffer[(size_t) i1];
        return s0 + (s1 - s0) * frac; // linear interp
    }

    inline float getDelayedSample(float totalDelaySamples) const noexcept { return readFractional(totalDelaySamples, 0.0f); }

    // Utility for single-sample process flow: returns y (delayed), then writes x
    inline float processSample(float x, float totalDelaySamples, float lfoOffsetSamples = 0.0f) noexcept
    {
        const float y = readFractional(totalDelaySamples, lfoOffsetSamples);
        pushSample(x);
        return y;
    }

    void setBaseDelaySamples(int d)
    {
        baseDelaySamples = std::clamp(d, 1, mask - 3);
    }

    int getBaseDelaySamples() const noexcept { return baseDelaySamples; }

    int capacity() const noexcept { return mask + 1; }

    static int nextPow2(int v) noexcept
    {
        v--; v |= v >> 1; v |= v >> 2; v |= v >> 4; v |= v >> 8; v |= v >> 16; v++;
        return v > 0 ? v : 1;
    }

private:
    double fs = 48000.0;
    std::vector<float> buffer;
    int mask = 0;
    int writeIdx = 0;
    int baseDelaySamples = 1; // integer portion baseline
    float fracDelay = 0.0f;   // reserved for alt interpolation
};

} // namespace hgr::dsp

