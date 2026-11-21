"""Measure CrushingSlash's two-palm grip, joint continuity, and actual sword reach."""
import csv
import json
import math
from collections import defaultdict
from pathlib import Path
import sys


PROJECT = next(folder for folder in Path(__file__).resolve().parents if (folder / 'Rogue10m.uproject').is_file())
ROOT = PROJECT / 'tmp/MeleeVfxQARuns'


def table(path):
    with path.open(newline='', encoding='utf-8-sig') as file:
        return list(csv.DictReader(file))


def vec(row, keys):
    return tuple(float(row[key]) for key in keys)


def subtract(a, b):
    return tuple(x-y for x, y in zip(a, b))


def midpoint(a, b):
    return tuple((x+y)*.5 for x, y in zip(a, b))


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def normalized(a):
    return tuple(x/math.hypot(*a) for x in a)


def angle(a, b):
    return math.degrees(math.acos(max(-1., min(1., dot(normalized(a), normalized(b))))))


def multiply(a, b):
    x, y, z, w = a
    u, v, s, t = b
    return (w*u+x*t+y*s-z*v, w*v-x*s+y*t+z*u, w*s+x*v-y*u+z*t, w*t-x*u-y*v-z*s)


def local_rotation(parent, child):
    x, y, z, w = normalized(parent)
    return multiply((-x, -y, -z, w), normalized(child))


def rotation_step(a, b):
    return math.degrees(2*math.acos(min(1., abs(dot(normalized(a), normalized(b))))))


def inspect(label):
    folder = ROOT / label
    blade = {int(r['frame']): r for r in table(folder / 'frames.csv')}
    actor = {int(r['case_frame']): r for r in table(folder / 'technique-actor.csv')}
    maps = defaultdict(lambda: defaultdict(dict))
    for row in table(folder / 'technique-poses.csv'):
        i = int(row['case_frame'])
        if row['skill'] == 'DA_Hybrid_GreatSword_CrushingSlash' and blade[i]['phase'] == 'attack' and float(row['montage_weight']) > 0:
            maps[int(row['case'])][row['mesh']].setdefault(i, {})[row['bone']] = row
    output = {}
    for case, meshes in sorted(maps.items()):
        frames = sorted(meshes['visible'])
        best = min(frames, key=lambda i: float(blade[i]['sword_capsule_gap_cm']))
        damages = [blade[i] for i in frames if float(blade[i]['damage']) > .01]
        axes = []
        forward = []
        for i in frames:
            face = math.radians(float(actor[i]['actor_yaw']))
            direction = (math.cos(face), math.sin(face), 0.)
            base = vec(blade[i], ('blade_base_x','blade_base_y','blade_base_z'))
            tip = vec(blade[i], ('blade_tip_x','blade_tip_y','blade_tip_z'))
            origin = vec(actor[i], ('actor_x','actor_y','actor_z'))
            axes.append(subtract(tip, base))
            forward.append(max(dot(subtract(tip, origin), direction), dot(subtract(base, origin), direction)))
        entry = {'active_frames': len(frames), 'damage_pulses': len(damages),
                 'first_damage_elapsed_s': float(damages[0]['elapsed']) if damages else None,
                 'first_damage_montage_s': float(damages[0]['montage_s']) if damages else None,
                 'closest_blade_capsule_gap_cm': float(blade[best]['sword_capsule_gap_cm']),
                 'closest_blade_montage_s': float(blade[best]['montage_s']),
                 'closest_blade_elapsed_s': float(blade[best]['elapsed']),
                 'maximum_forward_blade_span_cm': max(forward),
                 'maximum_blade_adjacent_angle_deg': max(angle(a, b) for a,b in zip(axes, axes[1:])),
                 'minimum_blade_capsule_floor_cm': min(min(float(blade[i][k]) for k in ('blade_base_z','blade_tip_z'))-
                                                      float(blade[i]['capsule_bottom_world_z']) for i in frames),
                 'actor_translation_cm': max(math.dist(vec(actor[frames[0]],('actor_x','actor_y','actor_z')),
                                                      vec(actor[i],('actor_x','actor_y','actor_z'))) for i in frames)}
        for mesh, by_frame in meshes.items():
            wrists = []
            palms = []
            rides = []
            rotations = defaultdict(list)
            for i in frames:
                points = by_frame[i]
                hands = {side: vec(points['hand_'+side], 'xyz') for side in 'rl'}
                finger = {side: vec(points['middle_01_'+side], 'xyz') for side in 'rl'}
                wrists.append(math.dist(hands['r'], hands['l']))
                palms.append(math.dist(midpoint(hands['r'], finger['r']), midpoint(hands['l'], finger['l'])))
                rides.append(subtract(hands['r'], vec(points['pelvis'], 'xyz')))
                for side in 'rl':
                    u, l, h = (vec(points[bone+'_'+side], ('qx','qy','qz','qw')) for bone in ('upperarm','lowerarm','hand'))
                    rotations['forearm_'+side].append(local_rotation(u,l))
                    rotations['wrist_'+side].append(local_rotation(l,h))
            joint_steps = {name: max(rotation_step(a, b) for a,b in zip(values,values[1:])) for name,values in rotations.items()}
            motions = [math.dist(a,b) for a,b in zip(rides,rides[1:])]
            step_frame = frames[motions.index(max(motions))+1]
            entry[mesh] = {'wrist_span_cm': [min(wrists), max(wrists)], 'palm_span_cm': [min(palms), max(palms)],
                           'palm_frames_over_33cm': sum(span>33 for span in palms), 'wrist_step_cm': max(motions),
                           'wrist_step_end_montage_s': float(blade[step_frame]['montage_s']),
                           'largest_joint_local_rotation_step_deg': joint_steps}
        output[str(case)] = entry
    return output


if __name__=='__main__':
    for label in sys.argv[1:]:
        report = inspect(label)
        result = json.dumps(report, indent=2)+'\n'
        (ROOT / label / 'crushing-motion.json').write_text(result, encoding='utf-8')
        print(result)
