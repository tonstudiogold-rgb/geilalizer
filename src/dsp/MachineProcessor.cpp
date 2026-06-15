#include "MachineProcessor.h"

#include "AssetPath.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <utility>

namespace geilalizer::dsp
{
namespace
{
float dbToLinear(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

float linearToDb(float linear)
{
    if (linear <= 0.0f || !std::isfinite(linear))
        return -120.0f;
    return 20.0f * std::log10(linear);
}

void updatePeak(float& meter, float value)
{
    meter = std::max(meter, std::abs(value));
}

float peakOf(const float* samples, std::size_t count)
{
    float peak = 0.0f;
    if (samples == nullptr)
        return peak;
    for (std::size_t i = 0; i < count; ++i)
        updatePeak(peak, samples[i]);
    return peak;
}

float deltaRmsBetween(const float* before, const float* after, std::size_t count)
{
    if (before == nullptr || after == nullptr || count == 0)
        return 0.0f;

    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double delta = static_cast<double>(after[i]) - static_cast<double>(before[i]);
        sum += delta * delta;
    }
    return static_cast<float>(std::sqrt(sum / static_cast<double>(count)));
}

const char* fixedConsoleNamModelPath()
{
    return "private_nam_models/model_002/NEVE 5315 ANALOG CONSOLE.nam";
}

const char* fixedTapeNamModelPath()
{
    return "private_nam_models/model_003/STUDER C37 TUBE TAPE 15 IPS.nam";
}

const char* fixedEmtLimiterNamModelPath()
{
    return "private_nam_models/model_006_emt_limiter/EMT 266X ANALOG LIMITER.nam";
}

const char* fixedFinalHiloNamModelPath()
{
    return "private_nam_models/model_007_hilo_final/Hilo Nam 1.nam";
}
} // namespace

void MachineProcessor::applyPreparedMachineState(PreparedMachineState state)
{
    irAdapter_.setPreampIrSlots(std::move(state.preampIrSlots));
    postFaderIrAdapter_.setSlots(std::move(state.postFaderIrSlots));
    mixbusIrAdapter_.setSlots(std::move(state.mixbusIrSlots));

    if (state.consoleNamAvailable)
        namAdapter_.bindFixedInternalModelForAllChannels(state.consoleNam);

    if (state.tapeNamAvailable)
    {
        tapeNamAdapter_.bindFixedInternalModelForAllChannels(state.tapeNam);
        masterTapeNamAdapter_.bindSingleInternalModelForNextIntegration(
            0, { state.tapeNam.modelIdentifier, "fixed hidden Studer C37 tape NAM again on left mixbus after Model 5" });
        masterTapeNamAdapter_.bindSingleInternalModelForNextIntegration(
            1, { state.tapeNam.modelIdentifier, "fixed hidden Studer C37 tape NAM again on right mixbus after Model 5" });
    }

    if (state.emtLimiterNamAvailable)
    {
        emtLimiterNamAdapter_.bindSingleInternalModelForNextIntegration(0, state.emtLimiterNam);
        emtLimiterNamAdapter_.bindSingleInternalModelForNextIntegration(
            1, { state.emtLimiterNam.modelIdentifier, "switchable hidden EMT 266X NAM limiter on right master bus" });
    }

    if (state.finalHiloNamAvailable)
    {
        finalHiloNamAdapter_.bindSingleInternalModelForNextIntegration(0, state.finalHiloNam);
        finalHiloNamAdapter_.bindSingleInternalModelForNextIntegration(
            1, { state.finalHiloNam.modelIdentifier, "fixed final Hilo NAM 1 on right output after post-EMT safety limiter" });
    }
}

MachineProcessor::PreparedMachineState MachineProcessor::loadDefaultAssetsOffAudioThread()
{
    PreparedMachineState state;

    HiddenIrAdapter preampLoader;
    preampLoader.loadNeve1073DirectoryIfPresent();
    state.preampIrSlots = preampLoader.preampIrSlots();

    HiddenPostFaderIrAdapter postFaderLoader;
    postFaderLoader.loadSsl4000GDirectoryIfPresent();
    state.postFaderIrSlots = postFaderLoader.slots();

    HiddenMixbusIrAdapter mixbusLoader;
    mixbusLoader.loadMixbusDirectoryIfPresent();
    state.mixbusIrSlots = mixbusLoader.slots();

    const std::filesystem::path fixedConsoleNamPath = resolveProductAssetPath(fixedConsoleNamModelPath());
    if (std::filesystem::exists(fixedConsoleNamPath))
    {
        state.consoleNamAvailable = true;
        state.consoleNam = { fixedConsoleNamPath.string(),
            "fixed hidden Neve/console NAM after selected IR and before tape/fader coloration stages" };
    }

    const std::filesystem::path fixedTapeNamPath = resolveProductAssetPath(fixedTapeNamModelPath());
    if (std::filesystem::exists(fixedTapeNamPath))
    {
        state.tapeNamAvailable = true;
        state.tapeNam = { fixedTapeNamPath.string(),
            "fixed hidden Studer C37 tape NAM after console NAM and before fader coloration stages" };
    }

    const std::filesystem::path fixedEmtLimiterNamPath = resolveProductAssetPath(fixedEmtLimiterNamModelPath());
    if (std::filesystem::exists(fixedEmtLimiterNamPath))
    {
        state.emtLimiterNamAvailable = true;
        state.emtLimiterNam = { fixedEmtLimiterNamPath.string(),
            "switchable hidden EMT 266X NAM limiter on left master bus" };
    }

    const std::filesystem::path fixedFinalHiloNamPath = resolveProductAssetPath(fixedFinalHiloNamModelPath());
    if (std::filesystem::exists(fixedFinalHiloNamPath))
    {
        state.finalHiloNamAvailable = true;
        state.finalHiloNam = { fixedFinalHiloNamPath.string(),
            "fixed final Hilo NAM 1 on left output after post-EMT safety limiter" };
    }

    return state;
}

void MachineProcessor::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max(0, maxBlockSize);
    for (auto& scratch : channelScratch_)
        scratch.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    masterLeftScratch_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    masterRightScratch_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    emtLimiterDryScratch_.assign(static_cast<std::size_t>(maxBlockSize_) * 2, 0.0f);
    emtLimiterWetScratch_.assign(static_cast<std::size_t>(maxBlockSize_) * 2, 0.0f);
    emtLimiterBypassCrossfadeSamples_ = std::clamp(static_cast<int>(std::round(sampleRate_ * 0.003)), 64, 256);
    namAdapter_.prepare(sampleRate_, maxBlockSize_);
    tapeNamAdapter_.prepare(sampleRate_, maxBlockSize_);
    masterTapeNamAdapter_.prepare(sampleRate_, maxBlockSize_);
    emtLimiterNamAdapter_.prepare(sampleRate_, maxBlockSize_);
    finalHiloNamAdapter_.prepare(sampleRate_, maxBlockSize_);
    irAdapter_.prepare(sampleRate_, maxBlockSize_);
    postFaderIrAdapter_.prepare(sampleRate_, maxBlockSize_);
    mixbusIrAdapter_.prepare(sampleRate_, maxBlockSize_);

