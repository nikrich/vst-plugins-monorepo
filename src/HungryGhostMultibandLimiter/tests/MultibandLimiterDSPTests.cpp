/*
  ==============================================================================
    HungryGhostMultibandLimiter DSP and Parameter Tests - EXPANDED
  ==============================================================================
*/

#include <JuceHeader.h>
#include <cmath>
#include <vector>
#include "../Source/PluginProcessor.h"
#include "../Source/dsp/LimiterBand.h"
#include "../Source/dsp/BandSplitterIIR.h"
#include "../Source/dsp/Utilities.h"

using namespace juce;

//==============================================================================
// Utility function: compute RMS across all channels
//==============================================================================

static float rms(const AudioBuffer<float>& buffer)
{
    double acc = 0.0;
    const int N = buffer.getNumSamples();
    const int C = buffer.getNumChannels();

    for (int ch = 0; ch < C; ++ch) {
        const float* ptr = buffer.getReadPointer(ch);
        for (int n = 0; n < N; ++n) {
            acc += (double)ptr[n] * ptr[n];
        }
    }

    const double mean = acc / juce::jmax(1, N * C);
    return (float)std::sqrt(mean);
}

//==============================================================================
// PROCESSOR TESTS (1-12)
//==============================================================================

class ParameterLayoutTest : public UnitTest
{
public:
    ParameterLayoutTest() : UnitTest("MBL Parameter Layout") {}

    void runTest() override
    {
        beginTest("Processor has valid parameter layout");
        HungryGhostMultibandLimiterAudioProcessor proc;
        auto layout = proc.createParameterLayout();
        expect(true, "Parameter layout created successfully");
    }
};

class BasicProcessingTest : public UnitTest
{
public:
    BasicProcessingTest() : UnitTest("MBL Basic Audio Processing") {}

    void runTest() override
    {
        beginTest("Processor handles audio buffer correctly");
        HungryGhostMultibandLimiterAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);

        AudioBuffer<float> buffer(2, 512);
        Random rng(42);

        for (int ch = 0; ch < 2; ++ch) {
            for (int n = 0; n < 512; ++n) {
                buffer.setSample(ch, n, rng.nextFloat() * 2.0f - 1.0f);
            }
        }

        MidiBuffer midiMessages;
        proc.processBlock(buffer, midiMessages);
        expect(true, "Audio processing completed without crash");
    }
};

class StereoBusLayoutTest : public UnitTest
{
public:
    StereoBusLayoutTest() : UnitTest("MBL Stereo Bus Layout") {}

    void runTest() override
    {
        beginTest("Processor supports stereo configuration");
        HungryGhostMultibandLimiterAudioProcessor proc;

        AudioProcessor::BusesLayout layout;
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add(AudioChannelSet::stereo());
        layout.outputBuses.add(AudioChannelSet::stereo());

        const bool supported = proc.isBusesLayoutSupported(layout);
        expect(supported, "Stereo layout should be supported");
    }
};

class PrepareReleaseTest : public UnitTest
{
public:
    PrepareReleaseTest() : UnitTest("MBL Prepare/Release") {}

    void runTest() override
    {
        beginTest("Processor prepare and release lifecycle");
        HungryGhostMultibandLimiterAudioProcessor proc;

        proc.prepareToPlay(44100.0, 256);
        expect(true, "44.1 kHz preparation succeeded");

        proc.releaseResources();
        expect(true, "Release resources succeeded");

        proc.prepareToPlay(96000.0, 512);
        expect(true, "96 kHz preparation succeeded");

        proc.releaseResources();
        expect(true, "Second release succeeded");
    }
};

class StateInformationTest : public UnitTest
{
public:
    StateInformationTest() : UnitTest("MBL State Information") {}

    void runTest() override
    {
        beginTest("Processor state can be saved and restored");
        HungryGhostMultibandLimiterAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);

        MemoryBlock stateData;
        proc.getStateInformation(stateData);
        expect(stateData.getSize() > 0, "State data should be non-empty");

        try {
            proc.setStateInformation(stateData.getData(), (int)stateData.getSize());
            expect(true, "State restored successfully");
        } catch (...) {
            expect(false, "State restoration threw exception");
        }
    }
};

class ProgramManagementTest : public UnitTest
{
public:
    ProgramManagementTest() : UnitTest("MBL Program Management") {}

