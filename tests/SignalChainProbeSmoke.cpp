#include "core/SessionState.h"
#include "dsp/AudioBlock.h"
#include "dsp/MachineProcessor.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <string>
#include <vector>

namespace
{
bool hasStage(const std::vector<geilalizer::dsp::MachineProcessor::StageProbe>& probes,
              const std::string& name)
{
    return std::any_of(probes.begin(), probes.end(), [&name](const auto& probe) {
        return probe.name == name;
    });
}

const geilalizer::dsp::MachineProcessor::StageProbe& stage(
    const std::vector<geilalizer::dsp::MachineProcessor::StageProbe>& probes,
    const std::string& name)
{
    const auto it = std::find_if(probes.begin(), probes.end(), [&name](const auto& probe) {
        return probe.name == name;
    });
    assert(it != probes.end());
    return *it;
}
} // namespace

int main()
{
    using namespace geilalizer;

    core::SessionState session;
    session.channel(0).setInputGainDb(0.0f);
    session.channel(0).setPreampIndex(5);
    session.channel(0).faderGainDb = 0.0f;
    session.channel(0).setPan(0.0f);
    session.master.outputTrimDb = 0.0f;
    session.master.limiterEnabled = true;

    std::vector<std::vector<float>> monoInputs(core::kMaxMonoChannels);
    monoInputs[0].assign(256, 0.0f);
    monoInputs[0][0] = 1.0f;
    for (std::size_t i = 1; i < monoInputs[0].size(); ++i)
        monoInputs[0][i] = std::sin(static_cast<float>(i) * 0.071f) * 0.2f;

    std::vector<float> out(monoInputs[0].size() * 2, 0.0f);

    dsp::MachineProcessor processor;
    processor.prepare(48000.0, static_cast<int>(monoInputs[0].size()));
    processor.setStageProbeEnabled(true);

    dsp::AudioBlockView block { &monoInputs, &out, monoInputs[0].size() };
    processor.process(session, block);

    const auto& probes = processor.lastStageProbes();
    assert(!probes.empty());

    const std::vector<std::string> requiredStages {
        "channel 1 input gain",
        "channel 1 preamp IR",
        "channel 1 console NAM",
        "channel 1 tape NAM",
        "channel 1 fader",
        "channel 1 post-fader IR",
        "channel 1 pan/sum",
        "mixbus pre-IR safety limiter",
        "mixbus IR",
        "master tape NAM",
        "master output trim",
        "EMT limiter NAM",
        "post-EMT safety limiter",
        "final Hilo NAM",
        "final safety limiter"
    };

    for (const auto& name : requiredStages)
    {
        assert(hasStage(probes, name));
        const auto& probe = stage(probes, name);
        assert(probe.frames > 0);
        assert(std::isfinite(probe.beforePeak));
        assert(std::isfinite(probe.afterPeak));
        assert(std::isfinite(probe.deltaRms));
    }

    assert(stage(probes, "channel 1 preamp IR").changed());
    assert(stage(probes, "channel 1 post-fader IR").changed());
    assert(stage(probes, "mixbus IR").changed());

    processor.setStageProbeEnabled(false);
    std::fill(out.begin(), out.end(), 0.0f);
    processor.process(session, block);
    assert(processor.lastStageProbes().empty());

    return 0;
}
