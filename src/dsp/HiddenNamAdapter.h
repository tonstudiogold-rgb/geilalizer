#pragma once

#include "../core/AudioConstants.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace geilalizer::dsp
{

class HiddenNamAdapter
{
public:
    struct Request
    {
        std::string modelIdentifier;
        std::string reason;
    };

    enum class Stage
    {
        PlaceholderPassThrough,
        IntegratedNamModel
    };

    struct ChannelSlot
    {
        Stage stage = Stage::PlaceholderPassThrough;
        std::string modelIdentifier;
        bool prepared = false;
    };

    void prepare(double sampleRate, int maxBlockSize)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
        maxBlockSize_ = std::max(0, maxBlockSize);
        for (auto& slot : channelSlots_)
            slot.prepared = true;
    }

    void reset()
    {
        // Do not unbind integrated hidden models on transport reset. Actual NAM state reset belongs here
        // once a real model instance is wired. Model binding is product/development state, not playback state.
    }

    bool hasAnyModel() const
    {
        return std::any_of(channelSlots_.begin(), channelSlots_.end(),
                           [](const ChannelSlot& slot) { return slot.stage == Stage::IntegratedNamModel; });
    }

    bool hasModelForChannel(std::size_t channelIndex) const
    {
        return channelIndex < channelSlots_.size()
            && channelSlots_[channelIndex].stage == Stage::IntegratedNamModel;
    }

    const ChannelSlot& slot(std::size_t channelIndex) const
    {
        return channelSlots_.at(channelIndex);
    }

    // Hidden integration point only. NAM is not user visible and has no user load/select API.
    // Development rule: request and integrate exactly one concrete NAM model at a time.
    // This binds one internal model to one mono channel slot; bulk model loading is intentionally absent.
    bool bindSingleInternalModelForNextIntegration(std::size_t channelIndex, const Request& request)
    {
        if (channelIndex >= channelSlots_.size() || request.modelIdentifier.empty())
            return false;

        auto& target = channelSlots_[channelIndex];
        target.stage = Stage::IntegratedNamModel;
        target.modelIdentifier = request.modelIdentifier;
        target.prepared = maxBlockSize_ > 0;
        lastIntegrationReason_ = request.reason;
        return true;
    }

    const std::string& lastIntegrationReason() const { return lastIntegrationReason_; }

    void processChannel(std::size_t channelIndex, float* samples, std::size_t numFrames)
    {
        if (samples == nullptr || numFrames == 0 || channelIndex >= channelSlots_.size())
            return;

        const auto& slotToUse = channelSlots_[channelIndex];
        switch (slotToUse.stage)
        {
            case Stage::PlaceholderPassThrough:
                return;

            case Stage::IntegratedNamModel:
                processIntegratedNamPlaceholder(slotToUse, samples, numFrames);
                return;
        }
    }

private:
    static void processIntegratedNamPlaceholder(const ChannelSlot& slotToUse,
                                                float* samples,
                                                std::size_t numFrames)
    {
        // Stable adapter contract for the later real NAM call:
        // - Input gain has already been applied before this point.
        // - Fader/pan have NOT been applied yet.
        // - No allocation, file IO, model loading, prepare, or reset is allowed here.
        // - The real implementation replaces only this internal call path; UI remains unaware.
        (void) slotToUse;
        (void) samples;
        (void) numFrames;
    }

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 0;
    std::array<ChannelSlot, core::kMaxMonoChannels> channelSlots_;
    std::string lastIntegrationReason_;
};

} // namespace geilalizer::dsp
