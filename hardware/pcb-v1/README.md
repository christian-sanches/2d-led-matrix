# 2D LED Matrix — carrier PCB v1

Design notes for the KiCad carrier board. Schematic and PCB are ERC/DRC clean; the remaining items are the two physical confirmations at the end of this file. Do not order fabrication until they are answered.

## Mechanical target

- PCB outline: 70 × 90 mm (matching the current perfboard).
- Four M3 clearance holes are planned 3.5 mm in from the board corners, matching the mounting style of the perfboard.
- Controller: LoLin-style NodeMCU ESP8266, 30 pins (2 × 15), 2.54 mm pitch, 27.94 mm row spacing; micro-USB at the lower edge, antenna toward the upper edge.
- SD breakout: directly plugged into a project-local 1 × 6 female socket, 2.54 mm pitch. It is oriented with the card slot toward the upper board edge; the carrier silk labels the header pads `CS, SCK, MOSI, MISO, VCC, GND` from left to right as photographed.
- IR receiver: remote 3-wire lead to a clearly labelled 3-pin 2.54 mm header; use signal names, not wire colors.
- LED matrix: existing red/green/white lead terminates at a 3-position high-current connector.

## Firmware-matched connections

| Function | NodeMCU pad | Net / connector |
|---|---|---|
| Matrix data | D2 / GPIO4 | 74AHCT1G125 input → 330 Ω → `LED_DATA` |
| Button | D1 / GPIO5 | `BTN` to momentary switch; other side to GND |
| IR signal | D3 / GPIO0 | `IR_SIG` |
| SD CS | D8 / GPIO15 | `SD_CS` |
| SD SCK | D5 / GPIO14 | `SD_SCK` |
| SD MISO | D6 / GPIO12 | `SD_MISO` |
| SD MOSI | D7 / GPIO13 | `SD_MOSI` |

SD module header order as viewed in the supplied photo, left to right: `CS, SCK, MOSI, MISO, VCC, GND`.

Matrix harness order as supplied by the user: red = +5 V, green = DATA, white = GND. The PCB connector will be silk-labelled `+5V / DATA / GND`.

IR connector will be silk-labelled `SIG / 3V3 / GND`; user wire colors are not assumed.

## Power and protection plan

- External 5 V supply for the LED matrix, recommended 5 V / 5 A. The NodeMCU remains powered by its own USB connection; do not connect the matrix supply to NodeMCU VIN/VU.
- Common ground between external supply, matrix, NodeMCU, SD breakout, and IR receiver.
- 3-position solder-jumper or header selector for SD module VCC: `3V3` or `5V`; select only after checking the breakout's input specification.
- SN74AHCT1G125DBVR (SOT-23-5), powered from external +5 V for reliable 3.3 V-to-5 V NeoPixel data translation; 100 nF local bypass capacitor.
- 330–470 Ω series resistor on `LED_DATA` and 500–1000 µF bulk capacitor across matrix +5 V/GND.
- High-current +5 V and GND routing to use wide copper pours/tracks and connectors rated for at least 5 A. Verify copper weight and temperature-rise assumptions before release.
- Preserve the ESP8266 boot-strap states: GPIO0/D3 high and GPIO15/D8 low at reset; keep the antenna area free of copper/components.

## Release checklist

- [x] Schematic ERC: 0 violations.
- [x] PCB DRC: 0 violations, 0 unconnected pads.
- [x] Gerbers and Excellon drill files generated in `fabrication/`; ready-to-upload archive at `2d-led-matrix-carrier-gerbers.zip`.
- [ ] Confirm physical placement/clearance of the SD breakout and card insertion path.
- [ ] Confirm whether the existing perfboard's four corner holes should be carried over and their exact centers (currently assumed 3.5 mm from each corner).
- [ ] Confirm the SD breakout VCC rating and set JP1 to `3V3` or `5V`.
- [ ] Inspect the board at 1:1 scale (print `previews/pcb-1to1.pdf`) before ordering.
