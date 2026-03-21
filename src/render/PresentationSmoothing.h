#pragma once

namespace musevis {

inline float smoothPresentationLevel(float previous, float raw) {
    constexpr float kRiseCoeff = 0.18f;
    constexpr float kFallCoeff = 0.72f;
    const float coeff = raw > previous ? kRiseCoeff : kFallCoeff;
    return coeff * previous + (1.0f - coeff) * raw;
}

inline float clampQuietTail(float smoothed, float raw) {
    if (raw <= 0.0f && smoothed < 0.02f) {
        return 0.0f;
    }

    return smoothed;
}

} // namespace musevis
