#include "dsp/FilterBankProcessor.h"

#include <algorithm>
#include <cmath>

namespace musevis {

namespace {

float envelopeAlpha(float timeMs) {
    return std::exp(-1.0f / ((timeMs / 1000.0f) * static_cast<float>(SAMPLE_RATE)));
}

const float kAttackAlpha = envelopeAlpha(4.0f);
const float kReleaseAlpha = envelopeAlpha(180.0f);
constexpr float kAgcAttackAlpha = 0.90f;
constexpr float kAgcReleaseAlpha = 0.995f;
constexpr float kNoiseGate = 0.0005f;
constexpr float kQuietThreshold = 0.002f;
constexpr int kQuietHoldFrames = 20;

} // namespace

FilterBankProcessor::FilterBankProcessor(SharedState& state)
    : state_(state)
    , bands_{{
        {BiquadFilter::bandPass(kTeensyBandLayout[0].centerHz, kTeensyBandLayout[0].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[1].centerHz, kTeensyBandLayout[1].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[2].centerHz, kTeensyBandLayout[2].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[3].centerHz, kTeensyBandLayout[3].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[4].centerHz, kTeensyBandLayout[4].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[5].centerHz, kTeensyBandLayout[5].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[6].centerHz, kTeensyBandLayout[6].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[7].centerHz, kTeensyBandLayout[7].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[8].centerHz, kTeensyBandLayout[8].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[9].centerHz, kTeensyBandLayout[9].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[10].centerHz, kTeensyBandLayout[10].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[11].centerHz, kTeensyBandLayout[11].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[12].centerHz, kTeensyBandLayout[12].q, SAMPLE_RATE)},
        {BiquadFilter::bandPass(kTeensyBandLayout[13].centerHz, kTeensyBandLayout[13].q, SAMPLE_RATE)},
    }} {}

void FilterBankProcessor::process(const float* stereoFrames, int numFrames) {
    for (int i = 0; i < numFrames; ++i) {
        const float mono = 0.5f * (stereoFrames[i * 2] + stereoFrames[i * 2 + 1]);

        for (int bandIndex = 0; bandIndex < NUM_BANDS; ++bandIndex) {
            auto& band = bands_[bandIndex];
            const float filtered = band.filter.process(mono);
            const float energy = std::fabs(filtered) * kTeensyBandLayout[bandIndex].gain;
            const float alpha = energy > band.envelope ? kAttackAlpha : kReleaseAlpha;

            band.envelope = alpha * band.envelope + (1.0f - alpha) * energy;
            band.level = band.envelope;
        }
    }

    publishFrame();
}

void FilterBankProcessor::publishFrame() {
    auto& back = state_.backBuffer();

    float aggregateEnergy = 0.0f;
    int activeBands = 0;
    for (int bandIndex = 0; bandIndex < NUM_BANDS; ++bandIndex) {
        const float gated = std::max(0.0f, bands_[bandIndex].level - kNoiseGate);
        if (gated > 0.0f) {
            aggregateEnergy += gated;
            ++activeBands;
        }
    }

    const float meanEnergy = activeBands > 0 ? aggregateEnergy / static_cast<float>(activeBands) : 0.0f;
    const float agcAlpha = meanEnergy > agcReference_ ? kAgcAttackAlpha : kAgcReleaseAlpha;
    agcReference_ = agcAlpha * agcReference_ + (1.0f - agcAlpha) * std::max(meanEnergy, kNoiseGate);
    agcReference_ = std::max(agcReference_, kNoiseGate);

    if (meanEnergy < kQuietThreshold) {
        quietFrames_++;
    } else {
        quietFrames_ = 0;
    }

    if (quietFrames_ >= kQuietHoldFrames) {
        agcReference_ = 0.01f;
        for (auto& band : bands_) {
            band.envelope = 0.0f;
            band.level = 0.0f;
            band.filter.reset();
        }
        back.magnitudes.fill(0.0f);
        state_.swapBuffers();
        return;
    }

    const float scale = std::max(agcReference_ * 3.5f, 0.01f);
    for (int bandIndex = 0; bandIndex < NUM_BANDS; ++bandIndex) {
        const float gated = std::max(0.0f, bands_[bandIndex].level - kNoiseGate);
        back.magnitudes[bandIndex] = std::clamp(gated / scale, 0.0f, 1.0f);
    }

    state_.swapBuffers();
}

} // namespace musevis
