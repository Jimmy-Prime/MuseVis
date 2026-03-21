#pragma once

#include "musevis/SharedState.h"

namespace musevis {

class BandFrameObserver {
public:
    virtual ~BandFrameObserver() = default;
    virtual void onFrame(const char* analyzerName,
                         uint64_t frameNumber,
                         const BandData& data) = 0;
};

class BandAnalyzer {
public:
    explicit BandAnalyzer(SharedState& state, BandFrameObserver* observer = nullptr)
        : state_(state), observer_(observer) {}
    virtual ~BandAnalyzer() = default;

    virtual const char* name() const = 0;
    virtual void process(const float* stereoFrames, int numFrames) = 0;

protected:
    void publish(const BandData& data) {
        state_.backBuffer() = data;
        state_.swapBuffers();
        if (observer_)
            observer_->onFrame(name(), state_.frame(), data);
    }

    SharedState& state_;
    BandFrameObserver* observer_;
};

} // namespace musevis
