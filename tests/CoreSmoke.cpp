#include "core/AudioEngine.h"
#include "core/RenderEngine.h"
#include "dsp/MachineProcessor.h"

#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
bool nearlyEqual(float a, float b, float tolerance = 0.0001f)
{
    return std::abs(a - b) <= tolerance;
}
}

int main()
{
    using namespace geilalizer;

    core::SessionState session;
    assert(session.channels.size() == core::kMaxMonoChannels);
    assert(session.channel(0).name == "1");
    assert(session.channel(0).preampIndex == 5);
    assert(session.reserveStereoImportSplit("stereo.wav", 0).accepted);
    assert(!session.reserveStereoImportSplit("too-late.wav", 23).accepted);

    session.channel(0).setInputGainDb(99.0f);
    assert(nearlyEqual(session.channel(0).inputGainDb, core::kInputGainMaxDb));
    session.channel(0).setPan(-2.0f);
    assert(nearlyEqual(session.channel(0).pan, -1.0f));
    session.channel(0).setPreampIndex(999);
    assert(session.channel(0).preampIndex == 10);

    std::vector<std::vector<float>> monoInputs(core::kMaxMonoChannels);
    monoInputs[0] = { 0.25f, 0.25f, 0.25f, 0.25f };

    session.channel(0).setInputGainDb(0.0f);
    session.channel(0).setPreampIndex(5);
    session.channel(0).faderGainDb = 0.0f;
    session.channel(0).setPan(-1.0f);
    session.master.limiterEnabled = false;
    session.master.outputTrimDb = 0.0f;

    dsp::MachineProcessor processor;
    processor.prepare(44100.0, 4);
    assert(!processor.namAdapter().hasAnyModel());
    assert(processor.irAdapter().defaultPreampIndex() == 5);
    assert(processor.irAdapter().preampIrSlots()[0].fileName == "Neve 20.wav");
    assert(processor.irAdapter().preampIrSlots()[5].fileName == "Neve 55.wav");
    assert(processor.irAdapter().preampIrSlots()[10].fileName == "Neve 80.wav");
    assert(processor.irAdapter().hasAnyIr());
    assert(processor.irAdapter().hasIrForPreampIndex(5));
    assert(processor.namAdapter().bindSingleInternalModelForNextIntegration(
        0, { "placeholder-model-1", "smoke test single model bind" }));
    assert(processor.namAdapter().hasModelForChannel(0));

    std::vector<float> out(8, 123.0f);
    dsp::AudioBlockView block { &monoInputs, &out, 4 };
    processor.process(session, block);
    assert(std::isfinite(out[0]));
    assert(std::isfinite(out[1]));

    // Stepped Preamp changes must be click-managed by the IR adapter and remain finite.
    session.channel(0).setPreampIndex(10);
    processor.process(session, block);
    for (float sample : out)
        assert(std::isfinite(sample));

    session.master.limiterEnabled = true;
    session.master.outputTrimDb = 48.0f;
    monoInputs[0] = { 2.0f, -2.0f, 0.5f, -0.5f };
    processor.process(session, block);
    assert(processor.lastMeters().limiterActive);
    const float ceiling = std::pow(10.0f, core::kSafetyLimiterCeilingDb / 20.0f);
    assert(std::abs(out[0]) <= ceiling + 0.0001f);
    assert(std::abs(out[2]) <= ceiling + 0.0001f);
    session.master.outputTrimDb = 0.0f;

    core::AudioEngine realtime;
    realtime.prepare(44100.0, 4);
    realtime.session() = session;
    std::vector<float> realtimeOut(8, 0.0f);
    realtime.processRealtime(monoInputs, 4, realtimeOut);

    core::RenderEngine renderer;
    core::RenderRequest request;
    request.sampleRate = 44100.0;
    request.bitDepth = 24;
    request.fullLength = true;
    auto renderResult = renderer.render(session, monoInputs, request);
    assert(renderResult.completed);
    assert(renderResult.interleavedStereo.size() == realtimeOut.size());
    for (std::size_t i = 0; i < realtimeOut.size(); ++i)
        assert(nearlyEqual(renderResult.interleavedStereo[i], realtimeOut[i]));

    return 0;
}
