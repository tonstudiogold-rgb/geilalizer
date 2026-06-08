#include "HiddenIrAdapter.h"

#include "AssetPath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>

namespace geilalizer::dsp
{
namespace
{
std::uint16_t readU16(const std::vector<unsigned char>& data, std::size_t offset)
{
    return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
}

std::uint32_t readU32(const std::vector<unsigned char>& data, std::size_t offset)
{
    return static_cast<std::uint32_t>(data[offset]
        | (data[offset + 1] << 8)
        | (data[offset + 2] << 16)
        | (data[offset + 3] << 24));
}

std::int32_t readSigned24(const unsigned char* p)
{
    std::int32_t value = static_cast<std::int32_t>(p[0] | (p[1] << 8) | (p[2] << 16));
    if ((value & 0x00800000) != 0)
        value |= static_cast<std::int32_t>(0xff000000);
    return value;
}
} // namespace

HiddenIrAdapter::HiddenIrAdapter()
{
    const auto& names = preampFileNames();
    const auto& tweaks = preampOriginalTweaks();
    for (std::size_t i = 0; i < preampIrSlots_.size(); ++i)
    {
        preampIrSlots_[i].fileName = std::string(names[i]);
        preampIrSlots_[i].originalTweak = std::string(tweaks[i]);
    }
}

void HiddenIrAdapter::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max(0, maxBlockSize);
    crossfadeSamples_ = std::max(1, static_cast<int>(std::round(sampleRate_ * static_cast<double>(kCrossfadeMs) / 1000.0)));

    for (auto& channel : channels_)
    {
        channel.current.prepare(kMaxIrTaps);
        channel.previous.prepare(kMaxIrTaps);
        channel.activePreampIndex = kDefaultPreampIndex;
        channel.pendingPreampIndex = kDefaultPreampIndex;
        channel.crossfadeSamplesRemaining = 0;
        channel.crossfadeSamplesTotal = crossfadeSamples_;
        channel.current.setIr(tapsForIndex(kDefaultPreampIndex));
        channel.previous.setIr(nullptr);
    }

    (void) maxBlockSize_;
}

void HiddenIrAdapter::reset()
{
    for (auto& channel : channels_)
    {
        channel.current.reset();
        channel.previous.reset();
        channel.crossfadeSamplesRemaining = 0;
    }
}

bool HiddenIrAdapter::loadNeve1073Directory(const std::filesystem::path& directory)
{
    bool loadedAny = false;
    const auto& names = preampFileNames();

    for (std::size_t i = 0; i < names.size(); ++i)
    {
        std::vector<float> taps;
        if (readWavAsMonoFloat(directory / std::string(names[i]), taps))
        {
            trimAndNormalizeIr(taps);
            preampIrSlots_[i].monoTaps = std::move(taps);
            loadedAny = true;
        }
    }

    for (auto& channel : channels_)
        channel.current.setIr(tapsForIndex(channel.activePreampIndex));

    return loadedAny;
}

bool HiddenIrAdapter::loadNeve1073DirectoryIfPresent()
{
    const std::array<std::filesystem::path, 4> candidates {
        resolveProductAssetPath("private_nam_models/model_001"),
        std::filesystem::path("private_nam_models/model_001"),
        std::filesystem::path("/opt/data/geilalizer/private_nam_models/model_001"),
        std::filesystem::current_path() / "private_nam_models/model_001"
    };

    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(candidate, ec) && loadNeve1073Directory(candidate))
            return true;
    }

    return false;
}

bool HiddenIrAdapter::hasAnyIr() const
{
    return std::any_of(preampIrSlots_.begin(), preampIrSlots_.end(),
                       [](const IrSlot& slot) { return !slot.monoTaps.empty(); });
}

bool HiddenIrAdapter::hasIrForPreampIndex(int preampIndex) const
{
    return tapsForIndex(preampIndex) != nullptr;
}

