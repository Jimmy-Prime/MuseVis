#pragma once

#include <ws2811.h>
#include "musevis/SharedState.h"

namespace musevis {

class LEDController {
public:
    LEDController();
    ~LEDController();

    // Write one full 14-band frame to the LED strip.
    void render(const ws2811_led_t* pixels);

private:
    ws2811_t ledString_{};
};

} // namespace musevis