    reset();
}

void MachineProcessor::reset()
{
    limiter_.reset();
    namAdapter_.reset();
    tapeNamAdapter_.reset();
    masterTapeNamAdapter_.reset();
    emtLimiterNamAdapter_.reset();
    finalHiloNamAdapter_.reset();
    irAdapter_.reset();
    postFaderIrAdapter_.reset();
    mixbusIrAdapter_.reset();
    mixbusPreIrLimiter_.reset();
    postEmtSafetyLimiter_.reset();
    emtLimiterBypassSamplesRemaining_ = 0;
    emtLimiterBypassCurrentEnabled_ = false;
    emtLimiterBypassTargetEnabled_ = false;
    lastMeters_ = {};
    clearStageProbes();
}

void MachineProcessor::clearStageProbes()
{
    lastStageProbes_.clear();
}

void MachineProcessor::recordMonoStageProbe(const std::string& name, const std::vector<float>& before,
                                            const float* after, std::size_t frames)
{
    if (!stageProbeEnabled_ || after == nullptr)
        return;

    const std::size_t samples = std::min(frames, before.size());
    StageProbe probe;
    probe.name = name;
    probe.frames = samples;
    probe.beforePeak = peakOf(before.data(), samples);
    probe.afterPeak = peakOf(after, samples);
    probe.deltaRms = deltaRmsBetween(before.data(), after, samples);
    lastStageProbes_.push_back(std::move(probe));
}

