import { BYTES_PER_FRAME, type RgbFrame } from './frames';

export const DAT_MAGIC = new Uint8Array([0x4c, 0x45, 0x44, 0x4d, 0x41, 0x54, 0x0d, 0x0a]);
export const DAT_HEADER_LENGTH = 20;
const DAT_VERSION = 1;
const DAT_RGB888 = 1;
const UINT32_MAX = 0xffff_ffff;

export type DatFormat = 'legacy' | 'v1';
export interface DecodedDat {
  format: DatFormat;
  frames: RgbFrame[];
}

export class DatFormatError extends Error {
  constructor(message: string) {
    super(message);
    this.name = 'DatFormatError';
  }
}

function checkedFrames(frames: readonly RgbFrame[]): number {
  if (frames.length < 1) throw new DatFormatError('A DAT file must contain at least one frame.');
  if (frames.length > UINT32_MAX) throw new DatFormatError('The frame count exceeds the DAT v1 limit.');
  for (const frame of frames) {
    if (frame.length !== BYTES_PER_FRAME) {
      throw new DatFormatError(`Every frame must contain exactly ${BYTES_PER_FRAME} RGB bytes.`);
    }
  }
  return frames.length;
}

export function encodeLegacy(frames: readonly RgbFrame[]): Uint8Array {
  const frameCount = checkedFrames(frames);
  const output = new Uint8Array(frameCount * BYTES_PER_FRAME);
  frames.forEach((frame, index) => output.set(frame, index * BYTES_PER_FRAME));
  return output;
}

export function encodeV1(frames: readonly RgbFrame[]): Uint8Array {
  const frameCount = checkedFrames(frames);
  const payloadLength = frameCount * BYTES_PER_FRAME;
  const output = new Uint8Array(DAT_HEADER_LENGTH + payloadLength);
  output.set(DAT_MAGIC, 0);
  output[8] = DAT_VERSION;
  output[9] = DAT_HEADER_LENGTH;
  output[10] = 8;
  output[11] = 8;
  output[12] = DAT_RGB888;
  output[13] = 0;
  output[14] = BYTES_PER_FRAME & 0xff;
  output[15] = (BYTES_PER_FRAME >> 8) & 0xff;
  new DataView(output.buffer).setUint32(16, frameCount, true);
  frames.forEach((frame, index) => output.set(frame, DAT_HEADER_LENGTH + index * BYTES_PER_FRAME));
  return output;
}

function hasMagic(data: Uint8Array): boolean {
  return data.length >= DAT_MAGIC.length && DAT_MAGIC.every((byte, index) => data[index] === byte);
}

function copyFrames(data: Uint8Array, start: number, frameCount: number): RgbFrame[] {
  const frames: RgbFrame[] = [];
  for (let index = 0; index < frameCount; index += 1) {
    const offset = start + index * BYTES_PER_FRAME;
    frames.push(data.slice(offset, offset + BYTES_PER_FRAME));
  }
  return frames;
}

export function decodeDat(input: ArrayBuffer | Uint8Array): DecodedDat {
  const data = input instanceof Uint8Array ? input : new Uint8Array(input);

  if (hasMagic(data)) {
    if (data.length < DAT_HEADER_LENGTH) {
      throw new DatFormatError('The DAT v1 header is truncated.');
    }
    if (data[8] !== DAT_VERSION) throw new DatFormatError(`Unsupported DAT version ${data[8]}.`);
    if (data[9] !== DAT_HEADER_LENGTH) throw new DatFormatError('Invalid DAT v1 header length.');
    if (data[10] !== 8 || data[11] !== 8) throw new DatFormatError('DAT v1 dimensions must be 8×8.');
    if (data[12] !== DAT_RGB888) throw new DatFormatError('DAT v1 pixel format must be RGB888.');
    if (data[13] !== 0) throw new DatFormatError('DAT v1 flags must be zero.');
    const bytesPerFrame = data[14] | (data[15] << 8);
    if (bytesPerFrame !== BYTES_PER_FRAME) {
      throw new DatFormatError(`DAT v1 frame size must be ${BYTES_PER_FRAME} bytes.`);
    }
    const frameCount = new DataView(data.buffer, data.byteOffset, data.byteLength).getUint32(16, true);
    if (frameCount === 0) throw new DatFormatError('DAT v1 must declare at least one frame.');
    const expectedLength = DAT_HEADER_LENGTH + frameCount * BYTES_PER_FRAME;
    if (!Number.isSafeInteger(expectedLength) || expectedLength !== data.length) {
      throw new DatFormatError('DAT v1 payload length does not match its frame count.');
    }
    return { format: 'v1', frames: copyFrames(data, DAT_HEADER_LENGTH, frameCount) };
  }

  if (data.length === 0 || data.length % BYTES_PER_FRAME !== 0) {
    throw new DatFormatError(`Legacy DAT files must be a positive multiple of ${BYTES_PER_FRAME} bytes.`);
  }
  return { format: 'legacy', frames: copyFrames(data, 0, data.length / BYTES_PER_FRAME) };
}
