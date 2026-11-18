"""Prepare an Unreal Landscape height patch from the recovered OpenRunMap grid sample.

The output is a little-endian R16 rectangle. The JSON reports its inclusive
Landscape vertex coordinates and measured properties of the offline surface.
"""

import argparse
import array
from bisect import bisect_right
import hashlib
import json
import math
from pathlib import Path
import sys


GRID_SIZE = 1009
GRID_CENTER = 504
GRID_STEP_CM = 1250 / 7
ARCHIVE_BOUNDS = (315, 630, 882, 945)
EDGE_BLEND_CM = 3600
PLANE_Z_CM = 9600
BRIDGE_EXIT_Y_CM = 13400
SOUTH_ENTRY_Y_CM = 32550
SOUTH_ROUTE = ((0, BRIDGE_EXIT_Y_CM), (0, 16200), (-7000, 22500), (-7300, 25050), (4000, 29500), (15000, SOUTH_ENTRY_Y_CM))
BOSS_THRESHOLD_Y_CM = 62550
BOSS_PLATFORM = (9500, 62500, 20500, 73500)
BOSS_APPROACH_XY = (12000, 60000)
BOSS_SUBGRADE_Z_CM = 9598
WEST_START_X_CM = -35000
WEST_END_X_CM = -19500
WEST_SPAWNERS = ((-37200, 44000), (-32800, 44000))
VILLAGE_PADS = {"H": (1000, 42550, 3500), "M": (29000, 42550, 3500), "T": (22000, 55550, 3400)}
CAVE_PATCHES = {"Q": (-12500, 57000, -4000, 69500), "Z": (50500, 45900, 60500, 59200)}


def world_cm(index):
    return (index - GRID_CENTER) * GRID_STEP_CM


def grid_index(world_coordinate):
    return world_coordinate / GRID_STEP_CM + GRID_CENTER


def sample_indices(lower, upper):
    return range(max(0, math.ceil(grid_index(lower) - 1e-8)), min(GRID_SIZE, math.floor(grid_index(upper) + 1e-8) + 1))


def to_z(height):
    return (height - 32768) * 2


def to_height(z):
    return max(0, min(65535, round(z / 2 + 32768)))


def smooth(value):
    t = max(0.0, min(1.0, value))
    return t * t * (3 - 2 * t)


def mix(a, b, weight):
    return a + (b - a) * weight


def footprint_weight(distance, core, edge):
    return 1 - smooth((distance - core) / (edge - core))


def sample_z(heights, x, y):
    ix, iy = grid_index(x), grid_index(y)
    u, v = min(GRID_SIZE - 2, math.floor(ix)), min(GRID_SIZE - 2, math.floor(iy))
    i = v * GRID_SIZE + u
    lower = mix(heights[i], heights[i + 1], ix - u)
    upper = mix(heights[i + GRID_SIZE], heights[i + GRID_SIZE + 1], ix - u)
    return to_z(mix(lower, upper, iy - v))


def south_center_x(y):
    for (x1, y1), (x2, y2) in zip(SOUTH_ROUTE, SOUTH_ROUTE[1:]):
        if y <= y2:
            return mix(x1, x2, smooth((y - y1) / (y2 - y1)))
    return SOUTH_ROUTE[-1][0]


def west_center_y(x):
    if x <= -32000:
        return 42000 - 2200 * smooth((x - WEST_START_X_CM) / 3000)
    return 39800 + 2600 * smooth((x + 32000) / (WEST_END_X_CM + 32000))


def centerline_distances(start, end, point):
    stations = [*range(start, end, 50), end]
    distances = [0.0]
    for i, j in zip(stations, stations[1:]):
        distances.append(distances[-1] + math.dist(point(i), point(j)))
    return stations, distances


def grade_fraction(position, centerline, taper):
    stations, distances = centerline
    if position <= stations[0]:
        return 0.0
    if position >= stations[-1]:
        return 1.0
    i = bisect_right(stations, position) - 1
    distance = mix(distances[i], distances[i + 1], (position - stations[i]) / (stations[i + 1] - stations[i]))
    total = distances[-1]
    usable = total - taper
    if distance < taper:
        return distance * distance / (2 * taper * usable)
    if total - distance < taper:
        return 1 - (total - distance) ** 2 / (2 * taper * usable)
    return (distance - taper / 2) / usable


