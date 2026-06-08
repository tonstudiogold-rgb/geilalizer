#pragma once

#include "AudioConstants.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace geilalizer::core
{

struct ChannelState
{
    std::size_t index = 0;
    std::string name = "1";
    bool armed = false;
    bool faderVisible = true;
    float inputGainDb = kDefaultInputGainDb;
    int preampIndex = 5; // 0..10 stepped hidden 1073 IR selector; default = Neve 55.wav.
    float faderGainDb = kDefaultFaderGainDb;
    float pan = kDefaultPan; // -1.0 = left, 0.0 = center, +1.0 = right.
    float meterPeak = 0.0f;
    bool hasAudioFile = false;
    std::string sourcePath;
    int sourceChannelIndex = 0;
    std::size_t lengthFrames = 0;

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

    void setPreampIndex(int value)
    {
        preampIndex = std::clamp(value, 0, 10);
    }

    void setFaderGainDb(float value)
    {
        faderGainDb = std::clamp(value, kFaderGainMinDb, kFaderGainMaxDb);
    }

    void setPan(float value)
    {
        pan = std::clamp(value, -1.0f, 1.0f);
    }

    void setLoadedAudioFile(std::string path, int sourceChannel, std::size_t frames)
    {
        hasAudioFile = true;
        sourcePath = std::move(path);
        sourceChannelIndex = std::max(0, sourceChannel);
        lengthFrames = frames;
    }

    void clearLoadedAudioFile()
    {
        hasAudioFile = false;
        sourcePath.clear();
        sourceChannelIndex = 0;
        lengthFrames = 0;
        meterPeak = 0.0f;
    }
};

} // namespace geilalizer::core
