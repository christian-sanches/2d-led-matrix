#include "DisplayController.h"

// =========================================================
// CONSTRUCTOR
// =========================================================
// The project contract is one fixed 8x8 matrix (64 pixels).
DisplayController::DisplayController(uint8_t dataPin)
    : _brightness(255), _strip(MatrixFormat::kPixelCount, dataPin) {
    for (size_t i = 0; i < sizeof(_sourceRgb); ++i) _sourceRgb[i] = 0;
}

// =========================================================
// begin
// =========================================================
// Call once in setup(). Prepares the NeoPixel strip and
// clears all LEDs to off.
void DisplayController::begin() {
    _strip.Begin();
    _strip.ClearTo(RgbColor(0, 0, 0));
    _strip.Show();
}

// =========================================================
// setPixel
// =========================================================
// Sets the color of a pixel at coordinates (x, y).
// The coordinate is converted to the physical strip index
// using the snake-pattern (see getStripIndex for details).
void DisplayController::setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    if (x >= MatrixFormat::kWidth || y >= MatrixFormat::kHeight) return;
    const size_t sourceIndex = MatrixFormat::logicalRgbOffset(x, y, 0);
    _sourceRgb[sourceIndex] = r;
    _sourceRgb[sourceIndex + 1] = g;
    _sourceRgb[sourceIndex + 2] = b;
    updatePixel(x, y);
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
    for (size_t i = 0; i < sizeof(_sourceRgb); ++i) _sourceRgb[i] = 0;
    _strip.ClearTo(RgbColor(0, 0, 0));
    _strip.Show();
}

void DisplayController::setBrightness(uint8_t brightness) {
    _brightness = brightness;
    for (uint8_t y = 0; y < MatrixFormat::kHeight; ++y) {
        for (uint8_t x = 0; x < MatrixFormat::kWidth; ++x) updatePixel(x, y);
    }
    _strip.Show();
}

void DisplayController::fill(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t y = 0; y < MatrixFormat::kHeight; ++y) {
        for (uint8_t x = 0; x < MatrixFormat::kWidth; ++x) setPixel(x, y, r, g, b);
    }
    _strip.Show();
}

void DisplayController::updatePixel(uint8_t x, uint8_t y) {
    const size_t sourceIndex = MatrixFormat::logicalRgbOffset(x, y, 0);
    const uint8_t r = MatrixFormat::scaleChannel(_sourceRgb[sourceIndex], _brightness);
    const uint8_t g = MatrixFormat::scaleChannel(_sourceRgb[sourceIndex + 1], _brightness);
    const uint8_t b = MatrixFormat::scaleChannel(_sourceRgb[sourceIndex + 2], _brightness);
    _strip.SetPixelColor(getStripIndex(x, y), RgbColor(r, g, b));
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
    return MatrixFormat::serpentineIndex(x, y);
}
