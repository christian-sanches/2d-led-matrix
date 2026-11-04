#include "MenuController.h"

#include <stdio.h>
#include <string.h>

namespace {

struct Glyph {
    char character;
    uint8_t rows[5];
};

// Compact 3x5 uppercase font. It keeps the scrolling labels legible on an
// 8x8 matrix without adding a display/font library or dynamic allocation.
const Glyph FONT[] = {
    {' ', {0, 0, 0, 0, 0}}, {'+', {0, 2, 7, 2, 0}},
    {'-', {0, 0, 7, 0, 0}}, {'.', {0, 0, 0, 0, 2}},
    {'%', {5, 1, 2, 4, 5}},
    {'A', {2, 5, 7, 5, 5}}, {'B', {6, 5, 6, 5, 6}},
    {'C', {3, 4, 4, 4, 3}}, {'D', {6, 5, 5, 5, 6}},
    {'E', {7, 4, 6, 4, 7}}, {'F', {7, 4, 6, 4, 4}},
    {'G', {3, 4, 5, 5, 3}}, {'H', {5, 5, 7, 5, 5}},
    {'I', {7, 2, 2, 2, 7}}, {'J', {1, 1, 1, 5, 2}},
    {'K', {5, 5, 6, 5, 5}}, {'L', {4, 4, 4, 4, 7}},
    {'M', {5, 7, 7, 5, 5}}, {'N', {5, 7, 7, 7, 5}},
    {'O', {2, 5, 5, 5, 2}}, {'P', {6, 5, 6, 4, 4}},
    {'Q', {2, 5, 5, 3, 1}}, {'R', {6, 5, 6, 5, 5}},
    {'S', {3, 4, 2, 1, 6}}, {'T', {7, 2, 2, 2, 2}},
    {'U', {5, 5, 5, 5, 7}}, {'V', {5, 5, 5, 5, 2}},
    {'W', {5, 5, 7, 7, 5}}, {'X', {5, 5, 2, 5, 5}},
    {'Y', {5, 5, 2, 2, 2}}, {'Z', {7, 1, 2, 4, 7}},
    {'0', {7, 5, 5, 5, 7}}, {'1', {2, 6, 2, 2, 7}},
    {'2', {6, 1, 2, 4, 7}}, {'3', {6, 1, 2, 1, 6}},
    {'4', {5, 5, 7, 1, 1}}, {'5', {7, 4, 6, 1, 6}},
    {'6', {3, 4, 6, 5, 2}}, {'7', {7, 1, 2, 2, 2}},
    {'8', {2, 5, 2, 5, 2}}, {'9', {2, 5, 3, 1, 6}}
};

const MenuRgb UI_COLOR = {0, 180, 255};
const MenuRgb ON_COLOR = {0, 220, 40};
const MenuRgb OFF_COLOR = {255, 24, 0};
const MenuRgb MARKER_COLOR = {255, 255, 255};
const MenuRgb BLACK_COLOR = {0, 0, 0};

const MenuRgb PALETTE[MenuController::COLOR_COUNT] = {
    {255, 0, 0},       // red
    {0, 255, 0},       // green
    {0, 0, 255},       // blue
    {255, 255, 0},     // yellow
    {0, 255, 255},     // cyan
    {255, 0, 255},     // magenta
    {255, 255, 255},   // white
    {255, 96, 0}       // amber
};

const char* const COLOR_NAMES[MenuController::COLOR_COUNT] = {
    "RED", "GREEN", "BLUE", "YELLOW", "CYAN", "MAGENTA", "WHITE", "AMBER"
};

const uint8_t FPS_PRESETS[MenuController::FPS_PRESET_COUNT] = {15, 24, 30, 48, 60};

const Glyph& glyphFor(char character) {
    for (size_t i = 0; i < sizeof(FONT) / sizeof(FONT[0]); ++i) {
        if (FONT[i].character == character) return FONT[i];
    }
    return FONT[0];
}

void pixel(MenuDisplay& display, int16_t x, uint8_t y, const MenuRgb& color) {
    if (x >= 0 && x < 8 && y < 8) {
        display.setPixel(static_cast<uint8_t>(x), y,
                         color.red, color.green, color.blue);
    }
}

void drawText(MenuDisplay& display, int16_t startX, const char* text,
              const MenuRgb& color) {
    for (size_t characterIndex = 0; text[characterIndex] != '\0'; ++characterIndex) {
        const Glyph& glyph = glyphFor(text[characterIndex]);
        const int16_t glyphX = startX + static_cast<int16_t>(characterIndex * 4U);
        for (uint8_t row = 0; row < 5; ++row) {
            for (uint8_t column = 0; column < 3; ++column) {
                if (glyph.rows[row] & static_cast<uint8_t>(1U << (2U - column))) {
                    pixel(display, glyphX + column, row + 1, color);
                }
            }
        }
    }
}

void drawStatusBar(MenuDisplay& display, uint8_t segments, const MenuRgb& color) {
    if (segments > 8) segments = 8;
    for (uint8_t x = 0; x < segments; ++x) pixel(display, x, 7, color);
}

MenuPage nextPage(MenuPage page, int8_t direction) {
    int8_t index = static_cast<int8_t>(page) + direction;
    if (index < 0) index = 3;
    if (index > 3) index = 0;
    return static_cast<MenuPage>(index);
}

} // namespace

