#include <array>
#include <cstdlib>

#include "render/TerminalPresentation.h"

int main() {
    std::array<float, musevis::NUM_BANDS> smoothed{};
    std::array<float, musevis::NUM_BANDS> peaks{};
    musevis::BandData loud{};
    loud.magnitudes.fill(1.0f);

    musevis::applyTerminalPresentationFrame(smoothed, peaks, loud);

    musevis::BandData silent{};
    for (int i = 0; i < 30; ++i) {
        musevis::applyTerminalPresentationFrame(smoothed, peaks, silent);
    }

    for (int i = 0; i < musevis::NUM_BANDS; ++i) {
        if (smoothed[i] != 0.0f || peaks[i] != 0.0f) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
