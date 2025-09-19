#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace hgr::dsp {

// Simple fractional delay line with selectable interpolation.
// Preallocate in prepare(). No allocations in process.
class DelayLine {
public:
    enum class InterpMode { Linear, Lagrange3 };

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
        const float d = std::clamp(dRaw, 1.0f, cap - 3.0f);
        
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

    inline float readLagrange3(float totalDelaySamples, float lfoOffsetSamples = 0.0f) const noexcept
    {
        // 3rd-order Lagrange interpolation using 4 taps around the read index
        const float cap = (float) capacity();
        const float dRaw = totalDelaySamples + lfoOffsetSamples;
        const float d = std::clamp(dRaw, 2.0f, cap - 3.0f);

        const float rIdx = (float) writeIdx - d;
        const float kf = std::floor(rIdx);
        const int k = (int) kf;
        const float a = rIdx - kf; // fractional part in [0,1)

        const int im1 = (k - 1) & mask; // x[-1]
        const int i0  = k & mask;       // x[0]
        const int i1  = (i0 + 1) & mask;// x[1]
        const int i2  = (i0 + 2) & mask;// x[2]

        const float xm1 = buffer[(size_t) im1];
        const float x0  = buffer[(size_t) i0];
        const float x1  = buffer[(size_t) i1];
        const float x2  = buffer[(size_t) i2];

        const float c0 = -a * (a - 1.0f) * (a - 2.0f) / 6.0f;
        const float c1 =  (a + 1.0f) * (a - 1.0f) * (a - 2.0f) / 2.0f;
        const float c2 = -(a + 1.0f) * a * (a - 2.0f) / 2.0f;
        const float c3 =  (a + 1.0f) * a * (a - 1.0f) / 6.0f;

        return c0 * xm1 + c1 * x0 + c2 * x1 + c3 * x2;
    }

    inline float readInterpolated(float totalDelaySamples, float lfoOffsetSamples, InterpMode mode) const noexcept
    {
        return (mode == InterpMode::Lagrange3)
            ? readLagrange3(totalDelaySamples, lfoOffsetSamples)
            : readFractional(totalDelaySamples, lfoOffsetSamples);
    }

    inline float getDelayedSample(float totalDelaySamples) const noexcept { return readFractional(totalDelaySamples, 0.0f); }

    // Utility for single-sample process flow: returns y (delayed), then writes x
    inline float processSample(float x, float totalDelaySamples, float lfoOffsetSamples = 0.0f, InterpMode mode = InterpMode::Linear) noexcept
    {
        const float y = readInterpolated(totalDelaySamples, lfoOffsetSamples, mode);
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

