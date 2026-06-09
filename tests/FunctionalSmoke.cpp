#include "core/AudioEngine.h"
#include "core/ImportPlanner.h"
#include "core/LevelMeter.h"
#include "core/LinearResampler.h"
#include "core/RenderEngine.h"
#include "core/SessionState.h"

#include <array>
#include <cassert>
#include <cmath>
#include <vector>

int main()
{
    using namespace geilalizer;

    std::array<bool, core::kMaxMonoChannels> occupied {};
    assert(core::findFirstContiguousFreeChannels(occupied, 1) == 0);
    assert(core::findFirstContiguousFreeChannels(occupied, 2) == 0);

    occupied[0] = true;
    occupied[2] = true;
    assert(core::findFirstContiguousFreeChannels(occupied, 2) == 3);

    occupied.fill(true);
    assert(!core::findFirstContiguousFreeChannels(occupied, 1).has_value());
    assert(!core::findFirstContiguousFreeChannels(occupied, 2).has_value());

    occupied.fill(false);
    occupied[5] = true;
    assert(core::canImportAtChannel(occupied, 4, 1));
    assert(!core::canImportAtChannel(occupied, 4, 2));
    assert(!core::canImportAtChannel(occupied, core::kMaxMonoChannels - 1, 2));

    const std::vector<float> quarterRateRamp { 0.0f, 1.0f, 0.0f };
    const auto doubled = core::resampleLinear(quarterRateRamp, 24000.0, 48000.0);
    assert(doubled.size() == 6);
    assert(std::fabs(doubled.front() - 0.0f) < 0.0001f);
    assert(std::fabs(doubled[2] - 1.0f) < 0.0001f);
    assert(std::fabs(doubled[4] - 0.0f) < 0.0001f);

    const auto unchanged = core::resampleLinear(quarterRateRamp, 48000.0, 48000.0);
    assert(unchanged == quarterRateRamp);

    core::SessionState session;
    auto& channel = session.channel(0);

    assert(!channel.hasAudioFile);
    assert(channel.sourcePath.empty());
    assert(channel.sourceChannelIndex == 0);
    assert(channel.lengthFrames == 0);

    channel.setLoadedAudioFile("kick.wav", 1, 1234);
    assert(channel.hasAudioFile);
    assert(channel.sourcePath == "kick.wav");
    assert(channel.sourceChannelIndex == 1);
    assert(channel.lengthFrames == 1234);

    channel.clearLoadedAudioFile();
    assert(!channel.hasAudioFile);
    assert(channel.sourcePath.empty());
    assert(channel.lengthFrames == 0);

    channel.setFaderGainDb(999.0f);
    assert(channel.faderGainDb == core::kFaderGainMaxDb);
    channel.setFaderGainDb(-999.0f);
    assert(channel.faderGainDb == core::kFaderGainMinDb);

    core::LevelMeter meter;
    meter.configure(0.5f, 0.25f);
    meter.pushPeak(0.25f);
    assert(meter.displayPeak() > 0.0f);
    assert(meter.holdPeak() >= meter.displayPeak());
    assert(!meter.clipped());
    meter.pushPeak(1.25f);
    assert(meter.clipped());
    assert(meter.displayDb() <= 0.0f);
    assert(meter.holdPeak() >= meter.displayPeak());
    meter.decay();
    assert(meter.displayPeak() < 1.0f);
    assert(meter.holdPeak() >= meter.displayPeak());
    meter.resetClip();
    assert(!meter.clipped());
    meter.reset();
    assert(meter.displayPeak() == 0.0f);
    assert(meter.holdPeak() == 0.0f);
    assert(!meter.clipped());

    core::AudioEngine engine;
    engine.prepare(44100.0, 8);
    std::vector<std::vector<float>> monoInputs(core::kMaxMonoChannels);
    monoInputs[0] = { 0.25f, -0.25f, 0.5f, -0.5f, 0.125f, -0.125f, 0.0f, 0.0f };
    std::vector<float> out(16, 0.0f);
    engine.processRealtime(monoInputs, 8, out);

    assert(engine.session().channel(0).meterPeak > 0.0f);
    assert(engine.session().master.meterLeftPeak >= 0.0f);
    assert(engine.session().master.meterRightPeak >= 0.0f);
    for (float sample : out)
        assert(std::isfinite(sample));

    core::RenderEngine renderer;
    core::RenderRequest rangeRequest;
    rangeRequest.sampleRate = 44100.0;
    rangeRequest.bitDepth = 24;
    rangeRequest.fullLength = false;
    rangeRequest.startFrame = 2;
    rangeRequest.numFrames = 4;
    rangeRequest.blockSize = 2;
    std::size_t streamedFrames = 0;
    std::size_t callbackCount = 0;
    const auto rangeResult = renderer.renderToSink(engine.session(), monoInputs, rangeRequest,
        [&](const float* interleavedStereo, std::size_t frames)
        {
            assert(interleavedStereo != nullptr);
            assert(frames <= 2);
            streamedFrames += frames;
            ++callbackCount;
            return true;
        });
    assert(rangeResult.completed);
    assert(streamedFrames == 4);
    assert(callbackCount == 2);
    assert(rangeResult.interleavedStereo.empty());

    rangeRequest.startFrame = 6;
    rangeRequest.numFrames = 99;
    streamedFrames = 0;
    callbackCount = 0;
    const auto clampedRangeResult = renderer.renderToSink(engine.session(), monoInputs, rangeRequest,
        [&](const float*, std::size_t frames)
        {
            streamedFrames += frames;
            ++callbackCount;
            return true;
        });
    assert(clampedRangeResult.completed);
    assert(streamedFrames == 2);
    assert(callbackCount == 1);

    rangeRequest.startFrame = 99;
    rangeRequest.numFrames = 10;
    streamedFrames = 0;
    callbackCount = 0;
    const auto emptyRangeResult = renderer.renderToSink(engine.session(), monoInputs, rangeRequest,
        [&](const float*, std::size_t frames)
        {
            streamedFrames += frames;
            ++callbackCount;
            return true;
        });
    assert(emptyRangeResult.completed);
    assert(streamedFrames == 0);
    assert(callbackCount == 0);

    return 0;
}
