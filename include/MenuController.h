#pragma once

#include <BitmapText.h>

#include <stddef.h>
#include <stdint.h>

// The menu deliberately knows neither IR keycodes nor the application state
// implementation. The main loop maps configured remote keys to MenuInput and
// implements MenuActions using the application's shared state/actions.
class MenuActions {
public:
    virtual ~MenuActions() {}

    virtual uint8_t brightness() const = 0;
    virtual void setBrightness(uint8_t value) = 0;

    virtual uint8_t fps() const = 0;
    virtual void setFps(uint8_t value) = 0;

    // The adapter controls the radio only; server/AP/provisioning behavior is
    // outside the menu.
    virtual bool wifiEnabled() const = 0;
    virtual void setWifiEnabled(bool enabled) = 0;

    virtual bool solidModeEnabled() const = 0;
    virtual void setSolidModeEnabled(bool enabled) = 0;
    // Palette indices: red, green, blue, yellow, cyan, magenta, white, amber.
    virtual uint8_t solidColorIndex() const = 0;
    virtual void setSolidColorIndex(uint8_t index) = 0;
};

// Thin drawing adapter. A DisplayController wrapper can implement this
// without making this module depend on M1's final display API.
class MenuDisplay {
public:
    virtual ~MenuDisplay() {}
    virtual void setPixel(uint8_t x, uint8_t y,
                          uint8_t red, uint8_t green, uint8_t blue) = 0;
    virtual void show() = 0;
};

struct MenuRgb {
    uint8_t red;
    uint8_t green;
    uint8_t blue;
};

enum class MenuInput : uint8_t {
    None,
    Up,
    Down,
    Select,
    Back,
    Menu
};

enum class MenuPage : uint8_t {
    Brightness,
    Wifi,
    SolidColor,
    Fps
};

enum class MenuUpdateResult : uint8_t {
    NoChange,
    Entered,
    Changed,
    Exited,
    Animated
};

class MenuController {
public:
    // Volatile setting ranges: brightness 0..255 by 16, five FPS presets, and
    // an eight-color palette. The default inactivity exit is 7 seconds;
    // caller-supplied timeouts are bounded to 5..10 seconds.
    static const uint8_t MIN_FPS = 15;
    static const uint8_t MAX_FPS = 60;
    static const uint8_t FPS_PRESET_COUNT = 5;
    static const uint8_t COLOR_COUNT = 8;
    static const uint32_t DEFAULT_TIMEOUT_MS = 7000;
    static const uint32_t MIN_TIMEOUT_MS = 5000;
    static const uint32_t MAX_TIMEOUT_MS = 10000;
    static const uint8_t BRIGHTNESS_STEP = 16;
    static const uint32_t SCROLL_STEP_MS =
        BitmapTextScroller::DEFAULT_INTERVAL_MS;
    static const uint32_t VALUE_DISPLAY_MS = 3000;

    explicit MenuController(MenuActions& actions,
                            uint32_t timeoutMs = DEFAULT_TIMEOUT_MS);

    // MenuInput::Menu opens/closes. At the root, Up/Down selects a page;
    // Select edits brightness/color, toggles Wi-Fi, or cycles FPS presets.
    // In color editing, Select toggles solid mode. Back leaves editing or
    // exits at the root. Labels scroll right-to-left in reading order.
    // Settings are volatile.
    MenuUpdateResult update(uint32_t nowMs, MenuInput input = MenuInput::None);

    // One pixel is scrolled per interval. Defaults to 100 ms per pixel.
    void setScrollIntervalMs(uint32_t intervalMs);

    // Reset inactivity timeout for another recognized control while visible
    // (e.g. power/brightness), without restarting the current text scroll.
    void noteActivity(uint32_t nowMs);

    // Draws only while active. It never clears or changes the display while
    // inactive, allowing the caller to resume normal content rendering.
    void render(MenuDisplay& display) const;

    bool isActive() const { return _active; }
    bool isEditing() const { return _editing; }
    MenuPage page() const { return _page; }
    static const char* pageName(MenuPage page);

    // Fixed palette used for color labels and by the application adapter.
    static MenuRgb colorAt(uint8_t index);

private:
    static uint8_t clampFps(uint8_t value);
    static uint8_t clampColorIndex(uint8_t value);
    static uint8_t wrapColorIndex(int16_t value);
    static uint8_t nextFpsPreset(uint8_t value);
    uint32_t normalizedTimeout(uint32_t value) const;
    void enter(uint32_t nowMs);
    void exit();
    void resetScroll(uint32_t nowMs);
    void formatLabel(char* text, size_t capacity, MenuRgb& color) const;
    void formatValueLabel(char* text, size_t capacity, MenuRgb& color) const;
    void beginValueDisplay(uint32_t nowMs, bool showMode = false);
    bool expireValueDisplay(uint32_t nowMs);
    MenuUpdateResult handleActiveInput(uint32_t nowMs, MenuInput input);
    MenuUpdateResult adjust(uint32_t nowMs, uint8_t direction);

    MenuActions& _actions;
    uint32_t _timeoutMs;
    uint32_t _lastActivityMs;
    MenuPage _page;
    bool _active;
    bool _editing;
    BitmapTextScroller _labelScroller;
    BitmapTextScroller _valueScroller;
    uint32_t _lastValueChangeMs;
    bool _showingValue;
    bool _valueModeOnly;
};
