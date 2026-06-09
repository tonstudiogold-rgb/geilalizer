#pragma once

#include "AudioConstants.h"

#include <array>
#include <cstddef>
#include <optional>

namespace geilalizer::core
{

inline bool canImportAtChannel(const std::array<bool, kMaxMonoChannels>& occupied,
                               std::size_t firstChannelIndex,
                               std::size_t channelCount) noexcept
{
    if (channelCount == 0 || channelCount > 2)
        return false;

    if (firstChannelIndex >= occupied.size())
        return false;

    if (firstChannelIndex + channelCount > occupied.size())
        return false;

    for (std::size_t offset = 0; offset < channelCount; ++offset)
    {
        if (occupied[firstChannelIndex + offset])
            return false;
    }

    return true;
}

inline std::optional<std::size_t> findFirstContiguousFreeChannels(const std::array<bool, kMaxMonoChannels>& occupied,
                                                                  std::size_t channelCount) noexcept
{
    if (channelCount == 0 || channelCount > 2 || channelCount > occupied.size())
        return std::nullopt;

    for (std::size_t i = 0; i + channelCount <= occupied.size(); ++i)
    {
        if (canImportAtChannel(occupied, i, channelCount))
            return i;
    }

    return std::nullopt;
}

} // namespace geilalizer::core