def spawner_grade_weight(x, y):
    return 1 - max(footprint_weight(math.hypot(x - sx, y - sy), 1700, 3100) for sx, sy in WEST_SPAWNERS)


def load_archive(path):
    plan = json.loads(path.read_text(encoding="utf-8-sig"))
    if (tuple(plan["vertex_bounds"]) != ARCHIVE_BOUNDS or plan["width"] != 568 or plan["height"] != 316
            or plan["plateau_z_cm"] != PLANE_Z_CM or plan["profile_revision"] != "20261118-QZ-floor-collar150cm"):
        raise ValueError("Expected the recovered 568×316 Q/Z floor-collar plan on the canonical vertex lattice")
    samples = plan["target_heights"]
    if len(samples) != plan["width"] * plan["height"] or any(type(v) is not int or not 0 <= v <= 65535 for v in samples):
        raise ValueError("The recovered height field must contain 179488 unsigned 16-bit samples")
    return array.array("H", samples)


def r16_bytes(heights):
    if sys.byteorder == "little":
        return heights.tobytes()
    copy = heights[:]
    copy.byteswap()
    return copy.tobytes()


def patch_archive(baseline, archive):
    result = baseline[:]
    left, bottom, right, top = ARCHIVE_BOUNDS
    for v in range(bottom, top + 1):
        y = world_cm(v)
        for u in range(left, right + 1):
            x = world_cm(u)
            i = v * GRID_SIZE + u
            distance_to_edge = min(u - left, right - u, v - bottom, top - v) * GRID_STEP_CM
            weight = smooth(distance_to_edge / EDGE_BLEND_CM) * spawner_grade_weight(x, y)
            result[i] = round(mix(baseline[i], archive[(v - bottom) * 568 + u - left], weight))
    return result


def grade_connectors(baseline, result):
    start_z = sample_z(baseline, 0, BRIDGE_EXIT_Y_CM)
    south = centerline_distances(BRIDGE_EXIT_Y_CM, SOUTH_ENTRY_Y_CM, lambda y: (south_center_x(y), y))
    for v in sample_indices(BRIDGE_EXIT_Y_CM, SOUTH_ENTRY_Y_CM):
        y = world_cm(v)
        x_center = south_center_x(y)
        dx_dy = (south_center_x(y + 50) - south_center_x(y - 50)) / 100
        half_width = 1500 * math.hypot(1, dx_dy)
        shoulder = half_width + 4700
        grade = mix(start_z, PLANE_Z_CM, grade_fraction(y, south, 2500))
        land_at_start = smooth((y - BRIDGE_EXIT_Y_CM) / 2500)
        for u in sample_indices(x_center - shoulder, x_center + shoulder):
            weight = footprint_weight(abs(world_cm(u) - x_center), half_width, shoulder) * land_at_start
            i = v * GRID_SIZE + u
            result[i] = round(mix(result[i], to_height(grade), weight))

    start_z = sample_z(baseline, WEST_START_X_CM, west_center_y(WEST_START_X_CM))
    west = centerline_distances(WEST_START_X_CM, -21000, lambda x: (x, west_center_y(x)))
    for u in sample_indices(WEST_START_X_CM, WEST_END_X_CM):
        x = world_cm(u)
        y_center = west_center_y(x)
        grade = mix(start_z, PLANE_Z_CM, grade_fraction(x, west, 1700))
        land_at_start = smooth((x - WEST_START_X_CM) / 1600)
        for v in sample_indices(y_center - 5700, y_center + 5700):
            y = world_cm(v)
            weight = footprint_weight(abs(y - y_center), 1300, 5700) * land_at_start * spawner_grade_weight(x, y)
            i = v * GRID_SIZE + u
            result[i] = round(mix(result[i], to_height(grade), weight))


