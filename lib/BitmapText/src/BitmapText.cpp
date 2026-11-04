#include "BitmapText.h"

#include <stddef.h>

namespace {

const BitmapGlyph FONT[] = {
    {' ', 3, {0, 0, 0, 0, 0}},
    {'!', 1, {1, 1, 1, 0, 1}},
    {'%', 4, {9, 2, 4, 9, 0}},
    {'+', 3, {0, 2, 7, 2, 0}},
    {'.', 1, {0, 0, 0, 0, 1}},
    {'-', 3, {0, 0, 7, 0, 0}},
    {'/', 3, {1, 1, 2, 4, 4}},
    {'0', 3, {7, 5, 5, 5, 7}},
    {'1', 3, {2, 6, 2, 2, 7}},
    {'2', 3, {6, 1, 2, 4, 7}},
    {'3', 3, {6, 1, 2, 1, 6}},
    {'4', 3, {5, 5, 7, 1, 1}},
    {'5', 3, {7, 4, 6, 1, 6}},
    {'6', 3, {3, 4, 6, 5, 2}},
    {'7', 3, {7, 1, 2, 2, 2}},
    {'8', 3, {2, 5, 2, 5, 2}},
    {'9', 3, {2, 5, 3, 1, 6}},
    {':', 1, {0, 1, 0, 1, 0}},
    {'?', 4, {14, 1, 2, 0, 2}},
    {'A', 4, {6, 9, 15, 9, 9}},
    {'B', 4, {14, 9, 14, 9, 14}},
    {'C', 4, {7, 8, 8, 8, 7}},
    {'D', 4, {14, 9, 9, 9, 14}},
    {'E', 4, {15, 8, 14, 8, 15}},
    {'F', 4, {15, 8, 14, 8, 8}},
    {'G', 4, {7, 8, 11, 9, 7}},
    {'H', 4, {9, 9, 15, 9, 9}},
    {'I', 3, {7, 2, 2, 2, 7}},
    {'J', 4, {1, 1, 1, 9, 6}},
    {'K', 4, {9, 10, 12, 10, 9}},
    {'L', 4, {8, 8, 8, 8, 15}},
    {'M', 4, {9, 15, 15, 9, 9}},
    {'N', 4, {9, 13, 11, 9, 9}},
    {'O', 4, {6, 9, 9, 9, 6}},
    {'P', 4, {15, 9, 15, 8, 8}},
    {'Q', 4, {6, 9, 9, 11, 7}},
    {'R', 4, {14, 9, 14, 10, 9}},
    {'S', 4, {7, 8, 6, 1, 14}},
    {'T', 4, {15, 2, 2, 2, 2}},
    {'U', 4, {9, 9, 9, 9, 6}},
    {'V', 4, {9, 9, 9, 6, 6}},
    {'W', 4, {9, 9, 15, 15, 9}},
    {'X', 4, {9, 9, 6, 6, 9}},
    {'Y', 4, {9, 9, 6, 2, 2}},
    {'Z', 4, {15, 1, 2, 4, 15}},
    {'a', 4, {0, 6, 9, 7, 9}},
    {'b', 4, {8, 8, 14, 9, 14}},
    {'c', 4, {0, 6, 8, 8, 6}},
    {'d', 4, {1, 1, 7, 9, 7}},
    {'e', 4, {0, 6, 9, 15, 8}},
    {'f', 4, {6, 8, 14, 8, 8}},
    {'g', 4, {6, 9, 7, 1, 14}},
    {'h', 4, {8, 8, 14, 9, 9}},
    {'i', 1, {1, 0, 1, 1, 1}},
    {'j', 2, {1, 0, 1, 1, 2}},
    {'k', 4, {8, 8, 10, 12, 10}},
    {'l', 2, {2, 2, 2, 2, 3}},
    {'m', 4, {0, 14, 15, 9, 9}},
    {'n', 4, {0, 14, 9, 9, 9}},
    {'o', 4, {0, 6, 9, 9, 6}},
    {'p', 4, {14, 9, 14, 8, 8}},
    {'q', 4, {0, 6, 9, 7, 1}},
    {'r', 4, {0, 10, 12, 8, 8}},
    {'s', 4, {0, 7, 8, 6, 14}},
    {'t', 4, {4, 4, 14, 4, 6}},
    {'u', 4, {0, 9, 9, 9, 7}},
    {'v', 4, {0, 9, 9, 6, 6}},
    {'w', 4, {0, 9, 9, 15, 6}},
    {'x', 4, {0, 9, 6, 6, 9}},
    {'y', 4, {0, 9, 7, 1, 14}},
    {'z', 4, {0, 15, 2, 4, 15}}
};

uint32_t normalizedInterval(uint32_t intervalMs) {
    return intervalMs == 0 ? 1 : intervalMs;
}

} // namespace

const BitmapGlyph& BitmapFont::glyph(char character) {
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); ++i) {
        if (FONT[i].character == character) return FONT[i];
    }
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); ++i) {
        if (FONT[i].character == '?') return FONT[i];
    }
    return FONT[0];
}

uint16_t BitmapFont::textWidth(const char* text, uint8_t characterSpacing) {
    if (!text || text[0] == '\0') return 0;

    uint16_t width = 0;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        width = static_cast<uint16_t>(width + glyph(text[i]).width);
        if (text[i + 1] != '\0') {
            width = static_cast<uint16_t>(width + characterSpacing);
        }
    }
    return width;
}

void BitmapFont::drawText(BitmapPixelSink& sink, const char* text,
                          int16_t x, int16_t y, uint8_t red, uint8_t green,
                          uint8_t blue, uint8_t characterSpacing) {
    if (!text) return;

    int16_t cursorX = x;
    for (size_t i = 0; text[i] != '\0'; ++i) {
        const BitmapGlyph& current = glyph(text[i]);
        for (uint8_t row = 0; row < HEIGHT; ++row) {
            for (uint8_t column = 0; column < current.width; ++column) {
                const uint8_t mask = static_cast<uint8_t>(
                    1U << (current.width - 1U - column));
                if (current.rows[row] & mask) {
                    sink.setPixel(cursorX + column, y + row,
                                  red, green, blue);
                }
            }
        }
        cursorX = static_cast<int16_t>(cursorX + current.width);
        if (text[i + 1] != '\0') {
            cursorX = static_cast<int16_t>(cursorX + characterSpacing);
        }
    }
}

BitmapTextScroller::BitmapTextScroller(uint32_t intervalMs)
    : _x(0), _lastStepAt(0), _intervalMs(normalizedInterval(intervalMs)) {}

void BitmapTextScroller::setIntervalMs(uint32_t intervalMs) {
    _intervalMs = normalizedInterval(intervalMs);
}

void BitmapTextScroller::reset(uint32_t nowMs, int16_t x) {
    _x = x;
    _lastStepAt = nowMs;
}

void BitmapTextScroller::setPosition(uint32_t nowMs, int16_t x) {
    reset(nowMs, x);
}

void BitmapTextScroller::retime(uint32_t nowMs) {
    _lastStepAt = nowMs;
}

bool BitmapTextScroller::advance(uint32_t nowMs, uint16_t textWidth,
                                 uint8_t viewportWidth) {
    if (viewportWidth == 0 || textWidth <= viewportWidth ||
        static_cast<uint32_t>(nowMs - _lastStepAt) < _intervalMs) {
        return false;
    }

    --_x;
    const int32_t wrapThreshold = 2 - static_cast<int32_t>(textWidth);
    if (static_cast<int32_t>(_x) < wrapThreshold) {
        _x = viewportWidth;
    }
    _lastStepAt = nowMs;
    return true;
}
