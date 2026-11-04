import { describe, expect, it } from 'vitest';
import { textCanvasFont } from './text';

describe('text style helpers', () => {
  it('combines selected font, bold, and italic properties', () => {
    expect(textCanvasFont({ font: 'monospace', bold: true, italic: true }, 8)).toBe('italic 700 8px monospace');
    expect(textCanvasFont({ font: 'serif', bold: false, italic: false }, 6)).toBe('400 6px serif');
  });
});
