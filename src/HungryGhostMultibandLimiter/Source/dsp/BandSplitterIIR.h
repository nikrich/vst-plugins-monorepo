#pragma once
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <algorithm>

namespace hgml {

// LR4 multi-band crossover: splits input into N+1 bands using N crossover frequencies
class BandSplitterIIR
{
public:
    void prepare(double sr, int channels)
    {
        sampleRate = sr;
        numChannels = juce::jmax(1, channels);
        chans.resize((size_t) numChannels);
        setCrossoverHz(fcHz);  // Initialize with legacy single crossover
        reset();
    }

    void reset()
    {
        for (auto& ch : chans)
        {
            for (auto& f : ch.lp) { f[0].reset(); f[1].reset(); }
            for (auto& f : ch.hp) { f[0].reset(); f[1].reset(); }
        }
    }

    // Set N crossover frequencies (up to 5 for 6 bands)
    void setCrossoverFrequencies(const std::vector<float>& freqs)
    {
        crossoverFreqs = freqs;
        if (crossoverFreqs.size() > 5)
            crossoverFreqs.resize(5);

        // Sort and validate frequencies
        for (auto& f : crossoverFreqs)
            f = juce::jlimit(20.0f, (float)(0.45 * sampleRate), f);
        std::sort(crossoverFreqs.begin(), crossoverFreqs.end());

        // Update filter coefficients
        for (size_t i = 0; i < crossoverFreqs.size(); ++i)
        {
            auto lpCoefs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, crossoverFreqs[i]);
            auto hpCoefs = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, crossoverFreqs[i]);

            for (auto& ch : chans)
            {
                ch.lp[i][0].coefficients = lpCoefs;
                ch.lp[i][1].coefficients = lpCoefs;
                ch.hp[i][0].coefficients = hpCoefs;
                ch.hp[i][1].coefficients = hpCoefs;
            }
        }
    }

    // Legacy single-crossover API
    void setCrossoverHz(float fc)
    {
        fcHz = juce::jlimit(20.0f, (float)(0.45 * sampleRate), fc);
        std::vector<float> freqs;
        if (fc > 0) freqs.push_back(fc);
        setCrossoverFrequencies(freqs);
    }

    // Multi-band process: src -> vector of N+1 bands
    void process(const juce::AudioBuffer<float>& src, std::vector<juce::AudioBuffer<float>>& bands)
    {
        const int N = src.getNumSamples();
        const int C = juce::jmin(src.getNumChannels(), numChannels);
        const int numBands = (int)crossoverFreqs.size() + 1;

        if ((int)bands.size() != numBands)
            bands.resize((size_t) numBands);

        for (int b = 0; b < numBands; ++b)
        {
            bands[b].setSize(src.getNumChannels(), N, false, true, true);
            bands[b].makeCopyOf(src, true);
        }

        for (int ch = 0; ch < C; ++ch)
        {
            auto& c = chans[(size_t) ch];
            for (int n = 0; n < N; ++n)
            {
                const float x = src.getSample(ch, n);
                if (crossoverFreqs.empty())
                {
                    bands[0].setSample(ch, n, x);
                }
                else
                {
                    float lowPass = x;
                    for (size_t xo = 0; xo < crossoverFreqs.size(); ++xo)
                    {
                        const float lpOut = c.lp[xo][1].processSample(c.lp[xo][0].processSample(lowPass));
                        const float hpOut = c.hp[xo][1].processSample(c.hp[xo][0].processSample(lowPass));
                        bands[xo].setSample(ch, n, hpOut);
                        lowPass = lpOut;
                    }
                    bands[crossoverFreqs.size()].setSample(ch, n, lowPass);
                }
            }
        }

        for (int b = 0; b < numBands; ++b)
            for (int ch = C; ch < bands[b].getNumChannels(); ++ch)
                bands[b].clear(ch, 0, N);
    }

    // Legacy 2-band API
    void process(const juce::AudioBuffer<float>& src, juce::AudioBuffer<float>& low, juce::AudioBuffer<float>& high)
    {
        std::vector<juce::AudioBuffer<float>> bands;
        process(src, bands);
        if (bands.size() >= 1) low.makeCopyOf(bands[0], true);
        if (bands.size() >= 2) high.makeCopyOf(bands[1], true);
    }

    int getNumBands() const { return (int)crossoverFreqs.size() + 1; }
    float getCrossoverHz() const { return fcHz; }

private:
    double sampleRate = 44100.0;
    int numChannels = 2;
    float fcHz = 120.0f;
    std::vector<float> crossoverFreqs;

    static constexpr int kMaxCrossovers = 5;

    struct ChannelFilters
    {
        std::array<std::array<juce::dsp::IIR::Filter<float>, 2>, kMaxCrossovers> lp, hp;
    };
    std::vector<ChannelFilters> chans;
};

} // namespace hgml
