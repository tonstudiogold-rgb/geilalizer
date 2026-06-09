#pragma once

#include "AudioBlock.h"
#include "HiddenIrAdapter.h"
#include "HiddenMixbusIrAdapter.h"
#include "HiddenNamAdapter.h"
#include "HiddenPostFaderIrAdapter.h"
#include "SafetyLimiter.h"
#include "../core/SessionState.h"

#include <array>
#include <cstddef>
#include <string>
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
        std::array<float, core::kMaxMonoChannels> channelPeaks {};
    };

    struct StageProbe
    {
        std::string name;
        std::size_t frames = 0;
        float beforePeak = 0.0f;
        float afterPeak = 0.0f;
        float deltaRms = 0.0f;

        bool changed(float threshold = 1.0e-6f) const { return deltaRms > threshold; }
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
    void setStageProbeEnabled(bool enabled) { stageProbeEnabled_ = enabled; }
    bool stageProbeEnabled() const { return stageProbeEnabled_; }
    const std::vector<StageProbe>& lastStageProbes() const { return lastStageProbes_; }

private:
    void processStereoNam(HiddenNamAdapter& adapter, std::vector<float>& interleavedStereo, std::size_t numFrames);
    void clearStageProbes();
    void recordMonoStageProbe(const std::string& name, const std::vector<float>& before,
                              const float* after, std::size_t frames);
    void recordStereoStageProbe(const std::string& name, const std::vector<float>& beforeInterleaved,
                                const std::vector<float>& afterInterleaved, std::size_t frames);

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
    std::vector<float> stageProbeScratch_;
    bool stageProbeEnabled_ = false;
    std::vector<StageProbe> lastStageProbes_;
    ProcessMeters lastMeters_;
};

} // namespace geilalizer::dsp
