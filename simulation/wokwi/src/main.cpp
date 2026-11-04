#include <Arduino.h>
#include <SPI.h>
#include <SD.h>  // Keep PlatformIO's dependency scanner aware of the shared SD player.
#include <WiFi.h>

#include "AnimationController.h"
#include "ButtonDebouncer.h"
#include "ControlState.h"
#include "DisplayController.h"
#include "IRController.h"
#include "MenuController.h"

// This pin map belongs only to the Wokwi ESP32 target. It intentionally does
// not replace the NodeMCU pin map in the real firmware.
static const uint8_t PIN_BTN = 25;
static const uint8_t PIN_IR = 26;
static const uint8_t PIN_LED_DATA = 4;
static const uint8_t PIN_SD_CS = 27;
static const uint8_t PIN_SD_SCK = 18;
static const uint8_t PIN_SD_MISO = 19;
static const uint8_t PIN_SD_MOSI = 23;

class SimulationMenuActions : public MenuActions {
public:
    SimulationMenuActions(ControlState& controls, DisplayController& display)
        : _controls(controls), _display(display), _fps(30) {}

    void begin() {
        WiFi.mode(WIFI_OFF);
    }

    uint8_t brightness() const { return _controls.brightness(); }
    void setBrightness(uint8_t value) {
        _controls.setBrightness(value);
        _display.setBrightness(value);
    }

    uint8_t fps() const { return _fps; }
    void setFps(uint8_t value) {
        if (value < MenuController::MIN_FPS) value = MenuController::MIN_FPS;
        if (value > MenuController::MAX_FPS) value = MenuController::MAX_FPS;
        _fps = value;
    }

    bool wifiEnabled() const { return WiFi.getMode() != WIFI_OFF; }
    void setWifiEnabled(bool enabled) {
        WiFi.mode(enabled ? WIFI_STA : WIFI_OFF);
    }

    bool solidModeEnabled() const {
        return _controls.mode() == DisplayMode::Solid;
    }
    void setSolidModeEnabled(bool enabled) {
        _controls.setMode(enabled ? DisplayMode::Solid : DisplayMode::Animation);
    }
    uint8_t solidColorIndex() const { return _controls.solidColorIndex(); }
    void setSolidColorIndex(uint8_t index) {
        if (index >= MenuController::COLOR_COUNT) index = 0;
        const MenuRgb color = MenuController::colorAt(index);
        _controls.setSolidColor(index, RgbColorValue{
            color.red, color.green, color.blue
        });
    }

    bool powerOn() const { return _controls.isPowered(); }
    void setPowerOn(bool enabled) { _controls.setPowered(enabled); }

private:
    ControlState& _controls;
    DisplayController& _display;
    uint8_t _fps;
};

class MatrixMenuDisplay : public MenuDisplay {
public:
    explicit MatrixMenuDisplay(DisplayController& display) : _display(display) {}

    void setPixel(uint8_t x, uint8_t y,
                  uint8_t red, uint8_t green, uint8_t blue) {
        _display.setPixel(x, y, red, green, blue);
    }
    void show() { _display.show(); }

private:
    DisplayController& _display;
};

DisplayController display(PIN_LED_DATA);
AnimationController animation(PIN_SD_CS);
IRController ir(PIN_IR);
ControlState controls;
ButtonDebouncer button;
SimulationMenuActions menuActions(controls, display);
MenuController menu(menuActions);
MatrixMenuDisplay menuDisplay(display);

uint32_t lastFrameAt = 0;
bool animationFramePending = false;

// These are NEC command bytes emitted by Wokwi's virtual remote. They are
// intentionally separate from the user's real-remote keymap.
MenuInput menuInputForRemote(uint32_t command) {
    switch (command) {
    case 0xE2: return MenuInput::Menu;   // Menu
    case 0x4A: return MenuInput::Up;     // 8
    case 0x52: return MenuInput::Down;   // 9
    case 0xA8: return MenuInput::Select; // Play
    case 0xC2: return MenuInput::Back;   // Back
    default: return MenuInput::None;
    }
}

ControlAction actionForRemote(uint32_t command) {
    switch (command) {
    case 0xA2: return ControlAction::PowerToggle;   // Power
    case 0x02: return ControlAction::BrightnessUp; // +
    case 0x98: return ControlAction::BrightnessDown; // -
    case 0xE0: return ControlAction::PreviousPreset; // Previous
    case 0x90: return ControlAction::NextPreset;     // Next
    case 0x30: return ControlAction::SolidRed;       // 1
    case 0x10: return ControlAction::SolidGreen;     // 4
    case 0x7A: return ControlAction::SolidBlue;      // 3
    case 0x68: return ControlAction::SolidWhite;     // 0
    default: return ControlAction::None;
    }
}

