"""Deterministic, stdlib-only Landscape source for the 2026-11-12 layout.

This module never imports Unreal or edits assets. All coordinates supplied to
height_at are Unreal centimetres. The retained JSON remains the graph authority.
Numeric verification describes the authored heightfield, not a runtime playtest.
"""

from __future__ import annotations

import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import sys
import time


PROJECT_ROOT = Path(__file__).resolve().parents[1]
LAYOUT_PATH = PROJECT_ROOT / "Docs/References/WorldMap/world-layout-20261112.json"
OUTPUT_DIR = PROJECT_ROOT / "Feature/doc/evidence/landscape-world-blockout-20261112"
HEIGHTMAP_SIZE = 1009
WORLD_SIZE_CM = 180000.0
WORLD_HALF_CM = WORLD_SIZE_CM / 2.0
HEIGHTMAP_SPACING_CM = WORLD_SIZE_CM / (HEIGHTMAP_SIZE - 1)
LANDSCAPE_ORIGIN = (-90000.0, -90000.0, 0.0)
LANDSCAPE_Z_SCALE = 256.0
FIELD_ANCHOR_CM = (15000.0, 32550.0)
BOSS_CENTER_CM = (15000.0, 68000.0)
BOSS_GRADE_CM = 9600.0
BOSS_RADIUS_CM = 5500.0
ROAD_WALK_HALF_WIDTH_CM = 400.0
# Extra graded shoulders keep the 8 m lane inside complete Landscape cells.
ROAD_GRADED_HALF_WIDTH_CM = 700.0
ROAD_FEATHER_CM = 1200.0
MAX_CONSTRUCTION_GRADIENT = 0.198
MAX_ROAD_SLOPE_DEGREES = 12.0
PAD_GRADE_SMOOTH_CM = 240.0
_BUCKET_CM = 4000.0

_SOURCE_BYTES = LAYOUT_PATH.read_bytes()
_LAYOUT = json.loads(_SOURCE_BYTES.decode("utf-8-sig"))
_NODE_SOURCES = {n["id"]: n for n in _LAYOUT["nodes"]}


def _smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3.0 - 2.0 * t)


def _mix(a, b, t):
    return a + (b - a) * t


def _pad_margin(distance_cm):
    return max(0.0, distance_cm)


def _smooth_positive(value, radius):
    if value <= -radius:
        return 0.0
    if value >= radius:
        return value
    return (value + radius) ** 2 / (4.0 * radius)


def _smooth_clamp(value, lower, upper):
    # Its three partial derivatives are nonnegative and sum to one: smoothing
    # the clamp preserves the source gradient bound and is exact for flat pads.
    return lower + _smooth_positive(value - lower, PAD_GRADE_SMOOTH_CM) - _smooth_positive(value - upper, PAD_GRADE_SMOOTH_CM)


def local_to_world(x_m, y_m):
    return FIELD_ANCHOR_CM[0] + float(x_m) * 100.0, FIELD_ANCHOR_CM[1] + float(y_m) * 100.0


def world_to_local(x_cm, y_cm):
    return (float(x_cm) - FIELD_ANCHOR_CM[0]) / 100.0, (float(y_cm) - FIELD_ANCHOR_CM[1]) / 100.0


def grade_at_local_y(y_m):
    """Central grade: flat start, <=9.57 degree ascent, flat boss approach.

    The integrated smoothstep derivative is zero below 10 m and above 298 m.
    Its maximum derivative is 4600 / 27300, rather than a steep cubic ramp.
    """
    y = float(y_m)
    if y <= 10.0:
        area_m = 0.0
    elif y < 25.0:
        t = (y - 10.0) / 15.0
        area_m = 15.0 * (t ** 3 - 0.5 * t ** 4)
    elif y <= 283.0:
        area_m = 7.5 + y - 25.0
    elif y < 298.0:
        t = (y - 283.0) / 15.0
        area_m = 265.5 + 15.0 * (t - t ** 3 + 0.5 * t ** 4)
    else:
        area_m = 273.0
    return 5000.0 + 4600.0 * area_m / 273.0


