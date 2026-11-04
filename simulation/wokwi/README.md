# Wokwi simulation (ESP32 compatibility target)

This is a **separate simulation-only target** for visually exercising the
application in Wokwi. It reuses the project's `ControlState`,
`AnimationController`, `DisplayController`, `IRController`, and
`MenuController`; small wrapper translation units include those shared source
files rather than copying their implementations.

Wokwi does not currently emulate the project's ESP8266 NodeMCU target. This
project uses an ESP32 DevKit to simulate the same application behavior, so it
does **not** validate NodeMCU boot straps, the ESP8266 bit-bang timing/IR
interaction, real SD-card timing, power integrity, or electrical levels.
The 5 V rail in the diagram is an ideal Wokwi source, not a wiring instruction;
the real matrix needs its own correctly sized external supply and a common
ground, as described in the root README.

## Run it

1. Install PlatformIO Core plus the Python requirement in this folder, and
   Wokwi for VS Code 4.0.0 or newer (needed for the local `sdcard/` fixture
   directory):

   ```sh
   python -m pip install -r requirements.txt
   ```
2. Open this `simulation/wokwi` folder as a PlatformIO project.
3. Build the ESP32 simulation firmware:

   ```sh
   pio run -e esp32dev
   ```

4. In VS Code, run **Wokwi: Start Simulator**. The `wokwi.toml` file points
   Wokwi at the PlatformIO build output. The `sdcard/PRESETS` directory contains
   synthetic DAT v1 files for the virtual SD card.

Open this folder itself as the VS Code workspace. If you keep the repository
root open, run **Wokwi: Select Config File** and choose
`simulation/wokwi/wokwi.toml`; the root `wokwi.toml` is an older mockup and
points to `.pio/build/esp32/`, not this target's `.pio/build/esp32dev/`.

The same project can be run with Wokwi CLI after installing/configuring that
tool; no Wokwi CLI is needed for the PlatformIO build itself.

## Controls

Use the virtual IR remote beside the receiver:

| Remote key | Action |
| --- | --- |
| Power | Toggle logical matrix power |
| `+` / `-` | Brightness up / down |
| Previous / Next | Previous / next SD preset; navigate/adjust the menu while open |
| `1`, `4`, `3`, `0` | Solid red, green, blue, white |
| Menu | Open / close menu |
| `8` / `9` | Alternate menu up / down |
| Play | Select; on the FPS page, cycle the frame-rate preset |
| Back | Back / exit |

The four text pages are, in order: `BRIGHT nn%`, `WIFI ON/OFF`, the selected
color and `SOLID`/`ANIM`, and `FPS nn`. Uppercase letters, digits, and simple
symbols use a built-in 3×5 bitmap font and scroll **from right to left in normal
reading order**. The status row shows a brightness/FPS bar, Wi-Fi state, or
selected color. The serial monitor also prints the page name whenever it
changes. The menu no longer has a power page; use the Power remote key or
physical button instead. Any menu navigation, recognized application action,
or button press refreshes the 7-second timeout.

When a setting changes, the display temporarily shows only its current value
for 3 seconds after the last change. The full page label then resumes from its
previous scroll position.

On the FPS page, each Play press cycles **15 → 24 → 30 → 48 → 60 FPS** and the
selected number scrolls across the matrix.

Click the green Power pushbutton or press **P** in the focused diagram to test
the physical-button path. The serial monitor reports startup, SD, and NEC
command messages at 115200 baud.

On boot, a red/blue X is shown briefly as a matrix-output check. If no SD
preset is found, the X remains on the matrix and the serial monitor explains
why animation playback did not start.

The virtual SD card is populated from `sdcard/`; its two presets exercise
versioned DAT parsing, playback, and next/previous selection:

- `PRESET00.dat` moves an orange bar across the matrix, holding each column for
  four frames so the motion is easy to follow.
- `PRESET01.dat` alternates blue and white, holding each color for half a second
  to make preset changes and color output visible without a rapid strobe.

Regenerate them with `python3 scripts/generate_sd_fixtures.py` if the format
fixture changes.

## What this validates

- Shared 8x8 serpentine mapping, RGB brightness scaling, and menu drawing.
- Shared menu state machine, timeout, animation pause/resume, and control state.
- Shared DAT parser/player and preset navigation against synthetic SD files.
- IR protocol decoding and application actions using the simulator's NEC
  remote commands.

The simulator uses an ESP32-specific NeoPixelBus method and a simulation-only
pin map. The production firmware remains the separate `nodemcuv2` target; use
its host tests and PlatformIO CI for ESP8266 code validation, then real hardware
for final electrical/timing checks.
