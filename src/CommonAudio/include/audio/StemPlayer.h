#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_devices/juce_audio_devices.h>

namespace audio {

/**
 * Represents a single audio stem with gain control.
 * Wraps AudioFormatReaderSource for file playback.
 */
class Stem {
public:
    Stem() = default;

    bool loadFile(const juce::File& file, juce::AudioFormatManager& formatManager)
    {
        auto* reader = formatManager.createReaderFor(file);
        if (reader == nullptr)
            return false;

        readerSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        filePath = file.getFullPathName();
        name = file.getFileNameWithoutExtension();
        lengthInSamples = reader->lengthInSamples;
        sampleRate = reader->sampleRate;
        numChannels = static_cast<int>(reader->numChannels);
        return true;
    }

    bool isLoaded() const noexcept { return readerSource != nullptr; }
    juce::AudioFormatReaderSource* getSource() const noexcept { return readerSource.get(); }

    const juce::String& getName() const noexcept { return name; }
    const juce::String& getFilePath() const noexcept { return filePath; }
    juce::int64 getLengthInSamples() const noexcept { return lengthInSamples; }
    double getSampleRate() const noexcept { return sampleRate; }
    int getNumChannels() const noexcept { return numChannels; }

    void setGain(float newGain) noexcept { gain.store(newGain); }
    float getGain() const noexcept { return gain.load(); }

    void setMuted(bool shouldMute) noexcept { muted.store(shouldMute); }
    bool isMuted() const noexcept { return muted.load(); }

    void setSolo(bool shouldSolo) noexcept { solo.store(shouldSolo); }
    bool isSolo() const noexcept { return solo.load(); }

private:
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::String filePath;
    juce::String name;
    juce::int64 lengthInSamples = 0;
    double sampleRate = 44100.0;
    int numChannels = 2;
    std::atomic<float> gain { 1.0f };
    std::atomic<bool> muted { false };
    std::atomic<bool> solo { false };
};

/**
 * StemPlayer - Synchronized multi-stem playback engine.
 *
 * Loads multiple audio stems, provides synchronized play/pause/stop/seek,
 * applies per-stem gain/mute/solo, and mixes to stereo output.
 *
 * Uses AudioFormatReaderSource for each stem and MixerAudioSource for mixing.
 */
class StemPlayer : public juce::AudioSource {
public:
    StemPlayer()
    {
        formatManager.registerBasicFormats();
    }

    ~StemPlayer() override
    {
        releaseResources();
    }

    //==========================================================================
    // Stem Management
    //==========================================================================

    /**
     * Load a stem from a file. Returns the index of the loaded stem, or -1 on failure.
     */
    int loadStem(const juce::File& file)
    {
        auto stem = std::make_unique<Stem>();
        if (!stem->loadFile(file, formatManager))
            return -1;

        const juce::ScopedLock sl(lock);

        auto* source = stem->getSource();
        source->setLooping(looping);

        int index = static_cast<int>(stems.size());
        stems.push_back(std::move(stem));

        // Add to mixer if already prepared
        if (isPrepared)
        {
            source->prepareToPlay(blockSize, currentSampleRate);
            mixer.addInputSource(source, false);
        }

        updateLongestStemLength();
        return index;
    }

    /**
     * Load multiple stems from files. Returns the number successfully loaded.
     */
    int loadStems(const juce::Array<juce::File>& files)
    {
        int loaded = 0;
        for (const auto& file : files)
        {
            if (loadStem(file) >= 0)
                ++loaded;
        }
        return loaded;
    }

    /**
     * Remove a stem by index. Returns true if successful.
     */
    bool removeStem(int index)
    {
        const juce::ScopedLock sl(lock);

        if (index < 0 || index >= static_cast<int>(stems.size()))
            return false;

        auto* source = stems[index]->getSource();
        mixer.removeInputSource(source);
        stems.erase(stems.begin() + index);
        updateLongestStemLength();
        return true;
    }

    /**
     * Remove all stems.
     */
    void clearStems()
    {
        const juce::ScopedLock sl(lock);
        mixer.removeAllInputs();
        stems.clear();
        longestStemSamples = 0;
    }

    int getNumStems() const noexcept
    {
        const juce::ScopedLock sl(lock);
        return static_cast<int>(stems.size());
    }

    Stem* getStem(int index)
    {
        const juce::ScopedLock sl(lock);
        if (index >= 0 && index < static_cast<int>(stems.size()))
            return stems[index].get();
        return nullptr;
    }

    const Stem* getStem(int index) const
    {
        const juce::ScopedLock sl(lock);
        if (index >= 0 && index < static_cast<int>(stems.size()))
            return stems[index].get();
        return nullptr;
    }

    //==========================================================================
    // Transport Controls
    //==========================================================================

    void play()
    {
        playing.store(true);
    }

    void pause()
    {
        playing.store(false);
    }

    void stop()
    {
        playing.store(false);
        setPositionInSamples(0);
    }

    bool isPlaying() const noexcept
    {
        return playing.load();
    }

    /**
     * Seek all stems to a position in samples.
     */
    void setPositionInSamples(juce::int64 newPosition)
    {
        const juce::ScopedLock sl(lock);
        for (auto& stem : stems)
        {
            if (stem->isLoaded())
                stem->getSource()->setNextReadPosition(newPosition);
        }
        currentPosition.store(newPosition);
    }

    /**
     * Seek all stems to a position in seconds.
     */
    void setPositionInSeconds(double seconds)
    {
        setPositionInSamples(static_cast<juce::int64>(seconds * currentSampleRate));
    }

