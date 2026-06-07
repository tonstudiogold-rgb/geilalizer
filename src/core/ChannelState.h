#pragma once

#include "AudioConstants.h"

#include <algorithm>
#include <cstddef>
#include <string>

namespace geilalizer::core
{

struct ChannelState
{
    std::size_t index = 0;
    std::string name = "1";
    bool armed = false;
    bool faderVisible = true;
    float inputGainDb = kDefaultInputGainDb;
    float faderGainDb = kDefaultFaderGainDb;
    float pan = kDefaultPan; // -1.0 = left, 0.0 = center, +1.0 = right.
    float meterPeak = 0.0f;

    static ChannelState createDefault(std::size_t channelIndex)
    {
        ChannelState state;
        state.index = channelIndex;
        state.name = std::to_string(channelIndex + 1); // Editable numeric default track name.
        return state;
    }

    void setInputGainDb(float value)
    {
        inputGainDb = std::clamp(value, kInputGainMinDb, kInputGainMaxDb);
    }

    void setPan(float value)
    {
        pan = std::clamp(value, -1.0f, 1.0f);
    }
};

} // namespace geilalizer::core
