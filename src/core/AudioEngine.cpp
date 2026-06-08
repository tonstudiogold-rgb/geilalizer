#include "AudioEngine.h"

namespace geilalizer::core
{

void AudioEngine::prepare(double sampleRate, int maxBlockSize)
{
    processor_.prepare(sampleRate, maxBlockSize);
}

void AudioEngine::reset()
{
    processor_.reset();
}

void AudioEngine::processRealtime(const std::vector<std::vector<float>>& monoChannelInputs,
                                  std::size_t numFrames,
                                  std::vector<float>& interleavedStereoOutput)
{
    dsp::AudioBlockView block;
    block.monoInputs = &monoChannelInputs;
    block.stereoOutput = &interleavedStereoOutput;
    block.numFrames = numFrames;
    processor_.process(session_, block);

    const auto& meters = processor_.lastMeters();
    session_.master.meterLeftPeak = meters.masterLeftPeak;
    session_.master.meterRightPeak = meters.masterRightPeak;
    session_.master.limiterActive = meters.limiterActive;
    for (std::size_t i = 0; i < session_.channels.size(); ++i)
        session_.channels[i].meterPeak = meters.channelPeaks[i];
}

} // namespace geilalizer::core
