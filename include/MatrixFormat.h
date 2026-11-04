#pragma once

#include <stddef.h>
#include <stdint.h>

namespace MatrixFormat {

static const uint8_t kWidth = 8;
static const uint8_t kHeight = 8;
static const uint16_t kPixelCount = kWidth * kHeight;
static const size_t kBytesPerPixel = 3;
static const size_t kFrameBytes = kPixelCount * kBytesPerPixel;
static const size_t kV1HeaderBytes = 20;
static const uint8_t kV1Signature[8] = {'L', 'E', 'D', 'M', 'A', 'T', '\r', '\n'};

enum class DatFormat : uint8_t {
    Invalid,
    Legacy,
    Version1
};

struct DatFileInfo {
    DatFormat format;
    size_t payloadOffset;
    uint32_t frameCount;

    DatFileInfo() : format(DatFormat::Invalid), payloadOffset(0), frameCount(0) {}
};

inline uint16_t readLe16(const uint8_t* bytes) {
    return static_cast<uint16_t>(bytes[0]) |
           (static_cast<uint16_t>(bytes[1]) << 8);
}

inline uint32_t readLe32(const uint8_t* bytes) {
    return static_cast<uint32_t>(bytes[0]) |
           (static_cast<uint32_t>(bytes[1]) << 8) |
           (static_cast<uint32_t>(bytes[2]) << 16) |
           (static_cast<uint32_t>(bytes[3]) << 24);
}

inline bool expectedV1Length(uint32_t frameCount, uint64_t maxLength, uint64_t& length) {
    if (frameCount == 0 || maxLength < kV1HeaderBytes ||
        static_cast<uint64_t>(frameCount) >
            (maxLength - kV1HeaderBytes) / kFrameBytes) {
        return false;
    }
    length = kV1HeaderBytes + static_cast<uint64_t>(frameCount) * kFrameBytes;
    return true;
}

inline size_t logicalRgbOffset(uint8_t x, uint8_t y, uint8_t channel) {
    if (x >= kWidth || y >= kHeight || channel >= kBytesPerPixel) return SIZE_MAX;
    return (static_cast<size_t>(y) * kWidth + x) * kBytesPerPixel + channel;
}

inline uint8_t scaleChannel(uint8_t channel, uint8_t brightness) {
    return static_cast<uint8_t>((static_cast<uint16_t>(channel) * brightness) / 255);
}

// prefix contains up to the first 20 file bytes; the complete file length is
// supplied separately so the same validation can be used before streaming.
inline bool parseDatFile(const uint8_t* prefix, size_t prefixBytes,
                         size_t fileLength, DatFileInfo& result) {
    result = DatFileInfo();
    if (fileLength == 0) return false;

    bool hasSignature = prefix != NULL && prefixBytes >= sizeof(kV1Signature);
    if (hasSignature) {
        for (size_t i = 0; i < sizeof(kV1Signature); ++i) {
            if (prefix[i] != kV1Signature[i]) {
                hasSignature = false;
                break;
            }
        }
    }

    if (hasSignature) {
        if (prefixBytes < kV1HeaderBytes || fileLength < kV1HeaderBytes) return false;
        if (prefix[8] != 1 || prefix[9] != kV1HeaderBytes ||
            prefix[10] != kWidth || prefix[11] != kHeight ||
            prefix[12] != 1 || prefix[13] != 0 ||
            readLe16(prefix + 14) != kFrameBytes) {
            return false;
        }

        const uint32_t frames = readLe32(prefix + 16);
        uint64_t expectedLength = 0;
        if (!expectedV1Length(frames, static_cast<uint64_t>(SIZE_MAX), expectedLength) ||
            static_cast<uint64_t>(fileLength) != expectedLength) return false;

        result.format = DatFormat::Version1;
        result.payloadOffset = kV1HeaderBytes;
        result.frameCount = frames;
        return true;
    }

    if (fileLength % kFrameBytes != 0) return false;
    const size_t frames = fileLength / kFrameBytes;
    if (frames == 0 || frames > UINT32_MAX) return false;

    result.format = DatFormat::Legacy;
    result.payloadOffset = 0;
    result.frameCount = static_cast<uint32_t>(frames);
    return true;
}

inline size_t frameOffset(const DatFileInfo& info, uint32_t frameIndex) {
    if (info.frameCount == 0) return 0;
    return info.payloadOffset +
           static_cast<size_t>(frameIndex % info.frameCount) * kFrameBytes;
}

inline uint8_t serpentineIndex(uint8_t x, uint8_t y) {
    if (x >= kWidth || y >= kHeight) return UINT8_MAX;
    const uint8_t rowOffset = static_cast<uint8_t>(y * kWidth);
    return static_cast<uint8_t>(rowOffset + ((y & 1) ? (kWidth - 1 - x) : x));
}

}  // namespace MatrixFormat
