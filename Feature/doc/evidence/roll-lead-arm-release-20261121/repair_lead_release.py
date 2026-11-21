"""Measure and repair the lead-arm handoff in the existing Manny roll."""

import json
import math
import os
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector


SOURCE = Path('D:/Project/Rogue10m/tmp/AltMotionBlender20261121')
OUT = Path(__file__).resolve().parent
SCRIPT = SOURCE / 'author_manny_roll.py'
SOURCE_BLEND = SOURCE / 'v4/MannyShoulderRoll.blend'
ARM = ('upperarm_r', 'lowerarm_r', 'hand_r')
DURATION = 0.35 / 0.9804
MATCHED_PHASES = [i / 60 / DURATION for i in range(22)]


def degrees_between(a, b):
    dot = abs(sum(float(a[i]) * float(b[i]) for i in range(4)))
    scale = math.sqrt(sum(float(v)**2 for v in a) * sum(float(v)**2 for v in b))
    return math.degrees(2 * math.acos(min(1.0, dot / scale)))


def select(rig, action):
    rig.animation_data.action = action
    rig.animation_data.action_slot = action.slots[0]


def evaluate(scene, phase):
    frame = 1 + 25 * phase
    integer = math.floor(frame)
    scene.frame_set(integer, subframe=frame - integer)
    bpy.context.view_layer.update()


def local_delta_quats(rig):
    return {name: rig.pose.bones[name].rotation_quaternion.copy() for name in ARM}


def sampled(rig, scene, phases, markers, gap_func, floor):
    rows = []
    for phase in phases:
        evaluate(scene, phase)
        bones = rig.pose.bones
        positions = {bone.name: bone.head.copy() for bone in bones}
        transforms = {bone.name: bone.matrix.to_quaternion() for bone in bones}
        im = bones['pelvis'].matrix.inverted()
        rel = {name: im @ positions[name] for name in ARM}
        roll_world_wrist = positions['hand_r'] + Vector((0, -250 * phase, 0))
        gaps = gap_func(rig, markers, floor)
        inf = {
            'p': phase,
            'loc': positions,
            'rot': transforms,
            'q': local_delta_quats(rig),
            'rel': rel,
            'gaps': gaps,
            'support': min(gaps, key=gaps.get),
            'travel_wrist': roll_world_wrist,
            'pelvis': bones['pelvis'].matrix.copy(),
            'palm_z': (bones['hand_r'].head.z + bones['middle_01_r'].head.z) / 2 - floor,
            'palm_offset': (bones['middle_01_r'].head - bones['hand_r'].head) / 2,
        }
        rows.append(inf)
    return rows


def pair(a, b):
    return {
        'p': [a['p'], b['p']],
        'step': {name: degrees_between(a['q'][name], b['q'][name]) for name in ARM},
        'wrist_step_cm': (a['rel']['hand_r'] - b['rel']['hand_r']).length,
        'world_wrist_step_cm': (a['travel_wrist'] - b['travel_wrist']).length,
    }


