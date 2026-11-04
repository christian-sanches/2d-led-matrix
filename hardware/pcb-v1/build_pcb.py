#!/usr/bin/env python3
"""Build the 2D LED Matrix carrier board from the KiCad schematic netlist.

Run with KiCad's bundled Python (the one that can import pcbnew):

    /Applications/KiCad/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9 build_pcb.py

The script places every footprint, routes the signal nets with a small
two-layer A* router, pours the +5 V and GND zones, refills them and saves the
board. Always finish with `kicad-cli pcb drc` before releasing fabrication
files.
"""

from __future__ import annotations

import heapq
import math
import re
from pathlib import Path

import pcbnew

HERE = Path(__file__).resolve().parent
NETLIST = HERE / "circuit.net"
BOARD_FILE = HERE / "2d-led-matrix-carrier.kicad_pcb"
SCHEMATIC_FILE = "2d-led-matrix-carrier.kicad_sch"
SHARE = Path("/Applications/KiCad/KiCad.app/Contents/SharedSupport/footprints")

BOARD_W, BOARD_H = 70.0, 90.0
EDGE_MARGIN = 0.55

from pcbnew import FromMM as FMM, ToMM as TMM  # noqa: E402


def V(x_mm: float, y_mm: float):
    return pcbnew.VECTOR2I(FMM(x_mm), FMM(y_mm))


# --------------------------------------------------------------------------
# netlist (small s-expression reader)
# --------------------------------------------------------------------------

def tokenize(text: str):
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in " \t\r\n":
            i += 1
        elif c in "()":
            yield c
            i += 1
        elif c == '"':
            j, buf = i + 1, []
            while j < n:
                if text[j] == "\\":
                    buf.append(text[j + 1])
                    j += 2
                elif text[j] == '"':
                    break
                else:
                    buf.append(text[j])
                    j += 1
            yield ("str", "".join(buf))
            i = j + 1
        else:
            j = i
            while j < n and text[j] not in ' \t\r\n()"':
                j += 1
            yield ("atom", text[i:j])
            i = j


def parse(text: str):
    stack = [[]]
    for tok in tokenize(text):
        if tok == "(":
            stack.append([])
        elif tok == ")":
            node = stack.pop()
            stack[-1].append(node)
        else:
            stack[-1].append(tok[1])
    return stack[0][0]


def find_all(node, name):
    return [x for x in node if isinstance(x, list) and x and x[0] == name]


def find(node, name):
    for x in node:
        if isinstance(x, list) and x and x[0] == name:
            return x
    return None


def canon(name: str) -> str:
    """KiCad exports hierarchical names like '/gnd'; boards use 'GND'."""
    return name.lstrip("/").upper()


def load_netlist():
    root = parse(NETLIST.read_text(encoding="utf-8"))
    components = {}
    for comp in find_all(find(root, "components"), "comp"):
        ref = find(comp, "ref")[1]
        components[ref] = {
            "value": find(comp, "value")[1] if find(comp, "value") else "",
            "footprint": find(comp, "footprint")[1] if find(comp, "footprint") else "",
            "uuid": find(comp, "tstamps")[1] if find(comp, "tstamps") else "",
        }
    pad_net = {}
    for net in find_all(find(root, "nets"), "net"):
        name = find(net, "name")[1]
        for node in find_all(net, "node"):
            pad_net[(find(node, "ref")[1], find(node, "pin")[1])] = canon(name)
    return components, pad_net


# --------------------------------------------------------------------------
# placement
# --------------------------------------------------------------------------

POSITIONS = {
    "J1": (12.70, 44.00),
    "J2": (40.64, 44.00),
    "J3": (48.00, 37.00),
    "J4": (8.00, 10.00),
    "J5": (20.00, 10.00),
    "J6": (48.00, 66.00),
    "J7": (48.00, 84.00),
    "JP1": (47.00, 43.20),
    "U2": (52.00, 52.00),
    "R1": (55.00, 56.00),
    "C1": (63.50, 50.00),
    "C2": (56.00, 48.00),
    "H1": (3.50, 3.50),
    "H2": (66.50, 3.50),
    "H3": (3.50, 86.50),
    "H4": (66.50, 86.50),
}

