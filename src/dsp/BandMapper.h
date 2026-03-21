#pragma once

#include <algorithm>
#include <cmath>
#include <vector>
#include "musevis/SharedState.h"

namespace musevis {

struct BandBoundary {
    int binLow;
    int binHigh;
};

// Compute log-spaced band boundaries covering freqLow–freqHigh Hz.
inline std::vector<BandBoundary> computeBandBoundaries(
    int   bandCount,
    float freqLow    = 20.0f,
    float freqHigh   = 20000.0f,
    int   fftSize    = FFT_SIZE,
    int   sampleRate = SAMPLE_RATE)
{
    std::vector<BandBoundary> bands(static_cast<size_t>(bandCount));
    const float binWidth = static_cast<float>(sampleRate) / fftSize;
    const int   maxBin   = fftSize / 2;

    for (int b = 0; b < bandCount; ++b) {
        float lo = freqLow * std::pow(freqHigh / freqLow, static_cast<float>(b) / bandCount);
        float hi = freqLow * std::pow(freqHigh / freqLow, static_cast<float>(b + 1) / bandCount);

        bands[b].binLow  = std::max(0,      static_cast<int>(lo / binWidth));
        bands[b].binHigh = std::min(maxBin, static_cast<int>(hi / binWidth));

        // Guarantee at least one bin per band
        if (bands[b].binHigh <= bands[b].binLow)
            bands[b].binHigh = bands[b].binLow + 1;
    }

    return bands;
}

} // namespace musevis
