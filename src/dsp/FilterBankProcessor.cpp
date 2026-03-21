#include "dsp/FilterBankProcessor.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace musevis {

namespace {

constexpr float kPi = 3.14159265358979323846f;

float timeConstantToCoeff(float milliseconds) {
    const float seconds = milliseconds / 1000.0f;
    return std::exp(-1.0f / (seconds * static_cast<float>(SAMPLE_RATE)));
}

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

FilterBankProcessor::FilterBankProcessor(SharedState& state, BandFrameObserver* observer)
    : BandAnalyzer(state, observer)
    , attackCoeff_(timeConstantToCoeff(8.0f))
    , releaseCoeff_(timeConstantToCoeff(140.0f)) {
    for (int i = 0; i < ANALYSIS_BANDS; ++i) {
        const auto& band = kTeensyBandLayout[i];
        const float bandwidth = std::max(1.0f, (band.highHz - band.lowHz) * 0.65f);
        const float q = band.centerHz / bandwidth;
        filters_[i] = makeBandPass(band.centerHz, q);
        noiseFloors_[i] = 1e-4f;
    }
}

float FilterBankProcessor::Biquad::process(float sample) {
    const float output = b0 * sample + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1;
    x1 = sample;
    y2 = y1;
    y1 = output;
    return output;
}

FilterBankProcessor::Biquad FilterBankProcessor::makeBandPass(float centerHz, float q) {
    const float omega = 2.0f * kPi * centerHz / static_cast<float>(SAMPLE_RATE);
    const float sinOmega = std::sin(omega);
    const float cosOmega = std::cos(omega);
    const float alpha = sinOmega / (2.0f * std::max(q, 0.1f));
    const float a0 = 1.0f + alpha;

    Biquad filter;
    filter.b0 = alpha / a0;
    filter.b1 = 0.0f;
    filter.b2 = -alpha / a0;
    filter.a1 = (-2.0f * cosOmega) / a0;
    filter.a2 = (1.0f - alpha) / a0;
    return filter;
}

float FilterBankProcessor::updateEnvelope(int bandIndex, float sample) {
    const float filtered = filters_[bandIndex].process(sample);
    const float magnitude = filtered * filtered;
    const float coeff = magnitude > envelopes_[bandIndex] ? attackCoeff_ : releaseCoeff_;
    envelopes_[bandIndex] =
        coeff * envelopes_[bandIndex] + (1.0f - coeff) * magnitude;
    return envelopes_[bandIndex];
}

void FilterBankProcessor::process(const float* stereoFrames, int numFrames) {
    for (int i = 0; i < numFrames; ++i) {
        const float mono = 0.5f * (stereoFrames[i * 2] + stereoFrames[i * 2 + 1]);
        for (int b = 0; b < ANALYSIS_BANDS; ++b)
            updateEnvelope(b, mono);
    }

    updateNoiseFloorAndPublish();
}

void FilterBankProcessor::updateNoiseFloorAndPublish() {
    std::array<float, ANALYSIS_BANDS> conditioned{};

    for (int b = 0; b < ANALYSIS_BANDS; ++b) {
        const float envelope = envelopes_[b] * kTeensyBandLayout[b].gain;
        if (envelope < noiseFloors_[b])
            noiseFloors_[b] = 0.98f * noiseFloors_[b] + 0.02f * envelope;
        else
            noiseFloors_[b] = 0.9995f * noiseFloors_[b] + 0.0005f * envelope;

        conditioned[b] = std::max(0.0f, envelope - noiseFloors_[b] * 1.2f);
    }

    auto sorted = conditioned;
    std::sort(sorted.begin(), sorted.end());
    const float percentile = sorted[(sorted.size() * 3) / 4];
    const float average = std::accumulate(conditioned.begin(), conditioned.end(), 0.0f) /
                          static_cast<float>(conditioned.size());
    const float target = std::max(percentile, average);
    const float attack = 0.35f;
    const float release = 0.995f;
    if (target > agcReference_)
        agcReference_ = attack * agcReference_ + (1.0f - attack) * target;
    else
        agcReference_ = std::max(1e-4f, release * agcReference_ + (1.0f - release) * target);

    BandData output;
    output.bandCount = ANALYSIS_BANDS;
    for (int b = 0; b < ANALYSIS_BANDS; ++b) {
        const float normalized = conditioned[b] / std::max(agcReference_, 1e-4f);
        output.magnitudes[b] = clamp01(normalized);
    }

    publish(output);
}

} // namespace musevis