def preserve_boss_floor(result):
    x_lo, y_lo, x_hi, y_hi = BOSS_PLATFORM
    for v in sample_indices(y_lo, y_hi):
        for u in sample_indices(x_lo, x_hi):
            i = v * GRID_SIZE + u
            result[i] = min(result[i], to_height(BOSS_SUBGRADE_Z_CM))

    center_x, center_y = BOSS_APPROACH_XY
    for v in sample_indices(center_y - 1350, center_y + 1350):
        for u in sample_indices(center_x - 1350, center_x + 1350):
            distance = math.hypot(world_cm(u) - center_x, world_cm(v) - center_y)
            weight = footprint_weight(distance, 450, 1350)
            i = v * GRID_SIZE + u
            result[i] = round(mix(result[i], to_height(BOSS_SUBGRADE_Z_CM), weight))


def changed_rectangle(baseline, result):
    changed = [i for i, (a, b) in enumerate(zip(baseline, result)) if a != b]
    if not changed:
        raise ValueError("The recovered patch did not change any Landscape samples")
    left, right = min(i % GRID_SIZE for i in changed), max(i % GRID_SIZE for i in changed)
    bottom, top = changed[0] // GRID_SIZE, changed[-1] // GRID_SIZE
    rectangle = array.array("H")
    for v in range(bottom, top + 1):
        rectangle.extend(result[v * GRID_SIZE + left : v * GRID_SIZE + right + 1])
    return (left, bottom, right, top), rectangle, len(changed)


def max_grade(heights, points):
    return max(abs(sample_z(heights, x2, y2) - sample_z(heights, x1, y1)) / math.hypot(x2 - x1, y2 - y1)
               for (x1, y1), (x2, y2) in zip(points, points[1:]))


