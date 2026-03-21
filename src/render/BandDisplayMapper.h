#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "musevis/SharedState.h"

namespace musevis {

inline std::array<float, DISPLAY_BANDS> projectToDisplayBands(const BandData& data) {
    std::array<float, DISPLAY_BANDS> display{};
    const int sourceBands = std::clamp(data.bandCount, 1, MAX_ANALYSIS_BANDS);

    if constexpr (DISPLAY_BANDS == 1) {
        display[0] = data.magnitudes[0];
        return display;
    }

    for (int i = 0; i < DISPLAY_BANDS; ++i) {
        const float position = static_cast<float>(i) * (sourceBands - 1) /
                               static_cast<float>(DISPLAY_BANDS - 1);
        const int left = static_cast<int>(std::floor(position));
        const int right = std::min(left + 1, sourceBands - 1);
        const float mix = position - static_cast<float>(left);
        display[i] = data.magnitudes[left] * (1.0f - mix) +
                     data.magnitudes[right] * mix;
    }

    return display;
}

} // namespace musevis