def summary(rows, reference=None):
    pairs = [pair(a, b) for a,b in zip(rows, rows[1:])]
    handoff = [p for p in pairs if p['p'][1] >= .35 and p['p'][0] <= .76]
    result = {
        'max_arm_step_deg': {name: max(handoff, key=lambda p: p['step'][name]) for name in ARM},
        'max_wrist_step_cm': max(handoff, key=lambda p: p['wrist_step_cm']),
        'matched_runtime_intervals': [p for p in pairs if .465 < p['p'][0] < .47 or .558 < p['p'][0] < .562],
        'head_clearance': min(row['gaps']['head'] for row in rows),
        'min_gap': min(min(row['gaps'].values()) for row in rows),
        'sampled_support': [[round(row['p'], 5), row['support'], round(row['gaps'][row['support']],4), round(row['gaps']['hand_r'],4)]
                            for row in rows if .35 < row['p'] < .75],
    }
    if reference:
        result['retained_support_wrist_world_error_cm'] = max(
            (a['travel_wrist'] - b['travel_wrist']).length for a,b in zip(rows, reference) if .34 < a['p'] < .445)
        result['unmodified_positions_error_cm'] = max(
            (a['loc'][name] - b['loc'][name]).length
            for a,b in zip(rows, reference) for name in ('root','pelvis','head','upperarm_l','hand_l','foot_l','foot_r'))
        result['shoulder_proxy_delta_cm'] = max(abs(a['gaps']['shoulder_r'] - b['gaps']['shoulder_r']) for a,b in zip(rows, reference))
        result['minimum_sword_palm_delta_cm'] = min(a['palm_z'] - b['palm_z'] for a,b in zip(rows, reference) if .46 < a['p'] < .63)
        result['palm_near_transfer'] = [[round(a['p'], 5), round(a['palm_z'], 3), round(b['palm_z'], 3)]
                                       for a,b in zip(rows,reference) if .34 < a['p'] < .74]
    return result


def write_keys(rig, scene, action, rows):
    select(rig, action)
    previous = {}
    for frame, sample in enumerate(rows, 1):
        scene.frame_set(frame)
        for name, rotation in sample.items():
            rotation = rotation.copy()
            if name in previous and previous[name].dot(rotation) < 0:
                rotation.negate()
            bone = rig.pose.bones[name]
            bone.rotation_quaternion = rotation
            bone.keyframe_insert(data_path='rotation_quaternion', frame=frame, group=name)
            previous[name] = rotation


def ease(factor, slope):
    return slope * factor + (1 - slope) * factor * factor * (3 - 2 * factor)


def uniform_handoff(key_values, first, last, slope):
    a, b = key_values[first - 1], key_values[last - 1]
    result = [dict(row) for row in key_values]
    for frame in range(first + 1, last):
        factor = ease((frame - first) / (last - first), slope)
        for name in ARM[:2]:
            result[frame - 1][name] = a[name].slerp(b[name], factor).normalized()
    return result


def unit_projection(vector, axis):
    return (vector - axis * vector.dot(axis)).normalized()


def limb_basis(direction, normal):
    direction = direction.normalized()
    second = normal.cross(direction).normalized()
    return Matrix((direction, second, normal)).transposed().to_quaternion()


def arm_guide(anchors, sample, lift, method):
    phase = sample['p']
    left, plant, right = anchors
    if phase <= plant['p']:
        return sample['loc']['hand_r'].copy()
    factor = (phase - plant['p']) / (right['p'] - plant['p'])
    if method == 'travel':
        # Ease away from the planted hand, then inherit the recovering wrist's velocity.
        span = (right['p'] - plant['p']) / .04
        v1 = (right['travel_wrist'] - left['travel_wrist']) * span
        root_path = plant['travel_wrist'].lerp(right['travel_wrist'], ease(factor, 0))
        root_path += (factor**3 - factor**2) * v1
        goal = root_path - Vector((0, -250 * phase, 0))
    elif method == 'pelvis':
        goal = sample['pelvis'] @ plant['rel']['hand_r'].lerp(right['rel']['hand_r'], ease(factor, .6))
    elif method.startswith('release'):
        # Use the planted world target until shoulder transfer, then bring the arm
        # toward the pelvis-space recovery while lifting clear of the carry floor.
        weight = int(method.removeprefix('release')) / 100
        path = plant['travel_wrist'].lerp(right['travel_wrist'], ease(factor, .25))
        goal_world = path - Vector((0, -250 * phase, 0))
        goal_body = sample['pelvis'] @ plant['rel']['hand_r'].lerp(right['rel']['hand_r'], ease(factor, .6))
        goal = goal_world.lerp(goal_body, weight)
    else:
        goal = sample['loc']['hand_r'].copy()
    # Keep the carrying palm clear when the shoulder takes load. The taper protects
    # the actual ground contact and the original recovery pose at either boundary.
    lift_factor = math.sin(math.pi * factor)
    goal.z = max(goal.z, sample['loc']['hand_r'].z + lift * lift_factor)
    return goal


