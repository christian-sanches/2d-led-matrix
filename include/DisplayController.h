#pragma once
#include "MatrixFormat.h"
#include <NeoPixelBus.h>

// =========================================================
// DisplayController - Manages the 8x8 LED Matrix
// =========================================================
// Responsibilities:
//   - Initialize and control the NeoPixel LED strip
//   - Convert X,Y coordinates to physical LED index
//   - Provide clear(), show(), and setPixel() methods
//
// Expected usage:
//   DisplayController display(PIN_LED_DATA);
//   display.begin();
//   display.setPixel(2, 3, 255, 0, 0); // x, y, r, g, b
//   display.show();

class DisplayController {
public:
    // dataPin is an ESP8266 GPIO number, not a NodeMCU Dx label.
    explicit DisplayController(uint8_t dataPin);

    // Initializes the LED strip (call once in setup())
    void begin();

    // Sets a pixel color at position (x, y)
    // Coordinates: x = left-to-right, y = top-to-bottom
    void setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);

    // Sends the pixel buffer to the physical matrix
    void show();

    // Turns off all LEDs (sets them to black)
    void clear();

    // Returns the matrix side length (e.g. 8 for 8x8)
    // Used by AnimationController to know the dimensions
    uint8_t sideSize() const { return MatrixFormat::kWidth; }

    // Brightness is applied to the original source RGB values, not to already
    // scaled output, so repeated changes never accumulate rounding loss.
    void setBrightness(uint8_t brightness);
    void fill(uint8_t r, uint8_t g, uint8_t b);

private:
    // Converts (x, y) into the linear strip index
    // Applies the snake-pattern for alternating row directions
    int getStripIndex(uint8_t x, uint8_t y) const;
    void updatePixel(uint8_t x, uint8_t y);

    uint8_t _brightness;
    uint8_t _sourceRgb[MatrixFormat::kFrameBytes];
#if defined(ARDUINO_ARCH_ESP8266)
    // ESP8266 DMA ignores the requested pin and outputs on GPIO3. Bit-bang
    // honors GPIO4; its timing/IR limitations are documented in README.md.
    using MatrixPixelMethod = NeoEsp8266BitBangWs2812xMethod;
#elif defined(ARDUINO_ARCH_ESP32)
    // The separate Wokwi target uses RMT explicitly; NeoWs2812xMethod
    // defaults to I2S on the original ESP32, which Wokwi does not simulate.
    using MatrixPixelMethod = NeoEsp32Rmt0Ws2812xMethod;
#else
#error "DisplayController supports ESP8266 firmware and the ESP32 Wokwi target only"
#endif
    NeoPixelBus<NeoGrbFeature, MatrixPixelMethod> _strip;
};