def grade_at_local(x_m, y_m=None):
    """Return base grade in cm; accepts y, (x,y), or x,y in local metres."""
    if y_m is None:
        y_m = x_m[1] if isinstance(x_m, (list, tuple)) else x_m
    return grade_at_local_y(y_m)


def _node_xy(source):
    p = source["positionM"]
    return local_to_world(p["x"], p["y"])


def _make_segments():
    result = []
    for source in _LAYOUT["edges"]:
        a, b = _node_xy(_NODE_SOURCES[source["from"]]), _node_xy(_NODE_SOURCES[source["to"]])
        dx, dy = b[0] - a[0], b[1] - a[1]
        length = math.hypot(dx, dy)
        if length <= 0.0:
            raise ValueError("Zero length road: " + source["id"])
        result.append({"id": source["id"], "from": source["from"], "to": source["to"],
                       "role": source.get("role", "optional"), "a_xy": a, "b_xy": b,
                       "dx": dx, "dy": dy, "length_cm": length, "inverse_sq": 1.0 / (length * length),
                       "bidirectional": source["bidirectional"]})
    return tuple(result)


_SEGMENTS = _make_segments()


def _make_buckets():
    buckets = {}
    margin = ROAD_GRADED_HALF_WIDTH_CM + ROAD_FEATHER_CM
    for segment in _SEGMENTS:
        ax, ay = segment["a_xy"]
        bx, by = segment["b_xy"]
        for ix in range(math.floor((min(ax, bx) - margin) / _BUCKET_CM), math.floor((max(ax, bx) + margin) / _BUCKET_CM) + 1):
            for iy in range(math.floor((min(ay, by) - margin) / _BUCKET_CM), math.floor((max(ay, by) + margin) / _BUCKET_CM) + 1):
                buckets.setdefault((ix, iy), []).append(segment)
    return {key: tuple(value) for key, value in buckets.items()}


_ROAD_BUCKETS = _make_buckets()


def distance_to_roads(x_cm, y_cm):
    """Nearest road distance, capped at the outer graded shoulder radius."""
    x, y = float(x_cm), float(y_cm)
    best_sq = (ROAD_GRADED_HALF_WIDTH_CM + ROAD_FEATHER_CM) ** 2
    for s in _ROAD_BUCKETS.get((math.floor(x / _BUCKET_CM), math.floor(y / _BUCKET_CM)), ()):
        ax, ay = s["a_xy"]
        t = min(1.0, max(0.0, ((x - ax) * s["dx"] + (y - ay) * s["dy"]) * s["inverse_sq"]))
        best_sq = min(best_sq, (x - ax - t * s["dx"]) ** 2 + (y - ay - t * s["dy"]) ** 2)
    return math.sqrt(best_sq)