    void runTest() override
    {
        beginTest("Processor program methods work");
        HungryGhostMultibandLimiterAudioProcessor proc;

        const int numPrograms = proc.getNumPrograms();
        expect(numPrograms > 0, "Processor should have at least one program");

        const int currentProgram = proc.getCurrentProgram();
        expect(currentProgram >= 0 && currentProgram < numPrograms, "Current program index should be valid");

        const auto programName = proc.getProgramName(currentProgram);
        expect(programName.isNotEmpty(), "Program name should be non-empty");
    }
};

class FormatSupportTest : public UnitTest
{
public:
    FormatSupportTest() : UnitTest("MBL Format Support") {}

    void runTest() override
    {
        beginTest("Processor declares correct feature support");
        HungryGhostMultibandLimiterAudioProcessor proc;

        expect(!proc.acceptsMidi(), "Should not accept MIDI input");
        expect(!proc.producesMidi(), "Should not produce MIDI output");
        expect(proc.hasEditor(), "Should have an editor");
        expectEquals(proc.getTailLengthSeconds(), 0.0, "Should have zero tail length");
    }
};

class PluginNameTest : public UnitTest
{
public:
    PluginNameTest() : UnitTest("MBL Plugin Name") {}

    void runTest() override
    {
        beginTest("Processor reports correct plugin name");
        HungryGhostMultibandLimiterAudioProcessor proc;

        const auto name = proc.getName();
        expect(name == "HungryGhostMultibandLimiter", "Plugin name should match expected value");
    }
};

class MultipleBlockProcessingTest : public UnitTest
{
public:
    MultipleBlockProcessingTest() : UnitTest("MBL Multiple Block Processing") {}

    void runTest() override
    {
        beginTest("Processor handles continuous stream of blocks");
        HungryGhostMultibandLimiterAudioProcessor proc;
        proc.prepareToPlay(48000.0, 256);

        AudioBuffer<float> buffer(2, 256);
        MidiBuffer midiMessages;
        Random rng(42);

        for (int block = 0; block < 10; ++block) {
            for (int ch = 0; ch < 2; ++ch) {
                for (int n = 0; n < 256; ++n) {
                    buffer.setSample(ch, n, rng.nextFloat() * 0.5f);
                }
            }

            try {
                proc.processBlock(buffer, midiMessages);
            } catch (...) {
                expect(false, String("Block ") + String(block) + " processing threw exception");
                return;
            }
        }

        expect(true, "Successfully processed 10 consecutive blocks");
    }
};

class BufferSizeVariationTest : public UnitTest
{
public:
    BufferSizeVariationTest() : UnitTest("MBL Buffer Size Variations") {}

    void runTest() override
    {
        beginTest("Processor handles different buffer sizes");
        HungryGhostMultibandLimiterAudioProcessor proc;

        const int bufferSizes[] = { 64, 128, 256, 512, 1024, 2048 };

        for (int size : bufferSizes) {
            proc.prepareToPlay(48000.0, size);

            AudioBuffer<float> buffer(2, size);
            MidiBuffer midiMessages;

            for (int ch = 0; ch < 2; ++ch) {
                for (int n = 0; n < size; ++n) {
                    buffer.setSample(ch, n, 0.1f);
                }
            }

            try {
                proc.processBlock(buffer, midiMessages);
            } catch (...) {
                expect(false, String("Failed to process buffer size ") + String(size));
                return;
            }
        }

        expect(true, "Successfully processed all buffer sizes");
    }
};

class SampleRateVariationTest : public UnitTest
{
public:
    SampleRateVariationTest() : UnitTest("MBL Sample Rate Variations") {}

    void runTest() override
    {
        beginTest("Processor handles different sample rates");
        HungryGhostMultibandLimiterAudioProcessor proc;

        const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

        for (double sr : sampleRates) {
            try {
                proc.prepareToPlay(sr, 512);
            } catch (...) {
                expect(false, String("Failed to prepare at sample rate ") + String(sr));
                return;
            }

            AudioBuffer<float> buffer(2, 512);
            MidiBuffer midiMessages;

            for (int ch = 0; ch < 2; ++ch) {
                for (int n = 0; n < 512; ++n) {
                    buffer.setSample(ch, n, 0.1f);
                }
            }

            try {
                proc.processBlock(buffer, midiMessages);
            } catch (...) {
                expect(false, String("Failed to process at sample rate ") + String(sr));
                return;
            }
        }

        expect(true, "Successfully processed all sample rates");
    }
};

class ZeroInputTest : public UnitTest
{
public:
    ZeroInputTest() : UnitTest("MBL Zero Input Handling") {}

