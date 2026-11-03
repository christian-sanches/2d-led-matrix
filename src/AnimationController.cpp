#include "AnimationController.h"

// =========================================================
// CONSTRUCTOR
// =========================================================
// Stores the SD card CS pin. Actual initialization happens
// in begin().
AnimationController::AnimationController(uint8_t csPin) : _csPin(csPin) {}

// =========================================================
// begin
// =========================================================
// Initializes the SD card over SPI and opens the animation
// file (.dat) for reading.
//
// ESP8266 uses the HSPI peripheral with fixed pins:
//   MOSI = GPIO13 (D7)
//   MISO = GPIO12 (D6)
//   SCK  = GPIO14 (D5)
// The CS pin can be any free GPIO (here GPIO15 / D8).
//
// filename: full path on SD (e.g. "/PRESETS/PRESET00.dat")
// Returns true if the file opened successfully.
bool AnimationController::begin(const char* filename) {
    Serial.print(F("Initializing SD card on CS="));
    Serial.println(_csPin);

    // Start SPI communication with the SD card
    if (!SD.begin(_csPin)) {
        Serial.println(F("ERROR: SD card not found or failed to initialize."));
        return false;
    }
    Serial.println(F("SD card initialized successfully."));

    // Open the .dat animation file in read mode
    _file = SD.open(filename, FILE_READ);
    if (!_file) {
        Serial.print(F("ERROR: Could not open file: "));
        Serial.println(filename);
        return false;
    }

    // Print file info for debugging
    Serial.print(F("Animation file opened: "));
    Serial.print(_file.size());
    Serial.println(F(" bytes"));
    Serial.print(F("Total frames: "));
    Serial.println(_file.size() / FRAME_SIZE);

    return true;
}

// =========================================================
// update
// =========================================================
// Reads the next frame (192 bytes) from the SD file into
// the internal _frameBuffer.
//
// If the file runs out of data (end of animation), it
// automatically restarts (seek to 0).
//
// Returns true if a full frame was read successfully.
// Returns false if the animation ended or no file is open.
bool AnimationController::update() {
    if (!isRunning()) return false;

    // Check if enough bytes remain for one full frame
    if (_file.available() < (int)FRAME_SIZE) {
        // End of animation: loop back to the start
        restart();
        return false;
    }

    // Read 192 bytes (R, G, B for each of the 64 LEDs)
    int bytesRead = _file.read(_frameBuffer, FRAME_SIZE);
    return bytesRead == (int)FRAME_SIZE;
}

// =========================================================
// render
// =========================================================
// Draws the current frame (from _frameBuffer) onto the LED
// matrix through the DisplayController.
//
// Iterates over all pixels in (x, y) coordinates, extracts
// R, G, B from the sequential buffer, then calls show()
// to display everything at once.
void AnimationController::render(DisplayController& display) {
    size_t ptr = 0;                          // Pointer into _frameBuffer
    uint8_t side = display.sideSize();       // Matrix side (e.g. 8)

    for (int y = 0; y < side; y++) {
        for (int x = 0; x < side; x++) {
            // Buffer is sequential: R, G, B, R, G, B, ...
            uint8_t r = _frameBuffer[ptr];
            uint8_t g = _frameBuffer[ptr + 1];
            uint8_t b = _frameBuffer[ptr + 2];
            ptr += 3;

            display.setPixel(x, y, r, g, b);
        }
    }

    // Send everything to the physical matrix at once
    display.show();
}

// =========================================================
// restart
// =========================================================
// Seeks the file read pointer back to the beginning,
// making the animation start over from frame 1.
void AnimationController::restart() {
    if (_file) {
        _file.seek(0);
        Serial.println(F("Restarting animation from beginning..."));
    }
}
