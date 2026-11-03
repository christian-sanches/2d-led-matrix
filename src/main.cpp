#include <Arduino.h>
#include "DisplayController.h"
#include "AnimationController.h"
#include "IRController.h"

// =========================================================
// PINOUT DEFINITIONS
// =========================================================
// ESP8266 NodeMCU pin assignments (no conflicts with SD SPI):
//
//   HSPI (fixed by hardware, for SD card):
//     MOSI = GPIO13 (D7),  MISO = GPIO12 (D6),  SCK = GPIO14 (D5)
//
//   Free GPIOs for our peripherals:
#define PIN_BTN       5   // D1 - Power button (INPUT_PULLUP)
#define PIN_IR        0   // D3 - IR receiver data out
#define PIN_LED_DATA  4   // D2 - NeoPixel matrix DIN
#define PIN_SD_CS    15   // D8 - SD card chip select

// =========================================================
// MATRIX CONFIGURATION
// =========================================================
const uint16_t LedSide     = 8;                        // 8x8 matrix
const uint16_t LedQuantity = LedSide * LedSide;        // 64 LEDs
const uint8_t  bufferSize  = LedQuantity * 3;          // 192 bytes per frame
const int      displayFps  = 30;                       // Target framerate

// =========================================================
// COMPONENT INSTANCES
// =========================================================
// Each controller is self-contained and receives only its
// GPIO pin. This keeps main.cpp clean and makes the
// controllers reusable in other projects.
DisplayController   display(LedQuantity, PIN_LED_DATA); // LED matrix
AnimationController animation(PIN_SD_CS);              // SD card animation player
IRController        ir(PIN_IR);                        // IR remote receiver

// =========================================================
// SYSTEM STATE
// =========================================================
bool systemOn = true;              // Master power state (toggled by button)

unsigned long timeLastFrame    = 0; // millis() of the last rendered frame
unsigned long timeLastBtnClick = 0; // millis() of the last button press
const uint8_t debounceTime     = 200; // Button debounce window (ms)
bool lastButtonState           = HIGH; // Previous button reading

// =========================================================
// setup()
// =========================================================
// Runs once at power-up. Initializes all peripherals.
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n === 2D LED Matrix ===");

    // Button: internal pull-up means "LOW = pressed"
    pinMode(PIN_BTN, INPUT_PULLUP);

    // Initialize the NeoPixel matrix
    display.begin();

    // Test pattern: light the first 3 LEDs R, G, B to verify wiring
    // If you see these colors, the matrix is wired correctly.
    display.setPixel(0, 0, 255, 0, 0);  // Red at top-left
    display.setPixel(1, 0, 0, 255, 0);  // Green
    display.setPixel(2, 0, 0, 0, 255);  // Blue
    display.show();
    delay(2000);
    display.clear();

    // Start listening for IR remote commands
    ir.begin();

    // Load the first animation from the SD card
    if (!animation.begin("/PRESETS/PRESET00.dat")) {
        Serial.println("No animation file found — matrix will stay off.");
    }
}

// =========================================================
// loop()
// =========================================================
// Runs continuously after setup(). Each iteration:
//   1. Checks the power button (with debounce)
//   2. If system is off, skip everything
//   3. Polls the IR receiver for new commands
//   4. Renders the next animation frame at 30 FPS
void loop() {
    unsigned long currentTime = millis();
    bool currentButtonState = digitalRead(PIN_BTN);

    // -------- POWER BUTTON (with debounce) --------
    // Detects a HIGH -> LOW transition (button press) and
    // ignores bounces within the debounceTime window.
    if (currentButtonState == LOW &&
        lastButtonState == HIGH &&
        (currentTime - timeLastBtnClick) > debounceTime) {
        systemOn = !systemOn;
        timeLastBtnClick = currentTime;

        if (!systemOn) {
            display.clear();  // Turn off all LEDs when powering off
        }
    }
    lastButtonState = currentButtonState;

    // -------- POWER OFF --------
    // When the system is off, skip the rest of the loop
    // to save CPU and avoid drawing to the matrix.
    if (!systemOn) {
        return;
    }

    // -------- IR REMOTE --------
    // Check if a new IR command was received.
    // For now we just log it — add your command handling here.
    ir.update();
    if (ir.hasCommand()) {
        Serial.print("IR command received: 0x");
        Serial.println(ir.getCommand(), HEX);
        ir.clearCommand();
    }

    // -------- ANIMATION FRAME --------
    // Render the next frame only when enough time has passed
    // to maintain the target framerate (displayFps).
    if (currentTime - timeLastFrame >= (1000 / displayFps)) {
        timeLastFrame = currentTime;

        // Read the next frame from the SD card and draw it
        if (animation.update()) {
            animation.render(display);
        }
    }
}
