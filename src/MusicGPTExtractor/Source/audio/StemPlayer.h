#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <memory>

namespace audio {

// Individual stem track
struct StemTrack
{
    juce::String name;
    std::unique_ptr<juce::AudioBuffer<float>> buffer;
    std::atomic<float> gain { 1.0f };
    std::atomic<bool> muted { false };
    std::atomic<bool> solo { false };
};

// Player for multiple stem tracks with mixing
class StemPlayer
{
public:
    static constexpr int kMaxStems = 5;

    StemPlayer() = default;
    ~StemPlayer() = default;

    // Prepare for playback
    void prepare(double sampleRate, int samplesPerBlock);

    // Load stems from files
    bool loadStems(const juce::StringArray& stemPaths);

    // Clear all loaded stems
    void clearStems();

    // Process audio block (mix all stems to output)
    void processBlock(juce::AudioBuffer<float>& outputBuffer);

    // Transport controls
    void play() { playing.store(true); }
    void pause() { playing.store(false); }
    void stop();
    void setPosition(double normalizedPosition);
    double getPosition() const;
    double getTotalDuration() const { return totalDurationSeconds; }

    // Stem controls
    void setStemGain(int stemIndex, float gain);
    void setStemMuted(int stemIndex, bool muted);
    void setStemSolo(int stemIndex, bool solo);

    float getStemGain(int stemIndex) const;
    bool isStemMuted(int stemIndex) const;
    bool isStemSolo(int stemIndex) const;

    // Query
    int getNumStems() const { return numStems; }
    juce::String getStemName(int index) const;
    bool isPlaying() const { return playing.load(); }

private:
    double sampleRateHz = 44100.0;
    int blockSize = 512;

    std::array<StemTrack, kMaxStems> stems;
    int numStems = 0;

    std::atomic<bool> playing { false };
    std::atomic<int64_t> playheadSamples { 0 };
    int64_t totalSamples = 0;
    double totalDurationSeconds = 0.0;

    juce::AudioFormatManager formatManager;

    bool hasSoloActive() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemPlayer)
};

} // namespace audio
