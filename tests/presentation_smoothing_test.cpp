#include <cstdlib>

#include "render/PresentationSmoothing.h"

int main() {
    const float rising = musevis::smoothPresentationLevel(0.0f, 1.0f);
    if (rising < 0.75f) {
        return EXIT_FAILURE;
    }

    float falling = 1.0f;
    for (int i = 0; i < 6; ++i) {
        falling = musevis::smoothPresentationLevel(falling, 0.0f);
    }

    if (falling > 0.2f) {
        return EXIT_FAILURE;
    }

    if (musevis::clampQuietTail(0.01f, 0.0f) != 0.0f) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
