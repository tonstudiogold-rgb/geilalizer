#pragma once

#include "../core/AudioConstants.h"

#include <algorithm>
#include <cmath>

namespace geilalizer::dsp
{

class SafetyLimiter
{
public:
    void reset()
    {
        active_ = false;
        lastGainReduction_ = 0.0f;
    }

    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }
    bool active() const { return active_; }
    float lastGainReduction() const { return lastGainReduction_; }

    void processInterleavedStereo(float* samples, int numFrames)
    {
        active_ = false;
        lastGainReduction_ = 0.0f;

        if (samples == nullptr || numFrames <= 0)
            return;

        const float ceiling = dbToLinear(core::kSafetyLimiterCeilingDb);
        const float threshold = dbToLinear(core::kSafetyLimiterThresholdDb);

        for (int frame = 0; frame < numFrames; ++frame)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float& sample = samples[frame * 2 + ch];
                const float absSample = std::abs(sample);
                if (enabled_ && absSample > threshold)
                {
                    const float limited = std::clamp(sample, -ceiling, ceiling);
                    lastGainReduction_ = std::max(lastGainReduction_, absSample - std::abs(limited));
                    sample = limited;
                    active_ = true;
                }
                else if (enabled_ && absSample > ceiling)
                {
                    // Ceiling guard even below the 0 dBFS threshold branch cannot normally happen,
                    // but this keeps the public rule explicit in one place.
                    sample = std::clamp(sample, -ceiling, ceiling);
                    active_ = true;
                }
            }
        }
    }

    static float dbToLinear(float db)
    {
        return std::pow(10.0f, db / 20.0f);
    }

private:
    bool enabled_ = true;
    bool active_ = false;
    float lastGainReduction_ = 0.0f;
};

} // namespace geilalizer::dsp
