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

void AudioEngine::loadDefaultAssetsBeforeAudioStart()
{
    processor_.applyPreparedMachineState(dsp::MachineProcessor::loadDefaultAssetsOffAudioThread());
}

void AudioEngine::processRealtime(const std::vector<std::vector<float>>& monoChannelInputs,
                                  std::size_t numFrames,
                                  std::vector<float>& interleavedStereoOutput)
{
    RealtimeMeters ignored;
    processRealtime(session_, monoChannelInputs, numFrames, interleavedStereoOutput, ignored);

    session_.master.meterLeftPeak = ignored.masterLeftPeak;
    session_.master.meterRightPeak = ignored.masterRightPeak;
    session_.master.limiterActive = ignored.limiterActive;
    for (std::size_t i = 0; i < session_.channels.size(); ++i)
        session_.channels[i].meterPeak = ignored.channelPeaks[i];
}

void AudioEngine::processRealtime(const SessionState& sessionSnapshot,
                                  const std::vector<std::vector<float>>& monoChannelInputs,
                                  std::size_t numFrames,
                                  std::vector<float>& interleavedStereoOutput,
                                  RealtimeMeters& metersOut)
{
    dsp::AudioBlockView block;
    block.monoInputs = &monoChannelInputs;
    block.stereoOutput = &interleavedStereoOutput;
    block.numFrames = numFrames;
    processor_.process(sessionSnapshot, block);

    const auto& meters = processor_.lastMeters();
    metersOut.masterLeftPeak = meters.masterLeftPeak;
    metersOut.masterRightPeak = meters.masterRightPeak;
    metersOut.limiterActive = meters.limiterActive;
    for (std::size_t i = 0; i < metersOut.channelPeaks.size(); ++i)
        metersOut.channelPeaks[i] = meters.channelPeaks[i];
}

} // namespace geilalizer::core
