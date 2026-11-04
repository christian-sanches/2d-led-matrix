import { decompressFrames, parseGIF, type DecompressedGifFrame, type ParsedGif } from 'gifuct-js';
import { MATRIX_HEIGHT, MATRIX_WIDTH, rgbaToRgbFrame, type RgbColor, type RgbFrame } from './frames';

export const GIF_FRAME_RATE = 30;
export const MAX_GIF_BYTES = 20 * 1024 * 1024;
export const MAX_GIF_SOURCE_FRAMES = 600;
export const MAX_GIF_DECODED_PIXELS = 16_000_000;
export const MAX_GIF_DURATION_MS = 30_000;

interface GifPatchFrame {
  dims: { left: number; top: number; width: number; height: number };
  patch: Uint8ClampedArray;
  delay?: number;
  disposalType?: number;
}

function pixelOffset(width: number, x: number, y: number): number {
  return (y * width + x) * 4;
}

function clearRect(
  pixels: Uint8ClampedArray,
  canvasWidth: number,
  canvasHeight: number,
  rect: GifPatchFrame['dims'],
): void {
  const left = Math.max(0, rect.left);
  const top = Math.max(0, rect.top);
  const right = Math.min(canvasWidth, rect.left + rect.width);
  const bottom = Math.min(canvasHeight, rect.top + rect.height);
  for (let y = top; y < bottom; y += 1) {
    for (let x = left; x < right; x += 1) pixels.fill(0, pixelOffset(canvasWidth, x, y), pixelOffset(canvasWidth, x, y) + 4);
  }
}

function drawPatch(
  pixels: Uint8ClampedArray,
  canvasWidth: number,
  canvasHeight: number,
  frame: GifPatchFrame,
): void {
  const { left, top, width, height } = frame.dims;
  for (let y = 0; y < height; y += 1) {
    for (let x = 0; x < width; x += 1) {
      const destinationX = left + x;
      const destinationY = top + y;
      if (destinationX < 0 || destinationY < 0 || destinationX >= canvasWidth || destinationY >= canvasHeight) continue;
      const sourceOffset = pixelOffset(width, x, y);
      const destinationOffset = pixelOffset(canvasWidth, destinationX, destinationY);
      const alpha = frame.patch[sourceOffset + 3] / 255;
      if (alpha === 0) continue;
      if (alpha === 1 || pixels[destinationOffset + 3] === 0) {
        pixels.set(frame.patch.subarray(sourceOffset, sourceOffset + 4), destinationOffset);
      } else {
        const destinationAlpha = pixels[destinationOffset + 3] / 255;
        const outputAlpha = alpha + destinationAlpha * (1 - alpha);
        for (let channel = 0; channel < 3; channel += 1) {
          pixels[destinationOffset + channel] = Math.round(
            (frame.patch[sourceOffset + channel] * alpha + pixels[destinationOffset + channel] * destinationAlpha * (1 - alpha)) / outputAlpha,
          );
        }
        pixels[destinationOffset + 3] = Math.round(outputAlpha * 255);
      }
    }
  }
}

/** Compose GIF patch frames into full logical-screen snapshots, honoring disposal 2 and 3. */
export function composeGifFrames(
  width: number,
  height: number,
  frames: readonly GifPatchFrame[],
): Uint8ClampedArray[] {
  const pixels = new Uint8ClampedArray(width * height * 4);
  const snapshots: Uint8ClampedArray[] = [];
  let previous: GifPatchFrame | undefined;
  let restorePrevious: Uint8ClampedArray | undefined;

  for (const frame of frames) {
    if (previous?.disposalType === 2) clearRect(pixels, width, height, previous.dims);
    else if (previous?.disposalType === 3 && restorePrevious) pixels.set(restorePrevious);

    restorePrevious = frame.disposalType === 3 ? new Uint8ClampedArray(pixels) : undefined;
    drawPatch(pixels, width, height, frame);
    snapshots.push(new Uint8ClampedArray(pixels));
    previous = frame;
  }
  return snapshots;
}