HOLE_FOOTPRINT = "MountingHole:MountingHole_3.2mm_M3"

# Routing order: most constrained first.  IR/BTN travel the whole board to
# the top-left, so they must claim their corridor before the short nets.
SIGNAL_NETS = [
    "IR_SIG", "BTN", "+3V3",
    "LED_DATA_LV", "LED_DATA_BUF", "LED_DATA",
    "SD_CS", "SD_SCK", "SD_MOSI", "SD_MISO", "SD_VCC",
]

NET_WIDTH = {
    "SD_CS": 0.30, "SD_SCK": 0.30, "SD_MOSI": 0.30, "SD_MISO": 0.30,
    "SD_VCC": 0.40,
    "LED_DATA_LV": 0.30, "LED_DATA_BUF": 0.30, "LED_DATA": 0.30,
    "IR_SIG": 0.30, "BTN": 0.30, "+3V3": 0.50,
}

# GPIO pins that carry signals: routed to their local header/level shifter.
# The pair list is (net, pad_ref_pin, pad_ref_pin).
NET_PINS = {
    "SD_CS":   [("J2", "11"), ("J3", "1")],
    "SD_SCK":  [("J2", "8"), ("J3", "2")],
    "SD_MOSI": [("J2", "10"), ("J3", "3")],
    "SD_MISO": [("J2", "9"), ("J3", "4")],
    "SD_VCC":  [("J3", "5"), ("JP1", "2")],
    "LED_DATA_LV": [("J2", "3"), ("U2", "2")],
    "LED_DATA_BUF": [("U2", "4"), ("R1", "1")],
    "LED_DATA": [("R1", "2"), ("J6", "2")],
    "IR_SIG":  [("J2", "4"), ("J4", "3")],
    "BTN":     [("J2", "2"), ("J5", "1")],
    "+3V3":    [("J2", "6"), ("JP1", "1"), ("J4", "2")],
}

ANTENNA_KEEPOUT = (10.0, 33.0, 40.5, 41.0)  # x0, y0, x1, y1


# --------------------------------------------------------------------------
# router grid
# --------------------------------------------------------------------------

STEP = 0.254
NX = int(round(BOARD_W / STEP)) + 1
NY = int(round(BOARD_H / STEP)) + 1
F_CU, B_CU = 0, 1
DIRS = ((1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, 1.4142), (1, -1, 1.4142), (-1, 1, 1.4142), (-1, -1, 1.4142))
# Prefer the top layer so the bottom stays a mostly solid ground plane.
LAYER_COST = (1.0, 1.40)


def cell(x, y):
    return (int(round(x / STEP)), int(round(y / STEP)))


def gidx(i, j):
    return j * NX + i


BLOCK_MARGIN = 0.50


def blank_grid():
    g = bytearray(NX * NY)
    edge = EDGE_MARGIN + BLOCK_MARGIN
    for j in range(NY):
        y = j * STEP
        if y < edge or y > BOARD_H - edge:
            g[j * NX:(j + 1) * NX] = b"\x01" * NX
            continue
        row = j * NX
        for i in range(NX):
            x = i * STEP
            if x < edge or x > BOARD_W - edge:
                g[row + i] = 1
    x0, y0, x1, y1 = ANTENNA_KEEPOUT
    x0 -= BLOCK_MARGIN
    y0 -= BLOCK_MARGIN
    x1 += BLOCK_MARGIN
    y1 += BLOCK_MARGIN
    for j in range(NY):
        y = j * STEP
        if y0 <= y <= y1:
            row = j * NX
            for i in range(NX):
                if x0 <= i * STEP <= x1:
                    g[row + i] = 1
    return g


def mark_pad(g, px, py, hw, hh, extra):
    hw += extra
    hh += extra
    i0 = max(0, int(math.floor((px - hw) / STEP)))
    i1 = min(NX - 1, int(math.ceil((px + hw) / STEP)))
    j0 = max(0, int(math.floor((py - hh) / STEP)))
    j1 = min(NY - 1, int(math.ceil((py + hh) / STEP)))
    for j in range(j0, j1 + 1):
        if abs(j * STEP - py) > hh:
            continue
        row = j * NX
        for i in range(i0, i1 + 1):
            if abs(i * STEP - px) <= hw:
                g[row + i] = 1


