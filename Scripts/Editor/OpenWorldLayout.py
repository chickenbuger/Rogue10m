"""Deterministic OpenRunMap geometry in Unreal centimeters, without Unreal imports.

This module is shared by the editor layout builder and the terrain/foliage
preparation steps. Running it from a normal Python interpreter writes a 1009 x
1009 little-endian R16 heightmap and a compact geometry validation report.
Only files below this project's Feature/doc/evidence directory are written.
"""

from array import array
import argparse
import json
import math
from pathlib import Path
import sys
import time


WORLD_SIZE_CM = 180000.0
WORLD_HALF_CM = WORLD_SIZE_CM * 0.5
HEIGHTMAP_SIZE = 1009
HEIGHTMAP_SPACING_CM = WORLD_SIZE_CM / (HEIGHTMAP_SIZE - 1)
LANDSCAPE_ORIGIN = (-WORLD_HALF_CM, -WORLD_HALF_CM, 0.0)
LANDSCAPE_Z_SCALE = 256.0
ROAD_VISIBLE_WIDTH_CM = 1200.0
ROAD_FLAT_HALF_WIDTH_CM = 1000.0
ROAD_FEATHER_HALF_WIDTH_CM = 2000.0
RIVER_CENTER_Y = 5000.0
RIVER_HALF_LENGTH_CM = 75000.0
RIVER_HALF_WIDTH_CM = 3000.0
RIVER_BED_Z = 2300.0
WATER_SURFACE_Z = 2600.0
RIVER_BANK_Z = 3200.0
BRIDGE_DECK_Z = 3300.0


POIS = {
    "camp": {"name": "Entrance Camp", "x": 0.0, "y": -65000.0,
             "grade_z": 2800.0, "placement_z": 2800.0, "clearance_radius": 8000.0},
    "forest": {"name": "Western Forest", "x": -50000.0, "y": -25000.0,
               "grade_z": 3800.0, "placement_z": 3800.0, "clearance_radius": 6500.0},
    "crossroads": {"name": "River Crossroads", "x": 0.0, "y": RIVER_CENTER_Y,
                   "grade_z": RIVER_BANK_Z, "placement_z": BRIDGE_DECK_Z,
                   "clearance_radius": 7500.0},
    "mill": {"name": "Eastern Old Mill", "x": 50000.0, "y": -5000.0,
             "grade_z": 3500.0, "placement_z": 3500.0, "clearance_radius": 6500.0},
    "highlands": {"name": "Rocky Highlands", "x": -35000.0, "y": 42000.0,
                  "grade_z": 7400.0, "placement_z": 7400.0, "clearance_radius": 7000.0},
    "boss": {"name": "Northern Boss Ruins", "x": 15000.0, "y": 68000.0,
             "grade_z": 9600.0, "placement_z": 9600.0, "clearance_radius": 10000.0},
}

# Authored road elevations are bank grades at river crossings. Actual ground
# under the bridge remains the river bed; callers render bridge decks separately.
ROAD_ROUTES = {
    "main": [(0.0, -65000.0, 2800.0), (0.0, -35000.0, 3000.0),
             (0.0, -4000.0, 3200.0), (0.0, 5000.0, 3200.0),
             (0.0, 14000.0, 3200.0), (0.0, 28000.0, 5200.0),
             (15000.0, 68000.0, 9600.0)],
    "west": [(0.0, -65000.0, 2800.0), (-50000.0, -25000.0, 3800.0),
             (-44000.0, -4000.0, 3200.0), (-44000.0, 5000.0, 3200.0),
             (-44000.0, 14000.0, 3200.0), (-35000.0, 42000.0, 7400.0),
             (15000.0, 68000.0, 9600.0)],
    "east": [(0.0, 14000.0, 3200.0), (30000.0, 18000.0, 3500.0),
             (45000.0, 14000.0, 3200.0), (45000.0, 5000.0, 3200.0),
             (45000.0, -4000.0, 3200.0), (50000.0, -5000.0, 3500.0),
             (34000.0, -35000.0, 3200.0), (0.0, -65000.0, 2800.0)],
}

