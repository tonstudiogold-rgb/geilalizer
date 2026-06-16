#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
std::string readFile(const std::filesystem::path& path)
{
    std::ifstream in(path);
    assert(in && "MachineProcessor.cpp must be readable");
    return { std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
}

std::string extractFunctionBody(const std::string& text, const std::string& signature)
{
    const auto sig = text.find(signature);
    assert(sig != std::string::npos && "function signature not found");
    const auto open = text.find('{', sig);
    assert(open != std::string::npos && "function body open not found");
    int depth = 0;
    for (std::size_t i = open; i < text.size(); ++i)
    {
        if (text[i] == '{')
            ++depth;
        else if (text[i] == '}')
        {
            --depth;
            if (depth == 0)
                return text.substr(open, i - open + 1);
        }
    }
    assert(false && "function body close not found");
    return {};
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
    const auto audioEngineSource = readFile("src/core/AudioEngine.cpp");
    const auto mainComponentSource = readFile("src/MainComponent.cpp");
    const auto prepare = extractFunctionBody(source, "void MachineProcessor::prepare");
    const auto prepareToPlay = extractFunctionBody(mainComponentSource, "void MainComponent::prepareToPlay");
    const auto audioCallback = extractFunctionBody(mainComponentSource, "void MainComponent::getNextAudioBlock");

    const std::vector<std::string> forbidden {
        "std::filesystem",
        "resolveProductAssetPath",
        "loadNeve1073DirectoryIfPresent",
        "loadSsl4000GDirectoryIfPresent",
        "loadMixbusDirectoryIfPresent",
        "fixedConsoleNamModelPath",
        "fixedTapeNamModelPath",
        "fixedEmtLimiterNamModelPath",
        "fixedFinalHiloNamModelPath",
        "bindFixedInternalModelForAllChannels",
        "bindSingleInternalModelForNextIntegration",
        "exists("
    };

    for (const auto& token : forbidden)
    {
        if (contains(prepare, token))
        {
            std::cerr << "MachineProcessor::prepare contains asset/model loading token: " << token << '\n';
            return 1;
        }
    }

    assert(contains(header, "struct PreparedMachineState"));
    assert(contains(source, "loadDefaultAssetsOffAudioThread"));
    assert(contains(source, "applyPreparedMachineState"));
    assert(contains(audioEngineSource, "loadDefaultAssetsBeforeAudioStart"));
    assert(contains(mainComponentSource, "audioEngine.loadDefaultAssetsBeforeAudioStart();"));

    if (contains(prepareToPlay, "loadDefaultAssets") || contains(prepareToPlay, "applyPreparedMachineState"))
    {
        std::cerr << "prepareToPlay must not load/apply NAM/IR assets; it runs on device changes.\n";
        return 1;
    }
    if (contains(audioCallback, "loadDefaultAssets") || contains(audioCallback, "applyPreparedMachineState"))
    {
        std::cerr << "Audio callback must never load/apply NAM/IR assets.\n";
        return 1;
    }

    const auto loadBeforeAudio = mainComponentSource.find("audioEngine.loadDefaultAssetsBeforeAudioStart();");
    const auto startAudio = mainComponentSource.find("setAudioChannels(channelCount, 2);");
    assert(loadBeforeAudio != std::string::npos);
    assert(startAudio != std::string::npos);
    if (loadBeforeAudio > startAudio)
    {
        std::cerr << "Default asset load/apply must happen before setAudioChannels starts audio.\n";
        return 1;
    }

    std::cout << "MachinePrepareAssetPolicy: PASS\n";
    return 0;
}
