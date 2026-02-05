#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include "Utilities.h"

namespace hgml {

// Per-band limiter parameters
struct LimiterBandParams
{
    float thresholdDb = -6.0f;    // Limiting threshold in dB
    float attackMs = 2.0f;         // Attack time in milliseconds
    float releaseMs = 100.0f;      // Release time in milliseconds
    float mixPct = 100.0f;         // Dry/wet mix (0-100%)
    bool bypass = false;           // Bypass this limiter
};

// Single-band limiting processor for use in multiband limiter
class LimiterBand
{
public:
    void prepare(float sampleRate)
    {
        sr = sampleRate;
        reset();
    }

    void reset()
    {
        currentGainDb = 0.0f;
        envelopeDb = 0.0f;
    }

    void setParams(const LimiterBandParams& p)
    {
        params = p;
        // Pre-calculate time constants
        attackCoef = hgml::coefFromMs(params.attackMs, sr);
        releaseCoef = hgml::coefFromMs(params.releaseMs, sr);
    }

    // Process a single band buffer
    // Returns peak gain reduction in dB (positive value, 0..60)
    float processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (params.bypass)
            return 0.0f;

        const float thresholdLin = hgml::dbToLin(params.thresholdDb);
        const float mixLinear = params.mixPct * 0.01f;  // 0-1
        float maxGrDb = 0.0f;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            const int numSamples = buffer.getNumSamples();

            for (int n = 0; n < numSamples; ++n)
            {
                const float x = data[n];
                const float xAbs = std::abs(x);

                // Calculate required gain to stay below threshold
                float gainRequired = 1.0f;
                if (xAbs > thresholdLin)
                {
                    gainRequired = thresholdLin / (xAbs + 1.0e-12f);
                }

                const float grDb = hgml::linToDb(gainRequired);  // <= 0 dB

                // Envelope follower with attack/release
                if (grDb < envelopeDb)
                {
                    // Attack: move toward required gain
                    envelopeDb = attackCoef * envelopeDb + (1.0f - attackCoef) * grDb;
                }
                else
                {
                    // Release: return to 0 dB
                    envelopeDb = releaseCoef * envelopeDb + (1.0f - releaseCoef) * grDb;
                }

                // Apply limiting with mix
                const float gainLin = hgml::dbToLin(envelopeDb);
                const float dryGain = 1.0f - mixLinear;
                const float wetGain = gainLin * mixLinear;
                data[n] = x * (dryGain + wetGain);

                // Track peak GR for metering
                const float currentGr = -envelopeDb;
                if (currentGr > maxGrDb)
                    maxGrDb = currentGr;
            }
        }

        currentGainDb = envelopeDb;
        return maxGrDb;
    }

    float getCurrentGainDb() const { return currentGainDb; }
    float getEnvelopeDb() const { return envelopeDb; }

private:
    float sr = 44100.0f;
    LimiterBandParams params;
    float currentGainDb = 0.0f;      // Current gain in dB (negative for limiting)
    float envelopeDb = 0.0f;         // Attack/release envelope in dB
    float attackCoef = 0.1f;         // Attack coefficient
    float releaseCoef = 0.95f;       // Release coefficient
};

} // namespace hgml
