"""Read-only analysis of Rogue10m.TestAnimationStability CSV captures.

A capture PASS is not a pose-quality PASS. Large values are diagnostics; authored
motion, coordinate axes, retarget pose offsets and actual footage must be checked.
"""
from __future__ import annotations
import argparse
import csv
import json
import math
from pathlib import Path


def v(row, prefix):
    return tuple(float(row[f'{prefix}_t{axis}']) for axis in 'xyz')


def q(row, prefix):
    return tuple(float(row[f'{prefix}_q{axis}']) for axis in 'xyzw')


def sub(a, b):
    return tuple(x-y for x, y in zip(a, b))


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def norm(a):
    return math.sqrt(dot(a, a))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def angle(a, b):
    length = norm(a)*norm(b)
    return math.degrees(math.acos(max(-1., min(1., dot(a, b)/length)))) if length > 1e-8 else None


def mul(a, b):
    av, bv = a[:3], b[:3]
    c = cross(av, bv)
    return (*(a[3]*bv[i]+b[3]*av[i]+c[i] for i in range(3)), a[3]*b[3]-dot(av, bv))


def inverse(a):
    n = dot(a, a)
    return (-a[0]/n, -a[1]/n, -a[2]/n, a[3]/n)


def qangle(a, b):
    d = abs(dot(a, b)/(norm(a)*norm(b)))
    return math.degrees(2*math.acos(min(1., max(0., d))))


def read_csv(path):
    with path.open(encoding='utf-8-sig', newline='') as handle:
        rows = list(csv.DictReader(handle))
    # Unreal FName lookup is case-insensitive; source root/pelvis and target Root/Pelvis are the same keys.
    for row in rows:
        for field in ('bone', 'parent'):
            if field in row:
                row[field] = row[field].casefold()
    return rows


def percentile(values, percentile_value):
    values = sorted(values)
    return values[min(len(values)-1, int((len(values)-1)*percentile_value))] if values else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    frames = {int(r['frame']): r for r in read_csv(args.capture/'frames.csv')}
    poses = read_csv(args.capture/'poses.csv')
    reference = {}
    for row in read_csv(args.capture/'skeletons.csv'):
        key = (row['stage'], row['bone'])
        parent = reference.get((row['stage'], row['parent']))
        reference[key] = mul(parent, q(row, 'reference_local')) if parent else q(row, 'reference_local')
    by_frame = {}
    tracks = {}
    for row in poses:
        frame, stage, bone = int(row['frame']), row['stage'], row['bone']
        by_frame.setdefault((frame, stage), {})[bone] = row
        tracks.setdefault((stage, bone), []).append(row)
    summary = {'capture': str(args.capture.resolve()), 'frame_count': len(frames),
        'note': 'Diagnostics, not anatomical PASS criteria. Foot speeds are component-space speeds, not proven ground contact or sliding. Reference-relative stage deltas can include intentional retarget-pose offsets.',
        'tracks': {}, 'bend_planes': {}, 'source_appearance_delta': {}}
    for (stage, bone), rows in tracks.items():
        max_step = max(rows, key=lambda r: float(r['step_deg']))
        lengths = [float(r['length_ratio']) for r in rows]
        positions = [v(r, 'raw_cs') for r in rows]
        speeds = [float(r['step_cm'])*30 for r in rows]
        summary['tracks'][stage+'/'+bone] = {
            'max_rotation_step_deg': float(max_step['step_deg']), 'max_rotation_step_frame': int(max_step['frame']),
            'max_rotation_step_attack': int(frames[int(max_step['frame'])]['attack']),
            'p95_rotation_step_deg': percentile([float(r['step_deg']) for r in rows], .95),
            'max_component_speed_cm_s': max(speeds), 'p95_component_speed_cm_s': percentile(speeds, .95),
            'local_length_ratio_min': min(lengths), 'local_length_ratio_max': max(lengths),
            'component_position_range_cm': [max(p[i] for p in positions)-min(p[i] for p in positions) for i in range(3)],
            'max_rotation_from_first_frame_deg': max(qangle(q(rows[0], 'raw_cs'), q(r, 'raw_cs')) for r in rows)}
    for stage in ('source', 'appearance'):
        for chain, bones in {'arm_l': ('upperarm_l', 'lowerarm_l', 'hand_l'), 'arm_r': ('upperarm_r', 'lowerarm_r', 'hand_r'),
                'leg_l': ('thigh_l', 'calf_l', 'foot_l'), 'leg_r': ('thigh_r', 'calf_r', 'foot_r')}.items():
            records, previous = [], None
            for frame in sorted(frames):
                data = by_frame.get((frame, stage), {})
                if not all(b in data for b in bones):
                    continue
                a, b, c = (v(data[bone], 'raw_cs') for bone in bones)
                first, second = sub(a, b), sub(c, b)
                bend = angle(first, second)
                normal = cross(first, second)
                # Near-straight chains have unstable plane normals even with a valid pose.
                reliable = bend is not None and 10 < bend < 170
                plane_step = angle(previous, normal) if previous is not None and reliable else None
                previous = normal if reliable else None
                records.append({'frame': frame, 'joint_angle_deg': bend, 'plane_step_deg': plane_step})
            valid = [r for r in records if r['plane_step_deg'] is not None]
            angles = [r['joint_angle_deg'] for r in records if r['joint_angle_deg'] is not None]
            summary['bend_planes'][stage+'/'+chain] = {
                'joint_angle_min_deg': min(angles) if angles else None,
                'joint_angle_max_deg': max(angles) if angles else None,
                'largest_plane_steps': sorted(valid, key=lambda r: r['plane_step_deg'], reverse=True)[:8]}
    for bone in ('root', 'pelvis', 'head', 'hand_l', 'hand_r', 'foot_l', 'foot_r'):
        values = []
        for frame in sorted(frames):
            source = by_frame.get((frame, 'source'), {}).get(bone)
            target = by_frame.get((frame, 'appearance'), {}).get(bone)
            if source and target:
                sd = mul(q(source, 'raw_cs'), inverse(reference[('source', bone)]))
                td = mul(q(target, 'raw_cs'), inverse(reference[('appearance', bone)]))
                values.append({'frame': frame, 'attack': int(frames[frame]['attack']), 'degrees': qangle(sd, td)})
        summary['source_appearance_delta'][bone] = {'largest_reference_relative_rotation_differences': sorted(values, key=lambda r: r['degrees'], reverse=True)[:8],
            'p95_difference_deg': percentile([r['degrees'] for r in values], .95)}
    output = args.output or args.capture/'analysis.json'
    output.write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'output': str(output.resolve()), 'frame_count': len(frames), 'pose_rows': len(poses)}, ensure_ascii=False))


if __name__ == '__main__':
    main()
