#pragma once

#include <stdint.h>
#include "ControlState.h"

// Replace these deliberately disabled values with the full NEC values observed
// on the serial monitor. No physical remote codes have been verified yet.
#ifndef IR_COMMAND_POWER
#define IR_COMMAND_POWER 0ULL
#endif
#ifndef IR_COMMAND_BRIGHTNESS_UP
#define IR_COMMAND_BRIGHTNESS_UP 0ULL
#endif
#ifndef IR_COMMAND_BRIGHTNESS_DOWN
#define IR_COMMAND_BRIGHTNESS_DOWN 0ULL
#endif
#ifndef IR_COMMAND_PREVIOUS
#define IR_COMMAND_PREVIOUS 0ULL
#endif
#ifndef IR_COMMAND_NEXT
#define IR_COMMAND_NEXT 0ULL
#endif
#ifndef IR_COMMAND_SOLID_RED
#define IR_COMMAND_SOLID_RED 0ULL
#endif
#ifndef IR_COMMAND_SOLID_GREEN
#define IR_COMMAND_SOLID_GREEN 0ULL
#endif
#ifndef IR_COMMAND_SOLID_BLUE
#define IR_COMMAND_SOLID_BLUE 0ULL
#endif
#ifndef IR_COMMAND_SOLID_WHITE
#define IR_COMMAND_SOLID_WHITE 0ULL
#endif

inline ControlAction actionForIrCommand(uint64_t code) {
    if (code == 0) return ControlAction::None;
    if (IR_COMMAND_POWER && code == IR_COMMAND_POWER) return ControlAction::PowerToggle;
    if (IR_COMMAND_BRIGHTNESS_UP && code == IR_COMMAND_BRIGHTNESS_UP) return ControlAction::BrightnessUp;
    if (IR_COMMAND_BRIGHTNESS_DOWN && code == IR_COMMAND_BRIGHTNESS_DOWN) return ControlAction::BrightnessDown;
    if (IR_COMMAND_PREVIOUS && code == IR_COMMAND_PREVIOUS) return ControlAction::PreviousPreset;
    if (IR_COMMAND_NEXT && code == IR_COMMAND_NEXT) return ControlAction::NextPreset;
    if (IR_COMMAND_SOLID_RED && code == IR_COMMAND_SOLID_RED) return ControlAction::SolidRed;
    if (IR_COMMAND_SOLID_GREEN && code == IR_COMMAND_SOLID_GREEN) return ControlAction::SolidGreen;
    if (IR_COMMAND_SOLID_BLUE && code == IR_COMMAND_SOLID_BLUE) return ControlAction::SolidBlue;
    if (IR_COMMAND_SOLID_WHITE && code == IR_COMMAND_SOLID_WHITE) return ControlAction::SolidWhite;
    return ControlAction::None;
}
