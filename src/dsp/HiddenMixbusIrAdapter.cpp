#include "HiddenMixbusIrAdapter.h"

#include "AssetPath.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <string_view>

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

HiddenMixbusIrAdapter::HiddenMixbusIrAdapter()
{
    slots_[0] = { "0db", "4000 G mixbuss 0db_dc.wav", -120.0f, {}, {} };
    slots_[1] = { "5db", "4000 G mixbuss 5db_dc.wav", -12.0f, {}, {} };
    slots_[2] = { "10db", "4000 G mixbuss 10db_dc.wav", -7.0f, {}, {} };
    slots_[3] = { "extreme1", "4000 G mixbuss extreme 1_dc.wav", -3.0f, {}, {} };
}

void HiddenMixbusIrAdapter::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max(0, maxBlockSize);
    leftConvolver_.prepare(kMaxIrTaps);
    rightConvolver_.prepare(kMaxIrTaps);
    activeSlotIndex_ = 1;
    (void) sampleRate_;
    (void) maxBlockSize_;
}

void HiddenMixbusIrAdapter::reset()
{
    leftConvolver_.reset();
    rightConvolver_.reset();
    activeSlotIndex_ = 1;
}

bool HiddenMixbusIrAdapter::loadMixbusDirectory(const std::filesystem::path& directory)
{
    bool loadedAny = false;
    for (auto& slot : slots_)
    {
        std::vector<float> leftTaps;
        std::vector<float> rightTaps;
        if (readWavAsStereoFloat(directory / slot.fileName, leftTaps, rightTaps))
        {
            trimAndNormalizeIrPair(leftTaps, rightTaps);
            slot.leftTaps = std::move(leftTaps);
            slot.rightTaps = std::move(rightTaps);
            loadedAny = true;
        }
    }
    return loadedAny;
}

bool HiddenMixbusIrAdapter::loadMixbusDirectoryIfPresent()
{
    const std::array<std::filesystem::path, 4> candidates {
        resolveProductAssetPath("private_nam_models/model_005_mixbus_irs"),
        std::filesystem::path("private_nam_models/model_005_mixbus_irs"),
        std::filesystem::path("/opt/data/geilalizer/private_nam_models/model_005_mixbus_irs"),
        std::filesystem::current_path() / "private_nam_models/model_005_mixbus_irs"
    };

    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(candidate, ec) && loadMixbusDirectory(candidate))
            return true;
    }
    return false;
}

bool HiddenMixbusIrAdapter::hasAllMixbusIrs() const
{
    return std::all_of(slots_.begin(), slots_.end(), [](const IrSlot& slot) {
        return !slot.leftTaps.empty() && !slot.rightTaps.empty();
    });
}

void HiddenMixbusIrAdapter::setSlots(std::array<IrSlot, kSlotCount> slots)
{
    slots_ = std::move(slots);
    leftConvolver_.reset();
    rightConvolver_.reset();
}

int HiddenMixbusIrAdapter::slotIndexForPeakDb(float peakDb) const
{
    if (!std::isfinite(peakDb))
        return 0;
    if (peakDb >= -3.0f)
        return 3;
    if (peakDb >= -7.0f)
        return 2;
    if (peakDb >= -12.0f)
        return 1;
    return 0;
}

const HiddenMixbusIrAdapter::IrSlot& HiddenMixbusIrAdapter::slotForPeakDb(float peakDb) const
{
    return slots_[static_cast<std::size_t>(slotIndexForPeakDb(peakDb))];
}

void HiddenMixbusIrAdapter::processInterleavedStereo(float peakDbBeforePreLimiter, float* samples, std::size_t numFrames)
{
    if (samples == nullptr || numFrames == 0)
        return;

    const int slotIndex = slotIndexForPeakDb(peakDbBeforePreLimiter);
    const auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
    if (slot.leftTaps.empty() || slot.rightTaps.empty())
        return;

    if (activeSlotIndex_ != slotIndex)
    {
        leftConvolver_.reset();
        rightConvolver_.reset();
        activeSlotIndex_ = slotIndex;
    }

    leftConvolver_.setIr(&slot.leftTaps);
    rightConvolver_.setIr(&slot.rightTaps);

    for (std::size_t frame = 0; frame < numFrames; ++frame)
    {
        samples[frame * 2] = leftConvolver_.processSample(samples[frame * 2]);
        samples[frame * 2 + 1] = rightConvolver_.processSample(samples[frame * 2 + 1]);
    }
}

void HiddenMixbusIrAdapter::FirConvolver::prepare(std::size_t maxTapCount)
{
    history_.assign(std::max<std::size_t>(1, maxTapCount), 0.0f);
    writePosition_ = 0;
}

void HiddenMixbusIrAdapter::FirConvolver::reset()
{
    std::fill(history_.begin(), history_.end(), 0.0f);
    writePosition_ = 0;
}

void HiddenMixbusIrAdapter::FirConvolver::setIr(const std::vector<float>* ir)
{
    ir_ = (ir != nullptr && !ir->empty()) ? ir : nullptr;
}

