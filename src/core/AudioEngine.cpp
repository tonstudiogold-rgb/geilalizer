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
}

} // namespace geilalizer::core
