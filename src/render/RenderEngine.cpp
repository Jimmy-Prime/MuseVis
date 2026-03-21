#include "render/RenderEngine.h"
#include "led/LEDController.h"
#include "led/ColorMapper.h"
#include "render/PresentationSmoothing.h"

#include <ws2811.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace musevis {

namespace {
    constexpr int   TARGET_FPS   = 60;
}

RenderEngine::RenderEngine(SharedState& state, LEDController& leds)
    : state_(state), leds_(leds) {}

RenderEngine::~RenderEngine() {
    stop();
}

void RenderEngine::start() {
    running_ = true;
    thread_  = std::thread(&RenderEngine::renderLoop, this);
}

void RenderEngine::stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
}

void RenderEngine::renderLoop() {
    using clock = std::chrono::steady_clock;
    const auto frameDuration = std::chrono::microseconds(1'000'000 / TARGET_FPS);
    auto nextFrame = clock::now();

    while (running_) {
        uint64_t currentFrame = state_.frame();
        BandData data = state_.frontBuffer();

        if (currentFrame == lastFrame_) {
            staleCount_++;
            if (staleCount_ > 12) {  // ~200ms at 60fps
                for (auto& m : data.magnitudes)
                    m = 0.0f;
            }
        } else {
            lastFrame_ = currentFrame;
            staleCount_ = 0;
        }

        buildFrame(data);
        nextFrame += frameDuration;
        std::this_thread::sleep_until(nextFrame);
    }
}

void RenderEngine::buildFrame(const BandData& data) {
    // Keep only light presentation smoothing here; the analyzer owns the envelope.
    for (int b = 0; b < NUM_BANDS; ++b) {
        const float raw = data.magnitudes[b];
        smoothed_[b] = clampQuietTail(smoothPresentationLevel(smoothed_[b], raw), raw);
    }

    // Build GRB pixel array
    ws2811_led_t pixels[LED_COUNT]{};

    for (int b = 0; b < NUM_BANDS; ++b) {
        const int litCount = std::clamp(
            static_cast<int>(std::round(smoothed_[b] * LEDS_PER_BAND)), 0, LEDS_PER_BAND);

        // Hue sweeps red (0°) → violet (270°) across the 14 bands
        const float hue = (static_cast<float>(b) / (NUM_BANDS - 1)) * 270.0f;

        for (int i = 0; i < LEDS_PER_BAND; ++i) {
            if (i < litCount) {
                const RGB c = hsvToRgb(hue, 1.0f, 1.0f);
                // WS2811 GRB: 0x00GGRRBB
                pixels[b * LEDS_PER_BAND + i] =
                    (static_cast<uint32_t>(c.g) << 16) |
                    (static_cast<uint32_t>(c.r) <<  8) |
                     static_cast<uint32_t>(c.b);
            }
            // else pixel stays 0 (off)
        }
    }

    leds_.render(pixels);
}

} // namespace musevis
