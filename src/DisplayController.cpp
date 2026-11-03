#include "DisplayController.h"

// =========================================================
// CONSTRUCTOR
// =========================================================
// Initializes the NeoPixel strip with the given LED count
// and data pin. Automatically computes the matrix side length
// (e.g. 8 for 64 LEDs, 16 for 256 LEDs).
DisplayController::DisplayController(uint16_t ledCount, uint8_t dataPin)
    : _ledCount(ledCount), _dataPin(dataPin), _strip(ledCount, dataPin) {
    // Calculate the side of the square matrix
    _side = 0;
    while (_side * _side < _ledCount) _side++;
}

// =========================================================
// begin
// =========================================================
// Call once in setup(). Prepares the NeoPixel strip and
// clears all LEDs to off.
void DisplayController::begin() {
    _strip.Begin();
    _strip.Show();
}

// =========================================================
// setPixel
// =========================================================
// Sets the color of a pixel at coordinates (x, y).
// The coordinate is converted to the physical strip index
// using the snake-pattern (see getStripIndex for details).
void DisplayController::setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    _strip.SetPixelColor(getStripIndex(x, y), RgbColor(r, g, b));
}

// =========================================================
// show
// =========================================================
// Sends all buffered pixel data to the physical LED matrix.
// Always call this after setPixel() to make colors appear.
void DisplayController::show() {
    _strip.Show();
}

// =========================================================
// clear
// =========================================================
// Turns off all LEDs (sets them to black).
// Calls show() internally so the change takes effect immediately.
void DisplayController::clear() {
    _strip.ClearTo(RgbColor(0, 0, 0));
    _strip.Show();
}

// =========================================================
// getStripIndex (private)
// =========================================================
// Converts cartesian coordinates (x, y) to the linear index
// on the NeoPixel strip.
//
// 8x8 LED matrices typically route the LEDs in a snake pattern:
//   Even rows (0, 2, 4...):  left -> right
//   Odd rows  (1, 3, 5...):  right -> left
//
// This matches how the physical LEDs are wired internally.
int DisplayController::getStripIndex(uint8_t x, uint8_t y) const {
    if (y % 2 == 0) {
        // Even row: left to right
        return (y * _side) + x;
    } else {
        // Odd row: right to left
        return (y * _side) - (_side - 1 - x);
    }
}
