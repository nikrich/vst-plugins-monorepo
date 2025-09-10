#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

namespace hgl {

struct LimiterParams
{
    float preGainL = 1.0f;      // linear
    float preGainR = 1.0f;      // linear
    float ceilLin  = 1.0f;      // linear ceiling
    float releaseAlphaOS = 0.0f; // one-pole release coefficient at OS rate
    int   lookAheadSamplesOS = 1;  // samples, OS rate
    bool  scHpfOn = true;       // sidechain HPF enable
    bool  safetyOn = false;     // safety clip enable
    bool  autoReleaseOn = false; // program-dependent release when true
};

class LimiterDSP
{
public:
    void prepare(float osSampleRateIn, int maxLookAheadSamplesOS)
    {
        osSampleRate = osSampleRateIn;
        // Pre-allocate look-ahead delay and sliding max
        delayL.reset(maxLookAheadSamplesOS + 64);
        delayR.reset(maxLookAheadSamplesOS + 64);
        slidingMax.reset(maxLookAheadSamplesOS + 64);
        updateSidechainFilter();
        currentGainDb = 0.0f;
        grEnvFastDb = 0.0f;
        grEnvSlowDb = 0.0f;
    }

    void reset()
    {
        currentGainDb = 0.0f;
    }

    void setParams(const LimiterParams& p) { params = p; }

    // Process OS-rate interleaved arrays by channel pointers. Returns peak attenuation dB (positive, 0..)
    float processBlockOS(float* upL, float* upR, int N)
    {
        jassert(upL != nullptr && upR != nullptr);
        float meterMaxAttenDb = 0.0f;

        for (int i = 0; i < N; ++i)
        {
            // 1) pre-gain at OS rate
            float xl = upL[i] * params.preGainL;
            float xr = upR[i] * params.preGainR;

            // 2) sidechain detection (optional HPF)
            float dl = params.scHpfOn ? scHPF_L.processSample(xl) : xl;
            float dr = params.scHpfOn ? scHPF_R.processSample(xr) : xr;

            const float a = juce::jmax(std::abs(dl), std::abs(dr)); // stereo max-link
            slidingMax.push(a, params.lookAheadSamplesOS);
            const float aMax = slidingMax.getMax();

            // 3) required gain to hit ceiling (true-peak @ OS): <= 1
            float gReq = 1.0f;
            if (aMax > params.ceilLin)
                gReq = params.ceilLin / (aMax + 1.0e-12f);

            const float gReqDb = linToDb(gReq); // <= 0 dB (negative)

            // 4) Attack + Release
            if (!params.autoReleaseOn)
            {
                // legacy/manual: instant attack, one-pole release in dB-domain
                if (gReqDb < currentGainDb)               // need more attenuation (more negative dB)
                    currentGainDb = gReqDb;               // jump instantly
                else                                      // release toward target (usually 0 dB)
                    currentGainDb = currentGainDb * params.releaseAlphaOS + gReqDb * (1.0f - params.releaseAlphaOS);
            }
            else
            {
                // program-dependent auto release using two envelopes in positive-dB domain
                const float targetAttenDb = -gReqDb;                // positive dB of desired reduction
                float currentAttenDb = -currentGainDb;              // positive dB currently applied

                if (targetAttenDb > currentAttenDb + 1.0e-6f)
                {
                    // instant attack: jump to target and reset envelopes
                    currentGainDb = gReqDb;
                    grEnvFastDb = targetAttenDb;
                    grEnvSlowDb = targetAttenDb;
                }
                else
                {
                    // releasing: converge toward target using fast/slow blend
                    const float kSlow = juce::jlimit(0.0f, 1.0f, params.releaseAlphaOS);
                    const float kFast = std::exp(-1.0f / juce::jmax(1.0f, osSampleRate * 0.02f)); // ~20 ms

                    grEnvFastDb = juce::jmax(targetAttenDb, kFast * grEnvFastDb + (1.0f - kFast) * targetAttenDb);
                    grEnvSlowDb = juce::jmax(targetAttenDb, kSlow * grEnvSlowDb + (1.0f - kSlow) * targetAttenDb);

                    float alpha = juce::jlimit(0.0f, 1.0f, grEnvSlowDb / 12.0f);
                    alpha = alpha * alpha * (3.0f - 2.0f * alpha); // smoothstep

                    const float grSmoothDb = alpha * grEnvFastDb + (1.0f - alpha) * grEnvSlowDb;
                    currentGainDb = -grSmoothDb; // back to negative dB domain
                }
            }

            const float gLin = dbToLin(currentGainDb);

            // 5) delay main (look-ahead), then apply same gain to both channels
            float yl = delayL.processSample(xl, params.lookAheadSamplesOS) * gLin;
            float yr = delayR.processSample(xr, params.lookAheadSamplesOS) * gLin;

            // 6) optional ultra-gentle soft clip as a safety (still at OS rate)
            if (params.safetyOn)
            {
                constexpr float safetyBelowCeilDb = -0.1f; // 0.1 dB beneath ceiling
                const float safetyLimit = params.ceilLin * dbToLin(safetyBelowCeilDb);

                if (std::abs(yl) > safetyLimit) yl = softClipTanhTo(yl, safetyLimit);
                if (std::abs(yr) > safetyLimit) yr = softClipTanhTo(yr, safetyLimit);
            }

            // write back to OS buffer
            upL[i] = yl;
            upR[i] = yr;

            const float attenDb = -currentGainDb; // positive dB of reduction
            if (attenDb > meterMaxAttenDb) meterMaxAttenDb = attenDb;
        }

        return meterMaxAttenDb;
    }

private:
    static inline float dbToLin(float dB) noexcept { return juce::Decibels::decibelsToGain(dB); }
    static inline float linToDb(float g)  noexcept { return juce::Decibels::gainToDecibels(juce::jmax(g, 1.0e-12f)); }

