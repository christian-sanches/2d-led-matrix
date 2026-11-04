#pragma once

#include <stdint.h>

enum class ControlAction : uint8_t {
    None,
    PowerToggle,
    BrightnessUp,
    BrightnessDown,
    PreviousPreset,
    NextPreset,
    SolidRed,
    SolidGreen,
    SolidBlue,
    SolidWhite
};

enum class DisplayMode : uint8_t {
    Animation,
    Solid
};

struct RgbColorValue {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

class ControlState {
public:
    ControlState()
        : _powered(true), _brightness(96), _mode(DisplayMode::Animation),
          _solidColor{255, 255, 255}, _solidColorIndex(6) {}

    void apply(ControlAction action) {
        switch (action) {
            case ControlAction::PowerToggle:
                _powered = !_powered;
                break;
            case ControlAction::BrightnessUp:
                _brightness = _brightness > 223 ? 255 : _brightness + 32;
                break;
            case ControlAction::BrightnessDown:
                _brightness = _brightness < 32 ? 0 : _brightness - 32;
                break;
            case ControlAction::PreviousPreset:
            case ControlAction::NextPreset:
                _mode = DisplayMode::Animation;
                break;
            case ControlAction::SolidRed:
                selectSolid(0, 255, 0, 0);
                break;
            case ControlAction::SolidGreen:
                selectSolid(1, 0, 255, 0);
                break;
            case ControlAction::SolidBlue:
                selectSolid(2, 0, 0, 255);
                break;
            case ControlAction::SolidWhite:
                selectSolid(6, 255, 255, 255);
                break;
            case ControlAction::None:
            default:
                break;
        }
    }

    bool isPowered() const { return _powered; }
    uint8_t brightness() const { return _brightness; }
    DisplayMode mode() const { return _mode; }
    RgbColorValue solidColor() const { return _solidColor; }
    uint8_t solidColorIndex() const { return _solidColorIndex; }

    // Direct setters let the menu update the same shared state as IR/button
    // actions without introducing a second copy of power, color, or mode.
    void setPowered(bool powered) { _powered = powered; }
    void setBrightness(uint8_t brightness) { _brightness = brightness; }
    void setMode(DisplayMode mode) { _mode = mode; }
    void setSolidColor(uint8_t index, RgbColorValue color) {
        _solidColorIndex = index;
        _solidColor = color;
    }

private:
    void selectSolid(uint8_t index, uint8_t red, uint8_t green, uint8_t blue) {
        _mode = DisplayMode::Solid;
        _solidColorIndex = index;
        _solidColor = RgbColorValue{red, green, blue};
    }

    bool _powered;
    uint8_t _brightness;
    DisplayMode _mode;
    RgbColorValue _solidColor;
    uint8_t _solidColorIndex;
};
