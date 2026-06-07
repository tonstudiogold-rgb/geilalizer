#include "RenderEngine.h"

#include "AudioConstants.h"

#include <algorithm>

namespace geilalizer::core
{
namespace
{
bool isSupportedSampleRate(double sampleRate)
{
    return std::any_of(kSupportedExportSampleRates.begin(), kSupportedExportSampleRates.end(),
                       [sampleRate](int supported) { return static_cast<double>(supported) == sampleRate; });
}

bool isSupportedBitDepth(int bitDepth)
{
    return std::any_of(kSupportedExportBitDepths.begin(), kSupportedExportBitDepths.end(),
                       [bitDepth](int supported) { return supported == bitDepth; });
}
} // namespace

RenderResult RenderEngine::render(const SessionState& session,
                                  const std::vector<std::vector<float>>& monoChannelInputs,
                                  const RenderRequest& request)
{
    RenderResult result;

    if (!isSupportedSampleRate(request.sampleRate))
    {
        result.message = "Unsupported export sample rate. Use 44.1 kHz or 48 kHz.";
        return result;
    }

    if (!isSupportedBitDepth(request.bitDepth))
    {
        result.message = "Unsupported export bit depth. Use 16-bit or 24-bit.";
        return result;
    }

    const std::size_t availableFrames = monoChannelInputs.empty()
        ? 0
        : std::max_element(monoChannelInputs.begin(), monoChannelInputs.end(),
                           [](const auto& lhs, const auto& rhs) { return lhs.size() < rhs.size(); })->size();

    const std::size_t start = request.fullLength ? 0 : std::min(request.startFrame, availableFrames);
    const std::size_t wanted = request.fullLength ? availableFrames : request.numFrames;
    const std::size_t frames = std::min(wanted, availableFrames - start);

    std::vector<std::vector<float>> slicedInputs;
    slicedInputs.reserve(monoChannelInputs.size());
    for (const auto& channel : monoChannelInputs)
    {
        if (start >= channel.size())
        {
            slicedInputs.emplace_back();
            continue;
        }
        const std::size_t count = std::min(frames, channel.size() - start);
        slicedInputs.emplace_back(channel.begin() + static_cast<std::ptrdiff_t>(start),
                                  channel.begin() + static_cast<std::ptrdiff_t>(start + count));
    }

    processor_.prepare(request.sampleRate, static_cast<int>(frames));
    dsp::AudioBlockView block;
    block.monoInputs = &slicedInputs;
    block.stereoOutput = &result.interleavedStereo;
    block.numFrames = frames;
    processor_.process(session, block);

    result.completed = true;
    result.message = "Rendered through shared MachineProcessor path.";
    return result;
}

} // namespace geilalizer::core
