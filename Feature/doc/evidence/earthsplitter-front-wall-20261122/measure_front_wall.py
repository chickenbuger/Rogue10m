"""Compare the displayed starter sword mesh and a sampled wall volume at every 60 Hz game observation."""

import argparse
import csv
from itertools import product
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
RUNS = ROOT / "tmp/MeleeVfxQARuns"
OBJ = ROOT / "Art/Weapons/Starter20261110/SM_StarterGreatSword.obj"
AXES = ((1, 0, 0), (0, 1, 0), (0, 0, 1))
PHASES = ("ready", "charge", "release", "recovery")


def load_obj():
    vertices, triangles = [], []
    for line in OBJ.read_text().splitlines():
        if line.startswith("v "):
            vertices.append(tuple(map(float, line.split()[1:4])))
        elif line.startswith("f "):
            face = [int(word.split("/")[0]) - 1 for word in line.split()[1:]]
            triangles.extend((face[0], a, b) for a, b in zip(face[1:], face[2:]))
    bbox = tuple((min(values), max(values)) for values in zip(*vertices))
    return vertices, triangles, tuple(product(*bbox))


def load(label, name):
    with (RUNS / label / f"{name}.csv").open(newline="") as stream:
        return list(csv.DictReader(stream))


def value(row, prefix, suffixes="xyz"):
    return tuple(float(row[prefix + suffix]) for suffix in suffixes)


def sub(left, right):
    return tuple(a - b for a, b in zip(left, right))


def dot(left, right):
    return sum(a * b for a, b in zip(left, right))


def cross(left, right):
    x, y, z = left
    p, q, r = right
    return y * r - z * q, z * p - x * r, x * q - y * p


def quaternion_axes(rotation):
    magnitude = math.hypot(*rotation)
    x, y, z, w = (coordinate / magnitude for coordinate in rotation)
    return (
        (1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)),
        (2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)),
        (2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)),
    )


def intersects(triangle, extent):
    a, b, c = triangle
    sides = sub(b, a), sub(c, b), sub(a, c)
    axes = (*AXES, cross(sides[0], sides[1]), *(cross(side, axis) for side in sides for axis in AXES))
    for axis in axes:
        support = sum(radius * abs(component) for radius, component in zip(extent, axis))
        pa, pb, pc = dot(a, axis), dot(b, axis), dot(c, axis)
        if min(pa, pb, pc) > support or max(pa, pb, pc) < -support:
            return False
    return True


def maximum(rows, column):
    return max((float(row[column]) for row in rows), default=None)


def minimum(rows, column):
    return min((float(row[column]) for row in rows), default=None)


def positive_hit_signature(rows):
    hits = [row for row in rows if float(row["damage"]) > 0]
    return [
        {"frame": int(row["frame"]),
         **{field: float(row[field]) for field in ("hold_s", "accepted_s", "montage_s", "damage")}}
        for row in hits
    ]


def hit_timing(result):
    # Combat already rolls damage variation between isolated processes.
    return [{field: value for field, value in hit.items() if field not in {"damage", "frame"}}
            for hit in result["damage"]]


def compare_controls(candidate, stock):
    result = {}
    changing_counters = {"frame", "case_frame", "world_s", "revision", "health", "damage"}
    for name in ("frames", "earthsplitter-charge", "technique-actor", "technique-poses"):
        on, off = load(candidate, name), load(stock, name)
        if not on or not off:
            raise ValueError(f"Missing control observations: {name}")
        fields = set(on[0]) - changing_counters
        if fields != set(off[0]) - changing_counters:
            raise ValueError(f"Different control schema: {name}")
        differences = sum(
            any(a[key] != b[key] for key in fields)
            or ("damage" in a and (float(a["damage"]) > 0) != (float(b["damage"]) > 0))
            for a, b in zip(on, off)
        )
        result[name] = {"candidate": len(on), "stock": len(off), "unequal": differences}
    return result


