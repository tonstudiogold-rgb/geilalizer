#pragma once

#include "../core/AudioConstants.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace geilalizer::dsp
{

class HiddenPostFaderIrAdapter
{
public:
    static constexpr int kSlotCount = 3;
    static constexpr int kMaxIrTaps = 4096;

    struct IrSlot
    {
        std::string driveLabel;
        std::string fileName;
        std::vector<float> monoTaps;
    };

    HiddenPostFaderIrAdapter();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    bool loadSsl4000GDirectory(const std::filesystem::path& directory);
    bool loadSsl4000GDirectoryIfPresent();
    bool hasAllFaderThirdIrs() const;

    int slotIndexForFaderPosition(float normalizedFaderPosition) const;
    int slotIndexForFaderDb(float faderGainDb) const;
    const IrSlot& slotForFaderPosition(float normalizedFaderPosition) const;
    const IrSlot& slotForFaderDb(float faderGainDb) const;
    const std::array<IrSlot, kSlotCount>& slots() const { return slots_; }
    void setSlots(std::array<IrSlot, kSlotCount> slots);

    void processChannel(std::size_t channelIndex, float faderGainDb, float* samples, std::size_t numFrames);

private:
    class FirConvolver
    {
    public:
        void prepare(std::size_t maxTapCount);
        void reset();
        void setIr(const std::vector<float>* ir);
        float processSample(float input);

    private:
        const std::vector<float>* ir_ = nullptr;
        std::vector<float> history_;
        std::size_t writePosition_ = 0;
    };

    static bool readWavAsMonoFloat(const std::filesystem::path& file, std::vector<float>& monoTaps);
    static void trimAndNormalizeIr(std::vector<float>& monoTaps);

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 0;
    std::array<IrSlot, kSlotCount> slots_;
    std::array<FirConvolver, core::kMaxMonoChannels> convolvers_;
    std::array<int, core::kMaxMonoChannels> activeSlotIndexes_;
};

} // namespace geilalizer::dsp
