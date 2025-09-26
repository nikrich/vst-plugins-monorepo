#pragma once

#include <JuceHeader.h>

class HungryGhostSaturationAudioProcessor : public juce::AudioProcessor {
public:
    enum class Model { TANH = 0, ATAN = 1, SOFT = 2, FEXP = 3 };
    enum class ChannelMode { Stereo = 0, DualMono = 1, MonoSum = 2 };

    HungryGhostSaturationAudioProcessor();
    ~HungryGhostSaturationAudioProcessor() override = default;

    // AudioProcessor overrides
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // Program/State
    const juce::String getName() const override { return "HungryGhostSaturation"; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 2; }
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

private:
    // Parameters and layout
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Cached runtime settings
    float sampleRate { 48000.0f };
    int maxBlock { 512 };
    int lastNumChannels { 2 };

    // Gains and mix
    float inGain { 1.0f };
    float outGain { 1.0f };
    float mixTarget { 1.0f };
    juce::SmoothedValue<float> mixSmoothed;

    // Drive mapping and model
    Model model { Model::TANH };
    float driveDb { 12.0f };
    float k { 2.5f };
    float driveGain { 1.0f }; // NEW: pre-gain into the shaper
    float invTanhK { 1.0f };
    float invAtanK { 1.0f };
    float asym { 0.0f };

    // Channel mode
    ChannelMode channelMode { ChannelMode::Stereo };

    // Filters
    juce::dsp::IIR::Filter<float> preTilt;
    juce::dsp::IIR::Filter<float> postDeTilt;
    juce::dsp::IIR::Filter<float> postLP;
    bool enablePreTilt { false };
    bool enablePostLP { false };

    // Oversampling
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int osFactor { 2 };
    int osStages { 1 }; // 1->2x, 2->4x

    // DC blocker for FEXP (per-channel)
    struct DCState { float x1 { 0.0f }; float y1 { 0.0f }; };
    std::vector<DCState> dcStates;
    float dcR { 0.999f }; // set from sampleRate

    // Auto gain RMS trackers and makeup
    bool autoGain { true };
    float alphaRms { 1.0e-3f };
    // mean-square trackers (per-channel for dual mono)
    float eIn[2] { 1.0e-4f, 1.0e-4f };
    float eOut[2] { 1.0e-4f, 1.0e-4f };
    juce::SmoothedValue<float> makeupSmoothedL;
    juce::SmoothedValue<float> makeupSmoothedR;

    // Buffers
    juce::AudioBuffer<float> dryBuffer;   // dry tap after input trim
    juce::AudioBuffer<float> monoScratch; // for mono-sum processing

    // Internal helpers
    void updateParameters();
    void updateOversamplingIfNeeded(int numChannels);
    void resetDSPState();
    static float mapDriveDbToK(float driveDb);

    int currentProgram { 0 }; // 0 = Default, 1 = Obvious
};
