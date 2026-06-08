#include "MachineProcessor.h"

#include "AssetPath.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>

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

void MachineProcessor::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max(0, maxBlockSize);
    for (auto& scratch : channelScratch_)
        scratch.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    masterLeftScratch_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    masterRightScratch_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0f);
    namAdapter_.prepare(sampleRate_, maxBlockSize_);
    tapeNamAdapter_.prepare(sampleRate_, maxBlockSize_);
    masterTapeNamAdapter_.prepare(sampleRate_, maxBlockSize_);
    emtLimiterNamAdapter_.prepare(sampleRate_, maxBlockSize_);
    finalHiloNamAdapter_.prepare(sampleRate_, maxBlockSize_);
    irAdapter_.prepare(sampleRate_, maxBlockSize_);
    postFaderIrAdapter_.prepare(sampleRate_, maxBlockSize_);
    mixbusIrAdapter_.prepare(sampleRate_, maxBlockSize_);
    irAdapter_.loadNeve1073DirectoryIfPresent();
    postFaderIrAdapter_.loadSsl4000GDirectoryIfPresent();
    mixbusIrAdapter_.loadMixbusDirectoryIfPresent();

    const std::filesystem::path fixedConsoleNamPath = resolveProductAssetPath(fixedConsoleNamModelPath());
    if (std::filesystem::exists(fixedConsoleNamPath))
    {
        namAdapter_.bindFixedInternalModelForAllChannels({
            fixedConsoleNamPath.string(),
            "fixed hidden Neve/console NAM after selected IR and before tape/fader coloration stages" });
    }

    const std::filesystem::path fixedTapeNamPath = resolveProductAssetPath(fixedTapeNamModelPath());
    if (std::filesystem::exists(fixedTapeNamPath))
    {
        tapeNamAdapter_.bindFixedInternalModelForAllChannels({
            fixedTapeNamPath.string(),
            "fixed hidden Studer C37 tape NAM after console NAM and before fader coloration stages" });
        masterTapeNamAdapter_.bindSingleInternalModelForNextIntegration(
            0, { fixedTapeNamPath.string(), "fixed hidden Studer C37 tape NAM again on left mixbus after Model 5" });
        masterTapeNamAdapter_.bindSingleInternalModelForNextIntegration(
            1, { fixedTapeNamPath.string(), "fixed hidden Studer C37 tape NAM again on right mixbus after Model 5" });
    }

    const std::filesystem::path fixedEmtLimiterNamPath = resolveProductAssetPath(fixedEmtLimiterNamModelPath());
    if (std::filesystem::exists(fixedEmtLimiterNamPath))
    {
        emtLimiterNamAdapter_.bindSingleInternalModelForNextIntegration(
            0, { fixedEmtLimiterNamPath.string(), "switchable hidden EMT 266X NAM limiter on left master bus" });
        emtLimiterNamAdapter_.bindSingleInternalModelForNextIntegration(
            1, { fixedEmtLimiterNamPath.string(), "switchable hidden EMT 266X NAM limiter on right master bus" });
    }

    const std::filesystem::path fixedFinalHiloNamPath = resolveProductAssetPath(fixedFinalHiloNamModelPath());
    if (std::filesystem::exists(fixedFinalHiloNamPath))
    {
        finalHiloNamAdapter_.bindSingleInternalModelForNextIntegration(
            0, { fixedFinalHiloNamPath.string(), "fixed final Hilo NAM 1 on left output after post-EMT safety limiter" });
        finalHiloNamAdapter_.bindSingleInternalModelForNextIntegration(
            1, { fixedFinalHiloNamPath.string(), "fixed final Hilo NAM 1 on right output after post-EMT safety limiter" });
    }

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
    lastMeters_ = {};
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

void MachineProcessor::process(const core::SessionState& session, AudioBlockView block)
{
    ensureStereoOutput(block);
    lastMeters_ = {};
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

        // Product order: input gain drives the selected per-channel IR/preamp colour first.
        // The hidden console NAM and tape NAM are fixed stages after that.
        // Future hidden fader model stage belongs between tape NAM and visible fader gain.
        irAdapter_.processChannel(channelIndex, channel.preampIndex, scratch.data(), frames);
        namAdapter_.processChannel(channelIndex, scratch.data(), frames);
        tapeNamAdapter_.processChannel(channelIndex, scratch.data(), frames);

        for (std::size_t frame = 0; frame < frames; ++frame)
            scratch[frame] *= fader;

        postFaderIrAdapter_.processChannel(channelIndex, channel.faderGainDb, scratch.data(), frames);

        for (std::size_t frame = 0; frame < frames; ++frame)
        {
            const float mono = scratch[frame];
            out[frame * 2] += mono * leftPan;
            out[frame * 2 + 1] += mono * rightPan;
        }
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
    mixbusPreIrLimiter_.setEnabled(true);
    mixbusPreIrLimiter_.processInterleavedStereo(out.data(), static_cast<int>(block.numFrames));
    lastMeters_.mixbusPreLimiterActive = mixbusPreIrLimiter_.active();
    mixbusIrAdapter_.processInterleavedStereo(lastMeters_.mixbusPreLimiterPeakDb, out.data(), block.numFrames);

    // Corrected master chain: Model 5 feeds a second fixed Model 3 tape pass on the stereo bus.
    processStereoNam(masterTapeNamAdapter_, out, block.numFrames);

    const float outputTrim = dbToLinear(session.master.outputTrimDb);
    for (float& sample : out)
        sample *= outputTrim;

    // The visible limiter switch controls only the EMT 266X NAM limiter stage.
    // Safety limiters are fixed protection stages and stay on.
    if (session.master.limiterEnabled)
        processStereoNam(emtLimiterNamAdapter_, out, block.numFrames);

    postEmtSafetyLimiter_.setEnabled(true);
    postEmtSafetyLimiter_.processInterleavedStereo(out.data(), static_cast<int>(block.numFrames));
    lastMeters_.postEmtSafetyLimiterActive = postEmtSafetyLimiter_.active();

    processStereoNam(finalHiloNamAdapter_, out, block.numFrames);

    limiter_.setEnabled(true);
    limiter_.processInterleavedStereo(out.data(), static_cast<int>(block.numFrames));
    lastMeters_.finalSafetyLimiterActive = limiter_.active();

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