void HiddenIrAdapter::processChannel(std::size_t channelIndex, int preampIndex, float* samples, std::size_t numFrames)
{
    if (samples == nullptr || numFrames == 0 || channelIndex >= channels_.size())
        return;

    auto& channel = channels_[channelIndex];
    const int wantedIndex = clampPreampIndex(preampIndex);
    if (wantedIndex != channel.activePreampIndex)
        startSwitch(channel, wantedIndex);

    if (channel.current.ir() == nullptr && channel.previous.ir() == nullptr)
        return;

    for (std::size_t i = 0; i < numFrames; ++i)
    {
        const float input = samples[i];
        const float current = channel.current.processSample(input);

        if (channel.crossfadeSamplesRemaining > 0 && channel.previous.ir() != nullptr)
        {
            const float old = channel.previous.processSample(input);
            const float fadeIn = 1.0f - (static_cast<float>(channel.crossfadeSamplesRemaining)
                / static_cast<float>(std::max(1, channel.crossfadeSamplesTotal)));
            samples[i] = old + (current - old) * fadeIn;
            --channel.crossfadeSamplesRemaining;
            if (channel.crossfadeSamplesRemaining == 0)
                channel.previous.setIr(nullptr);
        }
        else
        {
            samples[i] = current;
        }
    }
}

void HiddenIrAdapter::FirConvolver::prepare(std::size_t maxTapCount)
{
    history_.assign(std::max<std::size_t>(1, maxTapCount), 0.0f);
    writePosition_ = 0;
}

void HiddenIrAdapter::FirConvolver::reset()
{
    std::fill(history_.begin(), history_.end(), 0.0f);
    writePosition_ = 0;
}

void HiddenIrAdapter::FirConvolver::setIr(const std::vector<float>* ir)
{
    ir_ = (ir != nullptr && !ir->empty()) ? ir : nullptr;
}

void HiddenIrAdapter::FirConvolver::copyFrom(const FirConvolver& other)
{
    ir_ = other.ir_;
    if (history_.size() == other.history_.size())
        std::copy(other.history_.begin(), other.history_.end(), history_.begin());
    writePosition_ = other.writePosition_;
}

float HiddenIrAdapter::FirConvolver::processSample(float input)
{
    if (ir_ == nullptr || ir_->empty())
        return input;

    history_[writePosition_] = input;
    float output = 0.0f;
    std::size_t historyIndex = writePosition_;
    const std::size_t tapsToUse = std::min(ir_->size(), history_.size());

    for (std::size_t tap = 0; tap < tapsToUse; ++tap)
    {
        output += (*ir_)[tap] * history_[historyIndex];
        historyIndex = historyIndex == 0 ? history_.size() - 1 : historyIndex - 1;
    }

    ++writePosition_;
    if (writePosition_ >= history_.size())
        writePosition_ = 0;

    return output;
}

int HiddenIrAdapter::clampPreampIndex(int preampIndex)
{
    return std::clamp(preampIndex, kPreampMinIndex, kPreampMaxIndex);
}

const std::array<std::string_view, 11>& HiddenIrAdapter::preampFileNames()
{
    static constexpr std::array<std::string_view, 11> names {
        "Neve 20.wav",
        "Neve 30.wav",
        "Neve 40.wav",
        "Neve 45.wav",
        "Neve 50.wav",
        "Neve 55.wav",
        "Neve 60.wav",
        "Neve 65.wav",
        "Neve 70.wav",
        "Neve 75.wav",
        "Neve 80.wav"
    };
    return names;
}

const std::array<std::string_view, 11>& HiddenIrAdapter::preampOriginalTweaks()
{
    static constexpr std::array<std::string_view, 11> tweaks {
        "Mic/Preamp Gain ca. 20 dB",
        "Mic/Preamp Gain ca. 30 dB",
        "Mic/Preamp Gain ca. 40 dB",
        "Mic/Preamp Gain ca. 45 dB / Zwischenposition",
        "Mic/Preamp Gain ca. 50 dB",
        "Mic/Preamp Gain ca. 55 dB / Zwischenposition",
        "Mic/Preamp Gain ca. 60 dB",
        "Mic/Preamp Gain ca. 65 dB / Zwischenposition",
        "Mic/Preamp Gain ca. 70 dB / Hero Push",
        "Mic/Preamp Gain ca. 75 dB / Zwischenposition hot",
        "Mic/Preamp Gain ca. 80 dB / Maximum hot"
    };
    return tweaks;
}