def verify_surface(baseline, archive, result):
    south_end_row = math.floor(grid_index(BRIDGE_EXIT_Y_CM)) + 1
    if baseline[:south_end_row * GRID_SIZE] != result[:south_end_row * GRID_SIZE]:
        raise ValueError("The authored southern start area, river, or bridges changed")

    left, bottom, _, _ = ARCHIVE_BOUNDS
    cave_sample_counts = {}
    for name, (x_lo, y_lo, x_hi, y_hi) in CAVE_PATCHES.items():
        count = 0
        for v in sample_indices(y_lo, y_hi):
            for u in sample_indices(x_lo, x_hi):
                if result[v * GRID_SIZE + u] != archive[(v - bottom) * 568 + u - left]:
                    raise ValueError(f"Recovered cave excavation {name} changed at vertex {(u, v)}")
                count += 1
        cave_sample_counts[name] = count

    village_samples = {}
    for name, (center_x, center_y, radius) in VILLAGE_PADS.items():
        grid = [result[v * GRID_SIZE + u] for v in sample_indices(center_y - radius, center_y + radius)
                for u in sample_indices(center_x - radius, center_x + radius)
                if math.hypot(world_cm(u) - center_x, world_cm(v) - center_y) <= radius]
        if any(value != to_height(PLANE_Z_CM) for value in grid):
            raise ValueError(f"Village pad {name} lost its flat {PLANE_Z_CM} cm grade")
        village_samples[name] = len(grid)

    for y in range(SOUTH_ENTRY_Y_CM, 62301, 50):
        for x in (14400, 15000, 15600):
            if not math.isclose(sample_z(result, x, y), PLANE_Z_CM, abs_tol=0.05):
                raise ValueError(f"The central route is no longer flat at {(x, y)}")

    central_threshold_z = sample_z(result, 15000, BOSS_THRESHOLD_Y_CM)
    boss_start_z = sample_z(result, *BOSS_APPROACH_XY)
    if not (math.isclose(central_threshold_z, BOSS_SUBGRADE_Z_CM, abs_tol=0.05)
            and math.isclose(boss_start_z, BOSS_SUBGRADE_Z_CM, abs_tol=0.05)):
        raise ValueError("The native boss approach samples do not give precedence to the original platform")

    south = [(south_center_x(y), y) for y in range(BRIDGE_EXIT_Y_CM, SOUTH_ENTRY_Y_CM, 50)]
    south.append((15000, SOUTH_ENTRY_Y_CM))
    west = [(x, west_center_y(x)) for x in range(WEST_START_X_CM, WEST_END_X_CM + 1, 50)]
    south_grade, west_grade = max_grade(result, south), max_grade(result, west)
    if max(south_grade, west_grade) > math.tan(math.radians(12)):
        raise ValueError(f"Connector exceeds 12° grade: south={south_grade:.3f}, west={west_grade:.3f}")
    return {
        "south_protected_rows": south_end_row,
        "cave_unchanged_samples": cave_sample_counts,
        "village_flat_samples": village_samples,
        "central_height_cm": PLANE_Z_CM,
        "central_route_cm": [SOUTH_ENTRY_Y_CM, BOSS_THRESHOLD_Y_CM],
        "boss_approach_landscape_z_cm": round(boss_start_z, 3),
        "boss_threshold_landscape_z_cm": round(central_threshold_z, 3),
        "boss_threshold_expected_platform_top_cm": PLANE_Z_CM,
        "south_connector_max_grade_deg": round(math.degrees(math.atan(south_grade)), 3),
        "west_connector_max_grade_deg": round(math.degrees(math.atan(west_grade)), 3),
        "south_connector_centerline_cm": [[round(south_center_x(y), 2), y, round(sample_z(result, south_center_x(y), y), 2)]
                                          for y in (13400, 16200, 22500, 25050, 28000, 29500, 30600, 32550)],
        "west_connector_centerline_cm": [[x, round(west_center_y(x), 2), round(sample_z(result, x, west_center_y(x)), 2)]
                                         for x in (-35000, -32800, -30500, -27500, -22500, -19500)],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-r16", type=Path, required=True)
    parser.add_argument("--archive-plan", type=Path, required=True)
    parser.add_argument("--output-r16", type=Path, required=True, help="Minimal inclusive changed rectangle in R16 format")
    parser.add_argument("--proof", type=Path, required=True, help="Offline validation and inclusive rectangle coordinates")
    args = parser.parse_args()
    if args.output_r16.resolve() == args.proof.resolve() or args.output_r16.exists() or args.proof.exists():
        raise ValueError("Use distinct, fresh output paths")

    source_bytes = args.baseline_r16.read_bytes()
    if len(source_bytes) != GRID_SIZE * GRID_SIZE * 2:
        raise ValueError("The baseline must be a 1009×1009 little-endian R16 height grid")
    baseline = array.array("H")
    baseline.frombytes(source_bytes)
    if sys.byteorder != "little":
        baseline.byteswap()
    archive = load_archive(args.archive_plan)
    result = patch_archive(baseline, archive)
    grade_connectors(baseline, result)
    preserve_boss_floor(result)
    proof = verify_surface(baseline, archive, result)
    bounds, rect, changed_count = changed_rectangle(baseline, result)
    rect_bytes = r16_bytes(rect)
    proof.update({"vertex_bounds": bounds, "width": bounds[2] - bounds[0] + 1, "height": bounds[3] - bounds[1] + 1,
                  "changed_samples": changed_count, "world_xy_bounds_cm": [round(world_cm(i), 4) for i in bounds],
                  "height_encoding": "uint16_le; Z_cm=(sample-32768)*2; rows increasing Y; columns increasing X",
                  "output_r16_sha256": hashlib.sha256(rect_bytes).hexdigest(),
                  "input_full_r16_sha256": hashlib.sha256(source_bytes).hexdigest(),
                  "archive_r16_sha256": hashlib.sha256(r16_bytes(archive)).hexdigest(),
                  "output_full_r16_sha256": hashlib.sha256(r16_bytes(result)).hexdigest()})
    for path in (args.output_r16, args.proof):
        path.parent.mkdir(parents=True, exist_ok=True)
    with args.output_r16.open("xb") as stream:
        stream.write(rect_bytes)
    with args.proof.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(json.dumps(proof, indent=2) + "\n")
    print(json.dumps(proof))


if __name__ == "__main__":
    main()
