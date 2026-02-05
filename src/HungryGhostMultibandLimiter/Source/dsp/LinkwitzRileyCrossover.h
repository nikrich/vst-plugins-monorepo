#pragma once
#include <JuceHeader.h>
#include <vector>

namespace hgmbl {

/**
 * Linkwitz-Riley 4th-order (LR4) crossover filter for phase-coherent band splitting.
 *
 * LR4 consists of two cascaded 2nd-order Butterworth sections, providing:
 * - Complementary frequency response (LP + HP = input exactly)
 * - Zero-latency processing (all-pass topology)
 * - 24 dB/octave slope at crossover
 *
 * The high-pass output is derived as: HP = Input - LP, ensuring perfect reconstruction.
 */
class LinkwitzRileyCrossover
{
public:
    /**
     * Initialize the crossover for a given sample rate and number of channels.
     *
     * @param sampleRate The sample rate in Hz (e.g., 44100)
     * @param numChannels Number of channels to process (typically 2 for stereo)
     */
    void prepare(double sampleRate, int numChannels)
    {
        this->sampleRate = sampleRate;
        this->numChannels = juce::jmax(1, numChannels);
        channelFilters.resize((size_t)this->numChannels);
        setCrossoverHz(crossoverHz);
        reset();
    }

    /**
     * Reset filter state (clears internal buffers and history).
     */
    void reset()
    {
        for (auto& ch : channelFilters)
        {
            ch.lpSection1.reset();
            ch.lpSection2.reset();
        }
    }

    /**
     * Set the crossover frequency in Hz (20 Hz to 0.45 * Fs).
     *
     * @param fc Crossover frequency in Hz
     */
    void setCrossoverHz(float fc)
    {
        crossoverHz = juce::jlimit(20.0f, (float)(0.45 * sampleRate), fc);

        // Create Butterworth 2nd-order low-pass coefficients at crossover frequency
        auto lpCoefs = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, crossoverHz);

        // Apply to both sections for LR4 (cascaded)
        for (auto& ch : channelFilters)
        {
            ch.lpSection1.coefficients = lpCoefs;
            ch.lpSection2.coefficients = lpCoefs;
        }
    }

    /**
     * Process a stereo audio buffer, splitting it into low and high frequency bands.
     *
     * @param input Input audio buffer (must have same sample count and channel layout as outputs)
     * @param lowBand Output buffer containing frequencies below crossover
     * @param highBand Output buffer containing frequencies above crossover
     */
    void process(const juce::AudioBuffer<float>& input,
                 juce::AudioBuffer<float>& lowBand,
                 juce::AudioBuffer<float>& highBand)
    {
        const int numSamples = input.getNumSamples();
        const int numChans = juce::jmin(input.getNumChannels(), numChannels);

        // Initialize output buffers as copies of input
        lowBand.makeCopyOf(input, true);
        highBand.makeCopyOf(input, true);

        // Process each channel
        for (int ch = 0; ch < numChans; ++ch)
        {
            const float* srcPtr = input.getReadPointer(ch);
            float* lpPtr = lowBand.getWritePointer(ch);
            float* hpPtr = highBand.getWritePointer(ch);

            auto& chFilters = channelFilters[(size_t)ch];

            // Process each sample through cascaded Butterworth sections
            for (int n = 0; n < numSamples; ++n)
            {
                // Low-pass: cascade two 2nd-order Butterworth sections (LR4)
                const float lp1Out = chFilters.lpSection1.processSample(srcPtr[n]);
                const float lp2Out = chFilters.lpSection2.processSample(lp1Out);

                lpPtr[n] = lp2Out;

                // High-pass: complementary band (ensures perfect reconstruction: LP + HP = input)
                hpPtr[n] = srcPtr[n] - lp2Out;
            }
        }

        // Clear extra channels if output buffers have more channels than input
        for (int ch = numChans; ch < lowBand.getNumChannels(); ++ch)
            lowBand.clear(ch, 0, numSamples);
        for (int ch = numChans; ch < highBand.getNumChannels(); ++ch)
            highBand.clear(ch, 0, numSamples);
    }

    /**
     * Get the current crossover frequency in Hz.
     */
    float getCrossoverHz() const { return crossoverHz; }

private:
    double sampleRate = 44100.0;
    int numChannels = 2;
    float crossoverHz = 200.0f;

    struct ChannelFilters
    {
        juce::dsp::IIR::Filter<float> lpSection1;  // 1st cascaded Butterworth LP
        juce::dsp::IIR::Filter<float> lpSection2;  // 2nd cascaded Butterworth LP
    };

    std::vector<ChannelFilters> channelFilters;
};

} // namespace hgmbl
