#include "dsp/FFTProcessor.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace musevis {

FFTProcessor::FFTProcessor(SharedState& state, BandFrameObserver* observer)
    : BandAnalyzer(state, observer)
    , bands_(computeBandBoundaries(FFT_BANDS))
    , overlapBuf_(FFT_SIZE, 0.0)
    , hannWindow_(FFT_SIZE)
{
    // Precompute Hann window
    for (int i = 0; i < FFT_SIZE; ++i)
        hannWindow_[i] = 0.5 * (1.0 - std::cos(2.0 * M_PI * i / (FFT_SIZE - 1)));

    fftIn_  = fftw_alloc_real(FFT_SIZE);
    fftOut_ = fftw_alloc_complex(FFT_SIZE / 2 + 1);
    // FFTW_ESTIMATE avoids long plan-creation time at startup
    plan_   = fftw_plan_dft_r2c_1d(FFT_SIZE, fftIn_, fftOut_, FFTW_ESTIMATE);
}

FFTProcessor::~FFTProcessor() {
    fftw_destroy_plan(plan_);
    fftw_free(fftIn_);
    fftw_free(fftOut_);
}

void FFTProcessor::process(const float* stereoFrames, int numFrames) {
    // Shift overlap buffer left by numFrames, append new mono samples at the end
    std::memmove(overlapBuf_.data(),
                 overlapBuf_.data() + numFrames,
                 (FFT_SIZE - numFrames) * sizeof(double));

    for (int i = 0; i < numFrames; ++i) {
        // Mix stereo → mono
        overlapBuf_[FFT_SIZE - numFrames + i] =
            0.5 * (static_cast<double>(stereoFrames[i * 2])
                 + static_cast<double>(stereoFrames[i * 2 + 1]));
    }

    // Apply Hann window and feed into FFTW input
    for (int i = 0; i < FFT_SIZE; ++i)
        fftIn_[i] = overlapBuf_[i] * hannWindow_[i];

    fftw_execute(plan_);
    computeBands();
}

void FFTProcessor::computeBands() {
    // dB-scale mapping: show DYNAMIC_RANGE_DB of range below the running peak.
    // This lets quiet high-frequency content remain visible even when bass dominates.
    constexpr float DYNAMIC_RANGE_DB = 48.0f;

    float maxMag = 0.0f;
    std::array<float, FFT_BANDS> rawMags{};
    BandData output;
    output.bandCount = FFT_BANDS;

    for (int b = 0; b < FFT_BANDS; ++b) {
        double sumSq = 0.0;
        int    count = 0;

        for (int k = bands_[b].binLow; k < bands_[b].binHigh; ++k) {
            double re = fftOut_[k][0];
            double im = fftOut_[k][1];
            sumSq += re * re + im * im;
            ++count;
        }

        rawMags[b] = count > 0
            ? static_cast<float>(std::sqrt(sumSq / count))
            : 0.0f;

        if (rawMags[b] > maxMag)
            maxMag = rawMags[b];
    }

    // Slowly-decaying running maximum (global reference for 0 dB)
    runningMax_ = std::max(runningMax_ * 0.9999, static_cast<double>(maxMag));
    runningMax_ = std::max(runningMax_, 1e-9);

    const float refMax = static_cast<float>(runningMax_);
    for (int b = 0; b < FFT_BANDS; ++b) {
        if (rawMags[b] < 1e-10f) {
            output.magnitudes[b] = 0.0f;
        } else {
            // dB relative to running max: 0 dB = loudest, negative = quieter
            float db = 20.0f * std::log10(rawMags[b] / refMax);
            // Map [-DYNAMIC_RANGE_DB, 0] → [0, 1]
            output.magnitudes[b] = std::clamp(1.0f + db / DYNAMIC_RANGE_DB, 0.0f, 1.0f);
        }
    }

    publish(output);
}

} // namespace musevis