BRIDGES = [
    {"id": "central", "x": 0.0, "y": RIVER_CENTER_Y},
    {"id": "western", "x": -44000.0, "y": RIVER_CENTER_Y},
    {"id": "eastern", "x": 45000.0, "y": RIVER_CENTER_Y},
]
for _bridge in BRIDGES:
    _bridge.update({"deck_z": BRIDGE_DECK_Z, "bank_z": RIVER_BANK_Z,
                    "width": 1800.0, "length": 12000.0, "ramp_length": 2400.0})


def _smooth(value):
    if value <= 0.0:
        return 0.0
    if value >= 1.0:
        return 1.0
    return value * value * (3.0 - 2.0 * value)


def _mix(a, b, weight):
    return a + (b - a) * weight


def _make_segments():
    result = []
    for route_name, route in ROAD_ROUTES.items():
        for index, (a, b) in enumerate(zip(route, route[1:])):
            ax, ay, az = a
            bx, by, bz = b
            dx, dy = bx - ax, by - ay
            result.append((ax, ay, az, dx, dy, bz - az, 1.0 / (dx * dx + dy * dy),
                           min(ax, bx) - ROAD_FEATHER_HALF_WIDTH_CM,
                           max(ax, bx) + ROAD_FEATHER_HALF_WIDTH_CM,
                           min(ay, by) - ROAD_FEATHER_HALF_WIDTH_CM,
                           max(ay, by) + ROAD_FEATHER_HALF_WIDTH_CM,
                           route_name, index))
    return tuple(result)


_ROAD_SEGMENTS = _make_segments()
_POI_FIELDS = tuple((p["x"], p["y"], p["grade_z"], p["clearance_radius"])
                    for p in POIS.values())


def _distance_to_segment(x, y, segment):
    ax, ay, az, dx, dy, dz, inverse_length_squared = segment[:7]
    t = ((x - ax) * dx + (y - ay) * dy) * inverse_length_squared
    t = 0.0 if t < 0.0 else 1.0 if t > 1.0 else t
    px, py = x - ax - t * dx, y - ay - t * dy
    return px * px + py * py, az + t * dz


def distance_to_roads(x, y):
    """Return the closest XY distance to a road centerline, in centimeters."""
    best = math.inf
    for segment in _ROAD_SEGMENTS:
        distance_squared, _ = _distance_to_segment(float(x), float(y), segment)
        if distance_squared < best:
            best = distance_squared
    return math.sqrt(best)


def in_clearing(x, y, margin=0.0):
    """True for one of the six playable clearings, optionally expanded."""
    for px, py, _, radius in _POI_FIELDS:
        expanded = max(0.0, radius + margin)
        if (x - px) ** 2 + (y - py) ** 2 <= expanded * expanded:
            return True
    return False


def river_distance(x, y):
    """Distance to the finite river centerline, including rounded end caps."""
    dx = max(0.0, abs(float(x)) - RIVER_HALF_LENGTH_CM)
    dy = float(y) - RIVER_CENTER_Y
    return math.hypot(dx, dy) if dx else abs(dy)


def water_mask(x, y, buffer=0.0):
    """True inside the water footprint, optionally expanded for vegetation."""
    return river_distance(x, y) <= max(0.0, RIVER_HALF_WIDTH_CM + buffer)


def legal_foliage(point_or_x, y=None, road_buffer=900.0, clearing_margin=1200.0):
    """Deterministic vegetation exclusion; accepts (x,y[,z]) or x,y.

    Collision-bearing trees and rocks should use the default road clearance or
    a larger one. The implementation permits tuning density independently of
    this legal-position mask.
    """
    if y is None:
        x, y = float(point_or_x[0]), float(point_or_x[1])
    else:
        x, y = float(point_or_x), float(y)
    if abs(x) >= WORLD_HALF_CM - 3000.0 or abs(y) >= WORLD_HALF_CM - 3000.0:
        return False
    if water_mask(x, y, buffer=3500.0) or in_clearing(x, y, clearing_margin):
        return False
    if distance_to_roads(x, y) < max(900.0, road_buffer):
        return False
    return True


