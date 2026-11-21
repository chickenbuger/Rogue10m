"""Bake the existing two-handed spin into the Manny pose with a stationary root."""
from bisect import bisect_right
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix


FPS = 60
INTERVALS = 100
BONES = ('root', 'pelvis', 'upperarm_l', 'upperarm_r', 'hand_l', 'hand_r',
         'foot_l', 'foot_r', 'ball_l', 'ball_r', 'ik_foot_l', 'ik_foot_r')


def import_rig(path):
    existing = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path), automatic_bone_orientation=False)
    return next(obj for obj in bpy.data.objects if obj not in existing and obj.type == 'ARMATURE')


def evaluated_matrices(rig, phase, scene, scene_to_cm):
    start, end = rig.animation_data.action.frame_range
    frame = start + phase * (end - start)
    scene.frame_set(math.floor(frame), subframe=frame % 1.0)
    object_in_cm = scene_to_cm @ rig.matrix_world
    return {bone.name: object_in_cm @ bone.matrix for bone in rig.pose.bones}


def add_fixed_root(rig):
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.mode_set(mode='EDIT')
    bones = rig.data.edit_bones
    root = bones.new('root')
    root.head, root.tail = (0, 0, 0), (0, 10, 0)
    for bone in list(bones):
        if bone != root and bone.parent is None:
            bone.parent = root
    bpy.ops.object.mode_set(mode='OBJECT')


def local_basis(rig, component):
    result = {'root': Matrix.Identity(4)}
    for pose_bone in rig.pose.bones:
        bone = pose_bone.bone
        if bone.name == 'root':
            continue
        parent = bone.parent
        parent_pose = component.get(parent.name, parent.matrix_local)
        result[bone.name] = bone.convert_local_to_pose(
            component[bone.name], bone.matrix_local, parent_matrix=parent_pose,
            parent_matrix_local=parent.matrix_local, invert=True)
    return result


def ease(value):
    u = min(1.0, max(0.0, value))
    return u * u * (3.0 - 2.0 * u)


def angle_degrees(quat_a, quat_b):
    return math.degrees(2 * math.acos(min(1, abs(quat_a.normalized().dot(quat_b.normalized())))))


def yaw_delta(a, b):
    return (b - a + 180) % 360 - 180


def shoulder_yaw(component):
    side = component['upperarm_l'].translation - component['upperarm_r'].translation
    return math.degrees(math.atan2(side.y, side.x))


def distribute_turn(rig, scene, scene_to_cm):
    """Give the fastest donor portion more frames without moving any poses."""
    ticks = 400
    yaws = [shoulder_yaw(evaluated_matrices(rig, tick / ticks, scene, scene_to_cm))
            for tick in range(ticks + 1)]
    steps = [abs(yaw_delta(a, b)) for a, b in zip(yaws, yaws[1:])]
    durations = [math.sqrt(1.0 + (step / 2.4) ** 4) ** .5 for step in steps]
    clock = [0.0]
    for duration in durations:
        clock.append(clock[-1] + duration)
    phases = [0.0]
    for index in range(1, INTERVALS):
        mapped = clock[-1] * index / INTERVALS
        i = bisect_right(clock, mapped) - 1
        phases.append((i + (mapped - clock[i]) / durations[i]) / ticks)
    return phases + [1.0]


def landmarks(rig):
    return {name: list(rig.pose.bones[name].head) for name in BONES}


def summarize(samples):
    previous = None
    shoulder_yaw = 0.0
    yaw_values = [0.0]
    for row in samples:
        left, right = row['upperarm_l'], row['upperarm_r']
        angle = math.degrees(math.atan2(left[1] - right[1], left[0] - right[0]))
        if previous is not None:
            shoulder_yaw += yaw_delta(previous, angle)
            yaw_values.append(shoulder_yaw)
        previous = angle
    yaw_steps = [abs(b - a) for a, b in zip(yaw_values, yaw_values[1:])]
    result = {'shoulder_yaw_range_deg': max(yaw_values) - min(yaw_values),
              'shoulder_yaw_largest_step_deg': max(yaw_steps),
              'shoulder_largest_step_phase': (yaw_steps.index(max(yaw_steps)) + 1) / INTERVALS,
              'root_travel_cm': max(math.dist(row['root'], samples[0]['root']) for row in samples)}
    for name in ('hand_l', 'hand_r', 'foot_l', 'foot_r', 'ball_l', 'ball_r'):
        values = [row[name] for row in samples]
        result[name] = {'lowest_z_cm': min(row[2] for row in values),
                        'largest_step_cm': max(math.dist(a, b) for a, b in zip(values, values[1:])),
                        'endpoint_delta_cm': math.dist(values[0], values[-1])}
    result['hand_spacing_cm'] = [min(math.dist(row['hand_l'], row['hand_r']) for row in samples),
                                 max(math.dist(row['hand_l'], row['hand_r']) for row in samples)]
    return result


