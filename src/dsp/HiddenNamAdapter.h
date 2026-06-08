#pragma once

#include "../core/AudioConstants.h"

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#ifdef GEILALIZER_USE_NAMCORE
namespace nam
{
class DSP;
}
#endif

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
        bool realDspLoaded = false;
    };

    HiddenNamAdapter();
    ~HiddenNamAdapter();

    HiddenNamAdapter(const HiddenNamAdapter&) = delete;
    HiddenNamAdapter& operator=(const HiddenNamAdapter&) = delete;
    HiddenNamAdapter(HiddenNamAdapter&&) noexcept;
    HiddenNamAdapter& operator=(HiddenNamAdapter&&) noexcept;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    bool hasAnyModel() const;
    bool hasModelForChannel(std::size_t channelIndex) const;
    bool hasRealDspForChannel(std::size_t channelIndex) const;

    const ChannelSlot& slot(std::size_t channelIndex) const;

    // Hidden integration point only. NAM is not user visible and has no user load/select API.
    // Development rule: request and integrate exactly one concrete NAM model at a time.
    // This binds one internal model to one mono channel slot for focused experiments.
    bool bindSingleInternalModelForNextIntegration(std::size_t channelIndex, const Request& request);

    const std::string& lastIntegrationReason() const { return lastIntegrationReason_; }

    bool bindFixedInternalModelForAllChannels(const Request& request);

    void processChannel(std::size_t channelIndex, float* samples, std::size_t numFrames);

private:
    bool bindModelForChannel(std::size_t channelIndex, const Request& request);
    bool loadRealDspForChannel(std::size_t channelIndex, const std::string& modelIdentifier);
    void processIntegratedNam(const ChannelSlot& slotToUse,
                              std::size_t channelIndex,
                              float* samples,
                              std::size_t numFrames);

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 0;
    std::array<ChannelSlot, core::kMaxMonoChannels> channelSlots_;
    std::string lastIntegrationReason_;

#ifdef GEILALIZER_USE_NAMCORE
    std::array<std::unique_ptr<nam::DSP>, core::kMaxMonoChannels> channelDsps_;
#endif
};

} // namespace geilalizer::dsp