def height_at(x, y):
    """Actual Landscape ground height; bridges are separate physical surfaces."""
    x, y = float(x), float(y)
    # Low-frequency relief keeps paths readable and avoids stair-like terrain.
    height = (3200.0 + 650.0 * math.sin(x / 16500.0) * math.cos(y / 19000.0)
              + 420.0 * math.sin((x + y) / 21000.0))
    highland_radius_squared = ((x + 35000.0) / 27000.0) ** 2 + ((y - 42000.0) / 24000.0) ** 2
    boss_radius_squared = ((x - 15000.0) / 32000.0) ** 2 + ((y - 68000.0) / 30000.0) ** 2
    height += 4000.0 * math.exp(-highland_radius_squared) + 6100.0 * math.exp(-boss_radius_squared)
    edge_weight = _smooth((max(abs(x), abs(y)) - 66000.0) / 24000.0)
    edge_target = 11000.0 + 3000.0 * math.sin(x / 24000.0 + y / 35000.0)
    height = _mix(height, edge_target, edge_weight)

    # Wide flat clearing centers plus a 2,000 cm smooth transition ring.
    for px, py, plane_z, radius in _POI_FIELDS:
        dx, dy = x - px, y - py
        feather_radius = radius + 2000.0
        if abs(dx) > feather_radius or abs(dy) > feather_radius:
            continue
        distance_squared = dx * dx + dy * dy
        if distance_squared >= feather_radius * feather_radius:
            continue
        distance = math.sqrt(distance_squared)
        height = _mix(height, plane_z, 1.0 - _smooth((distance - radius) / 2000.0))

    # Blend toward the closest graded road within a broad 40 m corridor.
    best_squared = ROAD_FEATHER_HALF_WIDTH_CM ** 2
    road_z = None
    for segment in _ROAD_SEGMENTS:
        if not (segment[7] <= x <= segment[8] and segment[9] <= y <= segment[10]):
            continue
        distance_squared, grade_z = _distance_to_segment(x, y, segment)
        if distance_squared < best_squared:
            best_squared, road_z = distance_squared, grade_z
    if road_z is not None:
        distance = math.sqrt(best_squared)
        road_weight = 1.0 - _smooth((distance - ROAD_FLAT_HALF_WIDTH_CM)
                                  / (ROAD_FEATHER_HALF_WIDTH_CM - ROAD_FLAT_HALF_WIDTH_CM))
        height = _mix(height, road_z, road_weight)

    # Carve last: a crossing road never creates an invisible dam across water.
    distance = river_distance(x, y)
    if distance <= 1800.0:
        height = RIVER_BED_Z
    elif distance <= RIVER_HALF_WIDTH_CM:
        height = _mix(RIVER_BED_Z, WATER_SURFACE_Z, _smooth((distance - 1800.0) / 1200.0))
    elif distance <= 6000.0:
        height = _mix(WATER_SURFACE_Z, RIVER_BANK_Z, _smooth((distance - 3000.0) / 3000.0))
    elif distance < 8000.0:
        height = _mix(RIVER_BANK_Z, height, _smooth((distance - 6000.0) / 2000.0))
    return height


def bridge_at(x, y, margin=0.0):
    for bridge in BRIDGES:
        if (abs(x - bridge["x"]) <= bridge["width"] * 0.5 + margin
                and abs(y - bridge["y"]) <= bridge["length"] * 0.5 + margin):
            return bridge
    return None


def walkable_height_at(x, y):
    bridge = bridge_at(x, y)
    if bridge:
        return bridge["deck_z"]
    for bridge in BRIDGES:
        along = abs(y - bridge["y"]) - bridge["length"] * 0.5
        if abs(x - bridge["x"]) <= bridge["width"] * 0.5 and 0.0 < along <= bridge["ramp_length"]:
            return _mix(bridge["deck_z"], bridge["bank_z"], along / bridge["ramp_length"])
    return height_at(x, y)


def route_samples(spacing=500.0):
    """Yield route name, XY, terrain Z and bridge-aware walking Z."""
    for name, route in ROAD_ROUTES.items():
        for a, b in zip(route, route[1:]):
            count = max(1, math.ceil(math.hypot(b[0] - a[0], b[1] - a[1]) / spacing))
            for i in range(count):
                t = i / count
                x, y = _mix(a[0], b[0], t), _mix(a[1], b[1], t)
                yield name, x, y, height_at(x, y), walkable_height_at(x, y)
        x, y = route[-1][:2]
        yield name, x, y, height_at(x, y), walkable_height_at(x, y)


