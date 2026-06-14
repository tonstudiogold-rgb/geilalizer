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
    assert(in && "MainComponent.cpp must be readable");
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
    const auto source = readFile("src/MainComponent.cpp");
    const auto callback = extractFunctionBody(source, "void MainComponent::getNextAudioBlock");

    const std::vector<std::string> forbidden {
        "std::lock_guard",
        "trackMutex",
        ".resize(",
        ".assign(",
        "std::vector<",
        "setLoadedAudioFile",
        "setStatus(",
        "audioEngine.session()",
        "tapeMachine.",
        "createReaderFor",
        "createOutputStream",
        "juce::File"
    };

    for (const auto& token : forbidden)
    {
        if (contains(callback, token))
        {
            std::cerr << "Realtime callback contains forbidden token: " << token << '\n';
            return 1;
        }
    }

    const auto prepare = extractFunctionBody(source, "void MainComponent::prepareToPlay");
    assert(contains(prepare, "audioBlockInputs.resize"));
    assert(contains(prepare, "audioInputScratch"));
    assert(contains(prepare, "audioBlockInterleaved.resize"));

    std::cout << "RealtimeCallbackSourceAudit: PASS\n";
    return 0;
}
