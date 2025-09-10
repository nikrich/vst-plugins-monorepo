/*
  ==============================================================================
    HungryGhostLimiter � true-peak look-ahead limiter (JUCE)
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>
#include <atomic>
#include <limits>
#include "dsp/LimiterDSP.h"

//==============================================================================

class HungryGhostLimiterAudioProcessor : public juce::AudioProcessor
{
public:
    HungryGhostLimiterAudioProcessor();
    ~HungryGhostLimiterAudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlockExpected) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return "HungryGhostLimiter"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    //==============================================================================
    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS apvts{ *this, nullptr, "params", createParameterLayout() };
    static APVTS::ParameterLayout createParameterLayout();

// exposed to editor (gain-reduction, dB, >= 0). UI applies smoothing.
float getSmoothedAttenDb() const { return attenDbRaw.load(); }

private:
    // ========= Parameters we read every block =========
    float sampleRateHz = 44100.0f;
    int   osFactor = 1;        // 1, 4, or 8 (set in prepare based on sample rate)
    float osSampleRate = 44100.0f; // sampleRateHz * osFactor

// --- metering (host-rate): raw dB reduction (smoothed in UI component) ---
std::atomic<float> attenDbRaw { 0.0f }; // 0..24 dB

    // ========= Oversampling =========
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler; // built in prepare
    int oversamplingLatencyNative = 0; // in native samples

    // ========= DSP Core =========
    // Extracted into hgl::LimiterDSP; processor manages oversampling, state, and params.
    hgl::LimiterDSP limiter;

    // ========= Sliding-window maximum (monotonic queue, allocation-free in audio) =========
    struct SlidingMax
    {
        void reset(int capacitySamples)
        {
            // capacity is the maximum look-ahead window we�ll ever use
            vals.assign((size_t)juce::jmax(capacitySamples + 8, 32), 0.0f);
            idxs.assign(vals.size(), 0);
            head = tail = 0;
            currentIdx = 0;
            capacity = (int)vals.size();
        }

        inline void push(float v, int windowSamples) noexcept
        {
            // pop smaller items from the tail
            while (head != tail)
            {
                const int last = (tail + capacity - 1) % capacity;
                if (v <= vals[(size_t)last]) break;
                tail = last; // drop the last item
            }

            vals[(size_t)tail] = v;
            idxs[(size_t)tail] = currentIdx;
            tail = (tail + 1) % capacity;

            // purge items that are out of the window from the head
            const int lowerBound = currentIdx - windowSamples;
            while (head != tail && idxs[(size_t)head] <= lowerBound)
                head = (head + 1) % capacity;

            ++currentIdx;
        }

        inline float getMax() const noexcept
        {
            return (head != tail) ? vals[(size_t)head] : 0.0f;
        }

        std::vector<float> vals;
        std::vector<int>   idxs;
        int head = 0, tail = 0, capacity = 0;
        int currentIdx = 0;
    };

    SlidingMax slidingMax; // (legacy leftover, will be unused after migration)

    // ========= Gain computer state =========
    float currentGainDb = 0.0f; // <= 0 dB; negative means attenuation

    // ========= Helpers =========
    void buildOversampler(double sr, int samplesPerBlockExpected);
void updateLatencyReport(float lookMs); // calls setLatencySamples()

    // --- cached derived values (updated per block) ---
int   lookAheadSamplesOS = 0;      // at OS rate
    float releaseAlphaOS = 0.0f;   // one-pole release coefficient at OS rate
    float lastReportedLookMs = std::numeric_limits<float>::quiet_NaN();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostLimiterAudioProcessor)
};
