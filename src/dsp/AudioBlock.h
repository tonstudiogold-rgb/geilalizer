#pragma once

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
    if (block.stereoOutput != nullptr)
        block.stereoOutput->assign(block.numFrames * 2, 0.0f);
}

} // namespace geilalizer::dsp
