#pragma once

#include <ws2811.h>
#include "musevis/SharedState.h"

namespace musevis {

class LEDController {
public:
    LEDController();
    ~LEDController();

    // Write DISPLAY_BANDS * LEDS_PER_BAND pixels to the LED strip.
    void render(const ws2811_led_t* pixels);

private:
    ws2811_t ledString_{};
};

} // namespace musevis
