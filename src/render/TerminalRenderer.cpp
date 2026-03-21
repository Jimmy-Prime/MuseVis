#include "render/TerminalRenderer.h"
#include "dsp/TeensyBandLayout.h"
#include "led/ColorMapper.h"
#include "render/PresentationSmoothing.h"
#include "render/TerminalPresentation.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>

namespace musevis {

namespace {
    constexpr int   TARGET_FPS      = 30;     // terminal doesn't need 60
    constexpr int   BAR_WIDTH       = 4;        // characters per band column
    constexpr int   DISPLAY_HEIGHT  = LEDS_PER_BAND;
}

TerminalRenderer::TerminalRenderer(SharedState& state)
    : state_(state) {}

TerminalRenderer::~TerminalRenderer() {
    stop();
}

void TerminalRenderer::start() {
    running_ = true;
    initScreen();
    thread_ = std::thread(&TerminalRenderer::renderLoop, this);
}

void TerminalRenderer::stop() {
    running_ = false;
    if (thread_.joinable())
        thread_.join();
    restoreScreen();
}

void TerminalRenderer::initScreen() {
    std::printf("\033[?25l");   // hide cursor
    std::printf("\033[2J");     // clear screen
    std::fflush(stdout);
}

void TerminalRenderer::restoreScreen() {
    std::printf("\033[0m");     // reset colors
    std::printf("\033[?25h");   // show cursor
    std::printf("\033[%dH\n", DISPLAY_HEIGHT + 5); // move below display
    std::fflush(stdout);
}

void TerminalRenderer::renderLoop() {
    using clock = std::chrono::steady_clock;
    const auto frameDuration = std::chrono::microseconds(1'000'000 / TARGET_FPS);
    auto nextFrame = clock::now();

    while (running_) {
        // Detect stale audio: if frameCounter hasn't advanced, audio thread
        // is blocked (e.g. music paused). Force magnitudes toward zero.
        uint64_t currentFrame = state_.frame();
        BandData data = state_.frontBuffer();

        if (currentFrame == lastFrame_) {
            staleCount_++;
            // After ~200ms of no updates (6 frames at 30fps), zero out
            if (staleCount_ > 6) {
                for (auto& m : data.magnitudes)
                    m = 0.0f;
            }
        } else {
            lastFrame_ = currentFrame;
            staleCount_ = 0;
        }

        drawFrame(data);
        nextFrame += frameDuration;
        std::this_thread::sleep_until(nextFrame);
    }
}

void TerminalRenderer::drawFrame(const BandData& data) {
    // Keep only light presentation smoothing + peak hold in the renderer.
    applyTerminalPresentationFrame(smoothed_, peaks_, data);

    // Build frame into a string buffer to reduce write() calls
    std::string frame;
    frame.reserve(8192);

    // Move cursor to top-left
    frame += "\033[H";

    // Title
    frame += "\033[1;37m  MuseVis — Terminal Spectrum Analyzer\033[0m\n\n";

    // Draw bars top-down: row DISPLAY_HEIGHT = loudest, row 1 = quietest
    for (int row = DISPLAY_HEIGHT; row >= 1; --row) {
        // Level label on the left
        char label[8];
        std::snprintf(label, sizeof(label), " %2d ", row);
        frame += "\033[38;5;240m";  // dim gray for labels
        frame += label;

        for (int b = 0; b < NUM_BANDS; ++b) {
            const int litCount = std::clamp(
                static_cast<int>(std::round(smoothed_[b] * DISPLAY_HEIGHT)),
                0, DISPLAY_HEIGHT);
            const int peakRow = std::clamp(
                static_cast<int>(std::round(peaks_[b] * DISPLAY_HEIGHT)),
                0, DISPLAY_HEIGHT);

            // Compute this band's color (HSV hue sweep red→violet)
            const float hue = (static_cast<float>(b) / (NUM_BANDS - 1)) * 270.0f;
            const RGB c = hsvToRgb(hue, 1.0f, 1.0f);

            if (row <= litCount) {
                // Lit segment — bright color with dimming toward the bottom
                const float brightness = 0.4f + 0.6f * (static_cast<float>(row) / DISPLAY_HEIGHT);
                const int r = static_cast<int>(c.r * brightness);
                const int g = static_cast<int>(c.g * brightness);
                const int bl = static_cast<int>(c.b * brightness);

                char color[32];
                std::snprintf(color, sizeof(color), "\033[48;2;%d;%d;%dm", r, g, bl);
                frame += color;
                frame += "    ";  // 4-char wide colored block
                frame += "\033[0m";
            } else if (row == peakRow && peakRow > 0) {
                // Peak hold indicator — thin colored line
                char color[32];
                std::snprintf(color, sizeof(color), "\033[38;2;%d;%d;%dm", c.r, c.g, c.b);
                frame += color;
                frame += "────";
                frame += "\033[0m";
            } else {
                // Empty segment
                frame += "    ";
            }
        }
        frame += "\n";
    }

    // Bottom axis: separator line
    frame += "     ";
    for (int b = 0; b < NUM_BANDS; ++b)
        frame += "────";
    frame += "\n";

    // Frequency labels
    frame += "     ";
    for (int b = 0; b < NUM_BANDS; ++b) {
        char cell[8];
        std::snprintf(cell, sizeof(cell), "%-4s", kTeensyBandLayout[b].label);
        frame += cell;
    }
    frame += "\n";
    frame += "\033[38;5;240m     Hz\033[0m\n";

    std::fwrite(frame.data(), 1, frame.size(), stdout);
    std::fflush(stdout);
}

} // namespace musevis
