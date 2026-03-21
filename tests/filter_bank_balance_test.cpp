#include <cmath>
#include <cstdlib>
#include <vector>

#include "dsp/FilterBankProcessor.h"

namespace {

constexpr float kPi = 3.14159265358979323846f;

std::vector<float> makeStereoMix(float phase, int frames) {
    std::vector<float> buffer(static_cast<std::size_t>(frames) * musevis::NUM_CHANNELS);
    for (int i = 0; i < frames; ++i) {
        const float index = phase + static_cast<float>(i);
        const float bass = 0.85f * std::sin(2.0f * kPi * 63.0f * index
                                            / static_cast<float>(musevis::SAMPLE_RATE));
        const float mid = 0.55f * std::sin(2.0f * kPi * 1000.0f * index
                                           / static_cast<float>(musevis::SAMPLE_RATE));
        const float sample = bass + mid;
        buffer[static_cast<std::size_t>(i) * musevis::NUM_CHANNELS] = sample;
        buffer[static_cast<std::size_t>(i) * musevis::NUM_CHANNELS + 1] = sample;
    }
    return buffer;
}

} // namespace

int main() {
    musevis::SharedState state;
    musevis::FilterBankProcessor processor(state);

    int phase = 0;
    for (int i = 0; i < 24; ++i) {
        auto mix = makeStereoMix(static_cast<float>(phase), musevis::CHUNK_SIZE);
        phase += musevis::CHUNK_SIZE;
        processor.process(mix.data(), musevis::CHUNK_SIZE);
    }

    const auto frame = state.frontBuffer();
    const float average = [] (const musevis::BandData& data) {
        float sum = 0.0f;
        for (float value : data.magnitudes) {
            sum += value;
        }
        return sum / static_cast<float>(data.magnitudes.size());
    }(frame);

    if (frame.magnitudes[1] < average * 1.75f) {
        return EXIT_FAILURE;
    }

    if (frame.magnitudes[7] < average * 1.15f) {
        return EXIT_FAILURE;
    }

    if (frame.magnitudes[7] <= frame.magnitudes[6] || frame.magnitudes[7] <= frame.magnitudes[8]) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