def _make_pads():
    pads = []
    for source in _LAYOUT["nodes"]:
        kind, node_id = source["kind"], source["id"]
        radius = {"major_hub": 3500.0, "outpost": 1800.0, "optional_mid_boss": 1800.0}.get(kind, 0.0)
        if node_id == "S":
            radius = 1000.0
        if node_id == "B":
            radius = 200.0
        if node_id == "T":
            radius = 3400.0
        if not radius:
            continue
        x, y = _node_xy(source)
        pads.append({"id": node_id, "x": x, "y": y, "radius_cm": radius,
                     "z": grade_at_local_y(source["positionM"]["y"]), "fixed": node_id in ("S", "B")})
    pads.append({"id": "FINAL_ARENA", "x": BOSS_CENTER_CM[0], "y": BOSS_CENTER_CM[1],
                 "radius_cm": BOSS_RADIUS_CM, "z": BOSS_GRADE_CM, "fixed": True})
    # A flat disk on a slope needs a gradual shoulder. Pairwise cone bounds
    # ensure a continuous <=0.20 rise/run surface between every pair of pads.
    # The corridor interval prevents any pad from cutting into the direct lane.
    for p in pads:
        lo, hi = -math.inf, math.inf
        for step in range(-40, 761):
            local_y = step * 0.5
            x = FIELD_ANCHOR_CM[0] + math.copysign(ROAD_GRADED_HALF_WIDTH_CM, p["x"] - FIELD_ANCHOR_CM[0])
            y = FIELD_ANCHOR_CM[1] + local_y * 100.0
            distance = _pad_margin(math.hypot(x - p["x"], y - p["y"]) - p["radius_cm"])
            z = grade_at_local_y(local_y)
            lo, hi = max(lo, z - MAX_CONSTRUCTION_GRADIENT * distance), min(hi, z + MAX_CONSTRUCTION_GRADIENT * distance)
        if lo > hi + 1e-5:
            raise ValueError("Pad cannot preserve direct corridor: " + p["id"])
        p["allowed_z_cm"] = (lo, hi)
        p["z"] = min(hi, max(lo, p["z"]))
    for iteration in range(5000):
        largest = 0.0
        for i, a in enumerate(pads):
            for b in pads[i + 1:]:
                limit = MAX_CONSTRUCTION_GRADIENT * max(0.0, math.hypot(a["x"] - b["x"], a["y"] - b["y"]) - a["radius_cm"] - b["radius_cm"])
                excess = abs(a["z"] - b["z"]) - limit
                if excess <= 1e-7:
                    continue
                largest = max(largest, excess)
                sign = 1.0 if a["z"] > b["z"] else -1.0
                if a["fixed"] and b["fixed"]:
                    raise ValueError("Conflicting fixed pads")
                shift_a = 0.0 if a["fixed"] else excess if b["fixed"] else excess * 0.5
                shift_b = 0.0 if b["fixed"] else excess if a["fixed"] else excess * 0.5
                a["z"] -= sign * shift_a
                b["z"] += sign * shift_b
                for p in (a, b):
                    lo, hi = p["allowed_z_cm"]
                    p["z"] = min(hi, max(lo, p["z"]))
        if largest < 1e-5:
            break
    else:
        raise ValueError("Flat pad grade solver failed to converge")
    return tuple(pads)


_PADS = _make_pads()
_PAD_FIELDS = tuple((p["x"], p["y"], p["radius_cm"], p["z"]) for p in _PADS)
_XS = [p["x"] for p in _PADS]
_YS = [p["y"] for p in _PADS]
_ACTIVE_BOX = (min(_XS) - 7000.0, max(_XS) + 7000.0, min(_YS) - 7000.0,
               max(max(_YS), BOSS_CENTER_CM[1] + BOSS_RADIUS_CM) + 7000.0)


def _legacy_base_height(x, y):
    """Old OpenWorldLayout low-frequency base only; no old roads/river/POIs."""
    height = 3200.0 + 650.0 * math.sin(x / 16500.0) * math.cos(y / 19000.0) + 420.0 * math.sin((x + y) / 21000.0)
    height += 4000.0 * math.exp(-(((x + 35000.0) / 27000.0) ** 2 + ((y - 42000.0) / 24000.0) ** 2))
    height += 6100.0 * math.exp(-(((x - 15000.0) / 32000.0) ** 2 + ((y - 68000.0) / 30000.0) ** 2))
    edge_weight = _smooth((max(abs(x), abs(y)) - 66000.0) / 24000.0)
    return _mix(height, 11000.0 + 3000.0 * math.sin(x / 24000.0 + y / 35000.0), edge_weight)