MenuController::MenuController(MenuActions& actions, uint32_t timeoutMs)
    : _actions(actions),
      _timeoutMs(normalizedTimeout(timeoutMs)),
      _lastActivityMs(0),
      _page(MenuPage::Brightness),
      _active(false),
      _editing(false),
      _scrollX(0),
      _lastScrollAt(0) {}

uint8_t MenuController::clampFps(uint8_t value) {
    for (uint8_t i = 0; i < FPS_PRESET_COUNT; ++i) {
        if (FPS_PRESETS[i] == value) return value;
    }
    return FPS_PRESETS[2];
}

uint8_t MenuController::nextFpsPreset(uint8_t value) {
    for (uint8_t i = 0; i < FPS_PRESET_COUNT; ++i) {
        if (FPS_PRESETS[i] == value) return FPS_PRESETS[(i + 1) % FPS_PRESET_COUNT];
    }
    return FPS_PRESETS[2];
}

uint8_t MenuController::clampColorIndex(uint8_t value) {
    return value < COLOR_COUNT ? value : 0;
}

uint8_t MenuController::wrapColorIndex(int16_t value) {
    if (value < 0) return COLOR_COUNT - 1;
    if (value >= COLOR_COUNT) return 0;
    return static_cast<uint8_t>(value);
}

MenuRgb MenuController::colorAt(uint8_t index) {
    return PALETTE[clampColorIndex(index)];
}

const char* MenuController::pageName(MenuPage page) {
    switch (page) {
    case MenuPage::Brightness: return "brightness";
    case MenuPage::Wifi: return "Wi-Fi";
    case MenuPage::SolidColor: return "solid color";
    case MenuPage::Fps: return "FPS";
    }
    return "unknown";
}

uint32_t MenuController::normalizedTimeout(uint32_t value) const {
    if (value < MIN_TIMEOUT_MS) return MIN_TIMEOUT_MS;
    if (value > MAX_TIMEOUT_MS) return MAX_TIMEOUT_MS;
    return value;
}

