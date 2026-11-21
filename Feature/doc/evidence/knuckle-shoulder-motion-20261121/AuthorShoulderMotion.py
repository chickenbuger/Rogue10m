"""Bake an in-place shoulder strike from the project's Manny ShortBlow guard."""
import json
import math
from pathlib import Path
import sys

import bpy
from mathutils import Matrix, Quaternion, Vector


FPS = 60
FORWARD = Vector((0, -1, 0))
RIGHT = Vector((-1, 0, 0))
UP = Vector((0, 0, 1))
TWIST_AXIS = RIGHT.cross(FORWARD).normalized()
LEAN_AXIS = UP.cross(FORWARD).normalized()
UNIT = Vector((1, 1, 1))
# Frame, pelvis advance, sink, torso turn, forward lean, right ankle advance, right ankle lift, arm tuck.
LANDMARKS = (
    (0, 0, 0, 0, 0, 0, 0, 0),
    (5, -7, -6, -13, -4, 0, 4, .22),
    (13, 25, -9, 31, 20, 24, 0, 1),
    (17, 23, -10, 30, 19, 24, 0, 1),
    (23, 13, -7, 20, 12, 21, 0, .65),
    (32, 2, -3, 6, 3, 5, 5, .18),
    (39, 0, 0, 0, 0, 0, 0, 0),
)
BONES = ('root', 'pelvis', 'spine_05', 'head', 'upperarm_r', 'lowerarm_r', 'hand_r', 'hand_l',
         'thigh_r', 'calf_r', 'foot_r', 'ball_r', 'thigh_l', 'calf_l', 'foot_l', 'ball_l')


def phase(frame):
    for start, end in zip(LANDMARKS, LANDMARKS[1:]):
        if frame <= end[0]:
            u = (frame - start[0]) / (end[0] - start[0])
            weight = u * u * (3 - 2 * u)
            return tuple(a + (b - a) * weight for a, b in zip(start[1:], end[1:]))
    return LANDMARKS[-1][1:]


def rotate(bone, rotation):
    bone.matrix = Matrix.LocRotScale(bone.head, rotation @ bone.matrix.to_quaternion(), UNIT)
    bpy.context.view_layer.update()


def bend_direction(start, middle, end):
    axis = (end - start).normalized()
    return ((middle - start) - axis * (middle - start).dot(axis)).normalized()


def solve_chain(root, middle, end, goal, bend, orientation):
    """Articulate the existing joint lengths toward a reachable world goal."""
    start = root.head.copy()
    upper_length, lower_length = (middle.head - start).length, (end.head - middle.head).length
    direction = (goal - start).normalized()
    distance = min(max((goal - start).length, abs(upper_length - lower_length) + .15), upper_length + lower_length - .15)
    projection = (upper_length**2 - lower_length**2 + distance**2) / (2 * distance)
    height = math.sqrt(max(0.0, upper_length**2 - projection**2))
    pole = (bend - direction * bend.dot(direction)).normalized()
    hinge = start + direction * projection + pole * height
    target = start + direction * distance
    rotate(root, (middle.head - start).rotation_difference(hinge - start))
    rotate(middle, (end.head - middle.head).rotation_difference(target - middle.head))
    end.matrix = Matrix.LocRotScale(end.head, orientation, UNIT)
    bpy.context.view_layer.update()


def key_pose(rig, frame, previous):
    for bone in rig.pose.bones:
        rotation = bone.rotation_quaternion
        if bone.name in previous and previous[bone.name].dot(rotation) < 0.0:
            rotation.negate()
        previous[bone.name] = rotation.copy()
        bone.keyframe_insert('location', frame=frame, group=bone.name)
        bone.keyframe_insert('rotation_quaternion', frame=frame, group=bone.name)


