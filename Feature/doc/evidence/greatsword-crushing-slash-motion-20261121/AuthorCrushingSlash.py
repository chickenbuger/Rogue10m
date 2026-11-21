"""Bake a two-handed overhead GreatSword strike from the existing Primary02."""
from bisect import bisect_right
import json
from pathlib import Path
import sys

import bpy
from mathutils import Matrix

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent / 'greatsword-guard-break-motion-20261121'))
sys.path.insert(0, str(HERE.parent / 'greatsword-technique-motion-20261121'))
from AuthorGuardBreak import keyed_pose
from AuthorWhirlwind import (FPS, add_fixed_root, angle_degrees, ease, evaluated_matrices,
                             import_rig, landmarks, local_basis, summarize)

INTERVALS = 110
DURATION = INTERVALS / FPS
DONOR_LENGTH = 76 / 60
KEYS = ((0., 0.), (.18, .10), (.34, .28), (.47, .49), (.62, .59),
        (.76, .659), (.92, .682), (1.14, .692), (DURATION, .699))


def source_time(t):
    times, source = zip(*KEYS)
    if t >= times[-1]:
        return source[-1]
    rates = [(b-a)/(v-u) for (u,a),(v,b) in zip(KEYS,KEYS[1:])]
    tangents = [rates[0], *(2*a*b/(a+b) for a,b in zip(rates,rates[1:])), rates[-1]]
    i = bisect_right(times, t)-1
    span = times[i+1]-times[i]
    u = (t-times[i])/span
    u2, u3 = u*u, u*u*u
    return ((2*u3-3*u2+1)*source[i]+(u3-2*u2+u)*span*tangents[i] +
            (-2*u3+3*u2)*source[i+1]+(u3-u2)*span*tangents[i+1])


def main():
    source_dir, output = (Path(path).resolve() for path in sys.argv[sys.argv.index('--') + 1:])
    output.mkdir(exist_ok=False)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    donor = import_rig(source_dir / 'primary02.fbx')
    guard = import_rig(source_dir / 'guard.fbx')
    if len(donor.pose.bones) != 88 or set(donor.pose.bones.keys()) != set(guard.pose.bones.keys()):
        raise RuntimeError('Expected the same Manny skeleton-only rig in both original assets')
    cm_to_scene = Matrix.Diagonal((.01, .01, .01, 1.0))
    scene_to_cm = cm_to_scene.inverted()
    neutral = evaluated_matrices(guard, 0.0, scene, scene_to_cm)
    source_seconds = [source_time(index / FPS) for index in range(INTERVALS + 1)]
    pose_matrices = [evaluated_matrices(donor, seconds / DONOR_LENGTH, scene, scene_to_cm)
                     for seconds in source_seconds]
    bpy.data.objects.remove(guard, do_unlink=True)
    donor.animation_data_clear()
    donor.name = 'Armature'
    donor.matrix_world = cm_to_scene
    add_fixed_root(donor)
    donor.animation_data_create()
    action = bpy.data.actions.new('Manny GreatSword Crushing Slash - Overhead')
    action.use_fake_user = True
    donor.animation_data.action = action
    scene.render.fps, scene.render.fps_base = FPS, 1.0
    scene.frame_start, scene.frame_end = 1, INTERVALS + 1

    ready_basis = local_basis(donor, neutral)
    last_rotations = {}
    raised_cm = []
    for index, selected in enumerate(pose_matrices):
        t = index / FPS
        weight = ease(t / .20) * ease((1.51-t) / .62)
        raised_cm.append(keyed_pose(donor, index + 1, local_basis(donor, selected), ready_basis, weight, last_rotations))

    for curve in action.fcurves:
        for point in curve.keyframe_points:
            point.interpolation = 'LINEAR'

    poses = []
    palm_spans = []
    blade_clearance = []
    root_turn = 0.
    for index in range(INTERVALS + 1):
        scene.frame_set(index + 1)
        poses.append(landmarks(donor))
        bones = donor.pose.bones
        palm_r = bones['hand_r'].head.lerp(bones['middle_01_r'].head,.5)
        palm_l = bones['hand_l'].head.lerp(bones['middle_01_l'].head,.5)
        shaft = palm_r-palm_l
        palm_spans.append(shaft.length)
        blade_clearance.append(min((palm_r-30*shaft.normalized()).z, (palm_r+140*shaft.normalized()).z))
        root_turn = max(root_turn, angle_degrees(bones['root'].matrix.to_quaternion(), Matrix.Identity(4).to_quaternion()))
    result = summarize(poses)
    result['shoulder_largest_step_phase'] *= (100 / INTERVALS)
    largest_floor_step = max(abs(a-b) for a,b in zip(raised_cm,raised_cm[1:]))
    result.update({'fps': FPS, 'intervals': INTERVALS, 'bone_count': len(donor.pose.bones),
                   'root_rotation_deg': root_turn, 'max_floor_raise_cm': max(raised_cm),
                   'floor_raise_peak_motion_s': raised_cm.index(max(raised_cm)) / FPS,
                   'largest_floor_raise_step_cm': largest_floor_step,
                   'palm_span_cm': [min(palm_spans), max(palm_spans)],
                   'two_palms_sword_floor_proxy_cm': min(blade_clearance)})
    (output / 'blender-motion.json').write_text(json.dumps({'summary': result, 'source_seconds': source_seconds}, indent=2)+'\n', encoding='utf-8')
    if (result['root_travel_cm'] > .001 or root_turn > .02 or max(palm_spans)>28 or min(palm_spans)<10 or
            max(raised_cm)>4.1 or largest_floor_step>1.25 or min(blade_clearance)<7 or result['shoulder_yaw_largest_step_deg']>20):
        raise RuntimeError(f'Candidate pose or sword proxy failed: {result}')

    bpy.ops.object.select_all(action='DESELECT')
    donor.select_set(True)
    bpy.context.view_layer.objects.active = donor
    bpy.ops.export_scene.fbx(filepath=str(output / 'MannyGreatSwordCrushingSlash.fbx'), use_selection=True,
        object_types={'ARMATURE'}, add_leaf_bones=False, armature_nodetype='NULL', bake_anim=True,
        bake_anim_use_nla_strips=False, bake_anim_use_all_actions=False, bake_anim_simplify_factor=0,
        bake_anim_force_startend_keying=True, axis_forward='-Y', axis_up='Z')
    scene.frame_set(round(.76*FPS) + 1)
    bpy.ops.wm.save_as_mainfile(filepath=str(output / 'MannyGreatSwordCrushingSlash.blend'))
    print(f'CRUSHING_SLASH_BAKED {json.dumps(result)}')


if __name__=='__main__':
    main()
