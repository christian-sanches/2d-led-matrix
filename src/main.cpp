#include <Arduino.h>

#include "AnimationController.h"
#include "ButtonDebouncer.h"
#include "ControlState.h"
#include "DisplayController.h"
#include "IRController.h"
#include "IRKeymap.h"

// ESP8266 NodeMCU pin assignments (GPIO numbers, not Dx labels).
// SD uses the fixed ESP8266 HSPI pins: SCK=14/D5, MISO=12/D6, MOSI=13/D7.
static const uint8_t PIN_BTN = 5;       // D1, active-low momentary power button
static const uint8_t PIN_IR = 0;        // D3, IR receiver output
static const uint8_t PIN_LED_DATA = 4;  // D2, matrix DIN
static const uint8_t PIN_SD_CS = 15;    // D8, SD chip select
static const uint32_t FRAME_INTERVAL_MS = 1000 / 30;

DisplayController display(PIN_LED_DATA);
AnimationController animation(PIN_SD_CS);
IRController ir(PIN_IR);
ControlState controls;
ButtonDebouncer button;
uint32_t lastFrameAt = 0;

void renderSolidColor() {
    const RgbColorValue color = controls.solidColor();
    display.fill(color.red, color.green, color.blue);
}

void renderCurrentAnimationFrame() {
    if (animation.update()) animation.render(display);
    else display.clear();
}

void renderCurrentMode() {
    if (!controls.isPowered()) return;
    if (controls.mode() == DisplayMode::Solid) renderSolidColor();
    else renderCurrentAnimationFrame();
    lastFrameAt = millis();
}

// Both the physical button and configured IR commands enter through this
// function so power/brightness/mode behavior is consistent.
void dispatch(ControlAction action) {
    if (action == ControlAction::None) return;

    controls.apply(action);
    switch (action) {
        case ControlAction::PowerToggle:
            if (controls.isPowered()) renderCurrentMode();
            else display.clear();
            break;

        case ControlAction::BrightnessUp:
        case ControlAction::BrightnessDown:
            display.setBrightness(controls.brightness());
            if (controls.isPowered() && controls.mode() == DisplayMode::Solid) renderSolidColor();
            break;

        case ControlAction::PreviousPreset:
            animation.previousPreset();
            if (controls.isPowered()) renderCurrentMode();
            break;

        case ControlAction::NextPreset:
            animation.nextPreset();
            if (controls.isPowered()) renderCurrentMode();
            break;

        case ControlAction::SolidRed:
        case ControlAction::SolidGreen:
        case ControlAction::SolidBlue:
        case ControlAction::SolidWhite:
            if (controls.isPowered()) renderSolidColor();
            break;

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
    Serial.println(F("IR key values default to disabled; see README.md and include/IRKeymap.h."));

    pinMode(PIN_BTN, INPUT_PULLUP);
    display.begin();
    display.setBrightness(controls.brightness());
    ir.begin();

    if (!animation.begin()) {
        Serial.println(F("No playable preset: display remains available for IR solid colors."));
        display.clear();
    } else {
        renderCurrentAnimationFrame();
    }
}

void loop() {
    const uint32_t now = millis();

    if (button.update(digitalRead(PIN_BTN) == HIGH, now)) {
        dispatch(ControlAction::PowerToggle);
    }

    // Keep decoding even when powered off so the IR power command can wake it.
    if (ir.update() && ir.hasCommand()) {
        logIrCommand();
        dispatch(actionForIrCommand(ir.getCommand()));
        ir.clearCommand();
    }

    if (controls.isPowered() && controls.mode() == DisplayMode::Animation &&
        static_cast<uint32_t>(now - lastFrameAt) >= FRAME_INTERVAL_MS) {
        lastFrameAt = now;
        const bool wasRunning = animation.isRunning();
        if (animation.update()) animation.render(display);
        else if (wasRunning && !animation.isRunning()) display.clear();
    }
}
