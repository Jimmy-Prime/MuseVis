#include <cstdlib>
#include <iostream>

#include "dsp/TeensyBandLayout.h"

namespace {

bool expectTrue(const char* label, bool condition) {
    if (condition) {
        return true;
    }

    std::cerr << label << " failed\n";
    return false;
}

} // namespace

int main() {
    bool ok = true;

    ok = expectTrue("band count matches shared state",
                    musevis::kTeensyBandLayout.size() == musevis::NUM_BANDS) && ok;
    ok = expectTrue("first band starts at 40 Hz",
                    musevis::kTeensyBandLayout.front().centerHz == 40.0f) && ok;
    ok = expectTrue("last band reaches 16 kHz",
                    musevis::kTeensyBandLayout.back().centerHz == 16000.0f) && ok;
    ok = expectTrue("band labels are non-empty",
                    musevis::kTeensyBandLayout.front().label[0] != '\0'
                    && musevis::kTeensyBandLayout.back().label[0] != '\0') && ok;

    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
