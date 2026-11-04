import { describe, expect, it } from 'vitest';
import { BYTES_PER_FRAME, createSolidFrame } from './frames';
import { DAT_HEADER_LENGTH, DAT_MAGIC, DatFormatError, decodeDat, encodeLegacy, encodeV1 } from './dat';

describe('DAT encoding and decoding', () => {
  it('writes the exact 20-byte v1 header and little-endian frame count', () => {
    const data = encodeV1([createSolidFrame([1, 2, 3]), createSolidFrame([4, 5, 6])]);
    expect(Array.from(data.slice(0, DAT_HEADER_LENGTH))).toEqual([
      0x4c, 0x45, 0x44, 0x4d, 0x41, 0x54, 0x0d, 0x0a,
      1, 20, 8, 8, 1, 0, 192, 0,
      2, 0, 0, 0,
    ]);
    expect(data.byteLength).toBe(DAT_HEADER_LENGTH + 2 * BYTES_PER_FRAME);
  });

  it('round-trips frames exactly in v1 and legacy formats', () => {
    const frames = [createSolidFrame([0, 9, 255]), createSolidFrame([240, 100, 3])];
    for (const [format, bytes] of [['v1', encodeV1(frames)], ['legacy', encodeLegacy(frames)]] as const) {
      const decoded = decodeDat(bytes);
      expect(decoded.format).toBe(format);
      expect(decoded.frames).toHaveLength(frames.length);
      decoded.frames.forEach((frame, index) => expect(frame).toEqual(frames[index]));
    }
  });

  it('keeps RGB bytes in row-major top-left-to-bottom-right order', () => {
    const frame = new Uint8Array(BYTES_PER_FRAME);
    frame.set([11, 22, 33], 0);
    frame.set([44, 55, 66], 3);
    frame.set([77, 88, 99], 7 * 8 * 3);
    frame.set([101, 111, 121], BYTES_PER_FRAME - 3);
    const output = encodeLegacy([frame]);
    expect(Array.from(output.slice(0, 6))).toEqual([11, 22, 33, 44, 55, 66]);
    expect(Array.from(output.slice(-3))).toEqual([101, 111, 121]);
    expect(decodeDat(output).frames[0]).toEqual(frame);
  });

  it('accepts one or more complete legacy frames only', () => {
    expect(decodeDat(new Uint8Array(BYTES_PER_FRAME)).frames).toHaveLength(1);
    expect(() => decodeDat(new Uint8Array())).toThrow(/positive multiple/);
    expect(() => decodeDat(new Uint8Array(BYTES_PER_FRAME + 1))).toThrow(/positive multiple/);
  });

  it('rejects unknown versions and never falls back to legacy after a v1 signature', () => {
    const data = encodeV1([createSolidFrame([0, 0, 0])]);
    data[8] = 2;
    expect(() => decodeDat(data)).toThrow(/Unsupported DAT version/);
    expect(() => decodeDat(data)).toThrow(DatFormatError);
  });

  it.each([
    [9, 19, /header length/],
    [10, 7, /dimensions/],
    [12, 2, /RGB888/],
    [13, 1, /flags/],
    [14, 0, /frame size/],
  ])('rejects invalid header field at offset %i', (offset, value, message) => {
    const data = encodeV1([createSolidFrame([0, 0, 0])]);
    data[offset] = value;
    expect(() => decodeDat(data)).toThrow(message);
  });

  it('rejects zero, overflow-sized, truncated, and trailing frame payloads', () => {
    const valid = encodeV1([createSolidFrame([0, 0, 0])]);
    const zeroFrames = new Uint8Array(valid);
    zeroFrames.fill(0, 16, 20);
    expect(() => decodeDat(zeroFrames)).toThrow(/at least one frame/);

    const oversizedCount = new Uint8Array(valid);
    oversizedCount.fill(0xff, 16, 20);
    expect(() => decodeDat(oversizedCount)).toThrow(/payload length/);
    expect(() => decodeDat(valid.slice(0, -1))).toThrow(/payload length/);
    expect(() => decodeDat(new Uint8Array([...valid, 0]))).toThrow(/payload length/);

    const truncatedHeader = DAT_MAGIC.slice();
    expect(() => decodeDat(truncatedHeader)).toThrow(/header is truncated/);
  });

  it('requires exactly 192 bytes per exported frame', () => {
    expect(() => encodeLegacy([new Uint8Array(BYTES_PER_FRAME - 1)])).toThrow(/exactly 192/);
    expect(() => encodeV1([])).toThrow(/at least one frame/);
  });
});
