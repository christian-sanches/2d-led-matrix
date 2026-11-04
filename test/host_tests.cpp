#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define IR_COMMAND_POWER 0x20DF10EFULL
#define IR_COMMAND_BRIGHTNESS_UP 0x20DF40BFULL
#define IR_COMMAND_NEXT 0x20DF02FDULL
#define IR_COMMAND_SOLID_RED 0x20DF1AE5ULL
#include "ButtonDebouncer.h"
#include "ControlState.h"
#include "IRKeymap.h"
#include "MatrixFormat.h"

namespace {

void putLe16(uint8_t* target, uint16_t value) {
    target[0] = static_cast<uint8_t>(value);
    target[1] = static_cast<uint8_t>(value >> 8);
}

void putLe32(uint8_t* target, uint32_t value) {
    target[0] = static_cast<uint8_t>(value);
    target[1] = static_cast<uint8_t>(value >> 8);
    target[2] = static_cast<uint8_t>(value >> 16);
    target[3] = static_cast<uint8_t>(value >> 24);
}

void makeV1Header(uint8_t* header, uint32_t frames) {
    memset(header, 0, MatrixFormat::kV1HeaderBytes);
    memcpy(header, MatrixFormat::kV1Signature, sizeof(MatrixFormat::kV1Signature));
    header[8] = 1;
    header[9] = MatrixFormat::kV1HeaderBytes;
    header[10] = MatrixFormat::kWidth;
    header[11] = MatrixFormat::kHeight;
    header[12] = 1;
    putLe16(header + 14, MatrixFormat::kFrameBytes);
    putLe32(header + 16, frames);
}

void testMappingAndRgbPlacement() {
    bool seen[MatrixFormat::kPixelCount] = {};
    for (uint8_t y = 0; y < MatrixFormat::kHeight; ++y) {
        for (uint8_t x = 0; x < MatrixFormat::kWidth; ++x) {
            const uint8_t index = MatrixFormat::serpentineIndex(x, y);
            assert(index < MatrixFormat::kPixelCount);
            assert(!seen[index]);
            seen[index] = true;
            const size_t offset = MatrixFormat::logicalRgbOffset(x, y, 0);
            assert(offset == (static_cast<size_t>(y) * 8 + x) * 3);
            assert(MatrixFormat::logicalRgbOffset(x, y, 1) == offset + 1);
            assert(MatrixFormat::logicalRgbOffset(x, y, 2) == offset + 2);
        }
    }
    for (uint8_t i = 0; i < MatrixFormat::kPixelCount; ++i) assert(seen[i]);
    assert(MatrixFormat::serpentineIndex(0, 0) == 0);
    assert(MatrixFormat::serpentineIndex(7, 0) == 7);
    assert(MatrixFormat::serpentineIndex(0, 1) == 15);
    assert(MatrixFormat::serpentineIndex(7, 1) == 8);
    assert(MatrixFormat::serpentineIndex(0, 7) == 63);
    assert(MatrixFormat::serpentineIndex(8, 0) == UINT8_MAX);
    assert(MatrixFormat::serpentineIndex(0, 8) == UINT8_MAX);

    uint8_t payload[MatrixFormat::kFrameBytes] = {};
    const size_t pixel = MatrixFormat::logicalRgbOffset(7, 0, 0);
    payload[pixel] = 0x12;
    payload[pixel + 1] = 0x34;
    payload[pixel + 2] = 0x56;
    assert(payload[21] == 0x12 && payload[22] == 0x34 && payload[23] == 0x56);
    assert(MatrixFormat::serpentineIndex(7, 0) == 7);
    assert(MatrixFormat::logicalRgbOffset(8, 0, 0) == SIZE_MAX);
    assert(MatrixFormat::scaleChannel(255, 128) == 128);
    assert(MatrixFormat::scaleChannel(128, 128) == 64);
    assert(MatrixFormat::scaleChannel(255, 255) == 255);
}

void testLegacyDatParsing() {
    uint8_t prefix[MatrixFormat::kV1HeaderBytes] = {};
    MatrixFormat::DatFileInfo info;
    assert(MatrixFormat::parseDatFile(NULL, 0, MatrixFormat::kFrameBytes, info));
    assert(info.format == MatrixFormat::DatFormat::Legacy);
    assert(info.payloadOffset == 0 && info.frameCount == 1);
    assert(MatrixFormat::parseDatFile(prefix, sizeof(prefix), 2 * MatrixFormat::kFrameBytes, info));
    assert(info.frameCount == 2);
    assert(!MatrixFormat::parseDatFile(NULL, 0, 0, info));
    assert(!MatrixFormat::parseDatFile(NULL, 0, MatrixFormat::kFrameBytes - 1, info));
    assert(!MatrixFormat::parseDatFile(NULL, 0, MatrixFormat::kFrameBytes + 1, info));
    assert(!MatrixFormat::parseDatFile(MatrixFormat::kV1Signature, 8, 8, info));

    uint8_t collision[MatrixFormat::kV1HeaderBytes] = {};
    memcpy(collision, MatrixFormat::kV1Signature, 8);
    assert(!MatrixFormat::parseDatFile(collision, sizeof(collision), MatrixFormat::kFrameBytes, info));
}

void testVersionedDatParsingAndRejection() {
    uint8_t header[MatrixFormat::kV1HeaderBytes];
    MatrixFormat::DatFileInfo info;
    makeV1Header(header, 2);
    const size_t exactLength = MatrixFormat::kV1HeaderBytes + 2 * MatrixFormat::kFrameBytes;
    assert(MatrixFormat::parseDatFile(header, sizeof(header), exactLength, info));
    assert(info.format == MatrixFormat::DatFormat::Version1);
    assert(info.payloadOffset == 20 && info.frameCount == 2);
    assert(MatrixFormat::frameOffset(info, 0) == 20);
    assert(MatrixFormat::frameOffset(info, 1) == 212);
    assert(MatrixFormat::frameOffset(info, 2) == 20);
    assert(MatrixFormat::frameOffset(info, 3) == 212);
    assert(!MatrixFormat::parseDatFile(header, sizeof(header), exactLength - 1, info));
    assert(!MatrixFormat::parseDatFile(header, sizeof(header), exactLength + 1, info));
    makeV1Header(header, 0x01020304UL);
    assert(MatrixFormat::readLe32(header + 16) == 0x01020304UL);

    const size_t fieldOffsets[] = {8, 9, 10, 11, 12, 13, 14, 16};
    const uint8_t badValues[] = {2, 19, 7, 9, 2, 1, 0, 0};
    for (size_t i = 0; i < sizeof(fieldOffsets) / sizeof(fieldOffsets[0]); ++i) {
        makeV1Header(header, 1);
        header[fieldOffsets[i]] = badValues[i];
        assert(!MatrixFormat::parseDatFile(header, sizeof(header),
                                           MatrixFormat::kV1HeaderBytes + MatrixFormat::kFrameBytes, info));
    }

    makeV1Header(header, 0);
    assert(!MatrixFormat::parseDatFile(header, sizeof(header), MatrixFormat::kV1HeaderBytes, info));
    makeV1Header(header, 1);
    assert(!MatrixFormat::parseDatFile(header, 8, MatrixFormat::kV1HeaderBytes + MatrixFormat::kFrameBytes, info));

    uint64_t expected = 0;
    assert(MatrixFormat::expectedV1Length(1, UINT32_MAX, expected));
    assert(expected == MatrixFormat::kV1HeaderBytes + MatrixFormat::kFrameBytes);
    assert(!MatrixFormat::expectedV1Length(UINT32_MAX, UINT32_MAX, expected));
}

void testControlsAndBrightness() {
    ControlState state;
    assert(state.isPowered());
    assert(state.brightness() == 96);
    assert(state.mode() == DisplayMode::Animation);
    state.apply(ControlAction::BrightnessDown);
    assert(state.brightness() == 64);
    state.apply(ControlAction::BrightnessUp);
    assert(state.brightness() == 96);
    for (int i = 0; i < 8; ++i) state.apply(ControlAction::BrightnessDown);
    assert(state.brightness() == 0);
    state.apply(ControlAction::BrightnessDown);
    assert(state.brightness() == 0);
    state.apply(ControlAction::BrightnessUp);
    assert(state.brightness() == 32);
    assert(MatrixFormat::scaleChannel(255, state.brightness()) == 32);
    assert(MatrixFormat::scaleChannel(255, 255) == 255);

    state.apply(ControlAction::SolidRed);
    assert(state.mode() == DisplayMode::Solid);
    RgbColorValue color = state.solidColor();
    assert(color.red == 255 && color.green == 0 && color.blue == 0);
    state.apply(ControlAction::BrightnessDown);
    assert(state.mode() == DisplayMode::Solid);
    state.apply(actionForIrCommand(0x20DF10EFULL));
    assert(!state.isPowered());
    state.apply(actionForIrCommand(0x20DF10EFULL));
    assert(state.isPowered());
    state.apply(ControlAction::SolidBlue);
    color = state.solidColor();
    assert(state.mode() == DisplayMode::Solid && color.blue == 255 && color.red == 0);
    state.apply(ControlAction::NextPreset);
    assert(state.mode() == DisplayMode::Animation);
    state.apply(ControlAction::SolidWhite);
    assert(state.mode() == DisplayMode::Solid);
    state.apply(ControlAction::PreviousPreset);
    assert(state.mode() == DisplayMode::Animation);
}

void testIrActionMapping() {
    assert(actionForIrCommand(0) == ControlAction::None);
    assert(actionForIrCommand(0x20DF10EFULL) == ControlAction::PowerToggle);
    assert(actionForIrCommand(0x20DF40BFULL) == ControlAction::BrightnessUp);
    assert(actionForIrCommand(0x20DF02FDULL) == ControlAction::NextPreset);
    assert(actionForIrCommand(0x20DF1AE5ULL) == ControlAction::SolidRed);
    assert(actionForIrCommand(0xDEADBEEFULL) == ControlAction::None);
}

void testButtonDebounce() {
    ButtonDebouncer button(30);
    assert(!button.update(true, 0));
    assert(!button.update(false, 10));
    assert(!button.update(true, 15));
    assert(!button.update(false, 20));
    assert(!button.update(false, 49));
    assert(button.update(false, 50));
    assert(!button.update(false, 70));
    assert(!button.update(true, 80));
    assert(!button.update(true, 109));
    assert(!button.update(true, 110));
    assert(!button.update(false, 120));
    assert(button.update(false, 150));

    ButtonDebouncer rollover(30);
    assert(!rollover.update(true, UINT32_MAX - 10));
    assert(!rollover.update(false, UINT32_MAX - 5));
    assert(rollover.update(false, 25));
}

}  // namespace

int main() {
    testMappingAndRgbPlacement();
    testLegacyDatParsing();
    testVersionedDatParsingAndRejection();
    testControlsAndBrightness();
    testIrActionMapping();
    testButtonDebounce();
    puts("All host tests passed.");
    return 0;
}
