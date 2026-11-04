#pragma once

#include <stdint.h>

struct BitmapGlyph {
    char character;
    uint8_t width;
    // Each row uses the low `width` bits; the most significant used bit is
    // the leftmost pixel. The font height is always five pixels.
    uint8_t rows[5];
};

class BitmapPixelSink {
public:
    virtual ~BitmapPixelSink() {}
    virtual void setPixel(int16_t x, int16_t y,
                          uint8_t red, uint8_t green, uint8_t blue) = 0;
};

// Small reusable bitmap font and text renderer. Letters use 4x5 glyphs;
// digits retain their compact 3x5 shapes. Unknown characters render as '?'.
class BitmapFont {
public:
    static const uint8_t HEIGHT = 5;
    static const uint8_t DEFAULT_CHARACTER_SPACING = 1;

    static const BitmapGlyph& glyph(char character);
    static uint16_t textWidth(
        const char* text,
        uint8_t characterSpacing = DEFAULT_CHARACTER_SPACING);
    static void drawText(
        BitmapPixelSink& sink, const char* text, int16_t x, int16_t y,
        uint8_t red, uint8_t green, uint8_t blue,
        uint8_t characterSpacing = DEFAULT_CHARACTER_SPACING);
};

// Non-blocking right-to-left marquee position. One pixel advances per
// configured interval; callers choose when and how often to draw the text.
class BitmapTextScroller {
public:
    static const uint32_t DEFAULT_INTERVAL_MS = 100;

    explicit BitmapTextScroller(
        uint32_t intervalMs = DEFAULT_INTERVAL_MS);

    void setIntervalMs(uint32_t intervalMs);
    uint32_t intervalMs() const { return _intervalMs; }

    void reset(uint32_t nowMs, int16_t x = 8);
    void setPosition(uint32_t nowMs, int16_t x);
    void retime(uint32_t nowMs);
    bool advance(uint32_t nowMs, uint16_t textWidth,
                 uint8_t viewportWidth);
    int16_t x() const { return _x; }

private:
    int16_t _x;
    uint32_t _lastStepAt;
    uint32_t _intervalMs;
};
