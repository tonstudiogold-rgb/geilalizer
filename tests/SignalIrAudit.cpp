#include "core/SessionState.h"
#include "dsp/AudioBlock.h"
#include "dsp/MachineProcessor.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace
{
using Probe = geilalizer::dsp::MachineProcessor::StageProbe;

const Probe& requireStage(const std::vector<Probe>& probes, const std::string& name)
{
    const auto it = std::find_if(probes.begin(), probes.end(), [&name](const Probe& probe) {
        return probe.name == name;
    });
    assert(it != probes.end());
    return *it;
}

bool finiteProbe(const Probe& probe)
{
    return probe.frames > 0
        && std::isfinite(probe.beforePeak)
        && std::isfinite(probe.afterPeak)
        && std::isfinite(probe.deltaRms);
}

void printProbeRow(const std::string& section, const Probe& probe)
{
    std::cout << std::left << std::setw(12) << section
              << " | " << std::setw(34) << probe.name
              << " | " << std::right << std::setw(6) << probe.frames
              << " | " << std::setw(11) << probe.beforePeak
              << " | " << std::setw(10) << probe.afterPeak
              << " | " << std::setw(10) << probe.deltaRms
              << " | " << (probe.changed() ? "PASS" : "PASS/no-change")
              << '\n';
}
} // namespace

int main()
{
    using namespace geilalizer;

    constexpr std::size_t channelIndex = 3; // User-facing channel 4.
    constexpr int channelNumber = 4;
    constexpr int preampIndex = 10; // Neve 80.wav, intentionally not the default slot.
    constexpr std::size_t frames = 512;

    core::SessionState session;
    auto& channel = session.channel(channelIndex);
    channel.setInputGainDb(3.0f);
    channel.setPreampIndex(preampIndex);
    channel.faderGainDb = -4.5f;
    channel.setPan(-0.25f);
    session.master.outputTrimDb = 1.5f;
    session.master.limiterEnabled = true;

    std::vector<std::vector<float>> monoInputs(core::kMaxMonoChannels);
    monoInputs[channelIndex].assign(frames, 0.0f);
    monoInputs[channelIndex][0] = 0.95f;
    for (std::size_t i = 1; i < frames; ++i)
    {
        const float sine = std::sin(static_cast<float>(i) * 0.047f) * 0.34f;
        const float ramp = (static_cast<float>(i % 31) / 31.0f - 0.5f) * 0.08f;
        monoInputs[channelIndex][i] = sine + ramp;
    }

    std::vector<float> out(frames * 2, 0.0f);

    dsp::MachineProcessor processor;
    processor.prepare(48000.0, static_cast<int>(frames));
    processor.setStageProbeEnabled(true);

    dsp::AudioBlockView block { &monoInputs, &out, frames };
    processor.process(session, block);

    const auto& probes = processor.lastStageProbes();
    assert(!probes.empty());

    const std::string prefix = "channel " + std::to_string(channelNumber) + " ";
    const std::vector<std::string> channelStages {
        prefix + "input gain",
        prefix + "preamp IR",
        prefix + "console NAM",
        prefix + "tape NAM",
        prefix + "fader",
        prefix + "post-fader IR",
        prefix + "pan/sum"
    };
    const std::vector<std::string> masterStages {
        "mixbus pre-IR safety limiter",
        "mixbus IR",
        "master tape NAM",
        "master output trim",
        "EMT limiter NAM",
        "post-EMT safety limiter",
        "final Hilo NAM",
        "final safety limiter"
    };

    for (const auto& name : channelStages)
        assert(finiteProbe(requireStage(probes, name)));
    for (const auto& name : masterStages)
        assert(finiteProbe(requireStage(probes, name)));

    const auto& preampProbe = requireStage(probes, prefix + "preamp IR");
    const auto& postFaderProbe = requireStage(probes, prefix + "post-fader IR");
    const auto& mixbusProbe = requireStage(probes, "mixbus IR");
    assert(preampProbe.changed());
    assert(postFaderProbe.changed());
    assert(mixbusProbe.changed());

    const auto& preampSlot = processor.irAdapter().preampIrSlots().at(static_cast<std::size_t>(preampIndex));
    assert(!preampSlot.fileName.empty());
    assert(!preampSlot.monoTaps.empty());

    const int postFaderSlotIndex = processor.postFaderIrAdapter().slotIndexForFaderDb(channel.faderGainDb);
    const auto& postFaderSlot = processor.postFaderIrAdapter().slots().at(static_cast<std::size_t>(postFaderSlotIndex));
    assert(!postFaderSlot.fileName.empty());
    assert(!postFaderSlot.monoTaps.empty());

    const int mixbusSlotIndex = processor.mixbusIrAdapter().activeSlotIndex();
    const auto& mixbusSlot = processor.mixbusIrAdapter().slots().at(static_cast<std::size_t>(mixbusSlotIndex));
    assert(!mixbusSlot.fileName.empty());
    assert(!mixbusSlot.leftTaps.empty());
    assert(!mixbusSlot.rightTaps.empty());

    const auto& meters = processor.lastMeters();
    assert(std::isfinite(meters.mixbusPreLimiterPeakDb));
    assert(std::isfinite(meters.masterLeftPeak));
    assert(std::isfinite(meters.masterRightPeak));
    assert(meters.masterLeftPeak > 0.0f || meters.masterRightPeak > 0.0f);

    std::cout << std::fixed << std::setprecision(6);
    std::cout << "Geilalizer Signal-/IR-Audit: PASS\n";
    std::cout << "Pfad: Channel " << channelNumber << " -> Mixbus/Master -> Stereo Out\n";
    std::cout << "Aktive IRs:\n";
    std::cout << "  Channel " << channelNumber << " Preamp IR: Slot " << preampIndex
              << " | Datei: " << preampSlot.fileName
              << " | Taps: " << preampSlot.monoTaps.size() << '\n';
    std::cout << "  Channel " << channelNumber << " Post-Fader IR: Slot " << postFaderSlotIndex
              << " | Label: " << postFaderSlot.driveLabel
              << " | Datei: " << postFaderSlot.fileName
              << " | Taps: " << postFaderSlot.monoTaps.size() << '\n';
    std::cout << "  Master/Mixbus IR: Slot " << mixbusSlotIndex
              << " | Label: " << mixbusSlot.driveLabel
              << " | Datei: " << mixbusSlot.fileName
              << " | L/R Taps: " << mixbusSlot.leftTaps.size() << '/' << mixbusSlot.rightTaps.size() << '\n';
    std::cout << "Meter:\n";
    std::cout << "  Mixbus vor IR/Limiter: " << meters.mixbusPreLimiterPeakDb << " dBFS"
              << " | Master L Peak: " << meters.masterLeftPeak
              << " | Master R Peak: " << meters.masterRightPeak
              << " | Mixbus IR Slot laut Meter: " << meters.mixbusIrSlotIndex << '\n';
    std::cout << "\nBereich      | Stage                              | Frames | BeforePeak  | AfterPeak  | DeltaRMS   | Ergebnis\n";
    std::cout << "-------------|------------------------------------|--------|-------------|------------|------------|----------\n";
    for (const auto& name : channelStages)
        printProbeRow("Channel", requireStage(probes, name));
    for (const auto& name : masterStages)
        printProbeRow("Master", requireStage(probes, name));

    return 0;
}
