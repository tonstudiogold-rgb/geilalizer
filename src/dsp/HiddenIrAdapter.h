#pragma once

#include "../core/AudioConstants.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace geilalizer::dsp
{

class HiddenIrAdapter
{
public:
    static constexpr int kPreampMinIndex = 0;
    static constexpr int kPreampMaxIndex = 10;
    static constexpr int kDefaultPreampIndex = 5; // Neve 55.wav
    static constexpr int kMaxIrTaps = 4096;
    static constexpr int kCrossfadeMs = 25;

    struct IrSlot
    {
        std::string fileName;
        std::string originalTweak;
        std::vector<float> monoTaps;
    };

    HiddenIrAdapter();

    void prepare(double sampleRate, int maxBlockSize);
    void reset();

    bool loadNeve1073Directory(const std::filesystem::path& directory);
    bool loadNeve1073DirectoryIfPresent();
    bool hasAnyIr() const;
    bool hasIrForPreampIndex(int preampIndex) const;

    int defaultPreampIndex() const { return kDefaultPreampIndex; }
    const std::array<IrSlot, 11>& preampIrSlots() const { return preampIrSlots_; }
    void setPreampIrSlots(std::array<IrSlot, 11> slots);

    void processChannel(std::size_t channelIndex, int preampIndex, float* samples, std::size_t numFrames);

private:
    class FirConvolver
    {
    public:
        void prepare(std::size_t maxTapCount);
        void reset();
        void setIr(const std::vector<float>* ir);
        void copyFrom(const FirConvolver& other);
        float processSample(float input);
        const std::vector<float>* ir() const { return ir_; }

    private:
        const std::vector<float>* ir_ = nullptr;
        std::vector<float> history_;
        std::size_t writePosition_ = 0;
    };

    struct ChannelSlot
    {
        int activePreampIndex = kDefaultPreampIndex;
        int pendingPreampIndex = kDefaultPreampIndex;
        int crossfadeSamplesRemaining = 0;
        int crossfadeSamplesTotal = 0;
        FirConvolver current;
        FirConvolver previous;
    };

    static int clampPreampIndex(int preampIndex);
    static const std::array<std::string_view, 11>& preampFileNames();
    static const std::array<std::string_view, 11>& preampOriginalTweaks();
    static bool readWavAsMonoFloat(const std::filesystem::path& file, std::vector<float>& monoTaps);
    static void trimAndNormalizeIr(std::vector<float>& monoTaps);

    void startSwitch(ChannelSlot& channel, int newPreampIndex);
    const std::vector<float>* tapsForIndex(int preampIndex) const;

    double sampleRate_ = 44100.0;
    int maxBlockSize_ = 0;
    int crossfadeSamples_ = 1102;
    std::array<IrSlot, 11> preampIrSlots_;
    std::array<ChannelSlot, core::kMaxMonoChannels> channels_;
};

} // namespace geilalizer::dsp
