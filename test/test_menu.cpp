// Standalone host test; compile together with src/MenuController.cpp.
#include "MenuController.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

class TestActions : public MenuActions {
public:
    TestActions()
        : brightnessValue(0), fpsValue(30), wifi(false), solid(false),
          color(0) {}

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
    uint8_t brightnessValue;
    uint8_t fpsValue;
    bool wifi;
    bool solid;
    uint8_t color;
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

static bool hasTextPixels(const TestDisplay& display) {
    for (uint8_t y = 1; y <= 5; ++y) {
        for (uint8_t x = 0; x < 8; ++x) {
            const TestDisplay::Pixel& pixel = display.pixels[y * 8U + x];
            if (pixel.r || pixel.g || pixel.b) return true;
        }
    }
    return false;
}

static void openMenu(MenuController& menu, uint32_t now) {
    assert(menu.update(now, MenuInput::Menu) == MenuUpdateResult::Entered);
}

int main() {
    TestActions actions;
    TestDisplay display;
    MenuController menu(actions);
    const MenuRgb firstColor = MenuController::colorAt(0);
    const MenuRgb fallbackColor = MenuController::colorAt(255);
    assert(firstColor.red == 255 && firstColor.green == 0 && firstColor.blue == 0);
    assert(fallbackColor.red == firstColor.red &&
           fallbackColor.green == firstColor.green &&
           fallbackColor.blue == firstColor.blue);

    assert(menu.update(0, MenuInput::Up) == MenuUpdateResult::NoChange);
    assert(menu.update(1, MenuInput::Menu) == MenuUpdateResult::Entered);
    assert(menu.isActive());
    assert(menu.page() == MenuPage::Brightness);
    assert(strcmp(MenuController::pageName(menu.page()), "brightness") == 0);
    menu.render(display);
    assert(display.shows == 1 && display.writes >= 64);

    // Brightness is bounded and adjustable in 16-point steps.
    assert(menu.update(2, MenuInput::Select) == MenuUpdateResult::Changed);
    assert(menu.update(3, MenuInput::Down) == MenuUpdateResult::NoChange);
    for (uint8_t i = 0; i < 20; ++i) menu.update(4 + i, MenuInput::Up);
    assert(actions.brightnessValue == 255);
    assert(menu.update(30, MenuInput::Select) == MenuUpdateResult::Changed);

    // Root navigation wraps across four pages; power is not a menu page.
    assert(menu.update(31, MenuInput::Up) == MenuUpdateResult::Changed);
    assert(menu.page() == MenuPage::Fps);
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

    // Select cycles the supported FPS presets: 15, 24, 30, 48, 60.
    menu.update(43, MenuInput::Back);
    menu.update(44, MenuInput::Down); // FPS page, wrapping from SolidColor
    assert(menu.page() == MenuPage::Fps);
    const uint8_t expectedFps[] = {48, 60, 15, 24, 30};
    for (uint8_t i = 0; i < sizeof(expectedFps); ++i) {
        assert(menu.update(45 + i, MenuInput::Select) == MenuUpdateResult::Changed);
        assert(actions.fpsValue == expectedFps[i]);
    }

    // Back exits and inactive render does not touch the matrix buffer.
    const uint16_t oldWrites = display.writes;
    assert(menu.update(120, MenuInput::Back) == MenuUpdateResult::Exited);
    menu.render(display);
    assert(display.writes == oldWrites);

    // The text ticker advances without blocking the application loop.
    openMenu(menu, 200);
    assert(menu.update(300, MenuInput::None) == MenuUpdateResult::Animated);
    assert(menu.update(301, MenuInput::Back) == MenuUpdateResult::Exited);

    // A value change temporarily replaces the full label with just the value.
    // Repeated changes keep that feedback visible until 3 seconds after the
    // latest change, then the regular marquee resumes from its saved position.
    TestActions feedbackActions;
    feedbackActions.brightnessValue = 128;
    TestDisplay feedbackDisplay;
    MenuController feedbackMenu(feedbackActions);
    openMenu(feedbackMenu, 1000);
    feedbackMenu.render(feedbackDisplay);
    assert(!hasTextPixels(feedbackDisplay));
    feedbackMenu.update(1001, MenuInput::Select);
    assert(feedbackMenu.update(1002, MenuInput::Up) == MenuUpdateResult::Changed);
    assert(feedbackActions.brightnessValue == 144);
    feedbackMenu.render(feedbackDisplay);
    assert(feedbackDisplay.pixels[8].g == 180); // Value-only "56" starts at x=0.

    assert(feedbackMenu.update(1100, MenuInput::Up) == MenuUpdateResult::Changed);
    assert(feedbackActions.brightnessValue == 160);
    assert(feedbackMenu.update(4099, MenuInput::None) == MenuUpdateResult::Animated);
    feedbackMenu.render(feedbackDisplay);
    assert(hasTextPixels(feedbackDisplay));

    assert(feedbackMenu.update(4100, MenuInput::None) == MenuUpdateResult::Animated);
    feedbackMenu.render(feedbackDisplay);
    assert(!hasTextPixels(feedbackDisplay)); // Full label waits at the saved x=8.
    assert(feedbackMenu.update(4200, MenuInput::None) == MenuUpdateResult::Animated);
    feedbackMenu.render(feedbackDisplay);
    assert(feedbackDisplay.pixels[15].g == 180); // "BRIGHT" resumes at the left edge.

    // Timeout and timestamp wraparound are both supported.
    openMenu(menu, 100);
    menu.noteActivity(6000); // A recognized app action also keeps the menu alive.
    const MenuUpdateResult refreshed = menu.update(12000, MenuInput::None);
    assert(refreshed == MenuUpdateResult::Animated ||
           refreshed == MenuUpdateResult::NoChange);
    assert(menu.isActive());
    assert(menu.update(13000, MenuInput::None) == MenuUpdateResult::Exited);

    openMenu(menu, 0xFFFFFF00UL);
    const MenuUpdateResult wrapped = menu.update(0x00001A57UL, MenuInput::None);
    assert(wrapped == MenuUpdateResult::Animated || wrapped == MenuUpdateResult::NoChange);
    assert(menu.isActive());
    assert(menu.update(0x00001A58UL) == MenuUpdateResult::Exited);

    puts("MenuController tests passed");
    return 0;
}
