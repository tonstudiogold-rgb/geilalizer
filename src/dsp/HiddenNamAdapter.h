#pragma once

#include <cstddef>
#include <string>

namespace geilalizer::dsp
{

class HiddenNamAdapter
{
public:
    struct Request
    {
        std::string modelIdentifier;
        std::string reason;
    };

    bool hasModel() const { return hasModel_; }
    const std::string& modelIdentifier() const { return modelIdentifier_; }

    // Hidden integration point only. NAM is not user visible and has no user load/select API.
    // When model integration is ready, request exactly one model at a time and bind it here.
    void bindSingleInternalModelForNextIntegration(const Request& request)
    {
        modelIdentifier_ = request.modelIdentifier;
        hasModel_ = !modelIdentifier_.empty();
    }

    void reset() {}

    void processMono(float* samples, std::size_t numFrames)
    {
        // Placeholder: pass-through until the single internal NAM model is integrated.
        (void) samples;
        (void) numFrames;
    }

private:
    bool hasModel_ = false;
    std::string modelIdentifier_;
};

} // namespace geilalizer::dsp