def geometric_handoff(rig, scene, original, before, first, plant, last, method, lift, pole_turn, slope, optimize=False):
    start, end = before[first-1], before[last-1]
    anchors = (before[last-2], before[plant-1], end)
    samples = before[first-1:last]
    guide = [arm_guide(anchors, sample, lift, method) for sample in samples]
    shoulder_name, elbow_name, wrist_name = ARM
    shoulder_positions = [sample['loc'][shoulder_name] for sample in samples]
    axes = [(wrist - shoulder).normalized() for wrist, shoulder in zip(guide, shoulder_positions)]
    start_pole = unit_projection(start['loc'][elbow_name] - shoulder_positions[0], axes[0])
    end_pole = unit_projection(end['loc'][elbow_name] - shoulder_positions[-1], axes[-1])
    poles = [start_pole]
    for old_axis, axis in zip(axes, axes[1:]):
        poles.append(unit_projection(old_axis.rotation_difference(axis) @ poles[-1], axis))
    pole_adjust = math.atan2(axes[-1].dot(poles[-1].cross(end_pole)), poles[-1].dot(end_pole)) + pole_turn*2*math.pi

    def local_orientation_offsets(sample, axis, pole):
        normal = axis.cross(pole).normalized()
        upper_direction = sample['loc'][elbow_name] - sample['loc'][shoulder_name]
        lower_direction = sample['loc'][wrist_name] - sample['loc'][elbow_name]
        return (limb_basis(upper_direction, normal).inverted() @ sample['rot'][shoulder_name],
                limb_basis(lower_direction, normal).inverted() @ sample['rot'][elbow_name])

    start_offsets = local_orientation_offsets(start, axes[0], start_pole)
    end_offsets = local_orientation_offsets(end, axes[-1], end_pole)
    forearm_span = (start['loc'][wrist_name] - start['loc'][elbow_name]).length
    upperarm_span = (start['loc'][elbow_name] - start['loc'][shoulder_name]).length
    results = [dict(row['q']) for row in before]
    if optimize:
        def anchor_state(sample):
            return sample['q'][shoulder_name].normalized(), sample['q'][elbow_name].normalized(), 0.0
        path = [[anchor_state(start)]]
    select(rig, original)
    for index in range(1, len(samples)-1):
        f = ease(index / (len(samples)-1), slope)
        sample, shoulder, axis = samples[index], shoulder_positions[index], axes[index]
        dist = min(upperarm_span + forearm_span - .2,
                   max(abs(upperarm_span - forearm_span) + .2, (guide[index] - shoulder).length))
        along = (upperarm_span**2 - forearm_span**2 + dist**2) / (2*dist)
        across = math.sqrt(max(0.0, upperarm_span**2 - along**2))
        if optimize:
            base_upper_world = sample['rot'][shoulder_name]
            base_lower_world = sample['rot'][elbow_name]
            local_recover_upper = sample['q'][shoulder_name] @ base_upper_world.inverted()
            local_recover_lower = sample['q'][elbow_name] @ base_lower_world.inverted() @ base_upper_world
            target_wrist = shoulder + axis * dist
            palm_target_z = sample['palm_z'] + .2 if .46 <= sample['p'] <= .64 else sample['palm_z'] - .2
            candidates = []
            for degrees in range(-144, 145, 12):
                pole = Quaternion(axis, pole_adjust * f + math.radians(degrees)) @ poles[index]
                normal = axis.cross(pole).normalized()
                upper_direction = axis * along + pole * across
                lower_direction = axis * dist - upper_direction
                upper_world = limb_basis(upper_direction, normal) @ start_offsets[0].slerp(end_offsets[0], f)
                lower_world = limb_basis(lower_direction, normal) @ start_offsets[1].slerp(end_offsets[1], f)
                for upper_twist in (-45, -22.5, 0, 22.5, 45):
                    upper_rot = Quaternion(upper_direction, math.radians(upper_twist)) @ upper_world
                    upper = (local_recover_upper @ upper_rot).normalized()
                    for lower_twist in (-45, -22.5, 0, 22.5, 45):
                        lower_rot = Quaternion(lower_direction, math.radians(lower_twist)) @ lower_world
                        lower = (local_recover_lower @ upper_rot.inverted() @ lower_rot).normalized()
                        palm = target_wrist + lower_rot @ (base_lower_world.inverted() @ sample['palm_offset'])
                        palm_clearance = palm.z - (sample['loc'][wrist_name].z + sample['palm_offset'].z - sample['palm_z'])
                        cost = .007 * math.radians(degrees)**2 + .003 * (math.radians(upper_twist)**2 + math.radians(lower_twist)**2)
                        cost += .045 * max(0.0, palm_target_z - palm_clearance)**2
                        cost += .025 * max(0.0, -0.7 - palm_clearance)**2
                        candidates.append((upper, lower, cost))
            path.append(candidates)
            continue
        pole = Quaternion(axis, pole_adjust * f) @ poles[index]
        upper_direction = axis * along + pole * across
        lower_direction = axis * dist - upper_direction
        normal = axis.cross(pole).normalized()
        frame = first + index
        scene.frame_set(frame)
        bpy.context.view_layer.update()
        for bone_name, direction, begin, finish in zip(ARM[:2], (upper_direction, lower_direction), start_offsets, end_offsets):
            bone = rig.pose.bones[bone_name]
            desired = limb_basis(direction, normal) @ begin.slerp(finish, f)
            local = (bone.rotation_quaternion @ bone.matrix.to_quaternion().inverted() @ desired).normalized()
            bone.rotation_quaternion = local
            bpy.context.view_layer.update()
            results[frame-1][bone_name] = local
    if optimize:
        path.append([anchor_state(end)])
        chosen = [anchor_state(end)]
        preceding = []
        total = np.zeros((1,))
        prior_u = np.array([[*path[0][0][0]]], dtype=float)
        prior_l = np.array([[*path[0][0][1]]], dtype=float)
        for layer in path[1:]:
            layer_u = np.array([list(q[0]) for q in layer], dtype=float)
            layer_l = np.array([list(q[1]) for q in layer], dtype=float)
            upper_cos = np.einsum('ij,kj->ik', layer_u, prior_u)
            lower_cos = np.einsum('ij,kj->ik', layer_l, prior_l)
            upper = 2 * np.arccos(np.clip(np.abs(upper_cos), 0, 1))
            lower = 2 * np.arccos(np.clip(np.abs(lower_cos), 0, 1))
            transition = (.85 * upper**2 + 1.2 * lower**2 + 12 * np.maximum(upper - .6, 0)**2
                          + 12 * np.maximum(lower - .6, 0)**2)
            combined = transition + total + np.array([state[2] for state in layer])[:, None]
            predecessor = np.argmin(combined, axis=1)
            preceding.append(predecessor)
            total = combined[np.arange(len(layer)), predecessor]
            prior_u, prior_l = layer_u, layer_l
        current = 0
        for index in range(len(path) - 2, -1, -1):
            current = preceding[index][current]
            chosen.append(path[index][current])
        for frame, (upper, lower, _) in enumerate(reversed(chosen), first):
            results[frame-1][shoulder_name] = upper
            results[frame-1][elbow_name] = lower
    return results