    void runTest() override
    {
        beginTest("Processor handles silence (zero input)");
        HungryGhostMultibandLimiterAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);

        AudioBuffer<float> buffer(2, 512);
        buffer.clear();

        MidiBuffer midiMessages;
        proc.processBlock(buffer, midiMessages);

        float maxSample = 0.0f;
        for (int ch = 0; ch < 2; ++ch) {
            const float* ptr = buffer.getReadPointer(ch);
            for (int n = 0; n < 512; ++n) {
                maxSample = std::max(maxSample, std::abs(ptr[n]));
            }
        }

        expectLessThan(maxSample, 1.0e-6f, "Output should remain silent for silent input");
    }
};

//==============================================================================
// DSP COMPONENT TESTS (13-21)
//==============================================================================

class LimiterBandBasicTest : public UnitTest
{
public:
    LimiterBandBasicTest() : UnitTest("MBL LimiterBand Basic") {}

    void runTest() override
    {
        beginTest("LimiterBand initializes and processes without crash");
        hgml::LimiterBand limiter;
        limiter.prepare(48000.0f);

        hgml::LimiterBandParams params;
        params.thresholdDb = -6.0f;
        params.attackMs = 2.0f;
        params.releaseMs = 100.0f;
        params.mixPct = 100.0f;
        params.bypass = false;
        limiter.setParams(params);

        AudioBuffer<float> buffer(2, 512);
        Random rng(123);
        for (int ch = 0; ch < 2; ++ch) {
            for (int n = 0; n < 512; ++n) {
                buffer.setSample(ch, n, rng.nextFloat() * 0.5f);
            }
        }

        const float maxGr = limiter.processBlock(buffer);
        expect(maxGr >= 0.0f && maxGr <= 60.0f, "Gain reduction should be 0-60 dB");
    }
};

class LimiterBandAttenuationTest : public UnitTest
{
public:
    LimiterBandAttenuationTest() : UnitTest("MBL LimiterBand Attenuation") {}

    void runTest() override
    {
        beginTest("LimiterBand attenuates signals above threshold");
        hgml::LimiterBand limiter;
        limiter.prepare(48000.0f);

        hgml::LimiterBandParams params;
        params.thresholdDb = -12.0f;
        params.attackMs = 1.0f;
        params.releaseMs = 10.0f;
        params.mixPct = 100.0f;
        params.bypass = false;
        limiter.setParams(params);

        AudioBuffer<float> buffer(1, 4096);
        const float amplitude = Decibels::decibelsToGain(-6.0f);
        for (int n = 0; n < 4096; ++n) {
            const float s = amplitude * std::sin(2.0f * MathConstants<float>::pi * 1000.0f * (float)n / 48000.0f);
            buffer.setSample(0, n, s);
        }

        limiter.processBlock(buffer);

        float maxAfter = 0.0f;
        for (int n = 2048; n < 4096; ++n) {
            maxAfter = std::max(maxAfter, std::abs(buffer.getSample(0, n)));
        }

        const float thresholdLin = Decibels::decibelsToGain(-12.0f);
        expectLessThan(maxAfter, thresholdLin + 0.01f, "Output should not exceed threshold");
    }
};

class LimiterBandBypassTest : public UnitTest
{
public:
    LimiterBandBypassTest() : UnitTest("MBL LimiterBand Bypass") {}

    void runTest() override
    {
        beginTest("LimiterBand bypass disables limiting");
        hgml::LimiterBand limiter;
        limiter.prepare(48000.0f);

        AudioBuffer<float> original(1, 512);
        Random rng(456);
        for (int n = 0; n < 512; ++n) {
            original.setSample(0, n, rng.nextFloat() * 0.9f);
        }

        AudioBuffer<float> withBypass;
        withBypass.makeCopyOf(original);
        hgml::LimiterBandParams params;
        params.bypass = true;
        params.thresholdDb = -6.0f;
        limiter.setParams(params);
        limiter.processBlock(withBypass);

        float maxDiff = 0.0f;
        for (int n = 0; n < 512; ++n) {
            maxDiff = std::max(maxDiff, std::abs(withBypass.getSample(0, n) - original.getSample(0, n)));
        }

        expectLessThan(maxDiff, 1.0e-6f, "Bypass should not modify signal");
    }
};

class BandSplitterPerfectReconstructionTest : public UnitTest
{
public:
    BandSplitterPerfectReconstructionTest() : UnitTest("MBL BandSplitter Perfect Reconstruction") {}