void renderSolidColor() {
    const RgbColorValue color = controls.solidColor();
    display.fill(color.red, color.green, color.blue);
}

void renderDiagnosticPattern() {
    display.clear();
    for (uint8_t y = 0; y < MatrixFormat::kHeight; ++y) {
        for (uint8_t x = 0; x < MatrixFormat::kWidth; ++x) {
            if (x == y) display.setPixel(x, y, 255, 0, 0);
            else if (x + y == MatrixFormat::kWidth - 1) {
                display.setPixel(x, y, 0, 80, 255);
            }
        }
    }
    display.show();
}

void renderStoredMode() {
    if (!controls.isPowered()) return;
    if (controls.mode() == DisplayMode::Solid) {
        renderSolidColor();
    } else if (animationFramePending) {
        const bool wasRunning = animation.isRunning();
        if (animation.update()) {
            animationFramePending = false;
            animation.render(display);
        } else {
            animationFramePending = false;
            if (wasRunning) display.clear();
        }
    } else if (animation.isRunning()) {
        animation.render(display);
    } else {
        display.clear();
    }
}

void renderNextAnimationFrame() {
    const bool wasRunning = animation.isRunning();
    if (animation.update()) {
        animationFramePending = false;
        animation.render(display);
    } else if (wasRunning && !animation.isRunning()) {
        display.clear();
    }
}

void dispatch(ControlAction action) {
    if (action == ControlAction::None) return;
    controls.apply(action);
    switch (action) {
    case ControlAction::BrightnessUp:
    case ControlAction::BrightnessDown:
        display.setBrightness(controls.brightness());
        break;
    case ControlAction::PreviousPreset:
        animationFramePending = animation.previousPreset();
        break;
    case ControlAction::NextPreset:
        animationFramePending = animation.nextPreset();
        break;
    case ControlAction::PowerToggle:
    case ControlAction::SolidRed:
    case ControlAction::SolidGreen:
    case ControlAction::SolidBlue:
    case ControlAction::SolidWhite:
    case ControlAction::None:
    default:
        break;
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println();
    Serial.println(F("2D LED Matrix — Wokwi ESP32 simulation"));
    Serial.println(F("This simulates app behavior, not ESP8266 electrical timing."));

    pinMode(PIN_BTN, INPUT_PULLUP);
    SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
    display.begin();
    display.setBrightness(controls.brightness());
    renderDiagnosticPattern();
    delay(700);
    menuActions.begin();
    ir.begin();

    if (!animation.begin()) {
        Serial.println(F("No playable preset on simulated SD; keeping diagnostic pattern."));
        renderDiagnosticPattern();
    } else {
        animationFramePending = true;
        renderStoredMode();
    }
    lastFrameAt = millis();
}

void loop() {
    const uint32_t now = millis();
    const bool wasPowered = controls.isPowered();
    const bool menuWasActive = menu.isActive();
    bool stateChanged = false;

    if (button.update(digitalRead(PIN_BTN) == HIGH, now)) {
        dispatch(ControlAction::PowerToggle);
        stateChanged = true;
    }

    MenuInput menuInput = MenuInput::None;
    if (ir.update() && ir.hasCommand()) {
        const uint32_t command = ir.getDecodedCommand();
        Serial.print(F("Wokwi NEC command: 0x"));
        Serial.println(command, HEX);
        menuInput = menuInputForRemote(command);
        if (menuInput == MenuInput::None) {
            const ControlAction action = actionForRemote(command);
            if (action != ControlAction::None) {
                dispatch(action);
                stateChanged = true;
            }
        }
        ir.clearCommand();
    }

    const MenuUpdateResult menuResult = menu.update(now, menuInput);
    if (menuResult != MenuUpdateResult::NoChange) stateChanged = true;

    const bool powered = controls.isPowered();
    if (!powered) {
        if (wasPowered) display.clear();
        lastFrameAt = now;
        return;
    }

    if (menu.isActive()) {
        if (stateChanged || !menuWasActive || !wasPowered) menu.render(menuDisplay);
        return;
    }

    if (menuWasActive || !wasPowered) {
        lastFrameAt = now;
        renderStoredMode();
        return;
    }

    if (stateChanged) {
        lastFrameAt = now;
        renderStoredMode();
        return;
    }

    const uint32_t frameInterval = 1000UL / menuActions.fps();
    if (controls.mode() == DisplayMode::Animation &&
        static_cast<uint32_t>(now - lastFrameAt) >= frameInterval) {
        lastFrameAt = now;
        renderNextAnimationFrame();
    }
}
