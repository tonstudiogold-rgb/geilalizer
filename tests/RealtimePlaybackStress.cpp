#include "core/AudioEngine.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <vector>

namespace
{
constexpr double kPi = 3.14159265358979323846;

void disablePrivateModelLoading()
{
#if defined(_WIN32)
    _putenv_s("GEILALIZER_DISABLE_REAL_NAMCORE", "1");
#else
    setenv("GEILALIZER_DISABLE_REAL_NAMCORE", "1", 1);
#endif
}

std::vector<std::vector<float>> makeInputs(int activeTracks, std::size_t frames, double sampleRate, int blockIndex)
{
    std::vector<std::vector<float>> inputs(geilalizer::core::kMaxMonoChannels,
                                           std::vector<float>(frames, 0.0f));
    for (int ch = 0; ch < activeTracks; ++ch)
    {
        const double freq = 110.0 + static_cast<double>(ch) * 17.0;
        for (std::size_t i = 0; i < frames; ++i)
        {
            const double t = (static_cast<double>(blockIndex) * static_cast<double>(frames) + static_cast<double>(i)) / sampleRate;
            inputs[static_cast<std::size_t>(ch)][i] = static_cast<float>(0.025 * std::sin(2.0 * kPi * freq * t));
        }
    }
    return inputs;
}

bool finiteBuffer(const std::vector<float>& values)
{
    return std::all_of(values.begin(), values.end(), [](float v) { return std::isfinite(v); });
}

float peakOf(const std::vector<float>& values)
{
    float peak = 0.0f;
    for (float v : values)
        peak = std::max(peak, std::abs(v));
    return peak;
}

void configureSession(geilalizer::core::SessionState& session, int activeTracks, std::size_t frames)
{
    for (std::size_t ch = 0; ch < session.channels.size(); ++ch)
    {
        auto& state = session.channel(ch);
        state = geilalizer::core::ChannelState::createDefault(ch);
        if (ch < static_cast<std::size_t>(activeTracks))
        {
            state.hasAudioFile = true;
            state.lengthFrames = frames * 16;
            state.inputAssigned = true;
            state.inputChannelIndex = static_cast<int>(ch);
            state.setInputGainDb(static_cast<float>((static_cast<int>(ch) % 5) - 2));
            state.setFaderGainDb(-6.0f + static_cast<float>(ch % 4));
            state.muted = false;
            state.solo = false;
        }
    }
}

void assertMetersFinite(const geilalizer::core::AudioEngine::RealtimeMeters& meters, int activeTracks)
{
    assert(std::isfinite(meters.masterLeftPeak));
    assert(std::isfinite(meters.masterRightPeak));
    assert(meters.masterLeftPeak >= 0.0f);
    assert(meters.masterRightPeak >= 0.0f);
    for (int i = 0; i < activeTracks; ++i)
    {
        assert(std::isfinite(meters.channelPeaks[static_cast<std::size_t>(i)]));
        assert(meters.channelPeaks[static_cast<std::size_t>(i)] >= 0.0f);
    }
}

void runTrackCountStress(int activeTracks, double sampleRate, int blockSize)
{
    geilalizer::core::AudioEngine engine;
    engine.prepare(sampleRate, blockSize);

    geilalizer::core::SessionState session;
    configureSession(session, activeTracks, static_cast<std::size_t>(blockSize));

    std::vector<float> stereo(static_cast<std::size_t>(blockSize) * 2, 0.0f);
    geilalizer::core::AudioEngine::RealtimeMeters meters;
    float accumulatedPeak = 0.0f;

    for (int block = 0; block < 12; ++block)
    {
        session.master.limiterEnabled = (block % 2) == 0; // click-safety regression coverage for ON/OFF during playback path.
        session.master.outputTrimDb = (block % 3 == 0) ? -3.0f : 0.0f;
        auto inputs = makeInputs(activeTracks, static_cast<std::size_t>(blockSize), sampleRate, block);
        std::fill(stereo.begin(), stereo.end(), 0.0f);
        engine.processRealtime(session, inputs, static_cast<std::size_t>(blockSize), stereo, meters);
        assert(finiteBuffer(stereo));
        assertMetersFinite(meters, activeTracks);
        accumulatedPeak = std::max(accumulatedPeak, peakOf(stereo));
    }

    assert(accumulatedPeak > 0.00001f);
    std::cout << "RealtimePlaybackStress tracks=" << activeTracks
              << " sampleRate=" << sampleRate
              << " blockSize=" << blockSize
              << " peak=" << accumulatedPeak << '\n';
}

void runDeviceSwitchStress()
{
    geilalizer::core::AudioEngine engine;
    geilalizer::core::SessionState session;
    configureSession(session, 8, 1024);
    geilalizer::core::AudioEngine::RealtimeMeters meters;

    const struct DeviceCase { double sampleRate; int blockSize; } cases[] {
        { 44100.0, 128 },
        { 48000.0, 256 },
        { 96000.0, 512 },
        { 48000.0, 64 }
    };

    for (int idx = 0; idx < 4; ++idx)
    {
        engine.prepare(cases[idx].sampleRate, cases[idx].blockSize);
        std::vector<float> stereo(static_cast<std::size_t>(cases[idx].blockSize) * 2, 0.0f);
        auto inputs = makeInputs(8, static_cast<std::size_t>(cases[idx].blockSize), cases[idx].sampleRate, idx);
        session.master.limiterEnabled = (idx % 2) == 1;
        engine.processRealtime(session, inputs, static_cast<std::size_t>(cases[idx].blockSize), stereo, meters);
        assert(finiteBuffer(stereo));
        assert(peakOf(stereo) > 0.00001f);
        assertMetersFinite(meters, 8);
        std::cout << "RealtimePlaybackStress device-switch sampleRate=" << cases[idx].sampleRate
                  << " blockSize=" << cases[idx].blockSize << '\n';
    }
}
} // namespace

int main()
{
    disablePrivateModelLoading();

    runTrackCountStress(1, 48000.0, 128);
    runTrackCountStress(8, 48000.0, 256);
    runTrackCountStress(16, 48000.0, 256);
    runTrackCountStress(24, 48000.0, 512);
    runDeviceSwitchStress();

    return 0;
}
