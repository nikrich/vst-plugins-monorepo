#pragma once
#include <JuceHeader.h>

namespace hgmbc { class BandSplitterIIR; class CompressorBand; }

class HungryGhostMultibandCompressorAudioProcessor : public juce::AudioProcessor
{
public:
    HungryGhostMultibandCompressorAudioProcessor();
    ~HungryGhostMultibandCompressorAudioProcessor() override;

    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlockExpected) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Editor
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Program/basics
    const juce::String getName() const override { return "HungryGhostMultibandCompressor"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    using APVTS = juce::AudioProcessorValueTreeState;
    APVTS apvts { *this, nullptr, "params", createParameterLayout() };
    static APVTS::ParameterLayout createParameterLayout();

    int getReportedLatencySamples() const { return reportedLatency.load(); }

private:
    float sampleRateHz = 44100.0f;

    // Global params (cached per block)
    int   bandCount = 2;
    std::vector<float> crossoverHz;
    float lookAheadMs = 3.0f;

    // Factory presets
    int currentProgramIndex = 0;
    struct PresetConfig {
        juce::String name;
        std::map<juce::String, float> parameters;
    };
    std::vector<PresetConfig> factoryPresets;
    void createFactoryPresets();
    void loadPreset(const PresetConfig& preset);

    std::atomic<int> reportedLatency { 0 };

    // Analyzer FIFOs (mono, decimated) and GR meters per band
    std::unique_ptr<juce::AbstractFifo> analyzerFifoPre;
    std::unique_ptr<juce::AbstractFifo> analyzerFifoPost;
    std::vector<float> analyzerRingPre;
    std::vector<float> analyzerRingPost;
    int analyzerDecimate = 4;

    std::atomic<float> grBandDb[6] { 0,0,0,0,0,0 };

public:
    // Pull analyzer samples into dst (returns count)
    int readAnalyzerPre(float* dst, int maxSamples);
    int readAnalyzerPost(float* dst, int maxSamples);
    int getAnalyzerDecimate() const { return analyzerDecimate; }
    float getBandGrDb(int index) const { if (index < 0 || index >= 6) return 0.0f; return grBandDb[index].load(); }

    // ====== N-band DSP graph state ======
    // Working buffers for N bands (dry = copy of input per band before compression)
    std::vector<juce::AudioBuffer<float>> bandDry;
    std::vector<juce::AudioBuffer<float>> bandProc;

    // DSP components
    std::unique_ptr<hgmbc::BandSplitterIIR> splitter;
    std::vector<std::unique_ptr<hgmbc::CompressorBand>> compressors;

    // ===== Parallel EQ Stage (Option B) =====
static constexpr int kMaxEqBands = 16;
    struct EqBandProc {
        bool enabled = false;
        int type = 0; // 0=Bell,1=LowShelf,2=HighShelf,3=LowPass,4=HighPass,5=Notch
        float freq = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
        juce::dsp::IIR::Filter<float> filt[2];
        void reset() { for (auto& f : filt) f.reset(); }
    };
    EqBandProc eq[kMaxEqBands];

    void ensureBandBuffers(int numChannels, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HungryGhostMultibandCompressorAudioProcessor)
};