def mark_segment(g, x1, y1, x2, y2, r):
    i0 = max(0, int(math.floor((min(x1, x2) - r) / STEP)) - 1)
    i1 = min(NX - 1, int(math.ceil((max(x1, x2) + r) / STEP)) + 1)
    j0 = max(0, int(math.floor((min(y1, y2) - r) / STEP)) - 1)
    j1 = min(NY - 1, int(math.ceil((max(y1, y2) + r) / STEP)) + 1)
    dx, dy = x2 - x1, y2 - y1
    length2 = dx * dx + dy * dy
    for j in range(j0, j1 + 1):
        y = j * STEP
        row = j * NX
        for i in range(i0, i1 + 1):
            x = i * STEP
            if length2 < 1e-12:
                d = math.hypot(x - x1, y - y1)
            else:
                t = ((x - x1) * dx + (y - y1) * dy) / length2
                t = 0.0 if t < 0 else (1.0 if t > 1 else t)
                d = math.hypot(x - (x1 + t * dx), y - (y1 + t * dy))
            if d <= r:
                g[row + i] = 1


class Router:
    def __init__(self, board, pad_entries):
        self.board = board
        self.pad_entries = pad_entries  # list of dicts
        self.tracks = []                # dicts: net, layer, x1,y1,x2,y2, width
        self.vias = []                  # dicts: net, x, y

    def add_track(self, net, layer, x1, y1, x2, y2, width):
        self.tracks.append(dict(net=net, layer=layer, x1=x1, y1=y1,
                                x2=x2, y2=y2, width=width))

    def add_via(self, net, x, y):
        self.vias.append(dict(net=net, x=x, y=y))

    def build_grid(self, net, width):
        base = blank_grid()
        # A placed via is 0.30 mm in radius; the new path may become a via, so
        # every obstacle must keep enough room for that as well.
        infl = 0.20 + max(width / 2.0, 0.30)
        for p in self.pad_entries:
            if p["net"] == net and p["net"]:
                continue
            mark_pad(base, p["x"], p["y"], p["hw"], p["hh"], infl)
        gF = bytearray(base)
        gB = bytearray(base)
        for t in self.tracks:
            if t["net"] == net:
                continue
            target = gF if t["layer"] == F_CU else gB
            mark_segment(target, t["x1"], t["y1"], t["x2"], t["y2"],
                         t["width"] / 2.0 + infl)
        for v in self.vias:
            if v["net"] == net:
                continue
            mark_pad(gF, v["x"], v["y"], 0.30, 0.30, infl)
            mark_pad(gB, v["x"], v["y"], 0.30, 0.30, infl)
        # Vias must not sit on top of drilled pads (hole-to-hole rule), even
        # pads of the same net.  Allow some breathing room for their drills.
        gVia = bytearray(NX * NY)
        for p in self.pad_entries:
            if p["smd"]:
                continue
            mark_pad(gVia, p["x"], p["y"], p["hw"], p["hh"], 0.70)
        return (gF, gB, gVia)

    def astar(self, grid, start, goals, via_cost=10.0):
        """grid: (gF, gB, gVia); start/goals: (layer, i, j) tuples."""
        gF, gB, gVia = grid
        grids = (gF, gB)
        goal_set = {(l, i, j) for l, i, j in goals}

        hx = [g[1] * STEP for g in goals]
        hy = [g[2] * STEP for g in goals]

        def heur(i, j):
            x, y = i * STEP, j * STEP
            return min(max(abs(x - ax), abs(y - ay)) +
                       0.4142 * min(abs(x - ax), abs(y - ay))
                       for ax, ay in zip(hx, hy)) / STEP

        start_node = start[0] * (NX * NY) + gidx(start[1], start[2])
        gscore = {start_node: 0.0}
        came = {}
        open_heap = [(heur(start[1], start[2]), 0.0, start_node)]
        closed = set()

        while open_heap:
            _, cost, node = heapq.heappop(open_heap)
            if node in closed:
                continue
            closed.add(node)
            layer, idx = divmod(node, NX * NY)
            i, j = idx % NX, idx // NX
            if (layer, i, j) in goal_set:
                return self._rebuild(came, node, start_node)
            for di, dj, step_cost in DIRS:
                ni, nj = i + di, j + dj
                if not (0 <= ni < NX and 0 <= nj < NY):
                    continue
                if grids[layer][gidx(ni, nj)]:
                    continue
                if di and dj:
                    if grids[layer][gidx(i + di, j)] or grids[layer][gidx(i, j + dj)]:
                        continue
                nnode = layer * (NX * NY) + gidx(ni, nj)
                if nnode in closed:
                    continue
                ncost = cost + step_cost * LAYER_COST[layer]
                if ncost < gscore.get(nnode, 1e18):
                    gscore[nnode] = ncost
                    came[nnode] = node
                    heapq.heappush(open_heap, (ncost + heur(ni, nj), ncost, nnode))
            # via
            other = 1 - layer
            if not grids[other][idx] and not gVia[idx]:
                nnode = other * (NX * NY) + idx
                if nnode not in closed:
                    ncost = cost + via_cost
                    if ncost < gscore.get(nnode, 1e18):
                        gscore[nnode] = ncost
                        came[nnode] = node
                        heapq.heappush(open_heap, (ncost + heur(i, j), ncost, nnode))
        return None

    @staticmethod
    def _rebuild(came, node, start_node):
        path = [node]
        while node != start_node:
            node = came[node]
            path.append(node)
        path.reverse()
        out = []
        for n in path:
            layer, idx = divmod(n, NX * NY)
            out.append((layer, idx % NX, idx // NX))
        return out

    def route(self, net, pins, width, pad_lookup):
        legal = {"F": F_CU, "B": B_CU}
        points = []
        for ref, pin in pins:
            entry = pad_lookup[(ref, pin)]
            points.append(entry)

        # Tree routing: first pad to each following pad.
        for k in range(1, len(points)):
            a = points[k - 1]
            b = points[k]
            layers_a = [F_CU] if a["smd"] else [F_CU, B_CU]
            layers_b = [F_CU] if b["smd"] else [F_CU, B_CU]
            grid = self.build_grid(net, width)
            start = None
            for lay in layers_a:
                ci, cj = cell(a["x"], a["y"])
                if not grid[lay][gidx(ci, cj)]:
                    start = (lay, ci, cj)
                    break
            if start is None:
                ci, cj = cell(a["x"], a["y"])
                start = (layers_a[0], ci, cj)
            goals = []
            for lay in layers_b:
                ci, cj = cell(b["x"], b["y"])
                goals.append((lay, ci, cj))
            path = self.astar(grid, start, goals)
            if path is None:
                raise RuntimeError(f"router failed for {net} {a['ref']}.{a['pin']} -> {b['ref']}.{b['pin']}")
            self._emit(net, path, width, a, b)
        return True

    def _emit(self, net, path, width, a, b):
        runs = []
        run_start = path[0]
        cur = path[0]
        prev_dir = None
        for node in path[1:]:
            if node[0] != cur[0]:
                runs.append(("seg", run_start, cur))
                runs.append(("via", cur[1], cur[2]))
                run_start = node
                prev_dir = None
                cur = node
                continue
            direction = (node[1] - cur[1], node[2] - cur[2])
            if direction != prev_dir:
                if prev_dir is not None:
                    runs.append(("seg", run_start, cur))
                run_start = cur
                prev_dir = direction
            cur = node
        runs.append(("seg", run_start, cur))

        # Convert to millimetres and pin the ends exactly on the pads.
        out = []
        for run in runs:
            if run[0] == "via":
                out.append(("via", run[1] * STEP, run[2] * STEP))
            else:
                _, p, q = run
                out.append(("seg", p[0], p[1] * STEP, p[2] * STEP,
                            q[1] * STEP, q[2] * STEP))

        # Snap first and last endpoints.
        for idx, run in enumerate(out):
            if run[0] != "seg":
                continue
            if idx == 0:
                out[idx] = ("seg", run[1], a["x"], a["y"], run[4], run[5])
            break
        for idx in range(len(out) - 1, -1, -1):
            run = out[idx]
            if run[0] != "seg":
                continue
            out[idx] = ("seg", run[1], run[2], run[3], b["x"], b["y"])
            break

        for run in out:
            if run[0] == "seg":
                _, layer, x1, y1, x2, y2 = run
                if abs(x1 - x2) < 1e-9 and abs(y1 - y2) < 1e-9:
                    continue
                self.add_track(net, layer, x1, y1, x2, y2, width)
            else:
                self.add_via(net, run[1], run[2])


# --------------------------------------------------------------------------
# board assembly
# --------------------------------------------------------------------------

def load_footprint(lib_id: str):
    lib, name = lib_id.split(":", 1)
    if lib == "LEDMatrix":
        directory = HERE / "footprints" / "LEDMatrix.pretty"
    else:
        directory = SHARE / (lib + ".pretty")
    fp = pcbnew.FootprintLoad(str(directory), name)
    if fp is None:
        raise FileNotFoundError(f"footprint {lib_id} not found in {directory}")
    return fp


def add_track(board, net, layer, x1, y1, x2, y2, width):
    t = pcbnew.PCB_TRACK(board)
    t.SetStart(V(x1, y1))
    t.SetEnd(V(x2, y2))
    t.SetWidth(FMM(width))
    t.SetLayer(pcbnew.F_Cu if layer == F_CU else pcbnew.B_Cu)
    t.SetNet(net)
    board.Add(t)


def add_via(board, net, x, y, size=0.60, drill=0.30):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(V(x, y))
    v.SetWidth(FMM(size))
    v.SetDrill(FMM(drill))
    v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
    v.SetNet(net)
    board.Add(v)


def polygon_chain(points):
    chain = pcbnew.SHAPE_LINE_CHAIN()
    for x, y in points:
        chain.Append(V(x, y))
    chain.SetClosed(True)
    return chain


def add_zone(board, net, layer, points, clearance=0.30, thermal_gap=0.45,
             spoke=0.80):
    z = pcbnew.ZONE(board)
    z.SetLayer(pcbnew.F_Cu if layer == F_CU else pcbnew.B_Cu)
    z.SetNet(net)
    z.SetLocalClearance(FMM(clearance))
    z.SetMinThickness(FMM(0.25))
    z.SetThermalReliefGap(FMM(thermal_gap))
    z.SetThermalReliefSpokeWidth(FMM(spoke))
    z.SetIslandRemovalMode(pcbnew.ISLAND_REMOVAL_MODE_ALWAYS)
    z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    z.AddPolygon(polygon_chain(points))
    board.Add(z)


def add_rule_area(board, layer, points, name):
    z = pcbnew.ZONE(board)
    z.SetLayer(pcbnew.F_Cu if layer == F_CU else pcbnew.B_Cu)
    z.SetIsRuleArea(True)
    z.SetDoNotAllowTracks(True)
    z.SetDoNotAllowVias(True)
    z.SetDoNotAllowPads(True)
    z.SetDoNotAllowZoneFills(True)
    z.SetDoNotAllowFootprints(False)
    z.SetZoneName(name)
    z.AddPolygon(polygon_chain(points))
    board.Add(z)


def add_silk_text(board, text, x, y, size=0.8, justify="center", layer=None):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(V(x, y))
    t.SetLayer(pcbnew.F_SilkS if layer is None else layer)
    t.SetTextSize(pcbnew.VECTOR2I(FMM(size), FMM(size)))
    t.SetTextThickness(FMM(max(0.10, size * 0.15)))
    align = {
        "center": pcbnew.GR_TEXT_H_ALIGN_CENTER,
        "left": pcbnew.GR_TEXT_H_ALIGN_LEFT,
        "right": pcbnew.GR_TEXT_H_ALIGN_RIGHT,
    }[justify]
    t.SetHorizJustify(align)
    board.Add(t)


def add_edge(board):
    corners = [(0, 0), (BOARD_W, 0), (BOARD_W, BOARD_H), (0, BOARD_H)]
    for a, b in zip(corners, corners[1:] + corners[:1]):
        seg = pcbnew.PCB_SHAPE(board)
        seg.SetShape(pcbnew.SHAPE_T_SEGMENT)
        seg.SetStart(V(*a))
        seg.SetEnd(V(*b))
        seg.SetLayer(pcbnew.Edge_Cuts)
        seg.SetWidth(FMM(0.10))
        board.Add(seg)


def build():
    components, pad_net = load_netlist()
    print("netlist loaded", flush=True)
    board = pcbnew.CreateEmptyBoard()
    board.SetFileName(str(BOARD_FILE))

    ds = board.GetDesignSettings()
    ds.SetBoardThickness(FMM(1.6))
    for attr, value in (
        ("m_MinClearance", 0.20), ("m_TrackMinWidth", 0.25),
        ("m_ViasMinSize", 0.60), ("m_ViasMinDrill", 0.30),
        ("m_HoleToHoleMin", 0.30),
    ):
        try:
            setattr(ds, attr, FMM(value))
        except Exception:
            pass
    try:
        nc = ds.GetDefaultNetclass()
        nc.SetClearance(FMM(0.20))
        nc.SetTrackWidth(FMM(0.30))
        nc.SetViaDiameter(FMM(0.60))
        nc.SetViaDrill(FMM(0.30))
    except Exception:
        pass

    # Nets from the schematic.
    nets = {}
    for (ref, pin), name in pad_net.items():
        if name not in nets:
            nets[name] = pcbnew.NETINFO_ITEM(board, name)
            board.Add(nets[name])

    pad_lookup = {}
    pad_entries = []

    for ref, pos in POSITIONS.items():
        if ref in components:
            info = components[ref]
            lib_id = info["footprint"]
            value = info["value"]
            uuid = info["uuid"]
        else:
            lib_id = HOLE_FOOTPRINT
            value = "M3"
            uuid = ""
        fp = load_footprint(lib_id)
        fp.SetReference(ref)
        fp.SetValue(value)
        fp.SetPosition(V(*pos))
        fp.SetOrientationDegrees(0.0)
        if ref.startswith("H"):
            fp.Reference().SetVisible(False)
        if uuid:
            try:
                fp.SetPath(pcbnew.KIID_PATH("/" + uuid))
            except Exception:
                pass
        board.Add(fp)

        for pad in fp.Pads():
            net_name = pad_net.get((ref, pad.GetNumber()))
            if net_name:
                pad.SetNet(nets[net_name])
                if net_name == "+5V":
                    # The +5 V pour only covers the power corner; connect the
                    # few pads it reaches solidly instead of with thermals.
                    pad.SetLocalZoneConnection(pcbnew.ZONE_CONNECTION_FULL)
            p = pad.GetPosition()
            size = pad.GetSize()
            x, y = TMM(p.x), TMM(p.y)
            shape = pad.GetShape()
            if shape == pcbnew.PAD_SHAPE_CIRCLE:
                hw = hh = TMM(size.x) / 2.0
            else:
                hw, hh = TMM(size.x) / 2.0, TMM(size.y) / 2.0
            entry = dict(ref=ref, pin=pad.GetNumber(), x=x, y=y, hw=hw, hh=hh,
                         net=net_name or "", smd=(pad.GetAttribute() == pcbnew.PAD_ATTRIB_SMD))
            pad_entries.append(entry)
            if pad.GetNumber():
                pad_lookup[(ref, pad.GetNumber())] = entry

    add_edge(board)
    print("footprints placed", flush=True)

    router = Router(board, pad_entries)

    # Wide +5 V link between the input terminal and the matrix terminal, plus
    # a short link from the SD VCC selector into the +5 V rail.
    router.add_track("+5V", F_CU, 48.0, 84.0, 48.0, 66.0, 1.50)
    router.route("+5V", [("JP1", "3"), ("J6", "1")], 0.60, pad_lookup)

    # Level shifter / decoupling ground drops to the B.Cu plane.
    for x, y, vx, vy in ((50.8625, 51.05, 50.8625, 49.6),
                         (50.8625, 52.95, 50.8625, 54.4),
                         (56.95, 48.0, 58.3, 48.0)):
        router.add_track("GND", F_CU, x, y, vx, vy, 0.40)
        router.add_via("GND", vx, vy)

    # Signal routing.
    ordered = []
    for name in SIGNAL_NETS:
        schematic_name = "/" + name.lower()
        if name == "+3V3":
            schematic_name = "/+3v3"
        ordered.append((name, schematic_name))
    failures = []
    for name, _schematic in ordered:
        try:
            router.route(name, NET_PINS[name], NET_WIDTH[name], pad_lookup)
        except RuntimeError as exc:
            failures.append(str(exc))
            print("ROUTE FAILED:", exc)

    for t in router.tracks:
        add_track(board, nets[t["net"]], t["layer"], t["x1"], t["y1"],
                  t["x2"], t["y2"], t["width"])
    for v in router.vias:
        add_via(board, nets[v["net"]], v["x"], v["y"])

    # Copper pours. +5 V only in the power corner, GND over the whole B.Cu.
    add_zone(board, nets["+5V"], F_CU,
             [(43.0, 40.0), (69.6, 40.0), (69.6, 89.6), (43.0, 89.6)],
             clearance=0.30, thermal_gap=0.45, spoke=0.90)
    add_zone(board, nets["GND"], B_CU,
             [(0.45, 0.45), (69.55, 0.45), (69.55, 89.55), (0.45, 89.55)],
             clearance=0.30, thermal_gap=0.40, spoke=0.70)

    # ESP8266 antenna keep-out, both copper layers.
    for layer in (F_CU, B_CU):
        add_rule_area(board, layer,
                      [(ANTENNA_KEEPOUT[0], ANTENNA_KEEPOUT[1]),
                       (ANTENNA_KEEPOUT[2], ANTENNA_KEEPOUT[1]),
                       (ANTENNA_KEEPOUT[2], ANTENNA_KEEPOUT[3]),
                       (ANTENNA_KEEPOUT[0], ANTENNA_KEEPOUT[3])],
                      "ESP8266 antenna keep-out")

    # Silkscreen.
    add_silk_text(board, "2D LED MATRIX V1", 30.0, 6.0, 1.3)
    add_silk_text(board, "NODEMCU LoLin V3", 26.5, 41.0, 0.8)
    add_silk_text(board, "ANTENNA KEEP-OUT", 25.5, 35.5, 0.8)

    left = ["A0", "GND", "VU", "S3", "S2", "S1", "SC", "S0", "SK", "GND",
            "3V3", "EN", "RST", "GND", "VIN"]
    right = ["D0", "D1", "D2", "D3", "D4", "3V3", "GND", "D5", "D6", "D7",
             "D8", "RX", "TX", "GND", "3V3"]
    for i, label in enumerate(left):
        add_silk_text(board, label, 15.5, 44.0 + 2.54 * i, 0.8, "left")
    for i, label in enumerate(right):
        add_silk_text(board, label, 37.9, 44.0 + 2.54 * i, 0.8, "right")

    add_silk_text(board, "IR", 9.8, 12.5, 0.8, "left")
    add_silk_text(board, "GND 3V3 SIG", 6.0, 18.2, 0.8)
    add_silk_text(board, "BTN", 21.6, 9.3, 0.8, "left")
    add_silk_text(board, "GND", 21.6, 15.6, 0.8, "left")
    add_silk_text(board, "MATRIX", 53.08, 74.0, 0.8)
    add_silk_text(board, "+5V DATA GND", 53.08, 76.4, 0.8)
    add_silk_text(board, "RED = +5V", 43.2, 55.4, 0.8, "left")
    add_silk_text(board, "GREEN = DATA", 43.2, 57.2, 0.8, "left")
    add_silk_text(board, "WHITE = GND", 43.2, 59.0, 0.8, "left")
    add_silk_text(board, "5V INPUT", 43.6, 84.0, 0.8, "right")

    pcbnew.SaveBoard(str(BOARD_FILE), board)
    print("saved unfilled board", flush=True)
    filler = pcbnew.ZONE_FILLER(board)
    if not filler.Fill(board.Zones()):
        print("zone fill returned false", flush=True)
    pcbnew.SaveBoard(str(BOARD_FILE), board)

    print(f"wrote {BOARD_FILE}")
    print(f"footprints: {len(board.GetFootprints())}  "
          f"tracks: {len(board.GetTracks())}  zones: {len(board.Zones())}")
    if failures:
        raise SystemExit("routing failures:\n  " + "\n  ".join(failures))


if __name__ == "__main__":
    build()
