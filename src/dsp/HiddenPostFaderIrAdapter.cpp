#include "HiddenPostFaderIrAdapter.h"

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

HiddenPostFaderIrAdapter::HiddenPostFaderIrAdapter()
{
    slots_[0] = { "0db", "4000 G Channel 0db_dc.wav", {} };
    slots_[1] = { "5db", "4000 G Channel 5db_dc.wav", {} };
    slots_[2] = { "10db", "4000 G Channel 10db_dc.wav", {} };
    activeSlotIndexes_.fill(1);
}

void HiddenPostFaderIrAdapter::prepare(double sampleRate, int maxBlockSize)
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max(0, maxBlockSize);
    for (auto& convolver : convolvers_)
        convolver.prepare(kMaxIrTaps);
    activeSlotIndexes_.fill(1);
    (void) sampleRate_;
    (void) maxBlockSize_;
}

void HiddenPostFaderIrAdapter::reset()
{
    for (auto& convolver : convolvers_)
        convolver.reset();
}

bool HiddenPostFaderIrAdapter::loadSsl4000GDirectory(const std::filesystem::path& directory)
{
    bool loadedAny = false;
    for (auto& slot : slots_)
    {
        std::vector<float> taps;
        if (readWavAsMonoFloat(directory / slot.fileName, taps))
        {
            trimAndNormalizeIr(taps);
            slot.monoTaps = std::move(taps);
            loadedAny = true;
        }
    }
    return loadedAny;
}

bool HiddenPostFaderIrAdapter::loadSsl4000GDirectoryIfPresent()
{
    const std::array<std::filesystem::path, 4> candidates {
        resolveProductAssetPath("private_nam_models/model_004_post_fader_irs"),
        std::filesystem::path("private_nam_models/model_004_post_fader_irs"),
        std::filesystem::path("/opt/data/geilalizer/private_nam_models/model_004_post_fader_irs"),
        std::filesystem::current_path() / "private_nam_models/model_004_post_fader_irs"
    };

    for (const auto& candidate : candidates)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(candidate, ec) && loadSsl4000GDirectory(candidate))
            return true;
    }
    return false;
}

bool HiddenPostFaderIrAdapter::hasAllFaderThirdIrs() const
{
    return std::all_of(slots_.begin(), slots_.end(), [](const IrSlot& slot) { return !slot.monoTaps.empty(); });
}

void HiddenPostFaderIrAdapter::setSlots(std::array<IrSlot, kSlotCount> slots)
{
    slots_ = std::move(slots);
    for (auto& convolver : convolvers_)
        convolver.reset();
}

int HiddenPostFaderIrAdapter::slotIndexForFaderPosition(float normalizedFaderPosition) const
{
    const float p = std::clamp(normalizedFaderPosition, 0.0f, 1.0f);
    if (p < (1.0f / 3.0f))
        return 0;
    if (p < (2.0f / 3.0f))
        return 1;
    return 2;
}

int HiddenPostFaderIrAdapter::slotIndexForFaderDb(float faderGainDb) const
{
    const float p = (std::clamp(faderGainDb, core::kFaderGainMinDb, core::kFaderGainMaxDb) - core::kFaderGainMinDb)
        / (core::kFaderGainMaxDb - core::kFaderGainMinDb);
    return slotIndexForFaderPosition(p);
}

const HiddenPostFaderIrAdapter::IrSlot& HiddenPostFaderIrAdapter::slotForFaderPosition(float normalizedFaderPosition) const
{
    return slots_[static_cast<std::size_t>(slotIndexForFaderPosition(normalizedFaderPosition))];
}

const HiddenPostFaderIrAdapter::IrSlot& HiddenPostFaderIrAdapter::slotForFaderDb(float faderGainDb) const
{
    return slots_[static_cast<std::size_t>(slotIndexForFaderDb(faderGainDb))];
}

void HiddenPostFaderIrAdapter::processChannel(std::size_t channelIndex, float faderGainDb, float* samples, std::size_t numFrames)
{
    if (samples == nullptr || numFrames == 0 || channelIndex >= convolvers_.size())
        return;

    const int slotIndex = slotIndexForFaderDb(faderGainDb);
    const auto& slot = slots_[static_cast<std::size_t>(slotIndex)];
    if (slot.monoTaps.empty())
        return;

    auto& convolver = convolvers_[channelIndex];
    if (activeSlotIndexes_[channelIndex] != slotIndex)
    {
        convolver.reset();
        activeSlotIndexes_[channelIndex] = slotIndex;
    }
    convolver.setIr(&slot.monoTaps);

    for (std::size_t i = 0; i < numFrames; ++i)
        samples[i] = convolver.processSample(samples[i]);
}

void HiddenPostFaderIrAdapter::FirConvolver::prepare(std::size_t maxTapCount)
{
    history_.assign(std::max<std::size_t>(1, maxTapCount), 0.0f);
    writePosition_ = 0;
}

void HiddenPostFaderIrAdapter::FirConvolver::reset()
{
    std::fill(history_.begin(), history_.end(), 0.0f);
    writePosition_ = 0;
}

void HiddenPostFaderIrAdapter::FirConvolver::setIr(const std::vector<float>* ir)
{
    ir_ = (ir != nullptr && !ir->empty()) ? ir : nullptr;
}

float HiddenPostFaderIrAdapter::FirConvolver::processSample(float input)
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

bool HiddenPostFaderIrAdapter::readWavAsMonoFloat(const std::filesystem::path& file, std::vector<float>& monoTaps)
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
    monoTaps.clear();
    monoTaps.reserve(frames);

    for (std::size_t frame = 0; frame < frames; ++frame)
    {
        float sum = 0.0f;
        for (std::uint16_t ch = 0; ch < channels; ++ch)
        {
            const unsigned char* p = bytes.data() + dataOffset + frame * frameSize + ch * bytesPerSample;
            if (bitsPerSample == 16)
            {
                const auto v = static_cast<std::int16_t>(readU16(bytes, static_cast<std::size_t>(p - bytes.data())));
                sum += static_cast<float>(v) / 32768.0f;
            }
            else if (bitsPerSample == 24)
            {
                sum += static_cast<float>(readSigned24(p)) / 8388608.0f;
            }
            else
            {
                const auto u = readU32(bytes, static_cast<std::size_t>(p - bytes.data()));
                const auto v = static_cast<std::int32_t>(u);
                sum += static_cast<float>(v) / 2147483648.0f;
            }
        }
        monoTaps.push_back(sum / static_cast<float>(channels));
    }
    return !monoTaps.empty();
}

void HiddenPostFaderIrAdapter::trimAndNormalizeIr(std::vector<float>& monoTaps)
{
    auto firstAudible = std::find_if(monoTaps.begin(), monoTaps.end(), [](float v) { return std::abs(v) > 1.0e-6f; });
    if (firstAudible != monoTaps.end())
        monoTaps.erase(monoTaps.begin(), firstAudible);

    if (monoTaps.size() > static_cast<std::size_t>(kMaxIrTaps))
        monoTaps.resize(static_cast<std::size_t>(kMaxIrTaps));

    float peak = 0.0f;
    for (float v : monoTaps)
        peak = std::max(peak, std::abs(v));

    if (peak > std::numeric_limits<float>::epsilon())
    {
        const float scale = 1.0f / peak;
        for (float& v : monoTaps)
            v *= scale;
    }
}

} // namespace geilalizer::dsp