float HiddenMixbusIrAdapter::FirConvolver::processSample(float input)
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

bool HiddenMixbusIrAdapter::readWavAsStereoFloat(const std::filesystem::path& file, std::vector<float>& leftTaps, std::vector<float>& rightTaps)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return false;

    std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (bytes.size() < 44 || std::string_view(reinterpret_cast<const char*>(bytes.data()), 4) != "RIFF"
        || std::string_view(reinterpret_cast<const char*>(bytes.data() + 8), 4) != "WAVE")
        return false;

    std::uint16_t audioFormat = 0;
    std::uint16_t channels = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint32_t dataOffset = 0;
    std::uint32_t dataSize = 0;

    for (std::size_t offset = 12; offset + 8 <= bytes.size();)
    {
        const auto chunkSize = readU32(bytes, offset + 4);
        const auto chunkData = offset + 8;
        if (chunkData + chunkSize > bytes.size())
            break;

        const std::string_view id(reinterpret_cast<const char*>(bytes.data() + offset), 4);
        if (id == "fmt ")
        {
            if (chunkSize < 16)
                return false;
            audioFormat = readU16(bytes, chunkData);
            channels = readU16(bytes, chunkData + 2);
            bitsPerSample = readU16(bytes, chunkData + 14);
        }
        else if (id == "data")
        {
            dataOffset = static_cast<std::uint32_t>(chunkData);
            dataSize = chunkSize;
        }

        offset = chunkData + chunkSize + (chunkSize % 2);
    }

    if (audioFormat != 1 || channels == 0 || dataOffset == 0 || dataSize == 0)
        return false;

    const std::size_t bytesPerSample = bitsPerSample / 8;
    if (!((bitsPerSample == 16 && bytesPerSample == 2) || (bitsPerSample == 24 && bytesPerSample == 3) || (bitsPerSample == 32 && bytesPerSample == 4)))
        return false;

    const std::size_t frameSize = bytesPerSample * channels;
    const std::size_t frames = dataSize / frameSize;
    leftTaps.clear();
    rightTaps.clear();
    leftTaps.reserve(frames);
    rightTaps.reserve(frames);

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        float values[2] { 0.0f, 0.0f };
        for (std::uint16_t ch = 0; ch < channels; ++ch)
        {
            const unsigned char* p = bytes.data() + dataOffset + frame * frameSize + ch * bytesPerSample;
            float sample = 0.0f;
            if (bitsPerSample == 16)
            {
                const auto v = static_cast<std::int16_t>(readU16(bytes, static_cast<std::size_t>(p - bytes.data())));
                sample = static_cast<float>(v) / 32768.0f;
            }
            else if (bitsPerSample == 24)
            {
                sample = static_cast<float>(readSigned24(p)) / 8388608.0f;
            }
            else
            {
                const auto u = readU32(bytes, static_cast<std::size_t>(p - bytes.data()));
                const auto v = static_cast<std::int32_t>(u);
                sample = static_cast<float>(v) / 2147483648.0f;
            }

            if (ch == 0)
                values[0] = sample;
            else if (ch == 1)
                values[1] = sample;
        }

        if (channels == 1)
            values[1] = values[0];
        leftTaps.push_back(values[0]);
        rightTaps.push_back(values[1]);
    }
    return !leftTaps.empty() && !rightTaps.empty();
}

void HiddenMixbusIrAdapter::trimAndNormalizeIrPair(std::vector<float>& leftTaps, std::vector<float>& rightTaps)
{
    const auto size = std::min(leftTaps.size(), rightTaps.size());
    leftTaps.resize(size);
    rightTaps.resize(size);

    std::size_t firstAudibleIndex = 0;
    while (firstAudibleIndex < size
        && std::abs(leftTaps[firstAudibleIndex]) <= 1.0e-6f
        && std::abs(rightTaps[firstAudibleIndex]) <= 1.0e-6f)
        ++firstAudibleIndex;

    if (firstAudibleIndex > 0 && firstAudibleIndex < size)
    {
        leftTaps.erase(leftTaps.begin(), leftTaps.begin() + static_cast<std::ptrdiff_t>(firstAudibleIndex));
        rightTaps.erase(rightTaps.begin(), rightTaps.begin() + static_cast<std::ptrdiff_t>(firstAudibleIndex));
    }

    if (leftTaps.size() > static_cast<std::size_t>(kMaxIrTaps))
    {
        leftTaps.resize(static_cast<std::size_t>(kMaxIrTaps));
        rightTaps.resize(static_cast<std::size_t>(kMaxIrTaps));
    }

    float peak = 0.0f;
    for (float v : leftTaps)
        peak = std::max(peak, std::abs(v));
    for (float v : rightTaps)
        peak = std::max(peak, std::abs(v));

    if (peak > std::numeric_limits<float>::epsilon())
    {
        const float scale = 1.0f / peak;
        for (float& v : leftTaps)
            v *= scale;
        for (float& v : rightTaps)
            v *= scale;
    }
}

} // namespace geilalizer::dsp
