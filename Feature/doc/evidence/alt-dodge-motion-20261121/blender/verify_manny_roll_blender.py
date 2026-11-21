"""Fresh Blender roundtrip of the authoring scene and its animation-only FBX."""

import json
import math
import os
from pathlib import Path

import bpy


OUT = Path(__file__).resolve().parent / os.getenv('ROGUE_SHOULDER_ROLL_LABEL', 'v2')
BONES = ('root', 'pelvis', 'head', 'upperarm_r', 'hand_l', 'hand_r', 'foot_l', 'foot_r')


def inspect():
    scene = bpy.context.scene
    armature = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
    frames = tuple(round(value) for value in armature.animation_data.action.frame_range)
    hierarchy = {bone.name: bone.parent.name if bone.parent else None for bone in armature.pose.bones}
    poses = []
    for frame in range(frames[0], frames[1] + 1):
        scene.frame_set(frame)
        root = armature.pose.bones['root']
        poses.append({name: tuple(armature.matrix_world @ armature.pose.bones[name].head) for name in BONES})
        if root.location.length > 1.0e-4 or abs(root.rotation_quaternion.w) < .999999:
            raise RuntimeError(f'Unexpected root drift at frame {frame}')
    return hierarchy, poses, scene.render.fps / scene.render.fps_base


def main():
    bpy.ops.wm.open_mainfile(filepath=str(OUT / 'MannyShoulderRoll.blend'))
    before, authored, authored_fps = inspect()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(OUT / 'MannyShoulderRoll.fbx'))
    after, imported, imported_fps = inspect()
    if before != after:
        raise RuntimeError(f'Hierarchy changed: before={len(before)}, after={len(after)}')
    if len(authored) != 26 or len(imported) != len(authored) or imported_fps != authored_fps:
        raise RuntimeError(f'Unexpected sample count or fps: {len(authored)}/{len(imported)}, {authored_fps}/{imported_fps}')

    position_error_cm = max(math.dist(src[name], dst[name]) * 100
                            for src, dst in zip(authored, imported, strict=True) for name in BONES)
    endpoint_error_cm = max(math.dist(imported[0][name], imported[-1][name]) * 100 for name in BONES)
    if position_error_cm > .1 or endpoint_error_cm > .01:
        raise RuntimeError(f'Animation roundtrip exceeded tolerance {position_error_cm}/{endpoint_error_cm}cm')

    report = {'bone_count': len(after), 'fps': imported_fps, 'sample_count': len(imported),
              'max_pose_position_error_cm': position_error_cm, 'max_endpoint_error_cm': endpoint_error_cm,
              'stationary_root': True, 'compared_bones': BONES}
    engine_manifest = OUT / 'unreal-reimport.json'
    if engine_manifest.is_file():
        engine_samples = json.loads(engine_manifest.read_text(encoding='utf-8'))['samples']
        cross_engine_error = max(
            math.dist((100*row[name][0], -100*row[name][1], 100*row[name][2]), engine[name]['p'])
            for row, engine in zip(imported, engine_samples, strict=True) for name in BONES
        )
        if cross_engine_error > .1:
            raise RuntimeError(f'Unreal roundtrip exceeded .1 cm: {cross_engine_error}')
        report['max_unreal_blender_position_error_cm'] = cross_engine_error
    (OUT / 'fresh-blender-reimport.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')


main()
