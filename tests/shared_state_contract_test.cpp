#include <cstdlib>
#include <cmath>
#include <iostream>

#include "musevis/SharedState.h"

namespace {

bool expectEqual(const char* label, int actual, int expected) {
    if (actual == expected) {
        return true;
    }

    std::cerr << label << " expected " << expected << " but got " << actual << '\n';
    return false;
}

bool expectNear(const char* label, float actual, float expected, float epsilon) {
    if (std::fabs(actual - expected) <= epsilon) {
        return true;
    }

    std::cerr << label << " expected " << expected << " but got " << actual << '\n';
    return false;
}

} // namespace

int main() {
    bool ok = true;
    musevis::SharedState state;

    ok = expectEqual("NUM_BANDS", musevis::NUM_BANDS, 14) && ok;
    ok = expectEqual("LEDS_PER_BAND", musevis::LEDS_PER_BAND, 20) && ok;
    ok = expectEqual("BandData size",
                     static_cast<int>(musevis::BandData{}.magnitudes.size()),
                     musevis::NUM_BANDS) && ok;

    state.backBuffer().magnitudes[0] = 0.75f;
    ok = expectNear("Front buffer should not see back-buffer writes before swap",
                    state.frontBuffer().magnitudes[0],
                    0.0f,
                    0.0001f) && ok;

    state.swapBuffers();
    ok = expectNear("Front buffer should publish swapped magnitude",
                    state.frontBuffer().magnitudes[0],
                    0.75f,
                    0.0001f) && ok;
    ok = expectEqual("Frame counter increments on publish",
                     static_cast<int>(state.frame()),
                     1) && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
