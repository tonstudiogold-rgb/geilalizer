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
    reset();
}

void MachineProcessor::reset()
{
    limiter_.reset();
    namAdapter_.reset();
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
        const float leftPan = std::sqrt(0.5f * (1.0f - channel.pan));
        const float rightPan = std::sqrt(0.5f * (1.0f + channel.pan));

        const std::size_t frames = std::min(block.numFrames, input.size());
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            float mono = input[frame] * inGain * fader;
            namAdapter_.processMono(&mono, 1);
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
