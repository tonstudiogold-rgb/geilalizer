#pragma once

#include <array>
#include <cstddef>

namespace geilalizer::core
{

constexpr std::size_t kMaxMonoChannels = 24;
constexpr float kInputGainMinDb = -24.0f;
constexpr float kInputGainMaxDb = 24.0f;
constexpr float kDefaultInputGainDb = 0.0f;
constexpr float kDefaultFaderGainDb = 0.0f;
constexpr float kDefaultPan = 0.0f;
constexpr bool kDefaultLimiterEnabled = true;
constexpr float kDefaultOutputTrimDb = 0.0f;

// Product safety limiter: threshold 0 dB, output ceiling -0.2 dBFS.
constexpr float kSafetyLimiterThresholdDb = 0.0f;
constexpr float kSafetyLimiterCeilingDb = -0.2f;

constexpr std::array<int, 2> kSupportedExportSampleRates { 44100, 48000 };
constexpr std::array<int, 2> kSupportedExportBitDepths { 16, 24 };

} // namespace geilalizer::core