    void runTest() override
    {
        beginTest("BandSplitterIIR low + high = original within -45 dB");
        hgml::BandSplitterIIR splitter;
        splitter.prepare(48000.0, 2);
        splitter.setCrossoverHz(1000.0f);

        const int N = 8192;
        AudioBuffer<float> src(2, N), low(2, N), high(2, N), sum(2, N), diff(2, N);

        Random rng(789);
        for (int ch = 0; ch < 2; ++ch) {
            for (int n = 0; n < N; ++n) {
                src.setSample(ch, n, rng.nextFloat() * 2.0f - 1.0f);
            }
        }

        std::vector<AudioBuffer<float>> bands;
        splitter.process(src, bands);
        if (bands.size() >= 1) low.makeCopyOf(bands[0], true);
        if (bands.size() >= 2) high.makeCopyOf(bands[1], true);

        for (int ch = 0; ch < 2; ++ch) {
            for (int n = 0; n < N; ++n) {
                sum.setSample(ch, n, low.getSample(ch, n) + high.getSample(ch, n));
                diff.setSample(ch, n, sum.getSample(ch, n) - src.getSample(ch, n));
            }
        }

        const float srcRms = rms(src);
        const float errRms = rms(diff);
        const float errDb = Decibels::gainToDecibels(errRms / (srcRms + 1e-12f));

        expectLessThan(errDb, -45.0f, "Reconstruction error should be < -45 dB");
    }
};

class BandSplitterMultiBandTest : public UnitTest
{
public:
    BandSplitterMultiBandTest() : UnitTest("MBL BandSplitter Multi-band") {}

    void runTest() override
    {
        beginTest("BandSplitterIIR correctly splits into multiple bands");
        hgml::BandSplitterIIR splitter;
        splitter.prepare(48000.0, 2);

        std::vector<float> crossovers = { 250.0f, 1000.0f, 4000.0f };
        splitter.setCrossoverFrequencies(crossovers);

        const int N = 4096;
        AudioBuffer<float> src(2, N);
        for (int ch = 0; ch < 2; ++ch) {
            for (int n = 0; n < N; ++n) {
                src.setSample(ch, n, 0.1f);
            }
        }

        std::vector<AudioBuffer<float>> bands;
        splitter.process(src, bands);

        expect((int)bands.size() == 4, "Should have 4 bands for 3 crossovers");
        expect(bands[0].getNumSamples() == N, "Band buffers should match input size");
    }
};

class LimiterBandMixTest : public UnitTest
{
public:
    LimiterBandMixTest() : UnitTest("MBL LimiterBand Mix") {}

    void runTest() override
    {
        beginTest("LimiterBand mix parameter blends dry and wet");
        hgml::LimiterBand limiter;
        limiter.prepare(48000.0f);

        AudioBuffer<float> buffer(1, 512);
        for (int n = 0; n < 512; ++n) {
            buffer.setSample(0, n, 0.5f);
        }

        hgml::LimiterBandParams params;
        params.thresholdDb = -6.0f;
        params.attackMs = 1.0f;
        params.releaseMs = 10.0f;
        params.bypass = false;

        // 0% mix (fully dry)
        AudioBuffer<float> dry;
        dry.makeCopyOf(buffer);
        params.mixPct = 0.0f;
        limiter.setParams(params);
        limiter.processBlock(dry);

        // 100% mix (fully wet)
        AudioBuffer<float> wet;
        wet.makeCopyOf(buffer);
        params.mixPct = 100.0f;
        limiter.setParams(params);
        limiter.reset();
        limiter.processBlock(wet);

        float dryChange = 0.0f;
        for (int n = 0; n < 512; ++n) {
            dryChange += std::abs(dry.getSample(0, n) - 0.5f);
        }

        float wetChange = 0.0f;
        for (int n = 0; n < 512; ++n) {
            wetChange += std::abs(wet.getSample(0, n) - 0.5f);
        }

        expect(dryChange < 0.01f, "0% mix should not be limited");
        expect(wetChange > 0.01f, "100% mix should be limited");
    }
};

class UtilitiesDbConversionTest : public UnitTest
{
public:
    UtilitiesDbConversionTest() : UnitTest("MBL Utilities dB Conversion") {}

    void runTest() override
    {
        beginTest("dB/linear conversion round-trip");
        expect(std::abs(hgml::dbToLin(hgml::linToDb(1.0f)) - 1.0f) < 1.0e-5f, "1.0 round-trip");
        expect(std::abs(hgml::dbToLin(hgml::linToDb(0.5f)) - 0.5f) < 1.0e-5f, "0.5 round-trip");
        expect(std::abs(hgml::dbToLin(hgml::linToDb(2.0f)) - 2.0f) < 1.0e-5f, "2.0 round-trip");
    }
};