def main():
    source, destination, helper_path = [Path(arg).resolve() for arg in sys.argv[sys.argv.index('--') + 1:]]
    sys.path.insert(0, str(helper_path))
    from AuthorWhirlwind import add_fixed_root, evaluated_matrices, import_rig, local_basis

    info = json.loads((source / 'reference.json').read_text(encoding='utf-8'))
    intervals = round(info['sequence_seconds'] * FPS)
    if intervals < LANDMARKS[-1][0] or abs(intervals / FPS - info['montage_seconds']) > 1e-6:
        raise RuntimeError(f'Unexpected original montage duration {info}')
    destination.mkdir(exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    rig = import_rig(source / 'ShortBlow.fbx')
    cm_to_scene = Matrix.Diagonal((.01, .01, .01, 1.0))
    ready = evaluated_matrices(rig, 0.0, scene, cm_to_scene.inverted())
    contact = evaluated_matrices(rig, 13 / intervals, scene, cm_to_scene.inverted())
    rig.animation_data_clear()
    rig.name = 'Armature'
    rig.matrix_world = cm_to_scene
    if 'root' not in rig.pose.bones:
        add_fixed_root(rig)
    rest = local_basis(rig, ready)
    p = rig.pose.bones
    leg_bend = {side: bend_direction(*(ready[f'{part}_{side}'].translation for part in ('thigh','calf','foot')))
                for side in ('l','r')}
    scene.render.fps = FPS
    scene.frame_start, scene.frame_end = 1, intervals + 1
    rig.animation_data_create()
    action = bpy.data.actions.new('Manny Shoulder Bash - Grounded Lead')
    rig.animation_data.action = action
    action.use_fake_user = True
    poses, previous = [], {}
    for index in range(intervals + 1):
        scene.frame_set(index + 1)
        for bone in p:
            bone.rotation_mode = 'QUATERNION'
            bone.matrix_basis = rest[bone.name]
        bpy.context.view_layer.update()
        advance, sink, yaw, lean, lead, lift, tuck = phase(index)
        if 0 < index < LANDMARKS[-1][0]:
            pelvis_turn = Quaternion(TWIST_AXIS, math.radians(yaw * .36))
            p['pelvis'].matrix = Matrix.LocRotScale(ready['pelvis'].translation + FORWARD * advance + UP * sink,
                                                    pelvis_turn @ ready['pelvis'].to_quaternion(), UNIT)
            bpy.context.view_layer.update()
            for side in ('l','r'):
                foot = ready[f'foot_{side}']
                offset = FORWARD * lead + UP * lift if side == 'r' else Vector((0, 0, 0))
                solve_chain(*(p[f'{part}_{side}'] for part in ('thigh','calf','foot')),
                            foot.translation + offset, leg_bend[side], foot.to_quaternion())
            rotate(p['spine_01'], Quaternion(LEAN_AXIS, math.radians(lean * .62)))
            rotate(p['spine_03'], Quaternion(TWIST_AXIS, math.radians(yaw * .64)) @ Quaternion(LEAN_AXIS, math.radians(lean * .38)))
            for side, offset, elbow in (
                ('r', -FORWARD * 12 - RIGHT * 16 - UP * 20, RIGHT - UP * 1.8 - FORWARD * .35),
                ('l', -FORWARD * 14 + RIGHT * 9 - UP * 12, -RIGHT - UP * 1.6 - FORWARD * .3),
            ):
                upper, forearm, hand = (p[f'{part}_{side}'] for part in ('upperarm','lowerarm','hand'))
                goal = hand.head.lerp(upper.head + offset, tuck)
                current_bend = bend_direction(upper.head, forearm.head, hand.head)
                bend = current_bend.lerp(elbow.normalized(), tuck)
                solve_chain(upper, forearm, hand, goal, bend, hand.matrix.to_quaternion())
            head = p['head']
            head.matrix = Matrix.LocRotScale(head.head, ready['head'].to_quaternion(), UNIT)
        for helper, target in (('ik_foot_l','foot_l'),('ik_foot_r','foot_r'),('ik_hand_l','hand_l'),('ik_hand_r','hand_r')):
            if helper in p:
                p[helper].matrix = p[target].matrix
        bpy.context.view_layer.update()
        poses.append({name: list(p[name].head) for name in BONES})
        key_pose(rig, index + 1, previous)
    for fcurve in action.fcurves:
        for point in fcurve.keyframe_points:
            point.interpolation = 'LINEAR'

    for check in ('root','ball_l'):
        if max(math.dist(row[check], poses[0][check]) for row in poses) > .08:
            raise RuntimeError(f'{check} lost its planted contact')
    if max(math.dist(poses[0][name], poses[-1][name]) for name in BONES) > .08:
        raise RuntimeError('Recovery does not match the ready pose')
    peak = poses[13]
    shoulder_lead = min((Vector(peak['upperarm_r']) - Vector(peak[hand])).dot(FORWARD) for hand in ('hand_r','hand_l'))
    if shoulder_lead < 2.0:
        raise RuntimeError(f'The fist still leads the shoulder: {shoulder_lead:.3f}cm')
    report = {'fps': FPS, 'intervals': intervals, 'bones': len(p), 'shoulder_lead_at_frame_13_cm': shoulder_lead,
              'stock_hand_ahead_of_shoulder_cm': (contact['hand_r'].translation - contact['upperarm_r'].translation).dot(FORWARD),
              'pose_landmarks': {str(frame): poses[frame] for frame in (0,5,13,17,23,32,39,intervals)}}
    (destination / 'shoulder-blender.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    bpy.ops.object.select_all(action='DESELECT')
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    scene.frame_set(14)
    bpy.ops.wm.save_as_mainfile(filepath=str(destination / 'MannyShoulderBash.blend'))
    bpy.ops.export_scene.fbx(filepath=str(destination / 'MannyShoulderBash.fbx'), use_selection=True, object_types={'ARMATURE'},
                             apply_unit_scale=True, add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
                             bake_anim_use_nla_strips=False, bake_anim_simplify_factor=0, axis_forward='-Y', axis_up='Z')


if __name__ == '__main__':
    main()
