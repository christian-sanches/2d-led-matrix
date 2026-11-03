#pragma once
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
//   DisplayController display(LedQuantity, PIN_LED_DATA);
//   display.begin();
//   display.setPixel(2, 3, 255, 0, 0); // x, y, r, g, b
//   display.show();

class DisplayController {
public:
    // ledCount: total number of LEDs (e.g. 64 for 8x8)
    // dataPin: GPIO pin connected to NeoPixel DIN
    DisplayController(uint16_t ledCount, uint8_t dataPin);

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
    uint8_t sideSize() const { return _side; }

private:
    // Converts (x, y) into the linear strip index
    // Applies the snake-pattern for alternating row directions
    int getStripIndex(uint8_t x, uint8_t y) const;

    uint16_t _ledCount;                                  // Total number of LEDs
    uint8_t  _dataPin;                                   // Data pin connected to NeoPixel
    uint8_t  _side;                                      // Matrix side length (derived from ledCount)
    NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> _strip; // NeoPixel strip instance
};
