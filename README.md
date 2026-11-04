# 2D LED Matrix (ESP8266 NodeMCU)

Firmware for a fixed 8×8 RGB matrix driven by an ESP8266 NodeMCU, a MicroSD
module, an IR receiver, and one active-low power button. The firmware target is
**PlatformIO `nodemcuv2` / ESP8266**. No physical validation is claimed yet.

## Pinout

| Function | NodeMCU label | ESP8266 GPIO | Notes |
| --- | --- | ---: | --- |
| Matrix data | D2 | GPIO4 | Through a 5V-compatible level shifter recommended for 5V strips |
| Button | D1 | GPIO5 | Momentary switch between pin and GND; internal pull-up |
| IR receiver OUT | D3 | GPIO0 | Idle must remain HIGH; GPIO0 is a boot-strap pin |
| SD chip select | D8 | GPIO15 | Also a boot-strap pin; keep SD module wiring compatible with boot |
| SD SCK | D5 | GPIO14 | ESP8266 HSPI clock |
| SD MISO | D6 | GPIO12 | ESP8266 HSPI input |
| SD MOSI | D7 | GPIO13 | ESP8266 HSPI output |
| SD / IR logic supply | 3V3 | — | Only if the particular module is rated for 3.3V |

The SD module must be compatible with ESP8266 3.3V logic. Verify its voltage
requirements; some SD breakout boards include regulators/level shifting and
some do not. Keep the SD card chip-select and SPI wiring as listed.

### Matrix power and signal safety

- Identify the exact matrix/LED type and its voltage/current requirements
  before connecting it. Use a separate, regulated supply sized for the actual
  matrix and a fuse/current limit appropriate to the hardware.
- **Do not power the matrix from the NodeMCU 3.3V pin.** Connect matrix supply
  ground, NodeMCU ground, and peripheral grounds together (common ground).
- A 5V WS2812-style matrix may not reliably recognize a 3.3V ESP8266 data
  signal. A 5V-compatible logic level shifter on the GPIO4 data line is
  recommended. Confirm the strip input voltage/timing specification.
- The wiring drawing is a pinout guide, not a reviewed power design. Verify the
  module/connector pin labels and polarity before applying power.

## Build and upload

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html),
connect the board, then run:

```sh
pio run -e nodemcuv2
pio run -e nodemcuv2 -t upload
pio device monitor -b 115200
```

The upload port is intentionally not fixed in `platformio.ini`; PlatformIO
should discover it, or pass `--upload-port <your-port>` explicitly.

The GPIO4 output uses `NeoEsp8266BitBangWs2812xMethod` because the default ESP8266
DMA method ignores the requested pin and transmits on GPIO3/RX. Bit-bang is the
NeoPixelBus method that honors GPIO4, but it is timing-sensitive and is not the
recommended ESP8266 hardware method. It occupies roughly 1.9 ms to transmit 64
pixels and may mask IR receiver edges during that interval; IR reception and
pixel signal integrity must be checked on the actual hardware. Wi-Fi is not
used. Do not treat compile success as electrical or timing validation.

## SD preset files

Place valid files directly in `/PRESETS` on the SD card (FAT filesystem). The
firmware discovers `.dat` files, rejects invalid files, sorts valid paths in
bytewise filename order, and cycles through that order. At most 32 valid
presets are retained; full paths must fit in 63 bytes (54 filename bytes after
`/PRESETS/`). Playback is 30 FPS; DAT v1 has no per-frame timing.

Each frame is exactly 192 bytes: 64 row-major RGB888 pixels, starting at the
logical top-left and proceeding across each row to the bottom-right. Serpentine
physical wiring is handled only by the firmware display driver. The header, if
present, is excluded from the frame payload.

### Legacy `.dat`

Raw RGB bytes, with a non-zero total length that is an exact multiple of 192.

### Versioned DAT v1

Exactly 20 header bytes followed by `frameCount` × 192 payload bytes:

| Bytes | Value |
| --- | --- |
| 0–7 | ASCII `LEDMAT\r\n` (`4c 45 44 4d 41 54 0d 0a`) |
| 8 | Version `1` |
| 9 | Header size `20` |
| 10 | Width `8` |
| 11 | Height `8` |
| 12 | Pixel format `1` (RGB888) |
| 13 | Flags `0` |
| 14–15 | Frame bytes `192`, unsigned little-endian |
| 16–19 | Positive frame count, unsigned little-endian |

The file length must equal `20 + frameCount * 192` exactly. A matching v1
signature with an invalid, unknown, truncated, or inconsistent header is
rejected; it is never retried as legacy data. Legacy playback remains supported.

## Controls

- The physical button toggles logical power. It is debounced in firmware; it
  does not disconnect supply power.
- IR actions support power toggle, brightness up/down, previous/next preset,
  and solid red/green/blue/white. Solid mode stays selected until previous or
  next preset is requested. Brightness changes scale the original RGB values,
  so successive adjustments do not compound rounding.
- NEC button codes are remote-specific and **not known or hardware-verified**.
  The default map is deliberately disabled (all values zero), rather than
  guessing codes. Open the serial monitor at 115200 baud and press remote keys;
  firmware prints `protocol-id`, bit count, full value, address, and command.
  Copy each observed full `value=0x...` into the matching `IR_COMMAND_*` entry
  in `include/IRKeymap.h` (as `0x...ULL`); use unique values. After rebuilding,
  verify every mapped action with the actual remote. Protocol id is diagnostic;
  this keymap currently compares full decoded values.
- Startup brightness is limited to 96/255 to reduce initial load; adjust the
  external supply and increase brightness only after verifying the matrix's
  power/current specification.

IR is polled while the logical power is off, so a mapped power command can wake
the matrix. Because LED data output can briefly block interrupts on ESP8266,
verify the IR receiver still decodes reliably during animation playback.

## Diagrams and validation status

- [`diagram.mermaid`](diagram.mermaid), [`2d-led-matrix.kicad_sch`](2d-led-matrix.kicad_sch),
  and [`kicad_schematic_project.kicad_sch`](kicad_schematic_project.kicad_sch)
  describe the ESP8266 pinout; the KiCad files are draft connector-level guides,
  not ERC-checked/validated power schematics.
- [`diagram.json`](diagram.json) and [`wokwi.toml`](wokwi.toml) are retained as
  an **old ESP32 simulation mockup**. Their board, pins, and simulated power
  wiring do not represent this ESP8266 build and must not be used as its wiring
  reference.
- No hardware photographs are included; no hardware image has been supplied
  or verified for this repository.

## Checks

Run host-testable logic checks and the ESP8266 compile:

```sh
./test/run_host_tests.sh
pio run -e nodemcuv2
```

The host tests cover all 64 unique serpentine indices, logical RGB placement,
legacy and v1 parsing/rejections, overflow checks, frame-loop offsets, control
state/brightness/solid mode, IR mapping, and button debounce. They do not run
Arduino/SD/display/IR hardware integration.

### Hands-on validation still required

- Confirm the exact matrix type, its supply voltage/current, connector
  orientation, color order, and top-left/serpentine direction.
- Check the GPIO4 signal with the actual level shifter/strip and verify all
  pixels and RGB placement at safe brightness.
- Test power-up/boot with the SD module on GPIO15 and IR output on GPIO0.
- Test SD initialization, malformed-file rejection, preset ordering and
  previous/next on the target card/module.
- Capture the actual remote NEC values, configure the keymap, then verify all
  actions, power-off wake, and IR reliability while frames are transmitting.
- Confirm button behavior and debounce with the physical switch.

Until these are performed, hardware compatibility and runtime behavior remain
unverified.
