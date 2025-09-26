#pragma once

#include <JuceHeader.h>

class HungryGhostSaturationAudioProcessor : public juce::AudioProcessor {
public:
    enum class Model { TANH = 0, ATAN = 1, SOFT = 2, FEXP = 3, AMP = 4 };
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

    // Vocal Lo-Fi block (switchable)
    bool vocalLoFi { false };
    juce::dsp::IIR::Filter<float> hpVox, lpVox, presencePeak;

    struct SimpleComp {
        float sr { 48000.0f };
        float thresh { -12.0f };
        float ratio { 8.0f };
        float attMs { 3.0f };
        float relMs { 40.0f };
        float env { 1.0f };
        void prepare(double s) { sr = (float) s; env = 1.0f; }
        float process(float x)
        {
            const float in = x;
            const float db = juce::Decibels::gainToDecibels(juce::jmax(std::abs(in), 1.0e-9f));
            const float over = db - thresh;
            const float grDb = over > 0.0f ? (over - over / ratio) : 0.0f;
            const float target = juce::Decibels::decibelsToGain(-grDb);
            const float aA = std::exp(-1.0f / (0.001f * attMs * sr));
            const float aR = std::exp(-1.0f / (0.001f * relMs * sr));
            env = (target < env ? aA * env + (1.0f - aA) * target
                                 : aR * env + (1.0f - aR) * target);
            return in * env;
        }
    };
    std::vector<SimpleComp> comps; // per-channel
    juce::SmoothedValue<float> vocalAmtSmoothed; // ramp for vocal amount

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> slap { 48000 };
    juce::dsp::IIR::Filter<float> slapHP, slapLP;
    float slapTimeMs { 95.0f }, slapMix { 0.15f }, slapFb { 0.05f };
    bool slapReady { false }; // guard to avoid popSample on unprepared delay line

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
    juce::AudioBuffer<float> voxDry;      // for Vocal Lo-Fi crossfade

    // Internal helpers
    void updateParameters();
    void updateOversamplingIfNeeded(int numChannels);
    void resetDSPState();
    static float mapDriveDbToK(float driveDb);

    static inline float diodeSat(float x, float kk, float a)
    {
        float xp = juce::jlimit(-1.0f, 1.0f, x + 0.5f * a);
        float xn = juce::jlimit(-1.0f, 1.0f, x - 0.5f * a);
        auto f = [kk](float v){ return 1.0f - std::exp(-kk * juce::jlimit(-1.0f, 1.0f, v)); };
        float y = 0.7f * (f(xp) - f(-xn));
        return juce::jlimit(-1.0f, 1.0f, y);
    }

    int currentProgram { 0 }; // 0 = Default, 1 = Obvious
};
