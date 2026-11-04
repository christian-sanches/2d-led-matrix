export const MATRIX_WIDTH = 8;
export const MATRIX_HEIGHT = 8;
export const CHANNELS_PER_PIXEL = 3;
export const PIXELS_PER_FRAME = MATRIX_WIDTH * MATRIX_HEIGHT;
export const BYTES_PER_FRAME = PIXELS_PER_FRAME * CHANNELS_PER_PIXEL;

export type RgbFrame = Uint8Array;
export type RgbColor = readonly [number, number, number];

export function createSolidFrame(color: RgbColor): RgbFrame {
  const frame = new Uint8Array(BYTES_PER_FRAME);
  for (let offset = 0; offset < frame.length; offset += CHANNELS_PER_PIXEL) {
    frame.set(color, offset);
  }
  return frame;
}

export function cloneFrame(frame: RgbFrame): RgbFrame {
  if (frame.length !== BYTES_PER_FRAME) {
    throw new Error(`A matrix frame must contain exactly ${BYTES_PER_FRAME} RGB bytes.`);
  }
  return new Uint8Array(frame);
}

export function colorAt(frame: RgbFrame, x: number, y: number): RgbColor {
  const offset = (y * MATRIX_WIDTH + x) * CHANNELS_PER_PIXEL;
  return [frame[offset], frame[offset + 1], frame[offset + 2]];
}

export function setPixel(frame: RgbFrame, x: number, y: number, color: RgbColor): RgbFrame {
  const next = cloneFrame(frame);
  const offset = (y * MATRIX_WIDTH + x) * CHANNELS_PER_PIXEL;
  next.set(color, offset);
  return next;
}

export function rgbToHex(color: RgbColor): string {
  return `#${color.map((channel) => channel.toString(16).padStart(2, '0')).join('')}`;
}

export function hexToRgb(hex: string): RgbColor {
  const normalized = hex.replace('#', '');
  if (!/^[0-9a-f]{6}$/i.test(normalized)) return [255, 255, 255];
  return [
    Number.parseInt(normalized.slice(0, 2), 16),
    Number.parseInt(normalized.slice(2, 4), 16),
    Number.parseInt(normalized.slice(4, 6), 16),
  ];
}

export function rgbaToRgbFrame(
  rgba: Uint8ClampedArray | Uint8Array,
  background: RgbColor = [0, 0, 0],
): RgbFrame {
  if (rgba.length !== PIXELS_PER_FRAME * 4) {
    throw new Error(`Expected ${PIXELS_PER_FRAME} RGBA pixels.`);
  }
  const frame = new Uint8Array(BYTES_PER_FRAME);
  for (let pixel = 0; pixel < PIXELS_PER_FRAME; pixel += 1) {
    const source = pixel * 4;
    const target = pixel * 3;
    const alpha = rgba[source + 3] / 255;
    for (let channel = 0; channel < 3; channel += 1) {
      frame[target + channel] = Math.round(
        rgba[source + channel] * alpha + background[channel] * (1 - alpha),
      );
    }
  }
  return frame;
}
