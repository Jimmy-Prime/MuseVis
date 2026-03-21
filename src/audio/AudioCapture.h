#pragma once

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include "dsp/AnalyzerFactory.h"
#include "dsp/BandAnalyzer.h"
#include "musevis/SharedState.h"

namespace musevis {

class AudioCapture {
public:
    AudioCapture(SharedState& state, const std::string& sourceName)
        : AudioCapture(sourceName, createDefaultAnalyzer(state)) {}
    AudioCapture(const std::string& sourceName,
                 std::unique_ptr<BandAnalyzer> analyzer)
        : sourceName_(sourceName)
        , analyzer_(std::move(analyzer))
    {
        if (!analyzer_) {
            throw std::invalid_argument("AudioCapture requires a non-null analyzer");
        }
    }
    ~AudioCapture() {
        stop();
    }

    void start() {
        running_ = true;
        thread_ = std::thread(&AudioCapture::captureLoop, this);
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

protected:
    void processCapturedFrames(const float* stereoFrames, int numFrames) {
        analyzer_->process(stereoFrames, numFrames);
    }

private:
    void captureLoop();

    std::string  sourceName_;
    std::unique_ptr<BandAnalyzer> analyzer_;
    std::atomic<bool> running_{false};
    std::thread  thread_;
};

} // namespace musevis
