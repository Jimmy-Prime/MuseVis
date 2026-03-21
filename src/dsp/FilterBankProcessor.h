#pragma once

#include <array>

#include "dsp/BandAnalyzer.h"
#include "dsp/TeensyBandLayout.h"

namespace musevis {

class FilterBankProcessor final : public BandAnalyzer {
public:
    explicit FilterBankProcessor(SharedState& state, BandFrameObserver* observer = nullptr);

    const char* name() const override {
        return "filterbank";
    }

    void process(const float* stereoFrames, int numFrames) override;

private:
    struct Biquad {
        float b0{0.0f};
        float b1{0.0f};
        float b2{0.0f};
        float a1{0.0f};
        float a2{0.0f};
        float x1{0.0f};
        float x2{0.0f};
        float y1{0.0f};
        float y2{0.0f};

        float process(float sample);
    };

    static Biquad makeBandPass(float centerHz, float q);
    float updateEnvelope(int bandIndex, float sample);
    void updateNoiseFloorAndPublish();

    std::array<Biquad, ANALYSIS_BANDS> filters_{};
    std::array<float, ANALYSIS_BANDS> envelopes_{};
    std::array<float, ANALYSIS_BANDS> noiseFloors_{};
    float attackCoeff_{0.0f};
    float releaseCoeff_{0.0f};
    float agcReference_{1e-4f};
};

} // namespace musevis
