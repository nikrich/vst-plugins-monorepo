#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace hgmbc {

// LR4 crossover built from cascaded 2nd-order Butterworth sections
class BandSplitterIIR
{
public:
    void prepare(double sr, int channels)
    {
        sampleRate = sr;
        numChannels = juce::jmax(1, channels);
        chans.resize((size_t) numChannels);
        setCrossoverHz(fcHz);
        reset();
    }

    void reset()
    {
        for (auto& ch : chans)
        {
            ch.lp1.reset(); ch.lp2.reset();
            ch.hp1.reset(); ch.hp2.reset();
        }
    }

    void setCrossoverHz(float fc)
    {
        fcHz = juce::jlimit(20.0f, (float) (0.45 * sampleRate), fc);
        auto lpCoefs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, fcHz);
        auto hpCoefs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, fcHz);
        for (auto& ch : chans)
        {
            ch.lp1.coefficients = lpCoefs; ch.lp2.coefficients = lpCoefs;
            ch.hp1.coefficients = hpCoefs; ch.hp2.coefficients = hpCoefs;
        }
    }

    // Process: src -> low & high (both must be allocated with same layout as src)
    void process(const juce::AudioBuffer<float>& src, juce::AudioBuffer<float>& low, juce::AudioBuffer<float>& high)
    {
        const int N = src.getNumSamples();
        const int C = juce::jmin(src.getNumChannels(), numChannels);
        low.makeCopyOf(src, true);
        high.makeCopyOf(src, true);
        for (int ch = 0; ch < C; ++ch)
        {
            const float* srcPtr = src.getReadPointer(ch);
            float* lptr = low.getWritePointer(ch);
            float* hptr = high.getWritePointer(ch);
            auto& c = chans[(size_t) ch];
            for (int n = 0; n < N; ++n)
            {
                // Low: LR4 via cascaded Butterworth LP2
                const float lp = c.lp2.processSample(c.lp1.processSample(srcPtr[n]));
                lptr[n] = lp;
                // High: complementary band (exact recombination)
                hptr[n] = srcPtr[n] - lp;
            }
        }
        // Clear extra channels if any
        for (int ch = C; ch < low.getNumChannels(); ++ch) low.clear(ch, 0, N);
        for (int ch = C; ch < high.getNumChannels(); ++ch) high.clear(ch, 0, N);
    }

    // N-band API: Configure multiple crossover frequencies for cascaded splitting
    void setCrossoverFrequencies(const std::vector<float>& freqs)
    {
        // Store up to 5 crossover frequencies (for up to 6 bands)
        numCrossovers = juce::jmin((int)freqs.size(), 5);
        for (int i = 0; i < numCrossovers; ++i)
            crossoverFreqs[i] = juce::jlimit(20.0f, (float)(0.45 * sampleRate), freqs[i]);

        // Update filter coefficients for each stage
        for (int stage = 0; stage < numCrossovers; ++stage)
        {
            auto lpCoefs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, crossoverFreqs[stage]);
            auto hpCoefs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, crossoverFreqs[stage]);
            for (auto& ch : chans)
            {
                ch.lpStages[stage][0].coefficients = lpCoefs;
                ch.lpStages[stage][1].coefficients = lpCoefs;
                ch.hpStages[stage][0].coefficients = hpCoefs;
                ch.hpStages[stage][1].coefficients = hpCoefs;
            }
        }
    }

    // N-band process: Split input into multiple bands using cascaded crossovers
    void process(const juce::AudioBuffer<float>& src, std::vector<juce::AudioBuffer<float>>& bands)
    {
        const int N = src.getNumSamples();
        const int C = juce::jmin(src.getNumChannels(), numChannels);
        const int numBands = numCrossovers + 1;

        // Ensure output vector has correct size
        if ((int)bands.size() != numBands)
            bands.resize(numBands);
        for (auto& band : bands)
            band.setSize(src.getNumChannels(), N, false, true);

        // Initialize all bands as copies of input
        for (auto& band : bands)
            band.makeCopyOf(src, true);

        // Process cascaded stages: each stage splits one band into two
        // Stage 0: splits bands[0] into low (stays in bands[0]) and high (goes to bands[1])
        // Stage 1: splits bands[1] into low (stays in bands[1]) and high (goes to bands[2])
        // etc.
        for (int stage = 0; stage < numCrossovers; ++stage)
        {
            juce::AudioBuffer<float> tempHigh(C, N);
            tempHigh.makeCopyOf(bands[stage], true);

            for (int ch = 0; ch < C; ++ch)
            {
                float* bandPtr = bands[stage].getWritePointer(ch);
                float* highPtr = tempHigh.getWritePointer(ch);
                auto& c = chans[(size_t)ch];

                for (int n = 0; n < N; ++n)
                {
                    // Low-pass for current band
                    const float lp = c.lpStages[stage][1].processSample(
                        c.lpStages[stage][0].processSample(bandPtr[n]));
                    bandPtr[n] = lp;
                    // High-pass (complementary)
                    highPtr[n] = highPtr[n] - lp;
                }
            }

            // Move high band to next output
            if (stage + 1 < (int)bands.size())
                bands[stage + 1].makeCopyOf(tempHigh, true);
        }
    }

    int getNumBands() const { return numCrossovers + 1; }

private:
    double sampleRate = 44100.0;
    int    numChannels = 2;
    float  fcHz = 120.0f;
    int    numCrossovers = 0;  // Number of crossover frequencies (0-5)
    float  crossoverFreqs[5] = {120.0f, 500.0f, 2000.0f, 8000.0f, 16000.0f};

    struct ChannelFilters
    {
        // 2-band (legacy)
        juce::dsp::IIR::Filter<float> lp1, lp2, hp1, hp2;
        // N-band cascaded stages (5 stages max for up to 6 bands)
        juce::dsp::IIR::Filter<float> lpStages[5][2];  // LP1, LP2 for each stage
        juce::dsp::IIR::Filter<float> hpStages[5][2];  // HP1, HP2 for each stage
    };
    std::vector<ChannelFilters> chans;
};

} // namespace hgmbc
