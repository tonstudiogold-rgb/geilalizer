#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

namespace
{
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::ostringstream out;
    out << in.rdbuf();
    return out.str();
}

bool containsUnpinnedNamCoreMain(const std::string& cmake)
{
    const auto dep = cmake.find("NeuralAmpModelerCore");
    if (dep == std::string::npos)
        return true;
    const auto tag = cmake.find("GIT_TAG", dep);
    if (tag == std::string::npos)
        return true;
    const auto nextDeclare = cmake.find("FetchContent_Declare", dep + 1);
    const auto end = nextDeclare == std::string::npos ? cmake.size() : nextDeclare;
    const auto stanza = cmake.substr(dep, end - dep);
    return stanza.find("GIT_TAG main") != std::string::npos
        || stanza.find("GIT_TAG master") != std::string::npos;
}
} // namespace

int main()
{
    const auto root = std::filesystem::current_path();
    const auto cmake = readFile(root / "CMakeLists.txt");
    if (containsUnpinnedNamCoreMain(cmake))
    {
        std::cerr << "NeuralAmpModelerCore must be pinned to a concrete commit, not main/master.\n";
        return 1;
    }

    const std::regex pinnedNamCore(
        R"(NeuralAmpModelerCore[\s\S]*?GIT_TAG\s+[0-9a-fA-F]{40})");
    if (! std::regex_search(cmake, pinnedNamCore))
    {
        std::cerr << "NeuralAmpModelerCore GIT_TAG must be a 40-character commit hash.\n";
        return 1;
    }

    const auto gitignore = readFile(root / ".gitignore");
    if (gitignore.find("private_nam_models/") == std::string::npos)
    {
        std::cerr << ".gitignore must explicitly ignore private_nam_models/.\n";
        return 1;
    }
    if (gitignore.find("*.nam") == std::string::npos)
    {
        std::cerr << ".gitignore must explicitly ignore *.nam.\n";
        return 1;
    }

    if (cmake.find("option(GEILALIZER_BUNDLE_PRIVATE_MODELS") == std::string::npos)
    {
        std::cerr << "CMake must gate private model bundling behind GEILALIZER_BUNDLE_PRIVATE_MODELS.\n";
        return 1;
    }
    const std::regex bundlingOptionOffByDefault(
        R"(option\s*\(\s*GEILALIZER_BUNDLE_PRIVATE_MODELS[\s\S]*?OFF\s*\))");
    if (! std::regex_search(cmake, bundlingOptionOffByDefault))
    {
        std::cerr << "GEILALIZER_BUNDLE_PRIVATE_MODELS must default to OFF.\n";
        return 1;
    }

    const auto optionGuard = cmake.find("if(GEILALIZER_BUNDLE_PRIVATE_MODELS AND EXISTS");
    const auto copyDirectory = cmake.find("copy_directory", optionGuard == std::string::npos ? 0 : optionGuard);
    const auto guardedPrivatePath = cmake.find("private_nam_models", optionGuard == std::string::npos ? 0 : optionGuard);
    if (optionGuard == std::string::npos || copyDirectory == std::string::npos || guardedPrivatePath == std::string::npos || copyDirectory < optionGuard || guardedPrivatePath < optionGuard)
    {
        std::cerr << "private_nam_models copy must be guarded by GEILALIZER_BUNDLE_PRIVATE_MODELS.\n";
        return 1;
    }

    return 0;
}
