#include "dsp/HiddenNamAdapter.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <vector>

int main()
{
#ifndef GEILALIZER_USE_NAMCORE
    return 0;
#else
    using geilalizer::dsp::HiddenNamAdapter;

    const std::filesystem::path consoleModel { "private_nam_models/model_002/NEVE 5315 ANALOG CONSOLE.nam" };
    const std::filesystem::path tapeModel { "private_nam_models/model_003/STUDER C37 TUBE TAPE 15 IPS.nam" };
    if (!std::filesystem::exists(consoleModel) || !std::filesystem::exists(tapeModel))
        return 0; // Private model assets are intentionally not committed.

    HiddenNamAdapter adapter;
    adapter.prepare(44100.0, 64);
    assert(adapter.bindFixedInternalModelForAllChannels({ consoleModel.string(), "NAM adapter smoke test" }));
    assert(adapter.hasAnyModel());
    assert(adapter.hasModelForChannel(0));
    assert(adapter.hasRealDspForChannel(0));

    std::vector<float> signal(64, 0.0f);
    signal[0] = 0.25f;
    signal[1] = -0.20f;
    signal[2] = 0.15f;
    signal[3] = -0.10f;
    const auto before = signal;

    adapter.processChannel(0, signal.data(), signal.size());

    bool changed = false;
    for (std::size_t i = 0; i < signal.size(); ++i)
    {
        assert(std::isfinite(signal[i]));
        if (std::abs(signal[i] - before[i]) > 1.0e-7f)
            changed = true;
    }
    assert(changed);

    HiddenNamAdapter secondAdapter;
    secondAdapter.prepare(44100.0, 64);
    assert(secondAdapter.bindFixedInternalModelForAllChannels({ tapeModel.string(), "tape NAM adapter smoke test" }));
    assert(secondAdapter.hasRealDspForChannel(23));

    return 0;
#endif
}
