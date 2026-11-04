#!/usr/bin/env python3
"""Add KiCad project instance metadata to a SchemaSeed-generated schematic.

SchemaSeed's geometry/connectivity output is checked before this step. This
postprocessor marks real carrier-board components as fitted, gives KiCad the
project/sheet paths it needs for netlist export, and leaves power flags out of
the BOM/PCB.
"""

from __future__ import annotations

import re
import sys
import uuid
from pathlib import Path


PROJECT = "2d-led-matrix-carrier"

TARGETS = {
    "J1": (45.72, 90.17),
    "J2": (81.28, 90.17),
    "J3": (134.62, 90.17),
    "JP1": (175.26, 90.17),
    "U2": (226.06, 90.17),
    "R1": (271.78, 90.17),
    "J6": (320.04, 90.17),
    "J4": (134.62, 180.34),
    "J5": (175.26, 180.34),
    "J7": (226.06, 180.34),
    "C1": (271.78, 180.34),
    "C2": (297.18, 180.34),
    "#FLG01": (322.58, 180.34),
    "#FLG02": (347.98, 180.34),
    "#FLG03": (373.38, 180.34),
    "H1": (45.72, 229.87),
    "H2": (373.38, 229.87),
    "H3": (45.72, 254.0),
    "H4": (373.38, 254.0),
}


def _matching_paren(text: str, start: int) -> int:
    depth = 0
    in_string = False
    escaped = False
    for pos in range(start, len(text)):
        ch = text[pos]
        if in_string:
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                in_string = False
            continue
        if ch == '"':
            in_string = True
        elif ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0:
                return pos + 1
    raise ValueError(f"Unclosed S-expression starting at byte {start}")


def _move_at_positions(block: str, dx: float, dy: float) -> str:
    pattern = re.compile(
        r'\(at\s+(-?\d+(?:\.\d*)?(?:e[-+]?\d+)?)\s+'
        r'(-?\d+(?:\.\d*)?(?:e[-+]?\d+)?)'
        r'(?:\s+(-?\d+(?:\.\d*)?(?:e[-+]?\d+)?))?\)'
    )

    def shift(match: re.Match[str]) -> str:
        x = float(match.group(1)) + dx
        y = float(match.group(2)) + dy
        angle = match.group(3)
        if angle is None:
            return f"(at {x:.4f} {y:.4f})"
        return f"(at {x:.4f} {y:.4f} {angle})"

    return pattern.sub(shift, block)


def _top_level_blocks(text: str, expression: str) -> list[tuple[int, int, str]]:
    results: list[tuple[int, int, str]] = []
    for match in re.finditer(expression, text):
        start = match.start()
        end = _matching_paren(text, start)
        results.append((start, end, text[start:end]))
    return results


def _hide_property(block: str, key: str, size: float = 1.0) -> str:
    match = re.search(rf'\(property\s+"{re.escape(key)}"\s+"[^"]*"', block)
    if not match:
        return block
    start = match.start()
    end = _matching_paren(block, start)
    prop = block[start:end]
    if "(hide yes)" not in prop:
        prop = prop[:-1] + f" (effects (font (size {size} {size})) (hide yes)))"
    return block[:start] + prop + block[end:]


def finalize(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    text = text.replace('(paper "A4")', '(paper "A3")', 1)
    if not re.search(r"\(uuid\s+\"", text[:400]):
        text = text.replace(
            '(generator kiutils)',
            f'(generator "eeschema")\n  (uuid "{uuid.uuid4()}")',
            1,
        )

    if "(title_block" not in text:
        text = text.replace(
            '(paper "A3")',
            '(paper "A3")\n  (title_block\n'
            '    (title "2D LED Matrix Carrier V1")\n'
            '    (date "2026-11-04")\n'
            '    (rev "1.0")\n'
            '    (comment 1 "LoLin NodeMCU ESP8266; carrier board 70 x 90 mm")\n'
            '  )',
            1,
        )

    symbol_blocks = _top_level_blocks(text, r'\(symbol\s+\(lib_id\s+"')
    original_positions: dict[str, tuple[float, float]] = {}
    edits: list[tuple[int, int, str]] = []
    for start, end, block in symbol_blocks:
        ref_m = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block)
        unit_m = re.search(r'\(unit\s+(\d+)\)', block)
        lib_m = re.search(r'\(lib_id\s+"([^"]+)"', block)
        at_m = re.search(r'\(at\s+(-?\d+(?:\.\d*)?)\s+(-?\d+(?:\.\d*)?)\s+', block)
        if not (ref_m and unit_m and lib_m and at_m):
            raise ValueError(f"Incomplete placed symbol at byte {start}")
        ref, unit, lib_id = ref_m.group(1), unit_m.group(1), lib_m.group(1)
        old_x, old_y = float(at_m.group(1)), float(at_m.group(2))
        original_positions[ref] = (old_x, old_y)

        is_power_symbol = lib_id.startswith("power:") or ref.startswith("#")
        is_mechanical = ref.startswith("H")
        if not is_power_symbol and not is_mechanical:
            block = block.replace("(in_bom no) (on_board no)", "(in_bom yes) (on_board yes)", 1)
        elif is_mechanical:
            block = block.replace("(in_bom yes) (on_board yes)", "(in_bom no) (on_board yes)", 1)
        else:
            block = block.replace("(in_bom yes) (on_board yes)", "(in_bom no) (on_board no)", 1)

        if ref not in TARGETS:
            raise ValueError(f"No schematic layout position defined for {ref}")
        target_x, target_y = TARGETS[ref]
        dx, dy = target_x - old_x, target_y - old_y
        block = _move_at_positions(block, dx, dy)

        # Footprints are metadata, not schematic artwork. Keep them hidden so
        # long library IDs do not overlap component values in the drawing.
        block = _hide_property(block, "Footprint", 1.27)

        # Power-flag references and values are intentionally non-BOM and hidden.
        if ref.startswith("#"):
            block = _hide_property(block, "Reference", 1.0)
            block = _hide_property(block, "Value", 1.0)

        if "(instances" not in block:
            instance = (
                f'\n    (instances (project "{PROJECT}" '
                f'(path "/" (reference "{ref}") (unit {unit}))))\n  '
            )
            block = block[:-1] + instance + ")"
        edits.append((start, end, block))

    for start, end, block in reversed(edits):
        text = text[:start] + block + text[end:]

    # Labels and no-connect markers belong to the symbol whose original x
    # coordinate is nearest; move them with that symbol after arranging rows.
    for expr in (r'\(label\s+"', r'\(no_connect\s'):
        blocks = _top_level_blocks(text, expr)
        shifted: list[tuple[int, int, str]] = []
        for start, end, block in blocks:
            at_m = re.search(r'\(at\s+(-?\d+(?:\.\d*)?)\s+(-?\d+(?:\.\d*)?)(?:\s+[-+]?\d+(?:\.\d*)?)?\)', block)
            if not at_m:
                raise ValueError(f"Missing label/marker coordinate at byte {start}")
            x, y = float(at_m.group(1)), float(at_m.group(2))
            ref = min(original_positions, key=lambda r: abs(original_positions[r][0] - x))
            old_x, old_y = original_positions[ref]
            new_x, new_y = TARGETS[ref]
            block = _move_at_positions(block, new_x - old_x, new_y - old_y)
            shifted.append((start, end, block))
        for start, end, block in reversed(shifted):
            text = text[:start] + block + text[end:]

    path.write_text(text, encoding="utf-8")
    return len(edits)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: finalize_schematic.py <file.kicad_sch>")
    print(f"Finalized {finalize(Path(sys.argv[1]))} symbol instances")
