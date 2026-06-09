#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace geilalizer::core
{

inline std::vector<float> resampleLinear(const std::vector<float>& input,
                                         double sourceSampleRate,
                                         double targetSampleRate)
{
    if (input.empty())
        return {};

    if (! std::isfinite(sourceSampleRate) || ! std::isfinite(targetSampleRate)
        || sourceSampleRate <= 0.0 || targetSampleRate <= 0.0)
        return input;

    if (std::abs(sourceSampleRate - targetSampleRate) < 0.5)
        return input;

    const double ratio = targetSampleRate / sourceSampleRate;
    const auto outputFrames = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(static_cast<double>(input.size()) * ratio)));
    std::vector<float> output(outputFrames, 0.0f);

    for (std::size_t i = 0; i < output.size(); ++i)
    {
        const double sourcePosition = static_cast<double>(i) / ratio;
        const auto index0 = static_cast<std::size_t>(std::floor(sourcePosition));
        const auto index1 = std::min(index0 + 1, input.size() - 1);
        const float fraction = static_cast<float>(sourcePosition - static_cast<double>(index0));
        const float a = input[std::min(index0, input.size() - 1)];
        const float b = input[index1];
        output[i] = a + (b - a) * fraction;
    }

    return output;
}

} // namespace geilalizer::core
