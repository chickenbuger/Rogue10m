"""Measure the actual displayed-pose step around six field monster deaths."""

import argparse
import csv
import json
import math
from collections import defaultdict
from pathlib import Path


def read_poses(path):
    groups = defaultdict(lambda: defaultdict(dict))
    with path.open(encoding='utf-8-sig', newline='') as stream:
        for row in csv.DictReader(stream):
            groups[row['species'], row['stance']][int(row['relative_frame'])][row['bone']] = row
    return groups


def angular_distance(a, b):
    angle_cosine = abs(sum(x * y for x, y in zip(a, b))) / (math.hypot(*a) * math.hypot(*b))
    return math.degrees(2 * math.acos(min(1.0, angle_cosine)))


def step(a, b):
    pos_a, pos_b = (tuple(float(row[f'{axis}_cm']) for axis in 'xyz') for row in (a, b))
    quat_a, quat_b = (tuple(float(row[f'q{axis}']) for axis in 'xyzw') for row in (a, b))
    scale_a, scale_b = (tuple(float(row[f's{axis}']) for axis in 'xyz') for row in (a, b))
    return math.dist(pos_a, pos_b), angular_distance(quat_a, quat_b), math.dist(scale_a, scale_b)


def peaks(case):
    metric_names = ('position_cm', 'rotation_deg', 'scale')
    result = {}
    for label, frames in {'synchronous': (0,), 'first_evaluation': (1,), 'early': range(1, 8), 'late': range(8, 13)}.items():
        selected = []
        for frame in frames:
            for bone, sample in case[frame].items():
                selected.append((frame, bone, step(case[frame - 1][bone], sample)))
        result[label] = {}
        for index, name in enumerate(metric_names):
            frame, bone, values = max(selected, key=lambda item: item[2][index])
            result[label][name] = {'value': values[index], 'bone': bone, 'frame': frame}
    result['samples'] = [{'frame': frame, 'clip': next(iter(sample.values()))['death_asset'],
                          'time_s': float(next(iter(sample.values()))['death_time_s'])} for frame, sample in case.items()]
    return result


def matched_pose_difference(before, after, frames):
    changes = [step(before[frame][bone], sample) for frame in frames for bone, sample in after[frame].items()]
    return dict(zip(('position_cm', 'rotation_deg', 'scale'), map(max, zip(*changes))))


parser = argparse.ArgumentParser()
parser.add_argument('baseline', type=Path)
parser.add_argument('candidate', type=Path, nargs='?')
args = parser.parse_args()
base = read_poses(args.baseline)
candidate = read_poses(args.candidate) if args.candidate else {}
results = {}
for key, case in base.items():
    result = {'baseline': peaks(case)}
    results['/'.join(key)] = result
    if candidate:
        result['candidate'] = peaks(candidate[key])
        controls = range(-4, 13) if key[0] == 'AbyssOverlordAzathor' else range(10, 13)
        result['matched_original_pose_delta'] = matched_pose_difference(case, candidate[key], controls)
        result['same_clip_times'] = all(a['time_s'] == b['time_s'] for a, b in zip(result['baseline']['samples'], result['candidate']['samples']))
print(json.dumps(results, indent=2))