    /**
     * Seek all stems to a normalized position (0.0 to 1.0).
     */
    void setPositionNormalized(double normalized)
    {
        auto pos = static_cast<juce::int64>(normalized * static_cast<double>(longestStemSamples));
        setPositionInSamples(pos);
    }

    juce::int64 getPositionInSamples() const noexcept
    {
        return currentPosition.load();
    }

    double getPositionInSeconds() const noexcept
    {
        return static_cast<double>(currentPosition.load()) / currentSampleRate;
    }

    double getPositionNormalized() const noexcept
    {
        if (longestStemSamples == 0)
            return 0.0;
        return static_cast<double>(currentPosition.load()) / static_cast<double>(longestStemSamples);
    }

    juce::int64 getLengthInSamples() const noexcept
    {
        return longestStemSamples;
    }

    double getLengthInSeconds() const noexcept
    {
        return static_cast<double>(longestStemSamples) / currentSampleRate;
    }

    //==========================================================================
    // Playback Options
    //==========================================================================

    void setLooping(bool shouldLoop)
    {
        looping = shouldLoop;
        const juce::ScopedLock sl(lock);
        for (auto& stem : stems)
        {
            if (stem->isLoaded())
                stem->getSource()->setLooping(shouldLoop);
        }
    }

    bool isLooping() const noexcept
    {
        return looping;
    }

    void setMasterGain(float newGain) noexcept
    {
        masterGain.store(newGain);
    }

    float getMasterGain() const noexcept
    {
        return masterGain.load();
    }

    //==========================================================================
    // AudioSource Implementation
    //==========================================================================

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override
    {
        const juce::ScopedLock sl(lock);

        currentSampleRate = sampleRate;
        blockSize = samplesPerBlockExpected;
        isPrepared = true;

        // Prepare all stem sources
        for (auto& stem : stems)
        {
            if (stem->isLoaded())
            {
                stem->getSource()->prepareToPlay(samplesPerBlockExpected, sampleRate);
                mixer.addInputSource(stem->getSource(), false);
            }
        }

        mixer.prepareToPlay(samplesPerBlockExpected, sampleRate);

        // Allocate working buffers for per-stem processing
        stemBuffer.setSize(2, samplesPerBlockExpected);
    }

    void releaseResources() override
    {
        const juce::ScopedLock sl(lock);

        mixer.removeAllInputs();
        mixer.releaseResources();

        for (auto& stem : stems)
        {
            if (stem->isLoaded())
                stem->getSource()->releaseResources();
        }

        isPrepared = false;
    }

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
    {
        const juce::ScopedLock sl(lock);

        if (!playing.load() || stems.empty())
        {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        // Clear output buffer
        bufferToFill.clearActiveBufferRegion();

        // Check if any stem is soloed
        bool anySolo = false;
        for (const auto& stem : stems)
        {
            if (stem->isSolo())
            {
                anySolo = true;
                break;
            }
        }

        // Process each stem individually for gain/mute/solo control
        for (auto& stem : stems)
        {
            if (!stem->isLoaded())
                continue;

            // Skip muted stems, or non-solo stems when solo is active
            bool shouldPlay = !stem->isMuted();
            if (anySolo)
                shouldPlay = stem->isSolo();

            if (!shouldPlay)
            {
                // Still need to advance the read position to stay in sync
                stem->getSource()->getNextAudioBlock(
                    juce::AudioSourceChannelInfo(stemBuffer, 0, bufferToFill.numSamples));
                continue;
            }

            // Read stem audio
            stemBuffer.clear();
            juce::AudioSourceChannelInfo stemInfo(stemBuffer, 0, bufferToFill.numSamples);
            stem->getSource()->getNextAudioBlock(stemInfo);

            // Apply stem gain and add to output
            float stemGain = stem->getGain();
            int numChannels = juce::jmin(bufferToFill.buffer->getNumChannels(), stemBuffer.getNumChannels());

            for (int ch = 0; ch < numChannels; ++ch)
            {
                bufferToFill.buffer->addFrom(
                    ch,
                    bufferToFill.startSample,
                    stemBuffer,
                    ch,
                    0,
                    bufferToFill.numSamples,
                    stemGain);
            }
        }

        // Apply master gain
        float master = masterGain.load();
        if (std::abs(master - 1.0f) > 0.0001f)
        {
            bufferToFill.buffer->applyGain(
                bufferToFill.startSample,
                bufferToFill.numSamples,
                master);
        }

        // Update position tracking (use first stem as reference)
        if (!stems.empty() && stems[0]->isLoaded())
        {
            currentPosition.store(stems[0]->getSource()->getNextReadPosition());
        }

        // Check for end of playback (non-looping)
        if (!looping && currentPosition.load() >= longestStemSamples)
        {
            playing.store(false);
        }
    }

    //==========================================================================
    // Format Manager Access
    //==========================================================================

    juce::AudioFormatManager& getFormatManager() noexcept
    {
        return formatManager;
    }

private:
    void updateLongestStemLength()
    {
        longestStemSamples = 0;
        for (const auto& stem : stems)
        {
            if (stem->getLengthInSamples() > longestStemSamples)
                longestStemSamples = stem->getLengthInSamples();
        }
    }

    juce::AudioFormatManager formatManager;
    juce::MixerAudioSource mixer;
    std::vector<std::unique_ptr<Stem>> stems;

    mutable juce::CriticalSection lock;

    double currentSampleRate = 44100.0;
    int blockSize = 512;
    bool isPrepared = false;
    bool looping = false;

    std::atomic<bool> playing { false };
    std::atomic<juce::int64> currentPosition { 0 };
    std::atomic<float> masterGain { 1.0f };
    juce::int64 longestStemSamples = 0;

    juce::AudioBuffer<float> stemBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StemPlayer)
};

} // namespace audio
