#include <JuceHeader.h>
#include "../Source/dsp/BandSplitterIIR.h"
#include "../Source/dsp/CompressorBand.h"
#include "../Source/PluginProcessor.h"

using namespace juce;

static float rms(const AudioBuffer<float>& b)
{
    double acc = 0.0; int N = b.getNumSamples(); int C = b.getNumChannels();
    for (int ch = 0; ch < C; ++ch)
    {
        const float* x = b.getReadPointer(ch);
        for (int n = 0; n < N; ++n) acc += (double)x[n] * x[n];
    }
    const double m = acc / juce::jmax(1, N * C);
    return (float) std::sqrt(m);
}

class CrossoverNullTest : public UnitTest
{
public:
    CrossoverNullTest() : UnitTest("MBC LR4 Crossover Null") {}
    void runTest() override
    {
        beginTest("LR4 LP+HP sums to ~original");
        const double sr = 48000.0; const int N = 8192; const int C = 2;
        hgmbc::BandSplitterIIR split; split.prepare(sr, C); split.setCrossoverHz(1000.0f);

        AudioBuffer<float> src(C, N), low(C, N), high(C, N), sum(C, N);
        Random rng(42);
        for (int ch = 0; ch < C; ++ch) for (int n = 0; n < N; ++n) src.setSample(ch, n, rng.nextFloat() * 2.0f - 1.0f);

        split.process(src, low, high);
        for (int ch = 0; ch < C; ++ch)
        {
            float* dst = sum.getWritePointer(ch);
            const float* l = low.getReadPointer(ch);
            const float* h = high.getReadPointer(ch);
            for (int n = 0; n < N; ++n) dst[n] = l[n] + h[n];
        }
        AudioBuffer<float> diff(C, N);
        for (int ch = 0; ch < C; ++ch)
        {
            float* d = diff.getWritePointer(ch);
            const float* a = sum.getReadPointer(ch);
            const float* b = src.getReadPointer(ch);
            for (int n = 0; n < N; ++n) d[n] = a[n] - b[n];
        }
        const float srcR = rms(src);
        const float errR = rms(diff);
        const float errDb = Decibels::gainToDecibels(errR / (srcR + 1e-12f));
        expectLessThan(errDb, -45.0f); // target null < -45 dB RMS
    }
};

class StaticGRTest : public UnitTest
{
public:
    StaticGRTest() : UnitTest("MBC Static GR") {}
    void runTest() override
    {
        beginTest("Steady-state GR matches theory (2:1 above -18 dB)");
        const double sr = 48000.0; const int N = 8192; const int C = 2;
        hgmbc::CompressorBand comp; comp.prepare(sr, C, 2048); comp.setLookaheadSamples(0);
        hgmbc::CompressorBandParams p; p.threshold_dB = -18.0f; p.ratio = 2.0f; p.knee_dB = 0.0f; p.attack_ms = 10.0f; p.release_ms = 1000.0f; p.mix_pct = 100.0f; p.detectorType = 0; // Peak
        comp.setParams(p);

        AudioBuffer<float> band(C, N);
        // Sine at -6 dBFS
        const float amp = Decibels::decibelsToGain(-6.0f);
        for (int n = 0; n < N; ++n)
        {
            const float s = amp * std::sin(2.0 * MathConstants<double>::pi * 1000.0 * (double)n / sr);
            for (int ch = 0; ch < C; ++ch) band.setSample(ch, n, s);
        }

        comp.process(band);
        // Ignore first 1000 samples for settling
        AudioBuffer<float> tail(C, N - 1000);
        for (int ch = 0; ch < C; ++ch)
        {
            memcpy(tail.getWritePointer(ch), band.getReadPointer(ch) + 1000, sizeof(float) * (size_t)(N - 1000));
        }
        const float outR = rms(tail);
        const float outDb = Decibels::gainToDecibels(outR + 1e-9f);
        // Theory (RMS): input -6 dBFS peak (~-9 dB RMS), output peak -12 dBFS (~-15 dB RMS)
        expectWithinAbsoluteError(outDb, -15.0f, 0.8f);
    }
};

// Latency test disabled for now due to JUCE timer/shutdown assertions in console harness on some setups.
// class LatencyReportTest : public UnitTest
// {
// public:
//     LatencyReportTest() : UnitTest("MBC Latency Reporting") {}

static CrossoverNullTest   crossoverNullTest;
static StaticGRTest        staticGRTest;
// static LatencyReportTest   latencyReportTest;

int main (int, char**)
{
    ConsoleApplication app;
    UnitTestRunner runner;
    runner.runAllTests();
    return 0;
}
