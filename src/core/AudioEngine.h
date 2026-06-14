#pragma once

#include "SessionState.h"
#include "../dsp/MachineProcessor.h"

#include <array>
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

    struct RealtimeMeters
    {
        float masterLeftPeak = 0.0f;
        float masterRightPeak = 0.0f;
        bool limiterActive = false;
        std::array<float, kMaxMonoChannels> channelPeaks {};
    };

    // Realtime callback entry. Uses the same MachineProcessor path as RenderEngine.
    void processRealtime(const std::vector<std::vector<float>>& monoChannelInputs,
                         std::size_t numFrames,
                         std::vector<float>& interleavedStereoOutput);
    void processRealtime(const SessionState& sessionSnapshot,
                         const std::vector<std::vector<float>>& monoChannelInputs,
                         std::size_t numFrames,
                         std::vector<float>& interleavedStereoOutput,
                         RealtimeMeters& metersOut);

private:
    SessionState session_;
    dsp::MachineProcessor processor_;
};

} // namespace geilalizer::core
