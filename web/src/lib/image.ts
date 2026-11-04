import { MATRIX_HEIGHT, MATRIX_WIDTH, rgbaToRgbFrame, type RgbColor, type RgbFrame } from './frames';

export function imageDataToFrame(
  rgba: Uint8ClampedArray | Uint8Array,
  background: RgbColor = [0, 0, 0],
): RgbFrame {
  return rgbaToRgbFrame(rgba, background);
}

export async function imageFileToFrame(file: File, background: RgbColor): Promise<RgbFrame> {
  const bitmap = await createImageBitmap(file);
  try {
    const canvas = document.createElement('canvas');
    canvas.width = MATRIX_WIDTH;
    canvas.height = MATRIX_HEIGHT;
    const context = canvas.getContext('2d', { willReadFrequently: true });
    if (!context) throw new Error('This browser cannot create a 2D canvas.');

    context.fillStyle = `rgb(${background.join(', ')})`;
    context.fillRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT);
    context.imageSmoothingEnabled = true;
    context.imageSmoothingQuality = 'high';

    // Fill the square with a centered crop so portrait and landscape images keep their proportions.
    const side = Math.min(bitmap.width, bitmap.height);
    const sourceX = (bitmap.width - side) / 2;
    const sourceY = (bitmap.height - side) / 2;
    context.drawImage(bitmap, sourceX, sourceY, side, side, 0, 0, MATRIX_WIDTH, MATRIX_HEIGHT);
    return imageDataToFrame(context.getImageData(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT).data, background);
  } finally {
    bitmap.close();
  }
}
