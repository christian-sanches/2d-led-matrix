#pragma once
#include <SD.h>
#include "DisplayController.h"

// =========================================================
// AnimationController - Reads & plays animations from SD
// =========================================================
// Responsibilities:
//   - Initialize the SD card
//   - Open a .dat animation file
//   - Read frames sequentially (192 bytes each)
//   - Render each frame onto the DisplayController
//   - Auto-loop the animation when it reaches the end
//
// .dat file format:
//   Each frame = 192 bytes (64 LEDs * 3 RGB channels)
//   Byte order: R, G, B sequentially for each LED
//   Multiple frames in sequence form the animation

class AnimationController {
public:
    // csPin: GPIO pin connected to the SD card module CS
    AnimationController(uint8_t csPin);

    // Initializes the SD card and opens the animation file
    // filename: path on the SD card (e.g. "/PRESETS/PRESET00.dat")
    // Returns true if the file was opened successfully
    bool begin(const char* filename);

    // Advances to the next animation frame
    // Reads 192 bytes from the file into the internal buffer
    // Returns true if a full frame was read
    // Returns false if the animation ended (auto-restarts)
    bool update();

    // Draws the current frame onto the DisplayController
    // Iterates over all X,Y pixels, sets colors, and calls show()
    void render(DisplayController& display);

    // Seeks back to the beginning of the file (restarts animation)
    void restart();

    // Returns true if the file is open and ready for reading
    bool isRunning() { return _file; }

private:
    static const size_t FRAME_SIZE = 192; // 64 pixels * 3 bytes (RGB)
    uint8_t  _csPin;                      // SD card chip-select pin
    File     _file;                       // Handle to the opened .dat file
    uint8_t  _frameBuffer[FRAME_SIZE];    // Buffer for one frame (192 bytes)
};
