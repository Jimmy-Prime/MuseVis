#pragma once

namespace musevis {

class BandAnalyzer {
public:
    virtual ~BandAnalyzer() = default;

    virtual void process(const float* stereoFrames, int numFrames) = 0;
};

} // namespace musevis
