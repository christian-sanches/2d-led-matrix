#include <Arduino.h>
#include <ESP8266WiFi.h>

#include "AnimationController.h"
#include "ButtonDebouncer.h"
#include "ControlState.h"
#include "DisplayController.h"
#include "IRController.h"
#include "IRKeymap.h"
#include "MenuController.h"

// ESP8266 NodeMCU pin assignments (GPIO numbers, not Dx labels).
// SD uses the fixed ESP8266 HSPI pins: SCK=14/D5, MISO=12/D6, MOSI=13/D7.
static const uint8_t PIN_BTN = 5;       // D1, active-low momentary power button
static const uint8_t PIN_IR = 0;        // D3, IR receiver output
static const uint8_t PIN_LED_DATA = 4;  // D2, matrix DIN
static const uint8_t PIN_SD_CS = 15;    // D8, SD chip select

class FirmwareMenuActions : public MenuActions {
public:
    FirmwareMenuActions(ControlState& controls, DisplayController& display)
        : _controls(controls), _display(display), _fps(30) {}

    void begin() {
        // Menu Wi-Fi is radio-only and volatile. Never store credentials or
        // initiate an AP/server/provisioning flow.
        WiFi.persistent(false);
        WiFi.setAutoConnect(false);
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
        WiFi.persistent(false);
        WiFi.setAutoConnect(false);
        if (enabled) {
            WiFi.mode(WIFI_STA);
            WiFi.disconnect(false);
        } else {
            WiFi.mode(WIFI_OFF);
        }
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
FirmwareMenuActions menuActions(controls, display);
MenuController menu(menuActions);
MatrixMenuDisplay menuDisplay(display);

uint32_t lastFrameAt = 0;
bool animationFramePending = false;

void renderSolidColor() {
    const RgbColorValue color = controls.solidColor();
    display.fill(color.red, color.green, color.blue);
}

// Renders the last frame already read from SD, without advancing playback.
// A preset change marks its first frame pending and reads it once on resume.
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

// Physical and IR application actions continue to use M1's shared dispatch.
// The caller decides whether the menu or content is currently visible.
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

void logIrCommand() {
    Serial.print(F("IR protocol-id="));
    Serial.print(ir.getProtocolId());
    Serial.print(F(" bits="));
    Serial.print(ir.getBitCount());
    const uint64_t value = ir.getCommand();
    Serial.print(F(" value=0x"));
    const uint32_t high = static_cast<uint32_t>(value >> 32);
    if (high) {
        Serial.print(high, HEX);
        Serial.printf("%08X", static_cast<uint32_t>(value));
    } else {
        Serial.print(static_cast<uint32_t>(value), HEX);
    }
    Serial.print(F(" address=0x"));
    Serial.print(ir.getAddress(), HEX);
    Serial.print(F(" command=0x"));
    Serial.println(ir.getDecodedCommand(), HEX);
}

void setup() {
    Serial.begin(115200);
    delay(250);
    Serial.println();
    Serial.println(F("2D LED Matrix — ESP8266 NodeMCU, 8x8 RGB888"));
    Serial.println(F("IR action/menu codes default to disabled; see README.md."));

    pinMode(PIN_BTN, INPUT_PULLUP);
    display.begin();
    display.setBrightness(controls.brightness());
    menuActions.begin();
    ir.begin();

    if (!animation.begin()) {
        Serial.println(F("No playable preset: display remains available for IR solid colors."));
        display.clear();
    } else {
        renderNextAnimationFrame();
    }
    lastFrameAt = millis();
}

void loop() {
    const uint32_t now = millis();
    const bool wasPowered = controls.isPowered();
    const bool menuWasActive = menu.isActive();
    bool stateChanged = false;

    // The physical button remains a short-press logical power toggle.
    if (button.update(digitalRead(PIN_BTN) == HIGH, now)) {
        dispatch(ControlAction::PowerToggle);
        stateChanged = true;
    }

    // Keep decoding while powered off so IR can wake the matrix or operate an
    // already-open (not visible while off) menu.
    MenuInput menuInput = MenuInput::None;
    if (ir.update() && ir.hasCommand()) {
        logIrCommand();
        const uint64_t code = ir.getCommand();
        menuInput = menuInputForIrCommand(code);
        if (menuInput == MenuInput::None) {
            const ControlAction action = actionForIrCommand(code);
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
        // No SD frame updates run while the overlay is open. Drawing occurs
        // only for changes; idle IR polling never rewrites the matrix.
        if (stateChanged || !menuWasActive || !wasPowered) menu.render(menuDisplay);
        return;
    }

    if (menuWasActive || !wasPowered) {
        // Restore the retained frame/mode, then restart the frame clock so a
        // long menu or powered-off interval cannot cause catch-up updates.
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