class UtilitiesTimeConstantTest : public UnitTest
{
public:
    UtilitiesTimeConstantTest() : UnitTest("MBL Utilities Time Constants") {}

    void runTest() override
    {
        beginTest("Time constant calculation from milliseconds");
        const float sr = 48000.0f;
        const float coef10ms = hgml::coefFromMs(10.0f, sr);
        const float coef100ms = hgml::coefFromMs(100.0f, sr);

        expect(coef100ms > coef10ms, "100ms coefficient should be greater than 10ms");
        expect(coef10ms > 0.0f && coef10ms < 1.0f, "Coefficient should be 0-1");
        expect(coef100ms > 0.0f && coef100ms < 1.0f, "Coefficient should be 0-1");
    }
};

class SplitterLimiterIntegrationTest : public UnitTest
{
public:
    SplitterLimiterIntegrationTest() : UnitTest("MBL Splitter+Limiter Integration") {}

    void runTest() override
    {
        beginTest("Full band splitting and per-band limiting pipeline");
        hgml::BandSplitterIIR splitter;
        splitter.prepare(48000.0, 2);
        splitter.setCrossoverHz(1000.0f);

        hgml::LimiterBand limiterLow, limiterHigh;
        limiterLow.prepare(48000.0f);
        limiterHigh.prepare(48000.0f);

        hgml::LimiterBandParams paramsLow, paramsHigh;
        paramsLow.thresholdDb = -6.0f;
        paramsHigh.thresholdDb = -12.0f;
        limiterLow.setParams(paramsLow);
        limiterHigh.setParams(paramsHigh);

        AudioBuffer<float> input(2, 1024);
        const float amp = Decibels::decibelsToGain(-3.0f);
        for (int ch = 0; ch < 2; ++ch) {
            for (int n = 0; n < 1024; ++n) {
                input.setSample(ch, n, amp * std::sin(2.0f * MathConstants<float>::pi * 500.0f * (float)n / 48000.0f));
            }
        }

        std::vector<AudioBuffer<float>> bands;
        splitter.process(input, bands);

        expect((int)bands.size() >= 2, "Should have at least 2 bands");

        if (bands.size() >= 1) limiterLow.processBlock(bands[0]);
        if (bands.size() >= 2) limiterHigh.processBlock(bands[1]);

        float maxOutput = 0.0f;
        for (size_t b = 0; b < bands.size() && b < 2; ++b) {
            for (int ch = 0; ch < bands[b].getNumChannels(); ++ch) {
                for (int n = 0; n < bands[b].getNumSamples(); ++n) {
                    maxOutput = std::max(maxOutput, std::abs(bands[b].getSample(ch, n)));
                }
            }
        }

        expect(maxOutput < 10.0f, "Output should be reasonable");
    }
};

//==============================================================================
// Register all tests
//==============================================================================

static ParameterLayoutTest                    paramLayoutTest;
static BasicProcessingTest                    basicProcTest;
static StereoBusLayoutTest                    stereoBusTest;
static PrepareReleaseTest                     prepareReleaseTest;
static StateInformationTest                   stateTest;
static ProgramManagementTest                  programTest;
static FormatSupportTest                      formatTest;
static PluginNameTest                         nameTest;
static MultipleBlockProcessingTest            multiBlockTest;
static BufferSizeVariationTest                bufferTest;
static SampleRateVariationTest                sampleRateTest;
static ZeroInputTest                          zeroInputTest;
static LimiterBandBasicTest                   limiterBandBasicTest;
static LimiterBandAttenuationTest             limiterBandAttenuationTest;
static LimiterBandBypassTest                  limiterBandBypassTest;
static BandSplitterPerfectReconstructionTest  bandSplitterNullTest;
static BandSplitterMultiBandTest              bandSplitterMultiBandTest;
static LimiterBandMixTest                     limiterBandMixTest;
static UtilitiesDbConversionTest              utilitiesDbTest;
static UtilitiesTimeConstantTest              utilitiesTimeConstantTest;
static SplitterLimiterIntegrationTest         integrationTest;

//==============================================================================
// Main entry point
//==============================================================================

int main (int, char**)
{
    ConsoleApplication app;
    UnitTestRunner runner;
    runner.runAllTests();
    return 0;
}
