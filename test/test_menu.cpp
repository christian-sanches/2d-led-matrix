// Standalone host test; compile together with src/MenuController.cpp.
#include "MenuController.h"

#include <assert.h>
#include <stdio.h>

class TestActions : public MenuActions {
public:
    TestActions()
        : brightnessValue(0), fpsValue(1), wifi(false), solid(false),
          color(0), power(true) {}

    uint8_t brightness() const { return brightnessValue; }
    void setBrightness(uint8_t value) { brightnessValue = value; }
    uint8_t fps() const { return fpsValue; }
    void setFps(uint8_t value) { fpsValue = value; }
    bool wifiEnabled() const { return wifi; }
    void setWifiEnabled(bool enabled) { wifi = enabled; }
    bool solidModeEnabled() const { return solid; }
    void setSolidModeEnabled(bool enabled) { solid = enabled; }
    uint8_t solidColorIndex() const { return color; }
    void setSolidColorIndex(uint8_t index) { color = index; }
    bool powerOn() const { return power; }
    void setPowerOn(bool enabled) { power = enabled; }

    uint8_t brightnessValue;
    uint8_t fpsValue;
    bool wifi;
    bool solid;
    uint8_t color;
    bool power;
};

class TestDisplay : public MenuDisplay {
public:
    struct Pixel { uint8_t r, g, b; } pixels[64];
    uint16_t writes;
    uint16_t shows;

    TestDisplay() : writes(0), shows(0) {}
    void setPixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
        pixels[y * 8U + x] = Pixel{r, g, b};
        ++writes;
    }
    void show() { ++shows; }
};

static void openMenu(MenuController& menu, uint32_t now) {
    assert(menu.update(now, MenuInput::Menu) == MenuUpdateResult::Entered);
}

int main() {
    TestActions actions;
    TestDisplay display;
    MenuController menu(actions);

    assert(menu.update(0, MenuInput::Up) == MenuUpdateResult::NoChange);
    assert(menu.update(1, MenuInput::Menu) == MenuUpdateResult::Entered);
    assert(menu.isActive());
    menu.render(display);
    assert(display.shows == 1 && display.writes >= 64);

    // Brightness is bounded and adjustable in 16-point steps.
    assert(menu.update(2, MenuInput::Select) == MenuUpdateResult::Changed);
    assert(menu.update(3, MenuInput::Down) == MenuUpdateResult::NoChange);
    for (uint8_t i = 0; i < 20; ++i) menu.update(4 + i, MenuInput::Up);
    assert(actions.brightnessValue == 255);
    assert(menu.update(30, MenuInput::Select) == MenuUpdateResult::Changed);

    // Root navigation wraps, and Wi-Fi is toggled only through the adapter.
    assert(menu.update(31, MenuInput::Up) == MenuUpdateResult::Changed);
    assert(menu.page() == MenuPage::Power);
    assert(menu.update(32, MenuInput::Down) == MenuUpdateResult::Changed);
    assert(menu.page() == MenuPage::Brightness);
    menu.update(33, MenuInput::Down);
    assert(menu.page() == MenuPage::Wifi);
    assert(menu.update(34, MenuInput::Select) == MenuUpdateResult::Changed);
    assert(actions.wifi);

    // Color selection wraps, while Select in detail toggles solid mode.
    menu.update(35, MenuInput::Down);
    assert(menu.page() == MenuPage::SolidColor);
    menu.update(36, MenuInput::Select);
    menu.update(37, MenuInput::Up);
    assert(actions.color == 1);
    menu.update(38, MenuInput::Select);
    assert(actions.solid);
    menu.update(39, MenuInput::Back);
    assert(!menu.isEditing());
    menu.update(40, MenuInput::Select);
    menu.update(41, MenuInput::Down);
    assert(actions.color == 0);
    actions.color = 7;
    menu.update(42, MenuInput::Up);
    assert(actions.color == 0); // Palette wraps at the upper edge.

    // FPS is clamped to 1..60.
    menu.update(43, MenuInput::Back);
    menu.update(44, MenuInput::Down); // FPS page, wrapping from SolidColor
    assert(menu.page() == MenuPage::Fps);
    menu.update(45, MenuInput::Select);
    menu.update(46, MenuInput::Down);
    assert(actions.fpsValue == 1);
    for (uint8_t i = 0; i < 70; ++i) menu.update(47 + i, MenuInput::Up);
    assert(actions.fpsValue == 60);

    // Power toggles through the adapter; Back exits and inactive render does
    // not touch the last rendered matrix buffer.
    menu.update(120, MenuInput::Back);
    menu.update(121, MenuInput::Down); // Power
    menu.update(122, MenuInput::Select);
    assert(!actions.power);
    const uint16_t oldWrites = display.writes;
    assert(menu.update(123, MenuInput::Back) == MenuUpdateResult::Exited);
    menu.render(display);
    assert(display.writes == oldWrites);

    // Timeout and timestamp wraparound are both supported.
    openMenu(menu, 0xFFFFFF00UL);
    assert(menu.update(0x00001A57UL) == MenuUpdateResult::NoChange);
    assert(menu.update(0x00001A58UL) == MenuUpdateResult::Exited);

    puts("MenuController tests passed");
    return 0;
}
