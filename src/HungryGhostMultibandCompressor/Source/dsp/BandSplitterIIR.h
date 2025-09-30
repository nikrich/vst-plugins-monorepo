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

private:
    double sampleRate = 44100.0;
    int    numChannels = 2;
    float  fcHz = 120.0f;

    struct ChannelFilters
    {
        juce::dsp::IIR::Filter<float> lp1, lp2, hp1, hp2;
    };
    std::vector<ChannelFilters> chans;
};

} // namespace hgmbc