void MachineProcessor::recordStereoStageProbe(const std::string& name, const std::vector<float>& beforeInterleaved,
                                              const std::vector<float>& afterInterleaved, std::size_t frames)
{
    if (!stageProbeEnabled_)
        return;

    const std::size_t samples = std::min({ frames * 2, beforeInterleaved.size(), afterInterleaved.size() });
    StageProbe probe;
    probe.name = name;
    probe.frames = samples / 2;
    probe.beforePeak = peakOf(beforeInterleaved.data(), samples);
    probe.afterPeak = peakOf(afterInterleaved.data(), samples);
    probe.deltaRms = deltaRmsBetween(beforeInterleaved.data(), afterInterleaved.data(), samples);
    lastStageProbes_.push_back(std::move(probe));
}

void MachineProcessor::processStereoNam(HiddenNamAdapter& adapter, std::vector<float>& interleavedStereo, std::size_t numFrames)
{
    if (interleavedStereo.size() < numFrames * 2)
        return;

    if (masterLeftScratch_.size() < numFrames)
        masterLeftScratch_.resize(numFrames, 0.0f);
    if (masterRightScratch_.size() < numFrames)
        masterRightScratch_.resize(numFrames, 0.0f);

    for (std::size_t frame = 0; frame < numFrames; ++frame)
    {
        masterLeftScratch_[frame] = interleavedStereo[frame * 2];
        masterRightScratch_[frame] = interleavedStereo[frame * 2 + 1];
    }

    adapter.processChannel(0, masterLeftScratch_.data(), numFrames);
    adapter.processChannel(1, masterRightScratch_.data(), numFrames);

    for (std::size_t frame = 0; frame < numFrames; ++frame)
    {
        interleavedStereo[frame * 2] = masterLeftScratch_[frame];
        interleavedStereo[frame * 2 + 1] = masterRightScratch_[frame];
    }
}

void MachineProcessor::processEmtLimiterBypass(bool enabled, std::vector<float>& interleavedStereo, std::size_t numFrames)
{
    const std::size_t sampleCount = numFrames * 2;
    if (interleavedStereo.size() < sampleCount)
        return;

    if (emtLimiterDryScratch_.size() < sampleCount || emtLimiterWetScratch_.size() < sampleCount)
        return;

    if (enabled != emtLimiterBypassTargetEnabled_)
    {
        emtLimiterBypassTargetEnabled_ = enabled;
        emtLimiterBypassSamplesRemaining_ = emtLimiterBypassCrossfadeSamples_;
        if (emtLimiterBypassTargetEnabled_)
            emtLimiterNamAdapter_.reset();
    }

    if (emtLimiterBypassSamplesRemaining_ <= 0)
    {
        emtLimiterBypassCurrentEnabled_ = emtLimiterBypassTargetEnabled_;
        if (emtLimiterBypassCurrentEnabled_)
            processStereoNam(emtLimiterNamAdapter_, interleavedStereo, numFrames);
        return;
    }

    std::copy(interleavedStereo.begin(), interleavedStereo.begin() + static_cast<std::ptrdiff_t>(sampleCount),
              emtLimiterDryScratch_.begin());
    std::copy(emtLimiterDryScratch_.begin(), emtLimiterDryScratch_.begin() + static_cast<std::ptrdiff_t>(sampleCount),
              emtLimiterWetScratch_.begin());
    processStereoNam(emtLimiterNamAdapter_, emtLimiterWetScratch_, numFrames);

    for (std::size_t frame = 0; frame < numFrames; ++frame)
    {
        const int remainingBeforeFrame = emtLimiterBypassSamplesRemaining_;
        const float fadeProgress = 1.0f - (static_cast<float>(remainingBeforeFrame)
            / static_cast<float>(std::max(1, emtLimiterBypassCrossfadeSamples_)));
        const float wetAmount = emtLimiterBypassTargetEnabled_ ? fadeProgress : 1.0f - fadeProgress;
        const float dryAmount = 1.0f - wetAmount;

        const std::size_t left = frame * 2;
        interleavedStereo[left] = emtLimiterDryScratch_[left] * dryAmount + emtLimiterWetScratch_[left] * wetAmount;
        interleavedStereo[left + 1] = emtLimiterDryScratch_[left + 1] * dryAmount + emtLimiterWetScratch_[left + 1] * wetAmount;

        if (emtLimiterBypassSamplesRemaining_ > 0)
            --emtLimiterBypassSamplesRemaining_;
    }

    if (emtLimiterBypassSamplesRemaining_ <= 0)
    {
        emtLimiterBypassCurrentEnabled_ = emtLimiterBypassTargetEnabled_;
        if (! emtLimiterBypassCurrentEnabled_)
            emtLimiterNamAdapter_.reset();
    }
}

