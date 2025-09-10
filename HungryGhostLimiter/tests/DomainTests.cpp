#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include "../Source/PluginProcessor.h"

struct DomainLatencyTest : juce::UnitTest {
    DomainLatencyTest() : juce::UnitTest("Processor: domain latency behavior") {}

    void runTest() override {
        beginTest("Digital domain reports lower latency than TruePeak for same look-ahead");
        HungryGhostLimiterAudioProcessor proc;
        const double sr = 48000.0;
        const int block = 512;
        proc.prepareToPlay(sr, block);

        auto* look = proc.apvts.getRawParameterValue("lookAheadMs");
        jassert(look != nullptr);
        *const_cast<std::atomic<float>*>(look) = 2.0f; // 2 ms

        auto setParam = [&](const char* id, float v){ if (auto* p = proc.apvts.getRawParameterValue(id)) *const_cast<std::atomic<float>*>(p) = v; };

        // TruePeak (default)
        setParam("domTruePeak", 1.0f);
        setParam("domDigital", 0.0f);
        setParam("domAnalog",  0.0f);
        {
            juce::AudioBuffer<float> buf(2, block);
            buf.clear();
            juce::MidiBuffer midi; proc.processBlock(buf, midi);
        }
        const int latTP = proc.getLatencySamples();

        // Digital
        setParam("domTruePeak", 0.0f);
        setParam("domDigital", 1.0f);
        setParam("domAnalog",  0.0f);
        {
            juce::AudioBuffer<float> buf(2, block);
            buf.clear();
            juce::MidiBuffer midi; proc.processBlock(buf, midi);
        }
        const int latDig = proc.getLatencySamples();

        expect(latDig <= latTP, "Digital latency should be <= TruePeak latency");
        expect(latDig >= 0, "Latency should be non-negative");
    }
};

static DomainLatencyTest domainLatencyTest;

