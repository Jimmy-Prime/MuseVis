#pragma once

#include <cmath>
#include <cstdint>

namespace musevis {

struct RGB {
    uint8_t r, g, b;
};

// Convert HSV to RGB. h in [0, 360), s and v in [0, 1].
inline RGB hsvToRgb(float h, float s, float v) {
    if (s == 0.0f) {
        const uint8_t c = static_cast<uint8_t>(v * 255.0f);
        return {c, c, c};
    }

    h = std::fmod(h, 360.0f) / 60.0f;
    const int   i = static_cast<int>(h);
    const float f = h - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - s * f);
    const float t = v * (1.0f - s * (1.0f - f));

    float r, g, b;
    switch (i) {
        case 0:  r = v; g = t; b = p; break;
        case 1:  r = q; g = v; b = p; break;
        case 2:  r = p; g = v; b = t; break;
        case 3:  r = p; g = q; b = v; break;
        case 4:  r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return {
        static_cast<uint8_t>(r * 255.0f),
        static_cast<uint8_t>(g * 255.0f),
        static_cast<uint8_t>(b * 255.0f),
    };
}

} // namespace musevis