    static inline float softClipTanhTo(float x, float limit, float k = 2.0f) noexcept
    {
        const float xn = x / juce::jmax(limit, 1.0e-9f);
        const float yn = std::tanh(k * xn) / std::tanh(k);
        return yn * limit;
    }

    void updateSidechainFilter()
    {
        scHPFCoefs = juce::dsp::IIR::Coefficients<float>::makeHighPass(osSampleRate, 30.0);
        scHPF_L.coefficients = scHPFCoefs;
        scHPF_R.coefficients = scHPFCoefs;
    }

    struct LookaheadDelay
    {
        void reset(int capacitySamples)
        {
            buf.assign((size_t)juce::jmax(capacitySamples, 1), 0.0f);
            w = 0;
        }
        inline float processSample(float x, int delaySamples) noexcept
        {
            const int cap = (int)buf.size();
            int r = w - delaySamples;
            if (r < 0) r += cap;
            const float y = buf[(size_t)r];
            buf[(size_t)w] = x;
            if (++w == cap) w = 0;
            return y;
        }
        inline int capacity() const noexcept { return (int)buf.size(); }
        std::vector<float> buf; int w = 0;
    };

    struct SlidingMax
    {
        void reset(int capacitySamples)
        {
            vals.assign((size_t)juce::jmax(capacitySamples + 8, 32), 0.0f);
            idxs.assign(vals.size(), 0);
            head = tail = 0;
            currentIdx = 0;
            capacity = (int)vals.size();
        }
        inline void push(float v, int windowSamples) noexcept
        {
            while (head != tail)
            {
                const int last = (tail + capacity - 1) % capacity;
                if (v <= vals[(size_t)last]) break;
                tail = last;
            }
            vals[(size_t)tail] = v;
            idxs[(size_t)tail] = currentIdx;
            tail = (tail + 1) % capacity;
            const int lowerBound = currentIdx - windowSamples;
            while (head != tail && idxs[(size_t)head] <= lowerBound)
                head = (head + 1) % capacity;
            ++currentIdx;
        }
        inline float getMax() const noexcept
        {
            return (head != tail) ? vals[(size_t)head] : 0.0f;
        }
        std::vector<float> vals; std::vector<int> idxs;
        int head = 0, tail = 0, capacity = 0, currentIdx = 0;
    };

    float osSampleRate = 44100.0f;
    LimiterParams params{};
    float currentGainDb = 0.0f;
    float grEnvFastDb = 0.0f; // auto-release fast envelope (positive dB)
    float grEnvSlowDb = 0.0f; // auto-release slow envelope (positive dB)
    LookaheadDelay delayL, delayR;
    SlidingMax slidingMax;
    juce::dsp::IIR::Filter<float> scHPF_L, scHPF_R;
    juce::dsp::IIR::Coefficients<float>::Ptr scHPFCoefs;
};

} // namespace hgl