uint8_t MenuController::formatLabel(char* text, size_t capacity,
                                    MenuRgb& color) const {
    if (capacity == 0) return 0;
    int written = 0;
    switch (_page) {
    case MenuPage::Brightness: {
        color = UI_COLOR;
        const unsigned percent =
            (static_cast<unsigned>(_actions.brightness()) * 100U + 127U) / 255U;
        written = snprintf(text, capacity, "BRIGHT %u%%", percent);
        break;
    }
    case MenuPage::Wifi:
        color = _actions.wifiEnabled() ? ON_COLOR : OFF_COLOR;
        written = snprintf(text, capacity, "WIFI %s",
                           _actions.wifiEnabled() ? "ON" : "OFF");
        break;
    case MenuPage::SolidColor: {
        const uint8_t index = clampColorIndex(_actions.solidColorIndex());
        color = PALETTE[index];
        written = snprintf(text, capacity, "%s %s", COLOR_NAMES[index],
                           _actions.solidModeEnabled() ? "SOLID" : "ANIM");
        break;
    }
    case MenuPage::Fps:
        color = UI_COLOR;
        written = snprintf(text, capacity, "FPS %u",
                           static_cast<unsigned>(clampFps(_actions.fps())));
        break;
    }
    if (written < 0) {
        text[0] = '\0';
        return 0;
    }
    if (static_cast<size_t>(written) >= capacity) written = static_cast<int>(capacity - 1);
    return static_cast<uint8_t>(written);
}

void MenuController::resetScroll(uint32_t nowMs) {
    _scrollX = 0;
    _lastScrollAt = nowMs;
}

void MenuController::enter(uint32_t nowMs) {
    _active = true;
    _editing = false;
    _page = MenuPage::Brightness;
    _lastActivityMs = nowMs;
    resetScroll(nowMs);
}

void MenuController::exit() {
    _active = false;
    _editing = false;
}

MenuUpdateResult MenuController::update(uint32_t nowMs, MenuInput input) {
    if (_active && static_cast<uint32_t>(nowMs - _lastActivityMs) >= _timeoutMs) {
        exit();
        return MenuUpdateResult::Exited;
    }

    if (!_active) {
        if (input == MenuInput::Menu) {
            enter(nowMs);
            return MenuUpdateResult::Entered;
        }
        return MenuUpdateResult::NoChange;
    }

    if (input != MenuInput::None) {
        _lastActivityMs = nowMs;
        const MenuUpdateResult result = handleActiveInput(nowMs, input);
        if (result == MenuUpdateResult::Changed) resetScroll(nowMs);
        return result;
    }

    if (static_cast<uint32_t>(nowMs - _lastScrollAt) >= SCROLL_STEP_MS) {
        char text[24];
        MenuRgb color = UI_COLOR;
        const uint8_t length = formatLabel(text, sizeof(text), color);
        const int16_t textWidth = static_cast<int16_t>(length * 4U);
        ++_scrollX;
        if (textWidth > 0 && _scrollX >= textWidth) _scrollX = 0;
        _lastScrollAt = nowMs;
        return MenuUpdateResult::Animated;
    }

    return MenuUpdateResult::NoChange;
}

void MenuController::noteActivity(uint32_t nowMs) {
    if (!_active) return;
    _lastActivityMs = nowMs;
    resetScroll(nowMs);
}

MenuUpdateResult MenuController::handleActiveInput(uint32_t,
                                                    MenuInput input) {
    if (input == MenuInput::Menu) {
        exit();
        return MenuUpdateResult::Exited;
    }

    if (input == MenuInput::Back) {
        if (_editing) {
            _editing = false;
            return MenuUpdateResult::Changed;
        }
        exit();
        return MenuUpdateResult::Exited;
    }

    if (!_editing && (input == MenuInput::Up || input == MenuInput::Down)) {
        _page = nextPage(_page, input == MenuInput::Up ? -1 : 1);
        return MenuUpdateResult::Changed;
    }

    if (_editing && (input == MenuInput::Up || input == MenuInput::Down)) {
        return adjust(input == MenuInput::Up ? 1 : 0);
    }

    if (input != MenuInput::Select) return MenuUpdateResult::NoChange;

    if (_editing) {
        if (_page == MenuPage::SolidColor) {
            _actions.setSolidModeEnabled(!_actions.solidModeEnabled());
            return MenuUpdateResult::Changed;
        }
        _editing = false;
        return MenuUpdateResult::Changed;
    }

    switch (_page) {
    case MenuPage::Brightness:
    case MenuPage::SolidColor:
        _editing = true;
        return MenuUpdateResult::Changed;
    case MenuPage::Wifi:
        _actions.setWifiEnabled(!_actions.wifiEnabled());
        return MenuUpdateResult::Changed;
    case MenuPage::Fps:
        _actions.setFps(nextFpsPreset(clampFps(_actions.fps())));
        return MenuUpdateResult::Changed;
    }
    return MenuUpdateResult::NoChange;
}