def height_at(x, y):
    """Authored Landscape height in cm. No engine, file writes, or random state."""
    x, y = float(x), float(y)
    if not (math.isfinite(x) and math.isfinite(y)):
        raise ValueError("Nonfinite world coordinate")
    xmin, xmax, ymin, ymax = _ACTIVE_BOX
    outside = math.hypot(max(xmin - x, 0.0, x - xmax), max(ymin - y, 0.0, y - ymax))
    if outside >= 16000.0:
        return _legacy_base_height(x, y)
    base = grade_at_local_y((y - FIELD_ANCHOR_CM[1]) / 100.0)
    relief_weight = _smooth((distance_to_roads(x, y) - ROAD_GRADED_HALF_WIDTH_CM) / ROAD_FEATHER_CM)
    relief = 70.0 * math.sin(x / 8500.0) * math.sin(y / 11000.0) * relief_weight
    lower, upper = -math.inf, math.inf
    for px, py, radius, z in _PAD_FIELDS:
        margin = MAX_CONSTRUCTION_GRADIENT * _pad_margin(math.hypot(x - px, y - py) - radius)
        lower, upper = max(lower, z - margin), min(upper, z + margin)
    result = _smooth_clamp(base + relief, lower, upper)
    if outside > 0.0:
        result = _mix(result, _legacy_base_height(x, y), _smooth(outside / 16000.0))
    return result


def nodes_world():
    """Return a fresh ID->node dict; x/y/z and world_cm are in centimetres."""
    result = {}
    for source in _LAYOUT["nodes"]:
        x, y = _node_xy(source)
        z = height_at(x, y)
        result[source["id"]] = {"id": source["id"], "label": source["label"], "kind": source["kind"],
                                "x": x, "y": y, "z": z, "world_cm": [x, y, z],
                                "positionM": dict(source["positionM"])}
    return result


def roadsegments():
    """Return all 29 source edges as dictionaries with world a/b endpoints."""
    nodes = nodes_world()
    return tuple({"id": s["id"], "from": s["from"], "to": s["to"], "role": s["role"],
                  "a": list(nodes[s["from"]]["world_cm"]), "b": list(nodes[s["to"]]["world_cm"]),
                  "length_cm": s["length_cm"], "bidirectional": s["bidirectional"]} for s in _SEGMENTS)


road_segments = roadsegments


def encode_height(z_cm):
    raw = round(float(z_cm) * 128.0 / LANDSCAPE_Z_SCALE + 32768.0)
    if raw < 0 or raw > 65535:
        raise ValueError("Height is outside R16 range: " + str(z_cm))
    return raw


def decode_height(raw):
    if not 0 <= int(raw) <= 65535:
        raise ValueError("Invalid R16 sample")
    return (int(raw) - 32768.0) * LANDSCAPE_Z_SCALE / 128.0


def _guard_output(path, expected_name):
    path = Path(path).resolve()
    expected = (OUTPUT_DIR / expected_name).resolve()
    if not expected.is_relative_to(PROJECT_ROOT.resolve()):
        raise ValueError("Evidence output resolved outside the project")
    if path != expected:
        raise ValueError("Output must be exactly " + str(expected))
    return path


def write_heightmap(path=None):
    """Write only the dedicated evidence R16; return samples and encoding stats."""
    path = _guard_output(path or OUTPUT_DIR / "terrain-height.r16", "terrain-height.r16")
    samples = array("H")
    minimum, maximum, max_error = math.inf, -math.inf, 0.0
    for iy in range(HEIGHTMAP_SIZE):
        y = LANDSCAPE_ORIGIN[1] + iy * HEIGHTMAP_SPACING_CM
        for ix in range(HEIGHTMAP_SIZE):
            x = LANDSCAPE_ORIGIN[0] + ix * HEIGHTMAP_SPACING_CM
            z = height_at(x, y)
            raw = encode_height(z)
            samples.append(raw)
            minimum, maximum = min(minimum, z), max(maximum, z)
            max_error = max(max_error, abs(decode_height(raw) - z))
    output = array("H", samples)
    if sys.byteorder != "little":
        output.byteswap()
    data = output.tobytes()
    if len(data) != HEIGHTMAP_SIZE * HEIGHTMAP_SIZE * 2:
        raise AssertionError("Incorrect R16 byte count")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    return samples, {"path": str(path), "size": HEIGHTMAP_SIZE, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
                     "minimumHeightCm": minimum, "maximumHeightCm": maximum, "maxQuantizationErrorCm": max_error,
                     "endianness": "little", "zScale": LANDSCAPE_Z_SCALE, "originCm": list(LANDSCAPE_ORIGIN),
                     "sampleSpacingCm": HEIGHTMAP_SPACING_CM}