def sample(label, mesh_data):
    verts, faces, bound_corners = mesh_data
    rows = load(label, "earthsplitter-front-wall")
    if not rows:
        raise ValueError(f"No frontal wall observations for {label}")
    meshes = sorted({row["mesh_path"] for row in rows})
    if len(meshes) != 1 or not meshes[0].endswith("SM_StarterGreatSword.SM_StarterGreatSword"):
        raise ValueError(f"{label} path does not match the authored StarterGreatSword: {meshes}")
    identities = {row["case"] for row in rows}
    if len(identities) != 1:
        raise ValueError(f"{label} sampled more or fewer than one active front-wall case")
    reference = [row for row in load(label, "earthsplitter-charge") if row["case"] in identities]
    per_phase = {
        phase: {
            "samples": 0, "triangle_intersection_samples": 0, "min_mesh_front_plane_cm": None,
            "maximum_blade_axis_step_deg": 0, "maximum_hilt_step_cm": 0,
        }
        for phase in PHASES
    }
    intersecting = []
    previous = None
    worst_bbox_error = 0
    largest_axis_step, largest_hilt_step, largest_held_hilt_step = 0, 0, 0
    for row in rows:
        phase = row["phase"]
        item = per_phase[phase]
        item["samples"] += 1
        extent = value(row, "wall_e")
        position = value(row, "sword_")
        scaling = value(row, "sword_s")
        rotation = value(row, "sword_q", "xyzw")
        axes = quaternion_axes(rotation)
        frame = int(row["frame"])
        blade = tuple(axis[2] for axis in axes)
        if previous and frame == previous[0] + 1:
            cosine = max(-1, min(1, dot(blade, previous[1])))
            axis_step = math.degrees(math.acos(cosine))
            hilt_step = math.dist(position, previous[2])
            largest_axis_step = max(largest_axis_step, axis_step)
            largest_hilt_step = max(largest_hilt_step, hilt_step)
            item["maximum_blade_axis_step_deg"] = max(item["maximum_blade_axis_step_deg"], axis_step)
            item["maximum_hilt_step_cm"] = max(item["maximum_hilt_step_cm"], hilt_step)
            if phase == "charge":
                largest_held_hilt_step = max(largest_held_hilt_step, hilt_step)
        previous = frame, blade, position

        def place(origin):
            scaled = tuple(a * b for a, b in zip(origin, scaling))
            return tuple(bias + dot(axis, scaled) for axis, bias in zip(axes, position))

        world_bounds = [place(corner) for corner in bound_corners]
        gap = max(max(min(values) - radius, -radius - max(values))
                  for values, radius in zip(zip(*world_bounds), extent))
        worst_bbox_error = max(worst_bbox_error, abs(float(row["wall_axis_gap_cm"]) - gap))
        located = [place(vertex) for vertex in verts]
        front_plane = -extent[0] - max(v[0] for v in located)
        last_min = item["min_mesh_front_plane_cm"]
        item["min_mesh_front_plane_cm"] = min(last_min, front_plane) if last_min is not None else front_plane
        if gap > 0:
            continue
        triangle_hits = sum(intersects(tuple(located[k] for k in face), extent) for face in faces)
        if triangle_hits:
            item["triangle_intersection_samples"] += 1
            intersecting.append({
                "frame": frame, "phase": phase, "hold_s": float(row["hold_s"]),
                "accepted_s": float(row["accepted_s"]), "montage_s": float(row["montage_s"]),
                "triangles": triangle_hits, "mesh_front_plane_cm": front_plane,
            })
    if worst_bbox_error > .005:
        raise ValueError(f"{label} OBJ/runtime bounding gap differs by {worst_bbox_error:.6f}cm")
    if any(item["samples"] == 0 for item in per_phase.values()):
        raise ValueError(f"{label} did not observe ready, charge, release, and recovery")
    result = {
        "label": label, "wall_half_extents_cm": value(rows[0], "wall_e"), "mesh": meshes[0],
        "damage": positive_hit_signature(rows), "phase_measurements": per_phase,
        "minimum_capsule_plane_gap_cm": minimum(rows, "capsule_plane_gap_cm"),
        "minimum_weapon_floor_gap_cm": minimum(reference, "bounds_min_above_capsule_cm"),
        "maximum_actor_travel_cm": maximum(reference, "actor_travel_cm"),
        "maximum_palm_span_cm": maximum(reference, "palm_span_cm"),
        "maximum_hilt_anchor_error_cm": maximum(reference, "grip_palm_error_cm"),
        "maximum_blade_angle_step_deg": largest_axis_step,
        "maximum_hilt_step_cm": largest_hilt_step,
        "maximum_charge_hilt_step_cm": largest_held_hilt_step,
        "maximum_obj_runtime_aabb_gap_error_cm": worst_bbox_error,
        "intersections": intersecting,
    }
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("labels", nargs="+")
    parser.add_argument("--compare", action="store_true", help="Compare one stock label with the candidate label after it.")
    parser.add_argument("--compare-controls", action="store_true", help="Compare all open or side-wall observations for one stock and candidate pair.")
    parser.add_argument("--expect-clear", action="store_true", help="Require zero sword/wall intersection across all sampled phases.")
    args = parser.parse_args()
    if args.compare_controls:
        if len(args.labels) != 2:
            parser.error("--compare-controls takes a stock label and a candidate label")
        comparisons = compare_controls(args.labels[1], args.labels[0])
        print(json.dumps(comparisons, indent=2))
        if any(item["candidate"] != item["stock"] or item["unequal"] for item in comparisons.values()):
            raise SystemExit("The frontal recovery changed an open or side-wall observation.")
        return
    mesh_data = load_obj()
    results = [sample(label, mesh_data) for label in args.labels]
    if args.compare:
        if len(results) != 2:
            parser.error("--compare takes a stock label and a candidate label")
        stock, candidate = results
        if hit_timing(stock) != hit_timing(candidate):
            raise ValueError(f"Original hit timing or pulse count differed: {hit_timing(stock)} vs {hit_timing(candidate)}")
    print(json.dumps(results, indent=2))
    checked = results[1:] if args.compare else results
    if args.expect_clear:
        fails = [result["label"] for result in checked if result["intersections"]]
        if fails:
            raise SystemExit(f"Wall triangle overlap remained: {', '.join(fails)}")


if __name__ == "__main__":
    main()
