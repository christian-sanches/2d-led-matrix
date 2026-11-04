# Matrix Studio

A browser-only 8×8 RGB frame editor for the 2D LED matrix. It creates and edits frames, converts local image and GIF files, rasterizes text, previews sequences at 30 fps, and reads/writes both legacy raw `.dat` and DAT v1. There is no server or upload path; file processing happens in the browser.

The shared binary contract is documented in [`../docs/dat-format.md`](../docs/dat-format.md). DAT v1 needs updated firmware; use legacy export with older firmware.

## Run locally

Requires Node.js 20 or newer and npm.

```sh
npm ci
npm run dev
```

Vite prints a local URL. The app can also be built and served as static files:

```sh
npm run build
npm run preview
```

## Use

- Click any LED in the 8×8 editor to paint it with the selected foreground or background color. Choose a color with either swatch.
- Add and edit frames, reorder or delete them, and use **Play** for a looping 30 fps preview.
- Import a still image to add a centered, square-cropped 8×8 frame. GIFs are composited (including transparency and disposal modes 2/3), sampled at 30 fps, and appended to the sequence.
- Enter up to 32 characters, choose a system font and bold/italic styles, and create a text frame using the current foreground/background colors.
- Open `.dat` to replace the current sequence. The editor supports up to 900 frames per project; GIFs are limited to 20 MB, 30 seconds, 600 source frames, 1024×1024 canvas dimensions, and 16 million total decoded source pixels to keep browser memory bounded.
- Export DAT v1 (with a firmware compatibility confirmation) or legacy raw.

Image and text are rasterized at 8×8, so fine details and small lettering may not remain legible. Text uses fonts available to the user's browser/device. GIF delay is sampled on a 30 fps timeline; short source frames may be skipped, while disposal-to-background (mode 2) is treated as transparent clearing, matching common transparent GIF animations. GIFs longer than the import limit are rejected rather than silently truncated.

## Test

```sh
npm run typecheck
npm test
npm run build
```

Tests cover exact DAT v1 header bytes, valid legacy/v1 round trips, invalid versioned and legacy data, RGB row-major layout, alpha compositing, GIF disposal and 30 fps sampling, and text style generation.

GitHub Actions runs the typecheck, Vitest suite, and production build for pull requests and pushes that change `web/` or the web CI workflow.
