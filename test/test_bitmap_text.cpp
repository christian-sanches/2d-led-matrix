#include "BitmapText.h"

#include <assert.h>
#include <stdio.h>

class TestPixelSink : public BitmapPixelSink {
public:
    struct Pixel { uint8_t red, green, blue; } pixels[64];

    TestPixelSink() {
        for (uint8_t i = 0; i < 64; ++i) pixels[i] = Pixel{0, 0, 0};
    }

    void setPixel(int16_t x, int16_t y, uint8_t red, uint8_t green,
                  uint8_t blue) {
        if (x < 0 || x >= 8 || y < 0 || y >= 8) return;
        pixels[y * 8 + x] = Pixel{red, green, blue};
    }
};

int main() {
    const BitmapGlyph& upperA = BitmapFont::glyph('A');
    const BitmapGlyph& lowerA = BitmapFont::glyph('a');
    const BitmapGlyph& digitFive = BitmapFont::glyph('5');
    assert(upperA.width == 4 && lowerA.width == 4);
    assert(digitFive.width == 3);
    assert(upperA.rows[0] == 6 && upperA.rows[2] == 15);
    assert(upperA.rows[0] != lowerA.rows[0]);
    assert(BitmapFont::glyph('~').character == '?');

    assert(BitmapFont::textWidth("") == 0);
    assert(BitmapFont::textWidth("A") == 4);
    assert(BitmapFont::textWidth("A5") == 8);
    assert(BitmapFont::textWidth("FPS 30") == 26);

    TestPixelSink sink;
    BitmapFont::drawText(sink, "A", 2, 1, 10, 20, 30);
    for (uint8_t row = 0; row < BitmapFont::HEIGHT; ++row) {
        for (uint8_t column = 0; column < upperA.width; ++column) {
            const TestPixelSink::Pixel& pixel =
                sink.pixels[(row + 1U) * 8U + 2U + column];
            const bool lit = (upperA.rows[row] &
                static_cast<uint8_t>(1U << (upperA.width - 1U - column))) != 0;
            assert((pixel.red == 10) == lit);
            assert((pixel.green == 20) == lit);
            assert((pixel.blue == 30) == lit);
        }
    }

    BitmapTextScroller scroller;
    assert(scroller.intervalMs() == BitmapTextScroller::DEFAULT_INTERVAL_MS);
    scroller.reset(1000, 8);
    assert(!scroller.advance(1099, 16, 8));
    assert(scroller.x() == 8);
    assert(scroller.advance(1100, 16, 8));
    assert(scroller.x() == 7);

    scroller.setIntervalMs(50);
    assert(!scroller.advance(1149, 16, 8));
    assert(scroller.advance(1150, 16, 8));
    assert(scroller.x() == 6);

    scroller.setIntervalMs(0);
    assert(scroller.intervalMs() == 1);
    scroller.reset(2000, 8);
    for (uint8_t step = 1; step <= 23; ++step) {
        assert(scroller.advance(2000U + step, 16, 8));
    }
    assert(scroller.x() == 8); // It wrapped after the full string passed by.

    puts("BitmapText tests passed");
    return 0;
}
