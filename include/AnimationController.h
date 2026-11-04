#pragma once

#include <SD.h>
#include "DisplayController.h"
#include "MatrixFormat.h"

class AnimationController {
public:
    explicit AnimationController(uint8_t csPin);

    // Initializes the card, scans valid direct children of /PRESETS, and loads
    // the first filename in deterministic bytewise order.
    bool begin();
    bool update();
    void render(DisplayController& display);

    bool nextPreset();
    bool previousPreset();
    bool isRunning() const { return _running; }
    uint8_t presetCount() const { return _presetCount; }
    const char* currentPreset() const;

private:
    static const uint8_t kMaxPresets = 32;
    static const size_t kPathBytes = 64;

    bool scanPresets();
    bool loadPreset(uint8_t index);
    bool validateFile(File& file, MatrixFormat::DatFileInfo& info);
    void insertPreset(const char* path);

    uint8_t _csPin;
    File _file;
    uint8_t _frameBuffer[MatrixFormat::kFrameBytes];
    MatrixFormat::DatFileInfo _fileInfo;
    uint32_t _nextFrame;
    char _presets[kMaxPresets][kPathBytes];
    uint8_t _presetCount;
    uint8_t _presetIndex;
    bool _running;
};
