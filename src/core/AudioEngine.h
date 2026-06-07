#pragma once

#include "SessionState.h"
#include "../dsp/MachineProcessor.h"

#include <vector>

namespace geilalizer::core
{

class AudioEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    SessionState& session() { return session_; }
    const SessionState& session() const { return session_; }

    // Realtime callback entry. Uses the same MachineProcessor path as RenderEngine.
    void processRealtime(const std::vector<std::vector<float>>& monoChannelInputs,
                         std::size_t numFrames,
                         std::vector<float>& interleavedStereoOutput);

private:
    SessionState session_;
    dsp::MachineProcessor processor_;
};

} // namespace geilalizer::core
