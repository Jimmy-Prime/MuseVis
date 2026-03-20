#pragma once

#include <array>
#include <atomic>
#include <thread>
#include "musevis/SharedState.h"

namespace musevis {

class LEDController;

class RenderEngine {
public:
    RenderEngine(SharedState& state, LEDController& leds);
    ~RenderEngine();

    void start();
    void stop();

private:
    void renderLoop();
    void buildFrame(const BandData& data);

    SharedState&   state_;
    LEDController& leds_;
    std::atomic<bool> running_{false};
    std::thread    thread_;

    // Per-band smoothed magnitudes (attack/decay filter state)
    std::array<float, NUM_BANDS> smoothed_{};
};

} // namespace musevis