void MachineProcessor::process(const core::SessionState& session, AudioBlockView block)
{
    ensureStereoOutput(block);
    lastMeters_ = {};
    clearStageProbes();
    if (block.stereoOutput == nullptr || block.numFrames == 0)
        return;

    const auto& inputs = block.monoInputs;
    auto& out = *block.stereoOutput;

    for (std::size_t channelIndex = 0; channelIndex < core::kMaxMonoChannels; ++channelIndex)
    {
        if (inputs == nullptr || channelIndex >= inputs->size())
            break;

        const auto& input = (*inputs)[channelIndex];
        if (input.empty())
            continue;

        const auto& channel = session.channels[channelIndex];
        const float inGain = dbToLinear(channel.inputGainDb);
        const float fader = dbToLinear(channel.faderGainDb);
        const float pan = std::clamp(channel.pan, -1.0f, 1.0f);
        const float leftPan = std::sqrt(0.5f * (1.0f - pan));
        const float rightPan = std::sqrt(0.5f * (1.0f + pan));

        const std::size_t frames = std::min(block.numFrames, input.size());
        auto& scratch = channelScratch_[channelIndex];
        if (scratch.size() < frames)
            scratch.resize(frames, 0.0f); // Non-realtime safety for tests/offline; prepare() prevents this in realtime.

        for (std::size_t frame = 0; frame < frames; ++frame)
            scratch[frame] = input[frame] * inGain;
        if (stageProbeEnabled_)
        {
            stageProbeScratch_.assign(input.begin(), input.begin() + static_cast<std::ptrdiff_t>(frames));
            recordMonoStageProbe("channel " + std::to_string(channelIndex + 1) + " input gain",
                                 stageProbeScratch_, scratch.data(), frames);
        }

        // Product order: input gain drives the selected per-channel IR/preamp colour first.
        // The hidden console NAM and tape NAM are fixed stages after that.
        // Future hidden fader model stage belongs between tape NAM and visible fader gain.
        if (stageProbeEnabled_)
            stageProbeScratch_.assign(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(frames));
        irAdapter_.processChannel(channelIndex, channel.preampIndex, scratch.data(), frames);
        recordMonoStageProbe("channel " + std::to_string(channelIndex + 1) + " preamp IR", stageProbeScratch_, scratch.data(), frames);

        if (stageProbeEnabled_)
            stageProbeScratch_.assign(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(frames));
        namAdapter_.processChannel(channelIndex, scratch.data(), frames);
        recordMonoStageProbe("channel " + std::to_string(channelIndex + 1) + " console NAM", stageProbeScratch_, scratch.data(), frames);

        if (stageProbeEnabled_)
            stageProbeScratch_.assign(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(frames));
        tapeNamAdapter_.processChannel(channelIndex, scratch.data(), frames);
        recordMonoStageProbe("channel " + std::to_string(channelIndex + 1) + " tape NAM", stageProbeScratch_, scratch.data(), frames);

        if (stageProbeEnabled_)
            stageProbeScratch_.assign(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(frames));
        for (std::size_t frame = 0; frame < frames; ++frame)
            scratch[frame] *= fader;
        recordMonoStageProbe("channel " + std::to_string(channelIndex + 1) + " fader", stageProbeScratch_, scratch.data(), frames);

        if (stageProbeEnabled_)
            stageProbeScratch_.assign(scratch.begin(), scratch.begin() + static_cast<std::ptrdiff_t>(frames));
        postFaderIrAdapter_.processChannel(channelIndex, channel.faderGainDb, scratch.data(), frames);
        recordMonoStageProbe("channel " + std::to_string(channelIndex + 1) + " post-fader IR", stageProbeScratch_, scratch.data(), frames);

        if (stageProbeEnabled_)
            stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const float mono = scratch[frame];
            updatePeak(lastMeters_.channelPeaks[channelIndex], mono);
            out[frame * 2] += mono * leftPan;
            out[frame * 2 + 1] += mono * rightPan;
        }
        recordStereoStageProbe("channel " + std::to_string(channelIndex + 1) + " pan/sum", stageProbeScratch_, out, block.numFrames);
    }

    float mixbusPreLimiterPeak = 0.0f;
    for (std::size_t frame = 0; frame < block.numFrames; ++frame)
    {
        updatePeak(mixbusPreLimiterPeak, out[frame * 2]);
        updatePeak(mixbusPreLimiterPeak, out[frame * 2 + 1]);
    }
    lastMeters_.mixbusPreLimiterPeakDb = linearToDb(mixbusPreLimiterPeak);
    lastMeters_.mixbusIrSlotIndex = mixbusIrAdapter_.slotIndexForPeakDb(lastMeters_.mixbusPreLimiterPeakDb);

    // Hidden Model 5 mixbus path: choose the SSL 4000 G mixbus IR from the summed level only,
    // then protect the IR input with its own fixed safety limiter before convolution.
    if (stageProbeEnabled_)
        stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
    mixbusPreIrLimiter_.setEnabled(true);
    mixbusPreIrLimiter_.processInterleavedStereo(out.data(), static_cast<int>(block.numFrames));
    lastMeters_.mixbusPreLimiterActive = mixbusPreIrLimiter_.active();
    recordStereoStageProbe("mixbus pre-IR safety limiter", stageProbeScratch_, out, block.numFrames);

    if (stageProbeEnabled_)
        stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
    mixbusIrAdapter_.processInterleavedStereo(lastMeters_.mixbusPreLimiterPeakDb, out.data(), block.numFrames);
    recordStereoStageProbe("mixbus IR", stageProbeScratch_, out, block.numFrames);

    // Corrected master chain: Model 5 feeds a second fixed Model 3 tape pass on the stereo bus.
    if (stageProbeEnabled_)
        stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
    processStereoNam(masterTapeNamAdapter_, out, block.numFrames);
    recordStereoStageProbe("master tape NAM", stageProbeScratch_, out, block.numFrames);

    if (stageProbeEnabled_)
        stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
    const float outputTrim = dbToLinear(session.master.outputTrimDb);
    for (float& sample : out)
        sample *= outputTrim;
    recordStereoStageProbe("master output trim", stageProbeScratch_, out, block.numFrames);

    // The visible limiter switch controls only the EMT 266X NAM limiter stage.
    // Safety limiters are fixed protection stages and stay on.
    if (stageProbeEnabled_)
        stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
    processEmtLimiterBypass(session.master.limiterEnabled, out, block.numFrames);
    recordStereoStageProbe("EMT limiter NAM", stageProbeScratch_, out, block.numFrames);

    if (stageProbeEnabled_)
        stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
    postEmtSafetyLimiter_.setEnabled(true);
    postEmtSafetyLimiter_.processInterleavedStereo(out.data(), static_cast<int>(block.numFrames));
    lastMeters_.postEmtSafetyLimiterActive = postEmtSafetyLimiter_.active();
    recordStereoStageProbe("post-EMT safety limiter", stageProbeScratch_, out, block.numFrames);

    if (stageProbeEnabled_)
        stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
    processStereoNam(finalHiloNamAdapter_, out, block.numFrames);
    recordStereoStageProbe("final Hilo NAM", stageProbeScratch_, out, block.numFrames);

    if (stageProbeEnabled_)
        stageProbeScratch_.assign(out.begin(), out.begin() + static_cast<std::ptrdiff_t>(block.numFrames * 2));
    limiter_.setEnabled(true);
    limiter_.processInterleavedStereo(out.data(), static_cast<int>(block.numFrames));
    lastMeters_.finalSafetyLimiterActive = limiter_.active();
    recordStereoStageProbe("final safety limiter", stageProbeScratch_, out, block.numFrames);

    for (std::size_t frame = 0; frame < block.numFrames; ++frame)
    {
        updatePeak(lastMeters_.masterLeftPeak, out[frame * 2]);
        updatePeak(lastMeters_.masterRightPeak, out[frame * 2 + 1]);
    }
    lastMeters_.limiterActive = lastMeters_.postEmtSafetyLimiterActive || lastMeters_.finalSafetyLimiterActive;

    // Meters live in SessionState; UI/controller can copy these peaks into mutable state.
    (void) sampleRate_;
    (void) maxBlockSize_;
}

} // namespace geilalizer::dsp
