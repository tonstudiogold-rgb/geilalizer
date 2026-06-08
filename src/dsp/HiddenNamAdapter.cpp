#include "HiddenNamAdapter.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#ifdef GEILALIZER_USE_NAMCORE
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#endif

namespace geilalizer::dsp
{

HiddenNamAdapter::HiddenNamAdapter() = default;
HiddenNamAdapter::~HiddenNamAdapter() = default;
HiddenNamAdapter::HiddenNamAdapter(HiddenNamAdapter&&) noexcept = default;
HiddenNamAdapter& HiddenNamAdapter::operator=(HiddenNamAdapter&&) noexcept = default;

void HiddenNamAdapter::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max(0, maxBlockSize);
    for (std::size_t channelIndex = 0; channelIndex < channelSlots_.size(); ++channelIndex)
    {
        auto& slot = channelSlots_[channelIndex];
        slot.prepared = true;
#ifdef GEILALIZER_USE_NAMCORE
        if (channelDsps_[channelIndex] != nullptr)
            channelDsps_[channelIndex]->Reset(sampleRate_, maxBlockSize_);
#endif
    }
}

void HiddenNamAdapter::reset()
{
    // Do not unbind integrated hidden models on transport reset. Model binding is product/development
    // state, not playback state. Real NAM state is reset here so playback/render starts repeatably.
#ifdef GEILALIZER_USE_NAMCORE
    for (auto& dsp : channelDsps_)
    {
        if (dsp != nullptr)
            dsp->Reset(sampleRate_, maxBlockSize_);
    }
#endif
}

bool HiddenNamAdapter::hasAnyModel() const
{
    return std::any_of(channelSlots_.begin(), channelSlots_.end(),
                       [](const ChannelSlot& slot) { return slot.stage == Stage::IntegratedNamModel; });
}

bool HiddenNamAdapter::hasModelForChannel(std::size_t channelIndex) const
{
    return channelIndex < channelSlots_.size()
        && channelSlots_[channelIndex].stage == Stage::IntegratedNamModel;
}

bool HiddenNamAdapter::hasRealDspForChannel(std::size_t channelIndex) const
{
    if (channelIndex >= channelSlots_.size())
        return false;
#ifdef GEILALIZER_USE_NAMCORE
    return channelDsps_[channelIndex] != nullptr && channelSlots_[channelIndex].realDspLoaded;
#else
    return false;
#endif
}

const HiddenNamAdapter::ChannelSlot& HiddenNamAdapter::slot(std::size_t channelIndex) const
{
    return channelSlots_.at(channelIndex);
}

bool HiddenNamAdapter::bindSingleInternalModelForNextIntegration(std::size_t channelIndex, const Request& request)
{
    if (!bindModelForChannel(channelIndex, request))
        return false;
    lastIntegrationReason_ = request.reason;
    return true;
}

bool HiddenNamAdapter::bindFixedInternalModelForAllChannels(const Request& request)
{
    if (request.modelIdentifier.empty())
        return false;

    bool allBound = true;
    for (std::size_t channelIndex = 0; channelIndex < channelSlots_.size(); ++channelIndex)
        allBound = bindModelForChannel(channelIndex, request) && allBound;

    lastIntegrationReason_ = request.reason;
    return allBound;
}

void HiddenNamAdapter::processChannel(std::size_t channelIndex, float* samples, std::size_t numFrames)
{
    if (samples == nullptr || numFrames == 0 || channelIndex >= channelSlots_.size())
        return;

    const auto& slotToUse = channelSlots_[channelIndex];
    switch (slotToUse.stage)
    {
        case Stage::PlaceholderPassThrough:
            return;

        case Stage::IntegratedNamModel:
            processIntegratedNam(slotToUse, channelIndex, samples, numFrames);
            return;
    }
}

bool HiddenNamAdapter::bindModelForChannel(std::size_t channelIndex, const Request& request)
{
    if (channelIndex >= channelSlots_.size() || request.modelIdentifier.empty())
        return false;

    auto& target = channelSlots_[channelIndex];
    target.stage = Stage::IntegratedNamModel;
    target.modelIdentifier = request.modelIdentifier;
    target.prepared = maxBlockSize_ > 0;
    target.realDspLoaded = loadRealDspForChannel(channelIndex, request.modelIdentifier);
    return true;
}

bool HiddenNamAdapter::loadRealDspForChannel(std::size_t channelIndex, const std::string& modelIdentifier)
{
    (void) channelIndex;
    (void) modelIdentifier;
#ifdef GEILALIZER_USE_NAMCORE
    if (const char* disableRealNam = std::getenv("GEILALIZER_DISABLE_REAL_NAMCORE");
        disableRealNam != nullptr && disableRealNam[0] != '\0')
    {
        channelDsps_[channelIndex].reset();
        return false;
    }

    try
    {
        const std::filesystem::path modelPath { modelIdentifier };
        if (!std::filesystem::exists(modelPath))
            return false;

        auto dsp = nam::get_dsp(modelPath);
        if (dsp == nullptr || dsp->NumInputChannels() != 1 || dsp->NumOutputChannels() != 1)
            return false;

        dsp->Reset(sampleRate_, maxBlockSize_);
        channelDsps_[channelIndex] = std::move(dsp);
        return true;
    }
    catch (...)
    {
        channelDsps_[channelIndex].reset();
        return false;
    }
#else
    return false;
#endif
}

void HiddenNamAdapter::processIntegratedNam(const ChannelSlot& slotToUse,
                                            std::size_t channelIndex,
                                            float* samples,
                                            std::size_t numFrames)
{
    // Stable adapter contract:
    // - Input gain and the selected per-channel IR/preamp stage have already been applied before this point.
    // - Fader/pan have NOT been applied yet.
    // - No allocation, file IO, model loading, prepare, or reset is allowed here.
    // - UI remains unaware: no user-facing NAM loading/selection exists.
    (void) slotToUse;
#ifdef GEILALIZER_USE_NAMCORE
    auto& dsp = channelDsps_[channelIndex];
    if (dsp == nullptr)
        return;

    float* inputChannels[] { samples };
    float* outputChannels[] { samples };
    dsp->process(inputChannels, outputChannels, static_cast<int>(numFrames));
#else
    (void) channelIndex;
    (void) samples;
    (void) numFrames;
#endif
}

} // namespace geilalizer::dsp
