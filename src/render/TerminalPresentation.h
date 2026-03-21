#pragma once

#include <algorithm>
#include <array>

#include "musevis/SharedState.h"
#include "render/PresentationSmoothing.h"

namespace musevis {

inline void applyTerminalPresentationFrame(std::array<float, NUM_BANDS>& smoothed,
                                           std::array<float, NUM_BANDS>& peaks,
                                           const BandData& data) {
    constexpr float kPeakDecay = 0.02f;

    bool allRawSilent = true;
    bool allSmoothedSilent = true;

    for (int b = 0; b < NUM_BANDS; ++b) {
        const float raw = data.magnitudes[b];
        smoothed[b] = clampQuietTail(smoothPresentationLevel(smoothed[b], raw), raw);

        if (smoothed[b] > peaks[b]) {
            peaks[b] = smoothed[b];
        } else {
            peaks[b] = std::max(0.0f, peaks[b] - kPeakDecay);
        }

        allRawSilent = allRawSilent && raw <= 0.0f;
        allSmoothedSilent = allSmoothedSilent && smoothed[b] <= 0.0f;
    }

    if (allRawSilent && allSmoothedSilent) {
        smoothed.fill(0.0f);
        peaks.fill(0.0f);
    }
}

} // namespace musevis
