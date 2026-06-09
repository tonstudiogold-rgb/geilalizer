#include "RenderEngine.h"

#include "AudioConstants.h"

#include <algorithm>
#include <cmath>

namespace geilalizer::core
{
namespace
{
bool isSupportedSampleRate(double sampleRate)
{
    return std::any_of(kSupportedExportSampleRates.begin(), kSupportedExportSampleRates.end(),
                       [sampleRate](int supported) { return std::abs(static_cast<double>(supported) - sampleRate) < 0.5; });
}

bool isSupportedBitDepth(int bitDepth)
{
    return std::any_of(kSupportedExportBitDepths.begin(), kSupportedExportBitDepths.end(),
                       [bitDepth](int supported) { return supported == bitDepth; });
}

std::size_t maxAvailableFrames(const std::vector<std::vector<float>>& monoChannelInputs)
{
    if (monoChannelInputs.empty())
        return 0;
    return std::max_element(monoChannelInputs.begin(), monoChannelInputs.end(),
                            [](const auto& lhs, const auto& rhs) { return lhs.size() < rhs.size(); })->size();
}
} // namespace

RenderResult RenderEngine::render(const SessionState& session,
                                  const std::vector<std::vector<float>>& monoChannelInputs,
                                  const RenderRequest& request)
{
    RenderResult result;
    auto collectRequest = request;
    const auto sink = [&result](const float* interleavedStereo, std::size_t frames)
    {
        if (interleavedStereo == nullptr && frames > 0)
            return false;
        result.interleavedStereo.insert(result.interleavedStereo.end(), interleavedStereo, interleavedStereo + frames * 2);
        return true;
    };

    auto streamed = renderToSink(session, monoChannelInputs, collectRequest, sink);
    streamed.interleavedStereo = std::move(result.interleavedStereo);
    return streamed;
}

RenderResult RenderEngine::renderToSink(const SessionState& session,
                                        const std::vector<std::vector<float>>& monoChannelInputs,
                                        const RenderRequest& request,
                                        const RenderSink& sink)
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

    if (sink == nullptr)
    {
        result.message = "Render sink is not available.";
        return result;
    }

    const std::size_t availableFrames = maxAvailableFrames(monoChannelInputs);
    const std::size_t start = request.fullLength ? 0 : std::min(request.startFrame, availableFrames);
    const std::size_t wanted = request.fullLength ? availableFrames : request.numFrames;
    const std::size_t frames = std::min(wanted, availableFrames - start);
    const std::size_t blockSize = std::max<std::size_t>(1, request.blockSize);

    processor_.prepare(request.sampleRate, static_cast<int>(std::min(blockSize, frames)));

    std::vector<std::vector<float>> chunkInputs;
    chunkInputs.resize(monoChannelInputs.size());
    std::vector<float> chunkOutput;

    for (std::size_t offset = 0; offset < frames; offset += blockSize)
    {
        const std::size_t chunkFrames = std::min(blockSize, frames - offset);
        for (std::size_t channelIndex = 0; channelIndex < monoChannelInputs.size(); ++channelIndex)
        {
            const auto& channel = monoChannelInputs[channelIndex];
            auto& chunk = chunkInputs[channelIndex];
            chunk.clear();
            const std::size_t sourceStart = start + offset;
            if (sourceStart >= channel.size())
                continue;
            const std::size_t count = std::min(chunkFrames, channel.size() - sourceStart);
            chunk.insert(chunk.end(),
                         channel.begin() + static_cast<std::ptrdiff_t>(sourceStart),
                         channel.begin() + static_cast<std::ptrdiff_t>(sourceStart + count));
        }

        dsp::AudioBlockView block;
        block.monoInputs = &chunkInputs;
        block.stereoOutput = &chunkOutput;
        block.numFrames = chunkFrames;
        processor_.process(session, block);

        if (!sink(chunkOutput.data(), chunkFrames))
        {
            result.message = "Render sink rejected audio block.";
            return result;
        }
    }

    result.completed = true;
    result.message = "Rendered through shared MachineProcessor path.";
    return result;
}

} // namespace geilalizer::core
