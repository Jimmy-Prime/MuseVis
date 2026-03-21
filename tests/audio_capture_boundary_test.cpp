#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <type_traits>

#include "audio/AudioCapture.h"
#include "dsp/BandAnalyzer.h"

namespace {

class SpyAnalyzer : public musevis::BandAnalyzer {
public:
    void process(const float* stereoFrames, int numFrames) override {
        ++calls;
        lastFrameCount = numFrames;
        lastFirstSample = stereoFrames[0];
    }

    int calls{0};
    int lastFrameCount{0};
    float lastFirstSample{0.0f};
};

class TestableAudioCapture : public musevis::AudioCapture {
public:
    using musevis::AudioCapture::AudioCapture;
    using musevis::AudioCapture::processCapturedFrames;
};

} // namespace

int main() {
    constexpr bool isInjectable = std::is_constructible_v<
        musevis::AudioCapture,
        const std::string&,
        std::unique_ptr<musevis::BandAnalyzer>>;
    if (!isInjectable) {
        return EXIT_FAILURE;
    }

    auto analyzer = std::make_unique<SpyAnalyzer>();
    auto* spy = analyzer.get();
    TestableAudioCapture capture("test-source", std::move(analyzer));

    float frames[musevis::CHUNK_SIZE * musevis::NUM_CHANNELS];
    std::memset(frames, 0, sizeof(frames));
    frames[0] = 0.25f;
    capture.processCapturedFrames(frames, musevis::CHUNK_SIZE);

    return (spy->calls == 1
            && spy->lastFrameCount == musevis::CHUNK_SIZE
            && spy->lastFirstSample == 0.25f) ? EXIT_SUCCESS : EXIT_FAILURE;
}