def geometry_report():
    reports = {}
    for name, route in ROAD_ROUTES.items():
        length = sum(math.hypot(b[0] - a[0], b[1] - a[1]) for a, b in zip(route, route[1:]))
        authored_slope = max(abs(b[2] - a[2]) / math.hypot(b[0] - a[0], b[1] - a[1])
                             for a, b in zip(route, route[1:]))
        max_sampled_slope, previous = 0.0, None
        for _, x, y, _, walking_z in (s for s in route_samples(250.0) if s[0] == name):
            if previous:
                distance = math.hypot(x - previous[0], y - previous[1])
                if distance > 0.0:
                    max_sampled_slope = max(max_sampled_slope, abs(walking_z - previous[2]) / distance)
            previous = (x, y, walking_z)
        reports[name] = {"length_cm": length, "seconds_at_600_cm_per_second": length / 600.0,
                         "maximum_authored_grade_degrees": math.degrees(math.atan(authored_slope)),
                         "maximum_sampled_walking_grade_degrees": math.degrees(math.atan(max_sampled_slope))}
    return {"world_size_cm": WORLD_SIZE_CM, "heightmap_size": HEIGHTMAP_SIZE,
            "heightmap_spacing_cm": HEIGHTMAP_SPACING_CM, "landscape_origin": LANDSCAPE_ORIGIN,
            "landscape_z_scale": LANDSCAPE_Z_SCALE, "road_visible_width_cm": ROAD_VISIBLE_WIDTH_CM,
            "routes": reports, "bridges": BRIDGES,
            "pois": {name: dict(p, terrain_z=height_at(p["x"], p["y"]),
                                walking_z=walkable_height_at(p["x"], p["y"]))
                     for name, p in POIS.items()},
            "river": {"center_y": RIVER_CENTER_Y, "bed_z": RIVER_BED_Z,
                      "water_z": WATER_SURFACE_Z, "bank_z": RIVER_BANK_Z,
                      "width_cm": RIVER_HALF_WIDTH_CM * 2.0},
            "notes": ["Road samples include physical bridge decks; 100 cm end ramps are authored by the editor builder.",
                      "Crossroads terrain is the river bed; use placement_z for decoration on its bridge."]}


def write_heightmap(output_directory=None):
    project_root = Path(__file__).resolve().parents[2]
    evidence_root = project_root / "Feature" / "doc" / "evidence"
    output = (Path(output_directory) if output_directory else evidence_root / "open-world-20261005").resolve()
    if not output.is_relative_to(evidence_root.resolve()):
        raise ValueError("Heightmap output must stay inside this project's Feature/doc/evidence directory")
    output.mkdir(parents=True, exist_ok=True)
    start = time.monotonic()
    encoded = array("H")
    minimum, maximum = math.inf, -math.inf
    # Landscape decodes a sample as (sample - 32768) / 128 * ZScale.
    to_r16 = 128.0 / LANDSCAPE_Z_SCALE
    for row in range(HEIGHTMAP_SIZE):
        y = LANDSCAPE_ORIGIN[1] + row * HEIGHTMAP_SPACING_CM
        for column in range(HEIGHTMAP_SIZE):
            x = LANDSCAPE_ORIGIN[0] + column * HEIGHTMAP_SPACING_CM
            height = height_at(x, y)
            minimum, maximum = min(minimum, height), max(maximum, height)
            sample = round(32768.0 + height * to_r16)
            if not 0 <= sample <= 65535:
                raise ValueError("Height exceeds the Landscape R16 representable range")
            encoded.append(sample)
    if sys.byteorder != "little":
        encoded.byteswap()
    heightmap = output / "terrain-height.r16"
    with heightmap.open("wb") as stream:
        encoded.tofile(stream)
    report = geometry_report()
    report.update({"heightmap_file": str(heightmap), "samples": len(encoded),
                   "bytes": heightmap.stat().st_size, "minimum_height_cm": minimum,
                   "maximum_height_cm": maximum, "generation_seconds": time.monotonic() - start})
    report_path = output / "terrain-geometry.json"
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-directory")
    parser.add_argument("--report-only", action="store_true")
    args = parser.parse_args()
    result = geometry_report() if args.report_only else write_heightmap(args.output_directory)
    print(json.dumps(result, indent=2, ensure_ascii=False))
