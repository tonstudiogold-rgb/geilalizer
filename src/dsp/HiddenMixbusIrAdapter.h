#pragma once

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace geilalizer::dsp
{

class HiddenMixbusIrAdapter
{
public:
    static constexpr int kSlotCount = 4;
    static constexpr int kMaxIrTaps = 4096;

    struct IrSlot
    {
        std::string driveLabel;
        std::string fileName;
        float minimumPeakDb = -120.0f;
        std::vector<float> leftTaps;
        std::vector<float> rightTaps;
    };

    HiddenMixbusIrAdapter();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    bool loadMixbusDirectory(const std::filesystem::path& directory);
    bool loadMixbusDirectoryIfPresent();
    bool hasAllMixbusIrs() const;

    int slotIndexForPeakDb(float peakDb) const;
    const IrSlot& slotForPeakDb(float peakDb) const;
    const std::array<IrSlot, kSlotCount>& slots() const { return slots_; }
    void setSlots(std::array<IrSlot, kSlotCount> slots);
    int activeSlotIndex() const { return activeSlotIndex_; }

    void processInterleavedStereo(float peakDbBeforePreLimiter, float* samples, std::size_t numFrames);

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

    static bool readWavAsStereoFloat(const std::filesystem::path& file, std::vector<float>& leftTaps, std::vector<float>& rightTaps);
    static void trimAndNormalizeIrPair(std::vector<float>& leftTaps, std::vector<float>& rightTaps);

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 0;
    std::array<IrSlot, kSlotCount> slots_;
    FirConvolver leftConvolver_;
    FirConvolver rightConvolver_;
    int activeSlotIndex_ = 1;
};

} // namespace geilalizer::dsp
