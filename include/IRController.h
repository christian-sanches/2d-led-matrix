#pragma once
#include <IRrecv.h>
#include <IRremoteESP8266.h>

// =========================================================
// IRController - Decodes signals from the IR receiver
// =========================================================
// Responsibilities:
//   - Initialize the IR receiver (TSOP38238 or similar)
//   - Decode IR signals (NEC protocol, the most common)
//   - Store the latest received command
//
// Expected usage (polling):
//   IRController ir(PIN_IR);
//   ir.begin();
//   ...
//   ir.update();
//   if (ir.hasCommand()) {
//       uint64_t cmd = ir.getCommand();
//       // handle command...
//       ir.clearCommand();
//   }
//
// Note: The IR receiver has an internal pull-up on the data pin.
// When idle, it keeps the pin HIGH. When it receives a signal,
// it pulls LOW in pulses. This is why GPIO0 is safe to use —
// it must be HIGH during ESP8266 boot, which matches the idle state.

class IRController {
public:
    // pin: GPIO pin connected to the IR receiver OUT
    IRController(uint8_t pin);

    // Initializes the IR receiver (call once in setup())
    void begin();

    // Checks if a new IR signal has been received
    // Should be called frequently (every loop() iteration)
    // Returns true if a new command was decoded
    bool update();

    // Returns the last received command code (NEC format)
    uint64_t getCommand() const { return _results.value; }

    // Returns true if there's an unprocessed command
    bool hasCommand() const { return _hasNew; }

    // Marks the current command as processed
    void clearCommand() { _hasNew = false; }

private:
    uint8_t        _pin;      // IR receiver pin
    IRrecv         _irrecv;   // IR decoder instance
    decode_results _results;  // Last decoded result
    bool           _hasNew;   // Flag: true if there's an unread command
};
