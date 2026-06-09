#pragma once

#include <algorithm>
#include <cmath>

namespace geilalizer::core
{

class LevelMeter
{
public:
    void configure(float decayPerTick, float peakHoldDecayPerTick)
    {
        decayPerTick_ = sanitiseStep(decayPerTick);
        holdDecayPerTick_ = sanitiseStep(peakHoldDecayPerTick);
    }

    void pushPeak(float peak)
    {
        const float cleanPeak = sanitisePeak(peak);
        if (peak >= 1.0f)
            clipped_ = true;

        displayPeak_ = std::max(displayPeak_, cleanPeak);
        holdPeak_ = std::max(holdPeak_, cleanPeak);
    }

    void decay()
    {
        displayPeak_ = std::max(0.0f, displayPeak_ - decayPerTick_);
        holdPeak_ = std::max(displayPeak_, holdPeak_ - holdDecayPerTick_);
    }

    void resetClip() { clipped_ = false; }
    void reset()
    {
        displayPeak_ = 0.0f;
        holdPeak_ = 0.0f;
        clipped_ = false;
    }

    float displayPeak() const { return displayPeak_; }
    float holdPeak() const { return holdPeak_; }
    bool clipped() const { return clipped_; }

    float displayDb() const
    {
        if (displayPeak_ <= 0.0f || !std::isfinite(displayPeak_))
            return -120.0f;
        return std::clamp(20.0f * std::log10(displayPeak_), -120.0f, 0.0f);
    }

private:
    static float sanitisePeak(float peak)
    {
        if (!std::isfinite(peak) || peak < 0.0f)
            return 0.0f;
        return std::min(peak, 1.0f);
    }

    static float sanitiseStep(float value)
    {
        if (!std::isfinite(value) || value < 0.0f)
            return 0.0f;
        return std::min(value, 1.0f);
    }

    float decayPerTick_ = 0.08f;
    float holdDecayPerTick_ = 0.02f;
    float displayPeak_ = 0.0f;
    float holdPeak_ = 0.0f;
    bool clipped_ = false;
};

} // namespace geilalizer::core
