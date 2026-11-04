import { useEffect, useMemo, useRef, useState } from 'react';
import {
  BYTES_PER_FRAME,
  colorAt,
  createSolidFrame,
  hexToRgb,
  rgbToHex,
  setPixel,
  type RgbFrame,
} from './lib/frames';
import { decodeDat, encodeLegacy, encodeV1 } from './lib/dat';
import { gifFileToFrames } from './lib/gif';
import { imageFileToFrame } from './lib/image';
import { renderTextFrame, type TextFont } from './lib/text';

const MAX_FRAMES = 900;
const MAX_DAT_IMPORT_BYTES = 20 + BYTES_PER_FRAME * MAX_FRAMES;

interface MatrixFrame {
  id: number;
  name: string;
  pixels: RgbFrame;
}

function frameLabel(index: number): string {
  return `Frame ${String(index + 1).padStart(2, '0')}`;
}

function downloadDat(data: Uint8Array, filename: string): void {
  const bytes = new ArrayBuffer(data.byteLength);
  new Uint8Array(bytes).set(data);
  const blob = new Blob([bytes], { type: 'application/octet-stream' });
  const url = URL.createObjectURL(blob);
  const link = document.createElement('a');
  link.href = url;
  link.download = filename;
  link.click();
  URL.revokeObjectURL(url);
}

