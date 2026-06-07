#pragma once

#include "AudioBlock.h"
#include "HiddenNamAdapter.h"
#include "SafetyLimiter.h"
#include "../core/SessionState.h"

namespace geilalizer::dsp
{

class MachineProcessor
{
public:
    struct ProcessMeters
    {
        float masterLeftPeak = 0.0f;
        float masterRightPeak = 0.0f;
        bool limiterActive = false;
    };

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Shared processing path for realtime playback and offline render.
    void process(const core::SessionState& session, AudioBlockView block);

    SafetyLimiter& safetyLimiter() { return limiter_; }
    const SafetyLimiter& safetyLimiter() const { return limiter_; }
    HiddenNamAdapter& namAdapter() { return namAdapter_; }
    const ProcessMeters& lastMeters() const { return lastMeters_; }

private:
    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 0;
    SafetyLimiter limiter_;
    HiddenNamAdapter namAdapter_;
    ProcessMeters lastMeters_;
};

} // namespace geilalizer::dsp
