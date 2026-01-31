#include "StemPlayer.h"

namespace audio {

//==============================================================================

void StemPlayer::prepare(double sampleRate, int samplesPerBlock)
{
    sampleRateHz = sampleRate;
    blockSize = samplesPerBlock;

    // Register common audio formats
    formatManager.registerBasicFormats();
}

//==============================================================================

bool StemPlayer::loadStems(const juce::StringArray& stemPaths)
{
    clearStems();

    for (const auto& path : stemPaths)
    {
        if (numStems >= kMaxStems)
            break;

        juce::File file(path);
        if (!file.existsAsFile())
            continue;

        // Create reader for the file
        std::unique_ptr<juce::AudioFormatReader> reader(
            formatManager.createReaderFor(file)
        );

        if (reader == nullptr)
            continue;

        // Create buffer and read audio data
        auto buffer = std::make_unique<juce::AudioBuffer<float>>(
            static_cast<int>(reader->numChannels),
            static_cast<int>(reader->lengthInSamples)
        );

        reader->read(buffer.get(), 0, static_cast<int>(reader->lengthInSamples), 0, true, true);

        // Store in stem track
        auto& stem = stems[static_cast<size_t>(numStems)];
        stem.name = file.getFileNameWithoutExtension();
        stem.buffer = std::move(buffer);
        stem.gain.store(1.0f);
        stem.muted.store(false);
        stem.solo.store(false);

        // Track maximum length
        int64_t stemSamples = static_cast<int64_t>(stem.buffer->getNumSamples());
        if (stemSamples > totalSamples)
        {
            totalSamples = stemSamples;
            totalDurationSeconds = static_cast<double>(totalSamples) / sampleRateHz;
        }

        ++numStems;
    }

    return numStems > 0;
}

//==============================================================================

void StemPlayer::clearStems()
{
    playing.store(false);
    playheadSamples.store(0);

    for (int i = 0; i < kMaxStems; ++i)
    {
        stems[static_cast<size_t>(i)].buffer.reset();
        stems[static_cast<size_t>(i)].name.clear();
        stems[static_cast<size_t>(i)].gain.store(1.0f);
        stems[static_cast<size_t>(i)].muted.store(false);
        stems[static_cast<size_t>(i)].solo.store(false);
    }

    numStems = 0;
    totalSamples = 0;
    totalDurationSeconds = 0.0;
}

//==============================================================================

void StemPlayer::processBlock(juce::AudioBuffer<float>& outputBuffer)
{
    if (!playing.load() || numStems == 0)
    {
        outputBuffer.clear();
        return;
    }

    const int numSamples = outputBuffer.getNumSamples();
    const int numChannels = outputBuffer.getNumChannels();
    int64_t currentPos = playheadSamples.load();

    // Check if we've reached the end
    if (currentPos >= totalSamples)
    {
        outputBuffer.clear();
        playing.store(false);
        return;
    }

    // Clear output buffer before accumulating
    outputBuffer.clear();

    // Check if any stem is soloed
    bool soloActive = hasSoloActive();

    // Mix stems
    for (int s = 0; s < numStems; ++s)
    {
        const auto& stem = stems[static_cast<size_t>(s)];
        if (stem.buffer == nullptr)
            continue;

        // Skip muted stems
        if (stem.muted.load())
            continue;

        // If solo is active, only play soloed stems
        if (soloActive && !stem.solo.load())
            continue;

        float gain = stem.gain.load();
        const auto& srcBuffer = *stem.buffer;
        int srcChannels = srcBuffer.getNumChannels();
        int srcSamples = srcBuffer.getNumSamples();

        // Calculate how many samples we can read
        int64_t remaining = static_cast<int64_t>(srcSamples) - currentPos;
        int samplesToRead = static_cast<int>(juce::jmin(static_cast<int64_t>(numSamples), remaining));

        if (samplesToRead <= 0)
            continue;

        // Add to output buffer with gain
        for (int ch = 0; ch < numChannels; ++ch)
        {
            int srcCh = ch % srcChannels;  // Handle mono->stereo
            outputBuffer.addFrom(
                ch, 0,
                srcBuffer, srcCh, static_cast<int>(currentPos),
                samplesToRead, gain
            );
        }
    }

    // Advance playhead
    playheadSamples.store(currentPos + numSamples);
}

//==============================================================================

void StemPlayer::stop()
{
    playing.store(false);
    playheadSamples.store(0);
}

void StemPlayer::setPosition(double normalizedPosition)
{
    normalizedPosition = juce::jlimit(0.0, 1.0, normalizedPosition);
    int64_t newPos = static_cast<int64_t>(normalizedPosition * static_cast<double>(totalSamples));
    playheadSamples.store(newPos);
}

double StemPlayer::getPosition() const
{
    if (totalSamples == 0)
        return 0.0;
    return static_cast<double>(playheadSamples.load()) / static_cast<double>(totalSamples);
}

//==============================================================================

void StemPlayer::setStemGain(int stemIndex, float gain)
{
    if (stemIndex >= 0 && stemIndex < numStems)
        stems[static_cast<size_t>(stemIndex)].gain.store(juce::jmax(0.0f, gain));
}

void StemPlayer::setStemMuted(int stemIndex, bool muted)
{
    if (stemIndex >= 0 && stemIndex < numStems)
        stems[static_cast<size_t>(stemIndex)].muted.store(muted);
}

void StemPlayer::setStemSolo(int stemIndex, bool solo)
{
    if (stemIndex >= 0 && stemIndex < numStems)
        stems[static_cast<size_t>(stemIndex)].solo.store(solo);
}

float StemPlayer::getStemGain(int stemIndex) const
{
    if (stemIndex >= 0 && stemIndex < numStems)
        return stems[static_cast<size_t>(stemIndex)].gain.load();
    return 0.0f;
}

bool StemPlayer::isStemMuted(int stemIndex) const
{
    if (stemIndex >= 0 && stemIndex < numStems)
        return stems[static_cast<size_t>(stemIndex)].muted.load();
    return false;
}

bool StemPlayer::isStemSolo(int stemIndex) const
{
    if (stemIndex >= 0 && stemIndex < numStems)
        return stems[static_cast<size_t>(stemIndex)].solo.load();
    return false;
}

juce::String StemPlayer::getStemName(int index) const
{
    if (index >= 0 && index < numStems)
        return stems[static_cast<size_t>(index)].name;
    return {};
}

//==============================================================================

bool StemPlayer::hasSoloActive() const
{
    for (int i = 0; i < numStems; ++i)
    {
        if (stems[static_cast<size_t>(i)].solo.load())
            return true;
    }
    return false;
}

} // namespace audio
