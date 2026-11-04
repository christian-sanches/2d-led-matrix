import { describe, expect, it } from 'vitest';
import { BYTES_PER_FRAME, rgbaToRgbFrame } from './frames';
import { imageDataToFrame } from './image';

describe('pixel frame helpers', () => {
  it('maps RGBA pixels to RGB bytes in row-major order and composites alpha', () => {
    const rgba = new Uint8ClampedArray(64 * 4);
    rgba.set([255, 0, 0, 255], 0);
    rgba.set([0, 0, 255, 128], 4);
    rgba.set([100, 200, 50, 0], 63 * 4);
    const frame = rgbaToRgbFrame(rgba, [10, 20, 30]);
    expect(frame).toHaveLength(BYTES_PER_FRAME);
    expect(Array.from(frame.slice(0, 6))).toEqual([255, 0, 0, 5, 10, 143]);
    expect(Array.from(frame.slice(-3))).toEqual([10, 20, 30]);
  });

  it('exposes image pixel conversion through the same RGB888 path', () => {
    const rgba = new Uint8ClampedArray(64 * 4);
    rgba.set([40, 80, 120, 255], 8 * 4);
    const frame = imageDataToFrame(rgba);
    expect(Array.from(frame.slice(24, 27))).toEqual([40, 80, 120]);
  });
});
