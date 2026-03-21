#pragma once

#include <array>
#include <atomic>
#include <thread>
#include "musevis/SharedState.h"

namespace musevis {

class TerminalRenderer {
public:
    explicit TerminalRenderer(SharedState& state);
    ~TerminalRenderer();

    void start();
    void stop();

private:
    void renderLoop();
    void drawFrame(const BandData& data);
    void initScreen();
    void restoreScreen();

    SharedState& state_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::array<float, NUM_BANDS> smoothed_{};
    std::array<float, NUM_BANDS> peaks_{};     // peak-hold indicators
    uint64_t lastFrame_{0};                    // detect stale audio data
    int staleCount_{0};                        // frames since last audio update
};

} // namespace musevis
