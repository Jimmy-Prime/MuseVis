#pragma once

#include <array>
#include <vector>
#include <fftw3.h>
#include "dsp/BandAnalyzer.h"
#include "dsp/BandMapper.h"

namespace musevis {

class FFTProcessor final : public BandAnalyzer {
public:
    explicit FFTProcessor(SharedState& state, BandFrameObserver* observer = nullptr);
    ~FFTProcessor();

    const char* name() const override {
        return "fft";
    }

    // Process one chunk of interleaved stereo float samples (CHUNK_SIZE frames).
    void process(const float* stereoFrames, int numFrames);

private:
    void computeBands();

    std::vector<BandBoundary> bands_;

    // Rolling overlap buffer: always holds the last FFT_SIZE mono samples.
    std::vector<double> overlapBuf_;

    double*       fftIn_;
    fftw_complex* fftOut_;
    fftw_plan     plan_;

    std::vector<double> hannWindow_;
    double runningMax_{1e-9};
};

} // namespace musevis
