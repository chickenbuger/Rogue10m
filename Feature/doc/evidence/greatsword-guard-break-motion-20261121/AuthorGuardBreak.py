"""Adapt Manny Primary03 to a compact lift, downward cut, and guard recovery."""
from bisect import bisect_right
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Vector

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / 'greatsword-technique-motion-20261121'))
from AuthorWhirlwind import (FPS, INTERVALS, add_fixed_root, angle_degrees, ease, evaluated_matrices,
                             import_rig, landmarks, local_basis, summarize)

DURATION = INTERVALS / FPS
DONOR_LENGTH = 110 / 60
POSE_TIMES = (0.0, .16, .32, .48, .69, .83, 1.02, 1.36, DURATION)
SOURCE_TIMES = (0.0, .18, .41, .68, .91, .98, 1.19, 1.52, DONOR_LENGTH)


def transfer_time(seconds):
    """Map the lifted guard and short strike while keeping continuous playback speed."""
    if seconds >= POSE_TIMES[-1]:
        return SOURCE_TIMES[-1]
    slopes = [(b - a) / (y - x) for a, b, x, y in zip(SOURCE_TIMES, SOURCE_TIMES[1:], POSE_TIMES, POSE_TIMES[1:])]
    tangents = [slopes[0], *(2 * a * b / (a + b) for a, b in zip(slopes, slopes[1:])), slopes[-1]]
    i = bisect_right(POSE_TIMES, seconds) - 1
    interval = POSE_TIMES[i + 1] - POSE_TIMES[i]
    u = (seconds - POSE_TIMES[i]) / interval
    u2, u3 = u * u, u * u * u
    return ((2*u3 - 3*u2 + 1) * SOURCE_TIMES[i] + (u3 - 2*u2 + u) * interval * tangents[i] +
            (-2*u3 + 3*u2) * SOURCE_TIMES[i + 1] + (u3 - u2) * interval * tangents[i + 1])


def guard_weight(seconds):
    active = ease(seconds / .19) * ease((1.41 - seconds) / .54)
    windup = ease((seconds - .15) / .14) * ease((.59 - seconds) / .17)
    return active * (1.0 - .22 * windup)


def keyed_pose(rig, frame, selected, neutral, weight, previous):
    for bone in rig.pose.bones:
        name = bone.name
        neutral_position, neutral_rotation, _ = neutral[name].decompose()
        position, rotation, _ = selected[name].decompose()
        rotation = neutral_rotation.slerp(rotation, weight)
        if name in previous and previous[name].dot(rotation) < 0:
            rotation.negate()
        previous[name] = rotation.copy()
        bone.rotation_mode = 'QUATERNION'
        bone.location = neutral_position.lerp(position, weight)
        bone.rotation_quaternion = rotation
        bone.scale = (1, 1, 1)
    # Measure after blending pose transforms. Lerp/Slerp can change a toe's
    # component-space height even when both endpoints clear the floor.
    bpy.context.view_layer.update()
    lowest_toe = min(rig.pose.bones[name].head.z for name in ('ball_l', 'ball_r'))
    clearance = max(0.0, .6 - lowest_toe)
    shift = Vector((0, 0, clearance))
    for bone in rig.pose.bones:
        if bone.parent and bone.parent.name == 'root':
            bone.location += bone.bone.matrix_local.to_3x3().inverted() @ shift
        bone.keyframe_insert(data_path='location', frame=frame, group=bone.name)
        bone.keyframe_insert(data_path='rotation_quaternion', frame=frame, group=bone.name)
    return clearance


def main():
    donor_dir, output = (Path(path).resolve() for path in sys.argv[sys.argv.index('--') + 1:])
    output.mkdir(exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    donor = import_rig(donor_dir / 'primary03.fbx')
    guard = import_rig(donor_dir / 'guard.fbx')
    if len(donor.pose.bones) != 88 or set(donor.pose.bones.keys()) != set(guard.pose.bones.keys()):
        raise RuntimeError('Original Manny roots or skeletons changed')
    cm_to_scene = Matrix.Diagonal((.01, .01, .01, 1.0))
    scene_to_cm = cm_to_scene.inverted()
    guard_component = evaluated_matrices(guard, 0.0, scene, scene_to_cm)
    times = [transfer_time(frame / FPS) for frame in range(INTERVALS + 1)]
    source = [evaluated_matrices(donor, seconds / DONOR_LENGTH, scene, scene_to_cm) for seconds in times]
    bpy.data.objects.remove(guard, do_unlink=True)
    donor.animation_data_clear()
    donor.name = 'Armature'
    donor.matrix_world = cm_to_scene
    add_fixed_root(donor)
    donor.animation_data_create()
    action = bpy.data.actions.new('Manny GreatSword GuardBreak - Downward Diagonal')
    action.use_fake_user = True
    donor.animation_data.action = action
    scene.render.fps, scene.render.fps_base = FPS, 1.0
    scene.frame_start, scene.frame_end = 1, INTERVALS + 1

    idle = local_basis(donor, guard_component)
    previous = {}
    applied_clearances = []
    for index, selected in enumerate(source):
        applied_clearances.append(keyed_pose(donor, index + 1, local_basis(donor, selected), idle,
                                               guard_weight(index / FPS), previous))

    for curve in action.fcurves:
        for point in curve.keyframe_points:
            point.interpolation = 'LINEAR'

    poses = []
    root_rotation = 0.0
    palms = []
    for frame in range(1, INTERVALS + 2):
        scene.frame_set(frame)
        poses.append(landmarks(donor))
        bones = donor.pose.bones
        palms.append((bones['hand_r'].head.lerp(bones['middle_01_r'].head, .5) -
                      bones['hand_l'].head.lerp(bones['middle_01_l'].head, .5)).length)
        root_rotation = max(root_rotation, angle_degrees(bones['root'].matrix.to_quaternion(), Matrix.Identity(4).to_quaternion()))
    result = summarize(poses)
    result.update({'bone_count': len(donor.pose.bones), 'fps': FPS, 'intervals': INTERVALS,
                   'root_rotation_deg': root_rotation, 'palm_span_cm': [min(palms), max(palms)],
                   'floor_correction_max_cm': max(applied_clearances),
                   'shoulder_ready': poses[0]['upperarm_r'], 'shoulder_recovery': poses[-1]['upperarm_r']})
    (output / 'blender-motion.json').write_text(json.dumps({'summary': result, 'source_seconds': times}, indent=2) + '\n', encoding='utf-8')
    if (result['root_travel_cm'] > .001 or root_rotation > .02 or result['shoulder_yaw_largest_step_deg'] > 17 or
            min(result[name]['lowest_z_cm'] for name in ('ball_l', 'ball_r')) < -.15 or
            not 65 < result['shoulder_yaw_range_deg'] < 190 or min(palms) < 10 or max(applied_clearances) > 2.75):
        raise RuntimeError(f'Candidate geometry failed: {result}')

    bpy.ops.object.select_all(action='DESELECT')
    donor.select_set(True)
    bpy.context.view_layer.objects.active = donor
    bpy.ops.export_scene.fbx(filepath=str(output / 'MannyGreatSwordGuardBreak.fbx'), use_selection=True,
                             object_types={'ARMATURE'}, add_leaf_bones=False, armature_nodetype='NULL',
                             bake_anim=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                             bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True,
                             axis_forward='-Y', axis_up='Z')
    scene.frame_set(42)
    bpy.ops.wm.save_as_mainfile(filepath=str(output / 'MannyGreatSwordGuardBreak.blend'))
    print(f'GUARD_BREAK_BAKED {json.dumps(result)}')


if __name__ == '__main__':
    main()
