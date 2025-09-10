#include <juce_core/juce_core.h>
#include "../Source/PluginProcessor.h"

struct ParameterLayoutTest : juce::UnitTest {
    ParameterLayoutTest() : juce::UnitTest("APVTS parameter layout") {}

    void runTest() override {
        beginTest("Contains core parameters and advanced toggles via apvts");
        HungryGhostLimiterAudioProcessor proc;
        auto hasId = [&](const char* id){ return proc.apvts.getParameter(id) != nullptr; };

        // Core
        for (auto id : { "thresholdL", "thresholdR", "thresholdLink",
                         "outCeilingL", "outCeilingR", "outCeilingLink",
                         "release", "lookAheadMs", "scHpf", "safetyClip",
                         "inTrimL", "inTrimR", "inTrimLink", "autoRelease" })
            expect(hasId(id), juce::String(id) + " missing");
        // Advanced
        for (auto id : { "q24", "q20", "q16", "q12", "q8",
                         "dT1", "dT2", "sNone", "sArc",
                         "domDigital", "domAnalog", "domTruePeak" })
            expect(hasId(id), juce::String(id) + " missing");
    }
};

static ParameterLayoutTest paramLayoutTest;

