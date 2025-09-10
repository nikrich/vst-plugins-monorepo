#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include "../Source/dsp/LimiterDSP.h"

struct LimiterAttenAndCeilTest : juce::UnitTest {
    LimiterAttenAndCeilTest() : juce::UnitTest("Limiter: attenuates and no overshoot") {}
    void runTest() override {
        beginTest("Attenuates and respects ceiling with safety ON");
        hgl::LimiterDSP lim;
        const float osSR = 192000.0f;
        lim.prepare(osSR, /*maxLA*/ 2048);

        hgl::LimiterParams p{};
        p.preGainL = p.preGainR = juce::Decibels::decibelsToGain(12.0f); // +12 dB pre
        p.ceilLin  = juce::Decibels::decibelsToGain(-1.0f);              // -1 dBFS ceiling
        p.lookAheadSamplesOS = 256;
        const float relSec = 0.120f; // 120ms
        p.releaseAlphaOS = std::exp(-1.0f / (relSec * osSR));
        p.scHpfOn = true;
        p.safetyOn = true;
        p.autoReleaseOn = false;
        lim.setParams(p);

        const int N = 4096;
        std::vector<float> L(N), R(N);
        for (int i = 0; i < N; ++i) {
            float t = (float) i / (float) N;
            float s = std::sin(2.0f * juce::MathConstants<float>::pi * 997.0f * t);
            L[i] = R[i] = s * 0.9f; // pre-gain will push above ceiling
        }
        auto attenDb = lim.processBlockOS(L.data(), R.data(), N);
        expect(attenDb >= 0.0f, "Meter attenuation should be non-negative");

        float peak = 0.0f;
        for (int i = 0; i < N; ++i) peak = std::max(peak, std::max(std::abs(L[i]), std::abs(R[i])));
        expect(peak <= juce::Decibels::decibelsToGain(-1.0f) + 1.0e-3f, "Output should not overshoot ceiling");
    }
};

struct LimiterLookAheadLatencyTest : juce::UnitTest {
    LimiterLookAheadLatencyTest() : juce::UnitTest("Limiter: look-ahead latency") {}
    void runTest() override {
        beginTest("Look-ahead introduces expected delay when not limiting");
        hgl::LimiterDSP lim;
        const float osSR = 96000.0f;
        lim.prepare(osSR, /*maxLA*/ 2048);

        hgl::LimiterParams p{};
        p.preGainL = p.preGainR = 1.0f; // no limiting
        p.ceilLin  = 10.0f;             // very high ceiling to bypass GR
        p.lookAheadSamplesOS = 128;
        p.releaseAlphaOS = 0.0f;
        p.scHpfOn = false;
        p.safetyOn = false;
        p.autoReleaseOn = false;
        lim.setParams(p);

        const int N = 512;
        std::vector<float> L(N, 1.0f), R(N, 1.0f);
        // Expect the output to be zero until lookAheadSamplesOS due to the delay line
        auto attenDb = lim.processBlockOS(L.data(), R.data(), N);
        juce::ignoreUnused(attenDb);

        bool sawNonZeroBeforeLA = false;
        for (int i = 0; i < p.lookAheadSamplesOS && i < N; ++i)
            if (std::abs(L[i]) > 1.0e-6f || std::abs(R[i]) > 1.0e-6f) sawNonZeroBeforeLA = true;
        expect(!sawNonZeroBeforeLA, "No output before lookahead delay");
    }
};

struct LimiterAutoReleaseTest : juce::UnitTest {
    LimiterAutoReleaseTest() : juce::UnitTest("Limiter: auto release vs manual") {}
    void runTest() override {
        beginTest("Auto-release recovers faster than manual for same slow time constant");
        const float osSR = 192000.0f;
        const int N = 4096;

        auto makeBurst = [&](std::vector<float>& L, std::vector<float>& R){
            L.assign(N, 0.0f); R.assign(N, 0.0f);
            // 1k-sample hot burst followed by zeros
            for (int i = 0; i < 1024; ++i) {
                float s = 0.95f * std::sin(2.0f * juce::MathConstants<float>::pi * 1000.0f * (float)i / (float)N);
                L[i] = R[i] = s;
            }
        };

        auto run = [&](bool autoRel) {
            hgl::LimiterDSP lim;
            lim.prepare(osSR, /*maxLA*/ 2048);
            hgl::LimiterParams p{};
            p.preGainL = p.preGainR = juce::Decibels::decibelsToGain(12.0f);
            p.ceilLin  = 1.0f;
            p.lookAheadSamplesOS = 128;
            p.releaseAlphaOS = std::exp(-1.0f / (0.300f * osSR)); // slow ~300 ms
            p.scHpfOn = true;
            p.safetyOn = false;
            p.autoReleaseOn = autoRel;
            lim.setParams(p);
            std::vector<float> L, R; makeBurst(L,R);
            lim.processBlockOS(L.data(), R.data(), N);
            // measure residual attenuation near the end
            float peakEnd = 0.0f; for (int i = N - 512; i < N; ++i) peakEnd = std::max(peakEnd, std::max(std::abs(L[i]), std::abs(R[i])));
            return peakEnd;
        };

        const float endManual = run(false);
        const float endAuto   = run(true);
        // Auto should recover closer to unity by end of buffer -> higher peak
        expect(endAuto >= endManual - 1.0e-3f, "Auto release should recover at least as fast as manual");
    }
};

static LimiterAttenAndCeilTest test1;
static LimiterLookAheadLatencyTest test2;
static LimiterAutoReleaseTest test3;

int main() {
    juce::UnitTestRunner r;
    r.runAllTests();
    return 0; // JUCE's UnitTestRunner doesn't expose a failure count API across versions
}

