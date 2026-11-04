#include "MenuController.h"

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

void pixel(MenuDisplay& display, uint8_t x, uint8_t y, const MenuRgb& color) {
    display.setPixel(x, y, color.red, color.green, color.blue);
}

void drawRow(MenuDisplay& display, uint8_t y, uint8_t bits,
             const MenuRgb& color) {
    for (uint8_t x = 0; x < 8; ++x) {
        if (bits & (uint8_t)(0x80U >> x)) pixel(display, x, y, color);
    }
}

void drawBrightness(MenuDisplay& display, uint8_t brightness) {
    // A small sun/bulb glyph above an 8-step brightness bar.
    const uint8_t glyph[] = {0x18, 0x5A, 0x3C, 0x3C, 0x18, 0x3C};
    for (uint8_t row = 0; row < sizeof(glyph); ++row) {
        drawRow(display, row, glyph[row], UI_COLOR);
    }

    uint8_t lit = (uint16_t)brightness * 8U / 255U;
    if (brightness == 255) lit = 8;
    for (uint8_t x = 0; x < lit; ++x) pixel(display, x, 7, ON_COLOR);
}

void drawWifi(MenuDisplay& display, bool enabled) {
    // Three stepped radio arcs and a center point.
    const uint8_t glyph[] = {0x18, 0x24, 0x42, 0x81, 0x18};
    for (uint8_t row = 0; row < sizeof(glyph); ++row) {
        drawRow(display, row + 1, glyph[row], UI_COLOR);
    }
    if (enabled) {
        drawRow(display, 7, 0xFF, ON_COLOR);
    } else {
        pixel(display, 3, 7, OFF_COLOR);
        pixel(display, 4, 7, OFF_COLOR);
    }
}

uint8_t colorContrast(const MenuRgb& color) {
    const uint16_t luminance = (uint16_t)color.red * 30U +
                               (uint16_t)color.green * 59U +
                               (uint16_t)color.blue * 11U;
    return luminance > 12750U ? 0 : 1;
}

void drawSolidColor(MenuDisplay& display, const MenuRgb& color,
                    uint8_t colorIndex, bool solidMode, bool editing) {
    // Seven rows are a full-matrix color preview. The top row carries a
    // three-bit palette index plus mode/edit markers in a contrasting color.
    for (uint8_t y = 1; y < 8; ++y) {
        for (uint8_t x = 0; x < 8; ++x) pixel(display, x, y, color);
    }

    const MenuRgb marker = colorContrast(color) ? MARKER_COLOR : BLACK_COLOR;
    for (uint8_t bit = 0; bit < 3; ++bit) {
        if (colorIndex & (uint8_t)(1U << (2U - bit))) {
            pixel(display, bit, 0, marker);
        }
    }
    if (editing) pixel(display, 4, 0, MARKER_COLOR);
    if (solidMode) pixel(display, 7, 0, ON_COLOR);
    else pixel(display, 7, 0, OFF_COLOR);
}

void drawFps(MenuDisplay& display, uint8_t fps) {
    // A compact dial over a 5x8 histogram (one segment per 1/40 of range).
    drawRow(display, 0, 0x18, UI_COLOR);
    drawRow(display, 1, 0x3C, UI_COLOR);
    drawRow(display, 2, 0x18, UI_COLOR);

    const uint8_t value = fps < MenuController::MIN_FPS
                              ? MenuController::MIN_FPS
                              : (fps > MenuController::MAX_FPS
                                     ? MenuController::MAX_FPS
                                     : fps);
    const uint8_t segments = (uint16_t)(value - 1U) * 40U / 59U;
    for (uint8_t segment = 0; segment < segments; ++segment) {
        const uint8_t x = segment / 5U;
        const uint8_t y = (uint8_t)(7U - (segment % 5U));
        pixel(display, x, y, ON_COLOR);
    }
}

void drawPower(MenuDisplay& display, bool enabled) {
    // Power glyph: open ring and central stem.
    const uint8_t ring[] = {0x18, 0x24, 0x42, 0x42, 0x24, 0x18};
    for (uint8_t row = 0; row < sizeof(ring); ++row) {
        drawRow(display, row + 1, ring[row], UI_COLOR);
    }
    drawRow(display, 0, 0x18, UI_COLOR);
    drawRow(display, 1, 0x18, UI_COLOR);
    drawRow(display, 2, 0x18, UI_COLOR);
    drawRow(display, 7, enabled ? 0xFF : 0x18,
            enabled ? ON_COLOR : OFF_COLOR);
}

MenuPage nextPage(MenuPage page, int8_t direction) {
    int8_t index = (int8_t)page + direction;
    if (index < 0) index = 4;
    if (index > 4) index = 0;
    return (MenuPage)index;
}

} // namespace

