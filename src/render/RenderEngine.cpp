#include "render/RenderEngine.h"
#include "led/LEDController.h"
#include "led/ColorMapper.h"

#include <ws2811.h>
#include <algorithm>
#include <chrono>
#include <cmath>

namespace musevis {

namespace {
    constexpr float ATTACK_COEFF = 0.8f;   // fraction of old value kept when rising
    constexpr float DECAY_COEFF  = 0.15f;  // fraction of old value kept when falling
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
        buildFrame(state_.frontBuffer());
        nextFrame += frameDuration;
        std::this_thread::sleep_until(nextFrame);
    }
}

void RenderEngine::buildFrame(const BandData& data) {
    // Attack / decay smoothing per band
    for (int b = 0; b < NUM_BANDS; ++b) {
        const float raw = data.magnitudes[b];
        const float coeff = (raw > smoothed_[b]) ? ATTACK_COEFF : DECAY_COEFF;
        smoothed_[b] = coeff * smoothed_[b] + (1.0f - coeff) * raw;
    }

    // Build GRB pixel array
    ws2811_led_t pixels[NUM_BANDS * LEDS_PER_BAND]{};

    for (int b = 0; b < NUM_BANDS; ++b) {
        const int litCount = std::clamp(
            static_cast<int>(std::round(smoothed_[b] * LEDS_PER_BAND)), 0, LEDS_PER_BAND);

        // Hue sweeps red (0°) → violet (270°) across the 16 bands
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