bool HiddenIrAdapter::readWavAsMonoFloat(const std::filesystem::path& file, std::vector<float>& monoTaps)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return false;

    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 44 || std::string_view(reinterpret_cast<const char*>(bytes.data()), 4) != "RIFF"
        || std::string_view(reinterpret_cast<const char*>(bytes.data() + 8), 4) != "WAVE")
        return false;

    std::size_t cursor = 12;
    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint16_t bitsPerSample = 0;
    std::size_t dataOffset = 0;
    std::size_t dataSize = 0;

    while (cursor + 8 <= bytes.size())
    {
        const auto chunkId = std::string_view(reinterpret_cast<const char*>(bytes.data() + cursor), 4);
        const std::uint32_t chunkSize = readU32(bytes, cursor + 4);
        const std::size_t payload = cursor + 8;
        if (payload + chunkSize > bytes.size())
            break;

        if (chunkId == "fmt ")
        {
            if (chunkSize < 16)
                return false;
            audioFormat = readU16(bytes, payload);
            channels = readU16(bytes, payload + 2);
            bitsPerSample = readU16(bytes, payload + 14);
        }
        else if (chunkId == "data")
        {
            dataOffset = payload;
            dataSize = chunkSize;
        }

        cursor = payload + chunkSize + (chunkSize % 2);
    }

    if (audioFormat != 1 || channels == 0 || bitsPerSample != 24 || dataOffset == 0 || dataSize == 0)
        return false;

    const std::size_t bytesPerSample = bitsPerSample / 8;
    const std::size_t frameSize = static_cast<std::size_t>(channels) * bytesPerSample;
    const std::size_t frames = dataSize / frameSize;
    monoTaps.clear();
    monoTaps.reserve(std::min<std::size_t>(frames, kMaxIrTaps));

    for (std::size_t frame = 0; frame < frames && monoTaps.size() < kMaxIrTaps; ++frame)
    {
        float sum = 0.0f;
        for (std::uint16_t ch = 0; ch < channels; ++ch)
        {
            const auto* sample = bytes.data() + dataOffset + frame * frameSize + static_cast<std::size_t>(ch) * bytesPerSample;
            sum += static_cast<float>(readSigned24(sample)) / 8388608.0f;
        }
        monoTaps.push_back(sum / static_cast<float>(channels));
    }

    return !monoTaps.empty();
}

void HiddenIrAdapter::trimAndNormalizeIr(std::vector<float>& monoTaps)
{
    if (monoTaps.empty())
        return;

    const auto peakIt = std::max_element(monoTaps.begin(), monoTaps.end(),
        [](float a, float b) { return std::abs(a) < std::abs(b); });
    const auto peakIndex = static_cast<std::size_t>(std::distance(monoTaps.begin(), peakIt));
    const std::size_t preRoll = std::min<std::size_t>(16, peakIndex);
    const std::size_t start = peakIndex - preRoll;
    const std::size_t count = std::min<std::size_t>(kMaxIrTaps, monoTaps.size() - start);

    std::vector<float> trimmed(monoTaps.begin() + static_cast<std::ptrdiff_t>(start),
                               monoTaps.begin() + static_cast<std::ptrdiff_t>(start + count));

    // Preserve the captured tone but make bypass/smoke behavior sane: peak-normalize near unity.
    float peak = 0.0f;
    for (float sample : trimmed)
        peak = std::max(peak, std::abs(sample));
    if (peak > std::numeric_limits<float>::epsilon())
    {
        const float scale = 1.0f / peak;
        for (float& sample : trimmed)
            sample *= scale;
    }

    monoTaps = std::move(trimmed);
}

void HiddenIrAdapter::startSwitch(ChannelSlot& channel, int newPreampIndex)
{
    channel.previous.copyFrom(channel.current);
    channel.current.reset();
    channel.current.setIr(tapsForIndex(newPreampIndex));
    channel.activePreampIndex = newPreampIndex;
    channel.pendingPreampIndex = newPreampIndex;
    channel.crossfadeSamplesTotal = crossfadeSamples_;
    channel.crossfadeSamplesRemaining = crossfadeSamples_;
}

const std::vector<float>* HiddenIrAdapter::tapsForIndex(int preampIndex) const
{
    const int index = clampPreampIndex(preampIndex);
    const auto& taps = preampIrSlots_[static_cast<std::size_t>(index)].monoTaps;
    return taps.empty() ? nullptr : &taps;
}

} // namespace geilalizer::dsp
