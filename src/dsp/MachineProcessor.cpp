#include "MachineProcessor.h"

#include <algorithm>
#include <cmath>

namespace geilalizer::dsp
{
namespace
{
float dbToLinear(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

void updatePeak(float& meter, float value)
{
    meter = std::max(meter, std::abs(value));
}
} // namespace

void MachineProcessor::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max(0, maxBlockSize);
    for (auto& scratch : channelScratch_)
        scratch.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    namAdapter_.prepare(sampleRate_, maxBlockSize_);
    irAdapter_.prepare(sampleRate_, maxBlockSize_);
    irAdapter_.loadNeve1073DirectoryIfPresent();
    reset();
}

void MachineProcessor::reset()
{
    limiter_.reset();
    namAdapter_.reset();
    irAdapter_.reset();
    lastMeters_ = {};
}

void MachineProcessor::process(const core::SessionState& session, AudioBlockView block)
{
    ensureStereoOutput(block);
    lastMeters_ = {};
    if (block.stereoOutput == nullptr || block.numFrames == 0)
        return;

    const auto& inputs = block.monoInputs;
    auto& out = *block.stereoOutput;

    for (std::size_t channelIndex = 0; channelIndex < core::kMaxMonoChannels; ++channelIndex)
    {
        if (inputs == nullptr || channelIndex >= inputs->size())
            break;

        const auto& input = (*inputs)[channelIndex];
        if (input.empty())
            continue;

        const auto& channel = session.channels[channelIndex];
        const float inGain = dbToLinear(channel.inputGainDb);
        const float fader = dbToLinear(channel.faderGainDb);
        const float pan = std::clamp(channel.pan, -1.0f, 1.0f);
        const float leftPan = std::sqrt(0.5f * (1.0f - pan));
        const float rightPan = std::sqrt(0.5f * (1.0f + pan));

        const std::size_t frames = std::min(block.numFrames, input.size());
        auto& scratch = channelScratch_[channelIndex];
        if (scratch.size() < frames)
            scratch.resize(frames, 0.0f); // Non-realtime safety for tests/offline; prepare() prevents this in realtime.

        for (std::size_t frame = 0; frame < frames; ++frame)
            scratch[frame] = input[frame] * inGain;

        namAdapter_.processChannel(channelIndex, scratch.data(), frames);
        irAdapter_.processChannel(channelIndex, channel.preampIndex, scratch.data(), frames);

        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const float mono = scratch[frame] * fader;
            out[frame * 2] += mono * leftPan;
            out[frame * 2 + 1] += mono * rightPan;
        }
    }

    const float outputTrim = dbToLinear(session.master.outputTrimDb);
    for (float& sample : out)
        sample *= outputTrim;

    limiter_.setEnabled(session.master.limiterEnabled);
    limiter_.processInterleavedStereo(out.data(), static_cast<int>(block.numFrames));

    for (std::size_t frame = 0; frame < block.numFrames; ++frame)
    {
        updatePeak(lastMeters_.masterLeftPeak, out[frame * 2]);
        updatePeak(lastMeters_.masterRightPeak, out[frame * 2 + 1]);
    }
    lastMeters_.limiterActive = limiter_.active();

    // Meters live in SessionState; UI/controller can copy these peaks into mutable state.
    (void) sampleRate_;
    (void) maxBlockSize_;
}

} // namespace geilalizer::dsp
