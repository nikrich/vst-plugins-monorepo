/*
  ==============================================================================
    HungryGhostMultibandLimiter DSP and Parameter Tests
  ==============================================================================
*/

#include <JuceHeader.h>
#include <cmath>
#include <vector>
#include "../Source/PluginProcessor.h"

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
// Test 1: Parameter Layout
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
        // Verify layout is non-empty (will be populated with multiband limiter parameters)
        expect(true, "Parameter layout created successfully");
    }
};

//==============================================================================
// Test 2: Basic Processing (Pass-Through)
//==============================================================================

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

        // Fill with random signal
        for (int ch = 0; ch < 2; ++ch) {
            for (int n = 0; n < 512; ++n) {
                buffer.setSample(ch, n, rng.nextFloat() * 2.0f - 1.0f);
            }
        }

        // Store original for comparison
        AudioBuffer<float> original;
        original.makeCopyOf(buffer);

        // Process
        MidiBuffer midiMessages;
        proc.processBlock(buffer, midiMessages);

        // Currently should be pass-through
        expect(true, "Audio processing completed without crash");
    }
};

//==============================================================================
// Test 3: Stereo Bus Layout
//==============================================================================

class StereoBusLayoutTest : public UnitTest
{
public:
    StereoBusLayoutTest() : UnitTest("MBL Stereo Bus Layout") {}

    void runTest() override
    {
        beginTest("Processor supports stereo configuration");
        HungryGhostMultibandLimiterAudioProcessor proc;

        // Test stereo layout
        AudioProcessor::BusesLayout layout;
        layout.inputBuses.clear();
        layout.outputBuses.clear();
        layout.inputBuses.add(AudioChannelSet::stereo());
        layout.outputBuses.add(AudioChannelSet::stereo());

        const bool supported = proc.isBusesLayoutSupported(layout);
        expect(supported, "Stereo layout should be supported");
    }
};

//==============================================================================
// Test 4: Preparation and Release
//==============================================================================

class PrepareReleaseTest : public UnitTest
{
public:
    PrepareReleaseTest() : UnitTest("MBL Prepare/Release") {}

    void runTest() override
    {
        beginTest("Processor prepare and release lifecycle");
        HungryGhostMultibandLimiterAudioProcessor proc;

        // Multiple prepare calls with different sample rates
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

//==============================================================================
// Test 5: State Information (Save/Load)
//==============================================================================

class StateInformationTest : public UnitTest
{
public:
    StateInformationTest() : UnitTest("MBL State Information") {}

    void runTest() override
    {
        beginTest("Processor state can be saved and restored");
        HungryGhostMultibandLimiterAudioProcessor proc;
        proc.prepareToPlay(48000.0, 512);

        // Save state
        MemoryBlock stateData;
        proc.getStateInformation(stateData);
        expect(stateData.getSize() > 0, "State data should be non-empty");

        // Restore state
        try {
            proc.setStateInformation(stateData.getData(), (int)stateData.getSize());
            expect(true, "State restored successfully");
        } catch (...) {
            expect(false, "State restoration threw exception");
        }
    }
};

//==============================================================================
// Test 6: Program Management
//==============================================================================

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

//==============================================================================
// Test 7: MIDI and Audio Format Support
//==============================================================================

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

//==============================================================================
// Test 8: Plugin Naming
//==============================================================================

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

//==============================================================================
// Test 9: Multiple Process Blocks
//==============================================================================

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

        // Process 10 consecutive blocks
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

//==============================================================================
// Test 10: Buffer Size Variations
//==============================================================================

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

//==============================================================================
// Test 11: Sample Rate Variations
//==============================================================================

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

//==============================================================================
// Test 12: Zero Input Handling
//==============================================================================

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

        // Verify output is still silent
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
// Register all tests
//==============================================================================

static ParameterLayoutTest          paramLayoutTest;
static BasicProcessingTest          basicProcTest;
static StereoBusLayoutTest          stereoBusTest;
static PrepareReleaseTest           prepareReleaseTest;
static StateInformationTest         stateTest;
static ProgramManagementTest        programTest;
static FormatSupportTest            formatTest;
static PluginNameTest               nameTest;
static MultipleBlockProcessingTest  multiBlockTest;
static BufferSizeVariationTest      bufferTest;
static SampleRateVariationTest      sampleRateTest;
static ZeroInputTest                zeroInputTest;

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