MenuUpdateResult MenuController::adjust(uint8_t direction) {
    switch (_page) {
    case MenuPage::Brightness: {
        const uint8_t value = _actions.brightness();
        uint8_t next = value;
        if (direction) {
            next = value > static_cast<uint8_t>(255U - BRIGHTNESS_STEP)
                       ? 255
                       : static_cast<uint8_t>(value + BRIGHTNESS_STEP);
        } else {
            next = value < BRIGHTNESS_STEP
                       ? 0
                       : static_cast<uint8_t>(value - BRIGHTNESS_STEP);
        }
        if (next == value) return MenuUpdateResult::NoChange;
        _actions.setBrightness(next);
        return MenuUpdateResult::Changed;
    }
    case MenuPage::SolidColor: {
        const int16_t next = static_cast<int16_t>(clampColorIndex(
                                 _actions.solidColorIndex())) +
                             (direction ? 1 : -1);
        const uint8_t index = wrapColorIndex(next);
        if (index == _actions.solidColorIndex()) return MenuUpdateResult::NoChange;
        _actions.setSolidColorIndex(index);
        return MenuUpdateResult::Changed;
    }
    case MenuPage::Wifi:
    case MenuPage::Fps:
        break;
    }
    return MenuUpdateResult::NoChange;
}

void MenuController::render(MenuDisplay& display) const {
    if (!_active) return;

    for (uint8_t y = 0; y < 8; ++y) {
        for (uint8_t x = 0; x < 8; ++x) pixel(display, x, y, BLACK_COLOR);
    }

    char text[24];
    MenuRgb color = UI_COLOR;
    const uint8_t length = formatLabel(text, sizeof(text), color);
    const int16_t textWidth = static_cast<int16_t>(length * 4U);
    drawText(display, _scrollX, text, color);
    if (textWidth > 0) drawText(display, _scrollX - textWidth, text, color);

    switch (_page) {
    case MenuPage::Brightness: {
        uint8_t segments = static_cast<uint16_t>(_actions.brightness()) * 8U / 255U;
        if (_actions.brightness() == 255) segments = 8;
        drawStatusBar(display, segments, ON_COLOR);
        break;
    }
    case MenuPage::Wifi:
        if (_actions.wifiEnabled()) drawStatusBar(display, 8, ON_COLOR);
        else {
            pixel(display, 3, 7, OFF_COLOR);
            pixel(display, 4, 7, OFF_COLOR);
        }
        break;
    case MenuPage::SolidColor:
        drawStatusBar(display, 8, PALETTE[clampColorIndex(_actions.solidColorIndex())]);
        pixel(display, 7, 0,
              _actions.solidModeEnabled() ? ON_COLOR : OFF_COLOR);
        break;
    case MenuPage::Fps: {
        const uint8_t fps = clampFps(_actions.fps());
        uint8_t preset = 0;
        while (preset + 1 < FPS_PRESET_COUNT && FPS_PRESETS[preset] != fps) ++preset;
        drawStatusBar(display, static_cast<uint8_t>(preset + 1), ON_COLOR);
        break;
    }
    }

    if (_editing) pixel(display, 7, 6, MARKER_COLOR);
    display.show();
}
