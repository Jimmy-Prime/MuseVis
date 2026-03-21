#include "led/LEDController.h"

#include <cstring>
#include <stdexcept>
#include <string>

namespace musevis {

namespace {
    constexpr int GPIO_PIN    = 18;   // PWM; use 10 (SPI MOSI) if 3.5mm audio jack is in use
    constexpr int DMA_CHANNEL = 5;
}

LEDController::LEDController() {
    ledString_.freq   = WS2811_TARGET_FREQ;
    ledString_.dmanum = DMA_CHANNEL;

    ledString_.channel[0].gpionum    = GPIO_PIN;
    ledString_.channel[0].count      = LED_COUNT;
    ledString_.channel[0].invert     = 0;
    ledString_.channel[0].brightness = 255;
    ledString_.channel[0].strip_type = WS2811_STRIP_GRB;

    // Channel 1 unused
    ledString_.channel[1].gpionum = 0;
    ledString_.channel[1].count   = 0;

    const ws2811_return_t ret = ws2811_init(&ledString_);
    if (ret != WS2811_SUCCESS)
        throw std::runtime_error(std::string("ws2811_init failed: ") +
                                 ws2811_get_return_t_str(ret));
}

LEDController::~LEDController() {
    // Blank the strip before shutting down
    std::memset(ledString_.channel[0].leds, 0, LED_COUNT * sizeof(ws2811_led_t));
    ws2811_render(&ledString_);
    ws2811_fini(&ledString_);
}

void LEDController::render(const ws2811_led_t* pixels) {
    std::memcpy(ledString_.channel[0].leds, pixels, LED_COUNT * sizeof(ws2811_led_t));
    ws2811_render(&ledString_);
}

} // namespace musevis
