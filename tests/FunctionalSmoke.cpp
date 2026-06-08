#include "core/AudioEngine.h"
#include "core/SessionState.h"

#include <cassert>
#include <cmath>
#include <vector>

int main()
{
    using namespace geilalizer;

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
