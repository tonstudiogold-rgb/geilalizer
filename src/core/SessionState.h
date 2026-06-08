#pragma once

#include "AudioConstants.h"
#include "ChannelState.h"

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace geilalizer::core
{

struct MonoImportSlot
{
    std::size_t channelIndex = 0;
    std::string sourcePath;
    int sourceChannelIndex = 0;
};

struct StereoSplitImportResult
{
    bool accepted = false;
    std::array<std::optional<MonoImportSlot>, 2> monoSlots;
    std::string message;
};

struct MasterState
{
    // User-facing limiter switch controls only the hidden EMT 266X NAM limiter stage.
    // Product safety limiters remain always-on protection stages.
    bool limiterEnabled = kDefaultLimiterEnabled;
    float outputTrimDb = kDefaultOutputTrimDb;
    float meterLeftPeak = 0.0f;
    float meterRightPeak = 0.0f;
    bool limiterActive = false;
};

struct SessionState
{
    std::array<ChannelState, kMaxMonoChannels> channels;
    MasterState master;

    SessionState()
    {
        for (std::size_t i = 0; i < channels.size(); ++i)
            channels[i] = ChannelState::createDefault(i);
    }

    ChannelState& channel(std::size_t index) { return channels.at(index); }
    const ChannelState& channel(std::size_t index) const { return channels.at(index); }

    // API placeholder: stereo files are never stored as stereo tracks. They reserve/split into
    // two mono channels. Actual decoding/resampling is intentionally implemented later.
    StereoSplitImportResult reserveStereoImportSplit(const std::string& sourcePath,
                                                     std::size_t firstMonoChannelIndex)
    {
        StereoSplitImportResult result;
        if (firstMonoChannelIndex + 1 >= kMaxMonoChannels)
        {
            result.message = "Stereo import needs two available mono channels.";
            return result;
        }

        result.accepted = true;
        result.monoSlots[0] = MonoImportSlot { firstMonoChannelIndex, sourcePath, 0 };
        result.monoSlots[1] = MonoImportSlot { firstMonoChannelIndex + 1, sourcePath, 1 };
        result.message = "Stereo import reserved as two mono channels.";
        return result;
    }
};

} // namespace geilalizer::core
