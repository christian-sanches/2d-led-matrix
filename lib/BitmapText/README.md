# BitmapText

A small C++11 bitmap font, text renderer, and non-blocking right-to-left
marquee helper. It has no display-library dependency, dynamic allocation, or
Arduino-specific calls.

## Font and drawing

Include `BitmapText.h` and implement `BitmapPixelSink::setPixel()`. The sink
receives signed coordinates so a display adapter can clip pixels outside its
viewport.

- Letters use 4×5 glyph cells (narrow letters may use fewer columns); digits
  use the existing compact 3×5 glyphs.
- Rows store the used bits from left to right; e.g. a 4-pixel row value of
  `12` (`1100` in binary) lights the first two columns.
- One blank column is inserted between characters by default.
- Characters not in the font render as `?`.

`BitmapFont::textWidth(text)` returns the rendered pixel width, and
`BitmapFont::drawText(sink, text, x, y, red, green, blue)` renders the text.

## Scrolling

`BitmapTextScroller` moves one pixel each interval and never blocks the caller:

```cpp
BitmapTextScroller scroller; // 100 ms per pixel by default
scroller.reset(nowMs, viewportWidth);
if (scroller.advance(nowMs, BitmapFont::textWidth(label), viewportWidth)) {
    renderAt(scroller.x());
}
scroller.setIntervalMs(70); // faster; can be changed at runtime
```

Call `advance()` from the application loop and draw when it returns `true`.
The caller owns the label and viewport, so it can pause and resume separate
marquees without allocating strings or buffers.