def print_compact(name, runtime, integer):
    print(name,
          'forearm', round(runtime['max_arm_step_deg']['lowerarm_r']['step']['lowerarm_r'], 2),
          'wrist', round(runtime['max_wrist_step_cm']['wrist_step_cm'], 2),
          'min', round(integer['min_gap'], 2),
          'arm', round(runtime['max_arm_step_deg']['upperarm_r']['step']['upperarm_r'], 2),
          'retained_support_error', round(integer.get('retained_support_wrist_world_error_cm', 0), 2),
          'shoulder_delta', round(integer.get('shoulder_proxy_delta_cm', 0), 2),
          'sword_palm_transfer_delta', round(runtime.get('minimum_sword_palm_delta_cm', 0), 2),
          'plant_transfer', integer['sampled_support'], flush=True)


def main():
    # Share the original Blender author definitions without executing its driver.
    data = {'__file__': str(SCRIPT)}
    text = SCRIPT.read_text(encoding='utf-8').rsplit('\nmain()', 1)[0]
    exec(compile(text, str(SCRIPT), 'exec'), data)
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE_BLEND), load_ui=False)
    scene = bpy.context.scene
    rig = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
    original = bpy.data.actions['Manny Shoulder Roll']
    floor = json.loads((SOURCE / 'v4/roll-metrics.json').read_text())['neutral_floor_z_cm']
    markers = data['create_floor_markers'](rig)
    sample_args = (markers, data['floor_gaps'], floor)
    phases = [i / 25 for i in range(26)]
    select(rig, original)
    before = sampled(rig, scene, phases, *sample_args)
    before_runtime = sampled(rig, scene, MATCHED_PHASES, *sample_args)
    init_runtime, init_integer = summary(before_runtime), summary(before)
    print_compact('v4', init_runtime, init_integer)
    key_values = [sample['q'] for sample in before]

    reports = {'v4': {'runtime': init_runtime, 'integer': init_integer}}
    results = {}
    label = 'q-12-18-0.5'
    paths = {label: uniform_handoff(key_values, 12, 18, .5)}
    selected_label = os.getenv('ROGUE_SELECTED_ROLL_VARIANT')
    for first, plant, last in ((8,12,19), (9,12,19), (9,12,20), (9,11,20), (9,13,20)):
        for mode in ('travel', 'release30', 'release65'):
            for lift in (4, 8):
                label = f'opt-{first}-{plant}-{last}-{mode}-{lift}'
                if selected_label and selected_label != label:
                    continue
                paths[label] = geometric_handoff(rig, scene, original, before, first, plant, last, mode, lift, 0, .75, optimize=True)
    for label, modified in paths.items():
        action = original.copy()
        action.name = label
        write_keys(rig, scene, action, modified)
        runtime = summary(sampled(rig, scene, MATCHED_PHASES, *sample_args), before_runtime)
        integer = summary(sampled(rig, scene, phases, *sample_args), before)
        reports[label] = {'runtime': runtime, 'integer': integer}
        results[label] = action
        print_compact(label, runtime, integer)

    report_name = 'lead-arm-variants.json' if selected_label is None else 'lead-arm-verified.json'
    (OUT / report_name).write_text(json.dumps(reports, indent=2) + '\n', encoding='utf-8')
    if 'ROGUE_SELECTED_ROLL_VARIANT' not in os.environ:
        return
    selected = results[os.environ['ROGUE_SELECTED_ROLL_VARIANT']]
    for action in results.values():
        if action != selected:
            bpy.data.actions.remove(action)
    original.name = 'Manny Shoulder Roll v4 (baseline)'
    selected.name = 'Manny Shoulder Roll'
    selected.use_fake_user = True
    select(rig, selected)
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    scene.frame_set(13)
    bpy.ops.wm.save_as_mainfile(filepath=str(OUT / 'MannyShoulderRoll.blend'))
    bpy.ops.export_scene.fbx(filepath=str(OUT / 'MannyShoulderRoll.fbx'), use_selection=True,
                             object_types={'ARMATURE'}, add_leaf_bones=False, armature_nodetype='NULL',
                             bake_anim=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                             bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True,
                             axis_forward='-Y', axis_up='Z')


main()