/** Resample while compositing so long/high-resolution GIFs never retain full-size snapshots. */
export function resampleGifPatches(
  frames: readonly GifPatchFrame[],
  width: number,
  height: number,
  background: RgbColor,
  toMatrix: (rgba: Uint8ClampedArray) => RgbFrame,
): RgbFrame[] {
  const durations = frames.map((frame) => Math.max(20, frame.delay ?? 100));
  const totalDuration = durations.reduce((sum, duration) => sum + duration, 0);
  if (totalDuration > MAX_GIF_DURATION_MS) {
    throw new Error('This GIF is longer than 30 seconds. Trim it before importing to keep the preview responsive.');
  }

  const output: RgbFrame[] = [];
  const pixels = new Uint8ClampedArray(width * height * 4);
  let previous: GifPatchFrame | undefined;
  let restorePrevious: Uint8ClampedArray | undefined;
  let frameStart = 0;
  let sampleIndex = 0;

  frames.forEach((frame, index) => {
    if (previous?.disposalType === 2) clearRect(pixels, width, height, previous.dims);
    else if (previous?.disposalType === 3 && restorePrevious) pixels.set(restorePrevious);
    restorePrevious = frame.disposalType === 3 ? new Uint8ClampedArray(pixels) : undefined;
    drawPatch(pixels, width, height, frame);

    const frameEnd = frameStart + durations[index];
    while (sampleIndex * 1000 / GIF_FRAME_RATE < frameEnd && output.length < Math.ceil(MAX_GIF_DURATION_MS * GIF_FRAME_RATE / 1000)) {
      output.push(toMatrix(pixels));
      sampleIndex += 1;
    }
    frameStart = frameEnd;
    previous = frame;
  });
  if (output.length === 0 && frames.length > 0) output.push(toMatrix(pixels));
  return output;
}

function validateParsedGif(gif: ParsedGif): { width: number; height: number } {
  const width = gif.lsd.width;
  const height = gif.lsd.height;
  if (!Number.isInteger(width) || !Number.isInteger(height) || width < 1 || height < 1 || width > 1024 || height > 1024) {
    throw new Error('GIF canvas dimensions must be between 1×1 and 1024×1024 pixels.');
  }
  const imageFrames = gif.frames.filter((frame) => frame.image);
  if (imageFrames.length === 0) throw new Error('This GIF contains no image frames.');
  if (imageFrames.length > MAX_GIF_SOURCE_FRAMES) {
    throw new Error(`This GIF has more than ${MAX_GIF_SOURCE_FRAMES} source frames and is too large to import safely.`);
  }
  let decodedPixels = 0;
  for (const frame of imageFrames) {
    const dimensions = frame.image!.descriptor;
    if (dimensions.width < 1 || dimensions.height < 1) throw new Error('This GIF contains an empty frame.');
    decodedPixels += dimensions.width * dimensions.height;
    if (decodedPixels > MAX_GIF_DECODED_PIXELS) {
      throw new Error('This GIF has too many decoded pixels to process safely. Try a smaller or shorter GIF.');
    }
  }
  return { width, height };
}

function createBrowserDownsampler(
  width: number,
  height: number,
  background: RgbColor,
): (rgba: Uint8ClampedArray) => RgbFrame {
  const source = document.createElement('canvas');
  source.width = width;
  source.height = height;
  const sourceContext = source.getContext('2d', { willReadFrequently: true });
  const target = document.createElement('canvas');
  target.width = MATRIX_WIDTH;
  target.height = MATRIX_HEIGHT;
  const targetContext = target.getContext('2d', { willReadFrequently: true });
  if (!sourceContext || !targetContext) throw new Error('This browser cannot create a 2D canvas.');
  const sourceImage = sourceContext.createImageData(width, height);

  return (rgba) => {
    sourceImage.data.set(rgba);
    sourceContext.putImageData(sourceImage, 0, 0);
    targetContext.fillStyle = `rgb(${background.join(', ')})`;
    targetContext.fillRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT);
    targetContext.imageSmoothingEnabled = true;
    targetContext.imageSmoothingQuality = 'high';
    targetContext.drawImage(source, 0, 0, MATRIX_WIDTH, MATRIX_HEIGHT);
    return rgbaToRgbFrame(targetContext.getImageData(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT).data, background);
  };
}

export async function gifFileToFrames(file: File, background: RgbColor): Promise<RgbFrame[]> {
  if (file.size > MAX_GIF_BYTES) throw new Error('GIF files must be 20 MB or smaller.');
  if (file.size === 0) throw new Error('The selected GIF is empty.');

  let parsed: ParsedGif;
  try {
    parsed = parseGIF(await file.arrayBuffer());
  } catch {
    throw new Error('This file could not be decoded as a GIF.');
  }
  const { width, height } = validateParsedGif(parsed);
  const frames = decompressFrames(parsed, true).filter((frame): frame is DecompressedGifFrame => Boolean(frame));
  return resampleGifPatches(frames, width, height, background, createBrowserDownsampler(width, height, background));
}
