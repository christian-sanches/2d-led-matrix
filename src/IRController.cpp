#include "IRController.h"

// =========================================================
// CONSTRUCTOR
// =========================================================
// Stores the pin and creates the IR decoder instance.
//
// Note: IRrecv uses the pin to configure a hardware interrupt
// that detects the pulses of the IR signal.
//
// pin: GPIO connected to the IR receiver OUT pin (e.g. GPIO0/D3)
// The IR receiver has a pull-up: when idle, output is HIGH.
// During a signal, it pulses LOW. This is safe on ESP8266 GPIO0,
// which must be HIGH during boot.
IRController::IRController(uint8_t pin)
    : _pin(pin), _irrecv(pin), _hasNew(false) {}

// =========================================================
// begin
// =========================================================
// Activates the IR receiver to start listening for signals.
// Call once in setup().
void IRController::begin() {
    _irrecv.enableIRIn();
}

// =========================================================
// update
// =========================================================
// Checks if a new IR signal has been received and decoded.
// Call this every loop() iteration.
//
// How it works:
//   1. Attempts to decode a pending signal
//   2. If successful, sets _hasNew = true
//   3. Calls resume() to prepare for the next signal
//   4. Returns true if a new command was received
//
// The decoded command can be read with getCommand().
// The default protocol is NEC (most common for IR remotes).
bool IRController::update() {
    if (_irrecv.decode(&_results)) {
        _hasNew = true;           // Signal that a new command is available
        _irrecv.resume();         // Ready to receive the next signal
        return true;
    }
    return false;
}
