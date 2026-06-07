#pragma once

#include "SessionState.h"
#include "../dsp/MachineProcessor.h"

#include <cstddef>
#include <string>
#include <vector>

namespace geilalizer::core
{

struct RenderRequest
{
    double sampleRate = 44100.0;
    int bitDepth = 24;
    std::size_t startFrame = 0;
    std::size_t numFrames = 0;
    bool fullLength = true;
};

struct RenderResult
{
    bool completed = false;
    std::string message;
    std::vector<float> interleavedStereo;
};

class RenderEngine
{
public:
    RenderResult render(const SessionState& session,
                        const std::vector<std::vector<float>>& monoChannelInputs,
                        const RenderRequest& request);

private:
    dsp::MachineProcessor processor_;
};

} // namespace geilalizer::core
