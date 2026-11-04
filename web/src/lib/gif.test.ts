import { describe, expect, it } from 'vitest';
import { composeGifFrames, resampleGifPatches } from './gif';
import type { RgbFrame } from './frames';

function patch(width: number, pixels: number[][]): Uint8ClampedArray {
  return new Uint8ClampedArray(pixels.flat());
}

describe('GIF composition and sampling', () => {
  it('composes transparent patches and clears the previous rect for disposal mode 2', () => {
    const snapshots = composeGifFrames(2, 1, [
      { dims: { left: 0, top: 0, width: 2, height: 1 }, patch: patch(2, [[255, 0, 0, 255], [255, 0, 0, 255]]), disposalType: 2 },
      { dims: { left: 0, top: 0, width: 1, height: 1 }, patch: patch(1, [[0, 255, 0, 255]]) },
    ]);
    expect(Array.from(snapshots[0])).toEqual([255, 0, 0, 255, 255, 0, 0, 255]);
    expect(Array.from(snapshots[1])).toEqual([0, 255, 0, 255, 0, 0, 0, 0]);
  });

  it('restores the prior canvas for disposal mode 3', () => {
    const snapshots = composeGifFrames(2, 1, [
      { dims: { left: 0, top: 0, width: 1, height: 1 }, patch: patch(1, [[255, 0, 0, 255]]) },
      { dims: { left: 1, top: 0, width: 1, height: 1 }, patch: patch(1, [[0, 255, 0, 255]]), disposalType: 3 },
      { dims: { left: 1, top: 0, width: 1, height: 1 }, patch: patch(1, [[0, 0, 255, 255]]) },
    ]);
    expect(Array.from(snapshots[1])).toEqual([255, 0, 0, 255, 0, 255, 0, 255]);
    expect(Array.from(snapshots[2])).toEqual([255, 0, 0, 255, 0, 0, 255, 255]);
  });

  it('samples image patches on a 30 fps timeline and bounds duration', () => {
    const input = [
      { dims: { left: 0, top: 0, width: 1, height: 1 }, patch: patch(1, [[1, 2, 3, 255]]), delay: 100 },
      { dims: { left: 0, top: 0, width: 1, height: 1 }, patch: patch(1, [[4, 5, 6, 255]]), delay: 100 },
    ];
    const frames = resampleGifPatches(input, 1, 1, [0, 0, 0], (rgba) => new Uint8Array(rgba.slice(0, 3)) as RgbFrame);
    expect(frames).toHaveLength(6);
    expect(Array.from(frames[0])).toEqual([1, 2, 3]);
    expect(Array.from(frames[3])).toEqual([4, 5, 6]);

    const long = [{ ...input[0], delay: 31_000 }];
    expect(() => resampleGifPatches(long, 1, 1, [0, 0, 0], (rgba) => new Uint8Array(rgba) as RgbFrame)).toThrow(/30 seconds/);
  });
});
