#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

#include "dsp/FilterBankProcessor.h"
#include "dsp/TeensyBandLayout.h"

namespace {

constexpr float kPi = 3.14159265358979323846f;

std::vector<float> makeStereoSine(float frequencyHz, float amplitude, float phase, int frames) {
    std::vector<float> buffer(static_cast<std::size_t>(frames) * musevis::NUM_CHANNELS);
    for (int i = 0; i < frames; ++i) {
        const float sample = amplitude
            * std::sin(2.0f * kPi * frequencyHz * (phase + static_cast<float>(i))
                       / static_cast<float>(musevis::SAMPLE_RATE));
        buffer[static_cast<std::size_t>(i) * musevis::NUM_CHANNELS] = sample;
        buffer[static_cast<std::size_t>(i) * musevis::NUM_CHANNELS + 1] = sample;
    }
    return buffer;
}

int findDominantBand(const musevis::BandData& data) {
    return static_cast<int>(std::distance(
        data.magnitudes.begin(),
        std::max_element(data.magnitudes.begin(), data.magnitudes.end())));
}

bool allNearZero(const musevis::BandData& data, float epsilon) {
    return std::all_of(data.magnitudes.begin(),
                       data.magnitudes.end(),
                       [epsilon](float value) { return std::fabs(value) <= epsilon; });
}

void printFailure(const char* label, const musevis::BandData& data) {
    std::cerr << label << " failed:";
    for (float value : data.magnitudes) {
        std::cerr << ' ' << value;
    }
    std::cerr << '\n';
}

} // namespace

int main() {
    musevis::SharedState state;
    musevis::FilterBankProcessor processor(state);

    for (int i = 0; i < 12; ++i) {
        std::vector<float> silence(static_cast<std::size_t>(musevis::CHUNK_SIZE)
                                   * musevis::NUM_CHANNELS, 0.0f);
        processor.process(silence.data(), musevis::CHUNK_SIZE);
    }

    if (!allNearZero(state.frontBuffer(), 0.001f)) {
        printFailure("silence should stay dark", state.frontBuffer());
        return EXIT_FAILURE;
    }

    int phase = 0;
    for (int i = 0; i < 18; ++i) {
        auto tone = makeStereoSine(1000.0f, 0.8f, static_cast<float>(phase), musevis::CHUNK_SIZE);
        phase += musevis::CHUNK_SIZE;
        processor.process(tone.data(), musevis::CHUNK_SIZE);
    }

    const auto toneFrame = state.frontBuffer();
    const int dominantBand = findDominantBand(toneFrame);
    if (dominantBand != 7) {
        std::cerr << "expected 1.0k band (index 7) to dominate but got index "
                  << dominantBand << '\n';
        return EXIT_FAILURE;
    }

    if (toneFrame.magnitudes[7] <= toneFrame.magnitudes[6]
        || toneFrame.magnitudes[7] <= toneFrame.magnitudes[8]) {
        printFailure("1.0k band should beat its neighbors", toneFrame);
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 36; ++i) {
        std::vector<float> silence(static_cast<std::size_t>(musevis::CHUNK_SIZE)
                                   * musevis::NUM_CHANNELS, 0.0f);
        processor.process(silence.data(), musevis::CHUNK_SIZE);
    }

    if (!allNearZero(state.frontBuffer(), 0.02f)) {
        printFailure("silence should decay back to dark", state.frontBuffer());
        return EXIT_FAILURE;
    }

    for (int i = 0; i < 12; ++i) {
        auto tone = makeStereoSine(1000.0f, 0.8f, static_cast<float>(phase), musevis::CHUNK_SIZE);
        phase += musevis::CHUNK_SIZE;
        processor.process(tone.data(), musevis::CHUNK_SIZE);
    }

    if (findDominantBand(state.frontBuffer()) != 7) {
        printFailure("processor should recover cleanly after quiet reset", state.frontBuffer());
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
