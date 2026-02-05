#pragma once
#include <juce_dsp/juce_dsp.h>
#include "Utilities.h"

namespace hgmbc {

struct CompressorBandParams
{
    float threshold_dB = -18.0f;
    float ratio = 2.0f;
    float knee_dB = 6.0f;
    float attack_ms = 10.0f;
    float release_ms = 120.0f;
    float mix_pct = 100.0f; // 0..100
    int   detectorType = 1; // 0: Peak, 1: RMS
};

class CompressorBand
{
public:
    void prepare(double sr, int channels, int maxLookaheadSamples)
    {
        sampleRate = sr;
        numChannels = juce::jmax(1, channels);
        maxLA = juce::jmax(1, maxLookaheadSamples);
        delays.resize((size_t) numChannels);
        for (auto& d : delays) d.reset(maxLA + 32);
        env.assign((size_t) numChannels, 0.0f);
        env2.assign((size_t) numChannels, 0.0f);
        setLookaheadSamples(lookaheadSamples);
        updateTimeConstants();
    }

    void setParams(const CompressorBandParams& p)
    {
        params = p;
        updateTimeConstants();
    }

    void setLookaheadSamples(int la)
    {
        lookaheadSamples = juce::jlimit(0, maxLA - 1, la);
    }

    void setEffectiveSampleRate(double sr)
    {
        sampleRate = sr;
        updateTimeConstants();
    }

    void reset()
    {
        std::fill(env.begin(), env.end(), 0.0f);
        std::fill(env2.begin(), env2.end(), 0.0f);
    }

    // In-place process on band; dryIn required to compute delta if needed by caller
    void process(juce::AudioBuffer<float>& band)
    {
        const int N = band.getNumSamples();
        const int C = juce::jmin(band.getNumChannels(), numChannels);
        const float mix = juce::jlimit(0.0f, 100.0f, params.mix_pct) * 0.01f;

        for (int n = 0; n < N; ++n)
        {
            // Detector (stereo link via max)
            float det = 0.0f;
            for (int ch = 0; ch < C; ++ch)
            {
                float x = band.getReadPointer(ch)[n];
                if (params.detectorType == 0) // Peak
                {
                    float a = std::abs(x);
                    env[(size_t)ch] = (a > env[(size_t)ch])
                        ? a + atkAlpha * (env[(size_t)ch] - a)
                        : a + relAlpha * (env[(size_t)ch] - a);
                    det = juce::jmax(det, env[(size_t)ch]);
                }
                else // RMS (one-pole on x^2)
                {
                    float a2 = x * x;
                    env2[(size_t)ch] = a2 + rmsAlpha * (env2[(size_t)ch] - a2);
                    det = juce::jmax(det, std::sqrt(env2[(size_t)ch] + 1.0e-12f));
                }
            }

            const float inDb = linToDb(det);
            const float gDb = softKneeGainDb(inDb, params.threshold_dB, params.ratio, params.knee_dB); // <= 0
            // Instant attack, one-pole release on gain (in dB)
            if (gDb < currentGainDb)
                currentGainDb = gDb;
            else
                currentGainDb = currentGainDb * relAlpha + gDb * (1.0f - relAlpha);

            const float gLin = dbToLin(currentGainDb);

            // Apply to delayed main path per channel and mix
            for (int ch = 0; ch < C; ++ch)
            {
                float x = band.getReadPointer(ch)[n];
                float yd = delays[(size_t)ch].process(x, lookaheadSamples) * gLin; // delayed and gained
                float y = mix * yd + (1.0f - mix) * x; // parallel mix
                band.getWritePointer(ch)[n] = y;
            }
            for (int ch = C; ch < band.getNumChannels(); ++ch)
                band.getWritePointer(ch)[n] = 0.0f;
        }
    }

private:
    void updateTimeConstants()
    {
        atkAlpha = coefFromMs(params.attack_ms, sampleRate);
        relAlpha = coefFromMs(params.release_ms, sampleRate);
        // RMS window approx via one-pole coef (using release as integration time)
        rmsAlpha = coefFromMs(params.release_ms, sampleRate);
    }

    double sampleRate = 44100.0;
    int    numChannels = 2;
    int    maxLA = 1;
    int    lookaheadSamples = 0;

    CompressorBandParams params{};

    std::vector<hgmbc::LookaheadDelay> delays;
    std::vector<float> env;   // peak env
    std::vector<float> env2;  // RMS env^2

    float atkAlpha = 0.0f;
    float relAlpha = 0.0f;
    float rmsAlpha = 0.0f;

    float currentGainDb = 0.0f; // <= 0
public:
    float getCurrentGainDb() const noexcept { return currentGainDb; }
};

} // namespace hgmbc
