#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace geilalizer::dsp
{

struct AudioBlockView
{
    const std::vector<std::vector<float>>* monoInputs = nullptr; // one vector per mono channel
    std::vector<float>* stereoOutput = nullptr;     // interleaved L/R frames
    std::size_t numFrames = 0;
};

inline void ensureStereoOutput(AudioBlockView& block)
{
    if (block.stereoOutput == nullptr)
        return;

    const auto requiredSamples = block.numFrames * 2;
    if (block.stereoOutput->size() < requiredSamples)
        block.stereoOutput->resize(requiredSamples, 0.0f); // Non-realtime fallback; realtime caller should preallocate.

    std::fill(block.stereoOutput->begin(), block.stereoOutput->begin() + static_cast<std::ptrdiff_t>(requiredSamples), 0.0f);
}

} // namespace geilalizer::dsp
