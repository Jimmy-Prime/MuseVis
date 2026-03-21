#include "render/RenderEngine.h"
#include "render/BandDisplayMapper.h"
#include "led/LEDController.h"
#include "led/ColorMapper.h"

#include <ws2811.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <numeric>

namespace musevis {

namespace {
    constexpr float ATTACK_COEFF = 0.10f;  // light presentation smoothing; analyzer owns the envelope
    constexpr float DECAY_COEFF  = 0.80f;  // keep some visual persistence without duplicating DSP decay
    constexpr int   TARGET_FPS   = 60;
    constexpr float QUIET_THRESHOLD = 0.035f;
    constexpr auto  QUIET_DELAY     = std::chrono::seconds(8);
    constexpr auto  QUIET_FADE      = std::chrono::seconds(2);
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
    quietSince_ = nextFrame;

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
    const auto displayBands = projectToDisplayBands(data);
    const float average = std::accumulate(displayBands.begin(), displayBands.end(), 0.0f) /
                          static_cast<float>(displayBands.size());
    const auto now = std::chrono::steady_clock::now();

    if (average < QUIET_THRESHOLD) {
        if (!quiet_) {
            quiet_ = true;
            quietSince_ = now;
        }
    } else {
        quiet_ = false;
        quietFade_ = 1.0f;
    }

    if (quiet_) {
        const auto quietFor = now - quietSince_;
        if (quietFor > QUIET_DELAY) {
            const float fadeProgress = std::clamp(
                std::chrono::duration<float>(quietFor - QUIET_DELAY).count() /
                    std::chrono::duration<float>(QUIET_FADE).count(),
                0.0f,
                1.0f);
            quietFade_ = 1.0f - fadeProgress;
        } else {
            quietFade_ = 1.0f;
        }
    }

    // Attack / decay smoothing per band
    for (int b = 0; b < DISPLAY_BANDS; ++b) {
        const float raw = displayBands[b] * quietFade_;
        const float coeff = (raw > smoothed_[b]) ? ATTACK_COEFF : DECAY_COEFF;
        smoothed_[b] = coeff * smoothed_[b] + (1.0f - coeff) * raw;
    }

    // Build GRB pixel array
    ws2811_led_t pixels[DISPLAY_BANDS * LEDS_PER_BAND]{};

    for (int b = 0; b < DISPLAY_BANDS; ++b) {
        const int litCount = std::clamp(
            static_cast<int>(std::round(smoothed_[b] * LEDS_PER_BAND)), 0, LEDS_PER_BAND);

        // Hue sweeps red (0°) → violet (270°) across the 16 bands
        const float hue = (static_cast<float>(b) / (DISPLAY_BANDS - 1)) * 270.0f;

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
