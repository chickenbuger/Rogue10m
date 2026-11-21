"""Compare stock and forward-landing gameplay observations at fixed 60 Hz."""

import argparse
import bisect
import csv
import json
import math
from collections import defaultdict
from pathlib import Path


def read_frames(path):
    streams = defaultdict(dict)
    with path.open(encoding="utf-8-sig", newline="") as csv_input:
        for row in csv.DictReader(csv_input):
            frames = streams[row["case"], row["mesh"]]
            frame = frames.setdefault(int(row["frame"]), {
                "age": float(row["seconds_from_contact"]), "gait": float(row["gait_sample_time_s"]),
                "marker": (row["marker_prev"], row["marker_next"]), "floor": float(row["floor_z_cm"]),
                "land": row["land_asset"], "land_weight": float(row["land_weight"]),
                "seed": float(row["seed"]), "land_time": float(row["land_time_s"]), "bones": {},
            })
            frame["bones"][row["bone"]] = {
                "local": tuple(float(row[f"local_{axis}_cm"]) for axis in "xyz"),
                "world": tuple(float(row[f"world_{axis}_cm"]) for axis in "xyz"),
                "quat": tuple(float(row[f"q{axis}"]) for axis in "xyzw"),
            }
    return streams


def joint_angle(a, b):
    cosine = sum(x*y for x, y in zip(a, b)) / (math.hypot(*a) * math.hypot(*b))
    return math.degrees(math.acos(max(-1.0, min(1.0, cosine))))


def knee(frame, side):
    thigh, calf, foot = (frame["bones"][f"{bone}_{side}"]["local"] for bone in ("thigh", "calf", "foot"))
    return joint_angle(tuple(x-y for x, y in zip(thigh, calf)), tuple(x-y for x, y in zip(foot, calf)))


def rotations(frame, next_frame, side):
    a, b = (sample["bones"][f"foot_{side}"]["quat"] for sample in (frame, next_frame))
    cosine = abs(sum(x*y for x, y in zip(a, b))) / (math.hypot(*a) * math.hypot(*b))
    return math.degrees(2 * math.acos(min(1.0, cosine)))


def stats(rows, start, end):
    samples = [row for row in rows if start - 1.e-5 <= row["age"] <= end + 1.e-5]
    pairs = list(zip(samples, samples[1:]))
    result = {}
    for side in "lr":
        toes = [row["bones"][f"ball_{side}"]["world"] for row in samples]
        corrected = [(x, y, z-row["floor"]) for (x, y, z), row in zip(toes, samples)]
        result[side] = {
            "knee_step_deg": max(abs(knee(a, side)-knee(b, side)) for a, b in pairs),
            "foot_step_deg": max(rotations(a, b, side) for a, b in pairs),
            "min_toe_floor_cm": min(toe[2] for toe in corrected),
            "low_toe_path_cm": sum(math.dist(a[:2], b[:2]) for a, b in zip(corrected, corrected[1:]) if max(a[2], b[2]) <= 8),
            "pelvis_step_cm": max(math.dist(a["bones"]["pelvis"]["local"], b["bones"]["pelvis"]["local"]) for a, b in pairs),
            "head_step_cm": max(math.dist(a["bones"]["head"]["local"], b["bones"]["head"]["local"]) for a, b in pairs),
        }
    return result


def match_handoff(base, candidate):
    stable = sorted((row for row in base if row["age"] > .6 and row["gait"] > 0 and row["land_weight"] < .05), key=lambda row: row["gait"])
    phases = [row["gait"] for row in stable]
    rows = [row for row in candidate if .30 <= row["age"] <= .50 and row["gait"] > 0]
    differences = []
    for row in rows:
        index = bisect.bisect_left(phases, row["gait"])
        if not (0 < index < len(stable)):
            continue
        low, high = stable[index-1:index+1]
        mix = (row["gait"]-low["gait"])/(high["gait"]-low["gait"])
        deltas = {side: knee(row, side)-(knee(low, side)+(knee(high, side)-knee(low, side))*mix) for side in "lr"}
        differences.append({"age": row["age"], "gait": row["gait"], "land_weight": row["land_weight"], "candidate_minus_phase_matched_knee_deg": deltas})
    return differences


def compare(base, candidate):
    result = {}
    for stream, original in base.items():
        stock, new = list(original.values()), list(candidate[stream].values())
        windows = {"impact": (0, 4/60), "recovery": (4/60, 18/60), "handoff": (18/60, 30/60)}
        stages = {stage: {"baseline": stats(stock, *window), "candidate": stats(new, *window)} for stage, window in windows.items()}
        recovery = stages["recovery"]
        original_travel, new_travel = (sum(value["low_toe_path_cm"] for value in recovery[kind].values()) for kind in ("baseline", "candidate"))
        phases = [row for row in new if .3 <= row["age"] <= .55 and row["gait"] > 0]
        closest_r = min(phases, key=lambda row: abs(row["gait"]-.851702), default=None)
        result["/".join(stream)] = {
            **stages, "recovery_low_toe_reduction": 1.0-new_travel/original_travel if original_travel else None,
            "closest_second_R": {key: closest_r[key] for key in ("age", "gait", "marker", "land_weight", "land_time")} if closest_r else None,
            "matched_handoff": match_handoff(stock, new),
        }
    return result


parser = argparse.ArgumentParser()
parser.add_argument("baseline", type=Path)
parser.add_argument("candidate", type=Path)
parser.add_argument("output", type=Path)
args = parser.parse_args()
with args.output.open("x", encoding="utf-8") as output:
    json.dump(compare(read_frames(args.baseline), read_frames(args.candidate)), output, indent=2)
