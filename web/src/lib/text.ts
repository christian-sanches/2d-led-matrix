import { MATRIX_HEIGHT, MATRIX_WIDTH, rgbaToRgbFrame, type RgbColor, type RgbFrame } from './frames';

export type TextFont = 'monospace' | 'sans-serif' | 'serif';

export interface TextOptions {
  text: string;
  foreground: RgbColor;
  background: RgbColor;
  font: TextFont;
  bold: boolean;
  italic: boolean;
}

export function textCanvasFont(options: Pick<TextOptions, 'font' | 'bold' | 'italic'>, size: number): string {
  const style = options.italic ? 'italic ' : '';
  const weight = options.bold ? '700 ' : '400 ';
  return `${style}${weight}${size}px ${options.font}`;
}

export function renderTextFrame(options: TextOptions): RgbFrame {
  const canvas = document.createElement('canvas');
  canvas.width = MATRIX_WIDTH;
  canvas.height = MATRIX_HEIGHT;
  const context = canvas.getContext('2d', { willReadFrequently: true });
  if (!context) throw new Error('This browser cannot create a 2D canvas.');

  context.fillStyle = `rgb(${options.background.join(', ')})`;
  context.fillRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT);
  context.textAlign = 'center';
  context.textBaseline = 'middle';
  context.fillStyle = `rgb(${options.foreground.join(', ')})`;
  const text = Array.from(options.text).slice(0, 32).join('');
  let size = 8;
  context.font = textCanvasFont(options, size);
  while (size > 3 && context.measureText(text).width > MATRIX_WIDTH + 0.5) {
    size -= 1;
    context.font = textCanvasFont(options, size);
  }
  context.fillText(text, MATRIX_WIDTH / 2, MATRIX_HEIGHT / 2, MATRIX_WIDTH);
  return rgbaToRgbFrame(context.getImageData(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT).data, options.background);
}
