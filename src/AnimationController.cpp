#include "AnimationController.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

AnimationController::AnimationController(uint8_t csPin)
    : _csPin(csPin), _nextFrame(0), _presetCount(0), _presetIndex(0), _running(false) {}

bool AnimationController::begin() {
    Serial.print(F("Initializing SD card on CS GPIO "));
    Serial.println(_csPin);
    if (!SD.begin(_csPin)) {
        Serial.println(F("ERROR: SD card initialization failed."));
        return false;
    }
    if (!scanPresets() || _presetCount == 0) {
        Serial.println(F("ERROR: No valid .dat presets found in /PRESETS."));
        return false;
    }
    return loadPreset(0);
}

bool AnimationController::scanPresets() {
    _presetCount = 0;
    File directory = SD.open("/PRESETS");
    if (!directory || !directory.isDirectory()) {
        Serial.println(F("ERROR: /PRESETS directory is missing."));
        return false;
    }

    bool truncated = false;
    File entry = directory.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            String name = entry.name();
            const int slash = name.lastIndexOf('/');
            if (slash >= 0) name = name.substring(slash + 1);
            const int dot = name.lastIndexOf('.');
            bool isDat = dot >= 0 && name.length() - dot == 4;
            if (isDat) {
                const char* extension = name.c_str() + dot;
                isDat = tolower(static_cast<unsigned char>(extension[1])) == 'd' &&
                        tolower(static_cast<unsigned char>(extension[2])) == 'a' &&
                        tolower(static_cast<unsigned char>(extension[3])) == 't';
            }

            char path[kPathBytes];
            MatrixFormat::DatFileInfo info;
            const int pathLength = isDat ? snprintf(path, sizeof(path), "/PRESETS/%s", name.c_str()) : -1;
            if (isDat && pathLength > 0 && static_cast<size_t>(pathLength) < sizeof(path)) {
                if (validateFile(entry, info)) {
                    if (_presetCount < kMaxPresets) {
                        insertPreset(path);
                    } else {
                        truncated = true;
                        if (strcmp(path, _presets[kMaxPresets - 1]) < 0) insertPreset(path);
                    }
                } else {
                    Serial.print(F("Skipping invalid preset: "));
                    Serial.println(path);
                }
            } else if (isDat) {
                Serial.print(F("Skipping preset with an unsupported filename length: "));
                Serial.println(name);
            }
        }
        entry.close();
        entry = directory.openNextFile();
    }
    directory.close();

    if (truncated) {
        Serial.println(F("WARNING: preset list is limited to the first 32 valid files."));
    }
    Serial.print(F("Valid presets: "));
    Serial.println(_presetCount);
    return true;
}

bool AnimationController::validateFile(File& file, MatrixFormat::DatFileInfo& info) {
    const size_t fileLength = file.size();
    uint8_t prefix[MatrixFormat::kV1HeaderBytes];
    const size_t prefixLength = fileLength < sizeof(prefix) ? fileLength : sizeof(prefix);
    if (prefixLength && file.read(prefix, prefixLength) != static_cast<int>(prefixLength)) return false;
    if (!MatrixFormat::parseDatFile(prefix, prefixLength, fileLength, info)) return false;
    return file.seek(0);
}

void AnimationController::insertPreset(const char* path) {
    uint8_t position = 0;
    while (position < _presetCount && strcmp(_presets[position], path) < 0) ++position;

    if (_presetCount < kMaxPresets) {
        for (uint8_t i = _presetCount; i > position; --i) {
            memcpy(_presets[i], _presets[i - 1], kPathBytes);
        }
        strncpy(_presets[position], path, kPathBytes - 1);
        _presets[position][kPathBytes - 1] = '\0';
        ++_presetCount;
    } else if (position < kMaxPresets) {
        for (uint8_t i = kMaxPresets - 1; i > position; --i) {
            memcpy(_presets[i], _presets[i - 1], kPathBytes);
        }
        strncpy(_presets[position], path, kPathBytes - 1);
        _presets[position][kPathBytes - 1] = '\0';
    }
}

bool AnimationController::loadPreset(uint8_t index) {
    if (index >= _presetCount) return false;
    if (_file) _file.close();

    File candidate = SD.open(_presets[index], FILE_READ);
    if (!candidate) {
        Serial.print(F("ERROR: Could not open preset: "));
        Serial.println(_presets[index]);
        _running = false;
        return false;
    }

    MatrixFormat::DatFileInfo info;
    if (!validateFile(candidate, info)) {
        Serial.print(F("ERROR: Preset became invalid: "));
        Serial.println(_presets[index]);
        candidate.close();
        _running = false;
        return false;
    }

    _file = candidate;
    _fileInfo = info;
    _nextFrame = 0;
    _presetIndex = index;
    _running = true;
    Serial.print(F("Playing "));
    Serial.print(_presets[index]);
    Serial.print(F(" ("));
    Serial.print(_fileInfo.frameCount);
    Serial.println(F(" frames, 30 FPS default)"));
    return true;
}

bool AnimationController::update() {
    if (!_running || !_file) return false;

    // Compute every offset from the validated payload start. This excludes a
    // v1 header and makes EOF looping land on the first payload frame.
    const size_t offset = MatrixFormat::frameOffset(_fileInfo, _nextFrame);
    if (!_file.seek(offset) ||
        _file.read(_frameBuffer, MatrixFormat::kFrameBytes) != static_cast<int>(MatrixFormat::kFrameBytes)) {
        Serial.println(F("ERROR: Short read while playing preset; playback stopped."));
        _running = false;
        _file.close();
        return false;
    }
    _nextFrame = (_nextFrame + 1) % _fileInfo.frameCount;
    return true;
}

void AnimationController::render(DisplayController& display) {
    size_t ptr = 0;
    for (uint8_t y = 0; y < MatrixFormat::kHeight; ++y) {
        for (uint8_t x = 0; x < MatrixFormat::kWidth; ++x) {
            display.setPixel(x, y, _frameBuffer[ptr], _frameBuffer[ptr + 1], _frameBuffer[ptr + 2]);
            ptr += MatrixFormat::kBytesPerPixel;
        }
    }
    display.show();
}

bool AnimationController::nextPreset() {
    if (_presetCount == 0) return false;
    return loadPreset((_presetIndex + 1) % _presetCount);
}

bool AnimationController::previousPreset() {
    if (_presetCount == 0) return false;
    return loadPreset((_presetIndex + _presetCount - 1) % _presetCount);
}

const char* AnimationController::currentPreset() const {
    return _presetCount == 0 ? "" : _presets[_presetIndex];
}
