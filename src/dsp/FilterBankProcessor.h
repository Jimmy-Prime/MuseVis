#pragma once

#include <array>

#include "dsp/BandAnalyzer.h"
#include "dsp/BiquadFilter.h"
#include "dsp/TeensyBandLayout.h"
#include "musevis/SharedState.h"

namespace musevis {

class FilterBankProcessor : public BandAnalyzer {
public:
    explicit FilterBankProcessor(SharedState& state);

    void process(const float* stereoFrames, int numFrames) override;

private:
    struct BandState {
        BiquadFilter filter;
        float envelope{0.0f};
        float level{0.0f};
    };

    void publishFrame();

    SharedState& state_;
    std::array<BandState, NUM_BANDS> bands_;
    float agcReference_{0.08f};
    int quietFrames_{0};
};

} // namespace musevis
