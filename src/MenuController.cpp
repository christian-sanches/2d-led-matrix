#include "MenuController.h"

#include <stdio.h>

namespace {

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
    "Red", "Green", "Blue", "Yellow", "Cyan", "Magenta", "White", "Amber"
};

const uint8_t FPS_PRESETS[MenuController::FPS_PRESET_COUNT] = {15, 24, 30, 48, 60};

void pixel(MenuDisplay& display, int16_t x, uint8_t y, const MenuRgb& color) {
    if (x >= 0 && x < 8 && y < 8) {
        display.setPixel(static_cast<uint8_t>(x), y,
                         color.red, color.green, color.blue);
    }
}

class MenuFontSink : public BitmapPixelSink {
public:
    explicit MenuFontSink(MenuDisplay& display) : _display(display) {}

    void setPixel(int16_t x, int16_t y, uint8_t red, uint8_t green,
                  uint8_t blue) {
        if (x >= 0 && x < 8 && y >= 0 && y < 8) {
            _display.setPixel(static_cast<uint8_t>(x),
                              static_cast<uint8_t>(y), red, green, blue);
        }
    }

private:
    MenuDisplay& _display;
};

void drawText(MenuDisplay& display, const char* text, int16_t x,
              const MenuRgb& color) {
    MenuFontSink sink(display);
    BitmapFont::drawText(sink, text, x, 1, color.red, color.green, color.blue);
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
      _labelScroller(SCROLL_STEP_MS),
      _valueScroller(SCROLL_STEP_MS),
      _lastValueChangeMs(0),
      _showingValue(false),
      _valueModeOnly(false) {}

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

void MenuController::formatLabel(char* text, size_t capacity,
                                 MenuRgb& color) const {
    if (capacity == 0) return;
    switch (_page) {
    case MenuPage::Brightness: {
        color = UI_COLOR;
        const unsigned percent =
            (static_cast<unsigned>(_actions.brightness()) * 100U + 127U) / 255U;
        snprintf(text, capacity, "Bright %u%%", percent);
        break;
    }
    case MenuPage::Wifi:
        color = _actions.wifiEnabled() ? ON_COLOR : OFF_COLOR;
        snprintf(text, capacity, "Wi-Fi %s",
                           _actions.wifiEnabled() ? "ON" : "OFF");
        break;
    case MenuPage::SolidColor: {
        const uint8_t index = clampColorIndex(_actions.solidColorIndex());
        color = PALETTE[index];
        snprintf(text, capacity, "%s %s", COLOR_NAMES[index],
                 _actions.solidModeEnabled() ? "Solid" : "Anim");
        break;
    }
    case MenuPage::Fps:
        color = UI_COLOR;
        snprintf(text, capacity, "FPS %u",
                 static_cast<unsigned>(clampFps(_actions.fps())));
        break;
    }
}

void MenuController::formatValueLabel(char* text, size_t capacity,
                                      MenuRgb& color) const {
    if (capacity == 0) return;
    switch (_page) {
    case MenuPage::Brightness: {
        const unsigned percent =
            (static_cast<unsigned>(_actions.brightness()) * 100U + 127U) / 255U;
        color = UI_COLOR;
        snprintf(text, capacity, "%u", percent);
        break;
    }
    case MenuPage::Wifi:
        color = _actions.wifiEnabled() ? ON_COLOR : OFF_COLOR;
        snprintf(text, capacity, "%s",
                 _actions.wifiEnabled() ? "ON" : "OFF");
        break;
    case MenuPage::SolidColor:
        if (_valueModeOnly) {
            color = _actions.solidModeEnabled() ? ON_COLOR : OFF_COLOR;
            snprintf(text, capacity, "%s",
                     _actions.solidModeEnabled() ? "Solid" : "Anim");
        } else {
            const uint8_t index = clampColorIndex(_actions.solidColorIndex());
            color = PALETTE[index];
            snprintf(text, capacity, "%s", COLOR_NAMES[index]);
        }
        break;
    case MenuPage::Fps:
        color = UI_COLOR;
        snprintf(text, capacity, "%u",
                 static_cast<unsigned>(clampFps(_actions.fps())));
        break;
    }
}

void MenuController::resetScroll(uint32_t nowMs) {
    _showingValue = false;
    _valueModeOnly = false;
    _labelScroller.reset(nowMs, 8);
}

void MenuController::beginValueDisplay(uint32_t nowMs, bool showMode) {
    const bool wasShowingValue = _showingValue;
    _valueModeOnly = showMode;

    char text[16];
    MenuRgb color = UI_COLOR;
    formatValueLabel(text, sizeof(text), color);
    const uint16_t textWidth = BitmapFont::textWidth(text);

    if (!wasShowingValue) {
        if (textWidth > 0 && textWidth <= 8) {
            _valueScroller.reset(nowMs,
                static_cast<int16_t>((8 - textWidth) / 2));
        } else {
            _valueScroller.reset(nowMs, 0);
        }
    } else if (textWidth > 0 && textWidth <= 8) {
        // Short values fit on-screen; keep the complete value readable even
        // when the previous (longer) value was partway through its marquee.
        _valueScroller.setPosition(nowMs,
            static_cast<int16_t>((8 - textWidth) / 2));
    } else if (_valueScroller.x() > 7 ||
               static_cast<int32_t>(_valueScroller.x()) + textWidth <= 0) {
        _valueScroller.setPosition(nowMs, 0);
    } else {
        _valueScroller.retime(nowMs);
    }

    _showingValue = true;
    _lastValueChangeMs = nowMs;
}

bool MenuController::expireValueDisplay(uint32_t nowMs) {
    if (!_showingValue ||
        static_cast<uint32_t>(nowMs - _lastValueChangeMs) < VALUE_DISPLAY_MS) {
        return false;
    }
    _showingValue = false;
    _valueModeOnly = false;
    _labelScroller.retime(nowMs);
    return true;
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

void MenuController::setScrollIntervalMs(uint32_t intervalMs) {
    _labelScroller.setIntervalMs(intervalMs);
    _valueScroller.setIntervalMs(intervalMs);
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

    const bool valueDisplayExpired = expireValueDisplay(nowMs);
    if (input != MenuInput::None) {
        _lastActivityMs = nowMs;
        const MenuPage pageBeforeInput = _page;
        const bool editingBeforeInput = _editing;
        const MenuUpdateResult result = handleActiveInput(nowMs, input);
        if (result == MenuUpdateResult::Changed &&
            (_page != pageBeforeInput || _editing != editingBeforeInput)) {
            resetScroll(nowMs);
        }
        if (result != MenuUpdateResult::NoChange) return result;
        return valueDisplayExpired ? MenuUpdateResult::Animated
                                   : MenuUpdateResult::NoChange;
    }

    if (valueDisplayExpired) return MenuUpdateResult::Animated;

    char text[24];
    MenuRgb color = UI_COLOR;
    BitmapTextScroller& scroller = _showingValue
                                       ? _valueScroller
                                       : _labelScroller;
    if (_showingValue) formatValueLabel(text, sizeof(text), color);
    else formatLabel(text, sizeof(text), color);

    if (scroller.advance(nowMs, BitmapFont::textWidth(text), 8)) {
        return MenuUpdateResult::Animated;
    }

    return MenuUpdateResult::NoChange;
}

void MenuController::noteActivity(uint32_t nowMs) {
    if (!_active) return;
    _lastActivityMs = nowMs;
}

MenuUpdateResult MenuController::handleActiveInput(uint32_t nowMs,
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
        return adjust(nowMs, input == MenuInput::Up ? 1 : 0);
    }

    if (input != MenuInput::Select) return MenuUpdateResult::NoChange;

    if (_editing) {
        if (_page == MenuPage::SolidColor) {
            _actions.setSolidModeEnabled(!_actions.solidModeEnabled());
            beginValueDisplay(nowMs, true);
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
        beginValueDisplay(nowMs);
        return MenuUpdateResult::Changed;
    case MenuPage::Fps:
        _actions.setFps(nextFpsPreset(clampFps(_actions.fps())));
        beginValueDisplay(nowMs);
        return MenuUpdateResult::Changed;
    }
    return MenuUpdateResult::NoChange;
}

MenuUpdateResult MenuController::adjust(uint32_t nowMs, uint8_t direction) {
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
        beginValueDisplay(nowMs);
        return MenuUpdateResult::Changed;
    }
    case MenuPage::SolidColor: {
        const int16_t next = static_cast<int16_t>(clampColorIndex(
                                 _actions.solidColorIndex())) +
                             (direction ? 1 : -1);
        const uint8_t index = wrapColorIndex(next);
        if (index == _actions.solidColorIndex()) return MenuUpdateResult::NoChange;
        _actions.setSolidColorIndex(index);
        beginValueDisplay(nowMs);
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
    if (_showingValue) {
        formatValueLabel(text, sizeof(text), color);
        drawText(display, text, _valueScroller.x(), color);
    } else {
        formatLabel(text, sizeof(text), color);
        drawText(display, text, _labelScroller.x(), color);
    }

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
