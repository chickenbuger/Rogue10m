"""Reopen the Blender source and independently reimport its game FBX."""
import hashlib
import json
import math
from pathlib import Path
import sys

import bpy


def verify(path):
    if path.suffix == '.blend':
        bpy.ops.wm.open_mainfile(filepath=str(path))
    else:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(path), automatic_bone_orientation=False)
    scene = bpy.context.scene
    rig = next(obj for obj in scene.objects if obj.type == 'ARMATURE')
    start, stop = rig.animation_data.action.frame_range
    fps = scene.render.fps / scene.render.fps_base
    if len(rig.pose.bones) != 89 or int(stop - start) != 110 or 'root' not in rig.pose.bones or abs(fps - 60) > .01:
        raise RuntimeError(f'Unexpected rig or key grid for {path}: {len(rig.pose.bones)}, {start}..{stop} at {fps} FPS')
    result = []
    for frame in range(int(start), int(stop) + 1):
        scene.frame_set(frame)
        result.append({bone.name: (list((rig.matrix_world @ bone.head) * 100),
                      tuple((rig.matrix_world @ bone.matrix).to_quaternion().normalized())) for bone in rig.pose.bones})
    return result


def quat_angle(a, b):
    return math.degrees(2 * math.acos(min(1, abs(sum(x * y for x, y in zip(a, b))))))


def main():
    root = Path(sys.argv[-1]).resolve()
    blend = root / 'MannyGreatSwordCapstoneRelease.blend'
    fbx = root / 'MannyGreatSwordCapstoneRelease.fbx'
    reference, imported = verify(blend), verify(fbx)
    maximum = max(math.dist(ref[name][0], entry[name][0]) for ref, entry in zip(reference, imported) for name in ref)
    angle = max(quat_angle(ref[name][1], entry[name][1]) for ref, entry in zip(reference, imported) for name in ref)
    root_travel = max(math.dist(row['root'][0], imported[0]['root'][0]) for row in imported)
    root_yaw = max(quat_angle(row['root'][1], imported[0]['root'][1]) for row in imported)
    endpoint = max(math.dist(imported[0][name][0], imported[-1][name][0]) for name in imported[0])
    summary = {'fbx_sha256': hashlib.sha256(fbx.read_bytes()).hexdigest(),
               'blend_sha256': hashlib.sha256(blend.read_bytes()).hexdigest(),
               'bones': len(reference[0]), 'samples': len(reference), 'fps': 60,
               'max_world_landmark_error_cm': maximum, 'max_world_rotation_error_deg': angle,
               'root_travel_cm': root_travel, 'root_rotation_deg': root_yaw, 'endpoint_error_cm': endpoint}
    (root / 'fresh-blender-reimport.json').write_text(json.dumps(summary, indent=2) + '\n', encoding='utf-8')
    if maximum > .05 or angle > .12 or root_travel > .001 or root_yaw > .08 or endpoint > .02:
        raise RuntimeError(f'Round-tripped export did not preserve the authored sweep: {summary}')
    print(f'CAPSTONE_RELEASE_BLENDER_REIMPORT_PASSED {summary}')


main()
