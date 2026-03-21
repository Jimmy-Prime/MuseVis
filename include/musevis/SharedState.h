#pragma once

#include <array>
#include <atomic>

namespace musevis {

constexpr int ANALYSIS_BANDS     = 14;
constexpr int FFT_BANDS          = 16;
constexpr int MAX_ANALYSIS_BANDS = FFT_BANDS;
constexpr int DISPLAY_BANDS      = 16;
constexpr int LEDS_PER_BAND      = 16;
constexpr int FFT_SIZE           = 2048;
constexpr int CHUNK_SIZE         = 1024;
constexpr int SAMPLE_RATE        = 44100;
constexpr int NUM_CHANNELS       = 2;

struct BandData {
    std::array<float, MAX_ANALYSIS_BANDS> magnitudes{};  // [0, 1] normalized
    int bandCount{DISPLAY_BANDS};
};

// Lock-free double-buffer: audio thread writes to back, atomically swaps;
// render thread reads front. Occasional tearing is imperceptible at 60 FPS.
struct SharedState {
    BandData buffers[2];
    std::atomic<int> frontIndex{0};
    std::atomic<uint64_t> frameCounter{0};  // incremented by audio thread each published analysis frame

    BandData& backBuffer() {
        return buffers[1 - frontIndex.load(std::memory_order_acquire)];
    }

    void swapBuffers() {
        int front = frontIndex.load(std::memory_order_acquire);
        frontIndex.store(1 - front, std::memory_order_release);
        frameCounter.fetch_add(1, std::memory_order_release);
    }

    const BandData& frontBuffer() const {
        return buffers[frontIndex.load(std::memory_order_acquire)];
    }

    uint64_t frame() const {
        return frameCounter.load(std::memory_order_acquire);
    }
};

} // namespace musevis
