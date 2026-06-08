#include "core/AudioConstants.h"
#include "core/RenderEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int kFrames = 1024;

void require(bool condition, const char* message)
{
    if (! condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

void writeMonoWav(const juce::File& file, int channelIndex)
{
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    require(stream != nullptr && stream->openedOk(), "could not open mono WAV for writing");
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), kSampleRate, 1, 24, {}, 0));
    require(writer != nullptr, "could not create mono WAV writer");
    stream.release();

    juce::AudioBuffer<float> buffer(1, kFrames);
    const auto frequency = 110.0 + static_cast<double>(channelIndex) * 7.0;
    const auto gain = 0.02f + static_cast<float>(channelIndex) * 0.0005f;
    for (int frame = 0; frame < kFrames; ++frame)
    {
        const auto phase = 2.0 * juce::MathConstants<double>::pi * frequency * static_cast<double>(frame) / kSampleRate;
        buffer.setSample(0, frame, gain * static_cast<float>(std::sin(phase)));
    }
    require(writer->writeFromAudioSampleBuffer(buffer, 0, kFrames), "could not write mono WAV samples");
}

std::vector<float> readMonoWav(const juce::File& file)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    require(reader != nullptr, "could not create mono WAV reader");
    require(reader->numChannels == 1, "imported file is not mono");
    require(reader->lengthInSamples == kFrames, "imported mono file has wrong length");

    juce::AudioBuffer<float> buffer(1, kFrames);
    buffer.clear();
    require(reader->read(&buffer, 0, kFrames, 0, true, false), "could not read mono WAV samples");

    std::vector<float> mono(static_cast<std::size_t>(kFrames));
    const float* src = buffer.getReadPointer(0);
    std::copy(src, src + kFrames, mono.begin());
    return mono;
}

void writeStereoWav(const juce::File& file, const std::vector<float>& interleavedStereo)
{
    require(interleavedStereo.size() == static_cast<std::size_t>(kFrames) * 2, "rendered stereo buffer has wrong size");
    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream(file.createOutputStream());
    require(stream != nullptr && stream->openedOk(), "could not open stereo WAV for writing");
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), kSampleRate, 2, 24, {}, 0));
    require(writer != nullptr, "could not create stereo WAV writer");
    stream.release();

    juce::AudioBuffer<float> stereo(2, kFrames);
    for (int frame = 0; frame < kFrames; ++frame)
    {
        stereo.setSample(0, frame, interleavedStereo[static_cast<std::size_t>(frame) * 2]);
        stereo.setSample(1, frame, interleavedStereo[static_cast<std::size_t>(frame) * 2 + 1]);
    }
    require(writer->writeFromAudioSampleBuffer(stereo, 0, kFrames), "could not write stereo WAV samples");
}

float peakOfChannel(const juce::AudioBuffer<float>& buffer, int channel)
{
    float peak = 0.0f;
    for (int frame = 0; frame < buffer.getNumSamples(); ++frame)
        peak = std::max(peak, std::abs(buffer.getSample(channel, frame)));
    return peak;
}
} // namespace

int main()
{
    using namespace geilalizer;

    const auto tempRoot = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile("geilalizer-offline-render-slice");
    tempRoot.deleteRecursively();
    require(tempRoot.createDirectory(), "could not create temp dir");

    std::vector<std::vector<float>> monoInputs(core::kMaxMonoChannels);
    core::SessionState session;

    // Import up to 24 mono audio files into the 24 mono channels.
    for (std::size_t channelIndex = 0; channelIndex < core::kMaxMonoChannels; ++channelIndex)
    {
        const auto file = tempRoot.getChildFile("channel_" + juce::String(static_cast<int>(channelIndex + 1)).paddedLeft('0', 2) + ".wav");
        writeMonoWav(file, static_cast<int>(channelIndex));
        monoInputs[channelIndex] = readMonoWav(file);

        auto& channel = session.channel(channelIndex);
        channel.setLoadedAudioFile(file.getFullPathName().toStdString(), 0, monoInputs[channelIndex].size());
        channel.armed = true;
        channel.setInputGainDb(channelIndex % 3 == 0 ? 3.0f : 0.0f);
        channel.setPan(channelIndex < 12 ? -0.35f : 0.35f);
        require(channel.hasAudioFile, "channel did not mark imported audio as loaded");
        require(channel.lengthFrames == static_cast<std::size_t>(kFrames), "channel stored wrong import length");
    }

    session.master.outputTrimDb = 0.0f;
    session.master.limiterEnabled = true;

    // Core processing through the fixed chain, then offline stereo render.
    core::RenderEngine renderer;
    core::RenderRequest request;
    request.sampleRate = kSampleRate;
    request.bitDepth = 24;
    request.fullLength = true;
    const auto result = renderer.render(session, monoInputs, request);

    require(result.completed, "offline render did not complete");
    require(result.interleavedStereo.size() == static_cast<std::size_t>(kFrames) * 2, "offline render returned wrong length");
    for (float sample : result.interleavedStereo)
        require(std::isfinite(sample), "offline render produced non-finite sample");

    const auto output = tempRoot.getChildFile("geilalizer-offline-render.wav");
    writeStereoWav(output, result.interleavedStereo);
    require(output.existsAsFile(), "stereo render WAV was not created");
    require(output.getSize() > 44, "stereo render WAV is empty");

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(output));
    require(reader != nullptr, "could not read rendered stereo WAV");
    require(reader->numChannels == 2, "rendered WAV is not stereo");
    require(reader->lengthInSamples == kFrames, "rendered WAV has wrong length");

    juce::AudioBuffer<float> rendered(2, kFrames);
    rendered.clear();
    require(reader->read(&rendered, 0, kFrames, 0, true, true), "could not read rendered stereo samples");
    require(peakOfChannel(rendered, 0) > 0.0f, "rendered left channel is silent");
    require(peakOfChannel(rendered, 1) > 0.0f, "rendered right channel is silent");

    tempRoot.deleteRecursively();
    return 0;
}
