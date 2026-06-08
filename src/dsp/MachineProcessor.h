#pragma once

#include "AudioBlock.h"
#include "HiddenIrAdapter.h"
#include "HiddenMixbusIrAdapter.h"
#include "HiddenNamAdapter.h"
#include "HiddenPostFaderIrAdapter.h"
#include "SafetyLimiter.h"
#include "../core/SessionState.h"

#include <array>
#include <vector>

namespace geilalizer::dsp
{

class MachineProcessor
{
public:
    struct ProcessMeters
    {
        float masterLeftPeak = 0.0f;
        float masterRightPeak = 0.0f;
        float mixbusPreLimiterPeakDb = -120.0f;
        int mixbusIrSlotIndex = 0;
        bool mixbusPreLimiterActive = false;
        bool postEmtSafetyLimiterActive = false;
        bool finalSafetyLimiterActive = false;
        bool limiterActive = false;
    };

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    // Shared processing path for realtime playback and offline render.
    void process(const core::SessionState& session, AudioBlockView block);

    SafetyLimiter& safetyLimiter() { return limiter_; }
    const SafetyLimiter& safetyLimiter() const { return limiter_; }
    HiddenNamAdapter& namAdapter() { return namAdapter_; }
    HiddenNamAdapter& consoleNamAdapter() { return namAdapter_; }
    HiddenNamAdapter& tapeNamAdapter() { return tapeNamAdapter_; }
    HiddenNamAdapter& masterTapeNamAdapter() { return masterTapeNamAdapter_; }
    HiddenNamAdapter& emtLimiterNamAdapter() { return emtLimiterNamAdapter_; }
    HiddenNamAdapter& finalHiloNamAdapter() { return finalHiloNamAdapter_; }
    HiddenIrAdapter& irAdapter() { return irAdapter_; }
    HiddenPostFaderIrAdapter& postFaderIrAdapter() { return postFaderIrAdapter_; }
    HiddenMixbusIrAdapter& mixbusIrAdapter() { return mixbusIrAdapter_; }
    const ProcessMeters& lastMeters() const { return lastMeters_; }

private:
    void processStereoNam(HiddenNamAdapter& adapter, std::vector<float>& interleavedStereo, std::size_t numFrames);

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 0;
    SafetyLimiter limiter_;
    SafetyLimiter postEmtSafetyLimiter_;
    HiddenNamAdapter namAdapter_;
    HiddenNamAdapter tapeNamAdapter_;
    HiddenNamAdapter masterTapeNamAdapter_;
    HiddenNamAdapter emtLimiterNamAdapter_;
    HiddenNamAdapter finalHiloNamAdapter_;
    HiddenIrAdapter irAdapter_;
    HiddenPostFaderIrAdapter postFaderIrAdapter_;
    HiddenMixbusIrAdapter mixbusIrAdapter_;
    SafetyLimiter mixbusPreIrLimiter_;
    std::array<std::vector<float>, core::kMaxMonoChannels> channelScratch_;
    std::vector<float> masterLeftScratch_;
    std::vector<float> masterRightScratch_;
    ProcessMeters lastMeters_;
};

} // namespace geilalizer::dsp
