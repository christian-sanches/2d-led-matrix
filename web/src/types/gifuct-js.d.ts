declare module 'gifuct-js' {
  export interface ParsedGifFrame {
    image?: {
      descriptor: { left: number; top: number; width: number; height: number };
      data: { blocks: Uint8Array[] };
    };
  }

  export interface ParsedGif {
    lsd: { width: number; height: number };
    frames: ParsedGifFrame[];
  }

  export interface DecompressedGifFrame {
    dims: { left: number; top: number; width: number; height: number };
    patch: Uint8ClampedArray;
    delay?: number;
    disposalType?: number;
  }

  export function parseGIF(data: ArrayBuffer): ParsedGif;
  export function decompressFrames(gif: ParsedGif, buildImagePatches: boolean): DecompressedGifFrame[];
}