def main():
    donor_dir, output_dir = (Path(path).resolve() for path in sys.argv[sys.argv.index('--') + 1:])
    output_dir.mkdir(exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.fps = FPS
    cm_to_scene = Matrix.Diagonal((.01, .01, .01, 1.0))
    scene_to_cm = cm_to_scene.inverted()
    donor = import_rig(donor_dir / 'alt04.fbx')
    guard = import_rig(donor_dir / 'guard.fbx')
    if len(donor.pose.bones) != 88 or set(donor.pose.bones.keys()) != set(guard.pose.bones.keys()):
        raise RuntimeError('Expected matching Manny skeleton-only donors with their root represented as the armature.')
    guard_pose = evaluated_matrices(guard, 0.0, scene, scene_to_cm)
    phases = distribute_turn(donor, scene, scene_to_cm)
    source = [evaluated_matrices(donor, phase, scene, scene_to_cm) for phase in phases]
    bpy.data.objects.remove(guard, do_unlink=True)
    donor.animation_data_clear()
    donor.name = 'Armature'
    donor.matrix_world = cm_to_scene
    add_fixed_root(donor)
    donor.animation_data_create()
    action = bpy.data.actions.new('Manny GreatSword Whirlwind - In Place')
    action.use_fake_user = True
    donor.animation_data.action = action
    scene.render.fps, scene.render.fps_base = FPS, 1.0
    scene.frame_start, scene.frame_end = 1, INTERVALS + 1

    idle = local_basis(donor, guard_pose)
    previous = {}
    for index, component in enumerate(source):
        p = index / INTERVALS
        donor_weight = ease(p / .055) * ease((1 - p) / .085)
        moved = local_basis(donor, component)
        for bone in donor.pose.bones:
            name = bone.name
            idle_position, idle_rotation, _ = idle[name].decompose()
            moved_position, moved_rotation, _ = moved[name].decompose()
            rotation = idle_rotation.slerp(moved_rotation, donor_weight)
            if name in previous and previous[name].dot(rotation) < 0:
                rotation.negate()
            previous[name] = rotation.copy()
            bone.rotation_mode = 'QUATERNION'
            bone.location = idle_position.lerp(moved_position, donor_weight)
            bone.rotation_quaternion = rotation
            bone.scale = (1.0, 1.0, 1.0)
            bone.keyframe_insert(data_path='location', frame=index + 1, group=name)
            bone.keyframe_insert(data_path='rotation_quaternion', frame=index + 1, group=name)

    # Keep the baked intervals linear and the signed quaternions continuous.
    for curve in action.fcurves:
        for point in curve.keyframe_points:
            point.interpolation = 'LINEAR'

    baked = []
    reconstructed_error = 0.0
    reconstruction_angular_error = 0.0
    max_root_rotation = 0.0
    for index, component in enumerate(source):
        scene.frame_set(index + 1)
        baked.append(landmarks(donor))
        max_root_rotation = max(max_root_rotation, angle_degrees(donor.pose.bones['root'].matrix.to_quaternion(), Matrix.Identity(4).to_quaternion()))
        if .06 <= index / INTERVALS <= .91:
            for bone in donor.pose.bones:
                if bone.name == 'root':
                    continue
                reconstructed_error = max(reconstructed_error, (bone.head - component[bone.name].translation).length)
                reconstruction_angular_error = max(reconstruction_angular_error,
                    angle_degrees(bone.matrix.to_quaternion(), component[bone.name].to_quaternion()))
    metrics = summarize(baked)
    metrics.update({'bone_count': len(donor.pose.bones), 'sample_rate': FPS, 'intervals': INTERVALS,
                    'root_rotation_deg': max_root_rotation, 'core_reconstruction_error_cm': reconstructed_error,
                    'core_reconstruction_rotation_deg': reconstruction_angular_error})
    (output_dir / 'blender-motion.json').write_text(json.dumps({'summary': metrics, 'donor_phase': phases, 'poses_cm': baked}, indent=2) + '\n', encoding='utf-8')
    if (metrics['root_travel_cm'] > .001 or max_root_rotation > .02 or reconstructed_error > .03 or
            reconstruction_angular_error > .1 or metrics['shoulder_yaw_range_deg'] < 350 or
            metrics['shoulder_yaw_largest_step_deg'] > 15 or min(metrics[name]['lowest_z_cm'] for name in ('ball_l', 'ball_r')) < -.25):
        raise RuntimeError(f'Refusing to export the physically discontinuous motion: {metrics}')

    bpy.ops.object.select_all(action='DESELECT')
    donor.select_set(True)
    bpy.context.view_layer.objects.active = donor
    bpy.ops.export_scene.fbx(filepath=str(output_dir / 'MannyGreatSwordWhirlwind.fbx'), use_selection=True,
                             object_types={'ARMATURE'}, add_leaf_bones=False, armature_nodetype='NULL',
                             bake_anim=True, bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False,
                             bake_anim_simplify_factor=0, bake_anim_force_startend_keying=True,
                             axis_forward='-Y', axis_up='Z')
    scene.frame_set(27)
    bpy.ops.wm.save_as_mainfile(filepath=str(output_dir / 'MannyGreatSwordWhirlwind.blend'))
    print(f'WHIRLWIND_BAKED {json.dumps(metrics)}')


if __name__ == '__main__':
    main()
