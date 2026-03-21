#pragma once

#include <cmath>

namespace musevis {

class BiquadFilter {
public:
    static BiquadFilter bandPass(float centerHz, float q, float sampleRate) {
        const float omega = 2.0f * 3.14159265358979323846f * centerHz / sampleRate;
        const float sinOmega = std::sin(omega);
        const float cosOmega = std::cos(omega);
        const float alpha = sinOmega / (2.0f * q);
        const float a0 = 1.0f + alpha;

        return BiquadFilter(alpha / a0,
                            0.0f,
                            -alpha / a0,
                            (-2.0f * cosOmega) / a0,
                            (1.0f - alpha) / a0);
    }

    float process(float input) {
        const float output = b0_ * input + z1_;
        z1_ = b1_ * input - a1_ * output + z2_;
        z2_ = b2_ * input - a2_ * output;
        return output;
    }

    void reset() {
        z1_ = 0.0f;
        z2_ = 0.0f;
    }

private:
    BiquadFilter(float b0, float b1, float b2, float a1, float a2)
        : b0_(b0), b1_(b1), b2_(b2), a1_(a1), a2_(a2) {}

    float b0_;
    float b1_;
    float b2_;
    float a1_;
    float a2_;
    float z1_{0.0f};
    float z2_{0.0f};
};

} // namespace musevis
