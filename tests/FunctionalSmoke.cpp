#include "core/AudioEngine.h"
#include "core/ImportPlanner.h"
#include "core/LinearResampler.h"
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

    return 0;
}
