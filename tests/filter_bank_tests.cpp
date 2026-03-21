#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

#include "dsp/FilterBankProcessor.h"
#include "dsp/TeensyBandLayout.h"
#include "musevis/SharedState.h"
#include "render/BandDisplayMapper.h"

namespace {

using musevis::BandData;
using musevis::FilterBankProcessor;
using musevis::SharedState;
using musevis::kTeensyBandLayout;

constexpr float kPi = 3.14159265358979323846f;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

std::vector<float> makeStereoSine(float frequencyHz, float amplitude, int frames) {
    std::vector<float> samples(frames * musevis::NUM_CHANNELS);
    for (int i = 0; i < frames; ++i) {
        const float phase = 2.0f * kPi * frequencyHz * static_cast<float>(i) /
                            static_cast<float>(musevis::SAMPLE_RATE);
        const float sample = amplitude * std::sin(phase);
        samples[i * 2] = sample;
        samples[i * 2 + 1] = sample;
    }
    return samples;
}

float averageMagnitude(const BandData& data) {
    return std::accumulate(data.magnitudes.begin(),
                           data.magnitudes.begin() + data.bandCount,
                           0.0f) /
           static_cast<float>(data.bandCount);
}

int nearestBandIndex(float frequencyHz) {
    int nearest = 0;
    float bestDistance = std::fabs(kTeensyBandLayout[0].centerHz - frequencyHz);
    for (int i = 1; i < static_cast<int>(kTeensyBandLayout.size()); ++i) {
        const float distance = std::fabs(kTeensyBandLayout[i].centerHz - frequencyHz);
        if (distance < bestDistance) {
            bestDistance = distance;
            nearest = i;
        }
    }
    return nearest;
}

void feedChunks(FilterBankProcessor& processor, float frequencyHz, float amplitude, int chunks) {
    const auto block = makeStereoSine(frequencyHz, amplitude, musevis::CHUNK_SIZE);
    for (int i = 0; i < chunks; ++i)
        processor.process(block.data(), musevis::CHUNK_SIZE);
}

void test_band_layout_metadata() {
    require(kTeensyBandLayout.size() == musevis::ANALYSIS_BANDS,
            "layout count should match analysis band count");
    require(kTeensyBandLayout.front().lowHz <= 40.5f,
            "first Teensy-style band should start near 40 Hz");
    require(kTeensyBandLayout.back().highHz >= 15900.0f,
            "last Teensy-style band should extend near 16 kHz");
}

void test_low_tone_hits_nearest_band() {
    SharedState state;
    FilterBankProcessor processor(state);

    feedChunks(processor, 100.0f, 0.8f, 24);

    const auto data = state.frontBuffer();
    const int expected = nearestBandIndex(100.0f);
    const auto strongest = std::distance(
        data.magnitudes.begin(),
        std::max_element(data.magnitudes.begin(), data.magnitudes.begin() + data.bandCount));

    require(strongest == expected || std::abs(strongest - expected) <= 1,
            "100 Hz tone should peak in the nearest low-frequency band");
    require(data.magnitudes[expected] > 0.35f,
            "100 Hz tone should produce a visible low-band magnitude");
}

void test_high_tone_hits_nearest_band() {
    SharedState state;
    FilterBankProcessor processor(state);

    feedChunks(processor, 4000.0f, 0.8f, 24);

    const auto data = state.frontBuffer();
    const int expected = nearestBandIndex(4000.0f);
    const auto strongest = std::distance(
        data.magnitudes.begin(),
        std::max_element(data.magnitudes.begin(), data.magnitudes.begin() + data.bandCount));

    require(strongest == expected || std::abs(strongest - expected) <= 1,
            "4 kHz tone should peak in the nearest high-frequency band");
    require(data.magnitudes[expected] > 0.35f,
            "4 kHz tone should produce a visible high-band magnitude");
}

void test_silence_decays_after_signal() {
    SharedState state;
    FilterBankProcessor processor(state);

    feedChunks(processor, 250.0f, 0.8f, 18);

    const std::vector<float> silence(musevis::CHUNK_SIZE * musevis::NUM_CHANNELS, 0.0f);
    for (int i = 0; i < 80; ++i)
        processor.process(silence.data(), musevis::CHUNK_SIZE);

    require(averageMagnitude(state.frontBuffer()) < 0.08f,
            "silence should decay close to zero instead of staying pinned");
}

void test_display_mapping_preserves_edges() {
    BandData data;
    data.bandCount = musevis::ANALYSIS_BANDS;
    for (int i = 0; i < musevis::ANALYSIS_BANDS; ++i)
        data.magnitudes[i] = static_cast<float>(i);

    const auto display = musevis::projectToDisplayBands(data);

    require(display.size() == musevis::DISPLAY_BANDS,
            "display projection should produce one value per display column");
    require(std::fabs(display.front() - 0.0f) < 1e-4f,
            "display projection should preserve the first analysis band");
    require(std::fabs(display.back() -
                      static_cast<float>(musevis::ANALYSIS_BANDS - 1)) < 1e-4f,
            "display projection should preserve the last analysis band");
    require(std::is_sorted(display.begin(), display.end()),
            "display projection should stay monotonic for monotonic input");
}

} // namespace

int main() {
    test_band_layout_metadata();
    test_low_tone_hits_nearest_band();
    test_high_tone_hits_nearest_band();
    test_silence_decays_after_signal();
    test_display_mapping_preserves_edges();
    std::cout << "filter bank tests passed\n";
    return 0;
}
