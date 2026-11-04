#pragma once

#include <stdint.h>

class ButtonDebouncer {
public:
    explicit ButtonDebouncer(uint16_t debounceMs = 30)
        : _debounceMs(debounceMs), _lastRaw(true), _stable(true), _changedAt(0) {}

    // Returns true once for each debounced press (active-low input).
    bool update(bool rawHigh, uint32_t nowMs) {
        if (rawHigh != _lastRaw) {
            _lastRaw = rawHigh;
            _changedAt = nowMs;
        }

        if (_lastRaw != _stable && static_cast<uint32_t>(nowMs - _changedAt) >= _debounceMs) {
            _stable = _lastRaw;
            return !_stable;
        }
        return false;
    }

private:
    uint16_t _debounceMs;
    bool _lastRaw;
    bool _stable;
    uint32_t _changedAt;
};
