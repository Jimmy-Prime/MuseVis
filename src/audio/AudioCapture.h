#pragma once

#include <atomic>
#include <string>
#include <thread>
#include "musevis/SharedState.h"

namespace musevis {

class AudioCapture {
public:
    AudioCapture(SharedState& state, const std::string& sourceName);
    ~AudioCapture();

    void start();
    void stop();

private:
    void captureLoop();

    SharedState& state_;
    std::string  sourceName_;
    std::atomic<bool> running_{false};
    std::thread  thread_;
};

} // namespace musevis
