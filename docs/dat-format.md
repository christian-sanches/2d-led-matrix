# LED matrix `.dat` file format

This document is the canonical contract for browser tools and firmware readers. The format is intentionally small: fixed-size RGB frames only, with no timing or playback metadata.

## Shared pixel and playback contract

- Matrix dimensions: **8 × 8**.
- Pixel order: row-major, from the top-left pixel to the bottom-right pixel (left to right, then top to bottom).
- Pixel bytes: **RGB888**, one red byte, one green byte, then one blue byte per pixel.
- One frame is exactly `8 × 8 × 3 = 192` bytes.
- Physical LED wiring order is a firmware/driver concern. Files and browser tools must not apply serpentine ordering or convert RGB to GRB.
- Playback is nominally **30 frames per second**. Neither format stores per-frame duration or other timing metadata.

## Legacy raw format

A legacy `.dat` file is only a concatenation of frame payloads:

```text
frame 0 (192 bytes) | frame 1 (192 bytes) | ...
```

The file must be non-empty and its total size must be a positive multiple of 192 bytes. There is no header, frame count, or version field.

## DAT v1

DAT v1 starts with a fixed **20-byte** little-endian header, followed immediately by the same raw-compatible frame payloads used by legacy files.

| Offset | Size | Value | Meaning |
| ---: | ---: | --- | --- |
| 0 | 8 | ASCII `LEDMAT\r\n` (`4c 45 44 4d 41 54 0d 0a`) | File signature |
| 8 | 1 | `1` | Version |
| 9 | 1 | `20` | Header length in bytes |
| 10 | 1 | `8` | Width in pixels |
| 11 | 1 | `8` | Height in pixels |
| 12 | 1 | `1` | Pixel format: RGB888 |
| 13 | 1 | `0` | Flags; all other values are invalid in v1 |
| 14 | 2 | `192` (`c0 00`) | Bytes per frame, unsigned little-endian |
| 16 | 4 | Positive frame count | Unsigned little-endian frame count |
| 20 | `192 × frameCount` | — | RGB frame payload |

The total file size must equal exactly `20 + 192 × frameCount`. A zero frame count, an unknown version, invalid header field, overflow, truncated payload, or trailing data is invalid. Once the exact signature is present, invalid versioned data must be rejected; it must not be reinterpreted as legacy raw data.

Example for one frame (header only):

```text
4c 45 44 4d 41 54 0d 0a  01 14 08 08 01 00 c0 00  01 00 00 00
```

The frame data starts at byte 20 and remains byte-for-byte compatible with a one-frame legacy file.

## Detection and compatibility notes

Readers should identify DAT v1 by the complete 8-byte signature. If it is present, validate the entire v1 header and exact length. If it is absent, accept legacy data only when its length is a positive multiple of 192. Do not use a failed v1 validation as a reason to fall back to legacy.

Legacy raw has no signature or escaping. Therefore an arbitrary legacy pixel stream can coincidentally begin with the v1 signature bytes. A reader that auto-detects v1 will treat such a file as versioned and may reject it. The legacy format cannot distinguish or prevent that collision.

**Firmware compatibility:** DAT v1 requires firmware with the v1 header reader. Export legacy raw for firmware that only reads the original format. This browser editor warns before exporting v1.
