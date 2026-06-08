#include "core/AudioConstants.h"
#include "core/AudioEngine.h"
#include "core/RenderEngine.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace
{
bool nearlyEqual(float a, float b, float tolerance = 0.0005f)
{
    return std::abs(a - b) <= tolerance;
}

float absPeak(const std::vector<float>& interleavedStereo, std::size_t channel)
{
    float peak = 0.0f;
    for (std::size_t i = channel; i < interleavedStereo.size(); i += 2)
        peak = std::max(peak, std::abs(interleavedStereo[i]));
    return peak;
}

void assertFiniteStereo(const std::vector<float>& interleavedStereo)
{
    for (float sample : interleavedStereo)
        assert(std::isfinite(sample));
}
} // namespace

int main()
{
    using namespace geilalizer;

    constexpr std::size_t frames = 256;
    constexpr double sampleRate = 44100.0;

    core::SessionState session;
    assert(session.channels.size() == core::kMaxMonoChannels);

    std::vector<std::vector<float>> monoInputs(core::kMaxMonoChannels);
    for (std::size_t channelIndex = 0; channelIndex < core::kMaxMonoChannels; ++channelIndex)
    {
        auto& channel = session.channel(channelIndex);
        channel.setInputGainDb(channelIndex % 3 == 0 ? 6.0f : (channelIndex % 3 == 1 ? 0.0f : -6.0f));
        channel.faderGainDb = channelIndex % 4 == 0 ? 0.0f : -9.0f;
        channel.setPan(channelIndex < 12 ? -1.0f : 1.0f);
        channel.setPreampIndex(static_cast<int>(channelIndex % 11));

        monoInputs[channelIndex].resize(frames);
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const float polarity = frame % 2 == 0 ? 1.0f : -1.0f;
            monoInputs[channelIndex][frame] = polarity * (0.0015f + static_cast<float>(channelIndex) * 0.00003f);
        }
    }

    session.master.outputTrimDb = 0.0f;
    session.master.limiterEnabled = false;

    core::AudioEngine realtime;
    realtime.prepare(sampleRate, static_cast<int>(frames));
    realtime.session() = session;
    std::vector<float> realtimeOut(frames * 2, 0.0f);
    realtime.processRealtime(monoInputs, frames, realtimeOut);

    assert(realtimeOut.size() == frames * 2);
    assertFiniteStereo(realtimeOut);
    assert(absPeak(realtimeOut, 0) > 0.0f);
    assert(absPeak(realtimeOut, 1) > 0.0f);

    core::RenderEngine renderer;
    core::RenderRequest renderRequest;
    renderRequest.sampleRate = sampleRate;
    renderRequest.bitDepth = 24;
    renderRequest.fullLength = true;
    const auto renderWithoutEmt = renderer.render(session, monoInputs, renderRequest);
    assert(renderWithoutEmt.completed);
    assert(renderWithoutEmt.interleavedStereo.size() == frames * 2);
    assertFiniteStereo(renderWithoutEmt.interleavedStereo);

    for (std::size_t i = 0; i < realtimeOut.size(); ++i)
        assert(nearlyEqual(realtimeOut[i], renderWithoutEmt.interleavedStereo[i]));

    session.master.limiterEnabled = true;
    core::RenderEngine emtRenderer;
    const auto renderWithEmt = emtRenderer.render(session, monoInputs, renderRequest);
    assert(renderWithEmt.completed);
    assert(renderWithEmt.interleavedStereo.size() == frames * 2);
    assertFiniteStereo(renderWithEmt.interleavedStereo);

    const float ceiling = std::pow(10.0f, core::kSafetyLimiterCeilingDb / 20.0f);
    for (float sample : renderWithEmt.interleavedStereo)
        assert(std::abs(sample) <= ceiling + 0.0001f);

    return 0;
}
