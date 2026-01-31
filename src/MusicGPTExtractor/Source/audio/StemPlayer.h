#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>

// Multi-stem audio player with individual level control
class StemPlayer
{
public:
    enum class Stem { Vocals = 0, Drums, Bass, Other, Count };

    StemPlayer()
    {
        formatManager.registerBasicFormats();
    }

    void prepare(double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate;
        currentBlockSize = samplesPerBlock;

        for (auto& src : sources)
        {
            if (src)
                src->prepareToPlay(samplesPerBlock, sampleRate);
        }
    }

    void loadStems(const juce::File& vocals, const juce::File& drums,
                   const juce::File& bass, const juce::File& other)
    {
        loadStem(Stem::Vocals, vocals);
        loadStem(Stem::Drums, drums);
        loadStem(Stem::Bass, bass);
        loadStem(Stem::Other, other);
    }

    void loadStem(Stem stem, const juce::File& file)
    {
        const int idx = static_cast<int>(stem);
        if (idx < 0 || idx >= static_cast<int>(Stem::Count))
            return;

        auto* reader = formatManager.createReaderFor(file);
        if (reader == nullptr)
            return;

        auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
        newSource->prepareToPlay(currentBlockSize, currentSampleRate);

        sources[idx] = std::move(newSource);
        stemLoaded[idx] = true;
    }

    void setLevels(float vocals, float drums, float bass, float other)
    {
        levels[static_cast<int>(Stem::Vocals)] = vocals;
        levels[static_cast<int>(Stem::Drums)] = drums;
        levels[static_cast<int>(Stem::Bass)] = bass;
        levels[static_cast<int>(Stem::Other)] = other;
    }

    void play()
    {
        for (auto& src : sources)
        {
            if (src)
                src->setNextReadPosition(0);
        }
        isPlaying = true;
    }

    void stop()
    {
        isPlaying = false;
    }

    void processBlock(juce::AudioBuffer<float>& buffer)
    {
        if (!isPlaying.load())
            return;

        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        juce::AudioSourceChannelInfo info(&buffer, 0, numSamples);

        // Temporary buffer for each stem
        juce::AudioBuffer<float> stemBuffer(numChannels, numSamples);

        for (int i = 0; i < static_cast<int>(Stem::Count); ++i)
        {
            if (!sources[i] || !stemLoaded[i])
                continue;

            stemBuffer.clear();
            juce::AudioSourceChannelInfo stemInfo(&stemBuffer, 0, numSamples);
            sources[i]->getNextAudioBlock(stemInfo);

            const float level = levels[i].load();
            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.addFrom(ch, 0, stemBuffer, ch, 0, numSamples, level);
            }
        }
    }

    bool hasLoadedStems() const
    {
        for (int i = 0; i < static_cast<int>(Stem::Count); ++i)
        {
            if (stemLoaded[i])
                return true;
        }
        return false;
    }

private:
    juce::AudioFormatManager formatManager;
    std::array<std::unique_ptr<juce::AudioFormatReaderSource>, static_cast<size_t>(Stem::Count)> sources;
    std::array<bool, static_cast<size_t>(Stem::Count)> stemLoaded { false, false, false, false };
    std::array<std::atomic<float>, static_cast<size_t>(Stem::Count)> levels { 1.0f, 1.0f, 1.0f, 1.0f };

    std::atomic<bool> isPlaying { false };
    double currentSampleRate { 44100.0 };
    int currentBlockSize { 512 };
};