MenuController::MenuController(MenuActions& actions, uint32_t timeoutMs)
    : _actions(actions),
      _timeoutMs(normalizedTimeout(timeoutMs)),
      _lastActivityMs(0),
      _page(MenuPage::Brightness),
      _active(false),
      _editing(false) {}

uint8_t MenuController::clampFps(uint8_t value) {
    if (value < MIN_FPS) return MIN_FPS;
    if (value > MAX_FPS) return MAX_FPS;
    return value;
}

uint8_t MenuController::clampColorIndex(uint8_t value) {
    return value < COLOR_COUNT ? value : 0;
}

uint8_t MenuController::wrapColorIndex(int16_t value) {
    if (value < 0) return COLOR_COUNT - 1;
    if (value >= COLOR_COUNT) return 0;
    return (uint8_t)value;
}

MenuRgb MenuController::colorAt(uint8_t index) {
    return PALETTE[clampColorIndex(index)];
}

uint32_t MenuController::normalizedTimeout(uint32_t value) const {
    if (value < MIN_TIMEOUT_MS) return MIN_TIMEOUT_MS;
    if (value > MAX_TIMEOUT_MS) return MAX_TIMEOUT_MS;
    return value;
}

void MenuController::enter(uint32_t nowMs) {
    _active = true;
    _editing = false;
    _page = MenuPage::Brightness;
    _lastActivityMs = nowMs;
}

void MenuController::exit() {
    _active = false;
    _editing = false;
}

MenuUpdateResult MenuController::update(uint32_t nowMs, MenuInput input) {
    if (_active && (uint32_t)(nowMs - _lastActivityMs) >= _timeoutMs) {
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

    if (input == MenuInput::None) return MenuUpdateResult::NoChange;
    _lastActivityMs = nowMs;
    return handleActiveInput(nowMs, input);
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
    case MenuPage::Fps:
        _editing = true;
        return MenuUpdateResult::Changed;
    case MenuPage::Wifi:
        _actions.setWifiEnabled(!_actions.wifiEnabled());
        return MenuUpdateResult::Changed;
    case MenuPage::Power:
        _actions.setPowerOn(!_actions.powerOn());
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
            next = value > (uint8_t)(255U - BRIGHTNESS_STEP)
                       ? 255
                       : (uint8_t)(value + BRIGHTNESS_STEP);
        } else {
            next = value < BRIGHTNESS_STEP
                       ? 0
                       : (uint8_t)(value - BRIGHTNESS_STEP);
        }
        if (next == value) return MenuUpdateResult::NoChange;
        _actions.setBrightness(next);
        return MenuUpdateResult::Changed;
    }
    case MenuPage::SolidColor: {
        const int16_t next = (int16_t)clampColorIndex(
                                 _actions.solidColorIndex()) +
                             (direction ? 1 : -1);
        const uint8_t index = wrapColorIndex(next);
        if (index == _actions.solidColorIndex()) return MenuUpdateResult::NoChange;
        _actions.setSolidColorIndex(index);
        return MenuUpdateResult::Changed;
    }
    case MenuPage::Fps: {
        const uint8_t value = clampFps(_actions.fps());
        const uint8_t next = direction
                                 ? (value == MAX_FPS ? MAX_FPS : value + 1U)
                                 : (value == MIN_FPS ? MIN_FPS : value - 1U);
        if (next == _actions.fps()) return MenuUpdateResult::NoChange;
        _actions.setFps(next);
        return MenuUpdateResult::Changed;
    }
    case MenuPage::Wifi:
    case MenuPage::Power:
        break;
    }
    return MenuUpdateResult::NoChange;
}

void MenuController::render(MenuDisplay& display) const {
    if (!_active) return;

    for (uint8_t y = 0; y < 8; ++y) {
        for (uint8_t x = 0; x < 8; ++x) pixel(display, x, y, BLACK_COLOR);
    }

    switch (_page) {
    case MenuPage::Brightness:
        drawBrightness(display, _actions.brightness());
        break;
    case MenuPage::Wifi:
        drawWifi(display, _actions.wifiEnabled());
        break;
    case MenuPage::SolidColor: {
        const uint8_t index = clampColorIndex(_actions.solidColorIndex());
        drawSolidColor(display, colorAt(index), index,
                       _actions.solidModeEnabled(), _editing);
        break;
    }
    case MenuPage::Fps:
        drawFps(display, clampFps(_actions.fps()));
        break;
    case MenuPage::Power:
        drawPower(display, _actions.powerOn());
        break;
    }

    if (_editing && _page != MenuPage::SolidColor) {
        pixel(display, 7, 0, MARKER_COLOR);
    }
    display.show();
}
