#pragma once

#include <array>
#include <filesystem>
#include <string>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace geilalizer::dsp
{

inline std::filesystem::path executableDirectory()
{
#if defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size + 1, '\0');
    if (_NSGetExecutablePath(buffer.data(), &size) == 0)
        return std::filesystem::weakly_canonical(std::filesystem::path(buffer.c_str())).parent_path();
#elif defined(__linux__)
    std::array<char, 4096> buffer {};
    const auto length = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0)
    {
        buffer[static_cast<std::size_t>(length)] = '\0';
        return std::filesystem::weakly_canonical(std::filesystem::path(buffer.data())).parent_path();
    }
#endif
    return {};
}

inline std::filesystem::path resolveProductAssetPath(const std::filesystem::path& relative)
{
    if (std::filesystem::exists(relative))
        return relative;

    const auto cwdCandidate = std::filesystem::current_path() / relative;
    if (std::filesystem::exists(cwdCandidate))
        return cwdCandidate;

    const auto exeDir = executableDirectory();
    if (!exeDir.empty())
    {
        const auto besideExecutable = exeDir / relative;
        if (std::filesystem::exists(besideExecutable))
            return besideExecutable;

        const auto macResources = exeDir.parent_path() / "Resources" / relative;
        if (std::filesystem::exists(macResources))
            return macResources;
    }

    return relative;
}

} // namespace geilalizer::dsp
