#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path);
    assert(in && "MachineProcessor files must be readable");
    return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return haystack.find(needle) != std::string::npos;
}
} // namespace

int main()
{
    const auto source = readFile("src/dsp/MachineProcessor.cpp");
    const auto header = readFile("src/dsp/MachineProcessor.h");

    if (contains(source, "if (session.master.limiterEnabled)\n        processStereoNam(emtLimiterNamAdapter_"))
    {
        std::cerr << "EMT limiter bypass is still a hard if-switch; use clickless wet/dry crossfade.\n";
        return 1;
    }

    assert(contains(header, "emtLimiterBypassCrossfadeSamples_"));
    assert(contains(header, "emtLimiterDryScratch_"));
    assert(contains(header, "emtLimiterWetScratch_"));
    assert(contains(source, "processEmtLimiterBypass"));
    assert(contains(source, "emtLimiterBypassCrossfadeSamples_ = std::clamp"));
    assert(contains(source, "64"));
    assert(contains(source, "256"));

    std::cout << "LimiterBypassClickPolicy: PASS\n";
    return 0;
}