def _grid_height(samples, x, y, diagonal):
    gx, gy = (x - LANDSCAPE_ORIGIN[0]) / HEIGHTMAP_SPACING_CM, (y - LANDSCAPE_ORIGIN[1]) / HEIGHTMAP_SPACING_CM
    ix, iy = min(HEIGHTMAP_SIZE - 2, max(0, math.floor(gx))), min(HEIGHTMAP_SIZE - 2, max(0, math.floor(gy)))
    u, v = gx - ix, gy - iy
    a = decode_height(samples[iy * HEIGHTMAP_SIZE + ix])
    b = decode_height(samples[iy * HEIGHTMAP_SIZE + ix + 1])
    c = decode_height(samples[(iy + 1) * HEIGHTMAP_SIZE + ix])
    d = decode_height(samples[(iy + 1) * HEIGHTMAP_SIZE + ix + 1])
    if diagonal == 0:
        return a + u * (b - a) + v * (d - b) if u >= v else a + u * (d - c) + v * (c - a)
    return a + u * (b - a) + v * (c - a) if u + v <= 1.0 else d + (1.0 - v) * (b - d) + (1.0 - u) * (c - d)


def _proper_intersection(a, b, c, d):
    def cross(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    return cross(a, b, c) * cross(a, b, d) < -1e-6 and cross(c, d, a) * cross(c, d, b) < -1e-6


def _exact_grid_cell_audit(samples):
    """Inspect complete triangles touched by lane probes, beyond average slopes.

    A 100 cm walking profile averages over part of a triangle. This separate
    audit retains steeper short crossfall corners rather than hiding them in
    that average. Complete touched cells may extend outside the 8 m lane.
    """
    longitudinal, crossfall, full_gradient = 0.0, 0.0, 0.0
    touched_cells = 0
    for s in _SEGMENTS:
        tx, ty = s["dx"] / s["length_cm"], s["dy"] / s["length_cm"]
        cells = set()
        count = math.ceil(s["length_cm"] / 100.0)
        for j in range(count + 1):
            for off in (-400.0, -200.0, 0.0, 200.0, 400.0):
                x = s["a_xy"][0] + s["dx"] * j / count - ty * off
                y = s["a_xy"][1] + s["dy"] * j / count + tx * off
                cells.add((math.floor((x - LANDSCAPE_ORIGIN[0]) / HEIGHTMAP_SPACING_CM),
                           math.floor((y - LANDSCAPE_ORIGIN[1]) / HEIGHTMAP_SPACING_CM)))
        touched_cells += len(cells)
        for ix, iy in cells:
            indices = (iy * HEIGHTMAP_SIZE + ix, iy * HEIGHTMAP_SIZE + ix + 1,
                       (iy + 1) * HEIGHTMAP_SIZE + ix, (iy + 1) * HEIGHTMAP_SIZE + ix + 1)
            a, b, c, d = [decode_height(samples[k]) for k in indices]
            for gx, gy in ((b - a, d - b), (d - c, c - a), (b - a, c - a), (d - c, d - b)):
                gx, gy = gx / HEIGHTMAP_SPACING_CM, gy / HEIGHTMAP_SPACING_CM
                longitudinal = max(longitudinal, math.degrees(math.atan(abs(gx * tx + gy * ty))))
                crossfall = max(crossfall, math.degrees(math.atan(abs(-gx * ty + gy * tx))))
                full_gradient = max(full_gradient, math.degrees(math.atan(math.hypot(gx, gy))))
    return {"maximumLongitudinalDegrees": longitudinal, "maximumCrossfallDegrees": crossfall,
            "maximumFullGradientDegrees": full_gradient, "touchedCellCountWithPerEdgeDuplicates": touched_cells,
            "localCrossfallPolishRequiredForGlobal12DegreeLimit": max(crossfall, full_gradient) >= 12.0,
            "scope": "Both diagonals of complete grid cells touched by the five lane probes; includes parts of cells outside the lane.",
            "acceptanceBoundary": "The below-12-degree pass applies to sampled directional road profiles. Individual cell corners require editor inspection/polish for a global slope cap."}


def geometry_report(samples=None):
    """Offline graph and road checks, including both possible grid diagonals."""
    nodes = nodes_world()
    adjacency = {key: set() for key in nodes}
    for s in _SEGMENTS:
        adjacency[s["from"]].add(s["to"])
        adjacency[s["to"]].add(s["from"])
        if not s["bidirectional"]:
            raise AssertionError("One-way road in ungated graph")
    reached, pending = set(), ["S"]
    while pending:
        node = pending.pop()
        if node not in reached:
            reached.add(node)
            pending.extend(adjacency[node] - reached)
    crossings = []
    for i, a in enumerate(_SEGMENTS):
        for b in _SEGMENTS[i + 1:]:
            if {a["from"], a["to"]}.intersection((b["from"], b["to"])):
                continue
            if _proper_intersection(a["a_xy"], a["b_xy"], b["a_xy"], b["b_xy"]):
                crossings.append([a["id"], b["id"]])
    surfaces = [("analytic", height_at)]
    if samples is not None:
        surfaces.extend(("encodedGridDiagonal" + str(diag), lambda x, y, diag=diag: _grid_height(samples, x, y, diag)) for diag in (0, 1))
    surface_reports = {}
    direct_ids = {"S", "J100", "J180", "J240", "B"}
    for name, surface in surfaces:
        edge_reports, direct_length = [], 0.0
        overall_long, overall_cross = 0.0, 0.0
        for s in _SEGMENTS:
            ax, ay = s["a_xy"]
            count = max(1, math.ceil(s["length_cm"] / 100.0))
            step = s["length_cm"] / count
            nx, ny = -s["dy"] / s["length_cm"], s["dx"] / s["length_cm"]
            prev, maximum_long, maximum_cross, length_3d = {}, 0.0, 0.0, 0.0
            for j in range(count + 1):
                t = j / count
                x, y = ax + t * s["dx"], ay + t * s["dy"]
                current = {off: surface(x + nx * off, y + ny * off) for off in (-400.0, -200.0, 0.0, 200.0, 400.0)}
                if prev:
                    for off, z in current.items():
                        maximum_long = max(maximum_long, math.degrees(math.atan2(abs(z - prev[off]), step)))
                    length_3d += math.hypot(step, current[0.0] - prev[0.0])
                for left, right in ((-400.0, -200.0), (-200.0, 0.0), (0.0, 200.0), (200.0, 400.0)):
                    maximum_cross = max(maximum_cross, math.degrees(math.atan2(abs(current[right] - current[left]), right - left)))
                prev = current
            overall_long, overall_cross = max(overall_long, maximum_long), max(overall_cross, maximum_cross)
            edge_reports.append({"id": s["id"], "from": s["from"], "to": s["to"], "sampleIntervalCm": step,
                                 "maximumLongitudinalSlopeDegrees": maximum_long, "maximumCrossSlopeDegrees": maximum_cross,
                                 "surfaceLengthM": length_3d / 100.0})
            if s["from"] in direct_ids and s["to"] in direct_ids:
                direct_length += length_3d
        surface_reports[name] = {"maximumLongitudinalSlopeDegrees": overall_long, "maximumCrossSlopeDegrees": overall_cross,
                                 "directSurfaceLengthM": direct_length / 100.0,
                                 "directNominalAtUnchanged6MpsSeconds": direct_length / 600.0, "edges": edge_reports}
    corridor_error = max(abs(height_at(FIELD_ANCHOR_CM[0] + off, FIELD_ANCHOR_CM[1] + y * 100.0) - grade_at_local_y(y))
                         for y in range(301) for off in (-400.0, 0.0, 400.0))
    pad_errors = {p["id"]: max(abs(height_at(p["x"] + p["radius_cm"] * radius * math.cos(angle),
                                                p["y"] + p["radius_cm"] * radius * math.sin(angle)) - p["z"])
                               for radius in (0.0, 0.5, 1.0) for angle in (i * math.pi / 8 for i in range(16))) for p in _PADS}
    in_bounds = all(abs(n[axis]) + 1900.0 < WORLD_HALF_CM for n in nodes.values() for axis in ("x", "y"))
    checks = {"expected21Nodes": len(nodes) == 21, "expected29Roads": len(_SEGMENTS) == 29,
              "allNodesConnectedFromS": len(reached) == len(nodes), "noNonJunctionCenterlineIntersections": not crossings,
              "allNodesAndRoadMarginsWithinLandscape": in_bounds, "directCorridorBaseGradeDeviationUnderOneMetre": corridor_error < 100.0,
              "allPadsFlatUnderOneMillimetre": max(pad_errors.values()) < 0.1,
              "bossCenterExactly9600Cm": abs(height_at(*BOSS_CENTER_CM) - 9600.0) < 1e-6,
              "allCheckedRoadGradesBelow12Degrees": all(max(r["maximumLongitudinalSlopeDegrees"], r["maximumCrossSlopeDegrees"]) < MAX_ROAD_SLOPE_DEGREES for r in surface_reports.values())}
    return {"checks": checks, "passed": all(checks.values()), "nodeCount": len(nodes), "roadCount": len(_SEGMENTS),
            "nodesWorld": nodes, "padDefinitions": _PADS, "padMaximumHeightErrorCm": pad_errors,
            "directCorridorMaximumBaseGradeErrorCm": corridor_error, "nonJunctionIntersections": crossings,
            "roadWalkWidthCm": ROAD_WALK_HALF_WIDTH_CM * 2.0, "gradedShoulderWidthCm": ROAD_GRADED_HALF_WIDTH_CM * 2.0,
            "featherBeyondGradedHalfWidthCm": ROAD_FEATHER_CM, "surfaces": surface_reports,
            "exactGridCellAudit": _exact_grid_cell_audit(samples) if samples is not None else None,
            "validationBoundary": "Offline authored/quantized heightfield only; Unreal collision, route input, navigation and runtime timing require editor verification.",
            "gameplayMovementSpeedChanged": False}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, default=OUTPUT_DIR, help="Only the canonical feature evidence directory is accepted")
    parser.add_argument("--validate-only", action="store_true", help="Run analytic graph checks without writing files")
    args = parser.parse_args(argv)
    if args.output_dir.resolve() != OUTPUT_DIR.resolve():
        parser.error("Output directory must be " + str(OUTPUT_DIR))
    started = time.monotonic()
    if args.validate_only:
        report = geometry_report()
    else:
        samples, encoding = write_heightmap(args.output_dir / "terrain-height.r16")
        report = geometry_report(samples)
        report["heightmap"] = encoding
    report["layoutSource"] = str(LAYOUT_PATH)
    report["layoutSourceSha256"] = hashlib.sha256(_SOURCE_BYTES).hexdigest()
    report["elapsedSeconds"] = time.monotonic() - started
    report["anchorCm"] = list(FIELD_ANCHOR_CM)
    report["candidateMap"] = "/Game/Rogue10m/Maps/Production/WorldLayout20261112"
    if not args.validate_only:
        destination = _guard_output(args.output_dir / "layout-report.json", "layout-report.json")
        destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"passed": report["passed"], "checks": report["checks"], "elapsedSeconds": report["elapsedSeconds"],
                      "maximumRoadSlopes": {k: [v["maximumLongitudinalSlopeDegrees"], v["maximumCrossSlopeDegrees"]] for k, v in report["surfaces"].items()}}, ensure_ascii=False))
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