export default function App() {
  const [frames, setFrames] = useState<MatrixFrame[]>(() => [
    { id: 1, name: 'Starter frame', pixels: createSolidFrame([11, 22, 23]) },
  ]);
  const [activeId, setActiveId] = useState(1);
  const [nextId, setNextId] = useState(2);
  const [foregroundHex, setForegroundHex] = useState('#e8ff70');
  const [backgroundHex, setBackgroundHex] = useState('#0b1618');
  const [brush, setBrush] = useState<'foreground' | 'background'>('foreground');
  const [playing, setPlaying] = useState(false);
  const [message, setMessage] = useState('Ready when you are. Paint a pixel or bring in an image.');
  const [text, setText] = useState('HELLO');
  const [font, setFont] = useState<TextFont>('monospace');
  const [bold, setBold] = useState(false);
  const [italic, setItalic] = useState(false);
  const imageInput = useRef<HTMLInputElement>(null);
  const gifInput = useRef<HTMLInputElement>(null);
  const datInput = useRef<HTMLInputElement>(null);

  const activeIndex = Math.max(0, frames.findIndex((frame) => frame.id === activeId));
  const activeFrame = frames[activeIndex] ?? frames[0];
  const foreground = useMemo(() => hexToRgb(foregroundHex), [foregroundHex]);
  const background = useMemo(() => hexToRgb(backgroundHex), [backgroundHex]);

  useEffect(() => {
    if (!playing) return;
    const timer = window.setInterval(() => {
      setActiveId((currentId) => {
        const currentIndex = frames.findIndex((frame) => frame.id === currentId);
        return frames[(currentIndex + 1) % frames.length]?.id ?? currentId;
      });
    }, 1000 / 30);
    return () => window.clearInterval(timer);
  }, [playing, frames]);

  function updateActive(pixels: RgbFrame): void {
    setFrames((current) => current.map((frame) => frame.id === activeId ? { ...frame, pixels } : frame));
  }

  function appendFrames(newPixels: RgbFrame[], label: string): void {
    if (!newPixels.length) throw new Error('No frames were created.');
    if (frames.length + newPixels.length > MAX_FRAMES) {
      throw new Error(`The editor is limited to ${MAX_FRAMES} frames per sequence.`);
    }
    const items = newPixels.map((pixels, index) => ({
      id: nextId + index,
      name: newPixels.length === 1 ? label : `${label} ${String(index + 1).padStart(2, '0')}`,
      pixels,
    }));
    setFrames((current) => [...current, ...items]);
    setActiveId(items[0].id);
    setNextId((current) => current + items.length);
    setPlaying(false);
  }

  function addBlankFrame(): void {
    if (frames.length >= MAX_FRAMES) {
      setMessage(`The editor is limited to ${MAX_FRAMES} frames per sequence.`);
      return;
    }
    const frame = { id: nextId, name: 'Untitled frame', pixels: createSolidFrame(background) };
    setFrames((current) => [...current, frame]);
    setNextId((current) => current + 1);
    setActiveId(frame.id);
    setPlaying(false);
    setMessage('Blank frame added.');
  }

  async function importImage(file?: File): Promise<void> {
    if (!file) return;
    setMessage(`Preparing ${file.name}…`);
    try {
      appendFrames([await imageFileToFrame(file, background)], file.name);
      setMessage(`${file.name} imported as a new 8×8 frame.`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not open this image.');
    }
  }

  async function importGif(file?: File): Promise<void> {
    if (!file) return;
    setMessage(`Decoding ${file.name} locally…`);
    try {
      const gifFrames = await gifFileToFrames(file, background);
      appendFrames(gifFrames, file.name.replace(/\.gif$/i, ''));
      setMessage(`${file.name}: imported ${gifFrames.length} frames at 30 fps.`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not decode this GIF.');
    }
  }

  async function importDat(file?: File): Promise<void> {
    if (!file) return;
    try {
      if (file.size > MAX_DAT_IMPORT_BYTES) {
        throw new Error(`This editor accepts DAT sequences up to ${MAX_FRAMES} frames (${MAX_DAT_IMPORT_BYTES.toLocaleString()} bytes).`);
      }
      const decoded = decodeDat(await file.arrayBuffer());
      if (decoded.frames.length > MAX_FRAMES) throw new Error(`This editor is limited to ${MAX_FRAMES} frames.`);
      const imported = decoded.frames.map((pixels, index) => ({
        id: nextId + index,
        name: file.name.replace(/\.dat$/i, '') + (decoded.frames.length > 1 ? ` ${String(index + 1).padStart(2, '0')}` : ''),
        pixels,
      }));
      setFrames(imported);
      setActiveId(imported[0].id);
      setNextId((current) => current + imported.length);
      setPlaying(false);
      setMessage(`Loaded ${imported.length} ${decoded.format === 'v1' ? 'DAT v1' : 'legacy raw'} frame${imported.length === 1 ? '' : 's'}.`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not read this DAT file.');
    }
  }

  function addTextFrame(): void {
    try {
      appendFrames([renderTextFrame({ text, foreground, background, font, bold, italic })], 'Text frame');
      setMessage('Text rasterized to a new 8×8 frame.');
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not render text.');
    }
  }

  function removeFrame(id: number): void {
    if (frames.length === 1) {
      setMessage('A sequence needs at least one frame.');
      return;
    }
    const index = frames.findIndex((frame) => frame.id === id);
    const remaining = frames.filter((frame) => frame.id !== id);
    setFrames(remaining);
    if (id === activeId) setActiveId(remaining[Math.max(0, index - 1)].id);
    setMessage('Frame removed.');
  }

  function moveFrame(id: number, direction: -1 | 1): void {
    const index = frames.findIndex((frame) => frame.id === id);
    const destination = index + direction;
    if (destination < 0 || destination >= frames.length) return;
    const reordered = [...frames];
    [reordered[index], reordered[destination]] = [reordered[destination], reordered[index]];
    setFrames(reordered);
  }

  function exportFile(format: 'v1' | 'legacy'): void {
    if (format === 'v1' && !window.confirm('DAT v1 requires updated firmware. Older firmware may not read this file. Export anyway?')) return;
    try {
      const data = format === 'v1'
        ? encodeV1(frames.map((frame) => frame.pixels))
        : encodeLegacy(frames.map((frame) => frame.pixels));
      downloadDat(data, format === 'v1' ? 'matrix-animation-v1.dat' : 'matrix-animation-legacy.dat');
      setMessage(`Exported ${frames.length} ${format === 'v1' ? 'DAT v1' : 'legacy'} frame${frames.length === 1 ? '' : 's'}.`);
    } catch (error) {
      setMessage(error instanceof Error ? error.message : 'Could not export this sequence.');
    }
  }

  if (!activeFrame) return null;

  return (
    <main className="app-shell">
      <header className="topbar">
        <a className="brand" href="#" aria-label="Matrix Studio home">
          <span className="brand-mark" aria-hidden="true"><i /><i /><i /><i /></span>
          <span><strong>matrix</strong><small>STUDIO</small></span>
        </a>
        <div className="topbar-center"><span className="connection-dot" /> ALL CHANGES STAY IN YOUR BROWSER</div>
        <div className="topbar-actions">
          <button className="button button-quiet" onClick={() => datInput.current?.click()}><span aria-hidden="true">↥</span> Open .dat</button>
          <button className="button button-primary" onClick={() => exportFile('v1')}><span aria-hidden="true">↓</span> Export .dat</button>
        </div>
      </header>

      <div className="page-heading">
        <div>
          <div className="eyebrow">8 × 8 RGB ANIMATION EDITOR</div>
          <h1>Make it light up.</h1>
          <p>Build tiny animations for your 2D LED matrix.</p>
        </div>
        <div className="project-pill"><span className="project-dot" /> Untitled animation <span className="pill-separator">·</span> {frames.length} frame{frames.length === 1 ? '' : 's'}</div>
      </div>

      <div className="workspace">
        <section className="editor-column" aria-label="Frame editor">
          <div className="panel matrix-panel">
            <div className="panel-heading">
              <div><span className="section-index">01</span><div><h2>Pixel editor</h2><p>Click a pixel to paint it</p></div></div>
              <span className="size-tag">8 <span>×</span> 8</span>
            </div>
            <div className="matrix-stage">
              <div className="matrix-bezel" role="grid" aria-label={`8 by 8 pixel matrix, ${frameLabel(activeIndex)}`}>
                {Array.from({ length: 64 }, (_, index) => {
                  const x = index % 8;
                  const y = Math.floor(index / 8);
                  const color = colorAt(activeFrame.pixels, x, y);
                  return (
                    <button
                      className="pixel"
                      key={index}
                      role="gridcell"
                      aria-label={`Row ${y + 1}, column ${x + 1}, ${rgbToHex(color)}`}
                      onClick={() => updateActive(setPixel(activeFrame.pixels, x, y, brush === 'foreground' ? foreground : background))}
                      style={{ backgroundColor: rgbToHex(color) }}
                    />
                  );
                })}
              </div>
              <div className="matrix-caption"><span><i className="live-dot" /> LIVE PREVIEW</span><span>FRAME {String(activeIndex + 1).padStart(2, '0')}</span></div>
            </div>
            <div className="color-controls">
              <div className="color-swatches" aria-label="Paint colors">
                <div className={`color-swatch ${brush === 'foreground' ? 'selected' : ''}`}>
                  <input type="color" value={foregroundHex} onChange={(event) => setForegroundHex(event.target.value)} aria-label="Foreground color" />
                  <span className="swatch-chip" style={{ backgroundColor: foregroundHex }} />
                  <span><strong>Foreground</strong><small>{foregroundHex.toUpperCase()}</small></span>
                  <button className="swatch-select" aria-label="Use foreground color" onClick={(event) => { event.preventDefault(); setBrush('foreground'); }}>●</button>
                </div>
                <div className={`color-swatch ${brush === 'background' ? 'selected' : ''}`}>
                  <input type="color" value={backgroundHex} onChange={(event) => setBackgroundHex(event.target.value)} aria-label="Background color" />
                  <span className="swatch-chip" style={{ backgroundColor: backgroundHex }} />
                  <span><strong>Background</strong><small>{backgroundHex.toUpperCase()}</small></span>
                  <button className="swatch-select" aria-label="Use background color" onClick={(event) => { event.preventDefault(); setBrush('background'); }}>●</button>
                </div>
              </div>
              <div className="brush-note"><span aria-hidden="true">✦</span> Click the colored dot to select your brush</div>
            </div>
          </div>

          <div className="panel timeline-panel">
            <div className="panel-heading timeline-heading">
              <div><span className="section-index">02</span><div><h2>Frames</h2><p>Sequence plays at 30 frames per second</p></div></div>
              <div className="timeline-actions">
                <button className={`button ${playing ? 'button-stop' : 'button-quiet'}`} onClick={() => setPlaying((value) => !value)} aria-pressed={playing}>
                  <span aria-hidden="true">{playing ? 'Ⅱ' : '▶'}</span> {playing ? 'Pause' : 'Play'}
                </button>
                <button className="button button-quiet add-frame" onClick={addBlankFrame}><span aria-hidden="true">＋</span> Add frame</button>
              </div>
            </div>
            <div className="frame-strip" aria-label="Animation frames">
              {frames.map((frame, index) => (
                <article className={`frame-card ${frame.id === activeId ? 'active' : ''}`} key={frame.id}>
                  <button className="frame-select" onClick={() => { setActiveId(frame.id); setPlaying(false); }} aria-label={`Select ${frameLabel(index)}, ${frame.name}`} aria-current={frame.id === activeId ? 'true' : undefined}>
                    <span className="thumbnail" aria-hidden="true">
                      {Array.from({ length: 64 }, (_, pixel) => {
                        const color = colorAt(frame.pixels, pixel % 8, Math.floor(pixel / 8));
                        return <i key={pixel} style={{ backgroundColor: rgbToHex(color) }} />;
                      })}
                    </span>
                    <span className="frame-meta"><strong>{frameLabel(index)}</strong><small>{frame.name}</small></span>
                  </button>
                  <div className="frame-tools">
                    <button onClick={() => moveFrame(frame.id, -1)} disabled={index === 0} aria-label={`Move ${frameLabel(index)} earlier`}>←</button>
                    <button onClick={() => moveFrame(frame.id, 1)} disabled={index === frames.length - 1} aria-label={`Move ${frameLabel(index)} later`}>→</button>
                    <button className="remove-frame" onClick={() => removeFrame(frame.id)} aria-label={`Delete ${frameLabel(index)}`}>×</button>
                  </div>
                </article>
              ))}
              {frames.length < MAX_FRAMES && <button className="frame-add-card" onClick={addBlankFrame}><span>＋</span><small>New frame</small></button>}
            </div>
          </div>
        </section>

        <aside className="tools-column" aria-label="Import and create">
          <section className="panel tools-panel">
            <div className="panel-heading compact-heading"><div><span className="section-index">03</span><div><h2>Bring something in</h2><p>Files are processed on this device</p></div></div></div>
            <button className="import-card" onClick={() => imageInput.current?.click()}>
              <span className="import-icon image-icon" aria-hidden="true">▧</span><span><strong>Import an image</strong><small>PNG, JPG, WebP & more</small></span><span className="import-arrow">↗</span>
            </button>
            <button className="import-card" onClick={() => gifInput.current?.click()}>
              <span className="import-icon gif-icon" aria-hidden="true">GIF</span><span><strong>Import an animation</strong><small>GIF → 30 fps frame sequence</small></span><span className="import-arrow">↗</span>
            </button>
            <button className="import-card" onClick={() => datInput.current?.click()}>
              <span className="import-icon dat-icon" aria-hidden="true">.dat</span><span><strong>Open a matrix file</strong><small>Legacy raw or DAT v1</small></span><span className="import-arrow">↗</span>
            </button>
            <input ref={imageInput} className="visually-hidden" type="file" accept="image/*" onChange={(event) => { void importImage(event.currentTarget.files?.[0]); event.currentTarget.value = ''; }} />
            <input ref={gifInput} className="visually-hidden" type="file" accept="image/gif,.gif" onChange={(event) => { void importGif(event.currentTarget.files?.[0]); event.currentTarget.value = ''; }} />
            <input ref={datInput} className="visually-hidden" type="file" accept=".dat,application/octet-stream" onChange={(event) => { void importDat(event.currentTarget.files?.[0]); event.currentTarget.value = ''; }} />
            <p className="import-footnote"><span aria-hidden="true">⌑</span> Nothing is uploaded. Your files stay right here.</p>
          </section>

          <section className="panel text-panel">
            <div className="panel-heading compact-heading"><div><span className="section-index">04</span><div><h2>Make it a message</h2><p>Rasterize text to an 8×8 frame</p></div></div></div>
            <label className="field-label" htmlFor="text-content">YOUR TEXT</label>
            <input id="text-content" className="text-input" value={text} maxLength={32} onChange={(event) => setText(event.target.value)} placeholder="Type something…" />
            <div className="text-options">
              <label className="font-select-label"><span className="field-label">FONT</span>
                <select value={font} onChange={(event) => setFont(event.target.value as TextFont)} aria-label="Text font">
                  <option value="monospace">Mono</option><option value="sans-serif">Sans</option><option value="serif">Serif</option>
                </select>
              </label>
              <div className="style-buttons" aria-label="Text style">
                <button className={bold ? 'selected' : ''} onClick={() => setBold((value) => !value)} aria-pressed={bold} aria-label="Bold text"><strong>B</strong></button>
                <button className={italic ? 'selected' : ''} onClick={() => setItalic((value) => !value)} aria-pressed={italic} aria-label="Italic text"><em>I</em></button>
              </div>
            </div>
            <div className="text-colors"><span>Uses current</span><span><i style={{ backgroundColor: foregroundHex }} /> text</span><span><i style={{ backgroundColor: backgroundHex }} /> background</span></div>
            <button className="button button-create" onClick={addTextFrame} disabled={!text.trim()}><span aria-hidden="true">✳</span> Create text frame <span aria-hidden="true">→</span></button>
          </section>

          <section className="export-note">
            <span className="export-note-icon" aria-hidden="true">↗</span>
            <div><strong>Choose your file format</strong><p>DAT v1 has a header and requires updated firmware. Legacy raw works with older firmware.</p>
              <div className="export-links"><button onClick={() => exportFile('v1')}>Export DAT v1</button><span>·</span><button onClick={() => exportFile('legacy')}>Export legacy</button></div>
            </div>
          </section>
        </aside>
      </div>

      <footer className="statusbar" role="status" aria-live="polite"><span className="status-icon" aria-hidden="true">●</span><span>{message}</span><span className="status-right">8×8 · RGB888 · 30 FPS</span></footer>
    </main>
  );
}
