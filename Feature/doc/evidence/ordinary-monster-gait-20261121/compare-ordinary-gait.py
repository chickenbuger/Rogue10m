"""Compare evaluated monster poses and the settled authored loop at 60 Hz."""

import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path


def read_poses(path):
    poses = defaultdict(lambda: defaultdict(dict))
    with path.open(encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            for name in ("x_cm", "y_cm", "z_cm", "qx", "qy", "qz", "qw", "sx", "sy", "sz", "gait_time_s"):
                row[name] = float(row[name])
            poses[row["case"]][int(row["frame"])][row["bone"].lower()] = row
    return poses


def position(row):
    return tuple(row[f"{axis}_cm"] for axis in "xyz")


def scale(row):
    return tuple(row[f"s{axis}"] for axis in "xyz")


def rotation(row):
    return tuple(row[f"q{axis}"] for axis in "xyzw")


def angular_difference(a, b):
    cosine = abs(sum(x * y for x, y in zip(a, b))) / (math.hypot(*a) * math.hypot(*b))
    return math.degrees(2 * math.acos(min(1.0, cosine)))


def relative(rotation_a, rotation_b):
    ax, ay, az, aw = rotation_a
    bx, by, bz, bw = rotation_b
    return (aw * bx - ax * bw - ay * bz + az * by, aw * by - ay * bw - az * bx + ax * bz,
            aw * bz - az * bw - ax * by + ay * bx, aw * bw + ax * bx + ay * by + az * bz)


def changes(frames, bone, interval):
    samples = []
    for frame in interval:
        a, b = frames[frame - 1][bone], frames[frame][bone]
        if b["paired"] != "1":
            raise ValueError(f"Missing fresh pose at frame {frame}")
        samples.append((math.dist(position(a), position(b)), angular_difference(rotation(a), rotation(b)), math.dist(scale(a), scale(b))))
    return dict(zip(("position_cm", "rotation_deg", "scale"), map(max, zip(*samples))))


def amplitude(samples):
    def spread(values):
        return max(values) - min(values)

    values = {"body_height_cm": spread([row["body"]["z_cm"] for row in samples])}
    if "shell" in samples[0]:
        values["shell_relative_height_cm"] = spread([row["shell"]["z_cm"] - row["body"]["z_cm"] for row in samples])
        angles = [relative(rotation(row["body"]), rotation(row["shell"])) for row in samples]
        values["shell_relative_turn_deg"] = max(angular_difference(a, b) for a in angles for b in angles)
    else:
        values["crown_relative_height_cm"] = spread([row["crown"]["z_cm"] - row["body"]["z_cm"] for row in samples])
        values["side_span_cm"] = spread([math.dist(position(row["side_l"]), position(row["side_r"])) for row in samples])
        for bone in ("body", "crown", "side_l"):
            for axis in "xyz":
                values[f"{bone}_scale_{axis}"] = spread([row[bone][f"s{axis}"] for row in samples])
    return values


def compare(path):
    cases = read_poses(path)
    stock, candidate = cases.values()
    bones = stock[1].keys()
    windows = {"start": range(27, 55), "settled": range(68, 128), "stop": range(128, 160)}
    peaks = {case: {name: {bone: changes(frames, bone, window) for bone in bones} for name, window in windows.items()}
             for case, frames in cases.items()}
    loop = {case: amplitude([frames[frame] for frame in windows["settled"]]) for case, frames in cases.items()}
    original, blended = loop.values()
    ratios = {name: blended[name] / value for name, value in original.items() if value >= (.01 if "scale" in name else .1)}
    detail = "hornmidtip03" if "shell" in bones else "crown"
    stock_stats, blend_stats = peaks.values()
    start_before = stock_stats["start"][detail]["position_cm"]
    start_after = blend_stats["start"][detail]["position_cm"]
    stop_before = stock_stats["stop"][detail]["position_cm"]
    stop_after = blend_stats["stop"][detail]["position_cm"]
    steady_step = stock_stats["settled"][detail]["position_cm"]
    return {
        "file": path.name, "detail_bone": detail, "pose_peaks": peaks, "walk_loop_amplitude": loop,
        "walk_loop_amplitude_ratio": ratios,
        "reduces_start_discontinuity": start_before >= max(2.0, steady_step * 1.5) and start_after <= .65 * start_before,
        "avoids_new_stop_discontinuity": stop_after <= max(stop_before * 1.15, steady_step * 1.05) + .25,
        "preserves_settled_walk_amplitude": all(.90 <= ratio <= 1.10 for ratio in ratios.values()),
    }


parser = argparse.ArgumentParser()
parser.add_argument("paths", nargs="+", type=Path)
args = parser.parse_args()
print(json.dumps({(path.parent.name if path.stem == "ordinary-gait-frames" else path.stem): compare(path) for path in args.paths}, indent=2))
